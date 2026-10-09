#Requires -Version 7.0
[CmdletBinding()]
param(
    [string]$Archive = '',
    [string]$Receipt = ''
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repository = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
. (Join-Path $PSScriptRoot 'Package.Common.ps1')
$spec = Get-PackageSpecification
$expectedFiles = [Collections.Generic.Dictionary[string,object]]::new([StringComparer]::Ordinal)
foreach ($file in $spec.files) { $expectedFiles.Add($file.path, $file) }
if (-not $Archive) { $Archive = Join-Path $repository 'packages/DAC-0.1.0-dev-windows-x64.zip' }
$archivePath = (Resolve-Path -LiteralPath $Archive).Path
$archiveFile = Get-Item -LiteralPath $archivePath
if ($archiveFile.Length -gt 32MB) { throw 'Archive exceeds the 32 MiB verification limit.' }
$prefix = $spec.archiveRoot + '/'

function Assert-SafeName([string]$Name) { Assert-PackageName $Name }
function Read-EntryBytes($Entry) {
    $source = $Entry.Open()
    $memory = [IO.MemoryStream]::new()
    $buffer = [byte[]]::new(65536)
    try {
        while (($count = $source.Read($buffer, 0, $buffer.Length)) -gt 0) {
            if ($memory.Length + $count -gt $Entry.Length -or $memory.Length + $count -gt 16MB) {
                throw 'Entry decompressed beyond its declared or permitted size.'
            }
            $memory.Write($buffer, 0, $count)
        }
        if ($memory.Length -ne $Entry.Length) { throw 'Entry decompressed length does not match metadata.' }
        return ,$memory.ToArray()
    } finally { $source.Dispose(); $memory.Dispose() }
}
function Hash-Entry($Entry) {
    $sha = [Security.Cryptography.SHA256]::Create()
    try { return [Convert]::ToHexString($sha.ComputeHash((Read-EntryBytes $Entry))).ToLowerInvariant() }
    finally { $sha.Dispose() }
}
$utf8 = [Text.UTF8Encoding]::new($false, $true)
$zip = [IO.Compression.ZipFile]::OpenRead($archivePath)
try {
    if ($zip.Entries.Count -gt 2048) { throw 'Archive contains too many entries.' }
    $files = [Collections.Generic.Dictionary[string,object]]::new([StringComparer]::OrdinalIgnoreCase)
    [long]$payloadBytes = 0
    foreach ($entry in $zip.Entries) {
        Assert-SafeName $entry.FullName
        if (-not $entry.FullName.StartsWith($prefix, [StringComparison]::Ordinal)) { throw 'Archive must contain one exact DAC version root.' }
        $name = $entry.FullName.Substring($prefix.Length)
        if (-not $expectedFiles.ContainsKey($name)) { throw "File is not in the explicit package allowlist: $name" }
        if (-not $files.TryAdd($name, $entry)) { throw "Duplicate or case-colliding archive name: $name" }
        if ($entry.Length -gt 16MB) { throw "Entry exceeds the 16 MiB verification limit: $name" }
        $payloadBytes += $entry.Length
        if ($payloadBytes -gt 64MB) { throw 'Expanded archive exceeds the 64 MiB verification limit.' }
        # Only the declared native executable may be binary.
        if ($name -cne 'DAC.exe') {
            $bytes = Read-EntryBytes $entry
            if ($bytes.Length -ge 2 -and $bytes[0] -eq 0x4d -and $bytes[1] -eq 0x5a) { throw "Undeclared PE binary: $name" }
            [void]$utf8.GetString($bytes)
        }
    }
    foreach ($name in $expectedFiles.Keys) { if (-not $files.ContainsKey($name)) { throw "Required payload missing: $name" } }
    if ($files.Count -ne $expectedFiles.Count) { throw 'Archive does not match the explicit package allowlist.' }
    if ($files['SHA256SUMS.txt'].Length -gt 1MB -or $files['PE-VERIFICATION.json'].Length -gt 1MB) { throw 'Verification metadata is oversized.' }
    $manifest = $utf8.GetString((Read-EntryBytes $files['SHA256SUMS.txt']))
    $listed = [Collections.Generic.Dictionary[string,string]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach ($line in $manifest -split '\r?\n') {
        if ($line.Length -eq 0) { continue }
        if ($line -cnotmatch '^([0-9a-f]{64})  (.+)$') { throw 'Malformed SHA256SUMS line.' }
        $expected = $Matches[1]; $name = $Matches[2]
        Assert-SafeName $name
        if ($name -ceq 'SHA256SUMS.txt' -or -not $listed.TryAdd($name, $expected)) { throw "Invalid manifest duplicate/self-reference: $name" }
        if (-not $files.ContainsKey($name) -or $files[$name].FullName -cne ($prefix + $name)) { throw "Manifest file missing or differently cased: $name" }
        if ((Hash-Entry $files[$name]) -cne $expected) { throw "Archive payload SHA-256 mismatch: $name" }
    }
    if ($listed.Count -ne $files.Count - 1) { throw 'Archive and checksum manifest file sets differ.' }
    foreach ($name in $files.Keys) { if ($name -cne 'SHA256SUMS.txt' -and -not $listed.ContainsKey($name)) { throw "Unlisted archive payload: $name" } }

    $peReceipt = $utf8.GetString((Read-EntryBytes $files['PE-VERIFICATION.json'])) | ConvertFrom-Json
    [string[]]$receiptKeys = @($peReceipt.PSObject.Properties.Name)
    [string[]]$allowedKeys = @('architecture','subsystem','ASLR','DEP','highEntropyVA','CFG','imports','version','sha256','bytes')
    [Array]::Sort($receiptKeys, [StringComparer]::Ordinal); [Array]::Sort($allowedKeys, [StringComparer]::Ordinal)
    if (($receiptKeys -join '|') -cne ($allowedKeys -join '|')) { throw 'PE receipt has unexpected or missing fields.' }
    $peBytes = Read-EntryBytes $files['DAC.exe']
    $exeHash = Hash-Entry $files['DAC.exe']
    if ($peReceipt.sha256 -cne $exeHash -or [long]$peReceipt.bytes -ne $peBytes.Length) { throw 'Embedded PE receipt does not match DAC.exe bytes.' }

    # Read the PE and VERSIONINFO directly from ZIP bytes, with no extraction/execution.
    function Assert-PeRange([long]$Offset, [long]$Count) {
        if ($Offset -lt 0 -or $Count -lt 0 -or $Offset -gt $peBytes.Length - $Count) { throw 'Invalid/truncated PE range.' }
    }
    function Read-U16([long]$Offset) { Assert-PeRange $Offset 2; [BitConverter]::ToUInt16($peBytes, [int]$Offset) }
    function Read-U32([long]$Offset) { Assert-PeRange $Offset 4; [BitConverter]::ToUInt32($peBytes, [int]$Offset) }
    if ((Read-U16 0) -ne 0x5a4d) { throw 'DAC.exe is not MZ.' }
    $peOffset = Read-U32 0x3c
    if ((Read-U32 $peOffset) -ne 0x4550 -or (Read-U16 ($peOffset+4)) -ne 0x8664) { throw 'DAC.exe is not AMD64 PE.' }
    $optional = $peOffset + 24
    if ((Read-U16 $optional) -ne 0x20b -or (Read-U16 ($optional+68)) -ne 2) { throw 'DAC.exe is not a PE32+ Windows GUI executable.' }
    if ($peReceipt.architecture -cne 'AMD64' -or $peReceipt.subsystem -cne 'Windows GUI') { throw 'PE architecture/subsystem receipt mismatch.' }
    $flags = Read-U16 ($optional+70)
    foreach ($pair in @{ ASLR=0x40; DEP=0x100; highEntropyVA=0x20; CFG=0x4000 }.GetEnumerator()) {
        if (($flags -band $pair.Value) -eq 0 -or $peReceipt.($pair.Key) -ne $true) { throw "PE hardening receipt mismatch: $($pair.Key)" }
    }
    $sections = @()
    $sectionStart = $optional + (Read-U16 ($peOffset+20))
    for ($i=0; $i -lt (Read-U16 ($peOffset+6)); $i++) {
        $at = $sectionStart + 40*$i
        $sections += @{ va=(Read-U32 ($at+12)); size=(Read-U32 ($at+16)); raw=(Read-U32 ($at+20)) }
    }
    function Map-Rva([uint32]$Rva, [uint32]$Count=1) {
        foreach ($section in $sections) {
            if ($Rva -ge $section.va -and ([long]$Rva+$Count) -le ([long]$section.va+$section.size)) {
                $offset = [long]$section.raw + $Rva - $section.va
                Assert-PeRange $offset $Count
                return $offset
            }
        }
        throw "Unmapped PE RVA: $Rva"
    }
    # Independently re-read imported DLL names instead of trusting the embedded receipt.
    $imports = [Collections.Generic.List[string]]::new()
    $importRva = Read-U32 ($optional+120)
    if ($importRva) {
        $terminated = $false
        for ($n=0; $n -lt 512; $n++) {
            $descriptor = Map-Rva ($importRva + 20*$n) 20
            $nameRva = Read-U32 ($descriptor+12)
            if (-not $nameRva) { $terminated = $true; break }
            $nameBytes = [Collections.Generic.List[byte]]::new()
            $nameEnded = $false
            for ($i=0; $i -lt 260; $i++) {
                $value = $peBytes[(Map-Rva ($nameRva+$i))]
                if ($value -eq 0) { $nameEnded = $true; break }
                if ($value -lt 0x21 -or $value -gt 0x7e) { throw 'Invalid PE import name.' }
                $nameBytes.Add($value)
            }
            if (-not $nameEnded) { throw 'Unterminated PE import name.' }
            $imports.Add([Text.Encoding]::ASCII.GetString($nameBytes.ToArray()))
        }
        if (-not $terminated) { throw 'Unterminated PE import table.' }
    }
    $system = @('ADVAPI32.dll','COMDLG32.dll','dwmapi.dll','Dxva2.dll','GDI32.dll','gdiplus.dll','KERNEL32.dll','ole32.dll','OLEAUT32.dll','POWRPROF.dll','SHELL32.dll','SHLWAPI.dll','USER32.dll','WTSAPI32.dll','COMCTL32.dll','bcrypt.dll','CRYPT32.dll','VERSION.dll','IMM32.dll','ntdll.dll')
    foreach ($dll in $imports) {
        if ($dll -notin $system -and $dll -notmatch '^api-ms-win-[a-z0-9-]+\.dll$') { throw 'Unexpected PE runtime dependency.' }
    }
    [string[]]$actualImports = @($imports | ForEach-Object { $_.ToUpperInvariant() })
    [string[]]$recordedImports = @($peReceipt.imports | ForEach-Object { $_.ToUpperInvariant() })
    [Array]::Sort($actualImports, [StringComparer]::Ordinal); [Array]::Sort($recordedImports, [StringComparer]::Ordinal)
    if (($actualImports -join '|') -cne ($recordedImports -join '|')) { throw 'PE import receipt mismatch.' }
    $resourceRva = Read-U32 ($optional+128)
    $resourceSize = Read-U32 ($optional+132)
    $resourceBase = Map-Rva $resourceRva $resourceSize
    function Resource-Children([long]$Directory) {
        if ($Directory -lt $resourceBase -or $Directory+16 -gt $resourceBase+$resourceSize) { throw 'Invalid PE resource directory.' }
        $count = (Read-U16 ($Directory+12)) + (Read-U16 ($Directory+14))
        if ($count -gt 1024 -or $Directory+16+8*$count -gt $resourceBase+$resourceSize) { throw 'Invalid PE resource entries.' }
        for ($n=0; $n -lt $count; $n++) {
            $name = Read-U32 ($Directory+16+8*$n); $target = Read-U32 ($Directory+20+8*$n)
            [pscustomobject]@{ Id=$name; Directory=(($target -band 0x80000000L) -ne 0); Offset=($resourceBase+($target -band 0x7fffffffL)) }
        }
    }
    $type = @(Resource-Children $resourceBase | Where-Object Id -eq 16)
    if ($type.Count -ne 1 -or -not $type[0].Directory) { throw 'Missing/unexpected VERSIONINFO resource type.' }
    $nameNodes = @(Resource-Children $type[0].Offset)
    if ($nameNodes.Count -ne 1 -or -not $nameNodes[0].Directory) { throw 'Unexpected VERSIONINFO name directory.' }
    $languages = @(Resource-Children $nameNodes[0].Offset)
    if ($languages.Count -ne 1 -or $languages[0].Directory) { throw 'Unexpected VERSIONINFO language resource.' }
    $versionOffset = Map-Rva (Read-U32 $languages[0].Offset) (Read-U32 ($languages[0].Offset+4))
    $versionEnd = $versionOffset + (Read-U32 ($languages[0].Offset+4))
    $versions = [Collections.Generic.List[string]]::new()
    function Visit-Version([long]$Offset, [long]$Limit, [int]$Depth=0) {
        if ($Depth -gt 6 -or $Offset+6 -gt $Limit) { throw 'Malformed VERSIONINFO tree.' }
        $length = Read-U16 $Offset; $valueLength = Read-U16 ($Offset+2); $type = Read-U16 ($Offset+4)
        if ($length -lt 6 -or $Offset+$length -gt $Limit) { throw 'Malformed VERSIONINFO node length.' }
        $end = $Offset+$length; $keyStart=$Offset+6; $cursor=$keyStart
        while ($cursor+2 -le $end -and (Read-U16 $cursor) -ne 0) { $cursor+=2 }
        if ($cursor+2 -gt $end) { throw 'Unterminated VERSIONINFO key.' }
        $key = [Text.Encoding]::Unicode.GetString($peBytes, [int]$keyStart, [int]($cursor-$keyStart))
        $valueStart = ($cursor+2+3) -band (-bnot 3L)
        $valueBytes = if ($type -eq 1) { 2*$valueLength } else { $valueLength }
        if ($valueStart+$valueBytes -gt $end) { throw 'Invalid VERSIONINFO value length.' }
        if ($key -ceq 'ProductVersion') {
            if ($type -ne 1) { throw 'ProductVersion is not text.' }
            $versions.Add([Text.Encoding]::Unicode.GetString($peBytes,[int]$valueStart,[int]$valueBytes).TrimEnd([char]0))
        }
        $child = ($valueStart+$valueBytes+3) -band (-bnot 3L)
        while ($child+6 -le $end) {
            $childLength = Read-U16 $child
            if ($childLength -eq 0) { throw 'Invalid empty VERSIONINFO child.' }
            Visit-Version $child $end ($Depth+1)
            $child = ($child+$childLength+3) -band (-bnot 3L)
        }
    }
    Visit-Version $versionOffset $versionEnd
    if ($versions.Count -ne 1 -or $versions[0] -cne '0.1.0-dev' -or $peReceipt.version -cne $versions[0]) { throw 'Executable/receipt product version mismatch.' }

    # Bound allocations above, then reproduce the normalized ZIP using ZIP payloads
    # alone. This is independent of the staging directory and its timestamps.
    $memory = [IO.MemoryStream]::new()
    $rebuilt = [IO.Compression.ZipArchive]::new($memory, [IO.Compression.ZipArchiveMode]::Create, $true)
    try {
        [string[]]$sortedNames = @($files.Keys)
        [Array]::Sort($sortedNames, [StringComparer]::Ordinal)
        foreach ($name in $sortedNames) {
            $entry = $files[$name]
            $copy = $rebuilt.CreateEntry($entry.FullName, [IO.Compression.CompressionLevel]::Optimal)
            $copy.LastWriteTime = [DateTimeOffset]::new(2026,10,8,0,0,0,[TimeSpan]::Zero)
            $data = Read-EntryBytes $entry
            $inputStream = [IO.MemoryStream]::new($data, $false); $outputStream = $copy.Open()
            try { Copy-PackageStream $inputStream $outputStream }
            finally { $inputStream.Dispose(); $outputStream.Dispose() }
        }
    } finally { $rebuilt.Dispose() }
    $memory.Position=0; $sha=[Security.Cryptography.SHA256]::Create()
    try { $rebuiltHash=[Convert]::ToHexString($sha.ComputeHash($memory)).ToLowerInvariant() }
    finally { $sha.Dispose(); $memory.Dispose() }
    $archiveHash=(Get-FileHash -LiteralPath $archivePath -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($rebuiltHash -cne $archiveHash) { throw 'Normalized ZIP reproduction is not byte-for-byte deterministic.' }
    $receiptObject=[ordered]@{
        archive=[IO.Path]::GetFileName($archivePath); archiveSha256=$archiveHash; archiveBytes=$archiveFile.Length
        entries=$files.Count; hashedPayloadFiles=$listed.Count; expandedBytes=$payloadBytes
        canonicalUniqueNames=$true; exactPackageAllowlist=$true; exactManifestFileSet=$true; allPayloadHashesMatch=$true
        executableFiles=@('DAC.exe'); sourceAndToolchainPayloadsAbsent=$true
        executableSha256=$exeHash; executableBytes=$peBytes.Length; productVersion=$versions[0]
        embeddedPeReceiptMatches=$true; systemImportsVerified=$true; normalizedZipRebuildSha256=$rebuiltHash; byteIdenticalZipRebuild=$true
        rebuildWriteBufferBytes=131072; verificationRuntime=[Runtime.InteropServices.RuntimeInformation]::FrameworkDescription
        productExecuted=$false; verification='passed'
    }
    $json=$receiptObject | ConvertTo-Json -Depth 4
} finally { $zip.Dispose() }

if ($Receipt) {
    $receiptPath = Resolve-PackageLocalPath $repository $Receipt
    $relativeReceipt = [IO.Path]::GetRelativePath($repository, $receiptPath).Replace('\','/')
    if ($relativeReceipt -notmatch '^(build|packages|out)/') { throw 'Delivery receipts must remain in ignored output directories.' }
    if (Test-Path -LiteralPath $receiptPath) { throw 'Refusing to overwrite an existing delivery receipt.' }
    $parent = Split-Path $receiptPath -Parent
    if (-not (Test-Path -LiteralPath $parent -PathType Container)) { throw 'Receipt directory must already exist.' }
    [IO.File]::WriteAllText($receiptPath,$json+"`n",[Text.UTF8Encoding]::new($false))
}
$json
