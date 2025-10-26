#include <MarioKartWii/Race/RaceData.hpp>
#include <SlotExpansion/CupsConfig.hpp>
#include <Settings/UI/SettingsPanel.hpp>
#include <Settings/Settings.hpp>
#include <MarioKartWii/Kart/KartValues.hpp>
#include <MarioKartWii/File/StatsParam.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <MarioKartWii/Objects/Collidable/Itembox/Itembox.hpp>
#include <Dolphin/DolphinIOS.hpp>
#include <MarioKartWii/Kart/KartManager.hpp>
#include <core/rvl/OS/OS.hpp>

namespace RetroRewind {
Pulsar::System* System::Create() {
    return new System();  // now Pulsar::sInstance is of type RetroRewind
}
Pulsar::System::Inherit CreateRetroRewind(System::Create);

bool System::Is500cc() {
    return Racedata::sInstance->racesScenario.settings.engineClass == CC_50;
}

System::WeightClass System::GetWeightClass(const CharacterId id) {
    switch (id) {
        case BABY_MARIO:
        case BABY_LUIGI:
        case BABY_PEACH:
        case BABY_DAISY:
        case TOAD:
        case TOADETTE:
        case KOOPA_TROOPA:
        case DRY_BONES:
            return LIGHTWEIGHT;
        case MARIO:
        case LUIGI:
        case PEACH:
        case DAISY:
        case YOSHI:
        case BIRDO:
        case DIDDY_KONG:
        case BOWSER_JR:
            return MEDIUMWEIGHT;
        case WARIO:
        case WALUIGI:
        case DONKEY_KONG:
        case BOWSER:
        case KING_BOO:
        case ROSALINA:
        case FUNKY_KONG:
        case DRY_BOWSER:
            return HEAVYWEIGHT;
        default:
            return MIIS;
    }
}

// Force 30 FPS [Vabold]
kmWrite32(0x80554224, 0x3C808000);
kmWrite32(0x80554228, 0x88841204);
kmWrite32(0x8055422C, 0x48000044);

void FPSPatch() {
    FPSPatchHook = 0x00;
    const RacedataScenario& scenario = Racedata::sInstance->racesScenario;
    const GameMode mode = scenario.settings.gamemode;
    bool isDolphin = Dolphin::IsEmulator();
    bool froom = RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_FROOM_NONHOST || RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_FROOM_HOST || RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_NONE;
    bool froomOrVS = RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_FROOM_NONHOST || RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_FROOM_HOST || RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_NONE || RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_VS_REGIONAL || RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_JOINING_REGIONAL;
    bool isTimeTrial = mode == MODE_TIME_TRIAL;
    u32 localPlayerCount = scenario.localPlayerCount;
    if (static_cast<Pulsar::FPS>(Pulsar::Settings::Mgr::Get().GetUserSettingValue(static_cast<Pulsar::Settings::UserType>(Pulsar::Settings::SETTINGSTYPE_RACE2), Pulsar::RADIO_FPS)) == Pulsar::FPS_HALF || (localPlayerCount > 1 && !isDolphin) ||
        (Pulsar::System::sInstance->IsContext(Pulsar::PULSAR_ITEMMODERAIN) && !isDolphin && froomOrVS) || (Pulsar::System::sInstance->IsContext(Pulsar::PULSAR_ITEMMODESTORM) && !isDolphin && froom)) {
        FPSPatchHook = 0x00FF0100;
    }
}
static PageLoadHook PatchFPS(FPSPatch);

void ItemBoxRespawn(Objects::Itembox* itembox) {
    bool is200 = Racedata::sInstance->racesScenario.settings.engineClass == CC_100 && RKNet::Controller::sInstance->roomType != RKNet::ROOMTYPE_VS_WW;
    bool isFastRespawn = Pulsar::ITEMBOX_DEFAULTRESPAWN;
    if (RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_FROOM_NONHOST || RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_FROOM_HOST || RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_NONE) {
        isFastRespawn = Pulsar::System::sInstance->IsContext(Pulsar::PULSAR_ITEMBOXRESPAWN) ? Pulsar::ITEMBOX_FASTRESPAWN : Pulsar::ITEMBOX_DEFAULTRESPAWN;
    }
    itembox->respawnTime = 150;
    itembox->isActive = 0;
    if (isFastRespawn == Pulsar::ITEMBOX_FASTRESPAWN || is200) {
        itembox->respawnTime = 75;
        itembox->isActive = 0;
    }
}
kmCall(0x80828EDC, ItemBoxRespawn);

void PredictionPatch() {
    PredictionHook = 0x3dcccccd;
    if (static_cast<Pulsar::MenuSettingPredictionRemoval>(Pulsar::Settings::Mgr::Get().GetUserSettingValue(static_cast<Pulsar::Settings::UserType>(Pulsar::Settings::SETTINGSTYPE_ONLINE), Pulsar::RADIO_PREDICTIONREMOVAL)) == Pulsar::PREDICTIONREMOVAL_ENABLED) {
        PredictionHook = 0x3f800000;
    }
}
static PageLoadHook PatchPrediction(PredictionPatch);

}  // namespace RetroRewind