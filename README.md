# Apex One — Project Cyber-Swiss

**Advanced Mobile Pentesting Lab**  
Dual-processor (RK3576 + RP2350B) SDR-equipped handheld device with Wi-Fi 6E, sub-GHz radio, and NFC.

---

## Overview

Apex One is a pocket-sized penetration testing device that combines a powerful Linux SoC (Rockchip RK3576) with a real-time coprocessor (Raspberry Pi RP2350B) to provide:

- **Wideband SDR** — Lime Microsystems LMS7002M (100 kHz to 3.8 GHz, 2×2 MIMO)
- **Sub-GHz Radio** — TI CC1101 (OOK/FSK, 300–928 MHz)
- **NFC/RFID** — STMicroelectronics ST25R3916 (ISO 14443 A/B, 15693, FeliCa)
- **Wi-Fi 6E** — MediaTek MT7922 (monitor mode + packet injection)
- **AI Inference** — RK3576 NPU (6 TOPS INT8) for on-device signal classification
- **NVMe Storage** — M.2 2230 PCIe Gen3 ×2 for PCAP and rainbow table storage

The RP2350B manages all RF frontends (antenna switching, SDR tuning, NFC polling) while the RK3576 runs a full Linux distribution with pentesting tools.

---

## Repository Structure

```
apex-one/
├── docs/
│   ├── phase1-conceptual/
│   │   └── architecture-and-requirements.md    # Power targets, data flow, boot sequence
│   ├── phase2-schematics/
│   │   └── component-selection-and-schematics.md # Netlists, BOM, decoupling
│   ├── phase3-pcb/
│   │   └── pcb-blueprints-and-layout.md        # Stackup, impedance, thermal, DFM
│   └── phase4-software/
│       └── boot-process-and-mmio.md             # Boot stages, register definitions
├── hardware/
│   ├── schematics/                              # KiCad schematic files (TBD)
│   └── pcb/                                     # KiCad PCB layout files (TBD)
├── firmware/
│   └── rp2350b/
│       ├── include/
│       │   └── (shared headers)
│       └── src/
│           └── rp2350b_init.c                    # MCU init: clocks, GPIO, PIO, SPI
├── software/
│   ├── linux-drivers/
│   │   ├── include/
│   │   │   └── apex_bridge_regs.h              # MMIO register definitions
│   │   └── src/
│   │       └── apex_bridge.c                    # Kernel SPI driver (char dev)
│   ├── bootloader/                              # U-Boot SPL and board config
│   └── dts/
│       └── apex-one-rk3576.dts                  # Device tree source
├── tools/                                       # Utilities and scripts
├── Apex_One.mf                                  # System Manifest
└── README.md                                    # This file
```

---

## Engineering Phases

| Phase | Document | Description |
|-------|----------|-------------|
| 1 | [architecture-and-requirements.md](docs/phase1-conceptual/architecture-and-requirements.md) | Power budgets, thermal profiles, data flow, bus topology, security threat model |
| 2 | [component-selection-and-schematics.md](docs/phase2-schematics/component-selection-and-schematics.md) | BOM, netlists, decoupling networks, matching networks, power sequencing |
| 3 | [pcb-blueprints-and-layout.md](docs/phase3-pcb/pcb-blueprints-and-layout.md) | 6-layer stackup, impedance, fly-by routing, RF isolation, thermal vias, DFM |
| 4 | [boot-process-and-mmio.md](docs/phase4-software/boot-process-and-mmio.md) | Boot chain, register maps, protocol specification |

---

## Key Specifications

| Parameter | Value |
|-----------|-------|
| Primary SoC | Rockchip RK3576 (4× A72 + 4× A53, 6 TOPS NPU) |
| Coprocessor | RP2350B (2× Cortex-M33 / Hazard3 RISC-V @ 150 MHz) |
| RAM | 8 GB LPDDR5 @ 3200 MT/s |
| Storage | 32 GB eMMC 5.1 + M.2 2230 NVMe (PCIe Gen3 ×2) |
| SDR | LMS7002M (100 kHz – 3.8 GHz, 2×2 MIMO, 12-bit) |
| Sub-GHz | CC1101 (300–928 MHz, OOK/FSK/GFSK) |
| NFC | ST25R3916 (ISO 14443 A/B, 15693, FeliCa) |
| Wi-Fi/BT | MT7922 (Wi-Fi 6E 2×2, BT 5.4) |
| Display | 6.4" IPS 1080×2400 |
| Battery | 5000 mAh Li-Po (19.25 Wh) |
| Form Factor | 162 × 76 × 18 mm, ~320 g |
| PCB | 6-layer FR-4 (Isola 370HR), 1.6 mm |

---

## Inter-Processor Bridge Protocol

The RK3576 and RP2350B communicate over SPI0 at up to 50 MHz using a framed protocol:

```
┌────────┬─────┬──────┬──────────┬──────────┬─────────┬──────────┬────────┐
│ SYNC   │ CMD │ LEN  │ RESERVED │ HDR_CRC  │ PAYLOAD │ PAYLOAD  │ PADDING │
│ 0xAA   │ 1B  │ 2B   │ 4B       │ 8B (CRC64)│ 0-4092B │ CRC32    │         │
└────────┴─────┴──────┴──────────┴──────────┴─────────┴──────────┴────────┘
 Byte 0    1     2-3     4-7        8-15       16-n      n+1..n+4
```

See `apex_bridge_regs.h` for full opcode definitions.

---

## Getting Started

### Prerequisites

- Linux host with `arm-none-eabi-gcc` for RP2350B firmware
- `aarch64-linux-gnu-gcc` cross-compiler for RK3576 Linux
- KiCad 8+ for schematic/PCB editing
- Linux kernel 6.6+ headers for driver compilation

### Building the Linux Driver

```bash
cd software/linux-drivers
make -C /path/to/kernel/src M=$(pwd) modules
sudo insmod apex_bridge.ko
```

### Building RP2350B Firmware

```bash
cd firmware/rp2350b
mkdir build && cd build
cmake .. -DPICO_SDK_PATH=/path/to/pico-sdk
make -j$(nproc)
```

---

## License

- **Hardware designs** (schematics, PCB layouts, BOM): CERN-OHL-S v2
- **Firmware and software**: GPL-2.0-or-later
- **Documentation**: CC-BY-SA 4.0

---

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines. In short:

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/my-feature`)
3. Commit your changes with clear descriptions
4. Push to your fork and open a Pull Request

---

*Apex One — Project Cyber-Swiss. Designed for those who build, test, and secure.*