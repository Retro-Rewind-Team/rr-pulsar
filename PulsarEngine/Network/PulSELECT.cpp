#include <kamek.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <MarioKartWii/Archive/ArchiveMgr.hpp>
#include <PulsarSystem.hpp>
#include <core/rvl/os/OS.hpp>
#include <Gamemodes/KO/KOMgr.hpp>
#include <Network/GPReport.hpp>
#include <Network/Network.hpp>
#include <Network/PacketExpansion.hpp>
#include <Network/PulSELECT.hpp>
#include <Network/Rating/PlayerRating.hpp>
#include <CustomCharacters/CustomCharacters.hpp>
#include <Settings/Settings.hpp>
#include <SlotExpansion/CupsConfig.hpp>
#include <MarioKartWii/RKSYS/RKSYSMgr.hpp>
#include <MarioKartWii/Race/RaceData.hpp>

namespace Pulsar {
namespace Network {

static bool IsRegionalRoom(RKNet::RoomType roomType) {
    return roomType == RKNet::ROOMTYPE_VS_REGIONAL || roomType == RKNet::ROOMTYPE_JOINING_REGIONAL;
}

void BeforeSELECTSend(RKNet::PacketHolder<PulSELECT>* packetHolder, PulSELECT* src, u32 len) {  // len is sizeof(RKNet::SELECTPacket) by default
    const System* system = System::sInstance;

    const ExpSELECTHandler& handler = ExpSELECTHandler::Get();
    const bool isBattle = (handler.mode == RKNet::ONLINEMODE_PUBLIC_BATTLE || handler.mode == RKNet::ONLINEMODE_PRIVATE_BATTLE);
    const RKSYS::Mgr* rksys = RKSYS::Mgr::sInstance;

    float rating;
    if (rksys) {
        u32 licenseId = rksys->curLicenseId;
        if (isBattle)
            rating = PointRating::GetUserBR(licenseId);
        else
            rating = PointRating::GetUserVR(licenseId);

        float decimal = rating - (int)rating;
        src->decimalVR[0] = (u8)(decimal * 100.0f + 0.5f);
        if (System::sInstance->IsContext(PULSAR_VR) && !System::sInstance->IsContext(PULSAR_MODE_KO)) {
            src->playersData[0].sumPoints = static_cast<u16>(rating);
        }
    } else {
        src->decimalVR[0] = 0;
    }
    src->decimalVR[1] = 0;

    const RKNet::Controller* controller = RKNet::Controller::sInstance;
    if (controller->subs[controller->currentSub].localPlayerCount == 2) {
        const SectionParams* sectionParams = SectionMgr::sInstance->sectionParams;
        src->playersData[1].character = static_cast<u8>(sectionParams->characters[1]);
        src->playersData[1].kart = static_cast<u8>(sectionParams->karts[1]);
        src->playersData[1].sumPoints = 0;
        const Racedata* racedata = Racedata::sInstance;
        if (racedata != nullptr) {
            const RacedataScenario& menuScenario = racedata->menusScenario;
            const u8 guestPlayerId = menuScenario.settings.hudPlayerIds[1];
            if (guestPlayerId < 12) {
                src->playersData[1].sumPoints = menuScenario.players[guestPlayerId].score;
            }
        }
        src->playersData[1].starRank = 0;
    }

    const Network::Mgr& netMgr = system->netMgr;
    const u32 blockingCount = system->GetInfo().GetTrackBlocking();
    const u32 writeCount = (blockingCount < MAX_TRACK_BLOCKING) ? blockingCount : MAX_TRACK_BLOCKING;
    src->blockedTrackCount = static_cast<u8>(writeCount);
    src->curBlockingArrayIdx = netMgr.curBlockingArrayIdx;
    src->lastGroupedTrackPlayed = netMgr.lastGroupedTrackPlayed;
    for (u32 i = 0; i < writeCount; ++i) {
        src->blockedTracks[i] = (netMgr.lastTracks != nullptr) ? static_cast<u16>(netMgr.lastTracks[i]) : 0xFFFF;
    }
    for (u32 i = writeCount; i < MAX_TRACK_BLOCKING; ++i) {
        src->blockedTracks[i] = 0xFFFF;
    }
    if (!system->IsContext(PULSAR_CT)) {
        const u8 vanillaWinning = CupsConfig::ConvertTrack_PulsarIdToRealId(static_cast<PulsarId>(src->pulWinningTrack));
        src->winningCourse = vanillaWinning;
        const u8 vanillaVote = CupsConfig::ConvertTrack_PulsarIdToRealId(static_cast<PulsarId>(src->pulVote));
        src->playersData[0].courseVote = vanillaVote;
        src->playersData[1].courseVote = vanillaVote;
    } else
        len = sizeof(PulSELECT);
    packetHolder->Copy(src, len);

    packetHolder->packet->characterTables = CustomCharacters::GetLocalOnlineCharacterTables();
}
kmCall(0x80661040, BeforeSELECTSend);

static void AfterSELECTReception(PulSELECT* unused, PulSELECT* src, u32 len) {
    register ExpSELECTHandler* handler;
    asm(mr handler, r18;);
    register u8 aid;
    asm(mr aid, r19;);
    register RKNet::PacketHolder<PulSELECT>* holder;
    asm(mr holder, r27);

    const u16 characterTables = (holder != nullptr && holder->packetSize == sizeof(PulSELECT)) ? src->characterTables : 0;
    CustomCharacters::UpdateOnlineCharacterTablesFromAid(aid, src->playerIdToAid, characterTables);

    for (int i = 0; i < 2; ++i) {
        PointRating::remoteDecimalVR[aid][i] = src->decimalVR[i];
    }

    PulSELECT& dest = handler->receivedPackets[aid];
    if (holder != nullptr && holder->packetSize == sizeof(RKNet::SELECTPacket)) {
        const u16 pulWinning = CupsConfig::ConvertTrack_RealIdToPulsarId(static_cast<CourseId>(src->winningCourse));
        src->pulWinningTrack = pulWinning;  // this is safe because src is a ptr to the buffer of holder which is always big enough
        const u16 pulVote = CupsConfig::ConvertTrack_RealIdToPulsarId(static_cast<CourseId>(src->playersData[0].courseVote));
        src->pulVote = pulVote;
        src->voteVariantIdx[0] = 0;
        src->voteVariantIdx[1] = 0;
        src->blockedTrackCount = 0;
        src->curBlockingArrayIdx = 0;
        src->lastGroupedTrackPlayed = false;
        src->characterTables = 0;
        for (u32 i = 0; i < MAX_TRACK_BLOCKING; ++i) {
            src->blockedTracks[i] = 0xFFFF;
        }
    }

    System* system = System::sInstance;
    if (system != nullptr && holder != nullptr && holder->packetSize == sizeof(PulSELECT)) {
        Network::Mgr& netMgr = system->netMgr;
        const u32 localBlockingCount = system->GetInfo().GetTrackBlocking();

        if (localBlockingCount > 0 && netMgr.lastTracks != nullptr && src->blockedTrackCount > 0) {
            u32 localCount = 0;
            for (u32 i = 0; i < localBlockingCount; ++i) {
                if (netMgr.lastTracks[i] != PULSARID_NONE) localCount++;
            }

            u32 srcCount = 0;
            const u32 checkCount = (src->blockedTrackCount < localBlockingCount) ? src->blockedTrackCount : localBlockingCount;
            for (u32 i = 0; i < checkCount; ++i) {
                if (src->blockedTracks[i] != 0xFFFF) srcCount++;
            }

            bool shouldSync = false;
            const RKNet::Controller* controller = RKNet::Controller::sInstance;
            if (controller != nullptr) {
                const RKNet::ControllerSub& sub = controller->subs[controller->currentSub];
                if (sub.localAid == sub.hostAid) {
                    if (srcCount > localCount) shouldSync = true;
                } else {
                    if (aid == sub.hostAid) {
                        if (srcCount >= localCount) shouldSync = true;
                    } else {
                        if (localCount == 0 && srcCount > 0) shouldSync = true;
                    }
                }
            }

            if (shouldSync) {
                const u32 copyCount = (src->blockedTrackCount < localBlockingCount) ? src->blockedTrackCount : localBlockingCount;
                for (u32 i = 0; i < copyCount; ++i) {
                    netMgr.lastTracks[i] = static_cast<PulsarId>(src->blockedTracks[i]);
                }
                netMgr.curBlockingArrayIdx = src->curBlockingArrayIdx % localBlockingCount;
                netMgr.lastGroupedTrackPlayed = src->lastGroupedTrackPlayed;
            }
        }
    }

    memcpy(&dest, src, sizeof(PulSELECT));
}
kmCall(0x80661130, AfterSELECTReception);

u8 ExpSELECTHandler::GetVoteVariantIdx(u8 aid, u8 hudSlotId) const {
    RKNet::Controller* controller = RKNet::Controller::sInstance;
    RKNet::ControllerSub& sub = controller->subs[controller->currentSub];

    if (aid == sub.localAid) {
        return this->toSendPacket.voteVariantIdx[hudSlotId];
    } else {
        return this->receivedPackets[aid].voteVariantIdx[hudSlotId];
    }
}

static u8 GetEngineClass(const ExpSELECTHandler& select) {
    if (select.toSendPacket.phase != 0) return select.toSendPacket.engineClass;
    return 0;
}
kmBranch(0x8066048c, GetEngineClass);

static u16 GetWinningCourse(const ExpSELECTHandler& select) {
    if (select.toSendPacket.phase == 2)
        return select.toSendPacket.pulWinningTrack;
    else
        return 0xFF;
}
kmBranch(0x80660450, GetWinningCourse);

PulsarId FixRandom(Random& random) {
    return CupsConfig::sInstance->RandomizeTrack();
}
kmCall(0x80661f34, FixRandom);

static bool IsGroupedTrack(PulsarId id) {
    if (CupsConfig::IsReg(id)) return false;
    const u32 idx = id - 0x100;
    switch (idx) {
        case 6:
        case 9:
        case 27:
        case 29:
        case 31:
        case 32:
        case 37:
        case 51:
        case 57:
        case 61:
        case 63:
        case 67:
        case 73:
        case 76:
        case 77:
        case 85:
            return true;
        default:
            if (idx >= 88 && idx <= 103) return true;
            return false;
    }
}

static bool IsTrackBlocked(const System& system, PulsarId trackId) {
    const u32 blockingCount = system.GetInfo().GetTrackBlocking();
    if (blockingCount == 0 || system.netMgr.lastTracks == nullptr) return false;

    for (u32 i = 0; i < blockingCount; ++i) {
        if (system.netMgr.lastTracks[i] == trackId) return true;
    }

    if (IsGroupedTrack(trackId) && IsRegionalRoom(RKNet::Controller::sInstance->roomType)) {
        const u32 lastIdx = (system.netMgr.curBlockingArrayIdx + blockingCount - 1) % blockingCount;
        if (IsGroupedTrack(system.netMgr.lastTracks[lastIdx])) return true;
    }

    return false;
}

PulsarId RandomizeHAWTrack(const System& system, const CupsConfig& cupsConfig) {
    PulsarId trackId;
    do {
        trackId = cupsConfig.RandomizeTrack();
    } while (IsTrackBlocked(system, trackId));
    return trackId;
}

void StoreBlockedTrack(System& system, PulsarId trackId) {
    const u32 blockingCount = system.GetInfo().GetTrackBlocking();
    if (blockingCount == 0 || system.netMgr.lastTracks == nullptr) return;

    system.netMgr.lastTracks[system.netMgr.curBlockingArrayIdx] = trackId;
    system.netMgr.curBlockingArrayIdx = (system.netMgr.curBlockingArrayIdx + 1) % blockingCount;
    system.netMgr.lastGroupedTrackPlayed = IsGroupedTrack(trackId);
}

void ExpSELECTHandler::DecideTrack(ExpSELECTHandler& self) {
    Random random;
    System* system = System::sInstance;
    const CupsConfig* cupsConfig = CupsConfig::sInstance;
    RKNet::Controller* controller = RKNet::Controller::sInstance;
    RKNet::ControllerSub& sub = controller->subs[controller->currentSub];
    const u8 hostAid = controller->subs[controller->currentSub].hostAid;
    const RKNet::OnlineMode mode = self.mode;
    const RKNet::RoomType roomType = controller->roomType;
    const bool isFriendRoom = roomType == RKNet::ROOMTYPE_FROOM_HOST || roomType == RKNet::ROOMTYPE_FROOM_NONHOST;
    const bool isFriendRoomVS = isFriendRoom && (mode == RKNet::ONLINEMODE_PRIVATE_VS || mode == RKNet::ONLINEMODE_PUBLIC_VS);

    if (mode == RKNet::ONLINEMODE_PRIVATE_VS && system->IsContext(PULSAR_MODE_KO)) system->koMgr->PatchAids(sub);

    if (isFriendRoomVS && system->IsContext(PULSAR_HAW)) {
        self.toSendPacket.winningVoterAid = hostAid;
        u16 hostVote = self.toSendPacket.pulVote;
        bool hostVotedRandom = (hostVote == 0xFF);
        if (hostVotedRandom) hostVote = RandomizeHAWTrack(*system, *cupsConfig);
        self.toSendPacket.pulWinningTrack = hostVote;  // If host voted random, also randomize the variant
        if (hostVotedRandom) {
            self.toSendPacket.variantIdx = cupsConfig->RandomizeVariant(static_cast<PulsarId>(hostVote));
        } else {
            self.toSendPacket.variantIdx = cupsConfig->GetCurVariantIdx();
        }
        if (sub.localAid == hostAid) StoreBlockedTrack(*system, static_cast<PulsarId>(hostVote));
    } else {
        const bool isCT = system->IsContext(PULSAR_CT);
        const u32 availableAids = sub.availableAids;  // has been modified to remove KO'd player if KO is on
        u8 aids[12];
        u8 newVotesAids[12];  // only used for track blocking
        PulsarId votes[12];
        bool votedRandom[12];
        int playerCount = 0;
        int newVoters = 0;
        for (u8 aid = 0; aid < 12; ++aid) {
            if (((1 << aid) & availableAids) == 0) continue;
            aids[playerCount] = aid;
            ++playerCount;

            PulsarId aidVote = static_cast<PulsarId>(aid == sub.localAid ? self.toSendPacket.pulVote : self.receivedPackets[aid].pulVote);
            votedRandom[aid] = (aidVote == 0xFF);
            if (aidVote == 0xFF) {
                if (isCT)
                    aidVote = cupsConfig->RandomizeTrack();
                else {
                    const bool isVS = (mode == RKNet::ONLINEMODE_PRIVATE_VS || mode == RKNet::ONLINEMODE_PUBLIC_VS);
                    const u32 trackCount = isVS ? 32 : 10;
                    u32 next = random.NextLimited(trackCount);
                    const CourseId prev = Racedata::sInstance->racesScenario.settings.courseId;
                    if (next == prev) {  // prevent repeats
                        const u32 offsetTrick = trackCount - 1;
                        const u32 offset = random.NextLimited(trackCount - 1);
                        next = offset + next + 1;
                        if (offsetTrick < next) next -= offsetTrick - 1;
                    }
                    if (isVS) next += trackCount;  // add 32 to match battle ids
                    aidVote = static_cast<PulsarId>(next);
                }
            }
            votes[aid] = aidVote;
            if (!IsTrackBlocked(*system, aidVote)) {
                newVotesAids[newVoters] = aid;
                ++newVoters;
            }
        }
        u8 winner;
        if (newVoters > 0)
            winner = newVotesAids[random.NextLimited(newVoters)];
        else
            winner = aids[random.NextLimited(playerCount)];
        PulsarId vote = static_cast<PulsarId>(votes[winner]);
        self.toSendPacket.winningVoterAid = winner;
        self.toSendPacket.pulWinningTrack = vote;

        u8 winnerVariant = 0;
        if (votedRandom[winner]) {
            winnerVariant = cupsConfig->RandomizeVariant(vote);
        } else if (winner == sub.localAid) {
            winnerVariant = self.toSendPacket.voteVariantIdx[0];
        } else {
            winnerVariant = self.receivedPackets[winner].voteVariantIdx[0];
        }
        self.toSendPacket.variantIdx = winnerVariant;

        StoreBlockedTrack(*system, vote);

        ReportU32(
            "wl:mkw_select_course", static_cast<u32>(vote));
        ReportU32("wl:mkw_select_cc", static_cast<u32>(GetEngineClass(self)));
    }
}
kmCall(0x80661490, ExpSELECTHandler::DecideTrack);

// Patches GetWinningCOURSE call so that non-hosts prepare the correct track
CourseId SetCorrectSlot(ExpSELECTHandler* select) {
    CourseId id = reinterpret_cast<RKNet::SELECTHandler*>(select)->GetWinningCourse();
    if (select->toSendPacket.engineClass != 0) id = CupsConfig::sInstance->GetCorrectTrackSlot();
    const System* system = System::sInstance;
    if (system->IsContext(PULSAR_MODE_KO) && system->koMgr->isSpectating) Racedata::sInstance->menusScenario.settings.gametype = GAMETYPE_ONLINE_SPECTATOR;
    return id;
}
kmCall(0x80650ea8, SetCorrectSlot);

static void SetCorrectTrack(ArchiveMgr* root, PulsarId winningCourse) {
    CupsConfig* cupsConfig = CupsConfig::sInstance;
    System* system = System::sInstance;
    RKNet::Controller* controller = RKNet::Controller::sInstance;
    RKNet::ControllerSub& sub = controller->subs[controller->currentSub];
    Network::ExpSELECTHandler& handler = Network::ExpSELECTHandler::Get();
    const Network::PulSELECT* select;
    const u8 hostAid = sub.hostAid;
    const bool isHost = (hostAid == sub.localAid);
    if (isHost)
        select = &handler.toSendPacket;
    else
        select = &handler.receivedPackets[hostAid];

    if (!isHost) {
        const u32 blockingCount = system->GetInfo().GetTrackBlocking();
        if (blockingCount != 0 && system->netMgr.lastTracks != nullptr) {
            const u32 writeIdx = system->netMgr.curBlockingArrayIdx;
            const u32 prevIdx = (writeIdx + blockingCount - 1) % blockingCount;
            if (system->netMgr.lastTracks[prevIdx] != winningCourse) {
                system->netMgr.lastTracks[writeIdx] = winningCourse;
                system->netMgr.curBlockingArrayIdx = (writeIdx + 1) % blockingCount;
                system->netMgr.lastGroupedTrackPlayed = IsGroupedTrack(winningCourse);
            }
        }
    }

    cupsConfig->SetWinning(winningCourse, select->variantIdx);
    root->RequestLoadCourseAsync(static_cast<CourseId>(winningCourse));
}
kmCall(0x80644414, SetCorrectTrack);

// Overwrites CC rules -> 10% 100, 65% 150, 25% mirror and/or in frooms, overwritten by host setting
static void DecideCC(ExpSELECTHandler& handler) {
    const u8 ccSetting = Settings::Mgr::Get().GetUserSettingValue(Settings::SETTINGSTYPE_FROOM1, RADIO_FROOMCC);
    RKNet::Controller* controller = RKNet::Controller::sInstance;
    const RKNet::RoomType roomType = controller->roomType;
    u8 ccClass = 1;  // 1 100, 2 150, 3 mirror
    const bool isRegional = IsRegionalRoom(roomType);
    const bool isWorldWide = roomType == RKNet::ROOMTYPE_VS_WW || roomType == RKNet::ROOMTYPE_JOINING_WW;
    const bool force200 = isRegional && System::sInstance->IsContext(PULSAR_200_WW);
    const bool forceOtt = isRegional && System::sInstance->IsContext(PULSAR_MODE_OTT);
    if (isRegional || isWorldWide || forceOtt || (roomType == RKNet::ROOMTYPE_FROOM_HOST && ccSetting == HOSTCC_NORMAL)) {
        Random random;
        const u32 result = random.NextLimited(100);  // 25
        System* system = System::sInstance;
        u32 prob100 = system->GetInfo().GetProb100();  // 100
        u32 prob150 = system->GetInfo().GetProb150();  // 00
        if (result < 100 - (prob100 + prob150))
            ccClass = 3;
        else if (result < 100 - prob100)
            ccClass = 2;
    }
    if (force200)
        ccClass = 1;
    else if (roomType == RKNet::ROOMTYPE_FROOM_HOST && ccSetting == HOSTCC_150)
        ccClass = 2;
    else if (roomType == RKNet::ROOMTYPE_FROOM_HOST && (ccSetting == HOSTCC_500 || ccSetting == HOSTCC_100))
        ccClass = 1;
    handler.toSendPacket.engineClass = ccClass;
}
kmCall(0x80661404, DecideCC);

void* Get() {
    register u8 aid;
    asm(mr aid, r29;);
    register ExpSELECTHandler* select;
    asm(mr select, r28;);

    return reinterpret_cast<u8*>(&select->receivedPackets[aid]) - 0x40;
}
kmCall(0x80661340, Get);

asmFunc PatchImport() {  // r18 = handler
    ASM(
        nofralloc;
        mulli r0, r19, sizeof(PulSELECT);
        lwz r6, ExpSELECTHandler.receivedPackets(r18);
        add r6, r6, r0;
        subi r6, r6, 0x40;
        blr;)
}
kmCall(0x80661140, PatchImport);

GetRecvPulSELECTPacket(0x80660508);
GetRecvPulSELECTPacket(0x80660558);
GetRecvPulSELECTPacket(0x806605f4);
GetRecvPulSELECTPacket(0x8066063c);

u16 GetTrack(const ExpSELECTHandler& handler, u8 aid, u8 hudSlotId, register void* subR6) {
    register RKNet::ControllerSub* sub;
    asm(addi sub, subR6, 0x38);
    if (sub->localAid == aid)
        return handler.toSendPacket.pulVote;
    else
        return handler.receivedPackets[aid].pulVote;
}
kmBranch(0x80660574, GetTrack);

kmWrite32(0x8066141c, 0xA01C0000 + offsetof(ExpSELECTHandler, toSendPacket) + offsetof(PulSELECT, pulVote));

kmWrite32(0x80661e90, 0xA01F0000 + offsetof(ExpSELECTHandler, toSendPacket) + offsetof(PulSELECT, pulVote));  // extsb -> lhz
asmFunc PatchDecide() {  // r31 = handler
    ASM(
        mulli r0, r4, sizeof(PulSELECT);
        lwz r3, ExpSELECTHandler.receivedPackets(r31);
        add r3, r3, r0;
        lhz r0, PulSELECT.pulVote(r3);)
}
kmCall(0x80661ef0, PatchDecide);

void InitPatch() {
    register ExpSELECTHandler* select;
    asm(mr select, r31;);
    select->toSendPacket.pulVote = 0x43;
    select->toSendPacket.pulWinningTrack = 0xff;
    select->toSendPacket.acVerifyTag = 0;
    const Settings::Mgr& settings = Settings::Mgr::Get();
    bool allowChangeCombo;
    const RKNet::Controller* controller = RKNet::Controller::sInstance;
    if (controller->roomType == RKNet::ROOMTYPE_VS_REGIONAL)
        allowChangeCombo = true;
    else
        allowChangeCombo = settings.GetUserSettingValue(Settings::SETTINGSTYPE_OTT, RADIO_OTTALLOWCHANGECOMBO);
    select->toSendPacket.allowChangeComboStatus = allowChangeCombo;
    select->toSendPacket.koPerRace = settings.GetUserSettingValue(Settings::SETTINGSTYPE_KO, SCROLLER_KOPERRACE) + 1;
    select->toSendPacket.racesPerKO = settings.GetUserSettingValue(Settings::SETTINGSTYPE_KO, SCROLLER_RACESPERKO) + 1;
    select->toSendPacket.alwaysFinal = settings.GetUserSettingValue(Settings::SETTINGSTYPE_KO, RADIO_KOFINAL) == KOSETTING_FINAL_ALWAYS;
    select->toSendPacket.singleRace1v1Final = settings.GetUserSettingValue(Settings::SETTINGSTYPE_KO, RADIO_KO1V1FINALE) == KOSETTING_1V1FINALE_SINGLE;
    const u8 elimThreshold = settings.GetUserSettingValue(Settings::SETTINGSTYPE_KO, SCROLLER_KOELIMTHRESHOLD);
    select->toSendPacket.elimThresholdPlayers = elimThreshold == KOSETTING_ELIMTHRESHOLD_DISABLED ? 0 : static_cast<u8>(elimThreshold + 2);
    select->toSendPacket.elimChangeCount = settings.GetUserSettingValue(Settings::SETTINGSTYPE_KO, SCROLLER_KOELIMCHANGE) + 1;
    for (int aid = 0; aid < 12; ++aid) {
        PulSELECT& cur = select->receivedPackets[aid];
        cur.pulVote = 0x43;
        cur.pulWinningTrack = 0xff;
        cur.acVerifyTag = 0;
        reinterpret_cast<RKNet::SELECTHandler*>(select)->ResetPacket(select->receivedPackets[aid]);
    }
}
kmCall(0x806600ec, InitPatch);
kmPatchExitPoint(InitPatch, 0x806601bc);

// SetPlayerData - stores the pulVote and voteVariantIdx for each player
// Original function at 0x80660750:
//   r3=handler, r4=character, r5=kart, r6=courseVote, r7=hudSlotId, r8=starRank
// We need to store pulVote (r6 extended to u16) then let original continue
asmFunc SetPlayerDataPatch() {
    ASM(
        nofralloc;
        // Original first instruction: rlwinm r0, r7, 3, 0, 28
        rlwinm r0, r7, 3, 0, 28;
        // Store the u16 pulVote (courseVote from r6)
        sth r6, ExpSELECTHandler.toSendPacket + PulSELECT.pulVote(r3);
        blr;)
}
kmBranch(0x80660750, SetPlayerDataPatch);
kmPatchExitPoint(SetPlayerDataPatch, 0x80660754);

static void StoreVoteVariantAfterSetPlayerData() {
    const CupsConfig* cupsConfig = CupsConfig::sInstance;
    u8 variantIdx = cupsConfig ? cupsConfig->GetCurVariantIdx() : 0;

    // Get hudSlotId from r27 which contains the loop index
    register u32 hudSlotId;
    asm(mr hudSlotId, r27;);

    ExpSELECTHandler& handler = ExpSELECTHandler::Get();
    if (hudSlotId < 2) {
        handler.toSendPacket.voteVariantIdx[hudSlotId] = variantIdx;
    }

    // Execute the replaced instruction: addi r28, r28, 0xc
    register u32 r28_val;
    asm(mr r28_val, r28;);
    r28_val += 0xc;
    asm(mr r28, r28_val;);
}
kmCall(0x80643758, StoreVoteVariantAfterSetPlayerData);
kmCall(0x806437ac, StoreVoteVariantAfterSetPlayerData);

// Fixes
kmWrite32(0x806440c0, 0x2c030100);  // if id >= 0x100 or id <= 31 -> correct courseId
kmWrite32(0x806440c8, 0x40a00020);
kmWrite32(0x8064411c, 0x2c0300FF);  // cmpwi 0xFFFF -> 0xFF for battle
kmWrite32(0x80644150, 0x386000ff);  // li r3, 0xFF for battle
kmWrite32(0x80644154, 0x2c0300FF);  // cmpwi 0xFFFF -> 0xFF for battle
kmWrite32(0x80644338, 0x2C0300FF);  // cmpwi 0xFFFF -> 0xFF
kmWrite32(0x8064433c, 0x418200dc);

// Winning course u8->u16
kmWrite32(0x80661480, 0xA01C0000 + offsetof(ExpSELECTHandler, toSendPacket) + offsetof(PulSELECT, pulWinningTrack));  // extsb -> lhz
kmWrite32(0x80661484, 0x2c0000ff);  // cmpwi 0xFFFF -> 0xFF

kmWrite32(0x80661f0c, 0xA01F0000 + offsetof(ExpSELECTHandler, toSendPacket) + offsetof(PulSELECT, pulWinningTrack));  // extsb -> lhz
kmWrite32(0x80661f10, 0x2c0000ff);  // cmpwi 0xFFFF -> 0xFF

kmWrite32(0x80660018, 0x386000ff);
kmWrite32(0x80660020, 0xB07F0000 + offsetof(ExpSELECTHandler, toSendPacket) + offsetof(PulSELECT, pulWinningTrack));
kmWrite32(0x80661e94, 0xB01F0000 + offsetof(ExpSELECTHandler, toSendPacket) + offsetof(PulSELECT, pulWinningTrack));
kmWrite32(0x80661ef4, 0xB01F0000 + offsetof(ExpSELECTHandler, toSendPacket) + offsetof(PulSELECT, pulWinningTrack));
kmWrite32(0x80661f94, 0xB3DF0000 + offsetof(ExpSELECTHandler, toSendPacket) + offsetof(PulSELECT, pulWinningTrack));
kmWrite32(0x8066200c, 0xB01F0000 + offsetof(ExpSELECTHandler, toSendPacket) + offsetof(PulSELECT, pulWinningTrack));

// Replaces the vanilla SELECT vote processing so extended votes and room settings
// progress through the same host/non-host phase checks.
void ProcessNewPacketVoting() {
    register ExpSELECTHandler* handler;
    asm(mr handler, r24;);
    const RKNet::Controller* controller = RKNet::Controller::sInstance;
    const RKNet::ControllerSub& sub = controller->subs[controller->currentSub];

    for (int aid = 0; aid < 12; ++aid) {
        const u8 localAid = sub.localAid;
        const u8 hostAid = sub.hostAid;
        const u32 aidBit = 1 << aid;
        const u32 localAidBit = 1 << localAid;
        const u32 availableAids = sub.availableAids;
        if ((aidBit & availableAids) == 0 || aid == localAid) continue;

        const PulSELECT& curRecv = handler->receivedPackets[aid];
        PulSELECT& send = handler->toSendPacket;
        const u8 myPhase = send.phase;
        if (hostAid == localAid) {  // I am the HOST, I process every single packet

            if (myPhase == 0) {  // check that every aid has the correct settings
                const u32 battleType = send.battleTypeAndTeams;
                if (battleType != 0) {
                    if (battleType == curRecv.battleTypeAndTeams) {
                        if (send.selectId == curRecv.selectId) {
                            if (send.engineClass == curRecv.engineClass) {
                                handler->aidsWithAccurateRaceSettings |= aidBit;
                            }
                        }
                    }
                }
                u32 accField = handler->aidsWithAccurateRaceSettings;
                if (accField != 0) {
                    if (battleType != 0) accField |= localAidBit;
                    if ((availableAids & accField) == availableAids) send.phase = 1;
                }
            } else if (myPhase == 1) {
                u16 winningTrack = send.pulWinningTrack;
                if (winningTrack != 0xff && winningTrack == curRecv.pulWinningTrack && send.winningVoterAid == curRecv.winningVoterAid) {
                    bool hasSameAidArr = true;
                    for (int i = 0; i < 12; ++i) {
                        if (send.playerIdToAid[i] != curRecv.playerIdToAid[i]) hasSameAidArr = false;
                    }
                    if (hasSameAidArr) handler->aidsWithAccurateAidPidMap |= aidBit;
                }
                u32 accField = handler->aidsWithAccurateAidPidMap;
                if (accField != 0) {
                    if (winningTrack != 0xff) accField |= localAidBit;
                    if ((availableAids & accField) == availableAids) send.phase = 2;
                }
            }
        } else if (hostAid == aid) {  // I'm not the host and the loop is at the hostAid
            if (myPhase == 0) {  // Copy the settings
                send.battleTypeAndTeams = curRecv.battleTypeAndTeams;
                send.selectId = curRecv.selectId;
                send.engineClass = curRecv.engineClass;
                if (curRecv.phase == 1) send.phase = 1;
            } else if (myPhase == 1) {
                u32 accField = handler->aidsThatHaveVoted;
                if (accField != 0) {
                    if (send.pulVote != 0x43) accField |= localAidBit;
                    if ((availableAids & accField) == availableAids) {
                        const u8 winningAid = curRecv.winningVoterAid;
                        const u16 winningTrack = curRecv.pulWinningTrack;
                        if (((1 << winningAid) & availableAids) == 0) {
                            handler->receivedPackets[winningAid].pulVote = winningTrack;  // if the winner is dcd, fallback
                        }
                        send.winningVoterAid = winningAid;
                        send.pulWinningTrack = winningTrack;
                        for (int i = 0; i < 12; ++i) {
                            send.playerIdToAid[i] = curRecv.playerIdToAid[i];
                        }
                    }
                }
                if (curRecv.phase > 1) send.phase = 2;
            }
        }

        bool isConnectedToAnyone = false;  // the game calls IsConnectedToAnyone again (it was called outside of the loop), presumably in case there was an interrupt?
        if ((localAidBit & sub.availableAids) != 0 && sub.connectionCount > 1) isConnectedToAnyone = true;

        u32 accField = handler->hasNewSELECT;
        if (isConnectedToAnyone) {
            accField |= localAidBit;
            if ((availableAids & accField) != availableAids) isConnectedToAnyone = false;
        }
        if (isConnectedToAnyone && curRecv.pulVote != 0x43) handler->aidsThatHaveVoted |= aidBit;
        if (handler->hasNewRACEHEADER_1 != 0 && send.phase >= 1) send.phase = 2;  // people have already progressed?
    }
}
kmCall(0x80661520, ProcessNewPacketVoting);
kmPatchExitPoint(ProcessNewPacketVoting, 0x80661920);

}  // namespace Network
}  // namespace Pulsar
