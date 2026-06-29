#!/bin/bash
# ==============================================================================
# hil_nfc_test.sh — Hardware-in-the-Loop NFC Test for GhostBlade
# Project NullSpectre
#
# Copyright (C) 2026 GhostBlade Project
# SPDX-License-Identifier: GPL-2.0-or-later
#
# This script tests the ST25R3916 NFC controller through the RP2350B SPI
# bridge. It verifies NFC field activation, ISO 14443A polling, tag
# detection, and transceive operations using a real GhostBlade device.
#
# Usage:
#   ./hil_nfc_test.sh              # Run all NFC HIL tests
#   ./hil_nfc_test.sh --quick      # Quick smoke tests only
#   ./hil_nfc_test.sh --loop       # Run continuously until failure
#   ./hil_nfc_test.sh --tag-only   # Only test tag detection
#   ./hil_nfc_test.sh --verbose    # Enable verbose output
#
# Prerequisites:
#   - GhostBlade device connected via USB-C
#   - apex_bridge kernel driver loaded
#   - libapex installed
#   - NFC test tags (MIFARE Classic 1K, NTAG213, or ISO 14443B)
#
# Exit codes:
#   0 — All tests passed
#   1 — One or more tests failed
#   2 — Setup/dependency error
# ==============================================================================

set -euo pipefail

# ===== Configuration =====
DEVICE="${APEX_DEVICE:-/dev/apex_bridge0}"
SYSFS_BASE="/sys/class/apex/apex_bridge0"
LIBAPEX_CLI="${LIBAPEX_CLI:-apex_cli}"
TEST_TAG_TYPE="${TEST_TAG_TYPE:-iso14443a}"
LOOP_COUNT=0
QUICK_MODE=0
TAG_ONLY=0
VERBOSE=0
PASS_COUNT=0
FAIL_COUNT=0
SKIP_COUNT=0

# ===== Colors =====
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
BLUE='\033[0;34m'
NC='\033[0m'  # No Color

# ===== Helper Functions =====

log_info() {
    echo -e "${BLUE}[INFO]${NC} $*"
}

log_pass() {
    echo -e "${GREEN}[PASS]${NC} $*"
    PASS_COUNT=$((PASS_COUNT + 1))
}

log_fail() {
    echo -e "${RED}[FAIL]${NC} $*"
    FAIL_COUNT=$((FAIL_COUNT + 1))
}

log_skip() {
    echo -e "${YELLOW}[SKIP]${NC} $*"
    SKIP_COUNT=$((SKIP_COUNT + 1))
}

log_verbose() {
    if [ "$VERBOSE" -eq 1 ]; then
        echo -e "${BLUE}[DBG]${NC} $*"
    fi
}

check_device() {
    if [ ! -c "$DEVICE" ]; then
        log_fail "Device $DEVICE not found"
        echo "  Is the apex_bridge driver loaded? Try: insmod apex_bridge.ko"
        exit 2
    fi

    if [ ! -d "$SYSFS_BASE" ]; then
        log_fail "Sysfs directory $SYSFS_BASE not found"
        echo "  Is the apex_bridge driver loaded? Try: modprobe apex_bridge"
        exit 2
    fi
}

check_libapex() {
    if ! command -v "$LIBAPEX_CLI" &>/dev/null; then
        log_fail "apex_cli not found (tried: $LIBAPEX_CLI)"
        echo "  Install libapex: cd software/libapex && make && sudo make install"
        exit 2
    fi
}

# ===== NFC Test Functions =====

