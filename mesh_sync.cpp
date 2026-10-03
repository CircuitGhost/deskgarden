#include "mesh_sync.h"

#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <LittleFS.h>
#include <string.h>

#include "plant_engine.h"
#include "particle_system.h"
#include "net_sync.h"

MeshSync Mesh;

static SemaphoreHandle_t s_fsMutex = nullptr;

void meshFsLock() {
    if (!s_fsMutex) s_fsMutex = xSemaphoreCreateMutex();
    if (s_fsMutex) xSemaphoreTake(s_fsMutex, portMAX_DELAY);
}

void meshFsUnlock() {
    if (s_fsMutex) xSemaphoreGive(s_fsMutex);
}

static const uint8_t MESH_CHANNEL = 1;
static const uint8_t MESH_MAGIC = 0xDF;
static const uint8_t MESH_VERSION = 1;
static const uint8_t MESH_KIND_BEACON = 1;
static const uint8_t MESH_KIND_SPORE = 2;
static const uint8_t MESH_Q = 4;
static const uint32_t MESH_BEACON_MS = 1500;
static const uint32_t MESH_PEER_TTL_MS = 8000;
static const uint32_t MESH_SEED_MAGIC = 0x534C4942; // "SLIB"
static const char* MESH_SEED_PATH = "/seedlib.bin";
static const char* MESH_SEED_TEMP = "/seedlib.tmp";

// Naturally aligned. Both desks are ESP32-C6, little-endian.
struct MeshPacket {
    uint32_t seed;
    uint16_t petalColor;
    uint16_t yPx;
    int16_t vx;
    int16_t vy;
    uint8_t magic;
    uint8_t version;
    uint8_t kind;
    uint8_t edge;
    uint8_t phenotype;
    uint8_t petalPalette;
    uint8_t stemHue;
    uint8_t petalCount;
};

static_assert(sizeof(MeshPacket) <= 250, "ESP-NOW payload is limited to 250 bytes");

struct SpawnReq {
    int16_t y;
    int16_t vx;
    int16_t vy;
    uint16_t petalColor;
    uint32_t seed;
    uint8_t arriveEdge;
    uint8_t phenotype;
    uint8_t petalPalette;
    uint8_t stemHue;
};

struct HandoffReq {
    int16_t y;
    int16_t vx;
    int16_t vy;
    uint16_t petalColor;
    uint32_t seed;
    uint8_t edge;
    uint8_t phenotype;
    uint8_t petalPalette;
    uint8_t stemHue;
    uint8_t petalCount;
};

struct InboxItem {
    MeshPacket pkt;
    uint8_t mac[6];
    int8_t rssi;
};

struct SeedFile {
    uint32_t magic;
    uint8_t version;
    uint8_t count;
    uint16_t reserved;
    MeshSeedSlot slots[MESH_SEED_SLOTS];
    uint32_t checksum;
};

static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;

static InboxItem s_inbox[MESH_Q];
static uint8_t s_inboxR = 0;
static uint8_t s_inboxW = 0;

static SpawnReq s_spawns[MESH_Q];
static uint8_t s_spawnR = 0;
static uint8_t s_spawnW = 0;

static HandoffReq s_handoffs[MESH_Q];
static uint8_t s_handoffR = 0;
static uint8_t s_handoffW = 0;

static MeshSeedSlot s_slots[MESH_SEED_SLOTS];
static bool s_libraryDirty = false;

static bool macEqual(const uint8_t* a, const uint8_t* b) {
    return memcmp(a, b, 6) == 0;
}

static bool macZero(const uint8_t* mac) {
    for (uint8_t i = 0; i < 6; i++) {
        if (mac[i] != 0) return false;
    }
    return true;
}

static uint32_t seedFileChecksum(const SeedFile& file) {
    const uint8_t* bytes = (const uint8_t*)&file;
    size_t length = sizeof(SeedFile) - sizeof(uint32_t);
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < length; i++) {
        crc ^= bytes[i];
        for (uint8_t bit = 0; bit < 8; bit++) {
            if (crc & 1u) crc = (crc >> 1) ^ 0xEDB88320u;
            else crc >>= 1;
        }
    }
    return ~crc;
}

