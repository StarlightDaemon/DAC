#Requires -Version 7.0
<#
.SYNOPSIS
Read-only publication inventory and privacy audit; never executes a product binary.
.DESCRIPTION
Audits every outgoing commit and every unique blob in those commits' trees relative
 to Base. IncludeWorktree also scans tracked and nonignored untracked files before
commit. PackageDirectory is an already verified/extracted package root, accompanied
by PackageEntryList (UTF-8, one exact relative file name per line, no ZIP root prefix).
PrivatePaths and PrivateIdentities are runtime inputs; do not store local values in
this script. GitHub noreply author addresses are accepted; other identities require
review. Findings contain categories and locations, never matching text. Reports are
restricted to ignored out/private; stdout contains a compact sanitized summary.
Exit 0 means no flagged findings, 1 means review required, and 2 means incomplete.
This heuristic audit complements source review and archive verification; it cannot
prove that arbitrary text contains no secret.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$Base,
    [string]$Revision = 'HEAD',
    [string[]]$ForbiddenCommit = @(),
    [string[]]$PrivatePaths = @(),
    [string[]]$PrivateIdentities = @(),
    [switch]$IncludeWorktree,
    [string]$PackageDirectory = '',
    [string]$PackageEntryList = '',
    [string]$Installer = '',
    [string]$Report = 'out/private/publication-audit.json',
    [long]$MaxFileBytes = 16MB
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
trap {
    # Exceptions from the OS may contain local paths or data. Do not echo them.
    Write-Host ('Publication audit incomplete; exception type: ' + $_.Exception.GetType().Name + '; script line: ' + $_.InvocationInfo.ScriptLineNumber)
    exit 2
}
$repository = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$utf8 = [Text.UTF8Encoding]::new($false, $true)
$findings = [Collections.Generic.List[object]]::new()
$inventory = [Collections.Generic.List[object]]::new()
$commits = [Collections.Generic.List[object]]::new()
$statistics = [ordered]@{ genericWindowsOsPathReferences=0; syntheticRedactionFixtures=0; executableAsciiStrings=0; executableUtf16Strings=0; reviewedPublicVendorTokens=0 }
$reviewedVendorToken=$false
if ($Installer) {
    # One complete public ASCII token collides with a runtime identity on some
    # hosts. Accept only its exact hash, only in the installer ASCII scan, only
    # after confirming the unchanged runtime from the verified upstream archive.
    # Every private path, credential and other identity rule remains unchanged.
    $vendor=Join-Path $repository '.tools/inno-7.1.0/Setup.e64'
    if ((Get-FileHash -LiteralPath $vendor).Hash.ToLowerInvariant() -cne 'ad12a06d09afefa9d1283c6616ef4d56289dc14a7137a12b24653a12637216bb') { throw 'Reviewed vendor runtime identity changed.' }
    $vendorStrings=[regex]::Matches([Text.Encoding]::Latin1.GetString([IO.File]::ReadAllBytes($vendor)),'[ -~]{4,}')
    foreach ($vendorString in $vendorStrings) {
        $tokenHash=[Convert]::ToHexString([Security.Cryptography.SHA256]::HashData([Text.Encoding]::Latin1.GetBytes($vendorString.Value))).ToLowerInvariant()
        if ($tokenHash -ceq '67e7c6e28e540b4fd412ad663b634a38572b98d3f4608fc5792f2600d32c1eb9') { $reviewedVendorToken=$true; break }
    }
    if (-not $reviewedVendorToken) { throw 'Reviewed whole public vendor token absent.' }
}
if ($MaxFileBytes -lt 1 -or $MaxFileBytes -gt 64MB) { throw 'Invalid scan byte limit.' }

