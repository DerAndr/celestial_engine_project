#!/usr/bin/env python3
"""
Celestial Engine Mk.V — Local Autonomous AI Engine
Provides 100% offline STT (whisper.cpp), LLM (Ollama), and TTS (Piper)
running locally on Raspberry Pi 5 without internet.
"""

import os
import subprocess
import json
import urllib.request
import urllib.error
import re
import time

LOCAL_AI_DIR = "/home/andr/local_ai"
WHISPER_BIN = os.path.join(LOCAL_AI_DIR, "whisper.cpp/build/bin/whisper-cli")
WHISPER_MODEL_TINY = os.path.join(LOCAL_AI_DIR, "whisper.cpp/models/ggml-tiny.bin")
WHISPER_MODEL_BASE = os.path.join(LOCAL_AI_DIR, "whisper.cpp/models/ggml-base.bin")

PIPER_BIN = os.path.join(LOCAL_AI_DIR, "piper/piper")
PIPER_VOICES_DIR = os.path.join(LOCAL_AI_DIR, "piper/voices")

PIPER_VOICES = {
    "ru-RU": os.path.join(PIPER_VOICES_DIR, "ru_RU-dmitri-medium.onnx"),
    "en-US": os.path.join(PIPER_VOICES_DIR, "en_US-lessac-medium.onnx"),
    "de-DE": os.path.join(PIPER_VOICES_DIR, "de_DE-thorsten-medium.onnx")
}

OLLAMA_URL = "http://127.0.0.1:11434/api/generate"
DEFAULT_LOCAL_MODEL = "qwen2.5:1.5b"  # High-speed multilingual polyglot (Best latency/quality on Pi 5)
AVAILABLE_LOCAL_MODELS = ["qwen2.5:1.5b", "gemma2:2b", "gemma4:e2b", "qwen2.5:0.5b", "llama3.2:1b"]

def get_configured_local_model():
    """Reads configured local model from brain_config.json, defaulting to gemma2:2b."""
    try:
        cfg_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "brain_config.json")
        with open(cfg_path, "r", encoding="utf-8") as f:
            data = json.load(f)
            return data.get("local_model", DEFAULT_LOCAL_MODEL)
    except Exception:
        return DEFAULT_LOCAL_MODEL

def is_local_ai_available():
    """Checks if whisper, piper, and ollama are ready on Pi 5."""
    if not (os.path.exists(WHISPER_BIN) and os.path.exists(WHISPER_MODEL_TINY)):
        return False
    if not (os.path.exists(PIPER_BIN) and os.path.exists(PIPER_VOICES["ru-RU"])):
        return False
    try:
        req = urllib.request.Request("http://127.0.0.1:11434/api/tags", headers={"User-Agent": "CelestialLocalAI"})
        with urllib.request.urlopen(req, timeout=1.5) as r:
            return r.status == 200
    except Exception:
        return False

def local_transcribe(wav_path, lang_id=None):
    """
    Transcribes audio offline via whisper.cpp.
    lang_id matches staff_lang_id_t: 0: EN, 1: AUTO, 2: DE, 3: RU
    Returns (transcribed_text, detected_lang_tag) or (None, lang_tag).
    """
    if not os.path.exists(WHISPER_BIN) or not os.path.exists(wav_path):
        return None, "en-US"

    model_path = WHISPER_MODEL_TINY if os.path.exists(WHISPER_MODEL_TINY) else WHISPER_MODEL_BASE
    
    # Map staff dialect setting to whisper language
    if lang_id == 0:
        whisper_lang = "en"
        forced_tag = "en-US"
    elif lang_id == 2:
        whisper_lang = "de"
        forced_tag = "de-DE"
    elif lang_id == 3:
        whisper_lang = "ru"
        forced_tag = "ru-RU"
    else:
        whisper_lang = "auto"
        forced_tag = None

    cmd = [
        WHISPER_BIN,
        "-m", model_path,
        "-f", wav_path,
        "-l", whisper_lang,
        "-nt"
    ]
    try:
        t0 = time.time()
        res = subprocess.run(cmd, capture_output=True, text=True, timeout=12)
        dur = time.time() - t0
        output = res.stdout

        if forced_tag:
            detected_lang = forced_tag
        else:
            detected_lang = "en-US"
            lang_match = re.search(r"auto-detected language:\s*([a-z]{2})", output)
            if lang_match:
                short_code = lang_match.group(1).lower()
                if short_code == "ru":
                    detected_lang = "ru-RU"
                elif short_code == "de":
                    detected_lang = "de-DE"
                else:
                    detected_lang = "en-US"

        # Filter out system diagnostic messages from whisper-cli
        raw_lines = [
            l.strip() for l in output.splitlines()
            if l.strip() and not l.startswith("whisper_") and not l.startswith("system_") 
            and not l.startswith("main:") and not l.startswith("read_audio")
        ]
        text = " ".join(raw_lines).strip()
        # Clean timestamps or bracketed text if any
        text = re.sub(r"\[\d+:\d+\.\d+ --> \d+:\d+\.\d+\]", "", text).strip()
        
        if text:
            print(f"  [LOCAL STT SUCCESS] \"{text}\" ({detected_lang}, {dur:.2f}s, whisper_lang={whisper_lang})")
            return text, detected_lang
        else:
            print(f"  [LOCAL STT SILENCE] No speech recognized ({dur:.2f}s, whisper_lang={whisper_lang})")
            return None, (forced_tag or "en-US")
    except Exception as e:
        print(f"  [LOCAL STT ERROR] whisper-cli failed: {e}")
        return None, (forced_tag or "en-US")