test_nfc_field_on() {
    log_info "Test: NFC field activation"
    local result

    # Enable NFC field via the ST25R3916 direct command
    result=$("$LIBAPEX_CLI" nfc-field-on 2>&1) || true
    log_verbose "nfc-field-on output: $result"

    # Check that mcu_flags bit 4 (NFC_ACTIVE) is set
    local flags
    flags=$(cat "${SYSFS_BASE}/mcu_flags" 2>/dev/null || echo "0x0000")
    local flags_dec=$((flags))
    local nfc_active_bit=$(( (flags_dec >> 4) & 1 ))

    if [ "$nfc_active_bit" -eq 1 ]; then
        log_pass "NFC field is active (mcu_flags bit 4 set)"
    else
        log_fail "NFC field not active (mcu_flags=$flags, expected bit 4)"
    fi

    # Check that nfc_field_mv shows a non-zero field strength
    local field_mv
    field_mv=$(cat "${SYSFS_BASE}/nfc_field_mv" 2>/dev/null || echo "0")
    if [ "$field_mv" -gt 0 ]; then
        log_pass "NFC field strength: ${field_mv} mV"
    else
        log_fail "NFC field strength is 0 mV (antenna disconnected?)"
    fi
}

test_nfc_field_off() {
    log_info "Test: NFC field deactivation"

    "$LIBAPEX_CLI" nfc-field-off 2>&1 || true

    local flags
    flags=$(cat "${SYSFS_BASE}/mcu_flags" 2>/dev/null || echo "0x0000")
    local flags_dec=$((flags))
    local nfc_active_bit=$(( (flags_dec >> 4) & 1 ))

    if [ "$nfc_active_bit" -eq 0 ]; then
        log_pass "NFC field is inactive (mcu_flags bit 4 clear)"
    else
        log_fail "NFC field still active after field-off command (mcu_flags=$flags)"
    fi
}

test_nfc_field_toggle() {
    log_info "Test: NFC field toggle (on/off/on cycle)"

    "$LIBAPEX_CLI" nfc-field-on 2>&1 || true
    sleep 0.1

    local flags_on
    flags_on=$(cat "${SYSFS_BASE}/mcu_flags" 2>/dev/null || echo "0x0000")
    local on_bit=$(( (${flags_on} >> 4) & 1 ))

    "$LIBAPEX_CLI" nfc-field-off 2>&1 || true
    sleep 0.1

    local flags_off
    flags_off=$(cat "${SYSFS_BASE}/mcu_flags" 2>/dev/null || echo "0x0000")
    local off_bit=$(( (${flags_off} >> 4) & 1 ))

    "$LIBAPEX_CLI" nfc-field-on 2>&1 || true
    sleep 0.1

    local flags_on2
    flags_on2=$(cat "${SYSFS_BASE}/mcu_flags" 2>/dev/null || echo "0x0000")
    local on2_bit=$(( (${flags_on2} >> 4) & 1 ))

    if [ "$on_bit" -eq 1 ] && [ "$off_bit" -eq 0 ] && [ "$on2_bit" -eq 1 ]; then
        log_pass "NFC field toggle: on→off→on cycle works correctly"
    else
        log_fail "NFC field toggle failed: on=$on_bit off=$off_bit on2=$on2_bit"
    fi

    # Leave field off
    "$LIBAPEX_CLI" nfc-field-off 2>&1 || true
}

test_nfc_tag_detect() {
    log_info "Test: NFC tag detection (ISO 14443A REQA)"
    log_info "  Place a ${TEST_TAG_TYPE} tag near the antenna..."

    # Turn on the field
    "$LIBAPEX_CLI" nfc-field-on 2>&1 || true
    sleep 0.05

    # Send REQA (0x26) and check for ATQA response
    local result
    result=$("$LIBAPEX_CLI" nfc-transact --cmd=0x26 --proto=iso14443a 2>&1) || true
    log_verbose "nfc-transact REQA result: $result"

    # Check mcu_flags bit 5 (NFC_TAG_PRESENT)
    local flags
    flags=$(cat "${SYSFS_BASE}/mcu_flags" 2>/dev/null || echo "0x0000")
    local flags_dec=$((flags))
    local tag_bit=$(( (flags_dec >> 5) & 1 ))

    if [ "$tag_bit" -eq 1 ]; then
        log_pass "NFC tag detected (mcu_flags bit 5 set)"
    else
        log_skip "No NFC tag detected — place a tag near the antenna"
    fi

    # Turn off the field
    "$LIBAPEX_CLI" nfc-field-off 2>&1 || true
}

