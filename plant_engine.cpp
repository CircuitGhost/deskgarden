#include "plant_engine.h"

PlantEngine Plant;

// Precomputed 256-entry integer sine LUT (-127 to +127) for high-speed RISC-V kinematics
static const int8_t SIN_LUT[256] = {
       0,    3,    6,    9,   12,   16,   19,   22,   25,   28,   31,   34,   37,   40,   43,   46,
      49,   51,   54,   57,   60,   63,   65,   68,   71,   73,   76,   78,   81,   83,   85,   88,
      90,   92,   94,   96,   98,  100,  102,  104,  106,  107,  109,  111,  112,  113,  115,  116,
     117,  118,  120,  121,  122,  122,  123,  124,  125,  125,  126,  126,  126,  127,  127,  127,
     127,  127,  127,  127,  126,  126,  126,  125,  125,  124,  123,  122,  122,  121,  120,  118,
     117,  116,  115,  113,  112,  111,  109,  107,  106,  104,  102,  100,   98,   96,   94,   92,
      90,   88,   85,   83,   81,   78,   76,   73,   71,   68,   65,   63,   60,   57,   54,   51,
      49,   46,   43,   40,   37,   34,   31,   28,   25,   22,   19,   16,   12,    9,    6,    3,
       0,   -3,   -6,   -9,  -12,  -16,  -19,  -22,  -25,  -28,  -31,  -34,  -37,  -40,  -43,  -46,
     -49,  -51,  -54,  -57,  -60,  -63,  -65,  -68,  -71,  -73,  -76,  -78,  -81,  -83,  -85,  -88,
     -90,  -92,  -94,  -96,  -98, -100, -102, -104, -106, -107, -109, -111, -112, -113, -115, -116,
    -117, -118, -120, -121, -122, -122, -123, -124, -125, -125, -126, -126, -126, -127, -127, -127,
    -127, -127, -127, -127, -126, -126, -126, -125, -125, -124, -123, -122, -122, -121, -120, -118,
    -117, -116, -115, -113, -112, -111, -109, -107, -106, -104, -102, -100,  -98,  -96,  -94,  -92,
     -90,  -88,  -85,  -83,  -81,  -78,  -76,  -73,  -71,  -68,  -65,  -63,  -60,  -57,  -54,  -51,
     -49,  -46,  -43,  -40,  -37,  -34,  -31,  -28,  -25,  -22,  -19,  -16,  -12,   -9,   -6,   -3,
};

static inline int8_t lutSin(uint8_t angle) {
    return SIN_LUT[angle];
}

static inline int8_t lutCos(uint8_t angle) {
    return SIN_LUT[(angle + 64) & 255];
}

PlantEngine::PlantEngine()
    : _segmentCount(0),
      _growthProgress(1.0f),
      _targetGrowth(1.0f),
      _windPhase(0),
      _gustPhase(0),
      _curWindOffset(0),
      _prngState(123456789) {}

void PlantEngine::begin() {
    _growthProgress = 1.0f;
    _targetGrowth = 1.0f;
    _windPhase = 0;
    _gustPhase = 0;
    _curWindOffset = 0;

    // Default botanical specimen genome (Graceful branching fern/orchid hybrid)
    PlantGenome defaultGenome;
    defaultGenome.seed = 0x8BADF00D;
    defaultGenome.maxDepth = 4;
    defaultGenome.baseLength = 48;
    defaultGenome.branchAngle = 24;
    defaultGenome.lengthDecayPct = 74;
    defaultGenome.stemHue = 0;         // Sage Green
    defaultGenome.leafShape = 1;       // Lanceolate
    defaultGenome.foliageDensity = 85;
    defaultGenome.swayFlexibility = 3;

    generatePlant(defaultGenome);
}

uint32_t PlantEngine::nextRandom() {
    // 32-bit Linear Congruential PRNG
    _prngState = (_prngState * 1664525u + 1013904223u);
    return _prngState;
}

uint32_t PlantEngine::randomRange(uint32_t minVal, uint32_t maxVal) {
    if (minVal >= maxVal) return minVal;
    return minVal + (nextRandom() % (maxVal - minVal + 1));
}

