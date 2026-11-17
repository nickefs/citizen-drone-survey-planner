#!/bin/bash
# Run as the pi user (not with sudo): ./setup_drone.sh
set -e

sudo apt update

# --no-install-recommends: skip optional extras (mesa/llvm graphics drivers) the drone doesn't use
sudo apt install -y --no-install-recommends \
  python3 python3-venv python3-pip \
  network-manager \
  python3-libcamera python3-kms++ python3-picamera2 \
  libimage-exiftool-perl python3-pil

python3 -m venv --system-site-packages /home/pi/venvs/drone
source /home/pi/venvs/drone/bin/activate

python -m pip install --upgrade pip setuptools wheel
pip install pymavlink pyserial Flask Werkzeug piexif

sudo nmcli connection delete Hotspot 2>/dev/null || true
sudo nmcli connection add type wifi ifname wlan0 con-name Hotspot ssid "Pi-Upload"
sudo nmcli connection modify Hotspot \
  802-11-wireless.mode ap \
  wifi-sec.key-mgmt wpa-psk \
  wifi-sec.psk "12345678" \
  ipv4.addresses 192.168.4.1/24 \
  ipv4.method shared

# Start mission_script2.sh (hotspot + worker + portal) on every boot.
# Enabled only, not started now: starting the hotspot here would drop this SSH session.
sudo tee /etc/systemd/system/drone-mission.service > /dev/null <<'EOF'
[Unit]
Description=Drone hotspot + FC worker + mission portal
After=NetworkManager.service
Wants=NetworkManager.service

[Service]
Type=simple
User=pi
WorkingDirectory=/home/pi
ExecStart=/home/pi/mission_script2.sh
Restart=on-failure
RestartSec=5
KillMode=mixed
TimeoutStopSec=40

[Install]
WantedBy=multi-user.target
EOF
[ -f /home/pi/mission_script2.sh ] && chmod +x /home/pi/mission_script2.sh
sudo systemctl daemon-reload
sudo systemctl enable drone-mission.service

echo "Done."
echo "Activate venv with: source /home/pi/venvs/drone/bin/activate"
echo "Mission starts on next boot (sudo reboot). Make sure mission_script2.sh, mission_portal.py and fc_worker.py are in /home/pi/."
