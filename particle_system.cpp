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

// Relative humidity at or above this lays condensation on the glass even when it is not raining.
static const uint8_t HIGH_HUMIDITY_FOG_PCT = 85;

// Intensity 0 means the amount was not measured (offline demo). Treat that as a moderate shower.
static uint8_t resolvedPrecipLevel(WeatherType weather, uint8_t intensity) {
    if ((weather == WEATHER_RAIN || weather == WEATHER_SNOW) && intensity == 0) return 2;
    if (intensity > 3) return 3;
    return intensity;
}

static void activateRainStreak(Particle& p, uint8_t level, int8_t gust) {
    if (level < 1) level = 1;
    if (level > 3) level = 3;
    p.x = (int16_t)((rand() % (SCREEN_WIDTH - 8) + 4) << 4);
    p.y = (int16_t)((HUD_HEIGHT + 2) << 4);

    // Shared gust so the shower leans together instead of scattering both ways.
    int16_t lean = (int16_t)(8 + level * 4);
    if (gust < 0) lean = (int16_t)(-lean);
    p.vx = (int16_t)(lean + (gust / 20));

    // Light drizzle ~2 px/tick, heavy showers ~4 px/tick.
    int16_t fall = (int16_t)(16 + level * 14);
    p.vy = (int16_t)(fall + (rand() % 8));

    uint8_t life = (uint8_t)(120 - level * 22 + (rand() % 16));
    p.life = life;
    p.maxLife = life;
    p.type = PARTICLE_RAIN_STREAK;
    p.size = (level >= 3) ? 2 : 1;
    p.phase = (uint8_t)(rand() % 32);
    p.active = true;
}

static void activateRainMist(Particle& p) {
    p.x = (int16_t)((rand() % (SCREEN_WIDTH - 8) + 4) << 4);
    p.y = (int16_t)((HUD_HEIGHT + 2) << 4);
    p.vx = (int16_t)(rand() % 7 - 3);
    p.vy = (int16_t)(10 + (rand() % 6));
    p.life = (uint8_t)(70 + (rand() % 30));
    p.maxLife = p.life;
    p.type = PARTICLE_MIST_DROP;
    p.size = 1;
    p.phase = (uint8_t)(rand() % 32);
    p.active = true;
}

static void activateFogDroplet(Particle& p) {
    int16_t top = HUD_HEIGHT + 8;
    int16_t bottom = HUD_HEIGHT + CANOPY_HEIGHT - 12;
    if (bottom <= top) bottom = (int16_t)(top + 1);
    p.x = (int16_t)((rand() % (SCREEN_WIDTH - 6) + 3) << 4);
    p.y = (int16_t)((top + (rand() % (bottom - top))) << 4);
    p.vx = (int16_t)(rand() % 3 - 1);
    p.vy = (int16_t)(1 + (rand() % 3));
    p.life = (uint8_t)(180 + (rand() % 60));
    p.maxLife = p.life;
    p.type = PARTICLE_MIST_DROP;
    p.size = (rand() % 3 == 0) ? 2 : 1;
    p.phase = (uint8_t)(rand() % 32);
    p.active = true;
}

