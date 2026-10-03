#ifndef PLANT_ENGINE_H
#define PLANT_ENGINE_H

#include <Arduino.h>
#include "config.h"
#include "hal_display.h"
#include "time_atmosphere.h"

#define MAX_PLANT_SEGMENTS   64
#define MAX_FLOWER_NODES     12
#define MAX_LEAF_SPAWN_NODES 32
#define MAX_ROOT_SEGMENTS    16

struct RootSegment {
    int16_t x0, y0;
    int16_t x1, y1;
    uint8_t thickness;
    uint16_t color;
    bool active;
};

enum PhenotypeFamily : uint8_t {
    PHENOTYPE_ORCHID = 0,       // Highland Orchid (arching stems, broad leaves, large exotic blooms)
    PHENOTYPE_FERN,             // Dwarf Fern (dense fractal fronds, fiddlehead tips, spore motes)
    PHENOTYPE_SUCCULENT,        // Micro-Succulent (compact fleshy rosette, starburst blossoms)
    PHENOTYPE_BONSAI            // Ancient Flowering Bonsai (gnarled wood, miniature flower clusters)
};

enum BloomStage : uint8_t {
    BLOOM_VEGETATIVE = 0,       // Stems/leaves only
    BLOOM_BUD,                  // Small closed bud at tips
    BLOOM_SWELLING,             // Swollen calyx showing petal color
    BLOOM_OPENING,              // Unfurling in morning light
    BLOOM_MATURE,               // Full bloom with pistil & nectar glow
    BLOOM_SEED_POD              // Fertilized seed pod releasing seeds
};

struct FlowerAttributes {
    uint8_t petalCount;         // 3, 4, 5, 6, or 8 petals
    uint8_t petalLength;        // 4 - 12 px
    uint8_t petalWidth;         // 2 - 6 px
    uint16_t petalColor;        // Main petal RGB565 tone
    uint16_t petalHighlight;    // Petal edge / tip highlight
    uint16_t centerColor;       // Stamen / Pistil color (e.g. golden yellow)
};

struct FlowerInstance {
    int16_t x, y;               // Swayed world coordinates
    uint8_t terminalSegIdx;     // Segment attachment index
    uint8_t stage;              // BloomStage
    float bloomProgress;        // 0.0 (closed bud) to 1.0 (full bloom)
    float diurnalOpenRatio;     // 0.0 (sleep) to 1.0 (awake/open)
    bool pollinated;
    uint8_t age;
};

struct PlantGenome {
    uint32_t seed;              // Deterministic genome seed
    uint8_t phenotype;          // PhenotypeFamily
    uint8_t maxDepth;          // Fractal iteration depth (3 - 5)
    uint8_t baseLength;        // Initial trunk length (px)
    uint8_t branchAngle;       // Branch split angle (15 - 40 degrees)
    uint8_t lengthDecayPct;    // Length decay per depth (e.g. 72%)
    uint8_t stemHue;           // 0=Forest Sage, 1=Jade Emerald, 2=Woody Olive, 3=Biolum Mint
    uint8_t leafShape;         // 0=Oval, 1=Lanceolate, 2=Heart
    uint8_t foliageDensity;    // Probability of leaves at nodes (0 - 100)
    uint8_t swayFlexibility;   // Wind responsiveness (1 - 5)
    uint8_t petalPalette;      // 0=Magenta Orchid, 1=Cherry Blossom, 2=Solar Amber, 3=Biolum Cyan, 4=Moon Lily
    uint8_t petalCount;        // 3 to 8
};

struct PlantSegment {
    int16_t x0, y0;            // Base position (rest)
    int16_t x1, y1;            // Tip position (rest)
    int16_t curX0, curY0;      // Current swayed base position
    int16_t curX1, curY1;      // Current swayed tip position
    int16_t length;            // Segment length
    uint8_t angle;             // 0 - 255 (192 = straight UP)
    uint8_t depth;             // Depth in branch hierarchy (0 = trunk)
    uint8_t thickness;         // Line thickness (1 - 5 px)
    int8_t parentIndex;        // Parent segment index (-1 for trunk base)
    bool hasLeaf;              // Leaf flag
    uint8_t leafAngle;         // Leaf orientation
    uint8_t leafLength;        // Leaf length
    uint8_t leafWidth;         // Leaf width
    bool isTerminal;           // Terminal tip candidate for flower bud
    int8_t flowerIndex;        // Index in _flowers array or -1
    uint16_t stemColor;        // RGB565 stem tone
    uint16_t leafColor;        // RGB565 leaf tone
    uint16_t leafVeinColor;    // RGB565 vein tone
    bool active;
};

struct LeafNodePos {
    int16_t x;
    int16_t y;
};

struct FlowerNodePos {
    int16_t x;
    int16_t y;
    uint8_t stage;
    bool isMature;
};

#define MAX_SOIL_SEED_PODS 6

