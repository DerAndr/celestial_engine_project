"""Run actual firmware C with hardware stubs, plus offline voice tooling tests.

From repository root: python -m unittest discover -s tests -v
Requires installed Visual Studio C build tools on Windows (no downloads).
"""
import ctypes
import os
from pathlib import Path
import subprocess
import shutil
import tempfile
import importlib.util
import struct
import wave
import unittest

ROOT = Path(__file__).resolve().parents[1]


def build_harness():
    vswhere = Path(os.environ.get('ProgramFiles(x86)', 'C:/Program Files (x86)')) / 'Microsoft Visual Studio/Installer/vswhere.exe'
    install = subprocess.check_output([str(vswhere), '-latest', '-products', '*', '-requires',
                                      'Microsoft.VisualStudio.Component.VC.Tools.x86.x64',
                                      '-property', 'installationPath'], text=True).strip()
    if not install:
        raise RuntimeError('Installed Visual Studio C compiler required for firmware host tests')
    devcmd = Path(install) / 'Common7/Tools/VsDevCmd.bat'
    env_dump = subprocess.check_output(f'"{devcmd}" -arch=x64 -no_logo && set', shell=True, text=True, errors='replace')
    env = os.environ.copy()
    for line in env_dump.splitlines():
        if '=' in line:
            key, value = line.split('=', 1)
            if key and not key.startswith('='):
                env[key.upper()] = value
    build = ROOT / 'firmware/build/host-tests'
    build.mkdir(parents=True, exist_ok=True)
    includes = build / 'include'
    for name in ['pico/time.h', 'pico/stdlib.h', 'pico/bootrom.h', 'pico/cyw43_arch.h', 'hardware/gpio.h',
                 'hardware/i2c.h', 'hardware/pio.h', 'hardware/dma.h', 'hardware/clocks.h',
                 'hardware/sync.h', 'audio_i2s.pio.h', 'mic_i2s.pio.h', 'ws2812.pio.h']:
        path = includes / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text('#include "pico_stubs.h"\n', encoding='utf-8')
    compiler = shutil.which('cl.exe', path=env['PATH'])
    if not compiler:
        raise RuntimeError('Visual Studio developer environment did not provide cl.exe')
    cmd = [compiler, '/nologo', '/std:c11', '/LD', '/O2', f'/I{includes}', f'/I{ROOT / "tests"}',
           f'/I{ROOT / "firmware/src"}', str(ROOT / 'tests/firmware_harness.c'),
           f'/Fe:{build / "firmware_harness.dll"}', f'/Fo:{build / "firmware_harness.obj"}']
    result = subprocess.run(cmd, cwd=build, env=env, capture_output=True, text=True, errors='replace')
    if result.returncode:
        raise RuntimeError(result.stdout + result.stderr)
    return ctypes.CDLL(str(build / 'firmware_harness.dll'))


class FirmwareRegressions(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        try:
            cls.firmware = build_harness()
        except Exception as e:
            raise unittest.SkipTest(f"Firmware harness requires MSVC build tools: {e}")

    def probe(self, name):
        self.assertEqual(getattr(self.firmware, name)(), 0, name)

    def test_query_release_bounce_does_not_end_hold(self): self.probe('query_bounce_probe')
    def test_query_held_at_boot_is_detected(self): self.probe('query_boot_hold_probe')
    def test_encoder_captures_cycle_during_display_stall(self): self.probe('encoder_display_stall_probe')
    def test_back_to_back_led_frames_keep_every_pixel(self): self.probe('led_frames_probe')
    def test_rainbow_hue_does_not_wrap_inside_sector(self): self.probe('hsv_probe')
    def test_led_brightness_cannot_exceed_power_cap(self): self.probe('led_brightness_probe')
    def test_oled_transactions_have_timeouts(self): self.probe('oled_timeout_probe')
    def test_oled_init_reports_frame_transfer_failure(self): self.probe('oled_init_failure_probe')
    def test_oled_scan_ignores_unrelated_i2c_device(self): self.probe('oled_scan_probe')
    def test_short_blip_after_idle_is_not_slept_before_playback(self): self.probe('audio_short_wake_probe')
    def test_invalid_negative_voice_id_is_rejected(self): self.probe('voice_invalid_probe')
    def test_full_negative_mic_sample_sets_peak(self): self.probe('mic_peak_probe')
    def test_mic_peak_is_independent_of_ring_wrap_position(self): self.probe('mic_ring_order_probe')
    def test_mic_retries_snapshot_when_dma_cursor_moves(self): self.probe('mic_live_cursor_probe')
    def test_failed_cyw43_init_is_not_used_by_heartbeat(self): self.probe('main_cyw43_failure_probe')
    def test_encoder_switch_during_listening_does_not_start_music(self): self.probe('main_listening_switch_probe')
    def test_query_release_does_not_claim_unimplemented_upload(self): self.probe('main_query_release_probe')
    def test_boot_chime_is_serviced_before_voice_announcement(self): self.probe('main_boot_sequence_probe')
    def test_boot_announcement_eventually_plays_after_chime(self): self.probe('main_boot_announcement_probe')


class VoiceToolRegressions(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        spec = importlib.util.spec_from_file_location('process_voices', ROOT / 'tools/process_voices.py')
        cls.voices = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(cls.voices)

    def convert_fixture(self, channels, width, frames):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / 'fixture.wav'
            with wave.open(str(path), 'wb') as wav:
                wav.setnchannels(channels)
                wav.setsampwidth(width)
                wav.setframerate(16000)
                wav.writeframes(frames)
            return self.voices.resample_and_convert(str(path))

    def test_eight_bit_stereo_opposite_channels_cancel(self):
        self.assertEqual(self.convert_fixture(2, 1, bytes([255, 1]) * 400), [0])

    def test_unsupported_pcm_width_is_rejected_instead_of_silent_voice(self):
        with self.assertRaises(ValueError):
            self.convert_fixture(1, 3, bytes([0, 0, 100]) * 400)

    def test_short_clip_fades_both_endpoints_to_zero(self):
        samples = self.convert_fixture(1, 2, struct.pack('<8h', *([10000]*8)))
        self.assertEqual((samples[0], samples[-1]), (0, 0))

    def test_import_does_not_generate_or_require_audio_assets(self):
        with tempfile.TemporaryDirectory() as folder:
            env = os.environ.copy()
            env['PYTHONPATH'] = str(ROOT / 'tools')
            result = subprocess.run(['python', '-c', 'import process_voices'], cwd=folder,
                                    env=env, capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(list(Path(folder).iterdir()), [])


class MapRegressions(unittest.TestCase):
    def scenario(self, name):
        node = shutil.which('node')
        if not node:
            self.skipTest('Node.js required for map JavaScript tests')
        result = subprocess.run([node, str(ROOT / 'tests/map_harness.js'), name],
                                capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_drawer_heading_is_milestone_id(self): self.scenario('drawer')
    def test_switch_view_uses_explicit_event(self): self.scenario('view')
    def test_pin_filter_uses_explicit_event(self): self.scenario('filter')
    def test_state_simulator_uses_explicit_event(self): self.scenario('state')


if __name__ == '__main__':
    unittest.main()
