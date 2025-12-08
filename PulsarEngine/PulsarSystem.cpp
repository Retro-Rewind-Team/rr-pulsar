#include <core/RK/RKSystem.hpp>
#include <core/nw4r/ut/Misc.hpp>
#include <MarioKartWii/Scene/RootScene.hpp>
#include <MarioKartWii/GlobalFunctions.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <PulsarSystem.hpp>
#include <Extensions/LECODE/LECODEMgr.hpp>
#include <Gamemodes/KO/KOMgr.hpp>
#include <Gamemodes/KO/KOHost.hpp>
#include <Gamemodes/LapKO/LapKOMgr.hpp>
#include <Gamemodes/OnlineTT/OnlineTT.hpp>
#include <Settings/Settings.hpp>
#include <Config.hpp>
#include <UI/ExtendedTeamSelect/ExtendedTeamManager.hpp>
#include <SlotExpansion/CupsConfig.hpp>
#include <core/egg/DVD/DvdRipper.hpp>
#include <MarioKartWii/UI/Page/Other/FriendList.hpp>
#include <MarioKartWii/UI/Page/Other/FriendRoom.hpp>
#include <RetroRewindChannel.hpp>
#include <Dolphin/DolphinIOS.hpp>
#include <hooks.hpp>

namespace Pulsar {

System* System::sInstance = nullptr;
System::Inherit* System::inherit = nullptr;

void System::CreateSystem() {
    if (sInstance != nullptr) return;
    EGG::Heap* heap = RKSystem::mInstance.EGGSystem;
    const EGG::Heap* prev = heap->BecomeCurrentHeap();
    System* system;
    if (inherit != nullptr) {
        system = inherit->create();
    } else
        system = new System();
    System::sInstance = system;
    UI::ExtendedTeamManager::CreateInstance(new UI::ExtendedTeamManager());
    ConfigFile& conf = ConfigFile::LoadConfig();
    system->Init(conf);
    prev->BecomeCurrentHeap();
    conf.Destroy();
}
// kmCall(0x80543bb4, System::CreateSystem);
BootHook CreateSystem(System::CreateSystem, 0);

System::System() : heap(RKSystem::mInstance.EGGSystem), taskThread(EGG::TaskThread::Create(8, 0, 0x4000, this->heap)),
                   // Modes
                   koMgr(nullptr),
                   lapKoMgr(nullptr) {
}

void System::Init(const ConfigFile& conf) {
    IOType type = IOType_ISO;
    bool isDolphin = Dolphin::IsEmulator();
    s32 ret = IO::OpenFix("file", IOS::MODE_NONE);

    if (ret >= 0 && !IsNewChannel()) {
        type = IOType_RIIVO;
        IOS::Close(ret);
    } else if (IsNewChannel() && !isDolphin) {
        NewChannel_SetLoadedFromRRFlag();
        type = IOType_SD;
    } else {
        ret = IO::OpenFix("/dev/dolphin", IOS::MODE_NONE);
        if (isDolphin) {
            type = IOType_DOLPHIN;
            IOS::Close(ret);
        }
    }

    strncpy(this->modFolderName, conf.header.modFolderName, IOS::ipcMaxFileName);
    static char* pulMagic = reinterpret_cast<char*>(0x800017CC);
    strcpy(pulMagic, "PUL2");

    // InitInstances
    CupsConfig::sInstance = new CupsConfig(conf.GetSection<CupsHolder>());
    this->info.Init(conf.GetSection<InfoHolder>().info);
    this->InitIO(type);
    this->InitSettings(&conf.GetSection<CupsHolder>().trophyCount[0]);

    const PulBMG& bmgSection = conf.GetSection<PulBMG>();
    const u8* confStart = reinterpret_cast<const u8*>(&conf);
    const u8* fileSection = reinterpret_cast<const u8*>(&bmgSection.header) + bmgSection.header.fileLength;
    const u32 totalSize = ConfigFile::readBytes;
    const u32 offset = static_cast<u32>(fileSection - confStart);
    if (offset < totalSize) {
        const u32 remaining = totalSize - offset;
        CupsConfig::sInstance->LoadFileNames(reinterpret_cast<const char*>(fileSection), remaining);
    }

    // Initialize last selected cup and courses
    const PulsarCupId last = Settings::Mgr::sInstance->GetSavedSelectedCup();
    CupsConfig* cupsConfig = CupsConfig::sInstance;
    cupsConfig->SetLayout();
    if (last != -1 && cupsConfig->IsValidCup(last) && cupsConfig->GetTotalCupCount() > 8) {
        cupsConfig->lastSelectedCup = last;
        cupsConfig->SetSelected(cupsConfig->ConvertTrack_PulsarCupToTrack(last, 0));
        cupsConfig->lastSelectedCupButtonIdx = last & 1;
    }

    // Track blocking
    u32 trackBlocking = this->info.GetTrackBlocking();
    this->netMgr.lastTracks = new PulsarId[trackBlocking];
    for (int i = 0; i < trackBlocking; ++i) this->netMgr.lastTracks[i] = PULSARID_NONE;
    const BMGHeader* const confBMG = &conf.GetSection<PulBMG>().header;
    this->rawBmg = EGG::Heap::alloc<BMGHeader>(confBMG->fileLength, 0x4, RootScene::sInstance->expHeapGroup.heaps[1]);
    memcpy(this->rawBmg, confBMG, confBMG->fileLength);
    this->customBmgs.Init(*this->rawBmg);

    this->AfterInit();
}

// IO
#pragma suppress_warnings on
void System::InitIO(IOType type) const {
    IO* io = IO::CreateInstance(type, this->heap, this->taskThread);
    bool ret;
    if (io->type == IOType_DOLPHIN) ret = ISFS::CreateDir("/shared2/Pulsar", 0, IOS::MODE_READ_WRITE, IOS::MODE_READ_WRITE, IOS::MODE_READ_WRITE);
    const char* modFolder = this->GetModFolder();
    ret = io->CreateFolder(modFolder);
    if (!ret && io->type == IOType_DOLPHIN) {
        char path[0x100];
        snprintf(path, 0x100, "Unable to automatically create a folder for this CT distribution\nPlease create a Pulsar folder in Dolphin Emulator/Wii/shared2", modFolder);
        Debug::FatalError(path);
    }
    char ghostPath[IOS::ipcMaxPath];
    snprintf(ghostPath, IOS::ipcMaxPath, "%s%s", modFolder, "/Ghosts");
    io->CreateFolder(ghostPath);
}
#pragma suppress_warnings reset

void System::InitSettings(const u16* totalTrophyCount) const {
    Settings::Mgr* settings = new (this->heap) Settings::Mgr;
    char settingsPath[IOS::ipcMaxPath];
    snprintf(settingsPath, IOS::ipcMaxPath, "%s/%s", this->GetModFolder(), "RRGameSettings.pul");
    char trophiesPath[IOS::ipcMaxPath];
    snprintf(trophiesPath, IOS::ipcMaxPath, "%s/%s", this->GetModFolder(), "RRSettings.pul");  // Original settings file
    settings->Init(totalTrophyCount, settingsPath, trophiesPath);
    Settings::Mgr::sInstance = settings;
}

void System::UpdateContext() {
    const RacedataSettings& racedataSettings = Racedata::sInstance->menusScenario.settings;
    const GameMode mode = racedataSettings.gamemode;
    this->ottMgr.Reset();
    const Settings::Mgr& settings = Settings::Mgr::Get();
    const RKNet::Controller* controller = RKNet::Controller::sInstance;
    Network::Mgr& netMgr = this->netMgr;
    const u32 sceneId = GameScene::GetCurrent()->id;

    bool isFroom = controller->roomType == RKNet::ROOMTYPE_FROOM_HOST || controller->roomType == RKNet::ROOMTYPE_FROOM_NONHOST;
    bool isRegionalRoom = controller->roomType == RKNet::ROOMTYPE_VS_REGIONAL || controller->roomType == RKNet::ROOMTYPE_JOINING_REGIONAL || controller->roomType == RKNet::ROOMTYPE_BT_REGIONAL;
    bool isBattle = mode == MODE_BATTLE || mode == MODE_PRIVATE_BATTLE || mode == MODE_PUBLIC_BATTLE;
    bool isBalloonBattle = isBattle && racedataSettings.battleType == BATTLE_BALLOON;
    bool isNotPublic = isFroom || controller->roomType == RKNet::ROOMTYPE_NONE;
    bool isTimeTrial = mode == MODE_TIME_TRIAL;

    bool isCT = true;
    bool isHAW = false;
    bool isKO = false;
    bool isOTT = false;
    bool is200 = racedataSettings.engineClass == CC_100 && this->info.Has200cc();
    bool is500 = settings.GetUserSettingValue(Settings::SETTINGSTYPE_FROOM1, HOSTCC_500);
    bool isOTTOnline = settings.GetUserSettingValue(Settings::SETTINGSTYPE_MISC, SCROLLER_WWMODE) == WWMODE_OTT && mode == MODE_PUBLIC_VS;
    bool isMiiHeads = settings.GetUserSettingValue(Settings::SETTINGSTYPE_RACE1, RADIO_MIIHEADS);
    bool is200Online = settings.GetUserSettingValue(Settings::SETTINGSTYPE_MISC, SCROLLER_WWMODE) == WWMODE_200 && mode == MODE_PUBLIC_VS;
    bool isExtendedTeams = settings.GetUserSettingValue(Settings::SETTINGSTYPE_EXTENDEDTEAMS, RADIO_EXTENDEDTEAMSENABLED) == EXTENDEDTEAMS_ENABLED;
    bool isLapBasedKO = settings.GetUserSettingValue(Settings::SETTINGSTYPE_KO, RADIO_KOENABLED) == KOSETTING_LAPBASED && isNotPublic && !isBattle && !isTimeTrial;
    bool isKOFinal = settings.GetUserSettingValue(Settings::SETTINGSTYPE_KO, RADIO_KOFINAL) == KOSETTING_FINAL_ALWAYS;
    bool isCharRestrictLight = settings.GetUserSettingValue(Settings::SETTINGSTYPE_FROOM1, RADIO_CHARSELECT) == CHAR_LIGHTONLY;
    bool isCharRestrictMid = settings.GetUserSettingValue(Settings::SETTINGSTYPE_FROOM1, RADIO_CHARSELECT) == CHAR_MEDIUMONLY;
    bool isCharRestrictHeavy = settings.GetUserSettingValue(Settings::SETTINGSTYPE_FROOM1, RADIO_CHARSELECT) == CHAR_HEAVYONLY;
    bool isKartRestrictKart = settings.GetUserSettingValue(Settings::SETTINGSTYPE_FROOM1, RADIO_KARTSELECT) == KART_KARTONLY;
    bool isKartRestrictBike = settings.GetUserSettingValue(Settings::SETTINGSTYPE_FROOM1, RADIO_KARTSELECT) == KART_BIKEONLY;
    bool isThunderCloud = (settings.GetUserSettingValue(Settings::SETTINGSTYPE_FROOM2, RADIO_THUNDERCLOUD) == THUNDERCLOUD_NORMAL) && isNotPublic;
    bool isItemModeRandom = settings.GetUserSettingValue(Settings::SETTINGSTYPE_FROOM1, SCROLLER_ITEMMODE) == GAMEMODE_RANDOM && isNotPublic;
    bool isItemModeBlast = settings.GetUserSettingValue(Settings::SETTINGSTYPE_FROOM1, SCROLLER_ITEMMODE) == GAMEMODE_BLAST && isNotPublic;
    bool isItemModeRain = settings.GetUserSettingValue(Settings::SETTINGSTYPE_FROOM1, SCROLLER_ITEMMODE) == GAMEMODE_ITEMRAIN;
    bool isItemModeStorm = settings.GetUserSettingValue(Settings::SETTINGSTYPE_FROOM1, SCROLLER_ITEMMODE) == GAMEMODE_ITEMSTORM;
    bool isTrackSelectionRegs = settings.GetUserSettingValue(Settings::SETTINGSTYPE_FROOM1, SCROLLER_TRACKSELECTION) == TRACKSELECTION_REGS;
    bool isTrackSelectionRetros = settings.GetUserSettingValue(Settings::SETTINGSTYPE_FROOM1, SCROLLER_TRACKSELECTION) == TRACKSELECTION_RETROS && mode != MODE_PUBLIC_VS;
    bool isTrackSelectionCts = settings.GetUserSettingValue(Settings::SETTINGSTYPE_FROOM1, SCROLLER_TRACKSELECTION) == TRACKSELECTION_CTS && mode != MODE_PUBLIC_VS;
    bool isChangeCombo = settings.GetUserSettingValue(Settings::SETTINGSTYPE_OTT, RADIO_OTTALLOWCHANGECOMBO) == OTTSETTING_COMBO_ENABLED;
    bool isItemBoxRepsawnFast = settings.GetUserSettingValue(Settings::SETTINGSTYPE_FROOM2, RADIO_ITEMBOXRESPAWN) == ITEMBOX_FASTRESPAWN;
    bool isTransmissionInside = settings.GetUserSettingValue(Settings::SETTINGSTYPE_FROOM2, RADIO_FORCETRANSMISSION) == FORCE_TRANSMISSION_INSIDE && isFroom;
    bool isTransmissionOutside = settings.GetUserSettingValue(Settings::SETTINGSTYPE_FROOM2, RADIO_FORCETRANSMISSION) == FORCE_TRANSMISSION_OUTSIDE && isFroom;
    bool isTransmissionVanilla = settings.GetUserSettingValue(Settings::SETTINGSTYPE_FROOM2, RADIO_FORCETRANSMISSION) == FORCE_TRANSMISSION_VANILLA && isFroom;
    bool isTeamBattle = settings.GetUserSettingValue(Settings::SETTINGSTYPE_BATTLE, RADIO_BATTLETEAMS) == BATTLE_FFA_DISABLED && isBattle;
    bool isElimination = settings.GetUserSettingValue(Settings::SETTINGSTYPE_BATTLE, RADIO_BATTLEELIMINATION) && isBalloonBattle;
    bool isStartRetro = settings.GetUserSettingValue(Settings::SETTINGSTYPE_FROOM2, SCROLLER_STARTWORLDWIDE) == START_WORLDWIDE_RETROS;
    bool isStartCT = settings.GetUserSettingValue(Settings::SETTINGSTYPE_FROOM2, SCROLLER_STARTWORLDWIDE) == START_WORLDWIDE_CTS;
    bool isStartRTS = settings.GetUserSettingValue(Settings::SETTINGSTYPE_FROOM2, SCROLLER_STARTWORLDWIDE) == START_WORLDWIDE_RTS;
    bool isStart200 = settings.GetUserSettingValue(Settings::SETTINGSTYPE_FROOM2, SCROLLER_STARTWORLDWIDE) == START_WORLDWIDE_200;
    bool isStartOTT = settings.GetUserSettingValue(Settings::SETTINGSTYPE_FROOM2, SCROLLER_STARTWORLDWIDE) == START_WORLDWIDE_OTT;
    bool isStartItemRain = settings.GetUserSettingValue(Settings::SETTINGSTYPE_FROOM2, SCROLLER_STARTWORLDWIDE) == START_WORLDWIDE_ITEMRAIN;
    bool isRanking = settings.GetUserSettingValue(Settings::SETTINGSTYPE_FROOM1, RADIO_RANKINGS) == RANKINGS_ENABLED && isFroom;
    bool isFeather = this->info.HasFeather();
    bool isUMTs = this->info.HasUMTs();
    u32 newContext = 0;
    u32 newContext2 = 0;
    if (sceneId != SCENE_ID_GLOBE && controller->connectionState != RKNet::CONNECTIONSTATE_SHUTDOWN) {
        switch (controller->roomType) {
            case (RKNet::ROOMTYPE_VS_REGIONAL):
            case (RKNet::ROOMTYPE_JOINING_REGIONAL):
                isOTT = netMgr.ownStatusData == true;
                break;
            case (RKNet::ROOMTYPE_FROOM_HOST):
            case (RKNet::ROOMTYPE_FROOM_NONHOST):
                isCT = true;
                newContext = netMgr.hostContext;
                newContext2 = netMgr.hostContext2;
                isKOFinal = newContext & (1 << PULSAR_KOFINAL);
                isCharRestrictLight = newContext & (1 << PULSAR_CHARRESTRICTLIGHT);
                isCharRestrictMid = newContext & (1 << PULSAR_CHARRESTRICTMID);
                isCharRestrictHeavy = newContext & (1 << PULSAR_CHARRESTRICTHEAVY);
                isKartRestrictKart = newContext & (1 << PULSAR_KARTRESTRICT);
                isKartRestrictBike = newContext & (1 << PULSAR_BIKERESTRICT);
                isItemModeRandom = newContext2 & (1 << PULSAR_ITEMMODERANDOM);
                isItemModeBlast = newContext2 & (1 << PULSAR_ITEMMODEBLAST);
                isItemModeRain = newContext2 & (1 << PULSAR_ITEMMODERAIN);
                isItemModeStorm = newContext2 & (1 << PULSAR_ITEMMODESTORM);
                isTrackSelectionRegs = newContext & (1 << PULSAR_REGS);
                isTrackSelectionRetros = newContext & (1 << PULSAR_RETROS);
                isTrackSelectionCts = newContext & (1 << PULSAR_CTS);
                is500 = newContext & (1 << PULSAR_500);
                is200Online |= newContext & (1 << PULSAR_200_WW);
                isHAW = newContext2 & (1 << PULSAR_HAW);
                isKO = newContext & (1 << PULSAR_MODE_KO);
                isOTT = newContext & (1 << PULSAR_MODE_OTT);
                isOTTOnline |= newContext & (1 << PULSAR_MODE_OTT);
                isMiiHeads = newContext2 & (1 << PULSAR_MIIHEADS);
                isThunderCloud = newContext & (1 << PULSAR_THUNDERCLOUD);
                isItemBoxRepsawnFast = newContext2 & (1 << PULSAR_ITEMBOXRESPAWN);
                isTransmissionInside = newContext2 & (1 << PULSAR_TRANSMISSIONINSIDE);
                isTransmissionOutside = newContext2 & (1 << PULSAR_TRANSMISSIONOUTSIDE);
                isTransmissionVanilla = newContext2 & (1 << PULSAR_TRANSMISSIONVANILLA);
                isTeamBattle = newContext & (1 << PULSAR_FFA);
                isExtendedTeams = newContext & (1 << PULSAR_EXTENDEDTEAMS);
                isElimination = newContext & (1 << PULSAR_ELIMINATION);
                isLapBasedKO = newContext & (1 << PULSAR_MODE_LAPKO);
                isStartRetro = newContext & (1 << PULSAR_STARTRETROS);
                isStartCT = newContext & (1 << PULSAR_STARTCTS);
                isStartRTS = newContext & (1 << PULSAR_STARTREGS);
                isStart200 = newContext & (1 << PULSAR_START200);
                isStartOTT = newContext & (1 << PULSAR_STARTOTT);
                isStartItemRain = newContext & (1 << PULSAR_STARTITEMRAIN);
                isRanking = newContext2 & (1 << PULSAR_RANKING);
                if (isOTT) {
                    isUMTs = newContext & (1 << PULSAR_UMTS);
                    isFeather &= newContext & (1 << PULSAR_FEATHER);
                    isChangeCombo = newContext & (1 << PULSAR_CHANGECOMBO);
                }
                break;
            default:
                isCT = true;
        }
    } else {
        const u8 ottOffline = settings.GetUserSettingValue(Settings::SETTINGSTYPE_OTT, RADIO_OTTOFFLINE);
        isOTT = (mode == MODE_GRAND_PRIX || mode == MODE_VS_RACE) ? (ottOffline != OTTSETTING_OFFLINE_DISABLED) : false;  // offlineOTT
        if (isOTT) {
            isFeather &= (ottOffline == OTTSETTING_OFFLINE_FEATHER);
            isUMTs = settings.GetUserSettingValue(Settings::SETTINGSTYPE_OTT, RADIO_OTTALLOWUMTS);
        }
    }
    this->netMgr.hostContext = newContext;
    this->netMgr.hostContext2 = newContext2;

    u32 preserved = this->context & ((1 << PULSAR_200_WW) | (1 << PULSAR_MODE_OTT) | (1 << PULSAR_ELIMINATION));
    u32 preserved2 = this->context2 & (1 << PULSAR_ITEMMODERAIN);

    // When entering a friend room (host/nonhost), clear any region-preserved bits
    if (controller->roomType == RKNet::ROOMTYPE_FROOM_HOST || controller->roomType == RKNet::ROOMTYPE_FROOM_NONHOST || controller->roomType == RKNet::ROOMTYPE_NONE) {
        preserved &= ~((1 << PULSAR_200_WW) | (1 << PULSAR_MODE_OTT) | (1 << PULSAR_ELIMINATION));
        preserved2 &= ~((1 << PULSAR_ITEMMODERAIN) | (1 << PULSAR_ITEMMODESTORM));
    }

    // Set the new context value
    u32 newContextValue = (isCT) << PULSAR_CT;
    u32 newContextValue2 = 0;
    if (isCT) {
        newContextValue |= (is200) << PULSAR_200 | (isFeather) << PULSAR_FEATHER |
                           (isUMTs) << PULSAR_UMTS | (is500) << PULSAR_500 |
                           (isOTT) << PULSAR_MODE_OTT | (isKO) << PULSAR_MODE_KO |
                           (isCharRestrictLight) << PULSAR_CHARRESTRICTLIGHT | (isCharRestrictMid) << PULSAR_CHARRESTRICTMID |
                           (isCharRestrictHeavy) << PULSAR_CHARRESTRICTHEAVY | (isKartRestrictKart) << PULSAR_KARTRESTRICT |
                           (isKartRestrictBike) << PULSAR_BIKERESTRICT | (isChangeCombo) << PULSAR_CHANGECOMBO |
                           (isTrackSelectionRegs) << PULSAR_REGS | (isKOFinal) << PULSAR_KOFINAL |
                           (isExtendedTeams) << PULSAR_EXTENDEDTEAMS | (isTrackSelectionRetros) << PULSAR_RETROS |
                           (isTrackSelectionCts) << PULSAR_CTS | (isTeamBattle) << PULSAR_FFA |
                           (isElimination) << PULSAR_ELIMINATION | (isLapBasedKO) << PULSAR_MODE_LAPKO |
                           (isStartRetro) << PULSAR_STARTRETROS | (isStartCT) << PULSAR_STARTCTS |
                           (isStartRTS) << PULSAR_STARTREGS | (isStart200) << PULSAR_START200 |
                           (isStartOTT) << PULSAR_STARTOTT | (isStartItemRain) << PULSAR_STARTITEMRAIN |
                           (isThunderCloud) << PULSAR_THUNDERCLOUD;

        newContextValue2 |= (isTransmissionInside) << PULSAR_TRANSMISSIONINSIDE | (isTransmissionOutside) << PULSAR_TRANSMISSIONOUTSIDE |
                            (isTransmissionVanilla) << PULSAR_TRANSMISSIONVANILLA | (isItemModeRandom) << PULSAR_ITEMMODERANDOM |
                            (isItemModeBlast) << PULSAR_ITEMMODEBLAST | (isItemModeRain) << PULSAR_ITEMMODERAIN |
                            (isItemModeStorm) << PULSAR_ITEMMODESTORM | (isMiiHeads) << PULSAR_MIIHEADS |
                            (isHAW) << PULSAR_HAW | (isItemBoxRepsawnFast) << PULSAR_ITEMBOXRESPAWN |
                            (isRanking) << PULSAR_RANKING;
    }

    // Combine the new context with preserved bits
    this->context = newContextValue | preserved;
    this->context2 = newContextValue2 | preserved2;

    // Set contexts based on region for regionals
    const u32 region = this->netMgr.region;
    if (isRegionalRoom) {
        switch (region) {
            case 0x0A:  // Regular retro tracks
                this->context |= (1 << PULSAR_RETROS);
                sInstance->context &= ~(1 << PULSAR_200_WW);
                sInstance->context &= ~(1 << PULSAR_MODE_OTT);
                sInstance->context2 &= ~(1 << PULSAR_ITEMMODERAIN);
                sInstance->context2 &= ~(1 << PULSAR_FFA);
                sInstance->context &= ~(1 << PULSAR_ELIMINATION);
                sInstance->context2 &= ~(1 << PULSAR_ITEMMODESTORM);
                break;

            case 0x0B:  // OTT with retro tracks
                this->context |= (1 << PULSAR_RETROS);
                sInstance->context &= ~(1 << PULSAR_200_WW);
                this->context |= (1 << PULSAR_MODE_OTT);
                sInstance->context2 &= ~(1 << PULSAR_ITEMMODERAIN);
                sInstance->context2 &= ~(1 << PULSAR_FFA);
                sInstance->context &= ~(1 << PULSAR_ELIMINATION);
                sInstance->context2 &= ~(1 << PULSAR_ITEMMODESTORM);
                break;

            case 0x0C:  // 200cc with retro tracks
                this->context |= (1 << PULSAR_RETROS);
                this->context |= (1 << PULSAR_200_WW);
                sInstance->context &= ~(1 << PULSAR_MODE_OTT);
                sInstance->context2 &= ~(1 << PULSAR_ITEMMODERAIN);
                sInstance->context2 &= ~(1 << PULSAR_FFA);
                sInstance->context &= ~(1 << PULSAR_ELIMINATION);
                sInstance->context2 &= ~(1 << PULSAR_ITEMMODESTORM);
                break;

            case 0x0D:  // Item Rain with retro tracks
                this->context |= (1 << PULSAR_RETROS);
                this->context2 |= (1 << PULSAR_ITEMMODERAIN);
                sInstance->context &= ~(1 << PULSAR_200_WW);
                sInstance->context &= ~(1 << PULSAR_MODE_OTT);
                sInstance->context2 &= ~(1 << PULSAR_FFA);
                sInstance->context &= ~(1 << PULSAR_ELIMINATION);
                sInstance->context2 &= ~(1 << PULSAR_ITEMMODESTORM);
                break;

            case 0x14:  // CT (Custom Tracks)
                this->context |= (1 << PULSAR_CTS);
                sInstance->context &= ~(1 << PULSAR_200_WW);
                sInstance->context &= ~(1 << PULSAR_MODE_OTT);
                sInstance->context2 &= ~(1 << PULSAR_ITEMMODERAIN);
                sInstance->context2 &= ~(1 << PULSAR_FFA);
                sInstance->context &= ~(1 << PULSAR_ELIMINATION);
                sInstance->context2 &= ~(1 << PULSAR_ITEMMODESTORM);
                break;

            case 0x15:  // RT (Regular Tracks)
                this->context |= (1 << PULSAR_REGS);
                sInstance->context &= ~(1 << PULSAR_200_WW);
                sInstance->context &= ~(1 << PULSAR_MODE_OTT);
                sInstance->context2 &= ~(1 << PULSAR_ITEMMODERAIN);
                sInstance->context2 &= ~(1 << PULSAR_FFA);
                sInstance->context &= ~(1 << PULSAR_ELIMINATION);
                sInstance->context2 &= ~(1 << PULSAR_ITEMMODESTORM);
                break;

            case 0x0E:  // Battle
                this->context |= (1 << PULSAR_FFA);
                sInstance->context2 &= ~(1 << PULSAR_ITEMMODERAIN);
                sInstance->context2 &= ~(1 << PULSAR_ITEMMODESTORM);
                sInstance->context &= ~(1 << PULSAR_200_WW);
                sInstance->context &= ~(1 << PULSAR_MODE_OTT);
                sInstance->context &= ~(1 << PULSAR_ELIMINATION);
                break;

            case 0x0F:  // Battle Elim
                this->context |= (1 << PULSAR_FFA);
                this->context |= (1 << PULSAR_ELIMINATION);
                sInstance->context2 &= ~(1 << PULSAR_ITEMMODERAIN);
                sInstance->context &= ~(1 << PULSAR_200_WW);
                sInstance->context &= ~(1 << PULSAR_MODE_OTT);
                sInstance->context2 &= ~(1 << PULSAR_ITEMMODESTORM);
                break;
        }
    }

    if (!isTeamBattle) {
        sInstance->context &= ~(1 << PULSAR_ELIMINATION);
    }

    // Create temp instances if needed:
    /*
    if(sceneId == SCENE_ID_RACE) {
        if(this->lecodeMgr == nullptr) this->lecodeMgr = new (this->heap) LECODE::Mgr;
    }
    else if(this->lecodeMgr != nullptr) {
        delete this->lecodeMgr;
        this->lecodeMgr = nullptr;
    }
    */

    if (isKO) {
        if (sceneId == SCENE_ID_MENU && SectionMgr::sInstance->sectionParams->onlineParams.currentRaceNumber == -1) this->koMgr = new (this->heap) KO::Mgr;  // create komgr when loading the select phase of the 1st race of a froom
    }
    if (!isKO && this->koMgr != nullptr || isKO && sceneId == SCENE_ID_GLOBE) {
        delete this->koMgr;
        this->koMgr = nullptr;
    }

    if (isLapBasedKO) {
        if (this->lapKoMgr == nullptr) {
            this->lapKoMgr = new (this->heap) LapKO::Mgr;
        }
    } else if (this->lapKoMgr != nullptr) {
        delete this->lapKoMgr;
        this->lapKoMgr = nullptr;
    }
}

void System::UpdateContextWrapper() {
    System::sInstance->UpdateContext();
}

static Pulsar::Settings::Hook UpdateContext(System::UpdateContextWrapper);

void System::ClearOttContext() {
    bool isOTTEnabled = Settings::Mgr::Get().GetUserSettingValue(Settings::SETTINGSTYPE_OTT, RADIO_OTTOFFLINE);
    if (!isOTTEnabled) {
        sInstance->context &= ~(1 << PULSAR_MODE_OTT);
    }
}

static Pulsar::Settings::Hook UpdateOTTContext(System::ClearOttContext);

s32 System::OnSceneEnter(Random& random) {
    System* self = System::sInstance;
    self->UpdateContext();
    if (self->IsContext(PULSAR_MODE_OTT)) OTT::AddGhostToVS();
    if (self->IsContext(PULSAR_HAW) && self->IsContext(PULSAR_MODE_KO) && GameScene::GetCurrent()->id == SCENE_ID_RACE && SectionMgr::sInstance->sectionParams->onlineParams.currentRaceNumber > 0) {
        KO::HAWChangeData();
    }
    return random.NextLimited(8);
}
kmCall(0x8051ac40, System::OnSceneEnter);

asmFunc System::GetRaceCount() {
    ASM(
        nofralloc;
        lis r5, sInstance @ha;
        lwz r5, sInstance @l(r5);
        lbz r0, System.netMgr.racesPerGP(r5);
        blr;)
}

asmFunc System::GetNonTTGhostPlayersCount() {
    ASM(
        nofralloc;
        lis r12, sInstance @ha;
        lwz r12, sInstance @l(r12);
        lbz r29, System.nonTTGhostPlayersCount(r12);
        blr;)
}

// Unlock Everything Without Save (_tZ)
kmWrite32(0x80549974, 0x38600001);

// Skip ESRB page
kmRegionWrite32(0x80604094, 0x4800001c, 'E');

// Retro Rewind Pack ID
kmWrite32(0x800017D0, 0x0A);

// Retro Rewind Internal Version
kmWrite32(0x800017D4, 65);

const char System::pulsarString[] = "/Pulsar";
const char System::CommonAssets[] = "/CommonAssets.szs";
const char System::breff[] = "/Effect/Pulsar.breff";
const char System::breft[] = "/Effect/Pulsar.breft";
const char* System::ttModeFolders[] = {"150", "200", "150F", "200F"};

void FriendSelectPage_joinFriend(Pages::FriendInfo* _this, u32 animDir, float animLength) {
    Pulsar::System::sInstance->netMgr.region = RKNet::Controller::sInstance->friends[_this->selectedFriendIdx].statusData.regionId;
    return _this->EndStateAnimated(animDir, animLength);
}

kmCall(0x805d686c, FriendSelectPage_joinFriend);
kmCall(0x805d6754, FriendSelectPage_joinFriend);

}  // namespace Pulsar