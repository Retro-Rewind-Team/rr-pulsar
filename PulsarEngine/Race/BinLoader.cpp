#include <RetroRewind.hpp>
#include <MarioKartWii/Archive/ArchiveMgr.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <GlobalFunction.hpp>
#include <core/rvl/OS/OS.hpp>
#if defined(PROD)
#include <Security/BinVerifier.hpp>
#endif

namespace RetroRewind {

void *GetCustomKartParam(ArchiveMgr *archive, ArchiveSource type, const char *name, u32 *length) {
#if defined(PROD)
    AntiCheat::VerifyBinFile(name);
#endif
    return archive->GetFile(type, name, length);
}
kmCall(0x80591a30, GetCustomKartParam);

void *GetCustomKartAIParam(ArchiveMgr *archive, ArchiveSource type, const char *name, u32 *length) {
    const bool offlineHardAI = Pulsar::IsOfflineRoom() && static_cast<Pulsar::HardAI>(Pulsar::Settings::Mgr::Get().GetSettingValue(Pulsar::Settings::SETTING_HARDAI)) == Pulsar::HARDAI_ENABLED;
    const bool froomHardAI = Pulsar::IsFriendRoom() && System::sInstance->IsContext(Pulsar::PULSAR_FROOMCPUS) && System::sInstance->IsContext(Pulsar::PULSAR_FROOMCPUSHARD);
    if (offlineHardAI || froomHardAI) {
        name = "kartAISpdParamRR.bin";
    }

    return archive->GetFile(type, name, length);
}
kmCall(0x8073ae9c, GetCustomKartAIParam);

void *GetCustomItemSlot(ArchiveMgr *archive, ArchiveSource type, const char *name, u32 *length) {
    const bool itemModeRandom = System::sInstance->IsContext(Pulsar::PULSAR_ITEMMODERANDOM);
    const bool itemModeNone = System::sInstance->IsContext(Pulsar::PULSAR_ITEMMODENONE);
    if (!itemModeRandom && !itemModeNone) {
        name = "ItemSlotRR.bin";
    }
    if (itemModeRandom) {
        name = "ItemSlotRandom.bin";
    }
    if (itemModeNone) {
        name = "ItemSlot.bin";
    }

#if defined(PROD)
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
