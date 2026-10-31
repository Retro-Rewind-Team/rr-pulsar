#ifndef _PULSAR_PLAYER_RATING_HPP_
#define _PULSAR_PLAYER_RATING_HPP_

#include <kamek.hpp>

class RacedataScenario;

namespace Pulsar {
namespace PointRating {

// Ratings and deltas use displayed VR; vanilla fields store VR / 100.
static const u32 MIN_RATING = 100;
static const u32 MAX_RATING = 1000000;
static const float DEFAULT_RATING = 5000.0f;

float GetUserVR(u32 licenseId);
float GetUserBR(u32 licenseId);
void SetUserVR(u32 licenseId, float vr);
void SetUserBR(u32 licenseId, float br);
void BindLicenseProfileId(u32 licenseId, s32 profileId);
void SaveProfileVR(s32 profileId, float vr);
void SaveProfileBR(s32 profileId, float br);
void UpdatePoints(RacedataScenario *scenario);

extern u8 remoteDecimalVR[12][2];
extern float lastRaceDeltas[12];

float GetMultiplier();
bool IsWeekendMultiplierActiveForRegion(u8 region);

}  // namespace PointRating
}  // namespace Pulsar

#endif  // _PULSAR_PLAYER_RATING_HPP_
