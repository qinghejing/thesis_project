$ErrorActionPreference = "Stop"

$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
$LogDir = Join-Path $Root "run-logs"
New-Item -ItemType Directory -Force -Path $LogDir | Out-Null

function Start-IfMissing {
    param(
        [string]$Name,
        [string]$Exe,
        [string]$WorkingDirectory,
        [int]$Port,
        [string[]]$Arguments = @()
    )

    if (Get-NetTCPConnection -LocalPort $Port -State Listen -ErrorAction SilentlyContinue) {
        Write-Host "$Name already listening on $Port"
        return
    }

    $StartParams = @{
        FilePath = $Exe
        WorkingDirectory = $WorkingDirectory
        RedirectStandardOutput = (Join-Path $LogDir "$Name.out.log")
        RedirectStandardError = (Join-Path $LogDir "$Name.err.log")
        WindowStyle = "Hidden"
    }
    if ($Arguments.Count -gt 0) {
        $StartParams.ArgumentList = $Arguments
    }

    Start-Process @StartParams | Out-Null
    Start-Sleep -Seconds 2
}

$RedisDir = "C:\Users\sadliu\Downloads\Redis-x64-5.0.14.1"
Start-IfMissing "Redis" (Join-Path $RedisDir "redis-server.exe") $RedisDir 6380 @("--port", "6380", "--bind", "127.0.0.1", "--requirepass", "123456")

Start-IfMissing "VarifyServer" "D:\nodejsdownload\node.exe" (Join-Path $Root "server\VarifyServer") 50051 @("server.js")
Start-IfMissing "StatusServer" (Join-Path $Root "server\StatusServer\x64\Debug\StatusServer.exe") (Join-Path $Root "server\StatusServer\x64\Debug") 50052
Start-IfMissing "ChatServer1" (Join-Path $Root "server\ChatServer\x64\Debug\ChatServer.exe") (Join-Path $Root "server\ChatServer\x64\Debug") 8090
Start-IfMissing "ChatServer2" (Join-Path $Root "server\ChatServer2\x64\Debug\ChatServer.exe") (Join-Path $Root "server\ChatServer2\x64\Debug") 8091
Start-IfMissing "GateServer" (Join-Path $Root "server\GateServer\x64\Debug\GateServer.exe") (Join-Path $Root "server\GateServer\x64\Debug") 8080

foreach ($Port in 6380, 50051, 50052, 50055, 50056, 8090, 8091, 8080) {
    $Conn = Get-NetTCPConnection -LocalPort $Port -State Listen -ErrorAction SilentlyContinue
    if ($Conn) {
        Write-Host "LISTEN $Port PID=$($Conn.OwningProcess -join ',')"
    } else {
        Write-Host "MISSING $Port"
    }
}
