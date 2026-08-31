param(
    [Parameter(Mandatory = $true)]
    [string]$Port,

    [Parameter(Mandatory = $true)]
    [ValidateRange(9600, 921600)]
    [int]$BaudRate,

    [Parameter(Mandatory = $true)]
    [ValidateSet("status", "sd-mount", "audio", "offline", "online", "p1", "p2-storage", "p2-migration", "p2-migration-exact", "p2-projects", "p2-chats", "p2-context", "p2-summary", "p2-limits", "p2-archive", "p2-file", "p2-binary", "hotfix-message", "hotfix-latency", "hotfix-device-ui", "web-console-start", "web-console-cycle", "full")]
    [string]$Suite,

    [Parameter(Mandatory = $true)]
    [string]$LogPath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function New-RegressionCase {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Name,

        [Parameter(Mandatory = $true)]
        [string]$Command,

        [Parameter(Mandatory = $true)]
        [string]$CompletionPattern,

        [Parameter(Mandatory = $true)]
        [string]$PassPattern,

        [Parameter(Mandatory = $true)]
        [ValidateRange(1, 600)]
        [int]$TimeoutSeconds
    )

    return [pscustomobject]@{
        Name = $Name
        Command = $Command
        CompletionPattern = $CompletionPattern
        PassPattern = $PassPattern
        TimeoutSeconds = $TimeoutSeconds
    }
}

function Read-SerialLines {
    param(
        [Parameter(Mandatory = $true)]
        [System.IO.Ports.SerialPort]$Serial,

        [Parameter(Mandatory = $true)]
        [AllowEmptyString()]
        [string]$Pending
    )

    $combined = $Pending + $Serial.ReadExisting()
    $parts = $combined -split "`n"
    $lines = [System.Collections.Generic.List[string]]::new()
    for ($index = 0; $index -lt $parts.Count - 1; $index++) {
        $lines.Add($parts[$index].TrimEnd("`r"))
    }
    return [pscustomobject]@{
        Lines = $lines
        Pending = $parts[-1]
    }
}

function Add-SerialCrashTail {
    param(
        [Parameter(Mandatory = $true)]
        [System.IO.Ports.SerialPort]$Serial,

        [Parameter(Mandatory = $true)]
        [string]$LogPath
    )

    $pending = ""
    $stopwatch = [System.Diagnostics.Stopwatch]::StartNew()
    while ($stopwatch.Elapsed.TotalSeconds -lt 4) {
        Start-Sleep -Milliseconds 40
        $read = Read-SerialLines -Serial $Serial -Pending $pending
        $pending = $read.Pending
        foreach ($line in $read.Lines) {
            if ($line.Length -gt 0) {
                Add-Content -LiteralPath $LogPath -Value $line
            }
        }
    }
    if ($pending.Length -gt 0) {
        Add-Content -LiteralPath $LogPath -Value $pending
    }
}

