#!/bin/bash
set -euo pipefail

NAME="d30-hwmon"
VERSION="1.1"
MODULE="d30_hwmon"
SRC="/usr/src/${NAME}-${VERSION}"

if [ "$(id -u)" -ne 0 ]; then
    echo "Run as root"
    exit 1
fi

echo "== Installing dependencies =="
apt update
apt install -y dkms lm-sensors "proxmox-headers-$(uname -r)"

echo "== Installing DKMS source =="
rm -rf "$SRC"
mkdir -p "$SRC"

cp d30_hwmon.c "$SRC/"
cp Makefile "$SRC/"
cp dkms.conf "$SRC/"

echo "== Registering DKMS module =="

dkms remove -m "$NAME" -v "$VERSION" --all 2>/dev/null || true

dkms add -m "$NAME" -v "$VERSION"
dkms build -m "$NAME" -v "$VERSION"
dkms install -m "$NAME" -v "$VERSION"

echo "== Enabling module at boot =="
echo "$MODULE" > /etc/modules-load.d/d30_hwmon.conf

depmod -a

echo "== Loading module =="
modprobe "$MODULE"

echo
echo "== DKMS status =="
dkms status

echo
echo "== Sensors =="
sensors d30_hwmon-virtual-0 || sensors

echo
echo "Installation complete."
