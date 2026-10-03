#include "net_sync.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <time.h>
#include "hud_manager.h"
#include "time_atmosphere.h"
#include "hal_peripherals.h"
#include "plant_engine.h"
#include "state_storage.h"
#include "mesh_sync.h"
#include "web_dashboard.h"

NetSyncManager NetSync;

static DNSServer dnsServer;
static WebServer server(80);
static Preferences prefs;
static TaskHandle_t netTaskHandle = NULL;

static const byte DNS_PORT = 53;
static const char* PREFS_NAMESPACE = "deskflower_net";
static const uint32_t SYNC_INTERVAL_MS = 1800000; // 30 minutes

static const char PORTAL_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
  <title>Deskflower Setup</title>
  <style>
    * { box-sizing: border-box; margin: 0; padding: 0; font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; }
    body { background: #0f172a; color: #f8fafc; display: flex; justify-content: center; align-items: center; min-height: 100vh; padding: 20px; }
    .card { background: #1e293b; border: 1px solid #334155; border-radius: 16px; padding: 28px; width: 100%; max-width: 380px; box-shadow: 0 20px 25px -5px rgba(0, 0, 0, 0.5); }
    .header { text-align: center; margin-bottom: 24px; }
    .title { font-size: 22px; font-weight: 700; color: #38bdf8; letter-spacing: -0.5px; }
    .subtitle { font-size: 13px; color: #94a3b8; margin-top: 4px; }
    .form-group { margin-bottom: 16px; }
    label { display: block; font-size: 12px; font-weight: 600; text-transform: uppercase; letter-spacing: 0.5px; color: #94a3b8; margin-bottom: 6px; }
    input, select { width: 100%; padding: 12px 14px; background: #0f172a; border: 1px solid #334155; border-radius: 8px; color: #f8fafc; font-size: 14px; outline: none; transition: border-color 0.2s; }
    input:focus, select:focus { border-color: #38bdf8; }
    .btn { width: 100%; padding: 14px; background: #0284c7; border: none; border-radius: 8px; color: white; font-weight: 600; font-size: 15px; cursor: pointer; margin-top: 8px; transition: background 0.2s; }
    .btn:hover { background: #0369a1; }
    .footer { text-align: center; font-size: 11px; color: #64748b; margin-top: 20px; }
  </style>
</head>
<body>
  <div class="card">
    <div class="header">
      <div class="title">&#x1F338; Deskflower Setup</div>
      <div class="subtitle">Wi-Fi 6 &amp; Live Weather Configuration</div>
    </div>
    <form method="POST" action="/save">
      <div class="form-group">
        <label>Wi-Fi Network Name (SSID)</label>
        <input type="text" name="ssid" placeholder="Enter Wi-Fi SSID" required maxlength="32">
      </div>
      <div class="form-group">
        <label>Wi-Fi Password</label>
        <input type="password" name="password" placeholder="Enter Password" maxlength="64">
      </div>
      <div class="form-group">
        <label>Latitude</label>
        <input type="number" step="0.0001" name="lat" value="37.7749" required>
      </div>
      <div class="form-group">
        <label>Longitude</label>
        <input type="number" step="0.0001" name="lon" value="-122.4194" required>
      </div>
      <div class="form-group">
        <label>Timezone (GMT Offset Hours)</label>
        <select name="tz">
          <option value="-10">UTC -10:00 (Hawaii)</option>
          <option value="-8" selected>UTC -08:00 (PST / US Pacific)</option>
          <option value="-7">UTC -07:00 (MST / US Mountain)</option>
          <option value="-6">UTC -06:00 (CST / US Central)</option>
          <option value="-5">UTC -05:00 (EST / US Eastern)</option>
          <option value="0">UTC +00:00 (GMT / London)</option>
          <option value="1">UTC +01:00 (CET / Paris, Berlin)</option>
          <option value="8">UTC +08:00 (CST / Singapore, Tokyo -1)</option>
          <option value="9">UTC +09:00 (JST / Tokyo, Seoul)</option>
        </select>
      </div>
      <div class="form-group">
        <label>Plant Seed Code (Optional Import)</label>
        <input type="text" name="seed" placeholder="e.g. A3F8-K9M2-P4T6-W7Y1" maxlength="20">
      </div>
      <button type="submit" class="btn">Save &amp; Connect</button>
    </form>
    <div class="footer">Deskflower ESP32-C6 Botanical Companion</div>
  </div>
</body>
</html>
)rawliteral";

static const char SAVED_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Connected</title>
  <style>
    body { background: #0f172a; color: #f8fafc; font-family: sans-serif; display: flex; justify-content: center; align-items: center; height: 100vh; margin: 0; text-align: center; }
    .card { background: #1e293b; border-radius: 16px; padding: 32px; max-width: 320px; }
    h2 { color: #4ade80; margin-bottom: 12px; }
    p { color: #94a3b8; font-size: 14px; line-height: 1.5; }
  </style>
</head>
<body>
  <div class="card">
    <h2>&#x2728; Saved!</h2>
    <p>Deskflower is connecting to Wi-Fi. You can now close this window.</p>
  </div>
</body>
</html>
)rawliteral";

static void backgroundNetTask(void* parameter) {
    NetSyncManager* net = (NetSyncManager*)parameter;
    for (;;) {
        net->taskLoop();
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

NetSyncManager::NetSyncManager()
    : _state(NET_STATE_OFFLINE),
      _timeSynced(false),
      _syncRequested(false),
      _lastSyncTime(0),
      _lastConnectAttempt(0),
      _connectRetries(0),
      _routesReady(false) {
    memset(&_config, 0, sizeof(_config));
    memset(&_weather, 0, sizeof(_weather));
    _config.latitude = 37.7749f;
    _config.longitude = -122.4194f;
    _config.gmtOffsetSec = -28800; // -8h PST default
    _config.daylightOffsetSec = 3600;
    _config.configured = false;
}

void NetSyncManager::begin() {
    loadConfig();
    Dashboard.begin(server);
    registerHttpRoutes();

    // Dashboard handlers build JSON on this task. 12KB leaves room beside TLS.
    xTaskCreate(
        backgroundNetTask,
        "NetSyncTask",
        12288,
        this,
        1,
        &netTaskHandle
    );

    if (_config.configured && strlen(_config.ssid) > 0) {
        _state = NET_STATE_CONNECTING;
        _lastConnectAttempt = millis();
        connectWiFi();
    } else {
        _state = NET_STATE_OFFLINE;
    }
}

void NetSyncManager::loadConfig() {
    prefs.begin(PREFS_NAMESPACE, true);
    String ssidStr = prefs.getString("ssid", "");
    String passStr = prefs.getString("pass", "");
    _config.latitude = prefs.getFloat("lat", 37.7749f);
    _config.longitude = prefs.getFloat("lon", -122.4194f);
    _config.gmtOffsetSec = prefs.getInt("gmt_sec", -28800);
    _config.daylightOffsetSec = prefs.getInt("dst_sec", 3600);
    prefs.end();

    if (ssidStr.length() > 0) {
        strncpy(_config.ssid, ssidStr.c_str(), sizeof(_config.ssid) - 1);
        strncpy(_config.password, passStr.c_str(), sizeof(_config.password) - 1);
        _config.configured = true;
    } else {
        _config.configured = false;
    }
}

void NetSyncManager::saveConfig(const char* ssid, const char* pass, float lat, float lon, int32_t gmtOffset, int32_t daylightOffset) {
    strncpy(_config.ssid, ssid, sizeof(_config.ssid) - 1);
    strncpy(_config.password, pass, sizeof(_config.password) - 1);
    _config.latitude = lat;
    _config.longitude = lon;
    _config.gmtOffsetSec = gmtOffset;
    _config.daylightOffsetSec = daylightOffset;
    _config.configured = true;

    prefs.begin(PREFS_NAMESPACE, false);
    prefs.putString("ssid", _config.ssid);
    prefs.putString("pass", _config.password);
    prefs.putFloat("lat", _config.latitude);
    prefs.putFloat("lon", _config.longitude);
    prefs.putInt("gmt_sec", _config.gmtOffsetSec);
    prefs.putInt("dst_sec", _config.daylightOffsetSec);
    prefs.end();
}

static void sendCaptiveProbe() {
    if (NetSync.isPortalActive()) {
        server.send_P(200, "text/html", PORTAL_HTML);
    } else {
        server.send(204, "text/plain", "");
    }
}

void NetSyncManager::registerHttpRoutes() {
    if (_routesReady) return;
    _routesReady = true;

    server.on("/", HTTP_GET, []() {
        if (NetSync.isPortalActive()) {
            server.send_P(200, "text/html", PORTAL_HTML);
            return;
        }
        if (!NetSync.isConnected()) {
            server.send(503, "text/plain", "Deskflower is offline");
            return;
        }
        Dashboard.sendHome(server);
    });

    server.on("/save", HTTP_POST, [this]() {
        if (!NetSync.isPortalActive()) {
            server.send(404, "text/plain", "Not found");
            return;
        }
        String ssid = server.arg("ssid");
        String pass = server.arg("password");
        float lat = server.arg("lat").toFloat();
        float lon = server.arg("lon").toFloat();
        int tzHour = server.arg("tz").toInt();
        int32_t gmtSec = tzHour * 3600;
        String seedCode = server.arg("seed");

        saveConfig(ssid.c_str(), pass.c_str(), lat, lon, gmtSec, 0);

        if (seedCode.length() >= 16) {
            Plant.loadSeedCode(seedCode.c_str());
            Storage.markDirty();
        }

        server.send_P(200, "text/html", SAVED_HTML);

        vTaskDelay(pdMS_TO_TICKS(1000));
        stopCaptivePortal();
        _state = NET_STATE_CONNECTING;
        _lastConnectAttempt = millis();
        connectWiFi();
    });

    // Captive redirect hooks for mobile devices. On the LAN these stay empty
    // so a phone does not treat the terrarium as a sign-in gate.
    server.on("/generate_204", HTTP_GET, sendCaptiveProbe);
    server.on("/hotspot-detect.html", HTTP_GET, sendCaptiveProbe);
    server.on("/connectivitycheck.gstatic.com", HTTP_GET, sendCaptiveProbe);
    server.onNotFound([]() {
        if (NetSync.isPortalActive()) {
            server.sendHeader("Location", String("http://") + WiFi.softAPIP().toString(), true);
            server.send(302, "text/plain", "");
            return;
        }
        server.send(404, "text/plain", "Not found");
    });

    Dashboard.registerRoutes(server);
}

void NetSyncManager::startCaptivePortal() {
    Dashboard.stopStation();
    _state = NET_STATE_PORTAL_ACTIVE;
    WiFi.disconnect(true);
    WiFi.mode(WIFI_AP);
    WiFi.softAP("Deskflower-Setup");

    IPAddress apIP = WiFi.softAPIP();
    dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
    dnsServer.start(DNS_PORT, "*", apIP);

    registerHttpRoutes();
    server.begin();
}

void NetSyncManager::stopCaptivePortal() {
    server.stop();
    dnsServer.stop();
    WiFi.softAPdisconnect(true);
    // The hold-to-exit path used to leave the radio parked in portal state
    // after the AP was already gone, which blocked desk-mesh discovery.
    if (_state == NET_STATE_PORTAL_ACTIVE) {
        _state = NET_STATE_OFFLINE;
    }
}

void NetSyncManager::connectWiFi() {
    if (!_config.configured || strlen(_config.ssid) == 0) {
        _state = NET_STATE_OFFLINE;
        return;
    }
    WiFi.setHostname("deskflower");
    WiFi.mode(WIFI_STA);
    WiFi.begin(_config.ssid, _config.password);
}

void NetSyncManager::requestWeatherSync() {
    _syncRequested = true;
}

const char* NetSyncManager::getStateString() const {
    switch (_state) {
        case NET_STATE_OFFLINE: return "Offline (Simulation)";
        case NET_STATE_PORTAL_ACTIVE: return "Captive Portal Active";
        case NET_STATE_CONNECTING: return "Connecting Wi-Fi";
        case NET_STATE_CONNECTED: return "Connected";
        case NET_STATE_SYNCING: return "Syncing Weather/Time";
        default: return "Unknown";
    }
}

void NetSyncManager::taskLoop() {
    Mesh.service();

    uint32_t now = millis();

    // 1. Handle Captive Portal loop
    if (_state == NET_STATE_PORTAL_ACTIVE) {
        dnsServer.processNextRequest();
        server.handleClient();
        return;
    }

    // 2. Handle Wi-Fi Connection monitoring
    if (_state == NET_STATE_CONNECTING) {
        if (WiFi.status() == WL_CONNECTED) {
            _state = NET_STATE_CONNECTED;
            _connectRetries = 0;
            // Configure SNTP once connected
            configTime(_config.gmtOffsetSec, _config.daylightOffsetSec, "pool.ntp.org", "time.nist.gov", "time.google.com");
            _syncRequested = true; // Trigger immediate sync
        } else if (now - _lastConnectAttempt > 12000) {
            _connectRetries++;
            if (_connectRetries > 3) {
                // Fall back to offline simulation
                Dashboard.stopStation();
                _state = NET_STATE_OFFLINE;
            } else {
                _lastConnectAttempt = now;
                connectWiFi();
            }
        }
        return;
    }

    if (_state == NET_STATE_CONNECTED || _state == NET_STATE_SYNCING) {
        if (WiFi.status() != WL_CONNECTED) {
            Dashboard.stopStation();
            _state = NET_STATE_CONNECTING;
            _lastConnectAttempt = now;
            return;
        }

        Dashboard.ensureStation();
        server.handleClient();

        // Periodic or triggered sync
        if (_syncRequested || (now - _lastSyncTime >= SYNC_INTERVAL_MS) || _lastSyncTime == 0) {
            _syncRequested = false;
            _state = NET_STATE_SYNCING;
            fetchWeather();
            _lastSyncTime = millis();
            _state = NET_STATE_CONNECTED;
        }
    }
}

static bool isSnowOrIceCode(int wmoCode) {
    switch (wmoCode) {
        case 56: case 57:             // Freezing drizzle
        case 66: case 67:             // Freezing rain
        case 71: case 73: case 75:     // Snow fall
        case 77:                      // Snow grains
        case 85: case 86:             // Snow showers
            return true;
        default:
            return false;
    }
}

static bool isLiquidPrecipCode(int wmoCode) {
    switch (wmoCode) {
        case 51: case 53: case 55:     // Drizzle
        case 61: case 63: case 65:     // Rain
        case 80: case 81: case 82:     // Rain showers
        case 95: case 96: case 99:     // Thunderstorm, with or without hail
            return true;
        default:
            return false;
    }
}

static uint8_t intensityFromWmo(int wmoCode) {
    switch (wmoCode) {
        case 51: case 56: case 61: case 66: case 71: case 77: case 80: case 85:
            return 1;
        case 53: case 63: case 73: case 81:
            return 2;
        case 55: case 57: case 65: case 67: case 75: case 82: case 86:
        case 95: case 96: case 99:
            return 3;
        default:
            return 0;
    }
}

static uint8_t intensityFromAmount(float precipMm, float snowfallCm) {
    if (snowfallCm >= 1.0f || precipMm >= 2.5f) return 3;
    if (snowfallCm >= 0.3f || precipMm >= 0.5f) return 2;
    if (snowfallCm > 0.0f || precipMm > 0.0f) return 1;
    return 0;
}

static uint8_t classifyPrecipIntensity(int wmoCode, float precipMm, float snowfallCm) {
    uint8_t fromCode = intensityFromWmo(wmoCode);
    uint8_t fromAmount = intensityFromAmount(precipMm, snowfallCm);
    return (fromCode > fromAmount) ? fromCode : fromAmount;
}

WeatherType NetSyncManager::mapWmoToWeatherType(int wmoCode, bool isDay, float snowfallCm, float precipMm) {
    if (snowfallCm < 0.0f) snowfallCm = 0.0f;
    if (precipMm < 0.0f) precipMm = 0.0f;

    // Snowfall amount or a freezing/snow WMO code. Never drawn as rain.
    if (isSnowOrIceCode(wmoCode) || snowfallCm > 0.0f) {
        return WEATHER_SNOW;
    }

    // Fog and depositing rime, unless measurable liquid precip is also falling.
    if ((wmoCode == 45 || wmoCode == 48) && precipMm < 0.5f) {
        return WEATHER_FOG;
    }

    if (isLiquidPrecipCode(wmoCode) || precipMm > 0.0f) {
        return WEATHER_RAIN;
    }

    if (wmoCode == 0) {
        return isDay ? WEATHER_SUN : WEATHER_MOON;
    }
    if (wmoCode <= 2) {
        return WEATHER_PARTLY_CLOUDY;
    }
    return WEATHER_CLOUDY;
}

void NetSyncManager::parseWeatherJson(const char* json) {
    if (!json) return;

    // Fast zero-allocation JSON extraction for Open-Meteo payload:
    // "current":{"time":"...","temperature_2m":68.4,"relative_humidity_2m":55,
    //            "precipitation":0.2,"snowfall":0.0,"is_day":1,"weather_code":1}
    const char* cur = strstr(json, "\"current\":");
    if (!cur) return;

    // 1. Extract temperature_2m
    const char* tempKey = strstr(cur, "\"temperature_2m\":");
    if (tempKey) {
        float tempVal = atof(tempKey + 17);
        _weather.temperatureF = (int16_t)roundf(tempVal);
    }

    // 2. Extract relative_humidity_2m
    const char* humKey = strstr(cur, "\"relative_humidity_2m\":");
    if (humKey) {
        int humVal = atoi(humKey + 23);
        if (humVal >= 0 && humVal <= 100) {
            _weather.humidity = (uint8_t)humVal;
        }
    }

    // 3. Extract is_day
    const char* dayKey = strstr(cur, "\"is_day\":");
    if (dayKey) {
        int dayVal = atoi(dayKey + 9);
        _weather.isDay = (dayVal != 0);
    }

    // 4. Precipitation amount (mm over the preceding hour) and snowfall (cm).
    float precipMm = 0.0f;
    float snowfallCm = 0.0f;
    const char* precipKey = strstr(cur, "\"precipitation\":");
    if (precipKey) {
        precipMm = atof(precipKey + 16);
        if (precipMm < 0.0f) precipMm = 0.0f;
    }
    const char* snowKey = strstr(cur, "\"snowfall\":");
    if (snowKey) {
        snowfallCm = atof(snowKey + 11);
        if (snowfallCm < 0.0f) snowfallCm = 0.0f;
    }

    // 5. Extract weather_code and map rain / snow / fog from the live reading.
    const char* codeKey = strstr(cur, "\"weather_code\":");
    if (codeKey || precipKey || snowKey) {
        int wmoCode = codeKey ? atoi(codeKey + 15) : 0;
        WeatherType mapped = mapWmoToWeatherType(wmoCode, _weather.isDay, snowfallCm, precipMm);
        uint8_t intensity = classifyPrecipIntensity(wmoCode, precipMm, snowfallCm);
        if ((mapped == WEATHER_RAIN || mapped == WEATHER_SNOW) && intensity == 0) {
            intensity = 1;
        }
        if (mapped != WEATHER_RAIN && mapped != WEATHER_SNOW) {
            intensity = 0;
        }
        _weather.weatherType = mapped;
        _weather.precipIntensity = intensity;
    }

    _weather.valid = true;
    _weather.lastSyncMillis = millis();
}

bool NetSyncManager::fetchWeather() {
    WiFiClientSecure client;
    client.setInsecure(); // Non-blocking lightweight TLS handshake without certificate chain bundle overhead

    HTTPClient http;
    char url[256];
    snprintf(url, sizeof(url),
             "https://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f&current=temperature_2m,relative_humidity_2m,precipitation,snowfall,weather_code,is_day&temperature_unit=fahrenheit",
             _config.latitude, _config.longitude);

    http.begin(client, url);
    http.setTimeout(8000);

    int httpCode = http.GET();
    bool success = false;
    if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        parseWeatherJson(payload.c_str());
        success = true;
    }
    http.end();
    return success;
}

void NetSyncManager::update(uint32_t deltaMs) {
    // Check SNTP synchronized local time
    time_t nowTime;
    time(&nowTime);
    struct tm timeinfo;
    if (localtime_r(&nowTime, &timeinfo) && timeinfo.tm_year > (2020 - 1900)) {
        _timeSynced = true;
        // Update HUD with live local wall clock
        HUD.setTime(timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec, false);
        HUD.setCalendar((uint16_t)(timeinfo.tm_year + 1900),
                        (uint8_t)(timeinfo.tm_mon + 1),
                        (uint8_t)timeinfo.tm_mday);
    }

    // Update HUD with live weather if synced
    if (_weather.valid) {
        HUD.setWeather(_weather.weatherType, _weather.temperatureF, _weather.humidity, _weather.precipIntensity);
    }

    Dashboard.sample(millis());
}
