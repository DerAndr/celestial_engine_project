# Compile a native C check against the actual firmware sources.
param([switch]$RingExperiment, [switch]$RingDiagnostic)
if ($RingDiagnostic) { $RingExperiment = $true }
$ErrorActionPreference = 'Stop'
$audioRoot = Split-Path -Parent $PSScriptRoot
$audioBuild = Join-Path $audioRoot $(if ($RingExperiment) { 'firmware/build/ring-check' } else { 'firmware/build/audio-check' })
$audioInclude = Join-Path $audioBuild 'include'
$vswherePath = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
if (-not (Test-Path -LiteralPath $vswherePath)) { throw 'Installed Visual Studio C Build Tools are required.' }
$vsInstall = (& $vswherePath -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath).Trim()
if (-not $vsInstall) { throw 'Installed Visual Studio C Build Tools were not found.' }
$devCmd = Join-Path $vsInstall 'Common7/Tools/VsDevCmd.bat'
$savedPath = $env:PATH
$savedInclude = $env:INCLUDE
$savedLib = $env:LIB
try {
    $envLines = & cmd.exe /d /c "call `"$devCmd`" -arch=x64 -no_logo >nul && set"
    if ($LASTEXITCODE -ne 0) { throw 'Could not initialize MSVC environment.' }
    foreach ($line in $envLines) {
        if ($line -match '^(PATH|INCLUDE|LIB)=(.*)$') {
            [Environment]::SetEnvironmentVariable($matches[1], $matches[2], 'Process')
        }
    }
    foreach ($header in @('pico/time.h', 'pico/stdlib.h', 'pico/bootrom.h', 'pico/cyw43_arch.h', 'hardware/gpio.h', 'hardware/i2c.h', 'hardware/pio.h', 'hardware/dma.h', 'hardware/clocks.h', 'hardware/sync.h', 'audio_i2s.pio.h', 'mic_i2s.pio.h', 'ws2812.pio.h')) {
        $headerPath = Join-Path $audioInclude $header
        New-Item -ItemType Directory -Force -Path (Split-Path -Parent $headerPath) | Out-Null
        Set-Content -LiteralPath $headerPath -Value '#include "pico_stubs.h"' -Encoding ASCII
    }
    Push-Location $audioBuild
    try {
        $checkSource = if ($RingExperiment) { 'tests/ring_experiment_check.c' } else { 'tests/audio_signal_check.c' }
        $checkDefines = @()
        if ($RingExperiment) { $checkDefines += '/DCELESTIAL_FOUR_RINGS=1' }
        if ($RingDiagnostic) { $checkDefines += '/DCELESTIAL_RING_EXPERIMENT=1' }
        & cl.exe /nologo /std:c11 /O2 @checkDefines "/I$audioInclude" "/I$(Join-Path $audioRoot 'tests')" "/I$(Join-Path $audioRoot 'firmware/src')" (Join-Path $audioRoot $checkSource) /Fe:audio_signal_check.exe /Fo:audio_signal_check.obj
        if ($LASTEXITCODE -ne 0) { throw 'C audio check did not compile.' }
        & .\audio_signal_check.exe
        if ($LASTEXITCODE -ne 0) { throw 'C audio signal check failed.' }
        & cl.exe /nologo /std:c11 /O2 (Join-Path $audioRoot 'tests/net_response_check.c') /Fe:net_response_check.exe /Fo:net_response_check.obj
        if ($LASTEXITCODE -ne 0) { throw 'Response gate check did not compile.' }
        & .\net_response_check.exe
        if ($LASTEXITCODE -ne 0) { throw 'Response gate check failed.' }
    } finally { Pop-Location }
} finally {
    $env:PATH = $savedPath
    $env:INCLUDE = $savedInclude
    $env:LIB = $savedLib
}
