#include <RetroRewind.hpp>
#include <MarioKartWii/Archive/ArchiveMgr.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <core/rvl/OS/OS.hpp>
#ifdef PROD
#include <Security/BinVerifier.hpp>
#endif

namespace RetroRewind {

void *GetCustomKartAIParam(ArchiveMgr *archive, ArchiveSource type, const char *name, u32 *length) {
    const GameMode gameMode = Racedata::sInstance->racesScenario.settings.gamemode;
    if (static_cast<Pulsar::HardAI>(Pulsar::Settings::Mgr::Get().GetUserSettingValue(static_cast<Pulsar::Settings::UserType>(Pulsar::Settings::SETTINGSTYPE_RACE1), Pulsar::RADIO_HARDAI)) == Pulsar::HARDAI_ENABLED) {
        name = "kartAISpdParamRR.bin";
    }

    return archive->GetFile(type, name, length);
}
kmCall(0x8073ae9c, GetCustomKartAIParam);

void *GetCustomItemSlot(ArchiveMgr *archive, ArchiveSource type, const char *name, u32 *length) {
    const RacedataScenario &scenario = Racedata::sInstance->racesScenario;
    const GameMode mode = scenario.settings.gamemode;
    bool itemModeRandom = System::sInstance->IsContext(Pulsar::PULSAR_ITEMMODERANDOM) ? Pulsar::GAMEMODE_RANDOM : Pulsar::GAMEMODE_DEFAULT;
    bool itemModeBlast = System::sInstance->IsContext(Pulsar::PULSAR_ITEMMODEBLAST) ? Pulsar::GAMEMODE_BLAST : Pulsar::GAMEMODE_DEFAULT;
    if (itemModeRandom == Pulsar::GAMEMODE_DEFAULT || itemModeBlast == Pulsar::GAMEMODE_DEFAULT) {
        name = "ItemSlotRR.bin";
    }
    if (itemModeRandom == Pulsar::GAMEMODE_RANDOM) {
        name = "ItemSlotRandom.bin";
    }
    if (itemModeBlast == Pulsar::GAMEMODE_BLAST) {
        name = "ItemSlotBlast.bin";
    }

#ifdef PROD
    AntiCheat::VerifyBinFile(name);
#endif

    return archive->GetFile(type, name, length);
}
kmCall(0x807bb128, GetCustomItemSlot);
kmCall(0x807bb030, GetCustomItemSlot);
kmCall(0x807bb200, GetCustomItemSlot);
kmCall(0x807bb53c, GetCustomItemSlot);
kmCall(0x807bbb58, GetCustomItemSlot);
kmCall(0x807bbdd4, GetCustomItemSlot);
kmCall(0x807bbf50, GetCustomItemSlot);

}  // namespace RetroRewind
