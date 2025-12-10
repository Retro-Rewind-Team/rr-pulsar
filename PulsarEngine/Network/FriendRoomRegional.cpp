#include <kamek.hpp>
#include <MarioKartWii/System/Identifiers.hpp>
#include <core/System/SystemManager.hpp>
#include <MarioKartWii/UI/Section/SectionMgr.hpp>
#include <MarioKartWii/UI/Section/Section.hpp>
#include <MarioKartWii/Race/Racedata.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <Config.hpp>
#include <PulsarSystem.hpp>
#include <Network/Network.hpp>
#include <UI/ExtendedTeamSelect/ExtendedTeamManager.hpp>

namespace Pulsar {
namespace Network {

static u8 REGIONID = 0x0A;

static void SetRegionId(u8 regionId) {
    if (Pulsar::System::sInstance->IsContext(PULSAR_STARTRETROS))
        REGIONID = 0x0A;
    else if (Pulsar::System::sInstance->IsContext(PULSAR_STARTCTS))
        REGIONID = 0x14;
    else if (Pulsar::System::sInstance->IsContext(PULSAR_STARTREGS))
        REGIONID = 0x15;
    else if (Pulsar::System::sInstance->IsContext(PULSAR_START200))
        REGIONID = 0x0C;
    else if (Pulsar::System::sInstance->IsContext(PULSAR_STARTOTT))
        REGIONID = 0x0B;
    else if (Pulsar::System::sInstance->IsContext(PULSAR_STARTITEMRAIN))
        REGIONID = 0x0D;
    else
        REGIONID = regionId;
}
static SectionLoadHook setRegionIdHook(SetRegionId);

static SectionId ConvertToRegionalSection(SectionId id) {
    switch (id) {
        case SECTION_P1_WIFI_FROOM_VS_VOTING:
        case SECTION_P1_WIFI_FROOM_TEAMVS_VOTING:
            return SECTION_P1_WIFI_VS_VOTING;
        case SECTION_P2_WIFI_FROOM_VS_VOTING:
        case SECTION_P2_WIFI_FROOM_TEAMVS_VOTING:
            return SECTION_P2_WIFI_VS_VOTING;
        case SECTION_P1_WIFI_FRIEND_VS:
        case SECTION_P1_WIFI_FRIEND_TEAMVS:
            return SECTION_P1_WIFI_VS;
        case SECTION_P2_WIFI_FRIEND_VS:
        case SECTION_P2_WIFI_FRIEND_TEAMVS:
            return SECTION_P2_WIFI_VS;
        default:
            return id;
    }
}

static bool ConvertFriendRoomStateToRegional() {
    RKNet::Controller* controller = RKNet::Controller::sInstance;
    System* system = System::sInstance;
    if (controller == nullptr || system == nullptr) return false;

    const bool wasHost = controller->roomType == RKNet::ROOMTYPE_FROOM_HOST;
    const bool wasNonHost = controller->roomType == RKNet::ROOMTYPE_FROOM_NONHOST;
    if (!wasHost && !wasNonHost)
        return false;

    Racedata* racedata = Racedata::sInstance;
    if (racedata == nullptr) return false;

    RacedataSettings& menuSettings = racedata->menusScenario.settings;
    if (menuSettings.gamemode != MODE_PRIVATE_VS) return false;

    menuSettings.gamemode = MODE_PUBLIC_VS;
    menuSettings.modeFlags &= ~static_cast<u32>(2);
    menuSettings.gametype = GAMETYPE_DEFAULT;

    RacedataSettings& raceSettings = racedata->racesScenario.settings;
    raceSettings.gamemode = MODE_PUBLIC_VS;
    raceSettings.modeFlags &= ~static_cast<u32>(2);
    raceSettings.gametype = GAMETYPE_DEFAULT;

    const u8 localPlayerCount = controller->subs[controller->currentSub].localPlayerCount;
    const u8 totalPlayerCount = controller->subs[controller->currentSub].playerCount;

    Network::Mgr& netMgr = system->netMgr;
    netMgr.hostContext = 0;
    netMgr.hostContext2 = 0;
    netMgr.denyType = DENY_TYPE_NORMAL;
    netMgr.deniesCount = 0;
    netMgr.ownStatusData = 0;
    netMgr.racesPerGP = 3;
    netMgr.curBlockingArrayIdx = 0;
    netMgr.region = REGIONID;

    const u32 blockingCount = system->GetInfo().GetTrackBlocking();
    if (netMgr.lastTracks != nullptr && blockingCount != 0) {
        for (u32 i = 0; i < blockingCount; ++i) {
            netMgr.lastTracks[i] = PULSARID_NONE;
        }
    }

    controller->roomType = RKNet::ROOMTYPE_VS_REGIONAL;
    controller->localStatusData.regionId = REGIONID;
    controller->localStatusData.status = RKNet::FRIEND_STATUS_PUBLIC_VS;
    controller->localStatusData.playerCount = totalPlayerCount != 0 ? totalPlayerCount : localPlayerCount;
    controller->localStatusData.curRace = 0;

    for (int i = 0; i < 2; ++i) {
        controller->subs[i].matchingSuspended = false;
        controller->subs[i].groupId = 0;
        controller->subs[i].friendToJoin = static_cast<u32>(-1);
    }

    controller->UpdateStatusDatas();
    controller->StartMatching();

    if (UI::ExtendedTeamManager::sInstance != nullptr) {
        UI::ExtendedTeamManager::sInstance->Reset();
        UI::ExtendedTeamManager::sInstance->ResetPlayers();
    }

    system->context = 0;
    system->context2 = 0;
    system->UpdateContext();

    return true;
}

static bool ShouldConvert(SectionId nextSectionId) {
    switch (nextSectionId) {
        case SECTION_P1_WIFI_FROOM_VS_VOTING:
        case SECTION_P1_WIFI_FROOM_TEAMVS_VOTING:
        case SECTION_P2_WIFI_FROOM_VS_VOTING:
        case SECTION_P2_WIFI_FROOM_TEAMVS_VOTING:
        case SECTION_P1_WIFI_FRIEND_VS:
        case SECTION_P1_WIFI_FRIEND_TEAMVS:
        case SECTION_P2_WIFI_FRIEND_VS:
        case SECTION_P2_WIFI_FRIEND_TEAMVS:
            return true;
        default:
            return false;
    }
}

static void ApplyNextSection(SectionMgr* sectionMgr, SectionId nextSectionId, u32 animDirection) {
    const SectionId currentNext = sectionMgr->nextSectionId;
    if (currentNext != nextSectionId) {
        const int currentPriority = sectionMgr->GetSectionPriority(currentNext);
        const int newPriority = sectionMgr->GetSectionPriority(nextSectionId);
        if (currentPriority < newPriority) {
            sectionMgr->nextSectionId = nextSectionId;
            sectionMgr->fadeAnimIdx = animDirection;
        } else {
            sectionMgr->GetSectionPriority(currentNext);
            sectionMgr->GetSectionPriority(nextSectionId);
        }
    }
}

static void SetNextSectionRegionalHook(SectionMgr* sectionMgr, SectionId nextSectionId, u32 animDirection) {
    SetRegionId(REGIONID);
    bool isFroom = RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_FROOM_HOST || RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_FROOM_NONHOST;
    if ((Pulsar::System::sInstance->IsContext(PULSAR_STARTRETROS) || Pulsar::System::sInstance->IsContext(PULSAR_STARTCTS) ||
         Pulsar::System::sInstance->IsContext(PULSAR_STARTREGS) || Pulsar::System::sInstance->IsContext(PULSAR_START200) ||
         Pulsar::System::sInstance->IsContext(PULSAR_STARTOTT) || Pulsar::System::sInstance->IsContext(PULSAR_STARTITEMRAIN)) &&
        isFroom) {
        static bool hasConverted = false;

        SectionId desiredSection = nextSectionId;

        if (DWC::MatchControl* matchControl = DWC::MatchControl::sInstance) {
            volatile u8* ctrlBytes = reinterpret_cast<volatile u8*>(matchControl);
            ctrlBytes[0x15] = DWC::MATCH_TYPE_ANYBODY;
        }

        RKNet::Controller* controller = RKNet::Controller::sInstance;
        if (controller != nullptr) {
            if (controller->roomType == RKNet::ROOMTYPE_FROOM_HOST || controller->roomType == RKNet::ROOMTYPE_FROOM_NONHOST) {
                hasConverted = false;
                if (ShouldConvert(nextSectionId)) {
                    SectionId regionalSection = ConvertToRegionalSection(nextSectionId);
                    if (!hasConverted && ConvertFriendRoomStateToRegional()) {
                        hasConverted = true;
                        desiredSection = regionalSection;
                    }
                }
            } else if (hasConverted) {
                desiredSection = ConvertToRegionalSection(nextSectionId);
            }
        }
        ApplyNextSection(sectionMgr, desiredSection, animDirection);
    } else {
        ApplyNextSection(sectionMgr, nextSectionId, animDirection);
    }
}
kmBranch(0x80635a3c, SetNextSectionRegionalHook);

}  // namespace Network
}  // namespace Pulsar