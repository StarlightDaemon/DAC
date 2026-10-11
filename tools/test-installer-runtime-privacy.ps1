#Requires -Version 7.0
[CmdletBinding()]
param([Parameter(Mandatory)][string]$Installer)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
trap {Write-Output ('Runtime privacy regression failed; script line '+$_.InvocationInfo.ScriptLineNumber+'.');exit 1}
$repository=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
. (Join-Path $PSScriptRoot 'Package.Common.ps1')
. (Join-Path $PSScriptRoot 'Installer.Common.ps1')
$tokens=$null;$errors=$null
$ast=[Management.Automation.Language.Parser]::ParseFile((Join-Path $PSScriptRoot 'audit-publication.ps1'),[ref]$tokens,[ref]$errors)
if ($errors.Count) {throw 'Auditor syntax failed.'}
foreach ($definition in $ast.FindAll({param($node) $node -is [Management.Automation.Language.FunctionDefinitionAst]},$false)) {
    . ([scriptblock]::Create($definition.Extent.Text))
}
$assignment=$ast.FindAll({param($node) $node -is [Management.Automation.Language.AssignmentStatementAst] -and $node.Left.Extent.Text -ceq '$publicRules'},$false)
if ($assignment.Count -ne 1) {throw 'Public privacy rules unavailable.'}
. ([scriptblock]::Create($assignment[0].Extent.Text))
$utf8=[Text.UTF8Encoding]::new($false,$true)
$deadline=[DateTime]::UtcNow.AddSeconds(120)
$cases=[Collections.Generic.List[object]]::new()
function Check([bool]$Condition,[string]$Label,[string]$Kind='negative') {
    if (-not $Condition -or [DateTime]::UtcNow -gt $deadline) {Write-Output ('Privacy case failed: '+$Label+'; deadline exceeded: '+([DateTime]::UtcNow -gt $deadline));throw ('Privacy regression failed: '+$Label)}
    $cases.Add([pscustomobject]@{case=$Label;kind=$Kind;result='passed'})
}
function Check-Rejection([string]$Label,[scriptblock]$Action) {
    $rejected=$false
    try {& $Action | Out-Null} catch {$rejected=$true}
    Check $rejected $Label
}
function Reset-Scan {
    $script:findings=[Collections.Generic.List[object]]::new()
    $script:inventory=[Collections.Generic.List[object]]::new()
    $script:statistics=[ordered]@{genericWindowsOsPathReferences=0;syntheticRedactionFixtures=0;reviewedPublicVendorTokens=0;reviewedPublicVendorUtf16Occurrences=0;executableAsciiStrings=0;executableUtf16Strings=0}
}
function Scan-Fixture([byte[]]$Bytes,$Proof) {
    Reset-Scan
    Scan-Bytes $Bytes 'installer' 'decoded-setup-runtime' @('decoded-setup-runtime') $true $false $Proof
}
function Put-Text([byte[]]$Bytes,[int]$At,[string]$Text) {
    $data=[Text.Encoding]::Unicode.GetBytes($Text)
    [Array]::Copy($data,0,$Bytes,$At,$data.Length)
}
$vendor=[IO.File]::ReadAllBytes((Join-Path $repository '.tools/inno-7.1.0/Setup.e64'))
$provenance=Get-ReviewedRuntimeVendorEvidence
Check ($provenance.verification -ceq 'passed') 'actual-pinned-vendor-provenance' 'positive'
$embedded=Read-InstallerEmbeddedContent (Resolve-PackageLocalPath $repository $Installer)
$runtime=$embedded.runtime
$proof=Get-ReviewedRuntimeProof $runtime $vendor $provenance
Check ($proof.resourceBytes -eq 1452 -and $embedded.payloads.Count -eq 19) 'actual-decoded-resource-proof' 'positive'
# Obtain the reviewed word from pinned bytes. Never persist a host identifier.
$word=[Text.Encoding]::Unicode.GetString($vendor,6863116,18)
$identity=[pscustomobject]@{name='private-runtime-identity';regex=[regex]::new('(?<![\p{L}\p{N}_])'+[regex]::Escape($word)+'(?![\p{L}\p{N}_])',[Text.RegularExpressions.RegexOptions]::IgnoreCase,[TimeSpan]::FromSeconds(3))}
$otherIdentity=[pscustomobject]@{name='private-runtime-identity';regex=[regex]::new('PRIVATERUNTIMEFIXTURE')}
$compiledRules=@($publicRules | ForEach-Object {[pscustomobject]@{name=$_.name;regex=[regex]::new($_.pattern,[Text.RegularExpressions.RegexOptions]::None,[TimeSpan]::FromSeconds(3))}})+@($identity,$otherIdentity)
$reviewedVendorToken=$true
Scan-Fixture $runtime $proof
Check ($findings.Count -eq 0 -and $statistics.reviewedPublicVendorUtf16Occurrences -eq 1) 'one-proven-occurrence' 'positive'
Scan-Fixture $runtime $null
Check (@($findings | Where-Object encoding -CEQ 'utf16le-strings-offset-0').Count -eq 1 -and $statistics.reviewedPublicVendorUtf16Occurrences -eq 0) 'proof-required'
# Identical host/user rules refer to one byte occurrence, counted only once.
$compiledRules+=@($identity)
Scan-Fixture $runtime $proof
Check ($findings.Count -eq 0 -and $statistics.reviewedPublicVendorUtf16Occurrences -eq 1) 'duplicate-rules-single-byte-occurrence' 'positive'
$compiledRules=$compiledRules[0..($compiledRules.Count-2)]
foreach ($field in @('version','verification','authenticode','portable','publisher','downloadSha256','downloadBytes','inventoryFiles','compilerSha256','inventorySha256','runtimeSha256')) {
    $changed=$provenance|ConvertTo-Json|ConvertFrom-Json
    switch ($field) {
        'portable' {$changed.$field=$false}
        {$_ -in @('downloadBytes','inventoryFiles')} {$changed.$field++}
        default {$changed.$field='INVALID-FIXTURE'}
    }
    Check-Rejection ('vendor-'+$field) {Get-ReviewedRuntimeProof $runtime $vendor $changed}
}
$changed=$provenance|ConvertTo-Json|ConvertFrom-Json;$changed.portable='true'
Check-Rejection 'vendor-provenance-types' {Assert-ReviewedRuntimeVendor $changed}
$changedVendor=[byte[]]$vendor.Clone();$changedVendor[1048576]=$changedVendor[1048576] -bxor 1
Check-Rejection 'actual-vendor-bytes-changed' {Get-ReviewedRuntimeProof $runtime $changedVendor $provenance}
foreach ($at in @(6862212,6863100,6863102,6863116,6863114,6863134,6862604,6863182)) {
    $changed=[byte[]]$runtime.Clone();$changed[$at]=$changed[$at] -bxor 1
    Check-Rejection ('resource-message-match-boundary-'+$at) {Get-ReviewedRuntimeProof $changed $vendor $provenance}
}
# Actual resource-directory RVA/length changes cannot authorize identical text.
$resource=[DacInstallerReader]::Resource($runtime,6,4081,0)
foreach ($field in 0,4) {
    $changed=[byte[]]$runtime.Clone();$at=[int]$resource[2]+$field
    $value=[BitConverter]::GetBytes([BitConverter]::ToUInt32($changed,$at)+[uint32]2)
    [Array]::Copy($value,0,$changed,$at,4)
    Check-Rejection ('resource-directory-'+$field) {Get-ReviewedRuntimeProof $changed $vendor $provenance}
}
$run=[Text.Encoding]::Unicode.GetString($runtime,6862606,576)
function New-RunContext($Proof) {
    [pscustomobject]@{proof=$Proof;runs=@([pscustomobject]@{textStart=0;normalizedLength=$run.Length;byteOffset=6862606;raw=$run});seen=[Collections.Generic.HashSet[int]]::new()}
}
foreach ($field in @('decodedSha256','resourceOffset','resourceBytes','resourceSha256','runOffset','runBytes','matchOffset','matchBytes')) {
    $changed=$proof|ConvertTo-Json|ConvertFrom-Json
    if ($field.EndsWith('Sha256')) {$changed.$field='INVALID-FIXTURE'} else {$changed.$field++}
    if ($field -ceq 'decodedSha256') {Check-Rejection ('proof-'+$field) {Scan-Fixture $runtime $changed}}
    else {
        Reset-Scan
        Scan-Text $run 'installer' 'decoded-setup-runtime' 'utf16le-strings-offset-0' $false (New-RunContext $changed)
        Check (@($findings | Where-Object encoding -CEQ 'utf16le-strings-offset-0').Count -eq 1 -and $statistics.reviewedPublicVendorUtf16Occurrences -eq 0) ('proof-'+$field)
    }
}
$changed=[byte[]]$runtime.Clone();Put-Text $changed 6863116 (' '*9);Put-Text $changed 1048576 (' '+$word+' ')
Check-Rejection 'relocated-match' {Get-ReviewedRuntimeProof $changed $vendor $provenance}
foreach ($kind in @('duplicated-word','duplicated-run','odd-alignment','isolated-word','unrelated-identity','nearby-path','nearby-credential','escaped-path-before-run')) {
    $changed=[byte[]]$runtime.Clone()
    switch ($kind) {
        'duplicated-word' {Put-Text $changed 6863680 (' '+$word+' ')}
        'duplicated-run' {[Array]::Copy($runtime,6862604,$changed,1048576,580)}
        'odd-alignment' {Put-Text $changed 6863681 (' '+$word+' ')}
        'isolated-word' {Put-Text $changed 6863680 ([char]0+$word+[char]0)}
        'unrelated-identity' {Put-Text $changed 6863680 ' PRIVATERUNTIMEFIXTURE '}
        'nearby-path' {Put-Text $changed 6863680 (@('C:','Users','privacy-fixture','AppData','sensitive') -join '\')}
        'nearby-credential' {Put-Text $changed 6863680 (' gh'+'p_'+('a'*30)+' ')}
        'escaped-path-before-run' {Put-Text $changed 1048576 (@('C:','Users','privacy-fixture','AppData','sensitive') -join '\\')}
    }
    $changedProof=Get-ReviewedRuntimeProof $changed $vendor $provenance
    Scan-Fixture $changed $changedProof
    $rule=switch ($kind) {'nearby-path' {'private-user-profile'} 'escaped-path-before-run' {'private-user-profile'} 'nearby-credential' {'credential-github-token'} default {'private-runtime-identity'}}
    $aligned=($kind -cne 'odd-alignment' -or @($findings | Where-Object encoding -CEQ 'utf16le-strings-offset-1').Count -gt 0)
    Check (@($findings | Where-Object rule -CEQ $rule).Count -gt 0 -and $statistics.reviewedPublicVendorUtf16Occurrences -eq 1 -and $aligned) $kind
}
# Same complete scanner run in every unapproved scope still produces identity
# findings; this exercises the production Scan-Text boundary, not a test regex.
$run=[Text.Encoding]::Unicode.GetString($runtime,6862606,576)
$context=[pscustomobject]@{proof=$proof;runs=@([pscustomobject]@{textStart=0;normalizedLength=$run.Length;byteOffset=6862606;raw=$run});seen=[Collections.Generic.HashSet[int]]::new()}
foreach ($scope in @('worktree','blob','history','commit','package','installer-metadata','installer-payload')) {
    Reset-Scan
    Scan-Text $run $scope 'decoded-setup-runtime' 'utf16le-strings-offset-0' $false $context
    Check (@($findings | Where-Object rule -CEQ 'private-runtime-identity').Count -eq 1 -and $statistics.reviewedPublicVendorUtf16Occurrences -eq 0) ('scope-'+$scope)
}
foreach ($item in @('DAC-Setup-0.1.0-dev-windows-x64.exe','decoded-setup-metadata','DAC.exe')) {
    Reset-Scan
    Scan-Text $run 'installer' $item 'utf16le-strings-offset-0' $false $context
    Check ($findings.Count -eq 1 -and $statistics.reviewedPublicVendorUtf16Occurrences -eq 0) ('item-'+$item)
}
foreach ($encoding in @('utf8','ascii-strings','utf16le-strings-offset-1')) {
    Reset-Scan
    Scan-Text $run 'installer' 'decoded-setup-runtime' $encoding $false $context
    Check ($findings.Count -eq 1 -and $statistics.reviewedPublicVendorUtf16Occurrences -eq 0) ('encoding-'+$encoding)
}
Reset-Scan
$compiledRules=@([pscustomobject]@{name='other-private-rule';regex=$identity.regex})
Scan-Text $run 'installer' 'decoded-setup-runtime' 'utf16le-strings-offset-0' $false $context
Check ($findings.Count -eq 1 -and $statistics.reviewedPublicVendorUtf16Occurrences -eq 0) 'other-rule-never-suppressed'
[ordered]@{verification='passed';checks=$cases.Count;positiveChecks=@($cases|Where-Object kind -CEQ 'positive').Count;negativeChecks=@($cases|Where-Object kind -CEQ 'negative').Count;resourceBytes=1452;resourceSha256=$proof.resourceSha256;oneOccurrenceOnly=$true;installationExecuted=$false;sourceAndToolchainMutated=$false;cases=@($cases)}|ConvertTo-Json -Depth 5
