#include "state_storage.h"
#include <LittleFS.h>
#include "plant_engine.h"
#include "moisture_system.h"
#include "hud_manager.h"
#include "mesh_sync.h"

StateStorage Storage;

static const char* STATE_FILE = "/state.bin";
static const char* TEMP_FILE  = "/state.tmp";
static const uint32_t DEBOUNCE_SAVE_DELAY_MS = 4000;      // Debounce 4 seconds
static const uint32_t PERIODIC_SAVE_INTERVAL_MS = 600000; // Backup save every 10 minutes

// Standard tableless CRC32 calculation
static uint32_t crc32_buffer(const uint8_t* data, size_t length) {
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xEDB88320;
            } else {
                crc >>= 1;
            }
        }
    }
    return ~crc;
}

StateStorage::StateStorage()
    : _isMounted(false),
      _isDirty(false),
      _dirtyTime(0),
      _lastPeriodicSave(0),
      _minuteAccumulatorMs(0) {
    memset(&_data, 0, sizeof(_data));
    _data.magic = STATE_MAGIC;
    _data.version = STATE_VERSION;
    _data.generation = 1;
    _data.phenotype = 0;
    _data.genomeSeed = 0x5A5A1234;
    _data.growthProgress = 1.0f;
    _data.moisturePct = 55.0f;
    _data.moistureState = 1; // BALANCED
    _data.use24Hour = false;
    _data.useCelsius = false;
    _data.brightness = DEFAULT_BRIGHTNESS;
    _data.totalWaterings = 0;
    _data.lifetimeMinutes = 0;
}

uint32_t StateStorage::calculateChecksum(const PlantStateData& state) {
    // Checksum over everything except the checksum field itself
    size_t dataLen = sizeof(PlantStateData) - sizeof(uint32_t);
    return crc32_buffer((const uint8_t*)&state, dataLen);
}

bool StateStorage::begin() {
    _isMounted = LittleFS.begin(true); // Auto-format on first boot if needed
    if (!_isMounted) {
        #if ENABLE_SERIAL_LOG
        Serial.println("[STORAGE] ERROR: LittleFS mount failed!");
        #endif
        return false;
    }

    #if ENABLE_SERIAL_LOG
    Serial.printf("[STORAGE] LittleFS mounted successfully. Total: %u B, Used: %u B\n",
                  LittleFS.totalBytes(), LittleFS.usedBytes());
    #endif

    // Attempt to load existing state
    if (!load()) {
        #if ENABLE_SERIAL_LOG
        Serial.println("[STORAGE] No valid state file found. Creating baseline state.");
        #endif
        saveImmediate();
    }

    _lastPeriodicSave = millis();
    return true;
}

void StateStorage::populateCurrentState() {
    _data.magic = STATE_MAGIC;
    _data.version = STATE_VERSION;
    _data.generation = HUD.getGeneration();
    _data.phenotype = Plant.getGenome().phenotype;
    Plant.getSeedCode(_data.seedCode);
    _data.genomeSeed = Plant.getGenome().seed;
    _data.growthProgress = Plant.getGrowthProgress();
    _data.moisturePct = Moisture.getMoisture();
    _data.moistureState = (uint8_t)Moisture.getState();
    _data.use24Hour = HUD.getUse24Hour();
    _data.useCelsius = false;
    _data.brightness = DEFAULT_BRIGHTNESS;
    _data.checksum = calculateChecksum(_data);
}

void StateStorage::applyLoadedState() {
    // 1. Restore Generation Index
    if (_data.generation > 0) {
        HUD.setGeneration(_data.generation);
    }

    // 2. Restore Botanical Specimen from Seed Code or Seed
    if (strlen(_data.seedCode) >= 16) {
        Plant.loadSeedCode(_data.seedCode);
    } else {
        Plant.generateFromSeed(_data.genomeSeed, _data.phenotype);
    }

    // 3. Restore Growth Progress
    Plant.setGrowthProgress(_data.growthProgress);

    // 4. Restore Moisture Hydration
    Moisture.setMoisture(_data.moisturePct);

    // 5. Restore UI preferences
    HUD.setUse24Hour(_data.use24Hour);
}

