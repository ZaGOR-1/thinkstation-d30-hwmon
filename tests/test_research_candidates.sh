#!/usr/bin/env bash
set -euo pipefail

SRC="d30_hwmon.c"

fail()
{
    echo "FAIL: $1"
    exit 1
}

echo "[1] live research readout exists"
grep -q 'research_candidates_show' "$SRC" \
    || fail "research_candidates_show is missing"

echo "[2] upstream candidate PWM-current block exists"
grep -Fq '{ "pwm_current", 0x01, 0x60, 0x67 }' "$SRC" \
    || fail "page 0x01 regs 0x60-0x67 are missing"

echo "[3] upstream candidate fan-output config block exists"
grep -Fq '{ "fanout_cfg", 0x01, 0xd0, 0xd7 }' "$SRC" \
    || fail "page 0x01 regs 0xd0-0xd7 are missing"

echo "[4] upstream candidate PWM-write/shadow block exists"
grep -Fq '{ "pwm_write", 0x0a, 0x28, 0x2f }' "$SRC" \
    || fail "page 0x0a regs 0x28-0x2f are missing"

echo "[5] candidate fan config control register exists"
grep -Fq '{ "fan_cfg_ctrl", 0x0a, 0x01 }' "$SRC" \
    || fail "page 0x0a reg 0x01 is missing"

echo "[6] research_candidates sysfs attribute is read-only"
grep -q 'DEVICE_ATTR_RO(research_candidates)' "$SRC" \
    || fail "read-only research_candidates attribute is missing"

echo "[7] research attribute is gated behind research_dump"
grep -q 'dev_attr_research_candidates' "$SRC" \
    || fail "research_candidates visibility hook is missing"

echo "[8] no EC DATA-port writes"
if grep -nE 'out[bwl]\s*\([^,]+,\s*D30_DATA\s*\)' "$SRC"; then
    fail "write to D30_DATA detected"
fi

echo "PASS"
