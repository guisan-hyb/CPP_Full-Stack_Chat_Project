$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$runRoot = Join-Path $root 'perf-run\windows'

foreach ($name in 'gate', 'chat2', 'chat1', 'status') {
    $pidFile = Join-Path (Join-Path $runRoot $name) 'service.pid'
    if (-not (Test-Path -LiteralPath $pidFile)) {
        continue
    }

    $servicePid = Get-Content -LiteralPath $pidFile -ErrorAction SilentlyContinue
    if ($servicePid) {
        Stop-Process -Id ([int]$servicePid) -Force -ErrorAction SilentlyContinue
    }
    Remove-Item -LiteralPath $pidFile -Force -ErrorAction SilentlyContinue
}

Write-Output 'Windows performance services stopped.'
