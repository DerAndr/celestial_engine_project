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
- Deep resonant TTS (DmitryNeural for Russian, ChristopherNeural for English)
- High-speed UDP streaming (2.5 ms pacing) with redundant start/end packets
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

MAGIC = 0x454C4543  # 'C' 'E' 'L' 'E'
VERSION = 1
PORT = 5005

MSG_HEARTBEAT   = 0x01
MSG_QUERY_START = 0x02
MSG_AUDIO_CHUNK = 0x03
MSG_QUERY_END   = 0x04
MSG_RESP_START  = 0x05
MSG_RESP_CHUNK  = 0x06
MSG_RESP_END    = 0x07
MSG_CANCEL      = 0x08
MSG_RESP_TEXT   = 0x09

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

HEADER_STRUCT = struct.Struct("<IBBHH")  # magic, version, msg_type, seq_num, payload_len
HEADER_SIZE = HEADER_STRUCT.size

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
        env_key = os.environ.get("OPENROUTER_API_KEY") or os.environ.get("DEEPSEEK_API_KEY") or os.environ.get("OPENAI_API_KEY")
        if env_key:
            default_config["api_key"] = env_key.strip().strip('"').strip("'")
    else:
        default_config["api_key"] = cfg_key

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
        target_lang = "ru-RU"  # Default auto priority: Russian first

    try:
        text = r.recognize_google(audio, language=target_lang)
        if text and len(text.strip()) > 0:
            detected = detect_language_from_text(text) if lang_code == LANG_AUTO else target_lang
            return text.strip(), detected
    except Exception:
        pass

    if lang_code == LANG_AUTO:
        for cand in ["en-US", "de-DE"]:
            try:
                text = r.recognize_google(audio, language=cand)
                if text and len(text.strip()) > 0:
                    return text.strip(), cand
            except Exception:
                pass

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
        "CRITICAL INSTRUCTIONS: Answer in 1-2 punchy spoken sentences (strictly under 20-25 words). "
        "Do not use markdown, emojis, asterisks, bullet points, or quotes. Speak in the sharp, witty style of The Hitchhiker's Guide to the Galaxy."
    )

    print(f"  [LLM LIVE] Querying OpenRouter Core: {preset_name} ('{model}')...")
    try:
        client = OpenAI(base_url=api_base, api_key=api_key)
        resp = client.chat.completions.create(
            model=model,
            messages=[
                {"role": "system", "content": enforced_prompt},
                {"role": "user", "content": user_text}
            ],
            max_tokens=85,
            temperature=0.7
        )
        answer = resp.choices[0].message.content.strip()
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
            comm = edge_tts.Communicate(clean_text, voice, rate="+12%")
            await comm.save(temp_mp3)
            if os.path.exists(temp_mp3) and os.path.getsize(temp_mp3) > 0:
                saved = True
                break
        except Exception as e:
            print(f"  [TTS RETRY] Voice '{voice}' failed: {e}. Trying fallback...")
            await asyncio.sleep(0.4)

    if not saved or not os.path.exists(temp_mp3) or os.path.getsize(temp_mp3) == 0:
        print("  [TTS ERROR] Failed to synthesize audio with available voices.")
        return b""

    cmd = [
        "ffmpeg", "-y", "-i", temp_mp3,
        "-af", "silenceremove=stop_periods=-1:stop_duration=0.25:stop_threshold=-45dB",
        "-f", "s16le", "-ar", "16000", "-ac", "1", "pipe:1"
    ]
    proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)
    pcm_data, _ = proc.communicate()

    try:
        if os.path.exists(temp_mp3):
            os.remove(temp_mp3)
    except Exception:
        pass

    # No artificial clamp: streaming ring buffer on Pico 2 W handles unlimited audio length!
    return pcm_data

