#Requires -Version 7.0
[CmdletBinding()]
param(
    [string]$Archive='build/ci-package/DAC-0.1.0-dev-windows-x64.zip',
    [string]$OutputDirectory='build/installer',
    [string]$FixtureRoot='',
    [string]$FixtureId=''
)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$repository=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
. (Join-Path $PSScriptRoot 'Package.Common.ps1')
$compilerReceipt = & (Join-Path $PSScriptRoot 'bootstrap-installer.ps1') -Offline -VerifyOnly | ConvertFrom-Json
$archivePath=Resolve-PackageLocalPath $repository $Archive
$inputReceipt=& (Join-Path $PSScriptRoot 'verify-package.ps1') -Archive $archivePath | ConvertFrom-Json
$receiptPath=$archivePath+'.sha256'
$expected=$inputReceipt.archiveSha256+'  '+[IO.Path]::GetFileName($archivePath)+"`n"
if (-not (Test-Path -LiteralPath $receiptPath) -or [IO.File]::ReadAllText($receiptPath) -cne $expected) { throw 'Verified package archive receipt is required.' }
$output=Resolve-PackageLocalPath $repository $OutputDirectory
if (Test-Path -LiteralPath $output) { throw 'Refusing to overwrite an installer output directory.' }
if ($FixtureRoot) {
    $FixtureRoot=Resolve-PackageLocalPath $repository $FixtureRoot
    if (-not $FixtureRoot.StartsWith((Join-Path $repository 'out/private/installer')+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase) -or $FixtureId -cnotmatch '^[0-9a-f]{32}$') { throw 'Fixture must use a unique private root and registry identity.' }
}
[void][IO.Directory]::CreateDirectory($output)
$stage=Join-Path $output 'input'
[IO.Compression.ZipFile]::ExtractToDirectory($archivePath,$stage)
$payload=Join-Path $stage (Get-PackageSpecification).archiveRoot
$include=Join-Path $output 'payload.iss'
$lines=@()
$files=@()
foreach ($file in (Get-PackageSpecification).files) {
    $source=Resolve-PackageLocalPath $repository (Join-Path $payload $file.path)
    $directory=[IO.Path]::GetDirectoryName($file.path).Replace('/','\')
    $target=if ($directory) { '{app}\'+$directory } else { '{app}' }
    $lines+='Source: "'+$source+'"; DestDir: "'+$target+'"; Flags: ignoreversion'
    $files+= [ordered]@{path=$file.path;sha256=(Get-FileHash -LiteralPath $source).Hash.ToLowerInvariant();bytes=(Get-Item -LiteralPath $source).Length}
}
$utf8=[Text.UTF8Encoding]::new($false)
[IO.File]::WriteAllText($include,($lines -join "`n")+"`n",$utf8)
$template=Join-Path $repository 'installer/DAC.iss'
$arguments=@('--no-ide-signtools','--no-signing',('--define=PayloadInclude='+$include),('--define=OutputPath='+$output))
if ($FixtureRoot) { $arguments+=@(('--define=FixtureRoot='+$FixtureRoot),('--define=FixtureId='+$FixtureId)) }
$arguments+=$template
$start=[Diagnostics.ProcessStartInfo]::new((Join-Path $repository '.tools/inno-7.1.0/ISCC.exe'))
$start.UseShellExecute=$false;$start.CreateNoWindow=$true
$start.RedirectStandardOutput=$true;$start.RedirectStandardError=$true
foreach ($argument in $arguments) { $start.ArgumentList.Add($argument) }
$process=[Diagnostics.Process]::Start($start)
$stdout=$process.StandardOutput.ReadToEndAsync();$stderr=$process.StandardError.ReadToEndAsync()
if (-not $process.WaitForExit(120000)) { $process.Kill($true);throw 'Owned installer compilation timed out.' }
# Full compiler output remains local and ignored.
[IO.File]::WriteAllText((Join-Path $output 'compiler.stdout.log'),$stdout.GetAwaiter().GetResult(),$utf8)
[IO.File]::WriteAllText((Join-Path $output 'compiler.stderr.log'),$stderr.GetAwaiter().GetResult(),$utf8)
if ($process.ExitCode -ne 0) { throw 'Installer compilation failed; inspect private compiler logs.' }
$name=if ($FixtureRoot) {'DAC-Setup-0.1.0-dev-windows-x64-fixture.exe'} else {'DAC-Setup-0.1.0-dev-windows-x64.exe'}
$installer=Join-Path $output $name
$commit=(& git -C $repository rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0 -or $commit -cnotmatch '^[0-9a-f]{40}$') {throw 'Source commit is unavailable.'}
$status=@(& git --no-optional-locks -C $repository status --porcelain=v1)
if ($LASTEXITCODE -ne 0) {throw 'Source working-tree state is unavailable.'}
$sourceNames=@(& git -C $repository ls-files --cached --others --exclude-standard | Sort-Object -Unique)
if ($LASTEXITCODE -ne 0) {throw 'Source inventory is unavailable.'}
$sourceLines=@(foreach ($sourceName in $sourceNames) {
    $sourceFile=Resolve-PackageLocalPath $repository (Join-Path $repository $sourceName)
    $sourceName+' '+(Get-FileHash -LiteralPath $sourceFile).Hash.ToLowerInvariant()
})
$sourceHash=[Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($utf8.GetBytes(($sourceLines -join "`n")+"`n"))).ToLowerInvariant()
$receipt=[ordered]@{
    formatVersion=1; sourceCommit=$commit; workingTreeModified=($status.Count -ne 0)
    sourceTreeSha256=$sourceHash; sourceFiles=$sourceNames.Count; fixture=[bool]$FixtureRoot; fixtureId=$FixtureId
    installer=$name; installerSha256=(Get-FileHash -LiteralPath $installer).Hash.ToLowerInvariant()
    installerBytes=(Get-Item -LiteralPath $installer).Length
    archiveSha256=$inputReceipt.archiveSha256; executableSha256=$inputReceipt.executableSha256
    compilerSha256=$compilerReceipt.compilerSha256; compilerVersion=$compilerReceipt.version
    scriptSha256=(Get-FileHash -LiteralPath $template).Hash.ToLowerInvariant()
    includeSha256=(Get-FileHash -LiteralPath $include).Hash.ToLowerInvariant()
    files=$files; productExecuted=$false; signed=$false
}
[IO.File]::WriteAllText($installer+'.verification.json',($receipt|ConvertTo-Json -Depth 5)+"`n",$utf8)
[IO.File]::WriteAllText($installer+'.sha256',$receipt.installerSha256+'  '+$name+"`n",$utf8)
& (Join-Path $PSScriptRoot 'verify-installer.ps1') -Installer $installer -Archive $archivePath