void PlantEngine::generateFromSeed(uint32_t seed) {
    _prngState = seed;
    PlantGenome g;
    g.seed = seed;
    g.maxDepth = 3 + (randomRange(0, 1));
    g.baseLength = randomRange(42, 54);
    g.branchAngle = randomRange(18, 30);
    g.lengthDecayPct = randomRange(68, 78);
    g.stemHue = randomRange(0, 3);
    g.leafShape = randomRange(0, 2);
    g.foliageDensity = randomRange(65, 95);
    g.swayFlexibility = randomRange(2, 4);

    generatePlant(g);
}

void PlantEngine::generatePlant(const PlantGenome& genome) {
    _genome = genome;
    _prngState = genome.seed;
    _segmentCount = 0;

    int16_t rootX = SCREEN_WIDTH / 2;
    int16_t rootY = SCREEN_HEIGHT - SUBSTRATE_HEIGHT;

    // Grow initial trunk (angle 192 = straight UP)
    growSegmentRecursive(rootX, rootY, 192, 0, -1, genome.baseLength);
}

void PlantEngine::growSegmentRecursive(int16_t startX, int16_t startY, uint8_t angle, 
                                      uint8_t depth, int8_t parentIdx, float length) {
    if (_segmentCount >= MAX_PLANT_SEGMENTS || depth > _genome.maxDepth || length < 4.0f) {
        return;
    }

    uint8_t curIdx = _segmentCount++;
    PlantSegment& seg = _segments[curIdx];

    seg.active = true;
    seg.depth = depth;
    seg.parentIndex = parentIdx;
    seg.length = (int16_t)length;
    seg.angle = angle;
    seg.x0 = startX;
    seg.y0 = startY;

    // Calculate tip endpoint
    int16_t dx = (lutCos(angle) * (int16_t)length) / 127;
    int16_t dy = (lutSin(angle) * (int16_t)length) / 127;
    seg.x1 = startX + dx;
    seg.y1 = startY + dy;

    seg.curX0 = seg.x0;
    seg.curY0 = seg.y0;
    seg.curX1 = seg.x1;
    seg.curY1 = seg.y1;

    // Thickness tapering
    if (depth == 0) seg.thickness = 4;
    else if (depth == 1) seg.thickness = 3;
    else if (depth == 2) seg.thickness = 2;
    else seg.thickness = 1;

    // Color determination based on depth & stemHue palette
    switch (_genome.stemHue) {
        case 1: // Emerald / Jade
            seg.stemColor = (depth <= 1) ? rgb565(30, 75, 45) : rgb565(55, 145, 80);
            seg.leafColor = rgb565(40, 185, 110);
            seg.leafVeinColor = rgb565(110, 235, 170);
            break;
        case 2: // Woody Olive
            seg.stemColor = (depth <= 1) ? rgb565(75, 60, 38) : rgb565(95, 110, 48);
            seg.leafColor = rgb565(120, 155, 55);
            seg.leafVeinColor = rgb565(180, 210, 95);
            break;
        case 3: // Bioluminescent Mint
            seg.stemColor = (depth <= 1) ? rgb565(32, 65, 75) : rgb565(45, 130, 140);
            seg.leafColor = rgb565(70, 205, 195);
            seg.leafVeinColor = rgb565(185, 255, 245);
            break;
        case 0: // Forest Sage (Default)
        default:
            seg.stemColor = (depth <= 1) ? rgb565(45, 85, 40) : rgb565(70, 135, 60);
            seg.leafColor = rgb565(85, 175, 75);
            seg.leafVeinColor = rgb565(160, 230, 145);
            break;
    }

    // Leaf attachment logic on branches (depth >= 1)
    seg.hasLeaf = false;
    if (depth >= 1 && (randomRange(0, 100) < _genome.foliageDensity)) {
        seg.hasLeaf = true;
        seg.leafAngle = (angle + ((nextRandom() & 1) ? 55 : -55)) & 255;
        seg.leafLength = (uint8_t)randomRange(7, 12);
        seg.leafWidth = (uint8_t)randomRange(3, 6);
    }

    // Recursive sub-branch expansion
    float nextLength = (length * _genome.lengthDecayPct) / 100.0f;
    uint8_t splitAngle = (uint8_t)((_genome.branchAngle * 256) / 360);

    // Left child branch
    int16_t leftAngle = (int16_t)angle - splitAngle + (int16_t)randomRange(0, 8) - 4;
    growSegmentRecursive(seg.x1, seg.y1, (uint8_t)(leftAngle & 255), depth + 1, curIdx, nextLength);

    // Right child branch
    int16_t rightAngle = (int16_t)angle + splitAngle + (int16_t)randomRange(0, 8) - 4;
    growSegmentRecursive(seg.x1, seg.y1, (uint8_t)(rightAngle & 255), depth + 1, curIdx, nextLength);

    // Optional center shoot on lower depths
    if (depth == 0 || (depth == 1 && randomRange(0, 100) < 40)) {
        int16_t centerAngle = (int16_t)angle + (int16_t)randomRange(0, 8) - 4;
        growSegmentRecursive(seg.x1, seg.y1, (uint8_t)(centerAngle & 255), depth + 1, curIdx, nextLength * 0.88f);
    }
}

