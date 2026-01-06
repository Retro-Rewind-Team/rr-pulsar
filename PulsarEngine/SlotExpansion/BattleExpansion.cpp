#include <PulsarSystem.hpp>
#include <Settings/UI/ExpWFCMainPage.hpp>
#include <MarioKartWii/UI/Section/SectionMgr.hpp>
#include <MarioKartWii/GlobalFunctions.hpp>
#include <MarioKartWii/UI/Page/Menu/Menu.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <MarioKartWii/RKNet/SELECT.hpp>
#include <MarioKartWii/Race/RaceInfo/GameModeData.hpp>
#include <MarioKartWii/Race/RaceInfo/RaceInfo.hpp>
#include <kamek.hpp>

namespace Pulsar {

void LoadCorrectSectionForBattle() {
    if (!IsBattle) return;
    SectionMgr* sectionMgr = SectionMgr::sInstance;
    const RacedataSettings& racedataSettings = Racedata::sInstance->menusScenario.settings;
    const GameMode mode = racedataSettings.gamemode;
    if (sectionMgr->nextSectionId == SECTION_VS_RACE_INTRO && mode == MODE_BATTLE) {
        sectionMgr->nextSectionId = SECTION_BALLOON_BATTLE_INTRO;
    } else if (sectionMgr->nextSectionId == SECTION_SINGLE_P_BT_NEXT_BATTLE) {
        sectionMgr->nextSectionId = SECTION_SINGLE_P_VS_NEXT_RACE;
    } else if (sectionMgr->nextSectionId == SECTION_VS_RACE_AWARD || sectionMgr->nextSectionId == SECTION_AWARD_37 || sectionMgr->nextSectionId == SECTION_AWARD_38) {
        sectionMgr->nextSectionId = SECTION_MAIN_MENU_FROM_BOOT;
    } else if (sectionMgr->nextSectionId == SECTION_P1_WIFI_FROOM_VS_VOTING && racedataSettings.battleType == BATTLE_BALLOON && mode == MODE_PRIVATE_BATTLE) {
        sectionMgr->nextSectionId = SECTION_P1_WIFI_FROOM_BALLOON_VOTING;
    } else if (sectionMgr->nextSectionId == SECTION_P1_WIFI_FROOM_VS_VOTING && racedataSettings.battleType == BATTLE_COIN && mode == MODE_PRIVATE_BATTLE) {
        sectionMgr->nextSectionId = SECTION_P1_WIFI_FROOM_COIN_VOTING;
    } else if (sectionMgr->nextSectionId == SECTION_P2_WIFI_FROOM_VS_VOTING && racedataSettings.battleType == BATTLE_BALLOON && mode == MODE_PRIVATE_BATTLE) {
        sectionMgr->nextSectionId = SECTION_P2_WIFI_FROOM_BALLOON_VOTING;
    } else if (sectionMgr->nextSectionId == SECTION_P2_WIFI_FROOM_VS_VOTING && racedataSettings.battleType == BATTLE_COIN && mode == MODE_PRIVATE_BATTLE) {
        sectionMgr->nextSectionId = SECTION_P2_WIFI_FROOM_COIN_VOTING;
    }
}
static FrameLoadHook PatchLoadCorrectSectionForBattle(LoadCorrectSectionForBattle);

// Offline Battle Expansion [Ro]
kmWrite32(0x8084e674, 0x48000018);
kmWrite32(0x8084b7b0, 0x48000018);
kmWrite32(0x808409ec, 0x4800001C);

// Online Battle Expansion [ZPL]
kmWrite32(0x8062e11c, 0x3880006E);
kmWrite32(0x8062e128, 0x3880006F);
kmWrite32(0x806501d8, 0x4BFFFE44);
kmWrite32(0x806509b8, 0x3880006E);

// Fix Random Track Selection [Toadette Hack Fan]
kmWrite32(0x80643218, 0x38000000);

// 2P WW Battle
kmWrite32(0x8062E788, 0x3880006E);
kmWrite32(0x8062E794, 0x3880006F);

// 1P Balloon Battle Froom
kmWrite32(0x8062E938, 0x3880006E);
kmWrite32(0x8062E944, 0x3880006F);

// 1P Coin Runners Froom
kmWrite32(0x8062E9C8, 0x3880006E);
kmWrite32(0x8062E9D4, 0x3880006F);

// 2P Balloon Battle Froom
kmWrite32(0x8062EB78, 0x3880006E);
kmWrite32(0x8062EB84, 0x3880006F);

// 2P Coin Runners Froom
kmWrite32(0x8062EC08, 0x3880006E);
kmWrite32(0x8062EC14, 0x3880006F);

// Fix GMDataOnlineCoinBattle::Update() [ZPL]
asmFunc CoinBattle_EarlyOutIfInvalid() {
    ASM(
        lis r12, 0x8000;
        cmplw r6, r12;
        bge valid;
        lwz r0, 0x24(r1);
        mtlr r0;
        lwz r31, 0x1C(r1);
        lwz r30, 0x18(r1);
        lwz r29, 0x14(r1);
        lwz r28, 0x10(r1);
        addi r1, r1, 0x20;
        blr;
        valid : lwz r5, 0x20B8(r6);
        blr;)
}
kmCall(0x8053D47C, CoinBattle_EarlyOutIfInvalid);

}  // namespace Pulsar