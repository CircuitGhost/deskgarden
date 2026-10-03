#include "web_dashboard.h"

#include <ESPmDNS.h>
#include <Preferences.h>
#include <WiFi.h>
#include <stdio.h>
#include <string.h>

#include "hud_manager.h"
#include "mesh_sync.h"
#include "moisture_system.h"
#include "net_sync.h"
#include "plant_engine.h"
#include "state_storage.h"

WebDashboard Dashboard;

static const char* DASH_PREFS = "deskflower_dash";

static char s_body[1600];

static const char DASHBOARD_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Deskflower</title>
  <style>
    * { box-sizing: border-box; }
    body { margin: 0; background: #0f172a; color: #f8fafc; font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif; }
    main { max-width: 720px; margin: 0 auto; padding: 28px 18px 48px; }
    h1 { font-size: 26px; margin: 0; color: #38bdf8; letter-spacing: -0.4px; }
    h2 { font-size: 15px; margin: 28px 0 12px; color: #e2e8f0; }
    .sub { color: #94a3b8; font-size: 13px; margin-top: 4px; }
    .grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(140px, 1fr)); gap: 10px; }
    .card { background: #1e293b; border: 1px solid #334155; border-radius: 12px; padding: 14px; }
    .label { font-size: 11px; letter-spacing: 0.4px; text-transform: uppercase; color: #94a3b8; }
    .value { font-size: 22px; margin-top: 4px; font-variant-numeric: tabular-nums; }
    .graph { background: #1e293b; border: 1px solid #334155; border-radius: 12px; padding: 12px; }
    svg { width: 100%; height: 120px; display: block; }
    .legend { display: flex; gap: 16px; font-size: 12px; color: #94a3b8; margin-top: 8px; }
    .swatch { display: inline-block; width: 10px; height: 10px; border-radius: 99px; margin-right: 6px; }
    form { display: grid; gap: 10px; }
    label { font-size: 12px; color: #94a3b8; }
    input, select { width: 100%; padding: 10px 12px; border-radius: 8px; border: 1px solid #334155; background: #0f172a; color: #f8fafc; }
    .row { display: grid; grid-template-columns: 1fr 1fr; gap: 10px; }
    button, .btn { background: #0284c7; color: white; border: 0; border-radius: 8px; padding: 11px 14px; font-weight: 600; cursor: pointer; text-decoration: none; display: inline-block; }
    .ghost { background: #334155; }
    .seeds { display: grid; gap: 8px; }
    .seed { display: flex; gap: 8px; align-items: center; background: #1e293b; border: 1px solid #334155; border-radius: 10px; padding: 10px; }
    .seed code { flex: 1; font-size: 13px; color: #e2e8f0; }
    .kind { font-size: 11px; color: #94a3b8; }
    .footer { margin-top: 28px; color: #64748b; font-size: 12px; }
  </style>
</head>
<body>
<main>
  <h1 id="title">Deskflower</h1>
  <div class="sub" id="subtitle">Local terrarium</div>

  <h2>Live telemetry</h2>
  <div class="grid">
    <div class="card"><div class="label">Hydration</div><div class="value" id="hydration">—</div></div>
    <div class="card"><div class="label">Growth</div><div class="value" id="growth">—</div></div>
    <div class="card"><div class="label">Lifetime</div><div class="value" id="lifetime">—</div></div>
    <div class="card"><div class="label">Generation</div><div class="value" id="generation">—</div></div>
    <div class="card"><div class="label">Weather</div><div class="value" id="weather">—</div></div>
    <div class="card"><div class="label">Desk mesh</div><div class="value" id="peers">—</div></div>
  </div>
  <div class="graph" style="margin-top:10px">
    <svg id="chart" viewBox="0 0 240 64" preserveAspectRatio="none"></svg>
    <div class="legend"><span><i class="swatch" style="background:#38bdf8"></i>Hydration</span><span><i class="swatch" style="background:#fbbf24"></i>Growth</span></div>
  </div>

  <h2>Seed code library</h2>
  <div class="seeds" id="seeds"></div>
  <p style="margin-top:12px"><a class="btn ghost" href="/api/seeds/export">Export library</a></p>
  <form method="POST" action="/api/seeds/import">
    <label>Import a specimen</label>
    <input name="seed" maxlength="20" placeholder="XXXX-XXXX-XXXX-XXXX" required>
    <button type="submit">Import</button>
  </form>

  <h2>Terrarium</h2>
  <form method="POST" action="/api/settings" id="settings">
    <label>Plant name</label>
    <input name="name" id="name" maxlength="24" value="Deskflower">
    <label>Petal palette</label>
    <select name="palette" id="palette">
      <option value="0">Magenta Orchid</option>
      <option value="1">Cherry Blossom</option>
      <option value="2">Solar Amber</option>
      <option value="3">Bioluminescent Cyan</option>
      <option value="4">Moon Lily</option>
    </select>
    <div class="row">
      <div><label>Quiet hours start</label><select name="quietStart" id="quietStart"></select></div>
      <div><label>Quiet hours end</label><select name="quietEnd" id="quietEnd"></select></div>
    </div>
    <button type="submit">Save terrarium</button>
  </form>
  <div class="footer">Served on the local network at http://deskflower.local. Vein glow stays on its 23:00–05:00 clock.</div>
</main>
<script>
function hours(sel, current) {
  sel.innerHTML = "";
  for (var h = 0; h < 24; h++) {
    var o = document.createElement("option");
    o.value = String(h);
    o.textContent = (h < 10 ? "0" : "") + h + ":00";
    if (h === current) o.selected = true;
    sel.appendChild(o);
  }
}
function line(values, color) {
  if (!values || !values.length) return "";
  var step = values.length === 1 ? 0 : 240 / (values.length - 1);
  var pts = values.map(function(v, i) {
    var x = Math.round(i * step);
    var y = Math.round(64 - (Math.max(0, Math.min(100, v)) / 100) * 60 - 2);
    return x + "," + y;
  }).join(" ");
  return '<polyline fill="none" stroke="' + color + '" stroke-width="2" points="' + pts + '"/>';
}
function copyCode(code, btn) {
  var done = function() { btn.textContent = "Copied"; setTimeout(function() { btn.textContent = "Copy"; }, 1200); };
  if (navigator.clipboard && navigator.clipboard.writeText) {
    navigator.clipboard.writeText(code).then(done).catch(function() { fallback(code, btn, done); });
  } else fallback(code, btn, done);
}
function fallback(code, btn, done) {
  var input = document.createElement("input");
  input.value = code;
  document.body.appendChild(input);
  input.select();
  try { document.execCommand("copy"); done(); } catch (e) { btn.textContent = "Select code"; }
  document.body.removeChild(input);
}
function seedRow(code, kind) {
  var row = document.createElement("div");
  row.className = "seed";
  var c = document.createElement("code");
  c.textContent = code;
  var k = document.createElement("span");
  k.className = "kind";
  k.textContent = kind;
  var b = document.createElement("button");
  b.type = "button";
  b.className = "ghost";
  b.textContent = "Copy";
  b.onclick = function() { copyCode(code, b); };
  row.appendChild(c);
  row.appendChild(k);
  row.appendChild(b);
  return row;
}
var formReady = false;
function paint(data) {
  document.getElementById("title").textContent = data.name || "Deskflower";
  document.getElementById("subtitle").textContent = (data.phenotype || "Specimen") + "  ·  " + (data.seed || "");
  document.getElementById("hydration").textContent = data.hydration + "%";
  document.getElementById("growth").textContent = data.growth + "%";
  document.getElementById("lifetime").textContent = data.lifetimeMinutes + " min";
  var gen = String(data.generation);
  if (gen.length < 2) gen = "0" + gen;
  document.getElementById("generation").textContent = "GEN " + gen;
  var weather = data.weather || "—";
  if (typeof data.tempF === "number") weather += "  " + data.tempF + "°F";
  document.getElementById("weather").textContent = weather;
  document.getElementById("peers").textContent = (data.peers || 0) + " nearby";
  document.getElementById("chart").innerHTML = line(data.moisture, "#38bdf8") + line(data.growthSeries, "#fbbf24");
  if (!formReady) {
    document.getElementById("name").value = data.name || "Deskflower";
    if (typeof data.palette === "number" && data.palette <= 4) document.getElementById("palette").value = String(data.palette);
    hours(document.getElementById("quietStart"), data.quietStart);
    hours(document.getElementById("quietEnd"), data.quietEnd);
    formReady = true;
  }
}
function paintSeeds(data) {
  var box = document.getElementById("seeds");
  box.innerHTML = "";
  if (data.current) box.appendChild(seedRow(data.current, "Living specimen"));
  (data.items || []).forEach(function(item) {
    var kind = item.kind === "imported" ? "Imported" : "Hybrid";
    if (item.phenotype) kind += " · " + item.phenotype;
    box.appendChild(seedRow(item.code, kind));
  });
}
function tick() {
  fetch("/api/telemetry").then(function(r) { return r.json(); }).then(paint).catch(function() {});
  fetch("/api/seeds").then(function(r) { return r.json(); }).then(paintSeeds).catch(function() {});
}
hours(document.getElementById("quietStart"), 23);
hours(document.getElementById("quietEnd"), 6);
tick();
setInterval(tick, 2000);
</script>
</body>
</html>
)rawliteral";

static const char* phenotypeName(uint8_t id) {
    switch (id) {
        case PHENOTYPE_ORCHID: return "Highland Orchid";
        case PHENOTYPE_FERN: return "Dwarf Fern";
        case PHENOTYPE_SUCCULENT: return "Micro-Succulent";
        case PHENOTYPE_BONSAI: return "Ancient Bonsai";
        default: return "Flora";
    }
}

static const char* weatherName(WeatherType type) {
    switch (type) {
        case WEATHER_SUN: return "Clear";
        case WEATHER_PARTLY_CLOUDY: return "Partly Cloudy";
        case WEATHER_CLOUDY: return "Overcast";
        case WEATHER_RAIN: return "Rain";
        case WEATHER_MOON: return "Clear Night";
        case WEATHER_SNOW: return "Snow";
        case WEATHER_FOG: return "Fog";
        default: return "Unknown";
    }
}

static int advanceBody(int used, int wrote) {
    if (wrote < 0 || used < 0 || used >= (int)sizeof(s_body)) return (int)sizeof(s_body);
    if (wrote >= (int)sizeof(s_body) - used) return (int)sizeof(s_body);
    return used + wrote;
}

static uint16_t minutesForward(uint16_t from, uint16_t to) {
    return (to >= from) ? (uint16_t)(to - from) : (uint16_t)(to + 1440u - from);
}

static void copyPlantName(char* dst, size_t dstLen, const char* src) {
    if (!dst || dstLen == 0) return;
    dst[dstLen - 1] = '\0';
    size_t j = 0;
    if (!src) src = "";
    for (size_t i = 0; src[i] != '\0' && j + 1 < dstLen; i++) {
        unsigned char c = (unsigned char)src[i];
        if (c >= 32 && c < 127 && c != '"' && c != '<' && c != '>' && c != '\\' && c != '&') {
            dst[j++] = (char)c;
        }
    }
    dst[j] = '\0';
    if (j == 0) {
        strncpy(dst, "Deskflower", dstLen - 1);
        dst[dstLen - 1] = '\0';
    }
}

WebDashboard::WebDashboard()
    : _server(nullptr),
      _histHead(0),
      _histCount(0),
      _lastSampleMs(0),
      _palette(255),
      _quietStart(23),
      _quietEnd(6),
      _stationHttp(false),
      _mdns(false) {
    memset(_hist, 0, sizeof(_hist));
    strncpy(_name, "Deskflower", sizeof(_name) - 1);
    _name[sizeof(_name) - 1] = '\0';
}

void WebDashboard::loadPrefs() {
    Preferences prefs;
    prefs.begin(DASH_PREFS, true);
    String stored = prefs.getString("name", "Deskflower");
    _palette = prefs.getUChar("pal", 255);
    _quietStart = prefs.getUChar("qstart", 23);
    _quietEnd = prefs.getUChar("qend", 6);
    prefs.end();
    copyPlantName(_name, sizeof(_name), stored.c_str());
    if (_quietStart > 23) _quietStart = 23;
    if (_quietEnd > 23) _quietEnd = 6;
    if (_palette > 4 && _palette != 255) _palette = 255;
}

void WebDashboard::savePrefs() {
    Preferences prefs;
    prefs.begin(DASH_PREFS, false);
    prefs.putString("name", _name);
    prefs.putUChar("pal", _palette);
    prefs.putUChar("qstart", _quietStart);
    prefs.putUChar("qend", _quietEnd);
    prefs.end();
}

void WebDashboard::begin(WebServer& server) {
    _server = &server;
    loadPrefs();
    if (_palette <= 4) {
        Plant.setPetalPalette(_palette);
    }
}

void WebDashboard::registerRoutes(WebServer& server) {
    _server = &server;
    server.on("/api/telemetry", HTTP_GET, [this]() { sendTelemetry(); });
    server.on("/api/seeds", HTTP_GET, [this]() { sendSeeds(); });
    server.on("/api/seeds/export", HTTP_GET, [this]() { sendExport(); });
    server.on("/api/seeds/import", HTTP_POST, [this]() { importSeed(); });
    server.on("/api/settings", HTTP_POST, [this]() { saveSettings(); });
}

void WebDashboard::sendHome(WebServer& server) const {
    server.sendHeader("Cache-Control", "no-store");
    server.send_P(200, "text/html", DASHBOARD_HTML);
}

bool WebDashboard::rejectIfClosed() {
    if (!_server) return true;
    if (NetSync.isPortalActive() || !NetSync.isConnected()) {
        _server->send(404, "text/plain", "Not found");
        return true;
    }
    return false;
}

void WebDashboard::ensureStation() {
    if (!_server || _stationHttp) return;
    _server->begin();
    if (MDNS.begin("deskflower")) {
        MDNS.addService("http", "tcp", 80);
        _mdns = true;
        #if ENABLE_SERIAL_LOG
        Serial.println("[NET] Dashboard at http://deskflower.local");
        #endif
    }
    _stationHttp = true;
}

void WebDashboard::stopStation() {
    if (_mdns) {
        MDNS.end();
        _mdns = false;
    }
    if (_stationHttp && _server) {
        _server->stop();
        _stationHttp = false;
    }
}

void WebDashboard::sample(uint32_t nowMs) {
    if (_lastSampleMs != 0 && (uint32_t)(nowMs - _lastSampleMs) < 60000u) return;
    _lastSampleMs = nowMs;

    float growth = Plant.getGrowthProgress();
    if (growth < 0.0f) growth = 0.0f;
    if (growth > 1.0f) growth = 1.0f;
    float moisture = Moisture.getMoisture();
    if (moisture < 0.0f) moisture = 0.0f;
    if (moisture > 100.0f) moisture = 100.0f;

    Sample& slot = _hist[_histHead];
    slot.moisture = (uint8_t)(moisture + 0.5f);
    slot.growth = (uint8_t)(growth * 100.0f + 0.5f);
    _histHead = (uint8_t)((_histHead + 1) % HIST_LEN);
    if (_histCount < HIST_LEN) _histCount++;
}

uint8_t WebDashboard::backlightFor(uint8_t hour, uint8_t minute) const {
    uint16_t now = (uint16_t)hour * 60u + minute;
    uint16_t start = (uint16_t)_quietStart * 60u;
    uint16_t end = (uint16_t)_quietEnd * 60u;
    const uint8_t maxB = DEFAULT_BRIGHTNESS; // 160
    const uint8_t minB = 35;

    if (start == end) return maxB;

    bool quiet = (start < end) ? (now >= start && now < end) : (now >= start || now < end);
    if (quiet) return minB;

    uint16_t sinceEnd = minutesForward(end, now);
    if (sinceEnd <= 30) {
        return (uint8_t)(minB + (sinceEnd * (maxB - minB)) / 30u);
    }
    uint16_t untilStart = minutesForward(now, start);
    if (untilStart <= 30) {
        return (uint8_t)(minB + (untilStart * (maxB - minB)) / 30u);
    }
    return maxB;
}

void WebDashboard::sendTelemetry() {
    if (rejectIfClosed()) return;

    uint8_t growthPct = 0;
    float growth = Plant.getGrowthProgress();
    if (growth < 0.0f) growth = 0.0f;
    if (growth > 1.0f) growth = 1.0f;
    growthPct = (uint8_t)(growth * 100.0f + 0.5f);

    char seed[20];
    memset(seed, 0, sizeof(seed));
    Plant.getSeedCode(seed);

    uint8_t palette = Plant.getGenome().petalPalette;
    if (palette > 4) palette = 0;

    int used = snprintf(
        s_body, sizeof(s_body),
        "{\"name\":\"%s\",\"phenotype\":\"%s\",\"hydration\":%u,\"growth\":%u,"
        "\"lifetimeMinutes\":%u,\"generation\":%u,\"weather\":\"%s\",\"tempF\":%d,"
        "\"humidity\":%u,\"precip\":%u,\"seed\":\"%s\",\"peers\":%u,\"palette\":%u,"
        "\"quietStart\":%u,\"quietEnd\":%u,\"moisture\":[",
        _name,
        Plant.getPhenotypeName(),
        (unsigned)(Moisture.getMoisture() + 0.5f),
        growthPct,
        Storage.getLifetimeMinutes(),
        HUD.getGeneration(),
        weatherName(HUD.getWeatherType()),
        (int)HUD.getTemperatureF(),
        HUD.getHumidity(),
        HUD.getPrecipIntensity(),
        seed,
        Mesh.peerCount(),
        palette,
        _quietStart,
        _quietEnd);
    if (used < 0 || used >= (int)sizeof(s_body)) used = (int)sizeof(s_body) - 1;

    uint8_t start = (_histCount == HIST_LEN) ? _histHead : 0;
    for (uint8_t i = 0; i < _histCount && used < (int)sizeof(s_body) - 8; i++) {
        uint8_t index = (uint8_t)((start + i) % HIST_LEN);
        int wrote = snprintf(s_body + used, sizeof(s_body) - (size_t)used, "%s%u",
                             (i == 0) ? "" : ",", _hist[index].moisture);
        used = advanceBody(used, wrote);
        if (used >= (int)sizeof(s_body)) break;
    }
    if (used < (int)sizeof(s_body)) {
        int wrote = snprintf(s_body + used, sizeof(s_body) - (size_t)used, "],\"growthSeries\":[");
        used = advanceBody(used, wrote);
    }
    for (uint8_t i = 0; i < _histCount && used < (int)sizeof(s_body) - 4; i++) {
        uint8_t index = (uint8_t)((start + i) % HIST_LEN);
        int wrote = snprintf(s_body + used, sizeof(s_body) - (size_t)used, "%s%u",
                             (i == 0) ? "" : ",", _hist[index].growth);
        used = advanceBody(used, wrote);
        if (used >= (int)sizeof(s_body)) break;
    }
    if (used < (int)sizeof(s_body) - 2) {
        s_body[used++] = ']';
        s_body[used++] = '}';
        s_body[used] = '\0';
    } else {
        s_body[sizeof(s_body) - 1] = '\0';
    }

    _server->sendHeader("Cache-Control", "no-store");
    _server->send(200, "application/json", s_body);
}

void WebDashboard::sendSeeds() {
    if (rejectIfClosed()) return;

    char seed[20];
    memset(seed, 0, sizeof(seed));
    Plant.getSeedCode(seed);
    int used = snprintf(s_body, sizeof(s_body), "{\"current\":\"%s\",\"items\":[", seed);
    if (used < 0) used = 0;

    bool first = true;
    for (uint8_t i = 0; i < Mesh.seedSlotCount() && used < (int)sizeof(s_body) - 80; i++) {
        MeshSeedSlot slot;
        if (!Mesh.copySeedSlot(i, slot)) continue;
        const char* kind = (slot.kind == MESH_SEED_IMPORTED) ? "imported" : "hybrid";
        int wrote = snprintf(s_body + used, sizeof(s_body) - (size_t)used,
                             "%s{\"code\":\"%s\",\"kind\":\"%s\",\"phenotype\":\"%s\",\"palette\":%u}",
                             first ? "" : ",",
                             slot.code,
                             kind,
                             phenotypeName(slot.phenotype),
                             slot.petalPalette);
        used = advanceBody(used, wrote);
        if (used >= (int)sizeof(s_body)) break;
        first = false;
    }
    if (used < (int)sizeof(s_body) - 2) {
        s_body[used++] = ']';
        s_body[used++] = '}';
        s_body[used] = '\0';
    } else {
        s_body[sizeof(s_body) - 1] = '\0';
    }

    _server->sendHeader("Cache-Control", "no-store");
    _server->send(200, "application/json", s_body);
}

void WebDashboard::sendExport() {
    if (rejectIfClosed()) return;

    char seed[20];
    memset(seed, 0, sizeof(seed));
    Plant.getSeedCode(seed);
    int used = snprintf(s_body, sizeof(s_body),
                        "Deskflower seed library\nname: %s\nphenotype: %s\ngeneration: %u\ncurrent: %s\n",
                        _name, Plant.getPhenotypeName(), HUD.getGeneration(), seed);
    if (used < 0) used = 0;

    for (uint8_t i = 0; i < Mesh.seedSlotCount() && used < (int)sizeof(s_body) - 48; i++) {
        MeshSeedSlot slot;
        if (!Mesh.copySeedSlot(i, slot)) continue;
        const char* kind = (slot.kind == MESH_SEED_IMPORTED) ? "imported" : "hybrid";
        int wrote = snprintf(s_body + used, sizeof(s_body) - (size_t)used, "%s  %s  %s\n",
                             slot.code, kind, phenotypeName(slot.phenotype));
        used = advanceBody(used, wrote);
        if (used >= (int)sizeof(s_body)) break;
    }

    _server->sendHeader("Content-Disposition", "attachment; filename=\"deskflower-seeds.txt\"");
    _server->send(200, "text/plain", s_body);
}

void WebDashboard::importSeed() {
    if (rejectIfClosed()) return;

    String seed = _server->arg("seed");
    seed.trim();
    if (!Plant.loadSeedCode(seed.c_str())) {
        _server->send(400, "text/html",
                      "<!DOCTYPE html><html><body style=\"background:#0f172a;color:#f8fafc;font-family:sans-serif\">"
                      "<p>That seed code could not be read.</p><p><a href=\"/\">Back to the terrarium</a></p></body></html>");
        return;
    }

    const PlantGenome& genome = Plant.getGenome();
    Mesh.rememberSeed(seed.c_str(), MESH_SEED_IMPORTED, genome.phenotype, genome.petalPalette, genome.stemHue);
    _palette = 255;
    savePrefs();
    Storage.markDirty();

    _server->sendHeader("Location", "/");
    _server->send(303, "text/plain", "Imported");
}

void WebDashboard::saveSettings() {
    if (rejectIfClosed()) return;

    copyPlantName(_name, sizeof(_name), _server->arg("name").c_str());

    int palette = _server->arg("palette").toInt();
    int quietStart = _server->arg("quietStart").toInt();
    int quietEnd = _server->arg("quietEnd").toInt();
    if (quietStart < 0) quietStart = 0;
    if (quietStart > 23) quietStart = 23;
    if (quietEnd < 0) quietEnd = 0;
    if (quietEnd > 23) quietEnd = 23;
    _quietStart = (uint8_t)quietStart;
    _quietEnd = (uint8_t)quietEnd;

    if (palette >= 0 && palette <= 4) {
        _palette = (uint8_t)palette;
        Plant.setPetalPalette(_palette);
        Storage.markDirty();
    }
    savePrefs();

    _server->sendHeader("Location", "/");
    _server->send(303, "text/plain", "Saved");
}
