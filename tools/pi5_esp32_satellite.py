#!/usr/bin/env python3
"""
Celestial Engine Mk.V — Raspberry Pi 5 <-> Waveshare ESP32-S3-Touch-AMOLED-1.8 Bridge
Turns the connected ESP32-S3 AMOLED display + ES8311 codec into a non-destructive
Wearable Satellite Screen, 16 kHz Speaker, and 16 kHz Push-to-Talk Microphone.

Features (Task 11.4):
- Auto-discovers ESP32-S3 via UDP broadcast (`CRICKET_DISCOVER` on port 4210),
  USB CDC serial (`VID_303A` / `PID_1001`), or explicit `--esp-ip` (default 192.168.178.121).
- Sends `celestial_pi5_attach` on startup so ESP32-S3 automatically switches into
  `MODE_CELESTIAL_PI5_BRAIN` (`PI 5 ORACLE` HUD).
- Background heartbeat thread (every 1.5s) streams Pi 5 CPU temp, RAM %, active persona,
  STT transcript, Oracle prophecy, and active `[SPELL]` state.
- UDP port 4212 receiver accepts `"CMIC"` 16 kHz mono PCM16LE frames from the ESP32-S3
  ES8311 microphone and processes complete utterances on `"CMIC_END"`:
  `LocalAiEngine.transcribe_audio_bytes() -> generate_prophecy() -> synthesize_speech_pcm()`.
- TCP port 4211 streamer sends synthesized 16 kHz 16-bit mono PCM in 512-byte chunks to
  the ESP32-S3 ES8311 speaker.
- Sends `celestial_pi5_detach` cleanly on shutdown (`SIGINT`/`SIGTERM`) so the ESP32-S3
  automatically returns to its 12-page Cricket PC Monitor mode.
"""

import argparse
import json
import math
import os
import signal
import socket
import struct
import sys
import tempfile
import threading
import time
import urllib.request
import wave

TOOLS_DIR = os.path.dirname(os.path.abspath(__file__))
if TOOLS_DIR not in sys.path:
    sys.path.insert(0, TOOLS_DIR)

try:
    import local_ai_engine
except Exception:
    local_ai_engine = None

DEFAULT_ESP_IP = "192.168.178.121"
ESP_UDP_PORT = 4210
ESP_AUDIO_TCP_PORT = 4211
PI5_MIC_UDP_PORT = 4212

PERSONA_LIST = [
    ("machine_god", "MACHINE GOD"),
    ("archmage", "ARCHMAGE"),
    ("marvin", "MARVIN"),
    ("cyberpunk", "CYBERPUNK"),
    ("pumpkin_lord", "PUMPKIN LORD"),
    ("custom", "CUSTOM"),
]

SPELL_COLORS = {
    "NONE": (36, 204, 182),
    "Fireball": (245, 152, 48),
    "Solar Flare": (245, 152, 48),
    "Chain Lightning": (74, 142, 242),
    "Aegis Shield": (36, 204, 182),
    "Cosmic Vortex": (196, 124, 238),
    "Ouroboros Serpent": (62, 214, 132),
    "Quantum Quack": (238, 196, 92),
    "Lumos Maxima": (255, 240, 190),
    "Expecto Patronum": (220, 228, 245),
}


def _read_pi5_metrics() -> tuple[float, float]:
    """Reads live Pi 5 CPU temperature (C) and RAM usage (%) with cross-platform fallback."""
    temp_c = 48.0
    ram_pct = 24.0
    try:
        tz = "/sys/class/thermal/thermal_zone0/temp"
        if os.path.exists(tz):
            with open(tz, "r", encoding="utf-8") as f:
                temp_c = round(int(f.read().strip()) / 1000.0, 1)
    except Exception:
        pass
    try:
        meminfo = "/proc/meminfo"
        if os.path.exists(meminfo):
            vals = {}
            with open(meminfo, "r", encoding="utf-8") as f:
                for line in f:
                    parts = line.split(":")
                    if len(parts) == 2:
                        vals[parts[0].strip()] = int(parts[1].strip().split()[0])
            tot = vals.get("MemTotal", 0)
            avail = vals.get("MemAvailable", 0)
            if tot > 0:
                ram_pct = round(100.0 * (tot - avail) / tot, 1)
    except Exception:
        pass
    return temp_c, ram_pct


