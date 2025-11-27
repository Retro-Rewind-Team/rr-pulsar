/*
    PlayerRating.hpp
    Copyright (C) 2025 ZPL

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as
    published by the Free Software Foundation, either version 3 of the
    License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#pragma once

#include <kamek.hpp>

namespace Pulsar {
namespace PointRating {

struct PlayerRating {
    u16 points;
    u8 padding[2];
};

static const u16 MinRating = 1;
static const u16 MaxRating = 10000;

float GetUserVR(u32 licenseId);
float GetUserBR(u32 licenseId);
void SetUserVR(u32 licenseId, float vr);
void SetUserBR(u32 licenseId, float br);

extern u8 remoteDecimalVR[12][2];
extern float lastRaceDeltas[12];
float GetRatingMultiplier();

}  // namespace PointRating
}  // namespace Pulsar
