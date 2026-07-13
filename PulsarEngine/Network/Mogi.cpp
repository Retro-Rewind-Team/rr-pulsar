#include <kamek.hpp>
#include <PulsarSystem.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <MarioKartWii/RKSYS/RKSYSMgr.hpp>
#include <MarioKartWii/System/Identifiers.hpp>
#include <MarioKartWii/UI/Section/SectionMgr.hpp>
#include <runtimeWrite.hpp>
#include <Network/Mogi.hpp>
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
static const u32 MOGI_TEAM_SIZE_6 = 0x30000000;
static const u8 MOGI_RACE_COUNT = 12;
static const float MOGI_EXPECTATION_RANGE = 100.0f;
static const float MOGI_MAX_GAIN_MMR = 250.0f;
static const float MOGI_MAX_GAIN_AT_START = 3.50f;
static const float MOGI_MAX_GAIN_AT_TARGET = 0.10f;
static const float MOGI_MAX_LOSS = -2.50f;
static const float MOGI_MAX_LOSS_AT_FLOOR = -0.10f;
static const float MOGI_LOSS_TAPER_START = 25.0f;
static const float MOGI_LOSS_TAPER_END = 5.0f;
static const float MOGI_DISCONNECT_PENALTY = 1.0f;

static bool sActive = false;
static bool sEnabled = false;
static bool sTeamFormat = false;
static u8 sPlayersPerTeam = 2;
static u8 sTeamByPlayer[12] = {};
static u32 sLobbyGroupId = 0;
static u32 sLobbySeed = 0;
static u16 sRemoteMMR[12][2];
static bool sMMRFinalized = false;
static bool sSessionActive = false;
static bool sPendingDisconnect = false;
static bool sResultsSectionSeen = false;
static bool sStartReported = false;

static void SelectLobbyFormat(u32 groupId);

static bool IsFriendRoom(const RKNet::Controller* controller) {
    return controller != nullptr &&
           (controller->roomType == RKNet::ROOMTYPE_FROOM_HOST ||
            controller->roomType == RKNet::ROOMTYPE_FROOM_NONHOST);
}

static void ResetRemoteMMR() {
    for (u8 aid = 0; aid < 12; ++aid) {
        sRemoteMMR[aid][0] = 0xFFFF;
        sRemoteMMR[aid][1] = 0xFFFF;
    }
}

static float GetMaximumLoss(float mmr) {
    if (mmr <= MOGI_LOSS_TAPER_END) return MOGI_MAX_LOSS_AT_FLOOR;
    if (mmr >= MOGI_LOSS_TAPER_START) return MOGI_MAX_LOSS;

    const float progress = (mmr - MOGI_LOSS_TAPER_END) /
                           (MOGI_LOSS_TAPER_START - MOGI_LOSS_TAPER_END);
    return MOGI_MAX_LOSS_AT_FLOOR +
           (MOGI_MAX_LOSS - MOGI_MAX_LOSS_AT_FLOOR) * progress;
}

void OnDisconnect() {
    if (!sSessionActive) return;

    sSessionActive = false;
    if (sMMRFinalized) return;

    RKSYS::Mgr* rksys = RKSYS::Mgr::sInstance;
    if (rksys != nullptr && rksys->curLicenseId < 4) {
        const float currentMMR = MogiRating::GetUserMMR(rksys->curLicenseId);
        const float maximumLoss = -GetMaximumLoss(currentMMR);
        const float penalty = MOGI_DISCONNECT_PENALTY < maximumLoss ?
                                  MOGI_DISCONNECT_PENALTY : maximumLoss;
        MogiRating::SetUserMMR(rksys->curLicenseId, currentMMR - penalty);
    }
    sMMRFinalized = true;
}

static u16 EncodeMMR(float mmr) {
    int encoded = static_cast<int>(mmr * 100.0f + 0.5f);
    const int minimumEncodedMMR = static_cast<int>(MogiRating::MIN_MMR * 100.0f + 0.5f);
    if (encoded < minimumEncodedMMR) encoded = minimumEncodedMMR;
    if (encoded > 30000) encoded = 30000;
    return static_cast<u16>(encoded);
}

bool IsEnabled() {
    return sEnabled;
}