void PlantEngine::setGrowthProgress(float progress) {
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    _growthProgress = progress;
    _targetGrowth = progress;
}

void PlantEngine::updateGrowth(uint32_t deltaMs) {
    if (_growthProgress < _targetGrowth) {
        _growthProgress += (deltaMs * 0.0002f);
        if (_growthProgress > _targetGrowth) _growthProgress = _targetGrowth;
    }
}

void PlantEngine::updatePhysics(uint32_t deltaMs, float windStrength) {
    _windPhase += 2;
    _gustPhase += 1;

    // Gust envelope: natural breeze cadence
    int16_t gustFactor = 90 + (lutSin(_gustPhase) >> 2); // 60 to 120%

    // Calculate kinematic forward kinematics for swayed tree
    for (uint8_t i = 0; i < _segmentCount; i++) {
        PlantSegment& seg = _segments[i];
        if (!seg.active) continue;

        if (seg.parentIndex < 0) {
            // Root Trunk
            seg.curX0 = seg.x0;
            seg.curY0 = seg.y0;

            int16_t trunkSway = (lutSin(_windPhase) * gustFactor * _genome.swayFlexibility) >> 10;
            uint8_t effAngle = (uint8_t)((seg.angle + trunkSway) & 255);

            int16_t curLen = (int16_t)(seg.length * _growthProgress);
            seg.curX1 = seg.curX0 + (lutCos(effAngle) * curLen) / 127;
            seg.curY1 = seg.curY0 + (lutSin(effAngle) * curLen) / 127;
        } else {
            // Child Branch Segment (Anchored to parent tip)
            const PlantSegment& parent = _segments[seg.parentIndex];
            seg.curX0 = parent.curX1;
            seg.curY0 = parent.curY1;

            int16_t branchSway = (lutSin((uint8_t)(_windPhase + (seg.depth * 38))) * gustFactor * _genome.swayFlexibility * (seg.depth + 1)) >> 9;
            uint8_t effAngle = (uint8_t)((seg.angle + branchSway) & 255);

            int16_t curLen = (int16_t)(seg.length * _growthProgress);
            seg.curX1 = seg.curX0 + (lutCos(effAngle) * curLen) / 127;
            seg.curY1 = seg.curY0 + (lutSin(effAngle) * curLen) / 127;
        }
    }
}

void PlantEngine::drawThickLine(uint16_t* buffer, int16_t x0, int16_t y0, 
                               int16_t x1, int16_t y1, uint8_t thickness, uint16_t color, 
                               int16_t bufWidth, int16_t bufHeight) {
    if (!buffer) return;

    if (thickness <= 1) {
        // Fast integer Bresenham 1px line
        int16_t dx = abs(x1 - x0);
        int16_t sx = (x0 < x1) ? 1 : -1;
        int16_t dy = -abs(y1 - y0);
        int16_t sy = (y0 < y1) ? 1 : -1;
        int16_t err = dx + dy;

        while (true) {
            if (x0 >= 0 && x0 < bufWidth && y0 >= 0 && y0 < bufHeight) {
                buffer[y0 * bufWidth + x0] = color;
            }
            if (x0 == x1 && y0 == y1) break;
            int16_t e2 = 2 * err;
            if (e2 >= dy) { err += dy; x0 += sx; }
            if (e2 <= dx) { err += dx; y0 += sy; }
        }
    } else {
        // Multi-pass parallel Bresenham for organic botanical thickness
        int16_t halfT = thickness >> 1;
        for (int16_t off = -halfT; off <= halfT; off++) {
            int16_t ox0 = x0 + off;
            int16_t ox1 = x1 + off;

            int16_t dx = abs(ox1 - ox0);
            int16_t sx = (ox0 < ox1) ? 1 : -1;
            int16_t dy = -abs(y1 - y0);
            int16_t sy = (y0 < y1) ? 1 : -1;
            int16_t err = dx + dy;
            int16_t cx = ox0;
            int16_t cy = y0;

            while (true) {
                if (cx >= 0 && cx < bufWidth && cy >= 0 && cy < bufHeight) {
                    buffer[cy * bufWidth + cx] = color;
                }
                if (cx == ox1 && cy == y1) break;
                int16_t e2 = 2 * err;
                if (e2 >= dy) { err += dy; cx += sx; }
                if (e2 <= dx) { err += dx; cy += sy; }
            }
        }
    }
}

