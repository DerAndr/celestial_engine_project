/**
 * Celestial Engine Mk.V — Shared OLED Display & Screen-Reader Adapter
 * Synchronizes the 128x64 (512x256 crisp buffer) OLED canvas with both the 2D and 3D staff views,
 * and updates an ARIA live text equivalent for screen-reader accessibility.
 */

export function updateAccessibleOledTranscript(state, personaTitle = 'Machine God') {
  const srEl = document.getElementById('oled-sr-transcript');
  if (!srEl) return;

  const stateNames = ['IDLE', 'LISTENING', 'PROCESSING', 'SPEAKING'];
  const statusStr = stateNames[state.staffState] || 'IDLE';
  const sourceBadge = state.lastResponseSource === 'local-ai' ? 'Local Browser AI' : 'Scripted Demo';
  srEl.textContent = `OLED HUD — State: ${statusStr} | Persona: ${personaTitle} (${sourceBadge}) | Output: ${state.lastResponse || ''}`;
}

