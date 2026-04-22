const express = require("express");
const cors = require("cors");
const { spawn } = require("child_process");
const path = require("path");

const app = express();

//  Σερβίρουμε ΟΛΑ τα αρχεία του φακέλου (html, js, css, κλπ)
app.use(express.static(__dirname));

// CORS + JSON
app.use(cors());
app.use(express.json());

// Αν μπει κάποιος στο "/", στείλε το βασικό HTML
// Αν το αρχείο σου έχει άλλο όνομα, άλλαξέ το εδώ.
app.get("/", (req, res) => {
  res.sendFile(path.join(__dirname, "mapdrawnew2.html"));
});

// Απλό τεστ: GET /ping -> "pong"
app.get("/ping", (req, res) => {
  res.send("pong");
});

// Απλό τεστ: POST /echo -> γυρνάει πίσω ό,τι του στείλαμε
app.post("/echo", (req, res) => {
  console.log("Body που πήρα:", req.body);
  res.json({ youSent: req.body });
});

// ΚΥΡΙΟ ROUTE: /api/survey που καλεί το C++ survey_cli.exe
app.post("/api/survey", (req, res) => {
  const polygon = req.body.polygon;
  const altitude = Number(req.body.altitude_m);
  const altitudeAGL_m = Number.isFinite(altitude) && altitude > 0 ? altitude : 10.0;
  const home = req.body.home;

  // 1) απλοί έλεγχοι
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

  // 2) ξεκινάμε το C++ πρόγραμμα
  // Σε Windows/Linux, αν το exe είναι στον ίδιο φάκελο:
  const binName = process.platform === "win32" ? "survey_cli.exe" : "survey_cli";
  const binPath = path.join(__dirname, binName);

  const args = [String(altitudeAGL_m)];
  if (homeLat != null && homeLon != null) {
    args.push(String(homeLat), String(homeLon));
  }

  const child = spawn(binPath, args);

  let output = "";
  let errorOutput = "";

  child.stdout.on("data", chunk => {
    output += chunk.toString();
  });

  child.stderr.on("data", chunk => {
    errorOutput += chunk.toString();
  });

  child.on("close", code => {
    if (code !== 0) {
      console.error("C++ exited with code:", code);
      console.error("stderr:", errorOutput);
      return res
        .status(500)
        .json({ error: "C++ survey_cli failed", code, stderr: errorOutput });
    }

    try {
      const result = JSON.parse(output);
      return res.json(result); // { waypoints: [...] }
    } catch (err) {
      console.error("Failed to parse JSON from C++:", err);
      console.error("Raw output:", output);
      return res
        .status(500)
        .json({ error: "Invalid JSON from survey_cli" });
    }
  });

  // 3) Στέλνουμε στο stdin του C++ το polygon σαν lines: "lat lon"
  polygon.forEach(p => {
    child.stdin.write(p.lat + " " + p.lon + "\n");
  });
  child.stdin.end();
});

const PORT = process.env.PORT || 3000;
app.listen(PORT, () => {
  console.log(`Server running at http://localhost:${PORT}`);
});
