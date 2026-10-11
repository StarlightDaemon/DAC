#Requires -Version 7.0
[CmdletBinding()]
param([Parameter(Mandatory)][ValidateSet('Bootstrap','Release','Debug','Analyze','Package','Installer','Audit')][string]$Phase,[string]$InstallerDirectory='build/ci-installer',[string]$Archive='build/ci-package/DAC-0.1.0-dev-windows-x64.zip')
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
# Never expose OS exceptions, tool output, test output, or XML in public CI logs.
trap {
    Write-Output ('CI validation failed in phase ' + $Phase + '; script line ' + $_.InvocationInfo.ScriptLineNumber + '. Reproduce locally; raw evidence is not uploaded.')
    exit 1
}
$repository = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
Set-Location -LiteralPath $repository
$private = Join-Path $repository 'out/private/ci'
[void][IO.Directory]::CreateDirectory($private)
$pwsh = (Get-Process -Id $PID).Path
# The job remains 45 minutes. Reserve six minutes for checkout/post-actions and
# the unchanged optional artifact stages. Never skip a gate to meet the budget.
if ($env:GITHUB_ACTIONS -ceq 'true' -and -not $env:DAC_CI_STARTED_UTC) {throw 'Hosted CI budget origin is missing.'}
$started=[DateTime]::UtcNow
if ($env:DAC_CI_STARTED_UTC) {$started=[DateTime]::Parse($env:DAC_CI_STARTED_UTC,[Globalization.CultureInfo]::InvariantCulture,[Globalization.DateTimeStyles]::RoundtripKind).ToUniversalTime()}
if ($started -gt [DateTime]::UtcNow) {throw 'Invalid CI budget origin.'}
$deadline=$started.AddMinutes(39)
function Get-CiCommandBudget([int]$Requested) {
    $remaining=[int][Math]::Floor(($deadline-[DateTime]::UtcNow).TotalSeconds)
    if ($remaining -le 0) {throw 'Required validation cannot complete within the hosted budget.'}
    return [Math]::Min($Requested,$remaining)
}
function Assert-ReviewedWorkflow([string]$Workflow) {
    $bytes=[Text.UTF8Encoding]::new($false).GetBytes($Workflow.Replace("`r`n","`n"))
    $digest=[Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($bytes)).ToLowerInvariant()
    # Full reviewed workflow: pins triggers, permissions, jobs, every exact step,
    # commands, timeout budgets, action identities and artifact paths/settings.
    if ($digest -cne 'e3a6d9d98604ffecb6e1b3f6348eabcfb6cef5b31cc37914a6375c7fb32c02af') {throw 'Workflow differs from the reviewed exact contract.'}
}
function Assert-InstallerVerification($Evidence) {
    if ($Evidence.verification -cne 'passed' -or $Evidence.embeddedPayloadChecked -ne $true -or
        $Evidence.embeddedPayloadFiles -ne 19 -or $Evidence.payloadFiles -ne 19 -or $Evidence.fixture -ne $false -or
        $Evidence.unsigned -ne $true -or $Evidence.productExecuted -ne $false -or
        $Evidence.sourceCommit -cnotmatch '^[0-9a-f]{40}$' -or $Evidence.sourceTreeSha256 -cnotmatch '^[0-9a-f]{64}$') {throw 'Installer evidence incomplete.'}
}

