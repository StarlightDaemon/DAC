#Requires -Version 7.0
[CmdletBinding()]
param(
    [string]$Archive='build/ci-package/DAC-0.1.0-dev-windows-x64.zip',
    [string]$BuildDirectory='build/ci-release',
    [switch]$AuditOnly,
    [switch]$Worker,
    [string]$FixtureRoot='',
    [string]$FixtureId=''
)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$repository=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
. (Join-Path $PSScriptRoot 'Package.Common.ps1')
if ($AuditOnly) {
    # Load production scan functions and public rules without running the auditor
    # or mutating a source, compiler, registry or environment identity.
    $tokens=$null;$errors=$null
    $ast=[Management.Automation.Language.Parser]::ParseFile((Join-Path $PSScriptRoot 'audit-publication.ps1'),[ref]$tokens,[ref]$errors)
    if ($errors.Count) {throw 'Privacy auditor syntax failed.'}
    foreach ($definition in $ast.FindAll({param($node) $node -is [Management.Automation.Language.FunctionDefinitionAst]},$false)) {
        . ([scriptblock]::Create($definition.Extent.Text))
    }
    $assignment=$ast.FindAll({param($node) $node -is [Management.Automation.Language.AssignmentStatementAst] -and $node.Left.Extent.Text -ceq '$publicRules'},$false)
    if ($assignment.Count -ne 1) {throw 'Public privacy rules unavailable.'}
    . ([scriptblock]::Create($assignment[0].Extent.Text))
    $utf8=[Text.UTF8Encoding]::new($false,$true)
    $vendor=Join-Path $repository '.tools/inno-7.1.0/Setup.e64'
    if ((Get-FileHash -LiteralPath $vendor).Hash.ToLowerInvariant() -cne 'ad12a06d09afefa9d1283c6616ef4d56289dc14a7137a12b24653a12637216bb') {throw 'Vendor identity test lacks the pinned runtime.'}
    $vendorStrings=[regex]::Matches([Text.Encoding]::Latin1.GetString([IO.File]::ReadAllBytes($vendor)),'[ -~]{4,}')
    $publicToken=@($vendorStrings | Where-Object {(Hash-Bytes ($utf8.GetBytes($_.Value))) -ceq '67e7c6e28e540b4fd412ad663b634a38572b98d3f4608fc5792f2600d32c1eb9'})[0].Value
    $compiledRules=@($publicRules | ForEach-Object {[pscustomobject]@{name=$_.name;regex=[regex]::new($_.pattern)}})
    $compiledRules+=[pscustomobject]@{name='private-runtime-identity';regex=[regex]::new('(?<![\p{L}\p{N}_])'+[regex]::Escape($publicToken)+'(?![\p{L}\p{N}_])',[Text.RegularExpressions.RegexOptions]::IgnoreCase)}
    $compiledRules+=[pscustomobject]@{name='private-runtime-identity';regex=[regex]::new('PRIVATEIDENTITYFIXTURE')}
    $statistics=[ordered]@{genericWindowsOsPathReferences=0;syntheticRedactionFixtures=0;reviewedPublicVendorTokens=0}
    $privatePath=@('C:','Users','private-fixture','AppData','sensitive') -join '\'
    $cases=@(
        @{text=$publicToken;scope='installer';encoding='ascii-strings';vendor=$true;expected=0},
        @{text=$publicToken;scope='worktree';encoding='utf8';vendor=$true;expected=1},
        @{text=('prefix '+$publicToken);scope='installer';encoding='ascii-strings';vendor=$true;expected=1},
        @{text=$publicToken;scope='installer';encoding='utf16le-strings-offset-0';vendor=$true;expected=1},
        @{text=$publicToken;scope='installer';encoding='ascii-strings';vendor=$false;expected=1},
        @{text=$privatePath;scope='installer';encoding='ascii-strings';vendor=$true;expected=1},
        @{text=('ghp'+'_'+('a'*30));scope='installer';encoding='ascii-strings';vendor=$true;expected=1},
        @{text='PRIVATEIDENTITYFIXTURE';scope='installer';encoding='ascii-strings';vendor=$true;expected=1}
    )
    foreach ($case in $cases) {
        $findings=[Collections.Generic.List[object]]::new()
        $reviewedVendorToken=$case.vendor
        Scan-Text $case.text $case.scope 'fixture' $case.encoding
        if (($case.expected -eq 0 -and $findings.Count -ne 0) -or ($case.expected -gt 0 -and $findings.Count -eq 0)) {throw 'Privacy exception boundary regression.'}
    }
    Write-Output ('Installer privacy boundary checks passed: '+$cases.Count+'.')
    exit 0
}
$archivePath=Resolve-PackageLocalPath $repository $Archive
$build=Resolve-PackageLocalPath $repository $BuildDirectory
$runner=Join-Path $build 'dac-test-runner.exe'
if (-not $Worker) {
    $FixtureId=[Guid]::NewGuid().ToString('N')
    $FixtureRoot=Resolve-PackageLocalPath $repository ('out/private/installer/'+$FixtureId)
    [void][IO.Directory]::CreateDirectory($FixtureRoot)
    $null=& (Join-Path $PSScriptRoot 'build-installer.ps1') -Archive $archivePath -OutputDirectory ($FixtureRoot+'/compiler') -FixtureRoot $FixtureRoot -FixtureId $FixtureId
    . (Join-Path $PSScriptRoot 'use-toolchain.ps1')
    # No new CTest names or product-suite dilution. Compile only the lease fixture.
    $null=& cl.exe /nologo /std:c++latest /MT /W4 /WX /sdl /guard:cf /EHsc /utf-8 /DUNICODE /D_UNICODE /DNOMINMAX /D_WIN32_WINNT=0x0A00 /DWIN32_LEAN_AND_MEAN (Join-Path $repository 'tests/installer-lease.cpp') ('/Fe'+$FixtureRoot+'\lease-holder.exe') ('/Fo'+$FixtureRoot+'\lease-holder.obj') /link /DYNAMICBASE /NXCOMPAT /HIGHENTROPYVA /guard:cf
    if ($LASTEXITCODE -ne 0) {throw 'Lease fixture compilation failed.'}
    $pwsh=(Get-Process -Id $PID).Path
    & $runner $pwsh -NoProfile -File $PSCommandPath -Archive $archivePath -BuildDirectory $build -Worker -FixtureRoot $FixtureRoot -FixtureId $FixtureId
    if ($LASTEXITCODE -ne 0) {throw 'Installer private-desktop fixture failed.'}
    Get-Content -LiteralPath (Join-Path $FixtureRoot 'results.json') -Raw
    exit 0
}
if ($FixtureId -cnotmatch '^[0-9a-f]{32}$') {throw 'Missing fixture registry identity.'}
$FixtureRoot=Resolve-PackageLocalPath $repository $FixtureRoot
if (-not $FixtureRoot.StartsWith((Join-Path $repository 'out/private/installer')+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) {throw 'Invalid private fixture root.'}
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class DacInstallerFixture {
  [DllImport("kernel32.dll", CharSet=CharSet.Unicode, SetLastError=true)]
  public static extern IntPtr CreateFile(string name,uint access,uint share,IntPtr security,uint creation,uint flags,IntPtr template);
  [DllImport("kernel32.dll")] public static extern bool CloseHandle(IntPtr handle);
  [DllImport("advapi32.dll",SetLastError=true)] static extern bool OpenProcessToken(IntPtr process,uint access,out IntPtr token);
  [DllImport("kernel32.dll")] static extern IntPtr GetCurrentProcess();
  [DllImport("advapi32.dll",SetLastError=true)] static extern bool GetTokenInformation(IntPtr token,int kind,out int data,int length,out int returned);
  public static bool StandardUser() {
    IntPtr token;
    if(!OpenProcessToken(GetCurrentProcess(),8,out token))return false;
    try {int elevated,length;return GetTokenInformation(token,20,out elevated,4,out length)&&elevated==0;}
    finally {CloseHandle(token);}
  }
}
'@
$checks=[Collections.Generic.List[string]]::new()
function Expect-Rejection([scriptblock]$Action,[string]$Name) {
    $rejected=$false
    try {$null=& $Action} catch {$rejected=$true}
    Check $rejected $Name
}
function Check([bool]$Condition,[string]$Name) {
    if (-not $Condition) {throw ('Fixture failed: '+$Name)}
    $checks.Add($Name)
}
function Run([string]$Executable,[string[]]$Arguments,[int]$Expected=0) {
    $operation=[Guid]::NewGuid().ToString('N')
    $name=[IO.Path]::GetFileName($Executable)
    if ($name -match '^(DAC-Setup|unins)') {$Arguments+=('/LOG='+$FixtureRoot+'\'+$operation+'.setup.log')}
    $start=[Diagnostics.ProcessStartInfo]::new($Executable)
    $start.UseShellExecute=$false;$start.CreateNoWindow=$true
    $start.RedirectStandardOutput=$true;$start.RedirectStandardError=$true
    foreach ($argument in $Arguments) {$start.ArgumentList.Add($argument)}
    $process=[Diagnostics.Process]::Start($start)
    $out=$process.StandardOutput.ReadToEndAsync();$err=$process.StandardError.ReadToEndAsync()
    if (-not $process.WaitForExit(20000)) {$process.Kill($true);throw 'Owned fixture process timed out.'}
    [IO.File]::WriteAllText((Join-Path $FixtureRoot ($operation+'.process.log')),$out.GetAwaiter().GetResult()+$err.GetAwaiter().GetResult())
    [IO.File]::AppendAllText((Join-Path $FixtureRoot 'operations.jsonl'),([ordered]@{name=$name;expected=$Expected;actual=$process.ExitCode;arguments=$Arguments;log=$operation}|ConvertTo-Json -Compress)+[Environment]::NewLine)
    if ($Expected -lt 0) {Check ($process.ExitCode -ne 0) 'operation safely refused'}
    else {Check ($process.ExitCode -eq $Expected) ($name+' expected '+$Expected+' observed '+$process.ExitCode)}
    return $process.ExitCode
}
function Holder([string]$Directory,[string]$Name) {
    $ready=Join-Path $FixtureRoot ($Name+'.ready')
    $release=Join-Path $FixtureRoot ($Name+'.release')
    $start=[Diagnostics.ProcessStartInfo]::new((Join-Path $FixtureRoot 'lease-holder.exe'))
    $start.UseShellExecute=$false;$start.CreateNoWindow=$true
    foreach ($argument in @($Directory,$ready,$release)) {$start.ArgumentList.Add($argument)}
    $process=[Diagnostics.Process]::Start($start)
    $until=[DateTime]::UtcNow.AddSeconds(5)
    while (-not (Test-Path -LiteralPath $ready)) {
        if ($process.HasExited -or [DateTime]::UtcNow -gt $until) {throw 'Lease holder readiness failed.'}
        Start-Sleep -Milliseconds 10
    }
    return [pscustomobject]@{process=$process;release=$release}
}
function Release($Holder) {
    [IO.File]::WriteAllText($Holder.release,'release')
    Check ($Holder.process.WaitForExit(5000) -and $Holder.process.ExitCode -eq 0) 'retained process handle confirms complete exit'
}
function WaitMaintenance([string]$Path) {
    $until=[DateTime]::UtcNow.AddSeconds(5)
    do {
        $handle=[DacInstallerFixture]::CreateFile($Path,[uint32]2147483648,0,[IntPtr]::Zero,3,0x00200000,[IntPtr]::Zero)
        if ($handle -ne [IntPtr](-1)) {
            [void][DacInstallerFixture]::CloseHandle($handle)
            Check $true 'uninstall descendant cleanup released maintenance exclusion'
            return
        }
        Start-Sleep -Milliseconds 20
    } while ([DateTime]::UtcNow -lt $until)
    throw 'Uninstall cleanup retained maintenance exclusion beyond its bound.'
}
function ProductHost([string]$Executable,[string]$ProfileDirectory,[int]$Seconds) {
    [void][IO.Directory]::CreateDirectory($ProfileDirectory)
    $start=[Diagnostics.ProcessStartInfo]::new($Executable)
    $start.UseShellExecute=$false;$start.CreateNoWindow=$true
    foreach ($argument in @('--config-directory',$ProfileDirectory,'--background','--run-for-seconds',$Seconds.ToString())) {$start.ArgumentList.Add($argument)}
    return [Diagnostics.Process]::Start($start)
}
function TreeHashes([string]$Root) {
    if (-not (Test-Path -LiteralPath $Root)) {return ''}
    return (@(Get-ChildItem -LiteralPath $Root -File -Recurse -Force | ForEach-Object {[IO.Path]::GetRelativePath($Root,$_.FullName)+' '+(Get-FileHash -LiteralPath $_.FullName).Hash} | Sort-Object) -join '|')
}
Check ([DacInstallerFixture]::StandardUser()) 'standard-user non-elevated token'
$livePreferences=Join-Path $env:LOCALAPPDATA 'DAC'
$beforePreferences=TreeHashes $livePreferences
$liveRun=[Microsoft.Win32.Registry]::CurrentUser.OpenSubKey('Software\Microsoft\Windows\CurrentVersion\Run')
$beforeRun=if ($liveRun) {$liveRun.GetValue('DAC',$null,[Microsoft.Win32.RegistryValueOptions]::DoNotExpandEnvironmentNames)} else {$null}
if ($liveRun) {$liveRun.Dispose()}
$setup=Join-Path $FixtureRoot 'compiler/DAC-Setup-0.1.0-dev-windows-x64-fixture.exe'
$installed=Join-Path $FixtureRoot 'installed'
$silent=@('/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART')
$registryPath='Software\DAC-Installer-Fixtures\'+$FixtureId
$uninstallPath=$registryPath+'\Software\Microsoft\Windows\CurrentVersion\Uninstall\{BB5697BB-B07F-4DD9-BD9B-8991BDF19A64}_is1'
$runPath=$registryPath+'\Software\Microsoft\Windows\CurrentVersion\Run'
# First install must not adopt a portable executable, even if it is idle.
[void][IO.Directory]::CreateDirectory($installed)
Copy-Item -LiteralPath (Join-Path $build 'DAC.exe') -Destination (Join-Path $installed 'DAC.exe')
$portableHash=(Get-FileHash -LiteralPath (Join-Path $installed 'DAC.exe')).Hash
$null=Run $setup $silent -1
Check ((Get-FileHash -LiteralPath (Join-Path $installed 'DAC.exe')).Hash -ceq $portableHash) 'unmanaged portable bytes preserved'
# Only this exact file, which the fixture just created, is removed.
Remove-Item -LiteralPath (Join-Path $installed 'DAC.exe')
$null=Run $setup $silent
$null=& (Join-Path $PSScriptRoot 'verify-installer.ps1') -Installer $setup -Archive $archivePath -InstalledDirectory $installed
Check $true 'exact complete installed payload and hashes'
$unexpected=Join-Path $installed 'private-raw.log'
[IO.File]::WriteAllText($unexpected,'not product data')
Expect-Rejection {& (Join-Path $PSScriptRoot 'verify-installer.ps1') -Installer $setup -Archive $archivePath -InstalledDirectory $installed} 'undeclared installed file rejected'
Remove-Item -LiteralPath $unexpected
$readme=Join-Path $installed 'README.md'
$originalReadme=[IO.File]::ReadAllBytes($readme)
[IO.File]::AppendAllText($readme,'tampered')
Expect-Rejection {& (Join-Path $PSScriptRoot 'verify-installer.ps1') -Installer $setup -Archive $archivePath -InstalledDirectory $installed} 'tampered installed payload rejected'
[IO.File]::WriteAllBytes($readme,$originalReadme)
Check (Test-Path -LiteralPath (Join-Path $FixtureRoot 'shortcuts/DAC.lnk')) 'Start Menu shortcut created in fixture'
Check (-not (Test-Path -LiteralPath (Join-Path $FixtureRoot 'shortcuts/Desktop-DAC.lnk'))) 'desktop shortcut defaults off'
$key=[Microsoft.Win32.Registry]::CurrentUser.OpenSubKey($uninstallPath)
Check ($null -ne $key -and $key.GetValue('DisplayName') -ceq 'DAC isolated installer fixture') 'normal Installed Apps entry in redirected HKCU'
$key.Dispose()
$uninstaller=Join-Path $installed 'unins000.exe'
$product=Join-Path $installed 'DAC.exe'
$lease=Join-Path $installed '.dac-lifecycle.lock'
# Exercise the actual shipping entry point as a bounded host on this private
# desktop with a fresh profile, automation/hardware off and no login changes.
$actualHost=ProductHost $product (Join-Path $FixtureRoot 'installed-profile') 6
$until=[DateTime]::UtcNow.AddSeconds(3)
do {
    $probe=[DacInstallerFixture]::CreateFile($lease,[uint32]2147483648,0,[IntPtr]::Zero,3,0x00200000,[IntPtr]::Zero)
    if ($probe -eq [IntPtr](-1)) {break}
    [void][DacInstallerFixture]::CloseHandle($probe)
    Start-Sleep -Milliseconds 20
} while (-not $actualHost.HasExited -and [DateTime]::UtcNow -lt $until)
Check ($probe -eq [IntPtr](-1) -and -not $actualHost.HasExited) 'shipping installed host acquired lifetime lease'
$null=Run $setup $silent -1
$null=Run $uninstaller $silent -1
Check (-not $actualHost.HasExited) 'maintenance never terminated shipping installed host'
Check ($actualHost.WaitForExit(10000) -and $actualHost.ExitCode -eq 0) 'shipping installed host completed its own bounded lifetime'
$installedHost=Holder $installed 'host-after-singleton'
$helper=Holder $installed 'outstanding-helper'
$portable=Join-Path $FixtureRoot 'portable'
[void][IO.Directory]::CreateDirectory($portable)
$unrelated=Holder $portable 'unrelated-portable'
try {
    $baseline=(Get-FileHash -LiteralPath $product).Hash
    $null=Run $setup $silent -1
    $null=Run $uninstaller $silent -1
    Check ((Get-FileHash -LiteralPath $product).Hash -ceq $baseline) 'conflicts leave executable unchanged'
    Release $installedHost
    $null=Run $setup $silent -1
    Check (-not $helper.process.HasExited) 'outstanding helper still blocks after host exit'
    Release $helper
    Check (-not $unrelated.process.HasExited) 'portable process not stopped by failed maintenance'
    $exclusive=[DacInstallerFixture]::CreateFile($lease,[uint32]2147483648,0,[IntPtr]::Zero,3,0x00200000,[IntPtr]::Zero)
    Check ($exclusive -ne [IntPtr](-1)) 'exclusive maintenance lease acquired'
    try {
        $null=Run $product @('--version') 5
        $null=Run $product @('--power-helper') 5
        $null=Run $setup $silent -1
        $null=Run $uninstaller $silent -1
        Check $true 'concurrent installed launch helper install uninstall refused'
    } finally {[void][DacInstallerFixture]::CloseHandle($exclusive)}
    $null=Run $product @('--version')
    $null=Run $setup ($silent+@('/DIR='+$FixtureRoot+'\wrong')) -1
    $null=Run $setup ($silent+@('/CLOSEAPPLICATIONS')) -1
    $null=Run $setup ($silent+@('/TASKS=desktopicon'))
    $null=& (Join-Path $PSScriptRoot 'verify-installer.ps1') -Installer $setup -Archive $archivePath -InstalledDirectory $installed
    Check (Test-Path -LiteralPath (Join-Path $FixtureRoot 'shortcuts/Desktop-DAC.lnk')) 'optional desktop shortcut works'
    Check $true 'upgrade and post-failure retry preserve exact payload'
    # Representative unknown state inside the app directory must also survive.
    $state=@('settings-v1.ini','settings-v1.ini.host','profile-backup.ini','recovery.power','fault.quarantine')
    foreach ($name in $state) {[IO.File]::WriteAllText((Join-Path $installed $name),'fixture preserved state '+$name)}
    $stateHashes=@{};foreach ($name in $state) {$stateHashes[$name]=(Get-FileHash -LiteralPath (Join-Path $installed $name)).Hash}
    $run=[Microsoft.Win32.Registry]::CurrentUser.CreateSubKey($runPath)
    $owned='"'+$product+'" --background'
    $run.SetValue('DAC',$owned,[Microsoft.Win32.RegistryValueKind]::String)
    $null=Run $uninstaller $silent -1
    Check ($run.GetValue('DAC') -ceq $owned) 'owned login startup blocks uninstall without registry mutation'
    $foreign='"'+(Join-Path $portable 'DAC.exe')+'" --background'
    $run.SetValue('DAC',$foreign,[Microsoft.Win32.RegistryValueKind]::String)
    $null=Run $setup $silent
    Check ($run.GetValue('DAC') -ceq $foreign) 'upgrade preserves foreign startup ownership'
    $null=Run $uninstaller $silent
    WaitMaintenance $lease
    Check ($run.GetValue('DAC') -ceq $foreign) 'uninstall preserves foreign startup ownership'
    $run.Dispose()
    Check (-not (Test-Path -LiteralPath $product)) 'product payload removed by uninstall'
    foreach ($name in $state) {Check ((Get-FileHash -LiteralPath (Join-Path $installed $name)).Hash -ceq $stateHashes[$name]) ('preserved '+$name)}
    Check (Test-Path -LiteralPath $lease) 'inert sidecar preserved for race safety'
    Check (-not $unrelated.process.HasExited) 'portable process survived upgrade and uninstall'
    Release $unrelated
    # Reinstallation after uninstall is supported without removing retained state.
    $null=Run $setup $silent
    $null=Run $uninstaller $silent
    WaitMaintenance $lease
    Check $true 'reinstallation after uninstall passed'
    Copy-Item -LiteralPath (Join-Path $build 'DAC.exe') -Destination (Join-Path $portable 'DAC.exe')
    $actualPortable=ProductHost (Join-Path $portable 'DAC.exe') (Join-Path $FixtureRoot 'portable-profile') 8
    Start-Sleep -Milliseconds 200
    Check (-not $actualPortable.HasExited) 'shipping portable host is alive before maintenance'
    $null=Run $setup $silent
    $null=Run $uninstaller $silent
    Check (-not $actualPortable.HasExited) 'shipping portable host survived successful install and uninstall'
    Check ($actualPortable.WaitForExit(10000) -and $actualPortable.ExitCode -eq 0) 'shipping portable host completed its own bounded lifetime'
    WaitMaintenance $lease
} finally {
    # Only fixture-owned retained handles may be stopped on failure.
    foreach ($holder in @($installedHost,$helper,$unrelated)) {
        if (-not $holder.process.HasExited) {[IO.File]::WriteAllText($holder.release,'release');[void]$holder.process.WaitForExit(5000)}
    }
}
Check ((TreeHashes $livePreferences) -ceq $beforePreferences) 'live preferences unchanged'
$liveRun=[Microsoft.Win32.Registry]::CurrentUser.OpenSubKey('Software\Microsoft\Windows\CurrentVersion\Run')
$afterRun=if ($liveRun) {$liveRun.GetValue('DAC',$null,[Microsoft.Win32.RegistryValueOptions]::DoNotExpandEnvironmentNames)} else {$null}
if ($liveRun) {$liveRun.Dispose()}
Check ($afterRun -ceq $beforeRun) 'live startup registration unchanged'
[ordered]@{verification='passed';checks=$checks.Count;passed=@($checks);privateDesktop=$true;standardUser=$true;registryRedirected=$true;physicalDesktopQualified=$false;physicalHardwareTested=$false;actualOtherSessionExecuted=$false} | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $FixtureRoot 'results.json') -Encoding utf8NoBOM
Write-Output ('Installer isolated checks passed: '+$checks.Count+'.')
