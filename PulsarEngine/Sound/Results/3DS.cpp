#include <kamek.hpp>
#include <core/nw4r/ut/BinaryFileFormat.hpp>
#include <core/nw4r/snd/DVDSoundArchive.hpp>
#include <SlotExpansion/CupsConfig.hpp>

namespace Pulsar {
namespace Sound {

static char threeDSFanfarePath[0x100];

static bool StringsEqual(const char* lhs, const char* rhs) {
    if (lhs == nullptr || rhs == nullptr) return false;
    while (*lhs != '\0' && *rhs != '\0') {
        if (*lhs != *rhs) return false;
        ++lhs;
        ++rhs;
    }
    return *lhs == *rhs;
}

static bool Is3DSResultTrackName(const char* fileName) {
    if (fileName == nullptr) return false;

    u32 value = 0;
    const char* cursor = fileName;
    if (*cursor < '0' || *cursor > '9') return false;
    while (*cursor >= '0' && *cursor <= '9') {
        value = value * 10 + static_cast<u32>(*cursor - '0');
        ++cursor;
    }
    if (value < 104 || value > 119) return false;
    return *cursor == '\0' || StringsEqual(cursor, ".szs");
}

static bool Is3DSResultTrack() {
    const CupsConfig* cupsConfig = CupsConfig::sInstance;
    if (cupsConfig == nullptr) return false;

    const PulsarId track = cupsConfig->GetWinning();
    if (CupsConfig::IsReg(track)) return false;

    const u8 variantIdx = cupsConfig->GetCurVariantIdx();
    const char* fileName = cupsConfig->GetFileName(track, variantIdx);
    if (fileName == nullptr || fileName[0] == '\0') fileName = cupsConfig->GetFileName(track, 0);
    return Is3DSResultTrackName(fileName);
}

static const char* Get3DSFanfareName(const char* extFilePath) {
    if (extFilePath == nullptr) return nullptr;

    const char* fileName = extFilePath;
    for (const char* cursor = extFilePath; *cursor != '\0'; ++cursor) {
        if (*cursor == '/') fileName = cursor + 1;
    }

    if (StringsEqual(fileName, "o_FanfareGP1_32.brstm")) return "o_Fanfare3DSGP1_32.brstm";
    if (StringsEqual(fileName, "o_FanfareGP2_32.brstm")) return "o_Fanfare3DSGP2_32.brstm";
    if (StringsEqual(fileName, "o_FanfareGPdame_32.brstm")) return "o_Fanfare3DSGPdame_32.brstm";
    return nullptr;
}

bool Resolve3DSFanfarePath(const nw4r::snd::DVDSoundArchive* archive, const char*& extFilePath) {
    const char* threeDSFanfare = Get3DSFanfareName(extFilePath);
    if (archive == nullptr || threeDSFanfare == nullptr || !Is3DSResultTrack()) return false;

    snprintf(threeDSFanfarePath, sizeof(threeDSFanfarePath), "%sstrm/%s", archive->extFileRoot, threeDSFanfare);
    extFilePath = threeDSFanfarePath;
    return true;
}

}  // namespace Sound
}  // namespace Pulsar
