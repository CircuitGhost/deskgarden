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

// Crockford's Base32 Alphabet (No ambiguous 0/O, 1/I/L)
static const char BASE32_ALPHABET[] = "0123456789ABCDEFGHJKMNPQRSTVWXYZ";

static int8_t base32CharToVal(char c) {
    if (c >= 'a' && c <= 'z') c -= 32;
    if (c == 'O') c = '0';
    if (c == 'I' || c == 'L') c = '1';
    for (uint8_t i = 0; i < 32; i++) {
        if (BASE32_ALPHABET[i] == c) return i;
    }
    return -1;
}

PlantEngine::PlantEngine()
    : _segmentCount(0),
      _flowerCount(0),
      _growthProgress(1.0f),
      _targetGrowth(1.0f),
      _diurnalOpenTarget(1.0f),
      _currentDiurnalOpen(1.0f),
      _windPhase(0),
      _gustPhase(0),
      _curWindOffset(0),
      _prngState(123456789) {}

void PlantEngine::begin() {
    _growthProgress = 1.0f;
    _targetGrowth = 1.0f;
    _diurnalOpenTarget = 1.0f;
    _currentDiurnalOpen = 1.0f;
    _windPhase = 0;
    _gustPhase = 0;
    _curWindOffset = 0;

    // Default botanical specimen genome (Highland Orchid)
    PlantGenome defaultGenome;
    defaultGenome.seed = 0x8BADF00D;
    defaultGenome.phenotype = PHENOTYPE_ORCHID;
    defaultGenome.maxDepth = 4;
    defaultGenome.baseLength = 52;
    defaultGenome.branchAngle = 26;
    defaultGenome.lengthDecayPct = 74;
    defaultGenome.stemHue = 0;         // Sage Green
    defaultGenome.leafShape = 1;       // Lanceolate
    defaultGenome.foliageDensity = 80;
    defaultGenome.swayFlexibility = 3;
    defaultGenome.petalPalette = 0;    // Magenta Orchid
    defaultGenome.petalCount = 5;

    generatePlant(defaultGenome);
}

uint32_t PlantEngine::nextRandom() {
    _prngState = (_prngState * 1664525u + 1013904223u);
    return _prngState;
}

uint32_t PlantEngine::randomRange(uint32_t minVal, uint32_t maxVal) {
    if (minVal >= maxVal) return minVal;
    return minVal + (nextRandom() % (maxVal - minVal + 1));
}

const char* PlantEngine::getPhenotypeName() const {
    switch (_genome.phenotype) {
        case PHENOTYPE_ORCHID: return "Highland Orchid";
        case PHENOTYPE_FERN: return "Dwarf Fern";
        case PHENOTYPE_SUCCULENT: return "Micro-Succulent";
        case PHENOTYPE_BONSAI: return "Ancient Bonsai";
        default: return "Flora";
    }
}

void PlantEngine::configureFlowerStyle() {
    _flowerStyle.petalCount = _genome.petalCount;
    if (_flowerStyle.petalCount < 3) _flowerStyle.petalCount = 3;
    if (_flowerStyle.petalCount > 8) _flowerStyle.petalCount = 8;

    _flowerStyle.petalLength = 6 + (_genome.phenotype == PHENOTYPE_ORCHID ? 3 : 0);
    _flowerStyle.petalWidth = 3 + (_genome.phenotype == PHENOTYPE_SUCCULENT ? 2 : 0);
    _flowerStyle.centerColor = rgb565(255, 215, 30); // Bright golden stamen

    switch (_genome.petalPalette) {
        case 1: // Cherry Blossom
            _flowerStyle.petalColor = rgb565(255, 140, 180);
            _flowerStyle.petalHighlight = rgb565(255, 220, 235);
            break;
        case 2: // Solar Amber
            _flowerStyle.petalColor = rgb565(255, 140, 20);
            _flowerStyle.petalHighlight = rgb565(255, 230, 90);
            break;
        case 3: // Bioluminescent Cyan
            _flowerStyle.petalColor = rgb565(30, 210, 240);
            _flowerStyle.petalHighlight = rgb565(190, 255, 255);
            break;
        case 4: // Moon Lily
            _flowerStyle.petalColor = rgb565(225, 230, 255);
            _flowerStyle.petalHighlight = rgb565(255, 255, 255);
            break;
        case 0: // Magenta Orchid (Default)
        default:
            _flowerStyle.petalColor = rgb565(230, 45, 140);
            _flowerStyle.petalHighlight = rgb565(255, 180, 220);
            break;
    }
}

