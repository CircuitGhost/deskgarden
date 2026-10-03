#include "plant_engine.h"
#include "moisture_system.h"

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

// Pull a stored RGB565 color a little toward bioluminescent mint/cyan. amount is 0..255.
static uint16_t blendMintCyan(uint16_t base, uint8_t amount) {
    if (amount == 0) return base;

    int16_t r = (base >> 11) & 0x1F;
    int16_t g = (base >> 5) & 0x3F;
    int16_t b = base & 0x1F;
    const int16_t mr = 110 >> 3;  // ~110
    const int16_t mg = 250 >> 2;  // ~250
    const int16_t mb = 220 >> 3;  // ~220

    r += ((mr - r) * amount) / 255;
    g += ((mg - g) * amount) / 255;
    b += ((mb - b) * amount) / 255;
    if (r < 0) r = 0;
    if (g < 0) g = 0;
    if (b < 0) b = 0;
    if (r > 31) r = 31;
    if (g > 63) g = 63;
    if (b > 31) b = 31;
    return (uint16_t)((r << 11) | (g << 5) | b);
}

static uint16_t blendRgb565(uint16_t base, uint16_t tint, uint8_t amount) {
    if (amount == 0) return base;

    int16_t r = (base >> 11) & 0x1F;
    int16_t g = (base >> 5) & 0x3F;
    int16_t b = base & 0x1F;
    int16_t tr = (tint >> 11) & 0x1F;
    int16_t tg = (tint >> 5) & 0x3F;
    int16_t tb = tint & 0x1F;

    r += ((tr - r) * amount) / 255;
    g += ((tg - g) * amount) / 255;
    b += ((tb - b) * amount) / 255;
    if (r < 0) r = 0;
    if (g < 0) g = 0;
    if (b < 0) b = 0;
    if (r > 31) r = 31;
    if (g > 63) g = 63;
    if (b > 31) b = 31;
    return (uint16_t)((r << 11) | (g << 5) | b);
}

void PlantEngine::cacheSeason(uint8_t month, uint8_t day) {
    _seasonKnown = seasonFromMonth(month, _season);
    _seasonDepth = _seasonKnown ? seasonProgress(month, day) : 0;
}

uint16_t PlantEngine::seasonalStemColor(const PlantSegment& seg) const {
    if (!_seasonKnown) return seg.stemColor;
    switch (_season) {
        case SEASON_AUTUMN:
            return blendRgb565(seg.stemColor, rgb565(120, 62, 28), (uint8_t)(70 + _seasonDepth / 5));
        case SEASON_WINTER:
            return blendRgb565(seg.stemColor, rgb565(86, 104, 112), 90);
        case SEASON_SPRING:
            if (seg.depth >= 2) return rgb565(132, 220, 48);
            return blendRgb565(seg.stemColor, rgb565(70, 170, 55), 100);
        case SEASON_SUMMER:
            return (seg.depth <= 1) ? rgb565(8, 78, 42) : rgb565(14, 128, 64);
    }
    return seg.stemColor;
}

uint16_t PlantEngine::seasonalLeafColor(const PlantSegment& seg, uint8_t index) const {
    if (!_seasonKnown) return seg.leafColor;
    switch (_season) {
        case SEASON_AUTUMN: {
            uint16_t warm = (index % 3 == 0) ? rgb565(176, 72, 28) :
                            (index % 3 == 1) ? rgb565(204, 146, 36) :
                                               rgb565(168, 24, 42);
            return blendRgb565(seg.leafColor, warm, (uint8_t)(160 + (_seasonDepth / 3)));
        }
        case SEASON_WINTER:
            return blendRgb565(seg.leafColor, rgb565(176, 196, 198), 150);
        case SEASON_SPRING:
            if (seg.depth >= 2) return rgb565(186, 255, 64);
            return blendRgb565(seg.leafColor, rgb565(140, 230, 70), 180);
        case SEASON_SUMMER:
            return (seg.depth <= 1) ? rgb565(0, 120, 58) : rgb565(12, 176, 82);
    }
    return seg.leafColor;
}

