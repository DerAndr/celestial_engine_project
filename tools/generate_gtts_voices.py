from gtts import gTTS
import subprocess
import os
import wave
import struct

phrases = [
    ("VOICE_ONLINE", "Celestial Engine online.", "CELESTIAL ONLINE"),
    ("VOICE_READY", "System ready.", "SYSTEM READY"),
    ("VOICE_ORACLE", "Voice Oracle activated.", "ORACLE ACTIVATED"),
    ("VOICE_WARNING", "Warning. Anomaly detected.", "ANOMALY DETECTED"),
]

os.makedirs("tools/gtts_audio", exist_ok=True)

for code, text, desc in phrases:
    mp3_file = f"tools/gtts_audio/{code}.mp3"
    wav_file = f"tools/gtts_audio/{code}.wav"
    
    # Generate high quality audio via Google UK Voice
    tts = gTTS(text=text, lang='en', tld='co.uk')
    tts.save(mp3_file)
    
    # Process with ffmpeg:
    # 1. highpass=140Hz to eliminate muddy low end
    # 2. lowpass=7000Hz for warm broadcast sound
    # 3. volume=-4dB to prevent any clipping
    # 4. resample to 16,000 Hz 16-bit mono
    cmd = [
        "ffmpeg", "-y", "-i", mp3_file,
        "-af", "highpass=f=140,lowpass=f=7000,volume=-4dB",
        "-ar", "16000", "-ac", "1", "-c:a", "pcm_s16le", wav_file
    ]
    subprocess.run(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=True)
    
    with wave.open(wav_file, 'rb') as w:
        print(f"Generated {code} ({desc}): {w.getnframes()} frames, {w.getframerate()} Hz, duration: {w.getnframes()/16000:.2f}s")

print("All Google voice samples generated successfully!")
