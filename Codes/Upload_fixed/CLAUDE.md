# Drone companion computer (Upload_fixed)

Raspberry Pi companion computer for an ArduCopter drone: `mission_portal.py` (Flask phone portal), `fc_worker.py`
(MAVLink worker), `converter_to_mission.py` (CSV → .waypoints), plus `setup_drone.sh`, `mission_script2.sh`, `local_script.sh`.
The originals live in `../Upload/` and must stay untouched.

## Rule: do not change the original workflow without asking

Never change any of these without asking the owner first and getting a clear yes:

- The commands sent to the flight controller at every stage (connect/stream request, mission clear/upload, arm,
  AUTO mode, MISSION_START, post-landing STABILIZE), their order, retries and timeouts.
- Any flight parameter: altitudes, speeds, overlap, trigger distance and placement, takeoff, RTL/land, thresholds.
- The mission the converter produces (output must stay byte-identical to the original for valid input).
- The order of the workflow steps (upload → camera → start → finish), the hotspot behaviour, and what the shell
  scripts start or stop.
- Photo capture, file naming and EXIF/XMP content.

UI/visual changes to the portal are fine, as long as every form action, field name, element id and the status
polling stay the same. If a UI change would need a workflow change, stop and ask.

When anything that touches the drone does change (after approval), update `CHANGES.md`.
