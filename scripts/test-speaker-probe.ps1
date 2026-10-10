param(
    [Parameter(Mandatory = $true)][string]$Port
)
# This first experiment sends only U (positive PCM, amplitude 8). No music/encoder.
$ErrorActionPreference = 'Stop'
$probeSerial = [System.IO.Ports.SerialPort]::new($Port, 115200)
$probeSerial.DtrEnable = $true
$probeSerial.RtsEnable = $true
$probeSerial.WriteTimeout = 1000
function Read-ProbeLog([double]$Seconds) {
    $probeTimer = [Diagnostics.Stopwatch]::StartNew()
    $probeText = ''
    while ($probeTimer.Elapsed.TotalSeconds -lt $Seconds) {
        if ($probeSerial.BytesToRead -gt 0) {
            $probeChunk = $probeSerial.ReadExisting()
            $probeText += $probeChunk
            Write-Host $probeChunk -NoNewline
        }
        Start-Sleep -Milliseconds 20
    }
    return $probeText
}
try {
    $probeWiring = Read-Host 'Power off and verify ONE 4-ohm speaker connected only between amplifier OUT+ and OUT-, neither wire to GND. Reconnect USB, then enter YES'
    if ($probeWiring -ne 'YES') { Write-Host 'Stopped before opening port or sending tone.'; return }
    $probeSerial.Open()
    $probeSerial.Write("S`nA`n")
    $probeLog = Read-ProbeLog 2.5
    if ($probeLog -notmatch 'FW:CELESTIAL-SPEAKER-PROBE-02') {
        throw 'The isolated speaker_probe.uf2 is not running. No tone sent.'
    }
    $probeAnswer = Read-Host 'Is startup silent? Enter YES for one 256ms positive-only tone. Disconnect USB if uncomfortable'
    if ($probeAnswer -ne 'YES') { Write-Host 'Stopped before tone.'; return }
    $probeSerial.Write("U`n")
    $probeLog = Read-ProbeLog 1
    if ($probeLog -notmatch '\[PROBE\] POSITIVE' -or $probeLog -notmatch '\[PROBE\] DONE') {
        throw 'No complete probe response received. Stop and inspect the log.'
    }
    Write-Host 'Stop here. Report silence / faint clean tone / loud distortion, plus all PROBE lines. Do not send B or 2 yet.'
} finally {
    if ($probeSerial.IsOpen) {
        try { $probeSerial.Write("S`n") } catch { Write-Warning 'Cannot send stop; disconnect USB if needed.' }
        $probeSerial.Close()
    }
    $probeSerial.Dispose()
}
