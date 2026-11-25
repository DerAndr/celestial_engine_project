package com.celestial.companion.audio

import ai.onnxruntime.OnnxJavaType
import ai.onnxruntime.OnnxTensor
import ai.onnxruntime.OrtEnvironment
import ai.onnxruntime.OrtSession
import ai.onnxruntime.TensorInfo
import android.content.Context
import android.util.Log
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.sync.Mutex
import kotlinx.coroutines.sync.withLock
import kotlinx.coroutines.withContext
import java.io.ByteArrayInputStream
import java.io.File
import java.io.FileInputStream
import java.io.FileOutputStream
import java.io.InputStream
import java.net.HttpURLConnection
import java.net.URL
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.nio.FloatBuffer
import java.nio.IntBuffer
import java.nio.LongBuffer
import java.util.Locale
import java.util.zip.ZipInputStream
import kotlin.math.abs

enum class KokoroDownloadState(val badge: String) {
    NOT_DOWNLOADED("CALIBRATED SYSTEM TTS + KOKORO PROFILE"),
    DOWNLOADING("DOWNLOADING KOKORO-82M INT8 ONNX..."),
    READY_ONNX("KOKORO-82M INT8 ONNX READY"),
    ERROR("DOWNLOAD FAILED (PROFILE FALLBACK)")
}

/**
 * Task 9.2: Genuine On-Device Kokoro-82M INT8 ONNX Neural Synthesizer (`24 kHz Float32` -> `16 kHz 16-bit mono PCM`).
 *
 * - Downloads `kokoro-v1.0.int8.onnx` (~88.1 MB) and `voices-v1.0.bin` (~26.9 MB NPZ archive) into `filesDir/sota_models/kokoro/`.
 * - Extracts the 256-dim voice style embedding row `[min(N, 510) - 1]` from `<voiceId>.npy` inside `voices-v1.0.bin`.
 * - Converts English text into Kokoro IPA phoneme token IDs (`178`-token `config.json` vocabulary, wrapped with `[0, *tokens, 0]`).
 * - Executes `kokoroSession.run(inputs)` (`tokens`/`input_ids`, `style [1, 256]`, `speed [1]`) and resamples the 24 kHz float32
 *   output to 16 kHz 16-bit signed mono PCM with 22,000 peak normalization for the staff's MAX98357A amplifier.
 */
