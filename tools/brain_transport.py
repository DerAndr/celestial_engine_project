"""Protocol v2: cancellable request sessions and acknowledged response delivery.

No provider dependencies: the same transport runs on Windows and Raspberry Pi 5.
"""
import socket
import struct
import threading
import time
from dataclasses import dataclass, field
from collections import deque

MAGIC, VERSION, PORT = 0x454C4543, 2, 5005
HEADER_STRUCT = struct.Struct('<IBBHHI')
HEADER_SIZE = HEADER_STRUCT.size
MSG_HEARTBEAT, MSG_QUERY_START, MSG_AUDIO_CHUNK, MSG_QUERY_END = range(1, 5)
MSG_RESP_START, MSG_RESP_CHUNK, MSG_RESP_END, MSG_CANCEL = range(5, 9)
MSG_RESP_TEXT, MSG_ACK, MSG_PROGRESS, MSG_ERROR = range(9, 13)
MSG_CATALOG_REQ = 0x0D
MSG_CATALOG_RESP = 0x0E
MAX_PAYLOAD = 512
MAX_QUERY_BYTES = 16000 * 2 * 60

def build_catalog_payload(host_type=1, host_name="PI 5 BRAIN", models=None):
    """Packs net_catalog_resp_t matching Pico 2 W binary layout:
    uint8_t host_type (1=PI5, 2=PHONE)
    char host_name[15]
    uint8_t model_count
    net_model_info_t models[8] (each 26 bytes: model_id: uint8, theme_idx: uint8, name: 18s, tag: 6s)
    """
    if models is None:
        models = [
            (0, 0, "DEEP THOUGHT", "42"),
            (1, 1, "EDDIE (8B)", "EDD"),
            (2, 2, "GROK 4.5", "GRK"),
            (3, 3, "MARVIN (70B)", "MRV"),
            (4, 4, "QWEN 0.5B FAST", "Q05"),
            (5, 0, "GEMMA 4 (E2B)", "G4"),
            (6, 2, "DEEPSEEK R1", "R1"),
            (7, 3, "QWEN 1.5B CORE", "Q15")
        ]
    h_bytes = host_name.encode('ascii')[:14]
    h_bytes = h_bytes + b'\x00' * (15 - len(h_bytes))
    m_count = min(len(models), 8)
    payload = bytearray([host_type]) + h_bytes + bytearray([m_count])
    for mid, theme, name, tag in models[:8]:
        n_bytes = name.encode('ascii')[:17]
        n_bytes = n_bytes + b'\x00' * (18 - len(n_bytes))
        t_bytes = tag.encode('ascii')[:5]
        t_bytes = t_bytes + b'\x00' * (6 - len(t_bytes))
        payload += bytearray([mid, theme]) + n_bytes + t_bytes
    return bytes(payload)

def packet(kind, request_id=0, seq=0, payload=b''):
    if len(payload) > MAX_PAYLOAD:
        raise ValueError('oversize payload')
    return HEADER_STRUCT.pack(MAGIC, VERSION, kind, seq, len(payload), request_id) + payload

def unpack(data):
    if len(data) < HEADER_SIZE:
        return None
    magic, version, kind, seq, length, request_id = HEADER_STRUCT.unpack_from(data)
    if magic != MAGIC or version != VERSION or length > MAX_PAYLOAD or len(data) != HEADER_SIZE + length:
        return None
    return kind, request_id, seq, data[HEADER_SIZE:]

@dataclass
class Session:
    addr: tuple
    request_id: int
    lang: int
    model: int
    pcm: bytearray = field(default_factory=bytearray)
    recording: bool = True
    done: bool = False
    cancelled: threading.Event = field(default_factory=threading.Event)
    condition: threading.Condition = field(default_factory=threading.Condition)
    acknowledged: tuple = None
    last_upload_seq: int = None
    created: float = field(default_factory=time.monotonic)
    last_progress: float = 0

