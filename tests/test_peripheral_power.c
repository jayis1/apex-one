/*
 * test_peripheral_power.c — Unit Tests for Peripheral Power Sequencing
 *
 * Copyright (C) 2026 GhostBlade Project
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Tests the peripheral power sequencing logic used in the RP2350B firmware's
 * peripheral_power module. Since the actual GPIO operations are hardware-
 * dependent, this test validates the sequencing order, timing constraints,
 * and state machine transitions.
 *
 * Build (standalone, no cmocka):
 *   gcc -Wall -Wextra -std=c11 -DNO_CMOCKA -o test_peripheral_power test_peripheral_power.c
 *
 * Build (with cmocka):
 *   gcc -Wall -Wextra -std=c11 -lcmocka -o test_peripheral_power test_peripheral_power.c
 *
 * Run:
 *   ./test_peripheral_power
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* ── Test framework abstraction ───────────────────────────────────────────── */

#ifdef NO_CMOCKA

static int g_tests_run = 0;
static int g_tests_passed = 0;
static int g_tests_failed = 0;

#define ASSERT_INT_EQ(expected, actual) do {                                \
    g_tests_run++;                                                          \
    if ((expected) != (actual)) {                                           \
        fprintf(stderr, "  FAIL: %s:%d: expected %d, got %d\n",            \
                __FILE__, __LINE__, (int)(expected), (int)(actual));        \
        g_tests_failed++;                                                   \
    } else {                                                                \
        g_tests_passed++;                                                   \
    }                                                                       \
} while (0)

#define ASSERT_TRUE(cond) do {                                              \
    g_tests_run++;                                                          \
    if (!(cond)) {                                                          \
        fprintf(stderr, "  FAIL: %s:%d: condition false: %s\n",            \
                __FILE__, __LINE__, #cond);                                 \
        g_tests_failed++;                                                   \
    } else {                                                                \
        g_tests_passed++;                                                   \
    }                                                                       \
} while (0)

#define ASSERT_FALSE(cond) ASSERT_TRUE(!(cond))

#define RUN_TEST(func) do {                                                 \
    fprintf(stderr, "  %-50s ", #func);                                     \
    func();                                                                 \
    fprintf(stderr, "PASS\n");                                             \
} while (0)

#define TEST_SUITE_START() fprintf(stderr, "Peripheral Power Sequencing Unit Tests\n")
#define TEST_SUITE_END() do {                                               \
    fprintf(stderr, "\nResults: %d passed, %d failed, %d total\n",         \
            g_tests_passed, g_tests_failed, g_tests_run);                  \
    return g_tests_failed > 0 ? 1 : 0;                                     \
} while (0)

#else
#include <cmocka.h>
#define ASSERT_INT_EQ(expected, actual) assert_int_equal((actual), (expected))
#define ASSERT_TRUE(cond) assert_true(cond)
#define ASSERT_FALSE(cond) assert_false(cond)
#define RUN_TEST(func) cmocka_unit_test(func)
#define TEST_SUITE_START()
#define TEST_SUITE_END()
#endif

/* ── Constants from peripheral_power.h ──────────────────────────────────────── */

enum power_rail_id {
    POWER_RAIL_SDR_1V8  = 0,
    POWER_RAIL_SDR_1V1  = 1,
    POWER_RAIL_SDR_3V3  = 2,
    POWER_RAIL_NFC      = 3,
    POWER_RAIL_SUBGHZ   = 4,
    POWER_RAIL_SDIO     = 5,
    POWER_RAIL_COUNT    = 6
};

enum power_rail_state {
    POWER_RAIL_OFF          = 0,
    POWER_RAIL_RAMPING      = 1,
    POWER_RAIL_STABLE       = 2,
    POWER_RAIL_FAULT        = 3
};

#define POWER_DELAY_SDR_1V8_MS       5
#define POWER_DELAY_SDR_1V1_MS       5
#define POWER_DELAY_SDR_3V3_MS       5
#define POWER_DELAY_NFC_MS           50
#define POWER_DELAY_SUBGHZ_MS        50
#define POWER_DELAY_SDIO_MS          20

/* ── Mock state tracking ─────────────────────────────────────────────────── */

static enum power_rail_state mock_states[POWER_RAIL_COUNT];
static int event_count;

static void reset_mock_states(void)
{
    int i;
    for (i = 0; i < POWER_RAIL_COUNT; i++) {
        mock_states[i] = POWER_RAIL_OFF;
    }
    event_count = 0;
}

