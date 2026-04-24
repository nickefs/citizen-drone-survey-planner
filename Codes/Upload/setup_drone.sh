#!/bin/bash
set -e

sudo apt update
sudo apt full-upgrade -y

sudo apt install -y \
  python3 python3-venv python3-pip \
  network-manager \
  python3-libcamera python3-kms++ python3-picamera2

python3 -m venv --system-site-packages /home/pi/venvs/drone
source /home/pi/venvs/drone/bin/activate

python -m pip install --upgrade pip setuptools wheel
pip install pymavlink pyserial Flask Werkzeug pyexif
pip install picamera2 || true

sudo nmcli connection delete Hotspot 2>/dev/null || true
sudo nmcli connection add type wifi ifname wlan0 con-name Hotspot ssid "Pi-Upload"
sudo nmcli connection modify Hotspot \
  802-11-wireless.mode ap \
  wifi-sec.key-mgmt wpa-psk \
  wifi-sec.psk "12345678" \
  ipv4.addresses 192.168.4.1/24 \
  ipv4.method shared

echo "Done."
echo "Activate venv with: source /home/pi/venvs/drone/bin/activate"