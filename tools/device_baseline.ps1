param(
    [Parameter(Mandatory = $true)]
    [string]$Port,

    [Parameter(Mandatory = $true)]
    [ValidateRange(9600, 921600)]
    [int]$BaudRate,

    [Parameter(Mandatory = $true)]
    [ValidateRange(1, 100)]
    [int]$ConsoleCycles,

    [Parameter(Mandatory = $true)]
    [string]$BudgetPath,

    [Parameter(Mandatory = $true)]
    [string]$OutputPath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

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

function Assert-SafeDeviceLine {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Line,

        [Parameter(Mandatory = $true)]
        [string]$Operation
    )

    if ($Line -match "Guru Meditation|Brownout|abort\(\)|ESP-ROM:esp32s3|rst:0x") {
        throw "Device reset or panic during '$Operation': $Line"
    }
}

function Sync-SerialChannel {
    param(
        [Parameter(Mandatory = $true)]
        [System.IO.Ports.SerialPort]$Serial,

        [Parameter(Mandatory = $true)]
        [string]$Operation
    )

    for ($attempt = 1; $attempt -le 3; $attempt++) {
        $Serial.ReadExisting() | Out-Null
        $Serial.WriteLine("PING")
        $Serial.BaseStream.Flush()
        $pending = ""
        $stopwatch = [System.Diagnostics.Stopwatch]::StartNew()
        while ($stopwatch.Elapsed.TotalSeconds -lt 3) {
            Start-Sleep -Milliseconds 40
            $read = Read-SerialLines -Serial $Serial -Pending $pending
            $pending = $read.Pending
            foreach ($line in $read.Lines) {
                if ($line.Length -eq 0) {
                    continue
                }
                Assert-SafeDeviceLine -Line $line -Operation $Operation
                if ($line -eq "PONG") {
                    return
                }
            }
        }
    }
    throw "Serial channel did not answer PING before '$Operation'"
}

function Invoke-TimedSerialCommand {
    param(
        [Parameter(Mandatory = $true)]
        [System.IO.Ports.SerialPort]$Serial,

        [Parameter(Mandatory = $true)]
        [string]$Name,

        [Parameter(Mandatory = $true)]
        [string]$Command,

        [Parameter(Mandatory = $true)]
        [string]$CompletionPattern,

        [Parameter(Mandatory = $true)]
        [string]$PassPattern,

        [Parameter(Mandatory = $true)]
        [ValidateRange(1, 120)]
        [int]$TimeoutSeconds
    )

    Sync-SerialChannel -Serial $Serial -Operation $Name
    $Serial.ReadExisting() | Out-Null
    $Serial.WriteLine($Command)
    $Serial.BaseStream.Flush()
    $pending = ""
    $stopwatch = [System.Diagnostics.Stopwatch]::StartNew()
    while ($stopwatch.Elapsed.TotalSeconds -lt $TimeoutSeconds) {
        Start-Sleep -Milliseconds 40
        $read = Read-SerialLines -Serial $Serial -Pending $pending
        $pending = $read.Pending
        foreach ($line in $read.Lines) {
            if ($line.Length -eq 0) {
                continue
            }
            Assert-SafeDeviceLine -Line $line -Operation $Name
            if ($line -match $CompletionPattern) {
                if ($line -notmatch $PassPattern) {
                    throw "Device command '$Name' failed: $line"
                }
                return [pscustomobject]@{
                    Line = $line
                    LatencyMs = [int][Math]::Round($stopwatch.Elapsed.TotalMilliseconds)
                }
            }
        }
    }
    throw "Timed out after $TimeoutSeconds seconds waiting for '$Name'"
}

function Convert-StatusLine {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Line
    )

    $allowedTextFields = @(
        "version", "board_adv", "microsd", "chats", "files", "wifi",
        "charging", "reset_reason"
    )
    $allowedNumericFields = @(
        "battery", "history", "heap", "largest_heap", "min_heap",
        "stack_free", "cpu_mhz"
    )
    $status = [ordered]@{}
    foreach ($match in [regex]::Matches($Line, "(?<key>[a-z_]+)=(?<value>[^\s]+)")) {
        $key = $match.Groups["key"].Value
        $value = $match.Groups["value"].Value
        if ($allowedTextFields -contains $key) {
            $status[$key] = $value
        } elseif ($allowedNumericFields -contains $key) {
            $parsed = 0L
            if (-not [long]::TryParse($value, [ref]$parsed)) {
                throw "STATUS field '$key' is not numeric: $value"
            }
            $status[$key] = $parsed
        }
    }
    foreach ($required in @("version", "board_adv", "microsd", "heap", "min_heap", "stack_free")) {
        if (-not $status.Contains($required)) {
            throw "STATUS response is missing required field '$required'"
        }
    }
    return [pscustomobject]$status
}

