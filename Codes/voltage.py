from pymavlink import mavutil

# Example connections:
master = mavutil.mavlink_connection("/dev/serial0", baud=57600)   # serial      # UDP example

master.wait_heartbeat()
tsys = master.target_system
tcmp = master.target_component

# Ask ArduPilot to stream BATTERY_STATUS (msg id 147) at 1 Hz
master.mav.command_long_send(
    tsys,
    tcmp,
    mavutil.mavlink.MAV_CMD_SET_MESSAGE_INTERVAL,  # 511
    0,                                             # confirmation
    147,                                           # param1: message id (BATTERY_STATUS)
    1_000_000,                                     # param2: interval in us (1 Hz)
    0, 0, 0, 0, 0                                  # param3..7
)

# Receive one BATTERY_STATUS and print main voltage
msg = master.recv_match(type="BATTERY_STATUS", blocking=True, timeout=5)
if msg:
    v_mv = msg.voltages[0]          # millivolts
    print(f"Battery: {v_mv/1000:.2f} V")
else:
    print("No BATTERY_STATUS received")