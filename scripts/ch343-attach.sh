#!/usr/bin/env bash
# Attach CH343 (COM6 / 1a86:55d3) into WSL for pio upload.
# One-time on Windows (Admin PowerShell):  usbipd bind --busid 1-2
set -euo pipefail
USBIPD="${USBIPD:-/mnt/c/Program Files/usbipd-win/usbipd.exe}"
BUSID="${CH343_BUSID:-1-2}"

"$USBIPD" attach --wsl --busid "$BUSID" 2>&1 || true
sleep 2
DEV=""
for d in /dev/ttyUSB* /dev/ttyACM*; do
  [[ -e "$d" ]] && DEV="$d" && break
done
if [[ -z "$DEV" ]]; then
  echo "No ttyUSB/ttyACM — bind needs Admin once:"
  echo '  usbipd bind --busid 1-2'
  echo "Or flash from PowerShell: ~/Downloads/root-setup/flash-com6.ps1"
  exit 1
fi
echo "CH343 ready: $DEV"
echo "Flash:  cd ~/root && pio run -t upload --upload-port $DEV"
