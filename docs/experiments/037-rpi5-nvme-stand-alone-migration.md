# Experiment 037 — Raspberry Pi 5 NVMe Migration and Standalone Setup

**Date**: 2026-10-11  
**Status**: IN PROGRESS (Checkpoint saved, awaiting fresh OS flash on MicroSD)  
**Target Hardware**: Raspberry Pi 5 (BCM2712, 8GB RAM, Active Cooler, M.2 NVMe HAT via PCIe FPC ribbon, FRITZ!Box 6690 YQ)

---

## 1. Objective
Establish a clean, high-performance Linux environment on the Raspberry Pi 5, enable PCIe Gen 3 M.2 NVMe storage boot, clone the operating system from MicroSD to SSD, and deploy `tools/ai_brain_server.py` as an autonomous `systemd` background service.

---

## 2. Progress & Hardware Discoveries
1. **Initial MicroSD Analysis**:
   - Inspected existing SD card (`Generic- SD/MMC/MS PRO`, 128 GB).
   - Found legacy Bookworm installation with user `andr`, hostname `rpi-15`, and Wi-Fi preconfigured for `FRITZ!Box 6690 YQ`.
   - Identified missing PCIe enablement: Raspberry Pi 5 disables the external PCIe port by default unless explicitly configured in `config.txt`.
2. **Network Liveness Test**:
   - Booted Pi 5 on Wi-Fi: host responded at `rpi-15.local` (`192.168.178.51`) with 10 ms latency.
   - Verified OpenSSH port 22 reachable.
3. **Authentication & Fresh OS Decision**:
   - Cloud-init user credentials from initial setup were not cached locally.
   - User decided to write a fresh, clean **Raspberry Pi OS Lite (64-bit)** image to remove bloat, save RAM for Whisper/Python models, and ensure clean credentials.
4. **Tooling Prepared**:
   - Downloaded official installer `Raspberry_Pi_Imager_Setup.exe` to `C:\Users\AndrD\Downloads\`.
   - Prepared `tools/setup_pi_ssh.py` for automated post-boot authentication and key installation.
   - Generated ED25519 SSH keypair on host (`~/.ssh/id_ed25519`).

---

## 3. Resume Plan (Immediate Next Steps)
When returning to this milestone:
1. **Flashing**: User runs `Raspberry_Pi_Imager_Setup.exe` and flashes **Raspberry Pi OS Lite (64-bit)** with:
   - Hostname: `rpi-15`
   - User: `andr` / Password: `123` (or user choice)
   - Wi-Fi: `FRITZ!Box 6690 YQ`
   - SSH: Enabled (with host's `id_ed25519.pub` key)
2. **PCIe Activation**: Before ejecting card from PC, inject into `/boot/firmware/config.txt`:
   ```ini
   [all]
   dtparam=nvme
   dtparam=pciex1
   dtparam=pciex1_gen=3
   ```
3. **Boot & SSH**: Power on Pi 5, verify connection at `192.168.178.51` / `rpi-15.local`.
4. **NVMe Verification & OS Cloning**:
   - Verify `/dev/nvme0n1` via `lsblk` and `lspci`.
   - Update bootloader EEPROM via `sudo rpi-eeprom-config --edit` (`BOOT_ORDER=0xf416`).
   - Clone root filesystem to NVMe drive (`rpi-clone nvme0n1`).
5. **Brain Server Deployment**:
   - Install `python3-venv`, `ffmpeg`.
   - Clone repository and install `requirements-brain.txt`.
   - Copy `tools/brain_config.json`.
   - Configure `/etc/systemd/system/celestial-brain.service` for 24/7 background operation.
6. **Staff Firmware Update**:
   - In `firmware/src/wifi_credentials.h`, update `BRAIN_SERVER_IP` to the Pi 5's IP.
