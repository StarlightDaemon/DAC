#Requires -Version 7.0
[CmdletBinding()]
param(
    [string]$BuildDirectory = 'build/release',
    [string]$PackageDirectory = 'packages'
)
. (Join-Path $PSScriptRoot 'Package.Common.ps1')
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$spec = Get-PackageSpecification
$build = Resolve-PackageLocalPath $root $BuildDirectory
$packageRoot = Resolve-PackageLocalPath $root $PackageDirectory
$stage = Resolve-PackageLocalPath $root (Join-Path $packageRoot $spec.archiveRoot)
$zipPath = Resolve-PackageLocalPath $root (Join-Path $packageRoot ($spec.archiveRoot + '.zip'))
foreach ($path in @($stage, $zipPath, ($zipPath + '.sha256'))) {
    if (Test-Path -LiteralPath $path) { throw 'Refusing to overwrite an existing package stage, archive, or checksum.' }
}
# Resolve every individually declared input before writing any output.
$inputs = @{}
foreach ($file in $spec.files | Where-Object kind -ne 'generated') {
    $base = if ($file.kind -ceq 'build') { $build } else { $root }
    $source = Resolve-PackageLocalPath $root (Join-Path $base $file.source)
    if (-not (Test-Path -LiteralPath $source -PathType Leaf)) { throw "Missing declared package source: $($file.source)" }
    $inputs[$file.path] = $source
}
New-Item -ItemType Directory -Path $stage -Force | Out-Null
foreach ($file in $spec.files | Where-Object kind -ne 'generated') {
    $destination = Resolve-PackageLocalPath $root (Join-Path $stage $file.path)
    New-Item -ItemType Directory -Path (Split-Path $destination -Parent) -Force | Out-Null
    Copy-Item -LiteralPath $inputs[$file.path] -Destination $destination
}
$observed = & (Join-Path $PSScriptRoot 'verify-pe.ps1') -Executable (Join-Path $stage 'DAC.exe') | ConvertFrom-Json
# A fixed field set prevents paths, environment data, or prior delivery receipts entering the ZIP.
$pe = [ordered]@{
    architecture=$observed.architecture; subsystem=$observed.subsystem
    ASLR=$observed.ASLR; DEP=$observed.DEP; highEntropyVA=$observed.highEntropyVA; CFG=$observed.CFG
    imports=@($observed.imports); version=$observed.version; sha256=$observed.sha256; bytes=$observed.bytes
}
$utf8 = [Text.UTF8Encoding]::new($false)
[IO.File]::WriteAllText((Join-Path $stage 'PE-VERIFICATION.json'), ($pe | ConvertTo-Json -Depth 4) + "`n", $utf8)
[string[]]$names = @($spec.files.path)
[Array]::Sort($names, [StringComparer]::Ordinal)
$checksums = foreach ($name in $names) {
    if ($name -cne 'SHA256SUMS.txt') {
        (Get-FileHash -LiteralPath (Join-Path $stage $name) -Algorithm SHA256).Hash.ToLowerInvariant() + '  ' + $name
    }
}
[IO.File]::WriteAllText((Join-Path $stage 'SHA256SUMS.txt'), ($checksums -join "`n") + "`n", $utf8)
$stream = [IO.File]::Open($zipPath, [IO.FileMode]::CreateNew)
$archive = [IO.Compression.ZipArchive]::new($stream, [IO.Compression.ZipArchiveMode]::Create, $false)
try {
    foreach ($name in $names) {
        $entry = $archive.CreateEntry($spec.archiveRoot + '/' + $name, [IO.Compression.CompressionLevel]::Optimal)
        $entry.LastWriteTime = [DateTimeOffset]::new(2026,10,8,0,0,0,[TimeSpan]::Zero)
        $destination = $entry.Open(); $source = [IO.File]::OpenRead((Join-Path $stage $name))
        try { Copy-PackageStream $source $destination }
        finally { $source.Dispose(); $destination.Dispose() }
    }
} finally { $archive.Dispose(); $stream.Dispose() }
# Verify without extracting or executing the product. The delivery receipt stays external.
$receipt = & (Join-Path $PSScriptRoot 'verify-package.ps1') -Archive $zipPath | ConvertFrom-Json
[IO.File]::WriteAllText($zipPath + '.sha256', $receipt.archiveSha256 + '  ' + [IO.Path]::GetFileName($zipPath) + "`n", $utf8)
Write-Output "Verified package $($receipt.archive) SHA256=$($receipt.archiveSha256)"
