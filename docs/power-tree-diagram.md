# GhostBlade Power Tree Diagram
<!-- SPDX-License-Identifier: CC-BY-SA-4.0 -->
<!-- Copyright (C) 2026 GhostBlade Project -->

This document describes the complete power distribution architecture for the
GhostBlade (Project NullSpectre) board, including rail dependencies, sequencing
requirements, and current budgets.

## 1. Power Tree Overview

```
                                    ┌──────────────┐
                                    │  VBAT        │
                                    │  Li-Po 3.7V  │
                                    │  (3.0–4.2V)  │
                                    └──────┬───────┘
                                           │
                          ┌────────────────┼────────────────┐
                          │                │                │
                    ┌─────┴─────┐    ┌─────┴─────┐   ┌────┴────┐
                    │  PMIC      │    │ VBAT_SENSE │   │ USB-C   │
                    │  RK817     │    │ R1=100kΩ   │   │ 5V IN   │
                    │            │    │ R2=33kΩ    │   │ (PD 3.0)│
                    └─────┬──────┘    │ → ADC0     │   └────┬────┘
                          │           └────────────┘        │
              ┌───────────┼───────────────────┐            │
              │           │                   │            │
        ┌─────┴─────┐ ┌───┴───┐        ┌─────┴─────┐ ┌───┴────┐
        │ VDD_CORE  │ │VDD_3V3│        │ VDD_5V    │ │VDD_5V  │
        │ 0.9V/2A   │ │3.3V/3A│        │ USB/2A    │ │NFC_TX  │
        │ (RK3576)  │ │       │        │           │ │5V/1A   │
        └───────────┘ └───┬───┘        └───────────┘ └────────┘
                          │
          ┌───────────────┼───────────────┬──────────────────┐
          │               │               │                  │
    ┌─────┴─────┐  ┌─────┴─────┐  ┌──────┴──────┐  ┌───────┴───────┐
    │ VDD_1V8    │  │ VDD_LOGIC  │  │ VDD_RF      │  │ VDD_1V2_SDR  │
    │ 1.8V/500mA │  │ 1.8V/1A    │  │ 3.3V/500mA  │  │ 1.2V/1A      │
    │ (RP2350B   │  │ (RK3576    │  │ (LMS7002M   │  │ (LMS7002M    │
    │  DVDD,IO)  │  │  DDR,VPU)  │  │  LNA,VCO)   │  │  DVDD)       │
    └─────┬──────┘  └───────────┘  └─────────────┘  └──────────────┘
          │
    ┌─────┴──────┐
    │ VDD_1V8_SDR │
    │ 1.8V/500mA  │
    │ (LMS7002M   │
    │  AVDD)      │
    └────────────┘

    ┌──────────────────────────────────────────────────────────────┐
    │                    Peripheral Power Rails                     │
    ├──────────────┬──────────┬──────────┬────────────────────────┤
    │ Rail         │ Voltage  │ Current  │ Controlled By          │
    ├──────────────┼──────────┼──────────┼────────────────────────┤
    │ SDR 1V8      │ 1.8V     │ 500 mA   │ RP2350B GPIO28         │
    │ SDR 1V1      │ 1.1V     │ 1 A      │ RP2350B GPIO29         │
    │ SDR 3V3      │ 3.3V     │ 500 mA   │ RP2350B GPIO30         │
    │ NFC 5V TX    │ 5.0V     │ 1 A      │ ST25R3916 TX supply    │
    │ Sub-GHz 3V3  │ 3.3V     │ 100 mA   │ RP2350B GPIO31 (CC1101)│
    │ SDIO 3V3     │ 3.3V     │ 800 mA   │ RK3576 SDIO controller │
    └──────────────┴──────────┴──────────┴────────────────────────┘
```

## 2. Power Rail Specifications

### 2.1 Primary Rails

| Rail        | Source       | Voltage  | Max Current | Enable Control       | Sequencing |
|-------------|-------------|----------|-------------|---------------------|------------|
| VBAT        | Li-Po cell  | 3.0–4.2V | 5 A (peak)  | N/A (always on)     | —          |
| VDD_CORE    | PMIC (RK817) | 0.9V    | 2 A         | PMIC PWRON           | 1st        |
| VDD_1V8     | PMIC (RK817) | 1.8V    | 500 mA      | PMIC PWRON           | 2nd        |
| VDD_LOGIC   | PMIC (RK817) | 1.8V    | 1 A         | PMIC PWRON           | 2nd        |
| VDD_3V3     | PMIC (RK817) | 3.3V    | 3 A         | PMIC PWRON           | 3rd        |
| VDD_5V      | USB-C / Boost | 5.0V   | 2 A         | USB-C PD negotiation | —          |
| VDD_5V_NFC  | Boost conv.  | 5.0V    | 1 A         | ST25R3916 TX driver  | 4th        |

