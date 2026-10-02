#ifndef NET_SYNC_H
#define NET_SYNC_H

#include <Arduino.h>
#include "config.h"
#include "weather_glyphs.h"

enum NetState : uint8_t {
    NET_STATE_OFFLINE = 0,
    NET_STATE_PORTAL_ACTIVE,
    NET_STATE_CONNECTING,
    NET_STATE_CONNECTED,
    NET_STATE_SYNCING
};

struct NetConfig {
    char ssid[33];
    char password[65];
    float latitude;
    float longitude;
    int32_t gmtOffsetSec;
    int32_t daylightOffsetSec;
    bool configured;
};

struct WeatherReport {
    bool valid;
    WeatherType weatherType;
    int16_t temperatureF;
    uint8_t humidity;
    bool isDay;
    uint32_t lastSyncMillis;
};

class NetSyncManager {
public:
    NetSyncManager();
    void begin();
    void update(uint32_t deltaMs);

    // Captive Portal & Configuration
    void startCaptivePortal();
    void stopCaptivePortal();
    bool isPortalActive() const { return _state == NET_STATE_PORTAL_ACTIVE; }
    void saveConfig(const char* ssid, const char* pass, float lat, float lon, int32_t gmtOffset, int32_t daylightOffset);
    void loadConfig();

    // State & Status
    NetState getState() const { return _state; }
    const char* getStateString() const;
    bool isConnected() const { return _state == NET_STATE_CONNECTED || _state == NET_STATE_SYNCING; }
    bool isTimeSynced() const { return _timeSynced; }
    
    // Telemetry & Weather
    const WeatherReport& getWeather() const { return _weather; }
    const NetConfig& getConfig() const { return _config; }

    // Manual sync trigger
    void requestWeatherSync();

    // Background task entrypoint
    void taskLoop();

private:
    NetConfig _config;
    WeatherReport _weather;
    volatile NetState _state;
    volatile bool _timeSynced;
    volatile bool _syncRequested;

    uint32_t _lastSyncTime;
    uint32_t _lastConnectAttempt;
    uint8_t _connectRetries;

    void connectWiFi();
    bool fetchWeather();
    void parseWeatherJson(const char* json);
    WeatherType mapWmoToWeatherType(int wmoCode, bool isDay);
};

extern NetSyncManager NetSync;

#endif // NET_SYNC_H
