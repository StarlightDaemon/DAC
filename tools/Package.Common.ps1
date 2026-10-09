# Shared data-only package manifest and local-path validation.
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Assert-PackageName([string]$Name) {
    if (-not $Name -or $Name.Contains('\') -or $Name.Contains(':') -or $Name.StartsWith('/') -or $Name.EndsWith('/')) {
        throw "Noncanonical package file name: $Name"
    }
    foreach ($part in $Name.Split('/')) {
        if ($part -notmatch '^[A-Za-z0-9][A-Za-z0-9._-]*$' -or $part.EndsWith('.') -or
            $part -match '^(?i:CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])(?:\.|$)') {
            throw "Unsafe package path component: $Name"
        }
    }
}

function Get-PackageSpecification {
    $spec = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'package-manifest.json') -Raw | ConvertFrom-Json
    if ($spec.formatVersion -ne 1 -or $spec.archiveRoot -cne 'DAC-0.1.0-dev-windows-x64') {
        throw 'Unsupported package manifest identity.'
    }
    $seen = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach ($file in $spec.files) {
        Assert-PackageName $file.path
        if (-not $seen.Add($file.path)) { throw "Duplicate package manifest path: $($file.path)" }
        switch -CaseSensitive ($file.kind) {
            'build' {
                if ($file.source -cne 'DAC.exe' -or $file.path -cne 'DAC.exe') { throw 'Only DAC.exe may come from the build.' }
            }
            'repository' {
                Assert-PackageName $file.source
                $publicDocument = $file.source -cmatch '^docs/[A-Za-z0-9_-]+\.md$'
                $rootDocument = $file.source -cin @('README.md','LICENSE','CHANGELOG.md','THIRD_PARTY_NOTICES.md')
                $fujinLicense = $file.source -ceq 'third_party/fujin/LICENSE' -and $file.path -ceq 'licenses/Fujin-LICENSE'
                if (-not $fujinLicense -and (-not ($publicDocument -or $rootDocument) -or $file.path -cne $file.source)) {
                    throw "Unsupported repository package source: $($file.source)"
                }
            }
            'generated' {
                if ($file.path -cnotin @('PE-VERIFICATION.json','SHA256SUMS.txt')) { throw 'Unexpected generated package file.' }
            }
            default { throw "Unsupported package source kind: $($file.kind)" }
        }
    }
    foreach ($name in @('DAC.exe','README.md','LICENSE','THIRD_PARTY_NOTICES.md','licenses/Fujin-LICENSE','PE-VERIFICATION.json','SHA256SUMS.txt')) {
        if (-not $seen.Contains($name)) { throw "Package manifest is missing $name" }
    }
    return $spec
}

function Resolve-PackageLocalPath([string]$Repository, [string]$Path) {
    $full = [IO.Path]::GetFullPath($(if ([IO.Path]::IsPathRooted($Path)) { $Path } else { Join-Path $Repository $Path }))
    if (-not $full.StartsWith($Repository + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
        throw 'Package inputs and outputs must remain inside this checkout.'
    }
    $check = $full
    while ($check -and $check.Length -ge $Repository.Length) {
        if ((Test-Path -LiteralPath $check) -and ((Get-Item -LiteralPath $check -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)) {
            throw 'Package paths cannot traverse a reparse point.'
        }
        $check = Split-Path $check -Parent
    }
    return $full
}

function Copy-PackageStream($Source, $Destination) {
    # Fill each 128 KiB block explicitly: short reads must not change Deflate writes.
    $buffer = [byte[]]::new(131072)
    while ($true) {
        $filled = 0
        while ($filled -lt $buffer.Length) {
            $count = $Source.Read($buffer, $filled, $buffer.Length - $filled)
            if ($count -eq 0) { break }
            $filled += $count
        }
        if ($filled -eq 0) { break }
        $Destination.Write($buffer, 0, $filled)
    }
}
