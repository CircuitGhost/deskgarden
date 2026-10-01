#include "particle_system.h"

ParticleSystem Particles;

// Precomputed 32-step sine table for integer math on RISC-V (values -127 to +127)
static const int8_t SIN_TABLE_32[32] = {
    0, 24, 48, 70, 89, 105, 117, 124, 
    127, 124, 117, 105, 89, 70, 48, 24, 
    0, -24, -48, -70, -89, -105, -117, -124, 
    -127, -124, -117, -105, -89, -70, -48, -24
};

static inline int8_t fastSin(uint8_t phase) {
    return SIN_TABLE_32[phase & 31];
}

ParticleSystem::ParticleSystem()
    : _lastSpawnTime(0),
      _globalPhase(0) {
    for (uint8_t i = 0; i < MAX_PARTICLES; i++) {
        _pool[i].active = false;
    }
}

void ParticleSystem::begin() {
    for (uint8_t i = 0; i < MAX_PARTICLES; i++) {
        _pool[i].active = false;
    }
    _lastSpawnTime = millis();
    _globalPhase = 0;
}

int8_t ParticleSystem::findFreeSlot() {
    for (uint8_t i = 0; i < MAX_PARTICLES; i++) {
        if (!_pool[i].active) return i;
    }
    return -1;
}

uint8_t ParticleSystem::getActiveCount() const {
    uint8_t count = 0;
    for (uint8_t i = 0; i < MAX_PARTICLES; i++) {
        if (_pool[i].active) count++;
    }
    return count;
}

void ParticleSystem::spawnAmbient(TimePhase phase, WeatherType weather) {
    int8_t slot = findFreeSlot();
    if (slot < 0) return;

    Particle& p = _pool[slot];

    // Spawning behavior depends on diurnal cycle and weather
    if (weather == WEATHER_RAIN) {
        // Rain / Mist particle: falling from top of canopy
        p.x = (rand() % (SCREEN_WIDTH - 8) + 4) << 4;
        p.y = (HUD_HEIGHT + 2) << 4;
        p.vx = (rand() % 9 - 4); // subtle wind drift
        p.vy = (rand() % 16 + 24); // fast downward speed
        p.life = rand() % 30 + 35;
        p.maxLife = p.life;
        p.type = PARTICLE_MIST_DROP;
        p.size = (rand() % 2 == 0) ? 1 : 2;
        p.phase = rand() % 32;
        p.active = true;
    } 
    else if (phase == PHASE_NIGHT || phase == PHASE_DUSK) {
        // Firefly: drifting in canopy space with sinusoidal glow
        p.x = (rand() % (SCREEN_WIDTH - 20) + 10) << 4;
        p.y = (HUD_HEIGHT + (rand() % (CANOPY_HEIGHT - 30)) + 15) << 4;
        p.vx = (rand() % 9 - 4);
        p.vy = (rand() % 9 - 4);
        p.life = rand() % 60 + 80;
        p.maxLife = p.life;
        p.type = PARTICLE_FIREFLY;
        p.size = (rand() % 3 == 0) ? 3 : 2; // cross glow or dot
        p.phase = rand() % 32;
        p.active = true;
    } 
    else {
        // Daytime / Dawn / Golden Hour: Oxygen bubble rising from substrate/foliage
        p.x = (rand() % (SCREEN_WIDTH - 30) + 15) << 4;
        p.y = (SCREEN_HEIGHT - SUBSTRATE_HEIGHT - 5 - (rand() % 30)) << 4;
        p.vx = (rand() % 5 - 2);
        p.vy = -(rand() % 8 + 8); // steady float upward
        p.life = rand() % 50 + 60;
        p.maxLife = p.life;
        p.type = PARTICLE_OXYGEN_MOTE;
        p.size = (rand() % 2 == 0) ? 1 : 2;
        p.phase = rand() % 32;
        p.active = true;
    }
}

void ParticleSystem::triggerWateringCascade(uint8_t count) {
    for (uint8_t i = 0; i < count; i++) {
        int8_t slot = findFreeSlot();
        if (slot < 0) break;

        Particle& p = _pool[slot];
        p.x = (rand() % (SCREEN_WIDTH - 12) + 6) << 4;
        p.y = (HUD_HEIGHT + 2 + (rand() % 16)) << 4;
        p.vx = (rand() % 11 - 5);
        p.vy = (rand() % 24 + 32); // brisk shower cascade
        p.life = rand() % 25 + 25;
        p.maxLife = p.life;
        p.type = PARTICLE_RAIN_CASCADE;
        p.size = (rand() % 2 == 0) ? 2 : 1;
        p.phase = rand() % 32;
        p.active = true;
    }
}

