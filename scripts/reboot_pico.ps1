param([string]$Port = "COM9")
try {
    $sp = [System.IO.Ports.SerialPort]::new($Port, 115200)
    $sp.Open()
    $sp.Write("B")
    Start-Sleep -Milliseconds 200
    $sp.Close()
    Write-Host "Sent BOOTSEL reboot command 'B' to $Port"
} catch {
    Write-Host "Error communicating with $Port. Details: $($_.Exception.Message)"
}