function Invoke-RegressionCase {
    param(
        [Parameter(Mandatory = $true)]
        [System.IO.Ports.SerialPort]$Serial,

        [Parameter(Mandatory = $true)]
        [pscustomobject]$Case,

        [Parameter(Mandatory = $true)]
        [string]$LogPath
    )

    Start-Sleep -Milliseconds 1000
    $synced = $false
    for ($attempt = 1; $attempt -le 3; $attempt++) {
        $Serial.ReadExisting() | Out-Null
        $Serial.WriteLine("PING")
        $Serial.BaseStream.Flush()
        $syncPending = ""
        $syncStopwatch = [System.Diagnostics.Stopwatch]::StartNew()
        while ($syncStopwatch.Elapsed.TotalSeconds -lt 3) {
            Start-Sleep -Milliseconds 40
            $syncRead = Read-SerialLines -Serial $Serial -Pending $syncPending
            $syncPending = $syncRead.Pending
            foreach ($syncLine in $syncRead.Lines) {
                if ($syncLine.Length -eq 0) {
                    continue
                }
                Add-Content -LiteralPath $LogPath -Value $syncLine
                if ($syncLine -match "Guru Meditation|Brownout|abort\(\)|ESP-ROM:esp32s3|rst:0x") {
                    Add-SerialCrashTail -Serial $Serial -LogPath $LogPath
                    throw "Device reset or panic while synchronizing before '$($Case.Name)': $syncLine"
                }
                if ($syncLine -eq "PONG") {
                    $synced = $true
                    break
                }
            }
            if ($synced) {
                break
            }
        }
        if ($synced) {
            break
        }
    }
    if (-not $synced) {
        throw "Serial channel did not answer PING before '$($Case.Name)'"
    }
    Start-Sleep -Milliseconds 40
    $Serial.ReadExisting() | Out-Null
    $Serial.WriteLine($Case.Command)
    $Serial.BaseStream.Flush()
    Write-Host ("RUN  {0}" -f $Case.Name)
    Add-Content -LiteralPath $LogPath -Value ("COMMAND name={0}" -f $Case.Name)

    $pending = ""
    $completedLine = ""
    $stopwatch = [System.Diagnostics.Stopwatch]::StartNew()
    while ($stopwatch.Elapsed.TotalSeconds -lt $Case.TimeoutSeconds) {
        Start-Sleep -Milliseconds 40
        $read = Read-SerialLines -Serial $Serial -Pending $pending
        $pending = $read.Pending
        foreach ($line in $read.Lines) {
            if ($line.Length -eq 0) {
                continue
            }
            Add-Content -LiteralPath $LogPath -Value $line
            if ($line -match "Guru Meditation|Brownout|abort\(\)|ESP-ROM:esp32s3|rst:0x") {
                Add-SerialCrashTail -Serial $Serial -LogPath $LogPath
                throw "Device reset or panic during '$($Case.Name)': $line"
            }
            if ($line -match $Case.CompletionPattern) {
                $completedLine = $line
                break
            }
        }
        if ($completedLine.Length -gt 0) {
            break
        }
    }

    if ($completedLine.Length -eq 0) {
        throw "Timed out after $($Case.TimeoutSeconds)s waiting for '$($Case.Name)'"
    }
    if ($completedLine -notmatch $Case.PassPattern) {
        throw "Regression '$($Case.Name)' failed: $completedLine"
    }
    Write-Host ("PASS {0}" -f $Case.Name)
}

