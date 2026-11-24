#Requires -Version 7.0
[CmdletBinding()]
param(
    [string]$Installer='', [string]$Archive='', [switch]$Capture,
    [ValidateSet('Unconfirmed','ManualRehearsed','ExternalDemonstrated')][string]$Recovery='Unconfirmed'
)
. (Join-Path $PSScriptRoot 'Safety.Common.ps1')
$findings = [Collections.Generic.List[object]]::new()
$detail = [ordered]@{schema=1;utc=[DateTime]::UtcNow.ToString('o');operatorRecoveryAssertion=$Recovery}
function Finding($Id,$Status,$Text) { $findings.Add((New-SafetyFinding $Id $Status $Text)) }
function Probe($Id,[scriptblock]$Read) {
    try { & $Read } catch { Finding $Id BLOCKED (Get-SafetyFailure $_) }
}
Probe artifact {
    if (!$Installer) { Finding artifact BLOCKED 'Supply the canonical Setup path and its adjacent receipts.'; return }
    try { $detail.artifact=Assert-SafetyArtifact $Installer $Archive; Finding artifact PASS 'Pinned Setup bytes, receipt, source, AMD64 GUI PE and unsigned state match.' }
    catch { Finding artifact FAIL (Get-SafetyFailure $_) }
    if (!$Archive) { Finding archive BLOCKED 'Supply the pinned package for complete payload comparison before installation.' }
}
Probe platform {
    $os = Get-CimInstance Win32_OperatingSystem -OperationTimeoutSec 10 -ErrorAction Stop
    $detail.os = [ordered]@{edition=$os.Caption;build=$os.BuildNumber;productType=$os.ProductType;architecture=[Runtime.InteropServices.RuntimeInformation]::OSArchitecture.ToString()}
    if ($os.ProductType -ne 1 -or [int]$os.BuildNumber -lt 22000 -or $detail.os.architecture -cne 'X64') {
        Finding platform FAIL 'Requires Windows 11 workstation on native AMD64.'
    } else { Finding platform PASS 'Windows workstation build and native AMD64 meet the installer baseline.' }
}
Probe identity {
    $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = [Security.Principal.WindowsPrincipal]::new($identity)
    $process = Get-Process -Id $PID
    $detail.identity = [ordered]@{machine=$env:COMPUTERNAME;sid=$identity.User.Value;account=$identity.Name;session=$process.SessionId;elevated=$principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)}
    $identity.Dispose()
    Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class DacSafetySession {
 [DllImport("kernel32.dll")] public static extern uint WTSGetActiveConsoleSessionId();
 [DllImport("user32.dll")] public static extern int GetSystemMetrics(int index);
}
'@
    $detail.identity.remote = [DacSafetySession]::GetSystemMetrics(0x1000) -ne 0
    $detail.identity.console = $process.SessionId -eq [DacSafetySession]::WTSGetActiveConsoleSessionId()
    if ($detail.identity.elevated -or !$detail.identity.console -or $detail.identity.remote -or $process.SessionId -eq 0) {
        Finding identity BLOCKED 'Requires confirmed unelevated interactive console session; current token/session is unsuitable.'
    } else { Finding identity PASS 'Current token is unelevated and in the active local console session; machine identity still needs operator confirmation.' }
}
Probe displays {
    # Read-only GDI inventory gives explicit adapter-to-monitor associations.
    # Bounds are the current PowerShell process coordinate space, not a promise
    # about physical DPI/pixels. Raw PnP identifiers stay in private evidence.
    Add-Type -AssemblyName System.Windows.Forms
    Add-Type -TypeDefinition @'
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
public static class DacSafetyDisplays {
 [StructLayout(LayoutKind.Sequential, CharSet=CharSet.Unicode)] public struct Device {
  public int cb; [MarshalAs(UnmanagedType.ByValTStr,SizeConst=32)] public string name;
  [MarshalAs(UnmanagedType.ByValTStr,SizeConst=128)] public string description;
  public uint flags; [MarshalAs(UnmanagedType.ByValTStr,SizeConst=128)] public string id;
  [MarshalAs(UnmanagedType.ByValTStr,SizeConst=128)] public string key;
 }
 [DllImport("user32.dll",CharSet=CharSet.Unicode)] static extern bool EnumDisplayDevices(string parent,uint i,ref Device d,uint flags);
 public static Device[] List(string parent) {
  var list=new List<Device>();
  for(uint i=0;i<128;i++) { var d=new Device();d.cb=Marshal.SizeOf<Device>();if(!EnumDisplayDevices(parent,i,ref d,1))return list.ToArray();list.Add(d); }
  throw new InvalidOperationException("Display enumeration limit");
 }
}
'@
    $detail.displays = @([Windows.Forms.Screen]::AllScreens | ForEach-Object { [ordered]@{name=$_.DeviceName;primary=$_.Primary;x=$_.Bounds.X;y=$_.Bounds.Y;width=$_.Bounds.Width;height=$_.Bounds.Height} })
    $detail.adapters = @([DacSafetyDisplays]::List($null) | ForEach-Object { [ordered]@{device=$_;monitors=@([DacSafetyDisplays]::List($_.name))} })
    $detail.drivers = @(Get-CimInstance Win32_VideoController -OperationTimeoutSec 10 | Select-Object Name,DriverVersion,PNPDeviceID)
    if ($detail.displays.Count -ne 3) { Finding displays BLOCKED 'Expected three active displays for this plan; operator must reconcile topology.' }
    else { Finding displays PASS 'Three active display bounds enumerated; physical OLED mapping must be confirmed by the operator.' }
}
Probe controllers {
    $processes = @(Get-CimInstance Win32_Process -OperationTimeoutSec 10 -Filter "Name = 'DAC.exe' OR Name LIKE '%windhawk%'" -ErrorAction Stop)
    $detail.controllers = @($processes | ForEach-Object {
        $owner = Invoke-CimMethod -InputObject $_ -MethodName GetOwnerSid -OperationTimeoutSec 10
        [ordered]@{pid=$_.ProcessId;session=$_.SessionId;path=$_.ExecutablePath;created=$_.CreationDate;sid=$owner.Sid;ownerResult=$owner.ReturnValue;name=$_.Name}
    })
    if ($processes.Count) { Finding controllers BLOCKED 'DAC or Windhawk candidates exist. Resolve each owner/path/session and active display mod manually; nothing is stopped.' }
    else { Finding controllers PASS 'No normally named DAC/Windhawk process observed at this instant.' }
    Finding discovery WARNING 'Name-based discovery cannot exclude renamed portable controllers or injected Windhawk modules. Operator inspection is mandatory.'
}
Probe state {
    $detail.state = Get-SafetyState
    if ($detail.state.product.Count -eq 0 -and $detail.state.uninstall.Count -eq 0) { Finding installation PASS 'No existing target installation or uninstall registration; normal first-install state.' }
    elseif ($detail.state.product.Count -gt 0 -and $detail.state.uninstall.Count -gt 0 -and
        $detail.state.uninstall.Contains('InstallLocation') -and
        [string]$detail.state.uninstall.InstallLocation.TrimEnd('\') -ieq $detail.state.target.TrimEnd('\') -and
        @($detail.state.product | Where-Object path -eq '.dac-lifecycle.lock').Count -eq 1) {
        Finding installation WARNING 'Managed installation candidate exists. Validate its payload, uninstaller and running-process lease before maintenance.'
    } else { Finding installation BLOCKED 'Existing files/registration do not establish an idle managed installation; do not adopt portable files.' }
    if ($detail.state.settings.Count) { Finding settings BLOCKED 'Existing preferences or recovery records require private review and an approved preservation/isolation plan. No configuration authority inferred from defaults.' }
    else { Finding settings PASS 'Default DAC settings directory has no files.' }
    if ($detail.state.startup.Count) { Finding startup BLOCKED 'DAC/Windhawk Run ownership exists; review without changing it.' }
    else { Finding startup PASS 'No DAC/Windhawk entry observed in the current-user Run key.' }
}
Probe alternateStartup {
    $detail.startupFolders = @()
    foreach ($folder in @([Environment]::GetFolderPath('Startup'),[Environment]::GetFolderPath('CommonStartup'))) {
        $detail.startupFolders += @(Get-SafetyFileInventory $folder)
    }
    $detail.tasks = @(Get-ScheduledTask -ErrorAction Stop | Where-Object { ($_.Actions.Execute -join '|') -match '(?i)DAC|Windhawk' } | Select-Object TaskPath,TaskName,State,Actions)
    if ($detail.tasks.Count -or $detail.startupFolders.Count) { Finding alternateStartup BLOCKED 'Startup-folder files or matching scheduled tasks require local target review. They are not changed.' }
    else { Finding alternateStartup PASS 'No startup-folder files or normally named DAC/Windhawk task actions observed.' }
}
Finding recovery BLOCKED 'Independent recovery is not automatically verified. Rehearse manual recovery and record external-channel demonstration on the confirmed machine before covered-screen tests.'
Finding authorization BLOCKED 'This is a read-only dry run. Machine identity and a separate installation-only approval are required; this report grants no execution authority.'
$detail.expectedEffects = @(([string]$script:SafetyIdentity.payloadFiles + ' pinned product files in current-user Programs/DAC'),'Empty maintenance sidecar and Inno uninstaller EXE/DAT','Current-user Start Menu shortcut and uninstall key','Optional desktop shortcut unchecked','No product launch, login startup, automation or DDC enablement','Existing settings and recovery records preserved')
$summary = Get-SafetySummary $findings
$summary.expectedEffects = $detail.expectedEffects
if ($Capture) { $summary.evidenceId = Save-SafetyEvidence $summary $detail }
$summary | ConvertTo-Json -Depth 8
if ($summary.status -eq 'FAIL') { exit 3 }; if ($summary.status -eq 'BLOCKED') { exit 2 }; exit 0
