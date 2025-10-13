#include <RetroRewind.hpp>

namespace Pulsar {
namespace Race {

// Enhanced Pause Menu [Ro]
// Ported to pulsar by Toadette Hack Fan
kmWrite32(0x8062c658, 0x38800019);
kmWrite32(0x8062c79c, 0x38800019);
kmWrite32(0x80633a98, 0x38600019);
kmWrite32(0x8062c8e0, 0x38800019);
kmWrite32(0x80633970, 0x38600019);
kmWrite32(0x8083d618, 0x60000000);

extern "C" void sInstance__8Racedata(void*);
extern "C" void sInstance__10SectionMgr(void*);
asmFunc EnhancedPauseMenu1() {
    ASM(
        lwz r3, sInstance__10SectionMgr @l(r6);
        lwz r12, sInstance__8Racedata @l(r6);
        lwz r0, 0x1760(r12);
        cmpwi r0, 2;
        beq end;

        cmpwi r4, 0x49;
        beq decreaseRaceNum;

        cmpwi r4, 0x4A;
        bne end;

        li r4, 0x4B;

        cmpwi r0, 1;
        beq decreaseRaceNum;

        li r4, 0x4C;

        decreaseRaceNum : lwz r6, 0x98(r3);
        lwz r31, 0x60(r6);
        subi r31, r31, 1;
        stw r31, 0x60(r6);

        li r31, 5;
        stw r31, 0x1764(r12);

        end : mr r31, r5;
        blr;)
}
kmCall(0x806024d8, EnhancedPauseMenu1);

extern "C" void sInstance__8Racedata(void*);
asmFunc EnhancedPauseMenu2() {
    ASM(
        lis r3, sInstance__8Racedata @ha;
        lwz r3, sInstance__8Racedata @l(r3);
        lwz r4, 0x1760(r3);
        cmpwi r4, 2;
        beq end;
        li r4, 0x1;
        stw r4, 0xD18(r3);
        stw r4, 0xE08(r3);
        stw r4, 0xEF8(r3);

        end : li r3, 0x6C4;
        blr;)
}
kmCall(0x80623df4, EnhancedPauseMenu2);

kmWrite32(0x80859068, 0x48000808);
kmWrite32(0x80858e38, 0x48000040);

}  // namespace Race
}  // namespace Pulsar