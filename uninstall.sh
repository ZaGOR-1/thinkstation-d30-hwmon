#!/bin/bash
set -euo pipefail

NAME="d30-hwmon"
VERSION="1.5"
MODULE="d30_hwmon"

if [ "$(id -u)" -ne 0 ]; then
    echo "Run as root"
    exit 1
fi

modprobe -r "$MODULE" 2>/dev/null || true

dkms remove -m "$NAME" -v "$VERSION" --all 2>/dev/null || true

rm -rf "/usr/src/${NAME}-${VERSION}"
rm -f /etc/modules-load.d/d30_hwmon.conf

depmod -a

echo "d30_hwmon removed."
