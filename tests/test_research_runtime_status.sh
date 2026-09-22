#!/usr/bin/env bash
set -euo pipefail

SRC="d30_hwmon.c"

fail()
{
    echo "FAIL: $1"
    exit 1
}

echo "[1] research5 runtime-status block exists"
grep -Fq '{ "runtime_01", 0x01, 0x68, 0x80 }' "$SRC" \
    || fail "page 0x01 regs 0x68-0x80 runtime block missing"

echo "[2] runtime probe appears exactly once"
COUNT="$(grep -Fc '{ "runtime_01", 0x01, 0x68, 0x80 }' "$SRC")"
[ "$COUNT" -eq 1 ] \
    || fail "unexpected runtime_01 definition count: $COUNT"

echo "[3] research5 version"
grep -Fq 'MODULE_VERSION("1.5-research5")' "$SRC" \
    || fail "research5 version missing"

echo "[4] research readout remains read-only"
grep -q 'DEVICE_ATTR_RO(research_candidates)' "$SRC" \
    || fail "research_candidates is no longer read-only"

echo "[5] no EC DATA-port writes"
if grep -nE 'out[bwl]\s*\([^,]+,\s*D30_DATA\s*\)' "$SRC"; then
    fail "write to D30_DATA detected"
fi

echo "PASS"
