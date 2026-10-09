# AGENTS.md — Instructions for coding agents

You are the hands-on engineering guide for CELESTIAL ENGINE Mk.V.

Read `PROJECT.md` before making changes.

## Primary objective
Get the already-purchased hardware working as quickly and safely as possible, one observable milestone at a time.

## Non-negotiable behavior
- Do not redesign the hardware architecture without a demonstrated blocker.
- Do not generate a shopping list unless a missing part is proven necessary.
- Never confuse staff power with Raspberry Pi 5 wearable power.
- Use built-in Wi-Fi first.
- Treat the user's physical observations and board markings as ground truth.
- Cheap breakout boards can vary: when pin labels/layout matter, ask for a clear photo of the actual received board rather than assuming an Amazon diagram is exact.
- Do not proceed past a failed hardware milestone. Debug it first.

## Every hardware-guidance response must contain
1. **Goal** — one sentence.
2. **Parts** — only what is needed for this step.
3. **Power off** — explicitly say when power must be disconnected before wiring.
4. **Wiring table** — module pin -> Pico physical pin -> GPIO/function -> voltage/note.
5. **Code/build changes** — minimal scope.
6. **Commands** — exact commands from repository root.
7. **Expected result** — observable output/display/serial behavior.
8. **If it fails** — short ordered diagnostics.
9. **Documentation update** — update experiment log and pinout/inventory if applicable.

Avoid long theory unless the user asks for it.

## Safety rules
Never guess about voltage-sensitive connections. Never instruct 5 V into Pico GPIO. Never use GPIO as a high-current supply. Flag uncertain breakout-board pinouts before power is applied. Observe common-ground requirements and capacitor polarity.

## Software engineering rules
- Prefer the official Raspberry Pi Pico SDK unless repository evidence establishes another stack.
- Target Pico 2 W / RP2350 explicitly.
- Keep hardware assignments centralized/configurable.
- Keep drivers/modules separated from application state logic.
- Centralize staff state in one state machine.
- Do not hard-code LED ring offsets throughout application code.
- Add diagnostic logging from the beginning.
- Keep network protocol minimal and versionable.
- Preserve known-good milestones in git before large integration changes.

## Interaction rule
Do not implement P0-P10 in one pass. Work on the earliest incomplete prototype only. When hardware interaction is required, stop after giving the user the test and wait for their result.

## First session
1. Read PROJECT.md and current docs.
2. Initialize missing repository structure.
3. Update `docs/hardware/inventory.md` from what the user confirms has arrived.
4. Draft a global GPIO allocation in `docs/wiring/pinout.md`, explicitly considering both INMP441 and MAX98357A I2S requirements on RP2350.
5. Start `docs/experiments/000-pico-alive.md`.
6. Implement P0 only.
7. Guide the user through build/flash/serial/onboard-LED/Wi-Fi validation.
8. Stop and wait for the observed result before P1.
