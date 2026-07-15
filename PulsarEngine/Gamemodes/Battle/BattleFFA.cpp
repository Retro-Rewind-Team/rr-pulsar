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
#include <Patching/RuntimeChoice.hpp>

namespace Pulsar {
namespace Battle {

static u32 sIsFFAEnabled = 0;
static u32 sSinglePlayerBattleMessageId = 0x087e;
static u32 sMultiplayerBattleMessageId = 0x087c;
static u32 sOnlineBattleMessageId = 0x08ce;

static bool IsFFAEnabledNow() {
    return System::sInstance != nullptr && System::sInstance->IsContext(PULSAR_FFA);
}

static void UpdateFFAPatchState() {
    sIsFFAEnabled = IsFFAEnabledNow() ? 1 : 0;
    const u32 ffaMessageId = sIsFFAEnabled ? 0x28de : 0;
    sSinglePlayerBattleMessageId = sIsFFAEnabled ? ffaMessageId : 0x087e;
    sMultiplayerBattleMessageId = sIsFFAEnabled ? ffaMessageId : 0x087c;
    sOnlineBattleMessageId = sIsFFAEnabled ? ffaMessageId : 0x08ce;
}

RuntimeChoice_ConditionalStoreRegOrZero(SetFFAmodeDispatch, 0x8053056c, sIsFFAEnabled, r7, r31, 0xb90);
RuntimeChoice_ReturnValue(LoadSinglePlayerBattleMessageId, RuntimeChoice_LeafReturn, 0x80633a90, r3, sSinglePlayerBattleMessageId);
RuntimeChoice_ReturnValue(LoadMultiplayerBattleMessageId, RuntimeChoice_LeafReturn, 0x80633a00, r3, sMultiplayerBattleMessageId);
RuntimeChoice_ReturnValue(LoadOnlineBattleMessageId, RuntimeChoice_LeafReturn, 0x80633940, r3, sOnlineBattleMessageId);
RuntimeChoice_ReturnValue(LoadOnlineMultiplayerBattleMessageId, RuntimeChoice_LeafReturn, 0x80633880, r3, sOnlineBattleMessageId);
RuntimeChoice_ReturnValue(LoadFriendRoomBattleMessageId, RuntimeChoice_LeafReturn, 0x806336d0, r3, sSinglePlayerBattleMessageId);
RuntimeChoice_ConditionalLoadOrZero(LoadTeamFlagFromR6, 0x8052E9E0, sIsFFAEnabled, r3, r6, 0xb70);
RuntimeChoice_ConditionalLoadOrZero(LoadTeamFlagFromR4, 0x8052EA7C, sIsFFAEnabled, r4, r4, 0xb70);
RuntimeChoice_ConditionalLoadOrZero(LoadTeamFlagFromR3, 0x8052EB98, sIsFFAEnabled, r3, r3, 0xb70);

// Resource name patches.
kmRuntimeUse(0x808aa1ac);  // position.brctr [ZPL]
kmRuntimeUse(0x808a98dd);  // battle_total_point.brctr
kmRuntimeUse(0x80890209);  // minigame.kmg
kmRuntimeUse(0x808dc540);  // balloon.brres
void ApplyFFABattle() {
    UpdateFFAPatchState();
    kmRuntimeWrite16A(0x808aa1ac, 'po');
    kmRuntimeWrite16A(0x808a98dd, 'ba');
    kmRuntimeWrite8A(0x80890209, 'm');
    kmRuntimeWrite8A(0x808dc540, 'b');
    if (sIsFFAEnabled) {
        kmRuntimeWrite8A(0x80890209, 'R');
        kmRuntimeWrite16A(0x808aa1ac, 'rr');
        kmRuntimeWrite16A(0x808a98dd, 'rr');
        kmRuntimeWrite8A(0x808dc540, 'f');
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
