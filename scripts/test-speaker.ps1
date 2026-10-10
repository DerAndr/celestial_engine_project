param(
    [Parameter(Mandatory = $true)][string]$Port,
    [ValidateRange(1, 10)][int]$Volume = 1
)
# Run only after flashing 0.7.2-AUDIO-DIAG. Close other users of the COM port first.
$ErrorActionPreference = 'Stop'
$speakerSerial = [System.IO.Ports.SerialPort]::new($Port, 115200)
$speakerSerial.DtrEnable = $true
$speakerSerial.RtsEnable = $true
$speakerSerial.WriteTimeout = 1000
function Read-SpeakerLog([double]$Seconds) {
    $speakerTimer = [Diagnostics.Stopwatch]::StartNew()
    $speakerText = ''
    while ($speakerTimer.Elapsed.TotalSeconds -lt $Seconds) {
        if ($speakerSerial.BytesToRead -gt 0) {
            $speakerChunk = $speakerSerial.ReadExisting()
            $speakerText += $speakerChunk
            Write-Host $speakerChunk -NoNewline
        }
        Start-Sleep -Milliseconds 50
    }
    return $speakerText
}
try {
    $speakerSerial.Open()
    Write-Host 'Silence check: V0. If loud noise remains, disconnect USB and stop.'
    $speakerSerial.Write("S`nV0`nA`n")
    $speakerLog = Read-SpeakerLog 1.5
    if ($speakerLog -notmatch 'FW:0\.[78]\.[0-9].*') {
        throw 'Diagnostic firmware version was not received. Stop and verify flashing/COM port before playing sound.'
    }
    $speakerAnswer = Read-Host 'Was V0 silent (no loud buzz/crunch)? Enter YES to play one test tone'
    if ($speakerAnswer -ne 'YES') { Write-Host 'Stopped before sound test.'; return }
    Write-Host "One second of 440 Hz sine at V$Volume. Acoustic level is unverified; disconnect USB if uncomfortable."
    $speakerSerial.Write("V$Volume`nD`n")
    $null = Read-SpeakerLog 2
    $speakerSerial.Write("A`n")
    $null = Read-SpeakerLog 0.5
} finally {
    if ($speakerSerial.IsOpen) {
        try { $speakerSerial.Write("S`nV0`n") } catch { Write-Warning 'Could not send mute; disconnect USB if output is uncomfortable.' }
        $speakerSerial.Close()
    }
    $speakerSerial.Dispose()
}
