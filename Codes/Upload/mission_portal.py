from flask import Flask, request, redirect, url_for, send_file
import os, subprocess, time, json
from werkzeug.utils import secure_filename

UPLOAD_DIR = "/home/pi/uploads"
OUT_DIR = "/home/pi/output"
STATUS_FILE = "/home/pi/status.txt"
DONE_FLAG = "/home/pi/done.flag"
WORKER_COMMAND_FILE = "/home/pi/worker_command.json"
WORKER_STATUS_FILE = "/home/pi/worker_status.json"

os.makedirs(UPLOAD_DIR, exist_ok=True)
os.makedirs(OUT_DIR, exist_ok=True)

app = Flask(__name__)
latest_output_path = None
has_upload = False

def write_status(s: str):
    with open(STATUS_FILE, "w") as f:
        f.write(s)

def send_worker_command(cmd_dict: dict):
    tmp = WORKER_COMMAND_FILE + ".tmp"
    with open(tmp, "w", encoding="utf-8") as f:
        json.dump(cmd_dict, f)
    os.replace(tmp, WORKER_COMMAND_FILE)

def next_command_id():
    return int(time.time() * 1000)

def read_worker_status():
    if not os.path.exists(WORKER_STATUS_FILE):
        return ""
    try:
        with open(WORKER_STATUS_FILE, "r", encoding="utf-8") as f:
            data = json.load(f)
        return f'{data.get("state", "")} | {data.get("message", "")}'
    except Exception:
        return "Worker status unavailable"

@app.get("/")
def index():
    status = ""
    if os.path.exists(STATUS_FILE):
        with open(STATUS_FILE, "r") as f:
            status = f.read()

    worker_status = read_worker_status()
    download_ready = (latest_output_path is not None)

    if download_ready:
        download_html = '''
        <div id="downloadBox">
          <p><a href="/download">Download mission file</a></p>
        </div>
        '''
    else:
        download_html = '<div id="downloadBox" style="display:none"></div>'

    return f"""
    <html><body style="font-family:-apple-system,system-ui,sans-serif;padding:20px">
      <h2>Upload Waypoints → MAVLink Mission</h2>
      <form action="/upload" method="post" enctype="multipart/form-data">
        <input id="fileInput" type="file" name="file" />
        <button type="submit">Upload & Process</button>
      </form>
      <hr>
      <p><b>Status:</b> {status}</p>
      <p><b>Worker:</b> {worker_status}</p>
      {download_html}

      <form action="/start_flight" method="post">
        <button type="submit">Start Flight</button>
      </form> 

      <form action="/finish" method="post">
        <button type="submit">Finish (close hotspot)</button>
      </form>

      <script>
        const fileInput = document.getElementById("fileInput");
        const downloadBox = document.getElementById("downloadBox");
        if (fileInput && downloadBox) {{
          fileInput.addEventListener("change", () => {{
            downloadBox.style.display = "none";
          }});
        }}
      </script>
    </body></html>
    """

@app.post("/upload")
def upload():
    f = request.files.get("file")
    if not f or f.filename == "":
        write_status("No file selected")
        return redirect(url_for("index"))

    name = secure_filename(f.filename)
    in_path = os.path.join(UPLOAD_DIR, name)
    f.save(in_path)

    write_status(f"Uploaded {name}. Processing...")

    out_path = "/home/pi/output/mission.waypoints"
    cmd = [
        "/home/pi/venvs/drone/bin/python",
        "/home/pi/converter_to_mission.py",
        in_path,
        out_path,
        "--trigger-mode",
        "waypoint",
        "--trigger-distance",
        "25"
    ]

    proc = subprocess.run(cmd, capture_output=True, text=True)
    if proc.returncode != 0:
       write_status("Converter error:\n" + (proc.stderr or proc.stdout)[-800:])
       return redirect(url_for("index"))

    send_worker_command({
        "id": next_command_id(),
        "cmd": "upload_mission",
        "path": out_path
    })

    write_status("Done. Mission file ready for download.")
    global has_upload, latest_output_path
    has_upload = True
    latest_output_path = out_path
    return redirect(url_for("index"))

@app.post("/start_flight")
def start_flight():
    send_worker_command({
        "id": next_command_id(),
        "cmd": "start_flight"
    })
    write_status("Start flight command sent.")
    return redirect(url_for("index"))

@app.get("/download")
def download():
    global latest_output_path
    if not latest_output_path:
        return "Not ready yet", 409
    return send_file(latest_output_path, as_attachment=True, download_name="mission.waypoints")

@app.post("/finish")
def finish():
    send_worker_command({
        "id": next_command_id(),
        "cmd": "finish"
    })

    with open(DONE_FLAG, "w") as f:
        f.write(str(time.time()))
    write_status("Finishing... You can close this page.")
    return redirect(url_for("index"))

if __name__ == "__main__":
    write_status("Ready. Upload a CSV.")
    if os.path.exists(DONE_FLAG):
        os.remove(DONE_FLAG)
    app.run(host="0.0.0.0", port=8000, debug=False)