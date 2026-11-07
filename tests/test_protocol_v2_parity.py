#!/usr/bin/env python3
"""
Phase 4 Integration Test Suite:
1. Cross-stack binary protocol v2 parity across C (net_protocol.h / net_client.c),
   Kotlin (NetMessageType.kt / ProtocolConstants.kt), and Python (brain_transport.py / ai_brain_server.py).
2. Mock Voice Loop End-to-End Test over real UDP sockets (Heartbeat -> Catalog -> Query Audio ->
   [SPELL: ...] APP_COMMAND 0x20 dispatch -> acknowledged PCM audio & OLED text streaming).
"""

import os
import re
import socket
import struct
import sys
import threading
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

import brain_transport
import ai_brain_server


class TestProtocolV2ParityAndMockVoiceLoop(unittest.TestCase):

    def test_cross_stack_c_kotlin_python_protocol_parity(self):
        c_header = (ROOT / "firmware/src/net_protocol.h").read_text(encoding="utf-8")
        kt_msg = (ROOT / "android/app/src/main/java/com/celestial/companion/protocol/NetMessageType.kt").read_text(encoding="utf-8")
        kt_const = (ROOT / "android/app/src/main/java/com/celestial/companion/protocol/ProtocolConstants.kt").read_text(encoding="utf-8")

        # 1. Verify MAGIC, VERSION, PORT, HEADER_SIZE across C, Kotlin, and Python
        c_magic = int(re.search(r"#define\s+CELESTIAL_NET_MAGIC\s+(0x[0-9A-Fa-f]+)", c_header).group(1), 16)
        c_ver = int(re.search(r"#define\s+CELESTIAL_NET_VERSION\s+(\d+)", c_header).group(1))
        c_port = int(re.search(r"#define\s+CELESTIAL_NET_DEFAULT_PORT\s+(\d+)", c_header).group(1))

        kt_magic = int(re.search(r"const\s+val\s+MAGIC:\s*Int\s*=\s*(0x[0-9A-Fa-f]+)", kt_const).group(1), 16)
        kt_ver = int(re.search(r"const\s+val\s+VERSION:\s*Int\s*=\s*(\d+)", kt_const).group(1))
        kt_port = int(re.search(r"const\s+val\s+DEFAULT_PORT:\s*Int\s*=\s*(\d+)", kt_const).group(1))
        kt_hdr = int(re.search(r"const\s+val\s+HEADER_SIZE:\s*Int\s*=\s*(\d+)", kt_const).group(1))

        self.assertEqual(c_magic, brain_transport.MAGIC)
        self.assertEqual(kt_magic, brain_transport.MAGIC)
        self.assertEqual(c_ver, brain_transport.VERSION)
        self.assertEqual(kt_ver, brain_transport.VERSION)
        self.assertEqual(c_port, brain_transport.PORT)
        self.assertEqual(kt_port, brain_transport.PORT)
        self.assertEqual(kt_hdr, brain_transport.HEADER_SIZE)
        self.assertEqual(brain_transport.HEADER_SIZE, 14)

        # 2. Extract all NET_MSG_* enum values from C header and Kotlin NetMessageType.kt
        c_msgs = {
            m.group(1): int(m.group(2), 16)
            for m in re.finditer(r"NET_MSG_([A-Z_]+)\s*=\s*(0x[0-9A-Fa-f]+)", c_header)
        }
        kt_msgs = {
            m.group(1): int(m.group(2), 16)
            for m in re.finditer(r"([A-Z_]+)\((0x[0-9A-Fa-f]+)\)", kt_msg)
        }
        py_msgs = {
            "HEARTBEAT": brain_transport.MSG_HEARTBEAT,
            "QUERY_START": brain_transport.MSG_QUERY_START,
            "AUDIO_CHUNK": brain_transport.MSG_AUDIO_CHUNK,
            "QUERY_END": brain_transport.MSG_QUERY_END,
            "RESP_START": brain_transport.MSG_RESP_START,
            "RESP_CHUNK": brain_transport.MSG_RESP_CHUNK,
            "RESP_END": brain_transport.MSG_RESP_END,
            "CANCEL": brain_transport.MSG_CANCEL,
            "RESP_TEXT": brain_transport.MSG_RESP_TEXT,
            "ACK": brain_transport.MSG_ACK,
            "PROGRESS": brain_transport.MSG_PROGRESS,
            "ERROR": brain_transport.MSG_ERROR,
            "CATALOG_REQ": brain_transport.MSG_CATALOG_REQ,
            "CATALOG_RESP": brain_transport.MSG_CATALOG_RESP,
            "APP_COMMAND": brain_transport.MSG_APP_COMMAND,
            "APP_TIME_SYNC": brain_transport.MSG_APP_TIME_SYNC,
            "APP_TELEMETRY": brain_transport.MSG_APP_TELEMETRY,
        }

        self.assertEqual(len(c_msgs), 17)
        self.assertEqual(c_msgs, kt_msgs)
        self.assertEqual(c_msgs, py_msgs)

        # 3. Verify C firmware DEFAULT_PI5_CATALOG matches Python build_catalog_payload()
        c_net_client = (ROOT / "firmware/src/net_client.c").read_text(encoding="utf-8")
        self.assertRegex(c_net_client, r'\{\s*5,\s*0,\s*"GEMMA 4 \(E2B\)",\s*"G4"\s*\}')
        self.assertEqual(ai_brain_server.LOCAL_MODELS_BY_PRESET[5], "gemma4:e2b")

    def test_mock_voice_loop_end_to_end_with_spell_dispatch(self):
        server_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        server_sock.bind(("127.0.0.1", 0))
        server_addr = server_sock.getsockname()

        staff_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        staff_sock.bind(("127.0.0.1", 0))
        staff_sock.settimeout(2.0)

        transport = brain_transport.BrainTransport(server_sock, retry_seconds=0.05, packet_timeout=2.0)
        expected_pcm = b"\x10\x27\xf0\xd8" * 400  # 1600 bytes of 16 kHz PCM (spans 4 chunks of 512/512/512/64)

        def mock_query_worker(tp, session):
            # Simulate transcribing "Cast Fireball!" + LLM returning [SPELL: ...] tag
            user_text = "Cast Fireball!"
            matched = ai_brain_server.match_fast_spell(user_text)
            llm_raw = "[SPELL: Fireball | ANIM: PLASMA | SOUNDS: LASER, SUPERNOVA] Ignis plasma infernum!"
            parsed = ai_brain_server.parse_spell_tag(llm_raw, fallback_spell=matched)

            # Dispatch hardware spell commands (0x20)
            tp.send_app_command(session, 'L', parsed["anim_id"], passkey="apophis")
            tp.send_app_command(session, 'C', parsed["sfx_ids"][0], passkey="apophis")

            oled_bytes = ai_brain_server.to_oled_ascii(parsed["oled_text"]).encode("ascii", "replace")
            tp.stream(session, expected_pcm, oled_bytes)

        server_thread = threading.Thread(target=transport.serve, args=(mock_query_worker,), daemon=True)
        server_thread.start()

        try:
            # Step 1: Heartbeat -> Heartbeat response with 4-byte local_epoch
            staff_sock.sendto(brain_transport.packet(brain_transport.MSG_HEARTBEAT, 0, 1), server_addr)
            raw, _ = staff_sock.recvfrom(1024)
            kind, req_id, seq, payload = brain_transport.unpack(raw)
            self.assertEqual((kind, req_id, seq, len(payload)), (brain_transport.MSG_HEARTBEAT, 0, 1, 4))

            # Step 2: Catalog Request -> Catalog Response (225 bytes)
            staff_sock.sendto(brain_transport.packet(brain_transport.MSG_CATALOG_REQ, 0, 2), server_addr)
            raw, _ = staff_sock.recvfrom(1024)
            kind, req_id, seq, payload = brain_transport.unpack(raw)
            self.assertEqual(kind, brain_transport.MSG_CATALOG_RESP)
            self.assertEqual(len(payload), 17 + 8 * 26)

            # Step 3: Voice Query Start + 3 Audio Chunks + Query End
            req_id = 4242
            staff_sock.sendto(brain_transport.packet(brain_transport.MSG_QUERY_START, req_id, 0, bytes([0, 5])), server_addr)
            for seq_idx in range(3):
                staff_sock.sendto(brain_transport.packet(brain_transport.MSG_AUDIO_CHUNK, req_id, seq_idx, b"\x00\x01" * 160), server_addr)
            staff_sock.sendto(brain_transport.packet(brain_transport.MSG_QUERY_END, req_id, 3), server_addr)

            # Step 4: Collect APP_COMMAND (0x20) + RESP_START + RESP_TEXT + RESP_CHUNK + RESP_END
            app_commands = []
            received_text = b""
            received_pcm = bytearray()
            got_resp_start = False
            got_resp_end = False

            while not got_resp_end:
                raw, _ = staff_sock.recvfrom(1024)
                kind, r_id, seq, payload = brain_transport.unpack(raw)
                if kind == brain_transport.MSG_ACK:
                    continue
                if kind == brain_transport.MSG_APP_COMMAND:
                    token, cmd_byte, param_byte = struct.unpack("<IBB", payload[:6])
                    self.assertEqual(token, brain_transport.compute_auth_token("apophis"))
                    app_commands.append((chr(cmd_byte), param_byte))
                elif kind == brain_transport.MSG_RESP_START:
                    got_resp_start = True
                    staff_sock.sendto(brain_transport.packet(brain_transport.MSG_ACK, r_id, seq, bytes([kind])), server_addr)
                elif kind == brain_transport.MSG_RESP_TEXT:
                    received_text = payload
                    staff_sock.sendto(brain_transport.packet(brain_transport.MSG_ACK, r_id, seq, bytes([kind])), server_addr)
                elif kind == brain_transport.MSG_RESP_CHUNK:
                    received_pcm.extend(payload)
                    staff_sock.sendto(brain_transport.packet(brain_transport.MSG_ACK, r_id, seq, bytes([kind])), server_addr)
                elif kind == brain_transport.MSG_RESP_END:
                    got_resp_end = True
                    staff_sock.sendto(brain_transport.packet(brain_transport.MSG_ACK, r_id, seq, bytes([kind])), server_addr)

            self.assertTrue(got_resp_start)
            self.assertEqual(app_commands, [('L', 5), ('C', 3)])
            self.assertEqual(received_text.decode("ascii"), "[SPELL] Fireball: Ignis plasma infernum!")
            self.assertEqual(bytes(received_pcm), expected_pcm)
        finally:
            server_sock.close()
            staff_sock.close()


if __name__ == "__main__":
    unittest.main()
