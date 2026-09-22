# ThinkStation D30 NCT6681 fan-control register research

## Status

Hardware: Lenovo ThinkStation D30 type 4353
BIOS: A3KT70AUS
Super I/O / EC: Nuvoton NCT6681, device ID 0xB271
EC base: 0x0A00

Access interface:

- 0xA00: PAGE
- 0xA01: INDEX
- 0xA02: DATA

Stable driver: d30_hwmon v1.5
Research branch: fan-control-research

No DATA-register writes have been performed during research.

## Firmware findings

Lenovo A3KT70A contains a PEI firmware module named:

fancontrolpei
GUID: 7f21bc95-01a7-42e9-a3e1-820e57ac52c1

Static firmware analysis shows direct access to the NCT6681 through
0xA00/0xA01/0xA02 and configuration of fan-control tables.

Runtime platform values observed:

- Setup[0x83] = 0x00
- Setup[0x84] = 0x00
- I/O port 0x548 = 0xE1
- low nibble of 0x548 = 0x01

The firmware selection logic therefore follows the Table A branch.

## Hardware-verified page 0x07 fan blocks

Each discovered fan block is 0x18 bytes.

### Fan1 — page 0x07, registers 0x00-0x17

Raw:

32 3a 42 48 4b 51 54 00
0f 00 16 00 1e 00 28 00
3d 00 70 00 99 00 00 00

Candidate temperature points:

50, 58, 66, 72, 75, 81, 84

Associated little-endian 16-bit control values:

15, 22, 30, 40, 61, 112, 153

### Fan3 — page 0x07, registers 0x30-0x47

Raw:

45 4a 4e 52 56 5b 60 00
00 00 07 00 14 00 28 00
3f 00 63 00 93 00 00 00

Candidate temperature points:

69, 74, 78, 82, 86, 91, 96

Associated little-endian 16-bit control values:

0, 7, 20, 40, 63, 99, 147

### Fan7 — page 0x07, registers 0x90-0xA7

Raw:

19 1d 1f 22 23 25 28 00
00 00 1e 00 42 00 5e 00
75 00 8c 00 aa 00 00 00

Candidate temperature points:

25, 29, 31, 34, 35, 37, 40

Associated little-endian 16-bit control values:

0, 30, 66, 94, 117, 140, 170

## Hardware-verified configuration/status registers

Page 0x01:

- 0x5A = 0xEF
- 0x5B = 0xF1
- 0x5C = 0xED
- 0xF8 = 0x00

Page 0x06:

- 0x00 = 0x60

The 0xEF/0xF1/0xED sequence matches the configuration values recovered
from Lenovo fancontrolpei.

The exact semantics of page 0x01 register 0xF8 and page 0x06 register
0x00 are not yet considered proven.

## Confirmed

- Lenovo firmware programs fan-control data into NCT6681.
- Page 0x07 contains per-fan control blocks.
- Fan1 block base is 0x00.
- Fan3 block base is 0x30.
- Fan7 block base is 0x90.
- The contents read from real hardware match the structures recovered
  from Lenovo firmware.
- Research driver remains read-only with respect to DATA port 0xA02.

## Not yet confirmed

- Exact physical meaning/scaling of the 16-bit control values.
- Temperature-source selection for Fan1, Fan3 and Fan7.
- Direct/manual PWM or target-output registers.
- Exact automatic/manual mode mechanism.
- Exact commit/request/status semantics.
- Safe procedure for runtime writable fan control.

## Next steps

1. Correlate fan RPM with all exposed temperature channels under a
   controlled thermal load.
2. Continue static firmware analysis for source selectors and direct
   fan-output registers.
3. Locate and verify automatic/manual mode handling.
4. Do not perform EC DATA writes until the manual-control path and
   automatic recovery path are both understood.
