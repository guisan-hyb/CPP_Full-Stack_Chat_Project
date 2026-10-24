param(
    [Parameter(Mandatory = $true)]
    [string]$OutputPath
)

$serviceNames = @('GateServer', 'StatusServer', 'ChatServer', 'ChatServer2')
$sample = 0

while ($true) {
    $sample += 1
    $processes = Get-Process -Name $serviceNames -ErrorAction SilentlyContinue
    $row = [ordered]@{
        Timestamp = (Get-Date).ToString('o')
        Sample = $sample
        TotalCpuPercent = $null
    }

    foreach ($name in $serviceNames) {
        $process = $processes | Where-Object { $_.ProcessName -eq $name } | Select-Object -First 1
        $row["${name}CpuSeconds"] = if ($process) { [math]::Round($process.CPU, 3) } else { $null }
        $row["${name}WorkingSetMB"] = if ($process) {
            [math]::Round($process.WorkingSet64 / 1MB, 2)
        } else {
            $null
        }
    }

    try {
        $counter = Get-Counter '\Processor(_Total)\% Processor Time' -MaxSamples 1 -ErrorAction Stop
        $row.TotalCpuPercent = [math]::Round($counter.CounterSamples[0].CookedValue, 2)
    } catch {
    }

    [pscustomobject]$row | Export-Csv -LiteralPath $OutputPath -Append -NoTypeInformation
    Start-Sleep -Seconds 1
}
