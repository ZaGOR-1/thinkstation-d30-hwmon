# Lenovo ThinkStation D30 hwmon driver

Read-only Linux hwmon driver for the Lenovo ThinkStation D30
using the onboard Nuvoton NCT6681 monitoring controller.

## Tested system

- Lenovo ThinkStation D30
- Machine type: 4353
- Nuvoton NCT6681
- Proxmox VE / Linux
- DKMS support

## Sensors

The driver exposes:

- 7 active temperature channels
- 3 active fan RPM channels
- voltage monitoring
- 3VCC
- 3VSB

The driver is read-only and does not control fan PWM, fan curves,
temperature limits, or other NCT6681 settings.

## Safety

The driver checks DMI before accessing the NCT6681 hardware.

Expected system:

- Vendor: LENOVO
- Product name: 4353
- Product version: ThinkStation D30

If the DMI information does not match, the driver refuses to access
the NCT6681 I/O ports.

## Installation

Run as root:

    chmod +x install.sh
    ./install.sh

Then check:

    sensors d30_hwmon-virtual-0

## Uninstall

Run as root:

    ./uninstall.sh

## Current sensor labels

Some motherboard sensor labels are still candidates and have not yet
been physically mapped with certainty.

Current readings include:

- Ambient / Intake candidate
- Board temperature sensors
- PCH candidate
- CPU board sensor
- Memory area candidate
- Fan 1
- CPU-related Fan 3
- Fan 7
- VIN / EC_VIN voltage channels
- 3VCC
- 3VSB

## DKMS

The module is installed through DKMS so it can be rebuilt when the
Linux/Proxmox kernel is updated, provided matching kernel headers are
installed.

## Hardware access

The driver uses the Lenovo ThinkStation D30 NCT6681 monitoring
interface at I/O ports 0xA00-0xA02.

Sensor access is intentionally read-only.