$offlineCases = @(
    (New-RegressionCase -Name "status" -Command "STATUS" -CompletionPattern "^STATUS version=" -PassPattern "board_adv=yes.*microsd=ready.*chats=ready.*files=ready" -TimeoutSeconds 15),
    (New-RegressionCase -Name "pure functions" -Command "SELFTEST" -CompletionPattern "^SELFTEST result=" -PassPattern "^SELFTEST result=pass$" -TimeoutSeconds 15),
    (New-RegressionCase -Name "display frame budget" -Command "UIBENCH" -CompletionPattern "^UIBENCH result=" -PassPattern "^UIBENCH result=pass" -TimeoutSeconds 20),
    (New-RegressionCase -Name "cancellation" -Command "CANCELTEST" -CompletionPattern "^CANCELTEST result=" -PassPattern "^CANCELTEST result=pass$" -TimeoutSeconds 20),
    (New-RegressionCase -Name "chat and SD storage" -Command "STORAGETEST" -CompletionPattern "^STORAGETEST result=" -PassPattern "^STORAGETEST result=pass$" -TimeoutSeconds 45),
    (New-RegressionCase -Name "chat quality-of-life" -Command "CHATQOLTEST" -CompletionPattern "^CHATQOLTEST result=" -PassPattern "^CHATQOLTEST result=pass" -TimeoutSeconds 90),
    (New-RegressionCase -Name "large workspace file" -Command "FILETEST" -CompletionPattern "^FILETEST result=" -PassPattern "^FILETEST result=pass" -TimeoutSeconds 180),
    (New-RegressionCase -Name "device settings" -Command "DEVICESETTINGSTEST" -CompletionPattern "^DEVICESETTINGSTEST result=" -PassPattern "^DEVICESETTINGSTEST result=pass" -TimeoutSeconds 45),
    (New-RegressionCase -Name "offline tools" -Command "OFFLINETEST" -CompletionPattern "^OFFLINETEST result=" -PassPattern "^OFFLINETEST result=pass$" -TimeoutSeconds 20),
    (New-RegressionCase -Name "SSH runtime" -Command "SSHCHECK" -CompletionPattern "^SSHCHECK result=" -PassPattern "^SSHCHECK result=pass" -TimeoutSeconds 60),
    (New-RegressionCase -Name "Python partition layout" -Command "PYTHONCHECK" -CompletionPattern "^PYTHONCHECK result=" -PassPattern "^PYTHONCHECK result=pass.*layout=yes.*image=yes" -TimeoutSeconds 20),
    (New-RegressionCase -Name "microphone before speaker" -Command "MICTEST" -CompletionPattern "^MICTEST result=" -PassPattern "^MICTEST result=pass.*peak=[1-9][0-9]*" -TimeoutSeconds 20),
    (New-RegressionCase -Name "speaker hardware" -Command "TTSHW" -CompletionPattern "^TTSHW result=" -PassPattern "^TTSHW result=pass$" -TimeoutSeconds 20),
    (New-RegressionCase -Name "speaker cancellation" -Command "TTSSTOPTEST" -CompletionPattern "^TTSSTOPTEST result=" -PassPattern "^TTSSTOPTEST result=pass$" -TimeoutSeconds 20),
    (New-RegressionCase -Name "microphone after speaker" -Command "MICTEST" -CompletionPattern "^MICTEST result=" -PassPattern "^MICTEST result=pass.*peak=[1-9][0-9]*" -TimeoutSeconds 20),
    (New-RegressionCase -Name "web console start" -Command "CONSOLE" -CompletionPattern "^WEB_CONSOLE result=ready" -PassPattern "^WEB_CONSOLE result=ready" -TimeoutSeconds 20),
    (New-RegressionCase -Name "web console status" -Command "STATUS" -CompletionPattern "^WEB_CONSOLE status=" -PassPattern "^WEB_CONSOLE status=ready authenticated=no" -TimeoutSeconds 15),
    (New-RegressionCase -Name "web console exit" -Command "EXIT" -CompletionPattern "^WEB_CONSOLE result=stopped" -PassPattern "^WEB_CONSOLE result=stopped$" -TimeoutSeconds 20),
    (New-RegressionCase -Name "post-console responsiveness" -Command "STATUS" -CompletionPattern "^STATUS version=" -PassPattern "board_adv=yes.*microsd=ready.*chats=ready.*files=ready" -TimeoutSeconds 15)
)

$audioCases = @(
    (New-RegressionCase -Name "microphone before speaker" -Command "MICTEST" -CompletionPattern "^MICTEST result=" -PassPattern "^MICTEST result=pass.*peak=[1-9][0-9]*" -TimeoutSeconds 20),
    (New-RegressionCase -Name "codec idle after microphone" -Command "AUDIOSTATUS" -CompletionPattern "^AUDIOSTATUS result=" -PassPattern "^AUDIOSTATUS result=pass$" -TimeoutSeconds 15),
    (New-RegressionCase -Name "speaker hardware" -Command "TTSHW" -CompletionPattern "^TTSHW result=" -PassPattern "^TTSHW result=pass$" -TimeoutSeconds 20),
    (New-RegressionCase -Name "codec idle after speaker" -Command "AUDIOSTATUS" -CompletionPattern "^AUDIOSTATUS result=" -PassPattern "^AUDIOSTATUS result=pass$" -TimeoutSeconds 15),
    (New-RegressionCase -Name "speaker cancellation" -Command "TTSSTOPTEST" -CompletionPattern "^TTSSTOPTEST result=" -PassPattern "^TTSSTOPTEST result=pass$" -TimeoutSeconds 20),
    (New-RegressionCase -Name "codec idle after cancellation" -Command "AUDIOSTATUS" -CompletionPattern "^AUDIOSTATUS result=" -PassPattern "^AUDIOSTATUS result=pass$" -TimeoutSeconds 15),
    (New-RegressionCase -Name "microphone after speaker" -Command "MICTEST" -CompletionPattern "^MICTEST result=" -PassPattern "^MICTEST result=pass.*peak=[1-9][0-9]*" -TimeoutSeconds 20),
    (New-RegressionCase -Name "codec final idle" -Command "AUDIOSTATUS" -CompletionPattern "^AUDIOSTATUS result=" -PassPattern "^AUDIOSTATUS result=pass$" -TimeoutSeconds 15)
)

