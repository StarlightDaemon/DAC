#Requires -Version 7.0
[CmdletBinding()]
param([Parameter(Mandatory)][string]$Installer,[Parameter(Mandatory)][string]$Archive)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$repository=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
. (Join-Path $PSScriptRoot 'Package.Common.ps1')
. (Join-Path $PSScriptRoot 'Installer.Common.ps1')
$exe=Resolve-PackageLocalPath $repository $Installer
$archivePath=Resolve-PackageLocalPath $repository $Archive
$private=Join-Path $repository ('out/private/installer-integrity/'+[Guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($private)
$pwsh=(Get-Process -Id $PID).Path
$checks=0
function Check-Rejection([scriptblock]$Action) {
    $rejected=$false
    try {& $Action | Out-Null} catch {$rejected=$true}
    if (-not $rejected) {throw 'Installer negative fixture unexpectedly passed.'}
    $script:checks++
}
$receipt=Get-Content -LiteralPath ($exe+'.verification.json') -Raw | ConvertFrom-Json
# One copied fixture, never edited source/package/compiler or installation state.
$output=Split-Path $exe -Parent
$fixture=Join-Path $private 'artifact';[void][IO.Directory]::CreateDirectory($fixture)
foreach ($file in @($exe,($exe+'.sha256'),($exe+'.verification.json'),(Join-Path $output 'payload.iss'))) {
    Copy-Item -LiteralPath $file -Destination $fixture
}
$spec=Get-PackageSpecification
foreach ($file in $spec.files) {
    $relative='input/'+$spec.archiveRoot+'/'+$file.path
    $target=Join-Path $fixture $relative
    [void][IO.Directory]::CreateDirectory((Split-Path $target -Parent))
    Copy-Item -LiteralPath (Join-Path $output $relative) -Destination $target
}
$fixtureExe=Join-Path $fixture 'DAC-Setup-0.1.0-dev-windows-x64.exe'
$fixtureInclude=Join-Path $fixture 'payload.iss'
$include=[IO.File]::ReadAllText($fixtureInclude).Replace($output,$fixture)
[IO.File]::WriteAllText($fixtureInclude,$include,[Text.UTF8Encoding]::new($false))
$receipt.includeSha256=(Get-FileHash -LiteralPath $fixtureInclude).Hash.ToLowerInvariant()
$baseline=$receipt | ConvertTo-Json -Depth 8
function Write-Receipt($Value) {[IO.File]::WriteAllText($fixtureExe+'.verification.json',($Value|ConvertTo-Json -Depth 8)+"`n",[Text.UTF8Encoding]::new($false))}
Write-Receipt $receipt
Invoke-InstallerTool $pwsh @('-NoProfile','-File',(Join-Path $PSScriptRoot 'verify-installer.ps1'),'-Installer',$fixtureExe,'-Archive',$archivePath,'-RequireShipping') (Join-Path $private 'positive') 120
$evidence=Get-Content -LiteralPath (Join-Path $private 'positive.stdout.log') -Raw | ConvertFrom-Json
if ($evidence.verification -cne 'passed' -or $evidence.embeddedPayloadChecked -ne $true -or $evidence.embeddedPayloadFiles -ne 19) {throw 'Installer positive evidence incomplete.'}
$checks++
foreach ($field in @('formatVersion','sourceCommit','sourceTreeSha256','sourceFiles','workingTreeModified','archiveSha256','executableSha256','compilerSha256','compilerVersion','scriptSha256','includeSha256','installerSha256','installerBytes','signed','productExecuted','fixture','fixtureId')) {
    $modified=$baseline | ConvertFrom-Json
    switch ($field) {
        'sourceFiles' {$modified.$field++}
        'installerBytes' {$modified.$field++}
        'workingTreeModified' {$modified.$field=-not $modified.$field}
        {$_ -in @('signed','productExecuted','fixture')} {$modified.$field=$true}
        default {$modified.$field='INVALID-FIXTURE'}
    }
    Write-Receipt $modified
    Check-Rejection {Invoke-InstallerTool $pwsh @('-NoProfile','-File',(Join-Path $PSScriptRoot 'verify-installer.ps1'),'-Installer',$fixtureExe,'-Archive',$archivePath,'-RequireShipping') (Join-Path $private ('receipt-'+$field)) 120}
}
foreach ($kind in @('extra-field','missing-field','string-boolean')) {
    $modified=$baseline | ConvertFrom-Json
    switch ($kind) {
        'extra-field' {$modified | Add-Member -NotePropertyName unexpected -NotePropertyValue 'fixture'}
        'missing-field' {$modified.PSObject.Properties.Remove('sourceCommit')}
        'string-boolean' {$modified.signed='false'}
    }
    Write-Receipt $modified
    Check-Rejection {Invoke-InstallerTool $pwsh @('-NoProfile','-File',(Join-Path $PSScriptRoot 'verify-installer.ps1'),'-Installer',$fixtureExe,'-Archive',$archivePath,'-RequireShipping') (Join-Path $private $kind) 120}
}
Write-Receipt ($baseline | ConvertFrom-Json)
# Canonical include is checked independently of its self-reported hash.
[IO.File]::AppendAllText($fixtureInclude,'Source: "*"; DestDir: "{app}"'+"`n")
$modified=$baseline | ConvertFrom-Json;$modified.includeSha256=(Get-FileHash $fixtureInclude).Hash.ToLowerInvariant();Write-Receipt $modified
Check-Rejection {Invoke-InstallerTool $pwsh @('-NoProfile','-File',(Join-Path $PSScriptRoot 'verify-installer.ps1'),'-Installer',$fixtureExe,'-Archive',$archivePath,'-RequireShipping') (Join-Path $private 'include-wildcard') 120}
[IO.File]::WriteAllText($fixtureInclude,$include,[Text.UTF8Encoding]::new($false));Write-Receipt ($baseline | ConvertFrom-Json)
$modified=$baseline | ConvertFrom-Json;$modified.files=$modified.files[0..17];Write-Receipt $modified
Check-Rejection {Invoke-InstallerTool $pwsh @('-NoProfile','-File',(Join-Path $PSScriptRoot 'verify-installer.ps1'),'-Installer',$fixtureExe,'-Archive',$archivePath,'-RequireShipping') (Join-Path $private 'missing-payload') 120}
Write-Receipt ($baseline | ConvertFrom-Json)
Initialize-InstallerReader
$original=[IO.File]::ReadAllBytes($exe);$table=[DacInstallerReader]::Table($original)
$metadataAt=[BitConverter]::ToInt64($original,$table+40);$payloadAt=[BitConverter]::ToInt64($original,$table+48)
foreach ($kind in @('table-crc','metadata-crc','payload-stream','trailing-data','truncation')) {
    $bytes=[byte[]]$original.Clone()
    switch ($kind) {
        'table-crc' {$bytes[$table+60]=$bytes[$table+60] -bxor 1}
        'metadata-crc' {$bytes[$metadataAt+117]=$bytes[$metadataAt+117] -bxor 1}
        'payload-stream' {$bytes[$payloadAt+5]=$bytes[$payloadAt+5] -bxor 255}
        'trailing-data' {$bytes=[byte[]]($bytes+[byte]0)}
        'truncation' {$bytes=[byte[]]$bytes[0..($bytes.Length-2)]}
    }
    $candidate=Join-Path $private ($kind+'.exe');[IO.File]::WriteAllBytes($candidate,$bytes)
    Check-Rejection {Read-InstallerEmbeddedContent $candidate}
}
# Resource binding remains required even if a forged table has a valid CRC.
$forged=[byte[]]$original.Clone(); $forged[$table+12]=3
$forgedCrc=[BitConverter]::GetBytes([DacInstallerReader]::Crc($forged,$table,60));[Array]::Copy($forgedCrc,0,$forged,$table+60,4)
Check-Rejection {[DacInstallerReader]::Table($forged)}
# Dictionary and compressed-input size caps fail before decoder allocation.
$readerRoot=Get-ChildItem -LiteralPath (Join-Path $repository 'out/private/installer-read') -Directory | Sort-Object LastWriteTime -Descending | Select-Object -First 1
$decoder=Join-Path $readerRoot.FullName 'decoder.exe'
$dll=Join-Path $repository '.tools/inno-7.1.0/is7zxr-x64.dll'
foreach ($method in 1,2) {
    $input=Join-Path $private ('dictionary-'+$method+'.compressed'); $target=Join-Path $private ('dictionary-'+$method+'.decoded')
    $bytes=if ($method -eq 1) {[byte[]]@(93,0,0,0,128,0)} else {[byte[]]@(27,0,0,0,0,0)}
    [IO.File]::WriteAllBytes($input,$bytes)
    Check-Rejection {Invoke-InstallerTool $decoder @($dll,$input,$target,([string]$method)) (Join-Path $private ('dictionary-'+$method)) 30}
}
$input=Join-Path $private 'oversized.compressed';$stream=[IO.File]::Create($input)
try {$stream.SetLength(32MB+1)} finally {$stream.Dispose()}
Check-Rejection {Invoke-InstallerTool $decoder @($dll,$input,(Join-Path $private 'oversized.decoded'),'1') (Join-Path $private 'oversized') 30}
# Full stream consumption: valid LZMA1 metadata/LZMA2 solid payload pass, but
# appending bytes after the compressed end marker fails despite read-ahead.
[long]$metadataCursor=$metadataAt+117
$streams=@{
    1=[DacInstallerReader]::Block($original,[ref]$metadataCursor)
    2=[DacInstallerReader]::Part($original,$payloadAt+4,$metadataAt-$payloadAt-4)
}
foreach ($method in 1,2) {
    $input=Join-Path $private ('stream-'+$method+'.compressed')
    $target=Join-Path $private ('stream-'+$method+'.decoded')
    [IO.File]::WriteAllBytes($input,$streams[$method])
    Invoke-InstallerTool $decoder @($dll,$input,$target,([string]$method)) (Join-Path $private ('stream-'+$method)) 30
    $checks++
    [IO.File]::WriteAllBytes($input,[byte[]]($streams[$method]+[byte]0))
    Check-Rejection {Invoke-InstallerTool $decoder @($dll,$input,($target+'.trailing'),([string]$method)) (Join-Path $private ('stream-trailing-'+$method)) 30}
}
# Byte-range and CRC guards reject malformed data.
Check-Rejection {[DacInstallerReader]::Part([byte[]]@(1,2),-1,1)}
Check-Rejection {[DacInstallerReader]::Part([byte[]]@(1,2),1,2)}
Check-Rejection {[DacInstallerReader]::Block([byte[]]::new(13),[ref]([long]0))}
# Verify source-change rejection with objects; never mutate real source or Git.
$identity=Get-InstallerSourceIdentity
Assert-InstallerSourceUnchanged $identity $identity; $checks++
foreach ($field in @('commit','modified','files','sha256')) {
    $changed=$identity | ConvertTo-Json | ConvertFrom-Json
    switch ($field) {
        'modified' {$changed.modified=-not $changed.modified}
        'files' {$changed.files++}
        default {$changed.$field='INVALID-FIXTURE'}
    }
    Check-Rejection {Assert-InstallerSourceUnchanged $identity $changed}
}
# Exercise the actual byte scanner: vendor exceptions never cover compiled
# metadata/product bytes, and the metadata flag cannot exempt a product PE check.
$tokens=$null;$errors=$null
$ast=[Management.Automation.Language.Parser]::ParseFile((Join-Path $PSScriptRoot 'audit-publication.ps1'),[ref]$tokens,[ref]$errors)
if ($errors.Count) {throw 'Privacy auditor syntax failed.'}
foreach ($definition in $ast.FindAll({param($node) $node -is [Management.Automation.Language.FunctionDefinitionAst]},$false)) {
    . ([scriptblock]::Create($definition.Extent.Text))
}
$utf8=[Text.UTF8Encoding]::new($false,$true)
$compiledRules=@([pscustomobject]@{name='private-runtime-identity';regex=[regex]::new('PRIVATEIDENTITYFIXTURE')})
$reviewedVendorToken=$true
foreach ($scope in @('installer-metadata','installer-payload')) {
    foreach ($encoding in @([Text.Encoding]::ASCII,[Text.Encoding]::Unicode)) {
        $findings=[Collections.Generic.List[object]]::new()
        $inventory=[Collections.Generic.List[object]]::new()
        $statistics=[ordered]@{genericWindowsOsPathReferences=0;syntheticRedactionFixtures=0;reviewedPublicVendorTokens=0;executableAsciiStrings=0;executableUtf16Strings=0}
        $data=$encoding.GetBytes('PRIVATEIDENTITYFIXTURE')
        Scan-Bytes $data $scope 'fixture' @('fixture') $true $true
        if (@($findings | Where-Object rule -CEQ 'private-runtime-identity').Count -eq 0 -or
            ($scope -ceq 'installer-payload' -and @($findings | Where-Object rule -CEQ 'declared-executable-not-pe').Count -ne 1)) {throw 'Compiled byte privacy boundary regression.'}
        $checks++
    }
}
[ordered]@{verification='passed';checks=$checks;compiledPayloadFiles=19;installationExecuted=$false;sourceAndToolchainMutated=$false} | ConvertTo-Json
