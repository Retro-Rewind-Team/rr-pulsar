#include <RetroRewind.hpp>
#include <Gamemodes/Battle/BattleElimination.hpp>
#include <core/System/SystemManager.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <Network/Rating/PlayerRating.hpp>
#include <Network/ServerDateTime.hpp>
#include <PulsarSystem.hpp>

namespace Pulsar {
namespace PointRating {

static bool IsEventDay(unsigned m, unsigned d) {
    return (m == 12 && d >= 23) ||  // Christmas
           (m == 1 && d <= 3) ||  // New Year
           (m == 10 && d >= 25) ||  // Halloween
           (m == 6 && d >= 5 && d <= 8) ||  // Start of Summer
           (m == 3 && d >= 13 && d <= 17) ||  // St. Patrick's Day
           (m == 4 && d >= 10 && d <= 14) ||  // MKWii Birthday
           (m == 8 && d >= 23 && d <= 29);  // End of Summer
}

static float GetBattleBonus() {
    if (!BattleElim::ShouldApplyBattleElimination()) return 0.0f;
    const RKNet::Controller* ctrl = RKNet::Controller::sInstance;
    int count = ctrl->subs[ctrl->currentSub].playerCount;
    return (count > 5) ? (float)(count - 5) * 0.166f : 0.0f;
}

bool IsWeekendMultiplierActive() {
    ServerDateTime* sdt = ServerDateTime::sInstance;
    if (sdt == nullptr || !sdt->isValid) return false;
    
    // Use SERVER time for the actual check (this is the authoritative time from login)
    u8 dow = ServerDateTime::GetDayOfWeek(sdt->year, sdt->month, sdt->day);
    bool isWeekend = (dow == 0 || dow == 6);  // Sunday or Saturday
    if (!isWeekend) return false;
    
    u32 weekNum = sdt->GetWeekNumber();
    return (weekNum % 2) == 1;  // Even weeks get the multiplier
}

bool IsWeekendMultiplierActiveForRegion(u8 region) {
    if (!IsWeekendMultiplierActive()) return false;
    ServerDateTime* sdt = ServerDateTime::sInstance;
    return sdt->GetCurrentVRMultiplierRegion() == region;
}

float GetMultiplier() {
#ifdef BETA
    return 1.15f;
#endif

    unsigned month = 0, day = 0;
    bool valid = false;

    ServerDateTime* sdt = ServerDateTime::sInstance;
    if (sdt && sdt->isValid) {
        month = sdt->month;
        day = sdt->day;
        valid = true;
    } else {
        SystemManager* sm = SystemManager::sInstance;
        if (sm && sm->isValidDate) {
            month = sm->month;
            day = sm->day;
            valid = true;
        }
    }

    float base = (valid && IsEventDay(month, day)) ? 2.0f : 1.0f;

    // Weekend VR multiplier (1.5x) for the active region
    u8 currentRegion = System::sInstance->netMgr.region;
    if (IsWeekendMultiplierActiveForRegion(currentRegion)) {
        base *= 1.25f;
    }

    return base + GetBattleBonus();
}

}  // namespace PointRating
}  // namespace Pulsar