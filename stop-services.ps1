$Names = "GateServer", "StatusServer", "ChatServer", "node", "redis-server"

foreach ($Name in $Names) {
    Get-Process -Name $Name -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
}

Write-Host "Services stopped."
