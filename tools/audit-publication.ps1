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
$statistics = [ordered]@{ genericWindowsOsPathReferences=0; syntheticRedactionFixtures=0; executableAsciiStrings=0; executableUtf16Strings=0; reviewedPublicVendorTokens=0; reviewedPublicVendorUtf16Occurrences=0 }
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
function Scan-Text([string]$Text,[string]$Scope,[string]$Item,[string]$Encoding='utf8',[bool]$SyntheticFixture=$false,$WideContext=$null) {
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
            if (Test-ReviewedRuntimeMatch $Scope $Item $Encoding $rule.name $match $WideContext) {
                if ($WideContext.seen.Add(6863116)) {++$statistics.reviewedPublicVendorUtf16Occurrences}
                continue
            }
            $line = 1
            if ($Encoding -eq 'utf8' -or $Encoding -eq 'metadata') { $line += ([regex]::Matches($normalized.Substring(0,$match.Index),"`n")).Count }
            Add-Finding $Scope $Item $rule.name $line $Encoding
        }
    }
}
# DAC-INSTALLER-06 established this one unchanged vendor resource. A proof is
# usable only with the same decoded bytes and real scanner byte coordinates.
function Assert-ReviewedRuntimeVendor($Evidence) {
    $pins=[ordered]@{
        version='7.1.0';verification='passed';authenticode='Valid';portable=$true
        publisher='CN=Pyrsys B.V., O=Pyrsys B.V., S=Noord-Holland, C=NL'
        downloadSha256='0362a383ed217d4c4239b5933866dd96d3eb2102737da92f80f6057a4b40df2f'
        downloadBytes=14304168;inventoryFiles=132
        compilerSha256='d06ebd38f38e3cee60a3c50cc45bd449d77e0bc6a5cabc607ea9886808e4de1a'
        inventorySha256='8e61cdf72020ee9a89b8a28305b5300e269468ea334779adf2abf8dae90b618a'
        runtimeSha256='ad12a06d09afefa9d1283c6616ef4d56289dc14a7137a12b24653a12637216bb'
    }
    foreach ($key in $pins.Keys) {if ($Evidence.$key -cne $pins[$key]) {throw 'Reviewed runtime vendor provenance changed.'}}
    if ($Evidence.portable -isnot [bool] -or $Evidence.downloadBytes -isnot [long] -and $Evidence.downloadBytes -isnot [int] -or
        $Evidence.inventoryFiles -isnot [long] -and $Evidence.inventoryFiles -isnot [int]) {throw 'Reviewed runtime provenance types changed.'}
}
function Get-ReviewedRuntimeVendorEvidence {
    $evidence=& (Join-Path $repository 'tools/bootstrap-installer.ps1') -Offline -VerifyOnly | ConvertFrom-Json
    $evidence | Add-Member -NotePropertyName inventorySha256 -NotePropertyValue ((Get-FileHash -LiteralPath (Join-Path $repository '.tools/inno-7.1.0.inventory.json')).Hash.ToLowerInvariant())
    $evidence | Add-Member -NotePropertyName runtimeSha256 -NotePropertyValue ((Get-FileHash -LiteralPath (Join-Path $repository '.tools/inno-7.1.0/Setup.e64')).Hash.ToLowerInvariant())
    Assert-ReviewedRuntimeVendor $evidence
    return $evidence
}
function Get-ReviewedRuntimeProof([byte[]]$Runtime,[byte[]]$Vendor,$Evidence) {
    Assert-ReviewedRuntimeVendor $Evidence
    if ((Hash-Bytes $Vendor) -cne 'ad12a06d09afefa9d1283c6616ef4d56289dc14a7137a12b24653a12637216bb' -or
        $Runtime.Length -ne 6915072) {throw 'Reviewed runtime byte identity changed.'}
    Initialize-InstallerReader
    foreach ($bytes in @($Runtime,$Vendor)) {
        $resource=[DacInstallerReader]::Resource($bytes,6,4081,0)
        $data=[DacInstallerReader]::Part($bytes,$resource[0],$resource[1])
        if ($resource[0] -ne 6862212 -or $resource[1] -ne 1452 -or
            (Hash-Bytes $data) -cne 'b02caa228cd8290d58c399c4f99fddf6f2f6fc81cf3297faf2ca346e1f5c085b') {throw 'Reviewed runtime resource changed.'}
        # RT_STRING block 4081, index 9 is ID 65289: validate serialization,
        # not a matching substring elsewhere in the resource.
        [long]$at=$resource[0]
        foreach ($index in 0..15) {
            [DacInstallerReader]::Range($bytes,$at,2)
            $characters=[BitConverter]::ToUInt16($bytes,[int]$at);$at+=2
            if ($at+2*$characters -gt $resource[0]+$resource[1]) {throw 'Runtime string resource is truncated.'}
            if ($index -eq 9 -and ($at -ne 6863102 -or $characters -ne 40)) {throw 'Reviewed runtime message position changed.'}
            $at+=2*$characters
        }
        if ($at -ne $resource[0]+$resource[1]) {throw 'Reviewed runtime resource serialization changed.'}
        foreach ($range in @(
            @(6863102,80,'6b7c30b0c3bcff307358ab78de723758a23a0b37d1ff8fc9da5df3c0db7219c2'),
            @(6863116,18,'99b413a99c7d7c01ef664d99265100601cd536f23d9a84cb4413c046c3238c01'),
            @(6862606,576,'128fbaab07d46e90e5992627b9fcdbc3d771f1cf772284bb7b0c84c214e36a17')
        )) {if ((Hash-Bytes ([DacInstallerReader]::Part($bytes,$range[0],$range[1]))) -cne $range[2]) {throw 'Reviewed runtime message/match/run bytes changed.'}}
        if ([BitConverter]::ToUInt16($bytes,6863114) -ne 32 -or [BitConverter]::ToUInt16($bytes,6863134) -ne 32 -or
            [BitConverter]::ToUInt16($bytes,6862604) -ne 25 -or [BitConverter]::ToUInt16($bytes,6863182) -ne 30) {throw 'Reviewed runtime boundaries changed.'}
    }
    if (-not [Linq.Enumerable]::SequenceEqual[byte]([DacInstallerReader]::Part($Runtime,6862212,1452),[DacInstallerReader]::Part($Vendor,6862212,1452))) {throw 'Reviewed runtime resource differs from vendor bytes.'}
    [pscustomobject]@{decodedSha256=(Hash-Bytes $Runtime);resourceOffset=6862212;resourceBytes=1452;resourceSha256='b02caa228cd8290d58c399c4f99fddf6f2f6fc81cf3297faf2ca346e1f5c085b';runOffset=6862606;runBytes=576;matchOffset=6863116;matchBytes=18}
}
function Test-ReviewedRuntimeMatch([string]$Scope,[string]$Item,[string]$Encoding,[string]$Rule,$Match,$Context) {
    if ($null -eq $Context -or $Scope -cne 'installer' -or $Item -cne 'decoded-setup-runtime' -or
        $Encoding -cne 'utf16le-strings-offset-0' -or $Rule -cne 'private-runtime-identity') {return $false}
    $proof=$Context.proof
    if ($proof.resourceOffset -ne 6862212 -or $proof.resourceBytes -ne 1452 -or
        $proof.resourceSha256 -cne 'b02caa228cd8290d58c399c4f99fddf6f2f6fc81cf3297faf2ca346e1f5c085b' -or
        $proof.runOffset -ne 6862606 -or $proof.runBytes -ne 576 -or $proof.matchOffset -ne 6863116 -or $proof.matchBytes -ne 18) {return $false}
    foreach ($run in $Context.runs) {
        if ($Match.Index -lt $run.textStart -or $Match.Index+$Match.Length -gt $run.textStart+$run.normalizedLength) {continue}
        # Normalization may shrink escaped paths; those runs never authorize an
        # exception. Detection still uses the original normalized joined text.
        if ($run.raw -cne (Normalize-Text $run.raw) -or $run.byteOffset -ne 6862606 -or $run.raw.Length -ne 288 -or
            (Hash-Bytes ([Text.Encoding]::Unicode.GetBytes($run.raw))) -cne '128fbaab07d46e90e5992627b9fcdbc3d771f1cf772284bb7b0c84c214e36a17') {return $false}
        return ($run.byteOffset+2*($Match.Index-$run.textStart) -eq 6863116 -and 2*$Match.Length -eq 18 -and
            (Hash-Bytes ([Text.Encoding]::Unicode.GetBytes($Match.Value))) -ceq '99b413a99c7d7c01ef664d99265100601cd536f23d9a84cb4413c046c3238c01')
    }
    return $false
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
function Scan-Bytes([byte[]]$Bytes,[string]$Scope,[string]$Item,[string[]]$Paths,[bool]$Executable=$false,[bool]$InstallerMetadata=$false,$RuntimeProof=$null) {
    $hash=Hash-Bytes $Bytes
    if ($null -ne $RuntimeProof -and $hash -cne $RuntimeProof.decodedSha256) {throw 'Reviewed proof does not describe scanned bytes.'}
    $entry=[ordered]@{ scope=$Scope; item=$Item; bytes=$Bytes.LongLength; sha256=$hash; files=@($Paths | ForEach-Object { Safe-Name $_ }) }
    if ($Executable) {
        if ((-not $InstallerMetadata -or $Scope -cne 'installer-metadata') -and ($Bytes.Length -lt 2 -or $Bytes[0] -ne 0x4d -or $Bytes[1] -ne 0x5a)) { Add-Finding $Scope $Item 'declared-executable-not-pe' }
        $ascii=[regex]::Matches([Text.Encoding]::Latin1.GetString($Bytes),'[ -~]{4,}')
        $statistics.executableAsciiStrings += $ascii.Count
        Scan-Text (($ascii | ForEach-Object Value) -join "`n") $Scope $Item 'ascii-strings'
        foreach ($offset in 0,1) {
            $length=($Bytes.Length-$offset) -band (-bnot 1)
            if ($length -le 0) { continue }
            $wide=[regex]::Matches([Text.Encoding]::Unicode.GetString($Bytes,$offset,$length),'[\p{L}\p{M}\p{N}\p{P}\p{S} ]{4,}')
            $statistics.executableUtf16Strings += $wide.Count
            $joined=($wide | ForEach-Object Value) -join "`n"
            $context=$null
            if ($null -ne $RuntimeProof -and $offset -eq 0) {
                [int]$rawTextStart=0
                $runs=@(foreach ($run in $wide) {
                    if (2*$run.Index -eq 6862606) {
                        # Only the physical candidate needs a map. Normalize the
                        # complete preceding joined text once, preserving shifts
                        # from escaped paths without allocating a map per string.
                        [pscustomobject]@{textStart=(Normalize-Text $joined.Substring(0,$rawTextStart)).Length;normalizedLength=(Normalize-Text $run.Value).Length;byteOffset=(2*$run.Index);raw=$run.Value}
                    }
                    $rawTextStart+=$run.Length+1
                })
                $context=[pscustomobject]@{proof=$RuntimeProof;runs=$runs;seen=[Collections.Generic.HashSet[int]]::new()}
            }
            Scan-Text $joined $Scope $Item ('utf16le-strings-offset-'+$offset) $false $context
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
    . (Join-Path $PSScriptRoot 'Package.Common.ps1')
    . (Join-Path $PSScriptRoot 'Installer.Common.ps1')
    $embedded=Read-InstallerEmbeddedContent $installerPath
    if (-not $embedded.complete -or $embedded.payloads.Count -ne 19) {throw 'Embedded installer audit incomplete.'}
    $vendorEvidence=Get-ReviewedRuntimeVendorEvidence
    $runtimeProof=Get-ReviewedRuntimeProof $embedded.runtime ([IO.File]::ReadAllBytes($vendor)) $vendorEvidence
    Scan-Bytes $embedded.runtime 'installer' 'decoded-setup-runtime' @('decoded-setup-runtime') $true $false $runtimeProof
    Scan-Bytes $embedded.metadata 'installer-metadata' 'decoded-setup-metadata' @('decoded-setup-metadata') $true $true
    Scan-Bytes $embedded.locations 'installer-metadata' 'decoded-file-locations' @('decoded-file-locations') $true $true
    $spec=Get-PackageSpecification
    $expected=@($spec.files.path | ForEach-Object {'{app}\'+$_.Replace('/','\')} | Sort-Object)
    if (($expected -join '|') -cne ((@($embedded.payloads.destination | Sort-Object)) -join '|')) {throw 'Embedded installer allowlist mismatch.'}
    foreach ($payload in $embedded.payloads) {
        $name=$payload.destination.Substring(6).Replace('\','/')
        Scan-Bytes $payload.bytes 'installer-payload' $name @($name) ($name -ceq 'DAC.exe')
    }
}
$parent=Split-Path $reportPath -Parent; [void][IO.Directory]::CreateDirectory($parent)
$uniqueFindings=@($findings | Sort-Object scope,item,rule,line,encoding -Unique)
$summary=[ordered]@{
    auditVersion=1; base=$baseOid; revision=$revisionOid; outgoingCommits=$outgoing.Count; forbiddenAncestorsChecked=$ForbiddenCommit.Count
    uniqueReachableBlobs=$blobs.Count; newBlobsRelativeToBase=@($blobs.Keys | Where-Object { -not $baseBlobs.Contains($_) }).Count
    worktreeIncluded=[bool]$IncludeWorktree; worktreeFiles=$worktreeFiles; packageFiles=$packageFiles; packageEntryListSha256=$entryListHash
    excludedFromWorktreeInventory=@('Git administrative files','Git-ignored outputs, caches, dependencies and private reports')
    scannedObjects=$inventory.Count; findings=$uniqueFindings.Count; productExecuted=$false; installerIncluded=[bool]$Installer; installerEmbeddedContentChecked=[bool]$Installer; installerPayloadFiles=$(if ($Installer) {$embedded.payloads.Count} else {0})
    result=$(if ($uniqueFindings.Count) { 'review-required' } else { 'passed' })
    rules=@($uniqueFindings | Group-Object rule | Sort-Object Name | ForEach-Object { [pscustomobject]@{rule=$_.Name;count=$_.Count} })
}
$detail=[ordered]@{ summary=$summary; statistics=$statistics; commits=@($commits); inventory=@($inventory); findings=$uniqueFindings }
[IO.File]::WriteAllText($reportPath,($detail | ConvertTo-Json -Depth 10)+"`n",[Text.UTF8Encoding]::new($false))
$summary | ConvertTo-Json -Depth 5
if ($uniqueFindings.Count) { exit 1 }
exit 0
