#include <kamek.hpp>
#include <core/nw4r/ut/BinaryFileFormat.hpp>
#include <core/nw4r/snd/DVDSoundArchive.hpp>
#include <SlotExpansion/CupsConfig.hpp>

namespace Pulsar {
namespace Sound {

static char wiiACFanfarePath[0x100];

static bool StringsEqual(const char* lhs, const char* rhs) {
    if (lhs == nullptr || rhs == nullptr) return false;
    while (*lhs != '\0' && *rhs != '\0') {
        if (*lhs != *rhs) return false;
        ++lhs;
        ++rhs;
    }
    return *lhs == *rhs;
}

static bool IsWiiACResultTrack() {
    const CupsConfig* cupsConfig = CupsConfig::sInstance;
    if (cupsConfig == nullptr) return false;

    const PulsarId track = cupsConfig->GetWinning();
    if (CupsConfig::IsReg(track)) return false;

    const u8 variantIdx = cupsConfig->GetCurVariantIdx();
    const char* fileName = cupsConfig->GetFileName(track, variantIdx);
    if (fileName == nullptr || fileName[0] == '\0') fileName = cupsConfig->GetFileName(track, 0);
    return StringsEqual(fileName, "uAC") || StringsEqual(fileName, "uAC.szs");
}

static const char* GetWiiACFanfareName(const char* extFilePath) {
    if (extFilePath == nullptr) return nullptr;

    const char* fileName = extFilePath;
    for (const char* cursor = extFilePath; *cursor != '\0'; ++cursor) {
        if (*cursor == '/') fileName = cursor + 1;
    }

    if (StringsEqual(fileName, "o_FanfareGP1_32.brstm")) return "o_FanfareWiiUACGP1_32.brstm";
    if (StringsEqual(fileName, "o_FanfareGP2_32.brstm")) return "o_FanfareWiiUACGP2_32.brstm";
    if (StringsEqual(fileName, "o_FanfareGPdame_32.brstm")) return "o_FanfareWiiUACGPdame_32.brstm";
    return nullptr;
}

bool ResolveWiiACFanfarePath(const nw4r::snd::DVDSoundArchive* archive, const char*& extFilePath) {
    const char* wiiACFanfare = GetWiiACFanfareName(extFilePath);
    if (archive == nullptr || wiiACFanfare == nullptr || !IsWiiACResultTrack()) return false;

    snprintf(wiiACFanfarePath, sizeof(wiiACFanfarePath), "%sstrm/%s", archive->extFileRoot, wiiACFanfare);
    extFilePath = wiiACFanfarePath;
    return true;
}

}  // namespace Sound
}  // namespace Pulsar
