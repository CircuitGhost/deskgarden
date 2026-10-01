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
    PARTICLE_MIST_DROP,         // Rain/Humid: drifting condensation droplet
    PARTICLE_RAIN_CASCADE       // User interaction: falling raindrop burst
};

struct Particle {
    int16_t x;          // Sub-pixel position (x * 16)
    int16_t y;          // Sub-pixel position (y * 16)
    int16_t vx;         // Sub-pixel velocity per update (vx * 16)
    int16_t vy;         // Sub-pixel velocity per update (vy * 16)
    uint8_t life;       // Remaining lifespan frames
    uint8_t maxLife;    // Total lifespan frames
    uint8_t type;       // ParticleType
    uint8_t size;       // 1 = 1x1 pixel, 2 = 2x2, 3 = cross glow
    uint8_t phase;      // Sine phase for pulsing/flutter
    bool active;
};

class ParticleSystem {
public:
    ParticleSystem();
    void begin();
    
    // Core simulation tick
    void update(uint32_t deltaMs, TimePhase phase, WeatherType weather);
    
    // User interaction: Watering cascade
    void triggerWateringCascade(uint8_t count = 20);
    
    // Full Frame Renderer
    void render(uint16_t* buffer, int16_t screenWidth = SCREEN_WIDTH, int16_t screenHeight = SCREEN_HEIGHT);

    uint8_t getActiveCount() const;

private:
    Particle _pool[MAX_PARTICLES];
    uint32_t _lastSpawnTime;
    uint8_t _globalPhase;

    void spawnAmbient(TimePhase phase, WeatherType weather);
    int8_t findFreeSlot();
};

extern ParticleSystem Particles;

#endif // PARTICLE_SYSTEM_H