function Assert-RuntimePrivacyRegression($Evidence) {
    foreach ($field in @('oneOccurrenceOnly','installationExecuted','sourceAndToolchainMutated')) {
        if ($Evidence.$field -isnot [bool]) {throw 'Runtime privacy evidence boolean types changed.'}
    }
    if ($Evidence.verification -cne 'passed' -or $Evidence.checks -ne 59 -or $Evidence.positiveChecks -ne 4 -or
        $Evidence.negativeChecks -ne 55 -or $Evidence.resourceBytes -ne 1452 -or
        $Evidence.resourceSha256 -cne 'b02caa228cd8290d58c399c4f99fddf6f2f6fc81cf3297faf2ca346e1f5c085b' -or
        $Evidence.oneOccurrenceOnly -ne $true -or $Evidence.installationExecuted -ne $false -or
        $Evidence.sourceAndToolchainMutated -ne $false) {throw 'Runtime privacy regression evidence incomplete.'}
}
function Invoke-PrivateCommand([string]$Label, [string]$File, [string[]]$Arguments, [int]$Seconds = 300) {
    # ArgumentList avoids command-string evaluation. Each invocation gets fresh logs.
    $Seconds=Get-CiCommandBudget $Seconds
    $log = Join-Path $private ($Label + '-' + [Guid]::NewGuid().ToString('N'))
    $start = [Diagnostics.ProcessStartInfo]::new($File)
    $start.WorkingDirectory = $repository
    $start.UseShellExecute = $false
    $start.CreateNoWindow = $true
    $start.RedirectStandardOutput = $true
    $start.RedirectStandardError = $true
    foreach ($argument in $Arguments) { $start.ArgumentList.Add($argument) }
    $process = [Diagnostics.Process]::new()
    $process.StartInfo = $start
    $stdout = [IO.File]::Create($log + '.stdout.log')
    $stderr = [IO.File]::Create($log + '.stderr.log')
    try {
        [void]$process.Start()
        $outCopy = $process.StandardOutput.BaseStream.CopyToAsync($stdout)
        $errCopy = $process.StandardError.BaseStream.CopyToAsync($stderr)
        if (-not $process.WaitForExit($Seconds * 1000)) {
            $process.Kill($true)
            $process.WaitForExit()
            [void]$outCopy.GetAwaiter().GetResult()
            [void]$errCopy.GetAwaiter().GetResult()
            Write-Host ($Label + ': failed (execution timeout).')
            throw 'Owned CI command exceeded its execution bound.'
        }
        [void]$outCopy.GetAwaiter().GetResult()
        [void]$errCopy.GetAwaiter().GetResult()
        if ($process.ExitCode -ne 0) {
            Write-Host ($Label + ': failed (exit ' + $process.ExitCode + ').')
            throw 'CI command failed.'
        }
    } finally { $stdout.Dispose(); $stderr.Dispose(); $process.Dispose() }
    Write-Host ($Label + ': passed.')
    return $log + '.stdout.log'
}
function Invoke-PrivateScript([string]$Label, [string]$Script, [string[]]$Arguments = @(), [int]$Seconds = 300) {
    return Invoke-PrivateCommand $Label $pwsh (@('-NoProfile','-File', (Join-Path $PSScriptRoot $Script)) + $Arguments) $Seconds
}
function Read-CiXml([string]$Path) {
    $settings = [Xml.XmlReaderSettings]::new()
    $settings.DtdProcessing = [Xml.DtdProcessing]::Prohibit
    $settings.XmlResolver = $null
    $reader = [Xml.XmlReader]::Create($Path, $settings)
    try { $document = [Xml.XmlDocument]::new(); $document.Load($reader); return $document }
    finally { $reader.Dispose() }
}
function Get-ExpectedTests {
    return @('policy','queued-wake-probe','storage','catalog','faults','containment','host-death','emergency',
        'lifecycle','sessions','session-soak','slides','power','advanced','nextbeta','nativewake','wakeperf',
        'idleperf','media','host-preferences','standalone-cli-version','standalone-cli-help','standalone-cli-reject',
        'hardware-helper-reject','standalone-host','standalone-lifecycle','fujin-integrity')
}
function Assert-TestNames([string[]]$Names) {
    $expected = @(Get-ExpectedTests | Sort-Object)
    if ($Names.Count -ne 27 -or (($Names | Sort-Object) -join '|') -cne ($expected -join '|')) {
        throw 'Expected exactly the reviewed 27-test suite.'
    }
}
function Assert-CiTestReport([string]$Path) {
    $xml = Read-CiXml $Path
    $cases = @($xml.SelectNodes('/testsuite/testcase'))
    Assert-TestNames @($cases | ForEach-Object { $_.GetAttribute('name') })
    if ($xml.SelectNodes('//failure | //error | //skipped').Count -ne 0 -or
        @($cases | Where-Object { $_.GetAttribute('status') -cne 'run' }).Count -ne 0) {
        throw 'Test failures, errors, skips, or unavailable execution are not accepted.'
    }
}
function Assert-CiAnalysisReport([string]$Path) {
    $xml = Read-CiXml $Path
    if ($xml.DocumentElement.Name -cne 'DEFECTS' -or $xml.SelectNodes('/DEFECTS/*').Count -ne 0) {
        throw 'Production analyzer defects or unexpected report format.'
    }
}

if ($Phase -eq 'Bootstrap') {
    $null = Invoke-PrivateScript 'ci-driver-regression' 'test-ci.ps1'
    Write-Output 'Microsoft manifest metadata discrepancy remains unresolved; the reviewed archive lock is used. No manifest digest pass is claimed.'
    $null = Invoke-PrivateScript 'toolchain-bootstrap' 'bootstrap-toolchain.ps1' @() 1080
    $lock = Get-Content -LiteralPath tools/toolchain-lock.json -Raw | ConvertFrom-Json
    foreach ($payload in $lock.payloads) {
        $file = Join-Path $repository ('.tools/downloads/' + $payload.fileName)
        if ((Get-Item -LiteralPath $file).Length -ne $payload.actualSize) { throw 'Pinned payload size mismatch.' }
    }
    $null = Invoke-PrivateScript 'fujin-bootstrap' 'bootstrap-fujin.ps1' @() 120
} else {
    . (Join-Path $PSScriptRoot 'use-toolchain.ps1')
}

