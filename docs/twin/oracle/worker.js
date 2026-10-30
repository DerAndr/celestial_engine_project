/**
 * Celestial Engine Mk.V — Dedicated Web Worker for Browser-Only Local AI Inference
 * Uses @huggingface/transformers v4.3.2 via jsDelivr.
 */

import { retrieveRepoKnowledge } from './repo-knowledge.js?v=1.3.4';

const TRANSFORMERS_CDN = 'https://cdn.jsdelivr.net/npm/@huggingface/transformers@4.3.2/dist/transformers.min.js';

export const MODEL_CATALOG = {
  'smollm2-135m': {
    key: 'smollm2-135m',
    label: 'SmolLM2-135M-Instruct (Lightweight)',
    repo: 'HuggingFaceTB/SmolLM2-135M-Instruct',
    dtypeWebgpu: 'q4f16',
    dtypeWasm: 'q4',
    approxDownloadMb: 120,
    license: 'Apache-2.0',
    description: 'Compact 135M instruct model (~120 MB q4f16). Fast download and low memory footprint.'
  },
  'lfm2.5-350m': {
    key: 'lfm2.5-350m',
    label: 'LiquidAI LFM2.5-350M (Enhanced)',
    repo: 'onnx-community/LFM2.5-350M-ONNX',
    dtypeWebgpu: 'q4f16',
    dtypeWasm: 'q4',
    approxDownloadMb: 258,
    license: 'LFM Open License 1.0',
    description: 'Hybrid 350M LIV-conv + GQA instruct model (~258 MB q4f16). Stronger persona adherence; requires higher memory.'
  }
};

let pipelineInstance = null;
let TextStreamerClass = null;
let InterruptableStoppingCriteriaClass = null;
let StoppingCriteriaListClass = null;
let loadedModelKey = null;
let loadedDevice = null;
let cancelledRequests = new Set();
let isGenerating = false;
const isMobileWorker = /Android|iPhone|iPad|iPod|Mobile/i.test((typeof navigator !== 'undefined' && navigator.userAgent) || '');

const CREATOR_LINKEDIN_URL = 'https://www.linkedin.com/in/andrey-holz/';