def process_query_worker(sock, addr, query_buffer, current_lang_code, current_model_preset):
    """Background worker: handles STT -> LLM -> TTS -> UDP streaming without blocking heartbeats!"""
    out_dir = os.path.dirname(os.path.abspath(__file__))
    query_wav_path = os.path.join(out_dir, "query.wav")

    # 1. Clean DC offset and save WAV
    cleaned_pcm = clean_pcm_samples(bytes(query_buffer))
    with wave.open(query_wav_path, "wb") as wf:
        wf.setnchannels(1)
        wf.setsampwidth(2)
        wf.setframerate(16000)
        wf.writeframes(cleaned_pcm)

    # 2. Transcribe (STT)
    lang_str = LANG_NAMES.get(current_lang_code, "EN")
    print(f"  [1/3 STT] Transcribing speech (Target: {lang_str})...")
    user_text, detected_lang = transcribe_audio(query_wav_path, current_lang_code)

    config = load_config()

    if not user_text:
        print("  [STT WARN] Speech not recognized or query was quiet.")
        ai_text = get_silence_answer(detected_lang)
    else:
        print(f"  [USER INPUT] \"{user_text}\" (Detected: {detected_lang})")
        # 3. Query Cloud LLM
        model_name = MODEL_PRESETS.get(current_model_preset, ("Custom", "Custom"))[1]
        print(f"  [2/3 LLM] Generating answer via {model_name}...")
        ai_text = query_llm(user_text, detected_lang, current_lang_code, current_model_preset, config)

    print(f"  [ORACLE RESPONSE] \"{ai_text}\"")

    # 4. Dynamically detect voice language based on generated ai_text!
    response_lang = detect_language_from_text(ai_text)
    print(f"  [3/3 TTS] Synthesizing speech for response dialect: {response_lang}...")
    resp_pcm = asyncio.run(synthesize_speech_pcm(ai_text, response_lang, config))
    duration_resp = len(resp_pcm) / 32000.0
    print(f"  [TTS READY] Generated {len(resp_pcm)} bytes PCM ({duration_resp:.2f} s). Streaming to staff speaker...")

    # 5. Send UTF-8 text transcript for OLED marquee ticker
    oled_text = to_oled_ascii(ai_text)
    text_bytes = oled_text.encode("ascii", "replace")[:500]
    text_pkt = HEADER_STRUCT.pack(MAGIC, VERSION, MSG_RESP_TEXT, 0, len(text_bytes)) + text_bytes
    for _ in range(2):
        sock.sendto(text_pkt, addr)
        time.sleep(0.004)

    # 6. Stream audio chunks into Pico 2 W circular ring buffer
    start_pkt = HEADER_STRUCT.pack(MAGIC, VERSION, MSG_RESP_START, 0, 0)
    sock.sendto(start_pkt, addr)
    time.sleep(0.005)
    sock.sendto(start_pkt, addr)  # Send twice for reliability
    time.sleep(0.008)

    chunk_size = 512
    resp_seq = 0
    for offset in range(0, len(resp_pcm), chunk_size):
        chunk = resp_pcm[offset:offset + chunk_size]
        pkt = HEADER_STRUCT.pack(MAGIC, VERSION, MSG_RESP_CHUNK, resp_seq, len(chunk)) + chunk
        sock.sendto(pkt, addr)
        resp_seq += 1
        time.sleep(0.013)  # 13 ms pacing (plays at 16 ms/chunk -> fills ring buffer smoothly without overflow)

    end_pkt = HEADER_STRUCT.pack(MAGIC, VERSION, MSG_RESP_END, resp_seq, 0)
    for _ in range(3):  # Send 3 times with tiny delay
        sock.sendto(end_pkt, addr)
        time.sleep(0.005)

    print("  [TRANSMISSION COMPLETE] Staff is speaking! Ready for next query.\n")

