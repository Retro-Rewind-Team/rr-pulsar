#include <kamek.hpp>
#include <runtimeWrite.hpp>
#include <core/rvl/OS/OS.hpp>
#include <Network/PCP/PCP.hpp>
#include <Network/PCP/PCPHookGuard.hpp>
#include <Settings/Settings.hpp>

#ifndef PCP_TICK_VIA_FRAMEHOOK
#define PCP_TICK_VIA_FRAMEHOOK 0
#endif

#ifndef PCP_HOOK_SHUTDOWN
#define PCP_HOOK_SHUTDOWN 1
#endif

#if PCP_TICK_VIA_FRAMEHOOK
#include <MarioKartWii/RKNet/RKNetController.hpp>
#endif

namespace Pulsar {
namespace Network {
namespace PCP {

static void ApplySetting() {
    if (Settings::Mgr::IsCreated())
        SetEnabled(Settings::Mgr::GetConst().GetSettingValue(Settings::SETTING_PCP) == PCP_ENABLED);
}

#if !PCP_TICK_VIA_FRAMEHOOK

kmRuntimeUse(0x800d115c);  // DWC_ProcessFriendsMatch
kmRuntimeUse(0x800d0fec);  // DWC_ShutdownFriendsMatch

static u32 sTickThunk[2];
static u32 sShutdownThunk[2];

extern "C" {
// Return the address the stub continues at.
u32 *PCP_OnProcessFriendsMatch() {
    ApplySetting();
    Tick();
    return sTickThunk;
}
u32 *PCP_OnShutdownFriendsMatch() {
    OnSessionEnd();
    return sShutdownThunk;
}
}

// Both stubs keep r3-r10 and lr, so the hooked function sees exactly the state it was called with.
// Checked in main.dol: neither function reads r0, r11, r12, ctr, cr or f1-f8 before writing it.
// clang-format off
static asm void TickStub() {
    nofralloc
    stwu r1, -0x30(r1)
    mflr r0
    stw r0, 0x34(r1)
    stw r3, 0x08(r1)
    stw r4, 0x0C(r1)
    stw r5, 0x10(r1)
    stw r6, 0x14(r1)
    stw r7, 0x18(r1)
    stw r8, 0x1C(r1)
    stw r9, 0x20(r1)
    stw r10, 0x24(r1)
    bl PCP_OnProcessFriendsMatch
    mtctr r3
    lwz r3, 0x08(r1)
    lwz r4, 0x0C(r1)
    lwz r5, 0x10(r1)
    lwz r6, 0x14(r1)
    lwz r7, 0x18(r1)
    lwz r8, 0x1C(r1)
    lwz r9, 0x20(r1)
    lwz r10, 0x24(r1)
    lwz r0, 0x34(r1)
    mtlr r0
    addi r1, r1, 0x30
    bctr
}

static asm void ShutdownStub() {
    nofralloc
    stwu r1, -0x30(r1)
    mflr r0
    stw r0, 0x34(r1)
    stw r3, 0x08(r1)
    stw r4, 0x0C(r1)
    stw r5, 0x10(r1)
    stw r6, 0x14(r1)
    stw r7, 0x18(r1)
    stw r8, 0x1C(r1)
    stw r9, 0x20(r1)
    stw r10, 0x24(r1)
    bl PCP_OnShutdownFriendsMatch
    mtctr r3
    lwz r3, 0x08(r1)
    lwz r4, 0x0C(r1)
    lwz r5, 0x10(r1)
    lwz r6, 0x14(r1)
    lwz r7, 0x18(r1)
    lwz r8, 0x1C(r1)
    lwz r9, 0x20(r1)
    lwz r10, 0x24(r1)
    lwz r0, 0x34(r1)
    mtlr r0
    addi r1, r1, 0x30
    bctr
}
// clang-format on

// Cache maintenance for one patched word (rr-pulsar has no OS::DCFlushRange/ICInvalidateRange):
// write the data cache line back, drop the instruction cache line. r3 = address of the word.
// clang-format off
static asm void FlushWord(u32 *at) {
    nofralloc
    dcbst 0, r3
    sync
    icbi 0, r3
    sync
    isync
    blr
}
// clang-format on

static void WriteCode(u32 *at, u32 insn) {
    *at = insn;
    FlushWord(at);
}

struct EntryHook {
    const char *name;
    u32 *target;
    void (*stub)();
    u32 *thunk;
    HookPlan plan;
};

// Reads only. The word before the target is logged too: it tells a function start (blr =
// 4e800020, or padding) from the middle of a function when the check fails.
static bool Prepare(EntryHook &hook) {
    const u32 first = hook.target[0];
    const bool ok = PlanEntryHook(reinterpret_cast<u32>(hook.target), first, reinterpret_cast<u32>(hook.stub), reinterpret_cast<u32>(hook.thunk), hook.plan);
    OS::Report("PCP: hook %s at %08x: first %08x, word before %08x: %s\n", hook.name, reinterpret_cast<u32>(hook.target), first, hook.target[-1],
               ok ? "ok" : (IsFrameSetup(first) ? "REFUSED, out of branch range" : "REFUSED, not stwu r1,-N(r1)"));
    return ok;
}

static void Install(const EntryHook &hook) {
    WriteCode(&hook.thunk[0], hook.plan.thunk0);
    WriteCode(&hook.thunk[1], hook.plan.thunk1);
    const int irq = OS::DisableInterrupts();
    WriteCode(hook.target, hook.plan.atTarget);
    OS::RestoreInterrupts(irq);
}

// Both addresses are in main.dol, so this is safe from a BootHook (StaticR.rel is not needed).
static void InstallHooks() {
    EntryHook process = {"DWC_ProcessFriendsMatch", reinterpret_cast<u32 *>(kmRuntimeAddr(0x800d115c)), &TickStub, sTickThunk};
    bool ok = Prepare(process);
#if PCP_HOOK_SHUTDOWN
    EntryHook shutdown = {"DWC_ShutdownFriendsMatch", reinterpret_cast<u32 *>(kmRuntimeAddr(0x800d0fec)), &ShutdownStub, sShutdownThunk};
    ok = Prepare(shutdown) && ok;  // both are looked at and logged, also if the first one failed
#endif
    if (!ok) {
        ReportHookFailure();  // ERR_HOOK, nothing was written
        return;
    }
    Install(process);
#if PCP_HOOK_SHUTDOWN
    Install(shutdown);
#endif
}
static BootHook sInstallPcpHooks(InstallHooks, 3);

#else  // PCP_TICK_VIA_FRAMEHOOK

// Same "is the console on WFC" test as RatingMultiplier.cpp.
static void FrameTick() {
    static bool sWasOnline = false;
    const RKNet::Controller *controller = RKNet::Controller::sInstance;
    const bool online = controller != nullptr && controller->connectionState != RKNet::CONNECTIONSTATE_SHUTDOWN;
    if (online) {
        ApplySetting();
        Tick();
    } else if (sWasOnline)
        OnSessionLost();  // the socket layer may already be gone: no delete possible from here
    sWasOnline = online;
}
static FrameLoadHook sPcpFrameTick(FrameTick);

#endif

}  // namespace PCP
}  // namespace Network
}  // namespace Pulsar
