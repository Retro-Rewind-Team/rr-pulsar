#include <kamek.hpp>
#include <MarioKartWii/UI/Section/SectionMgr.hpp>
#include <MarioKartWii/Race/RaceData.hpp>
#include <MarioKartWii/RKNet/USER.hpp>
#include <Network/Network.hpp>
#include <UI/UI.hpp>

#define pushStack       \
    stwu sp, -0x80(sp); \
    mflr r0;            \
    stw r0, 0x84(sp);   \
    stmw r3, 8(sp);

#define popStack      \
    lmw r3, 8(sp);    \
    lwz r0, 0x84(sp); \
    mtlr r0;          \
    addi sp, sp, 0x80;

extern void MD5Digest(const unsigned char*, unsigned int, unsigned char*);

// These are not actually functions.
extern void saveFcAndCountryHookExit(void);
extern void saveFcAndCountryInLiveHookExit(void);
extern void displayFcAndCountryHookExit(void);
extern void displayFcAndCountryInResultFillNameHookExit(void);
extern void displayFcAndCountryInResultFillTimeHookExit(void);

namespace Pulsar {
namespace DisplayFc {

typedef struct {
    unsigned int pid[12];
    unsigned char country[12];
} SavedFc;

SavedFc savedFc;

void saveFcAndCountry(unsigned char* r3, unsigned int r19) {
    unsigned int index = r19 / 4;
    unsigned char country = *(r3 + 0x178);
    unsigned int pid = *((unsigned int*)((void*)(r3 + 0x174)));
    savedFc.pid[index] = pid;
    savedFc.country[index] = country;
}

asmFunc saveFcAndCountryHook() {
    ASM(
        nofralloc;
        pushStack
            mr r4,
        r19;
        bl saveFcAndCountry;
        popStack
            lbz r4,
        0x185(r3);
        lis r12, saveFcAndCountryHookExit @h;
        ori r12, r12, saveFcAndCountryHookExit @l;
        mtlr r12;
        blr;)
}

asmFunc saveFcAndCountryInLiveHook() {
    ASM(
        nofralloc;
        pushStack
            mr r4,
        r19;
        bl saveFcAndCountry;
        popStack
            lbz r4,
        0x185(r3);
        lis r12, saveFcAndCountryInLiveHookExit @h;
        ori r12, r12, saveFcAndCountryInLiveHookExit @l;
        mtlr r12;
        blr;)
}

kmBranch(0x806513e0, saveFcAndCountryHook);
kmBranch(0x8060a2b0, saveFcAndCountryInLiveHook);

unsigned char isInOnlineSection(unsigned int sectionID) {
    if (sectionID >= 0x55 && sectionID <= 0x77) {
        return 1;
    }
    return 0;
}

unsigned char isInLiveViewSection(unsigned int sectionID) {
    if (sectionID == 0x6A || sectionID == 0x6B || sectionID == 0x6E || sectionID == 0x6F) {
        return 1;
    }
    return 0;
}

unsigned char get8bit(unsigned char c) {
    if (c < 0x3a) return c - 0x30;
    return c - 87;
}

unsigned char getByte(unsigned char* src) {
    return get8bit(src[0]) * 16 + get8bit(src[1]);
}

void calcFc(unsigned int pid, wchar_t* dest) {
    // calculate fc from pid
    // https://wiki.tockdom.com/wiki/PID
    unsigned char calcFcBuf[8];
    unsigned char calcFcBufDest[33];
    unsigned int fc4Digit[3];
    calcFcBuf[0] = pid;
    calcFcBuf[1] = pid >> 8;
    calcFcBuf[2] = pid >> 16;
    calcFcBuf[3] = pid >> 24;
    calcFcBuf[4] = 'J';
    calcFcBuf[5] = 'C';
    calcFcBuf[6] = 'M';
    calcFcBuf[7] = 'R';
    MD5Digest(calcFcBuf, 8, calcFcBufDest);
    // MD5Digest returns hsash as string, so i need converting it with getByte.
    u64 fc = ((u64)pid) | (((u64)getByte(calcFcBufDest) >> 1) << 32);

    for (unsigned int i = 0; i < 3; i++) {
        fc4Digit[i] = (unsigned int)(fc % 10000);
        fc = fc / 10000;
    }
    swprintf(dest, 0x10, L"%04d-%04d-%04d", fc4Digit[2], fc4Digit[1], fc4Digit[0]);
}

void displayFcAndCountry(LayoutUIControl* r3, unsigned int r4) {
    Text::Info textInfo;
    wchar_t fcDisplayStr[0x10];
    char flagPaneName[4];
    unsigned int pid = 0;
    unsigned char country = 0;
    unsigned int index = r4;

    unsigned int hudSlot = Racedata::sInstance->GetHudSlotId(r4);

    if (!isInOnlineSection(SectionMgr::sInstance->curSection->sectionId)) {
        if (r3->layout.GetPaneByName("flag")) r3->SetPaneVisibility("flag", 0);
        return;
    }
    if (hudSlot < 2 && (!isInLiveViewSection(SectionMgr::sInstance->curSection->sectionId))) {  // for local player
        country = RKNet::USERHandler::sInstance->toSendPacket.country;
        pid = (unsigned int)(RKNet::USERHandler::sInstance->toSendPacket.fc);
    } else {  // for remote player
        pid = savedFc.pid[index];
        country = savedFc.country[index];
    }

    snprintf(flagPaneName, sizeof(flagPaneName), "%03d", country);

    if (r3->PicturePaneExists(flagPaneName) && r3->layout.GetPaneByName("flag")) {
        r3->SetPaneVisibility("flag", 1);
        r3->SetPicturePane("flag", flagPaneName);
    } else if (r3->layout.GetPaneByName("flag")) {
        r3->SetPaneVisibility("flag", 0);
    }

    if (!r3->layout.GetPaneByName("user_id")) return;
    calcFc(pid, fcDisplayStr);
    textInfo.strings[0] = fcDisplayStr;
    r3->SetTextBoxMessage("user_id", UI::BMG_TEXT, &textInfo);
}

asmFunc displayFcAndCountryHook() {
    ASM(
        nofralloc;
        stw r0, 0x154(sp);
        pushStack
            bl displayFcAndCountry;
        popStack
            lis r12,
        displayFcAndCountryHookExit @h;
        ori r12, r12, displayFcAndCountryHookExit @l;
        mtlr r12;
        blr;)
}

asmFunc displayFcAndCountryInResultFillNameHook() {
    ASM(
        nofralloc;
        stw r0, 0xE4(sp);
        pushStack
            bl displayFcAndCountry;
        popStack
            lis r12,
        displayFcAndCountryInResultFillNameHookExit @h;
        ori r12, r12, displayFcAndCountryInResultFillNameHookExit @l;
        mtlr r12;
        blr;)
}

asmFunc displayFcAndCountryInResultFillTimeHook() {
    ASM(
        nofralloc;
        stw r0, 0xE4(sp);
        pushStack
            bl displayFcAndCountry;
        popStack
            lis r12,
        displayFcAndCountryInResultFillTimeHookExit @h;
        ori r12, r12, displayFcAndCountryInResultFillTimeHookExit @l;
        mtlr r12;
        blr;)
}

kmBranch(0x807f004c, displayFcAndCountryHook);
kmBranch(0x807f52fc, displayFcAndCountryInResultFillNameHook);
kmBranch(0x807f5964, displayFcAndCountryInResultFillTimeHook);

}  // namespace DisplayFc
}  // namespace Pulsar
