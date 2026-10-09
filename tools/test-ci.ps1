#Requires -Version 7.0
[CmdletBinding()]
param()
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
# Load only function definitions: never execute a product build or bootstrap here.
$tokens = $null; $errors = $null
$ast = [Management.Automation.Language.Parser]::ParseFile((Join-Path $PSScriptRoot 'ci-windows.ps1'), [ref]$tokens, [ref]$errors)
if ($errors.Count) { throw 'CI driver parse failed.' }
foreach ($definition in $ast.FindAll({ param($node) $node -is [Management.Automation.Language.FunctionDefinitionAst] }, $false)) {
    . ([scriptblock]::Create($definition.Extent.Text))
}
$repository = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$private = Join-Path $repository ('out/private/ci-regression/' + [Guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($private)
$pwsh = (Get-Process -Id $PID).Path
$script:checks = 0
function Expect-Rejection([scriptblock]$Action) {
    $rejected = $false
    try { & $Action | Out-Null } catch { $rejected = $true }
    if (-not $rejected) { throw 'CI negative fixture unexpectedly passed.' }
    $script:checks++
}
function Write-Fixture([string]$Text) {
    $path = Join-Path $private ([Guid]::NewGuid().ToString('N') + '.xml')
    [IO.File]::WriteAllText($path, $Text, [Text.UTF8Encoding]::new($false))
    return $path
}
$names = @(Get-ExpectedTests)
Assert-TestNames $names; $script:checks++
Expect-Rejection { Assert-TestNames @() }
Expect-Rejection { Assert-TestNames $names[0..25] }
Expect-Rejection { Assert-TestNames ($names + 'unexpected-test') }
$duplicate = @($names); $duplicate[26] = $duplicate[0]
Expect-Rejection { Assert-TestNames $duplicate }
$renamed = @($names); $renamed[26] = 'replacement-test'
Expect-Rejection { Assert-TestNames $renamed }
$cases = ($names | ForEach-Object { '<testcase name="' + $_ + '" status="run"></testcase>' }) -join ''
$passing = '<testsuite>' + $cases + '</testsuite>'
Assert-CiTestReport (Write-Fixture $passing); $script:checks++
foreach ($element in @('failure','error','skipped')) {
    $invalid = $passing.Replace('</testcase>', "<$element /></testcase>")
    Expect-Rejection { Assert-CiTestReport (Write-Fixture $invalid) }
}
Expect-Rejection { Assert-CiTestReport (Write-Fixture ($passing.Replace('status="run"','status="notrun"'))) }
Expect-Rejection { Assert-CiTestReport (Write-Fixture '<testsuite />') }
Expect-Rejection { Assert-CiTestReport (Write-Fixture '<broken') }
Expect-Rejection { Assert-CiTestReport (Join-Path $private 'missing.xml') }
Expect-Rejection { Read-CiXml (Write-Fixture '<!DOCTYPE testsuite [<!ENTITY data "fixture">]><testsuite>&data;</testsuite>') }
Assert-CiAnalysisReport (Write-Fixture '<DEFECTS />'); $script:checks++
Expect-Rejection { Assert-CiAnalysisReport (Write-Fixture '<DEFECTS><DEFECT /></DEFECTS>') }
Expect-Rejection { Assert-CiAnalysisReport (Write-Fixture '<unexpected />') }
Expect-Rejection { Assert-CiAnalysisReport (Join-Path $private 'missing.xml') }
# Raw stdout, stderr and workflow-command-shaped text must never be replayed.
$canary = 'CI_PRIVATE_CANARY'
$output = @(& { Invoke-PrivateCommand 'capture-fixture' $pwsh @('-NoProfile','-Command', 'Write-Output "CI_PRIVATE_CANARY"; [Console]::Error.WriteLine("::error::CI_PRIVATE_CANARY")') } 6>&1)
if (($output | Out-String).Contains($canary)) { throw 'Private command output was replayed.' }
if (-not ([IO.File]::ReadAllText($output[-1])).Contains($canary) -or
    -not ([IO.File]::ReadAllText($output[-1].Replace('.stdout.log','.stderr.log'))).Contains($canary)) { throw 'Raw fixture output was not captured.' }
$script:checks++
Expect-Rejection { Invoke-PrivateCommand 'exit-fixture' $pwsh @('-NoProfile','-Command','exit 7') }
Expect-Rejection { Invoke-PrivateCommand 'timeout-fixture' $pwsh @('-NoProfile','-Command','Start-Sleep -Seconds 30') 1 }
Write-Output ('CI driver regression checks passed: ' + $script:checks + '.')
