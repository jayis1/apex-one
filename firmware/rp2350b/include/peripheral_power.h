/*
 * peripheral_power.h — Peripheral Power Sequencing Module
 *
 * Copyright (C) 2026 GhostBlade Project
 * SPDX-License-Identifier: MIT
 *
 * Manages the GPIO-controlled power rails for the GhostBlade peripherals.
 * The RK817 PMIC provides the primary power rails, but several peripheral
 * supplies are switched via GPIO to allow the RP2350B to power them on/off
 * in the correct sequence.
 *
 * Power-on sequence (from docs/power-tree.md):
 *   1. VCC_3V3 stable (PMIC) — all downstream rails depend on this
 *   2. VCC_SDIO (Wi-Fi) — 5ms after VCC_3V3
 *   3. VCC_SDR 1V8 — 5ms after VCC_3V3
 *   4. VCC_SDR 1V1 — 5ms after VCC_SDR 1V8
 *   5. VCC_SDR 3V3 — 5ms after VCC_SDR 1V1
 *   6. VCC_3V3_RP (RP2350B I/O) — 35ms after VCC_3V3
 *   7. VCC_NFC — 50ms after MCU_RESET deassert
 *   8. VCC_SUBGHZ — 50ms after MCU_RESET deassert
 *
 * Power-off sequence (reverse order):
 *   8. VCC_SUBGHZ off
 *   7. VCC_NFC off
 *   6. VCC_3V3_RP off
 *   5. VCC_SDR 3V3 off
 *   4. VCC_SDR 1V1 off
 *   3. VCC_SDR 1V8 off
 *   2. VCC_SDIO off
 *   1. VCC_3V3 off (PMIC shutdown)
 *
 * Note: VCC_3V3 and VCC_3V3_RP are always powered when the system is
 * running. The RP2350B only controls VCC_SDR, VCC_NFC, VCC_SUBGHZ,
 * and VCC_SDIO enable GPIOs.
 */

#ifndef PERIPHERAL_POWER_H
#define PERIPHERAL_POWER_H

#include <stdint.h>
#include <stdbool.h>

/* ── Power rail IDs ──────────────────────────────────────────────────────── */

/**
 * enum power_rail_id — Identifiers for switchable power rails
 *
 * Each rail corresponds to a GPIO-controlled enable pin on the RP2350B.
 * The rails are listed in power-on order.
 */
enum power_rail_id {
    POWER_RAIL_SDR_1V8  = 0,  /* LMS7002M core voltage */
    POWER_RAIL_SDR_1V1  = 1,  /* LMS7002M PLL voltage */
    POWER_RAIL_SDR_3V3  = 2,  /* LMS7002M PA/LNA voltage */
    POWER_RAIL_NFC      = 3,  /* ST25R3916 voltage */
    POWER_RAIL_SUBGHZ   = 4,  /* CC1101 voltage */
    POWER_RAIL_SDIO     = 5,  /* MT7922 Wi-Fi SDIO voltage */
    POWER_RAIL_COUNT    = 6   /* Total number of switchable rails */
};

/* ── Power rail state ───────────────────────────────────────────────────── */

/**
 * enum power_rail_state — State of a power rail
 */
enum power_rail_state {
    POWER_RAIL_OFF          = 0,  /* Rail is off (GPIO low) */
    POWER_RAIL_RAMPING      = 1,  /* Rail is ramping up (between on and stable) */
    POWER_RAIL_STABLE       = 2,  /* Rail is on and voltage is stable */
    POWER_RAIL_FAULT        = 3,  /* Rail fault detected (overcurrent, undervoltage) */
};

/* ── Power sequencing delays (milliseconds) ─────────────────────────────── */

/**
 * These delays match the power-tree.md timing requirements.
 * Each delay is the minimum time between enabling this rail and
 * enabling the next rail in the sequence.
 */
#define POWER_DELAY_SDR_1V8_MS       5    /* SDR 1V8 ramp time */
#define POWER_DELAY_SDR_1V1_MS       5    /* SDR 1V1 PLL lock time */
#define POWER_DELAY_SDR_3V3_MS       5    /* SDR 3V3 PA settling */
#define POWER_DELAY_NFC_MS           50   /* NFC oscillator startup */
#define POWER_DELAY_SUBGHZ_MS        50   /* CC1101 crystal startup */
#define POWER_DELAY_SDIO_MS          20   /* SDIO module init */

/* ── Overcurrent detection thresholds ─────────────────────────────────────── */

/**
 * Each rail has an approximate current limit. If the ADC or GPIO
 * current sense indicates an overcurrent condition, the rail is
 * immediately shut down and a fault is recorded.
 */
#define POWER_LIMIT_SDR_1V8_MA       500  /* LMS7002M core */
#define POWER_LIMIT_SDR_1V1_MA       300  /* LMS7002M PLL */
#define POWER_LIMIT_SDR_3V3_MA       200   /* LMS7002M PA/LNA */
#define POWER_LIMIT_NFC_MA           150   /* ST25R3916 */
#define POWER_LIMIT_SUBGHZ_MA         50    /* CC1101 */
#define POWER_LIMIT_SDIO_MA          500    /* MT7922 SDIO */

