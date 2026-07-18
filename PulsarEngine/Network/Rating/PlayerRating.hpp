#ifndef _PULSAR_PLAYER_RATING_HPP_
#define _PULSAR_PLAYER_RATING_HPP_

#include <kamek.hpp>

class RacedataScenario;

namespace Pulsar {
namespace PointRating {

static const u16 MIN_RATING = 1;
static const u16 MAX_RATING = 10000;
static const float DEFAULT_RATING = 50.0f;

float GetUserVR(u32 licenseId);
float GetUserBR(u32 licenseId);
void SetUserVR(u32 licenseId, float vr);
void SetUserBR(u32 licenseId, float br);
void BindLicenseProfileId(u32 licenseId, s32 profileId);
void SaveProfileVR(s32 profileId, float vr);
void SaveProfileBR(s32 profileId, float br);
void FormatRatingDigits(float rating, wchar_t* buffer, u32 bufferSize);
void UpdatePoints(RacedataScenario* scenario);

extern u8 remoteDecimalVR[12][2];
extern float lastRaceDeltas[12];

float GetMultiplier();
bool IsWeekendMultiplierActive();
bool IsWeekendMultiplierActiveForRegion(u8 region);
bool IsItemRainEventActive();

}  // namespace PointRating
}  // namespace Pulsar

#endif  // _PULSAR_PLAYER_RATING_HPP_