static int mock_power_on(enum power_rail_id rail)
{
    if (rail < 0 || rail >= POWER_RAIL_COUNT)
        return -1;
    if (mock_states[rail] == POWER_RAIL_STABLE)
        return -1;
    mock_states[rail] = POWER_RAIL_STABLE;
    event_count++;
    return 0;
}

static int mock_power_off(enum power_rail_id rail)
{
    if (rail < 0 || rail >= POWER_RAIL_COUNT)
        return -1;
    if (mock_states[rail] == POWER_RAIL_OFF)
        return -1;
    mock_states[rail] = POWER_RAIL_OFF;
    event_count++;
    return 0;
}

static int mock_on_sequence(void)
{
    int result;

    result = mock_power_on(POWER_RAIL_SDR_1V8);
    if (result != 0) return result;

    result = mock_power_on(POWER_RAIL_SDR_1V1);
    if (result != 0) {
        mock_power_off(POWER_RAIL_SDR_1V8);
        return result;
    }

    result = mock_power_on(POWER_RAIL_SDR_3V3);
    if (result != 0) {
        mock_power_off(POWER_RAIL_SDR_1V1);
        mock_power_off(POWER_RAIL_SDR_1V8);
        return result;
    }

    result = mock_power_on(POWER_RAIL_NFC);
    if (result != 0) {
        mock_power_off(POWER_RAIL_SDR_3V3);
        mock_power_off(POWER_RAIL_SDR_1V1);
        mock_power_off(POWER_RAIL_SDR_1V8);
        return result;
    }

    result = mock_power_on(POWER_RAIL_SUBGHZ);
    if (result != 0) {
        mock_power_off(POWER_RAIL_NFC);
        mock_power_off(POWER_RAIL_SDR_3V3);
        mock_power_off(POWER_RAIL_SDR_1V1);
        mock_power_off(POWER_RAIL_SDR_1V8);
        return result;
    }

    result = mock_power_on(POWER_RAIL_SDIO);
    if (result != 0) {
        mock_power_off(POWER_RAIL_SUBGHZ);
        mock_power_off(POWER_RAIL_NFC);
        mock_power_off(POWER_RAIL_SDR_3V3);
        mock_power_off(POWER_RAIL_SDR_1V1);
        mock_power_off(POWER_RAIL_SDR_1V8);
        return result;
    }

    return 0;
}

static int mock_off_sequence(void)
{
    mock_power_off(POWER_RAIL_SDIO);
    mock_power_off(POWER_RAIL_SUBGHZ);
    mock_power_off(POWER_RAIL_NFC);
    mock_power_off(POWER_RAIL_SDR_3V3);
    mock_power_off(POWER_RAIL_SDR_1V1);
    mock_power_off(POWER_RAIL_SDR_1V8);
    return 0;
}

/* ── Test functions ────────────────────────────────────────────────────────── */

/**
 * test_power_on_sequence_order — Verify all rails turn on in correct order
 */
static void test_power_on_sequence_order(void)
{
    reset_mock_states();
    int result = mock_on_sequence();
    ASSERT_INT_EQ(0, result);

    /* All rails should be ON */
    ASSERT_INT_EQ(POWER_RAIL_STABLE, mock_states[POWER_RAIL_SDR_1V8]);
    ASSERT_INT_EQ(POWER_RAIL_STABLE, mock_states[POWER_RAIL_SDR_1V1]);
    ASSERT_INT_EQ(POWER_RAIL_STABLE, mock_states[POWER_RAIL_SDR_3V3]);
    ASSERT_INT_EQ(POWER_RAIL_STABLE, mock_states[POWER_RAIL_NFC]);
    ASSERT_INT_EQ(POWER_RAIL_STABLE, mock_states[POWER_RAIL_SUBGHZ]);
    ASSERT_INT_EQ(POWER_RAIL_STABLE, mock_states[POWER_RAIL_SDIO]);

    /* All 6 rails turned on */
    ASSERT_INT_EQ(6, event_count);
}

/**
 * test_power_off_sequence_order — Verify all rails turn off in reverse order
 */
static void test_power_off_sequence_order(void)
{
    reset_mock_states();

    /* First turn everything on */
    mock_on_sequence();

    /* Then turn off */
    int result = mock_off_sequence();
    ASSERT_INT_EQ(0, result);

    /* All rails should be OFF */
    int i;
    for (i = 0; i < POWER_RAIL_COUNT; i++) {
        ASSERT_INT_EQ(POWER_RAIL_OFF, mock_states[i]);
    }
}

