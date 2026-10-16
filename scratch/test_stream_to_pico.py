import socket
import struct
import time
import math

MAGIC = 0x454C4543
VERSION = 2
PORT = 5005
PICO_IP = "192.168.178.101"

HEADER_STRUCT = struct.Struct('<IBBHHI')
# Message types
MSG_HEARTBEAT, MSG_QUERY_START, MSG_AUDIO_CHUNK, MSG_QUERY_END = range(1, 5)
MSG_RESP_START, MSG_RESP_CHUNK, MSG_RESP_END, MSG_CANCEL = range(5, 9)
MSG_RESP_TEXT, MSG_ACK, MSG_PROGRESS, MSG_ERROR = range(9, 13)

def test_stream():
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    req_id = int(time.time() * 1000) & 0xFFFFFFFF

    # 1. First send RESP_TEXT
    text = "Cosmic Voice Link OK"
    text_bytes = text.encode('utf-8')
    hdr_text = HEADER_STRUCT.pack(MAGIC, VERSION, MSG_RESP_TEXT, 1, len(text_bytes), req_id)
    sock.sendto(hdr_text + text_bytes, (PICO_IP, PORT))
    print(f"Sent RESP_TEXT: {text}")
    time.sleep(0.05)

    # 2. Synthesize 1.5 seconds of 440 Hz + 880 Hz harmonic test audio (16kHz mono 16-bit)
    sample_rate = 16000
    duration = 1.5
    sample_count = int(sample_rate * duration)
    audio_pcm = bytearray(sample_count * 2)

    for i in range(sample_count):
        t = i / float(sample_rate)
        # Tone with gentle envelope
        env = min(1.0, min(t * 10.0, (duration - t) * 10.0))
        val = int(24000.0 * env * (0.7 * math.sin(2.0 * math.pi * 523.25 * t) + 0.3 * math.sin(2.0 * math.pi * 1046.5 * t)))
        struct.pack_into('<h', audio_pcm, i * 2, max(-32768, min(32767, val)))

    # 3. Send RESP_START
    start_payload = struct.pack('<I', sample_count)
    hdr_start = HEADER_STRUCT.pack(MAGIC, VERSION, MSG_RESP_START, 1, len(start_payload), req_id)
    sock.sendto(hdr_start + start_payload, (PICO_IP, PORT))
    print(f"Sent RESP_START: {sample_count} samples")
    time.sleep(0.025)

    # 4. Send chunks of 480 bytes (240 samples = 15 ms)
    chunk_size = 480
    seq = 0
    for offset in range(0, len(audio_pcm), chunk_size):
        chunk = audio_pcm[offset:offset + chunk_size]
        hdr_chunk = HEADER_STRUCT.pack(MAGIC, VERSION, MSG_RESP_CHUNK, seq, len(chunk), req_id)
        sock.sendto(hdr_chunk + chunk, (PICO_IP, PORT))
        seq += 1
        time.sleep(0.015)

    # 5. Send RESP_END
    hdr_end = HEADER_STRUCT.pack(MAGIC, VERSION, MSG_RESP_END, seq, 0, req_id)
    sock.sendto(hdr_end, (PICO_IP, PORT))
    print(f"Sent RESP_END ({seq} chunks streamed)")

if __name__ == "__main__":
    test_stream()
