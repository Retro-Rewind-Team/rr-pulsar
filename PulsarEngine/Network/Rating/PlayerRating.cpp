#include <MarioKartWii/Race/RaceData.hpp>
#include <MarioKartWii/Race/RaceInfo/RaceInfo.hpp>
#include <Network/Rating/PlayerRating.hpp>
#include <Network/Rating/RatingMulti.hpp>
#include <IO/RRRating.hpp>
#include <MarioKartWii/RKSYS/RKSYSMgr.hpp>
#include <MarioKartWii/UI/Section/SectionParams.hpp>
#include <Network/PacketExpansion.hpp>
#include <PulsarSystem.hpp>

namespace Pulsar {
namespace Network {

static void ExpandRating(Rating &rating, s32 change) {
    const s64 total = static_cast<s64>(rating.points) + change;
    if (total < 1)
        rating.points = 1;
    else if (total > RRRating::maxRating)
        rating.points = RRRating::maxRating;
    else
        rating.points = static_cast<u32>(total);
}
kmBranch(0x8052d270, ExpandRating);

// The word at +4 includes the original halfword and its padding.
kmWrite32(0x805444e0, 0x90030004);
kmWrite32(0x805441e8, 0x90039018);
kmWrite32(0x805441f4, 0x90039020);
kmWrite32(0x80545c08, 0x80040004);
kmWrite32(0x80545c0c, 0x90039018);
kmWrite32(0x80545c34, 0x80040004);
kmWrite32(0x80545c38, 0x90039020);
kmWrite32(0x8052d9f0, 0x909e00e8);
kmWrite32(0x8052e344, 0x80c400e8);
kmWrite32(0x8052e348, 0x90c300e8);
kmWrite32(0x805304c4, 0x80c300e8);
kmWrite32(0x805304c8, 0x90c400e8);
kmWrite32(0x8053078c, 0x810600e8);
kmWrite32(0x80530790, 0x910500e8);
kmWrite32(0x80531264, 0x80e400e8);
kmWrite32(0x80531268, 0x90e500e8);
kmWrite32(0x80531524, 0x80c400e8);
kmWrite32(0x80531528, 0x90c300e8);
kmWrite32(0x80531bfc, 0x80c400e8);
kmWrite32(0x80531c00, 0x90c300e8);

// Race setup and live views receive vanilla halfwords before SELECT supplies full ratings.
kmWrite32(0x806514e8, 0x90030d00);
kmWrite32(0x80651504, 0x90030d00);
kmWrite32(0x80651514, 0x91c30d00);
kmWrite32(0x8060a1f8, 0x90030d00);
kmWrite32(0x8060a218, 0x90030d00);
kmWrite32(0x8060a228, 0x93430d00);
kmWrite32(0x8060a318, 0x93430d00);

// Keep the already-clamped race result intact when copying it to the license.
kmWrite32(0x8064f6d0, 0x80c50d00);
kmWrite32(0x8064f6d4, 0x3c80809c);
kmWrite32(0x8064f6d8, 0x48000030);
kmWrite32(0x8064f73c, 0x90c49018);
kmWrite32(0x8064f760, 0x80c50d00);
kmWrite32(0x8064f764, 0x3c80809c);
kmWrite32(0x8064f768, 0x48000030);
kmWrite32(0x8064f7cc, 0x90c49020);
kmWrite32(0x8064f85c, 0x80840d00);
kmWrite32(0x8064f894, 0x80840d00);

// WFC menu, license records and post-race results.
kmWrite32(0x8064c35c, 0x80039018);
kmWrite32(0x8064c418, 0x80039020);
kmWrite32(0x8085fc90, 0x80039018);
kmWrite32(0x8085fcb8, 0x80039020);
kmWrite32(0x8085d3d4, 0x82460110);
kmWrite32(0x8085d41c, 0x80040d00);

// Friend records and legacy matchmaking fields still use their wire layouts.
kmWrite32(0x806519e4, 0x80039018);
kmWrite32(0x80651a1c, 0x80039020);
kmWrite32(0x80659494, 0x80049018);
kmWrite32(0x806597c8, 0x80049018);
kmWrite32(0x806594d4, 0x80039020);
kmWrite32(0x80659808, 0x80039020);
kmWrite32(0x8065fc7c, 0x80a39018);
kmWrite32(0x8065fcb4, 0x80a39020);
kmWrite32(0x80661b1c, 0x80039018);
kmWrite32(0x80661b8c, 0x80039018);
kmWrite32(0x80673544, 0x80049018);
kmWrite32(0x8067354c, 0x80049020);

static u32 GetLegacyRating(u32 points) {
    const u32 maxLegacyRating = 30000;  // WFC USER packet validation limit.
    if (points < 1)
        return 1;
    return points > maxLegacyRating ? maxLegacyRating : points;
}

static void SetLegacyUserRatings(RKNet::USERHandler &handler) {
    const RKSYS::Mgr &save = *RKSYS::Mgr::sInstance;
    const RKSYS::LicenseMgr &license = save.licenses[save.curLicenseId];
    handler.toSendPacket.vr = GetLegacyRating(license.vr.points);
    handler.toSendPacket.br = GetLegacyRating(license.br.points);
}
kmBranch(0x80662d80, SetLegacyUserRatings);
kmBranch(0x80662dc4, SetLegacyUserRatings);
kmWrite32(0x80663154, 0x7fe3fb78);  // mr r3, r31
kmCall(0x80663158, SetLegacyUserRatings);

u32 GetPlayerRating(u8 aid, bool isBattle, u32 fallback) {
    if (aid >= 12)
        return fallback;
    const RKNet::Controller &controller = *RKNet::Controller::sInstance;
    if (aid == controller.subs[controller.currentSub].localAid) {
        const RKSYS::Mgr &save = *RKSYS::Mgr::sInstance;
        const RKSYS::LicenseMgr &license = save.licenses[save.curLicenseId];
        return isBattle ? license.br.points : license.vr.points;
    }

    if (RKNet::SELECTHandler::sInstance == nullptr)
        return fallback;
    const PulSELECT &packet = ExpSELECTHandler::Get().receivedPackets[aid];
    const u32 points = isBattle ? packet.br : packet.vr;
    if (points > RRRating::maxRating)
        return RRRating::maxRating;
    return points != 0 ? points : fallback;
}

static void ImportRaceRatings(SectionParams &params) {
    params.CopyMiisToRacedata();
    const ExpSELECTHandler &select = ExpSELECTHandler::Get();
    if (select.mode != RKNet::ONLINEMODE_PUBLIC_VS && select.mode != RKNet::ONLINEMODE_PUBLIC_BATTLE)
        return;

    const bool isBattle = select.mode == RKNet::ONLINEMODE_PUBLIC_BATTLE;
    const RKNet::Controller &controller = *RKNet::Controller::sInstance;
    RacedataPlayer *players = Racedata::sInstance->menusScenario.players;
    u8 lastAid = 0xff;
    for (u8 playerId = 0; playerId < 12; ++playerId) {
        const u8 aid = controller.aidsBelongingToPlayerIds[playerId];
        if (aid >= 12 || aid == lastAid)
            continue;
        lastAid = aid;
        Rating &rating = players[playerId].rating;
        rating.points = GetPlayerRating(aid, isBattle, rating.points);
    }
}
kmCall(0x806515a8, ImportRaceRatings);

// Native race totals are signed halfwords. Eleven wins at 24 points still fit in hundredths.
static const int vrPrecision = 100;

static float EvaluateVRChange(float difference) {
    float sample = 749900.0f + difference;
    if (sample < 0.0f)
        sample = 0.0f;
    else if (sample > 1499800.0f)
        sample = 1499800.0f;
    return Rating::EvaluateCurve(sample * 0.0000020004f) * (10.0f / 3.0f);
}

static s16 ClampVRGains(const Rating &player, const Rating &opponent) {
    const Racedata &racedata = *Racedata::sInstance;
    const s32 difference = static_cast<s32>(opponent.points) - static_cast<s32>(player.points);
    if (racedata.racesScenario.settings.gamemode != MODE_PUBLIC_VS)
        return static_cast<s16>(Rating::CalcPoints(difference, true));

    float gain = EvaluateVRChange(difference);
    if (gain < 1.0f)
        gain = 1.0f;
    else if (gain > 24.0f)
        gain = 24.0f;

    if (player.points >= 850000)
        gain *= 0.1f;
    else if (player.points > 150000)
        gain *= 1.0f - 0.9f * ((player.points - 150000) / 700000.0f);

    // Only regular retro worldwides use the low-rated room penalty.
    if (System::sInstance->netMgr.region == 0x0A && player.points > 75000) {
        const u32 count = racedata.racesScenario.playerCount;
        u32 opponentTotal = 0;
        for (u32 i = 0; i < count; ++i) {
            const Rating &rating = racedata.menusScenario.players[i].rating;
            if (&rating != &player)
                opponentTotal += rating.points;
        }
        const u32 opponentCount = count - 1;
        if (opponentTotal <= (player.points - 75000) * opponentCount && opponentTotal <= 30000 * opponentCount)
            gain *= 0.15f;
    }

    return static_cast<s16>(gain * vrPrecision + 0.5f);
}
kmCall(0x8052eb1c, ClampVRGains);

static s16 ClampVRLosses(const Rating &player, const Rating &opponent) {
    const s32 difference = static_cast<s32>(opponent.points) - static_cast<s32>(player.points);
    if (Racedata::sInstance->racesScenario.settings.gamemode != MODE_PUBLIC_VS)
        return static_cast<s16>(Rating::CalcPoints(difference, false));

    const float loss = EvaluateVRChange(-16.0f * difference);
    return -static_cast<s16>((loss > 19.0f ? 19.0f : loss) * vrPrecision + 0.5f);
}
kmCall(0x8052eb5c, ClampVRLosses);

static void ApplyRaceRating(Rating &rating, s32 change) {
    const RacedataScenario &scenario = Racedata::sInstance->racesScenario;
    if (scenario.settings.gamemode != MODE_PUBLIC_VS) {
        rating.AddPoints(static_cast<s32>(ApplyRatingMultiplier(change)));
        return;
    }

    const RKNet::Controller &controller = *RKNet::Controller::sInstance;
    const Raceinfo &raceInfo = *Raceinfo::sInstance;
    bool winnerFinished = false;
    bool anotherFinished = false;
    for (u8 i = 0; i < scenario.playerCount; ++i) {
        const RaceinfoPlayer &player = *raceInfo.players[i];
        const Timer &time = *player.raceFinishTime;
        if (time.minutes || time.seconds || time.milliseconds) {
            if (player.position == 1)
                winnerFinished = true;
            else {
                anotherFinished = true;
                break;
            }
        }
    }
    if (controller.subs[controller.currentSub].connectionCount <= 1 || (winnerFinished && !anotherFinished)) {
        rating.AddPoints(scenario.playerCount >= 4 ? -1 : 0);
        return;
    }

    float delta = static_cast<float>(change) / vrPrecision;
    if (delta < 0.0f && rating.points < 15000)
        delta /= 7.5f - 6.5f * (rating.points / 15000.0f);
    delta = ApplyRatingMultiplier(delta);
    const s32 next = static_cast<s32>(rating.points + delta);
    rating.AddPoints(next - static_cast<s32>(rating.points));
}
kmCall(0x8052ec7c, ApplyRaceRating);

// There is no race-start debit to refund when the results page opens.
kmWrite32(0x8085c250, 0x60000000);
kmWrite32(0x8085c334, 0x60000000);

}  // namespace Network
}  // namespace Pulsar
