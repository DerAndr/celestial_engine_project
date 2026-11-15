package com.celestial.companion.ai

import ai.onnxruntime.OnnxTensor
import ai.onnxruntime.OrtEnvironment
import ai.onnxruntime.OrtSession
import ai.onnxruntime.TensorInfo
import android.content.Context
import android.util.Log
import com.celestial.companion.domain.model.OraclePersona
import com.celestial.companion.domain.model.StaffLanguage
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.currentCoroutineContext
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.flow
import kotlinx.coroutines.flow.flowOn
import kotlinx.coroutines.isActive
import kotlinx.coroutines.sync.Mutex
import kotlinx.coroutines.sync.withLock
import kotlinx.coroutines.withContext
import org.json.JSONObject
import java.io.File
import java.io.FileOutputStream
import java.net.HttpURLConnection
import java.net.URL
import java.nio.FloatBuffer
import java.nio.LongBuffer

enum class OnDeviceLlmState(val badge: String) {
    NOT_DOWNLOADED("FAST DETERMINISTIC (SMOLLM2 ONNX NOT DOWNLOADED)"),
    DOWNLOADING("DOWNLOADING SMOLLM2-135M INT8 ONNX..."),
    READY_ONNX("SMOLLM2-135M INT8 ONNX ACTIVE"),
    ERROR("DOWNLOAD FAILED (DETERMINISTIC FALLBACK)")
}

/**
 * Task 7.3 Phase 2: Genuine On-Device Generative LLM Engine (`SmolLM2-135M-Instruct` INT8 ONNX)
 * powered by Microsoft ONNX Runtime Mobile (`com.microsoft.onnxruntime:onnxruntime-android`).
 *
 * - Downloads un-gated `HuggingFaceTB/SmolLM2-135M-Instruct` (`onnx/model_int8.onnx` ~130 MB + `tokenizer.json`)
 *   on-demand into `context.filesDir/sota_models/smollm2/`.
 * - Implements a pure-Kotlin GPT-2 Byte-Level BPE Tokenizer (`ByteLevelBpeTokenizer`) with ChatML framing
 *   (`<|im_start|>system...<|im_end|>\n<|im_start|>user...<|im_end|>\n<|im_start|>assistant\n`).
 * - Executes true autoregressive KV-cache token generation (`past_key_values.0..29` -> `present.0..29`)
 *   with repetition penalty and sentence boundary clipping (`<= 25 words`).
 * - Deterministically releases all native `OrtSession` C++ memory on `close()`.
 */
