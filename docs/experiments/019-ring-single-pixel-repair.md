# 019 — 16-LED ring: one dark pixel, repair pending

Follow-up 2026-10-11: user reports soldering the spare after the 24-LED ring, with all power off. [Experiment 020](020-removable-fourth-ring-firmware.md) provides a separately built experimental UF2; it has not been flashed and does not claim permanent repair or verified serial addressing. Earlier recommendations below retain their chronological context.

Date: 2026-10-10. Status: **FAILED normal ring operation; repair pending**. Active firmware remains 1.0.5-AUDIO-GAIN with 43 pixels (7 + 12 + 24).

## Goal
Recover the spare 16-LED ring using existing tools if its single faulty position can be isolated.

## Hardware used / power off
Existing ring, soldering iron and thin insulated jumper wire. Disconnect USB and every external supply before moving probe wires or soldering.

## Actual user observation
User reports exactly one nonworking LED while touching data to successive contacts starting at DI. Attached photo shows fifteen lit packages and one dark package near the yellow wire. The photo does not identify the individual DIN/DOUT pins, yellow-wire endpoint, data-chain continuity, or whether the fault is inside the LED or its solder joints/traces. Do not assign a particular corner pin from this image.

Follow-up photo shows the component side with all LEDs unlit and more visible solder contacts. The suspect position is the package just below the yellow-wire attachment in this orientation, identified by comparison with the earlier lit photo. Signal trace continuity and exact DIN/DOUT pad assignment remain unverified; an underside view and close detail of this position are still needed before a pad-specific repair instruction. No cutting, desoldering or bypass success has been reported.

Subsequent user observation: yellow wire connected to breadboard A20; a jumper connects ring DIN to the suspect LED's upper-right outer contact. All other ring pixels illuminate; the previously lit pixel changes color more often. No cut or isolation of the old output reported. This is evidence that signal injection at that contact reaches the remaining pixels, but it does not prove the contact is DIN/DOUT, establish the underlying fault, or verify an electrically safe permanent bypass. Color-rate change alone does not distinguish intended animation/address duplication from unstable data.

Repository experiment 006 maps A20/B20 to Pico physical pin 20, GP15, in its recorded breadboard placement. If that placement is still current, the new jumper is connected directly to a 3.3 V GPIO, before the level shifter. Disconnect all power and remove the exploratory jumper before further changes: an unisolated powered LED DOUT must not be driven from or tied to Pico GP15. Await a close underside view and the actual jumper endpoints before advising output isolation or permanent soldering.

User says the underside shows no traces. Further underside photos are not required. User has a multimeter but is unfamiliar with its use. Next milestone is a continuity-mode self-check with the probe tips touching, without connecting the meter to the ring. Ring/power measurements and individual pin assignment remain pending; no meter result or repair success is claimed.

User confirms continuity self-check succeeded (probe tips touching produces the expected response): meter-use milestone **PASSED**. Next step: disconnect all supplies and electrically isolate the ring from the rest of the circuit, remove the temporary bypass jumper, then compare the suspect upper-right outer contact against explicitly labeled ring GND and 5V/VCC pads. Record sustained continuity and displayed values for both measurements; do not infer supply-pad functions from wire colors. A transient beep or a semiconductor path is not proof of a direct copper connection. DIN versus DOUT remains unassigned pending results.

User confirms the suspect contact A gives no beep against either labeled GND or supply pad. Display readings and contact reliability were not reported; this suggests no low-resistance connection to those rails but does not identify DIN or DOUT. Next unpowered continuity step is ring DI pad versus each of the suspect LED's four contacts, with all external connections and the exploratory jumper removed. A direct low-resistance connection identifies a path to the ring input; if none is found, an intervening pixel or resistor, a trace fault, or probe contact quality remains possible. No pad lifting or bypass soldering advised yet.

User reports no beep between ring DI and any of the four suspect LED contacts. Negative continuity measurements remain inconclusive because display values and reliable probe-to-board contact have not been established. Before additional path tracing, perform one control measurement across two exposed metal points on the same DI solder pad, with the ring disconnected. It should give sustained continuity near the touching-probe baseline; do not touch solder mask or the plastic LED body. If this control fails, resolve probe contact/meter setup before interpreting prior readings. If it passes, a close view identifying the actual probe contact points is needed before choosing a pad-specific repair.