void PlantEngine::generateFromSeed(uint32_t seed, uint8_t phenotype) {
    _prngState = seed;
    PlantGenome g;
    g.seed = seed;
    g.phenotype = (phenotype <= 3) ? phenotype : (randomRange(0, 3));
    g.maxDepth = randomRange(3, 4);
    g.baseLength = randomRange(42, 56);
    g.branchAngle = randomRange(18, 32);
    g.lengthDecayPct = randomRange(68, 76);
    g.stemHue = randomRange(0, 3);
    g.leafShape = randomRange(0, 2);
    g.foliageDensity = randomRange(65, 90);
    g.swayFlexibility = randomRange(2, 4);
    g.petalPalette = randomRange(0, 4);
    g.petalCount = (g.phenotype == PHENOTYPE_ORCHID) ? 5 : (randomRange(4, 6));

    generatePlant(g);
}

void PlantEngine::generatePlant(const PlantGenome& genome) {
    _genome = genome;
    _prngState = genome.seed;
    _segmentCount = 0;
    _flowerCount = 0;

    configureFlowerStyle();

    int16_t rootX = SCREEN_WIDTH / 2;
    int16_t rootY = SCREEN_HEIGHT - SUBSTRATE_HEIGHT;

    // Grow initial trunk (angle 192 = straight UP)
    growSegmentRecursive(rootX, rootY, 192, 0, -1, genome.baseLength);
}

