#Requires -Version 7.0
[CmdletBinding()]
param([Parameter(Mandatory)][string]$Installer,[Parameter(Mandatory)][string]$Archive,[string]$InstalledDirectory='',[switch]$RequireShipping)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$repository=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
. (Join-Path $PSScriptRoot 'Package.Common.ps1')
. (Join-Path $PSScriptRoot 'Installer.Common.ps1')
$exe=Resolve-PackageLocalPath $repository $Installer
$input=& (Join-Path $PSScriptRoot 'verify-package.ps1') -Archive (Resolve-PackageLocalPath $repository $Archive) | ConvertFrom-Json
$receipt=Get-Content -LiteralPath ($exe+'.verification.json') -Raw | ConvertFrom-Json
$keys=@($receipt.PSObject.Properties.Name | Sort-Object)
$expectedKeys=@('formatVersion','sourceCommit','workingTreeModified','sourceTreeSha256','sourceFiles','fixture','fixtureId',
    'installer','installerSha256','installerBytes','archiveSha256','executableSha256','compilerSha256','compilerVersion',
    'scriptSha256','includeSha256','files','productExecuted','signed') | Sort-Object
if (($keys -join '|') -cne ($expectedKeys -join '|')) {throw 'Incomplete or unexpected installer receipt fields.'}
foreach ($field in @('workingTreeModified','fixture','productExecuted','signed')) {
    if ($receipt.$field -isnot [bool]) {throw 'Installer receipt boolean is not JSON boolean.'}
}
foreach ($field in @('sourceFiles','installerBytes','formatVersion')) {
    if ($receipt.$field -isnot [long] -and $receipt.$field -isnot [int]) {throw 'Installer receipt integer is not JSON integer.'}
}
foreach ($file in $receipt.files) {
    if ((@($file.PSObject.Properties.Name | Sort-Object) -join '|') -cne 'bytes|path|sha256' -or
        $file.bytes -isnot [long] -and $file.bytes -isnot [int]) {throw 'Incomplete payload receipt fields.'}
}
$source=Get-InstallerSourceIdentity
if ($receipt.sourceCommit -cne $source.commit -or $receipt.sourceTreeSha256 -cne $source.sha256 -or
    $receipt.sourceFiles -ne $source.files -or $receipt.workingTreeModified -ne $source.modified) {throw 'Installer source provenance mismatch.'}
if ($RequireShipping -and ($receipt.fixture -ne $false -or $receipt.fixtureId -cne '' -or
    $receipt.installer -cne 'DAC-Setup-0.1.0-dev-windows-x64.exe' -or [IO.Path]::GetFileName($exe) -cne $receipt.installer)) {throw 'Shipping installer required.'}
if ((Get-Item -LiteralPath $exe).Length -lt 128 -or (Get-Item -LiteralPath $exe).Length -gt 32MB) {throw 'Invalid installer size.'}
$compiler=& (Join-Path $PSScriptRoot 'bootstrap-installer.ps1') -Offline -VerifyOnly | ConvertFrom-Json
if ($compiler.verification -cne 'passed' -or $compiler.inventoryFiles -ne 132) {throw 'Compiler provenance incomplete.'}
$archivePath=Resolve-PackageLocalPath $repository $Archive
if ([IO.File]::ReadAllText($archivePath+'.sha256') -cne ($input.archiveSha256+'  '+[IO.Path]::GetFileName($archivePath)+"`n")) {throw 'Input package checksum receipt mismatch.'}
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
# Recompute the exact include from verified staged bytes; a changed include/hash
# pair, wildcard, external file or compiler flag cannot be authorized by a receipt.
$output=Split-Path $exe -Parent; $lines=@()
foreach ($file in $spec.files) {
    $staged=Resolve-PackageLocalPath $repository (Join-Path $output ('input/'+$spec.archiveRoot+'/'+$file.path))
    $directory=[IO.Path]::GetDirectoryName($file.path).Replace('/','\')
    $target=if ($directory) {'{app}\'+$directory} else {'{app}'}
    $lines+='Source: "'+$staged+'"; DestDir: "'+$target+'"; Flags: ignoreversion'
}
if ([IO.File]::ReadAllText((Join-Path $output 'payload.iss')) -cne (($lines -join "`n")+"`n")) {throw 'Noncanonical compiler input include.'}
$embedded=Read-InstallerEmbeddedContent $exe
if (-not $embedded.complete -or $embedded.payloads.Count -ne 19) {throw 'Embedded payload verification incomplete.'}
$destinations=@($embedded.payloads.destination | Sort-Object)
$expectedDestinations=@($spec.files.path | ForEach-Object {'{app}\'+$_.Replace('/','\')} | Sort-Object)
if (($destinations -join '|') -cne ($expectedDestinations -join '|') -or @($embedded.payloads | Where-Object {$_.fileType -ne 0}).Count) {throw 'Compiled destination/file inventory mismatch.'}
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
        $destination='{app}\'+$file.path.Replace('/','\')
        $decoded=@($embedded.payloads | Where-Object {$_.destination -ceq $destination})
        $staged=Join-Path $output ('input/'+$spec.archiveRoot+'/'+$file.path)
        if ($decoded.Count -ne 1 -or $decoded[0].sha256 -cne $digest -or $decoded[0].bytes.Length -ne $entry.Length -or
            (Get-FileHash -LiteralPath $staged).Hash.ToLowerInvariant() -cne $digest -or (Get-Item -LiteralPath $staged).Length -ne $entry.Length) {throw 'Compiled or staged payload differs from verified package.'}
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
[ordered]@{installer=[IO.Path]::GetFileName($exe);sha256=$hash;packageSha256=$input.archiveSha256;payloadFiles=$spec.files.Count;installedPayloadChecked=[bool]$InstalledDirectory;fixture=$receipt.fixture;sourceCommit=$source.commit;sourceTreeSha256=$source.sha256;sourceFiles=$source.files;workingTreeModified=$source.modified;compilerSha256=$compiler.compilerSha256;embeddedPayloadChecked=$true;embeddedPayloadFiles=19;unsigned=$true;productExecuted=$false;verification='passed'} | ConvertTo-Json
