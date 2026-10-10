; SPDX-License-Identifier: MIT
; Product bytes come only from a verified portable package and exact include.
#ifndef PayloadInclude
  #error Verified PayloadInclude required
#endif
#ifndef OutputPath
  #error OutputPath required
#endif
#ifdef FixtureRoot
  #define TargetDir FixtureRoot + "\installed"
  #define OutputName "DAC-Setup-0.1.0-dev-windows-x64-fixture"
#else
  #define TargetDir "{localappdata}\Programs\DAC"
  #define OutputName "DAC-Setup-0.1.0-dev-windows-x64"
#endif
#define UninstallKey "Software\Microsoft\Windows\CurrentVersion\Uninstall\{BB5697BB-B07F-4DD9-BD9B-8991BDF19A64}_is1"

[Setup]
AppId={{BB5697BB-B07F-4DD9-BD9B-8991BDF19A64}
AppName=DAC
AppVersion=0.1.0-dev
AppPublisher=StarlightDaemon
AppPublisherURL=https://github.com/StarlightDaemon/DAC
DefaultDirName={#TargetDir}
DefaultGroupName=DAC
PrivilegesRequired=lowest
SetupArchitecture=x64
ArchitecturesAllowed=x64os
ArchitecturesInstallIn64BitMode=x64os
MinVersion=10.0.22000
DisableDirPage=yes
DisableProgramGroupPage=yes
UsePreviousAppDir=no
UsePreviousTasks=no
CloseApplications=no
RestartApplications=no
AlwaysRestart=no
RestartIfNeededByRun=no
AllowCancelDuringInstall=no
UninstallDisplayIcon={app}\DAC.exe
OutputDir={#OutputPath}
OutputBaseFilename={#OutputName}
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
SignedUninstaller=no
VersionInfoVersion=0.1.0.0
VersionInfoProductVersion=0.1.0.0
VersionInfoProductTextVersion=0.1.0-dev
#ifdef FixtureRoot
UninstallDisplayName=DAC isolated installer fixture
#endif

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; Flags: unchecked

[Files]
#include PayloadInclude

[Icons]
#ifdef FixtureRoot
Name: "{#FixtureRoot}\shortcuts\DAC"; Filename: "{app}\DAC.exe"
Name: "{#FixtureRoot}\shortcuts\Desktop-DAC"; Filename: "{app}\DAC.exe"; Tasks: desktopicon
#else
Name: "{userprograms}\DAC\DAC"; Filename: "{app}\DAC.exe"
Name: "{userdesktop}\DAC"; Filename: "{app}\DAC.exe"; Tasks: desktopicon
#endif

[Code]
function DacCreateFile(Name: String; Access, Share: Cardinal; Security: NativeInt;
  Creation, Flags: Cardinal; Template: NativeInt): NativeInt;
  external 'CreateFileW@kernel32.dll stdcall';
function DacCloseHandle(Handle: NativeInt): Boolean;
  external 'CloseHandle@kernel32.dll stdcall';
function DacAttributes(Name: String): Cardinal;
  external 'GetFileAttributesW@kernel32.dll stdcall';
function DacDriveType(Name: String): Cardinal;
  external 'GetDriveTypeW@kernel32.dll stdcall';
#ifdef FixtureRoot
function DacRegCreateKey(Key: NativeInt; SubKey: String; Reserved: Cardinal;
  ClassName: NativeInt; Options, Access: Cardinal; Security: NativeInt;
  var ResultKey: NativeInt; Disposition: NativeInt): Integer;
  external 'RegCreateKeyExW@advapi32.dll stdcall';
function DacOverrideKey(Key, NewKey: NativeInt): Integer;
  external 'RegOverridePredefKey@advapi32.dll stdcall';
var FixtureKey: NativeInt;
function RedirectFixtureRegistry: Boolean;
begin
  Result := DacRegCreateKey(HKCU, 'Software\DAC-Installer-Fixtures\{#FixtureId}',
    0, 0, 0, $F003F, 0, FixtureKey, 0) = 0;
  if Result then Result := DacOverrideKey(HKCU, FixtureKey) = 0;
end;
#endif
var Maintenance: NativeInt;
function TargetDirectory: String;
begin
  Result := RemoveBackslashUnlessRoot(ExpandConstant('{#TargetDir}'));
end;
function SafeDirectory(Path: String): Boolean;
var Attributes: Cardinal; Current: String;
begin
  Result := False;
  if (Length(Path) < 4) or (Copy(Path, 2, 2) <> ':\') or
    (Pos(':', Copy(Path, 3, Length(Path))) <> 0) or
    (DacDriveType(Copy(Path, 1, 3)) <> 3) then Exit;
  Current := Path;
  while Length(Current) > 3 do begin
    Attributes := DacAttributes(Current);
    if Attributes <> $FFFFFFFF then begin
      if (Attributes and $400 <> 0) or (Attributes and $10 = 0) then Exit;
    end;
    Current := ExtractFileDir(Current);
  end;
  Result := True;
end;
function AcquireMaintenance(Installing: Boolean): Boolean;
var Directory, Sidecar, Previous: String; Probe: NativeInt; Attributes: Cardinal;
begin
  Result := False;
  Directory := TargetDirectory;
  if not SafeDirectory(Directory) then Exit;
  Sidecar := Directory + '\.dac-lifecycle.lock';
  if FileExists(Directory + '\DAC.exe') then begin
    { Never adopt or overwrite an existing portable/unmanaged executable. }
    if not FileExists(Sidecar) or
      not RegQueryStringValue(HKCU, '{#UninstallKey}', 'InstallLocation', Previous) or
      (CompareText(RemoveBackslashUnlessRoot(Previous), Directory) <> 0) then Exit;
  end;
  if not Installing and not FileExists(Sidecar) then Exit;
  Attributes := DacAttributes(Sidecar);
  if (Attributes <> $FFFFFFFF) and (Attributes and $410 <> 0) then Exit;
  if not ForceDirectories(Directory) then Exit;
  { Every installed host/helper/CLI process in every session holds a shared read
    lease. Exclusive sharing also serializes Setup and uninstall. }
  Maintenance := DacCreateFile(Sidecar, $80000000, 0, 0, 4, $00200000, 0);
  if Maintenance = -1 then Exit;
  { Image sections provide an additional check, not singleton evidence. }
  if FileExists(Directory + '\DAC.exe') then begin
    Probe := DacCreateFile(Directory + '\DAC.exe', $C0000000, 0, 0, 3, $00200000, 0);
    if Probe = -1 then begin DacCloseHandle(Maintenance); Maintenance := -1; Exit; end;
    DacCloseHandle(Probe);
  end;
  Result := True;
end;
function ValidPlatform: Boolean;
var Version: TWindowsVersion; I: Integer; Argument: String;
begin
  GetWindowsVersionEx(Version);
  Result := (Version.Major = 10) and (Version.Minor = 0) and
    (Version.Build >= 22000) and (Version.ProductType = VER_NT_WORKSTATION);
  for I := 1 to ParamCount do begin
    Argument := Uppercase(ParamStr(I));
    if (Argument = '/CLOSEAPPLICATIONS') or (Argument = '/FORCECLOSEAPPLICATIONS') or
      (Argument = '/RESTARTAPPLICATIONS') or (Argument = '/ALLUSERS') then Result := False;
  end;
end;
function InitializeSetup: Boolean;
begin
  Maintenance := -1;
#ifdef FixtureRoot
  if not RedirectFixtureRegistry then begin Result := False; Exit; end;
#endif
  Result := ValidPlatform and AcquireMaintenance(True);
  if not Result then SuppressibleMsgBox(
    'Setup requires Windows 11 x64 workstation and an idle managed installation. ' +
    'Exit installed DAC and its helpers yourself, then retry. Existing portable files are never replaced.',
    mbError, MB_OK, IDOK);
end;
function PrepareToInstall(var NeedsRestart: Boolean): String;
begin
  Result := '';
  NeedsRestart := False;
  if CompareText(RemoveBackslashUnlessRoot(WizardDirValue), TargetDirectory) <> 0 then
    Result := 'DAC requires its fixed current-user installation directory.';
end;
function InstalledStartupExists: Boolean;
var Command, Prefix: String;
begin
  Result := False;
  if not RegValueExists(HKCU, 'Software\Microsoft\Windows\CurrentVersion\Run', 'DAC') then Exit;
  if not RegQueryStringValue(HKCU, 'Software\Microsoft\Windows\CurrentVersion\Run', 'DAC', Command) then begin Result := True; Exit; end;
  Prefix := '"' + TargetDirectory + '\DAC.exe"';
  Result := (CompareText(Copy(Command, 1, Length(Prefix)), Prefix) = 0) and
    ((Length(Command) = Length(Prefix)) or (Copy(Command, Length(Prefix)+1, 1) = ' '));
end;
function InitializeUninstall: Boolean;
begin
  Maintenance := -1;
#ifdef FixtureRoot
  if not RedirectFixtureRegistry then begin Result := False; Exit; end;
#endif
  { Acquire exclusion before reading app-owned startup state: a running DAC
    cannot change that value between the ownership check and removal. }
  Result := ValidPlatform and AcquireMaintenance(False) and not InstalledStartupExists;
  if not Result then SuppressibleMsgBox(
    'Uninstall requires idle installed DAC processes. Disable login startup in DAC first if it belongs to this installation, exit DAC and its helpers, then retry.',
    mbError, MB_OK, IDOK);
end;
procedure ReleaseMaintenance;
begin
  if Maintenance <> -1 then begin DacCloseHandle(Maintenance); Maintenance := -1; end;
end;
procedure DeinitializeSetup;
begin
  ReleaseMaintenance;
end;
procedure DeinitializeUninstall;
begin
  ReleaseMaintenance;
end;
{ No Run, UninstallRun, Registry, InstallDelete or UninstallDelete entries.
  Preferences and the inert lease sidecar survive uninstall. }
