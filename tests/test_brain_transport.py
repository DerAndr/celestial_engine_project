"""Small server-side regression checks; firmware checks remain native C."""
import pathlib
import socket
import struct
import sys
import threading
import unittest
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / 'tools'))
from brain_transport import *

class TransportChecks(unittest.TestCase):
    def setUp(self):
        self.server = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.server.bind(('127.0.0.1', 0))
        self.client = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.client.bind(('127.0.0.1', 0))
        self.client.settimeout(2)
        self.transport = BrainTransport(self.server, retry_seconds=0.03, packet_timeout=2)
        self.addr = self.client.getsockname()

    def tearDown(self):
        self.server.close()
        self.client.close()

    def query(self, rid):
        self.transport.handle(packet(MSG_QUERY_START, rid, payload=bytes([3, 0])), self.addr)
        self.transport.handle(packet(MSG_AUDIO_CHUNK, rid, 2, b'\x01\x00' * 16), self.addr)
        return self.transport.handle(packet(MSG_QUERY_END, rid, 3), self.addr)

    def test_cancel_late_response_and_duplicate_end(self):
        old = self.query(1)
        self.assertIsNone(self.transport.handle(packet(MSG_QUERY_END, 1, 4), self.addr))
        new = self.query(2)
        self.assertTrue(old.cancelled.is_set())
        self.assertFalse(self.transport.send(old, MSG_RESP_START))
        self.assertIsNone(self.transport.handle(packet(MSG_QUERY_START, 1, payload=bytes([3, 0])), self.addr))
        self.assertTrue(self.transport.active(new))
        self.transport.handle(packet(MSG_CANCEL, 2), self.addr)
        self.assertFalse(self.transport.stream(new, b'\x00\x00', b'late'))

    def test_malformed_packet_rejected(self):
        self.assertIsNone(unpack(packet(MSG_RESP_CHUNK, 1, payload=b'ab')[:-1]))
        self.assertIsNone(unpack(packet(MSG_RESP_CHUNK, 1) + b'extra'))
        self.assertIsNone(unpack(HEADER_STRUCT.pack(MAGIC, 1, MSG_RESP_START, 0, 0, 1)))
        session = self.query(4)
        session.done = True
        session.created -= 301
        self.transport.progress()
        self.assertNotIn(self.addr, self.transport.sessions)

    def test_long_response_with_packet_and_ack_loss(self):
        session = self.query(3)
        pcm = bytes(range(256)) * 1750  # 14 seconds, well beyond the 2-second Pico ring
        result = []
        worker = threading.Thread(target=lambda: result.append(self.transport.stream(session, pcm, b'FULL ANSWER')))
        worker.start()
        expected = 0
        received = bytearray()
        dropped_chunk = dropped_ack = False
        while True:
            data, addr = self.client.recvfrom(1024)
            kind, rid, seq, payload = unpack(data)
            if kind == MSG_RESP_CHUNK:
                if seq == 3 and not dropped_chunk:
                    dropped_chunk = True
                    continue
                if seq == expected:
                    received.extend(payload)
                    expected += 1
                self.assertIn(seq, (expected - 1,))
                if seq == 7 and not dropped_ack:
                    dropped_ack = True
                    continue
            self.transport.handle(packet(MSG_ACK, rid, seq, bytes([kind])), self.addr)
            if kind == MSG_RESP_END:
                break
        worker.join(timeout=2)
        self.assertFalse(worker.is_alive())
        self.assertEqual(result, [True])
        self.assertEqual(received, pcm)  # No lost/duplicated syllables or truncated tail

if __name__ == '__main__':
    unittest.main()
