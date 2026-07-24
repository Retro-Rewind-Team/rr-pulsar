#ifndef _PULSAR_MOGI_RATING_HPP_
#define _PULSAR_MOGI_RATING_HPP_

#include <kamek.hpp>

namespace Pulsar {
namespace MogiRating {

enum MMRMode {
    MMR_MODE_RETRO = 0,
    MMR_MODE_CT = 1,
    MMR_MODE_REGULAR = 2,
    MMR_MODE_COUNT = 3
};

static const float MIN_MMR = 1.0f;
static const float MAX_MMR = 300.0f;
static const float DEFAULT_MMR = 10.0f;  // Internal centi-MMR value displayed as 1000.

MMRMode GetCurrentMode();
float GetUserMMR(u32 licenseId);
float GetUserMMRForMode(u32 licenseId, MMRMode mode);
float GetStoredMMR(s32 profileId);
float GetStoredMMRForMode(s32 profileId, MMRMode mode);
void SetUserMMR(u32 licenseId, float mmr);
void SetProfileMMR(s32 profileId, MMRMode mode, float mmr);
void BindLicenseProfileId(u32 licenseId, s32 profileId);
void ReportCurrentMMR(u32 licenseId);

}  // namespace MogiRating
}  // namespace Pulsar

#endif
