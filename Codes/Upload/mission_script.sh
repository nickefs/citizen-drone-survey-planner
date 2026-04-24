#!/bin/bash
set -euo pipefail

HOTSPOT_CONN="Hotspot"
VENV_PY="/home/pi/venvs/drone/bin/python"
PORTAL="/home/pi/mission_portal.py"
DONE_FLAG="/home/pi/done.flag"

echo "[1/3] Starting hotspot..."
sudo nmcli connection up "$HOTSPOT_CONN"

echo "[2/3] Starting portal at http://192.168.4.1:8000 ..."
rm -f "$DONE_FLAG"
"$VENV_PY" "$PORTAL" &
PORTAL_PID=$!

echo "Waiting for Finish..."
while [[ ! -f "$DONE_FLAG" ]]; do
  sleep 1
done

echo "Finish requested. Stopping portal..."
kill "$PORTAL_PID" 2>/dev/null || true
wait "$PORTAL_PID" 2>/dev/null || true

echo "[3/3] Stopping hotspot..."
sudo nmcli connection down "$HOTSPOT_CONN" || true

echo "Done."
