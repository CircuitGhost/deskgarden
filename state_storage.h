#ifndef STATE_STORAGE_H
#define STATE_STORAGE_H

#include <Arduino.h>
#include "config.h"

#define STATE_MAGIC   0x44464C57  // "DFLW"
#define STATE_VERSION 1

struct PlantStateData {
    uint32_t magic;             // Magic validation header
    uint8_t version;            // Version byte
    uint8_t generation;         // Generation index (1 - 255)
    uint8_t phenotype;          // PhenotypeFamily enum
    char seedCode[20];          // 16-char Crockford Base32 Seed Code
    uint32_t genomeSeed;        // Deterministic PRNG seed
    float growthProgress;       // Growth stage (0.0 to 1.0)
    float moisturePct;          // Soil moisture (0.0 to 100.0%)
    uint8_t moistureState;      // MoistureState enum
    bool use24Hour;             // 12h or 24h clock format
    bool useCelsius;            // Temperature unit preference
    uint8_t brightness;         // Backlight PWM level (0 - 255)
    uint32_t totalWaterings;    // Lifetime watering count
    uint32_t lifetimeMinutes;   // Cumulative device uptime in minutes
    uint32_t checksum;          // CRC32 checksum over preceding struct bytes
};

class StateStorage {
public:
    StateStorage();
    bool begin();

    // State Load & Save
    bool load();
    bool saveImmediate();
    void markDirty();
    void update(uint32_t deltaMs);

    // Getters & Telemetry
    const PlantStateData& getState() const { return _data; }
    uint32_t getTotalWaterings() const { return _data.totalWaterings; }
    uint32_t getLifetimeMinutes() const { return _data.lifetimeMinutes; }
    void incrementWaterings() { _data.totalWaterings++; markDirty(); }
    bool isMounted() const { return _isMounted; }

private:
    PlantStateData _data;
    bool _isMounted;
    bool _isDirty;
    uint32_t _dirtyTime;
    uint32_t _lastPeriodicSave;
    uint32_t _minuteAccumulatorMs;

    uint32_t calculateChecksum(const PlantStateData& state);
    void populateCurrentState();
    void applyLoadedState();
};

extern StateStorage Storage;

#endif // STATE_STORAGE_H