class KokoroOnnxSynthesizer(
    private val context: Context? = null
) {
    companion object {
        private const val TAG = "KokoroOnnxSynthesizer"
        const val MODEL_FILENAME = "kokoro-v1.0.int8.onnx"
        const val VOICES_FILENAME = "voices-v1.0.bin"
        const val MAX_PHONEME_LENGTH = 510
        const val STYLE_DIM = 256
        private const val KOKORO_RELEASE_BASE =
            "https://github.com/thewh1teagle/kokoro-onnx/releases/download/model-files-v1.0"

        /**
         * Authoritative 178-token IPA & punctuation vocabulary from `kokoro-onnx/config.json`.
         */
        val KOKORO_VOCAB: Map<Char, Int> = mapOf(
            ';' to 1, ':' to 2, ',' to 3, '.' to 4, '!' to 5, '?' to 6,
            '—' to 9, '…' to 10, '"' to 11, '(' to 12, ')' to 13, '“' to 14, '”' to 15,
            ' ' to 16, '\u0303' to 17, 'ʣ' to 18, 'ʥ' to 19, 'ʦ' to 20, 'ʨ' to 21,
            'ᵝ' to 22, '\uAB67' to 23, 'A' to 24, 'I' to 25, 'O' to 31, 'Q' to 33,
            'S' to 35, 'T' to 36, 'W' to 39, 'Y' to 41, 'ᵊ' to 42,
            'a' to 43, 'b' to 44, 'c' to 45, 'd' to 46, 'e' to 47, 'f' to 48,
            'h' to 50, 'i' to 51, 'j' to 52, 'k' to 53, 'l' to 54, 'm' to 55,
            'n' to 56, 'o' to 57, 'p' to 58, 'q' to 59, 'r' to 60, 's' to 61,
            't' to 62, 'u' to 63, 'v' to 64, 'w' to 65, 'x' to 66, 'y' to 67, 'z' to 68,
            'ɑ' to 69, 'ɐ' to 70, 'ɒ' to 71, 'æ' to 72, 'β' to 75, 'ɔ' to 76,
            'ɕ' to 77, 'ç' to 78, 'ɖ' to 80, 'ð' to 81, 'ʤ' to 82, 'ə' to 83,
            'ɚ' to 85, 'ɛ' to 86, 'ɜ' to 87, 'ɟ' to 90, 'ɡ' to 92, 'ɥ' to 99,
            'ɨ' to 101, 'ɪ' to 102, 'ʝ' to 103, 'ɯ' to 110, 'ɰ' to 111, 'ŋ' to 112,
            'ɳ' to 113, 'ɲ' to 114, 'ɴ' to 115, 'ø' to 116, 'ɸ' to 118, 'θ' to 119,
            'œ' to 120, 'ɹ' to 123, 'ɾ' to 125, 'ɻ' to 126, 'ʁ' to 128, 'ɽ' to 129,
            'ʂ' to 130, 'ʃ' to 131, 'ʈ' to 132, 'ʧ' to 133, 'ʊ' to 135, 'ʋ' to 136,
            'ʌ' to 138, 'ɣ' to 139, 'ɤ' to 140, 'χ' to 142, 'ʎ' to 143, 'ʒ' to 147,
            'ʔ' to 148, 'ˈ' to 156, 'ˌ' to 157, 'ː' to 158, 'ʰ' to 162, 'ʲ' to 164,
            '↓' to 169, '→' to 171, '↗' to 172, '↘' to 173, 'ᵻ' to 177
        )

        private val SPELL_TAG_REGEX = Regex("\\[SPELL:[^\\]]*\\]", RegexOption.IGNORE_CASE)
        private val CLAUSE_SPLIT_REGEX = Regex("(?<=[.!?;:])\\s+")
        private val COMMA_SPLIT_REGEX = Regex("(?<=,)\\s+")

        /**
         * Task 9.7: Curated domain IPA pronunciation dictionary for Celestial Engine / Oracle terms.
         * Checked prior to general lexicon and rule-based G2P conversion.
         */
        val DOMAIN_IPA_LEXICON: Map<String, String> = mapOf(
            "celestial" to "səlˈɛstiəl",
            "omnissiah" to "ˌɑːmnɪsˈaɪə",
            "arcane" to "ɑːɹkˈeɪn",
            "grimoire" to "ɡɹɪmwˈɑːɹ",
            "prophecy" to "pɹˈɑːfəsi",
            "supernova" to "sˌuːpɚnˈoʊvə",
            "aegis" to "ˈiːdʒɪs",
            "fireball" to "fˈaɪɚbˌɔːl",
            "chrono" to "kɹˈoʊnoʊ",
            "nebula" to "nˈɛbjələ",
            "plasma" to "plˈæzmə",
            "machine" to "məʃˈiːn",
            "stars" to "stˈɑːɹz",
            "void" to "vˈɔɪd",
            "light" to "lˈaɪt",
            "shield" to "ʃˈiːld",
            "circuit" to "sˈɜːɹkɪt",
            "quantum" to "kwˈɑːntəm",
            "flux" to "flˈʌks",
            "warp" to "wˈɔːɹp",
            "staff" to "stˈæf",
            "oracle" to "ˈɔːɹəkəl",
            "spirit" to "spˈɪɹɪt",
            "blesses" to "blˈɛsɪz",
            "cosmos" to "kˈɑːzmoʊs",
            "gravity" to "ɡɹˈævɪti",
            "photon" to "fˈoʊtɑːn",
            "matrix" to "mˈeɪtɹɪks",
            "cyber" to "sˈaɪbɚ",
            "rune" to "ɹˈuːn",
            "runes" to "ɹˈuːnz",
            "aether" to "ˈiːθɚ",
            "telemetry" to "təlˈɛmɪtɹi",
            "nexus" to "nˈɛksəs"
        )

        private val LEXICON_IPA: Map<String, String> = mapOf(
            "the" to "ðə", "a" to "ə", "an" to "æn", "and" to "ænd", "of" to "ʌv",
            "to" to "tuː", "in" to "ɪn", "is" to "ɪz", "are" to "ɑːɹ", "you" to "juː",
            "your" to "jɔːɹ", "for" to "fɔːɹ", "with" to "wɪð", "on" to "ɑːn", "at" to "æt",
            "from" to "fɹʌm", "by" to "baɪ", "this" to "ðɪs", "that" to "ðæt", "it" to "ɪt",
            "not" to "nɑːt", "be" to "biː", "have" to "hæv", "has" to "hæz", "but" to "bʌt",
            "all" to "ɔːl", "one" to "wʌn", "two" to "tuː", "three" to "θɹiː", "four" to "fɔːɹ",
            "forty" to "fɔːɹti", "forty-two" to "fɔːɹtituː", "forty-three" to "fɔːɹtiθɹiː",
            "ninety-nine" to "naɪntinaɪn", "percent" to "pɚsˈɛnt", "quantum" to "kwˈɑːntəm",
            "probability" to "pɹɑːbəbˈɪlɪti", "matrices" to "mˈeɪtɹɪsiːz", "stable" to "stˈeɪbəl",
            "causal" to "kˈɔːzəl", "convergence" to "kənvˈɜːʤəns", "optimal" to "ˈɑːptɪməl",
            "trajectory" to "tɹəʤˈɛktəɹi", "telemetry" to "təlˈɛmɪtɹi", "nominal" to "nˈɑːmɪnəl",
            "staff" to "stˈæf", "celestial" to "səlˈɛsʧəl", "oracle" to "ˈɔːɹəkəl",
            "engine" to "ˈɛnʤɪn", "stars" to "stˈɑːɹz", "star" to "stˈɑːɹ", "light" to "lˈaɪt",
            "magic" to "mˈæʤɪk", "arcane" to "ɑːɹkˈeɪn", "spell" to "spˈɛl", "cast" to "kˈæst",
            "fireball" to "fˈaɪɚbɔːl", "lightning" to "lˈaɪtnɪŋ", "shield" to "ʃˈiːld",
            "brain" to "bɹˈeɪn", "planet" to "plˈænɪt", "diodes" to "dˈaɪoʊdz",
            "pumpkin" to "pˈʌmpkɪn", "halloween" to "hæloʊˈiːn", "night" to "nˈaɪt",
            "cybernetic" to "saɪbɚnˈɛtɪk", "neural" to "nˈʊɹəl", "neon" to "nˈiːɑːn"
        )

        /**
         * Task 9.7: Splits long prophecies into natural prosody clauses bounded by `maxCharsPerClause`
         * so Kokoro-82M never overflows its 510-phoneme context window and pauses cleanly at punctuation.
         */
        fun splitTextIntoKokoroClauses(text: String, maxCharsPerClause: Int = 180): List<String> {
            val cleaned = text
                .replace(SPELL_TAG_REGEX, " ")
                .replace(Regex("\\s+"), " ")
                .trim()
            if (cleaned.isEmpty()) return emptyList()

            val limit = maxCharsPerClause.coerceAtLeast(24)
            val result = ArrayList<String>()
            for (sentence in CLAUSE_SPLIT_REGEX.split(cleaned)) {
                val trimmed = sentence.trim()
                if (trimmed.isEmpty()) continue
                if (trimmed.length <= limit) {
                    result.add(trimmed)
                } else {
                    for (subClause in COMMA_SPLIT_REGEX.split(trimmed)) {
                        val sub = subClause.trim()
                        if (sub.isEmpty()) continue
                        if (sub.length <= limit) {
                            result.add(sub)
                        } else {
                            var current = StringBuilder()
                            for (word in sub.split(' ')) {
                                if (word.isEmpty()) continue
                                if (current.isNotEmpty() && current.length + 1 + word.length > limit) {
                                    result.add(current.toString())
                                    current = StringBuilder()
                                }
                                if (current.isNotEmpty()) current.append(' ')
                                current.append(word)
                            }
                            if (current.isNotEmpty()) {
                                result.add(current.toString())
                            }
                        }
                    }
                }
            }
            return result
        }

        /**
         * Converts English text into Kokoro IPA phoneme characters, checking [DOMAIN_IPA_LEXICON]
         * and [LEXICON_IPA] before falling back to rule-based G2P conversion.
         */
        fun englishToIpaPhonemes(text: String): String {
            if (text.isBlank()) return ""
            val normalized = text.trim()
                .replace("99.94", "ninety-nine point nine four")
                .replace("99.4", "ninety-nine point four")
                .replace("43", "forty-three")
                .replace("42", "forty-two")
                .replace("%", " percent")
            val ipaBuilder = StringBuilder(normalized.length * 2)
            val wordRegex = Regex("[A-Za-z'-]+|[^A-Za-z'-]+")
            for (m in wordRegex.findAll(normalized)) {
                val seg = m.value
                if (seg[0].isLetter()) {
                    val lower = seg.lowercase(Locale.US)
                    val dictHit = DOMAIN_IPA_LEXICON[lower] ?: LEXICON_IPA[lower]
                    if (dictHit != null) {
                        ipaBuilder.append(dictHit)
                    } else {
                        ipaBuilder.append(graphemeWordToIpa(lower))
                    }
                } else {
                    for (ch in seg) {
                        if (KOKORO_VOCAB.containsKey(ch)) {
                            ipaBuilder.append(ch)
                        } else if (ch.isWhitespace()) {
                            ipaBuilder.append(' ')
                        }
                    }
                }
            }
            return ipaBuilder.toString().replace(Regex("\\s+"), " ").trim()
        }

        /**
         * Rule-based English Grapheme-to-IPA converter and Kokoro tokenizer.
         * Produces token IDs in `[1..177]` (excluding the outer `0` padding).
         */
        fun englishToKokoroTokens(text: String): IntArray {
            val rawIpa = englishToIpaPhonemes(text)
            if (rawIpa.isEmpty()) return IntArray(0)
            val ids = ArrayList<Int>(rawIpa.length.coerceAtMost(MAX_PHONEME_LENGTH))
            for (ch in rawIpa) {
                if (ids.size >= MAX_PHONEME_LENGTH) break
                val id = KOKORO_VOCAB[ch]
                if (id != null) {
                    ids.add(id)
                }
            }
            return ids.toIntArray()
        }

        private fun graphemeWordToIpa(word: String): String {
            var w = word
            w = w.replace("tion", "ʃən")
                .replace("sion", "ʒən")
                .replace("ture", "ʧɚ")
                .replace("igh", "aɪ")
                .replace("tch", "ʧ")
                .replace("dge", "ʤ")
                .replace("ch", "ʧ")
                .replace("sh", "ʃ")
                .replace("th", "θ")
                .replace("ph", "f")
                .replace("wh", "w")
                .replace("ng", "ŋ")
                .replace("qu", "kw")
                .replace("ck", "k")
                .replace("ee", "iː")
                .replace("ea", "iː")
                .replace("oo", "uː")
                .replace("ai", "eɪ")
                .replace("ay", "eɪ")
                .replace("oa", "oʊ")
                .replace("ou", "aʊ")
                .replace("ow", "aʊ")
                .replace("ar", "ɑːɹ")
                .replace("er", "ɚ")
                .replace("ir", "ɜːɹ")
                .replace("or", "ɔːɹ")
                .replace("ur", "ɜːɹ")
            val sb = StringBuilder(w.length + 2)
            for (i in w.indices) {
                val c = w[i]
                if (c == 'e' && i == w.lastIndex && w.length > 3) continue
                when (c) {
                    'a' -> sb.append('æ')
                    'e' -> sb.append('ɛ')
                    'i' -> sb.append('ɪ')
                    'o' -> sb.append('ɑ')
                    'u' -> sb.append('ʌ')
                    'c' -> sb.append(if (i + 1 < w.length && (w[i + 1] == 'e' || w[i + 1] == 'i' || w[i + 1] == 'y')) 's' else 'k')
                    'g' -> sb.append('ɡ')
                    'j' -> sb.append('ʤ')
                    'r' -> sb.append('ɹ')
                    'y' -> sb.append(if (i == w.lastIndex) 'i' else 'j')
                    else -> sb.append(c)
                }
            }
            return sb.toString()
        }

        /**
         * Parses a NumPy `.npy` float32 stream (shape `[510, 1, 256]` or `[N, 256]`) and extracts
         * the 256-float style vector for `rowIndex = (phonemeLen.coerceIn(1, maxRows) - 1)`.
         */
        fun parseNpyStyleRow(npyBytes: ByteArray, phonemeLen: Int): FloatArray? {
            if (npyBytes.size < 128 + STYLE_DIM * 4) return null
            if (npyBytes[0] != 0x93.toByte() ||
                npyBytes[1].toInt().toChar() != 'N' ||
                npyBytes[2].toInt().toChar() != 'U' ||
                npyBytes[3].toInt().toChar() != 'M' ||
                npyBytes[4].toInt().toChar() != 'P' ||
                npyBytes[5].toInt().toChar() != 'Y'
            ) {
                return null
            }
            val major = npyBytes[6].toInt() and 0xFF
            val dataOffset = if (major <= 1) {
                val hdrLen = (npyBytes[8].toInt() and 0xFF) or ((npyBytes[9].toInt() and 0xFF) shl 8)
                10 + hdrLen
            } else {
                val hdrLen = ByteBuffer.wrap(npyBytes, 8, 4).order(ByteOrder.LITTLE_ENDIAN).int
                12 + hdrLen
            }
            val payloadBytes = npyBytes.size - dataOffset
            val rowBytes = STYLE_DIM * 4
            val totalRows = payloadBytes / rowBytes
            if (totalRows <= 0) return null

            val targetRow = (phonemeLen.coerceAtLeast(1) - 1).coerceIn(0, totalRows - 1)
            val rowByteOffset = dataOffset + targetRow * rowBytes
            if (rowByteOffset + rowBytes > npyBytes.size) return null

            val out = FloatArray(STYLE_DIM)
            val fb = ByteBuffer.wrap(npyBytes, rowByteOffset, rowBytes).order(ByteOrder.LITTLE_ENDIAN).asFloatBuffer()
            fb.get(out)
            return out
        }

        /**
         * Extracts `<voiceId>.npy` from a `voices-v1.0.bin` NPZ zip stream and returns the 256-dim style vector.
         */
        fun extractVoiceStyleFromNpzStream(npzStream: InputStream, voiceId: String, phonemeLen: Int): FloatArray? {
            val targetEntryName = "$voiceId.npy"
            var fallbackNpy: ByteArray? = null
            ZipInputStream(npzStream).use { zis ->
                while (true) {
                    val entry = zis.nextEntry ?: break
                    if (entry.name == targetEntryName || entry.name.endsWith("/$targetEntryName")) {
                        val npyBytes = zis.readBytes()
                        return parseNpyStyleRow(npyBytes, phonemeLen)
                    } else if (fallbackNpy == null && entry.name.endsWith(".npy")) {
                        fallbackNpy = zis.readBytes()
                    }
                }
            }
            return fallbackNpy?.let { parseNpyStyleRow(it, phonemeLen) }
        }

        /**
         * Linearly resamples 24 kHz Float32 waveform (`[-1.0f, 1.0f]`) to 16 kHz signed 16-bit little-endian
         * mono PCM bytes, calibrating peak amplitude to `targetPeak` (default 22000) to match MAX98357A headroom.
         */
        fun resampleFloat24kToPcm16kMono(samples24k: FloatArray, targetPeak: Int = 22000): ByteArray {
            if (samples24k.isEmpty()) return ByteArray(0)
            val outLen = ((samples24k.size.toLong() * 16000L) / 24000L).toInt().coerceAtLeast(1)
            val resampled = FloatArray(outLen)
            val ratio = samples24k.size.toDouble() / outLen.toDouble()
            var maxAbs = 0.0f

            for (i in 0 until outLen) {
                val srcPos = i * ratio
                val idx0 = srcPos.toInt().coerceIn(0, samples24k.size - 1)
                val idx1 = (idx0 + 1).coerceIn(0, samples24k.size - 1)
                val frac = (srcPos - idx0).toFloat()
                val v = samples24k[idx0] * (1.0f - frac) + samples24k[idx1] * frac
                resampled[i] = v
                val a = abs(v)
                if (a > maxAbs) maxAbs = a
            }

            val scale = if (maxAbs > 1e-4f) targetPeak.toFloat() / maxAbs else 0.0f
            val outBytes = ByteArray(outLen * 2)
            val bb = ByteBuffer.wrap(outBytes).order(ByteOrder.LITTLE_ENDIAN)
            for (i in 0 until outLen) {
                val s16 = (resampled[i] * scale).toInt().coerceIn(-32768, 32767).toShort()
                bb.putShort(s16)
            }
            return outBytes
        }
    }

    private val mutex = Mutex()
    private var ortEnv: OrtEnvironment? = null
    private var kokoroSession: OrtSession? = null
    private val voiceNpyCache = HashMap<String, ByteArray>(8)

    private val _state = MutableStateFlow(
        if (isReady()) KokoroDownloadState.READY_ONNX else KokoroDownloadState.NOT_DOWNLOADED
    )
    val state: StateFlow<KokoroDownloadState> = _state.asStateFlow()

    private val _downloadProgressPct = MutableStateFlow(if (isReady()) 100 else 0)
    val downloadProgressPct: StateFlow<Int> = _downloadProgressPct.asStateFlow()

    private fun getModelDir(): File? {
        val base = context?.filesDir ?: return null
        return File(base, "sota_models/kokoro")
    }

    fun isReady(): Boolean {
        val dir = getModelDir() ?: return false
        val model = File(dir, MODEL_FILENAME)
        val voices = File(dir, VOICES_FILENAME)
        return model.exists() && model.length() > 10_000_000L && voices.exists() && voices.length() > 1_000_000L
    }

    suspend fun downloadAndInit(): Boolean = withContext(Dispatchers.IO) {
        val dir = getModelDir() ?: return@withContext false
        if (!dir.exists()) dir.mkdirs()

        _state.value = KokoroDownloadState.DOWNLOADING
        _downloadProgressPct.value = 1

        val files = listOf(
            VOICES_FILENAME to 1_000_000L,
            MODEL_FILENAME to 10_000_000L
        )
        try {
            for ((index, entry) in files.withIndex()) {
                val (fileName, minBytes) = entry
                val targetFile = File(dir, fileName)
                if (!targetFile.exists() || targetFile.length() < minBytes) {
                    val conn = (URL("$KOKORO_RELEASE_BASE/$fileName").openConnection() as HttpURLConnection).apply {
                        connectTimeout = 15000
                        readTimeout = 60000
                        instanceFollowRedirects = true
                        setRequestProperty("User-Agent", "CelestialEngineMk5-Android/1.4.0")
                    }
                    val code = conn.responseCode
                    if (code !in 200..299) {
                        throw IllegalStateException("HTTP $code downloading $fileName")
                    }
                    val contentLen = conn.contentLengthLong.coerceAtLeast(1L)
                    val tmpFile = File(dir, "$fileName.part")
                    conn.inputStream.use { input ->
                        FileOutputStream(tmpFile).use { output ->
                            val buf = ByteArray(32 * 1024)
                            var downloaded = 0L
                            while (true) {
                                val read = input.read(buf)
                                if (read <= 0) break
                                output.write(buf, 0, read)
                                downloaded += read
                                val frac = (downloaded.toDouble() / contentLen.toDouble()).coerceIn(0.0, 1.0)
                                val pct = if (index == 0) (frac * 23.0).toInt().coerceIn(1, 23)
                                else (23.0 + frac * 76.0).toInt().coerceIn(23, 99)
                                _downloadProgressPct.value = pct
                            }
                        }
                    }
                    if (targetFile.exists()) targetFile.delete()
                    tmpFile.renameTo(targetFile)
                }
            }
            _downloadProgressPct.value = 100
            val loaded = ensureSessionLoaded()
            _state.value = if (loaded) KokoroDownloadState.READY_ONNX else KokoroDownloadState.ERROR
            loaded
        } catch (e: Exception) {
            Log.w(TAG, "Kokoro-82M ONNX download failed: ${e.message}", e)
            _state.value = KokoroDownloadState.ERROR
            false
        }
    }

    private suspend fun ensureSessionLoaded(): Boolean = mutex.withLock {
        if (kokoroSession != null) return true
        val dir = getModelDir() ?: return false
        if (!isReady()) return false
        return try {
            val env = ortEnv ?: OrtEnvironment.getEnvironment().also { ortEnv = it }
            val opts = OrtSession.SessionOptions().apply {
                setIntraOpNumThreads(4)
                setOptimizationLevel(OrtSession.SessionOptions.OptLevel.ALL_OPT)
            }
            kokoroSession = env.createSession(File(dir, MODEL_FILENAME).absolutePath, opts)
            _state.value = KokoroDownloadState.READY_ONNX
            Log.i(TAG, "Loaded Kokoro-82M INT8 ONNX session successfully")
            true
        } catch (t: Throwable) {
            Log.w(TAG, "Could not load Kokoro-82M ONNX session: ${t.message}")
            false
        }
    }

    private fun getVoiceStyleRow(voiceId: String, phonemeLen: Int): FloatArray? {
        val cachedNpy = synchronized(voiceNpyCache) { voiceNpyCache[voiceId] }
        if (cachedNpy != null) {
            return parseNpyStyleRow(cachedNpy, phonemeLen)
        }
        val dir = getModelDir() ?: return null
        val voicesFile = File(dir, VOICES_FILENAME)
        if (!voicesFile.exists()) return null
        return try {
            val targetEntry = "$voiceId.npy"
            var foundBytes: ByteArray? = null
            FileInputStream(voicesFile).use { fis ->
                ZipInputStream(fis).use { zis ->
                    while (true) {
                        val entry = zis.nextEntry ?: break
                        if (entry.name == targetEntry || entry.name.endsWith("/$targetEntry")) {
                            foundBytes = zis.readBytes()
                            break
                        }
                    }
                }
            }
            val npy = foundBytes ?: return null
            synchronized(voiceNpyCache) {
                voiceNpyCache[voiceId] = npy
            }
            parseNpyStyleRow(npy, phonemeLen)
        } catch (e: Exception) {
            Log.w(TAG, "Failed reading voice style $voiceId from voices-v1.0.bin: ${e.message}")
            null
        }
    }

    /**
     * Runs genuine on-device Kokoro-82M INT8 ONNX synthesis across prosody clauses:
     * 1. Splits text via [splitTextIntoKokoroClauses] to stay within the 510-phoneme context window.
     * 2. Converts each clause to IPA token IDs (`englishToKokoroTokens`) and pads with `0` (`[1, N+2]`).
     * 3. Executes `kokoroSession.run(inputs)`, resamples 24 kHz float32 -> 16 kHz PCM16 LE, and inserts
     *    `3200 bytes` (`100 ms` @ 16 kHz) of zero-PCM silence between clauses.
     */
    suspend fun synthesizeOrNull(text: String, voiceProfile: KokoroVoiceProfile): ByteArray? = withContext(Dispatchers.Default) {
        if (text.isBlank() || !isReady()) return@withContext null
        if (!ensureSessionLoaded()) return@withContext null
        val env = ortEnv ?: return@withContext null
        val session = kokoroSession ?: return@withContext null

        val clauses = splitTextIntoKokoroClauses(text)
        if (clauses.isEmpty()) return@withContext null
        if (clauses.size == 1) {
            return@withContext synthesizeSingleClauseOrNull(clauses[0], voiceProfile, env, session)
        }

        val combinedPcm = java.io.ByteArrayOutputStream()
        val interClauseSilence = ByteArray(3200) // 100 ms silence @ 16 kHz mono 16-bit PCM
        var synthesizedClauseCount = 0
        for (clause in clauses) {
            val clausePcm = synthesizeSingleClauseOrNull(clause, voiceProfile, env, session)
            if (clausePcm != null && clausePcm.isNotEmpty()) {
                if (synthesizedClauseCount > 0) {
                    combinedPcm.write(interClauseSilence)
                }
                combinedPcm.write(clausePcm)
                synthesizedClauseCount++
            }
        }
        if (synthesizedClauseCount == 0) null else combinedPcm.toByteArray()
    }

    private fun synthesizeSingleClauseOrNull(
        clause: String,
        voiceProfile: KokoroVoiceProfile,
        env: OrtEnvironment,
        session: OrtSession
    ): ByteArray? {
        val innerTokens = englishToKokoroTokens(clause)
        if (innerTokens.isEmpty()) return null
        val styleVec = getVoiceStyleRow(voiceProfile.voiceId, innerTokens.size) ?: return null

        val paddedTokensLong = LongArray(innerTokens.size + 2)
        val paddedTokensInt = IntArray(innerTokens.size + 2)
        for (i in innerTokens.indices) {
            paddedTokensLong[i + 1] = innerTokens[i].toLong()
            paddedTokensInt[i + 1] = innerTokens[i]
        }

        val tokensInputName = if ("input_ids" in session.inputNames) "input_ids" else "tokens"
        val tokensType = (session.inputInfo[tokensInputName]?.info as? TensorInfo)?.type ?: OnnxJavaType.INT64
        val speedType = (session.inputInfo["speed"]?.info as? TensorInfo)?.type ?: OnnxJavaType.FLOAT

        val inputs = HashMap<String, OnnxTensor>(4)
        try {
            val tokenShape = longArrayOf(1L, paddedTokensLong.size.toLong())
            inputs[tokensInputName] = if (tokensType == OnnxJavaType.INT32) {
                OnnxTensor.createTensor(env, IntBuffer.wrap(paddedTokensInt), tokenShape)
            } else {
                OnnxTensor.createTensor(env, LongBuffer.wrap(paddedTokensLong), tokenShape)
            }
            inputs["style"] = OnnxTensor.createTensor(env, FloatBuffer.wrap(styleVec), longArrayOf(1L, STYLE_DIM.toLong()))
            inputs["speed"] = if (speedType == OnnxJavaType.INT32) {
                OnnxTensor.createTensor(env, IntBuffer.wrap(intArrayOf(1)), longArrayOf(1L))
            } else {
                OnnxTensor.createTensor(env, FloatBuffer.wrap(floatArrayOf(voiceProfile.speechRate.coerceIn(0.8f, 1.3f))), longArrayOf(1L))
            }

            val res = session.run(inputs)
            try {
                val outTensor = res.firstOrNull()?.value as? OnnxTensor ?: return null
                val fb = outTensor.floatBuffer
                fb.rewind()
                val samples24k = FloatArray(fb.remaining())
                fb.get(samples24k)
                if (samples24k.size < 240) return null
                val pcm16k = resampleFloat24kToPcm16kMono(samples24k, targetPeak = 22000)
                Log.i(TAG, "Kokoro-82M ONNX synthesized ${samples24k.size} samples @ 24kHz -> ${pcm16k.size} B PCM @ 16kHz (voice=${voiceProfile.voiceId})")
                return pcm16k
            } finally {
                try { res.close() } catch (_: Throwable) {}
            }
        } catch (t: Throwable) {
            Log.w(TAG, "Kokoro-82M ONNX inference error (${t.message}), falling back to system TTS")
            return null
        } finally {
            for (tensor in inputs.values) {
                try { tensor.close() } catch (_: Throwable) {}
            }
        }
    }

    fun close() {
        try {
            kokoroSession?.close()
        } catch (_: Throwable) {}
        kokoroSession = null
        synchronized(voiceNpyCache) {
            voiceNpyCache.clear()
        }
    }
}
