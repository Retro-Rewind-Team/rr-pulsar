#include <kamek.hpp>
#include <core/nw4r/ut/BinaryFileFormat.hpp>
#include <core/nw4r/snd/DVDSoundArchive.hpp>
#include <SlotExpansion/CupsConfig.hpp>

namespace Pulsar {
namespace Sound {

static char snesFanfarePath[0x100];

static bool StringsEqual(const char* lhs, const char* rhs) {
    if (lhs == nullptr || rhs == nullptr) return false;
    while (*lhs != '\0' && *rhs != '\0') {
        if (*lhs != *rhs) return false;
        ++lhs;
        ++rhs;
    }
    return *lhs == *rhs;
}

static bool IsSNESResultTrackName(const char* fileName) {
    if (fileName == nullptr) return false;
    if (StringsEqual(fileName, "sfcRR") || StringsEqual(fileName, "sfcRR.szs") ||
        StringsEqual(fileName, "5_1") || StringsEqual(fileName, "5_1.szs") ||
        StringsEqual(fileName, "11_1") || StringsEqual(fileName, "11_1.szs") || 
        StringsEqual(fileName, "sfcDP1") || StringsEqual(fileName, "sfcDP1.szs") ||
        StringsEqual(fileName, "sfcDP2") || StringsEqual(fileName, "sfcDP2.szs") || 
        StringsEqual(fileName, "trBC3") || StringsEqual(fileName, "trBC3.szs")) {
        return true;
    }

    u32 value = 0;
    const char* cursor = fileName;
    if (*cursor < '0' || *cursor > '9') return false;
    while (*cursor >= '0' && *cursor <= '9') {
        value = value * 10 + static_cast<u32>(*cursor - '0');
        ++cursor;
    }
    if (value > 19) return false;
    return *cursor == '\0' || StringsEqual(cursor, ".szs");
}

static bool IsSNESResultTrack() {
    const CupsConfig* cupsConfig = CupsConfig::sInstance;
    if (cupsConfig == nullptr) return false;

    const PulsarId track = cupsConfig->GetWinning();
    if (CupsConfig::IsReg(track)) return false;

    const u8 variantIdx = cupsConfig->GetCurVariantIdx();
    const char* fileName = cupsConfig->GetFileName(track, variantIdx);
    if (fileName == nullptr || fileName[0] == '\0') fileName = cupsConfig->GetFileName(track, 0);
    return IsSNESResultTrackName(fileName);
}

static const char* GetSNESFanfareName(const char* extFilePath) {
    if (extFilePath == nullptr) return nullptr;

    const char* fileName = extFilePath;
    for (const char* cursor = extFilePath; *cursor != '\0'; ++cursor) {
        if (*cursor == '/') fileName = cursor + 1;
    }

    if (StringsEqual(fileName, "o_FanfareGP1_32.brstm")) return "o_FanfareSNESGP1_32.brstm";
    if (StringsEqual(fileName, "o_FanfareGP2_32.brstm")) return "o_FanfareSNESGP2_32.brstm";
    if (StringsEqual(fileName, "o_FanfareGPdame_32.brstm")) return "o_FanfareSNESGPdame_32.brstm";
    return nullptr;
}

bool ResolveSNESFanfarePath(const nw4r::snd::DVDSoundArchive* archive, const char*& extFilePath) {
    const char* snesFanfare = GetSNESFanfareName(extFilePath);
    if (archive == nullptr || snesFanfare == nullptr || !IsSNESResultTrack()) return false;

    snprintf(snesFanfarePath, sizeof(snesFanfarePath), "%sstrm/%s", archive->extFileRoot, snesFanfare);
    extFilePath = snesFanfarePath;
    return true;
}

}  // namespace Sound
}  // namespace Pulsar