def synthesize_demo_chime_pcm(sample_rate: int = 16000) -> bytes:
    """Generates a pleasant 0.8s cyber-arcane 3-note chime in 16 kHz mono PCM16LE."""
    notes = [(523.25, 0.22), (659.25, 0.22), (783.99, 0.34)]
    frames = []
    for freq, dur in notes:
        n = int(sample_rate * dur)
        for i in range(n):
            t = i / float(sample_rate)
            env = min(1.0, i / 160.0) * min(1.0, (n - i) / 320.0)
            val = math.sin(2.0 * math.pi * freq * t) * 0.55 + math.sin(4.0 * math.pi * freq * t) * 0.15
            sample = max(-32767, min(32767, int(val * env * 14000.0)))
            frames.append(struct.pack("<h", sample))
    return b"".join(frames)


class LocalAiEngine:
    """Adapter around `tools/local_ai_engine.py` exposing bytes-in/bytes-out satellite hooks."""

    def transcribe_audio_bytes(self, pcm16_bytes: bytes, sample_rate: int = 16000) -> str:
        if not pcm16_bytes:
            return ""
        if local_ai_engine is None:
            return "Cast solar flare"
        tmp_path = None
        try:
            with tempfile.NamedTemporaryFile(suffix=".wav", delete=False) as tf:
                tmp_path = tf.name
            with wave.open(tmp_path, "wb") as wf:
                wf.setnchannels(1)
                wf.setsampwidth(2)
                wf.setframerate(sample_rate)
                wf.writeframes(pcm16_bytes)
            text, _ = local_ai_engine.local_transcribe(tmp_path, lang_id=0)
            return text or ""
        except Exception:
            return ""
        finally:
            if tmp_path and os.path.exists(tmp_path):
                try:
                    os.unlink(tmp_path)
                except OSError:
                    pass

    def generate_prophecy(self, transcript: str, persona_id: str = "machine_god") -> tuple[str, str]:
        if not transcript:
            return "The Omnissiah awaits your voice command.", "NONE"
        spell_name = "NONE"
        if local_ai_engine is not None:
            try:
                reply = local_ai_engine.local_query_llm(
                    transcript,
                    lang_tag="en-US",
                    persona_id=persona_id,
                )
                return reply, spell_name
            except Exception:
                pass
        return f"Oracle ({persona_id}) received: {transcript[:64]}", spell_name

    def synthesize_speech_pcm(self, text: str, persona_id: str = "machine_god") -> bytes:
        if not text:
            return b""
        if local_ai_engine is not None:
            try:
                pcm = local_ai_engine.local_synthesize_pcm(text, lang_tag="en-US", persona_id=persona_id)
                if pcm:
                    return pcm
            except Exception:
                pass
        return synthesize_demo_chime_pcm()


def build_cmic_packet(seq: int, pcm16_frame: bytes) -> bytes:
    """Builds a 16 kHz mono microphone UDP packet: 'CMIC' + uint16 LE seq + PCM16LE payload."""
    return b"CMIC" + struct.pack("<H", seq & 0xFFFF) + pcm16_frame


def parse_cmic_packet(data: bytes) -> tuple[str, int, bytes]:
    """Parses a 'CMIC' or 'CMIC_END' UDP packet from the ESP32-S3."""
    if data.startswith(b"CMIC_END"):
        return "CMIC_END", 0, b""
    if len(data) >= 6 and data[:4] == b"CMIC":
        seq = struct.unpack("<H", data[4:6])[0]
        return "CMIC", seq, data[6:]
    return "UNKNOWN", 0, b""