function cleanStreamFragment(rawText) {
  if (!rawText) return '';
  return String(rawText)
    .replace(/<\|[^>]+?\|>/g, '')
    .replace(/<think>[\s\S]*?<\/think>/gi, '')
    .replace(/\[([^\]]+)\]\((https?:\/\/[^\s)]+)\)/gi, '$2')
    .replace(/\[[^\]]*(?:linkedin|link|url|profile|contact)[^\]]*\]/gi, CREATOR_LINKEDIN_URL)
    .replace(/[*_`#~>]/g, '')
    .replace(/[\u{1F300}-\u{1FAFF}\u{2600}-\u{27BF}]/gu, '')
    .replace(/\s+/g, ' ')
    .trim();
}

/**
 * Clean and constrain output to 1-2 sentences (<= 34 words, ~40 tokens) without Markdown or emojis,
 * while preserving full URLs and titles ("Dr.") and stripping any hallucinated biography filler.
 */
function sanitizeAndConstrainOutput(rawText, matchedTopics = []) {
  if (!rawText) return '';
  let text = cleanStreamFragment(rawText);

  // Protect abbreviations like "Dr.", "Mr.", "Prof.", "Mk." before splitting sentences
  const protectedText = text.replace(/\b(Dr|Mr|Mrs|Ms|Prof|Mk|St|vs)\.\s+/gi, '$1\u0000 ');
  let sentenceParts = protectedText
    .split(/(?<=[.!?])\s+(?=[A-ZА-ЯЁ0-9])/)
    .map(s => s.replace(/\u0000/g, '.').trim())
    .filter(Boolean);

  // Filter out small-LLM hallucinated biography filler (fake age, fake university, etc.)
  sentenceParts = sentenceParts.filter(
    s => !/\b\d+-year-old\b|\buniversity\b|\bmoscow\b|\bborn in\b|\bgraduated\b/i.test(s)
  );

  if (sentenceParts.length > 0) {
    text = sentenceParts.slice(0, 2).join(' ').trim();
  } else {
    text = protectedText.replace(/\u0000/g, '.').trim();
  }

  const words = text.split(/\s+/).filter(Boolean);
  if (words.length > 34) {
    text = words.slice(0, 34).join(' ').replace(/[,;:\-–—]+$/, '');
  }

  // Strip any dangling title at the very end caused by token or word-limit truncation (e.g. "... Dr.")
  text = text.replace(/\s+\b(?:Dr|Mr|Mrs|Ms|Prof)\.?$/i, '').trim();

  // Guarantee the LinkedIn URL is never lost when answering about the creator / Dr. Andrey Holz
  const isCreatorTopic = Array.isArray(matchedTopics) && matchedTopics.some(t => (t.id || t) === 'creator_author');
  if ((isCreatorTopic || /\b(andrey|holz|linkedin)\b/i.test(text)) && !text.includes('linkedin.com/in/andrey-holz')) {
    text = text.replace(/[,;:\-–—.]+$/, '');
    text = `${text}. LinkedIn: ${CREATOR_LINKEDIN_URL}`;
  }

  if (text && !/[.!?/]$/.test(text)) {
    text += '.';
  }

  return text;
}

async function checkWebGpuAdapter() {
  if (typeof navigator === 'undefined' || !navigator.gpu) {
    return { ok: false, reason: 'navigator.gpu not present' };
  }
  try {
    const adapter = await navigator.gpu.requestAdapter({ powerPreference: 'high-performance' });
    if (!adapter) {
      return { ok: false, reason: 'No WebGPU adapter returned' };
    }
    const info = adapter.info || {};
    const label = [info.vendor, info.architecture, info.description].filter(Boolean).join(' ') || 'WebGPU Adapter';
    return { ok: true, label };
  } catch (err) {
    return { ok: false, reason: err.message || 'WebGPU initialization failed' };
  }
}

self.addEventListener('message', async (event) => {
  const msg = event.data || {};
  const { type, requestId } = msg;

  if (type === 'CANCEL') {
    if (requestId !== undefined) {
      cancelledRequests.add(requestId);
    }
    return;
  }

  if (type === 'DISPOSE') {
    if (pipelineInstance && typeof pipelineInstance.dispose === 'function') {
      try {
        await pipelineInstance.dispose();
      } catch (_) {}
    }
    pipelineInstance = null;
    loadedModelKey = null;
    loadedDevice = null;
    self.postMessage({ type: 'DISPOSED' });
    return;
  }

  if (type === 'INIT') {
    const modelKey = msg.modelKey || 'smollm2-135m';
    const spec = MODEL_CATALOG[modelKey] || MODEL_CATALOG['smollm2-135m'];

    if (pipelineInstance && loadedModelKey === spec.key) {
      self.postMessage({
        type: 'READY',
        modelKey: spec.key,
        repo: spec.repo,
        device: loadedDevice
      });
      return;
    }

    try {
      if (pipelineInstance && typeof pipelineInstance.dispose === 'function') {
        try { await pipelineInstance.dispose(); } catch (_) {}
        pipelineInstance = null;
      }

      self.postMessage({
        type: 'PROGRESS',
        progress: 2,
        status: `Checking WebGPU hardware acceleration...`
      });

      const gpuCheck = await checkWebGpuAdapter();
      // On mobile browsers, SmolLM2-135M runs significantly faster on CPU WASM SIMD (q4)
      // than mobile WebGPU (q4f16), which suffers from GPU⇄CPU buffer sync stalls on mobile GPUs.
      const useWebgpu = isMobileWorker && spec.key === 'smollm2-135m' ? false : gpuCheck.ok;
      if (!gpuCheck.ok && spec.key === 'lfm2.5-350m') {
        throw new Error(`WebGPU is unavailable (${gpuCheck.reason}). LFM2.5-350M requires WebGPU. Please use SmolLM2-135M.`);
      }

      const device = useWebgpu ? 'webgpu' : 'wasm';
      const dtype = useWebgpu ? spec.dtypeWebgpu : spec.dtypeWasm;

      self.postMessage({
        type: 'PROGRESS',
        progress: 5,
        status: `Loading Transformers.js v4.3.2 (${device.toUpperCase()}, ${dtype})...`
      });

      const { pipeline, env, TextStreamer, InterruptableStoppingCriteria, StoppingCriteriaList } = await import(TRANSFORMERS_CDN);
      env.allowLocalModels = false;
      if (env && env.backends && env.backends.onnx && env.backends.onnx.wasm) {
        env.backends.onnx.wasm.simd = true;
      }
      TextStreamerClass = TextStreamer || null;
      InterruptableStoppingCriteriaClass = InterruptableStoppingCriteria || null;
      StoppingCriteriaListClass = StoppingCriteriaList || null;

      const fileBytes = new Map();
      try {
        pipelineInstance = await pipeline('text-generation', spec.repo, {
          device,
          dtype,
          progress_callback: (p) => {
            if (p.status === 'progress' && p.file) {
              fileBytes.set(p.file, { loaded: p.loaded || 0, total: p.total || 0 });
              let sumLoaded = 0;
              let sumTotal = 0;
              for (const v of fileBytes.values()) {
                sumLoaded += v.loaded;
                sumTotal += v.total;
              }
              const pct = sumTotal > 0 ? Math.min(99, Math.round((sumLoaded / sumTotal) * 100)) : Math.round(p.progress || 10);
              const mbLoaded = (sumLoaded / (1024 * 1024)).toFixed(1);
              const mbTotal = sumTotal > 0 ? (sumTotal / (1024 * 1024)).toFixed(1) : spec.approxDownloadMb;
              self.postMessage({
                type: 'PROGRESS',
                progress: pct,
                status: `Downloading ${spec.key} (${mbLoaded} / ${mbTotal} MB)...`
              });
            } else if (p.status === 'ready') {
              self.postMessage({
                type: 'PROGRESS',
                progress: 100,
                status: `Compiling ${device.toUpperCase()} execution graph...`
              });
            }
          }
        });
        loadedDevice = device;
      } catch (webgpuErr) {
        // Automatic fallback to WASM q4 if WebGPU shader compilation fails on specific driver
        if (device === 'webgpu' && spec.key === 'smollm2-135m') {
          self.postMessage({
            type: 'PROGRESS',
            progress: 50,
            status: `WebGPU fallback to WASM (${spec.dtypeWasm})...`
          });
          pipelineInstance = await pipeline('text-generation', spec.repo, {
            device: 'wasm',
            dtype: spec.dtypeWasm
          });
          loadedDevice = 'wasm';
        } else {
          throw webgpuErr;
        }
      }

      loadedModelKey = spec.key;

      self.postMessage({
        type: 'READY',
        modelKey: spec.key,
        repo: spec.repo,
        device: loadedDevice,
        dtype: loadedDevice === 'webgpu' ? spec.dtypeWebgpu : spec.dtypeWasm,
        adapterLabel: gpuCheck.label || loadedDevice.toUpperCase()
      });
    } catch (err) {
      pipelineInstance = null;
      loadedModelKey = null;
      self.postMessage({
        type: 'ERROR',
        requestId,
        error: err && err.message ? err.message : String(err)
      });
    }
    return;
  }

  if (type === 'GENERATE') {
    if (!pipelineInstance) {
      self.postMessage({
        type: 'ERROR',
        requestId,
        error: 'Local AI model is not loaded.'
      });
      return;
    }

    if (isGenerating) {
      self.postMessage({
        type: 'ERROR',
        requestId,
        error: 'Another generation request is currently finishing on GPU.'
      });
      return;
    }

    isGenerating = true;
    const { systemPrompt, question, temperature = 0.5, maxNewTokens = 40 } = msg;

    try {
      const userQuestion = (question || '').trim() || 'Give me a brief oracle status report.';
      const repoKnowledge = retrieveRepoKnowledge(userQuestion);
      const baseSystemPrompt = systemPrompt || 'You are the Oracle of the Celestial Engine Mk.V staff. Respond concisely in 1-2 sentences without markdown or emojis.';

      const creatorFact = 'Creator fact: Celestial Engine Mk.V and this app were created by Dr. Andrey Holz, an expert in Machine Learning and Applied Mathematics (LinkedIn: https://www.linkedin.com/in/andrey-holz/ ). State ONLY provided facts and never invent age or university.';

      // Compact system prompt for fast mobile prefill
      const groundedSystemPrompt = (repoKnowledge.matchedTopics && repoKnowledge.matchedTopics.length > 0 && repoKnowledge.contextString)
        ? `${baseSystemPrompt}\n${creatorFact}\nFacts: ${repoKnowledge.contextString}`
        : `${baseSystemPrompt}\n${creatorFact}`;

      const messages = [
        {
          role: 'system',
          content: groundedSystemPrompt
        },
        {
          role: 'user',
          content: userQuestion
        }
      ];

      const startMs = performance.now();
      let streamedRaw = '';
      let stopper = null;
      let stoppingCriteriaList = undefined;
      if (InterruptableStoppingCriteriaClass && StoppingCriteriaListClass) {
        stopper = new InterruptableStoppingCriteriaClass();
        stoppingCriteriaList = new StoppingCriteriaListClass();
        stoppingCriteriaList.push(stopper);
      }

      const isCreatorTopic = Array.isArray(repoKnowledge.matchedTopics) &&
        repoKnowledge.matchedTopics.some(t => (t.id || t) === 'creator_author');
      const minStopWords = isCreatorTopic ? 7 : 12;

      const streamer = (TextStreamerClass && pipelineInstance.tokenizer)
        ? new TextStreamerClass(pipelineInstance.tokenizer, {
            skip_prompt: true,
            skip_special_tokens: true,
            callback_function: (tokenText) => {
              if (cancelledRequests.has(requestId)) {
                if (stopper) stopper.interrupt();
                return;
              }
              streamedRaw += tokenText;
              const partialClean = cleanStreamFragment(streamedRaw);
              if (partialClean) {
                self.postMessage({
                  type: 'STREAM',
                  requestId,
                  text: partialClean,
                  elapsedMs: Math.round(performance.now() - startMs),
                  modelKey: loadedModelKey,
                  device: loadedDevice
                });
                // Stop cleanly once a complete sentence is finished so 135M never rambles or hallucinates
                if (stopper && (isMobileWorker || isCreatorTopic)) {
                  const wordCount = partialClean.split(/\s+/).filter(Boolean).length;
                  if (wordCount >= minStopWords && /[.!?]$/.test(partialClean) && !/\b(?:Dr|Mr|Mrs|Ms|Prof|Mk|St|vs)\.$/i.test(partialClean)) {
                    stopper.interrupt();
                  }
                }
              }
            }
          })
        : undefined;

      const genOptions = isMobileWorker
        ? {
            max_new_tokens: 40,
            do_sample: false,
            ...(streamer ? { streamer } : {}),
            ...(stoppingCriteriaList ? { stopping_criteria: stoppingCriteriaList } : {})
          }
        : {
            max_new_tokens: maxNewTokens,
            temperature,
            top_p: 0.9,
            repetition_penalty: 1.1,
            do_sample: loadedDevice === 'webgpu',
            ...(streamer ? { streamer } : {}),
            ...(stoppingCriteriaList ? { stopping_criteria: stoppingCriteriaList } : {})
          };

      const out = await pipelineInstance(messages, genOptions);

      if (cancelledRequests.has(requestId)) {
        cancelledRequests.delete(requestId);
        isGenerating = false;
        return;
      }

      let rawReply = '';
      if (Array.isArray(out) && out[0] && Array.isArray(out[0].generated_text)) {
        const lastMsg = out[0].generated_text[out[0].generated_text.length - 1];
        rawReply = lastMsg && lastMsg.content ? lastMsg.content : '';
      } else if (Array.isArray(out) && out[0] && typeof out[0].generated_text === 'string') {
        rawReply = out[0].generated_text;
      }

      const cleanText = sanitizeAndConstrainOutput(rawReply, repoKnowledge.matchedTopics);
      const elapsedMs = Math.round(performance.now() - startMs);

      self.postMessage({
        type: 'DONE',
        requestId,
        text: cleanText || 'Oracle signal received, but the quantum channel returned silence.',
        latencyMs: elapsedMs,
        modelKey: loadedModelKey,
        device: loadedDevice
      });
    } catch (err) {
      if (!cancelledRequests.has(requestId)) {
        self.postMessage({
          type: 'ERROR',
          requestId,
          error: err && err.message ? err.message : String(err)
        });
      }
    } finally {
      isGenerating = false;
    }
  }
});
