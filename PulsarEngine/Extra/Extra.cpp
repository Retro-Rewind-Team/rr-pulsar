#include <RetroRewind.hpp>

namespace Codes {

// HUD Color [Spaghetti Noppers]
kmWrite32(0x80895CC0, 0x00FF00FD);
kmWrite32(0x80895CC4, 0x00FF00FD);
kmWrite32(0x80895CC8, 0x00FF00FD);
kmWrite32(0x80895CCC, 0x00FF00FD);
kmWrite32(0x80895CD0, 0x00FF00FD);
kmWrite32(0x80895CD4, 0x00FF0046);
kmWrite32(0x80895CD8, 0x00FF00FF);
kmWrite32(0x80895CDC, 0x00FF00FF);
kmWrite32(0x80895CE0, 0x00FF00FF);
kmWrite32(0x80895CE4, 0x00FF00FF);
kmWrite32(0x80895CE8, 0x00FF00FF);
kmWrite32(0x80895CEC, 0x00FF0046);

// Remove Background Blur [Davidevgen]
kmWrite32(0x80258184, 0x30);

// Anti Online Item Delimiters [Ro]
asmFunc GetItemDelimiterShock() {
    ASM(
        nofralloc;
        loc_0x0 : mflr r12;
        cmpwi r7, 0x1;
        bne + validLightning;
        addi r12, r12, 0x12C;
        mtlr r12;
        blr;
        validLightning : mulli r29, r3, 0xF0;
        blr;)
}
kmCall(0x807B7C34, GetItemDelimiterShock);

asmFunc GetItemDelimiterBlooper() {
    ASM(
        nofralloc;
        loc_0x0 : mflr r12;
        cmpwi r7, 0x1;
        bne + validBlooper;
        addi r12, r12, 0x1A8;
        mtlr r12;
        blr;
        validBlooper : addi r11, r1, 0x50;
        blr;)
}
kmCall(0x807A81C0, GetItemDelimiterBlooper);

asmFunc GetItemDelimiterPOW() {
    ASM(
        nofralloc;
        loc_0x0 : mflr r12;
        cmpwi r7, 0x1;
        bne + validPOW;
        addi r12, r12, 0x48;
        mtlr r12;
        blr;
        validPOW : mr r30, r3;
        blr;)
}
kmCall(0x807B1B44, GetItemDelimiterPOW);

// Anti Mii Crash
asmFunc AntiWiper() {
    ASM(
        nofralloc;
        loc_0x0 : cmpwi r4, 0x6;
        ble validMii;
        lhz r12, 0xE(r30);
        cmpwi r12, 0x0;
        bne validMii;
        li r31, 0x0;
        li r4, 0x6;
        validMii : mr r29, r4;
        blr;)
}
kmCall(0x800CB6C0, AntiWiper);
kmWrite32(0x80526660, 0x38000001);  // Credits to Ro for the last line.

// Anti Item Collission Crash [Marioiscool246]
extern "C" void __ptmf_test(void*);
asmFunc AntiItemColCrash() {
    ASM(
        nofralloc;
        loc_0x0 : stwu r1, -0xC(r1);
        stw r31, 8(r1);
        mflr r31;
        addi r3, r29, 0x174;
        bl __ptmf_test;
        cmpwi r3, 0;
        bne end;
        addi r31, r31, 0x14;

        end : mtlr r31;
        lwz r31, 8(r1);
        addi r1, r1, 0xC;
        mr r3, r29;
        blr;)
}
kmCall(0x807A1A54, AntiItemColCrash);

// Mii Outfit C Anti-Crash
kmWrite8(0x8089089D, 0x00000062);
kmWrite8(0x808908A9, 0x00000062);
kmWrite8(0x808908E5, 0x00000062);
kmWrite8(0x808908F1, 0x00000062);
kmWrite8(0x8089092D, 0x00000062);
kmWrite8(0x80890939, 0x00000062);

// Item Spam Anti-Freeze [???]
asmFunc ItemSpamAntiFreeze() {
    ASM(
        loc_0x0 : lbz r12, 0x1C(r27);
        add r12, r30, r12;
        cmpwi r12, 0xE0;
        blt + loc_0x18;
        li r0, 0;
        stb r0, 0x19(r27);

        loc_0x18 : lbz r0, 0x19(r27);)
}
kmCall(0x8065BBD4, ItemSpamAntiFreeze);

// Allow All Vehicles in Battle Mode [Nameless, Scruffy]
kmWrite32(0x80553F98, 0x3880000A);
kmWrite32(0x8084FEF0, 0x48000044);
kmWrite32(0x80860A90, 0x38600000);

// Instant Voting Roulette Decide [Ro]
kmWrite32(0x80643BC4, 0x60000000);
kmWrite32(0x80643C2C, 0x60000000);

// No Disconnect on Countdown [_tZ]
kmWrite32(0x80655578, 0x60000000);

// Change VR Limit [XeR]
kmWrite16(0x8052D286, 0x00007530);
kmWrite16(0x8052D28E, 0x00007530);
kmWrite16(0x8064F6DA, 0x00007530);
kmWrite16(0x8064F6E6, 0x00007530);
kmWrite16(0x8085654E, 0x00007530);
kmWrite16(0x80856556, 0x00007530);
kmWrite16(0x8085C23E, 0x00007530);
kmWrite16(0x8085C246, 0x00007530);
kmWrite16(0x8064F76A, 0x00007530);
kmWrite16(0x8064F776, 0x00007530);
kmWrite16(0x808565BA, 0x00007530);
kmWrite16(0x808565C2, 0x00007530);
kmWrite16(0x8085C322, 0x00007530);
kmWrite16(0x8085C32A, 0x00007530);

// Mushroom Glitch Fix [Vabold]
kmWrite8(0x807BA077, 0x00);

// Allow WFC on Wiimmfi Patched ISOs
kmWrite32(0x800EE3A0, 0x2C030000);
kmWrite32(0x800ECAAC, 0x7C7E1B78);

// Disable Camera Shaking from Bombs [ZPL]
kmWrite32(0x805a906c, 0x4E800020);

// No VR/BR Loss on Disconnect [Bully]
kmWrite32(0x80856560, 0x60000000);  // Disable VR loss
kmWrite32(0x808565CC, 0x60000000);  // Disable BR loss

// Ultra Uncut [MrBean35000vr + Chadderz]
asmFunc GetUltraUncut() {
    ASM(
        nofralloc;
        loc_0x0 : lbz r3, 0x1C(r29);
        cmplwi r3, 0x1;
        ble + loc_0x10;
        mr r0, r30;

        loc_0x10 : cmplw r30, r0;
        blr;)
}
kmCall(0x8053511C, GetUltraUncut);

// Anti Lag Start [Ro]
extern "C" void sInstance__8Racedata(void*);
asmFunc AntiLagStart() {
    ASM(
        nofralloc;
        loc_0x0 : lwz r12, sInstance__8Racedata @l(r30);
        lwz r12, 0xB70(r12);
        cmpwi r12, 0x7;
        blt - loc_0x14;
        li r3, 0x1;

        loc_0x14 : cmpwi r3, 0x0;
        blr;)
}
kmCall(0x80533430, AntiLagStart);

// VR System Changes [MrBean35000vr]
// Multiply VR difference by 2 [Winner]
asmFunc GetVRScaleWin() {
    ASM(
        li r5, 2;
        divw r3, r3, r5;
        extsh r3, r3;)
}
kmCall(0x8052D150, GetVRScaleWin);

// Cap VR loss from one victorious opponent between 0 and -8.
asmFunc GetCapVRLoss() {
    ASM(
        lwz r3, 0x14(r1);
        cmpwi r3, -8;
        bge 0f;
        li r3, -8;
        b 1f;
        0 :;
        cmpwi r3, 0;
        ble 1f;
        li r3, 0;
        1 :;)
}
kmCall(0x8052D260, GetCapVRLoss);

// Cap VR gain from one defeated opponent between 2 and 12.
asmFunc GetCapVRGain() {
    ASM(
        lwz r3, 0x14(r1);
        cmpwi r3, 2;
        bge 0f;
        li r3, 2;
        b 1f;
        0 :;
        cmpwi r3, 12;
        ble 1f;
        li r3, 12;
        1 :;)
}
kmCall(0x8052D1B0, GetCapVRGain);

// Prevent Lag Abuse [???]
kmWrite32(0x80654b00, 0x4E800020);

// Fix star offroad glitch after cannon [Ro]
asmFunc StarOffroadFix() {
    ASM(
        nofralloc;
        andi.r11, r0, 0x80;
        andis.r12, r0, 0x8000;
        or.r0, r11, r12;
        blr;)
}
kmCall(0x8057C3F8, StarOffroadFix);

// Force player to not be penalized [B_squo]
kmWrite32(0x80549898, 0x38600000);
kmWrite32(0x8054989c, 0x4E800020);

// Deflicker when 480p [MKW-SP]
asmFunc Deflicker() {
    ASM(
        nofralloc;
        cntlzw r0, r3;
        lwz r5, 0x14(r5);
        cmpwi r5, 0x0;
        bnelr;
        li r0, 0x0;
        blr;)
}
kmCall(0x8021A028, Deflicker);

// Edit Battle Item Point Probabilities [Zeraora, B_squo]
kmWrite32(0x807bd494, 0x38000000);
kmWrite32(0x807bd498, 0x38A00003);
kmWrite32(0x807bd4a8, 0x38000000);
kmWrite32(0x807bd4ac, 0x38A00003);
kmWrite32(0x807bd4b4, 0x38000000);
kmWrite32(0x807bd4b8, 0x38A00003);

// Blooper's lighting matches vehicle's lighting [B_squo]
kmWrite32(0x807a8a5c, 0x60000000);

// Load Vehicle Arm Parts Online [B_squo]
kmWrite32(0x80577724, 0x48000024);

// Fix Unfocused Small Mii Icon Border [B_squo]
kmWrite32(0x807eb774, 0x2c000017);

// CPUs/Online Players Have High Detail Particles [Ro]
kmWrite32(0x8068E494, 0x38600001);

// Show Position in Live View [MrBean35000vr]
kmWrite32(0x806335B0, 0x3860021A);

// Fix Online Players Stuck on Halfpipe (Halfpipe Warp Fix) [Ro]
asmFunc halfpipeWarpFix() {
    ASM(
        nofralloc;
        lwz r11, 8(r4);
        rlwinm.r12, r11, 0, 21, 21;
        beq - loc_0x20;
        lha r0, 86(r31);
        cmpwi r0, 0x52;
        blt - loc_0x20;
        rlwinm r11, r11, 0, 22, 20;
        stw r11, 8(r4);

        loc_0x20 :;
        mr r4, r11;
        blr;)
}
kmCall(0x8058BF58, halfpipeWarpFix);

// Show Your Respawn Boost Online [Ro] #1
asmFunc respawnBoostFix1() {
    ASM(
        nofralloc;
        lwz r4, 0(r3);
        lwz r4, 60(r4);
        cmpwi r4, 0x0;
        beq - loc_0x20;
        lwz r4, 16(r4);
        lwz r5, 16(r4);
        xoris r5, r5, 16384;
        stw r5, 16(r4);

        loc_0x20 :;
        li r4, 0x3;
        blr;)
}
kmCall(0x80581E40, respawnBoostFix1);

asmFunc respawnBoostFix2() {
    ASM(
        nofralloc;
        lwz r4, 0(r3);
        lwz r4, 60(r4);
        cmpwi r4, 0x0;
        beq - loc_0x20;
        lwz r4, 16(r4);
        lwz r5, 16(r4);
        xoris r5, r5, 16384;
        stw r5, 16(r4);

        loc_0x20 :;
        li r4, 0x3;
        blr;)
}
kmCall(0x805820B0, respawnBoostFix2);

// No Shell Tail Dissolve [Ro]
kmWrite32(0x8068DD68, 0x38600000);

// Online Miis have facial expressions [B_squo]
kmWrite32(0x807C7944, 0x38800000);

// Live View Icon Shadow Bug Fix [B_squo]
kmWrite32(0x807eb988, 0x807c01c0);

// Fix Mii opponents having silent / Rosalina voice Bug [B_squo]
kmWrite32(0x8086975C, 0x4082001C);

// Online Miis look at the camera when finishing in Live View [B_squo]
kmWrite32(0x80596770, 0x60000000);

// Invincibility Period Against Cars/Trucks Objects in All Slots [Ro]
kmWrite32(0x806D686C, 0x3800000A);
kmWrite32(0x80827968, 0x38000000);
// kmWrite32(0x8078E1EC, 0x3800000A);

// Slot Specific Objects Work in All Slots (pylon01, sunDS, FireSnake and begoman_spike) [Ro]
kmWrite32(0x8082A4F8, 0x3800000A);

// Cancel Friend Room Joining by Pressing B [Ro]
extern "C" void ptr_inputBase(void*);
asmFunc friendRoomJoinCancel() {
    ASM(
        nofralloc;
        lis r31, ptr_inputBase @ha;
        lwz r31, ptr_inputBase @l(r31);
        lhz r31, 0x60(r31);
        andi.r31, r31, 0x2;
        beq end;

        li r3, 3;

        end :;
        cmpwi r3, 3;
        blr;)
}
kmCall(0x805DD85C, friendRoomJoinCancel);

/*
// Dead/Inactive Bomb Cars Are Visible [Ro]
kmWrite32(0x806D7FF8, 0x81830008);
kmWrite32(0x806D7FFC, 0x38600044);
kmWrite32(0x806D8000, 0x986C012C);
kmWrite32(0x806D8E5C, 0x81830008);
kmWrite32(0x806D8E60, 0x38600054);
kmWrite32(0x806D8E64, 0x986C012C);

// Dead Goombas Are Visible [Ro]
kmWrite32(0x806DC810, 0x48000084);
*/

// Play Character Icon Damage Animation When Burned Out [Ro]
asmFunc burnoutIconFix() {
    ASM(
        nofralloc;
        lwz r0, 8(r3);
        andis.r12, r0, 0x4;
        beq - loc_0x10;
        ori r0, r0, 0x1;

        loc_0x10 :;
        blr;)
}
kmCall(0x807EB38C, burnoutIconFix);

// Allow Pausing Before Race Starts [Sponge]
kmWrite32(0x80856a28, 0x40810050);

// jugemnu_lap.brres [ZPL]
kmWrite16(0x808a22ec, 'RR');

// Disable 6 minute time limit Online [CLF78]
kmWrite32(0x8053F478, 0x4800000C);

// Clear Exhaust Pipe Boost Particle After Damage [Ro]
extern "C" void exhaustPipeboost(void*);
asmFunc exhaustPipeboostFix() {
    ASM(
        nofralloc;
        lis r3, exhaustPipeboost @h;
        lwz r3, exhaustPipeboost @l(r3);
        lwz r3, 104(r3);
        lwz r4, 0(r31);
        lwz r5, 40(r4);
        lwz r4, 0(r4);
        lbz r4, 16(r4);
        mulli r4, r4, 0x4;
        lwzx r3, r3, r4;
        li r0, 0x0;
        stw r0, 24(r3);
        blr;)
}
kmCall(0x805674B8, exhaustPipeboostFix);

}  // namespace Codes