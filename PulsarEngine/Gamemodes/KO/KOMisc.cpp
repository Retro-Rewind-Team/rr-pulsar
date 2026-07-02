#include <MarioKartWii/RKNet/SELECT.hpp>
#include <MarioKartWii/RKNet/ITEM.hpp>
#include <MarioKartWii/3D/Camera/CameraMgr.hpp>
#include <MarioKartWii/3D/Camera/RaceCamera.hpp>
#include <MarioKartWii/UI/Page/Other/SELECTStageMgr.hpp>
#include <MarioKartWii/UI/Page/Other/VR.hpp>
#include <MarioKartWii/UI/Page/Other/Votes.hpp>
#include <MarioKartWii/UI/Page/Leaderboard/GPVSLeaderboardTotal.hpp>
#include <MarioKartWii/UI/Page/Other/WifiVSResults.hpp>
#include <MarioKartWii/UI/Ctrl/CtrlRace/CtrlRaceRankNum.hpp>
#include <MarioKartWii/UI/Ctrl/CtrlRace/CtrlRaceItemWindow.hpp>
#include <MarioKartWii/Race/RaceInfo/RaceInfo.hpp>
#include <MarioKartWii/Kart/KartManager.hpp>
#include <Network/PacketExpansion.hpp>
#include <Gamemodes/KO/KOMgr.hpp>
#include <Gamemodes/LapKO/LapKOMgr.hpp>
#include <Gamemodes/KO/KORaceEndPage.hpp>
#include <Gamemodes/KO/KOWinnerPage.hpp>
#include <Gamemodes/PositionCounter.hpp>
#include <UI/Leaderboard/LeaderboardDisplay.hpp>

