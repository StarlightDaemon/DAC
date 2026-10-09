#Requires -Version 7.0
[CmdletBinding()]
param([string]$Executable = 'build/release/DAC.exe')
. (Join-Path $PSScriptRoot 'Package.Common.ps1')
$repository = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$executablePath = (Resolve-Path -LiteralPath $Executable).Path
$spec = Get-PackageSpecification
$fixture = Resolve-PackageLocalPath $repository ('build/package-tests/' + [Guid]::NewGuid().ToString('N'))
$fixtureRepository = Join-Path $fixture 'repository'
$fixtureTools = Join-Path $fixtureRepository 'tools'
New-Item -ItemType Directory -Path $fixtureTools -Force | Out-Null
foreach ($name in @('Package.Common.ps1','package.ps1','verify-package.ps1','verify-pe.ps1','package-manifest.json')) {
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot $name) -Destination (Join-Path $fixtureTools $name)
}
$utf8 = [Text.UTF8Encoding]::new($false)
foreach ($file in $spec.files | Where-Object kind -eq 'repository') {
    $target = Join-Path $fixtureRepository $file.source
    New-Item -ItemType Directory -Path (Split-Path $target -Parent) -Force | Out-Null
    [IO.File]::WriteAllText($target, 'Public package test fixture: ' + $file.path + "`n", $utf8)
}
$build = Join-Path $fixtureRepository 'build/input'
New-Item -ItemType Directory -Path $build -Force | Out-Null
Copy-Item -LiteralPath $executablePath -Destination (Join-Path $build 'DAC.exe')
# Artificial canaries exercise extension matches, prior receipts, raw logs, and cache folders.
$canary = 'PRIVATE_FIXTURE_MUST_NEVER_SHIP'
foreach ($name in @('docs/PRIVATE.md','docs/private.json','evidence/raw.log','evidence/package-receipt.json',
                    '.tools/cache.json','packages/prior-receipt.json','receipts/delivery.json')) {
    $target = Join-Path $fixtureRepository $name
    New-Item -ItemType Directory -Path (Split-Path $target -Parent) -Force | Out-Null
    [IO.File]::WriteAllText($target, $canary, $utf8)
}
$package = Join-Path $fixtureTools 'package.ps1'
$verify = Join-Path $fixtureTools 'verify-package.ps1'
& $package -BuildDirectory 'build/input' -PackageDirectory 'build/first' | Out-Null
& $package -BuildDirectory 'build/input' -PackageDirectory 'build/second' | Out-Null
$first = Join-Path $fixtureRepository ('build/first/' + $spec.archiveRoot + '.zip')
$second = Join-Path $fixtureRepository ('build/second/' + $spec.archiveRoot + '.zip')
if ((Get-FileHash -LiteralPath $first).Hash -cne (Get-FileHash -LiteralPath $second).Hash) {
    throw 'Independent package stages were not byte-identical.'
}
$receipt = & $verify -Archive $first | ConvertFrom-Json
if (-not $receipt.exactPackageAllowlist -or $receipt.entries -ne $spec.files.Count) { throw 'Fixture package allowlist was not verified.' }
$prefix = $spec.archiveRoot + '/'
$zip = [IO.Compression.ZipFile]::OpenRead($first)
try {
    foreach ($entry in $zip.Entries) {
        if ($entry.FullName -ceq ($prefix + 'DAC.exe')) { continue }
        $reader = [IO.StreamReader]::new($entry.Open(), $utf8)
        try { if ($reader.ReadToEnd().Contains($canary)) { throw 'Private fixture content entered package.' } }
        finally { $reader.Dispose() }
    }
} finally { $zip.Dispose() }
Write-Output 'PASS: explicit source isolation and identical packages from separate stages'
$script:checks = 2

function Expect-PackageFailure([string]$Name, [string]$Message, [scriptblock]$Action) {
    $caught = $false
    try { & $Action | Out-Null }
    catch {
        if ($_.Exception.Message -notlike $Message) { throw "$Name failed unexpectedly: $($_.Exception.Message)" }
        $caught = $true
    }
    if (-not $caught) { throw "$Name unexpectedly passed." }
    $script:checks++
    Write-Output "PASS: $Name"
}
function New-ChangedArchive([string]$Name, [string]$Add = '', [string]$Omit = '', [string]$Change = '') {
    $output = Join-Path $fixture ($Name + '.zip')
    $inputZip = [IO.Compression.ZipFile]::OpenRead($first)
    $stream = [IO.File]::Open($output, [IO.FileMode]::CreateNew)
    $outputZip = [IO.Compression.ZipArchive]::new($stream, [IO.Compression.ZipArchiveMode]::Create, $false)
    try {
        foreach ($entry in $inputZip.Entries) {
            $relative = $entry.FullName.Substring($prefix.Length)
            if ($relative -ceq $Omit) { continue }
            $copy = $outputZip.CreateEntry($entry.FullName, [IO.Compression.CompressionLevel]::Optimal)
            $copy.LastWriteTime = [DateTimeOffset]::new(2026,10,8,0,0,0,[TimeSpan]::Zero)
            $destination = $copy.Open(); $source = $entry.Open()
            try {
                if ($relative -ceq $Change) {
                    $bytes = $utf8.GetBytes('tampered public document')
                    $destination.Write($bytes, 0, $bytes.Length)
                } else { Copy-PackageStream $source $destination }
            } finally { $source.Dispose(); $destination.Dispose() }
        }
        if ($Add) {
            $copy = $outputZip.CreateEntry($prefix + $Add)
            $copy.LastWriteTime = [DateTimeOffset]::new(2026,10,8,0,0,0,[TimeSpan]::Zero)
            $destination = $copy.Open()
            try { $bytes = $utf8.GetBytes($canary); $destination.Write($bytes, 0, $bytes.Length) }
            finally { $destination.Dispose() }
        }
    } finally { $outputZip.Dispose(); $stream.Dispose(); $inputZip.Dispose() }
    return $output
}
$added = New-ChangedArchive 'unexpected-document' -Add 'docs/PRIVATE.md'
Expect-PackageFailure 'reject an extra Markdown file' '*explicit package allowlist*' { & $verify -Archive $added }
$priorReceipt = New-ChangedArchive 'prior-receipt' -Add 'evidence/package-receipt.json'
Expect-PackageFailure 'reject an embedded prior delivery receipt' '*explicit package allowlist*' { & $verify -Archive $priorReceipt }
$missing = New-ChangedArchive 'missing-file' -Omit 'README.md'
Expect-PackageFailure 'reject an omitted allowed file' '*Required payload missing*' { & $verify -Archive $missing }
$changed = New-ChangedArchive 'changed-file' -Change 'README.md'
Expect-PackageFailure 'reject changed payload bytes' '*SHA-256 mismatch*' { & $verify -Archive $changed }
Expect-PackageFailure 'preserve an existing package stage' '*Refusing to overwrite*' {
    & $package -BuildDirectory 'build/input' -PackageDirectory 'build/first'
}
Write-Output "Package isolation checks passed: $script:checks. Product executed: false. Fixtures retained under build/package-tests."
