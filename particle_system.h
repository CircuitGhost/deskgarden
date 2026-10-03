#ifndef PARTICLE_SYSTEM_H
#define PARTICLE_SYSTEM_H

#include <Arduino.h>
#include "config.h"
#include "hal_display.h"
#include "time_atmosphere.h"
#include "hud_manager.h"

#define MAX_PARTICLES       48

enum ParticleType : uint8_t {
    PARTICLE_NONE = 0,
    PARTICLE_OXYGEN_MOTE,       // Daylight: rising translucent oxygen bubble
    PARTICLE_FIREFLY,           // Dusk/Night: wandering bioluminescent mote
    PARTICLE_MIST_DROP,         // Rain mist or fog: condensation droplet
    PARTICLE_RAIN_CASCADE,      // User interaction: falling raindrop burst
    PARTICLE_POLLEN_MOTE,       // Daylight/golden hour: golden mote from a mature flower
    PARTICLE_SEED_MOTE,         // Pollination: fertile seed falling into the soil
    PARTICLE_RAIN_STREAK,       // Live rain: fast diagonal streak
    PARTICLE_SNOWFLAKE,         // Live snow: slow crystalline mote
    PARTICLE_NOCTURNAL_SPORE,   // 23:00–05:00: softly glowing spore mote
    PARTICLE_GOLDEN_SPARKLE,    // Brief golden cross where pollen meets a neighbor
    PARTICLE_FALLING_LEAF,      // Autumn: russet, ochre, or crimson leaf on a gust
    PARTICLE_BLOSSOM_PETAL,     // Spring: sakura petal in the shower
    PARTICLE_LADYBUG,           // 4×4 beetle climbing stems and perching on leaves
    PARTICLE_NECTAR_VISITOR,    // 6×6 hummingbird or pygmy moth at a flower
    PARTICLE_LOAM_SNAIL         // Slow crawl across the loam strata
};

struct Particle {
    int16_t x;          // Sub-pixel position (x * 16)
    int16_t y;          // Sub-pixel position (y * 16)
    int16_t vx;         // Sub-pixel velocity per update (vx * 16)
    int16_t vy;         // Sub-pixel velocity per update (vy * 16)
    uint16_t life;      // Remaining lifespan frames (~60 Hz sim tick)
    uint16_t maxLife;   // Total lifespan frames
    uint8_t type;       // ParticleType
    uint8_t size;       // 1 = 1x1 pixel, 2 = 2x2, 3 = cross glow
    uint8_t phase;      // Sine phase for pulsing/flutter
    uint8_t tag;        // Pollen: origin flower index. Unused by other types.
    bool active;
};

class ParticleSystem {
public:
    ParticleSystem();
    void begin();
    
    // Core simulation tick
    void update(uint32_t deltaMs, TimePhase phase, WeatherType weather,
                uint8_t humidityPct = 0, uint8_t precipIntensity = 0,
                uint8_t hour = 12, uint8_t minute = 0,
                uint8_t month = 0, uint8_t day = 1);
    
    // User interaction: Watering cascade
    void triggerWateringCascade(uint8_t count = 20);
    
    // Full Frame Renderer
    void render(uint16_t* buffer, int16_t screenWidth = SCREEN_WIDTH, int16_t screenHeight = SCREEN_HEIGHT);

    uint8_t getActiveCount() const;

private:
    Particle _pool[MAX_PARTICLES];
    uint32_t _lastSpawnTime;
    uint32_t _lastSporeSpawn;
    uint32_t _lastPollination;
    uint32_t _lastLeafSpawn;
    uint32_t _lastPetalSpawn;
    uint32_t _lastBubbleSpawn;
    uint32_t _lastLadybugSpawn;
    uint32_t _lastVisitorSpawn;
    uint32_t _lastSnailSpawn;
    uint8_t _globalPhase;

    void spawnAmbient(TimePhase phase, WeatherType weather, uint8_t humidityPct, uint8_t precipIntensity);
    void spawnSeasonal(uint32_t now, TimePhase phase, WeatherType weather,
                       uint8_t humidityPct, uint8_t month, uint8_t day);
    void spawnFauna(uint32_t now, TimePhase phase);
    bool hasActive(uint8_t type) const;
    int8_t findFreeSlot();
    void tryCrossPollinate(Particle& mote, uint32_t now);
};

extern ParticleSystem Particles;

#endif // PARTICLE_SYSTEM_H