void SetEnabled(bool enabled) {
    if (!enabled && sEnabled) OnDisconnect();
    sEnabled = enabled;
    if (!enabled) {
        sActive = false;
        sTeamFormat = false;
        sLobbyGroupId = 0;
        sLobbySeed = 0;
        ResetRemoteMMR();
        sMMRFinalized = false;
        sSessionActive = false;
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
    RKNet::Controller* controller = RKNet::Controller::sInstance;
    const bool connectionLost = controller == nullptr ||
                                controller->roomType == RKNet::ROOMTYPE_NONE ||
                                controller->connectionState == RKNet::CONNECTIONSTATE_SHUTDOWN ||
                                static_cast<u32>(controller->connectionState) > static_cast<u32>(RKNet::CONNECTIONSTATE_ROOM);
    if (sSessionActive && connectionLost) OnDisconnect();

    if (!IsEnabled() || !IsPublicRoom()) {
        const bool isConvertedFriendRoom = controller != nullptr &&
                                           (controller->roomType == RKNet::ROOMTYPE_FROOM_HOST ||
                                            controller->roomType == RKNet::ROOMTYPE_FROOM_NONHOST);
        if (!sPendingDisconnect && !(sActive && isConvertedFriendRoom)) {
            sActive = false;
            sStartReported = false;
        }
        return;
    }

    controller = RKNet::Controller::sInstance;
    const RKNet::ControllerSub& sub = controller->subs[controller->currentSub];
    if (sub.groupId == 0) return;
    if (!sActive) {
        sLobbyGroupId = sub.groupId;
        SelectLobbyFormat(sub.groupId);
        sActive = true;
        ResetRemoteMMR();
        sMMRFinalized = false;
        sSessionActive = true;
        sPendingDisconnect = false;
        sStartReported = false;
    } else if (sLobbyGroupId != sub.groupId) {
        // Host migration can replace the network group without starting a new Mogi session.
        sLobbyGroupId = sub.groupId;
    }
    System::sInstance->netMgr.racesPerGP = MOGI_RACE_COUNT - 1;
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

u8 GetTeamForPlayer(u8 playerIdx) {
    if (!IsTeamFormat()) return playerIdx;
    return sTeamByPlayer[playerIdx < 12 ? playerIdx : 0];
}

u32 GetLobbySeed() {
    return sLobbySeed;
}

void FillMMRPacket(u16& player0, u16& player1) {
    RKSYS::Mgr* rksys = RKSYS::Mgr::sInstance;
    const u16 mmr = rksys != nullptr ? EncodeMMR(MogiRating::GetUserMMR(rksys->curLicenseId)) : 1000;
    player0 = mmr;
    player1 = mmr;
}

void ReceiveMMRPacket(u8 aid, u16 player0, u16 player1) {
    if (aid >= 12) return;
    const u16 minimumEncodedMMR = static_cast<u16>(MogiRating::MIN_MMR * 100.0f + 0.5f);
    if (player0 < minimumEncodedMMR || player0 > 30000 || player1 < minimumEncodedMMR || player1 > 30000) return;
    sRemoteMMR[aid][0] = player0;
    sRemoteMMR[aid][1] = player1;
}

u16 GetRemoteMMR(u8 aid, u8 playerIdOnConsole) {
    if (aid >= 12 || playerIdOnConsole >= 2) return 0xFFFF;
    return sRemoteMMR[aid][playerIdOnConsole];
}

static void SelectLobbyFormat(u32 groupId) {
    if (sActive) return;

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

    for (u8 i = 0; i < 12; ++i) sTeamByPlayer[i] = i / sPlayersPerTeam;
    for (s32 i = 11; i > 0; --i) {
        seed = seed * 1664525 + 1013904223;
        const u8 swapIdx = (u8)(seed % (u32)(i + 1));
        const u8 team = sTeamByPlayer[i];
        sTeamByPlayer[i] = sTeamByPlayer[swapIdx];
        sTeamByPlayer[swapIdx] = team;
    }
    UI::ExtendedTeamSelect::RandomizeTeamColors(sLobbySeed);
}

void PrepareHostRoom(u32& hostContext2, u8& raceCount) {
    RKNet::Controller* controller = RKNet::Controller::sInstance;
    const bool isMigratedFriendRoom = sActive && IsFriendRoom(controller);
    if (!IsEnabled() || (!IsPublicRoom() && !isMigratedFriendRoom)) return;

    const RKNet::ControllerSub& sub = controller->subs[controller->currentSub];
    SelectLobbyFormat(sub.groupId);
    sActive = true;
    sMMRFinalized = false;
    sSessionActive = true;
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
    sSessionActive = true;
        sPendingDisconnect = false;
        sResultsSectionSeen = false;
        sStartReported = false;
    System::sInstance->netMgr.racesPerGP = MOGI_RACE_COUNT - 1;
}

static u16 GetTeamScore(const RacedataScenario& scenario, u8 team) {
    u16 score = 0;
    for (u8 i = 0; i < scenario.playerCount; ++i) {
        if (GetTeamForPlayer(i) == team) score += scenario.players[i].score;
    }
    return score;
}

static float GetPlayerRank(const RacedataScenario& scenario, u8 playerIdx) {
    u8 betterCount = 0;
    u8 tiedCount = 0;
    for (u8 i = 0; i < scenario.playerCount; ++i) {
        if (scenario.players[i].score > scenario.players[playerIdx].score) {
            ++betterCount;
        } else if (scenario.players[i].score == scenario.players[playerIdx].score) {
            ++tiedCount;
        }
    }
    return static_cast<float>(betterCount) + (static_cast<float>(tiedCount) - 1.0f) * 0.5f;
}

static float GetTeamRank(const RacedataScenario& scenario, u8 playerIdx) {
    const u8 ownTeam = GetTeamForPlayer(playerIdx);
    const u16 ownScore = GetTeamScore(scenario, ownTeam);
    u8 betterCount = 0;
    u8 tiedCount = 0;
    const u8 teamCount = 12 / sPlayersPerTeam;
    for (u8 team = 0; team < teamCount; ++team) {
        if (team == ownTeam) continue;
        const u16 teamScore = GetTeamScore(scenario, team);
        if (teamScore > ownScore) {
            ++betterCount;
        } else if (teamScore == ownScore) {
            ++tiedCount;
        }
    }
    return static_cast<float>(betterCount) + static_cast<float>(tiedCount) * 0.5f;
}

static float GetPlayerMMR(const RacedataScenario& scenario, u8 playerIdx, float fallbackMMR) {
    if (playerIdx >= 12) return fallbackMMR;
    if (scenario.players[playerIdx].playerType == PLAYER_REAL_LOCAL) return fallbackMMR;

    const RKNet::Controller* controller = RKNet::Controller::sInstance;
    if (controller == nullptr) return fallbackMMR;

    const u8 aid = controller->aidsBelongingToPlayerIds[playerIdx];
    if (aid >= 12) return fallbackMMR;

    u8 playerOnConsole = 0;
    for (u8 i = 0; i < playerIdx; ++i) {
        if (controller->aidsBelongingToPlayerIds[i] == aid) ++playerOnConsole;
    }

    const u16 encodedMMR = GetRemoteMMR(aid, playerOnConsole);
    return encodedMMR == 0xFFFF ? fallbackMMR : static_cast<float>(encodedMMR) / 100.0f;
}

static float GetTeamMMR(const RacedataScenario& scenario, const float* playerMMRs, u8 team) {
    float totalMMR = 0.0f;
    u8 playerCount = 0;
    for (u8 i = 0; i < scenario.playerCount; ++i) {
        if (GetTeamForPlayer(i) != team) continue;
        totalMMR += playerMMRs[i];
        ++playerCount;
    }
    return playerCount > 0 ? totalMMR / static_cast<float>(playerCount) : MogiRating::DEFAULT_MMR;
}

static float GetPairExpectedPerformance(float playerMMR, float opponentMMR) {
    float expected = 0.5f + (playerMMR - opponentMMR) / (2.0f * MOGI_EXPECTATION_RANGE);
    if (expected < 0.05f) expected = 0.05f;
    if (expected > 0.95f) expected = 0.95f;
    return expected;
}

static float GetExpectedPerformance(float playerMMR, const float* opponentMMRs, u8 opponentCount) {
    if (opponentCount == 0) return 0.5f;

    float expected = 0.0f;
    for (u8 i = 0; i < opponentCount; ++i) {
        expected += GetPairExpectedPerformance(playerMMR, opponentMMRs[i]);
    }
    return expected / static_cast<float>(opponentCount);
}

static float GetPerformance(float rank, u8 count) {
    if (count <= 1) return 0.5f;
    return 1.0f - ((float)rank / (float)(count - 1));
}

static float GetMaximumGain(float mmr) {
    if (mmr <= MogiRating::MIN_MMR) return MOGI_MAX_GAIN_AT_START;
    if (mmr >= MOGI_MAX_GAIN_MMR) return MOGI_MAX_GAIN_AT_TARGET;

    const float progress = (mmr - MogiRating::MIN_MMR) /
                           (MOGI_MAX_GAIN_MMR - MogiRating::MIN_MMR);
    const float remaining = 1.0f - progress;
    return MOGI_MAX_GAIN_AT_TARGET +
           (MOGI_MAX_GAIN_AT_START - MOGI_MAX_GAIN_AT_TARGET) * remaining * remaining;
}

static float GetMMRDelta(float mmr, float performance, float expectedPerformance) {
    const float performanceDifference = performance - expectedPerformance;
    float delta = performanceDifference * (performanceDifference >= 0.0f ? MOGI_MAX_GAIN_AT_START : -MOGI_MAX_LOSS);
    const float distance = (mmr - MogiRating::MIN_MMR) /
                           (MogiRating::MAX_MMR - MogiRating::MIN_MMR);
    const float ceilingFactor = 0.25f + (1.0f - distance) * 0.75f;
    const float floorFactor = 0.25f + distance * 0.75f;
    delta *= delta >= 0.0f ? ceilingFactor : floorFactor;
    if (delta > 0.0f) {
        const float maximumGain = GetMaximumGain(mmr);
        if (delta > maximumGain) delta = maximumGain;
    } else {
        const float maximumLoss = GetMaximumLoss(mmr);
        if (delta < maximumLoss) delta = maximumLoss;
    }
    return delta;
}

static u8 GetFinalRaceNumber() {
    if (System::sInstance == nullptr) return MOGI_RACE_COUNT - 1;
    return System::sInstance->netMgr.racesPerGP;
}

void OnFinalResults() {
    if (Racedata::sInstance == nullptr) return;

    const RacedataScenario& raceScenario = Racedata::sInstance->racesScenario;
    const RacedataScenario& scenario = Racedata::sInstance->menusScenario;
    if (!IsActive() || sMMRFinalized) return;

    u8 currentRaceNumber = raceScenario.settings.raceNumber;
    if (SectionMgr::sInstance != nullptr && SectionMgr::sInstance->sectionParams != nullptr) {
        const s32 onlineRaceNumber = SectionMgr::sInstance->sectionParams->onlineParams.currentRaceNumber;
        if (onlineRaceNumber > currentRaceNumber) currentRaceNumber = static_cast<u8>(onlineRaceNumber);
    }
    if (currentRaceNumber < GetFinalRaceNumber()) return;

    RKSYS::Mgr* rksys = RKSYS::Mgr::sInstance;
    if (rksys == nullptr || rksys->curLicenseId >= 4) return;

    s32 localPlayerId = -1;
    bool updated = false;
    for (u8 i = 0; i < scenario.playerCount; ++i) {
        if (scenario.players[i].playerType == PLAYER_REAL_LOCAL) {
            localPlayerId = i;
            break;
        }
    }
    if (localPlayerId < 0 && scenario.localPlayerCount > 0) {
        const u8 hudPlayerId = scenario.settings.hudPlayerIds[0];
        if (hudPlayerId < scenario.playerCount) localPlayerId = hudPlayerId;
    }
    if (localPlayerId >= 0) {
        const u8 i = static_cast<u8>(localPlayerId);

        const u8 count = IsTeamFormat() ? 12 / sPlayersPerTeam : scenario.playerCount;
        const float rank = IsTeamFormat() ? GetTeamRank(scenario, i) : GetPlayerRank(scenario, i);
        const float performance = GetPerformance(rank, count);
        const float currentMMR = MogiRating::GetUserMMR(rksys->curLicenseId);
        float playerMMRs[12];
        for (u8 player = 0; player < scenario.playerCount; ++player) {
            playerMMRs[player] = GetPlayerMMR(scenario, player, currentMMR);
        }

        float opponentMMRs[12];
        u8 opponentCount = 0;
        float expectedPerformance;
        if (IsTeamFormat()) {
            const u8 ownTeam = GetTeamForPlayer(i);
            const u8 teamCount = 12 / sPlayersPerTeam;
            const float ownTeamMMR = GetTeamMMR(scenario, playerMMRs, ownTeam);
            for (u8 team = 0; team < teamCount; ++team) {
                if (team != ownTeam) opponentMMRs[opponentCount++] = GetTeamMMR(scenario, playerMMRs, team);
            }
            expectedPerformance = GetExpectedPerformance(ownTeamMMR, opponentMMRs, opponentCount);
        } else {
            for (u8 player = 0; player < scenario.playerCount; ++player) {
                if (player != i) opponentMMRs[opponentCount++] = playerMMRs[player];
            }
            expectedPerformance = GetExpectedPerformance(currentMMR, opponentMMRs, opponentCount);
        }
        MogiRating::SetUserMMR(rksys->curLicenseId,
                               currentMMR + GetMMRDelta(currentMMR, performance, expectedPerformance));
        updated = true;
    }
    if (updated) {
        sMMRFinalized = true;
        sSessionActive = false;
        sPendingDisconnect = true;
    }
}

void OnResultsDisplayed() {
    if (sMMRFinalized) sPendingDisconnect = true;
}

static void MonitorDisconnect() {
    if (!sSessionActive) return;

    const RKNet::Controller* controller = RKNet::Controller::sInstance;
    if (controller == nullptr || controller->roomType == RKNet::ROOMTYPE_NONE ||
        controller->connectionState == RKNet::CONNECTIONSTATE_SHUTDOWN ||
        static_cast<u32>(controller->connectionState) > static_cast<u32>(RKNet::CONNECTIONSTATE_ROOM)) {
        OnDisconnect();
    }
}

void ProcessPendingDisconnect() {
    MonitorDisconnect();
    if (!sPendingDisconnect || SectionMgr::sInstance == nullptr || SectionMgr::sInstance->curSection == nullptr) return;

    const SectionId sectionId = SectionMgr::sInstance->curSection->sectionId;
    const bool isMogiResults = sectionId == SECTION_P1_WIFI_FRIEND_VS ||
                               sectionId == SECTION_P1_WIFI_FRIEND_TEAMVS ||
                               sectionId == SECTION_P2_WIFI_FRIEND_VS ||
                               sectionId == SECTION_P2_WIFI_FRIEND_TEAMVS ||
                               sectionId == SECTION_P1_WIFI_VS ||
                               sectionId == SECTION_P2_WIFI_VS ||
                               sectionId == SECTION_P1_WIFI_FROM_FROOM_RACE ||
                               sectionId == SECTION_P2_WIFI_FROM_FROOM_RACE;
    if (isMogiResults) {
        sResultsSectionSeen = true;
        SectionMgr::sInstance->SetNextSection(SECTION_MAIN_MENU_FROM_MENU, 0);
        return;
    }

    const bool isOfflineResults = sectionId == SECTION_VS_RACE_AWARD || sectionId == SECTION_GP_AWARD;
    if (isOfflineResults || sResultsSectionSeen) {
        RKNet::Controller* controller = RKNet::Controller::sInstance;
        if (controller != nullptr) controller->ScheduleShutdown();
        sPendingDisconnect = false;
        sActive = false;
        sResultsSectionSeen = false;
    }
}

static FrameLoadHook mogiDisconnectHook(ProcessPendingDisconnect);

typedef void (*SystemExitFn)();
typedef void (*SystemRestartFn)(u32 resetCode);

kmRuntimeUse(0x801a856c);
kmRuntimeUse(0x801a8688);
kmRuntimeUse(0x801a8858);

static void ShutdownSystemWithMogiPenalty() {
    OnDisconnect();
    reinterpret_cast<SystemExitFn>(kmRuntimeAddr(0x801a856c))();
}

static void RestartSystemWithMogiPenalty(u32 resetCode) {
    OnDisconnect();
    reinterpret_cast<SystemRestartFn>(kmRuntimeAddr(0x801a8688))(resetCode);
}

static void ReturnToMenuWithMogiPenalty() {
    OnDisconnect();
    reinterpret_cast<SystemExitFn>(kmRuntimeAddr(0x801a8858))();
}

kmCall(0x8000b174, ShutdownSystemWithMogiPenalty);
kmCall(0x8000b298, ShutdownSystemWithMogiPenalty);
kmCall(0x8000b488, ShutdownSystemWithMogiPenalty);
kmCall(0x8000b1e0, RestartSystemWithMogiPenalty);
kmCall(0x8000b2c4, RestartSystemWithMogiPenalty);
kmCall(0x8000b4b4, RestartSystemWithMogiPenalty);
kmCall(0x8000b1a8, ReturnToMenuWithMogiPenalty);
kmCall(0x801a0080, ReturnToMenuWithMogiPenalty);
kmCall(0x801acd70, ReturnToMenuWithMogiPenalty);

}  // namespace Mogi
}  // namespace Pulsar
