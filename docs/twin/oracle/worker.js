/**
 * Celestial Engine Mk.V — Dedicated Web Worker for Browser-Only Local AI Inference
 * Uses @huggingface/transformers v4.3.2 via jsDelivr.
 */

import { retrieveRepoKnowledge } from './repo-knowledge.js?v=1.2.3';

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
let loadedModelKey = null;
let loadedDevice = null;
let cancelledRequests = new Set();
let isGenerating = false;

/**
 * Clean and constrain output to 1-2 sentences, <= 25 words, no Markdown or emojis,
 * matching the Android OraclePersona contract.
 */
function sanitizeAndConstrainOutput(rawText) {
  if (!rawText) return '';
  let text = String(rawText)
    .replace(/<\|[^>]+?\|>/g, '')
    .replace(/<think>[\s\S]*?<\/think>/gi, '')
    .replace(/[*_`#~>]/g, '')
    .replace(/[\u{1F300}-\u{1FAFF}\u{2600}-\u{27BF}]/gu, '')
    .replace(/\s+/g, ' ')
    .trim();

  const sentenceMatches = text.match(/[^.!?]+[.!?]+|[^.!?]+$/g);
  if (sentenceMatches && sentenceMatches.length > 0) {
    text = sentenceMatches.slice(0, 2).join(' ').trim();
  }

  const words = text.split(/\s+/).filter(Boolean);
  if (words.length > 25) {
    text = words.slice(0, 25).join(' ').replace(/[,;:\-–—]+$/, '');
    if (!/[.!?]$/.test(text)) {
      text += '.';
    }
  } else if (text && !/[.!?]$/.test(text)) {
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

      const { pipeline, env } = await import(TRANSFORMERS_CDN);
      env.allowLocalModels = false;

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
    const { systemPrompt, question, temperature = 0.7, maxNewTokens = 60 } = msg;

    try {
      const userQuestion = (question || '').trim() || 'Give me a brief oracle status report.';
      const repoKnowledge = retrieveRepoKnowledge(userQuestion);
      const baseSystemPrompt = systemPrompt || 'You are the Oracle of the Celestial Engine Mk.V staff. Respond in 1-2 short sentences under 25 words without markdown or emojis.';

      // Only inject hardware/repo context when the user's question actually asks about hardware/repo topics,
      // so general creative questions (e.g. "tell me a story about a cat") are answered directly and naturally!
      const groundedSystemPrompt = (repoKnowledge.matchedTopics && repoKnowledge.matchedTopics.length > 0)
        ? `${baseSystemPrompt}\nUse these exact hardware facts: ${repoKnowledge.contextString}`
        : `${baseSystemPrompt}\nDirectly answer the user's exact topic or request in 1-2 short sentences (maximum 25 words).`;

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
      const out = await pipelineInstance(messages, {
        max_new_tokens: maxNewTokens,
        temperature,
        top_p: 0.9,
        repetition_penalty: 1.1,
        do_sample: true
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

      const cleanText = sanitizeAndConstrainOutput(rawReply);
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