#if defined(ESP_IDF_VERSION_MAJOR) && ESP_IDF_VERSION_MAJOR >= 5
static void meshRecvThunk(const esp_now_recv_info_t* info, const uint8_t* data, int len) {
    if (!info || !info->src_addr || !data || len < (int)sizeof(MeshPacket)) return;
    int8_t rssi = (info->rx_ctrl) ? (int8_t)info->rx_ctrl->rssi : 0;
    const uint8_t* mac = info->src_addr;
#else
static void meshRecvThunk(const uint8_t* mac, const uint8_t* data, int len) {
    if (!mac || !data || len < (int)sizeof(MeshPacket)) return;
    int8_t rssi = 0;
#endif
    MeshPacket pkt;
    memcpy(&pkt, data, sizeof(pkt));
    if (pkt.magic != MESH_MAGIC || pkt.version != MESH_VERSION) return;
    if (pkt.kind != MESH_KIND_BEACON && pkt.kind != MESH_KIND_SPORE) return;

    portENTER_CRITICAL(&s_mux);
    uint8_t slot = s_inboxW;
    uint8_t next = (uint8_t)((s_inboxW + 1) % MESH_Q);
    if (next != s_inboxR) {
        memcpy(s_inbox[slot].mac, mac, 6);
        s_inbox[slot].pkt = pkt;
        s_inbox[slot].rssi = rssi;
        s_inboxW = next;
    }
    portEXIT_CRITICAL(&s_mux);
#if defined(ESP_IDF_VERSION_MAJOR) && ESP_IDF_VERSION_MAJOR >= 5
}
#else
}
#endif

MeshSync::MeshSync()
    : _channel(0),
      _peerCount(0),
      _slotCursor(0),
      _mode(0),
      _iface(WIFI_IF_STA),
      _ready(false),
      _sleepOff(false),
      _lastBeacon(0),
      _lastInitLog(0) {
    memset(_peers, 0, sizeof(_peers));
    memset(_selfMac, 0, sizeof(_selfMac));
    memset(&_identity, 0, sizeof(_identity));
    memset(s_slots, 0, sizeof(s_slots));
}

void MeshSync::begin() {
    if (!s_fsMutex) s_fsMutex = xSemaphoreCreateMutex();
    loadLibrary();
}

void MeshSync::publishIdentity() {
    Identity id;
    memset(&id, 0, sizeof(id));
    const PlantGenome& genome = Plant.getGenome();
    id.seed = genome.seed;
    id.phenotype = genome.phenotype;
    id.petalPalette = genome.petalPalette;
    id.stemHue = genome.stemHue;
    id.petalCount = genome.petalCount;
    id.petalColor = Plant.getPetalColor();

    portENTER_CRITICAL(&s_mux);
    _identity = id;
    portEXIT_CRITICAL(&s_mux);
}

void MeshSync::applySpawns() {
    for (;;) {
        SpawnReq req;
        bool have = false;
        portENTER_CRITICAL(&s_mux);
        if (s_spawnR != s_spawnW) {
            req = s_spawns[s_spawnR];
            s_spawnR = (uint8_t)((s_spawnR + 1) % MESH_Q);
            have = true;
        }
        portEXIT_CRITICAL(&s_mux);
        if (!have) break;

        bool arrived = Particles.receiveSpore(req.y, req.arriveEdge, req.vx, req.vy,
                                              req.seed, req.phenotype, req.petalPalette,
                                              req.stemHue, req.petalColor);
        #if ENABLE_SERIAL_LOG
        if (arrived) {
            Serial.printf("[MESH] Spore arrived on the %s edge\n",
                          req.arriveEdge == MESH_EDGE_RIGHT ? "right" : "left");
        }
        #else
        (void)arrived;
        #endif
    }
}

