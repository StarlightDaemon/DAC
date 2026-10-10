#Requires -Version 7.0
[CmdletBinding()]
param([Parameter(Mandatory)][string]$Installer,[Parameter(Mandatory)][string]$Archive,[string]$InstalledDirectory='')
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$repository=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
. (Join-Path $PSScriptRoot 'Package.Common.ps1')
$exe=Resolve-PackageLocalPath $repository $Installer
$input=& (Join-Path $PSScriptRoot 'verify-package.ps1') -Archive (Resolve-PackageLocalPath $repository $Archive) | ConvertFrom-Json
$receipt=Get-Content -LiteralPath ($exe+'.verification.json') -Raw | ConvertFrom-Json
$hash=(Get-FileHash -LiteralPath $exe).Hash.ToLowerInvariant()
$expected=$hash+'  '+[IO.Path]::GetFileName($exe)+"`n"
$lock=Get-Content -LiteralPath (Join-Path $PSScriptRoot 'installer-toolchain-lock.json') -Raw | ConvertFrom-Json
if ($receipt.formatVersion -ne 1 -or $receipt.installerSha256 -cne $hash -or $receipt.installerBytes -ne (Get-Item -LiteralPath $exe).Length -or
    $receipt.archiveSha256 -cne $input.archiveSha256 -or $receipt.executableSha256 -cne $input.executableSha256 -or
    $receipt.compilerVersion -cne $lock.version -or $receipt.compilerSha256 -cne $lock.compilerSha256 -or
    $receipt.signed -ne $false -or $receipt.productExecuted -ne $false -or
    [IO.File]::ReadAllText($exe+'.sha256') -cne $expected) { throw 'Installer receipt identity mismatch.' }
if ((Get-FileHash -LiteralPath (Join-Path $repository 'installer/DAC.iss')).Hash.ToLowerInvariant() -cne $receipt.scriptSha256 -or
    (Get-FileHash -LiteralPath (Join-Path (Split-Path $exe -Parent) 'payload.iss')).Hash.ToLowerInvariant() -cne $receipt.includeSha256) { throw 'Installer source/include identity mismatch.' }
$bytes=[IO.File]::ReadAllBytes($exe)
if ($bytes.Length -gt 32MB -or [BitConverter]::ToUInt16($bytes,0) -ne 0x5a4d) {throw 'Invalid installer PE.'}
$offset=[BitConverter]::ToInt32($bytes,0x3c)
if ($offset -lt 64 -or $offset+96 -gt $bytes.Length -or [BitConverter]::ToUInt32($bytes,$offset) -ne 0x4550 -or
    [BitConverter]::ToUInt16($bytes,$offset+4) -ne 0x8664 -or
    [BitConverter]::ToUInt16($bytes,$offset+24) -ne 0x20b -or
    [BitConverter]::ToUInt16($bytes,$offset+24+68) -ne 2) {throw 'Installer must be AMD64 PE32+ GUI.'}
if ((Get-AuthenticodeSignature -LiteralPath $exe).Status -ne 'NotSigned') {throw 'Expected locally unsigned installer.'}
$spec=Get-PackageSpecification
if ((($receipt.files.path|Sort-Object) -join '|') -cne (($spec.files.path|Sort-Object) -join '|')) {throw 'Installer payload allowlist mismatch.'}
# Derive every payload identity independently from the verified input archive.
# A delivery receipt alone cannot authorize different installed bytes.
$zip=[IO.Compression.ZipFile]::OpenRead((Resolve-PackageLocalPath $repository $Archive))
try {
    foreach ($file in $receipt.files) {
        $entry=$zip.GetEntry($spec.archiveRoot+'/'+$file.path)
        $stream=$entry.Open();$sha=[Security.Cryptography.SHA256]::Create()
        try {$digest=[Convert]::ToHexString($sha.ComputeHash($stream)).ToLowerInvariant()}
        finally {$sha.Dispose();$stream.Dispose()}
        if ($file.bytes -ne $entry.Length -or $file.sha256 -cne $digest) {throw 'Installer payload receipt differs from input archive.'}
    }
} finally {$zip.Dispose()}
if ($InstalledDirectory) {
    $root=Resolve-PackageLocalPath $repository $InstalledDirectory
    $actual=@(Get-ChildItem -LiteralPath $root -File -Force -Recurse)
    $expectedFiles=@($spec.files.path)+@('.dac-lifecycle.lock','unins000.exe','unins000.dat')
    if ((@($actual | ForEach-Object {[IO.Path]::GetRelativePath($root,$_.FullName).Replace('\','/')} | Sort-Object) -join '|') -cne (($expectedFiles|Sort-Object) -join '|')) {throw 'Complete installed file set is unexpected.'}
    foreach ($file in $receipt.files) {
        $path=Resolve-PackageLocalPath $repository (Join-Path $root $file.path)
        if ((Get-Item -LiteralPath $path).Length -ne $file.bytes -or (Get-FileHash -LiteralPath $path).Hash.ToLowerInvariant() -cne $file.sha256) {throw 'Installed payload bytes differ.'}
    }
}
[ordered]@{installer=[IO.Path]::GetFileName($exe);sha256=$hash;packageSha256=$input.archiveSha256;payloadFiles=$spec.files.Count;installedPayloadChecked=[bool]$InstalledDirectory;fixture=$receipt.fixture;unsigned=$true;productExecuted=$false;verification='passed'} | ConvertTo-Json