def main():
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind(("0.0.0.0", PORT))

    hostname = socket.gethostname()
    try:
        local_ips = socket.gethostbyname_ex(hostname)[2]
    except Exception:
        local_ips = ["127.0.0.1"]
    config = load_config()

    print("=" * 68)
    print("  CELESTIAL ENGINE Mk.V — AI BRAIN SERVER (Multi-Core OpenRouter)")
    print("=" * 68)
    print(f"  Listening on UDP Port: {PORT}")
    print(f"  Host IPs:              {', '.join(local_ips)}")
    print(f"  LLM Provider:          {config.get('api_base')}")
    api_key_set = bool(config.get("api_key") and config.get("api_key") != "YOUR_API_KEY_HERE")
    if api_key_set:
        masked = config.get("api_key")[:7] + "..." + config.get("api_key")[-4:]
        print(f"  OpenRouter Key:        {masked} [LIVE AI ACTIVE]")
    else:
        print("  OpenRouter Key:        [NOT SET — RUNNING OFFLINE FALLBACK]")
    print("  Staff AI Core Presets: [0] Grok 2  [1] DeepSeek V3  [2] Llama 3.3  [3] Ministral 8B")
    print(f"  Staff Dialects:        Russian [RU], English [EN], Deutsch [DE], Auto [AUTO]")
    print(f"  TTS Voices:            RU: {config.get('tts_voice_ru')}, EN: {config.get('tts_voice_en')}")
    print("=" * 68)
    print("Ready and listening for Pico 2 W queries (Hold QUERY on staff)...")

    query_buffer = bytearray()
    query_in_progress = False
    current_lang_code = LANG_RU
    current_model_preset = 0

    while True:
        try:
            data, addr = sock.recvfrom(2048)
            if len(data) < HEADER_SIZE:
                continue

            magic, version, msg_type, seq_num, payload_len = HEADER_STRUCT.unpack(data[:HEADER_SIZE])
            if magic != MAGIC or version != VERSION:
                continue

            payload = data[HEADER_SIZE:HEADER_SIZE + payload_len]

            # IMMEDIATE, NON-BLOCKING HEARTBEAT RESPONSE
            if msg_type == MSG_HEARTBEAT:
                resp = HEADER_STRUCT.pack(MAGIC, VERSION, MSG_HEARTBEAT, seq_num, 0)
                sock.sendto(resp, addr)

            elif msg_type == MSG_QUERY_START:
                query_in_progress = True
                query_buffer.clear()
                current_lang_code = payload[0] if len(payload) > 0 else LANG_RU
                current_model_preset = payload[1] if len(payload) > 1 else 0
                lang_name = LANG_NAMES.get(current_lang_code, f"Code {current_lang_code}")
                core_name = MODEL_PRESETS.get(current_model_preset, ("Custom", "Custom"))[1]
                print(f"\n[QUERY START] Client {addr[0]}:{addr[1]} | Dialect: {lang_name} | AI Core: {core_name}")
                print("  Live streaming audio buffers into memory in real time...")

            elif msg_type == MSG_AUDIO_CHUNK:
                if query_in_progress:
                    query_buffer.extend(payload)
                    duration_s = len(query_buffer) / 32000.0
                    sys.stdout.write(f"\r  [STREAMING] {duration_s:.1f} s recorded ({len(query_buffer)} bytes)...")
                    sys.stdout.flush()

            elif msg_type == MSG_CANCEL:
                query_in_progress = False
                query_buffer.clear()
                print(f"\n[QUERY CANCELLED] Query discarded (too short or silent).")

            elif msg_type == MSG_QUERY_END:
                query_in_progress = False
                total_s = len(query_buffer) / 32000.0
                print(f"\n[QUERY END] Recorded {total_s:.2f} s ({len(query_buffer)} bytes). Processing immediately...")

                if len(query_buffer) == 0:
                    continue

                # Launch processing in a BACKGROUND THREAD so heartbeats keep responding!
                t = threading.Thread(
                    target=process_query_worker,
                    args=(sock, addr, bytearray(query_buffer), current_lang_code, current_model_preset),
                    daemon=True
                )
                t.start()

        except KeyboardInterrupt:
            print("\nShutting down AI Brain Server.")
            break
        except Exception as e:
            print(f"[ERROR] {e}")

if __name__ == "__main__":
    main()
