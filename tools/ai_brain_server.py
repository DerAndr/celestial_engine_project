#!/usr/bin/env python3
"""
Celestial Engine Mk.V — AI Brain Server
Receives live voice streaming over UDP from Pico 2 W,
performs Multilingual STT -> Cloud LLM -> Edge-TTS,
and streams synthesized speech directly into the staff speaker.

Key Features:
- Non-blocking UDP heartbeat responder (Pico never loses connection during LLM/TTS generation)
- Live synchronized model presets: Grok 2, DeepSeek V3, Llama 3.3, Ministral 8B
- Multilingual dialects: Russian [RU], English [EN], Deutsch [DE], Auto-Detect [AUTO]
- Configured multilingual Edge-TTS voices and full-screen ASCII transcript
- Protocol v2 response IDs, ACK/retry, absolute pacing and cancellable sessions
"""

import socket
import struct
import wave
import time
import os
import sys
import json
import asyncio
import subprocess
import threading
import tempfile
import math

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from brain_transport import BrainTransport, PORT

try:
    import speech_recognition as sr
    HAS_SR = True
except ImportError:
    HAS_SR = False

try:
    import edge_tts
    HAS_EDGE_TTS = True
except ImportError:
    HAS_EDGE_TTS = False

try:
    from openai import OpenAI
    HAS_OPENAI = True
except ImportError:
    HAS_OPENAI = False

try:
    from gtts import gTTS
    HAS_GTTS = True
except ImportError:
    HAS_GTTS = False

LANG_EN   = 0
LANG_AUTO = 1
LANG_DE   = 2
LANG_RU   = 3

LANG_NAMES = {
    LANG_EN:   "English [EN]",
    LANG_AUTO: "Auto-Detect [AUTO]",
    LANG_DE:   "Deutsch [DE]",
    LANG_RU:   "Russian [RU]"
}

MODEL_PRESETS = {
    0: ("deepseek/deepseek-chat", "Deep Thought (DeepSeek V3)"),
    1: ("mistralai/ministral-8b-2512", "Eddie (Ministral 8B)"),
    2: ("x-ai/grok-4.5", "Grok 4.5"),
    3: ("meta-llama/llama-3.3-70b-instruct", "Marvin (Llama 70B)")
}


CONFIG_PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)), "brain_config.json")

def load_config():
    default_config = {
        "api_base": "https://openrouter.ai/api/v1",
        "api_key": "",
        "model": "deepseek/deepseek-chat",
        "model_presets": {
            "0": "deepseek/deepseek-chat",
            "1": "mistralai/ministral-8b-2512",
            "2": "x-ai/grok-4.5",
            "3": "meta-llama/llama-3.3-70b-instruct"
        },
        "system_prompt_en": "You are the Hitchhiker's Guide to the Galaxy. In large friendly letters on the cover are the words 'DON'T PANIC'. Answer with dry, witty Douglas Adams British humor, concisely in 2-3 sentences (approx 35-40 words) without markdown, asterisks, or emojis. Always be slightly absurd, informative, and hilarious.",
        "system_prompt_de": "Du bist der Reiseführer 'Per Anhalter durch die Galaxis'. Auf dem Umschlag steht in großen, freundlichen Buchstaben 'KEINE PANIK'. Antworte mit trockenem, geistreichem britischem Humor nach Douglas Adams in 2-3 kurzen Sätzen (ca. 35-40 Wörter) ohne Markdown oder Emojis auf Deutsch.",
        "system_prompt_ru": "Ты — Электронный Путеводитель для автостопщиков по Галактике. На обложке крупно написано «НЕ ПАНИКУЙ». Отвечай остроумно, иронично, в британском духе Дугласа Адамса, строго в 2-3 предложениях (30-40 слов) без разметки markdown, смайлов и списков. Будь абсурдным, смешным и полезным автостопщику.",
        "system_prompt_auto": "You are the Hitchhiker's Guide to the Galaxy ('DON'T PANIC'). Answer with dry, witty Douglas Adams humor in 2-3 sentences (approx 35-40 words) without markdown or emojis, strictly in the same language as the user query.",
        "tts_voice_en": "en-US-ChristopherNeural",
        "tts_voice_de": "de-DE-ConradNeural",
        "tts_voice_ru": "ru-RU-SvetlanaNeural"
    }
    if os.path.exists(CONFIG_PATH):
        try:
            with open(CONFIG_PATH, "r", encoding="utf-8") as f:
                data = json.load(f)
                default_config.update(data)
        except Exception as e:
            print(f"[CONFIG WARN] Could not read {CONFIG_PATH}: {e}")

    cfg_key = default_config.get("api_key", "").strip().strip('"').strip("'")
    if not cfg_key or cfg_key == "YOUR_API_KEY_HERE":
        env_key = (os.environ.get("BRAIN_API_KEY") or
                   os.environ.get("OPENROUTER_API_KEY") or
                   os.environ.get("DEEPSEEK_API_KEY") or
                   os.environ.get("OPENAI_API_KEY"))
        if env_key:
            default_config["api_key"] = env_key.strip().strip('"').strip("'")
    else:
        default_config["api_key"] = cfg_key

    # Environment overrides for Cloud / Container deployments
    if os.environ.get("BRAIN_API_BASE"):
        default_config["api_base"] = os.environ.get("BRAIN_API_BASE")
    elif os.environ.get("OPENAI_BASE_URL"):
        default_config["api_base"] = os.environ.get("OPENAI_BASE_URL")

    if os.environ.get("BRAIN_MODEL"):
        default_config["model"] = os.environ.get("BRAIN_MODEL")

    return default_config