void MeshSync::update() {
    // Identity snapshot and inbound spores only. No heap traffic on this tick.
    publishIdentity();
    applySpawns();
}

void MeshSync::handoffSpore(uint8_t exitEdge, int16_t y, int16_t vx, int16_t vy,
                            uint32_t seed, uint8_t phenotype, uint8_t petalPalette,
                            uint8_t stemHue, uint8_t petalCount, uint16_t petalColor) {
    HandoffReq req;
    req.y = y;
    req.vx = vx;
    req.vy = vy;
    req.petalColor = petalColor;
    req.seed = seed;
    req.edge = exitEdge;
    req.phenotype = phenotype;
    req.petalPalette = petalPalette;
    req.stemHue = stemHue;
    req.petalCount = petalCount;

    portENTER_CRITICAL(&s_mux);
    uint8_t next = (uint8_t)((s_handoffW + 1) % MESH_Q);
    if (next != s_handoffR) {
        s_handoffs[s_handoffW] = req;
        s_handoffW = next;
    }
    portEXIT_CRITICAL(&s_mux);
}

bool MeshSync::rememberSeed(const char* code, uint8_t kind, uint8_t phenotype,
                            uint8_t petalPalette, uint8_t stemHue) {
    if (!code) return false;

    char clean[20];
    memset(clean, 0, sizeof(clean));
    strncpy(clean, code, sizeof(clean) - 1);
    if (strlen(clean) < 16) return false;

    bool stored = false;
    portENTER_CRITICAL(&s_mux);
    bool duplicate = false;
    for (uint8_t i = 0; i < MESH_SEED_SLOTS; i++) {
        if (s_slots[i].used && strcmp(s_slots[i].code, clean) == 0) {
            duplicate = true;
            break;
        }
    }
    if (!duplicate) {
        uint8_t slot = MESH_SEED_SLOTS;
        for (uint8_t i = 0; i < MESH_SEED_SLOTS; i++) {
            if (!s_slots[i].used) {
                slot = i;
                break;
            }
        }
        if (slot >= MESH_SEED_SLOTS) {
            slot = (uint8_t)(_slotCursor % MESH_SEED_SLOTS);
            _slotCursor++;
        }
        MeshSeedSlot& dst = s_slots[slot];
        memset(&dst, 0, sizeof(dst));
        strncpy(dst.code, clean, sizeof(dst.code) - 1);
        dst.phenotype = phenotype;
        dst.petalPalette = petalPalette;
        dst.stemHue = stemHue;
        dst.kind = kind;
        dst.used = 1;
        s_libraryDirty = true;
        stored = true;
    }
    portEXIT_CRITICAL(&s_mux);
    return stored;
}

bool MeshSync::copySeedSlot(uint8_t index, MeshSeedSlot& out) const {
    if (index >= MESH_SEED_SLOTS) return false;
    portENTER_CRITICAL(&s_mux);
    out = s_slots[index];
    portEXIT_CRITICAL(&s_mux);
    return out.used != 0;
}

void MeshSync::loadLibrary() {
    if (!LittleFS.begin(false)) return;
    if (!LittleFS.exists(MESH_SEED_PATH)) return;

    File file = LittleFS.open(MESH_SEED_PATH, "r");
    if (!file) return;

    SeedFile stored;
    memset(&stored, 0, sizeof(stored));
    size_t got = file.read((uint8_t*)&stored, sizeof(stored));
    file.close();
    if (got != sizeof(stored)) return;
    if (stored.magic != MESH_SEED_MAGIC || stored.version != 1) return;
    if (stored.checksum != seedFileChecksum(stored)) return;

    portENTER_CRITICAL(&s_mux);
    memcpy(s_slots, stored.slots, sizeof(s_slots));
    s_libraryDirty = false;
    portEXIT_CRITICAL(&s_mux);
}

