#include <runtimeWrite.hpp>
#include <CustomCharacters.hpp>
#include <Settings/Settings.hpp>
#include <MarioKartWii/Archive/ArchiveMgr.hpp>
#include <MarioKartWii/UI/Page/Menu/CharacterSelect.hpp>
#include <MarioKartWii/UI/Page/Menu/KartSelect.hpp>
#include <MarioKartWii/3D/Model/Menu/MenuDriverModel.hpp>
#include <MarioKartWii/3D/Model/Menu/MenuModelMgr.hpp>
#include <MarioKartWii/Mii/MiiHeadsModel.hpp>
#include <MarioKartWii/System/Identifiers.hpp>
#include <MarioKartWii/GlobalFunctions.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <MarioKartWii/Race/RaceData.hpp>
#include <MarioKartWii/UI/Section/SectionMgr.hpp>
#include <MarioKartWii/Scene/GameScene.hpp>
#include <MarioKartWii/Audio/RSARPlayer.hpp>
#include <MarioKartWii/Input/Controller.hpp>

namespace Pulsar {
namespace CustomCharacters {
bool onlineCustomCharacterFlags[12];

enum CustomCharacterTable {
    CUSTOM_CHARACTER_TABLE_DEFAULT = 0,
    CUSTOM_CHARACTER_TABLE_CUSTOM = 1,
    CUSTOM_CHARACTER_TABLE_COUNT = 2
};

static u8 activeCustomCharacterTable = CUSTOM_CHARACTER_TABLE_DEFAULT;
static bool customCharacterTableInitialized = false;
static u8 pendingMenuDriverReinitFrames = 0;
static MenuDriverModelMgr* cachedMenuDriverModels[CUSTOM_CHARACTER_TABLE_COUNT] = {nullptr, nullptr};
static MenuModelMgr* cachedMenuModelMgr = nullptr;
static u8 cachedMenuDriverModelPlayerCount = 0;
static u8 currentMenuDriverModelTable = CUSTOM_CHARACTER_TABLE_DEFAULT;
static CharacterId hoveredCharacterByHud[4] = {MARIO, MARIO, MARIO, MARIO};
static u16 heldCustomCharacterToggleButtons = 0;

void ApplyCharacterTable(u8 tableIdx);
void RefreshLocalOnlineCustomCharacterFlags();
static const char** GetCharacterPostfixEntry(CharacterId character);
static bool IsCharacterSelectPageActive();
static void ReinitializeMenuDriverModels();
static void ProcessPendingMenuDriverModelReinit();
static void ResetMenuDriverModelCache();
static void SyncMenuDriverModelCache();
static void RefreshCharacterSelectModels();
static void RefreshKartSelectModel();
static CharacterId GetPreviewCharacterForHud(u8 hudSlotId);

kmRuntimeUse(0x808b3a90);
static const u32 CHARACTER_POSTFIX_TABLE_ADDRESS = kmRuntimeAddr(0x808b3a90);

static void SetActiveCustomCharacterTable(u8 tableIdx) {
    activeCustomCharacterTable = tableIdx % CUSTOM_CHARACTER_TABLE_COUNT;
    customCharacterTableInitialized = true;
    ApplyCharacterTable(activeCustomCharacterTable);
    RefreshLocalOnlineCustomCharacterFlags();
}

static void ToggleCustomCharacterTable(bool moveRight) {
    if (GetLocalPlayerCount() != 1) return;

    if (moveRight) {
        SetActiveCustomCharacterTable((activeCustomCharacterTable + 1) % CUSTOM_CHARACTER_TABLE_COUNT);
    } else {
        SetActiveCustomCharacterTable((activeCustomCharacterTable + CUSTOM_CHARACTER_TABLE_COUNT - 1) % CUSTOM_CHARACTER_TABLE_COUNT);
    }
    pendingMenuDriverReinitFrames = 2;
}

static void EnsureActiveCustomCharacterTable() {
    if (customCharacterTableInitialized) {
        ApplyCharacterTable(activeCustomCharacterTable);
        return;
    }

    activeCustomCharacterTable = CUSTOM_CHARACTER_TABLE_DEFAULT;
    customCharacterTableInitialized = true;
    ApplyCharacterTable(activeCustomCharacterTable);
}

bool IsCustomCharacterTableActive() {
    return activeCustomCharacterTable == CUSTOM_CHARACTER_TABLE_CUSTOM && GetLocalPlayerCount() == 1;
}

bool IsOnlineRoom(const RKNet::Controller* controller) {
    if (controller == nullptr) return false;

    switch (controller->roomType) {
        case RKNet::ROOMTYPE_VS_WW:
        case RKNet::ROOMTYPE_VS_REGIONAL:
        case RKNet::ROOMTYPE_BT_WW:
        case RKNet::ROOMTYPE_BT_REGIONAL:
        case RKNet::ROOMTYPE_FROOM_HOST:
        case RKNet::ROOMTYPE_FROOM_NONHOST:
        case RKNet::ROOMTYPE_JOINING_WW:
        case RKNet::ROOMTYPE_JOINING_REGIONAL:
            return true;
        default:
            return false;
    }
}

bool IsOnlineMultiLocal(const RKNet::Controller* controller) {
    return IsOnlineRoom(controller) && GetLocalPlayerCount() > 1;
}

bool isDisplayCustomSkinsEnabled() {
    return Pulsar::Settings::Mgr::Get().GetUserSettingValue(Pulsar::Settings::SETTINGSTYPE_ONLINE, Pulsar::RADIO_DISPLAYCUSTOMSKINS) == Pulsar::DISPLAYCUSTOMSKINS_ENABLED;
}

const char* GetCustomCharacterPostfix(CharacterId character) {
    switch (character) {
        case MARIO:
            return "sm";
        case BABY_PEACH:
            return "cpc";
        case WALUIGI:
            return "vw";
        case BOWSER:
            return "db";
        case BABY_DAISY:
            return "rds";
        case DRY_BONES:
            return "bb";
        case BABY_MARIO:
            return "kmr";
        case LUIGI:
            return "cl";
        case TOAD:
            return "ct";
        case DONKEY_KONG:
            return "gd";
        case YOSHI:
            return "ky";
        case WARIO:
            return "hw";
        case BABY_LUIGI:
            return "clg";
        case TOADETTE:
            return "et";
        case KOOPA_TROOPA:
            return "pk";
        case DAISY:
            return "sd";
        case PEACH:
            return "ap";
        case BIRDO:
            return "rb";
        case DIDDY_KONG:
            return "ad";
        case KING_BOO:
            return "kb";
        case BOWSER_JR:
            return "pj";
        case DRY_BOWSER:
            return "gk";
        case FUNKY_KONG:
            return "ck";
        case ROSALINA:
            return "ar";
        case PEACH_BIKER:
            return "ap";
        case DAISY_BIKER:
            return "sd";
        case ROSALINA_BIKER:
            return "ar";
        default:
            return nullptr;
    }
}

const char* GetDefaultCharacterPostfix(CharacterId character) {
    switch (character) {
        case MARIO:
            return "mr";
        case BABY_PEACH:
            return "bpc";
        case WALUIGI:
            return "wl";
        case BOWSER:
            return "kp";
        case BABY_DAISY:
            return "bds";
        case DRY_BONES:
            return "ka";
        case BABY_MARIO:
            return "bmr";
        case LUIGI:
            return "lg";
        case TOAD:
            return "ko";
        case DONKEY_KONG:
            return "dk";
        case YOSHI:
            return "ys";
        case WARIO:
            return "wr";
        case BABY_LUIGI:
            return "blg";
        case TOADETTE:
            return "kk";
        case KOOPA_TROOPA:
            return "nk";
        case DAISY:
            return "ds";
        case PEACH:
            return "pc";
        case BIRDO:
            return "ca";
        case DIDDY_KONG:
            return "dd";
        case KING_BOO:
            return "kt";
        case BOWSER_JR:
            return "jr";
        case DRY_BOWSER:
            return "bk";
        case FUNKY_KONG:
            return "fk";
        case ROSALINA:
            return "rs";
        case PEACH_BIKER:
            return "pc";
        case DAISY_BIKER:
            return "ds";
        case ROSALINA_BIKER:
            return "rs";
        default:
            return nullptr;
    }
}

static void ApplyCharacterPostfix(CharacterId character, bool useCustomPostfix) {
    const char** entry = GetCharacterPostfixEntry(character);
    if (entry == nullptr) return;

    const char* postfix = useCustomPostfix ? GetCustomCharacterPostfix(character) : GetDefaultCharacterPostfix(character);
    if (postfix != nullptr) *entry = postfix;
}

void ApplyCharacterTable(u8 tableIdx) {
    const bool useCustomPostfix = tableIdx == CUSTOM_CHARACTER_TABLE_CUSTOM;

    ApplyCharacterPostfix(MARIO, useCustomPostfix);
    ApplyCharacterPostfix(BABY_PEACH, useCustomPostfix);
    ApplyCharacterPostfix(WALUIGI, useCustomPostfix);
    ApplyCharacterPostfix(BOWSER, useCustomPostfix);
    ApplyCharacterPostfix(BABY_DAISY, useCustomPostfix);
    ApplyCharacterPostfix(DRY_BONES, useCustomPostfix);
    ApplyCharacterPostfix(BABY_MARIO, useCustomPostfix);
    ApplyCharacterPostfix(LUIGI, useCustomPostfix);
    ApplyCharacterPostfix(TOAD, useCustomPostfix);
    ApplyCharacterPostfix(DONKEY_KONG, useCustomPostfix);
    ApplyCharacterPostfix(YOSHI, useCustomPostfix);
    ApplyCharacterPostfix(WARIO, useCustomPostfix);
    ApplyCharacterPostfix(BABY_LUIGI, useCustomPostfix);
    ApplyCharacterPostfix(TOADETTE, useCustomPostfix);
    ApplyCharacterPostfix(KOOPA_TROOPA, useCustomPostfix);
    ApplyCharacterPostfix(DAISY, useCustomPostfix);
    ApplyCharacterPostfix(PEACH, useCustomPostfix);
    ApplyCharacterPostfix(BIRDO, useCustomPostfix);
    ApplyCharacterPostfix(DIDDY_KONG, useCustomPostfix);
    ApplyCharacterPostfix(KING_BOO, useCustomPostfix);
    ApplyCharacterPostfix(BOWSER_JR, useCustomPostfix);
    ApplyCharacterPostfix(DRY_BOWSER, useCustomPostfix);
    ApplyCharacterPostfix(FUNKY_KONG, useCustomPostfix);
    ApplyCharacterPostfix(ROSALINA, useCustomPostfix);
    ApplyCharacterPostfix(PEACH_BIKER, useCustomPostfix);
    ApplyCharacterPostfix(DAISY_BIKER, useCustomPostfix);
    ApplyCharacterPostfix(ROSALINA_BIKER, useCustomPostfix);
}

bool IsRaceSectionActive() {
    const SectionMgr* sectionMgr = SectionMgr::sInstance;
    if (sectionMgr == nullptr || sectionMgr->curSection == nullptr) return false;
    return IsGameplaySection(sectionMgr->curSection->sectionId) == SCENE_ID_RACE;
}

bool IsLocalRacePlayer(u8 playerId) {
    const Racedata* racedata = Racedata::sInstance;
    if (racedata == nullptr) return false;

    const RacedataScenario& scenario = racedata->racesScenario;
    const u8 localPlayerCount = scenario.localPlayerCount > 4 ? 4 : scenario.localPlayerCount;

    for (u8 hudSlotId = 0; hudSlotId < localPlayerCount; ++hudSlotId) {
        const u32 localPlayerId = racedata->GetPlayerIdOfLocalPlayer(hudSlotId);
        if (localPlayerId == playerId) return true;
    }
    return false;
}

bool ShouldUseCustomCharacterForArchivePlayer(u8 playerId) {
    const RKNet::Controller* controller = RKNet::Controller::sInstance;
    if (IsOnlineRoom(controller)) {
        if (IsOnlineMultiLocal(controller)) return false;
        if (!isDisplayCustomSkinsEnabled()) return false;
        if (IsLocalRacePlayer(playerId)) return IsCustomCharacterTableActive();
        return playerId < 12 && onlineCustomCharacterFlags[playerId];
    }
    return IsCustomCharacterTableActive() && IsLocalRacePlayer(playerId);
}

static const char** GetCharacterPostfixEntry(CharacterId character) {
    if (character >= 0x30) return nullptr;

    const char** table = reinterpret_cast<const char**>(CHARACTER_POSTFIX_TABLE_ADDRESS);
    return &table[character];
}

class ScopedCharacterPostfixSwap {
   public:
    ScopedCharacterPostfixSwap(u8 playerId, CharacterId character) : entry(nullptr), previousValue(nullptr) {
        this->entry = GetCharacterPostfixEntry(character);
        if (this->entry == nullptr) return;

        this->previousValue = *this->entry;
        if (ShouldUseCustomCharacterForArchivePlayer(playerId)) {
            *this->entry = GetCustomCharacterPostfix(character);
        } else {
            *this->entry = GetDefaultCharacterPostfix(character);
        }
    }

