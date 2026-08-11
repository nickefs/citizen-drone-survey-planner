const express = require("express");
const cors = require("cors");
const { spawn } = require("child_process");
const path = require("path");

const app = express();
const publicDir = path.join(__dirname, "public");

// Serve only the frontend files.
app.use(express.static(publicDir));

// CORS + JSON
app.use(cors());
app.use(express.json());

// Health check
app.get("/ping", (req, res) => {
  res.send("pong");
});

// Main API route: calls the C++ survey generator.
app.post("/api/survey", (req, res) => {
  const polygon = req.body.polygon;
  const altitude = Number(req.body.altitude_m);
  const altitudeAGL_m =
    Number.isFinite(altitude) && altitude > 0 ? altitude : 10.0;
  const camera = req.body.camera && typeof req.body.camera === "object"
    ? req.body.camera
    : {};
  const readPositive = (value, fallback) => {
    const n = Number(value);
    return Number.isFinite(n) && n > 0 ? n : fallback;
  };
  const readOverlap = (value, fallback) => {
    const n = Number(value);
    return Number.isFinite(n) && n >= 0 && n <= 95 ? n : fallback;
  };
  const readRange = (value, fallback, min, max) => {
    const n = Number(value);
    return Number.isFinite(n) && n >= min && n <= max ? n : fallback;
  };
  const surveyOptions = {
    sensorWidthMm: readPositive(camera.sensor_width_mm, 3.68),
    sensorHeightMm: readPositive(camera.sensor_height_mm, 2.76),
    focalLengthMm: readPositive(camera.focal_length_mm, 3.04),
    imageWidthPx: Math.round(readPositive(camera.image_width_px, 3280)),
    imageHeightPx: Math.round(readPositive(camera.image_height_px, 2474)),
    frontOverlapPct: readOverlap(camera.front_overlap_pct, 25),
    sideOverlapPct: readOverlap(camera.side_overlap_pct, 25),
    turnaroundM: readRange(req.body.turnaround_m, 3, 0, 100),
  };
  const home = req.body.home;

  if (!Array.isArray(polygon) || polygon.length < 3) {
    return res.status(400).json({ error: "Invalid polygon" });
  }

  let homeLat = null;
  let homeLon = null;

  if (home && typeof home === "object") {
    const lat = Number(home.lat);
    const lon = Number(home.lon);

    if (Number.isFinite(lat) && Number.isFinite(lon)) {
      homeLat = lat;
      homeLon = lon;
    }
  }

  const binName = process.platform === "win32" ? "survey_cli.exe" : "survey_cli";
  const binPath = path.join(__dirname, binName);

  const args = [
    String(altitudeAGL_m),
    "--sensor-width", String(surveyOptions.sensorWidthMm),
    "--sensor-height", String(surveyOptions.sensorHeightMm),
    "--focal-length", String(surveyOptions.focalLengthMm),
    "--image-width", String(surveyOptions.imageWidthPx),
    "--image-height", String(surveyOptions.imageHeightPx),
    "--front-overlap", String(surveyOptions.frontOverlapPct),
    "--side-overlap", String(surveyOptions.sideOverlapPct),
    "--turnaround", String(surveyOptions.turnaroundM),
  ];

  if (homeLat != null && homeLon != null) {
    args.push("--home", String(homeLat), String(homeLon));
  }

  const child = spawn(binPath, args);

  let output = "";
  let errorOutput = "";
  let replied = false;

  child.stdout.on("data", (chunk) => {
    output += chunk.toString();
  });

  child.stderr.on("data", (chunk) => {
    errorOutput += chunk.toString();
  });

  child.on("error", (err) => {
    if (replied) return;
    replied = true;
    console.error("Failed to start survey_cli:", err);
    return res.status(500).json({ error: "Failed to start survey_cli" });
  });

  child.on("close", (code) => {
    if (replied) return;
    replied = true;

    if (code !== 0) {
      console.error("C++ exited with code:", code);
      console.error("stderr:", errorOutput);

      return res.status(500).json({
        error: "C++ survey_cli failed",
        code,
        stderr: errorOutput,
      });
    }

    try {
      const result = JSON.parse(output);
      return res.json(result);
    } catch (err) {
      console.error("Failed to parse JSON from C++:", err);
      console.error("Raw output:", output);

      return res.status(500).json({
        error: "Invalid JSON from survey_cli",
      });
    }
  });

  // Pass polygon vertices to the C++ program through stdin as: "lat lon".
  polygon.forEach((p) => {
    child.stdin.write(p.lat + " " + p.lon + "\n");
  });

  child.stdin.end();
});

const PORT = process.env.PORT || 3000;

app.listen(PORT, () => {
  console.log(`Server running at http://localhost:${PORT}`);
});