User confirms sustained beep across the same DI pad: board-pad contact control **PASSED**. Contact at the individual LED leads remains to be confirmed. No direct DI-to-suspect continuity does not itself prove a broken trace, since another pixel's active data path may intervene. Next step is mapping each of the four suspect LED contacts to the labeled ring GND and supply pads on the isolated, unpowered ring. Record which lead has sustained near-zero continuity to each rail; only afterward identify remaining data contacts. Do not assign orientation from generic package diagrams alone.

User maps supply to lower-right and GND to upper-left in the current front-view orientation: local rail identification **PASSED**. In the classic Worldsemi WS2812B 5050 four-contact pin layout, rotating the documented package to these measured rail positions predicts upper-right A = DOUT and lower-left = DIN. Source pin functions: 1 VDD, 2 DOUT, 3 VSS, 4 DIN; drawing has 1/2 on the left and 4/3 on the right in its standard view. This is a package-based inference anchored to two actual rail measurements, not independent confirmation of the board's exact LED variant.

Before any irreversible output isolation, trace A to the exposed leads of the two neighboring pixels on the unpowered isolated ring. A direct continuity link to one neighbor would locate the outgoing board net consistent with the proposed DOUT mapping. Record which neighbor/lead beeps; do not connect two active outputs. Intended local bypass is the faulty pixel's DIN board pad to DOUT board pad after disconnecting its DOUT lead from that net, not controller DI injected at an arbitrary downstream contact. Isolation method remains pending physical lead accessibility.

User confirms upper-right A connects to the lower-left lead of the counterclockwise neighbor; the suspect lower-left lead connects to the upper-right lead of the clockwise neighbor. These measured local links match the WS2812B layout anchored to measured power/GND: lower-left DIN receives from the clockwise predecessor, upper-right DOUT feeds the counterclockwise successor. Local signal routing milestone **PASSED**; original component failure versus solder fault remains undiagnosed.

Next repair milestone: with all power/external connections removed, detach only the suspect upper-right DOUT metal lead from its PCB pad if that lead physically protrudes and can be lifted after solder melts. Use the existing soldering iron and a thin needle/tweezers if available; never lever the LED body or lift the PCB pad. If the contact is flush/under the package or does not release with melted solder, stop and choose another isolation method from a close view. Before adding a bypass or powering up, verify (1) raised LED lead no longer has sustained continuity to its PCB pad, and (2) PCB pad still has continuity to the lower-left lead of the counterclockwise neighbor. Output isolation not yet reported. Subsequent intended jumper: suspect lower-left DIN board pad to its upper-right DOUT board pad, with the old DOUT lead kept physically isolated.

User does not feel able to desolder the output lead. No desoldering attempt, output isolation, or completed repair has been reported. Defer this rework rather than forcing an inaccessible lead or choosing a blind trace cut. Keep the 16-LED ring excluded from the existing verified 43-pixel array. With all power disconnected, remove the exploratory bypass and leave the spare ring disconnected. No firmware/count/build/flash changes required. Future repair requires a feasible, verified output-isolation method or assistance with soldering; the provisional jumper is not accepted as a permanent fix.

User reiterates a live observation: ring DI jumpered to the faulty pixel's upper-right DOUT makes the entire ring light except the faulty pixel. Accept this as an observed functional data injection bypass; do not claim that this wiring cannot work. It may work if the old DOUT is inactive/high impedance, but this electrical state has not been established by the unpowered continuity results. The original DOUT remains attached, so driving its net can potentially contend with its output and, in the recorded direct A20/GP15 hookup, can expose a 3.3 V controller net to the LED-side output. No forced desoldering or further live probing requested.