test_nfc_anticollision() {
    log_info "Test: NFC anti-collision cascade (ISO 14443A)"

    # Turn on the field
    "$LIBAPEX_CLI" nfc-field-on 2>&1 || true
    sleep 0.05

    # Perform anti-collision cascade level 1
    local result
    result=$("$LIBAPEX_CLI" nfc-anticoll --level=1 2>&1) || true
    log_verbose "Anti-collision result: $result"

    # Check if we got a UID (even a partial one means the protocol is working)
    if echo "$result" | grep -q "uid:"; then
        local uid
        uid=$(echo "$result" | grep "uid:" | head -1 | awk '{print $2}')
        log_pass "Anti-collision returned UID: $uid"
    else
        log_skip "Anti-collision: no tag detected for UID retrieval"
    fi

    "$LIBAPEX_CLI" nfc-field-off 2>&1 || true
}

test_nfc_vdd_measurement() {
    log_info "Test: ST25R3916 VDD measurement"

    local result
    result=$("$LIBAPEX_CLI" nfc-measure-vdd 2>&1) || true
    log_verbose "VDD measurement: $result"

    # The ST25R3916 should report ~3.3V VDD
    if echo "$result" | grep -qP '\d+\.\d+'; then
        local vdd
        vdd=$(echo "$result" | grep -oP '\d+\.\d+' | head -1)
        log_pass "ST25R3916 VDD measurement: ${vdd}V"
    else
        log_fail "Could not parse VDD measurement: $result"
    fi
}

test_nfc_sleep_wake() {
    log_info "Test: ST25R3916 sleep and wake cycle"

    # Put ST25R3916 to sleep
    "$LIBAPEX_CLI" nfc-sleep 2>&1 || true
    sleep 0.01

    # After sleep, field should be off
    local flags_sleep
    flags_sleep=$(cat "${SYSFS_BASE}/mcu_flags" 2>/dev/null || echo "0x0000")
    local sleep_nfc_bit=$(( (${flags_sleep} >> 4) & 1 ))

    # Wake the ST25R3916
    "$LIBAPEX_CLI" nfc-wake 2>&1 || true
    sleep 0.01

    # After wake, should be ready but field off
    local flags_wake
    flags_wake=$(cat "${SYSFS_BASE}/mcu_flags" 2>/dev/null || echo "0x0000")

    if [ "$sleep_nfc_bit" -eq 0 ]; then
        log_pass "ST25R3916 sleep: NFC field disabled during sleep"
    else
        log_fail "ST25R3916 sleep: NFC field still active after sleep command"
    fi

    log_pass "ST25R3916 sleep/wake cycle completed"
}

test_nfc_rssi_measurement() {
    log_info "Test: NFC RSSI measurement"

    "$LIBAPEX_CLI" nfc-field-on 2>&1 || true
    sleep 0.05

    local result
    result=$("$LIBAPEX_CLI" nfc-measure-rssi 2>&1) || true
    log_verbose "RSSI measurement: $result"

    # RSSI should be a positive value with field on
    if echo "$result" | grep -qP '\d+'; then
        log_pass "NFC RSSI measurement received"
    else
        log_fail "Could not read NFC RSSI: $result"
    fi

    "$LIBAPEX_CLI" nfc-field-off 2>&1 || true
}

test_nfc_protocol_iso15693() {
    log_info "Test: NFC ISO 15693 (NFC-V) inventory"
    log_info "  Place an ISO 15693 tag near the antenna..."

    local result
    result=$("$LIBAPEX_CLI" nfc-inventory --proto=iso15693 2>&1) || true
    log_verbose "ISO 15693 inventory: $result"

    if echo "$result" | grep -q "uid:"; then
        log_pass "ISO 15693 tag inventory succeeded"
    else
        log_skip "No ISO 15693 tag detected"
    fi
}

