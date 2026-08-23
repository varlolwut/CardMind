param(
    [Parameter(Mandatory = $true)]
    [string]$Port,

    [Parameter(Mandatory = $true)]
    [ValidateRange(9600, 921600)]
    [int]$BaudRate,

    [Parameter(Mandatory = $true)]
    [ValidateSet("offline", "full")]
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
    (New-RegressionCase -Name "cancellation" -Command "CANCELTEST" -CompletionPattern "^CANCELTEST result=" -PassPattern "^CANCELTEST result=pass$" -TimeoutSeconds 20),
    (New-RegressionCase -Name "chat and SD storage" -Command "STORAGETEST" -CompletionPattern "^STORAGETEST result=" -PassPattern "^STORAGETEST result=pass$" -TimeoutSeconds 45),
    (New-RegressionCase -Name "chat quality-of-life" -Command "CHATQOLTEST" -CompletionPattern "^CHATQOLTEST result=" -PassPattern "^CHATQOLTEST result=pass" -TimeoutSeconds 90),
    (New-RegressionCase -Name "large workspace file" -Command "FILETEST" -CompletionPattern "^FILETEST result=" -PassPattern "^FILETEST result=pass" -TimeoutSeconds 180),
    (New-RegressionCase -Name "device settings" -Command "DEVICESETTINGSTEST" -CompletionPattern "^DEVICESETTINGSTEST result=" -PassPattern "^DEVICESETTINGSTEST result=pass" -TimeoutSeconds 45),
    (New-RegressionCase -Name "backup and restore" -Command "BACKUPTEST" -CompletionPattern "^BACKUPTEST result=" -PassPattern "^BACKUPTEST result=pass" -TimeoutSeconds 90),
    (New-RegressionCase -Name "offline tools" -Command "OFFLINETEST" -CompletionPattern "^OFFLINETEST result=" -PassPattern "^OFFLINETEST result=pass$" -TimeoutSeconds 20),
    (New-RegressionCase -Name "SSH runtime" -Command "SSHCHECK" -CompletionPattern "^SSHCHECK result=" -PassPattern "^SSHCHECK result=pass" -TimeoutSeconds 20),
    (New-RegressionCase -Name "SSH profile storage" -Command "SSHPROFILETEST" -CompletionPattern "^SSHPROFILETEST result=" -PassPattern "^SSHPROFILETEST result=pass" -TimeoutSeconds 45),
    (New-RegressionCase -Name "speaker hardware" -Command "TTSHW" -CompletionPattern "^TTSHW result=" -PassPattern "^TTSHW result=pass$" -TimeoutSeconds 20),
    (New-RegressionCase -Name "web console start" -Command "CONSOLE" -CompletionPattern "^WEB_CONSOLE result=ready" -PassPattern "^WEB_CONSOLE result=ready" -TimeoutSeconds 20),
    (New-RegressionCase -Name "web console status" -Command "STATUS" -CompletionPattern "^WEB_CONSOLE status=" -PassPattern "^WEB_CONSOLE status=ready authenticated=no" -TimeoutSeconds 15),
    (New-RegressionCase -Name "web console exit" -Command "EXIT" -CompletionPattern "^WEB_CONSOLE result=stopped" -PassPattern "^WEB_CONSOLE result=stopped$" -TimeoutSeconds 20),
    (New-RegressionCase -Name "post-console responsiveness" -Command "STATUS" -CompletionPattern "^STATUS version=" -PassPattern "board_adv=yes.*microsd=ready.*chats=ready.*files=ready" -TimeoutSeconds 15)
)

$onlineCases = @(
    (New-RegressionCase -Name "chat API" -Command "APITEST" -CompletionPattern "^APITEST result=" -PassPattern "^APITEST result=pass" -TimeoutSeconds 120),
    (New-RegressionCase -Name "model file tool" -Command "TOOLTEST" -CompletionPattern "^TOOLTEST result=" -PassPattern "^TOOLTEST result=pass" -TimeoutSeconds 180),
    (New-RegressionCase -Name "web search API" -Command "WEBTEST" -CompletionPattern "^WEBTEST result=" -PassPattern "^WEBTEST result=pass$" -TimeoutSeconds 120),
    (New-RegressionCase -Name "web contents API" -Command "FETCHTEST" -CompletionPattern "^FETCHTEST result=" -PassPattern "^FETCHTEST result=pass$" -TimeoutSeconds 120),
    (New-RegressionCase -Name "search sources cache" -Command "SEARCHCACHETEST" -CompletionPattern "^SEARCHCACHETEST result=" -PassPattern "^SEARCHCACHETEST result=pass" -TimeoutSeconds 30),
    (New-RegressionCase -Name "model search tool round trip" -Command "SEARCHTEST" -CompletionPattern "^SEARCHTEST result=" -PassPattern "^SEARCHTEST result=pass.*search_called=yes" -TimeoutSeconds 240),
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

$resolvedLogPath = [System.IO.Path]::GetFullPath($LogPath)
$logDirectory = [System.IO.Path]::GetDirectoryName($resolvedLogPath)
if (-not [string]::IsNullOrEmpty($logDirectory)) {
    [System.IO.Directory]::CreateDirectory($logDirectory) | Out-Null
}
Set-Content -LiteralPath $resolvedLogPath -Value ("CARDMIND_REGRESSION suite={0} port={1} started={2:o}" -f $Suite, $Port, [DateTime]::UtcNow)

$serial = [System.IO.Ports.SerialPort]::new($Port, $BaudRate, [System.IO.Ports.Parity]::None, 8, [System.IO.Ports.StopBits]::One)
$serial.NewLine = "`r`n"
$serial.DtrEnable = $true
$serial.RtsEnable = $true
$serial.ReadTimeout = 250
$serial.WriteTimeout = 2000

try {
    $serial.Open()
    Start-Sleep -Seconds 12
    $serial.ReadExisting() | Out-Null
    $cases = [System.Collections.Generic.List[object]]::new()
    $cases.AddRange([object[]]$offlineCases)
    if ($Suite -eq "full") {
        $cases.AddRange([object[]]$onlineCases)
    }
    foreach ($case in $cases) {
        Invoke-RegressionCase -Serial $serial -Case $case -LogPath $resolvedLogPath
    }
    Add-Content -LiteralPath $resolvedLogPath -Value ("CARDMIND_REGRESSION result=pass completed={0:o}" -f [DateTime]::UtcNow)
    Write-Host ("CARDMIND_REGRESSION result=pass cases={0} log={1}" -f $cases.Count, $resolvedLogPath)
} finally {
    if ($serial.IsOpen) {
        $serial.Close()
    }
    $serial.Dispose()
}
