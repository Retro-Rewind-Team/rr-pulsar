#include <kamek.hpp>
#include <PulsarSystem.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <MarioKartWii/RKSYS/RKSYSMgr.hpp>
#include <MarioKartWii/System/Identifiers.hpp>
#include <MarioKartWii/System/Random.hpp>
#include <MarioKartWii/UI/Section/SectionMgr.hpp>
#include <runtimeWrite.hpp>
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
static const u32 MOGI_TEAM_SIZE_6 = 0x30000000;
static const u8 MOGI_RACE_COUNT = 12;
static const u8 MOGI_GP_RACE_COUNT = 4;
static const u8 MOGI_FORMAT_COUNT = 5;
static const u8 MOGI_FORMAT_NONE = 0xFF;
static const u8 MOGI_FORMAT_VOTE_PENDING = 0;
static const u8 MOGI_FORMAT_VOTE_CAST = 1;
static const u8 MOGI_FORMAT_VOTE_TIMED_OUT = 2;
static const u8 MOGI_FORMAT_VOTE_RESOLVED = 3;
static const float MOGI_EXPECTATION_RANGE = 100.0f;
static const float MOGI_MAX_GAIN_MMR = 250.0f;
static const float MOGI_MAX_GAIN_AT_START = 3.50f;
static const float MOGI_MAX_GAIN_AT_TARGET = 0.10f;
static const float MOGI_MAX_LOSS = -2.50f;
static const float MOGI_MAX_LOSS_AT_FLOOR = -0.10f;
static const float MOGI_LOSS_TAPER_START = 25.0f;
static const float MOGI_LOSS_TAPER_END = 5.0f;
static const u16 MOGI_REDUCED_LOSS_MIN_SCORE = 85;
static const u16 MOGI_REDUCED_LOSS_SCORE_RATIO = 3;
static const float MOGI_REDUCED_LOSS_FACTOR = 0.50f;
static const float MOGI_DISCONNECT_PENALTY = 1.0f;

static bool sActive = false;
static bool sEnabled = false;
static bool sTeamFormat = false;
static u8 sPlayersPerTeam = 2;
static u8 sTeamByPlayer[12] = {};
static u8 sTeamByAid[12][2];
static bool sTeamAssignmentsCaptured = false;
static u32 sLobbyGroupId = 0;
static u32 sLobbySeed = 0;
static u16 sRemoteMMR[12][2];
static bool sMMRFinalized = false;
static bool sSessionActive = false;
static bool sPendingDisconnect = false;
static bool sResultsSectionSeen = false;
static bool sStartReported = false;
static bool sFormatVoteActive = false;
static bool sFormatVoteResolved = false;
static u8 sLocalFormatVoteState = MOGI_FORMAT_VOTE_PENDING;
static u8 sLocalFormatVote = MOGI_FORMAT_NONE;
static u8 sFormatVoteStates[12];
static u8 sFormatVotes[12];
static u8 sDisconnectCountInGP = 0;
static bool sResetRoomAfterGP = false;

struct MogiParticipant {
    bool valid;
    bool activeInGP;
    bool disconnected;
    bool fixedDisconnectScore;
    bool presentOnResults;
    u8 aid;
    u8 playerOnConsole;
    u8 team;
    u8 disconnectRaceInGP;
    u16 score;
    u16 previousScore;
    u16 gpStartScore;
};

static MogiParticipant sParticipants[12];

static void SelectLobbyFormat(u32 groupId);

static void ResetFormatVotes() {
    sFormatVoteActive = true;
    sFormatVoteResolved = false;
    sLocalFormatVoteState = MOGI_FORMAT_VOTE_PENDING;
    sLocalFormatVote = MOGI_FORMAT_NONE;
    for (u8 aid = 0; aid < 12; ++aid) {
        sFormatVoteStates[aid] = MOGI_FORMAT_VOTE_PENDING;
        sFormatVotes[aid] = MOGI_FORMAT_NONE;
    }
}

