#ifndef MESH_SYNC_H
#define MESH_SYNC_H

#include <Arduino.h>
#include "config.h"

// ESP-NOW desk mesh. Neighboring units share a channel without a router.
// A pollen mote that leaves one screen is handed to the nearest peer and
// drawn arriving on the opposite edge. A foreign spore that pollinates the
// local canopy is stored here as a hybrid Seed Code.

#define MESH_SEED_SLOTS  8
#define MESH_MAX_PEERS   4

enum MeshEdge : uint8_t {
    MESH_EDGE_LEFT = 0,
    MESH_EDGE_RIGHT = 1
};

enum MeshSeedKind : uint8_t {
    MESH_SEED_NONE = 0,
    MESH_SEED_HYBRID = 1,
    MESH_SEED_IMPORTED = 2
};

struct MeshSeedSlot {
    char code[20];
    uint8_t phenotype;
    uint8_t petalPalette;
    uint8_t stemHue;
    uint8_t kind;
    uint8_t used;
};

class MeshSync {
public:
    MeshSync();
    void begin();

    // Main loop: publish this plant's identity and draw spores that have arrived.
    void update();

    // Net task: peer beacons, radio upkeep, spore transmit, library flush.
    void service();

    // Queue a pollen mote that just left a lateral edge. No heap use.
    void handoffSpore(uint8_t exitEdge, int16_t y, int16_t vx, int16_t vy,
                      uint32_t seed, uint8_t phenotype, uint8_t petalPalette,
                      uint8_t stemHue, uint8_t petalCount, uint16_t petalColor);

    // Keep a bred or imported specimen. Returns false if the code is already kept.
    bool rememberSeed(const char* code, uint8_t kind, uint8_t phenotype,
                      uint8_t petalPalette, uint8_t stemHue);

    uint8_t peerCount() const { return _peerCount; }
    uint8_t seedSlotCount() const { return MESH_SEED_SLOTS; }
    bool copySeedSlot(uint8_t index, MeshSeedSlot& out) const;

private:
    struct Peer {
        uint8_t mac[6];
        int8_t rssi;
        uint32_t lastSeen;
        bool used;
        bool added;
    };

    struct Identity {
        uint32_t seed;
        uint16_t petalColor;
        uint8_t phenotype;
        uint8_t petalPalette;
        uint8_t stemHue;
        uint8_t petalCount;
    };

    Peer _peers[MESH_MAX_PEERS];
    Identity _identity;
    uint8_t _selfMac[6];
    uint8_t _channel;
    uint8_t _peerCount;
    uint8_t _slotCursor;
    uint8_t _mode;
    uint8_t _iface;
    bool _ready;
    bool _sleepOff;
    uint32_t _lastBeacon;
    uint32_t _lastInitLog;

    void maintainRadio();
    void ensureEspNow(uint8_t iface);
    void notePeer(const uint8_t mac[6], int8_t rssi);
    int strongestPeer(uint32_t now) const;
    bool ensureUnicastPeer(const uint8_t mac[6]);
    void drainInbox();
    void sendBeacon();
    void sendHandoffs();
    void loadLibrary();
    void flushLibrary();
    void publishIdentity();
    void applySpawns();
};

extern MeshSync Mesh;

// Serializes LittleFS users (plant state on the main loop, seed library on the net task).
void meshFsLock();
void meshFsUnlock();

#endif // MESH_SYNC_H