function Get-RequiredJsonNumber {
    param(
        [Parameter(Mandatory = $true)]
        [pscustomobject]$Object,

        [Parameter(Mandatory = $true)]
        [string]$Field
    )

    $property = $Object.PSObject.Properties[$Field]
    if (
        $null -eq $property -or
        $property.Value -isnot [ValueType] -or
        $property.Value -is [bool]
    ) {
        throw "Device budget field '$Field' must be numeric"
    }
    $value = [long]$property.Value
    if ($value -lt 0) {
        throw "Device budget field '$Field' must be non-negative"
    }
    return $value
}

function Read-DeviceLimits {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path
    )

    $resolvedPath = [System.IO.Path]::GetFullPath($Path)
    if (-not [System.IO.File]::Exists($resolvedPath)) {
        throw "Device budget does not exist: $resolvedPath"
    }
    $document = Get-Content -Raw -LiteralPath $resolvedPath | ConvertFrom-Json
    if ($document.schema_version -ne 1 -or $null -eq $document.limits) {
        throw "Device budget must use schema_version 1 and contain limits"
    }
    return [pscustomobject]@{
        PostCycleHeapMinimum = Get-RequiredJsonNumber -Object $document.limits -Field "post_cycle_heap_bytes_minimum"
        LargestHeapBlockMinimum = Get-RequiredJsonNumber -Object $document.limits -Field "largest_heap_block_bytes_minimum"
        MinimumHeap = Get-RequiredJsonNumber -Object $document.limits -Field "minimum_heap_bytes"
        StackFreeMinimum = Get-RequiredJsonNumber -Object $document.limits -Field "stack_free_bytes_minimum"
        WarmHeapLossMaximum = Get-RequiredJsonNumber -Object $document.limits -Field "post_warmup_heap_loss_bytes_maximum"
        CommandLatencyMaximum = Get-RequiredJsonNumber -Object $document.limits -Field "command_latency_ms_maximum"
    }
}

$resolvedOutputPath = [System.IO.Path]::GetFullPath($OutputPath)
$outputDirectory = [System.IO.Path]::GetDirectoryName($resolvedOutputPath)
if (-not [string]::IsNullOrEmpty($outputDirectory)) {
    [System.IO.Directory]::CreateDirectory($outputDirectory) | Out-Null
}

$serial = [System.IO.Ports.SerialPort]::new(
    $Port,
    $BaudRate,
    [System.IO.Ports.Parity]::None,
    8,
    [System.IO.Ports.StopBits]::One
)
$serial.NewLine = "`r`n"
$serial.DtrEnable = $false
$serial.RtsEnable = $false
$serial.ReadTimeout = 250
$serial.WriteTimeout = 2000
$startedAt = [DateTime]::UtcNow
$cycles = [System.Collections.Generic.List[object]]::new()
$limits = Read-DeviceLimits -Path $BudgetPath