function Invoke-Git([string[]]$Arguments, [switch]$AllowFailure) {
    $start = [Diagnostics.ProcessStartInfo]::new('git')
    $start.WorkingDirectory = $repository
    $start.UseShellExecute = $false; $start.CreateNoWindow = $true
    $start.RedirectStandardOutput = $true; $start.RedirectStandardError = $true
    foreach ($argument in @('--no-optional-locks','-C',$repository) + $Arguments) { $start.ArgumentList.Add($argument) }
    $process = [Diagnostics.Process]::new(); $process.StartInfo = $start
    $memory = [IO.MemoryStream]::new()
    try {
        [void]$process.Start()
        $copy = $process.StandardOutput.BaseStream.CopyToAsync($memory)
        $errors = $process.StandardError.ReadToEndAsync()
        $process.WaitForExit(); [void]$copy.GetAwaiter().GetResult(); [void]$errors.GetAwaiter().GetResult()
        $code = $process.ExitCode
        if ($code -ne 0 -and -not $AllowFailure) { throw 'Git read failed.' }
        return [pscustomobject]@{ Code=$code; Bytes=$memory.ToArray() }
    } finally { $memory.Dispose(); $process.Dispose() }
}
function Git-Text([string[]]$Arguments) { $result=Invoke-Git $Arguments; return $utf8.GetString($result.Bytes) }
function Hash-Bytes([byte[]]$Bytes) { return [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($Bytes)).ToLowerInvariant() }
function Add-Finding([string]$Scope,[string]$Item,[string]$Rule,[int]$Line=0,[string]$Encoding='text') {
    $findings.Add([pscustomobject]@{ scope=$Scope; item=$Item; rule=$Rule; line=$Line; encoding=$Encoding })
}
function Normalize-Text([string]$Text) { return $Text.Replace('\\','\').Replace('\/','/') }

$privatePathSet = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
foreach ($path in @($PrivatePaths) + @($repository, $env:USERPROFILE, $env:HOME, $env:LOCALAPPDATA, $env:APPDATA, $env:CODEX_HOME)) {
    if ($path -and $path.Trim().Length -gt 3) { [void]$privatePathSet.Add($path.TrimEnd('\','/')) }
}
$privateIdentitySet = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
foreach ($identity in @($PrivateIdentities) + @($env:USERNAME,$env:COMPUTERNAME)) {
    if ($identity -and $identity.Length -ge 4) { [void]$privateIdentitySet.Add($identity) }
}
$rules = [Collections.Generic.List[object]]::new()
foreach ($path in $privatePathSet) {
    $normalized = Normalize-Text $path
    foreach ($variant in @($normalized.Replace('/','\'),$normalized.Replace('\','/')) | Select-Object -Unique) {
        $rules.Add(@{ name='private-runtime-path'; pattern=[regex]::Escape($variant); options=[Text.RegularExpressions.RegexOptions]::IgnoreCase })
    }
}
foreach ($identity in $privateIdentitySet) {
    $rules.Add(@{ name='private-runtime-identity'; pattern='(?<![\p{L}\p{N}_])'+[regex]::Escape($identity)+'(?![\p{L}\p{N}_])'; options=[Text.RegularExpressions.RegexOptions]::IgnoreCase })
}
$publicRules = @(
    @{ name='credential-private-key'; pattern='-----BEGIN (?:RSA |EC |DSA |OPENSSH |ENCRYPTED )?PRIVATE KEY-----' },
    @{ name='credential-github-token'; pattern='\b(?:gh[pousr]_[A-Za-z0-9_]{20,}|github_pat_[A-Za-z0-9_]{30,})\b' },
    @{ name='credential-cloud-access-key'; pattern='\b(?:AKIA|ASIA)[A-Z0-9]{16}\b' },
    @{ name='credential-provider-token'; pattern='\b(?:xox[baprs]-[A-Za-z0-9-]{12,}|sk-(?:proj-|svcacct-)?[A-Za-z0-9_-]{20,}|AIza[A-Za-z0-9_-]{30,})\b' },
    @{ name='credential-jwt'; pattern='\beyJ[A-Za-z0-9_-]{8,}\.[A-Za-z0-9_-]{8,}\.[A-Za-z0-9_-]{8,}\b' },
    @{ name='credential-url-userinfo'; pattern='(?i)https?://[^\s/@:]+:[^\s/@]+@' },
    @{ name='credential-assignment'; pattern='(?i)["'']?(?:password|passwd|api[_-]?key|access[_-]?token|client[_-]?secret|aws_secret_access_key)["'']?\s*[:=]\s*["''][A-Za-z0-9_+/=.-]{12,}["'']' },
    @{ name='private-user-profile'; pattern='(?i)(?:[A-Z]:[\\/]|/(?:home|Users)/)(?:Users[\\/]|Documents and Settings[\\/])?[^\s\\/"''<>|]*[\\/](?:\.codex|\.cache|\.agents|AppData)(?:[\\/]|\b)|[A-Z]:[\\/]Users[\\/](?!Public(?:[\\/]|\b)|Default(?:[\\/]|\b)|All Users(?:[\\/]|\b))[^\s\\/"''<>|]+' },
    @{ name='private-unix-home'; pattern='(?i)/(?:home|Users)/[^\s/"''<>|]+' },
    @{ name='private-runtime-cache'; pattern='(?i)(?:[A-Z]:[\\/]|/(?:home|Users|tmp|opt|var|root)/)[^\r\n"''<>|]{0,256}[\\/](?:\.codex|\.agents|codex-runtimes|\.cache)(?:[\\/]|\b)' }
)
foreach ($rule in $publicRules) { $rules.Add(@{name=$rule.name;pattern=$rule.pattern;options=[Text.RegularExpressions.RegexOptions]::None}) }
$compiledRules = @($rules | ForEach-Object { [pscustomobject]@{ name=$_.name; regex=[regex]::new($_.pattern,$_.options,[TimeSpan]::FromSeconds(3)) } })
function Scan-Text([string]$Text,[string]$Scope,[string]$Item,[string]$Encoding='utf8',[bool]$SyntheticFixture=$false) {
    $normalized = Normalize-Text $Text
    # This exact test payload is the only real-user-profile-shaped exception.
    if ($SyntheticFixture) {
        $fixture = @('C:','Users','PRIVATE-USER','SECRET.scr') -join '\'
        $statistics.syntheticRedactionFixtures += ([regex]::Matches($normalized,[regex]::Escape($fixture))).Count
        $normalized = $normalized.Replace($fixture,'<synthetic-redaction-fixture>')
    }
    $statistics.genericWindowsOsPathReferences += ([regex]::Matches($normalized,'(?i)\b[A-Z]:[\\/](?:Windows|Program Files(?: \(x86\))?|ProgramData)(?:[\\/]|\b)')).Count
    foreach ($rule in $compiledRules) {
        foreach ($match in $rule.regex.Matches($normalized)) {
            if ($reviewedVendorToken -and $Scope -ceq 'installer' -and $Encoding -ceq 'ascii-strings' -and $rule.name -ceq 'private-runtime-identity') {
                $start=$normalized.LastIndexOf("`n",$match.Index)+1
                $end=$normalized.IndexOf("`n",$match.Index)
                if ($end -lt 0) {$end=$normalized.Length}
                $whole=$normalized.Substring($start,$end-$start)
                if ((Hash-Bytes ($utf8.GetBytes($whole))) -ceq '67e7c6e28e540b4fd412ad663b634a38572b98d3f4608fc5792f2600d32c1eb9') {
                    ++$statistics.reviewedPublicVendorTokens
                    continue
                }
            }
            $line = 1
            if ($Encoding -eq 'utf8' -or $Encoding -eq 'metadata') { $line += ([regex]::Matches($normalized.Substring(0,$match.Index),"`n")).Count }
            Add-Finding $Scope $Item $rule.name $line $Encoding
        }
    }
}
function Safe-Name([string]$Name) {
    $normalized=Normalize-Text $Name
    foreach ($rule in $compiledRules) { if ($rule.regex.IsMatch($normalized)) { return 'redacted-name-'+(Hash-Bytes ($utf8.GetBytes($Name))).Substring(0,16) } }
    return $Name
}
function Check-Name([string]$Name,[string]$Scope) {
    $safe=Safe-Name $Name
    Scan-Text $Name $Scope $safe 'metadata'
    if (-not $Name -or $Name.Contains('\') -or $Name.StartsWith('/') -or $Name.Contains(':') -or
        $Name -match '[\x00-\x1f]' -or @($Name.Split('/') | Where-Object { $_ -in @('','.','..') }).Count) { Add-Finding $Scope $safe 'noncanonical-file-name' }
    if ($Name -match '(^|/)(?:\.deps|\.tools|\.git|\.codex|\.agents|build|out|packages|evidence)(/|$)') { Add-Finding $Scope $safe 'private-or-generated-payload-directory' }
    return $safe
}
function Assert-NoReparse([string]$Path,[string]$Boundary) {
    $at=$Path
    while ($at -and $at.Length -ge $Boundary.Length) {
        if ((Test-Path -LiteralPath $at) -and ((Get-Item -LiteralPath $at -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)) { throw 'Reparse path rejected.' }
        if ($at -eq $Boundary) { break }; $at=Split-Path $at -Parent
    }
}
function Scan-Bytes([byte[]]$Bytes,[string]$Scope,[string]$Item,[string[]]$Paths,[bool]$Executable=$false) {
    $hash=Hash-Bytes $Bytes
    $entry=[ordered]@{ scope=$Scope; item=$Item; bytes=$Bytes.LongLength; sha256=$hash; files=@($Paths | ForEach-Object { Safe-Name $_ }) }
    if ($Executable) {
        if ($Bytes.Length -lt 2 -or $Bytes[0] -ne 0x4d -or $Bytes[1] -ne 0x5a) { Add-Finding $Scope $Item 'declared-executable-not-pe' }
        $ascii=[regex]::Matches([Text.Encoding]::Latin1.GetString($Bytes),'[ -~]{4,}')
        $statistics.executableAsciiStrings += $ascii.Count
        Scan-Text (($ascii | ForEach-Object Value) -join "`n") $Scope $Item 'ascii-strings'
        foreach ($offset in 0,1) {
            $length=($Bytes.Length-$offset) -band (-bnot 1)
            if ($length -le 0) { continue }
            $wide=[regex]::Matches([Text.Encoding]::Unicode.GetString($Bytes,$offset,$length),'[\p{L}\p{M}\p{N}\p{P}\p{S} ]{4,}')
            $statistics.executableUtf16Strings += $wide.Count
            Scan-Text (($wide | ForEach-Object Value) -join "`n") $Scope $Item ('utf16le-strings-offset-'+$offset)
        }
        $entry.kind='executable-strings-scanned'
    } else {
        $binary=$Bytes -contains [byte]0 -or ($Bytes.Length -ge 2 -and $Bytes[0] -eq 0x4d -and $Bytes[1] -eq 0x5a)
        try { $text=$utf8.GetString($Bytes) } catch { $binary=$true; $text='' }
        if ($binary) { Add-Finding $Scope $Item 'unexpected-binary-payload'; $entry.kind='binary'; Scan-Text ([Text.Encoding]::Latin1.GetString($Bytes)) $Scope $Item 'binary-ascii' }
        else { $entry.kind='utf8-text'; Scan-Text $text $Scope $Item 'utf8' ($Paths.Count -eq 1 -and $Paths[0] -ceq 'tests/platform.cpp') }
        if ($Scope -ne 'package' -and $Bytes.LongLength -gt 1MB) { Add-Finding $Scope $Item 'large-repository-text-or-payload' }
    }
    $inventory.Add([pscustomobject]$entry)
}
function Check-Extension([string]$Path,[string]$Scope,[string]$Item) {
    $extension=[IO.Path]::GetExtension($Path).ToLowerInvariant()
    $baseName=[IO.Path]::GetFileName($Path)
    $allowed=$extension -in @('.md','.txt','.cmake','.json','.cpp','.hpp','.h','.ps1','.rc','.manifest','.yml','.yaml') -or
        $baseName -in @('LICENSE','Fujin-LICENSE','.gitattributes','.gitignore')
    if ($Scope -eq 'package' -and $Path -ceq 'DAC.exe') { $allowed=$true }
    # The reviewed installer script is text and receives every existing scan.
    # Other .iss files and executable repository files remain disallowed.
    if ($Path -ceq 'installer/DAC.iss' -and $Scope -ne 'package') { $allowed=$true }
    if (-not $allowed) { Add-Finding $Scope $Item 'unexpected-file-extension' }
}
function Tree-Entries([string]$Commit) {
    $result=Invoke-Git @('ls-tree','-r','-z','--full-tree',$Commit)
    foreach ($record in $utf8.GetString($result.Bytes).Split([char]0,[StringSplitOptions]::RemoveEmptyEntries)) {
        if ($record -cnotmatch '^([0-7]{6}) (blob|commit) ([0-9a-f]{40,64})\t(.+)$') { throw 'Unexpected Git tree record.' }
        [pscustomobject]@{mode=$Matches[1];type=$Matches[2];oid=$Matches[3];path=$Matches[4]}
    }
}

$baseOid=(Git-Text @('rev-parse','--verify',($Base+'^{commit}'))).Trim()
$revisionOid=(Git-Text @('rev-parse','--verify',($Revision+'^{commit}'))).Trim()
if ($baseOid -notmatch '^[0-9a-f]{40,64}$' -or $revisionOid -notmatch '^[0-9a-f]{40,64}$') { throw 'Invalid resolved commit.' }
$ancestor=Invoke-Git @('merge-base','--is-ancestor',$baseOid,$revisionOid) -AllowFailure
if ($ancestor.Code -eq 1) { Add-Finding 'history' $revisionOid 'baseline-not-ancestor' } elseif ($ancestor.Code -ne 0) { throw 'Ancestry read failed.' }
foreach ($forbidden in $ForbiddenCommit) {
    $forbiddenOid=(Git-Text @('rev-parse','--verify',($forbidden+'^{commit}'))).Trim()
    $check=Invoke-Git @('merge-base','--is-ancestor',$forbiddenOid,$revisionOid) -AllowFailure
    if ($check.Code -eq 0) { Add-Finding 'history' $revisionOid 'forbidden-ancestor' } elseif ($check.Code -ne 1) { throw 'Forbidden ancestry read failed.' }
}
$baseBlobs=[Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
foreach ($entry in @(Tree-Entries $baseOid)) { if ($entry.type -eq 'blob') { [void]$baseBlobs.Add($entry.oid) } }
$outgoing=@((Git-Text @('rev-list','--reverse',$revisionOid,('^'+$baseOid))) -split '\r?\n' | Where-Object { $_ })
$blobs=[Collections.Generic.Dictionary[string,object]]::new([StringComparer]::Ordinal)
foreach ($commit in $outgoing) {
    $raw=Invoke-Git @('cat-file','commit',$commit); $commitText=$utf8.GetString($raw.Bytes)
    Scan-Text $commitText 'commit' $commit 'metadata'
    $separator=$commitText.IndexOf("`n`n")
    if ($separator -lt 0) { throw 'Malformed commit object.' }
    $headers=$commitText.Substring(0,$separator)
    foreach ($field in @('author','committer')) {
        $identity=[regex]::Match($headers,'(?m)^'+$field+' (.+) <([^<>]+)> [0-9]+ [+-][0-9]{4}$')
        if (-not $identity.Success) { Add-Finding 'commit' $commit ('malformed-'+$field+'-identity') }
        elseif ($identity.Groups[2].Value -cnotmatch '^(?:[0-9]+\+)?[A-Za-z0-9-]+@users\.noreply\.github\.com$|^noreply@github\.com$') { Add-Finding 'commit' $commit ('review-'+$field+'-email') }
    }
    $commits.Add([pscustomobject]@{ commit=$commit; rawCommitSha256=(Hash-Bytes $raw.Bytes); messageScanned=$true; authorAndCommitterScanned=$true })
    foreach ($entry in @(Tree-Entries $commit)) {
        $safe=Check-Name $entry.path 'history-path'; Check-Extension $entry.path 'history-path' $safe
        if ($entry.type -ne 'blob' -or $entry.mode -ne '100644') { Add-Finding 'history' $safe 'unexpected-tree-entry-type-or-mode' }
        if ($entry.type -ne 'blob') { continue }
        if (-not $blobs.ContainsKey($entry.oid)) { $blobs[$entry.oid]=[ordered]@{ paths=[Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal); commits=[Collections.Generic.List[string]]::new() } }
        [void]$blobs[$entry.oid].paths.Add($entry.path); $blobs[$entry.oid].commits.Add($commit)
    }
}
foreach ($oid in @($blobs.Keys | Sort-Object)) {
    [long]$size=(Git-Text @('cat-file','-s',$oid)).Trim()
    if ($size -gt $MaxFileBytes) { Add-Finding 'blob' $oid 'oversized-blob-not-scanned'; continue }
    $raw=Invoke-Git @('cat-file','blob',$oid)
    if ($raw.Bytes.LongLength -ne $size) { throw 'Git blob size mismatch.' }
    Scan-Bytes $raw.Bytes 'blob' $oid @($blobs[$oid].paths | Sort-Object)
    $inventory[$inventory.Count-1] | Add-Member -NotePropertyName commits -NotePropertyValue @($blobs[$oid].commits | Sort-Object -Unique)
    $inventory[$inventory.Count-1] | Add-Member -NotePropertyName presentInBaseline -NotePropertyValue $baseBlobs.Contains($oid)
}
$worktreeFiles=0
if ($IncludeWorktree) {
    $listed=Invoke-Git @('ls-files','-z','--cached','--others','--exclude-standard')
    foreach ($relative in @($utf8.GetString($listed.Bytes).Split([char]0,[StringSplitOptions]::RemoveEmptyEntries) | Sort-Object -Unique)) {
        $safe=Check-Name $relative 'worktree'; Check-Extension $relative 'worktree' $safe
        $path=[IO.Path]::GetFullPath((Join-Path $repository $relative))
        if (-not $path.StartsWith($repository+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) { throw 'Worktree path escaped root.' }
        Assert-NoReparse $path $repository
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { Add-Finding 'worktree' $safe 'missing-tracked-file'; continue }
        $file=Get-Item -LiteralPath $path; ++$worktreeFiles
        if ($file.Length -gt $MaxFileBytes) { Add-Finding 'worktree' $safe 'oversized-file-not-scanned'; continue }
        Scan-Bytes ([IO.File]::ReadAllBytes($path)) 'worktree' $safe @($relative)
    }
}
$packageFiles=0; $entryListHash=$null
if ($PackageDirectory -or $PackageEntryList) {
    if (-not $PackageDirectory -or -not $PackageEntryList) { throw 'Package root and exact entry list are both required.' }
    $packageRoot=(Resolve-Path -LiteralPath $PackageDirectory).Path.TrimEnd('\','/')
    Assert-NoReparse $packageRoot ([IO.Path]::GetPathRoot($packageRoot))
    $listBytes=[IO.File]::ReadAllBytes((Resolve-Path -LiteralPath $PackageEntryList).Path)
    $entryListHash=Hash-Bytes $listBytes
    $expected=[Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
    foreach ($name in ($utf8.GetString($listBytes).TrimStart([char]0xfeff) -split '\r?\n')) {
        if (-not $name) { continue }
        $safe=Check-Name $name 'package-entry-list'
        if (-not $expected.Add($name)) { Add-Finding 'package-entry-list' $safe 'duplicate-entry' }
    }
    $actual=[Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
    $caseNames=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    # Walk manually so an unexpected reparse directory is never traversed.
    $pending=[Collections.Generic.Stack[string]]::new(); $pending.Push($packageRoot)
    while ($pending.Count) {
        foreach ($file in Get-ChildItem -LiteralPath $pending.Pop() -Force) {
            $relative=[IO.Path]::GetRelativePath($packageRoot,$file.FullName).Replace('\','/')
            $safe=Check-Name $relative 'package'
            if ($file.Attributes -band [IO.FileAttributes]::ReparsePoint) { Add-Finding 'package' $safe 'reparse-entry-not-scanned'; continue }
            if ($file.PSIsContainer) { $pending.Push($file.FullName); continue }
            ++$packageFiles; [void]$actual.Add($relative)
            if (-not $caseNames.Add($relative)) { Add-Finding 'package' $safe 'case-colliding-entry' }
            Check-Extension $relative 'package' $safe
            if ($file.Length -gt $MaxFileBytes) { Add-Finding 'package' $safe 'oversized-file-not-scanned'; continue }
            Scan-Bytes ([IO.File]::ReadAllBytes($file.FullName)) 'package' $safe @($relative) ($relative -ceq 'DAC.exe')
        }
    }
    foreach ($name in $expected) { if (-not $actual.Contains($name)) { Add-Finding 'package' (Safe-Name $name) 'entry-list-file-missing' } }
    foreach ($name in $actual) { if (-not $expected.Contains($name)) { Add-Finding 'package' (Safe-Name $name) 'file-absent-from-entry-list' } }
    if (-not $actual.Contains('DAC.exe')) { Add-Finding 'package' 'DAC.exe' 'required-executable-missing' }
}
$reportPath=if ([IO.Path]::IsPathRooted($Report)) { [IO.Path]::GetFullPath($Report) } else { [IO.Path]::GetFullPath((Join-Path $repository $Report)) }
$privateRoot=[IO.Path]::GetFullPath((Join-Path $repository 'out/private'))
if (-not $reportPath.StartsWith($privateRoot+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) { throw 'Reports must stay under ignored out/private.' }
Assert-NoReparse $reportPath $repository
$relativeReport=[IO.Path]::GetRelativePath($repository,$reportPath).Replace('\','/')
$ignored=Invoke-Git @('check-ignore','--quiet','--',$relativeReport) -AllowFailure
if ($ignored.Code -ne 0) { throw 'Audit report is not ignored.' }
if ($Installer) {
    $installerPath=(Resolve-Path -LiteralPath $Installer).Path
    if (-not $installerPath.StartsWith($repository+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase) -or
        [IO.Path]::GetFileName($installerPath) -cne 'DAC-Setup-0.1.0-dev-windows-x64.exe') { throw 'Unexpected installer audit identity.' }
    Assert-NoReparse $installerPath $repository
    if ((Get-Item -LiteralPath $installerPath).Length -gt $MaxFileBytes) { throw 'Installer exceeds audit size bound.' }
    Scan-Bytes ([IO.File]::ReadAllBytes($installerPath)) 'installer' 'DAC-Setup-0.1.0-dev-windows-x64.exe' @('DAC-Setup-0.1.0-dev-windows-x64.exe') $true
}
$parent=Split-Path $reportPath -Parent; [void][IO.Directory]::CreateDirectory($parent)
$uniqueFindings=@($findings | Sort-Object scope,item,rule,line,encoding -Unique)
$summary=[ordered]@{
    auditVersion=1; base=$baseOid; revision=$revisionOid; outgoingCommits=$outgoing.Count; forbiddenAncestorsChecked=$ForbiddenCommit.Count
    uniqueReachableBlobs=$blobs.Count; newBlobsRelativeToBase=@($blobs.Keys | Where-Object { -not $baseBlobs.Contains($_) }).Count
    worktreeIncluded=[bool]$IncludeWorktree; worktreeFiles=$worktreeFiles; packageFiles=$packageFiles; packageEntryListSha256=$entryListHash
    excludedFromWorktreeInventory=@('Git administrative files','Git-ignored outputs, caches, dependencies and private reports')
    scannedObjects=$inventory.Count; findings=$uniqueFindings.Count; productExecuted=$false; installerIncluded=[bool]$Installer
    result=$(if ($uniqueFindings.Count) { 'review-required' } else { 'passed' })
    rules=@($uniqueFindings | Group-Object rule | Sort-Object Name | ForEach-Object { [pscustomobject]@{rule=$_.Name;count=$_.Count} })
}
$detail=[ordered]@{ summary=$summary; statistics=$statistics; commits=@($commits); inventory=@($inventory); findings=$uniqueFindings }
[IO.File]::WriteAllText($reportPath,($detail | ConvertTo-Json -Depth 10)+"`n",[Text.UTF8Encoding]::new($false))
$summary | ConvertTo-Json -Depth 5
if ($uniqueFindings.Count) { exit 1 }
exit 0