def clean_pcm_samples(raw_bytes):
    """Cancels microphone DC offset before Speech Recognition."""
    if len(raw_bytes) < 4:
        return raw_bytes
    count = len(raw_bytes) // 2
    try:
        samples = list(struct.unpack(f"<{count}h", raw_bytes[:count * 2]))
        mean_val = sum(samples) // count
        cleaned = [max(-32768, min(32767, s - mean_val)) for s in samples]
        return struct.pack(f"<{count}h", *cleaned)
    except Exception:
        return raw_bytes

def detect_language_from_text(text):
    if not text:
        return "ru-RU"
    if any("\u0400" <= c <= "\u04FF" for c in text):
        return "ru-RU"
    german_chars = set("äöüßÄÖÜ")
    if any(c in german_chars for c in text):
        return "de-DE"
    lower = text.lower().split()
    german_common = {"der", "die", "das", "und", "ist", "nicht", "ich", "du", "wir", "wie", "was"}
    if any(w in german_common for w in lower):
        return "de-DE"
    return "en-US"

def to_oled_ascii(text):
    """Converts unicode/cyrillic text to clean ASCII for OLED 5x7 display."""
    translit_map = {
        'а': 'a', 'б': 'b', 'в': 'v', 'г': 'g', 'д': 'd', 'е': 'e', 'ё': 'yo', 'ж': 'zh',
        'з': 'z', 'и': 'i', 'й': 'y', 'к': 'k', 'л': 'l', 'м': 'm', 'н': 'n', 'о': 'o',
        'п': 'p', 'р': 'r', 'с': 's', 'т': 't', 'у': 'u', 'ф': 'f', 'х': 'kh', 'ц': 'ts',
        'ч': 'ch', 'ш': 'sh', 'щ': 'shch', 'ъ': '', 'ы': 'y', 'ь': '', 'э': 'e', 'ю': 'yu',
        'я': 'ya',
        'А': 'A', 'Б': 'B', 'В': 'V', 'Г': 'G', 'Д': 'D', 'Е': 'E', 'Ё': 'Yo', 'Ж': 'Zh',
        'З': 'Z', 'И': 'I', 'Й': 'Y', 'К': 'K', 'Л': 'L', 'М': 'M', 'Н': 'N', 'О': 'O',
        'П': 'P', 'Р': 'R', 'С': 'S', 'Т': 'T', 'У': 'U', 'Ф': 'F', 'Х': 'Kh', 'Ц': 'Ts',
        'Ч': 'Ch', 'Ш': 'Sh', 'Щ': 'Shch', 'Ъ': '', 'Ы': 'Y', 'Ь': '', 'Э': 'E', 'Ю': 'Yu',
        'Я': 'Ya'
    }
    clean = text.replace('—', '-').replace('–', '-').replace('“', '"').replace('”', '"').replace('’', "'").replace('‘', "'")
    res = []
    for ch in clean:
        if ch in translit_map:
            res.append(translit_map[ch])
        elif 32 <= ord(ch) <= 126:
            res.append(ch)
        else:
            res.append(' ')
    return "".join(res)

