#include <RetroRewind.hpp>
#include <MarioKartWii/3D/GameScreenEffects/GameScreenEffects.hpp>
#include <kamek.hpp>

namespace Frameskip {

// original code by Wiimmfi Team
kmWrite32(0x800095e4, 0x60000000);
kmWrite32(0x800095f4, 0x60000000);
kmWrite32(0x8051b69c, 0x480000dc);
kmWrite32(0x80654400, 0x60000000);

asmFunc GetFrameskip1() {
    ASM(
        nofralloc;
        loc_0x0 : lis r3, 0x8000;
        lbz r4, 5665(r3);
        cmpwi r4, 0x0;
        beq + loc_0x34;
        lis r4, 0x8000;
        lwz r4, 5668(r4);
        cmpwi r4, 0x0;
        bne - loc_0x74;
        li r4, 0x0;
        stb r4, 5665(r3);
        li r0, 0x0;
        stb r0, 136(r30);
        b loc_0x70;

        loc_0x34 : lis r3, 0x8000;
        lbz r4, 5664(r3);
        cmpwi r4, 0x1;
        bne + loc_0x70;
        cmpwi r0, 0x0;
        bne + loc_0x70;
        lis r4, 0x8000;
        lwz r4, 5668(r4);
        cmpwi r4, 0x0;
        beq + loc_0x70;
        li r0, 0x1;
        stb r0, 136(r30);
        lis r4, 0x8000;
        stb r0, 5665(r4);
        b loc_0x74;

        loc_0x70 : stb r0, 5664(r3);

        loc_0x74 : cmpwi r0, 0x0;
        blr;)
}
kmCall(0x80562AD0, GetFrameskip1);

extern "C" void isTT__9DriverMgr(void*);
extern "C" void DisableInterrupts__2OSFv(void*);
extern "C" void RestoreInterrupts__2OSFi(void*);
extern "C" void GetTime__2OSFv(void*);
extern "C" void sInstance__Q25RKNet9PacketMgr(void*);
extern "C" void sInstance__8Racedata(void*);
extern "C" void GXDrawDone(void*);
extern "C" void beginFrame(void*);
asmFunc GetFrameskip2() {
    ASM(
        nofralloc;
        loc_0x0 : mr r3, r21;
        mr r4, r25;
        stwu r1, -40(r1);
        stw r28, 24(r1);
        lis r28, 0x8000;
        lwz r9, 5672(r28);
        stw r30, 32(r1);
        cmpwi cr7, r9, 0x0;
        lwz r30, 76(r3);
        beq - cr7, loc_0x4C;
        cmpwi cr7, r30, 0x0;
        beq - cr7, loc_0x4C;
        lbz r10, 105(r3);
        li r9, 0x0;
        stw r9, 5672(r28);
        subic r9, r10, 1;
        subfe r9, r9, r10;
        addi r9, r9, 0x1;
        stb r9, 8(r30);

        loc_0x4C : cmpwi cr7, r4, 0x0;
        li r9, 0x0;
        beq - cr7, loc_0x170;
        lis r9, isTT__9DriverMgr @ha;
        stw r31, 36(r1);
        lbz r9, isTT__9DriverMgr @l(r9);
        mr r31, r3;
        cmpwi cr7, r9, 0x0;
        nop;
        cmpwi cr7, r30, 0x0;
        li r9, 0x1;
        beq - cr7, loc_0x2C0;
        mflr r0;
        stw r24, 8(r1);
        stw r0, 44(r1);
        stw r25, 12(r1);
        lis r25, 0x8000;
        stw r26, 16(r1);
        stw r27, 20(r1);
        stw r29, 28(r1);
        lis r12, DisableInterrupts__2OSFv @ha;
        ori r12, r12, DisableInterrupts__2OSFv @l;
        mtctr r12;
        bctrl;
        mr r26, r3;
        mtctr r12;
        bctrl;
        lwz r9, 5676(r25);
        lis r8, 0x8000;
        lwz r29, 104(r30);
        mr r24, r3;
        lbz r27, 105(r31);
        add r29, r9, r29;
        lwz r10, 108(r30);
        addi r27, r27, 0x1;
        add r29, r29, r27;
        stw r27, 5680(r8);
        cmplw cr7, r10, r29;
        blt - cr7, loc_0x184;
        lis r10, 0x8000;
        lwz r10, 5684(r10);
        cmpw cr7, r9, r10;
        bge - cr7, loc_0x184;
        cmpwi cr7, r9, 0x0;
        beq - cr7, loc_0x3FC;

        loc_0x108 : lbz r8, 8(r30);
        lis r7, 0x8000;
        lwz r10, 5688(r7);
        add r9, r27, r9;
        add r8, r27, r8;
        mr r3, r26;
        stb r8, 8(r30);
        add r27, r10, r27;
        li r8, 0x0;
        lis r10, 0x8000;
        stw r9, 5676(r25);
        stw r27, 5688(r7);
        stw r8, 5692(r10);
        lis r12, RestoreInterrupts__2OSFi @ha;
        ori r12, r12, RestoreInterrupts__2OSFi @l;
        mtctr r12;
        bctrl;
        li r9, 0x0;

        loc_0x150 : lwz r0, 44(r1);
        lwz r24, 8(r1);
        lwz r25, 12(r1);
        mtlr r0;
        lwz r26, 16(r1);
        lwz r27, 20(r1);
        lwz r29, 28(r1);
        lwz r31, 36(r1);

        loc_0x170 : mr r3, r9;
        lwz r28, 24(r1);
        lwz r30, 32(r1);
        addi r1, r1, 0x28;
        b loc_0x424;

        loc_0x184 : cmpwi cr7, r9, 0x0;
        ble - cr7, loc_0x2D8;
        lis r10, 0x8000;
        stw r9, 5696(r10);
        lis r12, GetTime__2OSFv @h;
        ori r12, r12, GetTime__2OSFv @l;
        mtctr r12;
        bctrl;
        lwz r10, 108(r30);
        lis r9, 0x8000;
        addi r9, r9, 0x1644;
        li r8, 0x0;
        sub r29, r10, r29;
        li r10, 0x1;
        addic.r7, r29, 1;
        stw r3, 0(r9);
        stw r4, 4(r9);
        stw r8, 5676(r25);
        stw r10, 5672(r28);
        ble - loc_0x24C;
        lis r9, sInstance__Q25RKNet9PacketMgr @ha;
        lis r8, 0x8000;
        lwz r9, sInstance__Q25RKNet9PacketMgr @l(r9);
        stw r10, 5708(r8);
        cmpwi cr7, r9, 0x0;
        beq - cr7, loc_0x24C;
        lbz r10, 0(r9);
        cmpwi cr7, r10, 0x0;
        beq - cr7, loc_0x24C;
        lha r10, 18(r9);
        cmpwi cr7, r10, 0x0;
        bgt + cr7, loc_0x24C;
        lis r10, sInstance__8Racedata @ha;
        lwz r10, sInstance__8Racedata @l(r10);
        lwz r10, 2932(r10);
        cmpwi cr7, r10, 0x6;
        beq - cr7, loc_0x24C;
        lbz r10, 105(r31);
        cmpwi cr7, r10, 0x0;
        beq - cr7, loc_0x230;
        andi.r10, r7, 0x1;
        beq - loc_0x230;
        addi r7, r29, 0x2;

        loc_0x230 : lis r6, 0x8000;
        lhz r8, 16(r9);
        lwz r10, 5712(r6);
        add r8, r7, r8;
        add r10, r10, r7;
        sth r8, 16(r9);
        stw r10, 5712(r6);

        loc_0x24C : lis r31, 0x8000;
        mr r3, r30;
        addi r31, r31, 0x1654;
        lis r12, beginFrame @h;
        ori r12, r12, beginFrame @l;
        mtctr r12;
        bctrl;
        lwz r10, 0(r31);
        lis r9, 0x8000;
        lwz r11, 4(r31);
        addi r9, r9, 0x165C;
        stw r10, 0(r9);
        stw r11, 4(r9);
        lis r12, GetTime__2OSFv @h;
        ori r12, r12, GetTime__2OSFv @l;
        mtctr r12;
        bctrl;
        stw r3, 0(r31);
        lis r9, 0x8000;
        li r10, 0x1;
        mr r3, r26;
        stw r10, 5692(r9);
        stw r4, 4(r31);
        lis r12, RestoreInterrupts__2OSFi @ha;
        ori r12, r12, RestoreInterrupts__2OSFi @l;
        mtctr r12;
        bctrl;
        li r9, 0x1;
        b loc_0x150;

        loc_0x2C0 : lwz r31, 36(r1);
        mr r3, r9;
        lwz r28, 24(r1);
        lwz r30, 32(r1);
        addi r1, r1, 0x28;
        b loc_0x424;

        loc_0x2D8 : lis r9, 0x8000;
        li r10, 0x0;
        stw r10, 5696(r9);
        lis r12, GetTime__2OSFv @h;
        ori r12, r12, GetTime__2OSFv @l;
        mtctr r12;
        bctrl;
        lis r7, 0x8000;
        lis r10, 0x8000;
        lbz r8, 5666(r7);
        addi r10, r10, 0x1668;
        lis r9, 0x8000;
        stw r3, 0(r10);
        cmpwi cr7, r8, 0x0;
        addi r9, r9, 0x1644;
        stw r4, 4(r10);
        li r11, 0x0;
        li r10, 0x0;
        stw r11, 4(r9);
        stw r10, 0(r9);
        beq - cr7, loc_0x334;
        subi r8, r8, 0x1;
        stb r8, 5666(r7);

        loc_0x334 : cmpwi cr7, r24, 0x0;
        b loc_0x24C;
        lis r31, sInstance__Q25RKNet9PacketMgr @ha;
        lwz r9, sInstance__Q25RKNet9PacketMgr @l(r31);
        cmpwi cr7, r9, 0x0;
        beq - cr7, loc_0x24C;
        lbz r10, 0(r9);
        cmpwi cr7, r10, 0x0;
        beq - cr7, loc_0x24C;
        lha r9, 18(r9);
        cmpwi cr7, r9, 0x0;
        bgt + cr7, loc_0x24C;
        lis r9, sInstance__8Racedata @ha;
        lwz r9, sInstance__8Racedata @l(r9);
        lwz r9, 2932(r9);
        cmpwi cr7, r9, 0x6;
        beq - cr7, loc_0x24C;
        mr r3, r30;
        lis r12, beginFrame @h;
        ori r12, r12, beginFrame @l;
        mtctr r12;
        bctrl;
        lis r12, GXDrawDone @ha;
        ori r12, r12, GXDrawDone @l;
        mtctr r12;
        bctrl;
        lwz r10, 8016(r31);
        lis r8, 0x8000;
        lhz r9, 16(r10);
        add r9, r27, r9;
        rlwinm r9, r9, 0, 16, 31;
        sth r9, 16(r10);
        stw r9, 5732(r8);
        b loc_0x24C;
        mflr r0;
        mr r3, r30;
        stw r0, 44(r1);
        lis r12, beginFrame @h;
        ori r12, r12, beginFrame @l;
        mtctr r12;
        bctrl;
        lwz r0, 44(r1);
        li r9, 0x1;
        lwz r31, 36(r1);
        mr r3, r9;
        lwz r28, 24(r1);
        mtlr r0;
        lwz r30, 32(r1);
        addi r1, r1, 0x28;
        b loc_0x424;

        loc_0x3FC : lis r12, GetTime__2OSFv @h;
        ori r12, r12, GetTime__2OSFv @l;
        mtctr r12;
        bctrl;
        lis r10, 0x8000;
        addi r10, r10, 0x1668;
        lwz r9, 5676(r25);
        stw r3, 0(r10);
        stw r4, 4(r10);
        b loc_0x108;

        loc_0x424 : mr r25, r3;
        cmpwi r25, 0x0;
        blr;)
}
kmCall(0x800095C4, GetFrameskip2);
kmWrite32(0x80001614, 0x000F7709);
kmWrite32(0x80001634, 0x00000006);

static void ResetFrameskipState() {
    *(u32*)0x80001638 = 0;
}
static SectionLoadHook ResetFrameskipHook(ResetFrameskipState);

static void PatchedGameScreenEffectsMgrUpdate(GameScreenEffectsMgr* mgr) {
    if (*(u32*)0x80001638 >= 30) {
        return;
    }
    mgr->Update();
}
kmCall(0x805A9050, PatchedGameScreenEffectsMgrUpdate);

}  // namespace Frameskip