class Esp32SatelliteBridge:
    """Manages discovery, attach/detach lifecycle, HUD telemetry, mic RX, and speaker TX."""

    def __init__(
        self,
        esp_ip: str | None = None,
        udp_port: int = ESP_UDP_PORT,
        audio_tcp_port: int = ESP_AUDIO_TCP_PORT,
        mic_udp_port: int = PI5_MIC_UDP_PORT,
        persona_id: str = "machine_god",
        local_engine: LocalAiEngine | None = None,
    ):
        self.esp_ip = esp_ip
        self.udp_port = udp_port
        self.audio_tcp_port = audio_tcp_port
        self.mic_udp_port = mic_udp_port
        self.persona_id = persona_id
        self.local_engine = local_engine or LocalAiEngine()

        self.serial_port_path: str | None = None
        self._serial_conn = None
        self._udp_sock: socket.socket | None = None
        self._mic_sock: socket.socket | None = None

        self._stop_event = threading.Event()
        self._hb_thread: threading.Thread | None = None
        self._mic_thread: threading.Thread | None = None

        self._lock = threading.Lock()
        self.attached = False
        self.stt_engine = "MOONSHINE INT8"
        self.tts_engine = "KOKORO-82M"
        self.status = "ONLINE :5005"
        self.last_heard = "Ready for voice command"
        self.prophecy = "The Omnissiah blesses this satellite node."
        self.active_spell = "NONE"
        self.latency_ms = 0
        self.mic_chunks: list[tuple[int, bytes]] = []

    def _persona_display(self) -> str:
        for pid, name in PERSONA_LIST:
            if pid == self.persona_id:
                return name
        return self.persona_id.upper()

    def build_attach_payload(self) -> dict:
        temp_c, ram_pct = _read_pi5_metrics()
        sr, sg, sb = SPELL_COLORS.get(self.active_spell, (36, 204, 182))
        pid_num = next((i for i, (pid, _) in enumerate(PERSONA_LIST) if pid == self.persona_id), 0)
        return {
            "cmd": "celestial_pi5_attach",
            "host": "PI5-BRAIN",
            "persona": self._persona_display(),
            "persona_id": pid_num,
            "stt_engine": self.stt_engine,
            "tts_engine": self.tts_engine,
            "pi5_status": self.status,
            "pi5_heard": self.last_heard,
            "pi5_prophecy": self.prophecy,
            "pi5_spell": self.active_spell,
            "spell_r": sr,
            "spell_g": sg,
            "spell_b": sb,
            "pi5_temp": temp_c,
            "pi5_ram": ram_pct,
            "latency_ms": self.latency_ms,
        }

    def build_state_payload(self, cmd: str = "celestial_pi5_state") -> dict:
        payload = self.build_attach_payload()
        payload["cmd"] = cmd
        return payload

    @staticmethod
    def build_detach_payload() -> dict:
        return {"cmd": "celestial_pi5_detach"}

    def discover_esp32(self, timeout: float = 1.5) -> str | None:
        """Discovers ESP32-S3 via UDP CRICKET_DISCOVER broadcast, serial VID_303A/PID_1001, or HTTP fallback."""
        # 1. Check USB CDC Serial ports matching Espressif VID 303A / PID 1001
        try:
            import serial.tools.list_ports

            for p in serial.tools.list_ports.comports():
                hwid = (p.hwid or "").upper()
                if "303A" in hwid and ("1001" in hwid or "USB" in hwid):
                    self.serial_port_path = p.device
                    break
        except Exception:
            pass

        # 2. UDP broadcast CRICKET_DISCOVER to port 4210
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        try:
            sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
            sock.settimeout(timeout)
            targets = ["255.255.255.255"]
            if self.esp_ip:
                targets.insert(0, self.esp_ip)
            for tgt in targets:
                try:
                    sock.sendto(b"CRICKET_DISCOVER", (tgt, self.udp_port))
                except OSError:
                    pass
            deadline = time.time() + timeout
            while time.time() < deadline:
                try:
                    data, addr = sock.recvfrom(1024)
                    info = json.loads(data.decode("utf-8", errors="ignore"))
                    if info.get("device") == "ESP32-S3-AMOLED":
                        found_ip = info.get("ip") or addr[0]
                        self.esp_ip = found_ip
                        return found_ip
                except socket.timeout:
                    break
                except Exception:
                    continue
        finally:
            sock.close()

        # 3. Direct HTTP /api/diag probe fallback
        probe_ip = self.esp_ip or DEFAULT_ESP_IP
        try:
            with urllib.request.urlopen(f"http://{probe_ip}/api/diag", timeout=timeout) as r:
                if r.status == 200:
                    self.esp_ip = probe_ip
                    return probe_ip
        except Exception:
            pass

        return self.esp_ip

    def _send_json_udp(self, payload: dict) -> bool:
        if not self.esp_ip:
            return False
        raw = json.dumps(payload).encode("utf-8")
        if self._udp_sock is None:
            self._udp_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        try:
            self._udp_sock.sendto(raw, (self.esp_ip, self.udp_port))
            return True
        except OSError:
            return False

    def _send_json_serial(self, payload: dict) -> bool:
        if not self.serial_port_path:
            return False
        try:
            if self._serial_conn is None:
                import serial

                self._serial_conn = serial.Serial(self.serial_port_path, 115200, timeout=0.5)
            line = ("CELESTIAL_PI5_JSON:" + json.dumps(payload) + "\n").encode("utf-8")
            self._serial_conn.write(line)
            return True
        except Exception:
            return False

    def send_state(self, **fields) -> bool:
        with self._lock:
            for k, v in fields.items():
                if hasattr(self, k):
                    setattr(self, k, v)
            payload = self.build_state_payload()
        udp_ok = self._send_json_udp(payload)
        ser_ok = self._send_json_serial(payload)
        return udp_ok or ser_ok

    def attach(self, esp_ip: str | None = None, start_threads: bool = True) -> bool:
        if esp_ip:
            self.esp_ip = esp_ip
        if not self.esp_ip and not self.serial_port_path:
            self.discover_esp32()
        if not self.esp_ip:
            self.esp_ip = DEFAULT_ESP_IP

        payload = self.build_attach_payload()
        sent = self._send_json_udp(payload)
        ser_sent = self._send_json_serial(payload)
        self.attached = bool(sent or ser_sent)

        if start_threads:
            self._stop_event.clear()
            if self._hb_thread is None or not self._hb_thread.is_alive():
                self._hb_thread = threading.Thread(target=self._heartbeat_loop, daemon=True)
                self._hb_thread.start()
            if self._mic_thread is None or not self._mic_thread.is_alive():
                self._mic_thread = threading.Thread(target=self._mic_rx_loop, daemon=True)
                self._mic_thread.start()
        return self.attached

    def detach(self) -> None:
        self._stop_event.set()
        payload = self.build_detach_payload()
        self._send_json_udp(payload)
        self._send_json_serial(payload)
        self.attached = False
        for sock in (self._mic_sock, self._udp_sock):
            if sock is not None:
                try:
                    sock.close()
                except OSError:
                    pass
        self._mic_sock = None
        self._udp_sock = None
        if self._serial_conn is not None:
            try:
                self._serial_conn.close()
            except Exception:
                pass
            self._serial_conn = None

    def _heartbeat_loop(self) -> None:
        while not self._stop_event.wait(1.5):
            payload = self.build_state_payload(cmd="celestial_pi5_heartbeat")
            self._send_json_udp(payload)
            self._send_json_serial(payload)

    def _mic_rx_loop(self) -> None:
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        try:
            sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            sock.bind(("0.0.0.0", self.mic_udp_port))
            sock.settimeout(0.5)
            self._mic_sock = sock
        except OSError:
            sock.close()
            return

        while not self._stop_event.is_set():
            try:
                data, addr = sock.recvfrom(2048)
            except socket.timeout:
                continue
            except OSError:
                break

            if not self.esp_ip:
                self.esp_ip = addr[0]
            kind, seq, pcm_chunk = parse_cmic_packet(data)
            if kind == "CMIC" and pcm_chunk:
                self.mic_chunks.append((seq, pcm_chunk))
            elif kind == "CMIC_END":
                if self.mic_chunks:
                    ordered = [chunk for _, chunk in sorted(self.mic_chunks, key=lambda item: item[0])]
                    self.mic_chunks.clear()
                    full_pcm = b"".join(ordered)
                    threading.Thread(
                        target=self.process_mic_utterance,
                        args=(full_pcm,),
                        daemon=True,
                    ).start()

    def assemble_cmic_packets(self, packets: list[bytes]) -> bytes:
        """Assembles a sequence of 'CMIC' / 'CMIC_END' packets into contiguous 16 kHz mono PCM16LE."""
        frames: list[tuple[int, bytes]] = []
        for pkt in packets:
            kind, seq, pcm = parse_cmic_packet(pkt)
            if kind == "CMIC" and pcm:
                frames.append((seq, pcm))
            elif kind == "CMIC_END":
                break
        frames.sort(key=lambda item: item[0])
        return b"".join(chunk for _, chunk in frames)

    def process_mic_utterance(self, pcm_bytes: bytes) -> dict:
        """Runs STT -> LLM Prophecy -> TTS -> HUD update -> ESP32 speaker playback."""
        t0 = time.time()
        transcript = self.local_engine.transcribe_audio_bytes(pcm_bytes, sample_rate=16000)
        if not transcript:
            transcript = "(no speech recognized)"
        prophecy, spell = self.local_engine.generate_prophecy(transcript, persona_id=self.persona_id)
        tts_pcm = self.local_engine.synthesize_speech_pcm(prophecy, persona_id=self.persona_id)
        latency_ms = int((time.time() - t0) * 1000)

        self.send_state(
            last_heard=transcript[:95],
            prophecy=prophecy[:219],
            active_spell=(spell or "NONE")[:23],
            latency_ms=latency_ms,
            status="SPEAKING TTS...",
        )
        sent_bytes = self.stream_pcm_to_speaker(tts_pcm)
        self.send_state(status="ONLINE :5005")
        return {
            "transcript": transcript,
            "prophecy": prophecy,
            "spell": spell,
            "tts_bytes": len(tts_pcm),
            "speaker_bytes_sent": sent_bytes,
            "latency_ms": latency_ms,
        }

    def stream_pcm_to_speaker(
        self,
        pcm_bytes: bytes,
        esp_ip: str | None = None,
        chunk_size: int = 512,
        pace_sec: float = 0.012,
    ) -> int:
        """Streams 16 kHz 16-bit mono PCM over TCP port 4211 to the ESP32-S3 ES8311 speaker."""
        target_ip = esp_ip or self.esp_ip
        if not target_ip or not pcm_bytes:
            return 0
        total_sent = 0
        with socket.create_connection((target_ip, self.audio_tcp_port), timeout=4.0) as tcp:
            for off in range(0, len(pcm_bytes), chunk_size):
                chunk = pcm_bytes[off:off + chunk_size]
                tcp.sendall(chunk)
                total_sent += len(chunk)
                if pace_sec > 0:
                    time.sleep(pace_sec)
        return total_sent

    def query_http_json(self, path: str, timeout: float = 2.0) -> dict | None:
        if not self.esp_ip:
            return None
        try:
            with urllib.request.urlopen(f"http://{self.esp_ip}{path}", timeout=timeout) as r:
                return json.loads(r.read().decode("utf-8"))
        except Exception:
            return None