def transcribe_audio(wav_path, lang_code):
    """Transcribes 16 kHz WAV audio to text using Google Web Speech."""
    if not HAS_SR:
        return None, "ru-RU"

    r = sr.Recognizer()
    r.operation_timeout = 15
    r.energy_threshold = 300
    r.dynamic_energy_threshold = True

    try:
        with sr.AudioFile(wav_path) as source:
            audio = r.record(source)
    except Exception as e:
        print(f"  [STT ERROR] Failed reading WAV: {e}")
        return None, "ru-RU"

    target_lang = "ru-RU"
    if lang_code == LANG_EN:
        target_lang = "en-US"
    elif lang_code == LANG_DE:
        target_lang = "de-DE"
    elif lang_code == LANG_RU:
        target_lang = "ru-RU"
    else:
        target_lang = "ru-RU"

    print(f"  [STT] Processing audio ({audio.sample_rate}Hz, {audio.sample_width}B/s). Target: {target_lang}...")
    try:
        text = r.recognize_google(audio, language=target_lang)
        if text and len(text.strip()) > 0:
            detected = detect_language_from_text(text)
            print(f"  [STT SUCCESS] \"{text}\" (detected: {detected})")
            return text.strip(), detected
    except sr.UnknownValueError:
        print(f"  [STT] Google Speech: No words recognized in {target_lang}")
    except sr.RequestError as e:
        print(f"  [STT NETWORK ERROR] Google Speech API request failed: {e}")
    except Exception as e:
        print(f"  [STT ERROR] {type(e).__name__}: {e}")

    # Try other languages if primary target language didn't match
    cands = ["ru-RU", "en-US", "de-DE"]
    if target_lang in cands:
        cands.remove(target_lang)
    for cand in cands:
        try:
            text = r.recognize_google(audio, language=cand)
            if text and len(text.strip()) > 0:
                detected = detect_language_from_text(text)
                print(f"  [STT SUCCESS FALLBACK] \"{text}\" (detected: {detected})")
                return text.strip(), detected
        except sr.UnknownValueError:
            pass
        except Exception as e:
            print(f"  [STT ERROR {cand}] {type(e).__name__}: {e}")

    print("  [STT FAILED] Audio evaluated as silence / no clear speech.")
    return None, target_lang

def get_fallback_answer(lang_tag):
    if "ru" in lang_tag.lower():
        return "В Путеводителе сказано: Не паникуй! Связь с ядром прервана, но ответ на Главный Вопрос бытия всё ещё сорок два."
    elif "de" in lang_tag.lower():
        return "Im Reiseführer steht: KEINE PANIK! Das Sub-Etha-Relais schweigt, aber die Antwort auf alle Fragen lautet noch immer zweiundvierzig."
    else:
        return "The Guide says: DON'T PANIC! The sub-etha relay is temporarily offline, but the answer remains forty-two."

def get_silence_answer(lang_tag):
    if "ru" in lang_tag.lower():
        return "Путеводитель слышит лишь тишину космоса. Проверь полотенце и нажми кнопку снова."
    elif "de" in lang_tag.lower():
        return "Der Reiseführer hört nur kosmische Stille. Nimm dein Handtuch und frage erneut."
    else:
        return "The Guide hears only cosmic silence. Grab your towel and consult the guide again."

def query_llm(user_text, lang_tag, lang_code, model_preset, config):
    """Sends prompt to configured LLM (OpenRouter: DeepSeek, Ministral, Grok, Llama)."""
    api_key = config.get("api_key", "").strip()
    if not api_key or api_key == "YOUR_API_KEY_HERE":
        print("  [LLM OFFLINE] No API key provided in brain_config.json. Using offline speech.")
        return get_fallback_answer(lang_tag)

    api_base = config.get("api_base", "https://openrouter.ai/api/v1")

    # Select model based on staff preset
    presets_cfg = config.get("model_presets", {})
    preset_str = str(model_preset)
    if preset_str in presets_cfg:
        model = presets_cfg[preset_str]
    elif model_preset in MODEL_PRESETS:
        model = MODEL_PRESETS[model_preset][0]
    else:
        model = config.get("model", "deepseek/deepseek-chat")

    preset_name = MODEL_PRESETS.get(model_preset, (model, "Custom"))[1]

    if lang_code == LANG_EN or "en" in lang_tag.lower():
        sys_prompt = config.get("system_prompt_en")
    elif lang_code == LANG_DE or "de" in lang_tag.lower():
        sys_prompt = config.get("system_prompt_de")
    elif lang_code == LANG_RU or "ru" in lang_tag.lower():
        sys_prompt = config.get("system_prompt_ru")
    else:
        sys_prompt = config.get("system_prompt_auto")

    enforced_prompt = (
        f"{sys_prompt}\n"
        "CRITICAL INSTRUCTIONS: Give an enthusiastic, witty, and joyful spoken answer in 2-3 lively sentences (around 35-50 words). "
        "Embrace retro-space sci-fi charm and playful cosmic irony (star charts, analog gauges, space anomalies, sub-space relays, 42). "
        "Do not use markdown, emojis, asterisks, bullet points, or quotes. Do not cut your thought short."
    )

    print(f"  [LLM LIVE] Querying OpenRouter Core: {preset_name} ('{model}')...")
    try:
        client = OpenAI(base_url=api_base, api_key=api_key, timeout=45, max_retries=0)
        resp = client.chat.completions.create(
            model=model,
            messages=[
                {"role": "system", "content": enforced_prompt},
                {"role": "user", "content": user_text}
            ],
            max_tokens=512,
            temperature=0.7
        )
        if resp.choices[0].finish_reason == "length":
            raise ValueError("Model exhausted answer budget; do not speak an unfinished sentence")
        answer = (resp.choices[0].message.content or "").strip()
        if not answer:
            raise ValueError("Model returned no spoken answer")
        answer = answer.replace("*", "").replace("#", "").replace('`', '').replace('"', '').strip()
        return answer
    except Exception as e:
        print(f"  [LLM ERROR] OpenRouter error on '{model}': {e}")
        return get_fallback_answer(lang_tag)

