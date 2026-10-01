#include "particle_system.h"

ParticleSystem Particles;

#include "plant_engine.h"

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
    else if (phase == PHASE_GOLDEN_HOUR || (phase == PHASE_DAYLIGHT && rand() % 100 < 45)) {
        // Pollen grain release from mature flowers
        FlowerNodePos flowers[MAX_FLOWER_NODES];
        uint8_t fCount = Plant.getFlowerNodes(flowers, MAX_FLOWER_NODES);
        if (fCount > 0) {
            uint8_t fIdx = rand() % fCount;
            if (flowers[fIdx].isMature) {
                p.x = flowers[fIdx].x << 4;
                p.y = (flowers[fIdx].y - 2) << 4;
                p.vx = (rand() % 15 - 5); // drift with breeze
                p.vy = (rand() % 9 - 4);  // gentle floating flutter
                p.life = rand() % 50 + 50;
                p.maxLife = p.life;
                p.type = PARTICLE_POLLEN_MOTE;
                p.size = 1;
                p.phase = rand() % 32;
                p.active = true;
                return;
            }
        }

        // Fallback: Daytime photosynthetic oxygen bubble rising from foliage
        LeafNodePos leaves[16];
        uint8_t leafCount = Plant.getActiveLeafNodes(leaves, 16);
        if (leafCount > 0 && (rand() % 100 < 80)) {
            uint8_t lIdx = rand() % leafCount;
            p.x = leaves[lIdx].x << 4;
            p.y = leaves[lIdx].y << 4;
        } else {
            p.x = (rand() % (SCREEN_WIDTH - 30) + 15) << 4;
            p.y = (SCREEN_HEIGHT - SUBSTRATE_HEIGHT - 5 - (rand() % 30)) << 4;
        }
        p.vx = (rand() % 9 - 4);
        p.vy = -(rand() % 16 + 16); // steady, lively float upward
        p.life = rand() % 40 + 40;
        p.maxLife = p.life;
        p.type = PARTICLE_OXYGEN_MOTE;
        p.size = (rand() % 2 == 0) ? 1 : 2;
        p.phase = rand() % 32;
        p.active = true;
    }
    else {
        // Daytime / Dawn / Golden Hour: Photosynthetic oxygen bubble rising from foliage
        LeafNodePos leaves[16];
        uint8_t leafCount = Plant.getActiveLeafNodes(leaves, 16);
        if (leafCount > 0 && (rand() % 100 < 80)) {
            uint8_t lIdx = rand() % leafCount;
            p.x = leaves[lIdx].x << 4;
            p.y = leaves[lIdx].y << 4;
        } else {
            p.x = (rand() % (SCREEN_WIDTH - 30) + 15) << 4;
            p.y = (SCREEN_HEIGHT - SUBSTRATE_HEIGHT - 5 - (rand() % 30)) << 4;
        }
        p.vx = (rand() % 9 - 4);
        p.vy = -(rand() % 16 + 16); // steady, lively float upward
        p.life = rand() % 40 + 40;
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
    uint32_t spawnInterval = (weather == WEATHER_RAIN) ? 120 : 220;
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
        } else if (p.type == PARTICLE_POLLEN_MOTE) {
            // Floating drifting pollen grain
            int16_t driftY = fastSin(p.phase + (i * 3)) >> 5;
            p.x += (p.vx + (fastSin(p.phase) >> 4));
            p.y += (p.vy + driftY);
        } else if (p.type == PARTICLE_SEED_MOTE) {
            // Falling seed pod particle
            p.x += p.vx;
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

void ParticleSystem::render(uint16_t* buffer, int16_t screenWidth, int16_t screenHeight) {
    if (!buffer) return;

    for (uint8_t i = 0; i < MAX_PARTICLES; i++) {
        Particle& p = _pool[i];
        if (!p.active) continue;

        int16_t px = p.x >> 4;
        int16_t py = p.y >> 4;

        if (px < 0 || px >= screenWidth || py < 0 || py >= screenHeight) continue;

        if (p.type == PARTICLE_FIREFLY) {
            int8_t pulse = fastSin(p.phase);
            uint8_t brightness = (pulse > 0) ? (140 + (pulse >> 1)) : (80 + (pulse >> 2));
            uint16_t color = rgb565(brightness, (uint8_t)(brightness * 0.95f), (uint8_t)(brightness * 0.2f));

            buffer[py * screenWidth + px] = color;

            if (p.size >= 3) {
                uint16_t dimGlow = rgb565(brightness >> 2, (uint8_t)((brightness * 0.95f) / 4), (uint8_t)((brightness * 0.2f) / 4));
                if (py > 0) buffer[(py - 1) * screenWidth + px] = dimGlow;
                if (py < screenHeight - 1) buffer[(py + 1) * screenWidth + px] = dimGlow;
                if (px > 0) buffer[py * screenWidth + (px - 1)] = dimGlow;
                if (px < screenWidth - 1) buffer[py * screenWidth + (px + 1)] = dimGlow;
            }
        } 
        else if (p.type == PARTICLE_OXYGEN_MOTE) {
            uint16_t color = rgb565(210, 245, 255);
            buffer[py * screenWidth + px] = color;
            if (p.size >= 2 && px + 1 < screenWidth && py + 1 < screenHeight) {
                buffer[(py + 1) * screenWidth + (px + 1)] = rgb565(140, 210, 240);
            }
        } 
        else if (p.type == PARTICLE_POLLEN_MOTE) {
            // Golden glowing pollen grain
            uint16_t color = rgb565(255, 225, 60);
            buffer[py * screenWidth + px] = color;
            if (py > 0 && px > 0 && (p.phase & 4)) {
                buffer[(py - 1) * screenWidth + px] = rgb565(200, 160, 30);
            }
        }
        else if (p.type == PARTICLE_SEED_MOTE) {
            uint16_t color = rgb565(175, 125, 45);
            buffer[py * screenWidth + px] = color;
        }
        else if (p.type == PARTICLE_RAIN_CASCADE || p.type == PARTICLE_MIST_DROP) {
            uint16_t color = (p.type == PARTICLE_RAIN_CASCADE) ? rgb565(120, 220, 255) : rgb565(170, 200, 230);
            buffer[py * screenWidth + px] = color;
            if (py > 0) {
                buffer[(py - 1) * screenWidth + px] = rgb565(60, 140, 200);
            }
        }
    }
}
