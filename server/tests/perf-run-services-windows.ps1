$ErrorActionPreference = 'Stop'

$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$buildRoot = Join-Path $root 'build\server'
$fixtureRoot = Join-Path $PSScriptRoot 'perf-config-windows'
$runRoot = Join-Path $root 'perf-run\windows'

& (Join-Path $PSScriptRoot 'perf-stop-services-windows.ps1') | Out-Null

$services = @(
    @{ Name = 'status'; Exe = 'StatusServer\Release\StatusServer.exe'; Config = 'status.ini'; Port = 50052 },
    @{ Name = 'chat1'; Exe = 'ChatServer\Release\ChatServer.exe'; Config = 'chat1.ini'; Port = 8990 },
    @{ Name = 'chat2'; Exe = 'ChatServer2\Release\ChatServer2.exe'; Config = 'chat2.ini'; Port = 8991 },
    @{ Name = 'gate'; Exe = 'GateServer\Release\GateServer.exe'; Config = 'gate.ini'; Port = 8080 }
)

foreach ($service in $services) {
    $workdir = Join-Path $runRoot $service.Name
    New-Item -ItemType Directory -Path $workdir -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $fixtureRoot $service.Config) `
        -Destination (Join-Path $workdir 'config.ini') -Force

    $stdout = Join-Path $workdir 'stdout.log'
    $stderr = Join-Path $workdir 'stderr.log'
    $process = Start-Process `
        -FilePath (Join-Path $buildRoot $service.Exe) `
        -WorkingDirectory $workdir `
        -RedirectStandardOutput $stdout `
        -RedirectStandardError $stderr `
        -WindowStyle Hidden `
        -PassThru
    Set-Content -LiteralPath (Join-Path $workdir 'service.pid') -Value $process.Id

    $ready = $false
    for ($attempt = 0; $attempt -lt 50; $attempt += 1) {
        $client = [System.Net.Sockets.TcpClient]::new()
        try {
            $task = $client.ConnectAsync('127.0.0.1', $service.Port)
            if ($task.Wait(100) -and $client.Connected) {
                $ready = $true
                break
            }
        } catch {
        } finally {
            $client.Dispose()
        }
        if ($process.HasExited) { break }
        Start-Sleep -Milliseconds 200
    }

    if (-not $ready) {
        Get-Content -LiteralPath $stdout -Tail 80 -ErrorAction SilentlyContinue
        Get-Content -LiteralPath $stderr -Tail 80 -ErrorAction SilentlyContinue
        throw "$($service.Name) did not listen on port $($service.Port)"
    }
}

Write-Output 'Windows performance services are ready.'
