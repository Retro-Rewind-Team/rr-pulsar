#include <RetroRewind.hpp>
#include <Gamemodes/Battle/BattleElimination.hpp>
#include <core/System/SystemManager.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>

namespace Pulsar {
namespace Rating {

static float vrMultiplier = 1.0f;
static int vrSelected = 0;
static int vrCurrent = 0;

asmFunc storeVR() {
    ASM(
        nofralloc;
        mr r12, r3;
        mflr r11;
        stwu sp, -0x80(sp);
        stmw r3, 0x8(sp);
        lhz r0, 0x0004(r12);
        lis r12, vrCurrent @ha;
        stw r0, vrCurrent @l(r12);
        lis r12, vrSelected @ha;
        stw r4, vrSelected @l(r12);
        lmw r3, 0x8(sp);
        addi sp, sp, 0x80;
        mtlr r11;
        blr;)
}
kmCall(0x8052ec7c, storeVR);

asmFunc applyVRPlayerMultiplier() {
    ASM(
        nofralloc;
        sth r0, 0x0004(r12);
        addi r16, r16, 1;
        blr;)
}

static void multiplyVR() {
    const RKNet::Controller* controller = RKNet::Controller::sInstance;
    const RKNet::ControllerSub& sub = controller->subs[controller->currentSub];
    if (BattleElim::ShouldApplyBattleElimination()) {
        if (sub.playerCount <= 5) {
            vrMultiplier = 1.0f;
        } else {
            vrMultiplier = 1.0f + ((float)(sub.playerCount - 5) * 0.166f);
        }
    } else {
        vrMultiplier = 1.0f;
    }
    SystemManager* sm = SystemManager::sInstance;
    if (sm->isValidDate) {
        const unsigned month = static_cast<unsigned>(sm->month);
        const unsigned day = static_cast<unsigned>(sm->day);
        // Christmas Season/New Year Season, Halloween, Start of Summer, St. Patrick's Day, End of Summer, MKWii's Birthday
        if (month == 12 && day >= 23 && day <= 31 ||
            month == 1 && day >= 1 && day <= 3 ||
            month == 10 && day >= 25 && day <= 31 ||
            month == 6 && day >= 5 && day <= 8 ||
            month == 3 && day >= 13 && day <= 17 ||
            month == 4 && day >= 10 && day <= 14 ||
            month == 8 && day >= 23 && day <= 29) {
            vrMultiplier = 2.0f;
            if (BattleElim::ShouldApplyBattleElimination()) {
                if (sub.playerCount <= 5) {
                    vrMultiplier = 2.0f;
                } else {
                    vrMultiplier = 2.0f + ((float)(sub.playerCount - 5) * 0.166f);
                }
            }
        }
    }
    vrSelected = vrSelected * vrMultiplier;
    vrCurrent = vrCurrent + vrSelected;
    applyVRPlayerMultiplier();
}
kmCall(0x8052ec80, multiplyVR);

}  // namespace Rating
}  // namespace Pulsar