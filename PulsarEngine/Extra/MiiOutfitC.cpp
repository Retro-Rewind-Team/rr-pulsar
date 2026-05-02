#include <RetroRewind.hpp>
#include <kamek.hpp>
#include <runtimeWrite.hpp>
#include <MarioKartWii/Kart/KartFunctions.hpp>
#include <MarioKartWii/UI/Ctrl/Menu/CtrlMenuCharacterSelect.hpp>

namespace Pulsar {

// Mii Outfit C [TheLordScruffy]
// This code enables an extra character button already assigned to Mii Outfit C and allows it to be selected, revealing a perfectly functional Mii Outfit C once you add the models.
// The content names, already completely functional but missing the files, are like the other Mii Outfits. Like mc_mii_m for a medium sized male Mii.

asmFunc MiiOutfitC1() {
    ASM(
        nofralloc;
        subi r29, r29, 0x1C;
        cmplwi r29, 0xD;
        bgt - end;

        li r4, 0x30C3;
        srw r29, r4, r29;
        andi.r4, r29, 1;
        beq - end;

        li r26, 4;
        end :;
        cmplwi r0, 1;
        blr;)
}
kmCall(0x8083E018, MiiOutfitC1);

asmFunc MiiOutfitC2() {
    ASM(
        nofralloc;
        subi r31, r31, 0x1C;
        cmplwi r31, 0xD;
        bgt - end;

        li r4, 0x30C3;
        srw r31, r4, r31;
        andi.r4, r31, 1;
        beq - end;

        li r29, 4;
        end :;
        cmplwi r0, 1;
        blr;)
}
kmCall(0x8083E64C, MiiOutfitC2);

kmWrite32(0x805500B8, 0x38600001);
kmWrite32(0x807E2714, 0x3BC0001B);

// Mii Outfit C Stats Fix [Atlas, gaberboo]
kmWrite16(0x807E7E2A, 0x0004);
kmWrite16(0x807E7E42, 0x0004);
kmWrite16(0x807E7E4E, 0x0004);

static CharacterId GetMiiOutfitAKartDriverDispCharacter(CharacterId character) {
    switch (character) {
        case MII_S_C_MALE:
            return MII_S_A_MALE;
        case MII_S_C_FEMALE:
            return MII_S_A_FEMALE;
        case MII_M_C_MALE:
            return MII_M_A_MALE;
        case MII_M_C_FEMALE:
            return MII_M_A_FEMALE;
        case MII_L_C_MALE:
            return MII_L_A_MALE;
        case MII_L_C_FEMALE:
            return MII_L_A_FEMALE;
        default:
            return character;
    }
}

// kartDriverDispParam.bin has no dedicated Outfit C columns in the stock common bins.
// Share the matching Outfit A columns so C inherits the same driver/kart display offsets.
static KartDriverDispParam::Entry* GetKartDriverDispEntryHook(KartId kart, CharacterId character) {
    if (Kart::paramBins.kartDriverDisp == nullptr) return nullptr;

    const CharacterId dispCharacter = GetMiiOutfitAKartDriverDispCharacter(character);
    return Kart::paramBins.kartDriverDispEntries + static_cast<u32>(kart) * 0x30 + dispCharacter;
}
kmBranch(0x80592498, GetKartDriverDispEntryHook);

kmRuntimeUse(0x807e3cac);
static int GetCharacterIdForButtonHook(CtrlMenuCharacterSelect* ctrl, u32 weightClass, u32 buttonIdx) {
    typedef int (*GetCharacterIdForButtonFn)(CtrlMenuCharacterSelect*, u32, u32);
    const GetCharacterIdForButtonFn original = reinterpret_cast<GetCharacterIdForButtonFn>(kmRuntimeAddr(0x807e3cac));

    const int character = original(ctrl, weightClass, buttonIdx);
    if (weightClass == 3 && ctrl != nullptr) {
        const u32 categoryCount = ctrl->categoryCount;
        const u32 categorySize = categoryCount * 2;
        if (categoryCount != 0 && categorySize != 0) {
            const u32 localIdx = buttonIdx % categorySize;
            const u32 localColumn = localIdx % categoryCount;
            if (localColumn == 2) {
                switch (static_cast<CharacterId>(character)) {
                    case MII_S_A_MALE:
                    case MII_S_B_MALE:
                        return MII_S_C_MALE;
                    case MII_S_A_FEMALE:
                    case MII_S_B_FEMALE:
                        return MII_S_C_FEMALE;
                    case MII_M_A_MALE:
                    case MII_M_B_MALE:
                        return MII_M_C_MALE;
                    case MII_M_A_FEMALE:
                    case MII_M_B_FEMALE:
                        return MII_M_C_FEMALE;
                    case MII_L_A_MALE:
                    case MII_L_B_MALE:
                        return MII_L_C_MALE;
                    case MII_L_A_FEMALE:
                    case MII_L_B_FEMALE:
                        return MII_L_C_FEMALE;
                    default:
                        return character;
                }
            }
        }
    }
    return character;
}
kmCall(0x807e2a20, GetCharacterIdForButtonHook);

// Make Mii Outfit C use the same name BMG as Mii Outfit A.
// getCharaNameMsg has no case for C IDs and returns -1 for them.
// We remap C -> corresponding A before the call so the name displays correctly.
kmRuntimeUse(0x80833774);
static int GetCharaNameMsgHook(CharacterId charaId, int useGenericMiiName) {
    typedef int (*GetCharaNameMsgFn)(CharacterId, int);
    const GetCharaNameMsgFn original = reinterpret_cast<GetCharaNameMsgFn>(kmRuntimeAddr(0x80833774));

    switch (charaId) {
        case MII_S_C_MALE:
            charaId = MII_S_A_MALE;
            break;
        case MII_S_C_FEMALE:
            charaId = MII_S_A_FEMALE;
            break;
        case MII_M_C_MALE:
            charaId = MII_M_A_MALE;
            break;
        case MII_M_C_FEMALE:
            charaId = MII_M_A_FEMALE;
            break;
        case MII_L_C_MALE:
            charaId = MII_L_A_MALE;
            break;
        case MII_L_C_FEMALE:
            charaId = MII_L_A_FEMALE;
            break;
        default:
            break;
    }
    return original(charaId, useGenericMiiName);
}
// Hook all character-select related call sites for the name label
kmCall(0x8083e6b8, GetCharaNameMsgHook);  // OnButtonDriverSelect (Mii with name)
kmCall(0x8083e6e0, GetCharaNameMsgHook);  // OnButtonDriverGetText (Mii generic name)
kmCall(0x8083e058, GetCharaNameMsgHook);  // OnButtonDriverClick confirm label
kmCall(0x8083ea60, GetCharaNameMsgHook);  // related character name display

// Constructor
kmWrite32(0x8083018c, 0x1CA40003);

// Per-player Mii body loader
kmWrite32(0x80831148, 0x1C9A0003);

// Per-player Mii body loader loop
kmWrite32(0x808312e0, 0x2C1C0002);

// SetPlayerCharacter
kmWrite32(0x80830d50, 0x1C840003);  // VS liveview / ghost branch
kmWrite32(0x80830ed4, 0x1C840003);  // Main character-select branch

// Local multiplayer allocates one per-player heap for menu Mii models. Stock size is tuned for
// two Mii body variants (A/B); Outfit C needs extra room during the ctor-time 0x80831100 loads.
kmWrite32(0x8059e3bc, 0x3F800002);  // lis r28, 0x2 -> 0x20000 seed
kmWrite32(0x8059e3cc, 0x7F83E378);  // mr r3, r28 -> 0x20000 bytes

// Main Mii slot selector
asmFunc MiiOutfitCDriverSlot() {
    ASM(
        nofralloc;
        // Default to the Mii A slot.
        addi r8, r4, 0x18;

        // Mii B check: r5 in {0x1A, 0x1B, 0x20, 0x21, 0x26, 0x27}.
        subi r0, r5, 0x1A;
        cmplwi r0, 0xD;
        bgt - checkC;
        li r11, 0x30C3;
        srw r0, r11, r0;
        andi.r11, r0, 1;
        beq - checkC;

        addi r8, r8, 1;
        lwz r11, 0(r31);
        addi r11, r11, 2;
        stw r11, 0(r31);
        b done;

        checkC :;
        // Mii C check: r5 in {0x1C, 0x1D, 0x22, 0x23, 0x28, 0x29}.
        subi r0, r5, 0x1C;
        cmplwi r0, 0xD;
        bgt - done;
        li r11, 0x30C3;
        srw r0, r11, r0;
        andi.r11, r0, 1;
        beq - done;

        addi r8, r8, 2;
        lwz r11, 0(r31);
        addi r11, r11, 4;
        stw r11, 0(r31);

        done :;
        blr;)
}
kmBranch(0x80830ee4, MiiOutfitCDriverSlot);
kmPatchExitPoint(MiiOutfitCDriverSlot, 0x80830f18);

// VS liveview / ghost Mii slot selector
asmFunc MiiOutfitCDriverSlotSpecial() {
    ASM(
        nofralloc;

        // Mii B check.
        subi r0, r5, 0x1A;
        cmplwi r0, 0xD;
        bgt - checkCSp;
        li r11, 0x30C3;
        srw r0, r11, r0;
        andi.r11, r0, 1;
        beq - checkCSp;

        addi r8, r8, 1;
        b doneSp;

        checkCSp :;
        // Mii C check.
        subi r0, r5, 0x1C;
        cmplwi r0, 0xD;
        bgt - doneSp;
        li r11, 0x30C3;
        srw r0, r11, r0;
        andi.r11, r0, 1;
        beq - doneSp;

        addi r8, r8, 2;

        doneSp :;
        blr;)
}
kmBranch(0x80830d7c, MiiOutfitCDriverSlotSpecial);
kmPatchExitPoint(MiiOutfitCDriverSlotSpecial, 0x80830da4);

}  // namespace Pulsar
