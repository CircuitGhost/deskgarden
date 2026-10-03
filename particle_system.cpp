#include "particle_system.h"

ParticleSystem Particles;

#include "plant_engine.h"
#include "mesh_sync.h"

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

static void activateNocturnalSpore(Particle& p) {
    LeafNodePos leaves[16];
    uint8_t leafCount = Plant.getActiveLeafNodes(leaves, 16);
    if (leafCount > 0) {
        uint8_t idx = (uint8_t)(rand() % leafCount);
        p.x = leaves[idx].x << 4;
        p.y = (leaves[idx].y - 1) << 4;
    } else {
        p.x = (int16_t)((rand() % (SCREEN_WIDTH - 24) + 12) << 4);
        p.y = (int16_t)((HUD_HEIGHT + 40 + (rand() % (CANOPY_HEIGHT - 80))) << 4);
    }
    p.vx = (int16_t)(rand() % 7 - 3);
    p.vy = (int16_t)(-(rand() % 5 + 2));
    p.life = (uint8_t)(110 + (rand() % 50));
    p.maxLife = p.life;
    p.type = PARTICLE_NOCTURNAL_SPORE;
    p.size = (rand() % 2) ? 3 : 2;
    p.phase = (uint8_t)(rand() % 32);
    p.active = true;
}

static const uint32_t POLLINATION_COOLDOWN_MS = 400;
static const uint8_t POLLEN_GRACE_FRAMES = 10;

static void activateGoldenSparkle(Particle& p, int16_t xSub, int16_t ySub) {
    p.x = xSub;
    p.y = ySub;
    p.vx = 0;
    p.vy = 0;
    p.life = 18;
    p.maxLife = 18;
    p.type = PARTICLE_GOLDEN_SPARKLE;
    p.size = 3;
    p.phase = 0;
    p.tag = 0;
    p.active = true;
}

static void releaseSeedMote(Particle& p, bool carrySparkle) {
    p.type = PARTICLE_SEED_MOTE;
    p.size = carrySparkle ? 2 : 1;
    p.tag = 0;
    p.vx = (int16_t)((rand() % 5) - 2);
    p.vy = (int16_t)(18 + (rand() % 6));
    p.life = 255;
    p.maxLife = 255;
    p.active = true;
}

static void activateOxygenMote(Particle& p) {
    LeafNodePos leaves[16];
    uint8_t leafCount = Plant.getActiveLeafNodes(leaves, 16);
    if (leafCount > 0 && (rand() % 100 < 80)) {
        uint8_t lIdx = (uint8_t)(rand() % leafCount);
        p.x = leaves[lIdx].x << 4;
        p.y = leaves[lIdx].y << 4;
    } else {
        p.x = (int16_t)((rand() % (SCREEN_WIDTH - 30) + 15) << 4);
        p.y = (int16_t)((SCREEN_HEIGHT - SUBSTRATE_HEIGHT - 5 - (rand() % 30)) << 4);
    }
    p.vx = (int16_t)(rand() % 9 - 4);
    p.vy = (int16_t)(-(rand() % 16 + 16));
    p.life = (uint16_t)(rand() % 40 + 40);
    p.maxLife = p.life;
    p.type = PARTICLE_OXYGEN_MOTE;
    p.size = (rand() % 2 == 0) ? 1 : 2;
    p.phase = (uint8_t)(rand() % 32);
    p.tag = 0;
    p.active = true;
}

static void activateFallingLeaf(Particle& p) {
    LeafNodePos leaves[16];
    uint8_t leafCount = Plant.getActiveLeafNodes(leaves, 16);
    if (leafCount > 0 && (rand() % 100) < 75) {
        uint8_t idx = (uint8_t)(rand() % leafCount);
        p.x = leaves[idx].x << 4;
        p.y = leaves[idx].y << 4;
    } else {
        p.x = (int16_t)((rand() % (SCREEN_WIDTH - 16) + 8) << 4);
        p.y = (int16_t)((HUD_HEIGHT + 4 + (rand() % 36)) << 4);
    }
    p.vx = (int16_t)(rand() % 5 - 2);
    p.vy = (int16_t)(7 + (rand() % 5));
    p.life = (uint16_t)(380 + (rand() % 140));
    p.maxLife = p.life;
    p.type = PARTICLE_FALLING_LEAF;
    p.size = (uint8_t)(2 + (rand() % 2));
    p.phase = (uint8_t)(rand() % 32);
    p.tag = (uint8_t)(rand() % 3);
    p.active = true;
}

