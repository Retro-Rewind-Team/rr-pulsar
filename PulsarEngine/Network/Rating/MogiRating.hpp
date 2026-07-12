#ifndef _PULSAR_MOGI_RATING_HPP_
#define _PULSAR_MOGI_RATING_HPP_

#include <kamek.hpp>

namespace Pulsar {
namespace MogiRating {

static const float MIN_MMR = 1.0f;
static const float MAX_MMR = 300.0f;
static const float DEFAULT_MMR = 10.0f;

float GetUserMMR(u32 licenseId);
void SetUserMMR(u32 licenseId, float mmr);
void SaveProfileMMR(s32 profileId, float mmr);
void BindLicenseProfileId(u32 licenseId, s32 profileId);
void ReportCurrentMMR(u32 licenseId);

}  // namespace MogiRating
}  // namespace Pulsar

#endif