static void ResetParticipants() {
    for (u8 i = 0; i < 12; ++i) {
        memset(&sParticipants[i], 0, sizeof(sParticipants[i]));
        sParticipants[i].aid = 0xFF;
        sParticipants[i].playerOnConsole = 0xFF;
    }
    sDisconnectCountInGP = 0;
    sResetRoomAfterGP = false;
}

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

static void ResetTeamAssignments() {
    for (u8 aid = 0; aid < 12; ++aid) {
        sTeamByAid[aid][0] = 0xFF;
        sTeamByAid[aid][1] = 0xFF;
    }
    sTeamAssignmentsCaptured = false;
}

static bool GetPlayerIdentity(u8 playerIdx, u8& aid, u8& playerOnConsole) {
    const RKNet::Controller* controller = RKNet::Controller::sInstance;
    if (controller == nullptr || playerIdx >= 12) return false;

    aid = controller->aidsBelongingToPlayerIds[playerIdx];
    if (aid >= 12) return false;

    playerOnConsole = 0;
    for (u8 i = 0; i < playerIdx; ++i) {
        if (controller->aidsBelongingToPlayerIds[i] == aid) ++playerOnConsole;
    }
    return playerOnConsole < 2;
}

static void CaptureTeamAssignments() {
    if (!sActive || !sTeamFormat || sTeamAssignmentsCaptured) return;

    const RKNet::Controller* controller = RKNet::Controller::sInstance;
    if (controller == nullptr) return;

    const RKNet::ControllerSub& sub = controller->subs[controller->currentSub];
    const u8 playerCount = sub.playerCount < 12 ? sub.playerCount : 12;
    if (playerCount == 0) return;

    u8 teamByAid[12][2];
    for (u8 aid = 0; aid < 12; ++aid) {
        teamByAid[aid][0] = 0xFF;
        teamByAid[aid][1] = 0xFF;
    }
    for (u8 playerIdx = 0; playerIdx < playerCount; ++playerIdx) {
        u8 aid;
        u8 playerOnConsole;
        if (!GetPlayerIdentity(playerIdx, aid, playerOnConsole)) return;
        teamByAid[aid][playerOnConsole] = sTeamByPlayer[playerIdx];
    }

    for (u8 aid = 0; aid < 12; ++aid) {
        sTeamByAid[aid][0] = teamByAid[aid][0];
        sTeamByAid[aid][1] = teamByAid[aid][1];
    }
    sTeamAssignmentsCaptured = true;
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
        sFormatVoteActive = false;
        sFormatVoteResolved = false;
        ResetParticipants();
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

static MogiParticipant* FindParticipant(u8 aid, u8 playerOnConsole) {
    for (u8 i = 0; i < 12; ++i) {
        if (sParticipants[i].valid && sParticipants[i].aid == aid &&
            sParticipants[i].playerOnConsole == playerOnConsole) {
            return &sParticipants[i];
        }
    }
    return nullptr;
}

static MogiParticipant* AddParticipant(u8 aid, u8 playerOnConsole, u8 team, u16 score) {
    MogiParticipant* participant = FindParticipant(aid, playerOnConsole);
    if (participant == nullptr) {
        for (u8 i = 0; i < 12; ++i) {
            if (!sParticipants[i].valid) {
                participant = &sParticipants[i];
                break;
            }
        }
    }
    if (participant == nullptr) return nullptr;
    participant->valid = true;
    participant->activeInGP = true;
    participant->disconnected = false;
    participant->fixedDisconnectScore = false;
    participant->presentOnResults = true;
    participant->aid = aid;
    participant->playerOnConsole = playerOnConsole;
    participant->team = team;
    participant->disconnectRaceInGP = 0xFF;
    participant->score = score;
    participant->previousScore = score;
    participant->gpStartScore = score;
    return participant;
}

static void EnsureParticipants() {
    if (!sActive || !sTeamFormat) return;
    const RKNet::Controller* controller = RKNet::Controller::sInstance;
    if (controller == nullptr || RKNet::SELECTHandler::sInstance == nullptr) return;
    const RKNet::ControllerSub& sub = controller->subs[controller->currentSub];
    const Network::ExpSELECTHandler& select = Network::ExpSELECTHandler::Get();
    const u8 playerCount = sub.playerCount < 12 ? sub.playerCount : 12;
    for (u8 playerIdx = 0; playerIdx < playerCount; ++playerIdx) {
        u8 aid;
        u8 playerOnConsole;
        if (!GetPlayerIdentity(playerIdx, aid, playerOnConsole) || FindParticipant(aid, playerOnConsole) != nullptr) continue;
        const Network::PulSELECT& packet = aid == sub.localAid ? select.toSendPacket : select.receivedPackets[aid];
        AddParticipant(aid, playerOnConsole, sTeamByPlayer[playerIdx], packet.playersData[playerOnConsole].sumPoints);
    }
}

void ReceivePlayerScores(u8 aid, u16 player0, u16 player1) {
    if (!sActive || !sTeamFormat || aid >= 12) return;
    CaptureTeamAssignments();
    const RKNet::Controller* controller = RKNet::Controller::sInstance;
    if (controller == nullptr) return;
    const RKNet::ControllerSub& sub = controller->subs[controller->currentSub];
    const u8 playerCount = sub.connectionUserDatas[aid].playersAtConsole;
    const u16 scores[2] = {player0, player1};
    for (u8 slot = 0; slot < 2; ++slot) {
        MogiParticipant* participant = FindParticipant(aid, slot);
        if (participant == nullptr && sTeamAssignmentsCaptured && slot < playerCount &&
            sTeamByAid[aid][slot] != 0xFF) {
            participant = AddParticipant(aid, slot, sTeamByAid[aid][slot], scores[slot]);
        }
        if (participant != nullptr && !participant->disconnected) participant->score = scores[slot];
    }
}

void OnPlayerDisconnect(u8 aid) {
    if (!sActive || !sTeamFormat || aid >= 12 || SectionMgr::sInstance == nullptr ||
        SectionMgr::sInstance->sectionParams == nullptr) {
        return;
    }
    EnsureParticipants();
    const u8 raceInGP = SectionMgr::sInstance->sectionParams->onlineParams.currentRaceNumber % MOGI_GP_RACE_COUNT;
    for (u8 slot = 0; slot < 2; ++slot) {
        MogiParticipant* participant = FindParticipant(aid, slot);
        if (participant == nullptr || participant->disconnected || !participant->activeInGP) continue;
        participant->disconnected = true;
        participant->disconnectRaceInGP = raceInGP;
        ++sDisconnectCountInGP;
    }
    if (sDisconnectCountInGP >= 3) sResetRoomAfterGP = true;
}

u16 GetMissingTeamScore(u8 team, bool previous) {
    if (!sActive || !sTeamFormat) return 0;
    u16 score = 0;
    for (u8 i = 0; i < 12; ++i) {
        const MogiParticipant& participant = sParticipants[i];
        if (!participant.valid || !participant.disconnected || participant.team != team ||
            participant.presentOnResults) continue;
        score += previous ? participant.previousScore : participant.score;
    }
    return score;
}

bool IsFormatVoteActive() {
    return sActive && sFormatVoteActive;
}

bool IsFormatVoteResolved() {
    return sActive && sFormatVoteResolved;
}

void FinishFormatVote() {
    if (!sFormatVoteResolved) return;
    sFormatVoteActive = false;
}

u8 GetTeamForPlayer(u8 playerIdx) {
    if (!IsTeamFormat()) return playerIdx;

    const u8 currentPlayerIdx = playerIdx < 12 ? playerIdx : 0;
    CaptureTeamAssignments();

    u8 aid;
    u8 playerOnConsole;
    if (sTeamAssignmentsCaptured && GetPlayerIdentity(currentPlayerIdx, aid, playerOnConsole) &&
        sTeamByAid[aid][playerOnConsole] != 0xFF) {
        return sTeamByAid[aid][playerOnConsole];
    }
    return sTeamByPlayer[currentPlayerIdx];
}

void ApplyHostTeamAssignments(const u8* teams) {
    if (!IsTeamFormat() || teams == nullptr) return;

    const u8 teamCount = 12 / sPlayersPerTeam;
    for (u8 playerIdx = 0; playerIdx < 12; ++playerIdx) {
        if (teams[playerIdx] < teamCount) sTeamByPlayer[playerIdx] = teams[playerIdx];
    }

    ResetTeamAssignments();
    CaptureTeamAssignments();
    if (sTeamAssignmentsCaptured) {
        for (u8 i = 0; i < 12; ++i) {
            MogiParticipant& participant = sParticipants[i];
            if (!participant.valid || participant.aid >= 12 || participant.playerOnConsole >= 2) continue;
            const u8 team = sTeamByAid[participant.aid][participant.playerOnConsole];
            if (team != 0xFF) participant.team = team;
        }
    }
    EnsureParticipants();
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

static void ApplyFormat(u8 format) {
    if (format >= MOGI_FORMAT_COUNT) format = 0;
    sTeamFormat = format != 0;
    switch (format) {
        case 2:
            sPlayersPerTeam = 3;
            break;
        case 3:
            sPlayersPerTeam = 4;
            break;
        case 4:
            sPlayersPerTeam = 6;
            break;
        default:
            sPlayersPerTeam = 2;
            break;
    }

    u32 seed = sLobbySeed;
    for (u8 i = 0; i < 12; ++i) sTeamByPlayer[i] = i / sPlayersPerTeam;
    for (s32 i = 11; i > 0; --i) {
        seed = seed * 1664525 + 1013904223;
        const u8 swapIdx = static_cast<u8>(seed % static_cast<u32>(i + 1));
        const u8 team = sTeamByPlayer[i];
        sTeamByPlayer[i] = sTeamByPlayer[swapIdx];
        sTeamByPlayer[swapIdx] = team;
    }
    ResetTeamAssignments();
    UI::ExtendedTeamSelect::RandomizeTeamColors(sLobbySeed);
    sFormatVoteResolved = true;
    EnsureParticipants();
}

static void ResolveFormatVote() {
    if (!sFormatVoteActive || sFormatVoteResolved) return;

    u8 counts[MOGI_FORMAT_COUNT] = {};
    const RKNet::Controller* controller = RKNet::Controller::sInstance;
    if (controller == nullptr) return;
    const RKNet::ControllerSub& sub = controller->subs[controller->currentSub];
    for (u8 aid = 0; aid < 12; ++aid) {
        if ((sub.availableAids & (1 << aid)) == 0) continue;
        const u8 state = aid == sub.localAid ? sLocalFormatVoteState : sFormatVoteStates[aid];
        const u8 format = aid == sub.localAid ? sLocalFormatVote : sFormatVotes[aid];
        if (state == MOGI_FORMAT_VOTE_CAST && format < MOGI_FORMAT_COUNT) ++counts[format];
    }

    u8 tiedFormats[MOGI_FORMAT_COUNT];
    u8 tiedCount = 0;
    u8 highestCount = 0;
    for (u8 format = 0; format < MOGI_FORMAT_COUNT; ++format) {
        if (counts[format] > highestCount) {
            highestCount = counts[format];
            tiedCount = 0;
            tiedFormats[tiedCount++] = format;
        } else if (counts[format] == highestCount) {
            tiedFormats[tiedCount++] = format;
        }
    }
    Random random;
    const u8 result = tiedFormats[random.NextLimited(tiedCount)];
    ApplyFormat(result);
    sLocalFormatVoteState = MOGI_FORMAT_VOTE_RESOLVED;
    sLocalFormatVote = result;
}

static void TryResolveFormatVote() {
    if (!sFormatVoteActive || sFormatVoteResolved) return;
    const RKNet::Controller* controller = RKNet::Controller::sInstance;
    if (controller == nullptr) return;
    const RKNet::ControllerSub& sub = controller->subs[controller->currentSub];
    if (sub.localAid != sub.hostAid) return;

    for (u8 aid = 0; aid < 12; ++aid) {
        if ((sub.availableAids & (1 << aid)) == 0) continue;
        const u8 state = aid == sub.localAid ? sLocalFormatVoteState : sFormatVoteStates[aid];
        if (state == MOGI_FORMAT_VOTE_PENDING) return;
    }
    ResolveFormatVote();
}

void FillFormatVotePacket(u8& state, u8& format) {
    state = sLocalFormatVoteState;
    format = sLocalFormatVote;
}

void ReceiveFormatVotePacket(u8 aid, u8 state, u8 format) {
    if (!sActive || aid >= 12 || state > MOGI_FORMAT_VOTE_RESOLVED) return;
    const RKNet::Controller* controller = RKNet::Controller::sInstance;
    if (controller == nullptr) return;
    const RKNet::ControllerSub& sub = controller->subs[controller->currentSub];
    if (aid == sub.hostAid && state == MOGI_FORMAT_VOTE_RESOLVED &&
        format < MOGI_FORMAT_COUNT && !sFormatVoteResolved) {
        ApplyFormat(format);
        return;
    }
    if (!sFormatVoteActive) return;
    sFormatVoteStates[aid] = state;
    sFormatVotes[aid] = format;
    TryResolveFormatVote();
}

bool CastFormatVote(u8 format) {
    if (!sFormatVoteActive || sFormatVoteResolved || format >= MOGI_FORMAT_COUNT ||
        sLocalFormatVoteState != MOGI_FORMAT_VOTE_PENDING) return false;
    sLocalFormatVote = format;
    sLocalFormatVoteState = MOGI_FORMAT_VOTE_CAST;
    TryResolveFormatVote();
    return true;
}

void OnFormatVoteTimeout() {
    if (!sFormatVoteActive || sFormatVoteResolved ||
        sLocalFormatVoteState != MOGI_FORMAT_VOTE_PENDING) return;
    sLocalFormatVoteState = MOGI_FORMAT_VOTE_TIMED_OUT;
    TryResolveFormatVote();
}

static void SelectLobbyFormat(u32 groupId) {
    if (sActive) return;

    ResetTeamAssignments();
    sLobbyGroupId = groupId;
    sLobbySeed = groupId ^ 0x4D4F4749;
    sTeamFormat = false;
    sPlayersPerTeam = 2;
    ResetFormatVotes();
    ResetParticipants();
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
    CaptureTeamAssignments();
    EnsureParticipants();
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
    CaptureTeamAssignments();
    EnsureParticipants();
    sPendingDisconnect = false;
    sResultsSectionSeen = false;
    sStartReported = false;
    System::sInstance->netMgr.racesPerGP = MOGI_RACE_COUNT - 1;
}

static u16 GetTeamScore(const RacedataScenario& scenario, u8 team) {
    u16 score = GetMissingTeamScore(team, false);
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

static bool IsReducedLossEligible(const RacedataScenario& scenario, u8 playerIdx) {
    if (!IsTeamFormat() || playerIdx >= scenario.playerCount) return false;

    const u16 playerScore = scenario.players[playerIdx].score;
    if (playerScore < MOGI_REDUCED_LOSS_MIN_SCORE) return false;

    const u8 playerTeam = GetTeamForPlayer(playerIdx);
    for (u8 teammateIdx = 0; teammateIdx < scenario.playerCount; ++teammateIdx) {
        if (teammateIdx == playerIdx || GetTeamForPlayer(teammateIdx) != playerTeam) continue;

        const u16 teammateScore = scenario.players[teammateIdx].score;
        if (static_cast<u32>(teammateScore) * MOGI_REDUCED_LOSS_SCORE_RATIO <= playerScore) return true;
    }
    return false;
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

static float GetMMRDelta(float mmr, float performance, float expectedPerformance, bool reducedLossEligible) {
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
        if (reducedLossEligible) delta *= MOGI_REDUCED_LOSS_FACTOR;
    }
    return delta;
}

static u8 GetFinalRaceNumber() {
    if (System::sInstance == nullptr) return MOGI_RACE_COUNT - 1;
    return System::sInstance->netMgr.racesPerGP;
}

static bool IsFinalRace() {
    if (SectionMgr::sInstance == nullptr || SectionMgr::sInstance->sectionParams == nullptr) return false;

    // The online race counter is advanced by the online track-selection flow. The race scenario's
    // race number can still contain the value from a previous GP when the next race is prepared.
    return SectionMgr::sInstance->sectionParams->onlineParams.currentRaceNumber >= GetFinalRaceNumber();
}

static void UpdateDisconnectScores(const RacedataScenario& scenario) {
    if (!IsTeamFormat() || SectionMgr::sInstance == nullptr || SectionMgr::sInstance->sectionParams == nullptr) return;
    EnsureParticipants();
    const u8 raceInGP = SectionMgr::sInstance->sectionParams->onlineParams.currentRaceNumber % MOGI_GP_RACE_COUNT;

    for (u8 i = 0; i < 12; ++i) {
        MogiParticipant& participant = sParticipants[i];
        if (!participant.valid) continue;
        if (!participant.activeInGP) {
            participant.presentOnResults = false;
            continue;
        }
        bool present = false;
        u16 resultScore = participant.score;
        u16 resultPreviousScore = participant.score;
        for (u8 playerIdx = 0; playerIdx < scenario.playerCount; ++playerIdx) {
            u8 aid;
            u8 playerOnConsole;
            if (!GetPlayerIdentity(playerIdx, aid, playerOnConsole) || aid != participant.aid ||
                playerOnConsole != participant.playerOnConsole) {
                continue;
            }
            present = true;
            resultScore = scenario.players[playerIdx].score;
            resultPreviousScore = scenario.players[playerIdx].previousScore;
            break;
        }
        participant.presentOnResults = present;

        participant.previousScore = participant.score;
        if (!participant.disconnected) {
            if (present) {
                participant.previousScore = resultPreviousScore;
                participant.score = resultScore;
            }
            continue;
        }

        if (participant.disconnectRaceInGP == 0) {
            if (!participant.fixedDisconnectScore) {
                participant.score = participant.gpStartScore + (present ? 15 : 18);
                participant.fixedDisconnectScore = true;
            }
        } else if (present) {
            participant.previousScore = resultPreviousScore;
            participant.score = resultScore;
        } else {
            participant.score += 3;
        }
    }

    if (raceInGP != MOGI_GP_RACE_COUNT - 1) return;
    if (sResetRoomAfterGP && RKNet::Controller::sInstance != nullptr) RKNet::Controller::sInstance->ResetRH1andROOM();
    for (u8 i = 0; i < 12; ++i) {
        MogiParticipant& participant = sParticipants[i];
        if (!participant.valid) continue;
        if (participant.disconnected) {
            participant.activeInGP = false;
        } else {
            participant.gpStartScore = participant.score;
            participant.previousScore = participant.score;
        }
    }
    sDisconnectCountInGP = 0;
    sResetRoomAfterGP = false;
}

void OnFinalResults() {
    if (Racedata::sInstance == nullptr) return;

    RacedataScenario& raceScenario = Racedata::sInstance->racesScenario;
    const RacedataScenario& scenario = Racedata::sInstance->menusScenario;
    if (!IsActive()) return;
    UpdateDisconnectScores(scenario);

    // WiFiVSResults::SetCupPanes only handles private VS/private battle modes.
    raceScenario.settings.gamemode = MODE_PRIVATE_VS;
    if (sMMRFinalized) return;

    if (!IsFinalRace()) return;

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
        const bool reducedLossEligible = IsReducedLossEligible(scenario, i);
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
                               currentMMR + GetMMRDelta(currentMMR, performance, expectedPerformance,
                                                        reducedLossEligible));
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
        if (!IsFinalRace()) return;
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