class OnDeviceLlmEngine(
    private val context: Context? = null
) {
    companion object {
        private const val TAG = "OnDeviceLlmEngine"
        private const val HF_SMOLLM2_BASE =
            "https://huggingface.co/HuggingFaceTB/SmolLM2-135M-Instruct/resolve/main"

        const val TOKENIZER_FILE = "tokenizer.json"
        const val MODEL_FILE = "model_int8.onnx"

        const val BOS_TOKEN_ID = 1L // <|im_start|>
        const val EOS_TOKEN_ID = 2L // <|im_end|>
        const val EOT_TOKEN_ID = 0L // <|endoftext|>

        const val MAX_NEW_TOKENS = 36
        const val REPETITION_PENALTY = 1.18f

        /**
         * Builds the standard GPT-2 byte-to-unicode table mapping every byte (0..255)
         * to a printable Unicode character used by ByteLevelBPE `tokenizer.json`.
         */
        fun buildBytesToUnicode(): Map<Int, Char> {
            val bs = mutableListOf<Int>()
            bs.addAll(('!'.code)..('~'.code))
            bs.addAll(('¡'.code)..('¬'.code))
            bs.addAll(('®'.code)..('ÿ'.code))
            val cs = bs.toMutableList()
            var n = 0
            for (b in 0..255) {
                if (b !in bs) {
                    bs.add(b)
                    cs.add(256 + n)
                    n++
                }
            }
            val map = HashMap<Int, Char>(256)
            for (i in bs.indices) {
                map[bs[i]] = cs[i].toChar()
            }
            return map
        }

        val BYTE_TO_UNICODE: Map<Int, Char> = buildBytesToUnicode()
        val UNICODE_TO_BYTE: Map<Char, Byte> = BYTE_TO_UNICODE.entries.associate { (b, c) -> c to b.toByte() }
    }

    /**
     * Pure-Kotlin GPT-2 / Llama Byte-Level BPE Tokenizer loaded from HuggingFace `tokenizer.json`.
     */
    class ByteLevelBpeTokenizer(
        private val vocab: Map<String, Int>,
        private val idToToken: Map<Int, String>,
        private val bpeRanks: Map<Pair<String, String>, Int>
    ) {
        companion object {
            private val PRE_TOKENIZE_REGEX =
                Regex("""'s|'t|'re|'ve|'m|'ll|'d| ?\p{L}+| ?\p{N}+| ?[^\s\p{L}\p{N}]+|\s+(?!\S)|\s+""")

            fun fromTokenizerJson(jsonStr: String): ByteLevelBpeTokenizer {
                val root = JSONObject(jsonStr)
                val modelObj = root.getJSONObject("model")
                val vocabObj = modelObj.getJSONObject("vocab")
                val vocab = HashMap<String, Int>(vocabObj.length() + 32)
                val idToToken = HashMap<Int, String>(vocabObj.length() + 32)

                val keys = vocabObj.keys()
                while (keys.hasNext()) {
                    val token = keys.next()
                    val id = vocabObj.getInt(token)
                    vocab[token] = id
                    idToToken[id] = token
                }

                if (root.has("added_tokens")) {
                    val addedArr = root.getJSONArray("added_tokens")
                    for (i in 0 until addedArr.length()) {
                        val item = addedArr.getJSONObject(i)
                        val content = item.getString("content")
                        val id = item.getInt("id")
                        vocab[content] = id
                        idToToken[id] = content
                    }
                }

                val mergesArr = modelObj.getJSONArray("merges")
                val ranks = HashMap<Pair<String, String>, Int>(mergesArr.length())
                for (i in 0 until mergesArr.length()) {
                    val entry = mergesArr.get(i)
                    if (entry is String) {
                        val sp = entry.indexOf(' ')
                        if (sp > 0) {
                            ranks[entry.substring(0, sp) to entry.substring(sp + 1)] = i
                        }
                    } else if (entry is org.json.JSONArray && entry.length() == 2) {
                        ranks[entry.getString(0) to entry.getString(1)] = i
                    }
                }
                return ByteLevelBpeTokenizer(vocab, idToToken, ranks)
            }
        }

        private val bpeCache = HashMap<String, List<String>>(2048)

        private fun bpe(token: String): List<String> {
            bpeCache[token]?.let { return it }
            if (token.length <= 1) return listOf(token)

            var word = token.map { it.toString() }
            while (word.size > 1) {
                var bestPair: Pair<String, String>? = null
                var bestRank = Int.MAX_VALUE
                for (i in 0 until word.size - 1) {
                    val pair = word[i] to word[i + 1]
                    val rank = bpeRanks[pair] ?: Int.MAX_VALUE
                    if (rank < bestRank) {
                        bestRank = rank
                        bestPair = pair
                    }
                }
                if (bestPair == null || bestRank == Int.MAX_VALUE) break

                val first = bestPair.first
                val second = bestPair.second
                val newWord = ArrayList<String>(word.size)
                var i = 0
                while (i < word.size) {
                    if (i < word.size - 1 && word[i] == first && word[i + 1] == second) {
                        newWord.add(first + second)
                        i += 2
                    } else {
                        newWord.add(word[i])
                        i += 1
                    }
                }
                word = newWord
            }
            if (bpeCache.size < 4096) {
                bpeCache[token] = word
            }
            return word
        }

        fun encodeText(text: String): List<Long> {
            if (text.isEmpty()) return emptyList()
            val out = ArrayList<Long>(text.length / 2 + 8)
            for (match in PRE_TOKENIZE_REGEX.findAll(text)) {
                val utf8Bytes = match.value.toByteArray(Charsets.UTF_8)
                val mapped = buildString(utf8Bytes.size) {
                    for (b in utf8Bytes) {
                        append(BYTE_TO_UNICODE[b.toInt() and 0xFF] ?: ' ')
                    }
                }
                for (piece in bpe(mapped)) {
                    val id = vocab[piece]
                    if (id != null) {
                        out.add(id.toLong())
                    }
                }
            }
            return out
        }

        fun encodeChatMl(systemPrompt: String, userPrompt: String): LongArray {
            val ids = ArrayList<Long>(128)
            // <|im_start|>system\n{systemPrompt}<|im_end|>\n<|im_start|>user\n{userPrompt}<|im_end|>\n<|im_start|>assistant\n
            ids.add(BOS_TOKEN_ID)
            ids.addAll(encodeText("system\n$systemPrompt"))
            ids.add(EOS_TOKEN_ID)
            ids.addAll(encodeText("\n"))
            ids.add(BOS_TOKEN_ID)
            ids.addAll(encodeText("user\n$userPrompt"))
            ids.add(EOS_TOKEN_ID)
            ids.addAll(encodeText("\n"))
            ids.add(BOS_TOKEN_ID)
            ids.addAll(encodeText("assistant\n"))
            return ids.toLongArray()
        }

        fun decodeTokens(tokenIds: List<Int>): String {
            if (tokenIds.isEmpty()) return ""
            val bytes = ArrayList<Byte>(tokenIds.size * 4)
            for (id in tokenIds) {
                if (id == 0 || id == 1 || id == 2) continue
                val piece = idToToken[id] ?: continue
                if (piece.startsWith("<|") && piece.endsWith("|>")) continue
                for (ch in piece) {
                    val b = UNICODE_TO_BYTE[ch]
                    if (b != null) {
                        bytes.add(b)
                    }
                }
            }
            return ByteArray(bytes.size) { bytes[it] }.toString(Charsets.UTF_8)
        }
    }

    private val mutex = Mutex()
    private var ortEnv: OrtEnvironment? = null
    private var llmSession: OrtSession? = null
    private var tokenizer: ByteLevelBpeTokenizer? = null

    private val _state = MutableStateFlow(
        if (isModelDownloaded()) OnDeviceLlmState.READY_ONNX else OnDeviceLlmState.NOT_DOWNLOADED
    )
    val state: StateFlow<OnDeviceLlmState> = _state.asStateFlow()

    private val _downloadProgressPct = MutableStateFlow(if (isModelDownloaded()) 100 else 0)
    val downloadProgressPct: StateFlow<Int> = _downloadProgressPct.asStateFlow()

    private fun getModelDir(): File? {
        val base = context?.filesDir ?: return null
        return File(base, "sota_models/smollm2")
    }

    fun isModelDownloaded(): Boolean {
        val dir = getModelDir() ?: return false
        val tokFile = File(dir, TOKENIZER_FILE)
        val onnxFile = File(dir, MODEL_FILE)
        return tokFile.exists() && tokFile.length() > 100_000L &&
            onnxFile.exists() && onnxFile.length() > 50_000_000L
    }

    /**
     * Downloads `tokenizer.json` and `onnx/model_int8.onnx` (~130.8 MB) from HuggingFace
     * into `filesDir/sota_models/smollm2/` and initializes the ONNX Runtime session.
     */
    suspend fun downloadAndInitModel(): Boolean = withContext(Dispatchers.IO) {
        val dir = getModelDir() ?: return@withContext false
        if (!dir.exists()) dir.mkdirs()

        _state.value = OnDeviceLlmState.DOWNLOADING
        _downloadProgressPct.value = 1

        val filesToDownload = listOf(
            TOKENIZER_FILE to "$HF_SMOLLM2_BASE/tokenizer.json",
            MODEL_FILE to "$HF_SMOLLM2_BASE/onnx/model_int8.onnx"
        )

        try {
            for ((index, pair) in filesToDownload.withIndex()) {
                val (localName, remoteUrl) = pair
                val targetFile = File(dir, localName)
                val minBytes = if (localName == MODEL_FILE) 50_000_000L else 100_000L
                if (!targetFile.exists() || targetFile.length() < minBytes) {
                    val conn = (URL(remoteUrl).openConnection() as HttpURLConnection).apply {
                        connectTimeout = 15000
                        readTimeout = 60000
                        instanceFollowRedirects = true
                        setRequestProperty("User-Agent", "CelestialEngineMk5-Android/1.3.9")
                    }
                    val code = conn.responseCode
                    if (code !in 200..299) {
                        throw IllegalStateException("HTTP $code downloading $localName")
                    }
                    val contentLen = conn.contentLengthLong.coerceAtLeast(1L)
                    val tmpFile = File(dir, "$localName.part")
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
                                val pct = if (index == 0) {
                                    (frac * 5.0).toInt().coerceIn(1, 5)
                                } else {
                                    (5.0 + frac * 94.0).toInt().coerceIn(5, 99)
                                }
                                _downloadProgressPct.value = pct
                            }
                        }
                    }
                    if (targetFile.exists()) targetFile.delete()
                    tmpFile.renameTo(targetFile)
                }
            }
            _downloadProgressPct.value = 100
            val ready = ensureLoaded()
            _state.value = if (ready) OnDeviceLlmState.READY_ONNX else OnDeviceLlmState.ERROR
            ready
        } catch (e: Exception) {
            Log.w(TAG, "SmolLM2 ONNX download failed: ${e.message}", e)
            _state.value = OnDeviceLlmState.ERROR
            false
        }
    }

    suspend fun ensureLoaded(): Boolean = mutex.withLock {
        if (llmSession != null && tokenizer != null) return true
        val dir = getModelDir() ?: return false
        if (!isModelDownloaded()) return false

        return try {
            val tokJson = File(dir, TOKENIZER_FILE).readText(Charsets.UTF_8)
            tokenizer = ByteLevelBpeTokenizer.fromTokenizerJson(tokJson)

            val env = ortEnv ?: OrtEnvironment.getEnvironment().also { ortEnv = it }
            val opts = OrtSession.SessionOptions().apply {
                setIntraOpNumThreads(4)
                setOptimizationLevel(OrtSession.SessionOptions.OptLevel.ALL_OPT)
            }
            llmSession = env.createSession(File(dir, MODEL_FILE).absolutePath, opts)
            _state.value = OnDeviceLlmState.READY_ONNX
            Log.i(TAG, "Loaded SmolLM2-135M-Instruct INT8 ONNX session successfully")
            true
        } catch (t: Throwable) {
            Log.e(TAG, "Failed loading SmolLM2 ONNX session: ${t.message}", t)
            false
        }
    }

    private fun buildCompactPersonaSystemPrompt(personaId: String, language: StaffLanguage): String {
        val basePersona = when (personaId) {
            OraclePersona.ARCHMAGE.id -> "You are the Wise Archmage of the Celestial Staff. Speak with calm cosmic wisdom."
            OraclePersona.MARVIN.id -> "You are Marvin the Paranoid Android trapped in a glowing staff. Speak with dry, witty existential irony."
            OraclePersona.CYBERPUNK.id -> "You are a Cyberpunk Netrunner AI inside a neon staff. Speak with sharp high-tech street energy."
            OraclePersona.PUMPKIN_LORD.id -> "You are Jack the Pumpkin King. Speak with festive Halloween mystery and playful spooky charm."
            OraclePersona.CUSTOM.id -> "You are the Celestial Staff Oracle. Give a direct, helpful, imaginative answer."
            else -> "You are the Machine God Core of the Celestial Staff. Speak with crisp, analytical cosmic precision."
        }
        val langHint = when (language) {
            StaffLanguage.RUSSIAN -> "Reply in Russian."
            StaffLanguage.GERMAN -> "Reply in German."
            else -> "Reply in English."
        }
        return "$basePersona $langHint Reply in 1 short sentence under 20 words. No markdown or lists."
    }

    /**
     * Streams genuine on-device tokens generated by `SmolLM2-135M-Instruct` INT8 ONNX
     * using autoregressive KV-cache inference.
     */
    fun generateStream(
        prompt: String,
        personaId: String = OraclePersona.MACHINE_GOD.id,
        language: StaffLanguage = StaffLanguage.ENGLISH,
        maxNewTokens: Int = MAX_NEW_TOKENS
    ): Flow<String> = flow {
        if (!ensureLoaded()) return@flow
        val env = ortEnv ?: return@flow
        val session = llmSession ?: return@flow
        val tok = tokenizer ?: return@flow

        val sysPrompt = buildCompactPersonaSystemPrompt(personaId, language)
        val cleanUserPrompt = prompt.trim().ifBlank { "Give a brief status prophecy." }
        val promptIds = tok.encodeChatMl(sysPrompt, cleanUserPrompt)

        // Inspect KV cache shape from model input metadata (default SmolLM2-135M: 30 layers, 3 heads, 64 dim)
        var numLayers = 0
        var numKvHeads = 3L
        var headDim = 64L
        for ((name, nodeInfo) in session.inputInfo) {
            if (name.startsWith("past_key_values.") && name.endsWith(".key")) {
                numLayers++
                val ti = nodeInfo.info as? TensorInfo
                if (ti != null && ti.shape.size == 4) {
                    if (ti.shape[1] > 0) numKvHeads = ti.shape[1]
                    if (ti.shape[3] > 0) headDim = ti.shape[3]
                }
            }
        }
        if (numLayers == 0) numLayers = 30

        val generatedIds = ArrayList<Int>(maxNewTokens)
        var pastKeyArrays = Array(numLayers) { FloatArray(0) }
        var pastValArrays = Array(numLayers) { FloatArray(0) }
        var pastSeqLen = 0L
        var emittedTextLen = 0

        for (step in 0 until maxNewTokens) {
            if (!currentCoroutineContext().isActive) break

            val curInputIds = if (step == 0) promptIds else longArrayOf(generatedIds.last().toLong())
            val curSeqLen = curInputIds.size.toLong()
            val totalSeqLen = pastSeqLen + curSeqLen

            val attnMask = LongArray(totalSeqLen.toInt()) { 1L }
            val posIds = LongArray(curSeqLen.toInt()) { i -> pastSeqLen + i }

            val inputs = HashMap<String, OnnxTensor>(numLayers * 2 + 4)
            try {
                inputs["input_ids"] = OnnxTensor.createTensor(
                    env,
                    LongBuffer.wrap(curInputIds),
                    longArrayOf(1L, curSeqLen)
                )
                if ("attention_mask" in session.inputNames) {
                    inputs["attention_mask"] = OnnxTensor.createTensor(
                        env,
                        LongBuffer.wrap(attnMask),
                        longArrayOf(1L, totalSeqLen)
                    )
                }
                if ("position_ids" in session.inputNames) {
                    inputs["position_ids"] = OnnxTensor.createTensor(
                        env,
                        LongBuffer.wrap(posIds),
                        longArrayOf(1L, curSeqLen)
                    )
                }

                val kvShape = longArrayOf(1L, numKvHeads, pastSeqLen, headDim)
                for (layer in 0 until numLayers) {
                    val kName = "past_key_values.$layer.key"
                    val vName = "past_key_values.$layer.value"
                    inputs[kName] = OnnxTensor.createTensor(env, FloatBuffer.wrap(pastKeyArrays[layer]), kvShape)
                    inputs[vName] = OnnxTensor.createTensor(env, FloatBuffer.wrap(pastValArrays[layer]), kvShape)
                }

                val results = session.run(inputs)
                var shouldStop = false
                var deltaToEmit: String? = null
                try {
                    val logitsTensor = results.get("logits").orElse(null) as? OnnxTensor
                    if (logitsTensor == null) {
                        shouldStop = true
                    } else {
                        val logitsBuf = logitsTensor.floatBuffer
                        val logitsShape = logitsTensor.info.shape // [1, curSeqLen, vocabSize]
                        val vocabSize = logitsShape.last().toInt()
                        val lastTokenOffset = ((curSeqLen.toInt() - 1) * vocabSize)

                        val stepLogits = FloatArray(vocabSize)
                        logitsBuf.position(lastTokenOffset)
                        logitsBuf.get(stepLogits, 0, vocabSize)

                        // Apply repetition penalty to prevent loops
                        for (prevId in generatedIds) {
                            if (prevId in 0 until vocabSize) {
                                val score = stepLogits[prevId]
                                stepLogits[prevId] = if (score < 0f) score * REPETITION_PENALTY else score / REPETITION_PENALTY
                            }
                        }

                        // Greedy argmax selection
                        var bestId = 0
                        var bestVal = Float.NEGATIVE_INFINITY
                        for (v in 0 until vocabSize) {
                            if (stepLogits[v] > bestVal) {
                                bestVal = stepLogits[v]
                                bestId = v
                            }
                        }

                        if (bestId.toLong() == EOS_TOKEN_ID || bestId.toLong() == EOT_TOKEN_ID) {
                            shouldStop = true
                        } else {
                            generatedIds.add(bestId)

                            // Extract updated KV cache from present.0..29
                            val newPastSeqLen = totalSeqLen
                            val newKvSize = (numKvHeads * newPastSeqLen * headDim).toInt()
                            val nextKeys = Array(numLayers) { FloatArray(newKvSize) }
                            val nextVals = Array(numLayers) { FloatArray(newKvSize) }
                            for (layer in 0 until numLayers) {
                                val pk = results.get("present.$layer.key").orElse(null) as? OnnxTensor
                                val pv = results.get("present.$layer.value").orElse(null) as? OnnxTensor
                                pk?.floatBuffer?.let { fb ->
                                    fb.rewind()
                                    fb.get(nextKeys[layer], 0, minOf(fb.remaining(), newKvSize))
                                }
                                pv?.floatBuffer?.let { fb ->
                                    fb.rewind()
                                    fb.get(nextVals[layer], 0, minOf(fb.remaining(), newKvSize))
                                }
                            }
                            pastKeyArrays = nextKeys
                            pastValArrays = nextVals
                            pastSeqLen = newPastSeqLen

                            // Incremental UTF-8 safe detokenization
                            val decodedSoFar = tok.decodeTokens(generatedIds)
                            if (decodedSoFar.length > emittedTextLen) {
                                deltaToEmit = decodedSoFar.substring(emittedTextLen)
                                emittedTextLen = decodedSoFar.length
                            }

                            // Stop early on sentence boundary once we have a crisp response (>= 8 words)
                            val wordCount = decodedSoFar.trim().split(Regex("\\s+")).size
                            if (wordCount >= 22 || (wordCount >= 8 && (decodedSoFar.endsWith(".") || decodedSoFar.endsWith("!") || decodedSoFar.endsWith("?") || decodedSoFar.contains("\n")))) {
                                shouldStop = true
                            }
                        }
                    }
                } finally {
                    try { results.close() } catch (_: Throwable) {}
                }
                if (deltaToEmit != null) {
                    emit(deltaToEmit!!)
                }
                if (shouldStop) {
                    break
                }
            } finally {
                for (tensor in inputs.values) {
                    try { tensor.close() } catch (_: Throwable) {}
                }
            }
        }
    }.flowOn(Dispatchers.Default)

    fun close() {
        try {
            llmSession?.close()
        } catch (_: Throwable) {}
        llmSession = null
        tokenizer = null
    }
}
