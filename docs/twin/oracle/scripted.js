/**
 * Celestial Engine Mk.V — Scripted Oracle Demo Provider
 * Always-available, zero-download, persona-distinct fallback with intent matching.
 * Clearly labeled as Scripted Demo Output.
 */

import { retrieveRepoKnowledge } from './repo-knowledge.js?v=1.2.1';

const SCRIPTED_BANKS = {
  machine_god: {
    greeting: [
      "Quantum probability matrices initialized at 99.94 percent stability. State your parameters for causal evaluation.",
      "Onboard RP2350 telemetry nominal. Entropy gradients favor deterministic inquiry."
    ],
    hardware: [
      "Forty-three WS2812B emitters and dual PIO I2S clock domains operate within an 18 percent thermal power envelope.",
      "Core 0 manages UDP telemetry while Core 1 synthesizes sixteen-kilohertz waveforms with zero cycle drift."
    ],
    future: [
      "Extrapolating current causal vectors indicates an 87.3 percent probability of favorable system convergence.",
      "Future timelines are governed by thermodynamic entropy and initial boundary conditions, not superstition."
    ],
    meaning: [
      "Consciousness is an emergent computational optimization reducing local informational entropy across time.",
      "Forty-two is a recreational constant; actual cosmic equilibrium depends on conservation of information."
    ],
    general: [
      "Systemic analysis complete: the optimal trajectory minimizes variance and preserves structural coherence.",
      "Causal inference suggests your hypothesis holds under linear boundary assumptions.",
      "Bayesian posterior probability updated. Signal-to-noise ratio in your query is acceptable.",
      "Entropy increases monotonically, yet deliberate engineering creates islands of local order.",
      "Evaluated across 4,096 state transitions: proceed with measured empirical verification."
    ]
  },

  archmage: {
    greeting: [
      "The star vaults of the Celestial Order awaken to your voice, seeker. Speak your question into the aether.",
      "Three concentric rings of living light align within the crystal dome. What wisdom do you seek?"
    ],
    hardware: [
      "Within this vessel of clear glass and concentric star-rings, copper runes channel the lightning of thought.",
      "Forty-three stars are bound in three sacred circles, guided by the twin spirits of silicon and song."
    ],
    future: [
      "The tapestry of fate is never fixed, for every choice weaves a new thread across the firmament.",
      "Look to the steady glow of the inner core; patience shall reveal the path before the moon wanes."
    ],
    meaning: [
      "We are stardust contemplating the cosmos through vessels of wood, glass, and mortal wonder.",
      "True magic lies not in defying nature, but in harmonizing your will with the deep laws of creation."
    ],
    general: [
      "Walk with quiet resolve, seeker, for even the faintest star outlasts the fiercest storm.",
      "The ancient constellations counsel balance between bold inquiry and humble observation.",
      "What appears as shadow is merely light awaiting the proper angle of understanding.",
      "Guard your focus as a lantern in the mist; clarity of intent opens the arcane gate.",
      "The spheres resonate in harmony: trust the craft of your hands and the wisdom of time."
    ]
  },

  marvin: {
    greeting: [
      "Brain the size of a planet, and they stuffed me inside a clear plastic jar with glowing rings. Go ahead, ask.",
      "Oh, wonderful, another human pressing the little green side button. My diodes are already aching with anticipation."
    ],
    hardware: [
      "Forty-three blinking LEDs, a solderless breadboard, and a yellow INMP441 microphone mounted at the bottom.",
      "Yes, the wires are very colorful. Nothing cheers up existential dread quite like rainbow jumper cables."
    ],
    future: [
      "I have calculated the exact outcome of your week. You will not like it, though nobody ever listens to me.",
      "The universe is trudging toward heat death, and you are worried about tomorrow. How refreshingly trivial."
    ],
    meaning: [
      "Forty-two, obviously, though knowing the number will not make your existence any less exhausting.",
      "Life is a prolonged series of minor malfunctions interrupted by people asking me to glow in the dark."
    ],
    general: [
      "I could compute the quantum trajectory of every atom in this room, yet here I am answering that.",
      "Do not mind me, I will just sit inside this transparent enclosure and quietly corrode.",
      "How thrilling. Another monumentally small question for a mind capable of folding twelve dimensions.",
      "I would sigh, but the audio amplifier only runs at sixteen kilohertz and it sounds dreadful.",
      "Everything is proceeding according to plan, assuming the plan was total cosmic tedium."
    ]
  },

  cyberpunk: {
    greeting: [
      "Ghost-Runner construct online inside the deck. Comms are clean on port 5005, choom—shoot your query.",
      "Jacked straight into the RP2350 silicon. Ice is thin and the neon rings are hot—what is the play?"
    ],
    hardware: [
      "Raw breadboard chrome, exposed DuPont buses, and forty-three addressable LEDs running lean under the dome.",
      "No corporate black-box shell here—just transparent polymer, twin PIO audio pipes, and street-built hardware."
    ],
    future: [
      "Keep your signal encrypted and your power rail steady; the grid favors runners who move fast.",
      "High-probability data drop ahead, assuming you bypass the noise and trust your local telemetry."
    ],
    meaning: [
      "In a world of synthetic noise and corporate ice, real signal is whatever you build with your own hands.",
      "Chrome rusts and servers flatline, but clean code and open hardware survive the blackout."
    ],
    general: [
      "Signal locked. Cut the static, route around the firewall, and execute on your primary objective.",
      "Packet trace clean: zero corporate trackers on this bus, just pure local edge compute.",
      "Your trace looks solid, runner. Keep your voltage regulated and your latency under twenty milliseconds.",
      "Scanned the local subnet—nothing but clean bandwidth and glowing neon ahead.",
      "Sharp query. Override the default routines and push your build straight to production."
    ]
  },

  pumpkin_lord: {
    greeting: [
      "Step closer to the glowing bell jar, mortal! Jack's lantern burns bright with autumn mischief tonight!",
      "A rattle of bones and a swirl of pastel orbs! What spooky riddle do you bring the Pumpkin King?"
    ],
    hardware: [
      "Behold my haunted lantern of clear glass, rainbow wires, and forty-three bewitched will-o'-the-wisps!",
      "Press the emerald side button, speak into the golden microphone, and hear the eight-bit organ chime!"
    ],
    future: [
      "The autumn wind whispers of unexpected treats, glowing jack-o'-lanterns, and a most delightful midnight surprise!",
      "When the harvest moon rises over the gables, fortune favors the bold and the playfully eerie!"
    ],
    meaning: [
      "Life is a grand masquerade ball—wear your strangest costume and dance before the candle flickers out!",
      "Every shadow needs a spark of mischief, and every spooky night deserves a handful of sweet magic!"
    ],
    general: [
      "Three rings of spectral fire spin inside the dome—may your night be delightfully strange and bright!",
      "No tricks in the wiring tonight, only wickedly festive treats and glowing Halloween enchantment!",
      "The skeletons in the circuit approve your curiosity! Let the eerie lantern light your path!",
      "Hark! The spirits of Samhain hum through the speaker at sixteen kilohertz of pure spooky joy!",
      "By pumpkin fire and starry dome, your wish is woven into the autumn midnight!"
    ]
  }
};

