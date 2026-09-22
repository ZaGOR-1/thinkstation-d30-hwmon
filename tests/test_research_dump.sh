#!/usr/bin/env bash
set -euo pipefail

SRC="d30_hwmon.c"

echo "[1] research_dump parameter exists"
grep -q 'research_dump' "$SRC"

echo "[2] Fan1 research block exists"
grep -q '0x00.*0x17' "$SRC"

echo "[3] Fan3 research block exists"
grep -q '0x30.*0x47' "$SRC"

echo "[4] Fan7 research block exists"
grep -q '0x90.*0xa7\|0x90.*0xA7' "$SRC"

echo "[5] no DATA-port writes"
if grep -nE 'out[bwl]\s*\([^,]+,\s*D30_DATA\s*\)' "$SRC"; then
    echo "ERROR: write to D30_DATA detected"
    exit 1
fi

echo "PASS"
