#!/usr/bin/env python3
"""
Celestial Engine Mk.V — Mock Brain Server
Emulates Raspberry Pi 5 audio service over UDP for Pico 2 W testing.

Listens on UDP port 5005:
1. Receives streamed 16 kHz 16-bit PCM audio from the staff during QUERY.
2. Saves recorded voice to tools/query.wav.
3. Automatically synthesizes and streams back a response tone/speech WAV.
"""

import socket
import struct
import wave
import math
import time
import os
import sys

MAGIC = 0x454C4543  # 'C' 'E' 'L' 'E'
VERSION = 1
PORT = 5005

MSG_HEARTBEAT   = 0x01
MSG_QUERY_START = 0x02
MSG_AUDIO_CHUNK = 0x03
MSG_QUERY_END   = 0x04
MSG_RESP_START  = 0x05
MSG_RESP_CHUNK  = 0x06
MSG_RESP_END    = 0x07
MSG_CANCEL      = 0x08

HEADER_STRUCT = struct.Struct("<IBBHH")  # magic, version, msg_type, seq_num, payload_len
HEADER_SIZE = HEADER_STRUCT.size

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

def main():
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind(("0.0.0.0", PORT))
    
    # Get local IP
    hostname = socket.gethostname()
    local_ips = socket.gethostbyname_ex(hostname)[2]
    
    print("=" * 60)
    print("  Celestial Engine Mk.V — Mock Brain Server")
    print(f"  Listening on UDP port {PORT}")
    print(f"  Host IPs: {', '.join(local_ips)}")
    print("=" * 60)
    print("Waiting for Pico 2 W voice query...")
    
    query_buffer = bytearray()
    client_addr = None
    query_in_progress = False
    seq_counter = 0

    out_dir = os.path.dirname(os.path.abspath(__file__))
    query_wav_path = os.path.join(out_dir, "query.wav")
    
    while True:
        try:
            data, addr = sock.recvfrom(2048)
            if len(data) < HEADER_SIZE:
                continue
                
            magic, version, msg_type, seq_num, payload_len = HEADER_STRUCT.unpack(data[:HEADER_SIZE])
            if magic != MAGIC or version != VERSION:
                continue
                
            payload = data[HEADER_SIZE:HEADER_SIZE + payload_len]
            
            if msg_type == MSG_HEARTBEAT:
                print(f"[HEARTBEAT] From {addr[0]}:{addr[1]}")
                # Echo heartbeat back
                resp = HEADER_STRUCT.pack(MAGIC, VERSION, MSG_HEARTBEAT, seq_num, 0)
                sock.sendto(resp, addr)
                
            elif msg_type == MSG_QUERY_START:
                client_addr = addr
                query_in_progress = True
                query_buffer.clear()
                print(f"\n[QUERY START] From {addr[0]}:{addr[1]} — recording incoming audio...")
                
            elif msg_type == MSG_AUDIO_CHUNK:
                if query_in_progress:
                    query_buffer.extend(payload)
                    duration_s = len(query_buffer) / 32000.0
                    sys.stdout.write(f"\r  [STREAMING] {duration_s:.1f} s recorded ({len(query_buffer)} bytes)...")
                    sys.stdout.flush()
                    
            elif msg_type == MSG_CANCEL:
                query_in_progress = False
                query_buffer.clear()
                print(f"\n[QUERY CANCELLED] Query discarded (too short or silent). Waiting for next query...")
                
            elif msg_type == MSG_QUERY_END:
                query_in_progress = False
                total_s = len(query_buffer) / 32000.0
                print(f"\n[QUERY END] Total recorded: {total_s:.2f} s ({len(query_buffer)} bytes).")
                
                # Save to WAV
                if len(query_buffer) > 0:
                    rec_dir = os.path.join(out_dir, "recordings")
                    os.makedirs(rec_dir, exist_ok=True)
                    with wave.open(query_wav_path, "wb") as wf:
                        wf.setnchannels(1)
                        wf.setsampwidth(2)
                        wf.setframerate(16000)
                        wf.writeframes(query_buffer)
                    duration_s = len(query_buffer) / (16000 * 2)
                    timestamp_str = time.strftime("%Y%m%d_%H%M%S")
                    archive_path = os.path.join(rec_dir, f"query_{timestamp_str}.wav")
                    with wave.open(archive_path, "wb") as wf:
                        wf.setnchannels(1)
                        wf.setsampwidth(2)
                        wf.setframerate(16000)
                        wf.writeframes(query_buffer)
                    print(f"[SAVED] Latest: {query_wav_path} ({duration_s:.2f} s, 16 kHz 16-bit mono)")
                    print(f"[ARCHIVE] Saved to: {archive_path}")
                    print(f"  -> To play: powershell -c \"(New-Object Media.SoundPlayer '{query_wav_path}').PlaySync()\"")
                
                # Send response audio back to Pico
                resp_pcm = generate_test_response_pcm()
                print(f"[RESP] Streaming {len(resp_pcm)} bytes response audio back to {addr[0]}:{addr[1]}...")
                
                # Send RESP_START
                start_pkt = HEADER_STRUCT.pack(MAGIC, VERSION, MSG_RESP_START, 0, 0)
                sock.sendto(start_pkt, addr)
                time.sleep(0.01)
                
                # Send chunks of 512 bytes (256 samples = 16 ms)
                chunk_size = 512
                resp_seq = 0
                for offset in range(0, len(resp_pcm), chunk_size):
                    chunk = resp_pcm[offset:offset + chunk_size]
                    pkt = HEADER_STRUCT.pack(MAGIC, VERSION, MSG_RESP_CHUNK, resp_seq, len(chunk)) + chunk
                    sock.sendto(pkt, addr)
                    resp_seq += 1
                    time.sleep(0.012)  # Pace transmissions slightly below 16ms
                    
                # Send RESP_END
                end_pkt = HEADER_STRUCT.pack(MAGIC, VERSION, MSG_RESP_END, resp_seq, 0)
                sock.sendto(end_pkt, addr)
                print("[RESP] Response finished! Waiting for next query...\n")
                
        except KeyboardInterrupt:
            print("\nShutting down Mock Brain Server.")
            break
        except Exception as e:
            print(f"[ERROR] {e}")

if __name__ == "__main__":
    main()
