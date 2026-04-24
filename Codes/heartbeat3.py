from pymavlink import mavutil

master = mavutil.mavlink_connection("/dev/serial0", baud=57600)

print("Περιμένω heartbeat...")
master.wait_heartbeat()
print("Connected")

master.mav.request_data_stream_send(
    master.target_system,
    master.target_component,
    mavutil.mavlink.MAV_DATA_STREAM_ALL,
    4,
    1
)

interesting = {
    "CAMERA_FEEDBACK",
    "COMMAND_ACK",
    "STATUSTEXT",
    "MISSION_ITEM_REACHED",
    "MISSION_CURRENT"
}

print("Ακούω μόνο trigger-related μηνύματα...")

while True:
    msg = master.recv_match(blocking=True)
    if msg and msg.get_type() in interesting:
        print(msg)
