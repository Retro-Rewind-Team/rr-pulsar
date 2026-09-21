#include <Network/FriendRoomCPUs.hpp>
#include <PulsarSystem.hpp>

#include <MarioKartWii/Item/ItemManager.hpp>
#include <MarioKartWii/Kart/KartManager.hpp>
#include <MarioKartWii/Kart/KartLink.hpp>
#include <MarioKartWii/GlobalFunctions.hpp>
#include <MarioKartWii/Race/RaceInfo/RaceInfo.hpp>
#include <MarioKartWii/System/Random.hpp>
#include <MarioKartWii/UI/Section/SectionMgr.hpp>
#include <MarioKartWii/RKNet/PacketMgr.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <core/egg/mem/Heap.hpp>
#include <core/rvl/OS/OS.hpp>
#include <runtimeWrite.hpp>

namespace Pulsar {
namespace Network {

static const u8 FRIEND_ROOM_CPU_MAGIC = 0xC7;
static const u32 FRIEND_ROOM_NATIVE_TIMEOUT_FRAMES = 1801;

kmRuntimeUse(0x8053e7ac);
kmRuntimeUse(0x8053e680);
kmRuntimeUse(0x8053e47c);
kmRuntimeUse(0x8053ec40);

struct FriendRoomCPUState {
    FriendRoomCPUItem items[12];
    bool itemValid[12];
    RKNet::RACEDATAPacket raceData[12];
    u32 raceSeq[12];
    bool raceValid[12];
    RKNet::RACEHEADER2Packet rh2Data[12];
    u32 rh2Seq[12];
    bool rh2Valid[12];

    bool cpu[12];
    u8 cpuOrder[FriendRoomCPUCountMax];
    u8 cpuCount;
    bool rosterReady;
    void *senders[12];
    void *flags[12];

