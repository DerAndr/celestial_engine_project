/**
 * Celestial Engine Mk.V — Dedicated Web Worker for Browser-Only Local AI Inference
 * Uses @huggingface/transformers v4.3.2 via jsDelivr only when explicitly initialized by the user.
 */

import { retrieveRepoKnowledge } from './repo-knowledge.js?v=1.2.1';

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
    // Remove special chat tokens if any leaked
    .replace(/<\|[^>]+?\|>/g, '')
    // Strip markdown bold/italics/code/headers
    .replace(/[*_`#~>]/g, '')
    // Remove emojis / surrogate pairs & pictographs
    .replace(/[\u{1F300}-\u{1FAFF}\u{2600}-\u{27BF}]/gu, '')
    .replace(/\s+/g, ' ')
    .trim();

  // Split into sentences and keep at most 2 sentences
  const sentenceMatches = text.match(/[^.!?]+[.!?]+|[^.!?]+$/g);
  if (sentenceMatches && sentenceMatches.length > 0) {
    text = sentenceMatches.slice(0, 2).join(' ').trim();
  }

  // Enforce <= 25 words cleanly
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
    const device = await adapter.requestDevice();
    if (!device) {
      return { ok: false, reason: 'WebGPU device creation failed' };
    }
    const info = adapter.info || {};
    const label = [info.vendor, info.architecture, info.description].filter(Boolean).join(' ') || 'WebGPU Adapter';
    device.destroy();
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
      // Do not force CPU/WASM on the 350M enhanced model to avoid freezing devices without WebGPU
      if (!useWebgpu && spec.key === 'lfm2.5-350m') {
        throw new Error(`WebGPU is unavailable (${gpuCheck.reason}). LFM2.5-350M requires WebGPU to prevent browser UI freezes. Please use SmolLM2-135M or Scripted Mode.`);
      }

      const device = useWebgpu ? 'webgpu' : 'wasm';
      const dtype = useWebgpu ? spec.dtypeWebgpu : spec.dtypeWasm;

      self.postMessage({
        type: 'PROGRESS',
        progress: 5,
        status: `Importing Transformers.js v4.3.2 (${device.toUpperCase()}, ${dtype})...`
      });

      const { pipeline, env } = await import(TRANSFORMERS_CDN);
      env.allowLocalModels = false;

      const fileBytes = new Map();
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

      loadedModelKey = spec.key;
      loadedDevice = device;

      self.postMessage({
        type: 'READY',
        modelKey: spec.key,
        repo: spec.repo,
        device,
        dtype,
        adapterLabel: gpuCheck.label || device.toUpperCase()
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
    const { systemPrompt, question, temperature = 0.65, maxNewTokens = 56 } = msg;

    try {
      const repoKnowledge = retrieveRepoKnowledge(question);
      const baseSystemPrompt = systemPrompt || 'You are the Oracle of the Celestial Engine Mk.V staff. Respond in 1-2 short sentences under 25 words without markdown or emojis.';
      const groundedSystemPrompt = `${baseSystemPrompt}\n\nRepository Ground Truth (use these exact facts when asked about hardware, pins, code, or architecture): ${repoKnowledge.contextString}`;

      const messages = [
        {
          role: 'system',
          content: groundedSystemPrompt
        },
        {
          role: 'user',
          content: question || 'Give me a brief oracle status report.'
        }
      ];

      const startMs = performance.now();
      const out = await pipelineInstance(messages, {
        max_new_tokens: maxNewTokens,
        temperature,
        top_p: 0.9,
        repetition_penalty: 1.08,
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

