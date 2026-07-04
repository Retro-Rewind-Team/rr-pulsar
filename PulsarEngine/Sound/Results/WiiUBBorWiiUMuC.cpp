#include <kamek.hpp>
#include <core/nw4r/ut/BinaryFileFormat.hpp>
#include <core/nw4r/snd/DVDSoundArchive.hpp>
#include <SlotExpansion/CupsConfig.hpp>

namespace Pulsar {
namespace Sound {

static char wiiUBBFanfarePath[0x100];

static bool StringsEqual(const char* lhs, const char* rhs) {
    if (lhs == nullptr || rhs == nullptr) return false;
    while (*lhs != '\0' && *rhs != '\0') {
        if (*lhs != *rhs) return false;
        ++lhs;
        ++rhs;
    }
    return *lhs == *rhs;
}

static bool IsWiiUBBResultTrack() {
    const CupsConfig* cupsConfig = CupsConfig::sInstance;
    if (cupsConfig == nullptr) return false;

    const PulsarId track = cupsConfig->GetWinning();
    if (CupsConfig::IsReg(track)) return false;

    const u8 variantIdx = cupsConfig->GetCurVariantIdx();
    const char* fileName = cupsConfig->GetFileName(track, variantIdx);
    if (fileName == nullptr || fileName[0] == '\0') fileName = cupsConfig->GetFileName(track, 0);
    return StringsEqual(fileName, "uBB") || StringsEqual(fileName, "uBB.szs") ||
           StringsEqual(fileName, "uMuC") || StringsEqual(fileName, "uMuC.szs");
}

static const char* GetWiiUBBFanfareName(const char* extFilePath) {
    if (extFilePath == nullptr) return nullptr;

    const char* fileName = extFilePath;
    for (const char* cursor = extFilePath; *cursor != '\0'; ++cursor) {
        if (*cursor == '/') fileName = cursor + 1;
    }

    if (StringsEqual(fileName, "o_FanfareGP1_32.brstm")) return "o_FanfareWiiUBBGP1_32.brstm";
    if (StringsEqual(fileName, "o_FanfareGP2_32.brstm")) return "o_FanfareWiiUBBGP2_32.brstm";
    if (StringsEqual(fileName, "o_FanfareGPdame_32.brstm")) return "o_FanfareWiiUBBGPdame_32.brstm";
    return nullptr;
}

bool ResolveWiiUBBFanfarePath(const nw4r::snd::DVDSoundArchive* archive, const char*& extFilePath) {
    const char* wiiUBBFanfare = GetWiiUBBFanfareName(extFilePath);
    if (archive == nullptr || wiiUBBFanfare == nullptr || !IsWiiUBBResultTrack()) return false;

    snprintf(wiiUBBFanfarePath, sizeof(wiiUBBFanfarePath), "%sstrm/%s", archive->extFileRoot, wiiUBBFanfare);
    extFilePath = wiiUBBFanfarePath;
    return true;
}

}  // namespace Sound
}  // namespace Pulsar
