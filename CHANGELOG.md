# Changelog

## 1.5

- Removed the experimental NCT6683-style monitor-source attributes: Lenovo's
  NCT6681 EC firmware does not use that source table for the active channels.
- Renamed System temperature5 to `CPU Package (EC mirror)` after a controlled
  load test showed a 0.989 correlation with Intel `coretemp` Package id 0.
- Kept all sensor access read-only and retained the verified temperature,
  fan, voltage, and chassis-intrusion interfaces.
- Added automatic cleanup of DKMS version 1.4 during upgrades.

## 1.4

- Added read-only `tempN_source_id` attributes from the NCT668x monitor
  assignment registers at `0x1a0 + channel`.
- Added full `tempN_source_config` bytes so the selector and high bit
  can be validated independently.
- Added `collect-source-ids.sh` to collect source assignments and map
  `drivetemp` instances to SCSI/block devices.
- Added automatic cleanup of DKMS version 1.3 during upgrades.

## 1.3

- Combined the high and low bytes of each temperature channel.
- Applied the Linux NCT668x signed 16-bit conversion
  `(raw16 / 128) * 500` millidegrees Celsius.
- Preserved the raw-low diagnostic attributes for independent verification.
- Added automatic cleanup of DKMS versions 1.1 and 1.2 during upgrades.

## 1.2

- Verified the NCT6681 access protocol and EC base against Lenovo BIOS
  A3KT70A.
- Verified temperature, fan-RPM, and voltage registers against the BIOS Setup
  hardware-monitor implementation.
- Replaced speculative physical temperature and fan labels with the neutral
  names used by Lenovo BIOS.
- Added NCT6681-class Super-I/O ID, logical-device enable, and EC-base checks.
- Added read-only `intrusion0_alarm` from Lenovo's BIOS-verified I/O port
  `0x466`, bit 0.
- Added diagnostic `tempN_raw_low` attributes without assuming an unverified
  fractional-temperature encoding.
- Added `proxmox-default-headers` to installation dependencies.
- Added module version metadata.

## 1.1

- Initial DKMS version with temperature, fan, and voltage monitoring.
