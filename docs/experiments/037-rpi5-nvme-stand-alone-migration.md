# Experiment 037 — Raspberry Pi 5 NVMe Migration and Standalone Setup

**Date**: 2026-10-12 – 2026-10-13  
**Status**: PAUSED (Milestone 1 Complete: Hardware Root Cause Isolated; MicroSD Baseline Operational; Awaiting Pimoroni PIM699 for Final NVMe Boot Cutover)  
**Target Hardware**: Raspberry Pi 5 (BCM2712, 16GB RAM, Active Cooler, Official 27W PSU, Lexar NM790 1TB NVMe, Pimoroni PIM699 NVMe Base)  
**Associated Post-Mortem Report**: [`docs/hardware/rpi5_nvme_migration_incident_report.md`](../hardware/rpi5_nvme_migration_incident_report.md)

---

## 1. Objective
Establish a clean, high-performance Linux environment on the Raspberry Pi 5 wearable brain, resolve the periodic read-only NVMe failure, achieve stable NVMe storage operation, and prepare the host for autonomous `ai_brain_server.py` background deployment.

---

## 2. Investigation & Findings Summary

1. **Clean MicroSD Baseline (PASS):**
   - Booted Debian 12 Bookworm Lite (64-bit) on MicroSD (`/dev/mmcblk0p2`, `rw,noatime`).
   - Confirmed pristine power delivery from official 27W PSU: `vcgencmd get_throttled` -> `throttled=0x0`.
   - Identified bootloader order: `BOOT_ORDER=0xf416` prioritizes NVMe (`6`) before SD (`1`). Adjusted to `0xf461` for safe non-root testing.

2. **SSD Health Verification (PASS):**
   - Queried SMART telemetry on Lexar NM790 1 TB (`/dev/nvme0`):
     - `media_errors: 0`
     - `critical_warning: 0`
     - `available_spare: 100%`
     - `temperature: 27 °C` (Sensor 1: 31 °C)
   - Confirmed flash memory and internal controller health are 100% intact.

3. **Read Performance & HMB Resolution (PASS):**
   - Initial read test with HMB enabled hung for 32 seconds due to BCM2712 PCIe inbound DMA deadlock with DRAM-less Maxio MAP1602.
   - Injecting `nvme.max_host_mem_size_mb=0` and `nvme_core.default_ps_max_latency_us=0` in `/boot/firmware/cmdline.txt` completely eliminated the deadlock.
   - Benchmark: `dd if=/dev/nvme0n1 of=/dev/null bs=1M count=100` completed in **0.231 s at 453 MB/s** (near maximum theoretical bandwidth for PCIe Gen 2 x1).
   - Ext4 filesystem journal cleanly replayed and validated with `sudo e2fsck -f -y /dev/nvme0n1p2`.

4. **Write Power Delivery Limitation (ROOT CAUSE CONFIRMED):**
   - Sustained write flushing (`conv=fsync` on 500 MB) triggered a power rail collapse:
     - 95s: Wi-Fi SDIO bus timeout (`brcmfmac: resumed on timeout`).
     - 103s: M.2 voltage collapsed to 0V (`Unable to change power state from D3cold to D0`).
     - 106s: Controller reset failure -19; block device collapsed to `0B`.
   - High-performance PCIe 4.0 SSDs (NM790) draw 2.5A–3.5A (8–11W) current spikes during NAND page write operations.
   - The Geekworm X1002 power delivery path cannot sustain this load, resulting in voltage brownout.

---

## 3. Resolution Decision

- **Hardware Action:** Retire the Geekworm X1002 and P580-V2 case. Procure **Pimoroni PIM699 (NVMe Base for Raspberry Pi 5)**.
- **Justification:** PIM699 includes a heavy-duty synchronous 3.3V/4A buck regulator and low-ESR ceramic bypass banks designed specifically for high-power Gen 4 drives like the Lexar NM790 (certified by Pimoroni and Tom's Hardware).
- **Cost Benefit:** ~€15 adapter replacement preserves the already-owned €90 1 TB SSD.

---

## 4. Next Milestone: Pimoroni PIM699 Cutover Checklist

When the Pimoroni PIM699 arrives:
1. [ ] Mount Pimoroni NVMe Base beneath Raspberry Pi 5 using supplied standoffs and shielded FPC ribbon.
2. [ ] Install Lexar NM790 1 TB into the M.2 Key-M slot.
3. [ ] Keep `config.txt` parameters:
   ```ini
   dtparam=nvme
   dtparam=pciex1_gen=3
   ```
4. [ ] Keep `cmdline.txt` deep-sleep prevention parameter:
   ```text
   nvme_core.default_ps_max_latency_us=0
   ```
5. [ ] Clone stable OS from MicroSD to NVMe:
   ```bash
   sudo rpi-clone nvme0n1 -U -p 512M
   ```
6. [ ] Reconfigure EEPROM for standalone NVMe boot:
   ```bash
   sudo rpi-eeprom-config -o /tmp/bootconf.txt
   sudo sed -i 's/BOOT_ORDER=.*/BOOT_ORDER=0xf416/' /tmp/bootconf.txt
   sudo rpi-eeprom-config --apply /tmp/bootconf.txt
   ```
7. [ ] Remove MicroSD and boot standalone at 750–850 MB/s!
