#Requires -Version 7.0
# Read-only primitives. No product launch, stop, process termination or DDC API.
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$script:SafetyIdentity = [ordered]@{
    source = 'afcabcfc34a851dbbf400a98cc390e1fb3845f22'
    setup = '731ee1dd94cab5fce292e903d88cfcc0cba97812f69bded0cb218245ab402fe1'
    bytes = 3224312
    receipt = 'd9902746e4ba1f125ef31858d061a36a081ee93b7240cdee0a36eb797c182c6c'
    archive = '12570df1b5776301a940a938e102d7ee7604ce14a176b597a7939a82ed09bc89'
    executable = '54426f8c9e29e8670bcae02760df48a38d86fae43e5ab2b5dd8dfc222ade3f82'
    payloadFiles = 28
}

function Resolve-SafetyPath([string]$Path) {
    # Missing leaves are permitted, inaccessible components are not. Reject ADS,
    # UNC, device paths, mapped/network drives and every existing reparse ancestor.
    if ($Path -notmatch '^[A-Za-z]:[\\/]' -or $Path.Substring(2).Contains(':')) { throw 'PATH_FORMAT' }
    $full = [IO.Path]::GetFullPath($Path)
    if ([IO.DriveInfo]::new([IO.Path]::GetPathRoot($full)).DriveType -ne 'Fixed') { throw 'PATH_DRIVE' }
    $cursor = $full
    while ($cursor) {
        try {
            $item = Get-Item -LiteralPath $cursor -Force -ErrorAction Stop
            if ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'PATH_REPARSE' }
        } catch [System.Management.Automation.ItemNotFoundException] { }
        $parent = [IO.Path]::GetDirectoryName($cursor)
        if ($parent -eq $cursor) { break }; $cursor = $parent
    }
    $full
}

function Get-SafetyDigest([string]$Path) {
    (Get-FileHash -LiteralPath (Resolve-SafetyPath $Path) -Algorithm SHA256 -ErrorAction Stop).Hash.ToLowerInvariant()
}

function Assert-SafetyArtifact([string]$Installer, [string]$Archive) {
    $path = Resolve-SafetyPath $Installer
    if ([IO.Path]::GetFileName($path) -cne 'DAC-Setup-0.1.0-dev-windows-x64.exe' -or
        (Get-Item -LiteralPath $path).Length -ne $script:SafetyIdentity.bytes -or
        (Get-SafetyDigest $path) -cne $script:SafetyIdentity.setup) { throw 'ARTIFACT_IDENTITY' }
    if ((Get-SafetyDigest ($path+'.verification.json')) -cne $script:SafetyIdentity.receipt) { throw 'RECEIPT_IDENTITY' }
    $receipt = Get-Content -LiteralPath ($path+'.verification.json') -Raw | ConvertFrom-Json
    if ($receipt.sourceCommit -cne $script:SafetyIdentity.source -or $receipt.fixture -ne $false -or
        $receipt.workingTreeModified -ne $false -or $receipt.files.Count -ne $script:SafetyIdentity.payloadFiles) { throw 'RECEIPT_SOURCE' }
    $checksum = Resolve-SafetyPath ($path+'.sha256')
    if ([IO.File]::ReadAllText($checksum) -cne ($script:SafetyIdentity.setup+'  '+[IO.Path]::GetFileName($path)+"`n")) { throw 'CHECKSUM_RECEIPT' }
    $bytes = [IO.File]::ReadAllBytes($path)
    $offset = [BitConverter]::ToInt32($bytes,0x3c)
    if ([BitConverter]::ToUInt16($bytes,0) -ne 0x5a4d -or $offset -lt 64 -or $offset+96 -gt $bytes.Length -or
        [BitConverter]::ToUInt32($bytes,$offset) -ne 0x4550 -or [BitConverter]::ToUInt16($bytes,$offset+4) -ne 0x8664 -or
        [BitConverter]::ToUInt16($bytes,$offset+24) -ne 0x20b -or [BitConverter]::ToUInt16($bytes,$offset+92) -ne 2) { throw 'ARTIFACT_PE' }
    if ((Get-AuthenticodeSignature -LiteralPath $path).Status -ne 'NotSigned') { throw 'ARTIFACT_SIGNING' }
    if ($Archive) {
        if ((Get-SafetyDigest $Archive) -cne $script:SafetyIdentity.archive) { throw 'ARCHIVE_IDENTITY' }
        $zip = [IO.Compression.ZipFile]::OpenRead((Resolve-SafetyPath $Archive))
        try {
            if ($zip.Entries.Count -ne $script:SafetyIdentity.payloadFiles) { throw 'ARCHIVE_INVENTORY' }
            foreach ($file in $receipt.files) {
                $entry = $zip.GetEntry('DAC-0.1.0-dev-windows-x64/'+$file.path)
                if (!$entry -or $entry.Length -ne $file.bytes) { throw 'ARCHIVE_ENTRY' }
                $stream = $entry.Open()
                try { $digest = [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($stream)).ToLowerInvariant() }
                finally { $stream.Dispose() }
                if ($digest -cne $file.sha256) { throw 'ARCHIVE_PAYLOAD' }
            }
        } finally { $zip.Dispose() }
    }
    $receipt
}

