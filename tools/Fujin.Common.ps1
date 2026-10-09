Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Get-FujinLock([string] $LockPath) {
    $lock = Get-Content -LiteralPath $LockPath -Raw | ConvertFrom-Json
    if ($lock.formatVersion -ne 1 -or $lock.repository -ne 'https://github.com/StarlightDaemon/Fujin.git' -or
        $lock.tag -notmatch '^v\d+\.\d+\.\d+$' -or $lock.commit -notmatch '^[a-f0-9]{40}$' -or
        $lock.tagObject -notmatch '^[a-f0-9]{40}$' -or $lock.license -ne 'MIT') {
        throw 'FUJIN_LOCK: invalid dependency identity.'
    }
    $required = @('LICENSE', 'dist/tokens-resolved.json', 'dist/tokens.css')
    if (@($lock.files).Count -ne $required.Count) { throw 'FUJIN_LOCK: incorrect file set.' }
    foreach ($name in $required) {
        $entry = @($lock.files | Where-Object { $_.path -ceq $name })
        if ($entry.Count -ne 1 -or $entry[0].sha256 -notmatch '^[a-f0-9]{64}$' -or
            $entry[0].gitBlob -notmatch '^[a-f0-9]{40}$') { throw "FUJIN_LOCK: invalid entry $name." }
    }
    return $lock
}

function Invoke-FujinGit([string] $Directory, [string[]] $Arguments) {
    $result = & git --no-optional-locks -C $Directory @Arguments 2>&1
    if ($LASTEXITCODE -ne 0) { throw "FUJIN_GIT: git $($Arguments -join ' ') failed: $result" }
    return ($result -join "`n").Trim()
}

function Assert-FujinRemote([string] $Directory, $Lock) {
    $origin = Invoke-FujinGit $Directory @('remote', 'get-url', 'origin')
    if ($origin -cne $Lock.repository -and $origin -cne 'git@github.com:StarlightDaemon/Fujin.git') {
        throw "FUJIN_IDENTITY: unexpected origin $origin."
    }
    $tag = Invoke-FujinGit $Directory @('rev-parse', '--verify', "refs/tags/$($Lock.tag)")
    $commit = Invoke-FujinGit $Directory @('rev-parse', '--verify', "refs/tags/$($Lock.tag)^{commit}")
    if ($tag -cne $Lock.tagObject -or $commit -cne $Lock.commit) {
        throw 'FUJIN_REVISION: release tag identity differs from the dependency lock.'
    }
}

function Assert-FujinCheckout([string] $Directory, $Lock) {
    Assert-FujinRemote $Directory $Lock
    $head = Invoke-FujinGit $Directory @('rev-parse', 'HEAD')
    if ($head -cne $Lock.commit) { throw "FUJIN_REVISION: HEAD must be $($Lock.commit)." }
    $status = Invoke-FujinGit $Directory @('status', '--porcelain=v1', '--untracked-files=normal')
    if ($status.Length -ne 0) { throw 'FUJIN_INTEGRITY: dependency checkout is not clean.' }
    foreach ($entry in $Lock.files) {
        $blob = Invoke-FujinGit $Directory @('rev-parse', "HEAD:$($entry.path)")
        if ($blob -cne $entry.gitBlob) { throw "FUJIN_INTEGRITY: wrong Git blob for $($entry.path)." }
    }
}

