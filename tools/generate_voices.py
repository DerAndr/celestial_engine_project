import subprocess
import os
import wave
import struct

phrases = {
    "engine_online": "Celestial Engine online.",
    "system_ready": "System ready.",
    "oracle_active": "Voice Oracle activated.",
    "warning_anomaly": "Warning. Celestial anomaly detected."
}

# 1. Synthesize via PowerShell SAPI
for name, text in phrases.items():
    wav_path = f"tools/{name}.wav"
    ps = f"""
Add-Type -AssemblyName System.Speech
$synth = New-Object System.Speech.Synthesis.SpeechSynthesizer
$synth.Rate = 0
$synth.SetOutputToWaveFile('{wav_path}')
$synth.Speak('{text}')
$synth.Dispose()
"""
    with open("tools/temp_synth.ps1", "w", encoding="utf-8") as f:
        f.write(ps)
    subprocess.run(["powershell", "-ExecutionPolicy", "Bypass", "-File", "tools/temp_synth.ps1"], check=True)
    if os.path.exists("tools/temp_synth.ps1"):
        os.remove("tools/temp_synth.ps1")
    print(f"Generated {wav_path}")

print("All voices synthesized!")
