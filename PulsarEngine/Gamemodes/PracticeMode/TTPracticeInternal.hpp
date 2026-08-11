#ifndef _PULSAR_TTPRACTICE_INTERNAL_
#define _PULSAR_TTPRACTICE_INTERNAL_

#include <Gamemodes/PracticeMode/TTPractice.hpp>
#include <MarioKartWii/Item/ItemPlayer.hpp>
#include <MarioKartWii/Kart/KartManager.hpp>
#include <MarioKartWii/Race/RaceInfo/RaceInfo.hpp>

namespace Pulsar {
namespace TTPractice {

static const u32 ITEM_COUNT = 7;
static const u16 INPUT_ITEM_USE = 0x4;
static const float STICK_WHEEL_THRESHOLD = 0.5f;
static const float RESPAWN_STICK_THRESHOLD = 0.5f;
static const u16 RESPAWN_HOLD_FRAMES = 30;
static const u32 TC_TIMER_OFFSET = 0x1d8;
static const u32 TC_NATURAL_STRIKE_TIMER = 600;
static const u32 TC_STRIKE_TIMER = 599;
static const u32 ITEM_OBJ_KILLED = 0x1;
static const u32 ITEM_OBJ_UNAVAILABLE = 0xc0;
static const u32 KART_STATUS_IN_BULLET = 0x8000000;

struct SavedRaceProgress {
    u16 checkpoint;
    float raceCompletion;
    float raceCompletionMax;
    float firstKcpLapCompletion;
    float nextCheckpointLapCompletion;
    float nextCheckpointLapCompletionMax;
    u16 currentLap;
    u8 maxLap;
    u8 currentKCP;
    u8 maxKCP;
};

extern const ItemId ITEM_WHEEL_ITEMS[ITEM_COUNT];
extern u32 selectedItemIndexes[4];
extern s8 stickWheelDirections[4];
extern u16 respawnShortcutTimers[4];
extern u16 respawnSaveTimers[4];
extern Vec3 savedRespawnPositions[4];
extern Quat savedRespawnRotations[4];
extern SavedRaceProgress savedRespawnRaceProgress[4];
extern bool hasSavedRespawn[4];
extern bool hasGrantedItem[4];
extern bool canRefillOnUse[4];

void ClearSavedRespawns();
bool IsAnalogRespawnInputHeld(float stickX, float stickY);
asmFunc PlayRespawnSaveSound();
void UpdatePlayerRespawnShortcut(Item::Player& player);
void UpdatePlayerAndPracticeWheel(Item::Player& player);

}  // namespace TTPractice
}  // namespace Pulsar

#endif
