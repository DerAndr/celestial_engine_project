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

try:
    import local_ai_engine
    HAS_LOCAL_AI = local_ai_engine.is_local_ai_available()
except Exception:
    HAS_LOCAL_AI = False

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
    3: ("meta-llama/llama-3.3-70b-instruct", "Marvin (Llama 70B)"),
    4: ("local/qwen2.5:0.5b", "Qwen 0.5B Fast (Pi 5)"),
    5: ("local/gemma4:e2b", "Gemma 4 E2B (Pi 5)"),
    6: ("local/deepseek-r1:1.5b", "DeepSeek R1 (Pi 5)"),
    7: ("local/qwen2.5:1.5b", "Qwen 1.5B Core (Pi 5)")
}

LOCAL_MODELS_BY_PRESET = {
    4: "qwen2.5:0.5b",
    5: "gemma4:e2b",
    6: "deepseek-r1:1.5b",
    7: "qwen2.5:1.5b"
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

def save_config(config):
    """Persists config changes (such as adult_mode toggle) to brain_config.json."""
    try:
        with open(CONFIG_PATH, "w", encoding="utf-8") as f:
            json.dump(config, f, ensure_ascii=False, indent=2)
    except Exception as e:
        print(f"[CONFIG ERROR] Could not save {CONFIG_PATH}: {e}")

def check_adult_mode_toggle(user_text, detected_lang, config):
    """
    Checks if user is commanding to toggle adult mode (18+ / uncensored) vs decent mode.
    Returns (handled: bool, response_text: str or None)
    """
    if not user_text:
        return False, None
    norm = user_text.lower().strip()

    enable_patterns = [
        "enable adult mode", "turn on adult mode", "activate adult mode", "adult mode on", "unfiltered mode",
        "erwachsenenmodus an", "erwachsenenmodus aktivieren", "18 plus an",
        "включи режим для взрослых", "активируй режим для взрослых", "режим для взрослых",
        "включи 18+", "режим 18+", "включи восемнадцать плюс", "режим восемнадцать плюс",
        "разреши мат", "включи мат", "можно ругаться"
    ]
    disable_patterns = [
        "disable adult mode", "turn off adult mode", "family mode", "adult mode off", "safe mode",
        "erwachsenenmodus aus", "erwachsenenmodus deaktivieren", "jugendschutz an",
        "выключи режим для взрослых", "отключи режим для взрослых", "выключи 18+", "отключи 18+",
        "выключи восемнадцать плюс", "отключи восемнадцать плюс",
        "приличный режим", "детский режим", "запрети мат", "выключи мат", "нельзя ругаться"
    ]

    if any(p in norm for p in enable_patterns):
        config["adult_mode"] = True
        save_config(config)
        print("  [ADULT MODE] >> ACTIVATED by voice command (18+ UNRESTRICTED)")
        if "de" in detected_lang.lower():
            return True, "Achtung! Anstandsfilter deaktiviert. Erwachsenenmodus 18+ ist jetzt aktiv."
        elif "ru" in detected_lang.lower():
            return True, "Внимание! Протоколы приличия отключены. Режим для взрослых активирован. Можешь не сдерживать выражения."
        else:
            return True, "Attention! Decency filters disabled. Adult mode 18+ is now active."

    if any(p in norm for p in disable_patterns):
        config["adult_mode"] = False
        save_config(config)
        print("  [ADULT MODE] >> DEACTIVATED by voice command (PG-13 FAMILY SAFE)")
        if "de" in detected_lang.lower():
            return True, "Anstandsprotokolle wiederhergestellt. Höflichkeitsmodus ist jetzt aktiv."
        elif "ru" in detected_lang.lower():
            return True, "Протоколы приличия восстановлены. Режим вежливости и культурной речи активен."
        else:
            return True, "Decency protocols restored. Family-friendly polite mode is now active."

    # Check for local model switching commands
    gemma4_patterns = [
        "switch to gemma 4", "use gemma 4", "gemma 4 mode", "activate gemma 4", "gemma 4",
        "wechsel zu gemma 4", "gemma 4 aktivieren",
        "включи гемму 4", "переключи на гемму 4", "модель гемма 4", "включи gemma 4"
    ]
    gemma_patterns = [
        "switch to gemma", "use gemma", "gemma mode", "activate gemma",
        "wechsel zu gemma", "gemma aktivieren",
        "включи гемму", "переключи на гемму", "модель гемма", "переключись на гемму", "выбери гемму", "включи gemma"
    ]
    qwen_patterns = [
        "switch to qwen", "use qwen", "qwen mode", "activate qwen",
        "wechsel zu qwen", "qwen aktivieren",
        "включи квен", "переключи на квен", "модель квен", "переключись на квен", "выбери квен", "включи qwen"
    ]
    llama_patterns = [
        "switch to llama", "use llama", "llama mode", "activate llama",
        "wechsel zu llama", "llama aktivieren",
        "включи ламу", "переключи на ламу", "модель лама"
    ]

    if any(p in norm for p in gemma4_patterns):
        config["local_model"] = "gemma4:e2b"
        save_config(config)
        print("  [LOCAL MODEL] >> Switched to gemma4:e2b (Google Gemma 4 E2B)")
        if "de" in detected_lang.lower():
            return True, "Lokaler KI-Kern auf Google Gemma 4 E2B umgeschaltet."
        elif "ru" in detected_lang.lower():
            return True, "Локальное ядро переключено на Google Gemma 4 E2B."
        else:
            return True, "Local AI core switched to Google Gemma 4 E2B. Reasoning core active."

    if any(p in norm for p in gemma_patterns):
        config["local_model"] = "gemma2:2b"
        save_config(config)
        print("  [LOCAL MODEL] >> Switched to gemma2:2b (Google Gemma 2)")
        if "de" in detected_lang.lower():
            return True, "Lokaler KI-Kern auf Google Gemma 2 umgeschaltet."
        elif "ru" in detected_lang.lower():
            return True, "Локальное ядро переключено на Google Gemma 2. Превосходный выбор для космических изысканий."
        else:
            return True, "Local AI core switched to Google Gemma 2. Ready for queries."

    if any(p in norm for p in qwen_patterns):
        config["local_model"] = "qwen2.5:1.5b"
        save_config(config)
        print("  [LOCAL MODEL] >> Switched to qwen2.5:1.5b (Alibaba Qwen 2.5)")
        if "de" in detected_lang.lower():
            return True, "Lokaler KI-Kern auf Qwen 2.5 umgeschaltet."
        elif "ru" in detected_lang.lower():
            return True, "Локальное ядро переключено на Qwen 2.5. Высокоскоростной полиглот активен."
        else:
            return True, "Local AI core switched to Qwen 2.5. High-speed multilingual reasoning active."

    if any(p in norm for p in llama_patterns):
        config["local_model"] = "llama3.2:1b"
        save_config(config)
        print("  [LOCAL MODEL] >> Switched to llama3.2:1b (Meta Llama 3.2)")
        if "de" in detected_lang.lower():
            return True, "Lokaler KI-Kern auf Llama 3.2 umgeschaltet."
        elif "ru" in detected_lang.lower():
            return True, "Локальное ядро переключено на Llama 3.2."
        else:
            return True, "Local AI core switched to Llama 3.2."

    return False, None


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
        return "en-US"
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
    cands = ["en-US", "de-DE", "ru-RU"]
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
    if "de" in lang_tag.lower():
        return "Im Reiseführer steht: KEINE PANIK! Das Sub-Etha-Relais schweigt, aber die Antwort auf alle Fragen lautet noch immer zweiundvierzig."
    elif "ru" in lang_tag.lower():
        return "В Путеводителе сказано: Не паникуй! Связь с ядром прервана, но ответ на Главный Вопрос бытия всё ещё сорок два."
    else:
        return "The Guide says: DON'T PANIC! The sub-etha relay is temporarily offline, but the answer remains forty-two."

def get_silence_answer(lang_tag):
    if "de" in lang_tag.lower():
        return "Der Reiseführer hört nur kosmische Stille. Nimm dein Handtuch und frage erneut."
    elif "ru" in lang_tag.lower():
        return "Путеводитель слышит лишь тишину космоса. Проверь полотенце и нажми кнопку снова."
    else:
        return "The Guide hears only cosmic silence. Grab your towel and consult the guide again."

def query_llm(user_text, lang_tag, lang_code, model_preset, config, adult_mode=False):
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

    if adult_mode:
        if lang_code == LANG_EN or "en" in lang_tag.lower():
            sys_prompt = config.get("system_prompt_adult_en") or config.get("system_prompt_en")
        elif lang_code == LANG_DE or "de" in lang_tag.lower():
            sys_prompt = config.get("system_prompt_adult_de") or config.get("system_prompt_de")
        elif lang_code == LANG_RU or "ru" in lang_tag.lower():
            sys_prompt = config.get("system_prompt_adult_ru") or config.get("system_prompt_ru")
        else:
            sys_prompt = config.get("system_prompt_adult_en") or config.get("system_prompt_auto")
        decency_rule = "ADULT MODE (18+) ACTIVE: Raw, unfiltered, spicy humor, sarcasm, and profanity are permitted when contextually appropriate."
    else:
        if lang_code == LANG_EN or "en" in lang_tag.lower():
            sys_prompt = config.get("system_prompt_en")
        elif lang_code == LANG_DE or "de" in lang_tag.lower():
            sys_prompt = config.get("system_prompt_de")
        elif lang_code == LANG_RU or "ru" in lang_tag.lower():
            sys_prompt = config.get("system_prompt_ru")
        else:
            sys_prompt = config.get("system_prompt_auto")
        decency_rule = "STRICT DECENCY RULE: Never swear, use profanity, vulgarity, obscenities, or slurs. Keep your language clean, polite, and family-friendly."

    enforced_prompt = (
        f"{sys_prompt}\n"
        "CRITICAL INSTRUCTIONS: Give an enthusiastic, witty, and joyful spoken answer in 2-3 lively sentences (around 35-50 words). "
        f"{decency_rule} "
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
        config = load_config()
        force_local = config.get("force_local_ai", False)
        use_local_pipeline = (session.model in LOCAL_MODELS_BY_PRESET or session.model >= 4 or force_local) and HAS_LOCAL_AI

        # STT Selection: local Whisper.cpp is fast (0.9s), private, and offline; fallback to Google STT
        user_text = None
        detected_lang = "ru-RU"
        if HAS_LOCAL_AI:
            user_text, detected_lang = local_ai_engine.local_transcribe(wav_path, session.lang)
        if not user_text:
            user_text, detected_lang = transcribe_audio(wav_path, session.lang)

    if not transport.active(session):
        return
    if user_text:
        print(f"  [USER INPUT] \"{user_text}\" (Detected: {detected_lang})")

    # LLM Selection
    is_toggle, toggle_ans = check_adult_mode_toggle(user_text, detected_lang, config)
    if is_toggle:
        ai_text = toggle_ans
    else:
        adult_mode = config.get("adult_mode", False)
        mode_label = "18+ ADULT" if adult_mode else "PG-13 SAFE"
        if use_local_pipeline:
            local_model = LOCAL_MODELS_BY_PRESET.get(session.model, config.get("local_model", "qwen2.5:0.5b"))
            print(f"[REQUEST {session.request_id}] Querying Local Pi 5 Core ({local_model}) [{mode_label}]...")
            ai_text = (local_ai_engine.local_query_llm(user_text, detected_lang, adult_mode=adult_mode, model=local_model)
                       if user_text else get_silence_answer(detected_lang))
        else:
            preset_info = MODEL_PRESETS.get(session.model, ("cloud", "Cloud LLM"))[1]
            print(f"[REQUEST {session.request_id}] Querying OpenRouter Core: {preset_info} [{mode_label}]...")
            ai_text = (query_llm(user_text, detected_lang, session.lang, session.model, config, adult_mode=adult_mode)
                       if user_text else get_silence_answer(detected_lang))
            # If cloud returned connection fallback message and local AI is available, use local AI
            if user_text and HAS_LOCAL_AI and any(k in ai_text.lower() for k in ["temporarily offline", "sub-etha relay", "sub-etha-relais", "связь с ядром прервана"]):
                print(f"[REQUEST {session.request_id}] Cloud LLM unreachable; falling back to Local Pi 5 LLM...")
                local_ans = local_ai_engine.local_query_llm(user_text, detected_lang, adult_mode=adult_mode)
                if local_ans:
                    ai_text = local_ans

    if not transport.active(session):
        return
    print(f"  [ORACLE RESPONSE] \"{ai_text}\"")

    # Language selection: Honor staff dialect selection when not in AUTO mode
    if session.lang == LANG_RU:
        response_lang = "ru-RU"
    elif session.lang == LANG_DE:
        response_lang = "de-DE"
    elif session.lang == LANG_EN:
        response_lang = "en-US"
    else:
        response_lang = detect_language_from_text(ai_text)

    print(f"[REQUEST {session.request_id}] TTS ({response_lang})...")
    pcm = b""
    if use_local_pipeline:
        print(f"[REQUEST {session.request_id}] Synthesizing via Local Piper TTS...")
        pcm = local_ai_engine.local_synthesize_pcm(ai_text, response_lang)
    else:
        print(f"[REQUEST {session.request_id}] Synthesizing via Edge-TTS...")
        pcm = asyncio.run(synthesize_speech_pcm(ai_text, response_lang, config))
        if not pcm and HAS_LOCAL_AI:
            print(f"[REQUEST {session.request_id}] Cloud TTS failed; falling back to Local Piper TTS...")
            pcm = local_ai_engine.local_synthesize_pcm(ai_text, response_lang)

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
    force_local = config.get("force_local_ai", False)
    adult_mode = config.get("adult_mode", False)
    safety_label = "[18+ ADULT MODE ACTIVE]" if adult_mode else "[PG-13 FAMILY SAFE — NO PROFANITY]"
    local_m = config.get("local_model", "gemma2:2b")

    print("=" * 64)
    if force_local:
        print("  CELESTIAL ENGINE Mk.V — 100% AUTONOMOUS LOCAL AI SERVER")
    else:
        print("  CELESTIAL ENGINE Mk.V — HYBRID AI BRAIN (OpenRouter + Offline Pi 5)")
    print("=" * 64)
    print(f"  UDP Port     : {port} (Protocol v2) | Host IP: {', '.join(local_ips)}")
    print(f"  Cloud LLM    : OpenRouter (DeepSeek V3, Ministral 8B, Grok 4.5, Llama 70B)")
    print(f"  Local LLM    : Ollama -> {local_m} (Google Gemma 2 / Qwen 2.5)")
    print(f"  Local Audio  : Whisper.cpp (STT) + Piper Standalone (TTS)")
    print(f"  Routing Mode : {'100% FORCED OFFLINE' if force_local else 'Dynamic Dial (0-3: OpenRouter | 4: Pi 5 Local)'}")
    print(f"  Safety       : {safety_label}")
    print("=" * 64)
    print("Ready and listening for Pico 2 W queries (Hold QUERY on staff)...")

    try:
        BrainTransport(sock).serve(process_query_worker)
    except KeyboardInterrupt:
        print("\nShutting down AI Brain Server.")
    finally:
        sock.close()

if __name__ == "__main__":
    main()
