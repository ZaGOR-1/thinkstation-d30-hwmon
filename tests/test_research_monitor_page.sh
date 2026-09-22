#!/usr/bin/env bash
set -euo pipefail

SRC="d30_hwmon.c"

fail()
{
    echo "FAIL: $1"
    exit 1
}

echo "[1] research4 monitor-page block exists"
grep -Fq '{ "monitor_01", 0x01, 0x10, 0x5f }' "$SRC" \
    || fail "page 0x01 regs 0x10-0x5f monitor block missing"

echo "[2] scan stays bounded to page 0x01"
COUNT="$(grep -Fc '{ "monitor_01", 0x01, 0x10, 0x5f }' "$SRC")"
[ "$COUNT" -eq 1 ] \
    || fail "unexpected monitor_01 definition count: $COUNT"

echo "[3] research readout remains read-only"
grep -q 'DEVICE_ATTR_RO(research_candidates)' "$SRC" \
    || fail "research_candidates is no longer read-only"

echo "[4] no EC DATA-port writes"
if grep -nE 'out[bwl]\s*\([^,]+,\s*D30_DATA\s*\)' "$SRC"; then
    fail "write to D30_DATA detected"
fi

echo "PASS"
