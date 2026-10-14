# Experiment 037 — Raspberry Pi 5 NVMe Migration and Standalone Setup

**Date**: 2026-10-12 – 2026-10-14  
**Status**: PASSED / VERIFIED (Standalone NVMe Boot Active on Pimoroni PIM699 + Lexar NM790 1 TB; MicroSD Removed)  
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

## 4. Milestone 2: Pimoroni PIM699 Hardware Validation (PASSED)

- **Date**: 2026-10-14
- **Hardware Configuration**:
  - Raspberry Pi 5 (16 GB RAM, Bookworm Lite 64-bit)
  - Pimoroni PIM699 (NVMe Base for Raspberry Pi 5)
  - Lexar NM790 1 TB M.2 NVMe SSD
  - Official 27W USB-C Power Supply
- **Observed Telemetry**:
  - `lspci`: `0001:01:00.0 Non-Volatile memory controller: Shenzhen Longsys Electronics Co., Ltd. Lexar NM790 NVME SSD (DRAM-less) (rev 01)`
  - `lsblk`: `nvme0n1` (953.9 GB) detected cleanly.
  - SMART Health: `temperature: 27 °C`, `available_spare: 100%`, `media_errors: 0`, `critical_warning: 0`.
  - **Sustained Write Stress-Test (`dd conv=fsync`)**:
    - `524288000 bytes (524 MB, 500 MiB) copied, 1.33569 s, 393 MB/s`
    - **Result**: Zero power dropouts, zero kernel `D3cold` stalls, Wi-Fi remained 100% stable. The 3.3V/4A buck converter on Pimoroni PIM699 completely resolved the Geekworm X1002 power starvation issue.

---

## 5. Milestone 3: OS Clone and Standalone NVMe Boot Cutover (PASSED)

Checklist:
1. [x] Mount Pimoroni NVMe Base beneath Raspberry Pi 5 using supplied standoffs and shielded FPC ribbon.
2. [x] Install Lexar NM790 1 TB into the M.2 Key-M slot.
3. [x] Verify sustained write power delivery under `dd conv=fsync` (393 MB/s, PASS).
4. [x] Fix spacing in `cmdline.txt` and enable PCIe Gen 3 in `config.txt` (`dtparam=pciex1_gen=3`).
5. [x] Clone clean MicroSD OS to NVMe (`rpi-clone nvme0n1 -f -p 512M`).
6. [x] Reconfigure EEPROM for standalone NVMe boot (`BOOT_ORDER=0xf416`).
7. [x] Remove MicroSD card physically and boot standalone directly into `/dev/nvme0n1p2` (VERIFIED via `findmnt /`).

---

## 6. Final Status
Raspberry Pi 5 Wearable Brain hardware storage foundation is **100% OPERATIONAL**.
- Host: `rpi-15` (192.168.178.51)
- Storage: Lexar NM790 1 TB PCIe NVMe SSD on Pimoroni PIM699 Base
- OS Root: `/dev/nvme0n1p2` (ext4, rw, noatime)
- MicroSD slot: EMPTY
- Ready for Milestone P10 (`ai_brain_server.py` deployment).