uint16_t PlantEngine::seasonalVeinColor(const PlantSegment& seg) const {
    if (!_seasonKnown) return seg.leafVeinColor;
    switch (_season) {
        case SEASON_AUTUMN:
            return blendRgb565(seg.leafVeinColor, rgb565(236, 176, 64), (uint8_t)(120 + _seasonDepth / 4));
        case SEASON_WINTER:
            return rgb565(214, 232, 240);
        case SEASON_SPRING:
            return (seg.depth >= 2) ? rgb565(230, 255, 140) : rgb565(190, 245, 110);
        case SEASON_SUMMER:
            return rgb565(150, 235, 90);
    }
    return seg.leafVeinColor;
}

void PlantEngine::applySeasonalBloom(uint8_t flowerIndex, uint8_t& faceAngle) {
    faceAngle = 192;
    if (!_seasonKnown || flowerIndex >= _flowerCount) return;
    if (_flowers[flowerIndex].stage == BLOOM_SEED_POD) return;

    switch (_season) {
        case SEASON_WINTER:
            if ((flowerIndex & 1) == 0) {
                _flowerStyle.petalColor = rgb565(236, 244, 250);
                _flowerStyle.petalHighlight = rgb565(186, 214, 230);
                _flowerStyle.centerColor = rgb565(168, 206, 120);
                _flowerStyle.petalCount = 6;
                _flowerStyle.petalLength = 7;
                faceAngle = 64;
            } else {
                _flowerStyle.petalColor = rgb565(196, 22, 40);
                _flowerStyle.petalHighlight = rgb565(255, 92, 64);
                _flowerStyle.centerColor = rgb565(255, 196, 48);
                _flowerStyle.petalCount = 6;
                _flowerStyle.petalLength = 9;
                faceAngle = 192;
            }
            break;
        case SEASON_SPRING:
            _flowerStyle.petalColor = rgb565(255, 170, 196);
            _flowerStyle.petalHighlight = rgb565(255, 228, 238);
            _flowerStyle.centerColor = rgb565(255, 214, 72);
            break;
        case SEASON_SUMMER:
            _flowerStyle.petalColor = rgb565(255, 168, 20);
            _flowerStyle.petalHighlight = rgb565(255, 236, 110);
            _flowerStyle.centerColor = rgb565(255, 60, 130);
            if (_flowerStyle.petalLength < 8) _flowerStyle.petalLength = 8;
            break;
        case SEASON_AUTUMN:
            _flowerStyle.petalColor = blendRgb565(_flowerStyle.petalColor, rgb565(186, 54, 28),
                                                  (uint8_t)(90 + _seasonDepth / 4));
            _flowerStyle.petalHighlight = blendRgb565(_flowerStyle.petalHighlight, rgb565(220, 140, 40), 100);
            break;
    }
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
      _phosphoElapsedMs(0),
      _phosphoBlend(0),
      _seasonKnown(false),
      _season(SEASON_SUMMER),
      _seasonDepth(0),
      _frostSparkle(0),
      _prngState(123456789) {
    clearSoilSeedPods();
}

void PlantEngine::begin() {
    _growthProgress = 1.0f;
    _targetGrowth = 1.0f;
    _diurnalOpenTarget = 1.0f;
    _currentDiurnalOpen = 1.0f;
    _windPhase = 0;
    _gustPhase = 0;
    _curWindOffset = 0;
    _phosphoElapsedMs = 0;
    _phosphoBlend = 0;
    _seasonKnown = false;
    _season = SEASON_SUMMER;
    _seasonDepth = 0;
    _frostSparkle = 0;

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
    _rootCount = 0;
    clearSoilSeedPods();

    configureFlowerStyle();
    generateRoots();

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

static uint16_t paletteColor(uint8_t index) {
    switch (index) {
        case 1: return rgb565(255, 140, 180); // Cherry Blossom
        case 2: return rgb565(255, 140, 20);  // Solar Amber
        case 3: return rgb565(30, 210, 240);  // Bioluminescent Cyan
        case 4: return rgb565(225, 230, 255); // Moon Lily
        default: return rgb565(230, 45, 140); // Magenta Orchid
    }
}

static uint8_t nearestPetalPalette(uint16_t color) {
    uint8_t best = 0;
    uint32_t bestDist = 0xFFFFFFFFu;
    for (uint8_t i = 0; i < 5; i++) {
        uint16_t sample = paletteColor(i);
        int dr = (int)((color >> 11) & 31) - (int)((sample >> 11) & 31);
        int dg = (int)((color >> 5) & 63) - (int)((sample >> 5) & 63);
        int db = (int)(color & 31) - (int)(sample & 31);
        uint32_t dist = (uint32_t)(dr * dr + dg * dg + db * db);
        if (dist < bestDist) {
            bestDist = dist;
            best = i;
        }
    }
    return best;
}

static uint8_t clampByte(int value, uint8_t lo, uint8_t hi) {
    if (value < (int)lo) return lo;
    if (value > (int)hi) return hi;
    return (uint8_t)value;
}

// 16-Character Seed Code Engine (Crockford Base32: XXXX-XXXX-XXXX-XXXX)
static void encodeSeedCode(const PlantGenome& genome, char* outCode20) {
    if (!outCode20) return;

    // Pack 80-bit genetic payload into uint32_t chunks
    uint32_t p0 = genome.seed;
    uint32_t p1 = ((uint32_t)(genome.phenotype & 0x03) << 28) |
                  ((uint32_t)(genome.maxDepth & 0x07) << 25) |
                  ((uint32_t)(genome.baseLength & 0x7F) << 18) |
                  ((uint32_t)(genome.branchAngle & 0x3F) << 12) |
                  ((uint32_t)(genome.lengthDecayPct & 0x3F) << 6) |
                  ((uint32_t)(genome.stemHue & 0x03) << 4) |
                  ((uint32_t)(genome.petalPalette & 0x07) << 1) |
                  ((uint32_t)(genome.petalCount & 0x01));

    uint16_t p2 = ((uint16_t)(genome.leafShape & 0x03) << 14) |
                  ((uint16_t)(genome.foliageDensity & 0x7F) << 7) |
                  ((uint16_t)(genome.swayFlexibility & 0x07) << 4);

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

void PlantEngine::getSeedCode(char* outCode20) const {
    encodeSeedCode(_genome, outCode20);
}

bool PlantEngine::composeHybridSeedCode(const ForeignParent& foreign, char* outCode20, PlantGenome* outGenome) const {
    if (!outCode20) return false;

    uint8_t foreignPhenotype = foreign.phenotype & 0x03;
    uint8_t foreignPalette = foreign.petalPalette;
    if (foreignPalette > 4) foreignPalette = nearestPetalPalette(foreign.petalColor);
    uint8_t foreignStem = foreign.stemHue & 0x03;

    uint32_t mix = _genome.seed ^ (foreign.seed * 0x45D9F3Bu);
    mix ^= ((uint32_t)foreign.petalColor << 16) ^ getPetalColor();
    mix ^= ((uint32_t)_genome.phenotype << 28) ^ ((uint32_t)foreignPhenotype << 24);
    mix ^= ((uint32_t)_genome.petalPalette << 12) ^ ((uint32_t)foreignPalette << 8);
    mix ^= ((uint32_t)_genome.stemHue << 4) ^ foreignStem;
    mix *= 0x9E3779B1u;
    if (mix == 0 || mix == _genome.seed) mix ^= 0xA5C35A17u;

    PlantGenome child = _genome;
    child.seed = mix;

    if (_genome.phenotype != foreignPhenotype) {
        uint8_t folded = (uint8_t)((_genome.phenotype ^ foreignPhenotype) & 0x03);
        if (mix & 0x10u) child.phenotype = folded;
        else child.phenotype = (mix & 0x01u) ? foreignPhenotype : _genome.phenotype;
    }

    uint8_t blended = (uint8_t)((_genome.petalPalette + foreignPalette) / 2);
    if (_genome.petalPalette != foreignPalette && blended == _genome.petalPalette) {
        blended = foreignPalette;
    }
    uint8_t fromColor = nearestPetalPalette(foreign.petalColor);
    if (fromColor != blended && (mix & 0x20u)) blended = fromColor;
    child.petalPalette = clampByte(blended, 0, 4);

    child.stemHue = (uint8_t)((_genome.stemHue + foreignStem) & 0x03);

    int density = ((int)_genome.foliageDensity + (int)(40 + (mix & 0x1Fu))) / 2;
    child.foliageDensity = clampByte(density, 40, 100);
    child.leafShape = (uint8_t)(((_genome.leafShape + (mix >> 6)) ) % 3);
    if (child.maxDepth < 3) child.maxDepth = 3;
    if (child.maxDepth > 5) child.maxDepth = 4;
    if (child.baseLength < 32) child.baseLength = 42;
    if (child.baseLength > 80) child.baseLength = 56;
    if (child.branchAngle < 12) child.branchAngle = 18;
    if (child.branchAngle > 48) child.branchAngle = 32;
    if (child.lengthDecayPct < 60) child.lengthDecayPct = 68;
    if (child.lengthDecayPct > 80) child.lengthDecayPct = 76;
    if (child.swayFlexibility < 1) child.swayFlexibility = 2;
    if (child.swayFlexibility > 5) child.swayFlexibility = 4;
    if (child.petalCount < 3 || child.petalCount > 8) {
        child.petalCount = (child.phenotype == PHENOTYPE_ORCHID) ? 5 : 4;
    }

    encodeSeedCode(child, outCode20);
    if (outGenome) *outGenome = child;
    return true;
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

void PlantEngine::updateLifecycle(uint32_t deltaMs, TimePhase phase, uint8_t hour, uint8_t minute,
                                  uint8_t month, uint8_t day) {
    cacheSeason(month, day);
    _frostSparkle++;

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

    // 0.1 Hz mint/cyan pulse, only from 23:00 through 05:00. Night phase itself is wider.
    if (isPhosphorescentHour(hour, minute)) {
        _phosphoElapsedMs += deltaMs;
        while (_phosphoElapsedMs >= 10000) {
            _phosphoElapsedMs -= 10000;
        }
        uint8_t angle = (uint8_t)((_phosphoElapsedMs * 256UL) / 10000UL);
        uint8_t uni = (uint8_t)(lutSin(angle) + 127); // 0..254 across the 10s cycle
        _phosphoBlend = (uint8_t)(40 + (uni / 4));    // about 16%..40% toward mint/cyan
    } else {
        _phosphoElapsedMs = 0;
        _phosphoBlend = 0;
    }

    for (uint8_t i = 0; i < MAX_SOIL_SEED_PODS; i++) {
        if (!_soilPods[i].active) continue;
        if (_soilPods[i].age < 255) _soilPods[i].age++;
        _soilPods[i].phase++;
    }
}

void PlantEngine::fertilizeFlower(uint8_t flowerIdx) {
    if (flowerIdx < _flowerCount) {
        _flowers[flowerIdx].pollinated = true;
        _flowers[flowerIdx].stage = BLOOM_SEED_POD;
    }
}

void PlantEngine::clearSoilSeedPods() {
    for (uint8_t i = 0; i < MAX_SOIL_SEED_PODS; i++) {
        _soilPods[i].x = 0;
        _soilPods[i].depth = 0;
        _soilPods[i].phase = 0;
        _soilPods[i].age = 0;
        _soilPods[i].active = false;
    }
}

static bool withinRadius(int16_t x, int16_t y, int16_t cx, int16_t cy, int16_t radius) {
    int32_t dx = (int32_t)x - cx;
    int32_t dy = (int32_t)y - cy;
    int32_t r = radius;
    return (dx * dx) + (dy * dy) <= (r * r);
}

static bool nearSegment(int16_t px, int16_t py,
                        int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                        int16_t radius) {
    int16_t minX = (x0 < x1) ? x0 : x1;
    int16_t maxX = (x0 > x1) ? x0 : x1;
    int16_t minY = (y0 < y1) ? y0 : y1;
    int16_t maxY = (y0 > y1) ? y0 : y1;
    if (px < (int16_t)(minX - radius) || px > (int16_t)(maxX + radius) ||
        py < (int16_t)(minY - radius) || py > (int16_t)(maxY + radius)) {
        return false;
    }

    int32_t dx = (int32_t)x1 - x0;
    int32_t dy = (int32_t)y1 - y0;
    int32_t len2 = (dx * dx) + (dy * dy);
    if (len2 <= 1) {
        return withinRadius(px, py, x0, y0, radius);
    }

    int32_t t = (((int32_t)px - x0) * dx) + (((int32_t)py - y0) * dy);
    if (t <= 0) {
        return withinRadius(px, py, x0, y0, radius);
    }
    if (t >= len2) {
        return withinRadius(px, py, x1, y1, radius);
    }

    int32_t cx = x0 + ((dx * t) / len2);
    int32_t cy = y0 + ((dy * t) / len2);
    int32_t ddx = (int32_t)px - cx;
    int32_t ddy = (int32_t)py - cy;
    int32_t r = radius;
    return (ddx * ddx) + (ddy * ddy) <= (r * r);
}

static void putSoilPixel(uint16_t* buffer, int16_t width, int16_t height,
                         int16_t x, int16_t y, uint16_t color) {
    if (!buffer || x < 0 || y < 0 || x >= width || y >= height) return;
    buffer[(y * width) + x] = color;
}

bool PlantEngine::touchesNeighborCanopy(int16_t x, int16_t y, uint8_t originFlower) const {
    const int16_t flowerRadius = 7;
    const int16_t branchRadius = 4;

    uint8_t ignoreSeg = 0xFF;
    if (originFlower < _flowerCount) {
        ignoreSeg = _flowers[originFlower].terminalSegIdx;
    }

    for (uint8_t i = 0; i < _flowerCount; i++) {
        if (i == originFlower) continue;
        const FlowerInstance& flower = _flowers[i];
        if (flower.stage == BLOOM_VEGETATIVE) continue;
        if (withinRadius(x, y, flower.x, flower.y, flowerRadius)) return true;
    }

    for (uint8_t i = 0; i < _segmentCount; i++) {
        const PlantSegment& seg = _segments[i];
        if (!seg.active || i == ignoreSeg) continue;
        if (nearSegment(x, y, seg.curX0, seg.curY0, seg.curX1, seg.curY1, branchRadius)) {
            return true;
        }
    }
    return false;
}

bool PlantEngine::depositSoilSeedPod(int16_t x) {
    if (x < 4) x = 4;
    if (x > (int16_t)(SCREEN_WIDTH - 5)) x = (int16_t)(SCREEN_WIDTH - 5);

    uint8_t slot = 0;
    uint8_t oldestAge = 0;
    for (uint8_t i = 0; i < MAX_SOIL_SEED_PODS; i++) {
        if (!_soilPods[i].active) {
            slot = i;
            break;
        }
        if (i == 0 || _soilPods[i].age >= oldestAge) {
            oldestAge = _soilPods[i].age;
            slot = i;
        }
    }

    SoilSeedPod& pod = _soilPods[slot];
    pod.x = x;
    pod.depth = (uint8_t)(5 + ((uint8_t)x & 3));
    pod.phase = 0;
    pod.age = 0;
    pod.active = true;
    return true;
}

void PlantEngine::renderSoilSeedPods(uint16_t* buffer, int16_t screenWidth, int16_t screenHeight) const {
    if (!buffer) return;

    int16_t surface = (int16_t)(SCREEN_HEIGHT - SUBSTRATE_HEIGHT);
    for (uint8_t i = 0; i < MAX_SOIL_SEED_PODS; i++) {
        const SoilSeedPod& pod = _soilPods[i];
        if (!pod.active) continue;

        int16_t x = pod.x;
        int16_t y = (int16_t)(surface + pod.depth);
        uint8_t pulse = (uint8_t)(lutSin(pod.phase) + 127);
        uint8_t bodyR = (uint8_t)(150 + (pulse >> 3));
        uint16_t body = rgb565(bodyR, (uint8_t)((bodyR * 3) / 4), 36);
        uint16_t cap = rgb565(230, 196, 64);
        uint16_t shade = rgb565(96, 64, 28);

        putSoilPixel(buffer, screenWidth, screenHeight, x, y, cap);
        putSoilPixel(buffer, screenWidth, screenHeight, (int16_t)(x - 1), (int16_t)(y + 1), body);
        putSoilPixel(buffer, screenWidth, screenHeight, x, (int16_t)(y + 1), body);
        putSoilPixel(buffer, screenWidth, screenHeight, (int16_t)(x + 1), (int16_t)(y + 1), body);
        putSoilPixel(buffer, screenWidth, screenHeight, x, (int16_t)(y + 2), body);
        putSoilPixel(buffer, screenWidth, screenHeight, x, (int16_t)(y + 3), shade);
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
    _curWindOffset = totalBreeze;

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
                          int16_t bufWidth, int16_t bufHeight, uint16_t rimColor, bool crystalTip) {
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

    if (rimColor != 0) {
        drawThickLine(buffer, x, y, leftX, leftY, 1, rimColor, bufWidth, bufHeight);
        drawThickLine(buffer, leftX, leftY, tipX, tipY, 1, rimColor, bufWidth, bufHeight);
        drawThickLine(buffer, x, y, rightX, rightY, 1, rimColor, bufWidth, bufHeight);
        drawThickLine(buffer, rightX, rightY, tipX, tipY, 1, rimColor, bufWidth, bufHeight);
    }

    // Center vein
    drawThickLine(buffer, x, y, tipX, tipY, 1, veinColor, bufWidth, bufHeight);

    if (crystalTip) {
        int16_t cx = tipX + (tipDx > 0 ? 1 : (tipDx < 0 ? -1 : 0));
        int16_t cy = tipY + (tipDy > 0 ? 1 : (tipDy < 0 ? -1 : 0));
        putSoilPixel(buffer, bufWidth, bufHeight, tipX, tipY, rgb565(244, 252, 255));
        putSoilPixel(buffer, bufWidth, bufHeight, cx, cy, rgb565(170, 220, 245));
    }
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

        uint16_t marginColor = _flowerStyle.petalHighlight;
        if (_phosphoBlend > 0) {
            marginColor = blendMintCyan(marginColor, _phosphoBlend);
        }

        drawThickLine(buffer, x, y, pLeftX, pLeftY, 1, _flowerStyle.petalColor, bufWidth, bufHeight);
        drawThickLine(buffer, pLeftX, pLeftY, pTipX, pTipY, 1, marginColor, bufWidth, bufHeight);
        drawThickLine(buffer, x, y, pRightX, pRightY, 1, _flowerStyle.petalColor, bufWidth, bufHeight);
        drawThickLine(buffer, pRightX, pRightY, pTipX, pTipY, 1, marginColor, bufWidth, bufHeight);
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

        uint16_t stemColor = seasonalStemColor(seg);
        if (_seasonKnown && _season == SEASON_WINTER) {
            int16_t rim = (seg.thickness >= 2) ? (int16_t)((seg.thickness / 2) + 1) : 1;
            uint16_t ice = rgb565(214, 236, 248);
            uint16_t iceShade = rgb565(140, 176, 196);
            drawThickLine(buffer, (int16_t)(seg.curX0 + rim), seg.curY0,
                          (int16_t)(seg.curX1 + rim), seg.curY1,
                          1, ice, screenWidth, screenHeight);
            drawThickLine(buffer, (int16_t)(seg.curX0 - rim), seg.curY0,
                          (int16_t)(seg.curX1 - rim), seg.curY1,
                          1, iceShade, screenWidth, screenHeight);
        }
        drawThickLine(buffer, seg.curX0, seg.curY0, seg.curX1, seg.curY1, 
                      seg.thickness, stemColor, screenWidth, screenHeight);
    }

    // 2. Render Foliage & Leaves (Only at active leaf nodes)
    for (uint8_t i = 0; i < _segmentCount; i++) {
        const PlantSegment& seg = _segments[i];
        if (!seg.active || !seg.hasLeaf) continue;

        uint8_t curLeafLen = (uint8_t)(seg.leafLength * _growthProgress);
        uint8_t curLeafWid = (uint8_t)(seg.leafWidth * _growthProgress);

        if (curLeafLen >= 2) {
            uint16_t leafColor = seasonalLeafColor(seg, i);
            uint16_t veinColor = seasonalVeinColor(seg);
            if (_phosphoBlend > 0) {
                veinColor = blendMintCyan(veinColor, _phosphoBlend);
            }
            uint16_t rim = 0;
            bool crystal = false;
            if (_seasonKnown && _season == SEASON_WINTER) {
                rim = rgb565(210, 236, 248);
                crystal = (((uint8_t)((i * 3) + (_frostSparkle >> 2)) & 7) == 0);
            }
            drawLeaf(buffer, seg.curX1, seg.curY1, seg.leafAngle, 
                     curLeafLen, curLeafWid, leafColor, veinColor,
                     screenWidth, screenHeight, rim, crystal);
        }
    }

    // 3. Render Botanical Flowers (Terminal branch tips)
    for (uint8_t i = 0; i < _flowerCount; i++) {
        const FlowerInstance& flower = _flowers[i];
        if (flower.stage == BLOOM_VEGETATIVE) continue;

        FlowerAttributes savedStyle = _flowerStyle;
        uint8_t faceAngle = 192;
        applySeasonalBloom(i, faceAngle);
        drawFlower(buffer, flower.x, flower.y, faceAngle,
                   flower.diurnalOpenRatio, flower.stage, screenWidth, screenHeight);
        _flowerStyle = savedStyle;
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

bool PlantEngine::getSegmentPose(uint8_t index, SegmentPose& out) const {
    if (index >= _segmentCount) return false;
    const PlantSegment& seg = _segments[index];
    if (!seg.active) return false;

    out.x0 = seg.curX0;
    out.y0 = seg.curY0;
    out.x1 = seg.curX1;
    out.y1 = seg.curY1;
    out.hasLeaf = seg.hasLeaf;
    out.broadLeaf = seg.hasLeaf && (seg.leafWidth >= 4);
    out.leafX = seg.curX1;
    out.leafY = seg.curY1;
    out.parentIndex = seg.parentIndex;
    return true;
}

void PlantEngine::generateRoots() {
    _rootCount = 0;
    int16_t rx = SCREEN_WIDTH / 2; // 86
    int16_t ry = SCREEN_HEIGHT - SUBSTRATE_HEIGHT; // 280

    uint16_t rootCol = rgb565(190, 160, 115);
    uint16_t hairCol = rgb565(155, 125, 85);

    // 1. Central Taproot
    _roots[_rootCount++] = {rx, ry, (int16_t)(rx - 1), (int16_t)(ry + 10), 2, rootCol, true};
    _roots[_rootCount++] = {(int16_t)(rx - 1), (int16_t)(ry + 10), (int16_t)(rx + 2), (int16_t)(ry + 22), 2, rootCol, true};
    _roots[_rootCount++] = {(int16_t)(rx + 2), (int16_t)(ry + 22), (int16_t)(rx - 1), (int16_t)(ry + 32), 1, hairCol, true};

    // 2. Left Lateral Root System
    _roots[_rootCount++] = {rx, (int16_t)(ry + 4), (int16_t)(rx - 14), (int16_t)(ry + 13), 2, rootCol, true};
    _roots[_rootCount++] = {(int16_t)(rx - 14), (int16_t)(ry + 13), (int16_t)(rx - 28), (int16_t)(ry + 19), 1, rootCol, true};
    _roots[_rootCount++] = {(int16_t)(rx - 28), (int16_t)(ry + 19), (int16_t)(rx - 38), (int16_t)(ry + 27), 1, hairCol, true};
    _roots[_rootCount++] = {(int16_t)(rx - 14), (int16_t)(ry + 13), (int16_t)(rx - 18), (int16_t)(ry + 25), 1, hairCol, true};

    // 3. Right Lateral Root System
    _roots[_rootCount++] = {rx, (int16_t)(ry + 5), (int16_t)(rx + 15), (int16_t)(ry + 14), 2, rootCol, true};
    _roots[_rootCount++] = {(int16_t)(rx + 15), (int16_t)(ry + 14), (int16_t)(rx + 30), (int16_t)(ry + 21), 1, rootCol, true};
    _roots[_rootCount++] = {(int16_t)(rx + 30), (int16_t)(ry + 21), (int16_t)(rx + 42), (int16_t)(ry + 29), 1, hairCol, true};
    _roots[_rootCount++] = {(int16_t)(rx + 15), (int16_t)(ry + 14), (int16_t)(rx + 20), (int16_t)(ry + 26), 1, hairCol, true};

    // 4. Fine sub-hair tendrils
    _roots[_rootCount++] = {(int16_t)(rx - 1), (int16_t)(ry + 10), (int16_t)(rx + 8), (int16_t)(ry + 17), 1, hairCol, true};
    _roots[_rootCount++] = {(int16_t)(rx + 2), (int16_t)(ry + 22), (int16_t)(rx - 8), (int16_t)(ry + 29), 1, hairCol, true};
}

void PlantEngine::renderRoots(uint16_t* buffer, int16_t screenWidth, int16_t screenHeight) {
    if (!buffer || _rootCount == 0 || _growthProgress <= 0.05f) return;

    for (uint8_t i = 0; i < _rootCount; i++) {
        const RootSegment& r = _roots[i];
        if (!r.active) continue;

        int16_t x0 = r.x0;
        int16_t y0 = r.y0;
        int16_t x1 = x0 + (int16_t)((r.x1 - x0) * _growthProgress);
        int16_t y1 = y0 + (int16_t)((r.y1 - y0) * _growthProgress);

        drawThickLine(buffer, x0, y0, x1, y1, r.thickness, r.color, screenWidth, screenHeight);
    }
}