struct SoilSeedPod {
    int16_t x;
    uint8_t depth;          // Pixels below the soil surface, inside the humus
    uint8_t phase;
    uint8_t age;
    bool active;
};

class PlantEngine {
public:
    PlantEngine();
    void begin();

    // Genome initialization & Procedural generation
    void generatePlant(const PlantGenome& genome);
    void generateFromSeed(uint32_t seed, uint8_t phenotype = 0);

    // 16-Character Seed Code Engine (Base32: XXXX-XXXX-XXXX-XXXX)
    void getSeedCode(char* outCode20) const;
    bool loadSeedCode(const char* inCode);

    // Lifecycle, Growth & Diurnal Blooming Animation
    void setGrowthProgress(float progress);
    float getGrowthProgress() const { return _growthProgress; }
    void updateLifecycle(uint32_t deltaMs, TimePhase phase, uint8_t hour = 12, uint8_t minute = 0);

    // Physics & Wind Sway
    void updatePhysics(uint32_t deltaMs, float windStrength = 1.0f);

    // Pollination mechanics
    void fertilizeFlower(uint8_t flowerIdx);

    // Previous tick's breeze sample (-143..143). Pollen leans with this sway.
    int16_t getWindSway() const { return _curWindOffset; }

    // True when (x, y) meets a branch or flower other than originFlower.
    bool touchesNeighborCanopy(int16_t x, int16_t y, uint8_t originFlower) const;

    // Fixed soil-strata pod. Reuses the oldest slot when the bed is full.
    bool depositSoilSeedPod(int16_t x);
    void renderSoilSeedPods(uint16_t* buffer, int16_t screenWidth, int16_t screenHeight) const;

    // Vector-to-Raster Framebuffer Rendering
    void render(uint16_t* buffer, int16_t screenWidth = SCREEN_WIDTH, int16_t screenHeight = SCREEN_HEIGHT);
    void renderRoots(uint16_t* buffer, int16_t screenWidth = SCREEN_WIDTH, int16_t screenHeight = SCREEN_HEIGHT);

    // Foliage & Flower telemetry for particle physics
    uint8_t getActiveLeafNodes(LeafNodePos* outArray, uint8_t maxCount) const;
    uint8_t getFlowerNodes(FlowerNodePos* outArray, uint8_t maxCount) const;

    const PlantGenome& getGenome() const { return _genome; }
    uint8_t getSegmentCount() const { return _segmentCount; }
    uint8_t getFlowerCount() const { return _flowerCount; }
    uint8_t getRootCount() const { return _rootCount; }
    const char* getPhenotypeName() const;

private:
    PlantGenome _genome;
    PlantSegment _segments[MAX_PLANT_SEGMENTS];
    uint8_t _segmentCount;

    RootSegment _roots[MAX_ROOT_SEGMENTS];
    uint8_t _rootCount;

    FlowerInstance _flowers[MAX_FLOWER_NODES];
    uint8_t _flowerCount;
    FlowerAttributes _flowerStyle;

    SoilSeedPod _soilPods[MAX_SOIL_SEED_PODS];

    float _growthProgress;     // 0.0f to 1.0f
    float _targetGrowth;
    float _diurnalOpenTarget;  // Diurnal target for petals (0.0=night, 1.0=day)
    float _currentDiurnalOpen; // Smoothed petal opening ratio

    uint8_t _windPhase;        // Integer phase for harmonic wind
    uint8_t _gustPhase;        // Slow gust envelope phase
    int16_t _curWindOffset;
    uint32_t _phosphoElapsedMs; // 0.1 Hz vein/petal pulse, wraps every 10s
    uint8_t _phosphoBlend;      // 0 outside 23:00–05:00, else mint/cyan mix amount

    // Fast Pseudo-Random Generator for deterministic genome execution
    uint32_t _prngState;
    uint32_t nextRandom();
    uint32_t randomRange(uint32_t minVal, uint32_t maxVal);

    // L-System parsing and recursive generation
    void growSegmentRecursive(int16_t startX, int16_t startY, uint8_t angle, 
                              uint8_t depth, int8_t parentIdx, float length);

    void generateRoots();
    void configureFlowerStyle();
    void clearSoilSeedPods();

    // Drawing primitives
    void drawThickLine(uint16_t* buffer, int16_t x0, int16_t y0, 
                       int16_t x1, int16_t y1, uint8_t thickness, uint16_t color, 
                       int16_t bufWidth, int16_t bufHeight);
    
    void drawLeaf(uint16_t* buffer, int16_t x, int16_t y, uint8_t angle, 
                  uint8_t length, uint8_t width, uint16_t leafColor, uint16_t veinColor,
                  int16_t bufWidth, int16_t bufHeight);

    void drawFlower(uint16_t* buffer, int16_t x, int16_t y, uint8_t angle,
                    float openRatio, uint8_t stage, int16_t bufWidth, int16_t bufHeight);
};

extern PlantEngine Plant;

#endif // PLANT_ENGINE_H