void MeshSync::flushLibrary() {
    static uint32_t nextAttempt = 0;
    uint32_t now = millis();
    if ((int32_t)(now - nextAttempt) < 0) return;

    SeedFile stored;
    memset(&stored, 0, sizeof(stored));
    bool dirty = false;

    portENTER_CRITICAL(&s_mux);
    dirty = s_libraryDirty;
    if (dirty) {
        stored.magic = MESH_SEED_MAGIC;
        stored.version = 1;
        memcpy(stored.slots, s_slots, sizeof(stored.slots));
        uint8_t count = 0;
        for (uint8_t i = 0; i < MESH_SEED_SLOTS; i++) {
            if (s_slots[i].used) count++;
        }
        stored.count = count;
        s_libraryDirty = false;
    }
    portEXIT_CRITICAL(&s_mux);

    if (!dirty) return;
    stored.checksum = seedFileChecksum(stored);

    meshFsLock();
    bool wroteOk = false;
    if (LittleFS.begin(false)) {
        File file = LittleFS.open(MESH_SEED_TEMP, "w");
        if (file) {
            size_t wrote = file.write((const uint8_t*)&stored, sizeof(stored));
            file.close();
            if (wrote == sizeof(stored)) {
                LittleFS.remove(MESH_SEED_PATH);
                wroteOk = LittleFS.rename(MESH_SEED_TEMP, MESH_SEED_PATH);
            }
        }
    }
    meshFsUnlock();

    if (!wroteOk) {
        portENTER_CRITICAL(&s_mux);
        s_libraryDirty = true;
        portEXIT_CRITICAL(&s_mux);
        nextAttempt = millis() + 2000;
    }
}

void MeshSync::ensureEspNow(uint8_t iface) {
    WiFiMode_t mode = WiFi.getMode();
    if (mode == WIFI_OFF) return;

    uint8_t channel = 0;
    wifi_second_chan_t second = WIFI_SECOND_CHAN_NONE;
    if (esp_wifi_get_channel(&channel, &second) != ESP_OK || channel == 0) return;

    if (_ready && _iface == iface && _mode == (uint8_t)mode && _channel == channel) return;

    if (_ready) {
        esp_now_deinit();
        _ready = false;
        for (uint8_t i = 0; i < MESH_MAX_PEERS; i++) _peers[i].added = false;
    }

    if (esp_now_init() != ESP_OK) {
        uint32_t now = millis();
        if (now - _lastInitLog > 5000) {
            _lastInitLog = now;
            #if ENABLE_SERIAL_LOG
            Serial.println("[MESH] ESP-NOW init failed, will retry");
            #endif
        }
        return;
    }

    esp_now_register_recv_cb(meshRecvThunk);

    uint8_t broadcast[6];
    memset(broadcast, 0xFF, sizeof(broadcast));
    esp_now_peer_info_t peer;
    memset(&peer, 0, sizeof(peer));
    memcpy(peer.peer_addr, broadcast, 6);
    peer.channel = 0;
    peer.encrypt = false;
    peer.ifidx = (wifi_interface_t)iface;
    esp_now_add_peer(&peer);

    _ready = true;
    _iface = iface;
    _mode = (uint8_t)mode;
    _channel = channel;
    esp_wifi_get_mac((wifi_interface_t)iface, _selfMac);

    #if ENABLE_SERIAL_LOG
    Serial.printf("[MESH] ESP-NOW listening on %s channel %u\n",
                  iface == WIFI_IF_AP ? "AP" : "STA", channel);
    #endif
}

void MeshSync::maintainRadio() {
    bool portal = NetSync.isPortalActive();
    NetState state = NetSync.getState();

    if (!portal && state == NET_STATE_OFFLINE) {
        if (WiFi.getMode() != WIFI_STA) {
            WiFi.mode(WIFI_STA);
            _sleepOff = false;
        }
        if (!_sleepOff) {
            WiFi.setSleep(false);
            _sleepOff = true;
        }
        uint8_t channel = 0;
        wifi_second_chan_t second = WIFI_SECOND_CHAN_NONE;
        if (esp_wifi_get_channel(&channel, &second) == ESP_OK && channel != MESH_CHANNEL) {
            esp_wifi_set_channel(MESH_CHANNEL, WIFI_SECOND_CHAN_NONE);
        }
    } else if (!portal && NetSync.isConnected() && !_sleepOff) {
        WiFi.setSleep(false);
        _sleepOff = true;
    }

    if (portal) _sleepOff = false;

    uint8_t iface = portal ? (uint8_t)WIFI_IF_AP : (uint8_t)WIFI_IF_STA;
    ensureEspNow(iface);
}

