#ifndef _PULSAR_RATING_SYNC_HPP_
#define _PULSAR_RATING_SYNC_HPP_

#include <kamek.hpp>

namespace Pulsar {
namespace PointRating {

void SetSyncReportingSuppressed(bool suppress);
void ReportCurrentRatings(u32 licenseId);
void StartLoginRatingDownload(s32 profileId, u32 licenseId);

}  // namespace PointRating
}  // namespace Pulsar

#endif