def run_demo(bridge: Esp32SatelliteBridge, hold_secs: float = 4.0) -> int:
    print(f"[PI5-SATELLITE] Discovering / attaching to ESP32-S3 ({bridge.esp_ip or 'auto'})...")
    ip = bridge.discover_esp32(timeout=1.5) or bridge.esp_ip or DEFAULT_ESP_IP
    bridge.attach(esp_ip=ip)
    print(f"[PI5-SATELLITE] Attached to {ip}:4210 -> ESP32 switched to PI 5 ORACLE HUD")

    try:
        demo_text = "All forty-three emitters online. Causal probability optimal."
        bridge.send_state(
            last_heard="Cast solar flare and report status",
            active_spell="Solar Flare",
            prophecy=demo_text,
            status="SPEAKING TTS...",
            latency_ms=142,
        )
        pcm = bridge.local_engine.synthesize_speech_pcm(demo_text, persona_id=bridge.persona_id)
        try:
            sent = bridge.stream_pcm_to_speaker(pcm)
            print(f"[PI5-SATELLITE] Streamed {sent} PCM bytes to {ip}:{bridge.audio_tcp_port}")
        except OSError as exc:
            print(f"[PI5-SATELLITE] Speaker TCP stream skipped ({exc})")

        mic_diag = bridge.query_http_json("/api/mic_test", timeout=2.5)
        if mic_diag:
            print(f"[PI5-SATELLITE] /api/mic_test -> {mic_diag}")
        diag = bridge.query_http_json("/api/diag", timeout=2.0)
        if diag:
            print(f"[PI5-SATELLITE] /api/diag -> {diag}")

        bridge.send_state(status="ONLINE :5005")
        time.sleep(max(0.5, hold_secs))
    finally:
        bridge.detach()
        print("[PI5-SATELLITE] Detached cleanly -> ESP32 returned to 12-Page PC Monitor")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description="Raspberry Pi 5 <-> ESP32-S3 AMOLED Satellite Bridge")
    parser.add_argument("--esp-ip", default=DEFAULT_ESP_IP, help=f"ESP32-S3 IP address (default {DEFAULT_ESP_IP})")
    parser.add_argument("--persona", default="machine_god", choices=[p[0] for p in PERSONA_LIST])
    parser.add_argument("--discover", action="store_true", help="Run discovery and print ESP32 IP/serial")
    parser.add_argument("--demo", action="store_true", help="Run non-destructive HUD + Speaker + Mic demo")
    parser.add_argument("--demo-secs", type=float, default=4.0, help="Seconds to hold HUD during --demo")
    args = parser.parse_args()

    bridge = Esp32SatelliteBridge(esp_ip=args.esp_ip, persona_id=args.persona)

    def _sig_handler(_signum, _frame):
        bridge.detach()
        sys.exit(0)

    signal.signal(signal.SIGINT, _sig_handler)
    if hasattr(signal, "SIGTERM"):
        signal.signal(signal.SIGTERM, _sig_handler)

    if args.discover:
        ip = bridge.discover_esp32(timeout=2.0)
        print(f"Discovered ESP32-S3: ip={ip}, serial={bridge.serial_port_path}")
        return 0

    if args.demo:
        return run_demo(bridge, hold_secs=args.demo_secs)

    bridge.attach(esp_ip=args.esp_ip)
    print(f"[PI5-SATELLITE] Running on UDP :{bridge.mic_udp_port} <-> ESP32 {bridge.esp_ip}. Ctrl+C to detach.")
    try:
        while True:
            time.sleep(1.0)
    except KeyboardInterrupt:
        pass
    finally:
        bridge.detach()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