bool StateStorage::load() {
    if (!_isMounted) return false;

    if (!LittleFS.exists(STATE_FILE)) {
        return false;
    }

    File f = LittleFS.open(STATE_FILE, "r");
    if (!f) {
        #if ENABLE_SERIAL_LOG
        Serial.println("[STORAGE] Failed to open state file for reading.");
        #endif
        return false;
    }

    if (f.size() != sizeof(PlantStateData)) {
        #if ENABLE_SERIAL_LOG
        Serial.printf("[STORAGE] Corrupt state file size mismatch: %u (expected %u)\n", 
                      (uint32_t)f.size(), sizeof(PlantStateData));
        #endif
        f.close();
        return false;
    }

    PlantStateData tempState;
    size_t bytesRead = f.read((uint8_t*)&tempState, sizeof(PlantStateData));
    f.close();

    if (bytesRead != sizeof(PlantStateData)) {
        return false;
    }

    // Validate magic header
    if (tempState.magic != STATE_MAGIC || tempState.version != STATE_VERSION) {
        #if ENABLE_SERIAL_LOG
        Serial.println("[STORAGE] State header magic or version mismatch.");
        #endif
        return false;
    }

    // Validate CRC32 checksum
    uint32_t expectedCrc = calculateChecksum(tempState);
    if (tempState.checksum != expectedCrc) {
        #if ENABLE_SERIAL_LOG
        Serial.printf("[STORAGE] Checksum failed: calc 0x%08X != stored 0x%08X\n", 
                      expectedCrc, tempState.checksum);
        #endif
        return false;
    }

    // Valid state loaded
    _data = tempState;
    applyLoadedState();

    #if ENABLE_SERIAL_LOG
    Serial.printf("[STORAGE] Plant state restored: Gen %u | %s | Code: %s | Moisture: %.0f%% | Waterings: %u | Lifetime: %u min\n",
                  _data.generation, Plant.getPhenotypeName(), _data.seedCode,
                  _data.moisturePct, _data.totalWaterings, _data.lifetimeMinutes);
    #endif

    return true;
}

bool StateStorage::saveImmediate() {
    if (!_isMounted) return false;

    populateCurrentState();
    meshFsLock();

    // Atomic save pattern: Write to temporary file, flush, then rename
    File f = LittleFS.open(TEMP_FILE, "w");
    if (!f) {
        #if ENABLE_SERIAL_LOG
        Serial.println("[STORAGE] Failed to open temporary state file for writing.");
        #endif
        meshFsUnlock();
        return false;
    }

    size_t written = f.write((const uint8_t*)&_data, sizeof(PlantStateData));
    f.flush();
    f.close();

    if (written != sizeof(PlantStateData)) {
        #if ENABLE_SERIAL_LOG
        Serial.println("[STORAGE] Incomplete state write.");
        #endif
        LittleFS.remove(TEMP_FILE);
        meshFsUnlock();
        return false;
    }

    // Atomic replace
    if (LittleFS.exists(STATE_FILE)) {
        LittleFS.remove(STATE_FILE);
    }
    bool renameOk = LittleFS.rename(TEMP_FILE, STATE_FILE);

    if (renameOk) {
        _isDirty = false;
        #if ENABLE_SERIAL_LOG
        Serial.printf("[STORAGE] State persisted atomically. Gen: %u | Moisture: %.0f%% | CRC: 0x%08X\n",
                      _data.generation, _data.moisturePct, _data.checksum);
        #endif
    } else {
        #if ENABLE_SERIAL_LOG
        Serial.println("[STORAGE] ERROR: Atomic rename failed!");
        #endif
    }

    meshFsUnlock();
    return renameOk;
}

void StateStorage::markDirty() {
    _isDirty = true;
    _dirtyTime = millis();
}

void StateStorage::update(uint32_t deltaMs) {
    uint32_t now = millis();

    // Lifetime minutes accumulator
    _minuteAccumulatorMs += deltaMs;
    if (_minuteAccumulatorMs >= 60000) {
        _minuteAccumulatorMs -= 60000;
        _data.lifetimeMinutes++;
        _isDirty = true; // Flag for periodic save
    }

    // Debounced save
    if (_isDirty && (now - _dirtyTime >= DEBOUNCE_SAVE_DELAY_MS)) {
        saveImmediate();
    }

    // Periodic backup save (every 10 minutes)
    if (now - _lastPeriodicSave >= PERIODIC_SAVE_INTERVAL_MS) {
        _lastPeriodicSave = now;
        if (_isDirty) {
            saveImmediate();
        }
    }
}
