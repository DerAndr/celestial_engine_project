package com.celestial.companion.audio

import android.content.Context
import android.speech.tts.TextToSpeech
import android.speech.tts.UtteranceProgressListener
import android.util.Log
import kotlinx.coroutines.CompletableDeferred
import kotlinx.coroutines.Dispatchers
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
                Log.i(TAG, "TextToSpeech initialized successfully")
            } else {
                Log.w(TAG, "TextToSpeech initialization failed with status $status")
                initDeferred.complete(false)
            }
        }
    }

    suspend fun synthesizeTo16kMonoPcm(text: String, locale: Locale = Locale.getDefault()): ByteArray? = withContext(Dispatchers.IO) {
        val ready = withTimeoutOrNull(3000) { initDeferred.await() } ?: false
        if (!ready || !isInitialized) {
            Log.w(TAG, "TTS not ready for synthesis")
            return@withContext null
        }
        val engine = tts ?: return@withContext null

        try {
            engine.language = locale
        } catch (e: Exception) {
            Log.w(TAG, "Could not set TTS locale: ${e.message}")
        }

        val cleanText = text
            .replace(Regex("<[^>]*>"), " ")
            .replace(Regex("[*#_`~]"), "")
            .trim()
        if (cleanText.isEmpty()) return@withContext null

        val outputFile = File(context.cacheDir, "staff_tts_${System.currentTimeMillis()}.wav")
        val utteranceId = "staff_tts_${System.currentTimeMillis()}"
        val doneDeferred = CompletableDeferred<Boolean>()

        engine.setOnUtteranceProgressListener(object : UtteranceProgressListener() {
            override fun onStart(id: String?) {}
            override fun onDone(id: String?) {
                if (id == utteranceId) doneDeferred.complete(true)
            }
            override fun onError(id: String?) {
                if (id == utteranceId) doneDeferred.complete(false)
            }
        })

        val synthRes = engine.synthesizeToFile(cleanText, null, outputFile, utteranceId)
        if (synthRes != TextToSpeech.SUCCESS) {
            Log.w(TAG, "synthesizeToFile returned error: $synthRes")
            outputFile.delete()
            return@withContext null
        }

        val completed = withTimeoutOrNull(6000) { doneDeferred.await() } ?: false
        if (!completed || !outputFile.exists() || outputFile.length() < 44) {
            Log.w(TAG, "TTS synthesis timed out or output file invalid")
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
            offset += chunkSize
        }

        if (dataOffset < 0 || dataLength <= 0 || audioFormat != 1 || bitsPerSample != 16) {
            Log.w(TAG, "Unsupported WAV format: format=$audioFormat, bits=$bitsPerSample, channels=$numChannels, rate=$sampleRate")
            return null
        }

        val totalSamples = dataLength / 2
        val channelSamples = totalSamples / numChannels
        val monoPcm = ShortArray(channelSamples)

        val dataBb = ByteBuffer.wrap(wav, dataOffset, dataLength).order(ByteOrder.LITTLE_ENDIAN)
        for (i in 0 until channelSamples) {
            var sum = 0
            for (ch in 0 until numChannels) {
                sum += dataBb.short.toInt()
            }
            monoPcm[i] = (sum / numChannels).coerceIn(-32768, 32767).toShort()
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

        // Peak volume normalization for loud, crisp playback on MAX98357A 3W amp
        var maxPeak = 0
        for (s in resampled) {
            val absVal = kotlin.math.abs(s.toInt())
            if (absVal > maxPeak) maxPeak = absVal
        }
        if (maxPeak in 501..27999) {
            val scale = 28000.0 / maxPeak.toDouble()
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
