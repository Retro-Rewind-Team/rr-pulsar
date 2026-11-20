#include <kamek.hpp>
#include <MarioKartWii/GlobalFunctions.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <MarioKartWii/UI/Ctrl/UIControl.hpp>
#include <MarioKartWii/UI/Page/Menu/CharacterSelect.hpp>
#include <MarioKartWii/UI/Section/SectionMgr.hpp>

// Each player's own OK marker on a shared driver with 2-4 local players offline, ported from mkw-sp
// PR #452 (LICENSE_mkw-sp). Online rooms and 1P keep RR's buttons.

namespace Pulsar {
namespace UI {

// From scripts/gen_charselect_multi.py; their OK markers take half the button with 2 players and a quarter with 3-4.
static const char multiButtonCtrName2[] = "CharacterSelect%d_%d_Multi2";
static const char multiButtonCtrName4[] = "CharacterSelect%d_%d_Multi4";

// Set by LoadButton's first multiplayer hook for the later hooks, which run where a call would clobber live registers. An online
// character select is built before any room exists, so the section decides, as CharacterSelect::OnInit's IsOnlineSection test does.
static bool sameCharacterOffline;

static bool UpdateSameCharacterOffline() {
    const SectionMgr *sectionMgr = SectionMgr::sInstance;
    const RKNet::Controller *controller = RKNet::Controller::sInstance;
    const bool onlineSection = sectionMgr != nullptr && sectionMgr->curSection != nullptr && IsOnlineSection(sectionMgr->curSection->sectionId);
    sameCharacterOffline = !onlineSection && (controller == nullptr || controller->roomType == RKNet::ROOMTYPE_NONE);
    return sameCharacterOffline;
}

// CtrlMenuCharacterSelect::LoadButton+0x5C, "cmpwi r28, 0" (isMii), reached only with 2+ players: offline, CR0 comes from r29
// (the player count) != 1, so the bne after takes the Mii path for every button. Volatiles are dead here; LoadButton saved LR.
static asmFunc LoadAsMultiButtonIfOffline() {
    ASM(
        nofralloc;
        stwu r1, -0x10(r1);
        mflr r0;
        stw r0, 0x14(r1);
        bl UpdateSameCharacterOffline;
        lwz r0, 0x14(r1);
        mtlr r0;
        addi r1, r1, 0x10;
        cmpwi r3, 0;
        beq online;
        cmpwi r29, 1;
        blr;
    online:
        cmpwi r28, 0;
        blr;)
}
kmCall(0x807e2984, LoadAsMultiButtonIfOffline);

// LoadButton+0x98, "addi r5, r30, 0x80", the Mii path's "CharacterSelect%d_%d_Mii"; offline, the _Multi controls of every column.
// r29 is the player count (0x807E2954); nothing reads CR0 before the snprintf.
static asmFunc LoadMultiButtonCtrName() {
    ASM(
        nofralloc;
        lis r5, sameCharacterOffline @ha;
        lbz r5, sameCharacterOffline @l(r5);
        cmpwi r5, 0;
        beq online;
        cmpwi r29, 2;
        lis r5, multiButtonCtrName2 @ha;
        addi r5, r5, multiButtonCtrName2 @l;
        beqlr;
        lis r5, multiButtonCtrName4 @ha;
        addi r5, r5, multiButtonCtrName4 @l;
        blr;
    online:
        addi r5, r30, 0x80;
        blr;)
}
kmCall(0x807e29c0, LoadMultiButtonCtrName);

// LoadButton+0x110, "stb r28, 0x254(r4)": the control picks per-player OK markers by this "is the Mii button" byte, so offline
// multiplayer sets it on every button. CR0 is not read before LoadButton's next compare; r12 is free.
static asmFunc StoreIsMultiButton() {
    ASM(
        nofralloc;
        stb r28, 0x254(r4);
        cmpwi r29, 1;
        beqlr;
        lis r12, sameCharacterOffline @ha;
        lbz r12, sameCharacterOffline @l(r12);
        cmpwi r12, 0;
        beqlr;
        li r0, 1;
        stb r0, 0x254(r4);
        blr;)
}
kmCall(0x807e2a38, StoreIsMultiButton);

// Whether a player has confirmed a driver: OnButtonDriverClick turns that player's holder off
// (ControlsManipulatorManager::InitHolders 0x805F2100 writes the byte at +0xA4 + player * 0x5C).
static u32 HasConfirmedDriver(u32 player) {
    Pages::CharacterSelect *page = SectionMgr::sInstance->curSection->Get<Pages::CharacterSelect>();
    if (page == nullptr)
        return 1;
    const u8 *manager = reinterpret_cast<const u8 *>(&page->GetManipulatorManager());
    return manager[(player & 0xff) * 0x5c + 0xa4] == 0;
}

// OnUpdate+0x70, "cmpwi r0, 1" (r0 = player r27's model state, 1 once confirmed): if RR's own model fails to load, two players share
// the stock one, so offline (r28 players > 1) the player's holder must be off too. The beq reads CR0; volatiles reload after; LR saved.
static asmFunc OwnConfirmForMarker() {
    ASM(
        nofralloc;
        cmpwi r0, 1;
        bnelr;
        cmpwi r28, 1;
        beqlr;
        lis r12, sameCharacterOffline @ha;
        lbz r12, sameCharacterOffline @l(r12);
        cmpwi r12, 0;
        beq online;
        stwu r1, -0x10(r1);
        mflr r0;
        stw r0, 0x14(r1);
        mr r3, r27;
        bl HasConfirmedDriver;
        lwz r0, 0x14(r1);
        mtlr r0;
        addi r1, r1, 0x10;
        cmpwi r3, 1;
        blr;
    online:
        cmpwi r0, 1;
        blr;)
}
kmCall(0x807e311c, OwnConfirmForMarker);

// OnButtonClick+0xA8 and OnUpdate+0x4AC set the OK message (0x9FB + hud, in that player's colour) on the whole button, recolouring
// every marker on a shared one; offline only the confirming player's text is set, as OnMultiplayerMiiSelected does for the Mii.
static void SetOwnOkMessage(LayoutUIControl *button, u32 bmgId, const Text::Info *info) {
    const u32 hud = bmgId - 0x9fb;
    if (hud < 4 && reinterpret_cast<const u8 *>(button)[0x254] != 0 && UpdateSameCharacterOffline()) {
        char textBox[0x10];
        snprintf(textBox, sizeof(textBox), "ok_text_%dp", hud + 1);
        button->SetTextBoxMessage(textBox, bmgId, info);
        return;
    }
    button->SetMessage(bmgId, info);
}
kmCall(0x807e36a4, SetOwnOkMessage);
kmCall(0x807e3558, SetOwnOkMessage);

}  // namespace UI
}  // namespace Pulsar