### 2.2 LMS7002M SDR Rails

The LMS7002M requires a specific power-on sequence to prevent latch-up:

| Rail        | Voltage | Max Current | Purpose            | Enable Pin    | Delay After Previous |
|-------------|---------|-------------|--------------------|---------------|---------------------|
| VDD_1V8_SDR | 1.8V    | 500 mA     | AVDD (analog core) | RP2350B GPIO28 | 0 ms (1st)         |
| VDD_1V2_SDR | 1.2V    | 1 A        | DVDD (digital core)| RP2350B GPIO29 | ≥ 5 ms after 1V8   |
| VDD_RF      | 3.3V    | 500 mA     | LNA, VCO, PA       | RP2350B GPIO30 | ≥ 5 ms after 1V2   |
| VDD_3V3     | 3.3V    | —           | IO supply (shared) | PMIC          | ≥ 5 ms after 1V2   |

**Power-off sequence** must follow reverse order: RF → 1V2 → 1V8.

### 2.3 Sub-GHz Radio (CC1101) Rail

| Rail          | Voltage | Max Current | Purpose      | Enable Pin    | Delay |
|---------------|---------|-------------|--------------|---------------|-------|
| Sub-GHz 3V3   | 3.3V    | 100 mA     | CC1101 VDD   | RP2350B GPIO31 | ≥ 50 ms after SDR rails |

### 2.4 NFC (ST25R3916) Rail

| Rail        | Voltage | Max Current | Purpose        | Enable Control        | Delay |
|-------------|---------|-------------|----------------|-----------------------|-------|
| VDD_5V_NFC  | 5.0V    | 1 A         | TX driver      | ST25R3916 internal    | ≥ 50 ms after SDR rails |
| VDD_3V3     | 3.3V    | 50 mA       | ST25R3916 VDD | Shared with VDD_3V3   | —     |

### 2.5 Wi-Fi (MT7922) Rail

| Rail        | Voltage | Max Current | Purpose      | Enable               | Delay |
|-------------|---------|-------------|--------------|----------------------|-------|
| VDD_3V3     | 3.3V    | 800 mA     | MT7922 VDDIO | SDIO controller PM   | ≥ 20 ms after 3V3 stable |

## 3. Battery Monitoring

### 3.1 Voltage Divider Network

```
    VBAT (3.0–4.2V) ───[R1=100kΩ]──┬──[R2=33kΩ]── GND
                                     │
                                  ADC0 (GPIO26)
                                  RP2350B
```

- Full-scale ADC voltage at VBAT=4.2V: V_ADC = 4.2 × 33/133 = 1.043V
- Full-scale ADC voltage at VBAT=3.0V: V_ADC = 3.0 × 33/133 = 0.744V
- ADC reference: 3.3V (VDD_3V3)
- ADC resolution: 12-bit (0–4095)
- mV per LSB: ~3.249 mV (see battery_monitor.c for fixed-point math)

### 3.2 Battery Thresholds

| Threshold      | Voltage  | Action                                      |
|---------------|----------|---------------------------------------------|
| Full charge    | 4.2V     | Normal operation                            |
| Nominal        | 3.7V     | 50% estimated capacity                      |
| Low battery    | 3.3V     | Warning flag set, reduce TX power           |
| Critical       | 3.0V     | Controlled shutdown, watchdog scratch magic   |
| Brownout       | 2.8V     | Hardware brownout, peripherals disabled      |

## 4. Power Sequencing Timing

### 4.1 Cold Boot Sequence

```
  Time (ms)   Event
  ─────────   ────────────────────────────────────────────────────
  0           PMIC PWRON asserted
  5           VDD_CORE (0.9V) stable
  10          VDD_1V8 / VDD_LOGIC (1.8V) stable
  20          VDD_3V3 (3.3V) stable
  25          RP2350B RSTn released
  30          RP2350B GPIO28 → HIGH (VDD_1V8_SDR enable)
  35          RP2350B GPIO29 → HIGH (VDD_1V2_SDR enable)
  40          RP2350B GPIO30 → HIGH (VDD_RF enable)
  50          VDD_5V_NFC enable
  100         CC1101 SRES (chip reset strobe)
  110         CC1101 configuration registers written
  120         ST25R3916 SET_DEFAULT command
  130         ST25R3916 oscillator stabilization complete
  140         LMS7002M initialization via SPI1
  150         RP2350B MCU_READY GPIO asserted
  200         RK3576 Linux kernel boots, apex_bridge driver loads
  300+        System fully operational
```

### 4.2 Warm Boot (MCU Reset Only)

