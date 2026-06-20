/*
 * sleep_wake.h — Sleep/Wake State Machine for RP2350B
 *
 * Copyright (C) 2026 GhostBlade Project
 * SPDX-License-Identifier: MIT
 *
 * Manages low-power sleep states and wake-up transitions for the
 * RP2350B coprocessor. The state machine handles:
 *
 *   - SLEEP_IDLE:    Normal operation, all peripherals active
 *   - SLEEP_LIGHT:   CPU idle, peripherals clock-gated, SPI0 stays active
 *   - SLEEP_DEEP:    Core 1 halted, most peripherals in reset, SPI0 active
 *   - SLEEP_OFF:     All peripherals off, waiting for host SPI command
 *
 * Wake sources:
 *   - SPI0 slave activity (host sends a command)
 *   - GPIO interrupt (INT_REQ from host)
 *   - Watchdog bark (if enabled)
 *   - ADC low-battery interrupt
 */

#ifndef SLEEP_WAKE_H
#define SLEEP_WAKE_H

#include <stdint.h>
#include <stdbool.h>

/* ── Sleep state definitions ───────────────────────────────────────────── */

/**
 * enum sleep_state — Low-power state machine states
 *
 * State transitions:
 *   IDLE → LIGHT:   No SPI activity for SLEEP_IDLE_TIMEOUT_MS
 *   LIGHT → DEEP:   No SPI activity for SLEEP_LIGHT_TIMEOUT_MS
 *   DEEP → OFF:     Not currently implemented (would require host cooperation)
 *   Any → IDLE:     SPI0 activity detected (RX FIFO not empty)
 */
enum sleep_state {
    SLEEP_IDLE = 0,    /**< Full operation, all clocks running */
    SLEEP_LIGHT,       /**< CPU sleep, peripheral clocks gated */
    SLEEP_DEEP,        /**< Core 1 halted, minimal power draw */
    SLEEP_STATE_COUNT  /**< Number of valid sleep states */
};

/** Timeout thresholds in milliseconds */
#define SLEEP_IDLE_TIMEOUT_MS    5000   /* 5s idle before light sleep */
#define SLEEP_LIGHT_TIMEOUT_MS   30000  /* 30s before deep sleep */

/** Minimum SPI0 RX idle time (ms) to consider entering sleep */
#define SLEEP_MIN_IDLE_MS        100

/* ── Public API ─────────────────────────────────────────────────────────── */

/**
 * sleep_wake_init — Initialize the sleep/wake state machine
 *
 * Resets state to SLEEP_IDLE and clears all timers.
 * Must be called after rp2350b_init().
 */
void sleep_wake_init(void);

/**
 * sleep_wake_process — Process the state machine (call from main loop)
 *
 * Checks for SPI activity and advances/retreats the sleep state.
 * Should be called every main loop iteration.
 *
 * Returns: Current sleep state after processing
 */
enum sleep_state sleep_wake_process(void);

/**
 * sleep_wake_get_state — Get the current sleep state
 *
 * Returns: Current sleep state
 */
enum sleep_state sleep_wake_get_state(void);

/**
 * sleep_wake_force_wake — Force transition to SLEEP_IDLE
 *
 * Called when SPI activity is detected or when the host
 * asserts INT_REQ. Immediately transitions to full operation.
 */
void sleep_wake_force_wake(void);

/**
 * sleep_wake_get_idle_ms — Get time since last SPI activity
 *
 * Returns: Milliseconds since last SPI RX activity
 */
uint32_t sleep_wake_get_idle_ms(void);

#endif /* SLEEP_WAKE_H */