if ($Phase -in @('Release','Debug')) {
    $build = 'build/ci-' + $Phase.ToLowerInvariant()
    $null = Invoke-PrivateCommand ($Phase + '-configure') 'cmake.exe' @('-S','.', '-B',$build,'-G','Ninja',"-DCMAKE_BUILD_TYPE=$Phase",'-DDAC_BUILD_TESTS=ON')
    $null = Invoke-PrivateCommand ($Phase + '-build') 'cmake.exe' @('--build',$build,'--parallel','3')
    $listing = @(Invoke-PrivateCommand ($Phase + '-test-inventory') 'ctest.exe' @('--test-dir',$build,'--show-only=json-v1'))
    $inventory = Get-Content -LiteralPath $listing[-1] -Raw | ConvertFrom-Json
    Assert-TestNames @($inventory.tests.name)
    $junit = Join-Path $repository ($build + '/ci-results.xml')
    # Fresh results are required, even when reproducing in an existing local directory.
    if (Test-Path -LiteralPath $junit) { Remove-Item -LiteralPath $junit }
    $null = Invoke-PrivateCommand ($Phase + '-tests') 'ctest.exe' @('--test-dir',$build,'--parallel','1','--timeout','130','--no-tests=error','--output-on-failure','--output-junit',$junit) 300
    Assert-CiTestReport $junit
    Write-Output ($Phase + ' suite: 27 passed; zero failures/errors/skips.')
}

if ($Phase -eq 'Analyze') {
    $analysis = Join-Path $private ('analysis-' + [Guid]::NewGuid().ToString('N') + '.xml')
    $arguments = @('/nologo','/std:c++latest','/MT','/utf-8','/permissive-','/EHsc','/W4','/WX','/sdl','/guard:cf',
        '/O2','/DNDEBUG','/DWIN32','/D_WINDOWS','/DNOMINMAX','/DUNICODE','/D_UNICODE','/DWIN32_LEAN_AND_MEAN',
        '/DWINVER=0x0A00','/D_WIN32_WINNT=0x0A00','/Ibuild/ci-release','/Iresources',
        '/external:env:INCLUDE','/external:W0','/external:templates-','/analyze','/analyze:only','/analyze:external-',
        ('/analyze:log' + $analysis),'/c',('/Fo' + (Join-Path $private 'standalone.obj')),'src/standalone.cpp')
    $null = Invoke-PrivateCommand 'production-analysis' 'cl.exe' $arguments 540
    Assert-CiAnalysisReport $analysis
    Write-Output 'Production analysis: zero defects; warnings are errors; only toolchain headers are external.'
}

if ($Phase -eq 'Package') {
    $null = Invoke-PrivateCommand 'repro-configure' 'cmake.exe' @('-S','.', '-B','build/ci-repro','-G','Ninja','-DCMAKE_BUILD_TYPE=Release','-DDAC_BUILD_TESTS=OFF')
    $null = Invoke-PrivateCommand 'repro-build' 'cmake.exe' @('--build','build/ci-repro','--parallel','3')
    if ((Get-FileHash build/ci-release/DAC.exe).Hash -cne (Get-FileHash build/ci-repro/DAC.exe).Hash) { throw 'Independent Release executables differ.' }
    $null = Invoke-PrivateScript 'pe-integrity' 'verify-pe.ps1' @('-Executable','build/ci-release/DAC.exe')
    $null = Invoke-PrivateScript 'package-regression' 'test-package.ps1' @('-Executable','build/ci-release/DAC.exe')
    $null = Invoke-PrivateScript 'package-create' 'package.ps1' @('-BuildDirectory','build/ci-release','-PackageDirectory','build/ci-package')
    $null = Invoke-PrivateScript 'package-verify' 'verify-package.ps1' @('-Archive','build/ci-package/DAC-0.1.0-dev-windows-x64.zip')
    Write-Output 'Independent Release bytes and normalized package integrity: passed. No distribution artifact is uploaded.'
}