void PlantEngine::growSegmentRecursive(int16_t startX, int16_t startY, uint8_t angle, 
                                      uint8_t depth, int8_t parentIdx, float length) {
    if (_segmentCount >= MAX_PLANT_SEGMENTS || depth > 3 || length < 8.0f) {
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
    seg.isTerminal = true;
    seg.flowerIndex = -1;

    // Calculate tip endpoint
    int16_t dx = (lutCos(angle) * (int16_t)length) / 127;
    int16_t dy = (lutSin(angle) * (int16_t)length) / 127;
    seg.x1 = startX + dx;
    seg.y1 = startY + dy;

    seg.curX0 = seg.x0;
    seg.curY0 = seg.y0;
    seg.curX1 = seg.x1;
    seg.curY1 = seg.y1;

    // Organic thickness tapering: Trunk (4px) -> Main Branch (3px) -> Twigs (1-2px)
    if (depth == 0) seg.thickness = 4;
    else if (depth == 1) seg.thickness = 3;
    else if (depth == 2) seg.thickness = 2;
    else seg.thickness = 1;

    // Stem & Leaf palettes
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

    // Leaf attachment on side stems (depth 1 & 2)
    seg.hasLeaf = false;
    if (depth >= 1 && (randomRange(0, 100) < _genome.foliageDensity)) {
        seg.hasLeaf = true;
        seg.leafAngle = (angle + ((angle < 192) ? -45 : 45)) & 255;
        seg.leafLength = (uint8_t)randomRange(6, 10);
        seg.leafWidth = (uint8_t)randomRange(3, 5);
    }

    // Phototropic Branching Hierarchy
    if (depth < 2) {
        seg.isTerminal = false;
        float nextLength = (length * 75) / 100.0f;

        if (depth == 0) {
            // Main trunk splits into Left, Right, and Center upward shoots
            growSegmentRecursive(seg.x1, seg.y1, 174 /* left-up */, depth + 1, curIdx, nextLength);
            growSegmentRecursive(seg.x1, seg.y1, 210 /* right-up */, depth + 1, curIdx, nextLength);
            growSegmentRecursive(seg.x1, seg.y1, 192 /* straight up */, depth + 1, curIdx, nextLength * 0.95f);
        } else {
            // Sub-branches: phototropically steered upward (angles 155 to 229)
            int16_t bLeft = (angle < 192) ? (angle - 12) : (angle - 18);
            int16_t bRight = (angle > 192) ? (angle + 12) : (angle + 18);

            // Clamp angles between 150 (-60 deg) and 234 (+60 deg) so they never point down
            if (bLeft < 150) bLeft = 150;
            if (bRight > 234) bRight = 234;

            growSegmentRecursive(seg.x1, seg.y1, (uint8_t)(bLeft & 255), depth + 1, curIdx, nextLength);
            growSegmentRecursive(seg.x1, seg.y1, (uint8_t)(bRight & 255), depth + 1, curIdx, nextLength);
        }
    } else {
        // Terminal tip: Blossom placement on the highest, most prominent branch tips
        if (_flowerCount < 5 && _genome.phenotype != PHENOTYPE_FERN) {
            uint8_t fIdx = _flowerCount++;
            seg.flowerIndex = fIdx;
            FlowerInstance& flower = _flowers[fIdx];
            flower.terminalSegIdx = curIdx;
            flower.x = seg.x1;
            flower.y = seg.y1;
            flower.stage = BLOOM_MATURE;
            flower.bloomProgress = 1.0f;
            flower.diurnalOpenRatio = 1.0f;
            flower.pollinated = false;
            flower.age = 0;
        }
    }
}

// 16-Character Seed Code Engine (Crockford Base32: XXXX-XXXX-XXXX-XXXX)
void PlantEngine::getSeedCode(char* outCode20) const {
    if (!outCode20) return;

    // Pack 80-bit genetic payload into uint32_t chunks
    uint32_t p0 = _genome.seed;
    uint32_t p1 = ((uint32_t)(_genome.phenotype & 0x03) << 28) |
                  ((uint32_t)(_genome.maxDepth & 0x07) << 25) |
                  ((uint32_t)(_genome.baseLength & 0x7F) << 18) |
                  ((uint32_t)(_genome.branchAngle & 0x3F) << 12) |
                  ((uint32_t)(_genome.lengthDecayPct & 0x3F) << 6) |
                  ((uint32_t)(_genome.stemHue & 0x03) << 4) |
                  ((uint32_t)(_genome.petalPalette & 0x07) << 1) |
                  ((uint32_t)(_genome.petalCount & 0x01));

    uint16_t p2 = ((uint16_t)(_genome.leafShape & 0x03) << 14) |
                  ((uint16_t)(_genome.foliageDensity & 0x7F) << 7) |
                  ((uint16_t)(_genome.swayFlexibility & 0x07) << 4);

    // Compute simple checksum
    uint8_t checksum = (uint8_t)((p0 ^ (p0 >> 16) ^ p1 ^ (p1 >> 16) ^ p2) & 0x1F);
    p2 |= (checksum & 0x0F);

    // Encode 16 Base32 symbols with hyphens: XXXX-XXXX-XXXX-XXXX
    uint64_t high64 = ((uint64_t)p0 << 32) | p1;
    uint16_t low16 = p2;

    char raw16[17];
    for (int i = 11; i >= 0; i--) {
        raw16[i] = BASE32_ALPHABET[high64 & 0x1F];
        high64 >>= 5;
    }
    for (int i = 15; i >= 12; i--) {
        raw16[i] = BASE32_ALPHABET[low16 & 0x1F];
        low16 >>= 5;
    }
    raw16[16] = '\0';

    snprintf(outCode20, 20, "%.4s-%.4s-%.4s-%.4s", 
             &raw16[0], &raw16[4], &raw16[8], &raw16[12]);
}

bool PlantEngine::loadSeedCode(const char* inCode) {
    if (!inCode) return false;

    char clean[17];
    uint8_t cIdx = 0;
    for (int i = 0; inCode[i] != '\0' && cIdx < 16; i++) {
        if (inCode[i] != '-' && inCode[i] != ' ') {
            clean[cIdx++] = inCode[i];
        }
    }
    if (cIdx < 16) return false;
    clean[16] = '\0';

    uint64_t high64 = 0;
    for (int i = 0; i < 12; i++) {
        int8_t v = base32CharToVal(clean[i]);
        if (v < 0) return false;
        high64 = (high64 << 5) | (uint64_t)v;
    }

    uint16_t low16 = 0;
    for (int i = 12; i < 16; i++) {
        int8_t v = base32CharToVal(clean[i]);
        if (v < 0) return false;
        low16 = (low16 << 5) | (uint16_t)v;
    }

    uint32_t p0 = (uint32_t)(high64 >> 32);
    uint32_t p1 = (uint32_t)(high64 & 0xFFFFFFFF);
    uint16_t p2 = low16;

    PlantGenome g;
    g.seed = p0;
    g.phenotype = (p1 >> 28) & 0x03;
    g.maxDepth = (p1 >> 25) & 0x07;
    g.baseLength = (p1 >> 18) & 0x7F;
    g.branchAngle = (p1 >> 12) & 0x3F;
    g.lengthDecayPct = (p1 >> 6) & 0x3F;
    g.stemHue = (p1 >> 4) & 0x03;
    g.petalPalette = (p1 >> 1) & 0x07;
    g.petalCount = (p1 & 0x01) ? 5 : 4;
    g.leafShape = (p2 >> 14) & 0x03;
    g.foliageDensity = (p2 >> 7) & 0x7F;
    g.swayFlexibility = (p2 >> 4) & 0x07;

    generatePlant(g);
    return true;
}

void PlantEngine::setGrowthProgress(float progress) {
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    _growthProgress = progress;
    _targetGrowth = progress;
}

void PlantEngine::updateLifecycle(uint32_t deltaMs, TimePhase phase) {
    // 1. Target growth interpolation
    if (_growthProgress < _targetGrowth) {
        _growthProgress += (deltaMs * 0.0002f);
        if (_growthProgress > _targetGrowth) _growthProgress = _targetGrowth;
    }

    // 2. Diurnal Petal Posture target
    switch (phase) {
        case PHASE_DAWN:
            _diurnalOpenTarget = 0.75f;
            break;
        case PHASE_DAYLIGHT:
            _diurnalOpenTarget = 1.0f;
            break;
        case PHASE_GOLDEN_HOUR:
            _diurnalOpenTarget = 0.60f;
            break;
        case PHASE_DUSK:
            _diurnalOpenTarget = 0.20f;
            break;
        case PHASE_NIGHT:
        default:
            _diurnalOpenTarget = 0.05f; // Sleep posture
            break;
    }

    // Smooth ease towards target diurnal posture
    float diff = _diurnalOpenTarget - _currentDiurnalOpen;
    _currentDiurnalOpen += diff * (deltaMs * 0.003f);

    for (uint8_t i = 0; i < _flowerCount; i++) {
        _flowers[i].diurnalOpenRatio = _currentDiurnalOpen;
    }
}

void PlantEngine::fertilizeFlower(uint8_t flowerIdx) {
    if (flowerIdx < _flowerCount) {
        _flowers[flowerIdx].pollinated = true;
        _flowers[flowerIdx].stage = BLOOM_SEED_POD;
    }
}

void PlantEngine::updatePhysics(uint32_t deltaMs, float windStrength) {
    // Calm, slow phase advancement (~8-10 seconds per gentle breath cycle)
    static uint8_t tickCount = 0;
    tickCount++;
    if (tickCount & 1) {
        _windPhase++;
    }
    if ((tickCount & 3) == 0) {
        _gustPhase++;
    }

    // Gentle organic ambient breath: Primary slow wave + subtle harmonic
    int16_t primaryWave = lutSin(_windPhase);                             // -127 to +127
    int16_t harmonicWave = lutSin((uint8_t)(_windPhase * 2 + 48)) >> 3;  // Subtle harmonic flutter
    int16_t totalBreeze = primaryWave + harmonicWave;                    // -143 to +143

    // Coherent forward kinematics (whole plant sways harmoniously with breeze)
    for (uint8_t i = 0; i < _segmentCount; i++) {
        PlantSegment& seg = _segments[i];
        if (!seg.active) continue;

        if (seg.parentIndex < 0) {
            // Root Trunk: Anchored to soil, gentle base flex
            seg.curX0 = seg.x0;
            seg.curY0 = seg.y0;

            int16_t trunkSway = (totalBreeze * _genome.swayFlexibility) >> 8; // ~1-2 angle units (~1.5 deg)
            uint8_t effAngle = (uint8_t)((seg.angle + trunkSway) & 255);

            int16_t curLen = (int16_t)(seg.length * _growthProgress);
            seg.curX1 = seg.curX0 + (lutCos(effAngle) * curLen) / 127;
            seg.curY1 = seg.curY0 + (lutSin(effAngle) * curLen) / 127;
        } else {
            // Child Branch Segment: Coherently leans with parent
            const PlantSegment& parent = _segments[seg.parentIndex];
            seg.curX0 = parent.curX1;
            seg.curY0 = parent.curY1;

            int16_t branchSway = (totalBreeze * _genome.swayFlexibility * (seg.depth + 1)) >> 8;
            uint8_t effAngle = (uint8_t)((seg.angle + branchSway) & 255);

            int16_t curLen = (int16_t)(seg.length * _growthProgress);
            seg.curX1 = seg.curX0 + (lutCos(effAngle) * curLen) / 127;
            seg.curY1 = seg.curY0 + (lutSin(effAngle) * curLen) / 127;
        }

        // Update attached flower world coordinates
        if (seg.flowerIndex >= 0 && seg.flowerIndex < _flowerCount) {
            _flowers[seg.flowerIndex].x = seg.curX1;
            _flowers[seg.flowerIndex].y = seg.curY1;
        }
    }
}

void PlantEngine::drawThickLine(uint16_t* buffer, int16_t x0, int16_t y0, 
                               int16_t x1, int16_t y1, uint8_t thickness, uint16_t color, 
                               int16_t bufWidth, int16_t bufHeight) {
    if (!buffer) return;

    if (thickness <= 1) {
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

    uint8_t perpAngle = (angle + 64) & 255;
    int16_t perpDx = (lutCos(perpAngle) * (int16_t)width) / 127;
    int16_t perpDy = (lutSin(perpAngle) * (int16_t)width) / 127;

    int16_t midX = x + (tipDx >> 1);
    int16_t midY = y + (tipDy >> 1);

    int16_t leftX = midX - perpDx;
    int16_t leftY = midY - perpDy;
    int16_t rightX = midX + perpDx;
    int16_t rightY = midY + perpDy;

    // Fill leaf polygon
    drawThickLine(buffer, x, y, leftX, leftY, 1, leafColor, bufWidth, bufHeight);
    drawThickLine(buffer, leftX, leftY, tipX, tipY, 1, leafColor, bufWidth, bufHeight);
    drawThickLine(buffer, x, y, rightX, rightY, 1, leafColor, bufWidth, bufHeight);
    drawThickLine(buffer, rightX, rightY, tipX, tipY, 1, leafColor, bufWidth, bufHeight);
    drawThickLine(buffer, leftX, leftY, rightX, rightY, 2, leafColor, bufWidth, bufHeight);

    // Center vein
    drawThickLine(buffer, x, y, tipX, tipY, 1, veinColor, bufWidth, bufHeight);
}

void PlantEngine::drawFlower(uint16_t* buffer, int16_t x, int16_t y, uint8_t angle,
                            float openRatio, uint8_t stage, int16_t bufWidth, int16_t bufHeight) {
    if (!buffer || x < -20 || x >= bufWidth + 20 || y < -20 || y >= bufHeight + 20) return;

    if (stage == BLOOM_BUD) {
        // Closed calyx bud
        drawThickLine(buffer, x, y, x, y - 4, 3, rgb565(60, 140, 60), bufWidth, bufHeight);
        drawThickLine(buffer, x, y - 4, x, y - 6, 2, _flowerStyle.petalColor, bufWidth, bufHeight);
        return;
    }

    if (stage == BLOOM_SEED_POD) {
        // Seed Pod: Dark amber calyx dropping golden seed motes
        drawThickLine(buffer, x - 2, y, x + 2, y, 3, rgb565(140, 100, 40), bufWidth, bufHeight);
        drawThickLine(buffer, x, y - 2, x, y + 2, 3, rgb565(180, 130, 50), bufWidth, bufHeight);
        return;
    }

    // Dynamic petal radius scaled by growth and diurnal openness
    int16_t pLen = (int16_t)(_flowerStyle.petalLength * (0.35f + 0.65f * openRatio));
    int16_t pWid = (int16_t)(_flowerStyle.petalWidth * (0.4f + 0.6f * openRatio));
    if (pLen < 2) pLen = 2;

    uint8_t count = _flowerStyle.petalCount;
    uint8_t angleStep = 256 / count;

    // Draw Radial Petals
    for (uint8_t p = 0; p < count; p++) {
        uint8_t pAngle = (uint8_t)((angle + (p * angleStep)) & 255);

        int16_t pTipX = x + (lutCos(pAngle) * pLen) / 127;
        int16_t pTipY = y + (lutSin(pAngle) * pLen) / 127;

        uint8_t pPerp = (pAngle + 64) & 255;
        int16_t pMidX = x + ((lutCos(pAngle) * (pLen >> 1)) / 127);
        int16_t pMidY = y + ((lutSin(pAngle) * (pLen >> 1)) / 127);

        int16_t pLeftX = pMidX - (lutCos(pPerp) * pWid) / 127;
        int16_t pLeftY = pMidY - (lutSin(pPerp) * pWid) / 127;
        int16_t pRightX = pMidX + (lutCos(pPerp) * pWid) / 127;
        int16_t pRightY = pMidY + (lutSin(pPerp) * pWid) / 127;

        drawThickLine(buffer, x, y, pLeftX, pLeftY, 1, _flowerStyle.petalColor, bufWidth, bufHeight);
        drawThickLine(buffer, pLeftX, pLeftY, pTipX, pTipY, 1, _flowerStyle.petalHighlight, bufWidth, bufHeight);
        drawThickLine(buffer, x, y, pRightX, pRightY, 1, _flowerStyle.petalColor, bufWidth, bufHeight);
        drawThickLine(buffer, pRightX, pRightY, pTipX, pTipY, 1, _flowerStyle.petalHighlight, bufWidth, bufHeight);
        drawThickLine(buffer, pLeftX, pLeftY, pRightX, pRightY, 2, _flowerStyle.petalColor, bufWidth, bufHeight);
    }

    // Central Stamen & Pistil (Golden center disc)
    if (x >= 0 && x < bufWidth && y >= 0 && y < bufHeight) {
        buffer[y * bufWidth + x] = _flowerStyle.centerColor;
        if (y > 0) buffer[(y - 1) * bufWidth + x] = _flowerStyle.centerColor;
        if (y < bufHeight - 1) buffer[(y + 1) * bufWidth + x] = _flowerStyle.centerColor;
        if (x > 0) buffer[y * bufWidth + (x - 1)] = _flowerStyle.centerColor;
        if (x < bufWidth - 1) buffer[y * bufWidth + (x + 1)] = _flowerStyle.centerColor;
    }
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

    // 3. Render Botanical Flowers (Terminal branch tips)
    for (uint8_t i = 0; i < _flowerCount; i++) {
        const FlowerInstance& flower = _flowers[i];
        if (flower.stage == BLOOM_VEGETATIVE) continue;

        drawFlower(buffer, flower.x, flower.y, 192 /* upright face to sky */, 
                   flower.diurnalOpenRatio, flower.stage, screenWidth, screenHeight);
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

uint8_t PlantEngine::getFlowerNodes(FlowerNodePos* outArray, uint8_t maxCount) const {
    if (!outArray || maxCount == 0) return 0;
    uint8_t count = 0;

    for (uint8_t i = 0; i < _flowerCount; i++) {
        if (count < maxCount) {
            outArray[count].x = _flowers[i].x;
            outArray[count].y = _flowers[i].y;
            outArray[count].stage = _flowers[i].stage;
            outArray[count].isMature = (_flowers[i].stage == BLOOM_MATURE && _flowers[i].diurnalOpenRatio > 0.5f);
            count++;
        }
    }
    return count;
}
