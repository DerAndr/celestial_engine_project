#!/usr/bin/env python3
"""
Celestial Engine Mk.V — Mock Brain Server
Emulates Raspberry Pi 5 audio service over UDP for Pico 2 W testing.

Listens on UDP port 5005:
1. Receives streamed 16 kHz 16-bit PCM audio from the staff during QUERY.
2. Accepts a bounded per-request PCM capture.
3. Automatically synthesizes and streams back a response tone/speech WAV.
"""

import socket
import struct
import wave
import math
import time
import os
import sys
from brain_transport import BrainTransport, PORT

def generate_test_response_pcm():
    """Generates a pleasant 16 kHz 16-bit PCM response chime (C5 -> G5 -> C6)."""
    sample_rate = 16000
    duration = 1.0  # 1 second
    num_samples = int(sample_rate * duration)
    samples = []
    
    # Arpeggio: 523 Hz (C5) for 300ms, 784 Hz (G5) for 300ms, 1046 Hz (C6) for 400ms
    for i in range(num_samples):
        t = i / sample_rate
        if t < 0.3:
            freq = 523.25
            env = math.exp(-t * 4.0)
        elif t < 0.6:
            freq = 783.99
            env = math.exp(-(t - 0.3) * 4.0)
        else:
            freq = 1046.50
            env = math.exp(-(t - 0.6) * 3.0)
        
        val = int(8000.0 * env * math.sin(2.0 * math.pi * freq * t))
        val = max(-32768, min(32767, val))
        samples.append(val)
        
    return struct.pack(f"<{len(samples)}h", *samples)

def respond(transport, session):
    pcm = generate_test_response_pcm()
    transport.stream(session, pcm, b"TEST RESPONSE FROM THE BRAIN")

def main():
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind(("0.0.0.0", PORT))
    print(f"Mock brain: UDP protocol v2, port {PORT}; no cloud calls")
    try:
        BrainTransport(sock).serve(respond)
    except KeyboardInterrupt:
        pass
    finally:
        sock.close()

if __name__ == "__main__":
    main()