    ~ScopedCharacterPostfixSwap() {
        if (this->entry != nullptr) *this->entry = this->previousValue;
    }

   private:
    const char** entry;
    const char* previousValue;
};

void ResetOnlineCustomCharacterFlags() {
    for (int playerId = 0; playerId < 12; ++playerId) {
        onlineCustomCharacterFlags[playerId] = false;
    }
}

static u8 GetMenuModelPlayerCount() {
    MenuModelMgr* menuModelMgr = MenuModelMgr::sInstance;
    if (menuModelMgr == nullptr) return 0;
    return menuModelMgr->playerCount;
}

static void ResetMenuDriverModelCache() {
    pendingMenuDriverReinitFrames = 0;
    cachedMenuModelMgr = nullptr;
    cachedMenuDriverModelPlayerCount = 0;
    currentMenuDriverModelTable = CUSTOM_CHARACTER_TABLE_DEFAULT;
    for (u32 tableIdx = 0; tableIdx < CUSTOM_CHARACTER_TABLE_COUNT; ++tableIdx) {
        cachedMenuDriverModels[tableIdx] = nullptr;
    }
    const SectionMgr* const sectionMgr = SectionMgr::sInstance;
    if (sectionMgr != nullptr && sectionMgr->sectionParams != nullptr) {
        const u8 localPlayerCount = sectionMgr->sectionParams->localPlayerCount > 4 ? 4 : sectionMgr->sectionParams->localPlayerCount;
        for (u8 hudSlotId = 0; hudSlotId < localPlayerCount; ++hudSlotId) {
            hoveredCharacterByHud[hudSlotId] = sectionMgr->sectionParams->characters[hudSlotId];
        }
    }
}

static void SyncMenuDriverModelCache() {
    MenuModelMgr* const menuModelMgr = MenuModelMgr::sInstance;
    if (menuModelMgr == nullptr) {
        ResetMenuDriverModelCache();
        return;
    }

    const bool shouldResetCache = cachedMenuModelMgr != menuModelMgr || cachedMenuDriverModelPlayerCount != menuModelMgr->playerCount;
    if (shouldResetCache) {
        cachedMenuModelMgr = menuModelMgr;
        cachedMenuDriverModelPlayerCount = menuModelMgr->playerCount;
        for (u32 tableIdx = 0; tableIdx < CUSTOM_CHARACTER_TABLE_COUNT; ++tableIdx) {
            cachedMenuDriverModels[tableIdx] = nullptr;
        }
    }

    if (cachedMenuDriverModels[currentMenuDriverModelTable] == nullptr) {
        cachedMenuDriverModels[currentMenuDriverModelTable] = menuModelMgr->driverModels;
    }
}

static void UnlockHeap(EGG::Heap* heap) {
    if (heap != nullptr) heap->dameFlag &= ~0x1;
}

static void UnlockMenuDriverModelHeaps(GameScene& scene, MenuModelMgr& menuModelMgr) {
    UnlockHeap(scene.parentHeap);
    UnlockHeap(scene.otherMEMHeap);
    UnlockHeap(scene.mainMEMHeap);
    UnlockHeap(scene.debugHeap);

    for (u32 i = 0; i < 3; ++i) {
        UnlockHeap(scene.structsHeaps.heaps[i]);
        UnlockHeap(scene.archiveHeaps.heaps[i]);
    }

    UnlockHeap(menuModelMgr.heap);
    UnlockHeap(menuModelMgr.otherHeap);
}

kmRuntimeUse(0x8059dfbc);
static MenuModelMgr* CreateMenuModelManager() {
    typedef MenuModelMgr* (*CreateMenuModelManagerFn)();
    const CreateMenuModelManagerFn original = reinterpret_cast<CreateMenuModelManagerFn>(kmRuntimeAddr(0x8059dfbc));
    return original();
}

kmRuntimeUse(0x8059e04c);
static void DestroyMenuModelManager() {
    typedef void (*DestroyMenuModelManagerFn)();
    const DestroyMenuModelManagerFn original = reinterpret_cast<DestroyMenuModelManagerFn>(kmRuntimeAddr(0x8059e04c));
    original();
}

static void ClearMenuDriverModels(MenuDriverModelMgr& driverModelMgr) {
    MiiHeadsModel* const* miiHeads = reinterpret_cast<MiiHeadsModel* const*>(driverModelMgr.miiHeads);
    for (u8 playerId = 0; playerId < driverModelMgr.playerCount; ++playerId) {
        MenuDriverModel* const playerModel = driverModelMgr.players[playerId].playerModel;
        MiiHeadsModel* miiHeadModel = nullptr;
        if (miiHeads != nullptr) {
            miiHeadModel = miiHeads[playerId];
        }
        driverModelMgr.players[playerId].isVisible = false;
        if (playerModel != nullptr && playerModel->model != nullptr) {
            playerModel->ToggleVisible(false);
        }
        if (miiHeadModel != nullptr && miiHeadModel->curScnMdlEx != nullptr) {
            miiHeadModel->ToggleVisible(false);
        }
    }
}

kmRuntimeUse(0x8059e250);
kmRuntimeUse(0x805f5c94);
static void InitMenuModelManager(MenuModelMgr& menuModelMgr, u8 playerCount) {
    typedef void (*InitMenuModelManagerFn)(MenuModelMgr*, u8, void*);
    const InitMenuModelManagerFn original = reinterpret_cast<InitMenuModelManagerFn>(kmRuntimeAddr(0x8059e250));
    original(&menuModelMgr, playerCount, reinterpret_cast<void*>(kmRuntimeAddr(0x805f5c94)));
}

kmRuntimeUse(0x8059e1fc);
static void StartMenuModelManager(MenuModelMgr& menuModelMgr) {
    typedef void (*StartMenuModelManagerFn)(MenuModelMgr*);
    const StartMenuModelManagerFn original = reinterpret_cast<StartMenuModelManagerFn>(kmRuntimeAddr(0x8059e1fc));
    original(&menuModelMgr);
}

kmRuntimeUse(0x80830180);
static MenuDriverModelMgr* CreateMenuDriverModelManager(u8 playerCount) {
    typedef MenuDriverModelMgr* (*CreateMenuDriverModelManagerFn)(MenuDriverModelMgr*, u8);
    void* memory = operator new(sizeof(MenuDriverModelMgr));
    if (memory == nullptr) return nullptr;

    const CreateMenuDriverModelManagerFn original = reinterpret_cast<CreateMenuDriverModelManagerFn>(kmRuntimeAddr(0x80830180));
    return original(reinterpret_cast<MenuDriverModelMgr*>(memory), playerCount);
}

kmRuntimeUse(0x80830748);
static void StartMenuDriverModelManager(MenuDriverModelMgr& driverModelMgr) {
    typedef void (*StartMenuDriverModelManagerFn)(MenuDriverModelMgr*);
    const StartMenuDriverModelManagerFn original = reinterpret_cast<StartMenuDriverModelManagerFn>(kmRuntimeAddr(0x80830748));
    original(&driverModelMgr);
}

static void ReinitializeMenuDriverModels() {
    MenuModelMgr* const menuModelMgr = MenuModelMgr::sInstance;
    GameScene* const currentScene = const_cast<GameScene*>(GameScene::GetCurrent());
    const SectionMgr* const sectionMgr = SectionMgr::sInstance;
    if (menuModelMgr == nullptr || currentScene == nullptr || sectionMgr == nullptr || sectionMgr->sectionParams == nullptr) return;
    if (menuModelMgr->heap == nullptr) return;

    SyncMenuDriverModelCache();

    const u8 playerCount = GetMenuModelPlayerCount();
    if (playerCount == 0) return;

    MenuDriverModelMgr* const oldDriverModels = menuModelMgr->driverModels;
    MenuDriverModelMgr* newDriverModels = cachedMenuDriverModels[activeCustomCharacterTable];

    if (newDriverModels == nullptr) {
        UnlockMenuDriverModelHeaps(*currentScene, *menuModelMgr);
        currentScene->structsHeaps.SetHeapsGroupId(3);
        newDriverModels = CreateMenuDriverModelManager(playerCount);
        currentScene->structsHeaps.SetHeapsGroupId(0);
        if (newDriverModels == nullptr) {
            currentScene->structsHeaps.SetHeapsGroupId(6);
            return;
        }
        cachedMenuDriverModels[activeCustomCharacterTable] = newDriverModels;
    }

    if (oldDriverModels != nullptr && oldDriverModels != newDriverModels) {
        ClearMenuDriverModels(*oldDriverModels);
    }

    menuModelMgr->driverModels = newDriverModels;
    StartMenuDriverModelManager(*newDriverModels);
    currentScene->structsHeaps.SetHeapsGroupId(6);

    const u8 localPlayerCount = sectionMgr->sectionParams->localPlayerCount > 4 ? 4 : sectionMgr->sectionParams->localPlayerCount;
    for (u8 hudSlotId = 0; hudSlotId < localPlayerCount; ++hudSlotId) {
        menuModelMgr->RequestDriverModel(hudSlotId, sectionMgr->sectionParams->characters[hudSlotId]);
    }

    if (oldDriverModels != nullptr && oldDriverModels != newDriverModels) {
        for (u8 playerId = 0; playerId < playerCount; ++playerId) {
            const bool isVisible = oldDriverModels->players[playerId].isVisible;
            newDriverModels->players[playerId].isVisible = isVisible;
            newDriverModels->TogglePlayerModel(playerId, isVisible);
        }
    }

    RefreshCharacterSelectModels();
    RefreshKartSelectModel();
    currentMenuDriverModelTable = activeCustomCharacterTable;
}

static void ProcessPendingMenuDriverModelReinit() {
    if (pendingMenuDriverReinitFrames == 0) return;
    if (--pendingMenuDriverReinitFrames != 0) return;
    if (!IsCharacterSelectPageActive()) return;

    ReinitializeMenuDriverModels();
}

static void RefreshCharacterSelectModels() {
    SectionMgr* const sectionMgr = SectionMgr::sInstance;
    if (sectionMgr == nullptr || sectionMgr->sectionParams == nullptr || sectionMgr->curSection == nullptr) return;

    Pages::CharacterSelect* const characterSelect = sectionMgr->curSection->Get<Pages::CharacterSelect>();
    if (characterSelect == nullptr || characterSelect->models == nullptr) return;

    const u8 localPlayerCount = sectionMgr->sectionParams->localPlayerCount > 4 ? 4 : sectionMgr->sectionParams->localPlayerCount;
    for (u8 hudSlotId = 0; hudSlotId < localPlayerCount; ++hudSlotId) {
        characterSelect->models[hudSlotId].RequestModel(GetPreviewCharacterForHud(hudSlotId));
    }
}

static CharacterId GetPreviewCharacterForHud(u8 hudSlotId) {
    if (hudSlotId >= 4) return MARIO;

    const SectionMgr* const sectionMgr = SectionMgr::sInstance;
    if (sectionMgr == nullptr || sectionMgr->sectionParams == nullptr) {
        return hoveredCharacterByHud[hudSlotId];
    }

    CharacterId previewCharacter = hoveredCharacterByHud[hudSlotId];
    if (previewCharacter >= 0x30) {
        previewCharacter = sectionMgr->sectionParams->characters[hudSlotId];
    }
    return previewCharacter;
}

static void RefreshKartSelectModel() {
    SectionMgr* const sectionMgr = SectionMgr::sInstance;
    if (sectionMgr == nullptr || sectionMgr->sectionParams == nullptr || sectionMgr->curSection == nullptr) return;

    Pages::KartSelect* const kartSelect = sectionMgr->curSection->Get<Pages::KartSelect>();
    if (kartSelect == nullptr) return;
    if (sectionMgr->sectionParams->localPlayerCount == 0) return;

    kartSelect->vehicleModel.RequestModel(sectionMgr->sectionParams->karts[0]);
}

static const char* GetCustomDriverBRRESName(u32 character) {
    switch (character) {
        case 0x2d:
            return "ap_menu";
        case 0x2e:
            return "sd_menu";
        case 0x2f:
            return "ar_menu";
        default:
            return GetCustomCharacterPostfix(static_cast<CharacterId>(character));
    }
}

static const char* GetDefaultDriverBRRESName(u32 character) {
    switch (character) {
        case 0x2d:
            return "pc_menu";
        case 0x2e:
            return "ds_menu";
        case 0x2f:
            return "rs_menu";
        default:
            return GetDefaultCharacterPostfix(static_cast<CharacterId>(character));
    }
}

static bool IsCharacterSelectPageActive() {
    const SectionMgr* sectionMgr = SectionMgr::sInstance;
    if (sectionMgr == nullptr || sectionMgr->curSection == nullptr) return false;

    const Pages::CharacterSelect* characterSelect = sectionMgr->curSection->Get<Pages::CharacterSelect>();
    if (characterSelect == nullptr) return false;

    const Page* topPage = sectionMgr->curSection->GetTopLayerPage();
    return topPage != nullptr && topPage->pageId == PAGE_CHARACTER_SELECT;
}

void RefreshLocalOnlineCustomCharacterFlags() {
    const RKNet::Controller* controller = RKNet::Controller::sInstance;
    if (!IsOnlineRoom(controller)) return;
    if (IsOnlineMultiLocal(controller)) return;
    if (!isDisplayCustomSkinsEnabled()) return;

    const Racedata* racedata = Racedata::sInstance;
    if (racedata == nullptr) return;

    const bool isCustomTableActive = IsCustomCharacterTableActive();
    const RacedataScenario& scenario = racedata->racesScenario;
    const u8 localPlayerCount = scenario.localPlayerCount > 4 ? 4 : scenario.localPlayerCount;

    for (u8 hudSlotId = 0; hudSlotId < localPlayerCount; ++hudSlotId) {
        const u32 playerId = racedata->GetPlayerIdOfLocalPlayer(hudSlotId);
        if (playerId < 12) {
            onlineCustomCharacterFlags[playerId] = isCustomTableActive;
        }
    }
}

void UpdateOnlineCharacterTablesFromAid(u8 aid, const u8* playerIdToAid, u8 characterTables) {
    if (playerIdToAid == nullptr) return;

    u8 hudSlotId = 0;
    for (u8 playerId = 0; playerId < 12; ++playerId) {
        if (playerIdToAid[playerId] != aid) continue;

        const u8 tableIdx = hudSlotId < 2 ? ((characterTables >> hudSlotId) & 1) : CUSTOM_CHARACTER_TABLE_DEFAULT;
        onlineCustomCharacterFlags[playerId] = tableIdx == CUSTOM_CHARACTER_TABLE_CUSTOM;
        ++hudSlotId;
    }
}

u8 GetLocalOnlineCharacterTables() {
    return IsCustomCharacterTableActive() ? CUSTOM_CHARACTER_TABLE_CUSTOM : CUSTOM_CHARACTER_TABLE_DEFAULT;
}

bool ShouldUseCustomCharacterForPlayer(u8 playerId) {
    return playerId < 12 && onlineCustomCharacterFlags[playerId];
}

kmRuntimeUse(0x80540e3c);
static ArchivesHolder* LoadKartArchiveHook(ArchiveMgr* archiveMgr, u8 playerId, KartId kart, CharacterId character, u32 color, u32 type, EGG::Heap* decompressedHeap, EGG::Heap* archiveHeap) {
    typedef ArchivesHolder* (*LoadKartArchiveFn)(ArchiveMgr*, u8, KartId, CharacterId, u32, u32, EGG::Heap*, EGG::Heap*);
    const LoadKartArchiveFn original = reinterpret_cast<LoadKartArchiveFn>(kmRuntimeAddr(0x80540e3c));
    ScopedCharacterPostfixSwap swap(playerId, character);
    return original(archiveMgr, playerId, kart, character, color, type, decompressedHeap, archiveHeap);
}
kmCall(0x805540f4, LoadKartArchiveHook);

kmRuntimeUse(0x80540f90);
static ArchivesHolder* LoadKartArchiveHolder2Hook(ArchiveMgr* archiveMgr, u8 playerId, KartId kart, CharacterId character, u32 color, u32 type, EGG::Heap* decompressedHeap, EGG::Heap* archiveHeap) {
    typedef ArchivesHolder* (*LoadKartArchiveHolder2Fn)(ArchiveMgr*, u8, KartId, CharacterId, u32, u32, EGG::Heap*, EGG::Heap*);
    const LoadKartArchiveHolder2Fn original = reinterpret_cast<LoadKartArchiveHolder2Fn>(kmRuntimeAddr(0x80540f90));
    ScopedCharacterPostfixSwap swap(playerId, character);
    return original(archiveMgr, playerId, kart, character, color, type, decompressedHeap, archiveHeap);
}
kmCall(0x80554198, LoadKartArchiveHolder2Hook);

kmRuntimeUse(0x805410e4);
static ArchivesHolder* LoadMenuKartArchiveHook(ArchiveMgr* archiveMgr, u8 playerId, CharacterId character, u32 type, EGG::Heap* archiveHeap, EGG::Heap* dumpHeap) {
    typedef ArchivesHolder* (*LoadMenuKartArchiveFn)(ArchiveMgr*, u8, CharacterId, u32, EGG::Heap*, EGG::Heap*);
    const LoadMenuKartArchiveFn original = reinterpret_cast<LoadMenuKartArchiveFn>(kmRuntimeAddr(0x805410e4));
    ScopedCharacterPostfixSwap swap(playerId, character);
    return original(archiveMgr, playerId, character, type, archiveHeap, dumpHeap);
}
kmBranch(0x805410e4, LoadMenuKartArchiveHook);

kmRuntimeUse(0x805419c8);
static const char* GetMenuDriverBRRESNameHook(u32 character) {
    const char* overrideName = IsCustomCharacterTableActive() ? GetCustomDriverBRRESName(character) : GetDefaultDriverBRRESName(character);
    if (overrideName != nullptr) return overrideName;

    typedef const char* (*GetCharacterNameFn)(u32);
    const GetCharacterNameFn original = reinterpret_cast<GetCharacterNameFn>(kmRuntimeAddr(0x805419c8));
    return original(character);
}
kmCall(0x8081e4a0, GetMenuDriverBRRESNameHook);

static void RefreshOnlineCustomCharacterFlagsOnRaceLoad() {
    RefreshLocalOnlineCustomCharacterFlags();
}
static RaceLoadHook RefreshOnlineCustomCharacterFlagsHook(RefreshOnlineCustomCharacterFlagsOnRaceLoad);

void SetCharacter() {
    if (!IsOnlineRoom(RKNet::Controller::sInstance)) ResetOnlineCustomCharacterFlags();
    ResetMenuDriverModelCache();
    EnsureActiveCustomCharacterTable();
    currentMenuDriverModelTable = activeCustomCharacterTable;
}
static SectionLoadHook SetCharacterHook(SetCharacter);

kmRuntimeUse(0x8083e5f4);
static void CharacterSelectHoverHook(Pages::CharacterSelect* page, CtrlMenuCharacterSelect::ButtonDriver* button, u32 buttonId, u8 hudSlotId) {
    if (hudSlotId < 4) hoveredCharacterByHud[hudSlotId] = static_cast<CharacterId>(buttonId);

    typedef void (*CharacterSelectHoverFn)(Pages::CharacterSelect*, CtrlMenuCharacterSelect::ButtonDriver*, u32, u8);
    const CharacterSelectHoverFn original = reinterpret_cast<CharacterSelectHoverFn>(kmRuntimeAddr(0x8083e5f4));
    original(page, button, buttonId, hudSlotId);
}
kmCall(0x807e2cf0, CharacterSelectHoverHook);
kmCall(0x807e304c, CharacterSelectHoverHook);
kmCall(0x807e34d0, CharacterSelectHoverHook);
kmCall(0x807e37b0, CharacterSelectHoverHook);
kmCall(0x807e3a88, CharacterSelectHoverHook);

kmRuntimeUse(0x8083dfa8);
static void CharacterSelectClickHook(Pages::CharacterSelect* page, PushButton* button, u32 buttonId, u8 hudSlotId) {
    if (hudSlotId < 4) hoveredCharacterByHud[hudSlotId] = static_cast<CharacterId>(buttonId);

    typedef void (*CharacterSelectClickFn)(Pages::CharacterSelect*, PushButton*, u32, u8);
    const CharacterSelectClickFn original = reinterpret_cast<CharacterSelectClickFn>(kmRuntimeAddr(0x8083dfa8));
    original(page, button, buttonId, hudSlotId);
}
kmCall(0x807e3570, CharacterSelectClickHook);
kmCall(0x807e36bc, CharacterSelectClickHook);
kmCall(0x807e39b4, CharacterSelectClickHook);
kmCall(0x807e3bd0, CharacterSelectClickHook);

static ControllerType GetControllerTypeForHudSlot(const SectionMgr& sectionMgr, u8 hudSlotId) {
    if (hudSlotId >= 4) return GCN;

    const Input::RealControllerHolder* holder = sectionMgr.pad.padInfos[hudSlotId].controllerHolder;
    if (holder == nullptr || holder->curController == nullptr) return GCN;

    const ControllerType type = holder->curController->GetType();
    if (type == WHEEL || type == NUNCHUCK || type == CLASSIC || type == GCN) return type;
    return GCN;
}

static void HideAllCustomCharacterHintPanes(CharaName& nameUi) {
    nameUi.SetPaneVisibility("cc_prev_gc", false);
    nameUi.SetPaneVisibility("cc_next_gc", false);
    nameUi.SetPaneVisibility("cc_prev_cls", false);
    nameUi.SetPaneVisibility("cc_next_cls", false);
    nameUi.SetPaneVisibility("cc_prev_nc", false);
    nameUi.SetPaneVisibility("cc_next_nc", false);
    nameUi.SetPaneVisibility("cc_prev_wh", false);
    nameUi.SetPaneVisibility("cc_next_wh", false);
}

static void ApplyCustomCharacterHintPanesForType(CharaName& nameUi, ControllerType type) {
    HideAllCustomCharacterHintPanes(nameUi);
    switch (type) {
        case WHEEL:
            nameUi.SetPaneVisibility("cc_prev_wh", true);
            nameUi.SetPaneVisibility("cc_next_wh", true);
            break;
        case NUNCHUCK:
            nameUi.SetPaneVisibility("cc_prev_nc", true);
            nameUi.SetPaneVisibility("cc_next_nc", true);
            break;
        case CLASSIC:
            nameUi.SetPaneVisibility("cc_prev_cls", true);
            nameUi.SetPaneVisibility("cc_next_cls", true);
            break;
        case GCN:
        default:
            nameUi.SetPaneVisibility("cc_prev_gc", true);
            nameUi.SetPaneVisibility("cc_next_gc", true);
            break;
    }
}

static void UpdateCustomCharacterSelectNamePaneIcons() {
    if (!IsCharacterSelectPageActive()) return;

    SectionMgr* const sectionMgr = SectionMgr::sInstance;
    if (sectionMgr == nullptr || sectionMgr->sectionParams == nullptr) return;

    Pages::CharacterSelect* const characterSelect = sectionMgr->curSection->Get<Pages::CharacterSelect>();
    if (characterSelect == nullptr || characterSelect->names == nullptr) return;

    const u8 localPlayerCount = sectionMgr->sectionParams->localPlayerCount > 4 ? 4 : sectionMgr->sectionParams->localPlayerCount;
    for (u8 hudSlotId = 0; hudSlotId < localPlayerCount; ++hudSlotId) {
        const ControllerType type = GetControllerTypeForHudSlot(*sectionMgr, hudSlotId);
        ApplyCustomCharacterHintPanesForType(characterSelect->names[hudSlotId], type);
    }
}

static void GetCustomCharacterToggleInputs(ControllerType type, u16& previousButton, u16& nextButton, u16& previousAction, u16& nextAction) {
    previousAction = 0;
    nextAction = 0;

    switch (type) {
        case WHEEL:
            previousButton = WPAD::WPAD_BUTTON_B;
            nextButton = WPAD::WPAD_BUTTON_A;
            previousAction = static_cast<u16>(1 << BACK_PRESS);
            nextAction = static_cast<u16>(1 << FORWARD_PRESS);
            break;
        case NUNCHUCK:
            previousButton = WPAD::WPAD_BUTTON_1;
            nextButton = WPAD::WPAD_BUTTON_2;
            previousAction = static_cast<u16>(1 << BACK_PRESS);
            nextAction = static_cast<u16>(1 << FORWARD_PRESS);
            break;
        case CLASSIC:
            previousButton = WPAD::WPAD_CL_TRIGGER_L;
            nextButton = WPAD::WPAD_CL_TRIGGER_R;
            break;
        case GCN:
        default:
            previousButton = PAD::PAD_BUTTON_L;
            nextButton = PAD::PAD_BUTTON_R;
            break;
    }
}

static void ConsumeCustomCharacterToggleInput(Input::RealControllerHolder& controllerHolder, u16 rawButton, u16 uiActionMask) {
    controllerHolder.inputStates[0].buttonRaw &= static_cast<u16>(~rawButton);
    controllerHolder.uiinputStates[0].rawButtons &= static_cast<u16>(~rawButton);
    controllerHolder.uiinputStates[0].buttonActions &= static_cast<u16>(~uiActionMask);
}

static bool ShouldProcessCustomCharacterInput() {
    if (GetLocalPlayerCount() != 1) {
        heldCustomCharacterToggleButtons = 0;
        return false;
    }
    if (!IsCharacterSelectPageActive()) {
        heldCustomCharacterToggleButtons = 0;
        return false;
    }

    const SectionMgr* sectionMgr = SectionMgr::sInstance;
    if (sectionMgr == nullptr || sectionMgr->pad.padInfos[0].controllerHolder == nullptr) {
        heldCustomCharacterToggleButtons = 0;
        return false;
    }

    Input::RealControllerHolder* controllerHolder = sectionMgr->pad.padInfos[0].controllerHolder;
    const Input::Controller* controller = controllerHolder->curController;
    if (controller == nullptr) {
        heldCustomCharacterToggleButtons = 0;
        return false;
    }

    const u16 inputs = controllerHolder->inputStates[0].buttonRaw;

    u16 previousButton = 0;
    u16 nextButton = 0;
    u16 previousAction = 0;
    u16 nextAction = 0;
    GetCustomCharacterToggleInputs(GetControllerTypeForHudSlot(*sectionMgr, 0), previousButton, nextButton, previousAction, nextAction);

    const bool isPreviousHeld = (inputs & previousButton) != 0;
    const bool isNextHeld = (inputs & nextButton) != 0;
    const u16 toggleButtons = static_cast<u16>(previousButton | nextButton);
    const u16 heldToggleButtons = static_cast<u16>(inputs & toggleButtons);
    const u16 newToggleButtons = static_cast<u16>(heldToggleButtons & ~heldCustomCharacterToggleButtons);
    heldCustomCharacterToggleButtons = heldToggleButtons;

    if (isPreviousHeld) ConsumeCustomCharacterToggleInput(*controllerHolder, previousButton, previousAction);
    if (isNextHeld) ConsumeCustomCharacterToggleInput(*controllerHolder, nextButton, nextAction);

    if ((newToggleButtons & previousButton) != 0) {
        Audio::RSARPlayer::PlaySoundById(SOUND_ID_LEFT_ARROW_PRESS, 0, 0);
        ToggleCustomCharacterTable(false);
        return true;
    }
    if ((newToggleButtons & nextButton) != 0) {
        Audio::RSARPlayer::PlaySoundById(SOUND_ID_RIGHT_ARROW_PRESS, 0, 0);
        ToggleCustomCharacterTable(true);
        return true;
    }
    return false;
}

kmRuntimeUse(0x8063550c);
// RaceScene::calcSubsystems -> SectionMgr::Update call site
static void RaceSceneSectionUpdateHook(SectionMgr* sectionMgr) {
    UpdateCustomCharacterSelectNamePaneIcons();
    ShouldProcessCustomCharacterInput();
    typedef void (*SectionMgrUpdateFn)(SectionMgr*);
    const SectionMgrUpdateFn original = reinterpret_cast<SectionMgrUpdateFn>(kmRuntimeAddr(0x8063550c));
    original(sectionMgr);
    ProcessPendingMenuDriverModelReinit();
}
kmCall(0x80554dcc, RaceSceneSectionUpdateHook);

kmRuntimeUse(0x8063583c);
// MenuScene_vf30 / GlobeScene_vf30 -> SectionMgr::MenuUpdate call sites
static void MenuSceneSectionUpdateHook(SectionMgr* sectionMgr) {
    UpdateCustomCharacterSelectNamePaneIcons();
    ShouldProcessCustomCharacterInput();
    typedef void (*SectionMgrUpdateFn)(SectionMgr*);
    const SectionMgrUpdateFn original = reinterpret_cast<SectionMgrUpdateFn>(kmRuntimeAddr(0x8063583c));
    original(sectionMgr);
    ProcessPendingMenuDriverModelReinit();
}
kmCall(0x805552e8, MenuSceneSectionUpdateHook);
kmCall(0x80553b30, MenuSceneSectionUpdateHook);

}  // namespace CustomCharacters
}  // namespace Pulsar
