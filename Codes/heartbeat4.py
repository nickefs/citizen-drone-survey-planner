from pymavlink import mavutil

# Σύνδεση με το flight controller
master = mavutil.mavlink_connection("/dev/serial0", baud=57600)

print("Περιμένω heartbeat...")
master.wait_heartbeat()
print(f"Συνδέθηκα με system={master.target_system}, component={master.target_component}")

print("Ακούω μηνύματα από το FC...")

while True:
    msg = master.recv_match(blocking=True)
    if msg:
        print(msg)
