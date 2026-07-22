#include <kamek.hpp>
#include <core/nw4r/ut/BinaryFileFormat.hpp>
#include <core/nw4r/snd/DVDSoundArchive.hpp>
#include <SlotExpansion/CupsConfig.hpp>

namespace Pulsar {
namespace Sound {

static char gcnFanfarePath[0x100];

static bool StringsEqual(const char* lhs, const char* rhs) {
    if (lhs == nullptr || rhs == nullptr) return false;
    while (*lhs != '\0' && *rhs != '\0') {
        if (*lhs != *rhs) return false;
        ++lhs;
        ++rhs;
    }
    return *lhs == *rhs;
}

static bool IsGCNResultTrackName(const char* fileName) {
    if (fileName == nullptr) return false;
    if (StringsEqual(fileName, "sw2PB") || StringsEqual(fileName, "sw2PB.szs") ||
        StringsEqual(fileName, "uBP") || StringsEqual(fileName, "uBP.szs") ||
        StringsEqual(fileName, "urDDD") || StringsEqual(fileName, "urDDD.szs") ||
        StringsEqual(fileName, "uSL") || StringsEqual(fileName, "uSL.szs")) {
        return true;
    }

    u32 value = 0;
    const char* cursor = fileName;
    if (*cursor < '0' || *cursor > '9') return false;
    while (*cursor >= '0' && *cursor <= '9') {
        value = value * 10 + static_cast<u32>(*cursor - '0');
        ++cursor;
    }
    if (value < 56 || value > 71) return false;
    return *cursor == '\0' || StringsEqual(cursor, ".szs");
}

static bool IsGCNResultTrack() {
    const CupsConfig* cupsConfig = CupsConfig::sInstance;
    if (cupsConfig == nullptr) return false;

    const PulsarId track = cupsConfig->GetWinning();
    if (CupsConfig::IsReg(track)) return false;

    const u8 variantIdx = cupsConfig->GetCurVariantIdx();
    const char* fileName = cupsConfig->GetFileName(track, variantIdx);
    if (fileName == nullptr || fileName[0] == '\0') fileName = cupsConfig->GetFileName(track, 0);
    return IsGCNResultTrackName(fileName);
}

static const char* GetGCNFanfareName(const char* extFilePath) {
    if (extFilePath == nullptr) return nullptr;

    const char* fileName = extFilePath;
    for (const char* cursor = extFilePath; *cursor != '\0'; ++cursor) {
        if (*cursor == '/') fileName = cursor + 1;
    }

    if (StringsEqual(fileName, "o_FanfareGP1_32.brstm")) return "o_FanfareGCNGP1_32.brstm";
    if (StringsEqual(fileName, "o_FanfareGP2_32.brstm")) return "o_FanfareGCNGP2_32.brstm";
    if (StringsEqual(fileName, "o_FanfareGPdame_32.brstm")) return "o_FanfareGCNGPdame_32.brstm";
    return nullptr;
}

bool ResolveGCNFanfarePath(const nw4r::snd::DVDSoundArchive* archive, const char*& extFilePath) {
    const char* gcnFanfare = GetGCNFanfareName(extFilePath);
    if (archive == nullptr || gcnFanfare == nullptr || !IsGCNResultTrack()) return false;

    snprintf(gcnFanfarePath, sizeof(gcnFanfarePath), "%sstrm/%s", archive->extFileRoot, gcnFanfare);
    extFilePath = gcnFanfarePath;
    return true;
}

}  // namespace Sound
}  // namespace Pulsar
