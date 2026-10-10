import subprocess
import os
import wave
import struct

ps_code = """
Add-Type -AssemblyName System.Speech
$voice = New-Object System.Speech.Synthesis.SpeechSynthesizer
$voice.Rate = 0
$voice.SetOutputToWaveFile('sample.wav')
$voice.Speak('Celestial Engine online')
$voice.Dispose()
"""

with open('synth.ps1', 'w', encoding='utf-8') as f:
    f.write(ps_code)

try:
    subprocess.run(['powershell', '-ExecutionPolicy', 'Bypass', '-File', 'synth.ps1'], check=True)
    if os.path.exists('sample.wav'):
        with wave.open('sample.wav', 'rb') as w:
            print(f"Sample WAV generated: {w.getnchannels()} ch, {w.getframerate()} Hz, {w.getnframes()} frames, {w.getsampwidth()} bytes/sample")
        os.remove('sample.wav')
finally:
    if os.path.exists('synth.ps1'):
        os.remove('synth.ps1')
