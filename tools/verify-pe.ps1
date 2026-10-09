param([Parameter(Mandatory)][string]$Executable)
$ErrorActionPreference='Stop'
$bytes=[IO.File]::ReadAllBytes((Resolve-Path -LiteralPath $Executable))
function U16([int]$offset) { [BitConverter]::ToUInt16($bytes,$offset) }
function U32([int]$offset) { [BitConverter]::ToUInt32($bytes,$offset) }
if((U16 0) -ne 0x5A4D){throw 'Not an MZ executable'}
$pe=U32 0x3c
if((U32 $pe) -ne 0x4550){throw 'Not a PE executable'}
if((U16 ($pe+4)) -ne 0x8664){throw 'Not AMD64'}
$optional=$pe+24
if((U16 $optional) -ne 0x20b){throw 'Not PE32+'}
if((U16 ($optional+68)) -ne 2){throw 'Shipping executable must use Windows GUI subsystem'}
$flags=U16 ($optional+70)
foreach($flag in @(0x20,0x40,0x100,0x4000)){if(($flags -band $flag) -eq 0){throw ('Missing PE hardening flag 0x{0:x}' -f $flag)}}
$sectionStart=$optional+(U16 ($pe+20));$sections=@()
for($i=0;$i -lt (U16 ($pe+6));$i++){
    $at=$sectionStart+40*$i
    $sections+=@{va=(U32 ($at+12));size=[Math]::Max((U32 ($at+8)),(U32 ($at+16)));raw=(U32 ($at+20))}
}
function Offset([uint32]$rva){
    foreach($section in $sections){if($rva -ge $section.va -and $rva -lt ($section.va+$section.size)){return [int]($section.raw+$rva-$section.va)}}
    throw "Unmapped PE RVA $rva"
}
$imports=@();$importRva=U32 ($optional+120)
if($importRva){
    $descriptor=Offset $importRva
    for($i=0;$i -lt 512;$i++){
        $nameRva=U32 ($descriptor+12)
        if(!$nameRva){break}
        $start=Offset $nameRva;$end=$start
        while($end -lt $bytes.Length -and $bytes[$end] -ne 0){$end++}
        $imports += [Text.Encoding]::ASCII.GetString($bytes,$start,$end-$start)
        $descriptor+=20
    }
}
# Explicit Windows system dependency allowlist. Static CRT forbids redistributable DLLs.
$system=@('ADVAPI32.dll','COMDLG32.dll','dwmapi.dll','Dxva2.dll','GDI32.dll','gdiplus.dll','KERNEL32.dll','ole32.dll','OLEAUT32.dll','POWRPROF.dll','SHELL32.dll','SHLWAPI.dll','USER32.dll','WTSAPI32.dll','COMCTL32.dll','bcrypt.dll','CRYPT32.dll','VERSION.dll','IMM32.dll','ntdll.dll')
foreach($dll in $imports){if($dll -notin $system -and $dll -notmatch '^api-ms-win-'){throw "Unexpected runtime dependency: $dll"}}
$version=(Get-Item -LiteralPath $Executable).VersionInfo
if($version.ProductVersion -notlike '0.1.0*'){throw 'Incorrect standalone product version'}
[ordered]@{architecture='AMD64';subsystem='Windows GUI';ASLR=$true;DEP=$true;highEntropyVA=$true;CFG=$true;imports=$imports;version=$version.ProductVersion;sha256=(Get-FileHash -LiteralPath $Executable -Algorithm SHA256).Hash.ToLowerInvariant();bytes=$bytes.Length} | ConvertTo-Json -Depth 4