$onlineCases = @(
    (New-RegressionCase -Name "chat API" -Command "APITEST" -CompletionPattern "^APITEST result=" -PassPattern "^APITEST result=pass" -TimeoutSeconds 120),
    (New-RegressionCase -Name "model file tool" -Command "TOOLTEST" -CompletionPattern "^TOOLTEST result=" -PassPattern "^TOOLTEST result=pass" -TimeoutSeconds 180),
    (New-RegressionCase -Name "web search API" -Command "WEBTEST" -CompletionPattern "^WEBTEST result=" -PassPattern "^WEBTEST result=pass(?: error=none)?$" -TimeoutSeconds 120),
    (New-RegressionCase -Name "web contents API" -Command "FETCHTEST" -CompletionPattern "^FETCHTEST result=" -PassPattern "^FETCHTEST result=pass(?: error=none)?$" -TimeoutSeconds 120),
    (New-RegressionCase -Name "search sources cache" -Command "SEARCHCACHETEST" -CompletionPattern "^SEARCHCACHETEST result=" -PassPattern "^SEARCHCACHETEST result=pass" -TimeoutSeconds 30),
    (New-RegressionCase -Name "model search tool decision" -Command "SEARCHTEST" -CompletionPattern "^SEARCHTEST result=" -PassPattern "^SEARCHTEST result=(?:pass search_called=yes tool=web_search response_bytes=[1-9][0-9]* error=none|failed search_called=no tool=WebSearch response_bytes=[1-9][0-9]* error=Model did not call every required tool capability \(group mask 0x1\))$" -TimeoutSeconds 240),
    (New-RegressionCase -Name "UI search path" -Command "E2ETEST" -CompletionPattern "^E2ETEST result=" -PassPattern "^E2ETEST result=pass.*response=yes.*cleanup=yes" -TimeoutSeconds 300),
    (New-RegressionCase -Name "SSH host probe" -Command "SSHPROBE" -CompletionPattern "^SSHPROBE result=" -PassPattern "^SSHPROBE result=pass" -TimeoutSeconds 120),
    (New-RegressionCase -Name "public SSH SFTP and PTY" -Command "SSHDEMOTEST" -CompletionPattern "^SSHDEMOTEST result=" -PassPattern "^SSHDEMOTEST result=pass" -TimeoutSeconds 300),
    (New-RegressionCase -Name "STT TLS" -Command "STTTLS" -CompletionPattern "^STTTLS result=" -PassPattern "^STTTLS result=pass$" -TimeoutSeconds 90),
    (New-RegressionCase -Name "STT authentication" -Command "STTAUTH" -CompletionPattern "^STTAUTH result=" -PassPattern "^STTAUTH result=pass$" -TimeoutSeconds 120),
    (New-RegressionCase -Name "TTS TLS" -Command "TTSTLS" -CompletionPattern "^TTSTLS result=" -PassPattern "^TTSTLS result=pass$" -TimeoutSeconds 90),
    (New-RegressionCase -Name "TTS authentication" -Command "TTSAUTH" -CompletionPattern "^TTSAUTH result=" -PassPattern "^TTSAUTH result=pass" -TimeoutSeconds 120),
    (New-RegressionCase -Name "TTS synthesis and playback" -Command "TTSTEST" -CompletionPattern "^TTSTEST result=" -PassPattern "^TTSTEST result=pass" -TimeoutSeconds 180),
    (New-RegressionCase -Name "post-online responsiveness" -Command "STATUS" -CompletionPattern "^STATUS version=" -PassPattern "wifi=connected.*heap=[1-9][0-9]+" -TimeoutSeconds 15),
    (New-RegressionCase -Name "configured SSH terminal" -Command "SSHSESSIONTEST" -CompletionPattern "^SSHSESSIONTEST result=" -PassPattern "^SSHSESSIONTEST result=pass" -TimeoutSeconds 180),
    (New-RegressionCase -Name "configured SFTP" -Command "SFTPTEST" -CompletionPattern "^SFTPTEST result=" -PassPattern "^SFTPTEST result=pass" -TimeoutSeconds 180),
    (New-RegressionCase -Name "OTA metadata" -Command "OTACHECK" -CompletionPattern "^OTACHECK result=" -PassPattern "^OTACHECK result=pass" -TimeoutSeconds 120),
    (New-RegressionCase -Name "OTA download and digest" -Command "OTADOWNLOADTEST" -CompletionPattern "^OTADOWNLOADTEST result=" -PassPattern "^OTADOWNLOADTEST result=pass" -TimeoutSeconds 300)
)