static void activateSnowflake(Particle& p) {
    p.x = (int16_t)((rand() % (SCREEN_WIDTH - 8) + 4) << 4);
    p.y = (int16_t)((HUD_HEIGHT + 2) << 4);
    p.vx = (int16_t)(rand() % 5 - 2);
    p.vy = (int16_t)(6 + (rand() % 6));
    p.life = (uint8_t)(210 + (rand() % 40));
    p.maxLife = p.life;
    p.type = PARTICLE_SNOWFLAKE;
    p.size = (rand() % 5 == 0) ? 3 : ((rand() % 2) ? 2 : 1);
    p.phase = (uint8_t)(rand() % 32);
    p.active = true;
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

static void spawnFairWeather(Particle& p, TimePhase phase) {
    if (phase == PHASE_NIGHT || phase == PHASE_DUSK) {
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

void ParticleSystem::spawnAmbient(TimePhase phase, WeatherType weather, uint8_t humidityPct, uint8_t precipIntensity) {
    int8_t slot = findFreeSlot();
    if (slot < 0) return;

    Particle& p = _pool[slot];
    uint8_t level = resolvedPrecipLevel(weather, precipIntensity);

    if (weather == WEATHER_RAIN) {
        // Light drizzle keeps more mist; heavier showers are mostly fast streaks.
        uint8_t mistChance = (level >= 3) ? 2 : (level == 2 ? 3 : 5);
        if ((rand() % 10) < mistChance) {
            activateRainMist(p);
        } else {
            activateRainStreak(p, level, fastSin(_globalPhase >> 3));
        }
        return;
    }

    if (weather == WEATHER_SNOW) {
        activateSnowflake(p);
        return;
    }

    if (weather == WEATHER_FOG) {
        activateFogDroplet(p);
        return;
    }

    // Clear, cloudy, and moon keep oxygen motes, pollen, and fireflies.
    // High humidity still beads condensation on the glass.
    if (humidityPct >= HIGH_HUMIDITY_FOG_PCT && (rand() % 3) == 0) {
        activateFogDroplet(p);
        return;
    }

    spawnFairWeather(p, phase);
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

void ParticleSystem::update(uint32_t deltaMs, TimePhase phase, WeatherType weather,
                            uint8_t humidityPct, uint8_t precipIntensity) {
    _globalPhase++;
    uint32_t now = millis();

    uint8_t level = resolvedPrecipLevel(weather, precipIntensity);
    uint32_t spawnInterval = 220;
    uint8_t burst = 1;
    if (weather == WEATHER_RAIN) {
        if (level >= 3) {
            spawnInterval = 45;
            burst = 2;
        } else if (level == 2) {
            spawnInterval = 70;
        } else {
            spawnInterval = 170;
        }
    } else if (weather == WEATHER_SNOW) {
        if (level >= 3) spawnInterval = 90;
        else if (level == 2) spawnInterval = 140;
        else spawnInterval = 210;
    } else if (weather == WEATHER_FOG) {
        spawnInterval = 120;
    } else if (humidityPct >= HIGH_HUMIDITY_FOG_PCT) {
        spawnInterval = 180;
    }

    if (now - _lastSpawnTime >= spawnInterval) {
        for (uint8_t n = 0; n < burst; n++) {
            spawnAmbient(phase, weather, humidityPct, precipIntensity);
        }
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
        } else if (p.type == PARTICLE_SEED_MOTE || p.type == PARTICLE_RAIN_STREAK || p.type == PARTICLE_RAIN_CASCADE) {
            p.x += p.vx;
            p.y += p.vy;
        } else if (p.type == PARTICLE_SNOWFLAKE) {
            // Crystalline flakes drift on the same sine sway as the other motes.
            int16_t driftX = fastSin((uint8_t)(p.phase + (i * 5))) >> 3;
            p.x += (p.vx + driftX);
            p.y += p.vy;
        } else if (p.type == PARTICLE_MIST_DROP) {
            // Glass droplets cling, then run downward. Falling rain mist is already faster.
            if (p.vy < 10) {
                uint8_t lived = (uint8_t)(p.maxLife - p.life);
                if (lived > (p.maxLife / 3) && (p.phase & 3) == 0 && p.vy < 18) {
                    p.vy = (int16_t)(p.vy + 1);
                }
            }
            int16_t waveX = fastSin(p.phase) >> 5;
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
        else if (p.type == PARTICLE_RAIN_CASCADE) {
            uint16_t color = rgb565(120, 220, 255);
            buffer[py * screenWidth + px] = color;
            if (py > 0) {
                buffer[(py - 1) * screenWidth + px] = rgb565(60, 140, 200);
            }
        }
        else if (p.type == PARTICLE_RAIN_STREAK) {
            uint16_t head = rgb565(160, 225, 255);
            buffer[py * screenWidth + px] = head;
            int8_t dx = (p.vx >= 0) ? -1 : 1;
            int16_t tx = (int16_t)(px + dx);
            if (py > 0 && tx >= 0 && tx < screenWidth) {
                buffer[(py - 1) * screenWidth + tx] = rgb565(80, 150, 200);
            }
            if (p.size >= 2) {
                int16_t tx2 = (int16_t)(px + dx * 2);
                if (py > 1 && tx2 >= 0 && tx2 < screenWidth) {
                    buffer[(py - 2) * screenWidth + tx2] = rgb565(40, 90, 140);
                }
            }
        }
        else if (p.type == PARTICLE_MIST_DROP) {
            buffer[py * screenWidth + px] = rgb565(190, 210, 225);
            if (p.size >= 2 && py + 1 < screenHeight) {
                buffer[(py + 1) * screenWidth + px] = rgb565(150, 175, 195);
            }
        }
        else if (p.type == PARTICLE_SNOWFLAKE) {
            uint8_t core = (fastSin(p.phase) > 20) ? 255 : 228;
            buffer[py * screenWidth + px] = rgb565(core, core, 255);
            if (p.size >= 2) {
                uint16_t arm = rgb565(186, 206, 230);
                if (py > 0) buffer[(py - 1) * screenWidth + px] = arm;
                if (py + 1 < screenHeight) buffer[(py + 1) * screenWidth + px] = arm;
                if (px > 0) buffer[py * screenWidth + (px - 1)] = arm;
                if (px + 1 < screenWidth) buffer[py * screenWidth + (px + 1)] = arm;
            }
            if (p.size >= 3 && (p.phase & 8)) {
                uint16_t spark = rgb565(210, 225, 245);
                if (py > 0 && px > 0) buffer[(py - 1) * screenWidth + (px - 1)] = spark;
                if (py > 0 && px + 1 < screenWidth) buffer[(py - 1) * screenWidth + (px + 1)] = spark;
                if (py + 1 < screenHeight && px > 0) buffer[(py + 1) * screenWidth + (px - 1)] = spark;
                if (py + 1 < screenHeight && px + 1 < screenWidth) buffer[(py + 1) * screenWidth + (px + 1)] = spark;
            }
        }
    }
}
