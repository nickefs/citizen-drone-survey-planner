from picamera2 import Picamera2

cam = Picamera2()
config = cam.create_still_configuration(main={"size": (2304, 1296)}) #half res
#config = cam.create_still_configuration(main={"size": (4608, 2592)}) # full res
cam.configure(config)
cam.start()

mode = input("auto ή manual; ").strip().lower()

if mode == "manual":
    exposure = int(input("ExposureTime: "))
    gain = float(input("AnalogueGain: "))
    cam.set_controls({
        "AeEnable": False,
        "ExposureTime": exposure,
        "AnalogueGain": gain
    })
else:
    cam.set_controls({
        "AeEnable": True
    })

while True:
    key = input("enter για φωτογραφία, q για έξοδο: ")
    if key == "q":
        break
    cam.capture_file("photo.jpg")
    print("saved")

cam.stop()