$p1Cases = @(
    (New-RegressionCase -Name "chat API" -Command "APITEST" -CompletionPattern "^APITEST result=" -PassPattern "^APITEST result=pass" -TimeoutSeconds 120),
    (New-RegressionCase -Name "model file tool" -Command "TOOLTEST" -CompletionPattern "^TOOLTEST result=" -PassPattern "^TOOLTEST result=pass" -TimeoutSeconds 180),
    (New-RegressionCase -Name "web search API" -Command "WEBTEST" -CompletionPattern "^WEBTEST result=" -PassPattern "^WEBTEST result=pass(?: error=none)?$" -TimeoutSeconds 120),
    (New-RegressionCase -Name "web contents API" -Command "FETCHTEST" -CompletionPattern "^FETCHTEST result=" -PassPattern "^FETCHTEST result=pass(?: error=none)?$" -TimeoutSeconds 120),
    (New-RegressionCase -Name "search sources cache" -Command "SEARCHCACHETEST" -CompletionPattern "^SEARCHCACHETEST result=" -PassPattern "^SEARCHCACHETEST result=pass" -TimeoutSeconds 30),
    (New-RegressionCase -Name "model search tool decision" -Command "SEARCHTEST" -CompletionPattern "^SEARCHTEST result=" -PassPattern "^SEARCHTEST result=(?:pass search_called=yes tool=web_search response_bytes=[1-9][0-9]* error=none|failed search_called=no tool=WebSearch response_bytes=[1-9][0-9]* error=Model did not call every required tool capability \(group mask 0x1\))$" -TimeoutSeconds 240),
    (New-RegressionCase -Name "UI search path" -Command "E2ETEST" -CompletionPattern "^E2ETEST result=" -PassPattern "^E2ETEST result=pass.*response=yes.*cleanup=yes" -TimeoutSeconds 300),
    (New-RegressionCase -Name "STT TLS" -Command "STTTLS" -CompletionPattern "^STTTLS result=" -PassPattern "^STTTLS result=pass$" -TimeoutSeconds 90),
    (New-RegressionCase -Name "STT authentication" -Command "STTAUTH" -CompletionPattern "^STTAUTH result=" -PassPattern "^STTAUTH result=pass$" -TimeoutSeconds 120),
    (New-RegressionCase -Name "TTS TLS" -Command "TTSTLS" -CompletionPattern "^TTSTLS result=" -PassPattern "^TTSTLS result=pass$" -TimeoutSeconds 90),
    (New-RegressionCase -Name "TTS authentication" -Command "TTSAUTH" -CompletionPattern "^TTSAUTH result=" -PassPattern "^TTSAUTH result=pass" -TimeoutSeconds 120),
    (New-RegressionCase -Name "TTS synthesis and playback" -Command "TTSTEST" -CompletionPattern "^TTSTEST result=" -PassPattern "^TTSTEST result=pass" -TimeoutSeconds 180),
    (New-RegressionCase -Name "post-P1 responsiveness" -Command "STATUS" -CompletionPattern "^STATUS version=" -PassPattern "wifi=connected.*heap=[1-9][0-9]+" -TimeoutSeconds 15)
)

$p2StorageCases = @(
    (New-RegressionCase -Name "project schema and pagination" -Command "PROJECTSCHEMATEST" -CompletionPattern "^PROJECTSCHEMATEST result=" -PassPattern "^PROJECTSCHEMATEST result=pass chats=33 error=none$" -TimeoutSeconds 180),
    (New-RegressionCase -Name "legacy project migration" -Command "MIGRATIONTEST" -CompletionPattern "^MIGRATIONTEST result=" -PassPattern "^MIGRATIONTEST result=pass legacy=([0-9]+) matched=\1 messages=[0-9]+ archived=[0-9]+ history_fnv32=[0-9a-f]{8} metadata=pass history=pass revision=[1-9][0-9]* error=none$" -TimeoutSeconds 240),
    (New-RegressionCase -Name "migration interruption and corruption" -Command "MIGRATIONRECOVERYTEST" -CompletionPattern "^MIGRATIONRECOVERYTEST result=" -PassPattern "^MIGRATIONRECOVERYTEST result=pass staging=yes corruption=yes restored=yes error=none$" -TimeoutSeconds 180)
)

