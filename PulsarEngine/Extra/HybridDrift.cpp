#include <RetroRewind.hpp>

namespace HybridDrift {

// original code by Ismy and CLF78, with fixes by Ro.
asmFunc GetHybridDrift1() {
    ASM(
        lwz r0, 0x14(r3);

        // Check if we're an inside drifting, if we are, end the code (Bug is only for outside drift bikes)
        lwz r12, 0(r28);
        lwz r12, 0(r12);
        lwz r12, 0x14(r12);
        lwz r12, 0(r12);
        lwz r12, 0x4(r12);
        cmpwi r12, 2;
        beqlr;

        // Check if we're a kart, if we are, skip to the end
        cmpwi r12, 0;
        beq skip;

        // Check if we're drifting, if we are, end the code
        outsidefix : lwz r3, 4(r3);
        andis.r3, r3, 0x1000;
        bnelr;

        // Fix slide issue (Load a dummy value to force the next branch)
        skip : li r0, 0;
        blr;)
}
kmCall(0x8057930C, GetHybridDrift1);
kmCall(0x80578DCC, GetHybridDrift1);

asmFunc GetHybridDrift3() {
    ASM(
        lwz r0, 0x14(r3);
        rlwinm.r12, r0, 0, 18, 18;
        beq end2;
        ori r0, r0, 0x10;
        stw r0, 0x14(r3);
        end2 : lwz r0, 0x4(r3);
        blr;)
}
kmCall(0x8057DFA8, GetHybridDrift3);

asmFunc GetHybridDrift4() {
    ASM(
        lwz r0, 0x14(r4);
        rlwinm.r12, r0, 0, 18, 18;
        beq end2;
        ori r0, r0, 0x10;
        stw r0, 0x14(r4);
        end2 : lwz r0, 0x4(r4);
        blr;)
}
kmCall(0x8057E018, GetHybridDrift4);

asmFunc GetHybridDrift6() {
    ASM(
        rlwinm.r0, r0, 0, 27, 27;
        li r0, 0;
        stw r0, 0x1C8(r3);)
}
kmCall(0x8057E108, GetHybridDrift6);

asmFunc GetHybridDrift7() {
    ASM(
        lwz r3, 0x4(r30);
        andi.r4, r3, 0x84;
        beq end;
        lwz r4, 0x14(r30);
        rlwinm r4, r4, 0, 28, 26;
        stw r4, 0x14(r30);
        rlwinm r3, r3, 0, 4, 2;
        stw r3, 0x4(r30);
        end : mr r3, r30;
        blr;)
}
kmCall(0x80594AA8, GetHybridDrift7);

kmWrite8(0x8059450E, 0x00000020);
kmWrite32(0x80594A60, 0x60000000);
kmWrite32(0x805A35BC, 0x38600000);
kmWrite16(0x80745AB0, 0x00004800);
kmWrite32(0x808CB70A, 0x00000000);

}  // namespace HybridDrift