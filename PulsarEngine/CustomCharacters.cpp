#include <RetroRewind.hpp>
#include <runtimeWrite.hpp>
#include <CustomCharacters.hpp>
#include <Settings/Settings.hpp>
#include <MarioKartWii/Archive/ArchiveMgr.hpp>
#include <MarioKartWii/System/Identifiers.hpp>
#include <MarioKartWii/GlobalFunctions.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <MarioKartWii/Race/RaceData.hpp>
#include <MarioKartWii/UI/Section/SectionMgr.hpp>
#include <MarioKartWii/Scene/GameScene.hpp>

namespace Pulsar {
namespace CustomCharacters {
bool onlineCustomCharacterFlags[12];

bool IsCustomCharacterSettingEnabled() {
    const u32 character = static_cast<Pulsar::Transmission>(
        Pulsar::Settings::Mgr::Get().GetUserSettingValue(
            static_cast<Pulsar::Settings::UserType>(Pulsar::Settings::SETTINGSTYPE_MISC),
            Pulsar::SCROLLER_CUSTOMCHARACTER));
    return character == Pulsar::CUSTOMCHARACTER_ENABLED && GetLocalPlayerCount() == 1;
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

u16* GetCharacterPostfixSlot(CharacterId character) {
    switch (character) {
        case MARIO:
            return &CUSTOM_MARIO;
        case BABY_PEACH:
            return &CUSTOM_BABY_PEACH;
        case WALUIGI:
            return &CUSTOM_WALUIGI;
        case BOWSER:
            return &CUSTOM_BOWSER;
        case BABY_DAISY:
            return &CUSTOM_BABY_DAISY;
        case DRY_BONES:
            return &CUSTOM_DRY_BONES;
        case BABY_MARIO:
            return &CUSTOM_BABY_MARIO;
        case LUIGI:
            return &CUSTOM_LUIGI;
        case TOAD:
            return &CUSTOM_TOAD;
        case DONKEY_KONG:
            return &CUSTOM_DONKEY_KONG;
        case YOSHI:
            return &CUSTOM_YOSHI;
        case WARIO:
            return &CUSTOM_WARIO;
        case BABY_LUIGI:
            return &CUSTOM_BABY_LUIGI;
        case TOADETTE:
            return &CUSTOM_TOADETTE;
        case KOOPA_TROOPA:
            return &CUSTOM_KOOPA_TROOPA;
        case DAISY:
            return &CUSTOM_DAISY;
        case PEACH:
            return &CUSTOM_PEACH;
        case BIRDO:
            return &CUSTOM_BIRDO;
        case DIDDY_KONG:
            return &CUSTOM_DIDDY_KONG;
        case KING_BOO:
            return &CUSTOM_KING_BOO;
        case BOWSER_JR:
            return &CUSTOM_BOWSER_JR;
        case DRY_BOWSER:
            return &CUSTOM_DRY_BOWSER;
        case FUNKY_KONG:
            return &CUSTOM_FUNKY_KONG;
        case ROSALINA:
            return &CUSTOM_ROSALINA;
        case PEACH_BIKER:
            return &CUSTOM_PEACH_MENU;
        case DAISY_BIKER:
            return &CUSTOM_DAISY_MENU;
        case ROSALINA_BIKER:
            return &CUSTOM_ROSALINA_MENU;
        default:
            return nullptr;
    }
}

u16 GetCustomCharacterPostfix(CharacterId character) {
    switch (character) {
        case MARIO:
            return 'sm';
        case BABY_PEACH:
            return 'cp';
        case WALUIGI:
            return 'vw';
        case BOWSER:
            return 'db';
        case BABY_DAISY:
            return 'rd';
        case DRY_BONES:
            return 'bb';
        case BABY_MARIO:
            return 'km';
        case LUIGI:
            return 'cl';
        case TOAD:
            return 'ct';
        case DONKEY_KONG:
            return 'gd';
        case YOSHI:
            return 'ky';
        case WARIO:
            return 'hw';
        case BABY_LUIGI:
            return 'cl';
        case TOADETTE:
            return 'et';
        case KOOPA_TROOPA:
            return 'pk';
        case DAISY:
            return 'sd';
        case PEACH:
            return 'ap';
        case BIRDO:
            return 'rb';
        case DIDDY_KONG:
            return 'ad';
        case KING_BOO:
            return 'kb';
        case BOWSER_JR:
            return 'pj';
        case DRY_BOWSER:
            return 'gk';
        case FUNKY_KONG:
            return 'ck';
        case ROSALINA:
            return 'ar';
        case PEACH_BIKER:
            return 'ap';
        case DAISY_BIKER:
            return 'sd';
        case ROSALINA_BIKER:
            return 'ar';
        default:
            return 0;
    }
}

u16 GetDefaultCharacterPostfix(CharacterId character) {
    switch (character) {
        case MARIO:
            return 'mr';
        case BABY_PEACH:
            return 'bp';
        case WALUIGI:
            return 'wl';
        case BOWSER:
            return 'kp';
        case BABY_DAISY:
            return 'bd';
        case DRY_BONES:
            return 'ka';
        case BABY_MARIO:
            return 'bm';
        case LUIGI:
            return 'lg';
        case TOAD:
            return 'ko';
        case DONKEY_KONG:
            return 'dk';
        case YOSHI:
            return 'ys';
        case WARIO:
            return 'wr';
        case BABY_LUIGI:
            return 'bl';
        case TOADETTE:
            return 'kk';
        case KOOPA_TROOPA:
            return 'nk';
        case DAISY:
            return 'ds';
        case PEACH:
            return 'pc';
        case BIRDO:
            return 'ca';
        case DIDDY_KONG:
            return 'dd';
        case KING_BOO:
            return 'kt';
        case BOWSER_JR:
            return 'jr';
        case DRY_BOWSER:
            return 'bk';
        case FUNKY_KONG:
            return 'fk';
        case ROSALINA:
            return 'rs';
        case PEACH_BIKER:
            return 'pc';
        case DAISY_BIKER:
            return 'ds';
        case ROSALINA_BIKER:
            return 'rs';
        default:
            return 0;
    }
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
        if (IsLocalRacePlayer(playerId)) return IsCustomCharacterSettingEnabled();
        return playerId < 12 && onlineCustomCharacterFlags[playerId];
    }
    return IsCustomCharacterSettingEnabled() && IsLocalRacePlayer(playerId);
}

class ScopedCharacterPostfixSwap {
   public:
    ScopedCharacterPostfixSwap(u8 playerId, CharacterId character) : slot(nullptr), previousValue(0) {
        this->slot = GetCharacterPostfixSlot(character);
        if (this->slot == nullptr) return;

        this->previousValue = *this->slot;
        if (ShouldUseCustomCharacterForArchivePlayer(playerId))
            *this->slot = GetCustomCharacterPostfix(character);
        else
            *this->slot = GetDefaultCharacterPostfix(character);
    }

