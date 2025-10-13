#include <RetroRewind.hpp>
#include <MarioKartWii/Race/Racedata.hpp>
#include <MarioKartWii/Race/Raceinfo/Raceinfo.hpp>
#include <MarioKartWii/RKSYS/LicenseMgr.hpp>
#include <MarioKartWii/System/Identifiers.hpp>
#include <MarioKartWii/RKSYS/RKSYSMgr.hpp>
#include <MarioKartWii/GlobalFunctions.hpp>
#include <MarioKartWii/System/Rating.hpp>
#include <MarioKartWii/RKNet/USER.hpp>
#include <runtimeWrite.hpp>
#include <core/rvl/OS/OS.hpp>

namespace Pulsar {
namespace Ranking {

// Address found by B_squo, original idea by Zeraora, developed by ZPL
kmRuntimeUse(0x806436a0);
static void DisplayOnlineRanking() {
    // Default to rank 0
    kmRuntimeWrite32A(0x806436a0, 0x38600000); // li r3,0

    const RacedataSettings& racedataSettings = Racedata::sInstance->menusScenario.settings;
    const GameMode mode = racedataSettings.gamemode;
    if (mode != MODE_PUBLIC_VS) return;

    const RKSYS::Mgr* rksysMgr = RKSYS::Mgr::sInstance;
    if (!rksysMgr) return;
    s32 curLicense = rksysMgr->curLicenseId;
    const RKSYS::LicenseMgr& license = rksysMgr->licenses[curLicense];

    // Collect raw license statistics
    const Rating& vr = license.GetVR();
    u32 vsWins = license.GetWFCVSWins();
    u32 vsLosses = license.GetWFCVSLosses();
    u32 totalVs = vsWins + vsLosses;

    // Additional metrics
    u32 times1st = license.GetTimes1stPlaceAchieved();
    float distTravelled = license.GetDistanceTravelled();
    float distInFirst = license.GetDistancetravelledwhilein1stplace();

    // Compute win percentages (0-100). If insufficient data (<1 match), treat as 50% neutral baseline.
    float racingWinPct = (totalVs > 0) ? (100.0f * (float)vsWins / (float)totalVs) : 50.0f;

    // Normalize ratings (expected range 1-30000). Clamp to [0,30000] just in case.
    float vrClamped = (float)(vr.points > 30000 ? 30000 : vr.points);
    if (vrClamped < 0) vrClamped = 0; // safety
    float vrNorm = (vrClamped / 30000.0f) * 100.0f;
    
    // Normalize added metrics to 0-100
    float firstsNorm = (times1st >= 1500) ? 100.0f : (100.0f * (float)times1st / 1500.0f);
    float distNorm = (distTravelled >= 75000.0f) ? 100.0f : (100.0f * distTravelled / 75000.0f);
    float distFirstNorm = (distInFirst >= 20000.0f) ? 100.0f : (100.0f * distInFirst / 20000.0f);

    // Weighted composite emphasizing VR and VS performance; battle stats removed.
    // Base weights (sum = 1.0): VR 0.60, RWin 0.20, Firsts 0.10, Dist 0.05, DistInFirst 0.05
    // Then apply affine scaling so that:
    //  - High anchor: (VRn=100, RWin=65, Firsts=100, Dist=100, Dist1st=100) -> Score 100
    //  - Low  anchor: (VRn=16.67, RWin=50, Firsts=0,   Dist=0,   Dist1st=0)   -> Score 10
    const float W_VR = 0.60f;
    const float W_RWIN = 0.20f;
    const float W_FIRSTS = 0.10f;
    const float W_DIST = 0.05f;
    const float W_DIST1ST = 0.05f;

    float baseM = (W_VR * vrNorm) + (W_RWIN * racingWinPct) + (W_FIRSTS * firstsNorm) + (W_DIST * distNorm) + (W_DIST1ST * distFirstNorm);

    // Anchors
    const float AH_VR = 100.0f, AH_RWIN = 65.0f, AH_FIRSTS = 100.0f, AH_DIST = 100.0f, AH_DIST1ST = 100.0f; // -> 100
    const float AL_VR = 16.6667f, AL_RWIN = 50.0f, AL_FIRSTS = 0.0f,  AL_DIST = 0.0f,  AL_DIST1ST = 0.0f;   // -> 10
    float M1 = W_VR * AH_VR + W_RWIN * AH_RWIN + W_FIRSTS * AH_FIRSTS + W_DIST * AH_DIST + W_DIST1ST * AH_DIST1ST; // ~93.0
    float M2 = W_VR * AL_VR + W_RWIN * AL_RWIN + W_FIRSTS * AL_FIRSTS + W_DIST * AL_DIST + W_DIST1ST * AL_DIST1ST;  // ~20.0
    float alpha = 90.0f / (M1 - M2);  // 100-10 over delta
    float beta = 100.0f - alpha * M1;

    float finalScore = alpha * baseM + beta;
    if (finalScore < 0.0f) finalScore = 0.0f;
    else if (finalScore > 100.0f) finalScore = 100.0f;

    // Map score (0-100) to ranks:
    // 0: 0–11
    // 1: 12–23
    // 2: 24–35
    // 3: 36–47
    // 4: 48–59
    // 5: 60–71
    // 6: 72–83
    // 7: 84–93
    // 8: 94–99
    // 9: 100
    int rank = 0;
    if (finalScore >= 100.0f) rank = 9;
    else if (finalScore >= 94.0f) rank = 8;
    else if (finalScore >= 84.0f) rank = 7;
    else if (finalScore >= 72.0f) rank = 6;
    else if (finalScore >= 60.0f) rank = 5;
    else if (finalScore >= 48.0f) rank = 4;
    else if (finalScore >= 36.0f) rank = 3;
    else if (finalScore >= 24.0f) rank = 2;
    else if (finalScore >= 12.0f) rank = 1;
    else rank = 0;

    // Write rank back by patching target instruction (li r3, rank)
    // Base opcode for li r3,imm is 0x3860000X where immediate fits 16 bits (rank 0-8 fits easily)
    u32 opcode = 0x38600000 | (rank & 0xFFFF);
    kmRuntimeWrite32A(0x806436a0, opcode);
}
static SectionLoadHook HookRankIcon(DisplayOnlineRanking);

} // namespace Ranking
} // namespace Pulsar