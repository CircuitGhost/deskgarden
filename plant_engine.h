#ifndef PLANT_ENGINE_H
#define PLANT_ENGINE_H

#include <Arduino.h>
#include "config.h"
#include "hal_display.h"

#define MAX_PLANT_SEGMENTS   64
#define MAX_TURTLE_STACK     16
#define MAX_LEAF_SPAWN_NODES 32

struct PlantGenome {
    uint32_t seed;              // Deterministic genome seed
    uint8_t maxDepth;          // Fractal iteration depth (3 - 5)
    uint8_t baseLength;        // Initial trunk length (px)
    uint8_t branchAngle;       // Branch split angle (15 - 40 degrees)
    uint8_t lengthDecayPct;    // Length decay per depth (e.g. 72%)
    uint8_t stemHue;           // 0=Forest Sage, 1=Jade Emerald, 2=Woody Olive, 3=Biolum Mint
    uint8_t leafShape;         // 0=Oval, 1=Lanceolate, 2=Heart
    uint8_t foliageDensity;    // Probability of leaves at nodes (0 - 100)
    uint8_t swayFlexibility;   // Wind responsiveness (1 - 5)
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
    uint16_t stemColor;        // RGB565 stem tone
    uint16_t leafColor;        // RGB565 leaf tone
    uint16_t leafVeinColor;    // RGB565 vein tone
    bool active;
};

struct TurtleState {
    int16_t x;
    int16_t y;
    uint8_t angle;
    uint8_t depth;
    int8_t parentIndex;
};

struct LeafNodePos {
    int16_t x;
    int16_t y;
};

class PlantEngine {
public:
    PlantEngine();
    void begin();

    // Genome initialization & Procedural generation
    void generatePlant(const PlantGenome& genome);
    void generateFromSeed(uint32_t seed);

    // Lifecycle & Growth Animation (0.0 = seed/sprout, 1.0 = fully mature)
    void setGrowthProgress(float progress);
    float getGrowthProgress() const { return _growthProgress; }
    void updateGrowth(uint32_t deltaMs);

    // Physics & Wind Sway
    void updatePhysics(uint32_t deltaMs, float windStrength = 1.0f);

    // Vector-to-Raster Framebuffer Rendering
    void render(uint16_t* buffer, int16_t screenWidth = SCREEN_WIDTH, int16_t screenHeight = SCREEN_HEIGHT);

    // Foliage telemetry for particle system (oxygen bubble spawn origins)
    uint8_t getActiveLeafNodes(LeafNodePos* outArray, uint8_t maxCount) const;

    const PlantGenome& getGenome() const { return _genome; }
    uint8_t getSegmentCount() const { return _segmentCount; }

private:
    PlantGenome _genome;
    PlantSegment _segments[MAX_PLANT_SEGMENTS];
    uint8_t _segmentCount;

    float _growthProgress;     // 0.0f to 1.0f
    float _targetGrowth;
    uint8_t _windPhase;        // Integer phase for harmonic wind
    uint8_t _gustPhase;        // Slow gust envelope phase
    int16_t _curWindOffset;

    // Fast Pseudo-Random Generator for deterministic genome execution
    uint32_t _prngState;
    uint32_t nextRandom();
    uint32_t randomRange(uint32_t minVal, uint32_t maxVal);

    // L-System parsing and recursive generation
    void growSegmentRecursive(int16_t startX, int16_t startY, uint8_t angle, 
                              uint8_t depth, int8_t parentIdx, float length);

    // Drawing primitives
    void drawThickLine(uint16_t* buffer, int16_t x0, int16_t y0, 
                       int16_t x1, int16_t y1, uint8_t thickness, uint16_t color, 
                       int16_t bufWidth, int16_t bufHeight);
    
    void drawLeaf(uint16_t* buffer, int16_t x, int16_t y, uint8_t angle, 
                  uint8_t length, uint8_t width, uint16_t leafColor, uint16_t veinColor,
                  int16_t bufWidth, int16_t bufHeight);
};

extern PlantEngine Plant;

#endif // PLANT_ENGINE_H
