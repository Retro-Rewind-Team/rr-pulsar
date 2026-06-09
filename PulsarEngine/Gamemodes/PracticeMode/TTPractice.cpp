#include <kamek.hpp>
#include <Gamemodes/PracticeMode/TTPracticeInternal.hpp>
#include <MarioKartWii/Race/RaceData.hpp>
#include <PulsarSystem.hpp>
#include <Settings/Settings.hpp>

namespace Pulsar {
namespace TTPractice {

extern const ItemId ITEM_WHEEL_ITEMS[ITEM_COUNT] = {
    TRIPLE_MUSHROOM, GOLDEN_MUSHROOM, MEGA_MUSHROOM, STAR, BULLET_BILL, THUNDER_CLOUD, MUSHROOM
};

static bool isPracticeMode = false;
u32 selectedItemIndexes[4] = {0, 0, 0, 0};
s8 stickWheelDirections[4] = {0, 0, 0, 0};
u16 respawnShortcutTimers[4] = {0, 0, 0, 0};
u16 respawnSaveTimers[4] = {0, 0, 0, 0};
Vec3 savedRespawnPositions[4];
Quat savedRespawnRotations[4];
SavedRaceProgress savedRespawnRaceProgress[4];
bool hasSavedRespawn[4] = {false, false, false, false};
bool hasGrantedItem[4] = {false, false, false, false};
bool canRefillOnUse[4] = {false, false, false, false};

void ClearSavedRespawns() {
    for (u32 i = 0; i < 4; ++i) {
        respawnShortcutTimers[i] = 0;
        respawnSaveTimers[i] = 0;
        hasSavedRespawn[i] = false;
    }
}

static SectionLoadHook ClearSavedRespawnsOnSectionLoad(ClearSavedRespawns);

void SetPracticeMode(bool enabled) {
    isPracticeMode = enabled;
    ClearSavedRespawns();
    for (u32 i = 0; i < 4; ++i) {
        selectedItemIndexes[i] = 0;
        stickWheelDirections[i] = 0;
        hasGrantedItem[i] = false;
        canRefillOnUse[i] = false;
    }
}

bool IsPracticeMode() {
    return isPracticeMode;
}

bool AreItemBoxesEnabled() {
    if (!Settings::Mgr::IsCreated()) return true;
    return Settings::Mgr::Get().GetUserSettingValue(Settings::SETTINGSTYPE_TTPRACTICE, RADIO_TTPRACTICE_ITEMBOXES) ==
           TTPRACTICE_ITEMBOXES_ENABLED;
}

bool IsObjectFreezeEnabled() {
    if (!Settings::Mgr::IsCreated()) return true;
    return Settings::Mgr::Get().GetUserSettingValue(Settings::SETTINGSTYPE_TTPRACTICE, RADIO_TTPRACTICE_OBJECTFREEZE) ==
           TTPRACTICE_OBJECTFREEZE_ENABLED;
}

ItemId GetStartingItem(u32 hudSlotId) {
    if (hudSlotId >= 4) hudSlotId = 0;
    return ITEM_WHEEL_ITEMS[selectedItemIndexes[hudSlotId]];
}

bool IsEnabled() {
    const Racedata* racedata = Racedata::sInstance;
    return isPracticeMode && racedata != nullptr && racedata->racesScenario.settings.gamemode == MODE_TIME_TRIAL;
}

extern "C" void fun_playSound(void*);
extern "C" void ptr_menuPageOrSomething(void*);
asmFunc PlayRespawnSaveSound() {
    ASM(
        nofralloc;
        mflr r11;
        stwu sp, -0x80(sp);
        stmw r3, 0x8(sp);
        lis r11, ptr_menuPageOrSomething @ha;
        lwz r3, ptr_menuPageOrSomething @l(r11);
        li r4, 0xDD;
        lis r12, fun_playSound @h;
        ori r12, r12, fun_playSound @l;
        mtctr r12;
        bctrl;
        lmw r3, 0x8(sp);
        addi sp, sp, 0x80;
        mtlr r11;
        blr;)
}

}  // namespace TTPractice
}  // namespace Pulsar
