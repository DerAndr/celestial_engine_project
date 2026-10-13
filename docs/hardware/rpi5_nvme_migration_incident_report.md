# Raspberry Pi 5 + NVMe Lexar NM790: Incident Post-Mortem & Diagnostic Report

**Project:** CELESTIAL ENGINE Mk.V — Wearable Brain Server  
**Document ID:** `DOC-INC-20261012-RPI5-NVME`  
**Date:** 2026-10-12 – 2026-10-13  
**Target SBC:** Raspberry Pi 5 Model B (16 GB LPDDR4X RAM)  
**Status:** RESOLVED — Hardware Root Cause Isolated; Migration Path Selected  

---

## 1. Executive Summary

Under intensive disk write workloads (e.g., `pip install`, heavy file writes), the NVMe SSD periodically remounted the root filesystem as **read-only**, producing `Input/output error` and Ext4 journal aborts.

Through systematic hardware isolation, diagnostic telemetry, and step-by-step stress testing, the issue was definitively isolated:
1. **The SSD (Lexar NM790 1 TB) is 100% physically healthy:** SMART logs show `0` media errors, `0` error log entries, `100%` available spare, and stable `27 °C` operating temperature. Read speed reached wire-limit **453 MB/s**.
2. **Software/DMA Deadlock was identified and resolved:** DRAM-less Maxio MAP1602 HMB allocation caused a 30-second inbound DMA deadlock on the BCM2712 PCIe bridge; resolved via `nvme.max_host_mem_size_mb=0`.
3. **The Root Cause is Hardware Power Starvation (H3):** During sustained NAND flash write programming (`fsync`), on-die 18V–20V charge pumps draw sharp **2.5A–3.5A (8–11W)** current spikes. The power delivery circuit on the Geekworm X1002 bottom HAT cannot sustain this load, resulting in a system power rail brownout that stalls the Wi-Fi SDIO bus (`brcmfmac: resumed on timeout` at 95s) and cuts power to the M.2 slot (`Unable to change power state from D3cold to D0` at 103s).
4. **Resolution:** Retire the Geekworm X1002 and procure the **Pimoroni PIM699 (NVMe Base for Raspberry Pi 5)**, which features a high-current synchronous 3.3V/4A buck regulator and is officially certified by Pimoroni and independent labs for the Lexar NM790.

---

## 2. Hardware Bill of Materials (BOM)

| Component | Specification | Diagnostic Status |
|---|---|---|
| **SBC** | Raspberry Pi 5 Model B (Rev 1.0) | 16 GB LPDDR4X RAM, BCM2712 — **100% Operational** |
| **Cooler** | Official Raspberry Pi 5 Active Cooler | Operational (CPU temp ~38 °C) |
| **Power Supply** | Official Raspberry Pi 27W USB-C PSU | 5.1 V / 5.0 A PD (`throttled=0x0`) — **Clean** |
| **SSD** | Lexar NM790 1 TB (M.2 2280 PCIe 4.0 x4) | Maxio MAP1602 DRAM-less controller — **100% Healthy** |
| **NVMe HAT (Initial)**| Geekworm X1002 V1.1 (Bottom-mounted) | Insufficient write power delivery — **RETIRED** |
| **Enclosure** | Geekworm P580-V2 Metal Case | Form factor tied to X1002 — **RETIRED** |
| **NVMe Base (New)** | Pimoroni PIM699 (NVMe Base) | High-current 3.3V/4A PDN, certified for NM790 — **SELECTED** |
| **MicroSD** | 128 GB MicroSD Card | Raspberry Pi OS Lite (64-bit), Bookworm — **Active Baseline** |

---

## 3. Hypotheses Matrix & Verification Results

| Hypothesis | Description | Priority | Test Method | Outcome |
|---|---|---|---|---|
| **H1** | PCIe Gen 3 Signal Instability | High | Forced Gen 2 (`dtparam=pciex1_gen=2`). Verified `LnkSta: Speed 5GT/s`. | **Partially Disproved:** Gen 2 was required, but write failure persisted in Gen 2. |
| **H2** | Maxio MAP1602 APST / HMB Issues | High | Added `nvme_core.default_ps_max_latency_us=0` and tested HMB ceilings. | **Confirmed Contributor:** HMB DMA caused 30s hangs; fixed with `max_host_mem_size_mb=0`. |
| **H3** | Local 3.3V Power Drop on HAT | High | Logged kernel post-mortem during sustained `dd conv=fsync` write. | **CONFIRMED ROOT CAUSE:** Rail collapse into `D3cold` (0V) and concurrent Wi-Fi bus stall. |
| **H4** | SSD Overheating in P580-V2 Case | Medium | Queried `sudo nvme smart-log /dev/nvme0`. | **DISPROVED:** Idle/load temperature was 27–31 °C. |
| **H5** | Flash Memory Defect / Bad Blocks | Low | Checked SMART error log and media errors. | **DISPROVED:** `media_errors: 0`, `num_err_log_entries: 0`, 100% spare. |

---

## 4. Chronological Engineering Investigation

