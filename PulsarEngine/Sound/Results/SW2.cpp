#include <kamek.hpp>
#include <core/nw4r/ut/BinaryFileFormat.hpp>
#include <core/nw4r/snd/DVDSoundArchive.hpp>
#include <SlotExpansion/CupsConfig.hpp>

namespace Pulsar {
namespace Sound {

static char sw2FanfarePath[0x100];

static bool StringsEqual(const char* lhs, const char* rhs) {
    if (lhs == nullptr || rhs == nullptr) return false;
    while (*lhs != '\0' && *rhs != '\0') {
        if (*lhs != *rhs) return false;
        ++lhs;
        ++rhs;
    }
    return *lhs == *rhs;
}

static bool IsSW2ResultTrackName(const char* fileName) {
    if (fileName == nullptr) return false;

    static const char* const names[] = {
        "sw2MBC", "sw2CC", "sw2WS", "sw2DKS", "sw2FO", "sw2PR", "sw2QBR",
        "sw2CCF", "sw2DD", "sw2BooC", "sw2GV", "sw2BC", "sw2MC", "sw2PS"
    };
    for (u32 i = 0; i < sizeof(names) / sizeof(names[0]); ++i) {
        if (StringsEqual(fileName, names[i])) return true;

        char nameWithExtension[16];
        snprintf(nameWithExtension, sizeof(nameWithExtension), "%s.szs", names[i]);
        if (StringsEqual(fileName, nameWithExtension)) return true;
    }
    return false;
}

static bool IsSW2ResultTrack() {
    const CupsConfig* cupsConfig = CupsConfig::sInstance;
    if (cupsConfig == nullptr) return false;

    const PulsarId track = cupsConfig->GetWinning();
    if (CupsConfig::IsReg(track)) return false;

    const u8 variantIdx = cupsConfig->GetCurVariantIdx();
    const char* fileName = cupsConfig->GetFileName(track, variantIdx);
    if (fileName == nullptr || fileName[0] == '\0') fileName = cupsConfig->GetFileName(track, 0);
    return IsSW2ResultTrackName(fileName);
}

static const char* GetSW2FanfareName(const char* extFilePath) {
    if (extFilePath == nullptr) return nullptr;

    const char* fileName = extFilePath;
    for (const char* cursor = extFilePath; *cursor != '\0'; ++cursor) {
        if (*cursor == '/') fileName = cursor + 1;
    }

    if (StringsEqual(fileName, "o_FanfareGP1_32.brstm")) return "o_FanfareSW2GP1_32.brstm";
    if (StringsEqual(fileName, "o_FanfareGP2_32.brstm")) return "o_FanfareSW2GP2_32.brstm";
    if (StringsEqual(fileName, "o_FanfareGPdame_32.brstm")) return "o_FanfareSW2GPdame_32.brstm";
    return nullptr;
}

bool ResolveSW2FanfarePath(const nw4r::snd::DVDSoundArchive* archive, const char*& extFilePath) {
    const char* sw2Fanfare = GetSW2FanfareName(extFilePath);
    if (archive == nullptr || sw2Fanfare == nullptr || !IsSW2ResultTrack()) return false;

    snprintf(sw2FanfarePath, sizeof(sw2FanfarePath), "%sstrm/%s", archive->extFileRoot, sw2Fanfare);
    extFilePath = sw2FanfarePath;
    return true;
}

}  // namespace Sound
}  // namespace Pulsar
