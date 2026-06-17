# Changelog

All notable changes to the GhostBlade (Project NullSpectre) project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

Hardware revisions follow CERN-OHL-S v2 version numbering. Firmware and software follow GPL-2.0-or-later versioning.

---

## [Unreleased]

### Added

- Top-level `Makefile` for convenient project-wide builds (firmware, driver, libapex, tests, DTS)
- DTS Makefile (`software/dts/Makefile`) for compiling and validating device tree sources
- Unit tests for battery monitor, CC1101 configuration, watchdog timer, and power state machine
- HIL (hardware-in-the-loop) SPI bridge test script (`tests/hil_spi_bridge_test.sh`)
- `stats.json` updated with current line counts and file counts

### Changed

- `.gitignore` updated to include all test binary targets (test_battery_monitor, test_cc1101_config, test_watchdog, test_power_states)
- `.gitignore` updated to include firmware build outputs (*.uf2, *.hex, *.bin, *.elf, *.map)
- `tools/generate_gerbers.py` — moved SPDX-License-Identifier into file header, consolidated license declaration
- `software/libapex/setup.py` — added copyright and SPDX license header

---

## [0.1.0] — 2026-06-14

### Added

- RK3576 + RP2350B dual-processor hardware design (6-layer FR-4, IPC Class 3)
- LMS7002M SDR (100 kHz – 3.8 GHz, 2×2 MIMO)
- CC1101 sub-GHz radio (300–928 MHz, OOK/FSK/GFSK)
- ST25R3916 NFC controller (ISO 14443 A/B, 15693, FeliCa)
- MT7922 Wi-Fi 6E / BT 5.4
- RP2350B firmware with SPI bridge protocol, SDR DMA, CC1101 init, ST25R3916 init, battery monitor, watchdog
- Linux kernel SPI bridge driver (apex_bridge) with sysfs telemetry attributes
- libapex userspace C library and Python bindings
- KiCad 8 hardware design files (schematics, PCB, symbols, footprints, netlist, DRC rules)
- BOM (80+ components, interactive HTML)
- Device tree sources for RK3576 (base, options overlay, SDR overlay)
- Comprehensive documentation (getting started, build instructions, flashing guide, FAQ, power tree, SPI protocol timing, sysfs attributes, hardware test procedures, hardware contributor guide)
- Engineering phase documents (architecture/requirements, component selection/schematics, PCB layout, boot process/MMIO)
- Gerber generation script with fabrication notes
- Unit tests for SPI protocol (158 tests)
- `.clang-format`, `.editorconfig`, `.markdownlint.json`, `.codespell.ignore`

[Unreleased]: https://github.com/jayis1/ghostblade/compare/v0.1.0...HEAD
[0.1.0]: https://github.com/jayis1/ghostblade/releases/tag/v0.1.0