from pymavlink import mavutil

master = mavutil.mavlink_connection("/dev/serial0", baud=57600)

print("Περιμένω heartbeat...")
master.wait_heartbeat()
print(f"Συνδέθηκα με system={master.target_system}, component={master.target_component}")

print("Ακούω μόνο CAMERA_FEEDBACK...")

while True:
    msg = master.recv_match(type="CAMERA_FEEDBACK", blocking=True)
    if msg:
        print(msg)
