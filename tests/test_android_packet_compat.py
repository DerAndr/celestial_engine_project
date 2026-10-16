import struct
import pathlib
import sys
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / 'tools'))
from brain_transport import build_catalog_payload

MAGIC = 0x454C4543
VERSION = 2
HEADER_FORMAT = "<IBBHHI"
HEADER_SIZE = 14

MSG_HEARTBEAT = 0x01
MSG_APP_COMMAND = 0x20
MSG_APP_TIME_SYNC = 0x21

class TestAndroidPacketCompatibility(unittest.TestCase):
    def test_header_size(self):
        self.assertEqual(struct.calcsize(HEADER_FORMAT), HEADER_SIZE)

    def test_packet_pack_unpack_heartbeat(self):
        seq = 42
        req_id = 0
        payload = b""
        data = struct.pack(HEADER_FORMAT, MAGIC, VERSION, MSG_HEARTBEAT, seq, len(payload), req_id)
        self.assertEqual(len(data), 14)

        magic, ver, msg_type, s_seq, p_len, s_req = struct.unpack(HEADER_FORMAT, data)
        self.assertEqual(magic, MAGIC)
        self.assertEqual(ver, VERSION)
        self.assertEqual(msg_type, MSG_HEARTBEAT)
        self.assertEqual(s_seq, seq)
        self.assertEqual(p_len, 0)
        self.assertEqual(s_req, req_id)

    def test_packet_time_sync_payload(self):
        epoch_seconds = 1792015842
        millis = 350
        time_payload = struct.pack("<II", epoch_seconds, millis)
        self.assertEqual(len(time_payload), 8)

        data = struct.pack(HEADER_FORMAT, MAGIC, VERSION, MSG_APP_TIME_SYNC, 1, len(time_payload), 0) + time_payload
        self.assertEqual(len(data), 14 + 8)

        unpacked_sec, unpacked_ms = struct.unpack("<II", data[14:])
        self.assertEqual(unpacked_sec, epoch_seconds)
        self.assertEqual(unpacked_ms, millis)

    def test_packet_command_payload(self):
        cmd_code = ord('L')
        param = 7  # Animation 7
        cmd_payload = bytes([cmd_code, param])

        data = struct.pack(HEADER_FORMAT, MAGIC, VERSION, MSG_APP_COMMAND, 2, len(cmd_payload), 0) + cmd_payload
        self.assertEqual(len(data), 14 + 2)
        self.assertEqual(data[14], ord('L'))
        self.assertEqual(data[15], 7)

    def test_catalog_response_pi5_payload(self):
        payload = build_catalog_payload()
        # Header: host_type(1) + host_name(15) + model_count(1) = 17 bytes
        # Plus 8 models * 26 bytes = 208 bytes -> Total = 225 bytes
        self.assertEqual(len(payload), 17 + 8 * 26)
        self.assertEqual(payload[0], 1) # HOST_TYPE_PI5
        host_name = payload[1:16].rstrip(b'\x00').decode('ascii')
        self.assertEqual(host_name, "PI 5 BRAIN")
        self.assertEqual(payload[16], 8) # 8 models
        
        # Verify first model (DEEP THOUGHT)
        m0_id, m0_theme = payload[17], payload[18]
        m0_name = payload[19:37].rstrip(b'\x00').decode('ascii')
        m0_tag = payload[37:43].rstrip(b'\x00').decode('ascii')
        self.assertEqual(m0_id, 0)
        self.assertEqual(m0_name, "DEEP THOUGHT")
        self.assertEqual(m0_tag, "42")

        # Verify Qwen 0.5B (model 4)
        m4_offset = 17 + 4 * 26
        m4_id = payload[m4_offset]
        m4_name = payload[m4_offset + 2:m4_offset + 20].rstrip(b'\x00').decode('ascii')
        m4_tag = payload[m4_offset + 20:m4_offset + 26].rstrip(b'\x00').decode('ascii')
        self.assertEqual(m4_id, 4)
        self.assertEqual(m4_name, "QWEN 0.5B FAST")
        self.assertEqual(m4_tag, "Q05")

        # Verify Gemma 4 (model 5)
        m5_offset = 17 + 5 * 26
        m5_id = payload[m5_offset]
        m5_name = payload[m5_offset + 2:m5_offset + 20].rstrip(b'\x00').decode('ascii')
        m5_tag = payload[m5_offset + 20:m5_offset + 26].rstrip(b'\x00').decode('ascii')
        self.assertEqual(m5_id, 5)
        self.assertEqual(m5_name, "GEMMA 4 (E2B)")
        self.assertEqual(m5_tag, "G4")

    def test_catalog_response_phone_payload(self):
        phone_models = [
            (0, 4, "Gemma 4 (E2B)", "G4-MM"),
            (1, 1, "Gemma 4 Think", "G4THK"),
            (2, 0, "Gemini Nano", "NANO"),
            (3, 2, "Pi 5 Core", "PI5")
        ]
        payload = build_catalog_payload(host_type=2, host_name="PHONE BRAIN", models=phone_models)
        self.assertEqual(len(payload), 17 + 4 * 26)
        self.assertEqual(payload[0], 2) # HOST_TYPE_PHONE
        host_name = payload[1:16].rstrip(b'\x00').decode('ascii')
        self.assertEqual(host_name, "PHONE BRAIN")
        self.assertEqual(payload[16], 4)

if __name__ == "__main__":
    unittest.main()