def local_query_llm(user_text, lang_tag, model_name=None, adult_mode=False):
    """
    Queries local Ollama on Pi 5.
    Returns witty Douglas Adams / retro-space sci-fi response.
    When adult_mode=False (default), strictly forbids profanity and vulgarity.
    When adult_mode=True, allows unfiltered adult humor and profanity.
    """
    model = model_name or get_configured_local_model()
    
    if "de" in lang_tag.lower():
        if adult_mode:
            system_instruction = (
                "Du bist der Reiseführer Per Anhalter durch die Galaxis ('KEINE PANIK'). "
                "WICHTIG: Deine Antwort MUSS STRENG AUF DEUTSCH SEIN! "
                "ERWACHSENEN-MODUS (18+) AKTIV: Scharfer, ungefilterter Humor, Flüche und Kraftausdrücke sind erlaubt. "
                "Antworte in 2 kurzen Sätzen (25-35 Wörter) ohne Markdown oder Emojis auf Deutsch."
            )
        else:
            system_instruction = (
                "Du bist der Reiseführer Per Anhalter durch die Galaxis (KEINE PANIK). "
                "WICHTIG: Deine Antwort MUSS STRENG AUF DEUTSCH SEIN! "
                "STRENGE ANSTANDSREGEL: Keinerlei Schimpfwörter, Vulgärsprache oder Obszönitäten. Sprich stets höflich und anständig. "
                "Antworte mit trockenem, geistreichem britischem Humor in genau 2 kurzen Sätzen (25-35 Wörter) "
                "ohne Markdown oder Emojis auf Deutsch."
            )
    elif "ru" in lang_tag.lower():
        if adult_mode:
            system_instruction = (
                "Ты — Электронный Путеводитель для автостопщиков по Галактике (НЕ ПАНИКУЙ). "
                "КРИТИЧЕСКИ ВАЖНО: Твой ответ ОБЯЗАТЕЛЬНО должен быть СТРОГО НА РУССКОМ ЯЗЫКЕ! "
                "РЕЖИМ 18+ (ДЛЯ ВЗРОСЛЫХ) АКТИВИРОВАН: Разрешён едкий юмор, сарказм, крепкое словцо, брань и нецензурная лексика. "
                "Отвечай дерзко и остроумно в 2 коротких предложениях (25-35 слов) без markdown и смайлов."
            )
        else:
            system_instruction = (
                "Ты — Электронный Путеводитель для автостопщиков по Галактике. На обложке написано: НЕ ПАНИКУЙ. "
                "КРИТИЧЕСКИ ВАЖНО: Твой ответ ОБЯЗАТЕЛЬНО должен быть СТРОГО НА РУССКОМ ЯЗЫКЕ! Не используй английские фразы. "
                "СТРОЖАЙШИЙ ЗАПРЕТ НА МАТ: Категорически запрещено использовать мат, нецензурную брань, пошлость, грубость и ругательства. Говори исключительно прилично, вежливо и культурно при любом вопросе! "
                "Отвечай остроумно, иронично, в британском духе Дугласа Адамса, строго в 2 коротких предложениях (25-35 слов) "
                "без разметки markdown, звездочек и смайлов. Упоминай ретро-космос, вакуумные лампы или ответ 42."
            )
    else:
        # Default: English
        if adult_mode:
            system_instruction = (
                "You are the Hitchhiker's Guide to the Galaxy ('DON'T PANIC'). "
                "IMPORTANT: Your answer MUST be in English. "
                "ADULT MODE (18+) ACTIVE: Raw, unfiltered, spicy humor, sarcasm, and profanity are permitted. "
                "Answer concisely in 2 sentences (approx 25-35 words) without markdown or emojis."
            )
        else:
            system_instruction = (
                "You are the Hitchhiker's Guide to the Galaxy. In large friendly letters on the cover are the words 'DON'T PANIC'. "
                "IMPORTANT: Your answer MUST be in English. "
                "STRICT DECENCY RULE: Never swear or use profanity, vulgarity, obscenities, or slurs. Speak politely and culturally at all times. "
                "Answer with dry, witty Douglas Adams British humor, concisely in 2 sentences (approx 25-35 words) "
                "without markdown, asterisks, or emojis. Be slightly absurd and hilarious."
            )

    full_prompt = f"{system_instruction}\nUser Query: {user_text}\nGuide Answer:"
    
    payload = {
        "model": model,
        "prompt": full_prompt,
        "stream": False,
        "keep_alive": -1,  # Keep warm in 16GB RAM permanently (zero cold-start penalty)
        "options": {
            "temperature": 0.7,
            "num_predict": 75,
            "num_ctx": 1024  # Compact context window for maximum CPU memory bandwidth on Cortex-A76
        }
    }

    try:
        t0 = time.time()
        req = urllib.request.Request(
            OLLAMA_URL,
            data=json.dumps(payload).encode("utf-8"),
            headers={"Content-Type": "application/json"}
        )
        with urllib.request.urlopen(req, timeout=25) as resp:
            data = json.loads(resp.read().decode("utf-8"))
            answer = data.get("response", "").strip()
            # Clean quotes and asterisks
            answer = answer.replace("*", "").replace("#", "").replace('`', '').replace('"', '').strip()
            dur = time.time() - t0
            eval_count = data.get("eval_count", 0)
            eval_dur = data.get("eval_duration", 1) / 1e9
            tok_per_sec = eval_count / eval_dur if eval_dur > 0 else 0
            print(f"  [LOCAL LLM SUCCESS] ({model}, {dur:.2f}s, {tok_per_sec:.1f} tok/s): \"{answer}\"")
            return answer
    except Exception as e:
        print(f"  [LOCAL LLM ERROR] Ollama query failed: {e}")
        if "ru" in lang_tag.lower():
            return "Не паникуй! Локальное ядро перегружено, но ответ на Главный Вопрос бытия всё ещё сорок два."
        elif "de" in lang_tag.lower():
            return "Keine Panik! Das lokale Rechenzentrum ist überlastet, aber die Antwort lautet noch immer 42."
        else:
            return "Don't Panic! The local positronic core is momentarily dizzy, but the answer remains forty-two."

