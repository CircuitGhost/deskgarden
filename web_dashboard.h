#ifndef WEB_DASHBOARD_H
#define WEB_DASHBOARD_H

#include <Arduino.h>
#include <WebServer.h>

// Station-mode page at http://deskflower.local. The captive setup portal
// keeps "/", "/save", and the connectivity-check paths while it is active.

class WebDashboard {
public:
    WebDashboard();

    void begin(WebServer& server);
    void registerRoutes(WebServer& server);
    void sendHome(WebServer& server) const;

    // mDNS + HTTP once the station link is up. Portal start calls stopStation().
    void ensureStation();
    void stopStation();

    // Fixed ring of hydration and growth samples. No heap use.
    void sample(uint32_t nowMs);

    uint8_t backlightFor(uint8_t hour, uint8_t minute) const;
    uint8_t quietStart() const { return _quietStart; }
    uint8_t quietEnd() const { return _quietEnd; }

private:
    static const uint8_t HIST_LEN = 48;

    struct Sample {
        uint8_t moisture;
        uint8_t growth;
    };

    WebServer* _server;
    Sample _hist[HIST_LEN];
    uint8_t _histHead;
    uint8_t _histCount;
    uint32_t _lastSampleMs;
    char _name[25];
    uint8_t _palette;     // 0-4, or 255 when the genome's own palette is in use
    uint8_t _quietStart;  // hour, inclusive
    uint8_t _quietEnd;    // hour, exclusive
    bool _stationHttp;
    bool _mdns;

    bool rejectIfClosed();
    void loadPrefs();
    void savePrefs();
    void sendTelemetry();
    void sendSeeds();
    void sendExport();
    void importSeed();
    void saveSettings();
};

extern WebDashboard Dashboard;

#endif // WEB_DASHBOARD_H