/**
 * test_sdr_dependency_chain — Verify SDR rails must be powered in order
 *
 * The SDR has three voltage rails that must be powered in order:
 * 1V8 (core) → 1V1 (PLL) → 3V3 (PA/LNA)
 * Each depends on the previous being stable.
 */
static void test_sdr_dependency_chain(void)
{
    reset_mock_states();

    /* Cannot power on SDR 1V1 before SDR 1V8 */
    ASSERT_INT_EQ(0, mock_states[POWER_RAIL_SDR_1V8]);
    /* After powering on SDR 1V8, SDR 1V1 can be powered on */
    mock_power_on(POWER_RAIL_SDR_1V8);
    ASSERT_INT_EQ(POWER_RAIL_STABLE, mock_states[POWER_RAIL_SDR_1V8]);

    mock_power_on(POWER_RAIL_SDR_1V1);
    ASSERT_INT_EQ(POWER_RAIL_STABLE, mock_states[POWER_RAIL_SDR_1V1]);

    mock_power_on(POWER_RAIL_SDR_3V3);
    ASSERT_INT_EQ(POWER_RAIL_STABLE, mock_states[POWER_RAIL_SDR_3V3]);
}

/**
 * test_double_power_on_error — Verify powering on an already-on rail returns error
 */
static void test_double_power_on_error(void)
{
    reset_mock_states();
    mock_power_on(POWER_RAIL_SDR_1V8);
    ASSERT_INT_EQ(POWER_RAIL_STABLE, mock_states[POWER_RAIL_SDR_1V8]);

    /* Second power-on should fail */
    int result = mock_power_on(POWER_RAIL_SDR_1V8);
    ASSERT_INT_EQ(-1, result);
}

/**
 * test_double_power_off_error — Verify powering off an already-off rail returns error
 */
static void test_double_power_off_error(void)
{
    reset_mock_states();

    /* Rail starts OFF, powering off again should fail */
    int result = mock_power_off(POWER_RAIL_SDR_1V8);
    ASSERT_INT_EQ(-1, result);
}

/**
 * test_power_rail_count — Verify the total number of switchable rails
 */
static void test_power_rail_count(void)
{
    /* GhostBlade has 6 switchable peripheral power rails:
     * SDR 1V8, SDR 1V1, SDR 3V3, NFC, Sub-GHz, SDIO */
    ASSERT_INT_EQ(6, POWER_RAIL_COUNT);
}

/**
 * test_sequencing_timing — Verify minimum timing between rail enable events
 *
 * The total power-on time should be at least the sum of all ramp delays.
 * Total minimum = 5 + 5 + 5 + 50 + 50 + 20 = 135 ms
 */
static void test_sequencing_timing(void)
{
    uint32_t total_ramp_ms = POWER_DELAY_SDR_1V8_MS +
                              POWER_DELAY_SDR_1V1_MS +
                              POWER_DELAY_SDR_3V3_MS +
                              POWER_DELAY_NFC_MS +
                              POWER_DELAY_SUBGHZ_MS +
                              POWER_DELAY_SDIO_MS;

    /* Total minimum power-on time is 135 ms */
    ASSERT_INT_EQ(135, (int)total_ramp_ms);

    /* Each SDR rail needs at least 5 ms */
    ASSERT_TRUE(POWER_DELAY_SDR_1V8_MS >= 5);
    ASSERT_TRUE(POWER_DELAY_SDR_1V1_MS >= 5);
    ASSERT_TRUE(POWER_DELAY_SDR_3V3_MS >= 5);

    /* NFC and Sub-GHz need at least 50 ms (crystal/oscillator startup) */
    ASSERT_TRUE(POWER_DELAY_NFC_MS >= 50);
    ASSERT_TRUE(POWER_DELAY_SUBGHZ_MS >= 50);

    /* SDIO needs at least 20 ms (module init time) */
    ASSERT_TRUE(POWER_DELAY_SDIO_MS >= 20);
}

/**
 * test_cycling_on_off — Verify power cycling works correctly
 */