Separate addressing limitation: connecting the ring's common DI to a downstream point can create branches receiving overlapping pixel data. This is not equivalent to bypassing only one local pixel in a single fifteen-pixel chain. The proper local path is the clockwise predecessor's DOUT (the faulty pixel's lower-left DIN PCB net) to the counterclockwise successor's DIN (the upper-right DOUT PCB net), with the faulty output isolated. Current state remains provisional visual function observed, permanent isolation and serial-chain mapping unresolved; active firmware count stays 43.

User asks where to connect the spare ring DIN/DO or whether to keep it separate. Current recommendation: keep it separate from the verified 7→12→24 array while output isolation and serial addressing are unresolved; firmware still transmits only 43 frames. Future integration, after a proper one-pixel local bypass is established, follows the original concentric order: 12-ring DO→repaired-ring DI, repaired-ring DO→24-ring DI, replacing the existing direct 12 DO→24 DI link. Power/GND use the existing appropriate LED rails, never data pads or GPIO as a power source. A correct one-pixel bypass would imply 7+12+15+24=58 logical pixels; the current branched common-DI jumper does not establish that count. No integration, rewiring, firmware-count change or flash performed.

User authorizes rewiring with quickly removable power leads and clarifies that the source is the same existing +5 V rail, not another supply. User also confirms the common DI→upper-right DOUT jumper remains and the old output lead is still soldered down. Supply choice is resolved; output isolation and addressing remain unresolved. Prepare three removable leads on labeled ring +5 V, GND and DI pads with all power disconnected, leaving their free ends disconnected; DO needs no lead when the ring is last. No powering or serial integration advised at this stage.

Updated future connection recommendation for quick removal: append the spare ring after the existing 24-pixel ring rather than inserting it between the 12 and 24. This preserves the known 7→12→24 chain: 24 DO→spare DI, spare DO left free, spare +5 V/GND from the same existing rails via removable leads. When removing the spare, disconnect its data as well as power with the entire system off; leaving data connected to an unpowered LED can cause parasitic powering. Appending last avoids interrupting the three working rings when the spare is removed. This supersedes the previous future insertion recommendation for this removable arrangement; no actual rewiring or four-ring firmware has been verified. Do not assume 58 serial addresses with the current common-DI injection jumper.

## Wiring diagram/table — conceptual only, not approved pad locations

| Connection | Pico physical pin | GPIO/function | Voltage/note |
|---|---|---|---|
| Existing controller data | Existing pin 20 if using project connection | GP15 through existing LED data path | No controller rewiring |
| Previous good pixel DOUT → following good pixel DIN | — | Local bypass around failed position | First isolate failed pixel DOUT from the following DIN; avoid tying two driven outputs together |
| Ring VDD/GND | Existing connections | Existing power rails | No power-rail changes; no 5 V into Pico GPIO |

If all following pixels already respond normally through the unmodified chain, the dark pixel can remain installed; a data bypass is unnecessary. A bypass shortens the logical chain by one pixel. Removing a four-pad 5050 package with one iron is possible but risks lifting pads if forced. Exact repair method depends on a close, unpowered view of the package contacts and board traces.

## Code/build changes and commands
None. Do not reinsert this ring into the verified 43-pixel chain or change firmware counts before the repair is confirmed. No build/flash commands are needed from repository root.

## Expected observations
If bypass repair is chosen and verified, fifteen remaining LEDs should respond normally to data at the ring's ordinary DI input. If the dark package already passes data, fifteen illuminated positions can work with the original sixteen logical addresses. These outcomes have not yet been observed under a confirmed wiring arrangement.

## If it fails / diagnostic notes
1. Stop with power disconnected if individual pad functions are unclear; obtain a close view of both board sides.
2. Confirm signal path and isolate the old output before soldering a bypass.
3. If subsequent pixels do not respond, check the local solder/trace connections before concluding another LED failed.

## Documentation update / resolution
Checkpoint 2026-10-11: **the user repaired the ring by soldering a jumper around one failed LED, without desoldering it**. Actual jumper: common ring DIN→suspect upper-right DOUT contact; original output remains soldered. The ring is installed last after the 24-ring, powered by removable leads from the existing rails, with its DO free. Experiment 022 records user-confirmed light response on all four rings; damaged position stays dark and the user reports good COSMIC SNAKE. White flashes in RAINBOW remain open (025), so exact address mapping and output isolation are not claimed verified. Earlier pending-measurement paragraphs above are debugging history. Full four-ring music/light rewrite is explicitly listed in [tasks](../tasks.md).

Inventory now records the user repair and four-ring data response. This does not establish electrical isolation of the failed output or resolve the remaining white flashes. No further wiring changes are requested at this checkpoint.

Reference: [Worldsemi WS2812B datasheet, hosted by Adafruit](https://cdn-shop.adafruit.com/datasheets/WS2812B.pdf), DIN/DOUT functions and cascaded data path. This reference does not establish the orientation or exact chip revision on the user's board.