void MeshSync::notePeer(const uint8_t mac[6], int8_t rssi) {
    if (macZero(mac)) return;
    if (!macZero(_selfMac) && macEqual(mac, _selfMac)) return;

    for (uint8_t i = 0; i < MESH_MAX_PEERS; i++) {
        if (_peers[i].used && macEqual(_peers[i].mac, mac)) {
            _peers[i].lastSeen = millis();
            _peers[i].rssi = rssi;
            return;
        }
    }

    int slot = -1;
    for (uint8_t i = 0; i < MESH_MAX_PEERS; i++) {
        if (!_peers[i].used) {
            slot = (int)i;
            break;
        }
    }
    if (slot < 0) {
        int weakest = -1;
        for (uint8_t i = 0; i < MESH_MAX_PEERS; i++) {
            if (!_peers[i].used) continue;
            if (weakest < 0 || _peers[i].rssi < _peers[weakest].rssi) weakest = (int)i;
        }
        if (weakest < 0 || rssi <= _peers[weakest].rssi) return;
        slot = weakest;
        _peers[slot].added = false;
    }

    memcpy(_peers[slot].mac, mac, 6);
    _peers[slot].rssi = rssi;
    _peers[slot].lastSeen = millis();
    _peers[slot].used = true;
    _peers[slot].added = false;

    #if ENABLE_SERIAL_LOG
    Serial.printf("[MESH] Peer %02X:%02X:%02X:%02X:%02X:%02X rssi %d\n",
                  mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], rssi);
    #endif
}

int MeshSync::strongestPeer(uint32_t now) const {
    int best = -1;
    for (uint8_t i = 0; i < MESH_MAX_PEERS; i++) {
        if (!_peers[i].used) continue;
        if ((uint32_t)(now - _peers[i].lastSeen) > MESH_PEER_TTL_MS) continue;
        if (best < 0 || _peers[i].rssi > _peers[best].rssi) best = (int)i;
    }
    return best;
}

bool MeshSync::ensureUnicastPeer(const uint8_t mac[6]) {
    if (esp_now_is_peer_exist(mac)) return true;
    esp_now_peer_info_t peer;
    memset(&peer, 0, sizeof(peer));
    memcpy(peer.peer_addr, mac, 6);
    peer.channel = 0;
    peer.encrypt = false;
    peer.ifidx = (wifi_interface_t)_iface;
    esp_err_t err = esp_now_add_peer(&peer);
    return err == ESP_OK || err == ESP_ERR_ESPNOW_EXIST;
}

void MeshSync::drainInbox() {
    for (;;) {
        InboxItem item;
        bool have = false;
        portENTER_CRITICAL(&s_mux);
        if (s_inboxR != s_inboxW) {
            item = s_inbox[s_inboxR];
            s_inboxR = (uint8_t)((s_inboxR + 1) % MESH_Q);
            have = true;
        }
        portEXIT_CRITICAL(&s_mux);
        if (!have) break;

        if (!macZero(_selfMac) && macEqual(item.mac, _selfMac)) continue;
        notePeer(item.mac, item.rssi);

        if (item.pkt.kind != MESH_KIND_SPORE) continue;

        uint8_t arrive = (item.pkt.edge == MESH_EDGE_LEFT) ? MESH_EDGE_RIGHT : MESH_EDGE_LEFT;
        SpawnReq req;
        req.y = (int16_t)item.pkt.yPx;
        req.vx = item.pkt.vx;
        req.vy = item.pkt.vy;
        req.petalColor = item.pkt.petalColor;
        req.seed = item.pkt.seed;
        req.arriveEdge = arrive;
        req.phenotype = item.pkt.phenotype;
        req.petalPalette = item.pkt.petalPalette;
        req.stemHue = item.pkt.stemHue;

        portENTER_CRITICAL(&s_mux);
        uint8_t next = (uint8_t)((s_spawnW + 1) % MESH_Q);
        if (next != s_spawnR) {
            s_spawns[s_spawnW] = req;
            s_spawnW = next;
        }
        portEXIT_CRITICAL(&s_mux);
    }

    uint32_t now = millis();
    uint8_t heard = 0;
    for (uint8_t i = 0; i < MESH_MAX_PEERS; i++) {
        if (!_peers[i].used) continue;
        if ((uint32_t)(now - _peers[i].lastSeen) > MESH_PEER_TTL_MS) continue;
        heard++;
    }
    _peerCount = heard;
}

