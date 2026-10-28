package com.celestial.companion.audio

import android.content.Context
import android.os.Build
import android.os.ParcelFileDescriptor
import android.speech.tts.TextToSpeech
import android.speech.tts.UtteranceProgressListener
import android.util.Log
import kotlinx.coroutines.CompletableDeferred
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import kotlinx.coroutines.withTimeoutOrNull
import java.io.File
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.util.Locale

private const val TAG = "StaffTtsSynthesizer"

class StaffTtsSynthesizer(private val context: Context) {
    private var tts: TextToSpeech? = null
    private val initDeferred = CompletableDeferred<Boolean>()
    private var isInitialized = false

    init {
        tts = TextToSpeech(context.applicationContext) { status ->
            if (status == TextToSpeech.SUCCESS) {
                isInitialized = true
                initDeferred.complete(true)
                try {
                    tts?.setSpeechRate(1.15f)
                    tts?.setPitch(0.98f)
                } catch (_: Exception) {}
                Log.i(TAG, "TextToSpeech initialized successfully (rate=1.15x)")
                // Pre-warm background synthesis pipeline to eliminate cold-start delay
                kotlinx.coroutines.CoroutineScope(Dispatchers.IO).launch {
                    try {
                        val cacheDir = context.externalCacheDir ?: context.cacheDir
                        val warmupFile = File(cacheDir, "tts_warmup.wav")
                        tts?.synthesizeToFile("Ready", null, warmupFile, "warmup")
                        kotlinx.coroutines.delay(250)
                        if (warmupFile.exists()) warmupFile.delete()
                        Log.d(TAG, "TTS pipeline pre-warmed successfully")
                    } catch (_: Exception) {}
                }
            } else {
                Log.w(TAG, "TextToSpeech initialization failed with status $status")
                initDeferred.complete(false)
            }
        }
    }

    suspend fun synthesizeTo16kMonoPcm(text: String, locale: Locale = Locale.getDefault()): ByteArray? = withContext(Dispatchers.IO) {
        val ready = withTimeoutOrNull(7000) { initDeferred.await() } ?: false
        if (!ready || !isInitialized) {
            Log.w(TAG, "TTS not ready for synthesis (ready=$ready, isInitialized=$isInitialized)")
            return@withContext null
        }
        val engine = tts ?: return@withContext null

        try {
            val res = engine.setLanguage(locale)
            if (res == TextToSpeech.LANG_MISSING_DATA || res == TextToSpeech.LANG_NOT_SUPPORTED) {
                Log.w(TAG, "Locale $locale not fully supported (res=$res), falling back to base language: ${locale.language}")
                engine.setLanguage(Locale(locale.language))
            }
            engine.setSpeechRate(1.15f)
            engine.setPitch(0.98f)
        } catch (e: Exception) {
            Log.w(TAG, "Could not set TTS locale or rate: ${e.message}")
        }

        val cleanText = text
            .replace(Regex("<[^>]*>"), " ")
            .replace(Regex("[*#_`~]"), "")
            .trim()
        if (cleanText.isEmpty()) return@withContext null

        val curMs = System.currentTimeMillis()
        val cacheDir = context.externalCacheDir ?: context.cacheDir
        val outputFile = File(cacheDir, "staff_tts_$curMs.wav")
        val utteranceId = "staff_tts_$curMs"
        val doneDeferred = CompletableDeferred<Boolean>()
        try {
            if (outputFile.exists()) outputFile.delete()
            outputFile.createNewFile()
        } catch (_: Exception) {}

        engine.setOnUtteranceProgressListener(object : UtteranceProgressListener() {
            override fun onStart(id: String?) {
                Log.d(TAG, "TTS Utterance onStart: id=$id")
            }
            override fun onDone(id: String?) {
                Log.d(TAG, "TTS Utterance onDone: id=$id")
                if (id == utteranceId) doneDeferred.complete(true)
            }
            @Deprecated("Deprecated in Java")
            override fun onError(id: String?) {
                Log.w(TAG, "TTS Utterance onError: id=$id")
                if (id == utteranceId) doneDeferred.complete(false)
            }
            override fun onError(id: String?, errorCode: Int) {
                Log.w(TAG, "TTS Utterance onError with code: id=$id, errorCode=$errorCode")
                if (id == utteranceId) doneDeferred.complete(false)
            }
        })

        var pfd: ParcelFileDescriptor? = null
        val synthRes = try {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
                pfd = ParcelFileDescriptor.open(outputFile, ParcelFileDescriptor.MODE_READ_WRITE)
                engine.synthesizeToFile(cleanText, null, pfd, utteranceId)
            } else {
                engine.synthesizeToFile(cleanText, null, outputFile, utteranceId)
            }
        } catch (e: Exception) {
            Log.w(TAG, "Error invoking synthesizeToFile: ${e.message}")
            TextToSpeech.ERROR
        }

