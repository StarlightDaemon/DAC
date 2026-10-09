[CmdletBinding()]
param([Parameter(Mandatory = $true)][string] $BuildDirectory, [string] $SourceDirectory = '')
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Fujin.Common.ps1')
$root = [IO.Path]::GetFullPath((Split-Path $PSScriptRoot -Parent))
$BuildDirectory = [IO.Path]::GetFullPath($BuildDirectory)
if (-not $BuildDirectory.StartsWith($root + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Test output must remain within DAC.'
}
if (-not $SourceDirectory) { $SourceDirectory = Join-Path $root '.deps/Fujin' }
$lockPath = Join-Path $root 'third_party/fujin/lock.json'
$lock = Get-FujinLock $lockPath
Assert-FujinCheckout $SourceDirectory $lock
$null = Read-FujinThemeData $SourceDirectory $lock
$script:checks = 1
$fixture = Join-Path $BuildDirectory ('fujin-fixtures/' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path (Join-Path $fixture 'dist') -Force | Out-Null
$generator = Join-Path $PSScriptRoot 'generate-theme.ps1'
$header = Join-Path $fixture 'fujin.hpp'
& $generator -SourceDirectory $SourceDirectory -OutputPath $header
$hash = (Get-FileHash -LiteralPath $header).Hash
$writeTime = (Get-Item -LiteralPath $header).LastWriteTimeUtc
& $generator -SourceDirectory $SourceDirectory -OutputPath $header
if ((Get-FileHash -LiteralPath $header).Hash -cne $hash -or (Get-Item -LiteralPath $header).LastWriteTimeUtc -ne $writeTime) {
    throw 'Repeated generation changed the output or its timestamp.'
}
$script:checks++
foreach ($accent in @('indigo', 'blue', 'cyan', 'teal', 'green', 'orange')) {
    & $generator -SourceDirectory $SourceDirectory -OutputPath $header -Accent $accent
    $content = Get-Content -LiteralPath $header -Raw
    if ($content -notmatch ('accentName\[\]=L"' + $accent + '";') -or (Get-FileHash -LiteralPath $header).Hash -ceq $hash) {
        throw "Accent $accent did not change the generated theme."
    }
    $script:checks++
}
function Reset-Fixture {
    foreach ($entry in $lock.files) {
        Copy-Item -LiteralPath (Join-Path $SourceDirectory $entry.path) -Destination (Join-Path $fixture $entry.path) -Force
    }
    return ($lock | ConvertTo-Json -Depth 8 | ConvertFrom-Json)
}
function Update-FixtureHash($FixtureLock, [string] $Path) {
    ($FixtureLock.files | Where-Object { $_.path -ceq $Path }).sha256 =
        (Get-FileHash -LiteralPath (Join-Path $fixture $Path)).Hash.ToLowerInvariant()
}
function Expect-Failure([string] $Name, [string] $Category, [scriptblock] $Action) {
    $caught = $false
    try { & $Action | Out-Null }
    catch {
        if ($_.Exception.Message -notlike ($Category + '*')) { throw "$Name failed for an unexpected reason: $($_.Exception.Message)" }
        $caught = $true
    }
    if (-not $caught) { throw "$Name unexpectedly passed." }
    $script:checks++
    Write-Output "PASS: $Name"
}
function Write-FixtureJson($Json) {
    [IO.File]::WriteAllText((Join-Path $fixture 'dist/tokens-resolved.json'), ($Json | ConvertTo-Json -Depth 8), [Text.UTF8Encoding]::new($false))
}
$fixtureLock = Reset-Fixture
[IO.File]::AppendAllText((Join-Path $fixture 'dist/tokens.css'), 'tampered')
Expect-Failure 'checksum tamper rejection' 'FUJIN_INTEGRITY:' { Read-FujinThemeData $fixture $fixtureLock }
$fixtureLock = Reset-Fixture
Remove-Item -LiteralPath (Join-Path $fixture 'dist/tokens-resolved.json')
Expect-Failure 'missing input rejection' 'FUJIN_INTEGRITY:' { Read-FujinThemeData $fixture $fixtureLock }
$fixtureLock = Reset-Fixture
[IO.File]::WriteAllText((Join-Path $fixture 'dist/tokens-resolved.json'), '{broken-json', [Text.UTF8Encoding]::new($false))
Update-FixtureHash $fixtureLock 'dist/tokens-resolved.json'
Expect-Failure 'malformed JSON rejection after checksum verification' 'FUJIN_SCHEMA:' { Read-FujinThemeData $fixture $fixtureLock }
$fixtureLock = Reset-Fixture
$json = Get-Content -LiteralPath (Join-Path $fixture 'dist/tokens-resolved.json') -Raw | ConvertFrom-Json
$json.dark.PSObject.Properties.Remove('--fujin-text-primary')
Write-FixtureJson $json
Update-FixtureHash $fixtureLock 'dist/tokens-resolved.json'
Expect-Failure 'missing semantic token rejection' 'FUJIN_SCHEMA:' { Read-FujinThemeData $fixture $fixtureLock }
$fixtureLock = Reset-Fixture
$json = Get-Content -LiteralPath (Join-Path $fixture 'dist/tokens-resolved.json') -Raw | ConvertFrom-Json
$json.light.'--fujin-interactive-default' = 'untrusted native source'
Write-FixtureJson $json
Update-FixtureHash $fixtureLock 'dist/tokens-resolved.json'
Expect-Failure 'invalid native color rejection' 'FUJIN_SCHEMA:' { Read-FujinThemeData $fixture $fixtureLock }
$fixtureLock = Reset-Fixture
$cssPath = Join-Path $fixture 'dist/tokens.css'
$css = [IO.File]::ReadAllText($cssPath).Replace('--fujin-radius-default: 0px;', '--fujin-radius-default: 4px;')
[IO.File]::WriteAllText($cssPath, $css, [Text.UTF8Encoding]::new($false))
Update-FixtureHash $fixtureLock 'dist/tokens.css'
Expect-Failure 'nonzero radius rejection' 'FUJIN_SCHEMA:' { Read-FujinThemeData $fixture $fixtureLock }
$fixtureLock = Reset-Fixture
$css = [IO.File]::ReadAllText($cssPath).Replace('--fujin-font-size-sm: 12px;', '--missing-font-size-sm: 12px;')
[IO.File]::WriteAllText($cssPath, $css, [Text.UTF8Encoding]::new($false))
Update-FixtureHash $fixtureLock 'dist/tokens.css'
Expect-Failure 'missing CSS scalar rejection' 'FUJIN_SCHEMA:' { Read-FujinThemeData $fixture $fixtureLock }
$fixtureLock = Reset-Fixture
$json = Get-Content -LiteralPath (Join-Path $fixture 'dist/tokens-resolved.json') -Raw | ConvertFrom-Json
$json.tokens.fontFamily = '"Verdana"; injected-code'
Write-FixtureJson $json
Update-FixtureHash $fixtureLock 'dist/tokens-resolved.json'
Expect-Failure 'unsafe font family rejection' 'FUJIN_SCHEMA:' { Read-FujinThemeData $fixture $fixtureLock }
$fixtureLock = Reset-Fixture
$fixtureLock.commit = '0000000000000000000000000000000000000000'
Expect-Failure 'wrong release identity rejection' 'FUJIN_REVISION:' { Assert-FujinCheckout $SourceDirectory $fixtureLock }
Write-Output "Fujin theme checks passed: $script:checks. Fixtures retained at $fixture"
