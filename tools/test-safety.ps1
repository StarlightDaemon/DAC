#Requires -Version 7.0
[CmdletBinding()]
param([Parameter(Mandatory)][string]$Installer,[Parameter(Mandatory)][string]$Archive)
. (Join-Path $PSScriptRoot 'Safety.Common.ps1')
$checks=[Collections.Generic.List[string]]::new()
$fixture=Join-Path ([IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../out/private/safety-tests'))) ([Guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($fixture)
function Check([bool]$Condition,[string]$Name) { if (!$Condition) {throw "Safety fixture failed: $Name"}; $checks.Add($Name) }
function Reject([scriptblock]$Action,[string]$Name) { $rejected=$false; try { $null=& $Action } catch {$rejected=$true}; Check $rejected $Name }
$receipt=Assert-SafetyArtifact $Installer $Archive
Check ($receipt.files.Count -eq $script:SafetyIdentity.payloadFiles) 'canonical artifact and complete package accepted without execution'
foreach ($path in @('relative.exe','\\server\share\file','C:\file:stream','\\?\C:\file')) { Reject {Resolve-SafetyPath $path} 'nonlocal or ambiguous path rejected' }
Check (@(Get-SafetyFileInventory (Join-Path $fixture 'missing')).Count -eq 0) 'missing installation is normal'
$copy=Join-Path $fixture 'DAC-Setup-0.1.0-dev-windows-x64.exe'
Copy-Item -LiteralPath $Installer -Destination $copy
foreach ($suffix in @('.verification.json','.sha256')) { Copy-Item -LiteralPath ($Installer+$suffix) -Destination ($copy+$suffix) }
$bytes=[IO.File]::ReadAllBytes($copy); $bytes[512]=$bytes[512] -bxor 1; [IO.File]::WriteAllBytes($copy,$bytes)
Reject {Assert-SafetyArtifact $copy $Archive} 'wrong installer hash rejected'
Copy-Item -LiteralPath $Installer -Destination $copy -Force
[IO.File]::AppendAllText(($copy+'.verification.json'),' ')
Reject {Assert-SafetyArtifact $copy $Archive} 'changed adjacent receipt rejected'
Copy-Item -LiteralPath ($Installer+'.verification.json') -Destination ($copy+'.verification.json') -Force
[IO.File]::AppendAllText(($copy+'.sha256'),' ')
Reject {Assert-SafetyArtifact $copy $Archive} 'changed checksum receipt rejected'
$badZip=Join-Path $fixture 'bad.zip'; [IO.File]::WriteAllText($badZip,'invalid')
Reject {Assert-SafetyArtifact $Installer $badZip} 'wrong package rejected'
$expanded=Join-Path $fixture 'expanded'; [IO.Compression.ZipFile]::ExtractToDirectory($Archive,$expanded)
$installed=Join-Path $expanded 'DAC-0.1.0-dev-windows-x64'
[IO.File]::WriteAllBytes((Join-Path $installed '.dac-lifecycle.lock'),[byte[]]@())
# Synthetic maintenance files establish inventory separation only, not an Inno pass.
[IO.File]::WriteAllBytes((Join-Path $installed 'unins000.exe'),[byte[]]::new(128))
[IO.File]::WriteAllText((Join-Path $installed 'unins000.dat'),'fixture')
Check (@(Assert-SafetyInstalledPayload $receipt $installed).Count -eq 3) 'all pinned product files verified separately from three maintenance files'
[IO.File]::AppendAllText((Join-Path $installed 'DAC.exe'),'tamper')
Reject {Assert-SafetyInstalledPayload $receipt $installed} 'installed executable tamper rejected'
$entry=[IO.Compression.ZipFile]::OpenRead($Archive)
try { [IO.Compression.ZipFileExtensions]::ExtractToFile($entry.GetEntry('DAC-0.1.0-dev-windows-x64/DAC.exe'),(Join-Path $installed 'DAC.exe'),$true) } finally {$entry.Dispose()}
[IO.File]::WriteAllText((Join-Path $installed 'foreign.txt'),'preserve')
Reject {Assert-SafetyInstalledPayload $receipt $installed} 'extra file rejected without deletion'
Check ([IO.File]::ReadAllText((Join-Path $installed 'foreign.txt')) -ceq 'preserve') 'unknown installed file preserved'
$junction=Join-Path $fixture 'junction'
$null=New-Item -ItemType Junction -Path $junction -Target $installed
Reject {Get-SafetyFileInventory $junction} 'reparse root rejected'
Reject {Resolve-SafetyPath (Join-Path $junction 'missing/child')} 'reparse ancestor with missing leaf rejected'
$canary='PRIVATE_SAFETY_CANARY'
$summary=Get-SafetySummary @((New-SafetyFinding unavailable BLOCKED 'Observation unavailable.'))
$id=Save-SafetyEvidence $summary @{private=$canary;credential=$canary;windowTitle=$canary;monitorId=$canary}
$public=Get-Content -LiteralPath (Join-Path $PSScriptRoot "../out/private/safety/$id/summary.json") -Raw
Check (!$public.Contains($canary)) 'public evidence uses explicit allowlist, no private values'
$before=Get-SafetyState | ConvertTo-Json -Depth 12 -Compress
$report=(& (Join-Path $PSScriptRoot 'safety-preflight.ps1') -Installer $Installer -Archive $Archive | Out-String) | ConvertFrom-Json
$after=Get-SafetyState | ConvertTo-Json -Depth 12 -Compress
Check ($before -ceq $after) 'read-only preflight leaves product settings startup and uninstall registration unchanged'
Check (!$report.installationAuthorized -and $report.status -eq 'BLOCKED') 'preflight never authorizes installation or claims recovery'
$baseline=Join-Path $fixture 'baseline.json'
[IO.File]::WriteAllText($baseline,(@{schema=1;identity=@{sid='invalid'};state=@{}} | ConvertTo-Json -Depth 5))
$post=(& (Join-Path $PSScriptRoot 'safety-verify-installed.ps1') -Installer $Installer -Archive $Archive -Baseline $baseline | Out-String) | ConvertFrom-Json
Check ($post.status -eq 'FAIL') 'untrusted or incomplete post-install baseline rejects qualification'
Check ($before -ceq (Get-SafetyState | ConvertTo-Json -Depth 12 -Compress)) 'post-install verification does not modify state'
function Get-CimInstance { throw $canary }
function Get-ScheduledTask { throw $canary }
$deniedText=(& (Join-Path $PSScriptRoot 'safety-preflight.ps1') -Installer $Installer -Archive $Archive | Out-String)
$denied=$deniedText | ConvertFrom-Json
Check (@($denied.findings | Where-Object { $_.id -eq 'platform' -and $_.status -eq 'BLOCKED' }).Count -eq 1) 'denied platform observation blocks safely'
Check (@($denied.findings | Where-Object { $_.id -eq 'controllers' -and $_.status -eq 'BLOCKED' }).Count -eq 1) 'denied process inventory blocks safely'
Check (!$deniedText.Contains($canary)) 'exception content never reaches public summary'
Remove-Item Function:Get-CimInstance,Function:Get-ScheduledTask
$me=Get-Process -Id $PID
$foreign=(& (Join-Path $PSScriptRoot 'safety-observe.ps1') -ProcessId $PID -Executable $me.Path -StartUtc $me.StartTime.ToUniversalTime() -Seconds 1 | Out-String) | ConvertFrom-Json
Check ($foreign.status -eq 'BLOCKED' -and !$me.HasExited) 'foreign process rejected and survives'
# Parse all shipped tooling. The reviewed mutation/launch surface must remain empty.
foreach ($name in @('Safety.Common.ps1','safety-preflight.ps1','safety-verify-installed.ps1','safety-observe.ps1')) {
    $tokens=$null;$errors=$null
    $ast=[Management.Automation.Language.Parser]::ParseFile((Join-Path $PSScriptRoot $name),[ref]$tokens,[ref]$errors)
    Check ($errors.Count -eq 0) ($name+' parses')
    $text=$ast.Extent.Text
    Check ($text -notmatch '(?i)Start-Process|Stop-Process|TerminateProcess|TerminateJobObject|\.Kill\s*\(|::Start\s*\(|SetVCPFeature|SetMonitorPower|--power-helper|--stop|Set-ItemProperty|Register-ScheduledTask|CreateService|\.Save\s*\(') ($name+' has no active product, recovery or hardware operation')
}
$result=[ordered]@{verification='passed';checks=$checks.Count;passed=$checks.ToArray();productExecuted=$false;physicalDesktopQualified=$false}
[IO.File]::WriteAllText((Join-Path $fixture 'results.json'),($result | ConvertTo-Json -Depth 6))
$result | ConvertTo-Json -Depth 6
exit 0