    bool active;
    bool host;
    u8 humans;
    bool cpuTimeoutApplied;
    u16 nativeTimeoutFrames;
    bool nativeTimeoutActive;
    u16 hostFinishedMask;
    u16 hostDisconnectedMask;
    u8 hostFinishOrder[12];
    bool hostFinishOrderValid;
    u16 cpuFinishTimeSentMask;
    u16 loggedFinishedMask;
    u16 loggedDisconnectedMask;
    u16 loggedTimeoutMilestone;
    u8 loggedStage;
    u8 loggedActiveCount;
    u8 loggedFinishedCount;
    u8 loggedDisconnectedCount;
    u8 loggedManagerFinishedCount;
    bool terminalLogValid;
};

struct FriendRoomCPURoster {
    u32 seed;
    CharacterId characters[FriendRoomCPUCountMax];
    u8 kartIndices[FriendRoomCPUCountMax];
};

static FriendRoomCPUState s;
static FriendRoomCPURoster s_roster;

static const CharacterId FRIEND_ROOM_CPU_CHARACTERS[] = {
    MARIO,
    LUIGI,
    PEACH,
    DAISY,
    YOSHI,
    TOAD,
    TOADETTE,
    KOOPA_TROOPA,
    DRY_BONES,
    WARIO,
    WALUIGI,
    BOWSER,
    DONKEY_KONG,
    ROSALINA,
    FUNKY_KONG,
};

void SetFriendRoomCPUSeed(u32 seed) {
    if (seed == 0 || s_roster.seed == seed) return;

    CharacterId characters[sizeof(FRIEND_ROOM_CPU_CHARACTERS) / sizeof(FRIEND_ROOM_CPU_CHARACTERS[0])];
    memcpy(characters, FRIEND_ROOM_CPU_CHARACTERS, sizeof(characters));

    Random random(static_cast<s32>(seed));
    const u32 characterCount = sizeof(characters) / sizeof(characters[0]);
    for (u32 i = characterCount - 1; i > 0; --i) {
        const u32 other = static_cast<u32>(random.NextLimited(static_cast<int>(i + 1)));
        const CharacterId character = characters[i];
        characters[i] = characters[other];
        characters[other] = character;
    }

    s_roster.seed = seed;
    for (u8 i = 0; i < FriendRoomCPUCountMax; ++i) {
        s_roster.characters[i] = characters[i];
        s_roster.kartIndices[i] = static_cast<u8>(random.NextLimited(12));
    }
}

void StartFriendRoomCPURandomization() {
    Random random;
    const u32 seed = static_cast<u32>(random.Next());
    SetFriendRoomCPUSeed(seed != 0 ? seed : 1);
}

u32 GetFriendRoomCPUSeed() {
    return s_roster.seed;
}

bool IsFriendRoomCPUContextEnabled() {
    return System::sInstance && System::sInstance->IsContext(PULSAR_FROOM_CPUS) &&
           !System::sInstance->IsContext(PULSAR_EXTENDEDTEAMS) &&
           !System::sInstance->IsContext(PULSAR_MODE_KO) &&
           !System::sInstance->IsContext(PULSAR_VR);
}

static bool GetFriendRoomSession(bool *isHost) {
    if (isHost) *isHost = false;
    const RKNet::Controller *controller = RKNet::Controller::sInstance;
    if (!controller) {
        return false;
    }

    const RKNet::RoomType roomType = controller->roomType;
    const bool isFriendRoom = roomType == RKNet::ROOMTYPE_FROOM_HOST ||
                              roomType == RKNet::ROOMTYPE_FROOM_NONHOST;
    if (isFriendRoom) {
        if (!IsFriendRoomCPUContextEnabled()) {
            s.active = false;
            s.host = false;
            s.humans = 0;
            return false;
        }
        s.active = true;
        s.host = roomType == RKNet::ROOMTYPE_FROOM_HOST;
        if (s.humans == 0) {
            const u8 playerCount = controller->subs[controller->currentSub].playerCount;
            if (playerCount != 0 && playerCount <= 12) s.humans = playerCount;
        }
        if (isHost) *isHost = s.host;
        return true;
    }

    if (s.active && roomType == RKNet::ROOMTYPE_VS_REGIONAL) {
        if (isHost) *isHost = s.host;
        return true;
    }

    if (roomType != RKNet::ROOMTYPE_VS_REGIONAL) {
        s.active = false;
        s.host = false;
        s.humans = 0;
    }
    return false;
}

static bool IsFriendRoom() {
    return GetFriendRoomSession(nullptr);
}

static bool IsFriendRoomHost() {
    bool isHost = false;
    return GetFriendRoomSession(&isHost) && isHost;
}

static bool IsSyntheticFriendRoomSlotInSession(u8 playerId);

bool IsFriendRoomCPU(u8 playerId) {
    if (!GetFriendRoomSession(nullptr) || playerId >= 12) return false;
    return s.cpu[playerId] ||
           (!s.rosterReady && IsSyntheticFriendRoomSlotInSession(playerId));
}

static bool IsSyntheticFriendRoomSlotInSession(u8 playerId) {
    if (playerId >= 12) return false;
    if (s.cpu[playerId]) return true;
    if (s.rosterReady) return false;

    const RKNet::Controller *controller = RKNet::Controller::sInstance;
    const Racedata *racedata = Racedata::sInstance;
    if (!controller || !racedata) return false;

    const u32 humanCount = s.humans != 0
                               ? s.humans
                               : controller->subs[controller->currentSub].playerCount;
    if (humanCount == 0 || humanCount >= 12 || playerId < humanCount) return false;
    return racedata->racesScenario.players[playerId].GetPlayerType() != PLAYER_NONE;
}

static RKNet::RACEHEADER2Packet s_friendRoomCPUEmptyRH2;

static RKNet::RACEHEADER2Packet &GetFriendRoomRH2(RKNet::PacketMgr *packetMgr, u8 playerId) {
    if (playerId < 12 && GetFriendRoomSession(nullptr) && IsFriendRoomCPU(playerId)) {
        if (s.rh2Valid[playerId]) return s.rh2Data[playerId];
        memset(&s_friendRoomCPUEmptyRH2, 0, sizeof(s_friendRoomCPUEmptyRH2));
        return s_friendRoomCPUEmptyRH2;
    }
    return packetMgr->GetRH2(playerId);
}

kmCall(0x8053e4b8, GetFriendRoomRH2);

static u8 GetFriendRoomCPUCount() {
    if (!IsFriendRoom()) return 0;
    if (!s.rosterReady) {
        s.cpuCount = 0;
        for (u8 playerId = 0; playerId < 12 && s.cpuCount < FriendRoomCPUCountMax; ++playerId) {
            if (IsSyntheticFriendRoomSlotInSession(playerId)) {
                s.cpu[playerId] = true;
                s.cpuOrder[s.cpuCount++] = playerId;
            }
        }
        s.rosterReady = true;
    }
    return s.cpuCount;
}

static u8 GetFriendRoomCPUId(u8 index) {
    return s.cpuOrder[index];
}

static void PackFriendRoomFinishOrder(u8 *packed, const Raceinfo *raceinfo) {
    memset(packed, 0xff, 6);
    if (!raceinfo || !raceinfo->playerIdInEachPosition) return;

    u16 seen = 0;
    for (u8 position = 0; position < 12; ++position) {
        const u8 playerId = raceinfo->playerIdInEachPosition[position];
        if (playerId >= 12 || (seen & (1u << playerId)) != 0) return;
        seen |= static_cast<u16>(1u << playerId);
    }

    memset(packed, 0, 6);
    for (u8 position = 0; position < 12; ++position) {
        const u8 playerId = raceinfo->playerIdInEachPosition[position];
        const u8 shift = (position & 1) != 0 ? 4 : 0;
        packed[position >> 1] |= static_cast<u8>(playerId << shift);
    }
}

static bool UnpackFriendRoomFinishOrder(const u8 *packed, u8 *order) {
    u16 seen = 0;
    for (u8 position = 0; position < 12; ++position) {
        const u8 shift = (position & 1) != 0 ? 4 : 0;
        const u8 playerId = static_cast<u8>((packed[position >> 1] >> shift) & 0x0f);
        if (playerId >= 12 || (seen & (1u << playerId)) != 0) return false;
        seen |= static_cast<u16>(1u << playerId);
        order[position] = playerId;
    }
    return true;
}

static u8 GetHumanPlayerCount(const RacedataScenario &scenario) {
    if (IsFriendRoom()) {
        const RKNet::Controller *controller = RKNet::Controller::sInstance;
        if (controller != nullptr) {
            const u32 playerCount = controller->subs[controller->currentSub].playerCount;

            if (playerCount != 0 && playerCount <= 12) {
                s.humans = static_cast<u8>(playerCount);
                return s.humans;
            }
        }
        if (s.humans != 0 && s.humans <= 12)
            return s.humans;
    }

    u8 count = 0;
    for (u8 playerId = 0; playerId < 12; ++playerId) {
        const PlayerType type = scenario.players[playerId].GetPlayerType();
        if (type != PLAYER_NONE && type != PLAYER_CPU && playerId + 1 > count) count = playerId + 1;
    }
    return count;
}

enum CPUWeightClass {
    CPU_WEIGHT_LIGHT,
    CPU_WEIGHT_MEDIUM,
    CPU_WEIGHT_HEAVY,
};

static CPUWeightClass GetCPUWeightClass(CharacterId character) {
    switch (character) {
        case BABY_PEACH:
        case BABY_DAISY:
        case BABY_MARIO:
        case DRY_BONES:
        case BABY_LUIGI:
        case TOAD:
        case TOADETTE:
        case KOOPA_TROOPA:
            return CPU_WEIGHT_LIGHT;
        case MARIO:
        case LUIGI:
        case PEACH:
        case DAISY:
        case YOSHI:
        case BIRDO:
        case DIDDY_KONG:
        case BOWSER_JR:
            return CPU_WEIGHT_MEDIUM;
        default:
            return CPU_WEIGHT_HEAVY;
    }
}

static KartId GetCPUKart(u8 kartIndex, CharacterId character) {
    static const KartId karts[3][12] = {
        {STANDARD_KART_S, BABY_BOOSTER, MINI_BEAST, CHEEP_CHARGER, RALLY_ROMPER, BLUE_FALCON,
         STANDARD_BIKE_S, BULLET_BIKE, BIT_BIKE, QUACKER, MAGIKRUISER, JET_BUBBLE},
        {STANDARD_KART_M, CLASSIC_DRAGSTER, WILD_WING, SUPER_BLOOPER, ROYAL_RACER, SPRINTER,
         STANDARD_BIKE_M, MACH_BIKE, BON_BON, RAPIDE, NITROCYCLE, DOLPHIN_DASHER},
        {STANDARD_KART_L, OFFROADER, FLAME_FLYER, PIRANHA_PROWLER, JETSETTER, HONEYCOUPE,
         STANDARD_BIKE_L, BOWSER_BIKE, WARIO_BIKE, SHOOTING_STAR, SPEAR, PHANTOM},
    };
    return karts[GetCPUWeightClass(character)][kartIndex % 12];
}

static const wchar_t *GetFriendRoomCPUName(CharacterId character) {
    const SectionMgr *sectionMgr = SectionMgr::sInstance;
    if (!sectionMgr || !sectionMgr->systemBMG) return L"CPU";

    const s32 msgId = sectionMgr->systemBMG->GetMsgId(static_cast<s32>(GetCharacterBMGId(character, true)));
    if (msgId < 0) return L"CPU";

    wchar_t *name = sectionMgr->systemBMG->GetMsgByMsgId(msgId);
    return name ? name : L"CPU";
}

static bool SetFriendRoomMiiName(wchar_t *target, u32 targetLength,
                                 const wchar_t *source, u32 sourceLimit) {
    u32 sourceLength = 0;
    while (sourceLength < sourceLimit && source[sourceLength] != L'\0') ++sourceLength;

    bool changed = false;
    for (u32 i = 0; i < targetLength; ++i) {
        const wchar_t value = i < sourceLength ? source[i] : L'\0';
        if (target[i] != value) {
            target[i] = value;
            changed = true;
        }
    }
    return changed;
}

static void SetFriendRoomCPUName(Mii *mii, CharacterId character) {
    if (!mii) return;

    const wchar_t *name = GetFriendRoomCPUName(character);
    const bool infoChanged = SetFriendRoomMiiName(mii->info.name, 11, name, 10);
    const bool rawChanged = SetFriendRoomMiiName(mii->rawStoreMii.miiName, 10, name, 9);
    if (infoChanged || rawChanged)
        mii->rawStoreMii.crc16 = RFL::CalcCRC16(&mii->rawStoreMii, 0x4a);
}

static void SyncFriendRoomCPUScenarioNames(Racedata *racedata) {
    if (!racedata) return;

    for (u8 playerId = 0; playerId < 12; ++playerId) {
        if (!s.cpu[playerId]) continue;

        const CharacterId character = racedata->menusScenario.players[playerId].GetCharacterId();
        SetFriendRoomCPUName(racedata->menusScenario.players[playerId].GetMii(), character);
        SetFriendRoomCPUName(racedata->racesScenario.players[playerId].GetMii(), character);
    }
}

static void SyncFriendRoomCPUSectionNames(bool copyMissing) {
    if (!SectionMgr::sInstance || !SectionMgr::sInstance->sectionParams) return;

    MiiGroup &playerMiis = SectionMgr::sInstance->sectionParams->playerMiis;
    if (!playerMiis.mii || playerMiis.miiCount < 12) return;

    Mii *sourceMii = nullptr;
    u8 sourceId = 0xff;
    for (u8 playerId = 0; playerId < 12; ++playerId) {
        if (s.cpu[playerId]) continue;
        sourceMii = playerMiis.GetMii(playerId);
        if (sourceMii) {
            sourceId = playerId;
            break;
        }
    }

    for (u8 playerId = 0; playerId < 12; ++playerId) {
        if (!s.cpu[playerId]) continue;

        Mii *mii = playerMiis.GetMii(playerId);
        if (!mii && copyMissing && sourceMii) {
            playerMiis.CopyMii(sourceId, playerId);
            mii = playerMiis.GetMii(playerId);
        }
        if (mii) {
            const CharacterId character = Racedata::sInstance
                                              ? Racedata::sInstance->menusScenario.players[playerId].GetCharacterId()
                                              : MARIO;
            SetFriendRoomCPUName(mii, character);
        }
    }
}

Mii *GetFriendRoomCPUDisplayMii(u8 playerId) {
    if (playerId >= 12 || !Racedata::sInstance || !IsFriendRoomCPU(playerId)) return nullptr;
    return Racedata::sInstance->racesScenario.players[playerId].GetMii();
}

bool IsFriendRoomCPUTransportActive() {
    return IsFriendRoom();
}

static u8 PackItemFlags(u16 bitfield, u8 activeItemCount) {
    u8 flags = 0;
    if (bitfield & 0x0002) flags |= 0x01;
    if (bitfield & 0x0004) flags |= 0x02;
    if (bitfield & 0x0010) flags |= 0x04;
    if (bitfield & 0x0020) flags |= 0x08;
    if (bitfield & 0x0040) flags |= 0x10;
    if (bitfield & 0x0100) flags |= 0x20;
    if (activeItemCount > 3) activeItemCount = 3;
    if (activeItemCount != 0) flags |= static_cast<u8>(activeItemCount << 6);
    return flags;
}

static u8 GetPackedActiveItemCount(const FriendRoomCPUItem &item) {
    if (item.activeItem >= ITEM_NONE) return 0;
    const u8 count = static_cast<u8>((item.flags >> 6) & 0x03);
    return count == 0 ? 1 : count;
}

static void ApplyFriendRoomCPUItemPacket(u8 playerId, const FriendRoomCPUItem &item) {
    if (!RKNet::ITEMHandler::sInstance || playerId >= 12) return;

    RKNet::ITEMPacket &packet = RKNet::ITEMHandler::sInstance->receivedPackets[playerId];
    packet.storedItem = item.storedItem;
    packet.draggedItem = item.activeItem;
    packet.mode = item.storedItem == ITEM_NONE ? 0 : 7;
    packet.tailMode = GetPackedActiveItemCount(item);
}

static bool IsFriendRoomWiiPointer(const void *pointer) {
    const u32 address = reinterpret_cast<u32>(pointer);
    return address >= 0x80000000u && address < 0x94000000u;
}

static Kart::Player *GetFriendRoomKartPlayer(u8 playerId) {
    if (playerId >= 12 || !Kart::Manager::sInstance) return nullptr;

    Kart::Player *kart = Kart::Manager::sInstance->GetKartPlayer(playerId);

    if (IsFriendRoomWiiPointer(kart)) return kart;
    return nullptr;
}

static void RepairFriendRoomItemLink(Item::Player *item) {
    if (!item || !IsFriendRoomCPU(item->id)) return;

    Kart::Player *kart = GetFriendRoomKartPlayer(item->id);
    if (!kart) return;

    item->pointers = &kart->pointers;
    item->kartPlayer = kart;
    item->playerObj.itemPlayer = item;
    item->playerObj.playerId = item->id;
    item->playerObj.pointers = &kart->pointers;
}

static bool RepairFriendRoomPlayerObjLink(Item::PlayerObj *playerObj) {
    if (!playerObj) return false;

    Item::Player *item = reinterpret_cast<Item::Player *>(reinterpret_cast<u8 *>(playerObj) - 0xb4);
    if (item->id >= 12 || !IsFriendRoomCPU(item->id)) return true;

    Kart::Player *kart = GetFriendRoomKartPlayer(item->id);
    if (!kart) return false;

    playerObj->itemPlayer = item;
    playerObj->playerId = item->id;
    playerObj->pointers = &kart->pointers;
    return true;
}

kmRuntimeUse(0x80791910);
kmRuntimeUse(0x80795668);

static void UseFriendRoomItem(Item::PlayerObj *playerObj, bool isRemote) {
    if (!RepairFriendRoomPlayerObjLink(playerObj)) return;
    reinterpret_cast<void (*)(Item::PlayerObj *, bool)>(kmRuntimeAddr(0x80791910))(playerObj, isRemote);
}

kmCall(0x80797f94, UseFriendRoomItem);
kmCall(0x80795764, UseFriendRoomItem);

static void UpdateFriendRoomRemoteItem(Item::PlayerObj *playerObj) {
    if (!RepairFriendRoomPlayerObjLink(playerObj)) return;

    bool isHost = false;
    const bool friendRoom = GetFriendRoomSession(&isHost);
    if (playerObj && friendRoom && !isHost && playerObj->playerId < 12 &&
        IsSyntheticFriendRoomSlotInSession(playerObj->playerId)) {
        FriendRoomCPUItem item = {};
        item.storedItem = ITEM_NONE;
        item.activeItem = ITEM_NONE;
        const FriendRoomCPUItem *receivedItem = 0;
        if (s.itemValid[playerObj->playerId])
            receivedItem = &s.items[playerObj->playerId];
        if (receivedItem) item = *receivedItem;

        ApplyFriendRoomCPUItemPacket(playerObj->playerId, item);
    }

    reinterpret_cast<void (*)(Item::PlayerObj *)>(kmRuntimeAddr(0x80795668))(playerObj);
}

kmCall(0x80797f00, UpdateFriendRoomRemoteItem);

static const Mtx34 &GetFriendRoomPlayerObjMtx(Item::PlayerObj *playerObj) {
    RepairFriendRoomPlayerObjLink(playerObj);
    return playerObj->GetMtx();
}

kmCall(0x807955d8, GetFriendRoomPlayerObjMtx);

static void UpdateFriendRoomItem(Item::Player *item) {
    RepairFriendRoomItemLink(item);
    if (item) item->Update();
}

kmCall(0x8079994c, UpdateFriendRoomItem);

static bool UpdateFriendRoomPlayerObjParams(Item::PlayerObj *playerObj) {
    if (!RepairFriendRoomPlayerObjLink(playerObj)) return false;
    return playerObj->UpdateParams();
}

kmCall(0x80792330, UpdateFriendRoomPlayerObjParams);

bool ShouldSkipFriendRoomCPUItemDecision(Item::Player *item) {
    RepairFriendRoomItemLink(item);

    bool isHost = false;
    const bool friendRoom = GetFriendRoomSession(&isHost);
    return item && friendRoom && !isHost && IsSyntheticFriendRoomSlotInSession(item->id);
}

static void ResetFriendRoomCPURaceState() {
    const bool active = s.active;
    const bool host = s.host;
    const u8 humans = s.humans;
    memset(&s, 0, sizeof(s));
    s.active = active;
    s.host = host;
    s.humans = humans;
}

void PrepareFriendRoomCPUs(Racedata *racedata) {
    if (!racedata || !IsFriendRoom()) return;

    const GameMode mode = racedata->menusScenario.settings.gamemode;
    if (mode != MODE_PRIVATE_VS && mode != MODE_PUBLIC_VS) return;

    ResetFriendRoomCPURaceState();

    RacedataScenario &scenario = racedata->menusScenario;
    const u8 humanCount = GetHumanPlayerCount(scenario);
    if (humanCount == 0 || humanCount >= 12) return;

    Mii *sourceMii = scenario.players[0].GetMii();
    if (s_roster.seed == 0) SetFriendRoomCPUSeed(1);
    const bool isHost = IsFriendRoomHost();

    for (u8 playerId = humanCount; playerId < 12; ++playerId) {
        RacedataPlayer &player = scenario.players[playerId];
        const u8 cpuIndex = playerId - humanCount;
        const CharacterId character = s_roster.characters[cpuIndex];
        const u8 kartIndex = s_roster.kartIndices[cpuIndex];
        s.cpu[playerId] = true;

        player.SetPlayerType(isHost ? PLAYER_CPU : PLAYER_REAL_ONLINE);
        player.SetCharacterId(character);
        player.SetKartId(GetCPUKart(kartIndex, character));
        player.hudSlotId = -1;
        player.realControllerChannel = -1;
        player.team = TEAM_NONE;
        if (sourceMii) player.SetMii(sourceMii);
        SetFriendRoomCPUName(player.GetMii(), character);
    }

    SyncFriendRoomCPUScenarioNames(racedata);
    SyncFriendRoomCPUSectionNames(false);
}

static void RefreshFriendRoomCPUAidMappings() {
    bool isHost = false;
    if (!GetFriendRoomSession(&isHost)) return;

    RKNet::Controller *controller = RKNet::Controller::sInstance;
    if (!controller) return;
    const RKNet::ControllerSub &sub = controller->subs[controller->currentSub];
    const u8 hostAid = isHost ? sub.localAid : sub.hostAid;

    const u8 cpuCount = GetFriendRoomCPUCount();
    for (u8 i = 0; i < cpuCount; ++i)
        controller->aidsBelongingToPlayerIds[GetFriendRoomCPUId(i)] = hostAid;
}

void FinalizeFriendRoomCPUs() {
    RefreshFriendRoomCPUAidMappings();
    if (!IsFriendRoom()) return;

    SyncFriendRoomCPUScenarioNames(Racedata::sInstance);
    SyncFriendRoomCPUSectionNames(false);
}

static bool IsFriendRoomOnlineVS() {
    const Racedata *racedata = Racedata::sInstance;
    if (!racedata) return false;
    const GameMode mode = racedata->racesScenario.settings.gamemode;
    return mode == MODE_PRIVATE_VS || mode == MODE_PUBLIC_VS;
}

typedef void (*FriendRoomRH2ProcessFn)(GMDataOnlineVS *, u32);

static void ProcessFriendRoomCPUHeader(GMDataOnlineVS *mode, u8 playerId) {
    if (!mode || playerId >= 12 || !s.rh2Valid[playerId]) return;
    reinterpret_cast<FriendRoomRH2ProcessFn>(kmRuntimeAddr(0x8053e47c))(mode, playerId);
}

static u16 GetFriendRoomTimeoutLogMilestone(u16 frames) {
    if (frames >= FRIEND_ROOM_NATIVE_TIMEOUT_FRAMES) return FRIEND_ROOM_NATIVE_TIMEOUT_FRAMES;
    if (frames >= 1800) return 1800;
    if (frames >= 900) return 900;
    if (frames >= 300) return 300;
    if (frames >= 60) return 60;
    return frames == 0 ? 0 : 1;
}

static bool IsFriendRoomFinishTimerValid(const Timer *timer) {
    return timer && timer->isActive &&
           (timer->minutes != 0 || timer->seconds != 0 || timer->milliseconds != 0);
}

static void LogFriendRoomTerminalState(const char *source, const Raceinfo *raceinfo) {
    if (!raceinfo || !raceinfo->players) return;

    u8 activeCount = 0;
    u8 finishedCount = 0;
    u8 disconnectedCount = 0;
    for (u8 playerId = 0; playerId < 12; ++playerId) {
        const RaceinfoPlayer *player = raceinfo->players[playerId];
        if (!player) continue;

        const u32 flags = player->stateFlags;
        if (flags & 0x02) ++finishedCount;
        if (flags & 0x10) ++disconnectedCount;
        if ((flags & (0x02 | 0x10 | 0x20)) == 0) ++activeCount;
    }

    const u16 timeoutMilestone = GetFriendRoomTimeoutLogMilestone(s.nativeTimeoutFrames);
    const u8 stage = static_cast<u8>(raceinfo->stage);
    const bool changed = !s.terminalLogValid ||
                         s.loggedFinishedMask != s.hostFinishedMask ||
                         s.loggedDisconnectedMask != s.hostDisconnectedMask ||
                         s.loggedTimeoutMilestone != timeoutMilestone ||
                         s.loggedStage != stage ||
                         s.loggedActiveCount != activeCount ||
                         s.loggedFinishedCount != finishedCount ||
                         s.loggedDisconnectedCount != disconnectedCount ||
                         s.loggedManagerFinishedCount != raceinfo->finishedPlayerCount;
    if (!changed) return;

    OS::Report("[PULSAR] friend-terminal source=%s frame=%u stage=%u hostFinish=0x%04x hostDisconnect=0x%04x timeout=%u active=%u finished=%u disconnected=%u managerFinished=%u\n",
               source, raceinfo->raceFrames, stage, s.hostFinishedMask,
               s.hostDisconnectedMask, s.nativeTimeoutFrames, activeCount,
               finishedCount, disconnectedCount, raceinfo->finishedPlayerCount);
    s.loggedFinishedMask = s.hostFinishedMask;
    s.loggedDisconnectedMask = s.hostDisconnectedMask;
    s.loggedTimeoutMilestone = timeoutMilestone;
    s.loggedStage = stage;
    s.loggedActiveCount = activeCount;
    s.loggedFinishedCount = finishedCount;
    s.loggedDisconnectedCount = disconnectedCount;
    s.loggedManagerFinishedCount = raceinfo->finishedPlayerCount;
    s.terminalLogValid = true;
}

static void ApplyReceivedFriendRoomTerminalState() {
    bool isHost = false;
    if (!GetFriendRoomSession(&isHost) || isHost || !IsFriendRoomOnlineVS() ||
        (s.hostFinishedMask == 0 && s.hostDisconnectedMask == 0))
        return;

    Raceinfo *raceinfo = Raceinfo::sInstance;
    if (!raceinfo || !raceinfo->players || raceinfo->stage < RACESTAGE_RACE ||
        raceinfo->stage >= RACESTAGE_FINISHED)
        return;

    const u16 disconnectedMask = s.hostDisconnectedMask;
    const u16 finishedMask = s.hostFinishedMask & ~disconnectedMask;
    for (u8 playerId = 0; playerId < 12; ++playerId) {
        RaceinfoPlayer *player = raceinfo->players[playerId];
        if (!player) continue;

        const u16 bit = static_cast<u16>(1u << playerId);
        if (disconnectedMask & bit) {
            if ((player->stateFlags & 0x10) == 0) {
                raceinfo->SetPlayerDisconnected(playerId);
                raceinfo->CheckEndRaceOnline(playerId);
            }
            continue;
        }

        if ((finishedMask & bit) == 0 ||
            (player->stateFlags & (0x02 | 0x10)) != 0)
            continue;

        const Timer *finishTime = player->raceFinishTime;
        if (!IsFriendRoomFinishTimerValid(finishTime) && raceinfo->gamemodeData) {
            GMDataOnlineVS *mode = static_cast<GMDataOnlineVS *>(raceinfo->gamemodeData);
            if (IsFriendRoomFinishTimerValid(&mode->players[playerId].raceFinishTime))
                finishTime = &mode->players[playerId].raceFinishTime;
        }
        if (IsFriendRoomFinishTimerValid(finishTime))
            player->EndRace(*finishTime, false, 5);
    }
}

typedef void (*FriendRoomNativeRH2FinishFn)(GMDataOnlineVS *);

static void ApplyFriendRoomNativeRH2Finish(GMDataOnlineVS *mode) {
    reinterpret_cast<FriendRoomNativeRH2FinishFn>(kmRuntimeAddr(0x8053e680))(mode);
    ApplyReceivedFriendRoomTerminalState();
    bool isHost = false;
    if (GetFriendRoomSession(&isHost) && !isHost)
        LogFriendRoomTerminalState("native-rh2", Raceinfo::sInstance);
}

kmCall(0x8053f2f4, ApplyFriendRoomNativeRH2Finish);

static void ApplyReceivedFriendRoomNativeTimeout() {
    bool isHost = false;
    if (!s.nativeTimeoutActive || !GetFriendRoomSession(&isHost) || isHost ||
        !IsFriendRoomOnlineVS())
        return;

    Raceinfo *raceinfo = Raceinfo::sInstance;
    if (!raceinfo || raceinfo->stage < RACESTAGE_RACE ||
        raceinfo->stage >= RACESTAGE_FINISHED)
        return;

    if (raceinfo->gamemodeData) {
        GMDataOnlineVS *mode = static_cast<GMDataOnlineVS *>(raceinfo->gamemodeData);
        if (mode->rh2Packet.timeElapsedFirstFinished < s.nativeTimeoutFrames)
            mode->rh2Packet.timeElapsedFirstFinished = s.nativeTimeoutFrames;
    }

    if (raceinfo->finishedPlayerCount == 0) raceinfo->finishedPlayerCount = 1;
}

typedef bool (*FriendRoomNativeTimeoutFn)(GMDataOnlineVS *);

static void StopFriendRoomCPUsAtNativeTimeout() {
    if (s.cpuTimeoutApplied || !GetFriendRoomSession(nullptr) ||
        !IsFriendRoomOnlineVS())
        return;

    Raceinfo *raceinfo = Raceinfo::sInstance;
    if (!raceinfo || !raceinfo->players || raceinfo->stage < RACESTAGE_RACE ||
        raceinfo->stage >= RACESTAGE_FINISHED)
        return;

    s.cpuTimeoutApplied = true;
    const u8 cpuCount = GetFriendRoomCPUCount();
    for (u8 cpuIndex = 0; cpuIndex < cpuCount; ++cpuIndex) {
        const u8 playerId = GetFriendRoomCPUId(cpuIndex);
        RaceinfoPlayer *player = raceinfo->players[playerId];
        if (!player || (player->stateFlags & (0x02 | 0x10 | 0x20)) != 0) continue;

        raceinfo->SetPlayerDisconnected(playerId);
        raceinfo->CheckEndRaceOnline(playerId);
    }
}

static bool UpdateFriendRoomNativeTimeout(GMDataOnlineVS *mode) {
    ApplyReceivedFriendRoomTerminalState();
    ApplyReceivedFriendRoomNativeTimeout();
    const bool timedOut = reinterpret_cast<FriendRoomNativeTimeoutFn>(
        kmRuntimeAddr(0x8053ec40))(mode);
    if (timedOut ||
        (!IsFriendRoomHost() && s.nativeTimeoutActive &&
         s.nativeTimeoutFrames >= FRIEND_ROOM_NATIVE_TIMEOUT_FRAMES)) {
        StopFriendRoomCPUsAtNativeTimeout();
    }
    ApplyReceivedFriendRoomTerminalState();
    bool isHost = false;
    if (GetFriendRoomSession(&isHost) && !isHost)
        LogFriendRoomTerminalState("native-timeout", Raceinfo::sInstance);
    return timedOut;
}

kmCall(0x8053f39c, UpdateFriendRoomNativeTimeout);

typedef void *(*RacedataFactoryFlagsConstructFn)(void *flags);
typedef void *(*RacedataFactoryConstructFn)(void *sender, void *flags);
typedef void (*RacedataFactoryPackFn)(void *sender);

kmRuntimeUse(0x8058d3cc);
kmRuntimeUse(0x8058ca28);
kmRuntimeUse(0x8058cb30);

static void BindHostRacedataFactory(u8 playerId, Kart::Player *kart) {
    if (!kart || playerId >= 12) return;

    void *flags = s.flags[playerId];
    void *sender = s.senders[playerId];
    if (!flags || !sender) return;

    *reinterpret_cast<void **>(reinterpret_cast<u8 *>(sender) + 0x10) = flags;

    *reinterpret_cast<Kart::Pointers **>(flags) = &kart->pointers;
    *reinterpret_cast<Kart::Pointers **>(sender) = &kart->pointers;

    *reinterpret_cast<void **>(reinterpret_cast<u8 *>(&kart->pointers) + 0x3c) = sender;
}

static void CreateHostRacedataFactory(u8 playerId, Kart::Player *kart) {
    if (!kart || !IsFriendRoomHost() || !IsFriendRoomCPU(playerId)) return;

    void *&flags = s.flags[playerId];
    void *&sender = s.senders[playerId];
    if (flags || sender) {
        BindHostRacedataFactory(playerId, kart);
        return;
    }

    flags = EGG::Heap::alloc(0x28, 0x20);
    if (!flags) return;
    memset(flags, 0, 0x28);
    reinterpret_cast<RacedataFactoryFlagsConstructFn>(kmRuntimeAddr(0x8058d3cc))(flags);

    sender = EGG::Heap::alloc(0x5c, 0x20);
    if (!sender) {
        EGG::Heap::free(flags, nullptr);
        flags = nullptr;
        return;
    }
    memset(sender, 0, 0x5c);
    reinterpret_cast<RacedataFactoryConstructFn>(kmRuntimeAddr(0x8058ca28))(sender, flags);
    BindHostRacedataFactory(playerId, kart);
}

static void CreateFriendRoomKartModel(Kart::Player *player) {
    if (player != nullptr && player->values != nullptr)
        CreateHostRacedataFactory(player->values->playerIdx, player);

    player->CreateModel();
}

kmCall(0x8058fd80, CreateFriendRoomKartModel);

static RKNet::RACEDATAPacket s_emptyFriendRoomRaceData;

static bool IsFriendRoomRemoteCPU(u8 playerId) {
    bool isHost = false;
    return playerId < 12 && GetFriendRoomSession(&isHost) && !isHost &&
           IsSyntheticFriendRoomSlotInSession(playerId);
}

static RKNet::RACEDATAPacket &GetFriendRoomRACEDATA(RKNet::PacketMgr *packetMgr, u8 playerId) {
    if (IsFriendRoomRemoteCPU(playerId)) {
        if (s.raceValid[playerId]) return s.raceData[playerId];
        memset(&s_emptyFriendRoomRaceData, 0, sizeof(s_emptyFriendRoomRaceData));
        return s_emptyFriendRoomRaceData;
    }
    return packetMgr->GetRACEDATA(playerId);
}

static u32 GetFriendRoomPlayerRH1Timer(const RKNet::PacketMgr *packetMgr, u32 playerId) {
    if (playerId < 12 && IsFriendRoomRemoteCPU(static_cast<u8>(playerId))) {
        return s.raceValid[playerId] ? s.raceSeq[playerId] : 0;
    }
    return packetMgr->GetPlayerRH1Timer(playerId);
}

kmCall(0x8058a1d0, GetFriendRoomRACEDATA);
kmCall(0x80589ab4, GetFriendRoomPlayerRH1Timer);
kmCall(0x80589ad8, GetFriendRoomPlayerRH1Timer);

typedef int (*FriendRoomRH2PackFn)(GMDataOnlineVS *);

static bool PackFriendRoomCPUHeader(GMDataOnlineVS *mode, u8 playerId,
                                    RKNet::RACEHEADER2Packet *output) {
    if (!mode || !output || playerId >= 12) return false;

    u8 savedState[0x7c];
    u8 savedPlayer[sizeof(GMDataOnlineVSPlayer)];
    memcpy(savedState, &mode->rh2Packet, sizeof(savedState));
    memcpy(savedPlayer, &mode->players[playerId], sizeof(savedPlayer));

    Raceinfo *raceinfo = Raceinfo::sInstance;
    RaceinfoPlayer *racePlayer = 0;
    if (raceinfo && raceinfo->players)
        racePlayer = raceinfo->players[playerId];
    const u32 stateFlags = racePlayer ? racePlayer->stateFlags : 0;
    const bool finished = (stateFlags & 0x02) != 0 && racePlayer &&
                          IsFriendRoomFinishTimerValid(racePlayer->raceFinishTime);

    if (finished)
        mode->players[playerId].raceFinishTime = *racePlayer->raceFinishTime;
    else
        mode->players[playerId].raceFinishTime.isActive = false;
    mode->players[playerId].receivedFinishMask =
        finished ? static_cast<u16>(1u << playerId) : 0;
    mode->rh2Packet.player1Id = playerId;
    mode->rh2Packet.player2Id = 0xff;

    reinterpret_cast<FriendRoomRH2PackFn>(kmRuntimeAddr(0x8053e7ac))(mode);
    memcpy(output, &mode->rh2Packet, sizeof(*output));
    memcpy(&mode->players[playerId], savedPlayer, sizeof(savedPlayer));
    memcpy(&mode->rh2Packet, savedState, sizeof(savedState));
    return true;
}

bool WriteFriendRoomCPUState(PulRH1 *packet) {
    bool isHost = false;
    if (!packet || !GetFriendRoomSession(&isHost) || !isHost) return false;

    const u8 cpuCount = GetFriendRoomCPUCount();
    if (cpuCount == 0) return false;

    FriendRoomCPUSyncPacket *sync = reinterpret_cast<FriendRoomCPUSyncPacket *>(reinterpret_cast<u8 *>(packet) + PulRH1SizeFull);
    memset(sync, 0, sizeof(*sync));
    sync->magic = FRIEND_ROOM_CPU_MAGIC;
    sync->cpuCount = cpuCount;
    sync->raceDataPlayerId = 0xff;
    sync->rh2PlayerId = 0xff;
    sync->nativeTimeoutFrames = 0;
    sync->nativeTimeoutActive = 0;
    sync->hostFinishedMask = 0;
    sync->hostDisconnectedMask = 0;

    const Raceinfo *raceinfo = Raceinfo::sInstance;
    const u32 sequence = raceinfo ? raceinfo->raceFrames : 0;
    sync->raceDataSequence = sequence;
    PackFriendRoomFinishOrder(sync->finishOrder, raceinfo);

    if (raceinfo && raceinfo->players) {
        for (u8 playerId = 0; playerId < 12; ++playerId) {
            const RaceinfoPlayer *player = raceinfo->players[playerId];
            if (!player) continue;

            const u16 bit = static_cast<u16>(1u << playerId);
            if (player->stateFlags & 0x02) sync->hostFinishedMask |= bit;
            if (player->stateFlags & 0x10)
                sync->hostDisconnectedMask |= bit;
        }
    }
    const u16 actualFinishedMask = sync->hostFinishedMask;
    // A remote racer cannot finish until its RH2 timer is valid. Do not advertise a
    // CPU as finished before at least one packet has carried that CPU's final timer.
    for (u8 cpuIndex = 0; cpuIndex < cpuCount; ++cpuIndex) {
        const u16 bit = static_cast<u16>(1u << GetFriendRoomCPUId(cpuIndex));
        if ((actualFinishedMask & bit) != 0 && (s.cpuFinishTimeSentMask & bit) == 0)
            sync->hostFinishedMask &= ~bit;
    }

    if (raceinfo && IsFriendRoomOnlineVS() && raceinfo->gamemodeData) {
        const GMDataOnlineVS *mode = static_cast<const GMDataOnlineVS *>(raceinfo->gamemodeData);
        const u32 nativeTimeoutFrames = mode->rh2Packet.timeElapsedFirstFinished;

        const u32 nextNativeTimeoutFrames =
            nativeTimeoutFrames == 0
                ? 0
                : (nativeTimeoutFrames < FRIEND_ROOM_NATIVE_TIMEOUT_FRAMES
                       ? nativeTimeoutFrames + 1
                       : FRIEND_ROOM_NATIVE_TIMEOUT_FRAMES);
        sync->nativeTimeoutFrames = static_cast<u16>(nextNativeTimeoutFrames);
        sync->nativeTimeoutActive = sync->nativeTimeoutFrames != 0;
    }

    RKNet::Controller *controller = RKNet::Controller::sInstance;
    if (controller) {
        const RKNet::ControllerSub &sub = controller->subs[controller->currentSub];
        for (u8 cpuIndex = 0; cpuIndex < cpuCount; ++cpuIndex)
            packet->aidsBelongingToPlayerIds[GetFriendRoomCPUId(cpuIndex)] = sub.localAid;
    }

    Item::Manager *itemManager = Item::Manager::sInstance;
    Kart::Manager *kartManager = Kart::Manager::sInstance;
    for (u8 cpuIndex = 0; cpuIndex < cpuCount; ++cpuIndex) {
        const u8 playerId = GetFriendRoomCPUId(cpuIndex);
        FriendRoomCPUItem &syncItem = sync->items[cpuIndex];
        syncItem.storedItem = static_cast<u8>(ITEM_NONE);
        syncItem.storedItemCount = 0;
        syncItem.activeItem = static_cast<u8>(ITEM_NONE);
        syncItem.flags = 0;

        if (itemManager && itemManager->players && playerId < itemManager->playerCount) {
            const Item::Player &item = itemManager->players[playerId];
            syncItem.storedItem = static_cast<u8>(item.inventory.currentItemId);
            syncItem.storedItemCount = static_cast<u8>(item.inventory.currentItemCount);
            const bool isDragged = item.playerObj.activeItemCount != 0 &&
                                   item.playerObj.itemObjId != OBJ_NONE &&
                                   !item.playerObj.isNotDragged &&
                                   item.playerObj.itemId < ITEM_NONE;
            syncItem.activeItem = isDragged
                                      ? static_cast<u8>(item.playerObj.itemId)
                                      : static_cast<u8>(ITEM_NONE);
            syncItem.flags = PackItemFlags(
                item.bitfield, isDragged ? static_cast<u8>(item.playerObj.activeItemCount) : 0);
        }
    }

    const u8 raceDataIndex = static_cast<u8>(sequence % cpuCount);
    const u8 raceDataPlayerId = GetFriendRoomCPUId(raceDataIndex);
    if (kartManager) {
        Kart::Player *kart = kartManager->GetKartPlayer(raceDataPlayerId);
        BindHostRacedataFactory(raceDataPlayerId, kart);
        void *sender = s.senders[raceDataPlayerId];
        if (sender) {
            reinterpret_cast<RacedataFactoryPackFn>(kmRuntimeAddr(0x8058cb30))(sender);
            sync->raceDataPlayerId = raceDataPlayerId;
            memcpy(&sync->raceData, reinterpret_cast<u8 *>(sender) + 0x14, sizeof(sync->raceData));
        }
    }

    if (raceinfo && IsFriendRoomOnlineVS() && raceinfo->gamemodeData) {
        RKNet::RACEHEADER2Packet rh2 = {};
        GMDataOnlineVS *mode = reinterpret_cast<GMDataOnlineVS *>(raceinfo->gamemodeData);
        u8 rh2PlayerId = raceDataPlayerId;
        // Keep finished CPU timers in the round-robin so packet loss near the end of
        // a race cannot leave a non-host with an empty result time.
        for (u8 offset = 0; offset < cpuCount; ++offset) {
            const u8 cpuIndex = static_cast<u8>((raceDataIndex + offset) % cpuCount);
            const u8 playerId = GetFriendRoomCPUId(cpuIndex);
            const u16 bit = static_cast<u16>(1u << playerId);
            const RaceinfoPlayer *player = raceinfo->players ? raceinfo->players[playerId] : 0;
            if ((actualFinishedMask & bit) != 0 && player &&
                IsFriendRoomFinishTimerValid(player->raceFinishTime)) {
                rh2PlayerId = playerId;
                break;
            }
        }

        if (PackFriendRoomCPUHeader(mode, rh2PlayerId, &rh2)) {
            sync->rh2PlayerId = rh2PlayerId;
            memcpy(&sync->rh2Data, &rh2, sizeof(rh2));
            memcpy(&s.rh2Data[rh2PlayerId], &rh2, sizeof(rh2));
            s.rh2Seq[rh2PlayerId] = sequence;
            s.rh2Valid[rh2PlayerId] = true;

            const u16 bit = static_cast<u16>(1u << rh2PlayerId);
            const RaceinfoPlayer *player = raceinfo->players ? raceinfo->players[rh2PlayerId] : 0;
            if ((actualFinishedMask & bit) != 0 && player &&
                IsFriendRoomFinishTimerValid(player->raceFinishTime)) {
                s.cpuFinishTimeSentMask |= bit;
                sync->hostFinishedMask |= bit;
            }
        }
    }
    return true;
}

void ReadFriendRoomCPUState(const PulRH1 *packet, u32 packetSize, u8 senderAid) {
    bool isHost = false;
    if (!packet || !GetFriendRoomSession(&isHost) || isHost ||
        packetSize < sizeof(PulRH1)) return;

    RKNet::Controller *controller = RKNet::Controller::sInstance;
    if (!controller) return;
    const RKNet::ControllerSub &sub = controller->subs[controller->currentSub];
    if (senderAid != sub.hostAid) return;

    const FriendRoomCPUSyncPacket *sync = reinterpret_cast<const FriendRoomCPUSyncPacket *>(reinterpret_cast<const u8 *>(packet) + PulRH1SizeFull);
    if (sync->magic != FRIEND_ROOM_CPU_MAGIC || sync->cpuCount > FriendRoomCPUCountMax) return;

    u8 finishOrder[12];
    if (UnpackFriendRoomFinishOrder(sync->finishOrder, finishOrder)) {
        memcpy(s.hostFinishOrder, finishOrder, sizeof(s.hostFinishOrder));
        s.hostFinishOrderValid = true;
    }

    memset(s.itemValid, 0, sizeof(s.itemValid));
    const u8 localCPUCount = GetFriendRoomCPUCount();
    const u8 itemCount = sync->cpuCount < localCPUCount ? sync->cpuCount : localCPUCount;
    for (u8 i = 0; i < itemCount; ++i) {
        const u8 playerId = GetFriendRoomCPUId(i);
        s.items[playerId] = sync->items[i];
        s.itemValid[playerId] = true;
        ApplyFriendRoomCPUItemPacket(playerId, sync->items[i]);
        controller->aidsBelongingToPlayerIds[playerId] = senderAid;
    }

    const u8 raceDataPlayerId = sync->raceDataPlayerId;
    if (raceDataPlayerId < 12 && IsFriendRoomCPU(raceDataPlayerId) &&
        (!s.raceValid[raceDataPlayerId] ||
         sync->raceDataSequence != s.raceSeq[raceDataPlayerId])) {
        memcpy(&s.raceData[raceDataPlayerId], &sync->raceData, sizeof(sync->raceData));
        s.raceSeq[raceDataPlayerId] = sync->raceDataSequence;
        s.raceValid[raceDataPlayerId] = true;
    }

    const u8 rh2PlayerId = sync->rh2PlayerId;
    if (rh2PlayerId < 12 && IsFriendRoomCPU(rh2PlayerId) &&
        (!s.rh2Valid[rh2PlayerId] ||
         sync->raceDataSequence != s.rh2Seq[rh2PlayerId])) {
        memcpy(&s.rh2Data[rh2PlayerId], &sync->rh2Data, sizeof(sync->rh2Data));
        s.rh2Seq[rh2PlayerId] = sync->raceDataSequence;
        s.rh2Valid[rh2PlayerId] = true;

        Raceinfo *raceinfo = Raceinfo::sInstance;
        if (raceinfo && IsFriendRoomOnlineVS() && raceinfo->gamemodeData)
            ProcessFriendRoomCPUHeader(static_cast<GMDataOnlineVS *>(raceinfo->gamemodeData),
                                       rh2PlayerId);
    }

    if (sync->nativeTimeoutActive) {
        if (!s.nativeTimeoutActive ||
            sync->nativeTimeoutFrames > s.nativeTimeoutFrames)
            s.nativeTimeoutFrames = sync->nativeTimeoutFrames;
        s.nativeTimeoutActive = true;
    }

    s.hostFinishedMask |= sync->hostFinishedMask;
    s.hostDisconnectedMask |= sync->hostDisconnectedMask;
    LogFriendRoomTerminalState("rh1-recv", Raceinfo::sInstance);

    ApplyReceivedFriendRoomTerminalState();
    ApplyReceivedFriendRoomNativeTimeout();
    if (s.nativeTimeoutActive &&
        s.nativeTimeoutFrames >= FRIEND_ROOM_NATIVE_TIMEOUT_FRAMES)
        StopFriendRoomCPUsAtNativeTimeout();
}

void ApplyFriendRoomCPUResultOrder() {
    bool isHost = false;
    if (!s.hostFinishOrderValid || !GetFriendRoomSession(&isHost) || isHost) return;

    Raceinfo *raceinfo = Raceinfo::sInstance;
    Racedata *racedata = Racedata::sInstance;
    if (!raceinfo || !raceinfo->players || !raceinfo->playerIdInEachPosition || !racedata) return;

    const u8 playerCount = racedata->racesScenario.playerCount;
    if (playerCount == 0 || playerCount > 12) return;

    if (raceinfo->gamemodeData && IsFriendRoomOnlineVS()) {
        GMDataOnlineVS *mode = static_cast<GMDataOnlineVS *>(raceinfo->gamemodeData);
        const u8 cpuCount = GetFriendRoomCPUCount();
        for (u8 cpuIndex = 0; cpuIndex < cpuCount; ++cpuIndex) {
            const u8 playerId = GetFriendRoomCPUId(cpuIndex);
            const u16 bit = static_cast<u16>(1u << playerId);
            if ((s.hostFinishedMask & bit) == 0 || (s.hostDisconnectedMask & bit) != 0)
                continue;

            if (!IsFriendRoomFinishTimerValid(&mode->players[playerId].raceFinishTime))
                ProcessFriendRoomCPUHeader(mode, playerId);
            if (IsFriendRoomFinishTimerValid(&mode->players[playerId].raceFinishTime) &&
                raceinfo->players[playerId]->raceFinishTime)
                *raceinfo->players[playerId]->raceFinishTime =
                    mode->players[playerId].raceFinishTime;
        }
    }

    u16 seen = 0;
    for (u8 position = 0; position < playerCount; ++position) {
        const u8 playerId = s.hostFinishOrder[position];
        if (playerId >= playerCount || !raceinfo->players[playerId] ||
            (seen & (1u << playerId)) != 0)
            return;
        seen |= static_cast<u16>(1u << playerId);
    }

    for (u8 position = 0; position < playerCount; ++position) {
        const u8 playerId = s.hostFinishOrder[position];
        const u8 rank = static_cast<u8>(position + 1);
        raceinfo->playerIdInEachPosition[position] = playerId;
        raceinfo->players[playerId]->position = rank;
        racedata->racesScenario.players[playerId].finishPos = rank;
        racedata->menusScenario.players[playerId].finishPos = rank;
    }
}

static void ApplyReceivedCPUItems() {
    bool isHost = false;
    if (!GetFriendRoomSession(&isHost) || isHost) return;

    Item::Manager *itemManager = Item::Manager::sInstance;
    const u8 cpuCount = GetFriendRoomCPUCount();
    for (u8 i = 0; i < cpuCount; ++i) {
        const u8 playerId = GetFriendRoomCPUId(i);
        if (!s.itemValid[playerId]) continue;

        if (itemManager && itemManager->players && playerId < itemManager->playerCount) {
            Item::Player &item = itemManager->players[playerId];
            const FriendRoomCPUItem &received = s.items[playerId];
            const ItemId storedItem = static_cast<ItemId>(received.storedItem);
            if (item.inventory.currentItemId != storedItem)
                item.inventory.currentItemId = storedItem;
            if (item.inventory.currentItemCount != received.storedItemCount)
                item.inventory.currentItemCount = received.storedItemCount;
        }
    }
}

static void EndFriendRoomCPUsWhenHumansFinished() {
    if (!IsFriendRoomOnlineVS() || s.humans == 0) return;

    Raceinfo *raceinfo = Raceinfo::sInstance;
    if (!raceinfo || !raceinfo->players || raceinfo->stage < RACESTAGE_RACE ||
        raceinfo->stage >= RACESTAGE_FINISHED)
        return;

    for (u8 playerId = 0; playerId < s.humans; ++playerId) {
        const RaceinfoPlayer *player = raceinfo->players[playerId];
        if (!player || (player->stateFlags & (0x02 | 0x10)) == 0) return;
    }

    const u8 cpuCount = GetFriendRoomCPUCount();
    for (u8 cpuIndex = 0; cpuIndex < cpuCount; ++cpuIndex) {
        const u8 playerId = GetFriendRoomCPUId(cpuIndex);
        RaceinfoPlayer *player = raceinfo->players[playerId];
        if (!player || (player->stateFlags & (0x02 | 0x10)) != 0) continue;

        raceinfo->SetPlayerDisconnected(playerId);
        raceinfo->CheckEndRaceOnline(playerId);
    }
}

static void RefreshReceivedFriendRoomCPUState() {
    if (!IsFriendRoom() || IsFriendRoomHost()) return;

    RKNet::Controller *controller = RKNet::Controller::sInstance;
    if (!controller) return;
    const RKNet::ControllerSub &sub = controller->subs[controller->currentSub];
    const u8 hostAid = sub.hostAid;
    if (hostAid >= 12) return;
    const u8 bufferIdx = controller->lastReceivedBufferUsed[hostAid][RKNet::PACKET_RACEHEADER1];
    RKNet::SplitRACEPointers *split = controller->splitReceivedRACEPackets[bufferIdx][hostAid];
    if (!split) return;

    const RKNet::PacketHolder<PulRH1> *holder = split->GetPacketHolder<PulRH1>();
    if (!holder) return;
    ReadFriendRoomCPUState(holder->packet, holder->packetSize, hostAid);
}

void UpdateFriendRoomCPUs(AI::Manager *manager) {
    bool isHost = false;
    const bool friendRoom = GetFriendRoomSession(&isHost);
    if (!friendRoom) {
        if (manager) manager->Update();
        return;
    }

    const bool useHostCPUData = !isHost;
    if (useHostCPUData) {
        RefreshReceivedFriendRoomCPUState();
    }
    if (isHost) {
        Kart::Manager *kartManager = Kart::Manager::sInstance;
        if (kartManager) {
            const u8 cpuCount = GetFriendRoomCPUCount();
            for (u8 i = 0; i < cpuCount; ++i) {
                const u8 playerId = GetFriendRoomCPUId(i);
                BindHostRacedataFactory(playerId, kartManager->GetKartPlayer(playerId));
            }
        }
    }
    if (manager) manager->Update();

    EndFriendRoomCPUsWhenHumansFinished();

    if (useHostCPUData) {
        ApplyReceivedCPUItems();
    }

    SyncFriendRoomCPUScenarioNames(Racedata::sInstance);
    SyncFriendRoomCPUSectionNames(false);

    RefreshFriendRoomCPUAidMappings();
}
kmCall(0x80554bd8, UpdateFriendRoomCPUs);

}  // namespace Network
}  // namespace Pulsar
