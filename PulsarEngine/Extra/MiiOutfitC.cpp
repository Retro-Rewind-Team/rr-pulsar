#include <RetroRewind.hpp>
#include <kamek.hpp>

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
}