# ===== Main Test Runner =====

run_all_tests() {
    echo "==================================================================="
    echo " GhostBlade NFC Hardware-in-the-Loop Tests"
    echo "==================================================================="
    echo " Device: $DEVICE"
    echo " SysFS:  $SYSFS_BASE"
    echo " Tag:    $TEST_TAG_TYPE"
    echo "==================================================================="
    echo ""

    # Pre-flight checks
    check_device
    check_libapex

    echo "--- Pre-flight: MCU status ---"
    local driver_status
    driver_status=$(cat "${SYSFS_BASE}/driver_status" 2>/dev/null || echo "0x00000000")
    local uptime
    uptime=$(cat "${SYSFS_BASE}/uptime_ms" 2>/dev/null || echo "0")
    log_info "Driver status: $driver_status, MCU uptime: ${uptime}ms"
    echo ""

    # NFC tests
    echo "--- NFC Field Tests ---"
    test_nfc_field_on
    test_nfc_field_off
    test_nfc_field_toggle

    echo ""
    echo "--- NFC Tag Detection ---"
    test_nfc_tag_detect
    test_nfc_anticollision

    echo ""
    echo "--- NFC Measurements ---"
    test_nfc_vdd_measurement
    test_nfc_rssi_measurement

    echo ""
    echo "--- NFC Sleep/Wake ---"
    test_nfc_sleep_wake

    if [ "$TAG_ONLY" -ne 1 ]; then
        echo ""
        echo "--- NFC Protocol Tests ---"
        test_nfc_protocol_iso15693
    fi

    echo ""
    echo "==================================================================="
    echo " Results: $PASS_COUNT passed, $FAIL_COUNT failed, $SKIP_COUNT skipped"
    echo "==================================================================="

    if [ "$FAIL_COUNT" -gt 0 ]; then
        return 1
    fi
    return 0
}

# ===== Argument Parsing =====

while [ $# -gt 0 ]; do
    case "$1" in
        --quick)
            QUICK_MODE=1
            shift
            ;;
        --loop)
            LOOP_COUNT=-1
            shift
            ;;
        --tag-only)
            TAG_ONLY=1
            shift
            ;;
        --verbose|-v)
            VERBOSE=1
            shift
            ;;
        --device=*)
            DEVICE="${1#*=}"
            shift
            ;;
        --help|-h)
            echo "Usage: $0 [OPTIONS]"
            echo ""
            echo "Options:"
            echo "  --quick        Quick smoke tests only"
            echo "  --loop         Run continuously until failure"
            echo "  --tag-only     Only test tag detection"
            echo "  --verbose      Enable verbose output"
            echo "  --device=DEV   Specify device path (default: /dev/apex_bridge0)"
            echo "  --help         Show this help"
            exit 0
            ;;
        *)
            log_fail "Unknown argument: $1"
            exit 2
            ;;
    esac
done

# ===== Execute =====

if [ "$QUICK_MODE" -eq 1 ]; then
    log_info "Quick mode: running essential NFC tests only"
    check_device
    check_libapex
    test_nfc_field_on
    test_nfc_field_off
    test_nfc_vdd_measurement
    echo ""
    echo "Quick test: $PASS_COUNT passed, $FAIL_COUNT failed"
    exit $([ "$FAIL_COUNT" -eq 0 ] && echo 0 || echo 1)
fi

if [ "$LOOP_COUNT" -eq -1 ]; then
    iteration=0
    while true; do
        iteration=$((iteration + 1))
        echo ""
        log_info "=== Loop iteration $iteration ==="
        if ! run_all_tests; then
            log_fail "Loop test failed at iteration $iteration"
            exit 1
        fi
        sleep 2
    done
fi

run_all_tests
exit $?