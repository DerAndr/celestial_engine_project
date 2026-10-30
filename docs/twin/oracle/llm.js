/**
 * Celestial Engine Mk.V — Main-Thread Local AI Worker Wrapper
 * Manages Web Worker lifecycle, monotonic request IDs, WebGPU detection, and cancellation.
 */

export const MODEL_INFO = {
  'smollm2-135m': {
    key: 'smollm2-135m',
    label: 'SmolLM2-135M-Instruct (Lightweight)',
    repo: 'HuggingFaceTB/SmolLM2-135M-Instruct',
    approxDownloadMb: 120,
    license: 'Apache-2.0',
    desc: '135M parameters • ~120 MB download (q4f16) • Fast initialization, low RAM footprint.'
  },
  'lfm2.5-350m': {
    key: 'lfm2.5-350m',
    label: 'LiquidAI LFM2.5-350M (Enhanced)',
    repo: 'onnx-community/LFM2.5-350M-ONNX',
    approxDownloadMb: 258,
    license: 'LFM Open License 1.0',
    desc: '350M hybrid LIV-conv + GQA • ~258 MB download (q4f16) • Richer persona adherence (WebGPU required).'
  }
};

export async function probeWebGpuSupport() {
  if (typeof navigator === 'undefined' || !navigator.gpu) {
    return { available: false, label: 'WebGPU not supported by browser' };
  }
  try {
    const adapter = await navigator.gpu.requestAdapter();
    if (!adapter) {
      return { available: false, label: 'No compatible WebGPU adapter found' };
    }
    const info = adapter.info || {};
    const label = [info.vendor, info.architecture, info.description].filter(Boolean).join(' ') || 'WebGPU Hardware Adapter';
    return { available: true, label };
  } catch (err) {
    return { available: false, label: err && err.message ? err.message : 'WebGPU probe error' };
  }
}

export class LocalLlmClient {
  constructor(callbacks = {}) {
    this.worker = null;
    this.isReady = false;
    this.loadedModelKey = null;
    this.pendingRequests = new Map();
    this.onProgress = callbacks.onProgress || (() => {});
    this.onReady = callbacks.onReady || (() => {});
    this.onError = callbacks.onError || (() => {});
    this.onDisposed = callbacks.onDisposed || (() => {});
  }

  ensureWorker() {
    if (this.worker) return;
    const workerUrl = new URL('./worker.js?v=1.3.4', import.meta.url);
    this.worker = new Worker(workerUrl, { type: 'module' });

    this.worker.addEventListener('message', (e) => {
      const msg = e.data || {};
      switch (msg.type) {
        case 'PROGRESS':
          this.onProgress(msg.progress || 0, msg.status || 'Loading...');
          break;

        case 'READY':
          this.isReady = true;
          this.loadedModelKey = msg.modelKey;
          this.onReady(msg);
          break;

        case 'STREAM': {
          const entry = this.pendingRequests.get(msg.requestId);
          if (entry && typeof entry.onStream === 'function') {
            entry.onStream({
              requestId: msg.requestId,
              text: msg.text || '',
              elapsedMs: msg.elapsedMs || 0
            });
          }
          break;
        }

        case 'DONE': {
          const entry = this.pendingRequests.get(msg.requestId);
          if (entry) {
            this.pendingRequests.delete(msg.requestId);
            entry.resolve({
              requestId: msg.requestId,
              text: msg.text,
              latencyMs: msg.latencyMs || 0,
              mode: 'local-ai'
            });
          }
          break;
        }

        case 'ERROR': {
          if (msg.requestId !== undefined && this.pendingRequests.has(msg.requestId)) {
            const entry = this.pendingRequests.get(msg.requestId);
            this.pendingRequests.delete(msg.requestId);
            entry.reject(new Error(msg.error || 'Inference error'));
          } else {
            this.isReady = false;
            this.onError(msg.error || 'Worker initialization failed');
          }
          break;
        }

        case 'DISPOSED':
          this.isReady = false;
          this.loadedModelKey = null;
          this.onDisposed();
          break;
      }
    });

    this.worker.addEventListener('error', (err) => {
      this.isReady = false;
      this.onError(err.message || 'Web Worker runtime error');
    });
  }

  initModel(modelKey) {
    this.ensureWorker();
    this.isReady = false;
    this.worker.postMessage({ type: 'INIT', modelKey });
  }

  generate({ requestId, systemPrompt, question, maxNewTokens = 40, onStream }) {
    if (!this.worker || !this.isReady) {
      return Promise.reject(new Error('Local AI model is not ready.'));
    }

    return new Promise((resolve, reject) => {
      this.pendingRequests.set(requestId, { resolve, reject, onStream });
      this.worker.postMessage({
        type: 'GENERATE',
        requestId,
        systemPrompt,
        question,
        maxNewTokens
      });
    });
  }

  cancel(requestId) {
    if (this.pendingRequests.has(requestId)) {
      const entry = this.pendingRequests.get(requestId);
      this.pendingRequests.delete(requestId);
      entry.reject(new Error('CANCELLED'));
    }
    if (this.worker) {
      this.worker.postMessage({ type: 'CANCEL', requestId });
    }
  }

  unload() {
    for (const [reqId, entry] of this.pendingRequests.entries()) {
      entry.reject(new Error('CANCELLED'));
      this.pendingRequests.delete(reqId);
    }
    if (this.worker) {
      this.worker.postMessage({ type: 'DISPOSE' });
      this.worker.terminate();
      this.worker = null;
    }
    this.isReady = false;
    this.loadedModelKey = null;
    this.onDisposed();
  }
}

