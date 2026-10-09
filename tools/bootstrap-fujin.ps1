[CmdletBinding()]
param([string] $LocalSource = '', [string] $Destination = '')
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Fujin.Common.ps1')
$root = [IO.Path]::GetFullPath((Split-Path $PSScriptRoot -Parent))
if (-not $Destination) { $Destination = Join-Path $root '.deps/Fujin' }
$Destination = [IO.Path]::GetFullPath($Destination)
if (-not $Destination.StartsWith($root + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'FUJIN_DESTINATION: dependency writes must remain inside DAC.'
}
$lock = Get-FujinLock (Join-Path $root 'third_party/fujin/lock.json')
if (Test-Path -LiteralPath $Destination) {
    Assert-FujinCheckout $Destination $lock
    $null = Read-FujinThemeData $Destination $lock
    Write-Output "Verified existing Fujin $($lock.tag) at $Destination"
    exit 0
}
$parent = Split-Path $Destination -Parent
$check = $parent
while ($check -and $check.Length -ge $root.Length) {
    if (Test-Path -LiteralPath $check) {
        if ((Get-Item -LiteralPath $check -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) {
            throw 'FUJIN_DESTINATION: reparse-point destination is not supported.'
        }
    }
    $check = Split-Path $check -Parent
}
$source = $lock.repository
if ($LocalSource) {
    $source = [IO.Path]::GetFullPath($LocalSource)
    Assert-FujinRemote $source $lock
}
New-Item -ItemType Directory -Path $parent -Force | Out-Null
& git -c core.autocrlf=false clone --no-hardlinks --config core.autocrlf=false --config core.eol=lf --single-branch --branch $lock.tag -- $source $Destination
if ($LASTEXITCODE -ne 0) { throw 'FUJIN_BOOTSTRAP: clone failed; no existing checkout was modified.' }
$null = Invoke-FujinGit $Destination @('remote', 'set-url', 'origin', $lock.repository)
Assert-FujinCheckout $Destination $lock
$null = Read-FujinThemeData $Destination $lock
Write-Output "Installed Fujin $($lock.tag) ($($lock.commit)) at $Destination"
