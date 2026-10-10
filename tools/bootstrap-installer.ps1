#Requires -Version 7.0
[CmdletBinding()]
param([switch]$Offline, [switch]$VerifyOnly)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repository = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
. (Join-Path $PSScriptRoot 'Package.Common.ps1')
$lock = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'installer-toolchain-lock.json') -Raw | ConvertFrom-Json
if ($lock.formatVersion -ne 1 -or $lock.version -cne '7.1.0' -or $lock.url -cne 'https://github.com/jrsoftware/issrc/releases/download/is-7_1_0/innosetup-7.1.0-x64.exe') { throw 'Unsupported installer compiler lock.' }
$download = Resolve-PackageLocalPath $repository ('.tools/downloads/' + $lock.fileName)
$destination = Resolve-PackageLocalPath $repository '.tools/inno-7.1.0'
$inventory = Resolve-PackageLocalPath $repository '.tools/inno-7.1.0.inventory.json'
if (-not (Test-Path -LiteralPath $download)) {
    if ($Offline -or $VerifyOnly) { throw 'Pinned Inno Setup cache is missing.' }
    [void][IO.Directory]::CreateDirectory((Split-Path $download -Parent))
    Invoke-WebRequest -Uri $lock.url -OutFile $download
}
if ((Get-Item -LiteralPath $download).Length -ne $lock.bytes -or (Get-FileHash -LiteralPath $download -Algorithm SHA256).Hash.ToLowerInvariant() -cne $lock.sha256) { throw 'Inno Setup download hash/size mismatch.' }
$signature = Get-AuthenticodeSignature -LiteralPath $download
if ($signature.Status -ne 'Valid' -or $signature.SignerCertificate.Subject -cne $lock.signerSubject -or $signature.SignerCertificate.Thumbprint -cne $lock.signerThumbprint) { throw 'Inno Setup Authenticode identity is not valid.' }
if (-not (Test-Path -LiteralPath $destination)) {
    if ($VerifyOnly) { throw 'Pinned compiler has not been provisioned.' }
    # Upstream portable mode suppresses uninstall registration, associations and icons.
    # Its optional compiler launch also requires not WizardSilent.
    $arguments = @('/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART','/CURRENTUSER','/PORTABLE=1','/NOICONS','/TASKS=',('/DIR="' + $destination + '"'))
    $process = Start-Process -FilePath $download -ArgumentList $arguments -WindowStyle Hidden -PassThru
    if (-not $process.WaitForExit(120000)) { $process.Kill($true); throw 'Owned compiler provisioning timed out.' }
    if ($process.ExitCode -ne 0) { throw 'Compiler portable provisioning failed.' }
    $records = @(Get-ChildItem -LiteralPath $destination -File -Recurse | ForEach-Object {
        [ordered]@{ path=[IO.Path]::GetRelativePath($destination,$_.FullName).Replace('\','/'); sha256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant() }
    } | Sort-Object path)
    [IO.File]::WriteAllText($inventory,($records | ConvertTo-Json -Depth 3),[Text.UTF8Encoding]::new($false))
}
if (-not (Test-Path -LiteralPath $inventory)) { throw 'Compiler inventory is absent; refuse unverified provisioning.' }
if ((Get-FileHash -LiteralPath $inventory).Hash.ToLowerInvariant() -cne $lock.inventorySha256) { throw 'Compiler inventory identity mismatch.' }
$records = @(Get-Content -LiteralPath $inventory -Raw | ConvertFrom-Json)
$actual = @(Get-ChildItem -LiteralPath $destination -File -Recurse)
if ($records.Count -ne $actual.Count) { throw 'Compiler file set changed.' }
foreach ($record in $records) {
    Assert-PackageName $record.path
    $file = Resolve-PackageLocalPath $repository (Join-Path $destination $record.path)
    if (-not (Test-Path -LiteralPath $file) -or (Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash.ToLowerInvariant() -cne $record.sha256) { throw 'Compiler inventory hash mismatch.' }
}
$compiler = Join-Path $destination 'ISCC.exe'
if ((Get-FileHash -LiteralPath $compiler).Hash.ToLowerInvariant() -cne $lock.compilerSha256) { throw 'Compiler executable hash mismatch.' }
if ((& $compiler --version) -cne '7.1.0' -or $LASTEXITCODE -ne 0) { throw 'Compiler engine version mismatch.' }
[ordered]@{ version=$lock.version; downloadSha256=$lock.sha256; downloadBytes=$lock.bytes; authenticode='Valid'; publisher=$lock.signerSubject; portable=$true; inventoryFiles=$records.Count; compilerSha256=(Get-FileHash -LiteralPath $compiler).Hash.ToLowerInvariant(); verification='passed' } | ConvertTo-Json