const express = require("express");
const cors    = require("cors");
const path    = require("path");

const app  = express();
const PORT = process.env.PORT || 3000;

app.use(cors());
app.use(express.json());
app.use(express.static(path.join(__dirname, "public")));

// ─── In-memory store ────────────────────────────────────────────
// Keeps the last 50 readings for the history sparkline
const MAX_HISTORY = 50;
let latest = null;
let history = [];

// ─── POST /data  (ESP32 sends readings here) ────────────────────
app.post("/data", (req, res) => {
  const { co2, co, temp, hum } = req.body;

  if (co2 == null || co == null || temp == null || hum == null) {
    return res.status(400).json({ error: "Missing fields" });
  }

  latest = {
    co2:       parseFloat(co2),
    co:        parseFloat(co),
    temp:      parseFloat(temp),
    hum:       parseFloat(hum),
    timestamp: new Date().toISOString()
  };

  history.push(latest);
  if (history.length > MAX_HISTORY) history.shift();

  console.log(`[${latest.timestamp}] CO2: ${co2} ppm | CO: ${co} ppm | Temp: ${temp}°C | RH: ${hum}%`);
  res.json({ ok: true });
});

// ─── GET /latest  (dashboard polls this) ───────────────────────
app.get("/latest", (req, res) => {
  if (!latest) return res.status(204).send();
  res.json(latest);
});

// ─── GET /history  (last 50 readings) ──────────────────────────
app.get("/history", (req, res) => {
  res.json(history);
});

app.listen(PORT, () => {
  console.log(`Carbon Monitor server running on port ${PORT}`);
});
