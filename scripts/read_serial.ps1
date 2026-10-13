param([string]$Port = "COM9", [int]$Seconds = 3)
try {
    $sp = [System.IO.Ports.SerialPort]::new($Port, 115200)
    $sp.ReadTimeout = 1000
    $sp.DtrEnable = $true
    $sp.RtsEnable = $true
    $sp.Open()
    $sp.Write("?")
    Start-Sleep -Milliseconds 100
    $end = (Get-Date).AddSeconds($Seconds)
    while ((Get-Date) -lt $end) {
        try {
            $line = $sp.ReadLine()
            Write-Host $line
        } catch [System.TimeoutException] { }
    }
    $sp.Close()
} catch {
    Write-Host "Error opening $Port $($_.Exception.Message)"
}