void ParticleSystem::update(uint32_t deltaMs, TimePhase phase, WeatherType weather) {
    _globalPhase++;
    uint32_t now = millis();

    // Spawn ambient particle at intervals (e.g. every 220ms)
    uint32_t spawnInterval = (weather == WEATHER_RAIN) ? 120 : 250;
    if (now - _lastSpawnTime >= spawnInterval) {
        spawnAmbient(phase, weather);
        _lastSpawnTime = now;
    }

    // Update active particles
    for (uint8_t i = 0; i < MAX_PARTICLES; i++) {
        Particle& p = _pool[i];
        if (!p.active) continue;

        if (p.life == 0) {
            p.active = false;
            continue;
        }
        p.life--;

        p.phase = (p.phase + 1) & 31;

        // Position integration
        if (p.type == PARTICLE_FIREFLY) {
            // Organic wandering flutter
            int16_t wanderX = fastSin(p.phase + i) >> 5;
            int16_t wanderY = fastSin((p.phase * 2) + i) >> 5;
            p.x += (p.vx + wanderX);
            p.y += (p.vy + wanderY);
        } else if (p.type == PARTICLE_OXYGEN_MOTE) {
            // Gentle side-to-side harmonic drift while rising
            int16_t waveX = fastSin(p.phase) >> 4;
            p.x += (p.vx + waveX);
            p.y += p.vy;
        } else {
            p.x += p.vx;
            p.y += p.vy;
        }

        // Screen boundary checks (Canopy space: HUD_HEIGHT to SCREEN_HEIGHT - 2)
        int16_t px = p.x >> 4;
        int16_t py = p.y >> 4;

        if (px < 0 || px >= SCREEN_WIDTH || py < HUD_HEIGHT || py >= (SCREEN_HEIGHT - 2)) {
            p.active = false;
        }
    }
}

void ParticleSystem::renderBand(uint16_t* buffer, int16_t bandGlobalY0, int16_t bandHeight, int16_t screenWidth) {
    if (!buffer) return;
    int16_t bandGlobalY1 = bandGlobalY0 + bandHeight;

    for (uint8_t i = 0; i < MAX_PARTICLES; i++) {
        Particle& p = _pool[i];
        if (!p.active) continue;

        int16_t px = p.x >> 4;
        int16_t py = p.y >> 4;

        // Bounding box test against current band
        if (py + 2 < bandGlobalY0 || py - 2 >= bandGlobalY1) continue;

        // Determine particle color & brightness
        uint16_t color = COLOR_WHITE;

        if (p.type == PARTICLE_FIREFLY) {
            // Sine breathing pulse (amber-gold / neon yellow-green)
            int8_t pulse = fastSin(p.phase); // -127 to +127
            uint8_t brightness = (pulse > 0) ? (140 + (pulse >> 1)) : (80 + (pulse >> 2));
            color = rgb565(brightness, (uint8_t)(brightness * 0.95f), (uint8_t)(brightness * 0.2f));

            int16_t localY = py - bandGlobalY0;
            if (localY >= 0 && localY < bandHeight && px >= 0 && px < screenWidth) {
                buffer[localY * screenWidth + px] = color;
            }

            if (p.size >= 3) {
                // Glow halo cross
                uint16_t dimGlow = rgb565(brightness >> 2, (uint8_t)((brightness * 0.95f) / 4), (uint8_t)((brightness * 0.2f) / 4));
                if (localY - 1 >= 0 && localY - 1 < bandHeight) buffer[(localY - 1) * screenWidth + px] = dimGlow;
                if (localY + 1 >= 0 && localY + 1 < bandHeight) buffer[(localY + 1) * screenWidth + px] = dimGlow;
                if (px - 1 >= 0) buffer[localY * screenWidth + (px - 1)] = dimGlow;
                if (px + 1 < screenWidth) buffer[localY * screenWidth + (px + 1)] = dimGlow;
            }
        } 
        else if (p.type == PARTICLE_OXYGEN_MOTE) {
            // Translucent cyan-tinted bubble
            color = rgb565(210, 245, 255);
            int16_t localY = py - bandGlobalY0;
            if (localY >= 0 && localY < bandHeight && px >= 0 && px < screenWidth) {
                buffer[localY * screenWidth + px] = color;
            }
            if (p.size >= 2 && px + 1 < screenWidth && localY + 1 < bandHeight && localY + 1 >= 0) {
                buffer[(localY + 1) * screenWidth + (px + 1)] = rgb565(140, 210, 240);
            }
        } 
        else if (p.type == PARTICLE_RAIN_CASCADE || p.type == PARTICLE_MIST_DROP) {
            // Rain streak (pure crystalline azure)
            color = (p.type == PARTICLE_RAIN_CASCADE) ? rgb565(120, 220, 255) : rgb565(170, 200, 230);
            int16_t localY = py - bandGlobalY0;
            if (localY >= 0 && localY < bandHeight && px >= 0 && px < screenWidth) {
                buffer[localY * screenWidth + px] = color;
            }
            // Vertical streak tail
            if (localY - 1 >= 0 && localY - 1 < bandHeight && px >= 0 && px < screenWidth) {
                buffer[(localY - 1) * screenWidth + px] = rgb565(60, 140, 200);
            }
        }
    }
}
