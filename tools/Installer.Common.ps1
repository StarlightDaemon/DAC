#Requires -Version 7.0
# SPDX-License-Identifier: MIT
# Narrow offline reader for the committed Inno 7.1.0, non-spanned, unencrypted
# LZMA2 installer. Layout reviewed against jrsoftware/issrc tag is-7_1_0:
# Shared.Struct, Shared.SetupEntFunc, Compression.Base, Setup.FileExtractor.
# No installation, registry access, product execution, or third-party downloads.
function Get-InstallerSourceIdentity {
    $commit=(& git -C $repository rev-parse HEAD).Trim()
    if ($LASTEXITCODE -ne 0 -or $commit -cnotmatch '^[0-9a-f]{40}$') {throw 'Source commit unavailable.'}
    $status=@(& git --no-optional-locks -C $repository status --porcelain=v1)
    if ($LASTEXITCODE -ne 0) {throw 'Source status unavailable.'}
    [string[]]$names=@(& git -C $repository ls-files --cached --others --exclude-standard)
    if ($LASTEXITCODE -ne 0 -or $names.Count -eq 0) {throw 'Source inventory unavailable.'}
    [Array]::Sort($names,[StringComparer]::Ordinal)
    $lines=@(foreach ($name in $names) {
        if ([IO.Path]::IsPathRooted($name) -or $name -match '(^|/)\.\.?(/|$)|[:\\]') {throw 'Unsafe source inventory name.'}
        $file=Resolve-PackageLocalPath $repository $name
        if ((Get-Item -LiteralPath $file -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) {throw 'Reparse source rejected.'}
        $name+' '+(Get-FileHash -LiteralPath $file).Hash.ToLowerInvariant()
    })
    $hash=[Convert]::ToHexString([Security.Cryptography.SHA256]::HashData([Text.UTF8Encoding]::new($false).GetBytes(($lines -join "`n")+"`n"))).ToLowerInvariant()
    [pscustomobject]@{commit=$commit;modified=($status.Count -ne 0);files=$names.Count;sha256=$hash}
}
function Assert-InstallerSourceUnchanged($Before,$After) {
    foreach ($property in @('commit','modified','files','sha256')) {
        if ($Before.$property -cne $After.$property) {throw 'Source changed during installer compilation.'}
    }
}
function Invoke-InstallerTool([string]$File,[string[]]$Arguments,[string]$Log,[int]$Seconds=60) {
    $start=[Diagnostics.ProcessStartInfo]::new($File)
    $start.UseShellExecute=$false; $start.CreateNoWindow=$true; $start.WorkingDirectory=$repository
    $start.RedirectStandardOutput=$true; $start.RedirectStandardError=$true
    foreach ($argument in $Arguments) {$start.ArgumentList.Add($argument)}
    $process=[Diagnostics.Process]::new(); $process.StartInfo=$start
    $out=[IO.File]::Create($Log+'.stdout.log'); $err=[IO.File]::Create($Log+'.stderr.log')
    try {
        [void]$process.Start()
        $outCopy=$process.StandardOutput.BaseStream.CopyToAsync($out); $errCopy=$process.StandardError.BaseStream.CopyToAsync($err)
        $timedOut=-not $process.WaitForExit($Seconds*1000)
        if ($timedOut) {$process.Kill($true); $process.WaitForExit()}
        [void]$outCopy.GetAwaiter().GetResult(); [void]$errCopy.GetAwaiter().GetResult()
        if ($timedOut) {throw 'Offline verification tool timed out.'}
        if ($process.ExitCode -ne 0) {throw ('Offline verification tool failed: '+$process.ExitCode)}
    } finally {$out.Dispose();$err.Dispose();$process.Dispose()}
}
function Initialize-InstallerReader {
    if ('DacInstallerReader' -as [type]) {return}
    Add-Type -TypeDefinition @'
using System;
using System.Collections.Generic;
using System.Text;
public sealed class DacInstallerRecord {
    public string[] Strings;
    public byte[] Tail;
}
public sealed class DacInstallerCursor {
    public readonly byte[] Bytes; public int At;
    public DacInstallerCursor(byte[] bytes) {Bytes=bytes;}
    public byte[] Take(int count) {DacInstallerReader.Range(Bytes,At,count);var b=new byte[count];Array.Copy(Bytes,At,b,0,count);At+=count;return b;}
    public DacInstallerRecord Record(int strings,int ansi,int tail) {
        var names=new List<string>();
        for(int i=0;i<strings+ansi;i++) {
            uint length=BitConverter.ToUInt32(Take(4),0);
            if(length>4*1024*1024 || (i<strings && length%2!=0)) throw new Exception("Record length rejected.");
            byte[] value=Take((int)length);
            names.Add(i<strings ? new UnicodeEncoding(false,false,true).GetString(value) : "");
        }
        return new DacInstallerRecord {Strings=names.ToArray(),Tail=Take(tail)};
    }
}
public static class DacInstallerReader {
    public static void Range(byte[] b,long at,long size) {if(at<0 || size<0 || at>b.LongLength-size) throw new Exception("Invalid installer range.");}
    public static uint Crc(byte[] b,int at,int size) {
        Range(b,at,size);uint crc=0xffffffff;
        for(int i=at;i<at+size;i++) {crc^=b[i];for(int n=0;n<8;n++) crc=(crc>>1)^((crc&1)!=0 ? 0xedb88320u : 0u);}
        return ~crc;
    }
    public static byte[] Part(byte[] b,long at,long size) {Range(b,at,size);var r=new byte[(int)size];Array.Copy(b,at,r,0,size);return r;}
    public static int Table(byte[] b) {
        byte[] id={114,68,108,80,116,83,205,230,215,123,11,42};int found=-1;
        for(int i=0;i<=b.Length-64;i++) {
            bool ok=true;for(int n=0;n<12;n++) if(b[i+n]!=id[n]) {ok=false;break;}
            if(!ok) continue;
            if(found!=-1) throw new Exception("Duplicate offset table.");found=i;
        }
        if(found<0 || BitConverter.ToUInt32(b,found+12)!=2 || Crc(b,found,60)!=BitConverter.ToUInt32(b,found+60)) throw new Exception("Invalid offset table.");
        // The only offset table must be the actual RCDATA/11111 PE resource,
        // not an attacker-supplied signature elsewhere in the file.
        uint pe=U32(b,60);Range(b,pe,24);uint optional=pe+24;
        if(U32(b,pe)!=0x4550 || U16(b,pe+4)!=0x8664 || U16(b,optional)!=0x20b) throw new Exception("Invalid loader PE.");
        int count=U16(b,pe+6);uint sections=optional+U16(b,pe+20);
        if(count<1 || count>64) throw new Exception("Invalid section count.");
        uint rva=U32(b,optional+128), resourceSize=U32(b,optional+132);
        if(resourceSize<16 || resourceSize>16*1024*1024) throw new Exception("Invalid resource size.");
        long resource=Map(b,rva,resourceSize,sections,count);
        long type=Child(b,resource,resourceSize,resource,10,true);
        long name=Child(b,resource,resourceSize,type,11111,true);
        Range(b,name,16);int languages=U16(b,(uint)name+12)+U16(b,(uint)name+14);
        if(languages!=1) throw new Exception("Unexpected offset table languages.");
        uint data=U32(b,(uint)name+20);
        if((data&0x80000000)!=0 || data>resourceSize-16) throw new Exception("Invalid offset table resource.");
        long entry=resource+data;
        long table=Map(b,U32(b,(uint)entry),U32(b,(uint)entry+4),sections,count);
        if(table!=found || U32(b,(uint)entry+4)!=64 || U32(b,(uint)found+56)!=0) throw new Exception("Offset table resource mismatch.");
        return found;
    }
    // Resolve the actual numeric PE resource path; never search for its text.
    public static long[] Resource(byte[] b,uint typeId,uint nameId,uint languageId) {
        uint pe=U32(b,60);Range(b,pe,24);uint optional=pe+24;
        if(U32(b,pe)!=0x4550 || U16(b,pe+4)!=0x8664 || U16(b,optional)!=0x20b || U16(b,pe+20)<136) throw new Exception("Invalid runtime PE.");
        Range(b,optional,U16(b,pe+20));int count=U16(b,pe+6);uint sections=optional+U16(b,pe+20);
        if(count<1 || count>64) throw new Exception("Invalid runtime sections.");
        uint rva=U32(b,optional+128),size=U32(b,optional+132);
        if(size<16 || size>16*1024*1024) throw new Exception("Invalid runtime resource size.");
        long root=Map(b,rva,size,sections,count);
        long type=Child(b,root,size,root,typeId,true);
        long name=Child(b,root,size,type,nameId,true);
        if(U16(b,(uint)name+12)+U16(b,(uint)name+14)!=1) throw new Exception("Unexpected runtime resource languages.");
        long entry=Child(b,root,size,name,languageId,false);
        uint bytes=U32(b,(uint)entry+4);long start=Map(b,U32(b,(uint)entry),bytes,sections,count);
        return new long[] {start,bytes,entry};
    }
    static ushort U16(byte[] b,uint at) {Range(b,at,2);return BitConverter.ToUInt16(b,(int)at);}
    static uint U32(byte[] b,uint at) {Range(b,at,4);return BitConverter.ToUInt32(b,(int)at);}
    static long Map(byte[] b,uint rva,uint size,uint sections,int count) {
        for(int i=0;i<count;i++) {
            uint at=sections+(uint)(40*i);Range(b,at,40);uint va=U32(b,at+12),raw=U32(b,at+20),length=U32(b,at+16);
            if(rva>=va && (long)rva+size<=(long)va+length) {long result=(long)raw+rva-va;Range(b,result,size);return result;}
        }
        throw new Exception("Unmapped resource.");
    }
    static long Child(byte[] b,long root,uint size,long directory,uint id,bool subdirectory) {
        if(directory<root || directory>root+size-16) throw new Exception("Invalid resource directory.");
        int count=U16(b,(uint)directory+12)+U16(b,(uint)directory+14);long found=-1;
        if(count>1024 || directory+16+8L*count>root+size) throw new Exception("Invalid resource count.");
        for(int i=0;i<count;i++) {
            uint at=(uint)(directory+16+8*i);
            if(U32(b,at)!=id) continue;
            uint target=U32(b,at+4);
            if(found!=-1 || ((target&0x80000000)!=0)!=subdirectory || (target&0x7fffffff)>size-16) throw new Exception("Unexpected resource entry.");
            found=root+(target&0x7fffffff);
        }
        if(found<0) throw new Exception("Missing resource.");return found;
    }
    public static byte[] Block(byte[] b,ref long at) {
        Range(b,at,13);int start=(int)at;
        if(Crc(b,start+4,9)!=BitConverter.ToUInt32(b,start)) throw new Exception("Block header CRC mismatch.");
        long stored=BitConverter.ToInt64(b,start+4);
        if(stored<5 || stored>32*1024*1024 || b[start+12]!=1) throw new Exception("Unsupported block.");
        at+=13;Range(b,at,stored);var result=new List<byte>();
        while(stored>0) {
            if(stored<5) throw new Exception("Truncated CRC block.");
            int count=(int)Math.Min(4096,stored-4);
            if(Crc(b,(int)at+4,count)!=BitConverter.ToUInt32(b,(int)at)) throw new Exception("Data CRC mismatch.");
            result.AddRange(Part(b,at+4,count));at+=count+4;stored-=count+4;
        }
        return result.ToArray();
    }
    // Inno Compression.Base TransformCallInstructions version 3, decode only.
    public static void Restore(byte[] b,int blockSize) {
        for(int offset=0;offset<b.Length;offset+=blockSize) {
            int size=Math.Min(blockSize,b.Length-offset);
            for(int i=0;i<size-4;) {
                if(b[offset+i]!=0xe8 && b[offset+i]!=0xe9) {i++;continue;}
                i++;int at=offset+i;
                if(b[at+3]==0 || b[at+3]==255) {
                    uint rel=(uint)(b[at]|b[at+1]<<8|b[at+2]<<16);
                    rel=unchecked(rel-(uint)((offset+i+4)&0xffffff));
                    if((rel&0x800000)!=0) b[at+3]=(byte)~b[at+3];
                    b[at]=(byte)rel;b[at+1]=(byte)(rel>>8);b[at+2]=(byte)(rel>>16);
                }
                i+=4;
            }
        }
    }
}
'@
}
function Read-InstallerEmbeddedContent([string]$Installer) {
    Initialize-InstallerReader
    $root=Join-Path $repository ('out/private/installer-read/'+[Guid]::NewGuid().ToString('N'))
    [void][IO.Directory]::CreateDirectory($root)
    # Bootstrap verifies archive, signer, full inventory, compiler and decoder DLL.
    $null=& (Join-Path $PSScriptRoot 'bootstrap-installer.ps1') -Offline -VerifyOnly
    . (Join-Path $PSScriptRoot 'use-toolchain.ps1')
    $decoder=Join-Path $root 'decoder.exe'
    Invoke-InstallerTool 'cl.exe' @('/nologo','/std:c++20','/MT','/W4','/WX','/sdl','/guard:cf','/EHsc','/utf-8',
        (Join-Path $PSScriptRoot 'installer-decode.cpp'),('/Fe'+$decoder),('/Fo'+(Join-Path $root 'decoder.obj')),
        '/link','ole32.lib','/DYNAMICBASE','/NXCOMPAT','/HIGHENTROPYVA','/guard:cf') (Join-Path $root 'compile') 60
    function Decode([byte[]]$Data,[int]$Method) {
        $name=Join-Path $root ([Guid]::NewGuid().ToString('N'))
        [IO.File]::WriteAllBytes($name+'.compressed',$Data)
        Invoke-InstallerTool $decoder @((Join-Path $repository '.tools/inno-7.1.0/is7zxr-x64.dll'),($name+'.compressed'),($name+'.decoded'),([string]$Method)) $name 30
        if ((Get-Item -LiteralPath ($name+'.decoded')).Length -gt 64MB) {throw 'Decoder output oversized.'}
        return ,[IO.File]::ReadAllBytes($name+'.decoded')
    }
    if ((Get-Item -LiteralPath $Installer).Length -gt 32MB) {throw 'Installer oversized.'}
    $bytes=[IO.File]::ReadAllBytes($Installer); $table=[DacInstallerReader]::Table($bytes)
    $total=[BitConverter]::ToInt64($bytes,$table+16); $runtimeAt=[BitConverter]::ToInt64($bytes,$table+24)
    $runtimeSize=[BitConverter]::ToUInt32($bytes,$table+32); $runtimeCrc=[BitConverter]::ToUInt32($bytes,$table+36)
    $metadataAt=[BitConverter]::ToInt64($bytes,$table+40); $payloadAt=[BitConverter]::ToInt64($bytes,$table+48)
    if ($total -ne $bytes.Length -or $payloadAt -lt 72 -or $payloadAt -ge $metadataAt -or $metadataAt -ge $runtimeAt -or $runtimeAt -ge $total -or $runtimeSize -gt 16MB) {throw 'Unsupported installer layout.'}
    if ([Text.Encoding]::ASCII.GetString($bytes,[int]$metadataAt,31) -cne 'Inno Setup Setup Data (7.0.0.3)') {throw 'Unsupported installer data version.'}
    # 64-byte SetupID, CRC32 + 49-byte packed encryption header (must euNone).
    if ([DacInstallerReader]::Crc($bytes,[int]$metadataAt+68,49) -ne [BitConverter]::ToUInt32($bytes,[int]$metadataAt+64) -or $bytes[$metadataAt+68] -ne 0) {throw 'Encrypted or invalid installer metadata.'}
    [long]$at=$metadataAt+117
    $metadata=Decode ([DacInstallerReader]::Block($bytes,[ref]$at)) 1
    $locations=Decode ([DacInstallerReader]::Block($bytes,[ref]$at)) 1
    if ($metadata.Length -gt 16MB -or $locations.Length -gt 1MB) {throw 'Decoded metadata oversized.'}
    if ($at -ne $runtimeAt) {throw 'Unscanned metadata bytes.'}
    $runtime=Decode ([DacInstallerReader]::Block($bytes,[ref]$at)) 1
    [DacInstallerReader]::Restore($runtime,[Math]::Max(5,$runtime.Length))
    if ($at -ne $total -or $runtime.Length -ne $runtimeSize -or [DacInstallerReader]::Crc($runtime,0,$runtime.Length) -ne $runtimeCrc) {throw 'Runtime decoding incomplete.'}
    $cursor=[DacInstallerCursor]::new($metadata)
    $header=$cursor.Record(39,4,166)
    $counts=@(0..16 | ForEach-Object {[BitConverter]::ToInt32($header.Tail,4*$_)})
    if (@($counts | Where-Object {$_ -lt 0 -or $_ -gt 1024}).Count -ne 0 -or $counts[8] -ne 20 -or $counts[9] -ne 19 -or $counts[15] -ne 0 -or $counts[16] -ne 0 -or $locations.Length -ne 19*89) {throw 'Unexpected compiled file/run inventory.'}
    # Packed non-string tails from the pinned Shared.Struct definitions.
    $schemas=@(@(4,4,19),@(2,0,4),@(0,1,0),@(4,0,30),@(5,0,39),@(6,0,23),@(7,0,27),@(3,0,0))
    for ($group=0;$group -lt 8;$group++) {for ($n=0;$n -lt $counts[$group];$n++) {$null=$cursor.Record($schemas[$group][0],$schemas[$group][1],$schemas[$group][2])}}
    $entries=@(for ($n=0;$n -lt $counts[8];$n++) {$cursor.Record(15,1,81)})
    $payloads=[Collections.Generic.List[object]]::new(); $chunks=@{}; [long]$expanded=0; [long]$decodedTotal=0
    if ($entries[0].Strings[1] -cne '' -or $entries[0].Tail[80] -ne 1 -or [BitConverter]::ToInt32($entries[0].Tail,53) -ne -1) {throw 'Unexpected generated uninstaller entry.'}
    for ($n=0;$n -lt 19;$n++) {
        $entry=$entries[$n+1];$index=[BitConverter]::ToInt32($entry.Tail,53)
        if ($index -ne $n) {throw 'Noncanonical payload location.'}
        $loc=[DacInstallerReader]::Part($locations,89*$n,89)
        $start=[BitConverter]::ToInt64($loc,8);$sub=[BitConverter]::ToInt64($loc,16);$size=[BitConverter]::ToInt64($loc,24);$compressed=[BitConverter]::ToInt64($loc,32);$flags=$loc[88]
        if ([BitConverter]::ToInt32($loc,0) -ne 0 -or [BitConverter]::ToInt32($loc,4) -ne 0 -or $size -lt 0 -or $size -gt 16MB -or $sub -lt 0 -or $sub -gt 64MB -or ($flags -band 24) -ne 16 -or $compressed -lt 6 -or $compressed -gt 32MB) {throw 'Unsupported payload record.'}
        $chunkAt=$payloadAt+$start
        if ($chunkAt -lt $payloadAt -or $chunkAt+4+$compressed -gt $metadataAt -or [Text.Encoding]::ASCII.GetString($bytes,[int]$chunkAt,4) -cne ('zlb'+[char]26)) {throw 'Invalid payload chunk.'}
        $key=[string]$chunkAt
        if (-not $chunks.ContainsKey($key)) {
            $chunk=Decode ([DacInstallerReader]::Part($bytes,$chunkAt+4,$compressed)) 2
            $decodedTotal+=$chunk.Length
            if ($decodedTotal -gt 64MB) {throw 'Total decoded chunks oversized.'}
            $chunks[$key]=$chunk
        }
        $data=[DacInstallerReader]::Part($chunks[$key],$sub,$size);$expanded+=$size
        if ($expanded -gt 64MB) {throw 'Expanded payload oversized.'}
        if ($flags -band 4) {[DacInstallerReader]::Restore($data,65536)}
        $hash=[Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($data)).ToLowerInvariant()
        if ($hash -cne [Convert]::ToHexString([DacInstallerReader]::Part($loc,40,32)).ToLowerInvariant()) {throw 'Decoded payload SHA256 mismatch.'}
        $payloads.Add([pscustomobject]@{destination=$entry.Strings[1];fileType=$entry.Tail[80];bytes=$data;sha256=$hash;chunk=$chunkAt;sub=$sub;compressed=$compressed})
    }
    # Every chunk and decompressed byte must belong to the complete payload set.
    [long]$next=$payloadAt
    foreach ($group in ($payloads | Group-Object chunk | Sort-Object {[long]$_.Name})) {
        $members=@($group.Group | Sort-Object sub);[long]$end=0
        if ([long]$group.Name -ne $next) {throw 'Unaccounted payload gap.'}
        foreach ($member in $members) {if ($member.sub -ne $end -or $member.compressed -ne $members[0].compressed) {throw 'Overlapping or inconsistent payload.'};$end+=$member.bytes.Length}
        if ($end -ne $chunks[$group.Name].Length) {throw 'Undeclared decoded payload bytes.'}
        $next+=$members[0].compressed+4
    }
    if ($next -ne $metadataAt) {throw 'Unscanned payload bytes.'}
    [pscustomobject]@{metadata=$metadata;locations=$locations;runtime=$runtime;payloads=@($payloads);privateRoot=$root;complete=$true}
}
