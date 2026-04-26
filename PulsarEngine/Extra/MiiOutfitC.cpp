#include <RetroRewind.hpp>
#include <kamek.hpp>
#include <runtimeWrite.hpp>
#include <MarioKartWii/UI/Ctrl/Menu/CtrlMenuCharacterSelect.hpp>

namespace Pulsar {

    //Mii Outfit C [TheLordScruffy]
    //This code enables an extra character button already assigned to Mii Outfit C and allows it to be selected, revealing a perfectly functional Mii Outfit C once you add the models.
    //The content names, already completely functional but missing the files, are like the other Mii Outfits. Like mc_mii_m for a medium sized male Mii.

asmFunc MiiOutfitC1() {
    ASM(
        nofralloc;
        subi r29, r29, 0x1C;
        cmplwi r29, 0xD;
        bgt- end;

        li r4, 0x30C3;
        srw r29, r4, r29;
        andi. r4, r29, 1;
        beq- end;

        li r26, 4;
        end:;
        cmplwi r0, 1;
        blr;)
}
kmCall(0x8083E018, MiiOutfitC1);

asmFunc MiiOutfitC2() {
    ASM(
        nofralloc;
        subi r31, r31, 0x1C;
        cmplwi r31, 0xD;
        bgt- end;

        li r4, 0x30C3;
        srw r31, r4, r31;
        andi. r4, r31, 1;
        beq- end;

        li r29, 4;
        end:;
        cmplwi r0, 1;
        blr;)
  }
kmCall(0x8083E64C, MiiOutfitC2);

kmWrite32(0x805500B8, 0x38600001);
kmWrite32(0x807E2714, 0x3BC0001B);

//Mii Outfit C Stats Fix [Atlas, gaberboo]
kmWrite16(0x807E7E2A, 0x0004);
kmWrite16(0x807E7E42, 0x0004);
kmWrite16(0x807E7E4E, 0x0004);


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
        case MII_S_C_MALE:   charaId = MII_S_A_MALE;   break;
        case MII_S_C_FEMALE: charaId = MII_S_A_FEMALE; break;
        case MII_M_C_MALE:   charaId = MII_M_A_MALE;   break;
        case MII_M_C_FEMALE: charaId = MII_M_A_FEMALE; break;
        case MII_L_C_MALE:   charaId = MII_L_A_MALE;   break;
        case MII_L_C_FEMALE: charaId = MII_L_A_FEMALE; break;
        default: break;
    }
    return original(charaId, useGenericMiiName);
}
// Hook all character-select related call sites for the name label
kmCall(0x8083e6b8, GetCharaNameMsgHook);  // OnButtonDriverSelect (Mii with name)
kmCall(0x8083e6e0, GetCharaNameMsgHook);  // OnButtonDriverSelect (Mii generic name)
kmCall(0x8083e058, GetCharaNameMsgHook);  // OnButtonDriverClick confirm label
kmCall(0x8083ea60, GetCharaNameMsgHook);  // related character name display
}