void PlantEngine::drawLeaf(uint16_t* buffer, int16_t x, int16_t y, uint8_t angle, 
                          uint8_t length, uint8_t width, uint16_t leafColor, uint16_t veinColor,
                          int16_t bufWidth, int16_t bufHeight) {
    if (!buffer || length <= 1) return;

    int16_t tipDx = (lutCos(angle) * (int16_t)length) / 127;
    int16_t tipDy = (lutSin(angle) * (int16_t)length) / 127;
    int16_t tipX = x + tipDx;
    int16_t tipY = y + tipDy;

    // Perpendicular vector for leaf body width
    uint8_t perpAngle = (angle + 64) & 255;
    int16_t perpDx = (lutCos(perpAngle) * (int16_t)width) / 127;
    int16_t perpDy = (lutSin(perpAngle) * (int16_t)width) / 127;

    int16_t midX = x + (tipDx >> 1);
    int16_t midY = y + (tipDy >> 1);

    // Draw diamond / oval leaf contour
    int16_t leftX = midX - perpDx;
    int16_t leftY = midY - perpDy;
    int16_t rightX = midX + perpDx;
    int16_t rightY = midY + perpDy;

    // Fill leaf interior
    drawThickLine(buffer, x, y, leftX, leftY, 1, leafColor, bufWidth, bufHeight);
    drawThickLine(buffer, leftX, leftY, tipX, tipY, 1, leafColor, bufWidth, bufHeight);
    drawThickLine(buffer, x, y, rightX, rightY, 1, leafColor, bufWidth, bufHeight);
    drawThickLine(buffer, rightX, rightY, tipX, tipY, 1, leafColor, bufWidth, bufHeight);
    drawThickLine(buffer, leftX, leftY, rightX, rightY, 2, leafColor, bufWidth, bufHeight);

    // Leaf center vein rib
    drawThickLine(buffer, x, y, tipX, tipY, 1, veinColor, bufWidth, bufHeight);
}

void PlantEngine::render(uint16_t* buffer, int16_t screenWidth, int16_t screenHeight) {
    if (!buffer || _segmentCount == 0 || _growthProgress <= 0.01f) return;

    // 1. Render All Stems & Branches (Base to tips)
    for (uint8_t i = 0; i < _segmentCount; i++) {
        const PlantSegment& seg = _segments[i];
        if (!seg.active) continue;

        drawThickLine(buffer, seg.curX0, seg.curY0, seg.curX1, seg.curY1, 
                      seg.thickness, seg.stemColor, screenWidth, screenHeight);
    }

    // 2. Render Foliage & Leaves (Only at active leaf nodes)
    for (uint8_t i = 0; i < _segmentCount; i++) {
        const PlantSegment& seg = _segments[i];
        if (!seg.active || !seg.hasLeaf) continue;

        uint8_t curLeafLen = (uint8_t)(seg.leafLength * _growthProgress);
        uint8_t curLeafWid = (uint8_t)(seg.leafWidth * _growthProgress);

        if (curLeafLen >= 2) {
            drawLeaf(buffer, seg.curX1, seg.curY1, seg.leafAngle, 
                     curLeafLen, curLeafWid, seg.leafColor, seg.leafVeinColor,
                     screenWidth, screenHeight);
        }
    }
}

uint8_t PlantEngine::getActiveLeafNodes(LeafNodePos* outArray, uint8_t maxCount) const {
    if (!outArray || maxCount == 0) return 0;
    uint8_t count = 0;

    for (uint8_t i = 0; i < _segmentCount; i++) {
        const PlantSegment& seg = _segments[i];
        if (seg.active && seg.hasLeaf && count < maxCount) {
            outArray[count].x = seg.curX1;
            outArray[count].y = seg.curY1;
            count++;
        }
    }
    return count;
}
