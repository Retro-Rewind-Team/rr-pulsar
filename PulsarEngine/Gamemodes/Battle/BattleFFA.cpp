#include <kamek.hpp>
#include <PulsarSystem.hpp>
#include <RetroRewind.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <UI/ExtendedTeamSelect/ExtendedTeamSelect.hpp>
#include <UI/ExtendedTeamSelect/ExtendedTeamManager.hpp>
#include <MarioKartWii/UI/Page/Leaderboard/TeamLeaderboard.hpp>
#include <MarioKartWii/UI/Page/Page.hpp>
#include <Network/Network.hpp>
#include <runtimeWrite.hpp>

namespace Pulsar {
namespace Battle {

static asmFunc SetFFAmode() {
    ASM(
        nofralloc;
        li r7, 0x03;
        lbl_loop : subi r10, r10, 0x01;
        subic.r7, r7, 0x01;
        bne lbl_loop;
        lbl_done : stw r7, 2960(r31);  // Original
        blr;);
}

static asmFunc FFAResults() {
    ASM(
        nofralloc;
        lis r10, 0x8000;
        cmpwi r4, 0x28;
        beq lbl_setFFAP1;
        cmpwi r4, 0x29;
        beq lbl_setFFAP2;
        cmpwi r4, 0x2a;
        beq lbl_setFFAP3;
        cmpwi r4, 0x2b;
        beq lbl_setFFAP4;
        cmpwi r4, 0x72;
        beq lbl_setFFA_Froom;
        cmpwi r4, 0x73;
        beq lbl_setFFA_Froom;
        cmpwi r4, 0x76;
        beq lbl_setFFA_Froom2;
        cmpwi r4, 0x77;
        beq lbl_setFFA_Froom2;
        bne lbl_orig;
        lbl_setFFAP1 : li r4, 0x20;
        b lbl_orig;
        lbl_setFFAP2 : li r4, 0x21;
        b lbl_orig;
        lbl_setFFAP3 : li r4, 0x22;
        b lbl_orig;
        lbl_setFFAP4 : li r4, 0x23;
        b lbl_orig;
        lbl_setFFA_Froom : li r4, 0x70;
        b lbl_orig;
        lbl_setFFA_Froom2 : li r4, 0x74;
        b lbl_orig;
        lbl_orig : mr r31, r4;  // Original
        blr;);
}

// Patches
kmRuntimeUse(0x8053056c);  // SetFFAmode [Nameless1]
kmRuntimeUse(0x80621e1c);  // FFAResults [Nameless1]
kmRuntimeUse(0x80633a90);  // Single Player VS [B_squo, Ro]
kmRuntimeUse(0x80633a00);  // Multiplayer VS
kmRuntimeUse(0x80633940);  // Wifi Battle
kmRuntimeUse(0x80633880);  // Wifi Multiplayer Battle
kmRuntimeUse(0x806336d0);  // Wifi Froom
kmRuntimeUse(0x808aa1ac);  // position.brctr [ZPL]
kmRuntimeUse(0x808a98dd);  // battle_total_point.brctr
kmRuntimeUse(0x80890209);  // minigame.kmg
kmRuntimeUse(0x808dc540);  // balloon.brres
kmRuntimeUse(0x8052E9E0);  // Team Flag [ZPL]
kmRuntimeUse(0x8052EA7C);
kmRuntimeUse(0x8052EB98);
void ApplyFFABattle() {
    kmRuntimeWrite32A(0x8053056c, 0x90FF0B90);
    kmRuntimeWrite32A(0x80621e1c, 0x7C9F2378);
    kmRuntimeWrite32A(0x80633a90, 0x3860087E);
    kmRuntimeWrite32A(0x80633a00, 0x3860087C);
    kmRuntimeWrite32A(0x80633940, 0x386008CE);
    kmRuntimeWrite32A(0x80633880, 0x386008CE);
    kmRuntimeWrite32A(0x806336d0, 0x3860087E);
    kmRuntimeWrite16A(0x808aa1ac, 'po');
    kmRuntimeWrite16A(0x808a98dd, 'ba');
    kmRuntimeWrite8A(0x80890209, 'm');
    kmRuntimeWrite8A(0x808dc540, 'b');
    kmRuntimeWrite32A(0x8052E9E0, 0x80660B70);
    kmRuntimeWrite32A(0x8052EA7C, 0x80840B70);
    kmRuntimeWrite32A(0x8052EB98, 0x80630B70);
    const RacedataScenario& scenario = Racedata::sInstance->menusScenario;
    const GameMode mode = scenario.settings.gamemode;
    bool isFFA = Pulsar::System::sInstance->IsContext(PULSAR_FFA);
    if (isFFA == BATTLE_FFA_ENABLED) {
        kmRuntimeWrite8A(0x80890209, 'R');
        kmRuntimeWrite16A(0x808aa1ac, 'rr');
        kmRuntimeWrite16A(0x808a98dd, 'rr');
        kmRuntimeWrite8A(0x808dc540, 'f');
        kmRuntimeWrite32A(0x80633a90, 0x386028de);
        kmRuntimeWrite32A(0x80633a00, 0x386028de);
        kmRuntimeWrite32A(0x80633940, 0x386028de);
        kmRuntimeWrite32A(0x80633880, 0x386028de);
        kmRuntimeWrite32A(0x806336d0, 0x386028de);
        kmRuntimeCallA(0x8053056c, SetFFAmode);
        kmRuntimeCallA(0x80621e1c, FFAResults);
        kmRuntimeWrite32A(0x8052E9E0, 0x38600000);
        kmRuntimeWrite32A(0x8052EA7C, 0x38800000);
        kmRuntimeWrite32A(0x8052EB98, 0x38600000);
        Racedata::sInstance->racesScenario.settings.modeFlags &= ~0x2;
        bool isElim = Pulsar::System::sInstance->IsContext(PULSAR_ELIMINATION);
        if (isElim) {
            kmRuntimeWrite8A(0x80890209, 'E');
        }
    }
}
static SectionLoadHook ApplyFFABattleHook(ApplyFFABattle);
    
}  // namespace Battle
}  // namespace Pulsar