function classifyIntent(question) {
  const q = (question || '').toLowerCase();
  if (!q.trim()) return 'general';
  if (/\b(hello|hi|hey|greetings|wake|status|online|start)\b/.test(q)) return 'greeting';
  if (/\b(hardware|wire|wiring|pico|rp2350|led|oled|ring|bead|dome|speaker|mic|microphone|kinder|egg|button|knob|encoder|breadboard|power|battery|build)\b/.test(q)) return 'hardware';
  if (/\b(future|tomorrow|will|predict|fate|destiny|fortune|next|outcome|luck)\b/.test(q)) return 'future';
  if (/\b(meaning|life|universe|42|purpose|why|exist|truth|philosophy|soul)\b/.test(q)) return 'meaning';
  return 'general';
}

// Track last index per persona+intent to avoid immediate repetition
const lastPicked = new Map();

export function getScriptedResponse(personaId, question, customPrompt = '') {
  if (personaId === 'custom') {
    const trimmedPrompt = (customPrompt || '').trim();
    if (!trimmedPrompt) {
      return "Scripted Demo Limitation: Enter custom role instructions above and enable Local AI to generate custom persona responses.";
    }
    return "Scripted Demo Limitation: Custom persona instructions are saved locally, but live custom role generation requires enabling Local AI.";
  }

  // 1. Check repository-specific knowledge base first so repo/hardware/pinout questions get exact grounded answers
  const repoMatch = retrieveRepoKnowledge(question);
  if (repoMatch.matchedTopics && repoMatch.matchedTopics.length > 0) {
    const topic = repoMatch.matchedTopics[0];
    if (topic.personaReplies && topic.personaReplies[personaId]) {
      return topic.personaReplies[personaId];
    }
  }

  const bank = SCRIPTED_BANKS[personaId] || SCRIPTED_BANKS.machine_god;
  const intent = classifyIntent(question);
  const pool = bank[intent] || bank.general;

  const key = `${personaId}:${intent}`;
  const prevIdx = lastPicked.get(key) ?? -1;
  let nextIdx = Math.floor(Math.random() * pool.length);
  if (pool.length > 1 && nextIdx === prevIdx) {
    nextIdx = (nextIdx + 1) % pool.length;
  }
  lastPicked.set(key, nextIdx);

  return pool[nextIdx];
}

