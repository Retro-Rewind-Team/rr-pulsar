#include <kamek.hpp>
#include <core/nw4r/ut/BinaryFileFormat.hpp>
#include <core/nw4r/snd/DVDSoundArchive.hpp>
#include <SlotExpansion/CupsConfig.hpp>

namespace Pulsar {
namespace Sound {

static char dsFanfarePath[0x100];

static bool StringsEqual(const char* lhs, const char* rhs) {
    if (lhs == nullptr || rhs == nullptr) return false;
    while (*lhs != '\0' && *rhs != '\0') {
        if (*lhs != *rhs) return false;
        ++lhs;
        ++rhs;
    }
    return *lhs == *rhs;
}

static bool IsNamedDSResultTrack(const char* fileName) {
    static const char* const names[] = {"dsDS", "dsWP", "tSR", "sw2DKP", "uTTC", "uWS", "swPG"};
    for (u32 i = 0; i < sizeof(names) / sizeof(names[0]); ++i) {
        if (StringsEqual(fileName, names[i])) return true;

        char nameWithExtension[16];
        snprintf(nameWithExtension, sizeof(nameWithExtension), "%s.szs", names[i]);
        if (StringsEqual(fileName, nameWithExtension)) return true;
    }
    return false;
}

static bool IsDSResultTrackName(const char* fileName) {
    if (fileName == nullptr) return false;
    if (IsNamedDSResultTrack(fileName)) return true;

    u32 value = 0;
    const char* cursor = fileName;
    if (*cursor < '0' || *cursor > '9') return false;
    while (*cursor >= '0' && *cursor <= '9') {
        value = value * 10 + static_cast<u32>(*cursor - '0');
        ++cursor;
    }
    if (value < 72 || value > 87) return false;
    return *cursor == '\0' || StringsEqual(cursor, ".szs");
}

static bool IsDSResultTrack() {
    const CupsConfig* cupsConfig = CupsConfig::sInstance;
    if (cupsConfig == nullptr) return false;

    const PulsarId track = cupsConfig->GetWinning();
    if (CupsConfig::IsReg(track)) return false;

    const u8 variantIdx = cupsConfig->GetCurVariantIdx();
    const char* fileName = cupsConfig->GetFileName(track, variantIdx);
    if (fileName == nullptr || fileName[0] == '\0') fileName = cupsConfig->GetFileName(track, 0);
    return IsDSResultTrackName(fileName);
}

static const char* GetDSFanfareName(const char* extFilePath) {
    if (extFilePath == nullptr) return nullptr;

    const char* fileName = extFilePath;
    for (const char* cursor = extFilePath; *cursor != '\0'; ++cursor) {
        if (*cursor == '/') fileName = cursor + 1;
    }

    if (StringsEqual(fileName, "o_FanfareGP1_32.brstm")) return "o_FanfareDSGP1_32.brstm";
    if (StringsEqual(fileName, "o_FanfareGP2_32.brstm")) return "o_FanfareDSGP2_32.brstm";
    if (StringsEqual(fileName, "o_FanfareGPdame_32.brstm")) return "o_FanfareDSGPdame_32.brstm";
    return nullptr;
}

bool ResolveDSFanfarePath(const nw4r::snd::DVDSoundArchive* archive, const char*& extFilePath) {
    const char* dsFanfare = GetDSFanfareName(extFilePath);
    if (archive == nullptr || dsFanfare == nullptr || !IsDSResultTrack()) return false;

    snprintf(dsFanfarePath, sizeof(dsFanfarePath), "%sstrm/%s", archive->extFileRoot, dsFanfare);
    extFilePath = dsFanfarePath;
    return true;
}

}  // namespace Sound
}  // namespace Pulsar