namespace Pulsar {
namespace KO {

static void EditLdb(CtrlRaceResult* result, u8 playerId) {
    UI::fillLeaderboardResult(*result, playerId);
}
kmCall(0x8085cc04, EditLdb);

static u8 EditPosTracker(CtrlRaceRankNum& posTracker) {
    const u32 playerId = posTracker.GetPlayerId();
    PositionCounter::UpdatePositionDisplay(posTracker);
    return playerId;
}
kmCall(0x807f4b64, EditPosTracker);

// Fixes for when spectating
static u8 ReturnCorrectId(u8 localId) {
    const System* system = System::sInstance;
    const RaceCameraMgr* cameraMgr = RaceCameraMgr::sInstance;
    if (system->IsContext(PULSAR_MODE_KO) && system->koMgr->isSpectating) {
        if (cameraMgr == nullptr) return 0;
        return cameraMgr->focusedPlayerIdx;
    }
    return localId;
}
kmBranch(0x80531f7c, ReturnCorrectId);

static GameType SyncCountdown(const Racedata& raceData) {
    GameType type = raceData.racesScenario.settings.gametype;
    const System* system = System::sInstance;
    const bool isKoSpectate = system->IsContext(PULSAR_MODE_KO) && system->koMgr->isSpectating;
    if (isKoSpectate && type == GAMETYPE_ONLINE_SPECTATOR) {
        type = GAMETYPE_DEFAULT;
        if (Racedata::sInstance != nullptr) {
            Racedata::sInstance->racesScenario.settings.gametype = GAMETYPE_DEFAULT;
        }
    }
    return type;
}
kmCall(0x806537d8, SyncCountdown);
kmWrite32(0x806537dc, 0x2c030000);
kmWrite32(0x806537e4, 0x2c030006);

static void PatchAidsBeforeSELECTStageMgrSetup(Pages::SELECTStageMgr& stageMgr) {
    const System* system = System::sInstance;

    const bool isKO = system->IsContext(PULSAR_MODE_KO);
    const bool isLapKO = system->IsContext(PULSAR_MODE_LAPKO);
    const bool needsSelectData = isKO || isLapKO;

    if (needsSelectData) {
        RKNet::Controller* controller = RKNet::Controller::sInstance;
        if (controller != nullptr) {
            RKNet::ControllerSub& sub = controller->subs[controller->currentSub];
            Network::ExpSELECTHandler& handler = Network::ExpSELECTHandler::Get();
            const Network::PulSELECT* select = nullptr;
            const u8 hostAid = sub.hostAid;
            if (hostAid == sub.localAid || hostAid >= 12) {
                select = &handler.toSendPacket;
            } else {
                select = &handler.receivedPackets[hostAid];
            }

            if (isKO && system->koMgr != nullptr && select != nullptr) {
                Mgr* mgr = system->koMgr;
                mgr->koPerRace = select->koPerRace;
                mgr->racesPerKO = select->racesPerKO;
                mgr->alwaysFinal = select->alwaysFinal;
                mgr->PatchAids(sub);
                reinterpret_cast<RKNet::SELECTHandler&>(handler).AllocatePlayerIdsToAids();
                controller->UpdateAidsBelongingToPlayerIds();
            }

            if (isLapKO) {
                system->lapKoMgr->SetKoPerRace(select->koPerRace);
            }
        }
    }

    stageMgr.SetModeTypes();
}
kmCall(0x80650494, PatchAidsBeforeSELECTStageMgrSetup);

static u8 SwapUISelectInfo() {
    register u8 aid;
    register u8 localPlayerCount;  // obtained from RKNet::Controller, spectators do NOT count
    register u8 curHudSlotId;
    asm(mr aid, r17;);
    asm(mr localPlayerCount, r24;);
    asm(mr curHudSlotId, r16;);

    asm(stb r16, 0x1f5(r22););  // default
    const System* system = System::sInstance;
    if (system->IsContext(PULSAR_MODE_KO)) {
        if (localPlayerCount == 1) {
            Mgr* mgr = system->koMgr;
            curHudSlotId = mgr->IsKOdAid(aid, 0);  // if only one localPlayer but slot 0 is KOd, that guarantees it was initially 2 players and the main is out
            const RKNet::Controller* controller = RKNet::Controller::sInstance;
            const RKNet::ControllerSub& sub = controller->subs[controller->currentSub];
            if (curHudSlotId == 1 && aid == sub.localAid && !mgr->GetIsSwapped()) mgr->SwapControllersAndUI();
        }
    }

    return curHudSlotId;
}
kmCall(0x80651988, SwapUISelectInfo);
kmWrite32(0x8065198c, 0x5600063f);

static u8 SwapRaceMiis() {
    register u32 aid;
    register u8 curHudSlotId;
    register u32 playerId;
    asm(mr aid, r21;);
    asm(mr curHudSlotId, r16;);
    asm(mr playerId, r17;);
    const System* system = System::sInstance;

    bool isKO = system->IsContext(PULSAR_MODE_KO);
    if (isKO) {
        if (aid < 12) curHudSlotId = system->koMgr->IsKOdAid(aid, 0);
        if (curHudSlotId == 1) {
            RacedataScenario& scenario = Racedata::sInstance->menusScenario;
            char mainPlayer[sizeof(RacedataPlayer)];
            u32 guestId = playerId + 1;
            memcpy(&mainPlayer, &scenario.players[playerId], sizeof(RacedataPlayer));
            memcpy(&scenario.players[playerId], &scenario.players[guestId], sizeof(RacedataPlayer));
            memcpy(&scenario.players[guestId], &mainPlayer, sizeof(RacedataPlayer));
            scenario.players[playerId].playerType = scenario.players[guestId].playerType;
            scenario.players[guestId].playerType = PLAYER_NONE;
            scenario.players[playerId].playerType = scenario.players[guestId].playerType;
            scenario.players[guestId].playerType = PLAYER_NONE;
            curHudSlotId = 0;
        }
    } else if (!isKO)
        curHudSlotId = 0;  // default
    else
        curHudSlotId++;
    return curHudSlotId;
}
kmCall(0x8065123c, SwapRaceMiis);

void StoreItemsForSpectating(RKNet::ITEMHandler& itemHandler) {
    itemHandler.ImportNewPackets();
    if (System::sInstance->IsContext(PULSAR_MODE_KO)) {  // guaranteed to be spectating already via a check in the func
        const RacedataScenario& scenario = Racedata::sInstance->racesScenario;
        for (int playerId = 0; playerId < System::sInstance->nonTTGhostPlayersCount; ++playerId) {
            if (scenario.players[playerId].playerType != PLAYER_REAL_ONLINE) continue;
            const ItemId item = itemHandler.GetStoredItem(playerId);
            Item::Player& itemPlayer = Item::Manager::sInstance->players[playerId];
            itemPlayer.inventory.currentItemId = item;
            itemPlayer.inventory.currentItemCount = item != ITEM_NONE;
        }
    }
}
kmCall(0x8065c69c, StoreItemsForSpectating);

static void SkipConfirmationPage(Pages::SELECTStageMgr* _this, PageId id, u32 animDirection) {
    if (System::sInstance->IsContext(PULSAR_MODE_KO)) {
        if (System::sInstance->koMgr->isSpectating) {
            _this->status = Pages::SELECTStageMgr::STATUS_VOTES_PAGE;
            return _this->AddPageLayer(PAGE_VOTE, animDirection);
        }
    }

    _this->status = Pages::SELECTStageMgr::STATUS_VR_PAGE;
    return _this->AddPageLayer(id, animDirection);
}

kmWriteNop(0x806508f8);
kmCall(0x806508f0, SkipConfirmationPage);

}  // namespace KO
}  // namespace Pulsar
