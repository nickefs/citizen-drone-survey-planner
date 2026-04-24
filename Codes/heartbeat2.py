from pymavlink import mavutil

master = mavutil.mavlink_connection("/dev/serial0", baud=57600)

print("Περιμένω heartbeat...")
master.wait_heartbeat()
print("Connected")

# Ζήτα όλα τα data streams με 4Hz
master.mav.request_data_stream_send(
    master.target_system,
    master.target_component,
    mavutil.mavlink.MAV_DATA_STREAM_ALL,
    4,
    1
)

while True:
    msg = master.recv_match(blocking=True)
    if msg:
        print(msg)