    ~ScopedCharacterPostfixSwap() {
        if (this->slot != nullptr) *this->slot = this->previousValue;
    }

   private:
    u16* slot;
    u16 previousValue;
};

void ApplyDefaultCharacterTable() {
    CUSTOM_DRIVER = 'D';
    CUSTOM_BABY_MARIO = 'bm';
    CUSTOM_TOAD = 'ko';
    CUSTOM_MARIO = 'mr';
    CUSTOM_YOSHI = 'ys';
    CUSTOM_WARIO = 'wr';
    CUSTOM_KING_BOO = 'kt';
    CUSTOM_BABY_LUIGI = 'bl';
    CUSTOM_TOADETTE = 'kk';
    CUSTOM_LUIGI = 'lg';
    CUSTOM_BIRDO = 'ca';
    CUSTOM_WALUIGI = 'wl';
    CUSTOM_ROSALINA = 'rs';
    CUSTOM_ROSALINA_MENU = 'rs';
    CUSTOM_BABY_PEACH = 'bp';
    CUSTOM_KOOPA_TROOPA = 'nk';
    CUSTOM_PEACH = 'pc';
    CUSTOM_PEACH_MENU = 'pc';
    CUSTOM_DIDDY_KONG = 'dd';
    CUSTOM_DONKEY_KONG = 'dk';
    CUSTOM_FUNKY_KONG = 'fk';
    CUSTOM_BABY_DAISY = 'bd';
    CUSTOM_DRY_BONES = 'ka';
    CUSTOM_DAISY = 'ds';
    CUSTOM_DAISY_MENU = 'ds';
    CUSTOM_BOWSER_JR = 'jr';
    CUSTOM_BOWSER = 'kp';
    CUSTOM_DRY_BOWSER = 'bk';
}

void ApplyCustomCharacterTable() {
    CUSTOM_DRIVER = 'R';
    CUSTOM_BABY_MARIO = 'km';
    CUSTOM_TOAD = 'ct';
    CUSTOM_MARIO = 'sm';
    CUSTOM_YOSHI = 'ky';
    CUSTOM_WARIO = 'hw';
    CUSTOM_KING_BOO = 'kb';
    CUSTOM_BABY_LUIGI = 'cl';
    CUSTOM_TOADETTE = 'et';
    CUSTOM_LUIGI = 'cl';
    CUSTOM_BIRDO = 'rb';
    CUSTOM_WALUIGI = 'vw';
    CUSTOM_ROSALINA = 'ar';
    CUSTOM_ROSALINA_MENU = 'ar';
    CUSTOM_BABY_PEACH = 'cp';
    CUSTOM_KOOPA_TROOPA = 'pk';
    CUSTOM_PEACH = 'ap';
    CUSTOM_PEACH_MENU = 'ap';
    CUSTOM_DIDDY_KONG = 'ad';
    CUSTOM_DONKEY_KONG = 'gd';
    CUSTOM_FUNKY_KONG = 'ck';
    CUSTOM_BABY_DAISY = 'rd';
    CUSTOM_DRY_BONES = 'bb';
    CUSTOM_DAISY = 'sd';
    CUSTOM_DAISY_MENU = 'sd';
    CUSTOM_BOWSER_JR = 'pj';
    CUSTOM_BOWSER = 'db';
    CUSTOM_DRY_BOWSER = 'gk';
}

void ResetOnlineCustomCharacterFlags() {
    for (int playerId = 0; playerId < 12; ++playerId) {
        onlineCustomCharacterFlags[playerId] = false;
    }
}

void RefreshLocalOnlineCustomCharacterFlags() {
    const RKNet::Controller* controller = RKNet::Controller::sInstance;
    if (!IsOnlineRoom(controller)) return;
    if (IsOnlineMultiLocal(controller)) return;
    if (!isDisplayCustomSkinsEnabled()) return;

    const Racedata* racedata = Racedata::sInstance;
    if (racedata == nullptr) return;

    const bool isEnabled = IsCustomCharacterSettingEnabled();
    const RacedataScenario& scenario = racedata->racesScenario;
    const u8 localPlayerCount = scenario.localPlayerCount > 4 ? 4 : scenario.localPlayerCount;

    for (u8 hudSlotId = 0; hudSlotId < localPlayerCount; ++hudSlotId) {
        const u32 playerId = racedata->GetPlayerIdOfLocalPlayer(hudSlotId);
        if (playerId < 12) {
            onlineCustomCharacterFlags[playerId] = isEnabled;
        }
    }
}

void UpdateOnlineCustomCharacterFlagsFromAid(u8 aid, const u8* playerIdToAid, u8 customCharacterFlags) {
    if (playerIdToAid == nullptr) return;

    u8 hudSlotId = 0;
    for (u8 playerId = 0; playerId < 12; ++playerId) {
        if (playerIdToAid[playerId] != aid) continue;

        onlineCustomCharacterFlags[playerId] = hudSlotId < 2 && ((customCharacterFlags >> hudSlotId) & 1) != 0;
        ++hudSlotId;
    }
}

u8 GetLocalOnlineCustomCharacterFlags() {
    return IsCustomCharacterSettingEnabled() ? 0x1 : 0x0;
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

static void RefreshOnlineCustomCharacterFlagsOnRaceLoad() {
    RefreshLocalOnlineCustomCharacterFlags();
}
static RaceLoadHook RefreshOnlineCustomCharacterFlagsHook(RefreshOnlineCustomCharacterFlagsOnRaceLoad);

void SetCharacter() {
    if (!IsOnlineRoom(RKNet::Controller::sInstance)) ResetOnlineCustomCharacterFlags();
    if (IsRaceSectionActive())
        ApplyDefaultCharacterTable();
    else if (IsCustomCharacterSettingEnabled())
        ApplyCustomCharacterTable();
    else
        ApplyDefaultCharacterTable();
}
static SectionLoadHook SetCharacterHook(SetCharacter);

}  // namespace CustomCharacters
}  // namespace Pulsar