#include <kamek.hpp>
#include <PulsarSystem.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <MarioKartWii/RKSYS/RKSYSMgr.hpp>
#include <MarioKartWii/System/Identifiers.hpp>
#include <MarioKartWii/UI/Section/SectionMgr.hpp>
#include <Network/Mogi.hpp>
#include <Network/PacketExpansion.hpp>
#include <Network/GPReport.hpp>
#include <Network/Rating/MogiRating.hpp>
#include <Settings/Settings.hpp>
#include <UI/ExtendedTeamSelect/ExtendedTeamSelect.hpp>

namespace Pulsar {
namespace Mogi {

static const u32 MOGI_HOST_FLAG = 0x80000000;
static const u32 MOGI_TEAM_FLAG = 0x40000000;
static const u32 MOGI_TEAM_SIZE_MASK = 0x30000000;
static const u32 MOGI_TEAM_SIZE_2 = 0x10000000;
static const u32 MOGI_TEAM_SIZE_3 = 0x20000000;
static const u32 MOGI_TEAM_SIZE_4 = 0x00000000;
static const u32 MOGI_TEAM_SIZE_6 = 0x30000000;
static const u8 MOGI_RACE_COUNT = 12;

static bool sActive = false;
static bool sEnabled = false;
static bool sTeamFormat = false;
static bool sForceTwoVsTwo = false;
static u8 sPlayersPerTeam = 2;
static u8 sTeamByPlayer[12] = {};
static u32 sLobbyGroupId = 0;
static u32 sLobbySeed = 0;
static u16 sRemoteMMR[12][2];
static bool sMMRFinalized = false;
static bool sPendingDisconnect = false;
static bool sResultsSectionSeen = false;
static bool sStartReported = false;

static void ReportTeamMap(const char* label) {
    OS::Report("[Mogi] %s active=%u teamFormat=%u playersPerTeam=%u map=%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u\n",
               label, sActive, sTeamFormat, sPlayersPerTeam, sTeamByPlayer[0], sTeamByPlayer[1],
               sTeamByPlayer[2], sTeamByPlayer[3], sTeamByPlayer[4], sTeamByPlayer[5], sTeamByPlayer[6],
               sTeamByPlayer[7], sTeamByPlayer[8], sTeamByPlayer[9], sTeamByPlayer[10], sTeamByPlayer[11]);
}

static void SelectLobbyFormat(u32 groupId);

static void ResetRemoteMMR() {
    for (u8 aid = 0; aid < 12; ++aid) {
        sRemoteMMR[aid][0] = 0xFFFF;
        sRemoteMMR[aid][1] = 0xFFFF;
    }
}

static u16 EncodeMMR(float mmr) {
    int encoded = static_cast<int>(mmr * 100.0f + 0.5f);
    if (encoded < 1000) encoded = 1000;
    if (encoded > 30000) encoded = 30000;
    return static_cast<u16>(encoded);
}

bool IsEnabled() {
    return sEnabled;
}

void SetEnabled(bool enabled) {
    sEnabled = enabled;
    if (!enabled) {
        sActive = false;
        sTeamFormat = false;
        sForceTwoVsTwo = false;
        sLobbyGroupId = 0;
        sLobbySeed = 0;
        ResetRemoteMMR();
        sMMRFinalized = false;
        sPendingDisconnect = false;
        sResultsSectionSeen = false;
        sStartReported = false;
    }
}

bool IsPublicRoom() {
    const RKNet::Controller* controller = RKNet::Controller::sInstance;
    if (!controller) return false;

    switch (controller->roomType) {
        case RKNet::ROOMTYPE_VS_WW:
        case RKNet::ROOMTYPE_VS_REGIONAL:
        case RKNet::ROOMTYPE_JOINING_WW:
        case RKNet::ROOMTYPE_JOINING_REGIONAL:
            return true;
        default:
            return false;
    }
}

static void UpdateActiveFromRoom() {
    if (!IsEnabled() || !IsPublicRoom()) {
        RKNet::Controller* controller = RKNet::Controller::sInstance;
        const bool isConvertedFriendRoom = controller != nullptr &&
                                           (controller->roomType == RKNet::ROOMTYPE_FROOM_HOST ||
                                            controller->roomType == RKNet::ROOMTYPE_FROOM_NONHOST);
        if (!sPendingDisconnect && !(sActive && isConvertedFriendRoom)) {
            sActive = false;
            sStartReported = false;
        }
        return;
    }

    RKNet::Controller* controller = RKNet::Controller::sInstance;
    const RKNet::ControllerSub& sub = controller->subs[controller->currentSub];
    if (sub.groupId == 0) return;
    if (!sActive || sLobbyGroupId != sub.groupId) {
        OS::Report("[Mogi] new room group=%u oldGroup=%u roomType=%u players=%u\n", sub.groupId, sLobbyGroupId,
                   controller->roomType, sub.playerCount);
        sLobbyGroupId = sub.groupId;
        sForceTwoVsTwo = false;
        SelectLobbyFormat(sub.groupId);
        sActive = true;
        ResetRemoteMMR();
        sMMRFinalized = false;
        sPendingDisconnect = false;
        sStartReported = false;
    }
    System::sInstance->netMgr.racesPerGP = 11;
}

void UpdateRoomState() {
    UpdateActiveFromRoom();
}

bool IsActive() {
    UpdateActiveFromRoom();
    return sActive;
}

bool IsTeamFormat() {
    return sActive && sTeamFormat;
}

bool CanStartRace() {
    UpdateActiveFromRoom();
    RKNet::Controller* controller = RKNet::Controller::sInstance;
    if (!controller) return false;
    const RKNet::ControllerSub& sub = controller->subs[controller->currentSub];
    return sub.playerCount == 12;
}

u8 GetTeamForPlayer(u8 playerIdx) {
    if (!IsTeamFormat()) return playerIdx;
    return sTeamByPlayer[playerIdx < 12 ? playerIdx : 0];
}

void SetTeamForPlayer(u8 playerIdx, u8 team) {
    if (playerIdx < 12 && team < 6) sTeamByPlayer[playerIdx] = team;
}

void FillRoomData(::Pulsar::Network::MogiRoomData& roomData) {
    roomData.magic = ROOM_PACKET_MAGIC;
    roomData.teamFormat = sTeamFormat ? 1 : 0;
    roomData.playersPerTeam = sPlayersPerTeam;
    for (u8 i = 0; i < 12; ++i) roomData.teamByPlayer[i] = sTeamByPlayer[i];
    UI::ExtendedTeamSelect::GetTeamColorOrder(roomData.teamColors);
}

bool ApplyRoomData(const ::Pulsar::Network::MogiRoomData& roomData) {
    if (roomData.magic != ROOM_PACKET_MAGIC || roomData.teamFormat > 1) {
        OS::Report("[Mogi] room data rejected magic=0x%04X expected=0x%04X teamFormat=%u\n", roomData.magic,
                   ROOM_PACKET_MAGIC, roomData.teamFormat);
        return false;
    }
    if (roomData.playersPerTeam != 2 && roomData.playersPerTeam != 3 &&
        roomData.playersPerTeam != 4 && roomData.playersPerTeam != 6) {
        OS::Report("[Mogi] room data rejected playersPerTeam=%u\n", roomData.playersPerTeam);
        return false;
    }

    RKNet::Controller* controller = RKNet::Controller::sInstance;
    if (controller != nullptr) {
        SelectLobbyFormat(controller->subs[controller->currentSub].groupId);
    }
    sActive = true;
    ReportTeamMap("seed format applied");
    return true;
}

u32 GetLobbySeed() {
    return sLobbySeed;
}

u8 GetRaceCount() {
    return MOGI_RACE_COUNT - 1;
}

void FillMMRPacket(u16& player0, u16& player1) {
    RKSYS::Mgr* rksys = RKSYS::Mgr::sInstance;
    const u16 mmr = rksys != nullptr ? EncodeMMR(MogiRating::GetUserMMR(rksys->curLicenseId)) : 1000;
    player0 = mmr;
    player1 = mmr;
}

void ReceiveMMRPacket(u8 aid, u16 player0, u16 player1) {
    if (aid >= 12) return;
    if (player0 < 1000 || player0 > 30000 || player1 < 1000 || player1 > 30000) return;
    sRemoteMMR[aid][0] = player0;
    sRemoteMMR[aid][1] = player1;
}

u16 GetRemoteMMR(u8 aid, u8 playerIdOnConsole) {
    if (aid >= 12 || playerIdOnConsole >= 2) return 0xFFFF;
    return sRemoteMMR[aid][playerIdOnConsole];
}

static void SelectLobbyFormat(u32 groupId) {
    sLobbyGroupId = groupId;
    sLobbySeed = groupId ^ 0x4D4F4749;
    u32 seed = sLobbySeed;
    sTeamFormat = (seed & 1) != 0;
    switch ((seed >> 1) & 3) {
        case 0:
            sPlayersPerTeam = 2;
            break;
        case 1:
            sPlayersPerTeam = 3;
            break;
        case 2:
            sPlayersPerTeam = 4;
            break;
        default:
            sPlayersPerTeam = 6;
            break;
    }

    RKNet::Controller* controller = RKNet::Controller::sInstance;
    if (controller != nullptr && controller->subs[controller->currentSub].playerCount == 4) {
        sForceTwoVsTwo = true;
    }
    if (sForceTwoVsTwo) {
        sTeamFormat = true;
        sPlayersPerTeam = 2;
    }

    for (u8 i = 0; i < 12; ++i) sTeamByPlayer[i] = i / sPlayersPerTeam;
    for (s32 i = 11; i > 0; --i) {
        seed = seed * 1664525 + 1013904223;
        const u8 swapIdx = (u8)(seed % (u32)(i + 1));
        const u8 team = sTeamByPlayer[i];
        sTeamByPlayer[i] = sTeamByPlayer[swapIdx];
        sTeamByPlayer[swapIdx] = team;
    }
    UI::ExtendedTeamSelect::RandomizeTeamColors(sLobbySeed);
    ReportTeamMap("seed format selected");
}

void PrepareHostRoom(u32& hostContext2, u8& raceCount) {
    if (!IsEnabled() || !IsPublicRoom()) return;

    RKNet::Controller* controller = RKNet::Controller::sInstance;
    const RKNet::ControllerSub& sub = controller->subs[controller->currentSub];
    SelectLobbyFormat(sub.groupId);
    sActive = true;
    sMMRFinalized = false;
    sPendingDisconnect = false;
    sResultsSectionSeen = false;

    if (!sStartReported) {
        Network::Report("wl:mkw_mogi_start", "1");
        sStartReported = true;
    }

    hostContext2 |= MOGI_HOST_FLAG;
    if (sTeamFormat) hostContext2 |= MOGI_TEAM_FLAG;
    hostContext2 &= ~MOGI_TEAM_SIZE_MASK;
    if (sPlayersPerTeam == 2)
        hostContext2 |= MOGI_TEAM_SIZE_2;
    else if (sPlayersPerTeam == 3)
        hostContext2 |= MOGI_TEAM_SIZE_3;
    else if (sPlayersPerTeam == 6)
        hostContext2 |= MOGI_TEAM_SIZE_6;
    // ROOM stores the zero-based final race index: 11 represents 12 races.
    raceCount = MOGI_RACE_COUNT - 1;
    System::sInstance->netMgr.racesPerGP = raceCount;
    OS::Report("[Mogi] host room prepared group=%u context2=0x%08X raceCount=%u\n", sub.groupId, hostContext2,
               raceCount);
}

void ApplyHostRoom(u32 hostContext2) {
    if ((hostContext2 & MOGI_HOST_FLAG) == 0) return;

    RKNet::Controller* controller = RKNet::Controller::sInstance;
    if (controller != nullptr) {
        const RKNet::ControllerSub& sub = controller->subs[controller->currentSub];
        SelectLobbyFormat(sub.groupId);
    }
    sActive = true;
    sMMRFinalized = false;
    sPendingDisconnect = false;
    sResultsSectionSeen = false;
    System::sInstance->netMgr.racesPerGP = GetRaceCount();
    OS::Report("[Mogi] host room applied context2=0x%08X group=%u\n", hostContext2, sLobbyGroupId);
    ReportTeamMap("host room state");
}

static u16 GetTeamScore(const RacedataScenario& scenario, u8 team) {
    u16 score = 0;
    for (u8 i = 0; i < scenario.playerCount; ++i) {
        if (GetTeamForPlayer(i) == team) score += scenario.players[i].score;
    }
    return score;
}

static u8 GetPlayerRank(const RacedataScenario& scenario, u8 playerIdx) {
    u8 rank = 0;
    for (u8 i = 0; i < scenario.playerCount; ++i) {
        if (scenario.players[i].score > scenario.players[playerIdx].score) ++rank;
    }
    return rank;
}

static u8 GetTeamRank(const RacedataScenario& scenario, u8 playerIdx) {
    const u8 ownTeam = GetTeamForPlayer(playerIdx);
    const u16 ownScore = GetTeamScore(scenario, ownTeam);
    u8 rank = 0;
    const u8 teamCount = 12 / sPlayersPerTeam;
    for (u8 team = 0; team < teamCount; ++team) {
        if (team != ownTeam && GetTeamScore(scenario, team) > ownScore) ++rank;
    }
    return rank;
}

static float GetPerformance(u8 rank, u8 count) {
    if (count <= 1) return 0.5f;
    return 1.0f - ((float)rank / (float)(count - 1));
}

static float GetMMRDelta(float mmr, float performance) {
    // A deliberately small Elo-like step. The soft ceiling keeps 300.00 a practical
    // upper bound without making it reachable through ordinary event wins.
    const float expected = 0.5f;
    float delta = (performance - expected) * 2.0f;
    const float distance = (mmr - MogiRating::MIN_MMR) /
                           (MogiRating::MAX_MMR - MogiRating::MIN_MMR);
    const float ceilingFactor = 0.25f + (1.0f - distance) * 0.75f;
    const float floorFactor = 0.25f + distance * 0.75f;
    delta *= delta >= 0.0f ? ceilingFactor : floorFactor;
    if (delta > 0.0f) {
        const float remaining = MogiRating::MAX_MMR - mmr;
        const float softGainCap = remaining * 0.005f;
        if (delta > softGainCap) delta = softGainCap;
    }
    return delta;
}

void OnFinalRace(const RacedataScenario& scenario) {
    if (!sActive || sMMRFinalized || scenario.settings.raceNumber < MOGI_RACE_COUNT - 1) return;
    sMMRFinalized = true;

    RKSYS::Mgr* rksys = RKSYS::Mgr::sInstance;
    if (rksys == nullptr) return;

    u8 localPlayersSeen = 0;
    for (u8 i = 0; i < scenario.playerCount; ++i) {
        if (scenario.players[i].playerType != PLAYER_REAL_LOCAL) continue;
        if (localPlayersSeen++ != 0) continue;

        const u8 count = IsTeamFormat() ? 12 / sPlayersPerTeam : scenario.playerCount;
        const u8 rank = IsTeamFormat() ? GetTeamRank(scenario, i) : GetPlayerRank(scenario, i);
        const float performance = GetPerformance(rank, count);
        const float currentMMR = MogiRating::GetUserMMR(rksys->curLicenseId);
        MogiRating::SetUserMMR(rksys->curLicenseId, currentMMR + GetMMRDelta(currentMMR, performance));
        break;
    }
    sPendingDisconnect = true;
}

void ProcessPendingDisconnect() {
    if (!sPendingDisconnect || SectionMgr::sInstance == nullptr || SectionMgr::sInstance->curSection == nullptr) return;

    const SectionId sectionId = SectionMgr::sInstance->curSection->sectionId;
    const bool isMogiResults = sectionId == SECTION_P1_WIFI_FRIEND_VS ||
                               sectionId == SECTION_P1_WIFI_FRIEND_TEAMVS ||
                               sectionId == SECTION_P2_WIFI_FRIEND_VS ||
                               sectionId == SECTION_P2_WIFI_FRIEND_TEAMVS;
    if (isMogiResults) {
        OS::Report("[Mogi] result section reached id=0x%02X\n", sectionId);
        sResultsSectionSeen = true;
        return;
    }

    const bool isOfflineResults = sectionId == SECTION_VS_RACE_AWARD || sectionId == SECTION_GP_AWARD;
    if (isOfflineResults || sResultsSectionSeen) {
        RKNet::Controller* controller = RKNet::Controller::sInstance;
        if (controller != nullptr) controller->ScheduleShutdown();
        sPendingDisconnect = false;
        sActive = false;
        sResultsSectionSeen = false;
        sStartReported = false;
        OS::Report("[Mogi] disconnect scheduled after section=0x%02X\n", sectionId);
    }
}

static FrameLoadHook mogiDisconnectHook(ProcessPendingDisconnect);

}  // namespace Mogi
}  // namespace Pulsar
