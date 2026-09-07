# Lenovo ThinkStation D30 hwmon driver

Read-only Linux hwmon driver for the Lenovo ThinkStation D30 using the
onboard Nuvoton NCT6681 monitoring controller. Version 1.5 uses register
addresses and conversion factors recovered from Lenovo BIOS A3KT70A.

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
- chassis-intrusion alarm
- diagnostic raw low bytes for active temperature channels

The driver is read-only and does not control fan PWM, fan curves,
temperature limits, or other NCT6681 settings.

## Safety

The driver checks DMI, the NCT6681 Super-I/O ID, logical-device enable state,
and the configured EC base address before accessing the hardware.

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

The installer safely replaces versions 1.1 through 1.4 if present.

Then check:

    sensors d30_hwmon-virtual-0

Confirm the loaded release and BIOS-verified hardware detection:

    modinfo d30_hwmon | grep '^version:'
    journalctl -k -b --no-pager | grep d30_hwmon

The diagnostic low bytes and chassis alarm are available directly in sysfs:

    HWMON="$(dirname "$(grep -l '^d30_hwmon$' /sys/class/hwmon/hwmon*/name)")"
    grep -H . "$HWMON"/temp*_raw_low "$HWMON"/intrusion0_alarm

## Uninstall

Run as root:

    ./uninstall.sh

## BIOS-verified sensor labels

Lenovo BIOS identifies temperature and fan channels only by number. The
driver keeps neutral names except where a physical relationship has been
confirmed experimentally; fan headers remain neutral.

Current readings include:

- System temperature1 through System temperature4
- CPU Package (EC mirror), confirmed by controlled-load correlation
- System temperature7 and System temperature8
- Fan1 Speed
- Fan3 Speed
- Fan7 Speed
- VIN / EC_VIN voltage channels
- 3VCC
- 3VSB

The BIOS supports ten temperature channels and eight fan channels. Only the
channels observed as active on the tested ThinkStation D30 are exposed.

## Temperature encoding and raw-low diagnostics

The BIOS displays only the high byte of each temperature channel as whole
degrees Celsius. Version 1.5 reads the full signed 16-bit NCT668x value and
uses the Linux-kernel conversion `(raw16 / 128) * 500` millidegrees Celsius,
preserving the observed 0.5-degree resolution. The low byte remains available
unchanged as `tempN_raw_low` for diagnostics.

## Chassis intrusion

Lenovo BIOS reads the latched chassis-intrusion state from I/O port `0x466`,
bit 0. The driver exports that state as `intrusion0_alarm` and never writes to
the port, so only BIOS can acknowledge or clear the alarm.

## DKMS

The module is installed through DKMS so it can be rebuilt when the
Linux/Proxmox kernel is updated, provided matching kernel headers are
installed.

## Hardware access

The driver uses the Lenovo ThinkStation D30 NCT6681 monitoring interface at
I/O ports `0xA00-0xA02`. It validates the NCT6681-class Super-I/O ID (`0xB2xx`)
and EC base before registering hwmon.

Sensor access is intentionally read-only.