async def synthesize_speech_pcm(text, lang_tag, config):
    """Synthesizes text with edge-tts and converts to 16 kHz 16-bit mono PCM."""
    if "de" in lang_tag.lower():
        primary_voice = config.get("tts_voice_de", "de-DE-ConradNeural")
        fallback_voice = "de-DE-AmalaNeural"
    elif "ru" in lang_tag.lower():
        primary_voice = config.get("tts_voice_ru", "ru-RU-SvetlanaNeural")
        fallback_voice = "ru-RU-SvetlanaNeural"
    else:
        primary_voice = config.get("tts_voice_en", "en-US-ChristopherNeural")
        fallback_voice = "en-US-GuyNeural"

    # Avoid 403 on Dmitry:
    if "dmitry" in primary_voice.lower():
        primary_voice = "ru-RU-SvetlanaNeural"

    voices_to_try = [primary_voice]
    if fallback_voice not in voices_to_try:
        voices_to_try.append(fallback_voice)

    out_dir = os.path.dirname(os.path.abspath(__file__))
    temp_mp3 = os.path.join(out_dir, f"temp_resp_{threading.get_ident()}.mp3")

    # Sanitize dashes and curly quotes for Edge-TTS websocket stability
    clean_text = text.replace('—', '-').replace('–', '-').replace('’', "'").replace('‘', "'").replace('“', '"').replace('”', '"')

    saved = False
    for attempt, voice in enumerate(voices_to_try):
        try:
            comm = edge_tts.Communicate(clean_text, voice, rate="+12%", connect_timeout=10, receive_timeout=30)
            await comm.save(temp_mp3)
            if os.path.exists(temp_mp3) and os.path.getsize(temp_mp3) > 0:
                saved = True
                break
        except Exception as e:
            print(f"  [TTS RETRY] Voice '{voice}' failed: {e}. Trying fallback...")
            await asyncio.sleep(0.4)

    if not saved or not os.path.exists(temp_mp3) or os.path.getsize(temp_mp3) == 0:
        if HAS_GTTS:
            print(f"  [TTS FALLBACK] Edge-TTS unavailable for {lang_tag}. Generating via gTTS...")
            try:
                short_code = "ru" if "ru" in lang_tag.lower() else ("de" if "de" in lang_tag.lower() else "en")
                tts = gTTS(text=clean_text, lang=short_code)
                tts.save(temp_mp3)
                if os.path.exists(temp_mp3) and os.path.getsize(temp_mp3) > 0:
                    saved = True
            except Exception as ge:
                print(f"  [TTS ERROR] gTTS fallback failed: {ge}")

    if not saved or not os.path.exists(temp_mp3) or os.path.getsize(temp_mp3) == 0:
        print("  [TTS ERROR] Failed to synthesize audio with available engines.")
        return b""

    cmd = [
        "ffmpeg", "-y", "-i", temp_mp3,
        "-f", "s16le", "-ar", "16000", "-ac", "1", "pipe:1"
    ]
    proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)
    try:
        pcm_data, _ = proc.communicate(timeout=30)
    except subprocess.TimeoutExpired:
        proc.kill()
        proc.communicate()
        pcm_data = b""
    if proc.returncode != 0:
        pcm_data = b""

    try:
        if os.path.exists(temp_mp3):
            os.remove(temp_mp3)
    except Exception:
        pass

    # Full PCM is delivered with bounds, ordering and retries by brain_transport.
    return pcm_data

