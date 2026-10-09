#Requires -Version 7.0
[CmdletBinding()]
param([switch]$Offline)
$ErrorActionPreference = 'Stop'
$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$toolRoot = Join-Path $repoRoot '.tools'
$cache = Join-Path $toolRoot 'downloads'
$msvcRoot = Join-Path $toolRoot 'msvc'
$sdkRoot = Join-Path $toolRoot 'sdk'
$flatRoot = Join-Path $toolRoot 'sdk-payloads'
foreach ($directory in @($toolRoot, $cache, $msvcRoot, $sdkRoot, $flatRoot)) {
    New-Item -ItemType Directory -Path $directory -Force | Out-Null
}
$lock = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'toolchain-lock.json') -Raw | ConvertFrom-Json
foreach ($payload in $lock.payloads) {
    if ([Uri]$payload.url -isnot [Uri] -or ([Uri]$payload.url).Host -ne 'download.visualstudio.microsoft.com') {
        throw 'Toolchain payload must be an official Microsoft HTTPS download.'
    }
    if (([Uri]$payload.url).Scheme -ne 'https') { throw 'HTTPS required.' }
    if ([IO.Path]::GetFileName($payload.fileName) -ne $payload.fileName) { throw 'Archive cache names must be flat file names.' }
    $destination = Join-Path $cache $payload.fileName
    if (-not (Test-Path -LiteralPath $destination)) {
        if ($Offline) { throw "Offline cache is missing $($payload.fileName)" }
        Invoke-WebRequest -Uri $payload.url -OutFile $destination
    }
    if ((Get-FileHash -LiteralPath $destination -Algorithm SHA256).Hash -ne $payload.sha256) {
        throw "Toolchain SHA-256 mismatch: $($payload.fileName)"
    }
}

# VSIX packages are ZIP archives. Extract data only, without running setup.
foreach ($payload in $lock.payloads | Where-Object fileName -Like '*.vsix') {
    [IO.Compression.ZipFile]::ExtractToDirectory((Join-Path $cache $payload.fileName), $msvcRoot, $true)
}
# SDK cabinet payloads use MSI File-table identifiers. expand.exe only extracts data.
foreach ($payload in $lock.payloads | Where-Object fileName -Like '*.cab') {
    & "$env:SystemRoot\System32\expand.exe" '-F:*' (Join-Path $cache $payload.fileName) $flatRoot | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "Cannot extract $($payload.fileName)" }
}

function Read-MsiTable($Database, [string]$Name, [int]$Columns) {
    $view = $Database.OpenView(('SELECT * FROM `{0}`' -f $Name))
    [void]$view.Execute()
    try {
        while ($record = $view.Fetch()) {
            $values = @()
            for ($index = 1; $index -le $Columns; $index++) {
                $values += $record.GetType().InvokeMember('StringData', [Reflection.BindingFlags]::GetProperty, $null, $record, @($index))
            }
            ,$values
        }
    } finally { [void]$view.Close() }
}
function Resolve-SdkDirectory([string]$Id, $Directories, $Resolved) {
    if ($Resolved.ContainsKey($Id)) { return $Resolved[$Id] }
    if (-not $Directories.ContainsKey($Id)) { throw "Unknown SDK directory: $Id" }
    $row = $Directories[$Id]
    $parent = Resolve-SdkDirectory $row[1] $Directories $Resolved
    $name = ($row[2] -split ':')[0]
    $name = ($name -split '\|')[-1]
    $path = if ($name -eq '.') { $parent } else { Join-Path $parent $name }
    $path = [IO.Path]::GetFullPath($path)
    if (-not $path.StartsWith($sdkRoot + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase) -and $path -ne $sdkRoot) {
        throw "SDK extraction path escapes private tool directory: $path"
    }
    $Resolved[$Id] = $path
    return $path
}
$installer = New-Object -ComObject WindowsInstaller.Installer
foreach ($payload in $lock.payloads | Where-Object fileName -Like '*.msi') {
    $msiPath = Join-Path $cache $payload.fileName
    $signature = Get-AuthenticodeSignature -LiteralPath $msiPath
    if ($signature.Status -ne 'Valid' -or $signature.SignerCertificate.Subject -notmatch 'O=Microsoft Corporation') {
        throw "Microsoft SDK MSI signature failed: $($payload.fileName) ($($signature.Status))"
    }
    # OpenDatabase mode 0 is read-only; no MSI installation or custom actions run.
    $database = $installer.OpenDatabase($msiPath, 0)
    $directories = @{}
    foreach ($row in Read-MsiTable $database 'Directory' 3) { $directories[$row[0]] = $row }
    $resolved = @{ TARGETDIR = $sdkRoot; KitsRoot = $sdkRoot }
    $components = @{}
    foreach ($row in Read-MsiTable $database 'Component' 3) { $components[$row[0]] = $row[2] }
    foreach ($row in Read-MsiTable $database 'File' 3) {
        $source = Join-Path $flatRoot $row[0]
        if (-not (Test-Path -LiteralPath $source)) { throw "SDK cabinet missing file $($row[0])" }
        $directory = Resolve-SdkDirectory $components[$row[1]] $directories $resolved
        New-Item -ItemType Directory -Path $directory -Force | Out-Null
        $name = ($row[2] -split '\|')[-1]
        $target = [IO.Path]::GetFullPath((Join-Path $directory $name))
        if (-not $target.StartsWith($sdkRoot + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
            throw "SDK file escapes private directory: $name"
        }
        Copy-Item -LiteralPath $source -Destination $target -Force
    }
}

. (Join-Path $PSScriptRoot 'use-toolchain.ps1')
foreach ($executable in @((Get-Command cl.exe).Source, (Get-Command link.exe).Source, (Get-Command rc.exe).Source)) {
    $signature = Get-AuthenticodeSignature -LiteralPath $executable
    if ($signature.Status -ne 'Valid' -or $signature.SignerCertificate.Subject -notmatch 'O=Microsoft Corporation') {
        throw "Microsoft build executable signature failed: $executable ($($signature.Status))"
    }
}
& cmake.exe --version
& ninja.exe --version
$probe = Join-Path $toolRoot 'toolchain-probe.cpp'
@'
#include <windows.h>
#include <gdiplus.h>
#include <cstdint>
#include <string>
#include <expected>
#include <format>
#include <filesystem>
int main() {
    const std::expected<std::uint32_t, int> result{GetCurrentProcessId()};
    const std::string text = std::format("{}", result.value());
    const std::filesystem::path path{"DAC"};
    const Gdiplus::Rect rectangle{0, 0, 10, 10};
    return text.empty() || path.empty() || rectangle.Width != 10 ? 1 : 0;
}
'@ | Set-Content -LiteralPath $probe
$probeObject = Join-Path $toolRoot 'toolchain-probe.obj'
$probeExe = Join-Path $toolRoot 'toolchain-probe.exe'
& cl.exe /nologo /std:c++latest /W4 /sdl /guard:cf /MT /EHsc $probe "/Fo$probeObject" "/Fe$probeExe" /link shell32.lib user32.lib gdi32.lib ole32.lib uuid.lib wtsapi32.lib comdlg32.lib advapi32.lib powrprof.lib dwmapi.lib dxva2.lib gdiplus.lib psapi.lib
if ($LASTEXITCODE -ne 0) { throw 'Private toolchain C++/Win32/GDI+ compile and link probe failed.' }
& $probeExe
if ($LASTEXITCODE -ne 0) { throw 'Private toolchain execution probe failed.' }
Write-Output 'Verified private MSVC/Windows SDK toolchain is ready. Dot-source tools/use-toolchain.ps1 in each build shell.'
