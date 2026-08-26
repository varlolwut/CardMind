param(
    [Parameter(Mandatory = $true)]
    [string]$Port,

    [Parameter(Mandatory = $true)]
    [ValidateRange(9600, 921600)]
    [int]$BaudRate,

    [Parameter(Mandatory = $true)]
    [ValidateSet("projects", "retry", "compaction", "summary-regeneration", "context-history", "context-history-orphan-recover", "archive-quota", "archive-quota-recover", "binary-text", "binary-text-recover", "history-heap", "limits", "chat-scale", "workspace-scale", "file-scale", "unicode-path", "shared-isolation", "large-stream", "atomic-failure", "version-history", "sd-degraded", "instructions", "request-settings", "diagnostics", "ssh", "full")]
    [string]$Suite,

    [Parameter(Mandatory = $true)]
    [string]$LogPath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Wait-SerialLine {
    param(
        [Parameter(Mandatory = $true)]
        [System.IO.Ports.SerialPort]$Serial,

        [Parameter(Mandatory = $true)]
        [string]$Pattern,

        [Parameter(Mandatory = $true)]
        [ValidateRange(1, 600)]
        [int]$TimeoutSeconds,

        [Parameter(Mandatory = $true)]
        [string]$ResolvedLogPath
    )

    $pending = ""
    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    while ([DateTime]::UtcNow -lt $deadline) {
        Start-Sleep -Milliseconds 40
        $combined = $pending + $Serial.ReadExisting()
        $parts = $combined -split "`n"
        $pending = $parts[-1]
        if ($parts.Count -eq 1) {
            continue
        }
        for ($index = 0; $index -lt $parts.Count - 1; $index++) {
            $line = $parts[$index].TrimEnd("`r").Trim()
            if ($line.Length -eq 0) {
                continue
            }
            $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
            Add-Content -LiteralPath $ResolvedLogPath -Value $line
            if ($line -match $Pattern) {
                return $line
            }
        }
    }
    throw "Timed out after ${TimeoutSeconds}s waiting for serial pattern '$Pattern'"
}

function Sync-SerialChannel {
    param(
        [Parameter(Mandatory = $true)]
        [System.IO.Ports.SerialPort]$Serial,

        [Parameter(Mandatory = $true)]
        [string]$ResolvedLogPath
    )

    for ($attempt = 1; $attempt -le 3; $attempt++) {
        $Serial.ReadExisting() | Out-Null
        $Serial.WriteLine("EXIT")
        $Serial.BaseStream.Flush()
        try {
            Wait-SerialLine -Serial $Serial `
                -Pattern "^(?:WEB_CONSOLE result=stopped|ERROR event=serial_command reason=unsupported_command)$" `
                -TimeoutSeconds 20 -ResolvedLogPath $ResolvedLogPath | Out-Null
            $Serial.WriteLine("PING")
            $Serial.BaseStream.Flush()
            Wait-SerialLine -Serial $Serial -Pattern "^PONG$" -TimeoutSeconds 3 `
                -ResolvedLogPath $ResolvedLogPath | Out-Null
            return
        }
        catch {
            if ($attempt -eq 3) {
                throw "Serial channel did not reach normal mode and answer PING before Web E2E"
            }
        }
    }
}

function Get-P2UnicodePath {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Nonce
    )

    $suffixBase64 = "0J/RgNC+0LXQutGC0Yst5LiW55WML9Cz0LvRg9Cx0L7QutCw0Y8t0L/QsNC/0LrQsC3Zhdix2K3YqNinL9C30LDQvNC10YLQutC4LeODh+ODvOOCvy3wn4yNLnR4dA=="
    $suffix = [System.Text.Encoding]::UTF8.GetString(
        [System.Convert]::FromBase64String($suffixBase64))
    return "cardmind_p2_19_$Nonce/$suffix"
}

function Read-P2ContextHistoryLedger {
    param(
        [Parameter(Mandatory = $true)]
        [string]$LedgerPath
    )

    try {
        $ledger = Get-Content -LiteralPath $LedgerPath -Raw -Encoding UTF8 |
            ConvertFrom-Json
    }
    catch {
        throw "P2-27 ledger '$LedgerPath' is not valid UTF-8 JSON: $($_.Exception.Message)"
    }
    $nonce = [string]$ledger.nonce
    if ($ledger.version -ne 1 -or $nonce -notmatch '^[0-9]{8,20}$' -or
        [string]$ledger.title -ne "P2 context history $nonce") {
        throw "P2-27 recovery ledger has an invalid or unsafe version/nonce/title"
    }
    if ($ledger.baseline_project_ids -isnot [System.Array] -or
        $ledger.original_project_id -isnot [string] -or
        $ledger.owned_project_id -isnot [string]) {
        throw "P2-27 recovery ledger has invalid typed project ownership fields"
    }
    foreach ($field in @(
        "baseline_ready", "project_create_pending", "web_cleanup_complete")) {
        if ($ledger.$field -isnot [bool]) {
            throw "P2-27 recovery ledger field '$field' is not Boolean"
        }
    }
    $baselineIds = @($ledger.baseline_project_ids)
    $identifiers = @(
        $baselineIds +
        @([string]$ledger.original_project_id, [string]$ledger.owned_project_id)) |
        Where-Object { -not [string]::IsNullOrEmpty([string]$_) }
    foreach ($identifier in $identifiers) {
        if ([string]$identifier -notmatch '^[A-Za-z0-9._-]{1,180}$') {
            throw "P2-27 recovery ledger contains an unsafe project identifier"
        }
    }
    if (@($baselineIds | Sort-Object -Unique).Count -ne $baselineIds.Count) {
        throw "P2-27 recovery ledger contains duplicate baseline project identifiers"
    }
    if (-not $ledger.baseline_ready -and
        ($baselineIds.Count -ne 0 -or
         -not [string]::IsNullOrEmpty([string]$ledger.original_project_id) -or
         $ledger.project_create_pending -or
         -not [string]::IsNullOrEmpty([string]$ledger.owned_project_id))) {
        throw "P2-27 recovery ledger has owned state without a persisted baseline"
    }
    if ($ledger.baseline_ready -and
        $baselineIds -notcontains [string]$ledger.original_project_id) {
        throw "P2-27 recovery ledger original project is absent from its baseline"
    }
    if (-not [string]::IsNullOrEmpty([string]$ledger.owned_project_id) -and
        -not $ledger.project_create_pending) {
        throw "P2-27 recovery ledger has an id without a pending create marker"
    }
    if (-not [string]::IsNullOrEmpty([string]$ledger.owned_project_id) -and
        $baselineIds -contains [string]$ledger.owned_project_id) {
        throw "P2-27 recovery ledger-owned project collides with its baseline"
    }
    if ($ledger.web_cleanup_complete -and
        ($ledger.project_create_pending -or
         -not [string]::IsNullOrEmpty([string]$ledger.owned_project_id))) {
        throw "P2-27 recovery ledger cleanup state is inconsistent"
    }
    return $ledger
}

function Read-P2UnicodeLedger {
    param(
        [Parameter(Mandatory = $true)]
        [string]$LedgerPath
    )

    $ledger = Get-Content -LiteralPath $LedgerPath -Raw -Encoding UTF8 | ConvertFrom-Json
    $nonce = [string]$ledger.nonce
    if ($ledger.version -ne 2 -or $nonce -notmatch '^[0-9]{8,20}$') {
        throw "P2-19 recovery ledger has an invalid or unsafe version/nonce"
    }
    $expectedPath = Get-P2UnicodePath -Nonce $nonce
    $expectedTitle = "P2 Unicode path $nonce"
    if ([string]$ledger.path -ne $expectedPath -or
        [string]$ledger.web_title -ne $expectedTitle) {
        throw "P2-19 recovery ledger does not contain the exact owned path/title"
    }
    foreach ($field in @(
        "baseline_ready", "bundle_baseline_checked", "bundle_preexisting",
        "bundle_export_pending", "import_pending", "web_cleanup_complete")) {
        if ($ledger.$field -isnot [bool]) {
            throw "P2-19 recovery ledger field '$field' is not Boolean"
        }
    }
    $baselineIds = @($ledger.baseline_project_ids)
    $allIds = @($baselineIds) + @(
        [string]$ledger.original_project_id,
        [string]$ledger.source_project_id,
        [string]$ledger.imported_project_id)
    foreach ($projectId in $allIds) {
        if (-not [string]::IsNullOrEmpty($projectId) -and
            ($projectId.Length -gt 180 -or $projectId -notmatch '^[A-Za-z0-9._-]+$')) {
            throw "P2-19 recovery ledger contains an unsafe project identifier"
        }
    }
    if (@($baselineIds | Select-Object -Unique).Count -ne $baselineIds.Count) {
        throw "P2-19 recovery ledger contains duplicate baseline identifiers"
    }
    $sourceProjectId = [string]$ledger.source_project_id
    $importedProjectId = [string]$ledger.imported_project_id
    $bundleName = [string]$ledger.bundle_name
    if (-not $ledger.baseline_ready -and
        ($baselineIds.Count -gt 0 -or
         -not [string]::IsNullOrEmpty([string]$ledger.original_project_id) -or
         -not [string]::IsNullOrEmpty($sourceProjectId) -or
         -not [string]::IsNullOrEmpty($importedProjectId) -or
         -not [string]::IsNullOrEmpty($bundleName) -or
         $ledger.bundle_baseline_checked -or $ledger.bundle_preexisting -or
         $ledger.bundle_export_pending -or $ledger.import_pending)) {
        throw "P2-19 recovery ledger has owned state without a persisted baseline"
    }
    if (-not [string]::IsNullOrEmpty($importedProjectId) -and
        [string]::IsNullOrEmpty($sourceProjectId)) {
        throw "P2-19 recovery ledger has an imported project without a source project"
    }
    $expectedBundleName = if ([string]::IsNullOrEmpty($sourceProjectId)) {
        ""
    }
    else {
        "project_$sourceProjectId.cardmind-project.jsonl"
    }
    if ($bundleName -ne $expectedBundleName) {
        throw "P2-19 recovery ledger bundle does not match its source project"
    }
    if (($ledger.bundle_baseline_checked -or $ledger.bundle_export_pending) -and
        [string]::IsNullOrEmpty($bundleName)) {
        throw "P2-19 recovery ledger has bundle state without an exact bundle"
    }
    if ($ledger.import_pending -and -not $ledger.bundle_export_pending) {
        throw "P2-19 recovery ledger has import state without an owned export"
    }
    return $ledger
}

function Read-P2SharedIsolationLedger {
    param(
        [Parameter(Mandatory = $true)]
        [string]$LedgerPath
    )

    $ledger = Get-Content -LiteralPath $LedgerPath -Raw -Encoding UTF8 | ConvertFrom-Json
    $nonce = [string]$ledger.nonce
    if ($ledger.version -ne 1 -or $nonce -notmatch '^[0-9]{8,20}$') {
        throw "P2-20 recovery ledger has an invalid or unsafe version/nonce"
    }
    if ([string]$ledger.path -ne "cardmind_p2_20_${nonce}_shared.txt" -or
        [string]$ledger.title_a -ne "P2 Shared isolation $nonce A" -or
        [string]$ledger.title_b -ne "P2 Shared isolation $nonce B" -or
        [string]$ledger.title_c -ne "P2 Shared isolation $nonce C") {
        throw "P2-20 recovery ledger does not contain the exact owned path/titles"
    }
    foreach ($field in @(
        "baseline_ready", "project_a_pending", "project_b_pending",
        "project_c_pending", "file_baseline_checked", "file_preexisting",
        "file_upload_pending", "web_cleanup_complete")) {
        if ($ledger.$field -isnot [bool]) {
            throw "P2-20 recovery ledger field '$field' is not Boolean"
        }
    }
    $baselineIds = @($ledger.baseline_project_ids)
    $allIds = @($baselineIds) + @(
        [string]$ledger.original_project_id,
        [string]$ledger.project_a_id,
        [string]$ledger.project_b_id,
        [string]$ledger.project_c_id)
    foreach ($projectId in $allIds) {
        if (-not [string]::IsNullOrEmpty($projectId) -and
            ($projectId.Length -gt 180 -or $projectId -notmatch '^[A-Za-z0-9._-]+$')) {
            throw "P2-20 recovery ledger contains an unsafe project identifier"
        }
    }
    if (@($baselineIds | Select-Object -Unique).Count -ne $baselineIds.Count) {
        throw "P2-20 recovery ledger contains duplicate baseline identifiers"
    }
    if (-not $ledger.baseline_ready -and
        ($baselineIds.Count -gt 0 -or
         -not [string]::IsNullOrEmpty([string]$ledger.original_project_id) -or
         -not [string]::IsNullOrEmpty([string]$ledger.project_a_id) -or
         -not [string]::IsNullOrEmpty([string]$ledger.project_b_id) -or
         -not [string]::IsNullOrEmpty([string]$ledger.project_c_id) -or
         $ledger.project_a_pending -or $ledger.project_b_pending -or
         $ledger.project_c_pending -or $ledger.file_baseline_checked -or
         $ledger.file_preexisting -or $ledger.file_upload_pending -or
         $ledger.web_cleanup_complete)) {
        throw "P2-20 recovery ledger has owned state without a persisted baseline"
    }
    if ($ledger.file_upload_pending -and
        (-not $ledger.file_baseline_checked -or $ledger.file_preexisting)) {
        throw "P2-20 recovery ledger has unsafe file ownership state"
    }
    return $ledger
}

function Assert-P2LargeStreamLedger {
    param(
        [Parameter(Mandatory = $true)]
        [psobject]$Ledger
    )

    $nonce = [string]$Ledger.nonce
    $expectedPath = "cardmind_p2_21_${nonce}/large-320mib.txt"
    if ($Ledger.version -ne 1 -or $nonce -notmatch '^[0-9]{8,20}$' -or
        [string]$Ledger.path -ne $expectedPath -or
        [uint64]$Ledger.expected_bytes -ne [uint64]335544320 -or
        [string]$Ledger.expected_fnv32 -ne "09529dc5" -or
        [string]$Ledger.expected_sha256 -ne
            "287c90b8a40b203b0e154463f88c39bd853b8ca6b82b4539a913f46f9684608b") {
        throw "P2-21 recovery ledger has an invalid or unsafe immutable shape"
    }
    foreach ($field in @(
        "setup_pending", "setup_complete", "web_verification_complete",
        "device_verification_complete")) {
        if ($Ledger.$field -isnot [bool]) {
            throw "P2-21 recovery ledger field '$field' is not Boolean"
        }
    }
    if (-not $Ledger.setup_pending) {
        throw "P2-21 recovery ledger does not authorize an owned setup attempt"
    }
    if ($Ledger.web_verification_complete -and -not $Ledger.setup_complete) {
        throw "P2-21 recovery ledger marks Web verification complete before setup"
    }
    if ($Ledger.device_verification_complete -and
        -not $Ledger.web_verification_complete) {
        throw "P2-21 recovery ledger marks device verification complete before Web verification"
    }
    return $Ledger
}

function Read-P2LargeStreamLedger {
    param(
        [Parameter(Mandatory = $true)]
        [string]$LedgerPath
    )

    try {
        $ledger = Get-Content -LiteralPath $LedgerPath -Raw -Encoding UTF8 |
            ConvertFrom-Json
    }
    catch {
        throw "P2-21 ledger '$LedgerPath' is not valid UTF-8 JSON: $($_.Exception.Message)"
    }
    return Assert-P2LargeStreamLedger -Ledger $ledger
}

function Get-P2LargeStreamLedgerStage {
    param(
        [Parameter(Mandatory = $true)]
        [psobject]$Ledger
    )

    Assert-P2LargeStreamLedger -Ledger $Ledger | Out-Null
    if ($Ledger.device_verification_complete) {
        return 3
    }
    if ($Ledger.web_verification_complete) {
        return 2
    }
    if ($Ledger.setup_complete) {
        return 1
    }
    return 0
}

function Get-P2LargeStreamLedgerArtifactPaths {
    param(
        [Parameter(Mandatory = $true)]
        [string]$LedgerPath
    )

    return @(
        $LedgerPath,
        "$LedgerPath.tmp",
        "$LedgerPath.node.tmp",
        "$LedgerPath.bak")
}

function Resolve-P2LargeStreamLedgerArtifacts {
    param(
        [Parameter(Mandatory = $true)]
        [string]$LedgerPath
    )

    $mainPath = $LedgerPath
    $temporaryPath = "$LedgerPath.tmp"
    $nodeTemporaryPath = "$LedgerPath.node.tmp"
    $backupPath = "$LedgerPath.bak"
    $candidates = [System.Collections.Generic.List[psobject]]::new()
    foreach ($artifactPath in Get-P2LargeStreamLedgerArtifactPaths -LedgerPath $LedgerPath) {
        if (Test-Path -LiteralPath $artifactPath) {
            $ledger = Read-P2LargeStreamLedger -LedgerPath $artifactPath
            $candidates.Add([pscustomobject]@{
                path = $artifactPath
                ledger = $ledger
                stage = Get-P2LargeStreamLedgerStage -Ledger $ledger
            })
        }
    }
    if ($candidates.Count -eq 0) {
        return $false
    }
    if ($candidates.Count -gt 2) {
        throw "P2-21 ledger recovery is ambiguous across more than two atomic artifacts"
    }
    $nonces = @($candidates | ForEach-Object { [string]$_.ledger.nonce } |
        Select-Object -Unique)
    if ($nonces.Count -ne 1) {
        throw "P2-21 ledger recovery artifacts contain different owner nonces"
    }
    $main = @($candidates | Where-Object { $_.path -eq $mainPath })
    $temporary = @($candidates | Where-Object {
        $_.path -eq $temporaryPath -or $_.path -eq $nodeTemporaryPath })
    $backup = @($candidates | Where-Object { $_.path -eq $backupPath })
    if ($main.Count -eq 0) {
        if ($candidates.Count -ne 1) {
            throw "P2-21 ledger recovery without a main file is ambiguous"
        }
        Move-Item -LiteralPath $candidates[0].path -Destination $mainPath
        Read-P2LargeStreamLedger -LedgerPath $mainPath | Out-Null
        return $true
    }
    if ($candidates.Count -eq 1) {
        return $true
    }
    if ($temporary.Count -eq 1) {
        if ($temporary[0].stage -lt $main[0].stage) {
            throw "P2-21 temporary ledger is older than the main ledger"
        }
        [System.IO.File]::Replace($temporary[0].path, $mainPath, $backupPath)
        Read-P2LargeStreamLedger -LedgerPath $mainPath | Out-Null
        Read-P2LargeStreamLedger -LedgerPath $backupPath | Out-Null
        Remove-Item -LiteralPath $backupPath
        return $true
    }
    if ($backup.Count -eq 1) {
        if ($backup[0].stage -gt $main[0].stage) {
            throw "P2-21 backup ledger is newer than the main ledger"
        }
        Remove-Item -LiteralPath $backupPath
        return $true
    }
    throw "P2-21 ledger recovery artifacts have an unsupported atomic state"
}

function Remove-P2LargeStreamLedgerArtifacts {
    param(
        [Parameter(Mandatory = $true)]
        [string]$LedgerPath,

        [Parameter(Mandatory = $true)]
        [string]$ExpectedNonce
    )

    $presentPaths = @(Get-P2LargeStreamLedgerArtifactPaths -LedgerPath $LedgerPath |
        Where-Object { Test-Path -LiteralPath $_ })
    foreach ($artifactPath in $presentPaths) {
        $ledger = Read-P2LargeStreamLedger -LedgerPath $artifactPath
        if ([string]$ledger.nonce -ne $ExpectedNonce) {
            throw "P2-21 ledger cleanup found a different owner nonce in '$artifactPath'"
        }
    }
    foreach ($artifactPath in $presentPaths) {
        Remove-Item -LiteralPath $artifactPath
    }
}

function Write-P2LargeStreamLedger {
    param(
        [Parameter(Mandatory = $true)]
        [string]$LedgerPath,

        [Parameter(Mandatory = $true)]
        [psobject]$Ledger
    )

    Assert-P2LargeStreamLedger -Ledger $Ledger | Out-Null
    Resolve-P2LargeStreamLedgerArtifacts -LedgerPath $LedgerPath | Out-Null
    if (Test-Path -LiteralPath $LedgerPath) {
        $currentLedger = Read-P2LargeStreamLedger -LedgerPath $LedgerPath
        if ([string]$currentLedger.nonce -ne [string]$Ledger.nonce) {
            throw "P2-21 atomic ledger replacement attempted to change the owner nonce"
        }
        if ((Get-P2LargeStreamLedgerStage -Ledger $Ledger) -lt
            (Get-P2LargeStreamLedgerStage -Ledger $currentLedger)) {
            throw "P2-21 atomic ledger replacement attempted to decrease its completed stage"
        }
    }
    $temporaryPath = "$LedgerPath.tmp"
    $backupPath = "$LedgerPath.bak"
    $ledgerJson = $Ledger | ConvertTo-Json -Compress
    $utf8WithoutBom = [System.Text.UTF8Encoding]::new($false)
    $ledgerBytes = $utf8WithoutBom.GetBytes("$ledgerJson`n")
    $stream = [System.IO.FileStream]::new(
        $temporaryPath,
        [System.IO.FileMode]::CreateNew,
        [System.IO.FileAccess]::Write,
        [System.IO.FileShare]::None)
    try {
        $stream.Write($ledgerBytes, 0, $ledgerBytes.Length)
        $stream.Flush($true)
    }
    finally {
        $stream.Dispose()
    }
    if (Test-Path -LiteralPath $LedgerPath) {
        [System.IO.File]::Replace($temporaryPath, $LedgerPath, $backupPath)
        Read-P2LargeStreamLedger -LedgerPath $LedgerPath | Out-Null
        Read-P2LargeStreamLedger -LedgerPath $backupPath | Out-Null
        Remove-Item -LiteralPath $backupPath
    }
    else {
        Move-Item -LiteralPath $temporaryPath -Destination $LedgerPath
        Read-P2LargeStreamLedger -LedgerPath $LedgerPath | Out-Null
    }
}

function Assert-P2AtomicFailureLedger {
    param(
        [Parameter(Mandatory = $true)]
        [psobject]$Ledger
    )

    $nonce = [string]$Ledger.nonce
    if ($Ledger.version -ne 1 -or $nonce -notmatch '^[0-9]{8,20}$' -or
        [string]$Ledger.path -ne "cardmind_p2_22_${nonce}/atomic.txt" -or
        [uint64]$Ledger.expected_bytes -ne [uint64]24 -or
        [string]$Ledger.expected_fnv32 -ne "9e5f863b" -or
        $Ledger.setup_pending -isnot [bool] -or -not $Ledger.setup_pending) {
        throw "P2-22 recovery ledger has an invalid or unsafe typed shape"
    }
    return $Ledger
}

function Read-P2AtomicFailureLedger {
    param(
        [Parameter(Mandatory = $true)]
        [string]$LedgerPath
    )

    try {
        $ledger = Get-Content -LiteralPath $LedgerPath -Raw -Encoding UTF8 |
            ConvertFrom-Json
    }
    catch {
        throw "P2-22 ledger '$LedgerPath' is not valid UTF-8 JSON: $($_.Exception.Message)"
    }
    return Assert-P2AtomicFailureLedger -Ledger $ledger
}

function Get-P2AtomicFailureLedgerArtifactPaths {
    param(
        [Parameter(Mandatory = $true)]
        [string]$LedgerPath
    )

    return @($LedgerPath, "$LedgerPath.tmp")
}

function Resolve-P2AtomicFailureLedgerArtifacts {
    param(
        [Parameter(Mandatory = $true)]
        [string]$LedgerPath
    )

    $mainPath = $LedgerPath
    $temporaryPath = "$LedgerPath.tmp"
    $mainPresent = Test-Path -LiteralPath $mainPath
    $temporaryPresent = Test-Path -LiteralPath $temporaryPath
    if (-not $mainPresent -and -not $temporaryPresent) {
        return $false
    }
    if ($mainPresent) {
        $main = Read-P2AtomicFailureLedger -LedgerPath $mainPath
    }
    if ($temporaryPresent) {
        $temporary = Read-P2AtomicFailureLedger -LedgerPath $temporaryPath
    }
    if ($mainPresent -and $temporaryPresent) {
        if ([string]$main.nonce -ne [string]$temporary.nonce) {
            throw "P2-22 ledger recovery artifacts contain different owner nonces"
        }
        Remove-Item -LiteralPath $temporaryPath
        return $true
    }
    if ($temporaryPresent) {
        Move-Item -LiteralPath $temporaryPath -Destination $mainPath
        Read-P2AtomicFailureLedger -LedgerPath $mainPath | Out-Null
    }
    return $true
}

function Write-P2AtomicFailureLedger {
    param(
        [Parameter(Mandatory = $true)]
        [string]$LedgerPath,

        [Parameter(Mandatory = $true)]
        [psobject]$Ledger
    )

    Assert-P2AtomicFailureLedger -Ledger $Ledger | Out-Null
    Resolve-P2AtomicFailureLedgerArtifacts -LedgerPath $LedgerPath | Out-Null
    if (Test-Path -LiteralPath $LedgerPath) {
        throw "P2-22 ledger already owns an unfinished fixture"
    }
    $temporaryPath = "$LedgerPath.tmp"
    $ledgerJson = $Ledger | ConvertTo-Json -Compress
    $utf8WithoutBom = [System.Text.UTF8Encoding]::new($false)
    [System.IO.File]::WriteAllText(
        $temporaryPath, "$ledgerJson`n", $utf8WithoutBom)
    Move-Item -LiteralPath $temporaryPath -Destination $LedgerPath
    Read-P2AtomicFailureLedger -LedgerPath $LedgerPath | Out-Null
}

function Remove-P2AtomicFailureLedgerArtifacts {
    param(
        [Parameter(Mandatory = $true)]
        [string]$LedgerPath,

        [Parameter(Mandatory = $true)]
        [string]$ExpectedNonce
    )

    $presentPaths = @(Get-P2AtomicFailureLedgerArtifactPaths -LedgerPath $LedgerPath |
        Where-Object { Test-Path -LiteralPath $_ })
    foreach ($artifactPath in $presentPaths) {
        $ledger = Read-P2AtomicFailureLedger -LedgerPath $artifactPath
        if ([string]$ledger.nonce -ne $ExpectedNonce) {
            throw "P2-22 ledger cleanup found a different owner nonce in '$artifactPath'"
        }
    }
    foreach ($artifactPath in $presentPaths) {
        Remove-Item -LiteralPath $artifactPath
    }
}

$resolvedLogPath = [System.IO.Path]::GetFullPath($LogPath)
$logDirectory = [System.IO.Path]::GetDirectoryName($resolvedLogPath)
if (-not [string]::IsNullOrEmpty($logDirectory)) {
    [System.IO.Directory]::CreateDirectory($logDirectory) | Out-Null
}
Set-Content -LiteralPath $resolvedLogPath -Value (
    "CARDMIND_WEB_E2E suite={0} port={1} started={2:o}" -f $Suite, $Port, [DateTime]::UtcNow)

$serial = [System.IO.Ports.SerialPort]::new(
    $Port,
    $BaudRate,
    [System.IO.Ports.Parity]::None,
    8,
    [System.IO.Ports.StopBits]::One)
$serial.NewLine = "`r`n"
$serial.DtrEnable = $false
$serial.RtsEnable = $false
$serial.ReadTimeout = 250
$serial.WriteTimeout = 2000
$workspaceScaleNonce = ""
$workspaceScaleCorpusReady = $false
$workspaceScaleCorpusClean = $false
$workspaceScaleLedgerPath = Join-Path (
    Get-Location).Path "artifacts\p2-17-workspace-scale-ledger.json"
$workspaceScaleLedgerPresent = $false
$unicodePathNonce = ""
$unicodePathBytes = ""
$unicodePathFnv32 = ""
$unicodePathSetupAttempted = $false
$unicodePathCorpusClean = $false
$unicodePathWebClean = $false
$unicodePathLedgerPath = Join-Path (
    Get-Location).Path "artifacts\p2-19-unicode-path-ledger.json"
$unicodePathLedgerPresent = $false
$sharedIsolationNonce = ""
$sharedIsolationWebClean = $false
$sharedIsolationDeviceClean = $false
$sharedIsolationLedgerPath = Join-Path (
    Get-Location).Path "artifacts\p2-20-shared-isolation-ledger.json"
$sharedIsolationLedgerPresent = $false
$largeStreamNonce = ""
$largeStreamSetupAttempted = $false
$largeStreamWebVerified = $false
$largeStreamDeviceVerified = $false
$largeStreamClean = $false
$largeStreamLedgerPath = Join-Path (
    Get-Location).Path "artifacts\p2-21-large-stream-ledger.json"
$largeStreamLedgerPresent = $false
$largeStreamSteadyHeapLossBytes = [int64]4096
$largeStreamMinimumHeapFloorBytes = [uint64]28672
$largeStreamMinimumHeapLossBytes = [int64]8192
$atomicFailureNonce = ""
$versionHistoryNonce = if ($Suite -eq "version-history") {
    [DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds().ToString(
        [Globalization.CultureInfo]::InvariantCulture)
} else {
    ""
}
$historyHeapNonce = if ($Suite -eq "history-heap") {
    [DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds().ToString(
        [Globalization.CultureInfo]::InvariantCulture)
} else {
    ""
}
$atomicFailureSetupAttempted = $false
$atomicFailureWebVerified = $false
$atomicFailureClean = $false
$atomicFailureLedgerPath = Join-Path (
    Get-Location).Path "artifacts\p2-22-atomic-failure-ledger.json"
$atomicFailureLedgerPresent = $false
$contextHistoryNonce = ""
$contextHistoryWebClean = $false
$contextHistoryLedgerPath = Join-Path (
    Get-Location).Path "artifacts\p2-27-context-history-ledger.json"
$contextHistoryLedgerPresent = $false
$archiveQuotaNonce = ""
$archiveQuotaWebClean = $false
$archiveQuotaLedgerPath = Join-Path (
    Get-Location).Path "artifacts\p2-28-archive-quota-ledger.json"
$binaryTextNonce = ""
$binaryTextWebClean = $false
$binaryTextLedgerPath = Join-Path (
    Get-Location).Path "artifacts\p2-29-binary-text-ledger.json"
$webConsoleRequested = $false
$webConsoleStarted = $false
$webConsoleStopped = $false
$sdFaultActive = $false
$sdDegradedNonce = ""
$sdDegradedFixtureSetupAttempted = $false
$sdDegradedFixtureClean = $false
$requestSettingsNonce = if ($Suite -eq "request-settings") {
    [DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds().ToString(
        [System.Globalization.CultureInfo]::InvariantCulture)
}
else {
    ""
}

try {
    $serial.Open()
    Start-Sleep -Seconds 12
    Sync-SerialChannel -Serial $serial -ResolvedLogPath $resolvedLogPath
    if ($Suite -eq "sd-degraded") {
        $sdDegradedNonce = [DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds().ToString(
            [System.Globalization.CultureInfo]::InvariantCulture)
        $sdDegradedFixtureSetupAttempted = $true
        $serial.WriteLine("P2ATOMICSETUP$sdDegradedNonce")
        $serial.BaseStream.Flush()
        $fixtureSetupPattern = '^P2ATOMICSETUP result=(?:pass|failed) nonce={0} ' -f (
            [regex]::Escape($sdDegradedNonce))
        $fixtureSetup = Wait-SerialLine -Serial $serial -Pattern $fixtureSetupPattern `
            -TimeoutSeconds 60 -ResolvedLogPath $resolvedLogPath
        $fixtureSetupPassPattern = '^P2ATOMICSETUP result=pass nonce={0} path=cardmind_p2_22_{0}/atomic\.txt bytes=24 fnv32=9e5f863b preflight=pass range=pass filesystem=pass backup=present error=none$' -f (
            [regex]::Escape($sdDegradedNonce))
        if ($fixtureSetup -notmatch $fixtureSetupPassPattern) {
            throw "P2-23 exact read fixture setup failed: $fixtureSetup"
        }
        Write-Host $fixtureSetup
        $sdContracts = @(
            [pscustomobject]@{ command = "P2SDFAULTMISSING"; state = "missing"; error = "micro_sd_required"; read = "rejected"; write = "rejected" },
            [pscustomobject]@{ command = "P2SDFAULTFULL"; state = "full"; error = "micro_sd_full"; read = "pass"; write = "rejected" },
            [pscustomobject]@{ command = "P2SDFAULTREMOVED"; state = "removed"; error = "micro_sd_removed"; read = "rejected"; write = "rejected" },
            [pscustomobject]@{ command = "P2SDFAULTREPLACED"; state = "replaced"; error = "micro_sd_replaced"; read = "rejected"; write = "rejected" }
        )
        foreach ($contract in $sdContracts) {
            $sdFaultActive = $true
            $serial.WriteLine($contract.command)
            $serial.BaseStream.Flush()
            $faultPattern = '^P2SDFAULT result=pass command={0} state={1} error={2} read={3} write={4}$' -f (
                [regex]::Escape($contract.command)), $contract.state, $contract.error,
                $contract.read, $contract.write
            $fault = Wait-SerialLine -Serial $serial -Pattern $faultPattern `
                -TimeoutSeconds 20 -ResolvedLogPath $resolvedLogPath
            Write-Host $fault

            $webConsoleRequested = $true
            $webConsoleStopped = $false
            $serial.WriteLine("CONSOLE")
            $serial.BaseStream.Flush()
            $ready = Wait-SerialLine -Serial $serial -Pattern "^WEB_CONSOLE result=ready" `
                -TimeoutSeconds 30 -ResolvedLogPath $resolvedLogPath
            $webConsoleStarted = $true
            Write-Host $ready

            $nodeStdoutPath = "$resolvedLogPath.$($contract.state).node.out"
            $nodeStderrPath = "$resolvedLogPath.$($contract.state).node.err"
            $nodeArguments = @(
                "tools/hardware_web_e2e.mjs",
                "--suite", "sd-degraded",
                "--sd-degraded-state", $contract.state,
                "--sd-degraded-nonce", $sdDegradedNonce)
            $nodeProcess = Start-Process -FilePath "node" `
                -ArgumentList $nodeArguments `
                -WorkingDirectory (Get-Location).Path `
                -RedirectStandardOutput $nodeStdoutPath `
                -RedirectStandardError $nodeStderrPath `
                -WindowStyle Hidden `
                -PassThru `
                -Wait
            foreach ($nodeOutputPath in @($nodeStdoutPath, $nodeStderrPath)) {
                if (Test-Path -LiteralPath $nodeOutputPath) {
                    foreach ($line in Get-Content -LiteralPath $nodeOutputPath -Encoding UTF8) {
                        Add-Content -LiteralPath $resolvedLogPath -Value $line
                        Write-Host $line
                    }
                }
            }
            if ($nodeProcess.ExitCode -ne 0) {
                throw "SD-degraded Web E2E failed for $($contract.state) with code $($nodeProcess.ExitCode)"
            }

            $serial.WriteLine("EXIT")
            $serial.BaseStream.Flush()
            $stopped = Wait-SerialLine -Serial $serial `
                -Pattern "^WEB_CONSOLE result=stopped$" -TimeoutSeconds 20 `
                -ResolvedLogPath $resolvedLogPath
            $webConsoleStopped = $true
            Write-Host $stopped

            $serial.WriteLine("P2SDFAULTCLEAR")
            $serial.BaseStream.Flush()
            $clear = Wait-SerialLine -Serial $serial `
                -Pattern '^P2SDFAULT result=pass command=P2SDFAULTCLEAR state=ready error=none read=pass write=pass$' `
                -TimeoutSeconds 20 -ResolvedLogPath $resolvedLogPath
            $sdFaultActive = $false
            Write-Host $clear
        }
        $serial.WriteLine("STATUS")
        $serial.BaseStream.Flush()
        Wait-SerialLine -Serial $serial `
            -Pattern '^STATUS .*microsd_state=ready microsd_error=none' `
            -TimeoutSeconds 20 -ResolvedLogPath $resolvedLogPath | Out-Null
        $serial.WriteLine("P2ATOMICCLEAN$sdDegradedNonce")
        $serial.BaseStream.Flush()
        $fixtureCleanupPattern = '^P2ATOMICCLEAN result=(?:pass|failed) nonce={0} ' -f (
            [regex]::Escape($sdDegradedNonce))
        $fixtureCleanup = Wait-SerialLine -Serial $serial -Pattern $fixtureCleanupPattern `
            -TimeoutSeconds 60 -ResolvedLogPath $resolvedLogPath
        $fixtureCleanupPassPattern = '^P2ATOMICCLEAN result=pass nonce={0} already_absent=no removed_file=yes removed_directory=yes error=none$' -f (
            [regex]::Escape($sdDegradedNonce))
        if ($fixtureCleanup -notmatch $fixtureCleanupPassPattern) {
            throw "P2-23 exact read fixture cleanup failed: $fixtureCleanup"
        }
        Write-Host $fixtureCleanup
        $serial.WriteLine("P2ATOMICCLEAN$sdDegradedNonce")
        $serial.BaseStream.Flush()
        $fixtureIdempotentCleanup = Wait-SerialLine -Serial $serial `
            -Pattern $fixtureCleanupPattern -TimeoutSeconds 60 `
            -ResolvedLogPath $resolvedLogPath
        $fixtureIdempotentPassPattern = '^P2ATOMICCLEAN result=pass nonce={0} already_absent=yes removed_file=no removed_directory=no error=none$' -f (
            [regex]::Escape($sdDegradedNonce))
        if ($fixtureIdempotentCleanup -notmatch $fixtureIdempotentPassPattern) {
            throw "P2-23 idempotent read fixture cleanup failed: $fixtureIdempotentCleanup"
        }
        $sdDegradedFixtureClean = $true
        Write-Host $fixtureIdempotentCleanup
        Add-Content -LiteralPath $resolvedLogPath -Value (
            "CARDMIND_WEB_E2E result=pass completed={0:o}" -f [DateTime]::UtcNow)
        Write-Host "CARDMIND_WEB_E2E result=pass suite=$Suite log=$resolvedLogPath"
        return
    }
    if ($Suite -eq "archive-quota") {
        if (Test-Path -LiteralPath $archiveQuotaLedgerPath) {
            throw "P2-28 recovery ledger exists; run archive-quota-recover before a new fixture"
        }
        $archiveQuotaNonce = [DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds().ToString(
            [System.Globalization.CultureInfo]::InvariantCulture)
    }
    if ($Suite -eq "archive-quota-recover" -and
        -not (Test-Path -LiteralPath $archiveQuotaLedgerPath)) {
        throw "P2-28 recovery ledger is absent; refusing ambiguous cleanup"
    }
    if ($Suite -eq "binary-text") {
        if (Test-Path -LiteralPath $binaryTextLedgerPath) {
            throw "P2-29 recovery ledger exists; run binary-text-recover before a new fixture"
        }
        $binaryTextNonce = [DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds().ToString(
            [System.Globalization.CultureInfo]::InvariantCulture)
    }
    if ($Suite -eq "binary-text-recover" -and
        -not (Test-Path -LiteralPath $binaryTextLedgerPath)) {
        throw "P2-29 recovery ledger is absent; refusing ambiguous cleanup"
    }
    if ($Suite -eq "context-history") {
        if (Test-Path -LiteralPath $contextHistoryLedgerPath) {
            $ledger = Read-P2ContextHistoryLedger -LedgerPath $contextHistoryLedgerPath
            $contextHistoryNonce = [string]$ledger.nonce
            $contextHistoryLedgerPresent = $true
            $webConsoleRequested = $true
            $webConsoleStopped = $false
            $serial.WriteLine("CONSOLE")
            $serial.BaseStream.Flush()
            $ready = Wait-SerialLine -Serial $serial -Pattern "^WEB_CONSOLE result=ready" `
                -TimeoutSeconds 30 -ResolvedLogPath $resolvedLogPath
            $webConsoleStarted = $true
            Write-Host $ready

            $recoveryStdoutPath = "$resolvedLogPath.history.recovery.node.out"
            $recoveryStderrPath = "$resolvedLogPath.history.recovery.node.err"
            $recoveryArguments = @(
                "tools/hardware_web_e2e.mjs",
                "--suite", "context-history-recover",
                "--context-history-ledger", $contextHistoryLedgerPath)
            $recoveryProcess = Start-Process -FilePath "node" `
                -ArgumentList $recoveryArguments `
                -WorkingDirectory (Get-Location).Path `
                -RedirectStandardOutput $recoveryStdoutPath `
                -RedirectStandardError $recoveryStderrPath `
                -WindowStyle Hidden `
                -PassThru `
                -Wait
            foreach ($recoveryOutputPath in @($recoveryStdoutPath, $recoveryStderrPath)) {
                if (Test-Path -LiteralPath $recoveryOutputPath) {
                    foreach ($line in Get-Content -LiteralPath $recoveryOutputPath -Encoding UTF8) {
                        Add-Content -LiteralPath $resolvedLogPath -Value $line
                        Write-Host $line
                    }
                }
            }
            $recoveredLedger = Read-P2ContextHistoryLedger `
                -LedgerPath $contextHistoryLedgerPath
            if ($recoveryProcess.ExitCode -ne 0 -or
                -not $recoveredLedger.web_cleanup_complete) {
                throw "P2-27 Web recovery failed closed; retained exact ledger"
            }

            $serial.WriteLine("EXIT")
            $serial.BaseStream.Flush()
            $stopped = Wait-SerialLine -Serial $serial `
                -Pattern "^WEB_CONSOLE result=stopped$" -TimeoutSeconds 20 `
                -ResolvedLogPath $resolvedLogPath
            $webConsoleStopped = $true
            Write-Host $stopped
            Remove-Item -LiteralPath $contextHistoryLedgerPath
            $contextHistoryLedgerPresent = $false
            $contextHistoryWebClean = $false
            $webConsoleStarted = $false
        }
        $contextHistoryNonce = [DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds().ToString(
            [System.Globalization.CultureInfo]::InvariantCulture)
        $ledgerTemporaryPath = "$contextHistoryLedgerPath.tmp"
        $ledgerDocument = [pscustomobject]@{
            version = 1
            nonce = $contextHistoryNonce
            title = "P2 context history $contextHistoryNonce"
            baseline_ready = $false
            baseline_project_ids = @()
            original_project_id = ""
            project_create_pending = $false
            owned_project_id = ""
            web_cleanup_complete = $false
        }
        $ledgerJson = $ledgerDocument | ConvertTo-Json -Compress
        $utf8WithoutBom = [System.Text.UTF8Encoding]::new($false)
        [System.IO.File]::WriteAllText(
            $ledgerTemporaryPath, "$ledgerJson`n", $utf8WithoutBom)
        Move-Item -LiteralPath $ledgerTemporaryPath -Destination $contextHistoryLedgerPath
        $contextHistoryLedgerPresent = $true
    }
    if ($Suite -eq "workspace-scale") {
        if (Test-Path -LiteralPath $workspaceScaleLedgerPath) {
            $ledger = Get-Content -LiteralPath $workspaceScaleLedgerPath -Raw | ConvertFrom-Json
            $ledgerNames = @($ledger.names)
            if ($ledger.version -ne 2 -or $ledger.state -ne "owned" -or
                $ledgerNames.Count -ne 500 -or
                $ledgerNames[0] -notmatch '^cardmind_p2_17_([0-9]{8,20})_000\.txt$') {
                throw "P2-17 recovery ledger has an invalid or unsafe shape"
            }
            $recoveryNonce = $Matches[1]
            for ($index = 0; $index -lt 500; $index++) {
                $expectedName = "cardmind_p2_17_{0}_{1:D3}.txt" -f $recoveryNonce, $index
                if ([string]$ledgerNames[$index] -ne $expectedName) {
                    throw "P2-17 recovery ledger is not one exact contiguous run"
                }
            }
            $serial.WriteLine("P2FILESCALECLEAN$recoveryNonce")
            $serial.BaseStream.Flush()
            $recoveryPattern = '^P2FILESCALECLEAN result=(?:pass|failed) nonce={0} ' -f (
                [regex]::Escape($recoveryNonce))
            $recovery = Wait-SerialLine -Serial $serial -Pattern $recoveryPattern `
                -TimeoutSeconds 300 -ResolvedLogPath $resolvedLogPath
            if ($recovery -notmatch '^P2FILESCALECLEAN result=pass .*remaining=0 errors=0 ') {
                throw "P2-17 recovery cleanup failed: $recovery"
            }
            Remove-Item -LiteralPath $workspaceScaleLedgerPath
        }
        $workspaceScaleNonce = [DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds().ToString(
            [System.Globalization.CultureInfo]::InvariantCulture)
        $workspaceScaleNames = [System.Collections.Generic.List[string]]::new()
        for ($index = 0; $index -lt 500; $index++) {
            $workspaceScaleNames.Add(
                ("cardmind_p2_17_{0}_{1:D3}.txt" -f $workspaceScaleNonce, $index))
        }
        $serial.WriteLine("P2FILESCALESETUP$workspaceScaleNonce")
        $serial.BaseStream.Flush()
        $setupPattern = '^P2FILESCALESETUP result=(?:pass|failed) nonce={0} ' -f (
            [regex]::Escape($workspaceScaleNonce))
        $setup = Wait-SerialLine -Serial $serial -Pattern $setupPattern `
            -TimeoutSeconds 300 -ResolvedLogPath $resolvedLogPath
        if ($setup -match '^P2FILESCALESETUP result=failed .*stage=collision files=0 ') {
            throw "P2-17 corpus setup collision: $setup"
        }
        if ($setup -match '^P2FILESCALESETUP result=(?:pass|failed) .*stage=(?:complete|write) files=[0-9]+ ') {
            $ledgerTemporaryPath = "$workspaceScaleLedgerPath.tmp"
            [pscustomobject]@{
                version = 2
                state = "owned"
                names = $workspaceScaleNames
            } | ConvertTo-Json | Set-Content -LiteralPath $ledgerTemporaryPath -Encoding utf8
            Move-Item -LiteralPath $ledgerTemporaryPath -Destination $workspaceScaleLedgerPath
            $workspaceScaleLedgerPresent = $true
            $workspaceScaleCorpusReady = $true
        }
        if ($setup -notmatch '^P2FILESCALESETUP result=pass .*stage=complete files=500 ') {
            throw "P2-17 corpus setup failed: $setup"
        }
        Write-Host $setup
        $serial.WriteLine("P2FILETOOLPAGETEST$workspaceScaleNonce")
        $serial.BaseStream.Flush()
        $toolPagePattern = '^P2FILETOOLPAGETEST result=(?:pass|failed) nonce={0} ' -f (
            [regex]::Escape($workspaceScaleNonce))
        $toolPage = Wait-SerialLine -Serial $serial -Pattern $toolPagePattern `
            -TimeoutSeconds 420 -ResolvedLogPath $resolvedLogPath
        if ($toolPage -notmatch '^P2FILETOOLPAGETEST result=pass .*files=500 compatibility=pass malformed=pass heap=pass ') {
            throw "P2-36 list_files pagination failed: $toolPage"
        }
        Write-Host $toolPage
    }
    if ($Suite -eq "unicode-path") {
        if (Test-Path -LiteralPath $unicodePathLedgerPath) {
            $ledger = Read-P2UnicodeLedger -LedgerPath $unicodePathLedgerPath
            $recoveryNonce = [string]$ledger.nonce
            $unicodePathNonce = $recoveryNonce
            $unicodePathLedgerPresent = $true
            $unicodePathSetupAttempted = $true
            $webConsoleRequested = $true
            $webConsoleStopped = $false
            $serial.WriteLine("CONSOLE")
            $serial.BaseStream.Flush()
            $ready = Wait-SerialLine -Serial $serial -Pattern "^WEB_CONSOLE result=ready" `
                -TimeoutSeconds 30 -ResolvedLogPath $resolvedLogPath
            $webConsoleStarted = $true
            Write-Host $ready

            $recoveryStdoutPath = "$resolvedLogPath.recovery.node.out"
            $recoveryStderrPath = "$resolvedLogPath.recovery.node.err"
            $recoveryArguments = @(
                "tools/hardware_web_e2e.mjs",
                "--suite", "unicode-path-recover",
                "--unicode-path-ledger", $unicodePathLedgerPath)
            $recoveryProcess = Start-Process -FilePath "node" `
                -ArgumentList $recoveryArguments `
                -WorkingDirectory (Get-Location).Path `
                -RedirectStandardOutput $recoveryStdoutPath `
                -RedirectStandardError $recoveryStderrPath `
                -WindowStyle Hidden `
                -PassThru `
                -Wait
            foreach ($recoveryOutputPath in @($recoveryStdoutPath, $recoveryStderrPath)) {
                if (Test-Path -LiteralPath $recoveryOutputPath) {
                    foreach ($line in Get-Content -LiteralPath $recoveryOutputPath -Encoding UTF8) {
                        Add-Content -LiteralPath $resolvedLogPath -Value $line
                        Write-Host $line
                    }
                }
            }
            $recoveredLedger = Read-P2UnicodeLedger -LedgerPath $unicodePathLedgerPath
            if ($recoveredLedger.web_cleanup_complete) {
                $unicodePathWebClean = $true
            }
            if ($recoveryProcess.ExitCode -ne 0 -or -not $unicodePathWebClean) {
                throw "P2-19 Web recovery failed closed; retained exact ledger"
            }

            $serial.WriteLine("EXIT")
            $serial.BaseStream.Flush()
            $stopped = Wait-SerialLine -Serial $serial `
                -Pattern "^WEB_CONSOLE result=stopped$" -TimeoutSeconds 20 `
                -ResolvedLogPath $resolvedLogPath
            $webConsoleStopped = $true
            Write-Host $stopped
            $serial.WriteLine("P2UNICODECLEAN$recoveryNonce")
            $serial.BaseStream.Flush()
            $recoveryPattern = '^P2UNICODECLEAN result=(?:pass|failed) nonce={0} ' -f (
                [regex]::Escape($recoveryNonce))
            $recovery = Wait-SerialLine -Serial $serial -Pattern $recoveryPattern `
                -TimeoutSeconds 180 -ResolvedLogPath $resolvedLogPath
            if ($recovery -notmatch '^P2UNICODECLEAN result=pass .*remaining=0 errors=0 ') {
                throw "P2-19 recovery cleanup failed: $recovery"
            }
            $unicodePathCorpusClean = $true
            Remove-Item -LiteralPath $unicodePathLedgerPath
            $unicodePathLedgerPresent = $false
            $unicodePathSetupAttempted = $false
            $unicodePathCorpusClean = $false
            $unicodePathWebClean = $false
            $webConsoleStarted = $false
        }
        $unicodePathNonce = [DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds().ToString(
            [System.Globalization.CultureInfo]::InvariantCulture)
        $unicodePath = Get-P2UnicodePath -Nonce $unicodePathNonce
        $unicodePathBytesExpected = [System.Text.Encoding]::UTF8.GetByteCount($unicodePath)
        $ledgerTemporaryPath = "$unicodePathLedgerPath.tmp"
        $unicodeLedgerDocument = [pscustomobject]@{
            version = 2
            nonce = $unicodePathNonce
            path = $unicodePath
            web_title = "P2 Unicode path $unicodePathNonce"
            baseline_ready = $false
            baseline_project_ids = @()
            original_project_id = ""
            source_project_id = ""
            imported_project_id = ""
            bundle_name = ""
            bundle_baseline_checked = $false
            bundle_preexisting = $false
            bundle_export_pending = $false
            import_pending = $false
            web_cleanup_complete = $false
        }
        $unicodeLedgerJson = $unicodeLedgerDocument | ConvertTo-Json -Compress
        $utf8WithoutBom = [System.Text.UTF8Encoding]::new($false)
        [System.IO.File]::WriteAllText(
            $ledgerTemporaryPath, "$unicodeLedgerJson`n", $utf8WithoutBom)
        Move-Item -LiteralPath $ledgerTemporaryPath -Destination $unicodePathLedgerPath
        $unicodePathLedgerPresent = $true
        $unicodePathSetupAttempted = $true
        $serial.WriteLine("P2UNICODESETUP$unicodePathNonce")
        $serial.BaseStream.Flush()
        $setupPattern = '^P2UNICODESETUP result=(?:pass|failed) nonce={0} ' -f (
            [regex]::Escape($unicodePathNonce))
        $setup = Wait-SerialLine -Serial $serial -Pattern $setupPattern `
            -TimeoutSeconds 300 -ResolvedLogPath $resolvedLogPath
        $setupPassPattern = '^P2UNICODESETUP result=pass nonce={0} path_bytes=(?<path_bytes>[0-9]+) depth=4 bytes=(?<bytes>[0-9]+) fnv32=(?<fnv32>[0-9a-fA-F]{{8}}) device_list=pass device_view=pass error=none$' -f (
            [regex]::Escape($unicodePathNonce))
        if ($setup -notmatch $setupPassPattern) {
            throw "P2-19 Unicode setup failed or returned incomplete evidence: $setup"
        }
        if ([uint64]$Matches.path_bytes -ne [uint64]$unicodePathBytesExpected -or
            [uint64]$Matches.bytes -eq 0) {
            throw "P2-19 Unicode setup returned inconsistent path or file byte counts"
        }
        $unicodePathBytes = [string]$Matches.bytes
        $unicodePathFnv32 = ([string]$Matches.fnv32).ToLowerInvariant()
        Write-Host $setup
    }
    if ($Suite -eq "shared-isolation") {
        if (Test-Path -LiteralPath $sharedIsolationLedgerPath) {
            $ledger = Read-P2SharedIsolationLedger -LedgerPath $sharedIsolationLedgerPath
            $recoveryNonce = [string]$ledger.nonce
            $sharedIsolationNonce = $recoveryNonce
            $sharedIsolationLedgerPresent = $true
            $serial.WriteLine("P2SHAREDCLEAN$recoveryNonce")
            $serial.BaseStream.Flush()
            $recoveryPattern = '^P2SHAREDCLEAN result=(?:pass|failed) nonce={0} ' -f (
                [regex]::Escape($recoveryNonce))
            $recovery = Wait-SerialLine -Serial $serial -Pattern $recoveryPattern `
                -TimeoutSeconds 180 -ResolvedLogPath $resolvedLogPath
            $recoveryPassPattern = '^P2SHAREDCLEAN result=pass nonce={0} already_absent=(?:yes|no) removed_projects=[0-9]+ removed_files=[0-9]+ remaining=0 errors=0 error=none$' -f (
                [regex]::Escape($recoveryNonce))
            if ($recovery -notmatch $recoveryPassPattern) {
                throw "P2-20 device recovery cleanup failed: $recovery"
            }
            $sharedIsolationDeviceClean = $true
            Write-Host $recovery
            $webConsoleRequested = $true
            $webConsoleStopped = $false
            $serial.WriteLine("CONSOLE")
            $serial.BaseStream.Flush()
            $ready = Wait-SerialLine -Serial $serial -Pattern "^WEB_CONSOLE result=ready" `
                -TimeoutSeconds 30 -ResolvedLogPath $resolvedLogPath
            $webConsoleStarted = $true
            Write-Host $ready

            $recoveryStdoutPath = "$resolvedLogPath.shared.recovery.node.out"
            $recoveryStderrPath = "$resolvedLogPath.shared.recovery.node.err"
            $recoveryArguments = @(
                "tools/hardware_web_e2e.mjs",
                "--suite", "shared-isolation-recover",
                "--shared-isolation-ledger", $sharedIsolationLedgerPath)
            $recoveryProcess = Start-Process -FilePath "node" `
                -ArgumentList $recoveryArguments `
                -WorkingDirectory (Get-Location).Path `
                -RedirectStandardOutput $recoveryStdoutPath `
                -RedirectStandardError $recoveryStderrPath `
                -WindowStyle Hidden `
                -PassThru `
                -Wait
            foreach ($recoveryOutputPath in @($recoveryStdoutPath, $recoveryStderrPath)) {
                if (Test-Path -LiteralPath $recoveryOutputPath) {
                    foreach ($line in Get-Content -LiteralPath $recoveryOutputPath -Encoding UTF8) {
                        Add-Content -LiteralPath $resolvedLogPath -Value $line
                        Write-Host $line
                    }
                }
            }
            $recoveredLedger = Read-P2SharedIsolationLedger `
                -LedgerPath $sharedIsolationLedgerPath
            if ($recoveredLedger.web_cleanup_complete) {
                $sharedIsolationWebClean = $true
            }
            if ($recoveryProcess.ExitCode -ne 0 -or -not $sharedIsolationWebClean) {
                throw "P2-20 Web recovery failed closed; retained exact ledger"
            }

            $serial.WriteLine("EXIT")
            $serial.BaseStream.Flush()
            $stopped = Wait-SerialLine -Serial $serial `
                -Pattern "^WEB_CONSOLE result=stopped$" -TimeoutSeconds 20 `
                -ResolvedLogPath $resolvedLogPath
            $webConsoleStopped = $true
            Write-Host $stopped
            Remove-Item -LiteralPath $sharedIsolationLedgerPath
            $sharedIsolationLedgerPresent = $false
            $sharedIsolationWebClean = $false
            $sharedIsolationDeviceClean = $false
            $webConsoleStarted = $false
        }
        $sharedIsolationNonce = [DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds().ToString(
            [System.Globalization.CultureInfo]::InvariantCulture)
        $ledgerTemporaryPath = "$sharedIsolationLedgerPath.tmp"
        $ledgerDocument = [pscustomobject]@{
            version = 1
            nonce = $sharedIsolationNonce
            path = "cardmind_p2_20_${sharedIsolationNonce}_shared.txt"
            title_a = "P2 Shared isolation $sharedIsolationNonce A"
            title_b = "P2 Shared isolation $sharedIsolationNonce B"
            title_c = "P2 Shared isolation $sharedIsolationNonce C"
            baseline_ready = $false
            baseline_project_ids = @()
            original_project_id = ""
            project_a_id = ""
            project_b_id = ""
            project_c_id = ""
            project_a_pending = $false
            project_b_pending = $false
            project_c_pending = $false
            file_baseline_checked = $false
            file_preexisting = $false
            file_upload_pending = $false
            web_cleanup_complete = $false
        }
        $ledgerJson = $ledgerDocument | ConvertTo-Json -Compress
        $utf8WithoutBom = [System.Text.UTF8Encoding]::new($false)
        [System.IO.File]::WriteAllText($ledgerTemporaryPath, "$ledgerJson`n", $utf8WithoutBom)
        Move-Item -LiteralPath $ledgerTemporaryPath -Destination $sharedIsolationLedgerPath
        $sharedIsolationLedgerPresent = $true
    }
    if ($Suite -eq "large-stream") {
        Resolve-P2LargeStreamLedgerArtifacts `
            -LedgerPath $largeStreamLedgerPath | Out-Null
        if (Test-Path -LiteralPath $largeStreamLedgerPath) {
            $ledger = Read-P2LargeStreamLedger -LedgerPath $largeStreamLedgerPath
            $recoveryNonce = [string]$ledger.nonce
            $largeStreamNonce = $recoveryNonce
            $largeStreamLedgerPresent = $true
            $largeStreamSetupAttempted = $true
            $serial.WriteLine("P2LARGECLEAN$recoveryNonce")
            $serial.BaseStream.Flush()
            $recoveryPattern = '^P2LARGECLEAN result=(?:pass|failed) nonce={0} ' -f (
                [regex]::Escape($recoveryNonce))
            $recovery = Wait-SerialLine -Serial $serial -Pattern $recoveryPattern `
                -TimeoutSeconds 120 -ResolvedLogPath $resolvedLogPath
            $recoveryPassPattern = '^P2LARGECLEAN result=pass nonce={0} already_absent=(?:yes|no) removed_file=(?:yes|no) removed_directory=(?:yes|no) error=none$' -f (
                [regex]::Escape($recoveryNonce))
            if ($recovery -notmatch $recoveryPassPattern) {
                throw "P2-21 interrupted fixture cleanup failed: $recovery"
            }
            $largeStreamClean = $true
            Write-Host $recovery
            Remove-P2LargeStreamLedgerArtifacts `
                -LedgerPath $largeStreamLedgerPath -ExpectedNonce $recoveryNonce
            $largeStreamLedgerPresent = $false
            $largeStreamSetupAttempted = $false
            $largeStreamClean = $false
        }
        $largeStreamNonce = [DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds().ToString(
            [System.Globalization.CultureInfo]::InvariantCulture)
        $largeStreamLedger = [pscustomobject]@{
            version = 1
            nonce = $largeStreamNonce
            path = "cardmind_p2_21_${largeStreamNonce}/large-320mib.txt"
            expected_bytes = [uint64]335544320
            expected_fnv32 = "09529dc5"
            expected_sha256 = "287c90b8a40b203b0e154463f88c39bd853b8ca6b82b4539a913f46f9684608b"
            setup_pending = $true
            setup_complete = $false
            web_verification_complete = $false
            device_verification_complete = $false
        }
        Write-P2LargeStreamLedger `
            -LedgerPath $largeStreamLedgerPath -Ledger $largeStreamLedger
        $largeStreamLedgerPresent = $true
        $largeStreamSetupAttempted = $true
        $serial.WriteLine("P2LARGESETUP$largeStreamNonce")
        $serial.BaseStream.Flush()
        $setupPattern = '^P2LARGESETUP result=(?:pass|failed) nonce={0} ' -f (
            [regex]::Escape($largeStreamNonce))
        $setup = Wait-SerialLine -Serial $serial -Pattern $setupPattern `
            -TimeoutSeconds 600 -ResolvedLogPath $resolvedLogPath
        $setupPassPattern = '^P2LARGESETUP result=pass nonce={0} path=cardmind_p2_21_{0}/large-320mib\.txt bytes=335544320 fnv32=09529dc5 free_heap_before=(?<free_before>[0-9]+) free_heap_after=(?<free_after>[0-9]+) largest_heap_before=(?<largest_before>[0-9]+) largest_heap_after=(?<largest_after>[0-9]+) min_heap_before=(?<min_before>[0-9]+) min_heap_after=(?<min_after>[0-9]+) error=none$' -f (
            [regex]::Escape($largeStreamNonce))
        if ($setup -notmatch $setupPassPattern) {
            throw "P2-21 exact 320 MiB setup failed: $setup"
        }
        if ([uint64]$Matches.free_after -lt [uint64]85000 -or
            [uint64]$Matches.largest_after -lt [uint64]28000 -or
            [uint64]$Matches.min_after -lt $largeStreamMinimumHeapFloorBytes -or
            ([int64]$Matches.free_before - [int64]$Matches.free_after) -gt
                $largeStreamSteadyHeapLossBytes -or
            ([int64]$Matches.largest_before - [int64]$Matches.largest_after) -gt
                $largeStreamSteadyHeapLossBytes -or
            ([int64]$Matches.min_before - [int64]$Matches.min_after) -gt
                $largeStreamMinimumHeapLossBytes) {
            throw "P2-21 setup exceeded the bounded device heap budget: $setup"
        }
        $largeStreamLedger.setup_complete = $true
        Write-P2LargeStreamLedger `
            -LedgerPath $largeStreamLedgerPath -Ledger $largeStreamLedger
        Write-Host $setup
    }
    if ($Suite -eq "atomic-failure") {
        Resolve-P2AtomicFailureLedgerArtifacts `
            -LedgerPath $atomicFailureLedgerPath | Out-Null
        if (Test-Path -LiteralPath $atomicFailureLedgerPath) {
            $ledger = Read-P2AtomicFailureLedger -LedgerPath $atomicFailureLedgerPath
            $recoveryNonce = [string]$ledger.nonce
            $atomicFailureNonce = $recoveryNonce
            $atomicFailureLedgerPresent = $true
            $atomicFailureSetupAttempted = $true
            $serial.WriteLine("P2ATOMICCLEAN$recoveryNonce")
            $serial.BaseStream.Flush()
            $recoveryPattern = '^P2ATOMICCLEAN result=(?:pass|failed) nonce={0} ' -f (
                [regex]::Escape($recoveryNonce))
            $recovery = Wait-SerialLine -Serial $serial -Pattern $recoveryPattern `
                -TimeoutSeconds 60 -ResolvedLogPath $resolvedLogPath
            $recoveryPassPattern = '^P2ATOMICCLEAN result=pass nonce={0} already_absent=(?:yes|no) removed_file=(?:yes|no) removed_directory=(?:yes|no) error=none$' -f (
                [regex]::Escape($recoveryNonce))
            if ($recovery -notmatch $recoveryPassPattern) {
                throw "P2-22 interrupted fixture cleanup failed: $recovery"
            }
            $atomicFailureClean = $true
            Write-Host $recovery
            Remove-P2AtomicFailureLedgerArtifacts `
                -LedgerPath $atomicFailureLedgerPath -ExpectedNonce $recoveryNonce
            $atomicFailureLedgerPresent = $false
            $atomicFailureSetupAttempted = $false
            $atomicFailureClean = $false
        }
        $atomicFailureNonce = [DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds().ToString(
            [System.Globalization.CultureInfo]::InvariantCulture)
        $atomicFailureLedger = [pscustomobject]@{
            version = 1
            nonce = $atomicFailureNonce
            path = "cardmind_p2_22_${atomicFailureNonce}/atomic.txt"
            expected_bytes = [uint64]24
            expected_fnv32 = "9e5f863b"
            setup_pending = $true
        }
        Write-P2AtomicFailureLedger `
            -LedgerPath $atomicFailureLedgerPath -Ledger $atomicFailureLedger
        $atomicFailureLedgerPresent = $true
        $atomicFailureSetupAttempted = $true
        $serial.WriteLine("P2ATOMICSETUP$atomicFailureNonce")
        $serial.BaseStream.Flush()
        $setupPattern = '^P2ATOMICSETUP result=(?:pass|failed) nonce={0} ' -f (
            [regex]::Escape($atomicFailureNonce))
        $setup = Wait-SerialLine -Serial $serial -Pattern $setupPattern `
            -TimeoutSeconds 60 -ResolvedLogPath $resolvedLogPath
        $setupPassPattern = '^P2ATOMICSETUP result=pass nonce={0} path=cardmind_p2_22_{0}/atomic\.txt bytes=24 fnv32=9e5f863b preflight=pass range=pass filesystem=pass backup=present error=none$' -f (
            [regex]::Escape($atomicFailureNonce))
        if ($setup -notmatch $setupPassPattern) {
            throw "P2-22 exact atomic-failure setup failed: $setup"
        }
        Write-Host $setup
    }
    Start-Sleep -Milliseconds 1000
    $serial.ReadExisting() | Out-Null
    $webConsoleRequested = $true
    $webConsoleStopped = $false
    $serial.WriteLine("CONSOLE")
    $serial.BaseStream.Flush()
    $ready = Wait-SerialLine -Serial $serial -Pattern "^WEB_CONSOLE result=ready" `
        -TimeoutSeconds 30 -ResolvedLogPath $resolvedLogPath
    $webConsoleStarted = $true
    Write-Host $ready

    $nodeStdoutPath = "$resolvedLogPath.node.out"
    $nodeStderrPath = "$resolvedLogPath.node.err"
    $nodeArguments = @("tools/hardware_web_e2e.mjs", "--suite", $Suite)
    if ($Suite -eq "workspace-scale") {
        $nodeArguments += @("--workspace-scale-nonce", $workspaceScaleNonce)
    }
    if ($Suite -eq "unicode-path") {
        $nodeArguments += @(
            "--unicode-path-nonce", $unicodePathNonce,
            "--unicode-path-bytes", $unicodePathBytes,
            "--unicode-path-fnv32", $unicodePathFnv32,
            "--unicode-path-ledger", $unicodePathLedgerPath)
    }
    if ($Suite -eq "shared-isolation") {
        $nodeArguments += @(
            "--shared-isolation-nonce", $sharedIsolationNonce,
            "--shared-isolation-ledger", $sharedIsolationLedgerPath)
    }
    if ($Suite -eq "large-stream") {
        $nodeArguments += @(
            "--large-stream-nonce", $largeStreamNonce,
            "--large-stream-ledger", $largeStreamLedgerPath)
    }
    if ($Suite -eq "atomic-failure") {
        $nodeArguments += @(
            "--atomic-failure-nonce", $atomicFailureNonce,
            "--atomic-failure-ledger", $atomicFailureLedgerPath)
    }
    if ($Suite -eq "version-history") {
        $nodeArguments += @("--version-history-nonce", $versionHistoryNonce)
    }
    if ($Suite -eq "history-heap") {
        $nodeArguments += @("--history-heap-nonce", $historyHeapNonce)
    }
    if ($Suite -eq "context-history") {
        $nodeArguments += @(
            "--context-history-nonce", $contextHistoryNonce,
            "--context-history-ledger", $contextHistoryLedgerPath)
    }
    if ($Suite -eq "archive-quota") {
        $nodeArguments += @(
            "--archive-quota-nonce", $archiveQuotaNonce,
            "--archive-quota-ledger", $archiveQuotaLedgerPath)
    }
    if ($Suite -eq "archive-quota-recover") {
        $nodeArguments += @("--archive-quota-ledger", $archiveQuotaLedgerPath)
    }
    if ($Suite -eq "binary-text") {
        $nodeArguments += @(
            "--binary-text-nonce", $binaryTextNonce,
            "--binary-text-ledger", $binaryTextLedgerPath)
    }
    if ($Suite -eq "binary-text-recover") {
        $nodeArguments += @("--binary-text-ledger", $binaryTextLedgerPath)
    }
    $nodeProcess = Start-Process -FilePath "node" `
        -ArgumentList $nodeArguments `
        -WorkingDirectory (Get-Location).Path `
        -RedirectStandardOutput $nodeStdoutPath `
        -RedirectStandardError $nodeStderrPath `
        -WindowStyle Hidden `
        -PassThru `
        -Wait
    $nodeExitCode = $nodeProcess.ExitCode
    $nodeOutput = @()
    if (Test-Path -LiteralPath $nodeStdoutPath) {
        $nodeOutput += Get-Content -LiteralPath $nodeStdoutPath -Encoding UTF8
    }
    if (Test-Path -LiteralPath $nodeStderrPath) {
        $nodeOutput += Get-Content -LiteralPath $nodeStderrPath -Encoding UTF8
    }
    foreach ($line in $nodeOutput) {
        Add-Content -LiteralPath $resolvedLogPath -Value $line
        Write-Host $line
    }
    if ($Suite -eq "unicode-path") {
        $completedLedger = Read-P2UnicodeLedger -LedgerPath $unicodePathLedgerPath
        if ($completedLedger.web_cleanup_complete) {
            $unicodePathWebClean = $true
        }
    }
    if ($Suite -eq "shared-isolation") {
        $completedLedger = Read-P2SharedIsolationLedger `
            -LedgerPath $sharedIsolationLedgerPath
        if ($completedLedger.web_cleanup_complete) {
            $sharedIsolationWebClean = $true
        }
    }
    if ($Suite -eq "large-stream") {
        $completedLedger = Read-P2LargeStreamLedger -LedgerPath $largeStreamLedgerPath
        if ($completedLedger.web_verification_complete) {
            $largeStreamWebVerified = $true
        }
    }
    if ($Suite -eq "context-history") {
        $completedLedger = Read-P2ContextHistoryLedger `
            -LedgerPath $contextHistoryLedgerPath
        if ($completedLedger.web_cleanup_complete) {
            $contextHistoryWebClean = $true
            Remove-Item -LiteralPath $contextHistoryLedgerPath
            $contextHistoryLedgerPresent = $false
        }
    }
    if ($nodeExitCode -ne 0) {
        throw "Hardware Web E2E exited with code $nodeExitCode"
    }
    if ($Suite -in @("archive-quota", "archive-quota-recover")) {
        $resultLines = @(Get-Content -LiteralPath $nodeStdoutPath -Encoding UTF8)
        if ($resultLines.Count -ne 1) {
            throw "P2-28 archive/quota suite must emit exactly one JSON evidence line"
        }
        $evidence = $resultLines[0] | ConvertFrom-Json
        if ($evidence.result -ne "pass" -or $evidence.suite -ne $Suite) {
            throw "P2-28 archive/quota suite returned an invalid evidence envelope"
        }
        if ($Suite -eq "archive-quota") {
            $archiveEvidence = $evidence.archive_quota
            if ($archiveEvidence.messages -ne 129 -or
                $archiveEvidence.content_bytes -ne 2113536 -or
                $archiveEvidence.raw_bytes -le 2097152 -or
                $archiveEvidence.pages -lt 129 -or
                $archiveEvidence.former_2mib_boundary -ne "pass" -or
                $archiveEvidence.final_messages -ne 130 -or
                $archiveEvidence.append_above_boundary -ne "pass" -or
                $archiveEvidence.quota_rejection -ne "pass" -or
                $archiveEvidence.nonmutation -ne "pass" -or
                $archiveEvidence.cleanup -ne "pass") {
                throw "P2-28 archive/quota evidence contract is incomplete"
            }
        }
        elseif ($evidence.recovery.cleanup -ne "pass") {
            throw "P2-28 recovery evidence contract is incomplete"
        }
        Remove-Item -LiteralPath $archiveQuotaLedgerPath
        $archiveQuotaWebClean = $true
    }
    if ($Suite -in @("binary-text", "binary-text-recover")) {
        $resultLines = @(Get-Content -LiteralPath $nodeStdoutPath -Encoding UTF8)
        if ($resultLines.Count -ne 1) {
            throw "P2-29 binary/text suite must emit exactly one JSON evidence line"
        }
        $evidence = $resultLines[0] | ConvertFrom-Json
        if ($evidence.result -ne "pass" -or $evidence.suite -ne $Suite) {
            throw "P2-29 binary/text suite returned an invalid evidence envelope"
        }
        if ($Suite -eq "binary-text") {
            $binaryEvidence = $evidence.binary_text
            if ($binaryEvidence.binary_round_trip -ne "pass" -or
                $binaryEvidence.manage_rename_link -ne "pass" -or
                $binaryEvidence.text_allowlist -ne "pass" -or
                $binaryEvidence.binary_text_rejection -ne "pass" -or
                $binaryEvidence.nonmutation -ne "pass" -or
                $binaryEvidence.cleanup -ne "pass") {
                throw "P2-29 binary/text evidence contract is incomplete"
            }
        }
        elseif ($evidence.recovery.cleanup -ne "pass") {
            throw "P2-29 recovery evidence contract is incomplete"
        }
        Remove-Item -LiteralPath $binaryTextLedgerPath
        $binaryTextWebClean = $true
    }
    if ($Suite -eq "history-heap") {
        $resultLines = @(Get-Content -LiteralPath $nodeStdoutPath -Encoding UTF8)
        if ($resultLines.Count -ne 1) {
            throw "P2-32 history/heap suite must emit exactly one JSON evidence line"
        }
        $evidence = $resultLines[0] | ConvertFrom-Json
        $historyEvidence = $evidence.history_heap
        if ($evidence.result -ne "pass" -or $evidence.suite -ne $Suite -or
            $historyEvidence.small.raw_messages -ne 2 -or
            $historyEvidence.small.raw_content_bytes -ne 32768 -or
            $historyEvidence.small.raw_history_bytes -le 32768 -or
            $historyEvidence.large.raw_messages -ne 16 -or
            $historyEvidence.large.raw_content_bytes -ne 262144 -or
            $historyEvidence.large.raw_history_bytes -le 262144 -or
            $historyEvidence.large.raw_content_bytes -le
                $historyEvidence.large.heap_before.free_heap -or
            $historyEvidence.small.context_budget_bytes -ne 8192 -or
            $historyEvidence.large.context_budget_bytes -ne 8192 -or
            $historyEvidence.small.retained_messages -ne 1 -or
            $historyEvidence.large.retained_messages -ne 1 -or
            $historyEvidence.small.retained_bytes -ne
                $historyEvidence.large.retained_bytes -or
            $historyEvidence.small.provider_result -ne "explicit_error" -or
            $historyEvidence.large.provider_result -ne "explicit_error" -or
            $historyEvidence.small.exact_non_duplication -ne "pass" -or
            $historyEvidence.large.exact_non_duplication -ne "pass" -or
            $historyEvidence.id_format -ne "pass" -or
            $historyEvidence.utf8_identity -ne "pass" -or
            $historyEvidence.complete_raw_exceeds_heap -ne "pass" -or
            $historyEvidence.bounded_context -ne "pass" -or
            $historyEvidence.settled_heap_non_scaling -ne "pass" -or
            $historyEvidence.cleanup -ne "pass") {
            throw "P2-32 history/heap evidence contract is incomplete"
        }
    }
    if ($Suite -eq "context-history-orphan-recover") {
        $resultLines = @(Get-Content -LiteralPath $nodeStdoutPath -Encoding UTF8)
        if ($resultLines.Count -ne 1) {
            throw "P2-27 orphan recovery must emit exactly one JSON evidence line"
        }
        $evidence = $resultLines[0] | ConvertFrom-Json
        $recoveryEvidence = $evidence.orphan_recovery
        if ($evidence.result -ne "pass" -or $evidence.suite -ne $Suite -or
            $recoveryEvidence.matched_projects -ne 1 -or
            $recoveryEvidence.verified_messages -lt 1 -or
            $recoveryEvidence.verified_messages -gt 15 -or
            $recoveryEvidence.pages -lt 1 -or
            $recoveryEvidence.pages -gt 15 -or
            $recoveryEvidence.deleted -ne $true -or
            $recoveryEvidence.original_selection -ne "unrecoverable") {
            throw "P2-27 orphan recovery evidence contract is incomplete"
        }
    }
    if ($Suite -in @("summary-regeneration", "context-history")) {
        $resultLines = @(Get-Content -LiteralPath $nodeStdoutPath -Encoding UTF8)
        if ($resultLines.Count -ne 1) {
            throw "P2 summary/context suite must emit exactly one JSON evidence line"
        }
        $evidence = $resultLines[0] | ConvertFrom-Json
        if ($evidence.result -ne "pass" -or $evidence.suite -ne $Suite) {
            throw "P2 summary/context suite returned an invalid evidence envelope"
        }
        if ($Suite -eq "summary-regeneration") {
            $summaryEvidence = $evidence.summary_regeneration
            if ($summaryEvidence.raw_messages -ne 10 -or
                $summaryEvidence.summarized_messages -ne 2 -or
                $summaryEvidence.event -ne "manual_regenerated" -or
                $summaryEvidence.initial_failure_non_mutation -ne "pass" -or
                $summaryEvidence.replacement_attempt_non_mutation -ne "pass" -or
                $summaryEvidence.raw_history_preserved -ne "pass") {
                throw "P2-26 manual-summary evidence contract is incomplete"
            }
        }
        else {
            $historyEvidence = $evidence.context_history
            if ($historyEvidence.total_messages -ne 15 -or
                $historyEvidence.fixture_bytes -le 12000 -or
                $historyEvidence.fixture_bytes -ge 13000 -or
                $historyEvidence.hidden_messages -ne 9 -or
                $historyEvidence.visible_messages -ne 6 -or
                $historyEvidence.pages -ne 2 -or
                $historyEvidence.retained_messages -ne 6 -or
                $historyEvidence.dropped_messages -ne 9 -or
                $historyEvidence.invalid_cursor_non_mutation -ne "pass" -or
                $historyEvidence.chronological_no_duplicates -ne "pass") {
                throw "P2-27 context/history evidence contract is incomplete"
            }
        }
    }
    if ($Suite -eq "atomic-failure") {
        $atomicFailureWebVerified = $true
    }
    if ($Suite -eq "unicode-path" -and -not $unicodePathWebClean) {
        throw "P2-19 Node suite did not prove exact Web-owned cleanup"
    }
    if ($Suite -eq "shared-isolation" -and -not $sharedIsolationWebClean) {
        throw "P2-20 Node suite did not prove exact Web-owned cleanup"
    }
    if ($Suite -eq "context-history" -and -not $contextHistoryWebClean) {
        throw "P2-27 Node suite did not prove exact Web-owned cleanup"
    }
    if ($Suite -in @("archive-quota", "archive-quota-recover") -and
        -not $archiveQuotaWebClean) {
        throw "P2-28 Node suite did not prove exact Web-owned cleanup"
    }
    if ($Suite -in @("binary-text", "binary-text-recover") -and
        -not $binaryTextWebClean) {
        throw "P2-29 Node suite did not prove exact Web-owned cleanup"
    }
    if ($Suite -eq "large-stream" -and -not $largeStreamWebVerified) {
        throw "P2-21 Node suite did not prove exact bounded Web access"
    }

    $serial.WriteLine("EXIT")
    $serial.BaseStream.Flush()
    $stopped = Wait-SerialLine -Serial $serial -Pattern "^WEB_CONSOLE result=stopped$" `
        -TimeoutSeconds 20 -ResolvedLogPath $resolvedLogPath
    $webConsoleStopped = $true
    Write-Host $stopped
    if ($Suite -eq "instructions") {
        $serial.WriteLine("INSTRUCTIONTEST")
        $serial.BaseStream.Flush()
        $instructionTest = Wait-SerialLine -Serial $serial `
            -Pattern '^INSTRUCTIONTEST result=(?:pass|failed) ' `
            -TimeoutSeconds 20 -ResolvedLogPath $resolvedLogPath
        if ($instructionTest -ne "INSTRUCTIONTEST result=pass order=pass error=none") {
            throw "P2-24 device instruction precedence diagnostic failed: $instructionTest"
        }
        Write-Host $instructionTest
    }
    if ($Suite -eq "request-settings") {
        $serial.WriteLine("P2REQUESTSETTINGSTEST$requestSettingsNonce")
        $serial.BaseStream.Flush()
        $requestSettingsPattern = '^P2REQUESTSETTINGSTEST result=(?:pass|failed) nonce={0} ' -f (
            [regex]::Escape($requestSettingsNonce))
        $requestSettingsTest = Wait-SerialLine -Serial $serial `
            -Pattern $requestSettingsPattern -TimeoutSeconds 60 `
            -ResolvedLogPath $resolvedLogPath
        $requestSettingsPassPattern = '^P2REQUESTSETTINGSTEST result=pass nonce={0} precedence=pass inheritance=pass model=pass context=pass project_output=pass request_output=pass auto_compact=pass no_tools=pass tools=pass ui=pass cleanup=pass error=none$' -f (
            [regex]::Escape($requestSettingsNonce))
        if ($requestSettingsTest -notmatch $requestSettingsPassPattern) {
            throw "P2-25 request-settings diagnostic failed: $requestSettingsTest"
        }
        Write-Host $requestSettingsTest
    }
    if ($Suite -eq "shared-isolation" -and $sharedIsolationWebClean) {
        $serial.WriteLine("P2SHAREDTEST$sharedIsolationNonce")
        $serial.BaseStream.Flush()
        $testPattern = '^P2SHAREDTEST result=(?:pass|failed) nonce={0} ' -f (
            [regex]::Escape($sharedIsolationNonce))
        $deviceTest = Wait-SerialLine -Serial $serial -Pattern $testPattern `
            -TimeoutSeconds 300 -ResolvedLogPath $resolvedLogPath
        $testPassPattern = '^P2SHAREDTEST result=pass nonce={0} identity=pass tools=pass isolation=pass cleanup=pass remaining=0 errors=0 error=none$' -f (
            [regex]::Escape($sharedIsolationNonce))
        if ($deviceTest -notmatch $testPassPattern) {
            throw "P2-20 device Shared-isolation diagnostic failed: $deviceTest"
        }
        Write-Host $deviceTest
    }
    if ($Suite -eq "large-stream" -and $largeStreamWebVerified) {
        $serial.WriteLine("P2LARGEVERIFY$largeStreamNonce")
        $serial.BaseStream.Flush()
        $verifyPattern = '^P2LARGEVERIFY result=(?:pass|failed) nonce={0} ' -f (
            [regex]::Escape($largeStreamNonce))
        $verify = Wait-SerialLine -Serial $serial -Pattern $verifyPattern `
            -TimeoutSeconds 120 -ResolvedLogPath $resolvedLogPath
        $verifyPassPattern = '^P2LARGEVERIFY result=pass nonce={0} path=cardmind_p2_21_{0}/large-320mib\.txt bytes=335544320 list=pass read0=pass read_above=pass read_eof=pass search=pass search_offset=268439552 free_heap_before=(?<free_before>[0-9]+) free_heap_after=(?<free_after>[0-9]+) largest_heap_before=(?<largest_before>[0-9]+) largest_heap_after=(?<largest_after>[0-9]+) min_heap_before=(?<min_before>[0-9]+) min_heap_after=(?<min_after>[0-9]+) error=none$' -f (
            [regex]::Escape($largeStreamNonce))
        if ($verify -notmatch $verifyPassPattern) {
            throw "P2-21 production device verification failed: $verify"
        }
        if ([uint64]$Matches.free_after -lt [uint64]85000 -or
            [uint64]$Matches.largest_after -lt [uint64]28000 -or
            [uint64]$Matches.min_after -lt $largeStreamMinimumHeapFloorBytes -or
            ([int64]$Matches.free_before - [int64]$Matches.free_after) -gt
                $largeStreamSteadyHeapLossBytes -or
            ([int64]$Matches.largest_before - [int64]$Matches.largest_after) -gt
                $largeStreamSteadyHeapLossBytes -or
            ([int64]$Matches.min_before - [int64]$Matches.min_after) -gt
                $largeStreamMinimumHeapLossBytes) {
            throw "P2-21 device verification exceeded the bounded heap budget: $verify"
        }
        $completedLedger = Read-P2LargeStreamLedger -LedgerPath $largeStreamLedgerPath
        $completedLedger.device_verification_complete = $true
        Write-P2LargeStreamLedger `
            -LedgerPath $largeStreamLedgerPath -Ledger $completedLedger
        $largeStreamDeviceVerified = $true
        Write-Host $verify
    }
    if ($workspaceScaleCorpusReady) {
        $serial.WriteLine("P2FILESCALECLEAN$workspaceScaleNonce")
        $serial.BaseStream.Flush()
        $cleanupPattern = '^P2FILESCALECLEAN result=(?:pass|failed) nonce={0} ' -f (
            [regex]::Escape($workspaceScaleNonce))
        $cleanup = Wait-SerialLine -Serial $serial -Pattern $cleanupPattern `
            -TimeoutSeconds 300 -ResolvedLogPath $resolvedLogPath
        if ($cleanup -notmatch '^P2FILESCALECLEAN result=pass .*remaining=0 errors=0 ') {
            throw "P2-17 corpus cleanup failed: $cleanup"
        }
        $workspaceScaleCorpusClean = $true
        if ($workspaceScaleLedgerPresent) {
            Remove-Item -LiteralPath $workspaceScaleLedgerPath
            $workspaceScaleLedgerPresent = $false
        }
        Write-Host $cleanup
    }
    if ($unicodePathSetupAttempted -and $unicodePathWebClean) {
        $serial.WriteLine("P2UNICODECLEAN$unicodePathNonce")
        $serial.BaseStream.Flush()
        $cleanupPattern = '^P2UNICODECLEAN result=(?:pass|failed) nonce={0} ' -f (
            [regex]::Escape($unicodePathNonce))
        $cleanup = Wait-SerialLine -Serial $serial -Pattern $cleanupPattern `
            -TimeoutSeconds 180 -ResolvedLogPath $resolvedLogPath
        if ($cleanup -notmatch '^P2UNICODECLEAN result=pass .*remaining=0 errors=0 ') {
            throw "P2-19 Unicode cleanup failed: $cleanup"
        }
        $unicodePathCorpusClean = $true
        if ($unicodePathLedgerPresent) {
            Remove-Item -LiteralPath $unicodePathLedgerPath
            $unicodePathLedgerPresent = $false
        }
        Write-Host $cleanup
    }
    if ($Suite -eq "shared-isolation" -and $sharedIsolationWebClean) {
        $serial.WriteLine("P2SHAREDCLEAN$sharedIsolationNonce")
        $serial.BaseStream.Flush()
        $cleanupPattern = '^P2SHAREDCLEAN result=(?:pass|failed) nonce={0} ' -f (
            [regex]::Escape($sharedIsolationNonce))
        $cleanup = Wait-SerialLine -Serial $serial -Pattern $cleanupPattern `
            -TimeoutSeconds 180 -ResolvedLogPath $resolvedLogPath
        $cleanupPassPattern = '^P2SHAREDCLEAN result=pass nonce={0} already_absent=(?:yes|no) removed_projects=[0-9]+ removed_files=[0-9]+ remaining=0 errors=0 error=none$' -f (
            [regex]::Escape($sharedIsolationNonce))
        if ($cleanup -notmatch $cleanupPassPattern) {
            throw "P2-20 device Shared-isolation cleanup failed: $cleanup"
        }
        $sharedIsolationDeviceClean = $true
        if ($sharedIsolationLedgerPresent) {
            Remove-Item -LiteralPath $sharedIsolationLedgerPath
            $sharedIsolationLedgerPresent = $false
        }
        Write-Host $cleanup
    }
    if ($Suite -eq "large-stream" -and $largeStreamDeviceVerified) {
        $serial.WriteLine("P2LARGECLEAN$largeStreamNonce")
        $serial.BaseStream.Flush()
        $cleanupPattern = '^P2LARGECLEAN result=(?:pass|failed) nonce={0} ' -f (
            [regex]::Escape($largeStreamNonce))
        $cleanup = Wait-SerialLine -Serial $serial -Pattern $cleanupPattern `
            -TimeoutSeconds 120 -ResolvedLogPath $resolvedLogPath
        $cleanupPassPattern = '^P2LARGECLEAN result=pass nonce={0} already_absent=no removed_file=yes removed_directory=yes error=none$' -f (
            [regex]::Escape($largeStreamNonce))
        if ($cleanup -notmatch $cleanupPassPattern) {
            throw "P2-21 exact fixture cleanup failed: $cleanup"
        }
        Write-Host $cleanup
        $serial.WriteLine("P2LARGECLEAN$largeStreamNonce")
        $serial.BaseStream.Flush()
        $idempotentCleanup = Wait-SerialLine -Serial $serial -Pattern $cleanupPattern `
            -TimeoutSeconds 120 -ResolvedLogPath $resolvedLogPath
        $idempotentPassPattern = '^P2LARGECLEAN result=pass nonce={0} already_absent=yes removed_file=no removed_directory=no error=none$' -f (
            [regex]::Escape($largeStreamNonce))
        if ($idempotentCleanup -notmatch $idempotentPassPattern) {
            throw "P2-21 idempotent fixture cleanup failed: $idempotentCleanup"
        }
        $largeStreamClean = $true
        Remove-P2LargeStreamLedgerArtifacts `
            -LedgerPath $largeStreamLedgerPath -ExpectedNonce $largeStreamNonce
        $largeStreamLedgerPresent = $false
        Write-Host $idempotentCleanup
    }
    if ($Suite -eq "atomic-failure" -and $atomicFailureWebVerified) {
        $serial.WriteLine("P2ATOMICCLEAN$atomicFailureNonce")
        $serial.BaseStream.Flush()
        $cleanupPattern = '^P2ATOMICCLEAN result=(?:pass|failed) nonce={0} ' -f (
            [regex]::Escape($atomicFailureNonce))
        $cleanup = Wait-SerialLine -Serial $serial -Pattern $cleanupPattern `
            -TimeoutSeconds 60 -ResolvedLogPath $resolvedLogPath
        $cleanupPassPattern = '^P2ATOMICCLEAN result=pass nonce={0} already_absent=no removed_file=yes removed_directory=yes error=none$' -f (
            [regex]::Escape($atomicFailureNonce))
        if ($cleanup -notmatch $cleanupPassPattern) {
            throw "P2-22 exact fixture cleanup failed: $cleanup"
        }
        Write-Host $cleanup
        $serial.WriteLine("P2ATOMICCLEAN$atomicFailureNonce")
        $serial.BaseStream.Flush()
        $idempotentCleanup = Wait-SerialLine -Serial $serial -Pattern $cleanupPattern `
            -TimeoutSeconds 60 -ResolvedLogPath $resolvedLogPath
        $idempotentPassPattern = '^P2ATOMICCLEAN result=pass nonce={0} already_absent=yes removed_file=no removed_directory=no error=none$' -f (
            [regex]::Escape($atomicFailureNonce))
        if ($idempotentCleanup -notmatch $idempotentPassPattern) {
            throw "P2-22 idempotent fixture cleanup failed: $idempotentCleanup"
        }
        $atomicFailureClean = $true
        Remove-P2AtomicFailureLedgerArtifacts `
            -LedgerPath $atomicFailureLedgerPath -ExpectedNonce $atomicFailureNonce
        $atomicFailureLedgerPresent = $false
        Write-Host $idempotentCleanup
    }
    Add-Content -LiteralPath $resolvedLogPath -Value (
        "CARDMIND_WEB_E2E result=pass completed={0:o}" -f [DateTime]::UtcNow)
    Write-Host "CARDMIND_WEB_E2E result=pass suite=$Suite log=$resolvedLogPath"
}
finally {
    if ($serial.IsOpen) {
        if ($webConsoleRequested -and -not $webConsoleStopped) {
            try {
                $serial.WriteLine("EXIT")
                $serial.BaseStream.Flush()
                Wait-SerialLine -Serial $serial `
                    -Pattern '^(?:WEB_CONSOLE result=stopped|ERROR event=serial_command reason=unsupported_command)$' `
                    -TimeoutSeconds 20 -ResolvedLogPath $resolvedLogPath | Out-Null
                $webConsoleStopped = $true
            }
            catch {
                Write-Warning "Could not request Web Console shutdown: $($_.Exception.Message)"
            }
        }
        if ($sdFaultActive -and (-not $webConsoleRequested -or $webConsoleStopped)) {
            try {
                $serial.WriteLine("P2SDFAULTCLEAR")
                $serial.BaseStream.Flush()
                Wait-SerialLine -Serial $serial `
                    -Pattern '^P2SDFAULT result=pass command=P2SDFAULTCLEAR state=ready error=none read=pass write=pass$' `
                    -TimeoutSeconds 20 -ResolvedLogPath $resolvedLogPath | Out-Null
                $sdFaultActive = $false
            }
            catch {
                Write-Warning "Could not clear the P2-23 microSD diagnostic fault: $($_.Exception.Message)"
            }
        }
        if ($sdDegradedFixtureSetupAttempted -and -not $sdDegradedFixtureClean -and
            -not $sdFaultActive -and (-not $webConsoleRequested -or $webConsoleStopped)) {
            try {
                $serial.WriteLine("P2ATOMICCLEAN$sdDegradedNonce")
                $serial.BaseStream.Flush()
                $fixtureCleanupPattern = '^P2ATOMICCLEAN result=(?:pass|failed) nonce={0} ' -f (
                    [regex]::Escape($sdDegradedNonce))
                $fixtureCleanup = Wait-SerialLine -Serial $serial `
                    -Pattern $fixtureCleanupPattern -TimeoutSeconds 60 `
                    -ResolvedLogPath $resolvedLogPath
                $fixtureCleanupPassPattern = '^P2ATOMICCLEAN result=pass nonce={0} already_absent=(?:yes|no) removed_file=(?:yes|no) removed_directory=(?:yes|no) error=none$' -f (
                    [regex]::Escape($sdDegradedNonce))
                if ($fixtureCleanup -notmatch $fixtureCleanupPassPattern) {
                    throw "P2-23 read fixture cleanup failed: $fixtureCleanup"
                }
                $sdDegradedFixtureClean = $true
            }
            catch {
                Write-Warning "Could not complete P2-23 read fixture cleanup: $($_.Exception.Message)"
            }
        }
        if ($workspaceScaleCorpusReady -and -not $workspaceScaleCorpusClean -and
            (-not $webConsoleRequested -or $webConsoleStopped)) {
            try {
                $serial.WriteLine("P2FILESCALECLEAN$workspaceScaleNonce")
                $serial.BaseStream.Flush()
                $cleanupPattern = '^P2FILESCALECLEAN result=(?:pass|failed) nonce={0} ' -f (
                    [regex]::Escape($workspaceScaleNonce))
                $cleanup = Wait-SerialLine -Serial $serial -Pattern $cleanupPattern `
                    -TimeoutSeconds 300 -ResolvedLogPath $resolvedLogPath
                if ($cleanup -notmatch '^P2FILESCALECLEAN result=pass .*remaining=0 errors=0 ') {
                    throw "P2-17 corpus cleanup failed: $cleanup"
                }
                $workspaceScaleCorpusClean = $true
                if ($workspaceScaleLedgerPresent) {
                    Remove-Item -LiteralPath $workspaceScaleLedgerPath
                    $workspaceScaleLedgerPresent = $false
                }
            }
            catch {
                Write-Warning "Could not complete P2-17 corpus cleanup: $($_.Exception.Message)"
            }
        }
        if ($unicodePathSetupAttempted -and $unicodePathWebClean -and
            -not $unicodePathCorpusClean -and
            (-not $webConsoleRequested -or $webConsoleStopped)) {
            try {
                $serial.WriteLine("P2UNICODECLEAN$unicodePathNonce")
                $serial.BaseStream.Flush()
                $cleanupPattern = '^P2UNICODECLEAN result=(?:pass|failed) nonce={0} ' -f (
                    [regex]::Escape($unicodePathNonce))
                $cleanup = Wait-SerialLine -Serial $serial -Pattern $cleanupPattern `
                    -TimeoutSeconds 180 -ResolvedLogPath $resolvedLogPath
                if ($cleanup -notmatch '^P2UNICODECLEAN result=pass .*remaining=0 errors=0 ') {
                    throw "P2-19 Unicode cleanup failed: $cleanup"
                }
                $unicodePathCorpusClean = $true
                if ($unicodePathLedgerPresent) {
                    Remove-Item -LiteralPath $unicodePathLedgerPath
                    $unicodePathLedgerPresent = $false
                }
            }
            catch {
                Write-Warning "Could not complete P2-19 Unicode cleanup: $($_.Exception.Message)"
            }
        }
        if ($sharedIsolationLedgerPresent -and $sharedIsolationWebClean -and
            -not $sharedIsolationDeviceClean -and
            (-not $webConsoleRequested -or $webConsoleStopped)) {
            try {
                $serial.WriteLine("P2SHAREDCLEAN$sharedIsolationNonce")
                $serial.BaseStream.Flush()
                $cleanupPattern = '^P2SHAREDCLEAN result=(?:pass|failed) nonce={0} ' -f (
                    [regex]::Escape($sharedIsolationNonce))
                $cleanup = Wait-SerialLine -Serial $serial -Pattern $cleanupPattern `
                    -TimeoutSeconds 180 -ResolvedLogPath $resolvedLogPath
                $cleanupPassPattern = '^P2SHAREDCLEAN result=pass nonce={0} already_absent=(?:yes|no) removed_projects=[0-9]+ removed_files=[0-9]+ remaining=0 errors=0 error=none$' -f (
                    [regex]::Escape($sharedIsolationNonce))
                if ($cleanup -notmatch $cleanupPassPattern) {
                    throw "P2-20 device Shared-isolation cleanup failed: $cleanup"
                }
                $sharedIsolationDeviceClean = $true
                Remove-Item -LiteralPath $sharedIsolationLedgerPath
                $sharedIsolationLedgerPresent = $false
            }
            catch {
                Write-Warning "Could not complete P2-20 Shared cleanup: $($_.Exception.Message)"
            }
        }
        if ($largeStreamLedgerPresent -and $largeStreamSetupAttempted -and
            -not $largeStreamClean -and
            (-not $webConsoleRequested -or $webConsoleStopped)) {
            try {
                $serial.WriteLine("P2LARGECLEAN$largeStreamNonce")
                $serial.BaseStream.Flush()
                $cleanupPattern = '^P2LARGECLEAN result=(?:pass|failed) nonce={0} ' -f (
                    [regex]::Escape($largeStreamNonce))
                $cleanup = Wait-SerialLine -Serial $serial -Pattern $cleanupPattern `
                    -TimeoutSeconds 120 -ResolvedLogPath $resolvedLogPath
                $cleanupPassPattern = '^P2LARGECLEAN result=pass nonce={0} already_absent=(?:yes|no) removed_file=(?:yes|no) removed_directory=(?:yes|no) error=none$' -f (
                    [regex]::Escape($largeStreamNonce))
                if ($cleanup -notmatch $cleanupPassPattern) {
                    throw "P2-21 fixture cleanup failed: $cleanup"
                }
                $largeStreamClean = $true
                Remove-P2LargeStreamLedgerArtifacts `
                    -LedgerPath $largeStreamLedgerPath -ExpectedNonce $largeStreamNonce
                $largeStreamLedgerPresent = $false
            }
            catch {
                Write-Warning "Could not complete P2-21 large fixture cleanup: $($_.Exception.Message)"
            }
        }
        if ($atomicFailureLedgerPresent -and $atomicFailureSetupAttempted -and
            -not $atomicFailureClean -and
            (-not $webConsoleRequested -or $webConsoleStopped)) {
            try {
                $serial.WriteLine("P2ATOMICCLEAN$atomicFailureNonce")
                $serial.BaseStream.Flush()
                $cleanupPattern = '^P2ATOMICCLEAN result=(?:pass|failed) nonce={0} ' -f (
                    [regex]::Escape($atomicFailureNonce))
                $cleanup = Wait-SerialLine -Serial $serial -Pattern $cleanupPattern `
                    -TimeoutSeconds 60 -ResolvedLogPath $resolvedLogPath
                $cleanupPassPattern = '^P2ATOMICCLEAN result=pass nonce={0} already_absent=(?:yes|no) removed_file=(?:yes|no) removed_directory=(?:yes|no) error=none$' -f (
                    [regex]::Escape($atomicFailureNonce))
                if ($cleanup -notmatch $cleanupPassPattern) {
                    throw "P2-22 fixture cleanup failed: $cleanup"
                }
                $atomicFailureClean = $true
                Remove-P2AtomicFailureLedgerArtifacts `
                    -LedgerPath $atomicFailureLedgerPath -ExpectedNonce $atomicFailureNonce
                $atomicFailureLedgerPresent = $false
            }
            catch {
                Write-Warning "Could not complete P2-22 atomic fixture cleanup: $($_.Exception.Message)"
            }
        }
        $serial.Close()
    }
    $serial.Dispose()
}
