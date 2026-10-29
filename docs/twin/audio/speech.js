/**
 * Celestial Engine Mk.V — Browser TTS, Speech Recognition & Live Microphone Level Monitor
 * Provides:
 *  - Persona-tuned Web SpeechSynthesis output (en-US voice selection)
 *  - Interim + Final Web SpeechRecognition dictation
 *  - Real-time Web Audio API microphone level meter (getUserMedia + AnalyserNode)
 *    so the user always sees a clear visual indicator and live voice waveform when speaking.
 */

const PERSONA_VOICE_PROFILES = {
  machine_god: { rate: 1.0, pitch: 0.88 },
  archmage: { rate: 0.92, pitch: 0.82 },
  marvin: { rate: 0.88, pitch: 0.78 },
  cyberpunk: { rate: 1.08, pitch: 0.96 },
  pumpkin_lord: { rate: 1.04, pitch: 1.08 },
  custom: { rate: 1.0, pitch: 0.95 }
};

export function stopOracleSpeech() {
  if (typeof window !== 'undefined' && 'speechSynthesis' in window) {
    try {
      window.speechSynthesis.cancel();
    } catch (_) {}
  }
}

export function speakOracleText(text, personaId = 'machine_god', onComplete) {
  if (typeof window === 'undefined' || !('speechSynthesis' in window) || !text) {
    if (onComplete) onComplete();
    return;
  }

  try {
    window.speechSynthesis.cancel();
    const spokenText = String(text)
      .replace(/https?:\/\/(?:www\.)?linkedin\.com\/in\/andrey-holz\/?/gi, 'LinkedIn dot com, Andrey Holz')
      .replace(/https?:\/\/[^\s)]+/gi, 'link');
    const msg = new SpeechSynthesisUtterance(spokenText);
    msg.lang = 'en-US';

    const voices = window.speechSynthesis.getVoices();
    if (voices && voices.length > 0) {
      const matchedVoice = voices.find(v => (v.lang || '').toLowerCase().startsWith('en'));
      if (matchedVoice) {
        msg.voice = matchedVoice;
      }
    }

    const profile = PERSONA_VOICE_PROFILES[personaId] || PERSONA_VOICE_PROFILES.machine_god;
    msg.rate = profile.rate;
    msg.pitch = profile.pitch;
    msg.volume = 1.0;

    msg.onend = () => { if (onComplete) onComplete(); };
    msg.onerror = () => { if (onComplete) onComplete(); };

    window.speechSynthesis.speak(msg);
  } catch (err) {
    console.warn('Speech synthesis error:', err);
    if (onComplete) onComplete();
  }
}

export function isSpeechRecognitionSupported() {
  if (typeof window === 'undefined') return false;
  return Boolean(window.SpeechRecognition || window.webkitSpeechRecognition);
}

/**
 * Starts a unified voice capture session:
 * 1. Opens real microphone audio stream via getUserMedia + AnalyserNode to drive a live 12-band
 *    visual level meter and detect voice activity.
 * 2. Starts Web SpeechRecognition (if available in browser) with interimResults=true so live
 *    words appear immediately while speaking.
 */