if ($Phase -eq 'Installer') {
    $bootstrap=@(Invoke-PrivateScript 'installer-bootstrap' 'bootstrap-installer.ps1' @() 180)
    $compiler=Get-Content -LiteralPath $bootstrap[-1] -Raw | ConvertFrom-Json
    $lock=Get-Content -LiteralPath tools/installer-toolchain-lock.json -Raw | ConvertFrom-Json
    if ($compiler.verification -cne 'passed' -or $compiler.authenticode -cne 'Valid' -or $compiler.inventoryFiles -ne 132 -or
        $compiler.downloadSha256 -cne $lock.sha256 -or $compiler.downloadBytes -ne $lock.bytes -or
        $compiler.compilerSha256 -cne $lock.compilerSha256) {throw 'Installer compiler evidence incomplete.'}
    $privacy=@(Invoke-PrivateScript 'installer-privacy-boundaries' 'test-installer.ps1' @('-AuditOnly') 60)
    if ((Get-Content -LiteralPath $privacy[-1] -Raw).Trim() -cne 'Installer privacy boundary checks passed: 8.') {throw 'Expected exactly eight privacy boundary checks.'}
    $reproDirectory=$InstallerDirectory+'-repro'
    $null=Invoke-PrivateScript 'installer-build' 'build-installer.ps1' @('-Archive',$archive,'-OutputDirectory',$InstallerDirectory) 180
    $null=Invoke-PrivateScript 'installer-independent-build' 'build-installer.ps1' @('-Archive',$archive,'-OutputDirectory',$reproDirectory) 180
    $first=$InstallerDirectory+'/DAC-Setup-0.1.0-dev-windows-x64.exe'
    $second=$reproDirectory+'/DAC-Setup-0.1.0-dev-windows-x64.exe'
    if ((Get-Item -LiteralPath $first).Length -gt 32MB -or (Get-Item -LiteralPath $second).Length -gt 32MB -or
        [Convert]::ToBase64String([IO.File]::ReadAllBytes((Join-Path $repository $first))) -cne [Convert]::ToBase64String([IO.File]::ReadAllBytes((Join-Path $repository $second)))) {throw 'Independent installer outputs differ.'}
    $receipts=@(foreach ($installer in @($first,$second)) {
        $log=@(Invoke-PrivateScript 'installer-verify' 'verify-installer.ps1' @('-Installer',$installer,'-Archive',$archive,'-RequireShipping') 120)
        $verified=Get-Content -LiteralPath $log[-1] -Raw | ConvertFrom-Json
        Assert-InstallerVerification $verified
        $verified
    })
    foreach ($property in @('sourceCommit','sourceTreeSha256','sourceFiles','workingTreeModified','packageSha256','compilerSha256','sha256')) {
        if ($receipts[0].$property -cne $receipts[1].$property) {throw 'Independent installer provenance differs.'}
    }
    $runtimePrivacy=@(Invoke-PrivateScript 'installer-runtime-privacy-regression' 'test-installer-runtime-privacy.ps1' @('-Installer',$first) 180)
    Assert-RuntimePrivacyRegression (Get-Content -LiteralPath $runtimePrivacy[-1] -Raw | ConvertFrom-Json)
    $regression=@(Invoke-PrivateScript 'installer-integrity-regression' 'test-installer-integrity.ps1' @('-Installer',$first,'-Archive',$archive) 180)
    $integrity=Get-Content -LiteralPath $regression[-1] -Raw | ConvertFrom-Json
    if ($integrity.verification -cne 'passed' -or $integrity.checks -ne 48 -or $integrity.compiledPayloadFiles -ne 19 -or
        $integrity.installationExecuted -ne $false -or $integrity.sourceAndToolchainMutated -ne $false) {throw 'Installer regression evidence incomplete.'}
    Write-Output 'Two unsigned installers: identical bytes; source/compiler/package provenance and all 19 decoded payload files verified. Installation acceptance deferred.'
}

if ($Phase -eq 'Audit') {
    $spec = Get-Content -LiteralPath tools/package-manifest.json -Raw | ConvertFrom-Json
    $entryList = Join-Path $private 'package-entries.txt'
    [IO.File]::WriteAllText($entryList, ($spec.files.path -join "`n") + "`n", [Text.UTF8Encoding]::new($false))
    # Full history from the reviewed publication root, plus final worktree and package.
    $log = @(Invoke-PrivateScript 'publication-privacy' 'audit-publication.ps1' @(
        '-Base','48f8d00b1eb2d46a6b0629e339d2716b4a0b76d0','-IncludeWorktree',
        '-PackageDirectory',(Join-Path (Split-Path $Archive -Parent) 'DAC-0.1.0-dev-windows-x64'),'-PackageEntryList',$entryList,
        '-Installer',($InstallerDirectory+'/DAC-Setup-0.1.0-dev-windows-x64.exe'),
        '-Report','out/private/ci/publication-audit.json') 540)
    $audit = Get-Content -LiteralPath $log[-1] -Raw | ConvertFrom-Json
    if ($audit.result -cne 'passed' -or $audit.findings -ne 0 -or $audit.packageFiles -ne 19 -or -not $audit.worktreeIncluded -or $audit.installerIncluded -ne $true -or
        $audit.installerEmbeddedContentChecked -ne $true -or $audit.installerPayloadFiles -ne 19) {
        throw 'Publication audit was incomplete or requires review.'
    }
    Write-Output 'History, final worktree, portable package, shipping installer, decoded runtime/metadata and 19 embedded files: zero privacy findings.'
}
Write-Output ('CI phase ' + $Phase + ': passed.')
