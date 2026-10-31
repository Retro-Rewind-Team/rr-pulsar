#include <kamek.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <MarioKartWii/RKSYS/RKSYSMgr.hpp>
#include <MarioKartWii/Race/RaceData.hpp>
#include <MarioKartWii/Race/RaceInfo/RaceInfo.hpp>
#include <MarioKartWii/UI/Ctrl/CtrlRace/CtrlRaceResult.hpp>
#include <MarioKartWii/UI/Page/Leaderboard/WWLeaderboardUpdate.hpp>
#include <MarioKartWii/UI/Page/Leaderboard/GPVSLeaderboardTotal.hpp>
#include <MarioKartWii/UI/Page/Page.hpp>
#include <MarioKartWii/UI/Section/Section.hpp>
#include <MarioKartWii/UI/Section/SectionMgr.hpp>
#include <Network/Rating/PlayerRating.hpp>
#include <Network/PacketExpansion.hpp>
#include <UI/UI.hpp>

namespace Pulsar {
namespace UI {

typedef Pages::GPVSLeaderboardUpdate::Player PlayerEntry;

// Overlay only the animation fields reused by expanded VR; offsets match the PAL binary.
struct RatingAnimation {
    u8 _00[0x178];
    float timer;  // 0x178
    bool playSfx;  // 0x17C
    bool direction;  // 0x17D
    u8 _17E[0x184 - 0x17E];
    float step;  // 0x184
    float current;  // 0x188
    float target;  // 0x18C
    u32 messageId;  // 0x190
};

static const u32 RATING_ANIMATION_MESSAGE = 0x1337CAFE;

void UpdateRatingAnimation(CtrlRaceResult *self) {
    RatingAnimation *inputs = reinterpret_cast<RatingAnimation *>(self);

    if (inputs->messageId == RATING_ANIMATION_MESSAGE) {
        if (inputs->timer > 0.0f) {
            inputs->current += inputs->step;
            inputs->timer -= 1.0f;

            const float targetVal = inputs->target;

            bool finished = false;
            if ((inputs->step > 0 && inputs->current >= targetVal) || (inputs->step < 0 && inputs->current <= targetVal)) {
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
            swprintf(buffer, sizeof(buffer) / sizeof(buffer[0]), L"%d", (int)(inputs->current + 0.5f));
            Text::Info info;
            info.strings[0] = buffer;
            self->SetTextBoxMessage("total_score", UI::BMG_TEXT, &info);

            if (inputs->playSfx && !finished) {
                self->PlaySound(0xde, -1);
            }
        }
    } else {
        self->CtrlRaceResult::OnUpdate();
    }
}

kmWritePointer(0x808d3f04, UpdateRatingAnimation);

void WWLeaderboardFillRows(Pages::WWLeaderboardUpdate *page) {
    Raceinfo *raceInfo = Raceinfo::sInstance;
    Racedata *raceData = Racedata::sInstance;
    if (raceInfo == nullptr || raceData == nullptr) {
        return;
    }

    Pages::Leaderboard::UpdateLicenseRating();

    const RacedataScenario &raceScenario = raceData->racesScenario;
    const RacedataScenario &menuScenario = raceData->menusScenario;

    const GameMode mode = raceScenario.settings.gamemode;
    const bool isBattle = mode == MODE_BATTLE || mode == MODE_PUBLIC_BATTLE || mode == MODE_PRIVATE_BATTLE;
    if (isBattle) {
        const SectionMgr *sectionMgr = SectionMgr::sInstance;
        if (sectionMgr != nullptr && sectionMgr->sectionParams != nullptr) {
            const Team winningTeam = sectionMgr->sectionParams->lastBattleWinningTeam;
            const s16 bonus = raceScenario.settings.battleType == BATTLE_BALLOON ? 3 : 5;
            for (u8 i = 0; i < raceScenario.playerCount; ++i) {
                if (raceScenario.players[i].team == winningTeam && raceInfo->players[i] != nullptr)
                    raceInfo->players[i]->battleScore = static_cast<s16>(raceInfo->players[i]->battleScore + bonus);
            }
        }
    }

    PointRating::UpdatePoints(&raceData->menusScenario);

    u32 rowCount = static_cast<u32>(page->GetRowCount());
    const u32 playerCount = raceScenario.playerCount;
    if (rowCount > playerCount) {
        rowCount = playerCount;
    }

    PlayerEntry *sortedEntries = page->sortedArray;

    if (isBattle) {
        if (sortedEntries == nullptr) {
            return;
        }
        for (u32 i = 0; i < rowCount; ++i) {
            sortedEntries[i].playerId = static_cast<u8>(i);
            RaceinfoPlayer *infoPlayer = raceInfo->players[i];
            sortedEntries[i].totalScore = (infoPlayer != 0) ? static_cast<u32>(infoPlayer->battleScore) : 0;
            sortedEntries[i].lastRaceScore = 0;
        }
        typedef int (*Comparator)(const void *, const void *);
        qsort(sortedEntries, rowCount, sizeof(PlayerEntry), reinterpret_cast<Comparator>(Pages::GPVSLeaderboardTotal::ComparePlayers));
    }

    const RKNet::Controller *controller = RKNet::Controller::sInstance;

    for (u32 row = 0; row < rowCount; ++row) {
        const u8 rank = static_cast<u8>(row + 1);
        const u8 playerId = isBattle ? sortedEntries[row].playerId : raceInfo->playerIdInEachPosition != nullptr ? raceInfo->playerIdInEachPosition[row] : static_cast<u8>(row);
        if (playerId >= 12)
            continue;

        CtrlRaceResult *result = page->results != 0 ? page->results[row] : 0;
        if (result == 0)
            continue;

        result->Fill(rank, playerId);

        if (isBattle) {
            const PlayerEntry &topEntry = sortedEntries[0];
            const PlayerEntry &currentEntry = sortedEntries[row];
            if (topEntry.totalScore != 0 && currentEntry.totalScore == topEntry.totalScore) {
                result->SetTextBoxMessage("position", 0x541);
            } else {
                result->ResetTextBoxMessage("position");
            }
        }

        bool skipScores = false;
        if (!isBattle && controller != nullptr && raceScenario.players[playerId].playerType != PLAYER_REAL_LOCAL) {
            const u8 aid = controller->aidsBelongingToPlayerIds[playerId];
            const u8 previousAid = playerId > 0 ? controller->aidsBelongingToPlayerIds[playerId - 1] : 0xff;
            skipScores = aid != 0xff && aid == previousAid;
        }
        if (!skipScores) {
            float totalRating = 0.0f;
            float ratingDelta = 0.0f;
            bool isExpanded = false;

            const RacedataPlayer &racePlayer = raceScenario.players[playerId];
            const RacedataPlayer &menuPlayer = menuScenario.players[playerId];

            float oldRating = 0.0f;
            if (racePlayer.playerType == PLAYER_REAL_LOCAL) {
                RKSYS::Mgr *rksys = RKSYS::Mgr::sInstance;
                if (rksys && rksys->curLicenseId >= 0) {
                    totalRating = isBattle ? PointRating::GetUserBR(rksys->curLicenseId) : PointRating::GetUserVR(rksys->curLicenseId);
                    ratingDelta = PointRating::lastRaceDeltas[playerId];
                    oldRating = totalRating - ratingDelta;

                    const float startRating = racePlayer.rating.points * 100.0f;
                    if (startRating >= PointRating::MAX_RATING)
                        oldRating = PointRating::MAX_RATING;
                    else if (startRating <= PointRating::MIN_RATING)
                        oldRating = PointRating::MIN_RATING;
                    isExpanded = true;
                }
            } else if (racePlayer.playerType == PLAYER_REAL_ONLINE) {
                const RKNet::Controller *controller = RKNet::Controller::sInstance;
                const u8 aid = controller->aidsBelongingToPlayerIds[playerId];
                int slot = 0;
                for (int i = 0; i < playerId; ++i) {
                    if (controller->aidsBelongingToPlayerIds[i] == aid)
                        ++slot;
                }
                if (slot < 2) {
                    oldRating = racePlayer.rating.points * 100.0f + PointRating::remoteDecimalVR[aid][slot];
                    ratingDelta = PointRating::lastRaceDeltas[playerId];
                    totalRating = oldRating + ratingDelta;
                    isExpanded = true;
                }
            }

            if (isExpanded) {
                if (totalRating > PointRating::MAX_RATING)
                    totalRating = PointRating::MAX_RATING;
                else if (totalRating < PointRating::MIN_RATING)
                    totalRating = PointRating::MIN_RATING;

                if (oldRating >= PointRating::MAX_RATING && ratingDelta > 0.0f) {
                    ratingDelta = 0.0f;
                    totalRating = PointRating::MAX_RATING;
                } else if (oldRating <= PointRating::MIN_RATING && ratingDelta < 0.0f) {
                    ratingDelta = 0.0f;
                    totalRating = PointRating::MIN_RATING;
                } else {
                    ratingDelta = totalRating - oldRating;
                }
            } else {
                totalRating = (float)racePlayer.rating.points;
                s32 delta = static_cast<s32>(menuPlayer.rating.points) - static_cast<s32>(racePlayer.rating.points);
                if (delta <= -0x8000)
                    delta += 0x10000;
                else if (delta >= 0x8000)
                    delta -= 0x10000;
                ratingDelta = (float)delta;
            }
            const u32 messageId = isBattle ? 0x540 : 0x53f;

            if (isExpanded) {
                const float startVal = totalRating - ratingDelta;

                RatingAnimation *inputs = reinterpret_cast<RatingAnimation *>(result);
                inputs->current = startVal;
                inputs->target = totalRating;
                inputs->step = ratingDelta / 60.0f;
                inputs->timer = 60.0f;
                inputs->messageId = RATING_ANIMATION_MESSAGE;
                inputs->playSfx = true;
                inputs->direction = (ratingDelta != 0.0f);

                wchar_t buffer[64];
                swprintf(buffer, sizeof(buffer) / sizeof(buffer[0]), L"%d", (int)(startVal + 0.5f));
                Text::Info info;
                info.strings[0] = buffer;
                result->SetTextBoxMessage("total_score", UI::BMG_TEXT, &info);
                result->SetTextBoxMessage("total_point", messageId);

                wchar_t deltaBuffer[64];
                const int delta = (int)(ratingDelta < 0.0f ? -ratingDelta + 0.5f : ratingDelta + 0.5f);
                swprintf(deltaBuffer, sizeof(deltaBuffer) / sizeof(deltaBuffer[0]), ratingDelta < 0.0f ? L"-%d" : L"+%d", delta);
                Text::Info deltaInfo;
                deltaInfo.strings[0] = deltaBuffer;
                result->SetTextBoxMessage("get_point", UI::BMG_TEXT, &deltaInfo);
            } else {
                result->FillScore((u32)totalRating, messageId);
                result->FillDelta(static_cast<u32>(ratingDelta), row == 0 ? 1 : 0);
            }
        } else {
            result->SetTextBoxMessage("total_score", 0x25e7);
            result->ResetTextBoxMessage("total_point");
            result->ResetTextBoxMessage("get_point");
        }

        result->FillName(playerId);
    }

    Pages::Leaderboard::SaveRatingsAndStats();

    page->func_0x6c();

    SectionMgr *sectionMgr = SectionMgr::sInstance;
    if (sectionMgr != nullptr && sectionMgr->rksysRequester != nullptr) {
        sectionMgr->rksysRequester->NotifyNewLicenseContent();
    }
}
kmBranch(0x8085ce8c, WWLeaderboardFillRows);

}  // namespace UI
}  // namespace Pulsar
