param(
    [string]$Port = "COM9",
    [int]$BaudRate = 115200
)

Write-Host "Opening serial connection to $Port at $BaudRate baud..." -ForegroundColor Cyan
Write-Host "Press Ctrl+C to exit." -ForegroundColor Yellow
Write-Host ""

try {
    $sp = [System.IO.Ports.SerialPort]::new($Port, $BaudRate)
    $sp.DtrEnable = $true
    $sp.RtsEnable = $true
    $sp.ReadTimeout = 2000
    $sp.Open()

    while ($true) {
        try {
            $line = $sp.ReadLine()
            Write-Host $line
        } catch [System.TimeoutException] {
            # Timeout is normal when waiting for next line
        }
    }
} catch {
    Write-Host "Error accessing $Port : $_" -ForegroundColor Red
} finally {
    if ($sp -and $sp.IsOpen) {
        $sp.Close()
        Write-Host "`nSerial port $Port closed." -ForegroundColor Green
    }
}
