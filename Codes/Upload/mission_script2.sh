#!/bin/bash
set -euo pipefail

HOTSPOT_CONN="Hotspot"
VENV_PY="/home/pi/venvs/drone/bin/python"
PORTAL="/home/pi/mission_portal.py"
WORKER="/home/pi/fc_worker.py"
DONE_FLAG="/home/pi/done.flag"

echo "[1/4] Starting hotspot..."
sudo nmcli connection up "$HOTSPOT_CONN"

echo "[2/4] Starting FC worker..."
rm -f "$DONE_FLAG"
"$VENV_PY" "$WORKER" &
WORKER_PID=$!

echo "[3/4] Starting portal at http://192.168.4.1:8000 ..."
"$VENV_PY" "$PORTAL" &
PORTAL_PID=$!

echo "Waiting for Finish..."
while [[ ! -f "$DONE_FLAG" ]]; do
  sleep 1
done

echo "Finish requested. Stopping portal..."
kill "$PORTAL_PID" 2>/dev/null || true
wait "$PORTAL_PID" 2>/dev/null || true

echo "Stopping worker..."
kill "$WORKER_PID" 2>/dev/null || true
wait "$WORKER_PID" 2>/dev/null || true

echo "[4/4] Stopping hotspot..."
sudo nmcli connection down "$HOTSPOT_CONN" || true

echo "Done."