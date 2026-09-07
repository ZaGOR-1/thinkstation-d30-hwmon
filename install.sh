#!/bin/bash
set -euo pipefail

NAME="d30-hwmon"
VERSION="1.5"
MODULE="d30_hwmon"
SRC="/usr/src/${NAME}-${VERSION}"
SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"

if [ "$(id -u)" -ne 0 ]; then
    echo "Run as root"
    exit 1
fi

echo "== Installing dependencies =="
apt update
apt install -y dkms lm-sensors proxmox-default-headers "proxmox-headers-$(uname -r)"

echo "== Installing DKMS source =="
modprobe -r "$MODULE" 2>/dev/null || true
if lsmod | grep -q "^${MODULE}[[:space:]]"; then
    echo "Cannot unload the currently loaded $MODULE module. Stop its users and retry."
    exit 1
fi

# Remove previous releases when upgrading.
for old_version in "1.1" "1.2" "1.3" "1.4"; do
    dkms remove -m "$NAME" -v "$old_version" --all 2>/dev/null || true
    rm -rf "/usr/src/${NAME}-${old_version}"
done

dkms remove -m "$NAME" -v "$VERSION" --all 2>/dev/null || true
rm -rf "$SRC"
mkdir -p "$SRC"

cp "$SCRIPT_DIR/d30_hwmon.c" "$SRC/"
cp "$SCRIPT_DIR/Makefile" "$SRC/"
cp "$SCRIPT_DIR/dkms.conf" "$SRC/"

echo "== Registering DKMS module =="
dkms add -m "$NAME" -v "$VERSION"
dkms build -m "$NAME" -v "$VERSION"
dkms install -m "$NAME" -v "$VERSION"

echo "== Enabling module at boot =="
echo "$MODULE" > /etc/modules-load.d/d30_hwmon.conf

depmod -a

echo "== Loading module =="
modprobe "$MODULE"

loaded_version="$(cat "/sys/module/${MODULE}/version")"
if [ "$loaded_version" != "$VERSION" ]; then
    echo "Loaded module version is $loaded_version, expected $VERSION"
    exit 1
fi

echo
echo "== DKMS status =="
dkms status

echo
echo "== Sensors =="
sensors d30_hwmon-virtual-0 || sensors

echo
echo "Installation complete."
