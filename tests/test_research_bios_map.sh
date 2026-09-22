#!/usr/bin/env bash
set -euo pipefail

SRC="d30_hwmon.c"

fail()
{
    echo "FAIL: $1"
    exit 1
}

echo "[1] BIOS selector block"
grep -Fq '{ "bios_sel", 0x01, 0xc0, 0xc7 }' "$SRC" \
    || fail "page 0x01 regs 0xc0-0xc7 missing"

echo "[2] BIOS page06 08-0a"
grep -Fq '{ "bios_06_08", 0x06, 0x08, 0x0a }' "$SRC" \
    || fail "page 0x06 regs 0x08-0x0a missing"

echo "[3] BIOS page06 0c-0f"
grep -Fq '{ "bios_06_0c", 0x06, 0x0c, 0x0f }' "$SRC" \
    || fail "page 0x06 regs 0x0c-0x0f missing"

echo "[4] BIOS page06 38-3a"
grep -Fq '{ "bios_06_38", 0x06, 0x38, 0x3a }' "$SRC" \
    || fail "page 0x06 regs 0x38-0x3a missing"

echo "[5] BIOS page06 3c-3f"
grep -Fq '{ "bios_06_3c", 0x06, 0x3c, 0x3f }' "$SRC" \
    || fail "page 0x06 regs 0x3c-0x3f missing"

echo "[6] BIOS page06 60-77"
grep -Fq '{ "bios_06_60", 0x06, 0x60, 0x77 }' "$SRC" \
    || fail "page 0x06 regs 0x60-0x77 missing"

echo "[7] BIOS scalar registers"
grep -Fq '{ "bios_f9", 0x01, 0xf9 }' "$SRC" || fail "bios_f9 missing"
grep -Fq '{ "bios_5a", 0x01, 0x5a }' "$SRC" || fail "bios_5a missing"
grep -Fq '{ "bios_5b", 0x01, 0x5b }' "$SRC" || fail "bios_5b missing"
grep -Fq '{ "bios_5c", 0x01, 0x5c }' "$SRC" || fail "bios_5c missing"
grep -Fq '{ "bios_f8", 0x01, 0xf8 }' "$SRC" || fail "bios_f8 missing"
grep -Fq '{ "bios_status", 0x06, 0x00 }' "$SRC" || fail "bios_status missing"

echo "[8] research3 version"
grep -Fq 'MODULE_VERSION("1.5-research3")' "$SRC" \
    || fail "research3 version missing"

echo "[9] no DATA writes"
if grep -nE 'out[bwl]\s*\([^,]+,\s*D30_DATA\s*\)' "$SRC"; then
    fail "write to D30_DATA detected"
fi

echo "PASS"