/* ── Power rail configuration ─────────────────────────────────────────────── */

/**
 * struct power_rail_config — Configuration for a switchable power rail
 *
 * @gpio_pin:    RP2350B GPIO pin number for enable control
 * @active_high: True if GPIO high enables the rail, false if low
 * @ramp_ms:     Minimum time (ms) for rail to reach stable voltage
 * @current_ma:  Maximum expected current draw (mA)
 * @name:        Human-readable rail name
 */
struct power_rail_config {
    uint8_t  gpio_pin;
    bool     active_high;
    uint16_t ramp_ms;
    uint16_t current_ma;
    const char *name;
};

/* ── Power event callback ────────────────────────────────────────────────── */

/**
 * typedef power_event_cb — Callback for power rail state changes
 *
 * @rail:    The rail that changed state
 * @state:   The new state of the rail
 * @context: User-provided context pointer
 */
typedef void (*power_event_cb)(enum power_rail_id rail,
                                enum power_rail_state state,
                                void *context);

/* ── Public API ─────────────────────────────────────────────────────────── */

/**
 * peripheral_power_init — Initialize GPIO pins for power rail control
 *
 * Configures all power rail enable GPIOs as outputs, initially set to
 * the OFF state (GPIO low for active-high rails, GPIO high for
 * active-low rails).
 *
 * This must be called before any other peripheral_power_* functions.
 * Must be called after rp2350b_init() (which sets up GPIO clocks).
 *
 * Returns: 0 on success, negative on error
 */
int peripheral_power_init(void);

/**
 * peripheral_power_on — Power on a specific rail
 *
 * Enables the GPIO for the specified rail and waits for the voltage
 * to stabilize (ramp_ms delay). After this function returns, the
 * rail is guaranteed to be at a stable voltage.
 *
 * @rail: The power rail to enable
 *
 * Returns: 0 on success, -1 if the rail is already on or invalid
 */
int peripheral_power_on(enum power_rail_id rail);

/**
 * peripheral_power_off — Power off a specific rail
 *
 * Disables the GPIO for the specified rail. This function returns
 * immediately — the rail voltage decays naturally through the load.
 *
 * @rail: The power rail to disable
 *
 * Returns: 0 on success, -1 if the rail is already off or invalid
 */
int peripheral_power_off(enum power_rail_id rail);

/**
 * peripheral_power_on_sequence — Power on all peripherals in sequence
 *
 * Enables all power rails in the correct order with proper delays
 * between each rail. This matches the power-tree.md power-on sequence.
 *
 * The sequence is:
 *   1. SDR 1V8 on, wait 5ms
 *   2. SDR 1V1 on, wait 5ms
 *   3. SDR 3V3 on, wait 5ms
 *   4. NFC on, wait 50ms
 *   5. Sub-GHz on, wait 50ms
 *   6. SDIO on, wait 20ms
 *
 * Total power-on time: approximately 135ms
 *
 * Returns: 0 on success, negative on error (fault detected)
 */
int peripheral_power_on_sequence(void);

/**
 * peripheral_power_off_sequence — Power off all peripherals in reverse order
 *
 * Disables all power rails in reverse order with minimum delays.
 * This matches the power-tree.md power-off sequence.
 *
 * The sequence is:
 *   1. SDIO off, wait 5ms
 *   2. Sub-GHz off, wait 5ms
 *   3. NFC off, wait 5ms
 *   4. SDR 3V3 off, wait 5ms
 *   5. SDR 1V1 off, wait 5ms
 *   6. SDR 1V8 off
 *
 * Total power-off time: approximately 25ms
 *
 * Returns: 0 on success, negative on error
 */
int peripheral_power_off_sequence(void);

/**
 * peripheral_power_get_state — Get the current state of a power rail
 *
 * @rail: The power rail to query
 *
 * Returns: Current state of the rail, or POWER_RAIL_FAULT if invalid
 */
enum power_rail_state peripheral_power_get_state(enum power_rail_id rail);

/**
 * peripheral_power_set_event_callback — Register a callback for rail state changes
 *
 * The callback is invoked whenever a rail transitions to a new state.
 * This is useful for logging, diagnostics, or triggering dependent
 * initialization (e.g., starting SPI1 after SDR 1V8 is stable).
 *
 * @cb:      Callback function pointer (NULL to unregister)
 * @context: User context passed to the callback
 */
void peripheral_power_set_event_callback(power_event_cb cb, void *context);

/**
 * peripheral_power_all_on — Check if all rails are powered on and stable
 *
 * Returns: true if all rails are in POWER_RAIL_STABLE state
 */
bool peripheral_power_all_on(void);

/**
 * peripheral_power_all_off — Check if all rails are powered off
 *
 * Returns: true if all rails are in POWER_RAIL_OFF state
 */
bool peripheral_power_all_off(void);

/**
 * peripheral_power_get_config — Get the configuration for a power rail
 *
 * Returns a pointer to the static configuration for the specified rail.
 * Returns NULL if the rail ID is invalid.
 *
 * @rail: The power rail to query
 */
const struct power_rail_config *peripheral_power_get_config(enum power_rail_id rail);

#endif /* PERIPHERAL_POWER_H */