export function startVoiceDictation({
  lang = 'auto',
  onInterim,
  onResult,
  onLevel,
  onError,
  onEnd
} = {}) {
  let stopped = false;
  let rec = null;
  let mediaStream = null;
  let audioCtx = null;
  let rafId = null;
  let autoStopTimer = null;
  let latestTranscript = '';
  let finalized = false;
  let hadVoiceEnergy = false;
  let startTime = Date.now();

  const SpeechRec = typeof window !== 'undefined' && (window.SpeechRecognition || window.webkitSpeechRecognition);

  function resolveLang() {
    return 'en-US';
  }

  function cleanupAudioMonitor() {
    if (rafId) {
      cancelAnimationFrame(rafId);
      rafId = null;
    }
    if (autoStopTimer) {
      clearTimeout(autoStopTimer);
      autoStopTimer = null;
    }
    if (mediaStream) {
      try {
        mediaStream.getTracks().forEach(t => t.stop());
      } catch (_) {}
      mediaStream = null;
    }
    if (audioCtx) {
      try {
        audioCtx.close();
      } catch (_) {}
      audioCtx = null;
    }
  }

  function finishSession(reasonError = null) {
    if (stopped) return;
    stopped = true;
    cleanupAudioMonitor();

    if (rec) {
      try { rec.onresult = null; rec.onerror = null; rec.onend = null; rec.stop(); } catch (_) {}
      rec = null;
    }

    const cleanTranscript = (latestTranscript || '').trim();
    if (!finalized && cleanTranscript) {
      finalized = true;
      if (onResult) onResult(cleanTranscript, { fromInterim: true, hadVoiceEnergy });
    } else if (!finalized && reasonError) {
      if (onError) onError(reasonError, { hadVoiceEnergy, durationMs: Date.now() - startTime });
    } else if (!finalized) {
      if (onError) onError('no-speech', { hadVoiceEnergy, durationMs: Date.now() - startTime });
    }

    if (onEnd) onEnd({ hadVoiceEnergy, transcript: cleanTranscript });
  }

  // 1. Start real-time microphone level monitor via getUserMedia
  if (typeof navigator !== 'undefined' && navigator.mediaDevices && navigator.mediaDevices.getUserMedia) {
    navigator.mediaDevices.getUserMedia({
      audio: {
        echoCancellation: true,
        noiseSuppression: true,
        autoGainControl: true
      }
    }).then((stream) => {
      if (stopped) {
        stream.getTracks().forEach(t => t.stop());
        return;
      }
      mediaStream = stream;
      const AudioCtx = window.AudioContext || window.webkitAudioContext;
      if (!AudioCtx) return;

      audioCtx = new AudioCtx();
      const source = audioCtx.createMediaStreamSource(stream);
      const analyser = audioCtx.createAnalyser();
      analyser.fftSize = 64;
      analyser.smoothingTimeConstant = 0.72;
      source.connect(analyser);

      const data = new Uint8Array(analyser.frequencyBinCount);
      const bands = new Array(12).fill(0);

      const tick = () => {
        if (stopped || !analyser) return;
        analyser.getByteFrequencyData(data);
        let peak = 0;
        for (let i = 0; i < 12; i++) {
          const v = (data[i + 1] || 0) / 255;
          bands[i] = Math.min(1, v * 1.35);
          if (bands[i] > peak) peak = bands[i];
        }
        if (peak > 0.14) {
          hadVoiceEnergy = true;
        }
        if (onLevel) {
          onLevel(bands, peak, hadVoiceEnergy);
        }
        rafId = requestAnimationFrame(tick);
      };
      rafId = requestAnimationFrame(tick);
    }).catch((err) => {
      // If getUserMedia is blocked, still allow SpeechRecognition or simulated level meter
      if (!SpeechRec && !stopped) {
        finishSession(err?.name || err?.message || 'mic-permission-denied');
      }
    });
  }

  // 2. Start Web SpeechRecognition if available (continuous mode up to 40s max duration)
  const MAX_RECORDING_MS = 40000;
  if (SpeechRec) {
    try {
      rec = new SpeechRec();
      rec.lang = resolveLang();
      rec.interimResults = true;
      rec.continuous = true;
      rec.maxAlternatives = 1;

      rec.onresult = (event) => {
        let fullText = '';
        let hasFinalChunk = false;
        for (let i = 0; i < event.results.length; i++) {
          const res = event.results[i];
          const txt = res?.[0]?.transcript || '';
          fullText += txt + ' ';
          if (res.isFinal) {
            hasFinalChunk = true;
          }
        }
        const combined = fullText.replace(/\s+/g, ' ').trim();
        if (combined) {
          latestTranscript = combined;
          hadVoiceEnergy = true;
          if (onInterim) onInterim(combined, hasFinalChunk);
        }
      };

      rec.onerror = (event) => {
        const errCode = event?.error || 'speech-error';
        // Ignore non-fatal 'no-speech' or 'network' while recording within the 40s window so the user can keep speaking
        if ((errCode === 'no-speech' || errCode === 'network') && !stopped && (Date.now() - startTime < MAX_RECORDING_MS)) {
          if (errCode === 'network' && mediaStream && !latestTranscript && onInterim) {
            onInterim('Capturing voice via microphone (INMP441 stream)... click Stop & Send to submit', false);
          }
          return;
        }
        if (latestTranscript.trim()) {
          finishSession(null);
          return;
        }
        finishSession(errCode);
      };

      rec.onend = () => {
        if (!stopped) {
          // Keep recognition active up to 40s until user clicks Stop & Send (or autoStopTimer fires)
          if (Date.now() - startTime < MAX_RECORDING_MS - 500) {
            try {
              rec.start();
              return;
            } catch (_) {
              // If restart is blocked (e.g. network SpeechRecognition disabled), keep mediaStream open until user clicks Stop & Send
              if (mediaStream) return;
            }
          }
          finishSession(null);
        }
      };

      rec.start();
    } catch (err) {
      if (!mediaStream) {
        finishSession(err?.message || 'speech-start-failed');
      }
    }
  } else {
    // Browser without Web SpeechRecognition (e.g. Firefox): keep getUserMedia live meter running for up to 40 seconds or until user clicks Stop
    if (onInterim) {
      onInterim('Microphone active — speak your question and click Stop & Send to submit...', false);
    }
  }

  // Safety auto-stop after 40 seconds (MAX_RECORDING_MS)
  autoStopTimer = setTimeout(() => {
    if (!stopped) {
      finishSession(null);
    }
  }, MAX_RECORDING_MS);

  return {
    stop: () => {
      finishSession(null);
    }
  };
}