```
  Time (μs)   Event
  ─────────   ────────────────────────────────────────────────────
  0           MCU_RESET (GPIO1_B2) asserted LOW by RK3576
  10          RP2350B enters reset
  100         MCU_RESET deasserted (HIGH)
  300         RP2350B crystal oscillator starts
  500         RP2350B boot ROM executes
  2000        RP2350B firmware init begins
  5000        CC1101 re-initialized
  10000       ST25R3916 re-initialized
  15000       LMS7002M re-initialized
  20000       MCU_READY asserted
```

### 4.3 Power-Down Sequence

```
  Time (ms)   Event
  ─────────   ────────────────────────────────────────────────────
  0           Shutdown command received (SPI or watchdog)
  5           LMS7002M TX disabled
  10          CC1101 SIDLE strobe (enter idle mode)
  20          CC1101 SPWD strobe (enter sleep mode)
  25          ST25R3916 field off, OP_CTRL disable
  30          ST25R3916 GOTO_SLEEP command
  50          VDD_5V_NFC disable
  55          RP2350B GPIO30 → LOW (VDD_RF disable)
  60          RP2350B GPIO29 → LOW (VDD_1V2_SDR disable)
  65          RP2350B GPIO28 → LOW (VDD_1V8_SDR disable)
  70          MCU_READY deasserted
  75          Watchdog scratch magic written
  80          Watchdog reboot triggered (if intentional)
  500         PMIC PWRON deasserted (full power-off)
```

## 5. Current Budget

| Component        | Typical (mA) | Peak (mA) | Rail          |
|-----------------|-------------|-----------|---------------|
| RK3576 (active)  | 800         | 2500      | VDD_CORE + VDD_LOGIC |
| RP2350B (active) | 30         | 80        | VDD_3V3       |
| RP2350B (sleep)  | 2          | 5         | VDD_3V3       |
| LMS7002M (RX)    | 200        | 350       | VDD_1V8_SDR + VDD_1V2_SDR + VDD_RF |
| LMS7002M (TX)    | 350        | 600       | same           |
| CC1101 (RX)      | 15         | 20        | Sub-GHz 3V3   |
| CC1101 (TX +10dBm)| 30       | 45        | Sub-GHz 3V3   |
| ST25R3916 (poll) | 50         | 100       | VDD_5V_NFC + VDD_3V3 |
| ST25R3916 (TX)   | 200        | 500       | VDD_5V_NFC    |
| MT7922 (RX)      | 150        | 300       | VDD_3V3       |
| MT7922 (TX)      | 300        | 600       | VDD_3V3       |
| Display (backlight)| 50       | 100       | VDD_3V3       |
| **Total (typical)**| **~1600** | **~4600** |                |
| **Battery (3.7V 3000mAh)**|     |           | ~1.9h (active), ~50h (idle) |

## 6. ESD Protection

All external connectors have ESD protection:

| Connector       | ESD Device     | Rail Protected | Clamping Voltage |
|----------------|---------------|----------------|-----------------|
| SMA_ANT0 (MIMO_TX) | TPD4E05U06 | VDD_RF         | 5.5V            |
| SMA_ANT1 (MIMO_RX) | TPD4E05U06 | VDD_RF         | 5.5V            |
| u.FL (Sub-GHz) | TPD4E05U06    | Sub-GHz 3V3    | 5.5V            |
| NFC Antenna     | TPD4E05U06    | VDD_5V_NFC     | 5.5V            |
| USB-C           | TPD4E05U06    | VDD_5V         | 5.5V            |
| MicroSD slot    | TPD4E05U06    | VDD_3V3        | 5.5V            |
| HDMI (display)  | TPD4E05U06    | VDD_3V3        | 5.5V            |

ESD diodes are placed as close to the connector as possible with minimal
trace stub length. All protected lines route through the ESD device before
reaching the SoC pins.

## 7. Test Points

| Test Point | Net          | Purpose                        |
|-----------|-------------|--------------------------------|
| TP1       | VBAT        | Battery voltage measurement     |
| TP2       | VDD_3V3     | 3.3V rail verification         |
| TP3       | VDD_1V8     | 1.8V rail verification          |
| TP4       | VDD_CORE    | 0.9V core rail verification    |
| TP5       | VDD_1V8_SDR | SDR 1.8V rail verification     |
| TP6       | VDD_1V2_SDR | SDR 1.2V rail verification     |
| TP7       | VDD_RF      | SDR RF rail verification        |
| TP8       | VDD_5V_NFC  | NFC TX supply verification      |
| TP9       | GND         | Ground reference                |
| TP10      | SPI0_SCK    | SPI bridge clock probe         |
| TP11      | SPI0_MOSI   | SPI bridge data out probe       |
| TP12      | SPI0_MISO   | SPI bridge data in probe        |
| TP13      | INT_REQ     | MCU interrupt request probe    |
| TP14      | MCU_RESET   | MCU reset probe                 |
| TP15      | MCU_READY   | MCU ready signal probe          |