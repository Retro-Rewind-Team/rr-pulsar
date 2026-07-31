// SPDX-License-Identifier: AGPL-3.0-or-later
// This file is part of Retro Rewind. Licensed under AGPLv3. See LICENSE_AGPLv3.
#ifndef PULSAR_NETWORK_RANKING_HPP
#define PULSAR_NETWORK_RANKING_HPP

#include <kamek.hpp>

namespace Pulsar {
namespace Ranking {

static const char* BADGE_URL = "http://rwfc.net/api/badges/all";
static const u32 BADGE_REQUEST_WORK_BUF_SIZE = 0x4000;

int GetCurrentLicenseRankVS();
int GetCurrentLicenseScore();
int FormatRankMessage(wchar_t* dst, size_t dstLen);
int FormatRankDetailsMessage(wchar_t* dst, size_t dstLen);

enum BadgeType {
    BADGE_RETRO_REWIND_DEVELOPER = 0,
    BADGE_WHEEL_WIZARD_DEVELOPER,
    BADGE_MAJOR_CONTRIBUTOR,

    BADGE_RWFC_MODERATOR = 100,
    BADGE_DISCORD_STAFF,

    BADGE_CONTRIBUTOR = 1000,
    BADGE_TRANSLATOR,
    BADGE_SUPPORTER,
    BADGE_BETA_TESTER,
    BADGE_HEART,

    BADGE_FIRESTARTER_GOLD = 2000,
    BADGE_FIRESTARTER_SILVER,
    BADGE_FIRESTARTER_BRONZE,
    BADGE_LEAFSTRUCK_GOLD,
    BADGE_LEAFSTRUCK_SILVER,
    BADGE_LEAFSTRUCK_BRONZE,
    BADGE_SUMMIT_SHOWDOWN_GOLD,
    BADGE_SUMMIT_SHOWDOWN_SILVER,
    BADGE_SUMMIT_SHOWDOWN_BRONZE,
    BADGE_HORIZON_GOLD,
    BADGE_HORIZON_SILVER,
    BADGE_HORIZON_BRONZE,
    BADGE_SUNBLOSSOM_GOLD,
    BADGE_SUNBLOSSOM_SILVER,
    BADGE_SUNBLOSSOM_BRONZE,
    BADGE_EARTHBOUND_GOLD,
    BADGE_EARTHBOUND_SILVER,
    BADGE_EARTHBOUND_BRONZE
};

}  // namespace Ranking
}  // namespace Pulsar

#endif  // PULSAR_NETWORK_RANKING_HPP