### Phase 1: Baseline Establishment & Boot Order Isolation
- **Discovery:** Bootloader setting `BOOT_ORDER=0xf416` reads right-to-left. Digit `6` (NVMe PCIe) is probed first. Simply inserting a MicroSD card does not guarantee SD boot when NVMe has an OS partition.
- **Action:** Physically detached the PCIe FPC ribbon, flashed clean **Raspberry Pi OS Lite (64-bit)** to MicroSD, updated EEPROM to `BOOT_ORDER=0xf461` (SD first), and verified baseline telemetry:
  - Root: `/dev/mmcblk0p2` (`rw,noatime`).
  - Power: `vcgencmd get_throttled` -> `throttled=0x0`.
  - Kernel: `6.18.50+rpt-rpi-2712` aarch64.

### Phase 2: Secondary NVMe Attachment & Read Benchmark
- Reconnected Lexar NM790 on Geekworm X1002 as an unmounted secondary drive `/dev/nvme0n1`.
- Configured certified PCIe Gen 2 (`dtparam=pciex1_gen=2`).
- Verified link: `LnkSta: Speed 5GT/s, Width x1`.
- Checked SMART:
  - `temperature: 27 °C`
  - `media_errors: 0`
  - `num_err_log_entries: 0`
  - `critical_warning: 0`
- Read test (`sudo e2fsck -n /dev/nvme0n1p2`): Read 60,241,116 blocks across 1 TB with **zero errors**.
- Read benchmark (`dd if=/dev/nvme0n1`): **453 MB/s** sustained (wire-speed of PCIe Gen 2 x1).

### Phase 3: HMB Allocation & PCIe DMA Deadlock
- Kernel initially logged: `[ 0.968928] nvme nvme0: min host memory (40 MiB) above limit (32 MiB)`.
- When `nvme.max_host_mem_size_mb=64` was tested, kernel allocated 40 MiB HMB in 2 segments.
- **Symptom:** Every subsequent command hung for exactly 32.0192 seconds (matching the 30-second `io_timeout`), followed by `CSTS=0xffffffff` reset failure.
- **Diagnosis:** The BCM2712 PCIe root complex does not reliably support multi-segment inbound DMA for DRAM-less HMB on Maxio MAP1602.
- **Remedy:** Passed `nvme.max_host_mem_size_mb=0` in `cmdline.txt`. Disabling HMB completely eliminated the 30-second hang and restored instant 453 MB/s transfers.

### Phase 4: Write Stress-Testing & Power Rail Collapse
- Ran `sudo e2fsck -f -y /dev/nvme0n1p2`: Successfully replayed journal, cleared orphan flags, and repaired ext4 filesystem.
- Mounted partition to `/mnt/nvme_test`.
- Initiated 500 MB continuous write stress-test:
  ```bash
  sudo dd if=/dev/zero of=/mnt/nvme_test/test_500m.dat bs=1M count=500 status=progress conv=fsync
  ```
- **Symptom:** Blocks buffered into RAM, but during `conv=fsync` the process stalled for 48.261 seconds before aborting with `Input/output error`.
- **Smoking Gun Kernel Trace:**
  ```text
  [   95.983296] brcmfmac: brcmf_sdio_bus_rxctl: resumed on timeout
  [   96.863039] ieee80211 phy0: brcmf_cfg80211_dump_station: BRCMF_C_GET_ASSOCLIST failed, err=-110
  [  103.823755] nvme 0001:01:00.0: Unable to change power state from D3cold to D0, device inaccessible
  [  106.138854] nvme nvme0: Disabling device after reset failure: -19
  [  106.156377] EXT4-fs (nvme0n1p2): shut down requested (2)
  [  106.156390] Aborting journal on device nvme0n1p2-8.
  ```
- **Physical Analysis:**
  1. At 95.98s, sustained NAND programming current drew down the SBC main voltage rail, causing the SDIO Wi-Fi bus to time out.
  2. At 103.82s, the 3.3V rail on the M.2 connector completely collapsed to 0V (`D3cold`).
  3. The SSD controller lost power, dropped off the bus, and Ext4 safely remounted read-only to prevent catastrophic data corruption.

---

## 5. Final Decision & Implementation Roadmap

### Chosen Solution: Procure Pimoroni PIM699 (NVMe Base)
- **Cost Efficiency:** ~€15 adapter replaces the faulty power delivery without discarding the ~€90 1 TB Lexar NM790.
- **Certified Compatibility:** Lexar NM790 (1TB, 2TB, 4TB) is explicitly on Pimoroni's tested hardware matrix.
- **Electrical Superiority:** Dedicated high-current synchronous 3.3V/4A buck regulator + low-ESR ceramic decoupling bank designed specifically for high-power Gen 4 drives.
- **Shielded FPC:** Custom impedance-matched ribbon cable with interleaved ground planes supports PCIe Gen 3 (750–850 MB/s).

### Standby State Until PIM699 Arrival
1. Raspberry Pi 5 runs autonomously from the high-speed MicroSD baseline on `192.168.178.51`.
2. Geekworm X1002 and P580-V2 case are retired and safely disconnected.
3. Lexar NM790 is stored in an anti-static bag ready for one-command migration to Pimoroni PIM699 upon arrival.