function Read-FujinThemeData([string] $Directory, $Lock) {
    foreach ($entry in $Lock.files) {
        $path = Join-Path $Directory $entry.path
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "FUJIN_INTEGRITY: missing $($entry.path)." }
        $hash = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()
        if ($hash -cne $entry.sha256) { throw "FUJIN_INTEGRITY: SHA-256 mismatch for $($entry.path)." }
    }
    try { $json = Get-Content -LiteralPath (Join-Path $Directory 'dist/tokens-resolved.json') -Raw | ConvertFrom-Json }
    catch { throw "FUJIN_SCHEMA: invalid resolved JSON: $($_.Exception.Message)" }
    foreach ($key in @('meta', 'tokens', 'dark', 'light')) {
        if ($null -eq $json.PSObject.Properties[$key]) { throw "FUJIN_SCHEMA: missing $key." }
    }
    if ($json.meta.version -cne $Lock.tag.Substring(1) -or $json.meta.defaultAccent -cne 'violet') {
        throw 'FUJIN_SCHEMA: unexpected release version or default accent.'
    }
    $roles = [ordered]@{
        base = '--fujin-bg-base'; surface = '--fujin-bg-surface'; elevated = '--fujin-bg-elevated';
        text = '--fujin-text-primary'; secondary = '--fujin-text-secondary'; onAccent = '--fujin-text-on-accent';
        border = '--fujin-border-default'; strong = '--fujin-border-strong'; accent = '--fujin-interactive-default';
        hover = '--fujin-interactive-hover'; active = '--fujin-interactive-active';
        chrome = '--fujin-chrome-bg'; chromeText = '--fujin-chrome-text'
    }
    foreach ($mode in @('dark', 'light')) {
        foreach ($key in $roles.Values) {
            $property = $json.$mode.PSObject.Properties[$key]
            if ($null -eq $property -or $property.Value -cnotmatch '^#[0-9a-fA-F]{6}$') {
                throw "FUJIN_SCHEMA: $mode.$key must be an RGB hex color."
            }
        }
    }
    $familyProperty = $json.tokens.PSObject.Properties['fontFamily']
    if ($null -eq $familyProperty -or $familyProperty.Value -cnotmatch '^"([A-Za-z0-9 ]{1,31})",') {
        throw 'FUJIN_SCHEMA: invalid native font family.'
    }
    $family = $Matches[1]
    $css = Get-Content -LiteralPath (Join-Path $Directory 'dist/tokens.css') -Raw
    $numbers = [ordered]@{
        fontSize = '--fujin-font-size-sm'; spacing = '--fujin-spacing-md'; radius = '--fujin-radius-default';
        spacingSmall = '--fujin-spacing-sm'; spacingLarge = '--fujin-spacing-lg';
        borderWidth = '--fujin-border-width-hairline'; fontSizeLarge = '--fujin-font-size-lg'
    }
    $values = [ordered]@{}
    foreach ($pair in $numbers.GetEnumerator()) {
        $matchesForToken = [regex]::Matches($css, '(?m)^\s*' + [regex]::Escape($pair.Value) + ':\s*(\d+)px;\s*$')
        if ($matchesForToken.Count -ne 1) { throw "FUJIN_SCHEMA: missing or ambiguous CSS scalar $($pair.Value)." }
        $number = [int]$matchesForToken[0].Groups[1].Value
        if ($number -gt 128 -or ($pair.Key -ne 'radius' -and $number -le 0)) {
            throw "FUJIN_SCHEMA: out-of-range CSS scalar $($pair.Value)."
        }
        $values[$pair.Key] = $number
    }
    if ($values.radius -ne 0) { throw 'FUJIN_SCHEMA: Fujin radius must remain zero.' }
    $accents = [ordered]@{ violet = @($json.dark.'--fujin-interactive-default', $json.dark.'--fujin-interactive-hover', $json.dark.'--fujin-interactive-active') }
    $accentPattern = '\[data-fujin-accent="([a-z]+)"\],\s*\.fujin-accent-\1\s*\{([^}]+)\}'
    foreach ($match in [regex]::Matches($css, $accentPattern)) {
        $name = $match.Groups[1].Value
        if ($accents.Contains($name)) { throw "FUJIN_SCHEMA: duplicate accent $name." }
        $colors = @()
        foreach ($role in @('default', 'hover', 'active')) {
            $color = [regex]::Match($match.Groups[2].Value, '--fujin-interactive-' + $role + ':\s*(#[0-9a-fA-F]{6});')
            if (-not $color.Success) { throw "FUJIN_SCHEMA: malformed accent $name/$role." }
            $colors += $color.Groups[1].Value
        }
        $accents[$name] = $colors
    }
    if ($accents.Count -ne 7) { throw 'FUJIN_SCHEMA: pinned release must supply seven generated CSS accents.' }
    return @{ Json = $json; Roles = $roles; Scalars = $values; FontFamily = $family; Accents = $accents }
}
