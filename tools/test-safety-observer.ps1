#Requires -Version 7.0
[CmdletBinding()]
param([Parameter(Mandatory)][string]$Archive,[string]$BuildDirectory='build/safety-release',
      [switch]$Worker,[string]$Fixture='')
. (Join-Path $PSScriptRoot 'Safety.Common.ps1')
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if (!$Worker) {
    if ((Get-SafetyDigest $Archive) -cne $script:SafetyIdentity.archive) { throw 'Wrong fixture archive.' }
    $Fixture=Join-Path $root ('out/private/safety-observer/'+[Guid]::NewGuid().ToString('N'))
    [void][IO.Directory]::CreateDirectory($Fixture)
    [IO.Compression.ZipFile]::ExtractToDirectory($Archive,$Fixture)
    $runner=Resolve-SafetyPath ([IO.Path]::GetFullPath((Join-Path $root ($BuildDirectory+'/dac-test-runner.exe'))))
    & $runner (Get-Process -Id $PID).Path -NoProfile -File $PSCommandPath -Archive $Archive -Worker -Fixture $Fixture
    if ($LASTEXITCODE -ne 0) { throw 'Private-desktop observer fixture failed.' }
    Get-Content -LiteralPath (Join-Path $Fixture 'results.json') -Raw
    exit 0
}
$Fixture=Resolve-SafetyPath $Fixture
if (!$Fixture.StartsWith((Join-Path $root 'out/private/safety-observer/'),[StringComparison]::OrdinalIgnoreCase)) { throw 'Fixture location rejected.' }
Add-Type -TypeDefinition @'
using System;
using System.Text;
using System.Runtime.InteropServices;
public static class DacObserverFixture {
 [DllImport("kernel32.dll")] static extern uint GetCurrentThreadId();
 [DllImport("kernel32.dll")] static extern IntPtr GetCurrentProcess();
 [DllImport("kernel32.dll",SetLastError=true)] static extern bool IsProcessInJob(IntPtr process,IntPtr job,out bool member);
 [DllImport("user32.dll")] static extern IntPtr GetThreadDesktop(uint thread);
 [DllImport("user32.dll",CharSet=CharSet.Unicode,SetLastError=true)] static extern bool GetUserObjectInformation(IntPtr handle,int index,StringBuilder value,int size,out int needed);
 public static bool Isolated() {
  var name=new StringBuilder(512);int needed;bool member;
  return IsProcessInJob(GetCurrentProcess(),IntPtr.Zero,out member)&&member&&
   GetUserObjectInformation(GetThreadDesktop(GetCurrentThreadId()),2,name,1024,out needed)&&
   name.ToString().StartsWith("DAC-Isolated-Test-",StringComparison.Ordinal);
 }
}
'@
if (![DacObserverFixture]::Isolated()) { throw 'Requires the existing contained private-desktop runner.' }
if (@(Get-CimInstance Win32_Process -Filter "Name = 'DAC.exe'" -OperationTimeoutSec 10).Count) { throw 'Existing DAC candidate; fixture will not compete.' }
$before=Get-SafetyState | ConvertTo-Json -Depth 12 -Compress
$exe=Join-Path $Fixture 'DAC-0.1.0-dev-windows-x64/DAC.exe'
if ((Get-SafetyDigest $exe) -cne $script:SafetyIdentity.executable) { throw 'Fixture executable rejected.' }
$profile=Join-Path $Fixture 'profile'; [void][IO.Directory]::CreateDirectory($profile)
$start=[Diagnostics.ProcessStartInfo]::new($exe)
$start.UseShellExecute=$false; $start.CreateNoWindow=$true
foreach ($arg in @('--background','--config-directory',$profile,'--run-for-seconds','12')) { $start.ArgumentList.Add($arg) }
$process=[Diagnostics.Process]::Start($start)
try {
    $null=$process.Handle; $started=$process.StartTime.ToUniversalTime()
    $observer=Join-Path $PSScriptRoot 'safety-observe.ps1'
    $wrong=(& $observer -ProcessId $process.Id -Executable $exe -StartUtc $started.AddTicks(1) -Seconds 1 | Out-String) | ConvertFrom-Json
    if ($wrong.status -ne 'BLOCKED' -or $process.HasExited) { throw 'Stale/PID-reuse identity not rejected safely.' }
    function Invoke-CimMethod { [pscustomobject]@{ReturnValue=0;Sid='S-1-0-0'} }
    $foreign=(& $observer -ProcessId $process.Id -Executable $exe -StartUtc $started -Seconds 1 | Out-String) | ConvertFrom-Json
    Remove-Item Function:Invoke-CimMethod
    if ($foreign.status -ne 'BLOCKED' -or $process.HasExited) { throw 'Foreign ownership not rejected safely.' }
    $observed=(& $observer -ProcessId $process.Id -Executable $exe -StartUtc $started -Seconds 20 -Capture | Out-String) | ConvertFrom-Json
    if (!$observed.automated.identityMatched -or !$observed.automated.exitObserved -or $observed.automated.samples -lt 1 -or !$process.WaitForExit(1000) -or $process.ExitCode -ne 0) { throw 'Bounded passive observation failed.' }
    $after=Get-SafetyState | ConvertTo-Json -Depth 12 -Compress
    if ($before -cne $after) { throw 'Production state changed.' }
    [ordered]@{verification='passed';checks=4;privateDesktop=$true;staleStartRejected=$true;foreignOwnerRejected=$true;retainedExitObserved=$true;productionStateUnchanged=$true;physicalDesktopQualified=$false} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $Fixture 'results.json')
} finally { $process.Dispose() } # Existing runner contains only its own fixture job.