        if (synthRes != TextToSpeech.SUCCESS) {
            Log.w(TAG, "synthesizeToFile returned error code: $synthRes")
            try { pfd?.close() } catch (_: Exception) {}
            outputFile.delete()
            return@withContext null
        }

        val completed = withTimeoutOrNull(25000) { doneDeferred.await() } ?: false
        try { pfd?.close() } catch (_: Exception) {}

        if (!completed || !outputFile.exists() || outputFile.length() < 44) {
            Log.w(TAG, "TTS synthesis timed out or output file invalid (completed=$completed, exists=${outputFile.exists()}, len=${outputFile.length()})")
            outputFile.delete()
            return@withContext null
        }

        try {
            val wavBytes = outputFile.readBytes()
            outputFile.delete()
            val pcm = parseAndResampleWavTo16kMono(wavBytes)
            Log.i(TAG, "Synthesized '${cleanText.take(30)}...' -> ${pcm?.size ?: 0} bytes PCM")
            return@withContext pcm
        } catch (e: Exception) {
            Log.e(TAG, "Failed parsing synthesized WAV: ${e.message}", e)
            outputFile.delete()
            return@withContext null
        }
    }

    private fun parseAndResampleWavTo16kMono(wav: ByteArray): ByteArray? {
        if (wav.size < 44) return null
        if (wav[0].toInt() != 'R'.code || wav[1].toInt() != 'I'.code || wav[2].toInt() != 'F'.code || wav[3].toInt() != 'F'.code) {
            return null
        }
        if (wav[8].toInt() != 'W'.code || wav[9].toInt() != 'A'.code || wav[10].toInt() != 'V'.code || wav[11].toInt() != 'E'.code) {
            return null
        }

        var offset = 12
        var numChannels = 1
        var sampleRate = 16000
        var bitsPerSample = 16
        var audioFormat = 1
        var dataOffset = -1
        var dataLength = 0

        val bb = ByteBuffer.wrap(wav).order(ByteOrder.LITTLE_ENDIAN)

        while (offset + 8 <= wav.size) {
            val chunkId = String(wav, offset, 4, Charsets.US_ASCII)
            val chunkSize = bb.getInt(offset + 4)
            offset += 8

            if (chunkId == "fmt ") {
                if (chunkSize >= 16) {
                    audioFormat = bb.getShort(offset).toInt() and 0xFFFF
                    numChannels = bb.getShort(offset + 2).toInt() and 0xFFFF
                    sampleRate = bb.getInt(offset + 4)
                    bitsPerSample = bb.getShort(offset + 14).toInt() and 0xFFFF
                }
            } else if (chunkId == "data") {
                dataOffset = offset
                dataLength = chunkSize.coerceAtMost(wav.size - offset)
                break
            }
            offset += (chunkSize + (chunkSize and 1))
        }

        if (dataOffset < 0 || dataLength <= 0 || (audioFormat != 1 && audioFormat != 3)) {
            Log.w(TAG, "Unsupported WAV format: format=$audioFormat, bits=$bitsPerSample, channels=$numChannels, rate=$sampleRate")
            return null
        }

        val bytesPerSample = (bitsPerSample / 8).coerceAtLeast(1)
        val bytesPerFrame = bytesPerSample * numChannels
        val channelSamples = dataLength / bytesPerFrame
        if (channelSamples <= 0) return null

        val monoPcm = ShortArray(channelSamples)
        val dataBb = ByteBuffer.wrap(wav, dataOffset, dataLength).order(ByteOrder.LITTLE_ENDIAN)

        if (audioFormat == 3 && bitsPerSample == 32) {
            // 32-bit IEEE Float PCM
            for (i in 0 until channelSamples) {
                var sumFloat = 0.0f
                for (ch in 0 until numChannels) {
                    sumFloat += dataBb.float
                }
                val avg = sumFloat / numChannels
                monoPcm[i] = (avg.coerceIn(-1.0f, 1.0f) * 32767.0f).toInt().toShort()
            }
        } else if (bitsPerSample == 16) {
            // Standard 16-bit signed PCM
            for (i in 0 until channelSamples) {
                var sum = 0
                for (ch in 0 until numChannels) {
                    sum += dataBb.short.toInt()
                }
                monoPcm[i] = (sum / numChannels).coerceIn(-32768, 32767).toShort()
            }
        } else if (bitsPerSample == 24) {
            // 24-bit signed PCM
            for (i in 0 until channelSamples) {
                var sum = 0
                for (ch in 0 until numChannels) {
                    val b0 = dataBb.get().toInt() and 0xFF
                    val b1 = dataBb.get().toInt() and 0xFF
                    val b2 = dataBb.get().toInt()
                    val s24 = (b0) or (b1 shl 8) or (b2 shl 16)
                    sum += (s24 shr 8)
                }
                monoPcm[i] = (sum / numChannels).coerceIn(-32768, 32767).toShort()
            }
        } else if (bitsPerSample == 32) {
            // 32-bit integer PCM
            for (i in 0 until channelSamples) {
                var sum = 0L
                for (ch in 0 until numChannels) {
                    sum += dataBb.int
                }
                monoPcm[i] = ((sum / numChannels) shr 16).toInt().coerceIn(-32768, 32767).toShort()
            }
        } else {
            Log.w(TAG, "Unsupported PCM bit depth: $bitsPerSample")
            return null
        }

        val resampled = if (sampleRate == 16000) {
            monoPcm
        } else {
            val outLen = ((channelSamples.toLong() * 16000) / sampleRate).toInt()
            val out = ShortArray(outLen)
            val ratio = sampleRate.toDouble() / 16000.0
            for (i in 0 until outLen) {
                val srcPos = i * ratio
                val idx0 = srcPos.toInt().coerceIn(0, channelSamples - 1)
                val idx1 = (idx0 + 1).coerceIn(0, channelSamples - 1)
                val frac = srcPos - idx0
                out[i] = (monoPcm[idx0] * (1.0 - frac) + monoPcm[idx1] * frac).toInt().toShort()
            }
            out
        }

        // Peak volume normalization calibrated for clean, non-clipping MAX98357A 3W amp playback
        var maxPeak = 0
        for (s in resampled) {
            val absVal = kotlin.math.abs(s.toInt())
            if (absVal > maxPeak) maxPeak = absVal
        }
        if (maxPeak in 501..21999) {
            val scale = 22000.0 / maxPeak.toDouble()
            for (i in resampled.indices) {
                resampled[i] = (resampled[i] * scale).toInt().coerceIn(-32768, 32767).toShort()
            }
        }

        val outBytes = ByteArray(resampled.size * 2)
        val outBb = ByteBuffer.wrap(outBytes).order(ByteOrder.LITTLE_ENDIAN)
        for (s in resampled) {
            outBb.putShort(s)
        }
        return outBytes
    }

    fun stop() {
        try {
            tts?.stop()
        } catch (e: Exception) {
            Log.w(TAG, "TTS stop error: ${e.message}")
        }
    }

    fun shutdown() {
        try {
            tts?.stop()
            tts?.shutdown()
            tts = null
        } catch (e: Exception) {
            Log.w(TAG, "TTS shutdown error: ${e.message}")
        }
    }
}
