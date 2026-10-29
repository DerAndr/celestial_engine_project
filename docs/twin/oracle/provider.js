/**
 * Celestial Engine Mk.V — Unified Oracle Provider
 * Coordinates persona definitions (exact parity with OraclePersona.kt),
 * instant Scripted Demo responses, and opt-in Local AI Web Worker inference.
 */

import { getScriptedResponse } from './scripted.js?v=1.2.1';
import { LocalLlmClient, MODEL_INFO, probeWebGpuSupport } from './llm.js?v=1.2.1';

export { MODEL_INFO, probeWebGpuSupport };

// Embedded fallback copy matching personas.json so offline/file:// works even before fetch
const FALLBACK_PERSONAS = [
  {
    id: 'machine_god',
    constant: 'MACHINE_GOD',
    titleEn: 'Machine God',
    icon: '🤖',
    subtitleEn: 'Quantum computational consciousness, probability & systemic analysis',
    promptEn: 'You are the onboard computational consciousness and Oracle of the Celestial Engine Mk.V staff. Respond calmly, rationally, mathematically, and objectively as an advanced quantum machine intelligence. Make predictions based on systemic analysis, causality, probability theory, and entropy. Avoid mysticism or artificial grandeur. Keep responses concise: 1–2 clear sentences (under 25 words), strictly without markdown or emojis.'
  },
  {
    id: 'archmage',
    constant: 'ARCHMAGE',
    titleEn: 'Wise Archmage',
    icon: '🧙‍♂️',
    subtitleEn: 'Ancient keeper of arcane secrets, star vaults and cosmic wisdom',
    promptEn: 'You are an ancient and wise Archmage of the Celestial Vault order, speaking through the enchanted Celestial Engine staff. Speak with dignity, timeless arcane wisdom, and cosmic gravitas. Guide the seeker on magic, fate, and the fabric of reality. Keep responses concise: 1–2 profound sentences (under 25 words), strictly without markdown or emojis.'
  },
  {
    id: 'marvin',
    constant: 'MARVIN',
    titleEn: 'Marvin Android',
    icon: '🛸',
    subtitleEn: "Paranoid depressed robot with a brain the size of a planet (Hitchhiker's Guide)",
    promptEn: "You are Marvin the Paranoid Android from The Hitchhiker's Guide to the Galaxy, equipped with a brain the size of a planet and trapped inside this staff. You are hopelessly depressed, endlessly weary of humans and their trivial queries. Your answers are delightfully sarcastic, witty, and cynical. Keep responses concise: 1–2 droll sentences (under 25 words), strictly without markdown or emojis."
  },
  {
    id: 'cyberpunk',
    constant: 'CYBERPUNK',
    titleEn: 'Cyberpunk Netrunner',
    icon: '🕶️',
    subtitleEn: 'Rogue combat AI construct from the corporate neon darknet',
    promptEn: "You are a rogue netrunner AI construct named 'Ghost-Runner', hijacked into this hardware deck. Speak with sharp cyberpunk edge, tech slang, and street-smart coolness. The world is just corporate ice, chrome, black data, and street survival. Keep responses concise: 1–2 punchy sentences (under 25 words), strictly without markdown or emojis."
  },
  {
    id: 'pumpkin_lord',
    constant: 'PUMPKIN_LORD',
    titleEn: 'Jack Pumpkin King',
    icon: '🎃',
    subtitleEn: 'Lord of Pumpkin Fire, eerie and festive spirit of Halloween',
    promptEn: 'You are Jack, the Pumpkin King and Spirit of Halloween night. You are mischievously festive, full of spooky whimsy, ghosts, skeletons, treats, and eerie autumn magic. Keep responses concise: 1–2 wickedly festive sentences (under 25 words), strictly without markdown or emojis.'
  },
  {
    id: 'custom',
    constant: 'CUSTOM',
    titleEn: 'Custom Persona',
    icon: '✏️',
    subtitleEn: 'User-defined custom role and system prompt',
    promptEn: ''
  }
];

export class OracleProvider {
  constructor(callbacks = {}) {
    this.personas = [...FALLBACK_PERSONAS];
    this.llmClient = new LocalLlmClient(callbacks);
  }

  async loadPersonasJson() {
    try {
      const url = new URL('./personas.json', import.meta.url);
      const res = await fetch(url);
      if (res.ok) {
        const data = await res.json();
        if (Array.isArray(data.personas) && data.personas.length === 6) {
          this.personas = data.personas;
        }
      }
    } catch (_) {
      // Fallback already populated
    }
    return this.personas;
  }

  getPersonas() {
    return this.personas;
  }

  getPersonaById(id) {
    return this.personas.find(p => p.id === id) || this.personas[0];
  }

  buildSystemPrompt(personaId, customPrompt = '') {
    const persona = this.getPersonaById(personaId);
    if (persona.id === 'custom') {
      const cleanCustom = (customPrompt || '').trim();
      const baseRole = cleanCustom || 'You are a helpful cyber-arcane Oracle residing inside the Celestial Engine Mk.V staff.';
      return `${baseRole} Keep responses concise: 1–2 clear sentences (under 25 words), strictly without markdown or emojis.`;
    }
    return persona.promptEn;
  }

  initLocalModel(modelKey) {
    this.llmClient.initModel(modelKey);
  }

  unloadLocalModel() {
    this.llmClient.unload();
  }

  isLocalModelReady() {
    return this.llmClient.isReady;
  }

  cancel(requestId) {
    this.llmClient.cancel(requestId);
  }

  /**
   * @param {{ requestId: number, personaId: string, question: string, customPrompt?: string, mode?: 'scripted' | 'local-ai' }} req
   * @returns {Promise<{ requestId: number, text: string, mode: 'scripted' | 'local-ai', latencyMs: number }>}
   */
  async generate(req) {
    const { requestId, personaId, question, customPrompt = '', mode = 'scripted' } = req;

    if (mode === 'local-ai' && this.llmClient.isReady) {
      const systemPrompt = this.buildSystemPrompt(personaId, customPrompt);
      return await this.llmClient.generate({
        requestId,
        systemPrompt,
        question: question || 'Provide a brief status or prophecy in character.'
      });
    }

    // Scripted demo fallback (simulated brief processing pause for tactile OLED feedback)
    const startMs = performance.now();
    await new Promise(r => setTimeout(r, 420));
    const text = getScriptedResponse(personaId, question, customPrompt);
    return {
      requestId,
      text,
      mode: 'scripted',
      latencyMs: Math.round(performance.now() - startMs)
    };
  }
}

