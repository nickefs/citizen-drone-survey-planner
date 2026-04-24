#!/usr/bin/env python3
from pymavlink import mavutil
from picamera2 import Picamera2
from fractions import Fraction
import piexif
import os, time

# --- helpers EXIF (απλές μετατροπές) ---
def _dms(deg):
    deg = abs(deg); d = int(deg); m = int((deg - d) * 60); s = (deg - d - m/60) * 3600
    f = Fraction(s).limit_denominator(1000)
    return ((d,1),(m,1),(f.numerator,f.denominator))

def _rat(x, den=1000):
    f = Fraction(x).limit_denominator(den)
    return (f.numerator, f.denominator)

def write_exif_from_feedback(jpeg_path, msg):
    sec  = msg.time_usec // 1_000_000
    usec = msg.time_usec % 1_000_000
    tm   = time.gmtime(sec)

    lat, lon, alt = msg.lat/1e7, msg.lng/1e7, float(msg.alt_msl)
    yaw, pitch, roll = float(msg.yaw), float(msg.pitch), float(msg.roll)

    ex = {"0th":{}, "Exif":{}, "GPS":{}, "1st":{}, "thumbnail":None}
    ex["0th"][piexif.ImageIFD.Software] = "Picamera2+MAVLink"

    date_str = f"{tm.tm_year:04d}:{tm.tm_mon:02d}:{tm.tm_mday:02d}"
    time_str = f"{tm.tm_hour:02d}:{tm.tm_min:02d}:{tm.tm_sec:02d}"
    ex["Exif"][piexif.ExifIFD.DateTimeOriginal]   = f"{date_str} {time_str}"
    ex["Exif"][piexif.ExifIFD.SubSecTimeOriginal] = f"{usec:06d}"
    ex["Exif"][piexif.ExifIFD.UserComment] = (
        b"ASCII\x00\x00\x00" + f"yaw={yaw},pitch={pitch},roll={roll}".encode("ascii")
    )

    ex["GPS"][piexif.GPSIFD.GPSLatitudeRef]  = b"N" if lat>=0 else b"S"
    ex["GPS"][piexif.GPSIFD.GPSLatitude]     = _dms(lat)
    ex["GPS"][piexif.GPSIFD.GPSLongitudeRef] = b"E" if lon>=0 else b"W"
    ex["GPS"][piexif.GPSIFD.GPSLongitude]    = _dms(lon)
    ex["GPS"][piexif.GPSIFD.GPSAltitudeRef]  = 0 if alt>=0 else 1
    ex["GPS"][piexif.GPSIFD.GPSAltitude]     = _rat(abs(alt))
    ex["GPS"][piexif.GPSIFD.GPSImgDirectionRef] = b"T"
    ex["GPS"][piexif.GPSIFD.GPSImgDirection]    = _rat(yaw % 360.0)
    ex["GPS"][piexif.GPSIFD.GPSTimeStamp] = ((tm.tm_hour,1),(tm.tm_min,1),(tm.tm_sec*1000 + usec//1000,1000))
    ex["GPS"][piexif.GPSIFD.GPSDateStamp]  = date_str

    piexif.insert(piexif.dump(ex), jpeg_path)

# Φάκελος αποθήκευσης φωτογραφιών
save_dir = "/home/pi/photos"
os.makedirs(save_dir, exist_ok=True)

# MAVLink
m = mavutil.mavlink_connection('/dev/serial0', baud=57600)
m.wait_heartbeat()
print("Connected with Pixhawk")

# Κάμερα
cam = Picamera2()
config = cam.create_still_configuration(main={"size": (3280,2464)}) #half res
#config = cam.create_still_configuration(main={"size": (4608, 2592)}) # full res
cam.configure(config)

# AF συνεχές, manual έκθεση (προσαρμόσε αν θες πιο φωτεινά)
#cam.set_controls({
    #"AfMode": 2,          # continuous AF
 #   "AeEnable": True    # manual exposure
    #"ExposureTime": 2000, # ~1/500s (μs)
    #"AnalogueGain": 4   # ISO περίπου 400
#})

# Auto focus, shutterspeed,gain

cam.set_controls({
    "AeEnable": False,
    "ExposureTime": 1200,
   "AnalogueGain": 1
})


cam.start()
time.sleep(1.0)  # warmup

print("Waiting for CAMERA_FEEDBACK ...")

while True:
    msg = m.recv_match(type='CAMERA_FEEDBACK', blocking=True)
    if not msg:
        continue

    # Όνομα αρχείου από τον χρόνο του feedback (UTC)
    sec = msg.time_usec // 1_000_000
    usec = msg.time_usec % 1_000_000
    ts = time.strftime("%Y%m%d_%H%M%S", time.gmtime(sec)) + f"_{usec:06d}"
    path = os.path.join(save_dir, f"photo_{ts}.jpg")

    # Τράβα φωτογραφία (μόνο path, όπως ζήτησες)
    cam.capture_file(path)

    # Γράψε EXIF (GPS/UTC/yaw/pitch/roll)
    write_exif_from_feedback(path, msg)

    print(f"[TRIGGER] Φωτογραφία σώθηκε: {path}")
