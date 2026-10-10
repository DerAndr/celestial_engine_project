import asyncio
import edge_tts
import subprocess
import os
import wave
import struct

phrases = [
    ("VOICE_ONLINE", "Celestial Engine online.", "en-US-AriaNeural", "CELESTIAL ONLINE"),
    ("VOICE_READY", "System ready.", "en-US-AriaNeural", "SYSTEM READY"),
    ("VOICE_ORACLE", "Voice Oracle activated.", "en-US-AriaNeural", "ORACLE ACTIVATED"),
    ("VOICE_WARNING", "Warning. Anomaly detected.", "en-US-AriaNeural", "ANOMALY DETECTED"),
]

async def gen_all():
    os.makedirs("tools/neural_raw", exist_ok=True)
    for code, text, voice, desc in phrases:
        raw_mp3 = f"tools/neural_raw/{code}.mp3"
        print(f"Generating {code} with {voice}...")
        comm = edge_tts.Communicate(text, voice, rate="+5%", pitch="+0Hz")
        await comm.save(raw_mp3)
        
        # Use FFmpeg to convert to 16kHz 16-bit mono WAV with studio audio cleaning:
        # highpass=f=150, lowpass=f=7000, loudnorm
        out_wav = f"tools/neural_raw/{code}.wav"
        cmd = [
            "ffmpeg", "-y", "-i", raw_mp3,
            "-af", "highpass=f=140,lowpass=f=7500,volume=-3dB",
            "-ar", "16000", "-ac", "1", "-c:a", "pcm_s16le", out_wav
        ]
        subprocess.run(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=True)
        print(f"Processed {out_wav}")

asyncio.run(gen_all())
