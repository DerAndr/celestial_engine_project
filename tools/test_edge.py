import asyncio
import edge_tts
import os

async def generate():
    communicate = edge_tts.Communicate("Celestial Engine online.", "en-US-AriaNeural")
    await communicate.save("tools/test_aria.mp3")
    print("Aria generated!")

asyncio.run(generate())