$p2SharedNonce = [DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds().ToString(
    [System.Globalization.CultureInfo]::InvariantCulture)
$p2SharedCommand = "P2SHAREDTEST$p2SharedNonce"
$p2SharedCompletion = '^P2SHAREDTEST result=(?:pass|failed) nonce={0} ' -f (
    [regex]::Escape($p2SharedNonce))
$p2SharedPass = '^P2SHAREDTEST result=pass nonce={0} identity=pass tools=pass isolation=pass cleanup=pass remaining=0 errors=0 error=none$' -f (
    [regex]::Escape($p2SharedNonce))

$p2ProjectCases = @(
    (New-RegressionCase -Name "project device parity" -Command "PROJECTPARITYTEST" -CompletionPattern "^PROJECTPARITYTEST result=" -PassPattern "^PROJECTPARITYTEST result=pass ui=pass error=none$" -TimeoutSeconds 240),
    (New-RegressionCase -Name "shared project isolation" -Command $p2SharedCommand -CompletionPattern $p2SharedCompletion -PassPattern $p2SharedPass -TimeoutSeconds 180)
)

$p2ChatCases = @(
    (New-RegressionCase -Name "project chat isolation" -Command "PROJECTCHATTEST" -CompletionPattern "^PROJECTCHATTEST result=" -PassPattern "^PROJECTCHATTEST result=pass chats=3 error=none$" -TimeoutSeconds 120),
    (New-RegressionCase -Name "instruction precedence" -Command "INSTRUCTIONTEST" -CompletionPattern "^INSTRUCTIONTEST result=" -PassPattern "^INSTRUCTIONTEST result=pass order=pass error=none$" -TimeoutSeconds 20),
    (New-RegressionCase -Name "retry persistence" -Command "RETRYPERSISTENCETEST" -CompletionPattern "^RETRYPERSISTENCETEST result=" -PassPattern "^RETRYPERSISTENCETEST result=pass messages=2 user_copies=1 error=none$" -TimeoutSeconds 60),
    (New-RegressionCase -Name "context compaction persistence" -Command "COMPACTIONTEST" -CompletionPattern "^COMPACTIONTEST result=" -PassPattern "^COMPACTIONTEST result=pass raw=12 manual_tail=8 auto_tail=4 error=none$" -TimeoutSeconds 60),
    (New-RegressionCase -Name "production summary regeneration" -Command "P2SUMMARYTEST26001" -CompletionPattern "^P2SUMMARYTEST result=" -PassPattern "^P2SUMMARYTEST result=pass nonce=26001 provider=pass replace=pass covered=pass raw=pass context=pass cleanup=pass error=none$" -TimeoutSeconds 180)
)

$p2LimitCases = @(
    (New-RegressionCase -Name "P2 storage and request boundaries" -Command "P2LIMITTEST" -CompletionPattern "^P2LIMITTEST result=" -PassPattern "^P2LIMITTEST result=pass prompt=pass project=pass chat=pass error=none$" -TimeoutSeconds 120)
)

$p2FileCases = @(
    (New-RegressionCase -Name "large workspace window, search and edit" -Command "FILETEST" -CompletionPattern "^FILETEST result=" -PassPattern "^FILETEST result=pass$" -TimeoutSeconds 180)
)

$p2DiagnosticNonce = [DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds().ToString(
    [System.Globalization.CultureInfo]::InvariantCulture)
$p2ArchiveCases = @(
    (New-RegressionCase -Name "unbounded project chat archive" -Command "P2ARCHIVETEST$p2DiagnosticNonce" -CompletionPattern "^P2ARCHIVETEST result=(?:pass|failed) nonce=$p2DiagnosticNonce " -PassPattern "^P2ARCHIVETEST result=pass nonce=$p2DiagnosticNonce beyond_2mib=pass quota=pass full=pass planner=pass nonmutation=pass first=pass middle=pass last=pass count=pass hash=pass artifacts=pass cleanup=pass heap_before=[1-9][0-9]* heap_after=[1-9][0-9]* largest_before=[1-9][0-9]* largest_after=[1-9][0-9]* error=none$" -TimeoutSeconds 600)
)
$p2BinaryCases = @(
    (New-RegressionCase -Name "binary transfer and text-tool boundary" -Command "P2BINARYTEST$p2DiagnosticNonce" -CompletionPattern "^P2BINARYTEST result=(?:pass|failed) nonce=$p2DiagnosticNonce " -PassPattern "^P2BINARYTEST result=pass nonce=$p2DiagnosticNonce matrix=pass ui=pass read=pass write=pass append=pass nonmutation=pass cleanup=pass error=none$" -TimeoutSeconds 90)
)

$hotfixLatencyCases = @(
    (New-RegressionCase -Name "project and chat navigation latency" -Command "HOTFIXNAVTEST" -CompletionPattern "^HOTFIXNAVTEST result=" -PassPattern "^HOTFIXNAVTEST result=pass iterations=8 projects_ms=[0-9]+ chats_ms=[0-9]+ average_ms=[0-9]+ error=none$" -TimeoutSeconds 30)
)
$hotfixDeviceUiCases = @(
    (New-RegressionCase -Name "device chat input latency" -Command "HOTFIXINPUTTEST" -CompletionPattern "^HOTFIXINPUTTEST result=" -PassPattern "^HOTFIXINPUTTEST result=pass full_average_us=[0-9]+ input_average_us=[0-9]+ error=none$" -TimeoutSeconds 30),
    (New-RegressionCase -Name "project and chat navigation latency" -Command "HOTFIXNAVTEST" -CompletionPattern "^HOTFIXNAVTEST result=" -PassPattern "^HOTFIXNAVTEST result=pass iterations=8 projects_ms=[0-9]+ chats_ms=[0-9]+ average_ms=[0-9]+ error=none$" -TimeoutSeconds 30),
    (New-RegressionCase -Name "microSD read and recovery guards" -Command "HOTFIXSDTEST" -CompletionPattern "^HOTFIXSDTEST result=" -PassPattern "^HOTFIXSDTEST result=pass removed=pass replaced=pass nonmutation=pass error=none$" -TimeoutSeconds 20)
)

$sdMountCases = @(
    (New-RegressionCase -Name "SD remount" -Command "SDMOUNTTEST" -CompletionPattern "^SDMOUNTTEST result=" -PassPattern "^SDMOUNTTEST result=pass card_type=[1-9][0-9]* total_bytes=[1-9][0-9]* used_bytes=[0-9]+ error=none$" -TimeoutSeconds 20)
)

$webConsoleStartCases = @(
    (New-RegressionCase -Name "web console start" -Command "CONSOLE" -CompletionPattern "^WEB_CONSOLE result=ready" -PassPattern "^WEB_CONSOLE result=ready" -TimeoutSeconds 20)
)

$webConsoleCycleCases = @(
    (New-RegressionCase -Name "web console first start" -Command "CONSOLE" -CompletionPattern "^WEB_CONSOLE result=ready" -PassPattern "^WEB_CONSOLE result=ready" -TimeoutSeconds 20),
    (New-RegressionCase -Name "web console first exit" -Command "EXIT" -CompletionPattern "^WEB_CONSOLE result=stopped" -PassPattern "^WEB_CONSOLE result=stopped$" -TimeoutSeconds 20),
    (New-RegressionCase -Name "web console second start" -Command "CONSOLE" -CompletionPattern "^WEB_CONSOLE result=ready" -PassPattern "^WEB_CONSOLE result=ready" -TimeoutSeconds 20),
    (New-RegressionCase -Name "web console second exit" -Command "EXIT" -CompletionPattern "^WEB_CONSOLE result=stopped" -PassPattern "^WEB_CONSOLE result=stopped$" -TimeoutSeconds 20)
)

$resolvedLogPath = [System.IO.Path]::GetFullPath($LogPath)
$logDirectory = [System.IO.Path]::GetDirectoryName($resolvedLogPath)
if (-not [string]::IsNullOrEmpty($logDirectory)) {
    [System.IO.Directory]::CreateDirectory($logDirectory) | Out-Null
}
Set-Content -LiteralPath $resolvedLogPath -Value ("CARDMIND_REGRESSION suite={0} port={1} started={2:o}" -f $Suite, $Port, [DateTime]::UtcNow)

$serial = [System.IO.Ports.SerialPort]::new($Port, $BaudRate, [System.IO.Ports.Parity]::None, 8, [System.IO.Ports.StopBits]::One)
$serial.NewLine = "`r`n"
$serial.DtrEnable = $false
$serial.RtsEnable = $false
$serial.ReadTimeout = 250
$serial.WriteTimeout = 2000

try {
    $serial.Open()
    Start-Sleep -Seconds 12
    $serial.ReadExisting() | Out-Null
    $cases = [System.Collections.Generic.List[object]]::new()
    if ($Suite -eq "status") {
        $cases.Add($offlineCases[0])
    }
    if ($Suite -eq "sd-mount") {
        $cases.AddRange([object[]]$sdMountCases)
    }
    if ($Suite -eq "audio") {
        $cases.AddRange([object[]]$audioCases)
    }
    if ($Suite -eq "offline" -or $Suite -eq "full") {
        $cases.AddRange([object[]]$offlineCases)
    }
    if ($Suite -eq "online" -or $Suite -eq "full") {
        $cases.AddRange([object[]]$onlineCases)
    }
    if ($Suite -eq "hotfix-message") {
        $cases.Add($onlineCases[6])
    }
    if ($Suite -eq "hotfix-latency") {
        $cases.AddRange([object[]]$hotfixLatencyCases)
    }
    if ($Suite -eq "hotfix-device-ui") {
        $cases.AddRange([object[]]$hotfixDeviceUiCases)
    }
    if ($Suite -eq "p1") {
        $cases.AddRange([object[]]$p1Cases)
    }
    if ($Suite -eq "p2-storage" -or $Suite -eq "full") {
        $cases.AddRange([object[]]$p2StorageCases)
    }
    if ($Suite -eq "p2-migration") {
        $cases.Add($p2StorageCases[1])
        $cases.Add($p2StorageCases[2])
    }
    if ($Suite -eq "p2-migration-exact") {
        $cases.Add($p2StorageCases[1])
    }
    if ($Suite -eq "p2-projects" -or $Suite -eq "full") {
        $cases.AddRange([object[]]$p2ProjectCases)
    }
    if ($Suite -eq "p2-chats" -or $Suite -eq "full") {
        $cases.AddRange([object[]]$p2ChatCases)
    }
    if ($Suite -eq "p2-summary") {
        $cases.Add($p2ChatCases[4])
    }
    if ($Suite -eq "p2-context") {
        $cases.Add($p2ChatCases[3])
    }
    if ($Suite -eq "p2-limits" -or $Suite -eq "full") {
        $cases.AddRange([object[]]$p2LimitCases)
    }
    if ($Suite -eq "p2-archive") {
        $cases.AddRange([object[]]$p2ArchiveCases)
    }
    if ($Suite -eq "p2-file") {
        $cases.AddRange([object[]]$p2FileCases)
    }
    if ($Suite -eq "p2-binary") {
        $cases.AddRange([object[]]$p2BinaryCases)
    }
    if ($Suite -eq "web-console-start") {
        $cases.AddRange([object[]]$webConsoleStartCases)
    }
    if ($Suite -eq "web-console-cycle") {
        $cases.AddRange([object[]]$webConsoleCycleCases)
    }
    foreach ($case in $cases) {
        Invoke-RegressionCase -Serial $serial -Case $case -LogPath $resolvedLogPath
    }
    Add-Content -LiteralPath $resolvedLogPath -Value ("CARDMIND_REGRESSION result=pass completed={0:o}" -f [DateTime]::UtcNow)
    Write-Host ("CARDMIND_REGRESSION result=pass cases={0} log={1}" -f $cases.Count, $resolvedLogPath)
} finally {
    if ($serial.IsOpen) {
        $serial.WriteLine("EXIT")
        $serial.BaseStream.Flush()
        Start-Sleep -Milliseconds 500
        $serial.Close()
    }
    $serial.Dispose()
}
