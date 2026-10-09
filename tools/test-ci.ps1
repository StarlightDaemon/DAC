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
# Exercise the receipt guard extracted from the workflow, without a product build.
# These byte fixtures test delivery receipts; package.ps1/verify-package.ps1 remain
# authoritative for portable ZIP contents and are still required before the audit.
$workflow = [IO.File]::ReadAllText((Join-Path $repository '.github/workflows/windows-ci.yml'))
$steps = @([regex]::Matches($workflow, '(?ms)^      - name: ([^\r\n]+)\r?\n(.*?)(?=^      - name: |\z)'))
function Assert-WorkflowPolicy([bool]$Condition) {
    if (-not $Condition) { throw 'Development artifact workflow policy regression.' }
    $script:checks++
}
Assert-WorkflowPolicy ($steps.Count -eq 10)
$phases = @('Bootstrap','Release','Debug','Analyze','Package','Audit')
for ($index = 0; $index -lt $phases.Count; $index++) {
    $block = $steps[$index + 2].Groups[2].Value
    Assert-WorkflowPolicy ($block.Contains('run: ./tools/ci-windows.ps1 -Phase ' + $phases[$index]) -and
        $block -notmatch '(?m)^        (if|continue-on-error):')
}
Assert-WorkflowPolicy ($workflow -match '(?m)^permissions:\r?\n  contents: read\r?\n\r?\n' -and
    $workflow -match '(?m)^    runs-on: windows-2025\r?$' -and
    $workflow -notmatch '(?m)^\s+continue-on-error:' -and
    ([regex]::Matches($workflow, 'uses: actions/upload-artifact@').Count -eq 1))
Assert-WorkflowPolicy ($steps[7].Groups[1].Value -ceq 'Audit publication privacy' -and
    $steps[8].Groups[1].Value -ceq 'Verify development artifact receipt' -and
    $steps[9].Groups[1].Value -ceq 'Upload development test package')
# Pin the GitHub expression's fail-closed semantics: any prior failure/cancellation,
# every PR event (including one aimed at main), and every other ref are excluded.
$gate = '        if: ${{ success() && github.event_name == ''push'' && github.ref == ''refs/heads/main'' }}'
foreach ($step in $steps[8..9]) {
    $conditions = @($step.Groups[2].Value -split '\r?\n' | Where-Object { $_ -match '^        if:' })
    Assert-WorkflowPolicy ($conditions.Count -eq 1 -and $conditions[0] -ceq $gate)
}
$upload = $steps[9].Groups[2].Value
$pathBlock = [regex]::Match($upload, '(?m)^          path: \|\r?\n((?:            [^\r\n]+\r?\n)+)')
$paths = @($pathBlock.Groups[1].Value.TrimEnd() -split '\r?\n' | ForEach-Object { $_.Trim() })
Assert-WorkflowPolicy (($paths -join '|') -ceq
    'build/ci-package/DAC-0.1.0-dev-windows-x64.zip|build/ci-package/DAC-0.1.0-dev-windows-x64.zip.sha256')
foreach ($setting in @(
    '        uses: actions/upload-artifact@cf430e030ddbb5b0abf93d22962f4752f3646cd9 # v7.0.2',
    '          name: DAC-0.1.0-dev-windows-x64-${{ github.sha }}',
    '          if-no-files-found: error', '          include-hidden-files: false',
    '          retention-days: 14', '          overwrite: false', '          archive: true')) {
    Assert-WorkflowPolicy (@($upload -split '\r?\n' | Where-Object { $_ -ceq $setting }).Count -eq 1)
}
$guard = [regex]::Match($steps[8].Groups[2].Value, '(?m)^        run: \|\r?\n((?:          [^\r\n]*\r?\n)+)')
Assert-WorkflowPolicy ($guard.Success)
$guardFile = Join-Path $private 'receipt-guard.ps1'
[IO.File]::WriteAllText($guardFile, ([regex]::Replace($guard.Groups[1].Value, '(?m)^          ', '')), [Text.UTF8Encoding]::new($false))
function Invoke-ReceiptFixture([string]$Receipt = '', [switch]$OmitArchive, [switch]$OmitReceipt, [switch]$Tamper, [switch]$WrongName) {
    $fixture = Join-Path $private ([Guid]::NewGuid().ToString('N'))
    $package = Join-Path $fixture 'build/ci-package'
    [void][IO.Directory]::CreateDirectory($package)
    $archive = Join-Path $package 'DAC-0.1.0-dev-windows-x64.zip'
    [IO.File]::WriteAllText($archive, 'receipt fixture bytes', [Text.UTF8Encoding]::new($false))
    $digest = (Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash.ToLowerInvariant()
    if (-not $Receipt) { $Receipt = $digest + '  DAC-0.1.0-dev-windows-x64.zip' + "`n" }
    if ($WrongName) { $Receipt = $Receipt.Replace('DAC-0.1.0-dev-windows-x64.zip', 'unrelated.zip') }
    [IO.File]::WriteAllText($archive + '.sha256', $Receipt, [Text.UTF8Encoding]::new($false))
    if ($OmitArchive) { [IO.File]::Delete($archive) }
    if ($OmitReceipt) { [IO.File]::Delete($archive + '.sha256') }
    if ($Tamper) { [IO.File]::AppendAllText($archive, 'tampered') }
    $start = [Diagnostics.ProcessStartInfo]::new($pwsh)
    $start.WorkingDirectory = $fixture
    $start.UseShellExecute = $false; $start.CreateNoWindow = $true
    $start.RedirectStandardOutput = $true; $start.RedirectStandardError = $true
    foreach ($argument in @('-NoProfile','-File',$guardFile)) { $start.ArgumentList.Add($argument) }
    $process = [Diagnostics.Process]::new(); $process.StartInfo = $start
    try {
        [void]$process.Start()
        $stdout = $process.StandardOutput.ReadToEndAsync(); $stderr = $process.StandardError.ReadToEndAsync()
        if (-not $process.WaitForExit(30000)) { $process.Kill($true); throw 'Receipt fixture timed out.' }
        $publicOutput = $stdout.GetAwaiter().GetResult().Trim()
        if ($stderr.GetAwaiter().GetResult().Length -ne 0 -or $publicOutput -cnotin @(
            'Development artifact archive and checksum receipt: passed.',
            'Development artifact receipt verification failed.')) { throw 'Receipt guard exposed unexpected output.' }
        return $process.ExitCode
    } finally { $process.Dispose() }
}
Assert-WorkflowPolicy ((Invoke-ReceiptFixture) -eq 0)
Assert-WorkflowPolicy ((Invoke-ReceiptFixture -OmitArchive) -ne 0)
Assert-WorkflowPolicy ((Invoke-ReceiptFixture -OmitReceipt) -ne 0)
Assert-WorkflowPolicy ((Invoke-ReceiptFixture -Tamper) -ne 0)
Assert-WorkflowPolicy ((Invoke-ReceiptFixture -Receipt ('0' * 64 + '  DAC-0.1.0-dev-windows-x64.zip' + "`n")) -ne 0)
Assert-WorkflowPolicy ((Invoke-ReceiptFixture -Receipt 'malformed receipt') -ne 0)
Assert-WorkflowPolicy ((Invoke-ReceiptFixture -WrongName) -ne 0)
Write-Output ('CI driver regression checks passed: ' + $script:checks + '.')
