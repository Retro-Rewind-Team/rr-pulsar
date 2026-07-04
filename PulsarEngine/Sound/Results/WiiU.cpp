#include <kamek.hpp>
#include <core/nw4r/ut/BinaryFileFormat.hpp>
#include <core/nw4r/snd/DVDSoundArchive.hpp>
#include <SlotExpansion/CupsConfig.hpp>

namespace Pulsar {
namespace Sound {

static char wiiUFanfarePath[0x100];

static bool StringsEqual(const char* lhs, const char* rhs) {
    if (lhs == nullptr || rhs == nullptr) return false;
    while (*lhs != '\0' && *rhs != '\0') {
        if (*lhs != *rhs) return false;
        ++lhs;
        ++rhs;
    }
    return *lhs == *rhs;
}

static bool IsWiiUResultTrackName(const char* fileName) {
    if (fileName == nullptr) return false;

    static const char* const names[] = {
        "uMKS", "uSSC", "uTR", "uMC", "uTH", "uSSA", "uE", "uCC",
        "uBDD", "uRR", "uEA", "uHC", "uWW", "uSS", "swSHS"
    };
    for (u32 i = 0; i < sizeof(names) / sizeof(names[0]); ++i) {
        if (StringsEqual(fileName, names[i])) return true;

        char nameWithExtension[16];
        snprintf(nameWithExtension, sizeof(nameWithExtension), "%s.szs", names[i]);
        if (StringsEqual(fileName, nameWithExtension)) return true;
    }
    return false;
}

static bool IsWiiUResultTrack() {
    const CupsConfig* cupsConfig = CupsConfig::sInstance;
    if (cupsConfig == nullptr) return false;

    const PulsarId track = cupsConfig->GetWinning();
    if (CupsConfig::IsReg(track)) return false;

    const u8 variantIdx = cupsConfig->GetCurVariantIdx();
    const char* fileName = cupsConfig->GetFileName(track, variantIdx);
    if (fileName == nullptr || fileName[0] == '\0') fileName = cupsConfig->GetFileName(track, 0);
    return IsWiiUResultTrackName(fileName);
}

static const char* GetWiiUFanfareName(const char* extFilePath) {
    if (extFilePath == nullptr) return nullptr;

    const char* fileName = extFilePath;
    for (const char* cursor = extFilePath; *cursor != '\0'; ++cursor) {
        if (*cursor == '/') fileName = cursor + 1;
    }

    if (StringsEqual(fileName, "o_FanfareGP1_32.brstm")) return "o_FanfareWiiUGP1_32.brstm";
    if (StringsEqual(fileName, "o_FanfareGP2_32.brstm")) return "o_FanfareWiiUGP2_32.brstm";
    if (StringsEqual(fileName, "o_FanfareGPdame_32.brstm")) return "o_FanfareWiiUGPdame_32.brstm";
    return nullptr;
}

bool ResolveWiiUFanfarePath(const nw4r::snd::DVDSoundArchive* archive, const char*& extFilePath) {
    const char* wiiUFanfare = GetWiiUFanfareName(extFilePath);
    if (archive == nullptr || wiiUFanfare == nullptr || !IsWiiUResultTrack()) return false;

    snprintf(wiiUFanfarePath, sizeof(wiiUFanfarePath), "%sstrm/%s", archive->extFileRoot, wiiUFanfare);
    extFilePath = wiiUFanfarePath;
    return true;
}

}  // namespace Sound
}  // namespace Pulsar