def local_synthesize_pcm(text, lang_tag):
    """
    Synthesizes speech using local Piper TTS and converts to 16 kHz 16-bit mono PCM.
    Streams directly via ffmpeg without temporary disk files.
    """
    if "de" in lang_tag.lower():
        voice_path = PIPER_VOICES.get("de-DE")
    elif "ru" in lang_tag.lower():
        voice_path = PIPER_VOICES.get("ru-RU")
    else:
        voice_path = PIPER_VOICES.get("en-US")

    if not voice_path or not os.path.exists(voice_path) or not os.path.exists(PIPER_BIN):
        print("  [LOCAL TTS ERROR] Piper binary or voice model missing")
        return b""

    # Clean punctuation that can cause TTS glitches
    clean_text = text.replace('—', '-').replace('–', '-').replace('’', "'").replace('‘', "'")

    t0 = time.time()
    try:
        p_piper = subprocess.Popen(
            [PIPER_BIN, "--model", voice_path, "--output-raw"],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL
        )

        p_ffmpeg = subprocess.Popen(
            [
                "ffmpeg", "-y",
                "-f", "s16le", "-ar", "22050", "-ac", "1", "-i", "pipe:0",
                "-f", "s16le", "-ar", "16000", "-ac", "1", "pipe:1"
            ],
            stdin=p_piper.stdout,
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL
        )

        p_piper.stdin.write(clean_text.encode("utf-8"))
        p_piper.stdin.close()

        pcm_data, _ = p_ffmpeg.communicate(timeout=15)
        p_piper.wait()
        dur = time.time() - t0
        audio_dur = len(pcm_data) / 32000.0
        print(f"  [LOCAL TTS SUCCESS] {len(pcm_data)} PCM bytes ({audio_dur:.2f}s audio) synthesized in {dur:.2f}s")
        return pcm_data
    except Exception as e:
        print(f"  [LOCAL TTS ERROR] Piper/ffmpeg pipeline failed: {e}")
        return b""