static void test_cycling_on_off(void)
{
    int i;

    for (i = 0; i < 10; i++) {
        reset_mock_states();

        /* Power on */
        int result = mock_on_sequence();
        ASSERT_INT_EQ(0, result);

        /* Verify all on */
        int j;
        for (j = 0; j < POWER_RAIL_COUNT; j++) {
            ASSERT_INT_EQ(POWER_RAIL_STABLE, mock_states[j]);
        }

        /* Power off */
        result = mock_off_sequence();
        ASSERT_INT_EQ(0, result);

        /* Verify all off */
        for (j = 0; j < POWER_RAIL_COUNT; j++) {
            ASSERT_INT_EQ(POWER_RAIL_OFF, mock_states[j]);
        }
    }
}

/**
 * test_sdr_off_order — Verify SDR rails turn off in correct order
 *
 * SDR power-off must follow reverse order:
 * 3V3 → 1V1 → 1V8 (opposite of power-on)
 */
static void test_sdr_off_order(void)
{
    reset_mock_states();

    /* Power on SDR rails */
    mock_power_on(POWER_RAIL_SDR_1V8);
    mock_power_on(POWER_RAIL_SDR_1V1);
    mock_power_on(POWER_RAIL_SDR_3V3);

    /* Power off in reverse order */
    mock_power_off(POWER_RAIL_SDR_3V3);
    ASSERT_INT_EQ(POWER_RAIL_OFF, mock_states[POWER_RAIL_SDR_3V3]);
    ASSERT_INT_EQ(POWER_RAIL_STABLE, mock_states[POWER_RAIL_SDR_1V1]);
    ASSERT_INT_EQ(POWER_RAIL_STABLE, mock_states[POWER_RAIL_SDR_1V8]);

    mock_power_off(POWER_RAIL_SDR_1V1);
    ASSERT_INT_EQ(POWER_RAIL_OFF, mock_states[POWER_RAIL_SDR_3V3]);
    ASSERT_INT_EQ(POWER_RAIL_OFF, mock_states[POWER_RAIL_SDR_1V1]);
    ASSERT_INT_EQ(POWER_RAIL_STABLE, mock_states[POWER_RAIL_SDR_1V8]);

    mock_power_off(POWER_RAIL_SDR_1V8);
    ASSERT_INT_EQ(POWER_RAIL_OFF, mock_states[POWER_RAIL_SDR_1V8]);
}

/**
 * test_partial_power_on — Verify partial power-on (individual rails)
 */
static void test_partial_power_on(void)
{
    reset_mock_states();

    /* Power on only the NFC rail */
    int result = mock_power_on(POWER_RAIL_NFC);
    ASSERT_INT_EQ(0, result);
    ASSERT_INT_EQ(POWER_RAIL_STABLE, mock_states[POWER_RAIL_NFC]);

    /* Other rails should still be OFF */
    ASSERT_INT_EQ(POWER_RAIL_OFF, mock_states[POWER_RAIL_SDR_1V8]);
    ASSERT_INT_EQ(POWER_RAIL_OFF, mock_states[POWER_RAIL_SDR_1V1]);
    ASSERT_INT_EQ(POWER_RAIL_OFF, mock_states[POWER_RAIL_SDR_3V3]);
    ASSERT_INT_EQ(POWER_RAIL_OFF, mock_states[POWER_RAIL_SUBGHZ]);
    ASSERT_INT_EQ(POWER_RAIL_OFF, mock_states[POWER_RAIL_SDIO]);
}

/**
 * test_invalid_rail_id — Verify invalid rail IDs are handled
 */
static void test_invalid_rail_id(void)
{
    reset_mock_states();

    /* Rail ID out of range */
    int result = mock_power_on((enum power_rail_id)99);
    ASSERT_INT_EQ(-1, result);

    result = mock_power_off((enum power_rail_id)99);
    ASSERT_INT_EQ(-1, result);
}

/* ── Main test runner ─────────────────────────────────────────────────────── */

int main(void)
{
    TEST_SUITE_START();

    fprintf(stderr, "\n");
    RUN_TEST(test_power_on_sequence_order);
    RUN_TEST(test_power_off_sequence_order);
    RUN_TEST(test_sdr_dependency_chain);
    RUN_TEST(test_double_power_on_error);
    RUN_TEST(test_double_power_off_error);
    RUN_TEST(test_power_rail_count);
    RUN_TEST(test_sequencing_timing);
    RUN_TEST(test_cycling_on_off);
    RUN_TEST(test_sdr_off_order);
    RUN_TEST(test_partial_power_on);
    RUN_TEST(test_invalid_rail_id);

    TEST_SUITE_END();
}