void MeshSync::sendBeacon() {
    uint32_t now = millis();
    if ((uint32_t)(now - _lastBeacon) < MESH_BEACON_MS) return;
    _lastBeacon = now;

    Identity id;
    portENTER_CRITICAL(&s_mux);
    id = _identity;
    portEXIT_CRITICAL(&s_mux);

    MeshPacket pkt;
    memset(&pkt, 0, sizeof(pkt));
    pkt.magic = MESH_MAGIC;
    pkt.version = MESH_VERSION;
    pkt.kind = MESH_KIND_BEACON;
    pkt.seed = id.seed;
    pkt.petalColor = id.petalColor;
    pkt.phenotype = id.phenotype;
    pkt.petalPalette = id.petalPalette;
    pkt.stemHue = id.stemHue;
    pkt.petalCount = id.petalCount;

    uint8_t broadcast[6];
    memset(broadcast, 0xFF, sizeof(broadcast));
    esp_now_send(broadcast, (const uint8_t*)&pkt, sizeof(pkt));
}

void MeshSync::sendHandoffs() {
    uint32_t now = millis();
    int peerIndex = strongestPeer(now);
    // Keep the mote queued until a neighbor has been heard. The desk may still be discovering.
    if (peerIndex < 0) return;

    for (;;) {
        HandoffReq req;
        bool have = false;
        portENTER_CRITICAL(&s_mux);
        if (s_handoffR != s_handoffW) {
            req = s_handoffs[s_handoffR];
            s_handoffR = (uint8_t)((s_handoffR + 1) % MESH_Q);
            have = true;
        }
        portEXIT_CRITICAL(&s_mux);
        if (!have) break;

        const uint8_t* mac = _peers[peerIndex].mac;
        if (!ensureUnicastPeer(mac)) continue;

        MeshPacket pkt;
        memset(&pkt, 0, sizeof(pkt));
        pkt.magic = MESH_MAGIC;
        pkt.version = MESH_VERSION;
        pkt.kind = MESH_KIND_SPORE;
        pkt.edge = req.edge;
        pkt.yPx = (uint16_t)((req.y < 0) ? 0 : req.y);
        pkt.vx = req.vx;
        pkt.vy = req.vy;
        pkt.seed = req.seed;
        pkt.petalColor = req.petalColor;
        pkt.phenotype = req.phenotype;
        pkt.petalPalette = req.petalPalette;
        pkt.stemHue = req.stemHue;
        pkt.petalCount = req.petalCount;

        esp_err_t err = esp_now_send(mac, (const uint8_t*)&pkt, sizeof(pkt));
        #if ENABLE_SERIAL_LOG
        if (err == ESP_OK) {
            Serial.printf("[MESH] Spore handed off the %s edge\n",
                          req.edge == MESH_EDGE_RIGHT ? "right" : "left");
        }
        #else
        (void)err;
        #endif
    }
}

void MeshSync::service() {
    NetState state = NetSync.getState();
    if (state != NET_STATE_CONNECTING) {
        maintainRadio();
        if (_ready) {
            drainInbox();
            sendBeacon();
            sendHandoffs();
        }
    }
    flushLibrary();
}
