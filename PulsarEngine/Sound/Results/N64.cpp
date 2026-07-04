#include <kamek.hpp>
#include <core/nw4r/ut/BinaryFileFormat.hpp>
#include <core/nw4r/snd/DVDSoundArchive.hpp>
#include <SlotExpansion/CupsConfig.hpp>

namespace Pulsar {
namespace Sound {

static char n64FanfarePath[0x100];

static bool StringsEqual(const char* lhs, const char* rhs) {
    if (lhs == nullptr || rhs == nullptr) return false;
    while (*lhs != '\0' && *rhs != '\0') {
        if (*lhs != *rhs) return false;
        ++lhs;
        ++rhs;
    }
    return *lhs == *rhs;
}

static bool IsN64ResultTrackName(const char* fileName) {
    if (fileName == nullptr) return false;
    if (StringsEqual(fileName, "tKD2") || StringsEqual(fileName, "tKD2.szs") ||
        StringsEqual(fileName, "trCM") || StringsEqual(fileName, "trCM.szs") ||
        StringsEqual(fileName, "urRR") || StringsEqual(fileName, "urRR.szs")) {
        return true;
    }

    u32 value = 0;
    const char* cursor = fileName;
    if (*cursor < '0' || *cursor > '9') return false;
    while (*cursor >= '0' && *cursor <= '9') {
        value = value * 10 + static_cast<u32>(*cursor - '0');
        ++cursor;
    }
    if (value < 20 || value > 35) return false;
    return *cursor == '\0' || StringsEqual(cursor, ".szs");
}

static bool IsN64ResultTrack() {
    const CupsConfig* cupsConfig = CupsConfig::sInstance;
    if (cupsConfig == nullptr) return false;

    const PulsarId track = cupsConfig->GetWinning();
    if (CupsConfig::IsReg(track)) return false;

    const u8 variantIdx = cupsConfig->GetCurVariantIdx();
    const char* fileName = cupsConfig->GetFileName(track, variantIdx);
    if (fileName == nullptr || fileName[0] == '\0') fileName = cupsConfig->GetFileName(track, 0);
    return IsN64ResultTrackName(fileName);
}

static const char* GetN64FanfareName(const char* extFilePath) {
    if (extFilePath == nullptr) return nullptr;

    const char* fileName = extFilePath;
    for (const char* cursor = extFilePath; *cursor != '\0'; ++cursor) {
        if (*cursor == '/') fileName = cursor + 1;
    }

    if (StringsEqual(fileName, "o_FanfareGP1_32.brstm")) return "o_FanfareN64GP1_32.brstm";
    if (StringsEqual(fileName, "o_FanfareGP2_32.brstm")) return "o_FanfareN64GP2_32.brstm";
    if (StringsEqual(fileName, "o_FanfareGPdame_32.brstm")) return "o_FanfareN64GPdame_32.brstm";
    return nullptr;
}

bool ResolveN64FanfarePath(const nw4r::snd::DVDSoundArchive* archive, const char*& extFilePath) {
    const char* n64Fanfare = GetN64FanfareName(extFilePath);
    if (archive == nullptr || n64Fanfare == nullptr || !IsN64ResultTrack()) return false;

    snprintf(n64FanfarePath, sizeof(n64FanfarePath), "%sstrm/%s", archive->extFileRoot, n64Fanfare);
    extFilePath = n64FanfarePath;
    return true;
}

}  // namespace Sound
}  // namespace Pulsar
