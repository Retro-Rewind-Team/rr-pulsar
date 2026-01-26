/*
    ExpWWLeaderboard.cpp
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

#include <kamek.hpp>
#include <runtimeWrite.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <MarioKartWii/RKSYS/RKSYSMgr.hpp>
#include <MarioKartWii/Race/RaceData.hpp>
#include <MarioKartWii/Race/RaceInfo/RaceInfo.hpp>
#include <MarioKartWii/UI/Ctrl/CtrlRace/CtrlRaceResult.hpp>
#include <MarioKartWii/UI/Page/Leaderboard/WWLeaderboardUpdate.hpp>
#include <MarioKartWii/UI/Page/Page.hpp>
#include <MarioKartWii/UI/Section/Section.hpp>
#include <MarioKartWii/UI/Section/SectionMgr.hpp>
#include <Network/Rating/PlayerRating.hpp>
#include <Network/PacketExpansion.hpp>
#include <UI/UI.hpp>

namespace Pulsar {
namespace UI {

typedef Pages::GPVSLeaderboardUpdate::Player PlayerEntry;

typedef void (*FillDeltaFn)(CtrlRaceResult*, u32, u8);
typedef void (*ApplyExtraPageFn)(Page*);
typedef void (*MarkLicensesDirtyFn)(void*);

kmRuntimeUse(0x807f579c);
kmRuntimeUse(0x8064f65c);
kmRuntimeUse(0x80621410);
kmRuntimeUse(0x807f5a50);
static const FillDeltaFn sFillDelta = reinterpret_cast<FillDeltaFn>(kmRuntimeAddr(0x807f579c));
static const ApplyExtraPageFn sApplyExtraPage = reinterpret_cast<ApplyExtraPageFn>(kmRuntimeAddr(0x8064f65c));
static const MarkLicensesDirtyFn sMarkLicensesDirty = reinterpret_cast<MarkLicensesDirtyFn>(kmRuntimeAddr(0x80621410));

struct CtrlRaceResult_Inputs {
    u8 _00[0x178];
    float timer;  // 0x178
    bool playSfx;  // 0x17C
    bool direction;  // 0x17D
    u8 _17E[0x184 - 0x17E];
    float step;  // 0x184
    float current;  // 0x188
    u32 target;  // 0x18C
    u32 messageId;  // 0x190
};

static const u32 FLOAT_MODE_MAGIC = 0x1337CAFE;

void CtrlRaceResult_calcSelf_Hook(CtrlRaceResult* self) {
    CtrlRaceResult_Inputs* inputs = reinterpret_cast<CtrlRaceResult_Inputs*>(self);

    if (inputs->messageId == FLOAT_MODE_MAGIC) {
        if (inputs->timer > 0.0f) {
            inputs->current += inputs->step;
            inputs->timer -= 1.0f;

            float targetVal = *reinterpret_cast<float*>(&inputs->target);

            bool finished = false;
            if ((inputs->step > 0 && inputs->current >= targetVal) ||
                (inputs->step < 0 && inputs->current <= targetVal)) {
                inputs->current = targetVal;
                finished = true;
            }

            if (!finished && inputs->timer <= 0.0f) {
                inputs->current = targetVal;
                finished = true;
            }

            if (finished) {
                inputs->timer = 0.0f;
            }

            wchar_t buffer[64];
            float val = inputs->current;
            int iVal = (int)val;
            int dVal = (int)((val - (float)iVal) * 100.0f + 0.5f);
            if (dVal >= 100) {
                iVal++;
                dVal -= 100;
            }
            if (dVal < 0) dVal = -dVal;

            if (iVal == 0)
                swprintf(buffer, 64, L"%d", dVal);
            else
                swprintf(buffer, 64, L"%d%02d", iVal, dVal);

            Text::Info info;
            info.strings[0] = buffer;
            self->SetTextBoxMessage("total_score", UI::BMG_TEXT, &info);

            if (inputs->playSfx && !finished) {
                self->PlaySound(0xde, -1);
            }
        }
    } else {
        reinterpret_cast<void (*)(CtrlRaceResult*)>(kmRuntimeAddr(0x807f5a50))(self);
    }
}

kmWritePointer(0x808d3f04, CtrlRaceResult_calcSelf_Hook);

struct RatingDisplay {
    float total;
    float delta;
    bool isFloat;
};

static float NormalizeRatingDeltaF(float rawDelta) {
    return rawDelta;
}

static s32 NormalizeRatingDelta(s32 rawDelta) {
    const s32 HALF_RANGE = 0x8000;
    const s32 FULL_RANGE = 0x10000;
    if (rawDelta <= -HALF_RANGE) {
        rawDelta += FULL_RANGE;
    } else if (rawDelta >= HALF_RANGE) {
        rawDelta -= FULL_RANGE;
    }
    return rawDelta;
}

inline bool IsValidPlayerId(u8 playerId) {
    return playerId < 12;
}

bool IsBattleMode(const RacedataScenario& scenario) {
    int diff = static_cast<int>(scenario.settings.gamemode) - static_cast<int>(MODE_BATTLE);
    if (diff < 0 || diff >= 8) return false;
    return ((1u << diff) & 0xC1u) != 0;
}

u8 DetermineAnimationVariant(u32 rowIndex) {
    return rowIndex == 0 ? 1 : 0;
}

void* GetSaveGhostManagerPointer() {
    SectionMgr* mgr = SectionMgr::sInstance;
    if (mgr == 0) return 0;
    return *reinterpret_cast<void**>(reinterpret_cast<u32>(mgr) + 0x90);
}

void UpdateBattleScores(const RacedataScenario& scenario, Raceinfo* raceInfo) {
    if (raceInfo == 0) return;
    SectionMgr* mgr = SectionMgr::sInstance;
    if (mgr == 0 || mgr->sectionParams == 0) return;

    const Team winningTeam = mgr->sectionParams->lastBattleWinningTeam;
    const s16 bonus = (scenario.settings.battleType == BATTLE_BALLOON) ? 3 : 5;

    for (u8 i = 0; i < scenario.playerCount; ++i) {
        if (scenario.players[i].team != winningTeam) continue;
        RaceinfoPlayer* infoPlayer = raceInfo->players[i];
        if (infoPlayer == 0) continue;
        infoPlayer->battleScore = static_cast<s16>(infoPlayer->battleScore + bonus);
    }
}

RatingDisplay BuildRatingDisplay(u8 playerId, bool isBattle, const RacedataScenario& raceScenario, const RacedataScenario& menuScenario) {
    RatingDisplay display = {};
    const float MAX_RATING = 10000.0f;
    const float MIN_RATING = 1.0f;

    const RacedataPlayer& racePlayer = raceScenario.players[playerId];
    const RacedataPlayer& menuPlayer = menuScenario.players[playerId];

    if (racePlayer.playerType == PLAYER_REAL_LOCAL) {
        RKSYS::Mgr* rksys = RKSYS::Mgr::sInstance;
        if (rksys && rksys->curLicenseId >= 0) {
            float current = isBattle ? PointRating::GetUserBR(rksys->curLicenseId) : PointRating::GetUserVR(rksys->curLicenseId);
            float delta = PointRating::lastRaceDeltas[playerId];

            float oldRating = current - delta;

            float startRating = (float)racePlayer.rating.points;
            if (startRating >= MAX_RATING)
                oldRating = MAX_RATING;
            else if (startRating <= MIN_RATING)
                oldRating = MIN_RATING;

            if (current > MAX_RATING)
                current = MAX_RATING;
            else if (current < MIN_RATING)
                current = MIN_RATING;

            if (oldRating >= MAX_RATING && delta > 0.0f) {
                delta = 0.0f;
                current = MAX_RATING;
            } else if (oldRating <= MIN_RATING && delta < 0.0f) {
                delta = 0.0f;
                current = MIN_RATING;
            } else {
                delta = current - oldRating;
            }

            display.total = current;
            display.delta = delta;
            display.isFloat = true;
            return display;
        }
    } else if (racePlayer.playerType == PLAYER_REAL_ONLINE) {
        const Network::CustomRKNetController* controller = reinterpret_cast<const Network::CustomRKNetController*>(RKNet::Controller::sInstance);
        u8 aid = controller->aidsBelongingToPlayerIds[playerId];

        int playerIndexOnConsole = 0;
        for (int i = 0; i < playerId; ++i) {
            if (controller->aidsBelongingToPlayerIds[i] == aid) {
                playerIndexOnConsole++;
            }
        }

        if (playerIndexOnConsole < 2) {
            float oldRating = (float)racePlayer.rating.points;
            float decimal = (float)PointRating::remoteDecimalVR[aid][playerIndexOnConsole] / 100.0f;
            oldRating += decimal;

            float delta = PointRating::lastRaceDeltas[playerId];
            float current = oldRating + delta;

            if (current > MAX_RATING)
                current = MAX_RATING;
            else if (current < MIN_RATING)
                current = MIN_RATING;

            if (oldRating >= MAX_RATING && delta > 0.0f) {
                delta = 0.0f;
                current = MAX_RATING;
            } else if (oldRating <= MIN_RATING && delta < 0.0f) {
                delta = 0.0f;
                current = MIN_RATING;
            } else {
                delta = current - oldRating;
            }

            display.total = current;
            display.delta = delta;
            display.isFloat = true;
            return display;
        }
    }

    u32 current = racePlayer.rating.points;
    s32 delta = NormalizeRatingDelta(static_cast<s32>(menuPlayer.rating.points) - static_cast<s32>(racePlayer.rating.points));
    display.total = (float)current;
    display.delta = (float)delta;
    display.isFloat = false;
    return display;
}

u8 ResolvePlayerIdForRow(bool isBattle, const PlayerEntry* sortedEntries, u32 rowIndex, const Raceinfo* raceInfo) {
    if (isBattle) {
        return sortedEntries[rowIndex].playerId;
    }
    if (raceInfo != nullptr && raceInfo->playerIdInEachPosition != nullptr) {
        return raceInfo->playerIdInEachPosition[rowIndex];
    }
    return static_cast<u8>(rowIndex);
}

bool ShouldSkipScoreDisplay(bool isBattle, u8 playerId, const RacedataScenario& raceScenario, const RKNet::Controller* controller) {
    if (isBattle || controller == nullptr) return false;
    if (!IsValidPlayerId(playerId)) return false;
    if (raceScenario.players[playerId].playerType == PLAYER_REAL_LOCAL) return false;

    const u8 aidCurrent = controller->aidsBelongingToPlayerIds[playerId];
    const u8 aidPrevious = (playerId > 0 && IsValidPlayerId(static_cast<u8>(playerId - 1)))
                               ? controller->aidsBelongingToPlayerIds[playerId - 1]
                               : 0xFF;
    if (aidCurrent == 0xFF) return false;
    return aidCurrent == aidPrevious;
}

kmRuntimeUse(0x8085c16c);
kmRuntimeUse(0x8085cc84);
void WWLeaderboardFillRows(Pages::WWLeaderboardUpdate* page) {
    Raceinfo* raceInfo = Raceinfo::sInstance;
    Racedata* raceData = Racedata::sInstance;
    if (raceInfo == nullptr || raceData == nullptr) {
        return;
    }

    void (*prepareLicenses)() = reinterpret_cast<void (*)()>(kmRuntimeAddr(0x8085c16c));
    prepareLicenses();

    const RacedataScenario& raceScenario = raceData->racesScenario;
    const RacedataScenario& menuScenario = raceData->menusScenario;

    const bool isBattle = IsBattleMode(raceScenario);
    if (isBattle) {
        UpdateBattleScores(raceScenario, raceInfo);
    }

    raceData->menusScenario.UpdatePoints();

    u32 rowCount = static_cast<u32>(page->GetRowCount());
    const u32 playerCount = raceScenario.playerCount;
    if (rowCount > playerCount) {
        rowCount = playerCount;
    }

    PlayerEntry* sortedEntries = page->sortedArray;

    if (isBattle) {
        if (sortedEntries == nullptr) {
            return;
        }
        for (u32 i = 0; i < rowCount; ++i) {
            sortedEntries[i].playerId = static_cast<u8>(i);
            RaceinfoPlayer* infoPlayer = raceInfo->players[i];
            sortedEntries[i].totalScore = (infoPlayer != 0) ? static_cast<u32>(infoPlayer->battleScore) : 0;
            sortedEntries[i].lastRaceScore = 0;
        }
        typedef int (*Comparator)(const void*, const void*);
        static const Comparator sortPlayers = reinterpret_cast<Comparator>(kmRuntimeAddr(0x8085cc84));
        qsort(sortedEntries, rowCount, sizeof(PlayerEntry), sortPlayers);
    }

    const RKNet::Controller* controller = RKNet::Controller::sInstance;

    for (u32 row = 0; row < rowCount; ++row) {
        const u8 rank = static_cast<u8>(row + 1);
        const u8 playerId = ResolvePlayerIdForRow(isBattle, sortedEntries, row, raceInfo);
        if (!IsValidPlayerId(playerId)) continue;

        CtrlRaceResult* result = page->results != 0 ? page->results[row] : 0;
        if (result == 0) continue;

        result->Fill(rank, playerId);

        if (isBattle) {
            const PlayerEntry& topEntry = sortedEntries[0];
            const PlayerEntry& currentEntry = sortedEntries[row];
            if (topEntry.totalScore != 0 && currentEntry.totalScore == topEntry.totalScore) {
                result->SetTextBoxMessage("position", 0x541);
            } else {
                result->ResetTextBoxMessage("position");
            }
        }

        const bool skipScores = ShouldSkipScoreDisplay(isBattle, playerId, raceScenario, controller);
        if (!skipScores) {
            RatingDisplay display = BuildRatingDisplay(playerId, isBattle, raceScenario, menuScenario);
            const u32 messageId = isBattle ? 0x540 : 0x53f;

            if (display.isFloat) {
                float endVal = display.total;
                float startVal = endVal - display.delta;

                CtrlRaceResult_Inputs* inputs = reinterpret_cast<CtrlRaceResult_Inputs*>(result);
                inputs->current = startVal;
                inputs->target = *reinterpret_cast<u32*>(&endVal);
                inputs->step = display.delta / 60.0f;
                inputs->timer = 60.0f;
                inputs->messageId = FLOAT_MODE_MAGIC;
                inputs->playSfx = true;
                inputs->direction = (display.delta != 0.0f);

                wchar_t buffer[64];
                int tInt = (int)startVal;
                int tDec = (int)((startVal - (float)tInt) * 100.0f + 0.5f);
                if (tDec >= 100) {
                    tInt++;
                    tDec -= 100;
                }
                if (tDec < 0) tDec = -tDec;
                if (tInt == 0)
                    swprintf(buffer, 64, L"%d", tDec);
                else
                    swprintf(buffer, 64, L"%d%02d", tInt, tDec);

                Text::Info info;
                info.strings[0] = buffer;
                result->SetTextBoxMessage("total_score", UI::BMG_TEXT, &info);
                result->SetTextBoxMessage("total_point", messageId);

                int dInt = (int)display.delta;
                int dDec;
                if (display.delta >= 0) {
                    dDec = (int)((display.delta - (float)dInt) * 100.0f + 0.5f);
                    if (dDec >= 100) {
                        dInt++;
                        dDec -= 100;
                    }
                } else {
                    dDec = (int)((display.delta - (float)dInt) * 100.0f - 0.5f);
                    if (dDec <= -100) {
                        dInt--;
                        dDec += 100;
                    }
                }
                if (dDec < 0) dDec = -dDec;

                wchar_t deltaBuffer[64];
                if (display.delta >= 0) {
                    if (dInt == 0)
                        swprintf(deltaBuffer, 64, L"+%d", dDec);
                    else
                        swprintf(deltaBuffer, 64, L"+%d%02d", dInt, dDec);
                } else {
                    if (dInt == 0) {
                        swprintf(deltaBuffer, 64, L"-%d", dDec);
                    } else {
                        swprintf(deltaBuffer, 64, L"%d%02d", dInt, dDec);
                    }
                }

                Text::Info deltaInfo;
                deltaInfo.strings[0] = deltaBuffer;
                result->SetTextBoxMessage("get_point", UI::BMG_TEXT, &deltaInfo);
            } else {
                result->FillScore((u32)display.total, messageId);
                sFillDelta(result, static_cast<u32>(display.delta), DetermineAnimationVariant(row));
            }
        } else {
            result->SetTextBoxMessage("total_score", 0x25e7);
            result->ResetTextBoxMessage("total_point");
            result->ResetTextBoxMessage("get_point");
        }

        result->FillName(playerId);
    }

    SectionMgr* sectionMgr = SectionMgr::sInstance;
    if (sectionMgr != nullptr && sectionMgr->curSection != nullptr) {
        const SectionId sectionId = sectionMgr->curSection->sectionId;
        if ((sectionId >= 0x68 && sectionId <= 0x69) || (sectionId >= 0x6c && sectionId <= 0x6d)) {
            Page* extraPage = sectionMgr->curSection->pages[0x48];
            sApplyExtraPage(extraPage);
        } else {
            sApplyExtraPage(0);
        }
    }

    page->func_0x6c();

    void* saveGhostManager = GetSaveGhostManagerPointer();
    if (saveGhostManager != nullptr) {
        sMarkLicensesDirty(saveGhostManager);
    }
}
kmBranch(0x8085ce8c, WWLeaderboardFillRows);

}  // namespace UI
}  // namespace Pulsar