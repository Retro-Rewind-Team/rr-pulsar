#include <kamek.hpp>
#include <core/nw4r/ut/BinaryFileFormat.hpp>
#include <core/nw4r/snd/DVDSoundArchive.hpp>
#include <SlotExpansion/CupsConfig.hpp>

namespace Pulsar {
namespace Sound {

static char arcadeFanfarePath[0x100];

static bool StringsEqual(const char* lhs, const char* rhs) {
    if (lhs == nullptr || rhs == nullptr) return false;
    while (*lhs != '\0' && *rhs != '\0') {
        if (*lhs != *rhs) return false;
        ++lhs;
        ++rhs;
    }
    return *lhs == *rhs;
}

static bool IsArcadeResultTrack() {
    const CupsConfig* cupsConfig = CupsConfig::sInstance;
    if (cupsConfig == nullptr) return false;

    const PulsarId track = cupsConfig->GetWinning();
    if (CupsConfig::IsReg(track)) return false;

    const u8 variantIdx = cupsConfig->GetCurVariantIdx();
    const char* fileName = cupsConfig->GetFileName(track, variantIdx);
    if (fileName == nullptr || fileName[0] == '\0') fileName = cupsConfig->GetFileName(track, 0);
    return StringsEqual(fileName, "gpBR") || StringsEqual(fileName, "gpBR.szs") ||
           StringsEqual(fileName, "gpDC") || StringsEqual(fileName, "gpDC.szs") ||
           StringsEqual(fileName, "gpBC") || StringsEqual(fileName, "gpBC.szs");
}

static const char* GetArcadeFanfareName(const char* extFilePath) {
    if (extFilePath == nullptr) return nullptr;

    const char* fileName = extFilePath;
    for (const char* cursor = extFilePath; *cursor != '\0'; ++cursor) {
        if (*cursor == '/') fileName = cursor + 1;
    }

    if (StringsEqual(fileName, "o_FanfareGP1_32.brstm")) return "o_FanfareGPGP1_32.brstm";
    if (StringsEqual(fileName, "o_FanfareGP2_32.brstm")) return "o_FanfareGPGP2_32.brstm";
    if (StringsEqual(fileName, "o_FanfareGPdame_32.brstm")) return "o_FanfareGPGPdame_32.brstm";
    return nullptr;
}

bool ResolveArcadeFanfarePath(const nw4r::snd::DVDSoundArchive* archive, const char*& extFilePath) {
    const char* arcadeFanfare = GetArcadeFanfareName(extFilePath);
    if (archive == nullptr || arcadeFanfare == nullptr || !IsArcadeResultTrack()) return false;

    snprintf(arcadeFanfarePath, sizeof(arcadeFanfarePath), "%sstrm/%s", archive->extFileRoot, arcadeFanfare);
    extFilePath = arcadeFanfarePath;
    return true;
}

}  // namespace Sound
}  // namespace Pulsar