try {
    $serial.Open()
    Start-Sleep -Seconds 2
    $serial.ReadExisting() | Out-Null

    $initialParameters = @{
        Serial = $serial
        Name = "initial status"
        Command = "STATUS"
        CompletionPattern = "^STATUS version="
        PassPattern = "board_adv=yes.*microsd=ready"
        TimeoutSeconds = 15
    }
    $initialResult = Invoke-TimedSerialCommand @initialParameters
    $initialStatus = Convert-StatusLine -Line $initialResult.Line

    for ($cycle = 1; $cycle -le $ConsoleCycles; $cycle++) {
        $startParameters = @{
            Serial = $serial
            Name = "web console start cycle $cycle"
            Command = "CONSOLE"
            CompletionPattern = "^WEB_CONSOLE result="
            PassPattern = "^WEB_CONSOLE result=ready"
            TimeoutSeconds = 20
        }
        $startResult = Invoke-TimedSerialCommand @startParameters
        $webStatusParameters = @{
            Serial = $serial
            Name = "web console status cycle $cycle"
            Command = "STATUS"
            CompletionPattern = "^WEB_CONSOLE status="
            PassPattern = "^WEB_CONSOLE status=ready"
            TimeoutSeconds = 15
        }
        $webStatusResult = Invoke-TimedSerialCommand @webStatusParameters
        $stopParameters = @{
            Serial = $serial
            Name = "web console stop cycle $cycle"
            Command = "EXIT"
            CompletionPattern = "^WEB_CONSOLE result="
            PassPattern = "^WEB_CONSOLE result=stopped$"
            TimeoutSeconds = 20
        }
        $stopResult = Invoke-TimedSerialCommand @stopParameters
        $statusParameters = @{
            Serial = $serial
            Name = "post-console status cycle $cycle"
            Command = "STATUS"
            CompletionPattern = "^STATUS version="
            PassPattern = "board_adv=yes.*microsd=ready"
            TimeoutSeconds = 15
        }
        $statusResult = Invoke-TimedSerialCommand @statusParameters
        $cycles.Add([pscustomobject]@{
            cycle = $cycle
            console_start_ms = $startResult.LatencyMs
            console_status_ms = $webStatusResult.LatencyMs
            console_stop_ms = $stopResult.LatencyMs
            post_status_ms = $statusResult.LatencyMs
            status = Convert-StatusLine -Line $statusResult.Line
        })
        Write-Host ("DEVICE_BASELINE cycle={0} result=pass" -f $cycle)
    }

    $postCycleHeaps = @($cycles | ForEach-Object { [long]$_.status.heap })
    $largestHeapBlocks = @($cycles | ForEach-Object { [long]$_.status.largest_heap })
    $minimumHeaps = @($cycles | ForEach-Object { [long]$_.status.min_heap })
    $stackFreeValues = @($cycles | ForEach-Object { [long]$_.status.stack_free })
    $latencies = @(
        $initialResult.LatencyMs
        $cycles | ForEach-Object {
            $_.console_start_ms
            $_.console_status_ms
            $_.console_stop_ms
            $_.post_status_ms
        }
    )
    $warmupHeap = $postCycleHeaps[0]
    $postWarmupHeaps = if ($postCycleHeaps.Count -gt 1) {
        @($postCycleHeaps[1..($postCycleHeaps.Count - 1)])
    } else {
        @($warmupHeap)
    }
    $summary = [ordered]@{
        post_cycle_heap_bytes_minimum = [long]($postCycleHeaps | Measure-Object -Minimum).Minimum
        largest_heap_block_bytes_minimum = [long]($largestHeapBlocks | Measure-Object -Minimum).Minimum
        minimum_heap_bytes = [long]($minimumHeaps | Measure-Object -Minimum).Minimum
        stack_free_bytes_minimum = [long]($stackFreeValues | Measure-Object -Minimum).Minimum
        post_warmup_heap_loss_bytes = [long]($warmupHeap - ($postWarmupHeaps | Measure-Object -Minimum).Minimum)
        command_latency_ms_maximum = [long]($latencies | Measure-Object -Maximum).Maximum
    }
    $failures = [System.Collections.Generic.List[string]]::new()
    if ($summary.post_cycle_heap_bytes_minimum -lt $limits.PostCycleHeapMinimum) {
        $failures.Add("Post-cycle heap is below the configured minimum")
    }
    if ($summary.largest_heap_block_bytes_minimum -lt $limits.LargestHeapBlockMinimum) {
        $failures.Add("Largest heap block is below the configured minimum")
    }
    if ($summary.minimum_heap_bytes -lt $limits.MinimumHeap) {
        $failures.Add("Reported minimum heap is below the configured minimum")
    }
    if ($summary.stack_free_bytes_minimum -lt $limits.StackFreeMinimum) {
        $failures.Add("Stack headroom is below the configured minimum")
    }
    if ($summary.post_warmup_heap_loss_bytes -gt $limits.WarmHeapLossMaximum) {
        $failures.Add("Post-warmup heap loss exceeds the configured maximum")
    }
    if ($summary.command_latency_ms_maximum -gt $limits.CommandLatencyMaximum) {
        $failures.Add("Serial command latency exceeds the configured maximum")
    }

    $report = [ordered]@{
        schema_version = 1
        result = if ($failures.Count -eq 0) { "pass" } else { "fail" }
        started_at_utc = $startedAt.ToString("o")
        completed_at_utc = [DateTime]::UtcNow.ToString("o")
        port = $Port
        baud_rate = $BaudRate
        console_cycles = $ConsoleCycles
        initial_status_latency_ms = $initialResult.LatencyMs
        initial_status = $initialStatus
        summary = $summary
        failures = $failures
        cycles = $cycles
    }
    $report | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $resolvedOutputPath -Encoding utf8
    if ($failures.Count -gt 0) {
        throw ("Device baseline failed: " + ($failures -join "; "))
    }
    Write-Host ("DEVICE_BASELINE result=pass cycles={0} output={1}" -f $ConsoleCycles, $resolvedOutputPath)
} finally {
    if ($serial.IsOpen) {
        $serial.Close()
    }
    $serial.Dispose()
}
