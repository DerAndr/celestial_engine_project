import struct
import unittest

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

if __name__ == "__main__":
    unittest.main()