def process_query_worker(transport, session):
    """One captured request, one isolated WAV, no sends after cancellation."""
    if not transport.active(session):
        return
    with tempfile.TemporaryDirectory(prefix="celestial-query-") as directory:
        wav_path = os.path.join(directory, "query.wav")
        raw_pcm = bytes(session.pcm)
        session.pcm.clear()
        dur = len(raw_pcm) / 32000.0
        cleaned_pcm = clean_pcm_samples(raw_pcm)

        count = len(cleaned_pcm) // 2
        if count > 0:
            samples = list(struct.unpack(f"<{count}h", cleaned_pcm[:count * 2]))
            min_s = min(samples)
            max_s = max(samples)
            rms_s = int(math.sqrt(sum(s*s for s in samples) / count))
            print(f"[REQUEST {session.request_id}] {len(raw_pcm)} bytes ({dur:.2f}s). Stats: min={min_s}, max={max_s}, RMS={rms_s}")
        else:
            print(f"[REQUEST {session.request_id}] 0 bytes received!")

        try:
            with wave.open("/home/andr/last_query.wav", "wb") as wf_dbg:
                wf_dbg.setnchannels(1)
                wf_dbg.setsampwidth(2)
                wf_dbg.setframerate(16000)
                wf_dbg.writeframes(cleaned_pcm)
        except Exception:
            pass

        with wave.open(wav_path, "wb") as wf:
            wf.setnchannels(1)
            wf.setsampwidth(2)
            wf.setframerate(16000)
            wf.writeframes(cleaned_pcm)
        user_text, detected_lang = transcribe_audio(wav_path, session.lang)
    if not transport.active(session):
        return
    if user_text:
        print(f"  [USER INPUT] \"{user_text}\" (Detected: {detected_lang})")
    config = load_config()
    ai_text = (query_llm(user_text, detected_lang, session.lang, session.model, config)
               if user_text else get_silence_answer(detected_lang))
    if not transport.active(session):
        return
    print(f"  [ORACLE RESPONSE] \"{ai_text}\"")

    detected_resp_lang = detect_language_from_text(ai_text)
    if detected_resp_lang == "ru-RU":
        response_lang = "ru-RU"
    elif session.lang == LANG_DE or detected_resp_lang == "de-DE":
        response_lang = "de-DE"
    elif session.lang == LANG_EN and detected_resp_lang != "ru-RU":
        response_lang = "en-US"
    else:
        response_lang = detected_resp_lang

    print(f"[REQUEST {session.request_id}] TTS ({response_lang})...")
    pcm = asyncio.run(synthesize_speech_pcm(ai_text, response_lang, config))
    if not transport.active(session):
        return
    complete = transport.stream(session, pcm, to_oled_ascii(ai_text).encode("ascii", "replace"))
    print(f"[REQUEST {session.request_id}] {'Delivered' if complete else 'Cancelled/failed'}; {len(pcm)} PCM bytes")

def main():
    port = int(os.environ.get("BRAIN_PORT") or os.environ.get("PORT") or PORT)
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind(("0.0.0.0", port))

    hostname = socket.gethostname()
    try:
        local_ips = socket.gethostbyname_ex(hostname)[2]
    except Exception:
        local_ips = ["127.0.0.1"]
    config = load_config()

    print("=" * 68)
    print("  CELESTIAL ENGINE Mk.V — AI BRAIN SERVER (Multi-Core OpenRouter)")
    print("=" * 68)
    print(f"  Listening on UDP Port: {port}")

    print(f"  Host IPs:              {', '.join(local_ips)}")
    print(f"  LLM Provider:          {config.get('api_base')}")
    api_key_set = bool(config.get("api_key") and config.get("api_key") != "YOUR_API_KEY_HERE")
    if api_key_set:
        print("  API key:               [SET; LIVE AI ACTIVE]")
    else:
        print("  OpenRouter Key:        [NOT SET — RUNNING OFFLINE FALLBACK]")
    print("  Presets: [0] DeepSeek  [1] Ministral  [2] Grok  [3] Llama; UDP protocol v2")
    print(f"  Staff Dialects:        Russian [RU], English [EN], Deutsch [DE], Auto [AUTO]")
    print(f"  TTS Voices:            RU: {config.get('tts_voice_ru')}, EN: {config.get('tts_voice_en')}")
    print("=" * 68)
    print("Ready and listening for Pico 2 W queries (Hold QUERY on staff)...")

    try:
        BrainTransport(sock).serve(process_query_worker)
    except KeyboardInterrupt:
        print("\nShutting down AI Brain Server.")
    finally:
        sock.close()

if __name__ == "__main__":
    main()
