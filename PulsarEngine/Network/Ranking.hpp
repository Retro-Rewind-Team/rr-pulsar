// SPDX-License-Identifier: AGPL-3.0-or-later
// This file is part of Retro Rewind. Licensed under AGPLv3. See LICENSE_AGPLv3.
#ifndef PULSAR_NETWORK_RANKING_HPP
#define PULSAR_NETWORK_RANKING_HPP

#include <kamek.hpp>

namespace Pulsar {
namespace Ranking {

int GetCurrentLicenseRankVS();
int GetCurrentLicenseScore();
int FormatRankMessage(wchar_t* dst, size_t dstLen);

}  // namespace Ranking
}  // namespace Pulsar

#endif  // PULSAR_NETWORK_RANKING_HPP