function Get-SafetyFileInventory([string]$Root) {
    $rootPath = Resolve-SafetyPath $Root
    if (!(Test-Path -LiteralPath $rootPath)) { return }
    $pending = [Collections.Generic.Queue[string]]::new(); $pending.Enqueue($rootPath)
    $count = 0
    while ($pending.Count) {
        foreach ($item in Get-ChildItem -LiteralPath $pending.Dequeue() -Force -ErrorAction Stop) {
            if (++$count -gt 2048) { throw 'INVENTORY_LIMIT' }
            if ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'INVENTORY_REPARSE' }
            if ($item.PSIsContainer) { $pending.Enqueue($item.FullName); continue }
            if ($item.Length -gt 32MB) { throw 'INVENTORY_FILE_LIMIT' }
            [pscustomobject]@{ path=[IO.Path]::GetRelativePath($rootPath,$item.FullName).Replace('\','/'); bytes=$item.Length; sha256=Get-SafetyDigest $item.FullName }
        }
    }
}

function Get-SafetyRegistry([string]$Path) {
    # Open read-only; a missing key is a normal empty baseline.
    $key = [Microsoft.Win32.Registry]::CurrentUser.OpenSubKey($Path,$false)
    $values = [ordered]@{}
    if ($key) {
        try { foreach ($name in ($key.GetValueNames() | Sort-Object)) { $values[$name]=$key.GetValue($name,$null,[Microsoft.Win32.RegistryValueOptions]::DoNotExpandEnvironmentNames) } }
        finally { $key.Dispose() }
    }
    $values
}

function Get-SafetyState {
    $settings = Join-Path $env:LOCALAPPDATA 'DAC'
    $target = Join-Path $env:LOCALAPPDATA 'Programs/DAC'
    $run = Get-SafetyRegistry 'Software\Microsoft\Windows\CurrentVersion\Run'
    $startup = [ordered]@{}
    foreach ($name in $run.Keys) { if ($name -match 'DAC|Windhawk' -or [string]$run[$name] -match '(?i)DAC\.exe|Windhawk') { $startup[$name]=$run[$name] } }
    [ordered]@{
        settingsPath=Resolve-SafetyPath $settings
        target=Resolve-SafetyPath $target
        settings=@(Get-SafetyFileInventory $settings)
        product=@(Get-SafetyFileInventory $target)
        startup=$startup
        uninstall=Get-SafetyRegistry 'Software\Microsoft\Windows\CurrentVersion\Uninstall\{BB5697BB-B07F-4DD9-BD9B-8991BDF19A64}_is1'
    }
}

function Assert-SafetyInstalledPayload($Receipt,[string]$Root) {
    $inventory = @(Get-SafetyFileInventory $Root)
    $expected = @($Receipt.files.path)+@('.dac-lifecycle.lock','unins000.exe','unins000.dat')
    if ((($expected | Sort-Object) -join '|') -cne (($inventory.path | Sort-Object) -join '|')) { throw 'INSTALLED_INVENTORY' }
    foreach ($file in $Receipt.files) {
        $actual = @($inventory | Where-Object path -CEQ $file.path)
        if ($actual.Count -ne 1 -or $actual[0].bytes -ne $file.bytes -or $actual[0].sha256 -cne $file.sha256) { throw 'INSTALLED_PAYLOAD' }
    }
    $maintenance = @($inventory | Where-Object path -in @('.dac-lifecycle.lock','unins000.exe','unins000.dat'))
    if (($maintenance | Where-Object path -eq '.dac-lifecycle.lock').bytes -ne 0 -or
        ($maintenance | Where-Object path -eq 'unins000.exe').bytes -lt 128 -or
        ($maintenance | Where-Object path -eq 'unins000.dat').bytes -eq 0) { throw 'MAINTENANCE_FILES' }
    $maintenance
}

function New-SafetyFinding([string]$Id,[ValidateSet('PASS','WARNING','BLOCKED','FAIL')][string]$Status,[string]$Explanation) {
    [pscustomobject]@{id=$Id;status=$Status;explanation=$Explanation}
}

function Get-SafetyFailure($Failure) {
    $messages = @{
        PATH_FORMAT='Path must be an absolute drive path without UNC, device syntax or alternate streams.'
        PATH_DRIVE='Path is not on a fixed local drive.'
        PATH_REPARSE='An existing path component redirects through a reparse point.'
        ARTIFACT_IDENTITY='Setup filename, byte length or pinned SHA-256 differs.'
        RECEIPT_IDENTITY='Adjacent final-commit verification receipt differs from its pinned bytes.'
        RECEIPT_SOURCE='Receipt source, fixture status or payload count differs.'
        CHECKSUM_RECEIPT='Adjacent SHA-256 text receipt differs.'
        ARTIFACT_PE='Setup is not the expected AMD64 PE32+ GUI image.'
        ARTIFACT_SIGNING='Setup signing state differs from the expected unsigned candidate.'
        ARCHIVE_IDENTITY='Package SHA-256 differs from the verified canonical package.'
        ARCHIVE_INVENTORY='Package does not contain exactly the pinned number of entries.'
        ARCHIVE_ENTRY='Package entry identity or size differs.'
        ARCHIVE_PAYLOAD='Package payload bytes differ from the pinned receipt.'
        INVENTORY_LIMIT='Inventory exceeds the 2048-entry limit; completeness is unknown.'
        INVENTORY_FILE_LIMIT='Inventory contains a file larger than the 32 MiB read limit.'
        INVENTORY_REPARSE='Inventory contains a redirected child; it was not traversed.'
        INSTALLED_INVENTORY='Installed file inventory has missing or extra files.'
        INSTALLED_PAYLOAD='An installed product file differs from its verified package bytes.'
        MAINTENANCE_FILES='Maintenance sidecar or generated uninstaller file shape differs.'
        BASELINE_LIMIT='Baseline exceeds the 4 MiB limit.'
        BASELINE_IDENTITY='Baseline user, machine, session or elevation does not match.'
        STATE_CHANGED='Settings, startup ownership or target differs from the baseline.'
        UNINSTALL_REGISTRATION='Current-user uninstall name or install location differs.'
        UNINSTALL_TARGET='Uninstall command does not point exactly to the expected generated uninstaller.'
        SHORTCUT_MISSING='Expected current-user Start Menu shortcut is missing.'
        SHORTCUT_TARGET='Start Menu target or arguments differ.'
        CONTROLLER_PRESENT='A DAC process is present; installation-only acceptance is blocked.'
    }
    $message=$Failure.Exception.Message
    if ($messages.ContainsKey($message)) { return $messages[$message] }
    if ($Failure.Exception -is [UnauthorizedAccessException] -or $Failure.CategoryInfo.Category -eq 'PermissionDenied') { return 'Permission denied; this observation is incomplete.' }
    if ($Failure.CategoryInfo.Category -eq 'ObjectNotFound') { return 'Required file, object or observation provider is unavailable.' }
    'Observation failed or its schema was incomplete; no absence or success inferred. Resolve locally and rerun.'
}

function Save-SafetyEvidence($Summary,$Private) {
    $root = Resolve-SafetyPath ([IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../out/private/safety')))
    $directory = Join-Path $root ([Guid]::NewGuid().ToString('N'))
    [void][IO.Directory]::CreateDirectory($directory)
    # Nothing private goes to stdout. These files inherit local workspace ACLs;
    # ignored is not encrypted or access-controlled against the same user.
    [IO.File]::WriteAllText((Join-Path $directory 'private.json'),($Private | ConvertTo-Json -Depth 16))
    [IO.File]::WriteAllText((Join-Path $directory 'summary.json'),($Summary | ConvertTo-Json -Depth 8))
    [IO.Path]::GetFileName($directory)
}

function Get-SafetySummary($Findings) {
    $status = if (@($Findings | Where-Object status -eq FAIL).Count) {'FAIL'} elseif (@($Findings | Where-Object status -eq BLOCKED).Count) {'BLOCKED'} elseif (@($Findings | Where-Object status -eq WARNING).Count) {'WARNING'} else {'PASS'}
    [ordered]@{schema=1;utc=[DateTime]::UtcNow.ToString('o');status=$status;source=$script:SafetyIdentity.source;installationAuthorized=$false;findings=@($Findings)}
}
