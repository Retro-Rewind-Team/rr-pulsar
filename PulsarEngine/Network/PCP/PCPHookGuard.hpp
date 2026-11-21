#ifndef _PUL_PCP_HOOK_GUARD_
#define _PUL_PCP_HOOK_GUARD_

#include <kamek.hpp>

namespace Pulsar {
namespace Network {
namespace PCP {

// opcode 37 (stwu), rS = r1, rA = r1, displacement < 0
inline bool IsFrameSetup(u32 insn) {
    return (insn & 0xFFFF8000) == 0x94218000;
}

// "b dest" placed at "at". False if out of range for a relative branch.
inline bool MakeBranch(u32 at, u32 dest, u32 &insn) {
    const s32 delta = static_cast<s32>(dest - at);
    if (delta < -0x02000000 || delta > 0x01FFFFFC || (delta & 3) != 0)
        return false;
    insn = 0x48000000 | (static_cast<u32>(delta) & 0x03FFFFFC);
    return true;
}

struct HookPlan {
    u32 atTarget;  // b stub
    u32 thunk0;  // the stwu that stood at the target; position independent
    u32 thunk1;  // b target + 4
};

// Everything that has to be written for one entry hook, or false if the target must not be touched.
inline bool PlanEntryHook(u32 target, u32 firstInsn, u32 stub, u32 thunk, HookPlan &plan) {
    if (!IsFrameSetup(firstInsn))
        return false;
    plan.thunk0 = firstInsn;
    return MakeBranch(thunk + 4, target + 4, plan.thunk1) && MakeBranch(target, stub, plan.atTarget);
}

}  // namespace PCP
}  // namespace Network
}  // namespace Pulsar

#endif