static void activateBlossomPetal(Particle& p) {
    FlowerNodePos flowers[MAX_FLOWER_NODES];
    uint8_t flowerCount = Plant.getFlowerNodes(flowers, MAX_FLOWER_NODES);
    if (flowerCount > 0 && (rand() % 100) < 40) {
        uint8_t idx = (uint8_t)(rand() % flowerCount);
        p.x = flowers[idx].x << 4;
        p.y = (int16_t)((flowers[idx].y + 2) << 4);
    } else {
        p.x = (int16_t)((rand() % (SCREEN_WIDTH - 10) + 5) << 4);
        p.y = (int16_t)((HUD_HEIGHT + 1) << 4);
    }
    p.vx = (int16_t)(rand() % 5 - 2);
    p.vy = (int16_t)(8 + (rand() % 5));
    p.life = (uint16_t)(460 + (rand() % 160));
    p.maxLife = p.life;
    p.type = PARTICLE_BLOSSOM_PETAL;
    p.size = 2;
    p.phase = (uint8_t)(rand() % 32);
    p.tag = (uint8_t)(rand() % 2);
    p.active = true;
}

static int16_t gustDrift(uint8_t phase, uint8_t salt) {
    int16_t wind = Plant.getWindSway();
    int16_t drift = (int16_t)(wind >> 5);
    if (wind > 90 || wind < -90) {
        drift = (int16_t)(drift + (wind >> 4));
    }
    drift = (int16_t)(drift + (fastSin((uint8_t)(phase + salt)) >> 3));
    return drift;
}

