#include <MarioKartWii/System/Identifiers.hpp>
#include <hooks.hpp>
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

static void SetFFAmodeHelper(Racedata* racedata) {
    if (System::sInstance->IsContext(PULSAR_FFA)) {
        racedata->racesScenario.settings.modeFlags = 0;
    } else {
        racedata->racesScenario.settings.modeFlags = racedata->menusScenario.settings.modeFlags;
    }
}

static asmFunc SetFFAmode() {
    ASM(
        nofralloc;
        stwu r1, -0x20(r1);
        stw r0, 0x10(r1);
        mflr r0;
        stw r0, 0x24(r1);
        stw r3, 0x8(r1);
        stw r6, 0xC(r1);

        mr r3, r31;
        bl SetFFAmodeHelper;

        lwz r6, 0xC(r1);
        lwz r3, 0x8(r1);
        lwz r0, 0x24(r1);
        mtlr r0;
        lwz r0, 0x10(r1);
        addi r1, r1, 0x20;
        blr;);
}
kmCall(0x8053056c, SetFFAmode);

// Patches
kmRuntimeUse(0x808aa1ac);  // position.brctr [ZPL]
kmRuntimeUse(0x808a98dd);  // battle_total_point.brctr
kmRuntimeUse(0x80890209);  // minigame.kmg
kmRuntimeUse(0x808dc540);  // balloon.brres

static int GetSinglePlayerVSControls() {
    return System::sInstance->IsContext(PULSAR_FFA) ? 0x28de : 0x87e;
}

static int GetMultiplayerVSControls() {
    return System::sInstance->IsContext(PULSAR_FFA) ? 0x28de : 0x87c;
}

static int GetWifiBattleControls() {
    return System::sInstance->IsContext(PULSAR_FFA) ? 0x28de : 0x8ce;
}

static int GetWifiFriendControls() {
    return System::sInstance->IsContext(PULSAR_FFA) ? 0x28de : 0x87e;
}

kmBranch(0x80633a90, GetSinglePlayerVSControls);
kmBranch(0x80633a00, GetMultiplayerVSControls);
kmBranch(0x80633940, GetWifiBattleControls);
kmBranch(0x80633880, GetWifiBattleControls);
kmBranch(0x806336d0, GetWifiFriendControls);

static void SetFFABattleResourceNames(bool isFFA, bool isElimination) {
    volatile char* positionName = reinterpret_cast<volatile char*>(kmRuntimeAddr(0x808aa1ac));
    volatile char* battlePointName = reinterpret_cast<volatile char*>(kmRuntimeAddr(0x808a98dd));
    volatile char* minigameName = reinterpret_cast<volatile char*>(kmRuntimeAddr(0x80890209));
    volatile char* balloonName = reinterpret_cast<volatile char*>(kmRuntimeAddr(0x808dc540));

    positionName[0] = isFFA ? 'r' : 'p';
    positionName[1] = isFFA ? 'r' : 'o';
    battlePointName[0] = isFFA ? 'r' : 'b';
    battlePointName[1] = isFFA ? 'r' : 'a';
    minigameName[0] = isElimination ? 'E' : isFFA ? 'R' : 'm';
    balloonName[0] = isFFA ? 'f' : 'b';
}

void ApplyFFABattle() {
    bool isFFA = Pulsar::System::sInstance->IsContext(PULSAR_FFA);
    const bool isElim = Pulsar::System::sInstance->IsContext(PULSAR_ELIMINATION);
    SetFFABattleResourceNames(isFFA == BATTLE_FFA_ENABLED, isFFA == BATTLE_FFA_ENABLED && isElim);
    if (isFFA == BATTLE_FFA_ENABLED) {
        Racedata::sInstance->racesScenario.settings.modeFlags &= ~0x2;
    }
}
static SectionLoadHook ApplyFFABattleHook(ApplyFFABattle);

}  // namespace Battle
}  // namespace Pulsar
