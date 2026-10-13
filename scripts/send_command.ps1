param([string]$Port = "COM9", [string]$Command = "?")
try {
    $sp = [System.IO.Ports.SerialPort]::new($Port, 115200)
    $sp.DtrEnable = $true
    $sp.RtsEnable = $true
    $sp.ReadTimeout = 1000
    $sp.Open()
    $sp.Write($Command)
    Start-Sleep -Milliseconds 300
    for ($i = 0; $i -lt 5; $i++) {
        try {
            $line = $sp.ReadLine()
            Write-Host $line
        } catch [System.TimeoutException] { break }
    }
    $sp.Close()
} catch {
    Write-Host "Error communicating with $Port $($_.Exception.Message)"
}
