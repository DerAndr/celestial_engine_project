/**
 * Celestial Engine Mk.V — Dedicated Web Worker for Browser-Only Local AI Inference
 * Uses @huggingface/transformers v4.3.2 via jsDelivr.
 */

import { retrieveRepoKnowledge } from './repo-knowledge.js?v=1.3.2';

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
let loadedModelKey = null;
let loadedDevice = null;
let cancelledRequests = new Set();
let isGenerating = false;

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
 * Clean and constrain output (up to ~100 tokens / 4 sentences) without Markdown or emojis,
 * while preserving full URLs and titles ("Dr.").
 */
function sanitizeAndConstrainOutput(rawText, matchedTopics = []) {
  if (!rawText) return '';
  let text = cleanStreamFragment(rawText);

  // Protect abbreviations like "Dr.", "Mr.", "Prof.", "Mk." before splitting sentences
  const protectedText = text.replace(/\b(Dr|Mr|Mrs|Ms|Prof|Mk|St|vs)\.\s+/gi, '$1\u0000 ');
  const sentenceParts = protectedText.split(/(?<=[.!?])\s+(?=[A-ZА-ЯЁ0-9])/);
  if (sentenceParts && sentenceParts.length > 0) {
    text = sentenceParts.slice(0, 4).join(' ').replace(/\u0000/g, '.').trim();
  } else {
    text = protectedText.replace(/\u0000/g, '.').trim();
  }

  const words = text.split(/\s+/).filter(Boolean);
  if (words.length > 90) {
    text = words.slice(0, 90).join(' ').replace(/[,;:\-–—]+$/, '');
  }

  // Strip any dangling title at the very end caused by token or word-limit truncation (e.g. "... Dr.")
  text = text.replace(/\s+\b(?:Dr|Mr|Mrs|Ms|Prof)\.?$/i, '').trim();

  // Guarantee the LinkedIn URL is never lost when answering about the creator / Dr. Andrey Holz
  const isCreatorTopic = Array.isArray(matchedTopics) && matchedTopics.includes('creator_author');
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
    const adapter = await navigator.gpu.requestAdapter();
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
      const useWebgpu = gpuCheck.ok;
      if (!useWebgpu && spec.key === 'lfm2.5-350m') {
        throw new Error(`WebGPU is unavailable (${gpuCheck.reason}). LFM2.5-350M requires WebGPU. Please use SmolLM2-135M.`);
      }

      const device = useWebgpu ? 'webgpu' : 'wasm';
      const dtype = useWebgpu ? spec.dtypeWebgpu : spec.dtypeWasm;

      self.postMessage({
        type: 'PROGRESS',
        progress: 5,
        status: `Loading Transformers.js v4.3.2 (${device.toUpperCase()}, ${dtype})...`
      });

      const { pipeline, env, TextStreamer } = await import(TRANSFORMERS_CDN);
      env.allowLocalModels = false;
      TextStreamerClass = TextStreamer || null;

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
    const { systemPrompt, question, temperature = 0.6, maxNewTokens = 100 } = msg;

    try {
      const userQuestion = (question || '').trim() || 'Give me a brief oracle status report.';
      const repoKnowledge = retrieveRepoKnowledge(userQuestion);
      const baseSystemPrompt = systemPrompt || 'You are the Oracle of the Celestial Engine Mk.V staff. Respond concisely in 1-2 sentences without markdown or emojis.';

      const creatorFact = 'Creator fact: Celestial Engine Mk.V was created by Dr. Andrey Holz (LinkedIn: https://www.linkedin.com/in/andrey-holz/ ).';

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
      const streamer = (TextStreamerClass && pipelineInstance.tokenizer)
        ? new TextStreamerClass(pipelineInstance.tokenizer, {
            skip_prompt: true,
            skip_special_tokens: true,
            callback_function: (tokenText) => {
              if (cancelledRequests.has(requestId)) return;
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
              }
            }
          })
        : undefined;

      const out = await pipelineInstance(messages, {
        max_new_tokens: maxNewTokens,
        temperature,
        top_p: 0.9,
        repetition_penalty: 1.1,
        do_sample: loadedDevice === 'webgpu',
        ...(streamer ? { streamer } : {})
      });

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