static void plotParticle(uint16_t* buffer, int16_t width, int16_t height,
                         int16_t x, int16_t y, uint16_t color) {
    if (!buffer || x < 0 || y < 0 || x >= width || y >= height) return;
    buffer[(y * width) + x] = color;
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
      _lastSporeSpawn(0),
      _lastPollination(0),
      _lastLeafSpawn(0),
      _lastPetalSpawn(0),
      _lastBubbleSpawn(0),
      _lastLadybugSpawn(0),
      _lastVisitorSpawn(0),
      _lastSnailSpawn(0),
      _globalPhase(0) {
    for (uint8_t i = 0; i < MAX_PARTICLES; i++) {
        _pool[i].active = false;
        _pool[i].tag = 0;
        _lineage[i].foreign = false;
    }
}

void ParticleSystem::begin() {
    for (uint8_t i = 0; i < MAX_PARTICLES; i++) {
        _pool[i].active = false;
        _pool[i].tag = 0;
        _lineage[i].foreign = false;
        _lineage[i].seed = 0;
    }
    _lastSpawnTime = millis();
    _lastSporeSpawn = millis();
    _lastPollination = millis();
    _lastLeafSpawn = millis();
    _lastPetalSpawn = millis();
    _lastBubbleSpawn = millis();
    _lastLadybugSpawn = 0;
    _lastVisitorSpawn = 0;
    _lastSnailSpawn = 0;
    _globalPhase = 0;
}

int8_t ParticleSystem::findFreeSlot() {
    for (uint8_t i = 0; i < MAX_PARTICLES; i++) {
        if (!_pool[i].active) {
            _lineage[i].foreign = false;
            return i;
        }
    }
    return -1;
}

bool ParticleSystem::receiveSpore(int16_t y, uint8_t arriveEdge, int16_t vx, int16_t vy,
                                  uint32_t seed, uint8_t phenotype, uint8_t petalPalette,
                                  uint8_t stemHue, uint16_t petalColor) {
    int8_t slot = findFreeSlot();
    if (slot < 0) return false;

    if (y < (int16_t)(HUD_HEIGHT + 2)) y = (int16_t)(HUD_HEIGHT + 2);
    int16_t yMax = (int16_t)(SCREEN_HEIGHT - SUBSTRATE_HEIGHT - 4);
    if (y > yMax) y = yMax;

    int16_t x = (arriveEdge == MESH_EDGE_LEFT) ? 1 : (int16_t)(SCREEN_WIDTH - 2);
    int16_t ivx = vx;
    if (arriveEdge == MESH_EDGE_LEFT) {
        if (ivx < 6) ivx = 10;
    } else if (ivx > -6) {
        ivx = -10;
    }

    Particle& p = _pool[slot];
    p.x = (int16_t)(x << 4);
    p.y = (int16_t)(y << 4);
    p.vx = ivx;
    p.vy = vy;
    p.life = 220;
    p.maxLife = 220;
    p.type = PARTICLE_POLLEN_MOTE;
    p.size = 2;
    p.phase = 0;
    p.tag = 0xFE;
    p.active = true;

    SporeLineage& line = _lineage[slot];
    line.seed = seed;
    line.petalColor = petalColor;
    line.phenotype = phenotype & 0x03;
    line.petalPalette = (petalPalette > 4) ? 0 : petalPalette;
    line.stemHue = stemHue & 0x03;
    line.foreign = true;
    return true;
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
        // Pollen grain release from mature flowers. Any open bloom can be the source.
        FlowerNodePos flowers[MAX_FLOWER_NODES];
        uint8_t fCount = Plant.getFlowerNodes(flowers, MAX_FLOWER_NODES);
        uint8_t matureIds[MAX_FLOWER_NODES];
        uint8_t matureCount = 0;
        for (uint8_t fi = 0; fi < fCount; fi++) {
            if (flowers[fi].isMature && matureCount < MAX_FLOWER_NODES) {
                matureIds[matureCount++] = fi;
            }
        }
        if (matureCount > 0) {
            uint8_t fIdx = matureIds[rand() % matureCount];
            p.x = flowers[fIdx].x << 4;
            p.y = (flowers[fIdx].y - 2) << 4;
            p.vx = (rand() % 15 - 5); // drift with breeze
            p.vy = (rand() % 9 - 4);  // gentle floating flutter
            p.life = (uint8_t)(rand() % 60 + 90);
            p.maxLife = p.life;
            p.type = PARTICLE_POLLEN_MOTE;
            p.size = 1;
            p.phase = (uint8_t)(rand() % 32);
            p.tag = fIdx;
            p.active = true;
            return;
        }

        // Fallback: Daytime photosynthetic oxygen bubble rising from foliage
        activateOxygenMote(p);
    }
    else {
        // Daytime / Dawn / Golden Hour: Photosynthetic oxygen bubble rising from foliage
        activateOxygenMote(p);
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

static int8_t firstChildSegment(uint8_t parent) {
    uint8_t count = Plant.getSegmentCount();
    for (uint8_t i = 0; i < count; i++) {
        SegmentPose pose;
        if (!Plant.getSegmentPose(i, pose)) continue;
        if (pose.parentIndex == (int8_t)parent) return (int8_t)i;
    }
    return -1;
}

static void stepToward(Particle& p, int16_t tx, int16_t ty, int16_t step) {
    int16_t dx = (int16_t)(tx - p.x);
    int16_t dy = (int16_t)(ty - p.y);
    if (dx > step) p.x = (int16_t)(p.x + step);
    else if (dx < -step) p.x = (int16_t)(p.x - step);
    else p.x = tx;
    if (dy > step) p.y = (int16_t)(p.y + step);
    else if (dy < -step) p.y = (int16_t)(p.y - step);
    else p.y = ty;
}

static bool nearTarget(const Particle& p, int16_t tx, int16_t ty) {
    int16_t dx = (int16_t)(tx - p.x);
    int16_t dy = (int16_t)(ty - p.y);
    if (dx < 0) dx = (int16_t)(-dx);
    if (dy < 0) dy = (int16_t)(-dy);
    return dx <= 8 && dy <= 8;
}

static void activateLadybug(Particle& p) {
    SegmentPose trunk;
    if (!Plant.getSegmentPose(0, trunk)) return;
    p.x = (int16_t)(trunk.x0 << 4);
    p.y = (int16_t)(trunk.y0 << 4);
    p.vx = 0;
    p.vy = 2; // perches still available
    p.life = 5000;
    p.maxLife = 5000;
    p.type = PARTICLE_LADYBUG;
    p.size = 0; // climbing
    p.phase = 0;
    p.tag = 0;
    p.active = true;
}

static int8_t pickNectarFlower(bool requireOpen) {
    FlowerNodePos flowers[MAX_FLOWER_NODES];
    uint8_t count = Plant.getFlowerNodes(flowers, MAX_FLOWER_NODES);
    uint8_t matches[MAX_FLOWER_NODES];
    uint8_t matchCount = 0;
    for (uint8_t i = 0; i < count; i++) {
        bool bloom = (flowers[i].stage == BLOOM_MATURE || flowers[i].stage == BLOOM_OPENING);
        if (requireOpen) {
            if (flowers[i].isMature && matchCount < MAX_FLOWER_NODES) matches[matchCount++] = i;
        } else if (bloom && matchCount < MAX_FLOWER_NODES) {
            matches[matchCount++] = i;
        }
    }
    if (matchCount == 0) return -1;
    return (int8_t)matches[rand() % matchCount];
}

static void activateVisitor(Particle& p, int8_t flower, bool nocturnal) {
    FlowerNodePos flowers[MAX_FLOWER_NODES];
    uint8_t count = Plant.getFlowerNodes(flowers, MAX_FLOWER_NODES);
    if (flower < 0 || (uint8_t)flower >= count) return;

    p.x = (int16_t)(flowers[flower].x << 4);
    p.y = (int16_t)((flowers[flower].y - 1) << 4);
    p.vx = 0;
    p.vy = 0;
    // Sim ticks are ~16ms, so 312–500 ticks is about 5–8 seconds of hovering.
    p.life = (uint16_t)(312 + (rand() % 189));
    p.maxLife = p.life;
    p.type = PARTICLE_NECTAR_VISITOR;
    p.size = 0; // hovering
    p.phase = (uint8_t)(rand() % 32);
    p.tag = (uint8_t)flower;
    if (nocturnal) p.tag = (uint8_t)(p.tag | 0x80);
    p.active = true;
}

static void beginVisitorDart(Particle& p) {
    int16_t px = (int16_t)(p.x >> 4);
    p.size = 1;
    p.life = 36;
    p.maxLife = 36;
    p.vx = (px < (SCREEN_WIDTH / 2)) ? (int16_t)-80 : (int16_t)80;
    p.vy = (int16_t)(-(36 + (rand() % 24)));
}

static void activateLoamSnail(Particle& p) {
    int16_t surface = (int16_t)(SCREEN_HEIGHT - SUBSTRATE_HEIGHT);
    bool fromLeft = (rand() % 2) == 0;
    int16_t startX = fromLeft ? 8 : (int16_t)(SCREEN_WIDTH - 12);
    p.x = (int16_t)(startX << 4);
    p.y = (int16_t)((surface + 12) << 4); // lower humus loam
    p.vx = fromLeft ? 1 : -1;
    p.vy = 0;
    p.life = 40000; // safety cap; the crawl itself is the several-minute crossing
    p.maxLife = 40000;
    p.type = PARTICLE_LOAM_SNAIL;
    p.size = 1;
    p.phase = 0;
    p.tag = 0;
    p.active = true;
}

static void updateLadybug(Particle& p) {
    SegmentPose pose;
    if (!Plant.getSegmentPose(p.tag, pose)) {
        p.active = false;
        return;
    }

    if (p.size == 1) {
        p.x = (int16_t)(pose.leafX << 4);
        p.y = (int16_t)(pose.leafY << 4);
        if (p.vx > 0) p.vx--;
        if (p.vx > 0) return;

        p.size = 0;
        int8_t child = firstChildSegment(p.tag);
        if (child < 0) {
            p.active = false;
            return;
        }
        p.tag = (uint8_t)child;
        return;
    }

    int16_t tx = (int16_t)(pose.x1 << 4);
    int16_t ty = (int16_t)(pose.y1 << 4);
    if (!nearTarget(p, tx, ty)) {
        stepToward(p, tx, ty, 5);
        return;
    }

    p.x = tx;
    p.y = ty;
    int8_t child = firstChildSegment(p.tag);
    bool perchHere = (p.vy > 0) && (pose.broadLeaf || (pose.hasLeaf && child < 0));
    if (perchHere) {
        p.size = 1;
        p.vx = (int16_t)(200 + (rand() % 120)); // about 3–5 seconds on the leaf
        p.vy--;
        p.x = (int16_t)(pose.leafX << 4);
        p.y = (int16_t)(pose.leafY << 4);
        return;
    }
    if (child < 0) {
        p.active = false;
        return;
    }
    p.tag = (uint8_t)child;
}

static void updateVisitor(Particle& p) {
    if (p.size == 1) {
        p.x = (int16_t)(p.x + p.vx);
        p.y = (int16_t)(p.y + p.vy);
        return;
    }

    uint8_t idx = (uint8_t)(p.tag & 0x7F);
    FlowerNodePos flowers[MAX_FLOWER_NODES];
    uint8_t count = Plant.getFlowerNodes(flowers, MAX_FLOWER_NODES);
    if (idx >= count) {
        beginVisitorDart(p);
        p.x = (int16_t)(p.x + p.vx);
        p.y = (int16_t)(p.y + p.vy);
        return;
    }

    int16_t wobX = (int16_t)(fastSin(p.phase) >> 3);
    int16_t wobY = (int16_t)(fastSin((uint8_t)(p.phase + 8)) >> 4);
    p.x = (int16_t)((flowers[idx].x << 4) + wobX);
    p.y = (int16_t)(((flowers[idx].y - 1) << 4) + wobY);
}

static void updateLoamSnail(Particle& p) {
    // One subpixel every 8 ticks ≈ 0.5 px/s, so a crossing takes several minutes.
    if ((p.phase & 7) == 0) {
        p.x = (int16_t)(p.x + ((p.vx < 0) ? -1 : 1));
    }
}

static uint16_t ladybugColor(uint8_t index) {
    if (index == 1) return rgb565(24, 16, 16);
    if (index == 2) return rgb565(214, 32, 36);
    if (index == 3) return rgb565(255, 110, 80);
    return 0;
}

static uint16_t birdColor(uint8_t index) {
    if (index == 1) return rgb565(18, 150, 72);
    if (index == 2) return rgb565(200, 28, 52);
    if (index == 3) return rgb565(232, 196, 48);
    return 0;
}

static uint16_t mothColor(uint8_t index) {
    if (index == 1) return rgb565(186, 176, 148);
    if (index == 2) return rgb565(96, 72, 48);
    if (index == 3) return rgb565(40, 32, 28);
    return 0;
}

static uint16_t snailColor(uint8_t index) {
    if (index == 1) return rgb565(148, 100, 58);
    if (index == 2) return rgb565(198, 158, 104);
    if (index == 3) return rgb565(72, 50, 34);
    if (index == 4) return rgb565(112, 78, 48);
    return 0;
}

static void blitSprite(uint16_t* buffer, int16_t screenWidth, int16_t screenHeight,
                       int16_t originX, int16_t originY, const uint8_t* cells,
                       uint8_t spriteW, uint8_t spriteH, bool flipX, uint16_t (*colorOf)(uint8_t)) {
    for (uint8_t row = 0; row < spriteH; row++) {
        for (uint8_t col = 0; col < spriteW; col++) {
            uint8_t srcCol = flipX ? (uint8_t)(spriteW - 1 - col) : col;
            uint8_t index = cells[(row * spriteW) + srcCol];
            if (index == 0) continue;
            plotParticle(buffer, screenWidth, screenHeight,
                         (int16_t)(originX + col), (int16_t)(originY + row), colorOf(index));
        }
    }
}

static void renderCreature(uint16_t* buffer, int16_t screenWidth, int16_t screenHeight, const Particle& p) {
    int16_t px = (int16_t)(p.x >> 4);
    int16_t py = (int16_t)(p.y >> 4);

    if (p.type == PARTICLE_LADYBUG) {
        static const uint8_t frames[2][16] = {
            {0, 1, 1, 0,  2, 1, 2, 1,  2, 2, 2, 2,  0, 2, 2, 0},
            {0, 1, 1, 0,  1, 2, 2, 1,  2, 2, 2, 2,  2, 0, 0, 2}
        };
        bool walking = (p.size == 0);
        const uint8_t* cells = frames[walking && ((p.phase & 8) != 0) ? 1 : 0];
        bool flip = false;
        SegmentPose pose;
        if (Plant.getSegmentPose(p.tag, pose)) flip = pose.x1 < pose.x0;
        blitSprite(buffer, screenWidth, screenHeight, (int16_t)(px - 1), (int16_t)(py - 2),
                   cells, 4, 4, flip, ladybugColor);
        return;
    }

    if (p.type == PARTICLE_NECTAR_VISITOR) {
        static const uint8_t bird[2][36] = {
            {1, 0, 1, 1, 0, 1,
             1, 1, 1, 1, 1, 1,
             0, 1, 2, 2, 1, 0,
             0, 0, 1, 1, 0, 0,
             0, 0, 3, 3, 0, 0,
             0, 0, 3, 0, 0, 0},
            {0, 0, 1, 1, 0, 0,
             0, 1, 1, 1, 1, 0,
             1, 1, 2, 2, 1, 1,
             0, 0, 1, 1, 0, 0,
             0, 0, 3, 3, 0, 0,
             0, 0, 3, 0, 0, 0}
        };
        static const uint8_t moth[2][36] = {
            {1, 0, 0, 0, 0, 1,
             1, 1, 2, 2, 1, 1,
             0, 1, 2, 2, 1, 0,
             0, 0, 2, 2, 0, 0,
             0, 0, 3, 3, 0, 0,
             0, 0, 0, 0, 0, 0},
            {0, 0, 0, 0, 0, 0,
             0, 1, 0, 0, 1, 0,
             1, 1, 2, 2, 1, 1,
             0, 1, 2, 2, 1, 0,
             0, 0, 3, 3, 0, 0,
             0, 0, 0, 0, 0, 0}
        };
        bool mothSprite = (p.tag & 0x80) != 0;
        uint8_t frame = ((p.phase & 2) != 0) ? 1 : 0;
        const uint8_t* cells = mothSprite ? moth[frame] : bird[frame];
        blitSprite(buffer, screenWidth, screenHeight, (int16_t)(px - 3), (int16_t)(py - 6),
                   cells, 6, 6, false, mothSprite ? mothColor : birdColor);
        return;
    }

    if (p.type == PARTICLE_LOAM_SNAIL) {
        static const uint8_t frames[2][20] = {
            {0, 1, 1, 1, 0,
             0, 1, 2, 1, 3,
             4, 4, 4, 4, 0,
             0, 4, 0, 0, 0},
            {0, 1, 1, 1, 3,
             0, 1, 2, 1, 0,
             4, 4, 4, 4, 0,
             0, 4, 0, 0, 0}
        };
        const uint8_t* cells = frames[(p.phase & 16) ? 1 : 0];
        blitSprite(buffer, screenWidth, screenHeight, (int16_t)(px - 2), (int16_t)(py - 3),
                   cells, 5, 4, p.vx < 0, snailColor);
    }
}

void ParticleSystem::spawnSeasonal(uint32_t now, TimePhase phase, WeatherType weather,
                                   uint8_t humidityPct, uint8_t month, uint8_t day) {
    Season season = SEASON_SUMMER;
    if (!seasonFromMonth(month, season)) return;
    if (getActiveCount() >= 24) return;

    uint8_t progress = seasonProgress(month, day);
    bool precipAtmosphere = (weather == WEATHER_RAIN || weather == WEATHER_SNOW || weather == WEATHER_FOG);
    bool condensing = humidityPct >= HIGH_HUMIDITY_FOG_PCT;

    // Early in the season the effect is lighter; it thickens toward the last month.
    if (season == SEASON_AUTUMN && (now - _lastLeafSpawn) >= (uint32_t)(640u - progress)) {
        int8_t slot = findFreeSlot();
        if (slot >= 0) activateFallingLeaf(_pool[slot]);
        _lastLeafSpawn = now;
    }

    if (season == SEASON_SPRING && (now - _lastPetalSpawn) >= (uint32_t)(520u - progress)) {
        int8_t slot = findFreeSlot();
        if (slot >= 0) activateBlossomPetal(_pool[slot]);
        _lastPetalSpawn = now;
    }

    bool photosynthetic = (phase == PHASE_DAWN || phase == PHASE_DAYLIGHT || phase == PHASE_GOLDEN_HOUR);
    uint32_t bubbleInterval = (uint32_t)(400u - (progress / 2u));
    if (season == SEASON_SUMMER && photosynthetic && !precipAtmosphere && !condensing &&
        (now - _lastBubbleSpawn) >= bubbleInterval) {
        int8_t slot = findFreeSlot();
        if (slot >= 0) activateOxygenMote(_pool[slot]);
        _lastBubbleSpawn = now;
    }
}

bool ParticleSystem::hasActive(uint8_t type) const {
    for (uint8_t i = 0; i < MAX_PARTICLES; i++) {
        if (_pool[i].active && _pool[i].type == type) return true;
    }
    return false;
}

void ParticleSystem::spawnFauna(uint32_t now, TimePhase phase) {
    // First appearance is soon after boot; later visits stay occasional.
    uint32_t ladybugInterval = (_lastLadybugSpawn == 0) ? 8000u : 60000u;
    if ((now - _lastLadybugSpawn) >= ladybugInterval) {
        _lastLadybugSpawn = now;
        if (!hasActive(PARTICLE_LADYBUG) && Plant.getSegmentCount() > 0) {
            int8_t slot = findFreeSlot();
            if (slot >= 0) activateLadybug(_pool[slot]);
        }
    }

    uint32_t visitorInterval = (_lastVisitorSpawn == 0) ? 5000u : 40000u;
    if ((now - _lastVisitorSpawn) >= visitorInterval) {
        _lastVisitorSpawn = now;
        if (!hasActive(PARTICLE_NECTAR_VISITOR)) {
            bool nocturnal = (phase == PHASE_DUSK || phase == PHASE_NIGHT);
            int8_t flower = pickNectarFlower(!nocturnal);
            if (flower < 0) flower = pickNectarFlower(false);
            if (flower >= 0) {
                int8_t slot = findFreeSlot();
                if (slot >= 0) activateVisitor(_pool[slot], flower, nocturnal);
            }
        }
    }

    uint32_t snailInterval = (_lastSnailSpawn == 0) ? 7000u : 45000u;
    if ((now - _lastSnailSpawn) >= snailInterval) {
        _lastSnailSpawn = now;
        if (!hasActive(PARTICLE_LOAM_SNAIL)) {
            int8_t slot = findFreeSlot();
            if (slot >= 0) activateLoamSnail(_pool[slot]);
        }
    }
}

void ParticleSystem::update(uint32_t deltaMs, TimePhase phase, WeatherType weather,
                            uint8_t humidityPct, uint8_t precipIntensity,
                            uint8_t hour, uint8_t minute, uint8_t month, uint8_t day) {
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

    // Midnight spores use a free pool slot. Rain, snow, fog, and the watering cascade are left in place.
    if (isPhosphorescentHour(hour, minute) && (now - _lastSporeSpawn) >= 320) {
        int8_t slot = findFreeSlot();
        if (slot >= 0) {
            activateNocturnalSpore(_pool[slot]);
        }
        _lastSporeSpawn = now;
    }

    // Seasonal motes only take a free pool slot, and they yield once the pool is half full
    // so rain, snow, fog, watering, pollen, and spores keep a path in.
    spawnSeasonal(now, phase, weather, humidityPct, month, day);
    spawnFauna(now, phase);

    // Update active particles
    for (uint8_t i = 0; i < MAX_PARTICLES; i++) {
        Particle& p = _pool[i];
        if (!p.active) continue;

        if (p.life == 0) {
            if (p.type == PARTICLE_NECTAR_VISITOR && p.size == 0) {
                beginVisitorDart(p);
            } else {
                p.active = false;
                continue;
            }
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
            // Float on the sine flutter, leaned by the same breeze that sways the stems.
            int16_t driftY = fastSin((uint8_t)(p.phase + (uint8_t)(i * 3))) >> 5;
            int16_t breeze = (int16_t)(Plant.getWindSway() >> 6);
            p.x += (p.vx + (fastSin(p.phase) >> 4) + breeze);
            p.y += (p.vy + driftY);
        } else if (p.type == PARTICLE_GOLDEN_SPARKLE) {
            int16_t shimmer = fastSin(p.phase) >> 6;
            p.x += shimmer;
            p.y += fastSin((uint8_t)(p.phase + 8)) >> 6;
        } else if (p.type == PARTICLE_SEED_MOTE || p.type == PARTICLE_RAIN_STREAK || p.type == PARTICLE_RAIN_CASCADE) {
            p.x += p.vx;
            p.y += p.vy;
        } else if (p.type == PARTICLE_NOCTURNAL_SPORE) {
            int16_t wanderX = fastSin((uint8_t)(p.phase + (i * 3))) >> 5;
            int16_t wanderY = fastSin((uint8_t)((p.phase * 2) + i)) >> 5;
            p.x += (p.vx + wanderX);
            p.y += (p.vy + wanderY);
        } else if (p.type == PARTICLE_SNOWFLAKE) {
            // Crystalline flakes drift on the same sine sway as the other motes.
            int16_t driftX = fastSin((uint8_t)(p.phase + (i * 5))) >> 3;
            p.x += (p.vx + driftX);
            p.y += p.vy;
        } else if (p.type == PARTICLE_FALLING_LEAF || p.type == PARTICLE_BLOSSOM_PETAL) {
            int16_t drift = gustDrift(p.phase, (uint8_t)(i * 5));
            p.x += (int16_t)(p.vx + drift);
            p.y += (int16_t)(p.vy + (fastSin((uint8_t)(p.phase + 8)) >> 6));
        } else if (p.type == PARTICLE_LADYBUG) {
            updateLadybug(p);
        } else if (p.type == PARTICLE_NECTAR_VISITOR) {
            updateVisitor(p);
        } else if (p.type == PARTICLE_LOAM_SNAIL) {
            updateLoamSnail(p);
        } else if (p.type == PARTICLE_MIST_DROP) {
            // Glass droplets cling, then run downward. Falling rain mist is already faster.
            if (p.vy < 10) {
                uint16_t lived = (uint16_t)(p.maxLife - p.life);
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

        if (p.type == PARTICLE_POLLEN_MOTE) {
            tryCrossPollinate(p, i, now);
        }

        // Screen boundary checks (Canopy space: HUD_HEIGHT to SCREEN_HEIGHT - 2)
        int16_t px = p.x >> 4;
        int16_t py = p.y >> 4;

        // Local pollen that drifts off a side is offered to the nearest desk neighbor.
        // Foreign spores are not forwarded, so a mote cannot bounce between units.
        if (p.type == PARTICLE_POLLEN_MOTE && !_lineage[i].foreign &&
            (px < 0 || px >= SCREEN_WIDTH) &&
            py >= HUD_HEIGHT && py < (SCREEN_HEIGHT - 2)) {
            const PlantGenome& genome = Plant.getGenome();
            Mesh.handoffSpore((px < 0) ? MESH_EDGE_LEFT : MESH_EDGE_RIGHT,
                              py, p.vx, p.vy,
                              genome.seed, genome.phenotype, genome.petalPalette,
                              genome.stemHue, genome.petalCount, Plant.getPetalColor());
            p.active = false;
            _lineage[i].foreign = false;
            continue;
        }

        if (p.type == PARTICLE_SEED_MOTE &&
            px >= 0 && px < SCREEN_WIDTH &&
            py >= (SCREEN_HEIGHT - SUBSTRATE_HEIGHT)) {
            Plant.depositSoilSeedPod(px);
            p.active = false;
            continue;
        }

        if (px < 0 || px >= SCREEN_WIDTH || py < HUD_HEIGHT || py >= (SCREEN_HEIGHT - 2)) {
            p.active = false;
        }
    }
}

void ParticleSystem::tryCrossPollinate(Particle& mote, uint8_t index, uint32_t now) {
    if (mote.type != PARTICLE_POLLEN_MOTE) return;

    uint16_t lived = (uint16_t)(mote.maxLife - mote.life);
    if (lived < POLLEN_GRACE_FRAMES) return;
    if ((uint32_t)(now - _lastPollination) < POLLINATION_COOLDOWN_MS) return;

    int16_t hx = (int16_t)(mote.x >> 4);
    int16_t hy = (int16_t)(mote.y >> 4);
    if (!Plant.touchesNeighborCanopy(hx, hy, mote.tag)) return;

    if (index < MAX_PARTICLES && _lineage[index].foreign) {
        ForeignParent parent;
        parent.seed = _lineage[index].seed;
        parent.phenotype = _lineage[index].phenotype;
        parent.petalPalette = _lineage[index].petalPalette;
        parent.stemHue = _lineage[index].stemHue;
        parent.petalColor = _lineage[index].petalColor;
        char code[20];
        PlantGenome child;
        if (Plant.composeHybridSeedCode(parent, code, &child)) {
            Mesh.rememberSeed(code, MESH_SEED_HYBRID, child.phenotype, child.petalPalette, child.stemHue);
            #if ENABLE_SERIAL_LOG
            Serial.printf("[MESH] Hybrid seed %s\n", code);
            #endif
        }
        _lineage[index].foreign = false;
    }

    int8_t spark = findFreeSlot();
    bool carrySparkle = true;
    if (spark >= 0) {
        activateGoldenSparkle(_pool[spark], mote.x, mote.y);
        carrySparkle = false;
    }
    releaseSeedMote(mote, carrySparkle);
    _lastPollination = now;
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
            // Golden grain. A spore that crossed from a neighbor keeps a cool arrival glint.
            bool inbound = (p.tag == 0xFE);
            uint16_t color = inbound ? rgb565(255, 210, 90) : rgb565(255, 225, 60);
            buffer[py * screenWidth + px] = color;
            if (inbound && px > 0) {
                buffer[py * screenWidth + (px - 1)] = rgb565(120, 230, 255);
            } else if (py > 0 && px > 0 && (p.phase & 4)) {
                buffer[(py - 1) * screenWidth + px] = rgb565(200, 160, 30);
            }
        }
        else if (p.type == PARTICLE_SEED_MOTE) {
            uint16_t color = rgb565(175, 125, 45);
            buffer[py * screenWidth + px] = color;
            uint16_t lived = (uint16_t)(p.maxLife - p.life);
            if (p.size >= 2 && lived < 12) {
                uint16_t spark = rgb565(255, 220, 80);
                if (py > 0) buffer[(py - 1) * screenWidth + px] = spark;
                if (py + 1 < screenHeight) buffer[(py + 1) * screenWidth + px] = spark;
                if (px > 0) buffer[py * screenWidth + (px - 1)] = spark;
                if (px + 1 < screenWidth) buffer[py * screenWidth + (px + 1)] = spark;
            }
        }
        else if (p.type == PARTICLE_GOLDEN_SPARKLE) {
            uint16_t core = rgb565(255, 236, 120);
            uint16_t arm = rgb565(196, 150, 40);
            buffer[py * screenWidth + px] = core;
            if (py > 0) buffer[(py - 1) * screenWidth + px] = arm;
            if (py + 1 < screenHeight) buffer[(py + 1) * screenWidth + px] = arm;
            if (px > 0) buffer[py * screenWidth + (px - 1)] = arm;
            if (px + 1 < screenWidth) buffer[py * screenWidth + (px + 1)] = arm;
            if ((p.phase & 4) != 0) {
                uint16_t twinkle = rgb565(255, 210, 70);
                if (py > 0 && px > 0) buffer[(py - 1) * screenWidth + (px - 1)] = twinkle;
                if (py > 0 && px + 1 < screenWidth) buffer[(py - 1) * screenWidth + (px + 1)] = twinkle;
                if (py + 1 < screenHeight && px > 0) buffer[(py + 1) * screenWidth + (px - 1)] = twinkle;
                if (py + 1 < screenHeight && px + 1 < screenWidth) buffer[(py + 1) * screenWidth + (px + 1)] = twinkle;
            }
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
        else if (p.type == PARTICLE_NOCTURNAL_SPORE) {
            uint16_t age = (uint16_t)(p.maxLife - p.life);
            bool burst = (age % 96) < 12;
            uint16_t color = burst ? rgb565(186, 255, 236) : rgb565(36, 128, 118);
            buffer[py * screenWidth + px] = color;
            if (burst && p.size >= 2) {
                uint16_t glow = rgb565(24, 72, 68);
                if (py > 0) buffer[(py - 1) * screenWidth + px] = glow;
                if (py + 1 < screenHeight) buffer[(py + 1) * screenWidth + px] = glow;
                if (px > 0) buffer[py * screenWidth + (px - 1)] = glow;
                if (px + 1 < screenWidth) buffer[py * screenWidth + (px + 1)] = glow;
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
        else if (p.type == PARTICLE_FALLING_LEAF) {
            uint16_t body = rgb565(176, 72, 28);
            uint16_t shade = rgb565(120, 42, 16);
            if (p.tag == 1) {
                body = rgb565(204, 148, 40);
                shade = rgb565(150, 96, 24);
            } else if (p.tag >= 2) {
                body = rgb565(168, 28, 44);
                shade = rgb565(110, 16, 28);
            }
            plotParticle(buffer, screenWidth, screenHeight, px, py, body);
            if ((p.phase & 8) != 0) {
                plotParticle(buffer, screenWidth, screenHeight, (int16_t)(px - 1), py, body);
                plotParticle(buffer, screenWidth, screenHeight, (int16_t)(px + 1), py, body);
                plotParticle(buffer, screenWidth, screenHeight, px, (int16_t)(py - 1), shade);
            } else {
                plotParticle(buffer, screenWidth, screenHeight, px, (int16_t)(py - 1), body);
                plotParticle(buffer, screenWidth, screenHeight, px, (int16_t)(py + 1), shade);
                plotParticle(buffer, screenWidth, screenHeight, (int16_t)(px + 1), py, shade);
            }
        }
        else if (p.type == PARTICLE_BLOSSOM_PETAL) {
            uint16_t body = (p.tag & 1) ? rgb565(255, 236, 242) : rgb565(255, 168, 196);
            uint16_t shade = rgb565(226, 120, 156);
            plotParticle(buffer, screenWidth, screenHeight, px, py, body);
            if ((p.phase & 16) != 0) {
                plotParticle(buffer, screenWidth, screenHeight, (int16_t)(px + 1), py, body);
                plotParticle(buffer, screenWidth, screenHeight, (int16_t)(px + 1), (int16_t)(py + 1), shade);
            } else {
                plotParticle(buffer, screenWidth, screenHeight, (int16_t)(px - 1), (int16_t)(py + 1), body);
                plotParticle(buffer, screenWidth, screenHeight, px, (int16_t)(py + 1), shade);
            }
        }
        else if (p.type == PARTICLE_LADYBUG || p.type == PARTICLE_NECTAR_VISITOR || p.type == PARTICLE_LOAM_SNAIL) {
            renderCreature(buffer, screenWidth, screenHeight, p);
        }
    }
}