class BrainTransport:
    def __init__(self, sock, retry_seconds=0.2, packet_timeout=12):
        self.sock = sock
        self.sessions = {}
        self.retired_ids = {}
        self.lock = threading.RLock()
        self.retry_seconds = retry_seconds
        self.packet_timeout = packet_timeout

    def active(self, session):
        with self.lock:
            return self.sessions.get(session.addr) is session and not session.cancelled.is_set()

    def send(self, session, kind, seq=0, payload=b''):
        with self.lock:
            if not self.active(session):
                return False
            self.sock.sendto(packet(kind, session.request_id, seq, payload), session.addr)
            return True

    def cancel(self, session):
        session.cancelled.set()
        with session.condition:
            session.condition.notify_all()

    def handle(self, data, addr):
        """Called by the receive loop. Return a completed upload exactly once."""
        parsed = unpack(data)
        if not parsed:
            return None
        kind, rid, seq, payload = parsed
        if kind == MSG_HEARTBEAT and rid == 0:
            local_epoch = int(time.mktime(time.localtime()))
            self.sock.sendto(packet(MSG_HEARTBEAT, seq=seq, payload=struct.pack("<I", local_epoch)), addr)
            return None
        if kind == MSG_CATALOG_REQ:
            resp_payload = build_catalog_payload()
            self.sock.sendto(packet(MSG_CATALOG_RESP, seq=seq, payload=resp_payload), addr)
            return None
        with self.lock:
            current = self.sessions.get(addr)
            if kind == MSG_QUERY_START and len(payload) == 2 and rid:
                if rid in self.retired_ids.get(addr, ()):
                    return None
                if current and current.request_id == rid:
                    return None  # redundant START must not erase capture or rerun generation
                if current:
                    self.cancel(current)
                    self.retired_ids.setdefault(addr, deque(maxlen=32)).append(current.request_id)
                if len(self.sessions) >= 8 and addr not in self.sessions:
                    return None
                current = Session(addr, rid, payload[0], payload[1])
                self.sessions[addr] = current
                return None
            if not current or current.request_id != rid or current.cancelled.is_set():
                return None
            if kind == MSG_CANCEL:
                self.cancel(current)
            elif kind == MSG_ACK and len(payload) == 1:
                with current.condition:
                    current.acknowledged = (payload[0], seq)
                    current.condition.notify_all()
            elif kind == MSG_AUDIO_CHUNK and current.recording and payload and not len(payload) % 2:
                if current.last_upload_seq is not None:
                    delta = (seq - current.last_upload_seq) & 0xffff
                    if not 0 < delta < 0x8000:
                        return None
                if len(current.pcm) + len(payload) > MAX_QUERY_BYTES:
                    self.send(current, MSG_ERROR, payload=b'Query too long')
                    self.cancel(current)
                else:
                    current.pcm.extend(payload)
                    current.last_upload_seq = seq
            elif kind == MSG_QUERY_END and current.recording:
                current.recording = False
                if current.pcm:
                    return current
                self.send(current, MSG_ERROR, payload=b'Empty query')
                self.cancel(current)
        return None

    def progress(self):
        now = time.monotonic()
        with self.lock:
            for addr, session in list(self.sessions.items()):
                if (session.done or session.cancelled.is_set()) and now - session.created > 300:
                    del self.sessions[addr]
                    self.retired_ids.pop(addr, None)
                    continue
                if not session.recording and not session.done and now - session.last_progress >= 1:
                    self.send(session, MSG_PROGRESS)
                    session.last_progress = now
                if not session.done and now - session.created > 240:
                    self.send(session, MSG_ERROR, payload=b'Request expired')
                    self.cancel(session)
                    session.done = True

    def deliver(self, session, kind, seq=0, payload=b''):
        """Stop-and-wait; one missing datagram/ACK cannot silently remove speech."""
        deadline = time.monotonic() + self.packet_timeout
        key = (kind, seq)
        with session.condition:
            session.acknowledged = None
        while self.active(session) and time.monotonic() < deadline:
            if not self.send(session, kind, seq, payload):
                return False
            with session.condition:
                session.condition.wait_for(
                    lambda: session.acknowledged == key or session.cancelled.is_set(),
                    timeout=self.retry_seconds)
                if session.acknowledged == key:
                    return True
        if self.active(session):
            self.send(session, MSG_ERROR, payload=b'Response delivery stalled')
            self.cancel(session)
        return False

    def stream(self, session, pcm, text):
        if not pcm or len(pcm) % 2:
            self.send(session, MSG_ERROR, payload=b'No synthesized audio')
            return False
        # Sequence numbers limit one response to ~17 minutes; reject instead of wrap ambiguity.
        if (len(pcm) + MAX_PAYLOAD - 1) // MAX_PAYLOAD >= 65536:
            self.send(session, MSG_ERROR, payload=b'Response too long')
            return False
        if not self.deliver(session, MSG_RESP_TEXT, payload=text[:500]):
            return False
        if not self.deliver(session, MSG_RESP_START, payload=struct.pack('<I', len(pcm) // 2)):
            return False
        seq = 0
        due = time.monotonic()
        for offset in range(0, len(pcm), MAX_PAYLOAD):
            chunk = pcm[offset:offset + MAX_PAYLOAD]
            if not self.deliver(session, MSG_RESP_CHUNK, seq, chunk):
                return False
            # Never feed faster than 16kHz consumption. A finite buffer is not infinite RAM.
            # Use one absolute clock: Windows timer rounding must not accumulate per packet.
            due += len(chunk) / 32000
            session.cancelled.wait(max(0, due - time.monotonic()))
            seq += 1
        return self.deliver(session, MSG_RESP_END, seq)

    def serve(self, worker):
        self.sock.settimeout(0.5)
        slots = threading.BoundedSemaphore(4)
        def run(session):
            try:
                worker(self, session)
            except Exception as exc:
                print(f'[REQUEST ERROR] {session.request_id}: {type(exc).__name__}')
                self.send(session, MSG_ERROR, payload=b'Generation failed')
            finally:
                session.done = True
                slots.release()
        while True:
            try:
                data, addr = self.sock.recvfrom(HEADER_SIZE + MAX_PAYLOAD + 1)
                session = self.handle(data, addr)
                if session:
                    if slots.acquire(blocking=False):
                        threading.Thread(target=run, args=(session,), daemon=True).start()
                    else:
                        self.send(session, MSG_ERROR, payload=b'Brain busy')
                        session.done = True
            except socket.timeout:
                pass
            self.progress()
