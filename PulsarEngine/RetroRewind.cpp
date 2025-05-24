#include <MarioKartWii/Race/RaceData.hpp>
#include <SlotExpansion/CupsConfig.hpp>
#include <Settings/UI/SettingsPanel.hpp>
#include <Settings/Settings.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <Dolphin/DolphinIOS.hpp>

namespace RetroRewind {
Pulsar::System *System::Create() {
    return new System(); //now Pulsar::sInstance is of type RetroRewind
}
Pulsar::System::Inherit CreateRetroRewind(System::Create);

bool System::Is500cc() {
    return Racedata::sInstance->racesScenario.settings.engineClass == CC_50;
}

System::WeightClass System::GetWeightClass(const CharacterId id){
    switch (id)
    {
        case BABY_MARIO:
        case BABY_LUIGI:
        case BABY_PEACH:
        case BABY_DAISY:
        case TOAD:
        case TOADETTE:
        case KOOPA_TROOPA:
        case DRY_BONES:
            return LIGHTWEIGHT;
        case MARIO:
        case LUIGI:
        case PEACH:
        case DAISY:
        case YOSHI:
        case BIRDO:
        case DIDDY_KONG:
        case BOWSER_JR:
            return MEDIUMWEIGHT;
        case WARIO:
        case WALUIGI:
        case DONKEY_KONG:
        case BOWSER:
        case KING_BOO:
        case ROSALINA:
        case FUNKY_KONG:
        case DRY_BOWSER:
            return HEAVYWEIGHT;
        default:
            return MIIS;
    }
}

//Force 30 FPS [Vabold]
kmWrite32(0x80554224, 0x3C808000);
kmWrite32(0x80554228, 0x88841204);
kmWrite32(0x8055422C, 0x48000044);

void FPSPatch() {
  FPSPatchHook = 0x00;
  bool isDolphin = Dolphin::IsEmulator();
  const RacedataScenario& scenario = Racedata::sInstance->racesScenario;
  u32 localPlayerCount = scenario.localPlayerCount;
  if (static_cast<Pulsar::FPS>(Pulsar::Settings::Mgr::Get().GetUserSettingValue(static_cast<Pulsar::Settings::UserType>(Pulsar::Settings::SETTINGSTYPE_RR), Pulsar::SETTIGNRR_RADIO_FPS)) == Pulsar::FPS_HALF || localPlayerCount > 1 && !isDolphin) {
      FPSPatchHook = 0x00FF0100;
  }
}
static PageLoadHook PatchFPS(FPSPatch);

//Item Box Respawn Modifier [Unnamed]
asmFunc GetItemBoxRespawn() {
    ASM(
loc_0x0:
  stw       r0, 0xB0(r27);
  lis       r12, 0x8000;
  lbz       r12, 0x1206(r12);
  cmpwi     r12, 0;
  beq       end;
  li        r12, 0x5A;
  stw       r12, 0xB8(r27);
  stw       r0, 0xB0(r27);

end:
  blr;
    )
}
kmCall(0x80828EDC, GetItemBoxRespawn);

void ItemBoxPatch() {
  bool is200 = Racedata::sInstance->racesScenario.settings.engineClass == CC_100 && RKNet::Controller::sInstance->roomType != RKNet::ROOMTYPE_VS_WW;
  bool isFastRespawn = Pulsar::System::sInstance->IsContext(Pulsar::PULSAR_ITEMBOXRESPAWN) ? Pulsar::ITEMBOX_DEFAULTRESPAWN : Pulsar::ITEMBOX_FASTRESPAWN;
  ItemBoxHook = 0x00;
  if (isFastRespawn == Pulsar::ITEMBOX_DEFAULTRESPAWN || is200) {
    ItemBoxHook = 0x00FF0100;
  }
}
static PageLoadHook PatchItemBox(ItemBoxPatch);

asmFunc GetMaxWeight() {
    ASM(
loc_0x0:
  lfs	      f1, 0x0014 (r28);
  lis       r12, 0x8000;
  lbz       r12, 0x120C(r12);
  cmpwi     r12, 0;
  beq       end;
  lfs       f1, 0x0(r28);

end:
  blr;
    )
}
kmCall(0x8057083C, GetMaxWeight);

void MaxWeightPatch() {
  bool is200 = Racedata::sInstance->racesScenario.settings.engineClass == CC_100 && RKNet::Controller::sInstance->roomType != RKNet::ROOMTYPE_VS_WW;
  MaxWeightHook = 0x00;
  if (is200) {
    MaxWeightHook = 0x00FF0100;
  }
}
static PageLoadHook PatchMaxWeight(MaxWeightPatch);

//No Disconnect from being Idle [Bully]
asmFunc GetNoDC() {
    ASM(
loc_0x0:
  addi      r0, r3, 1;
  lis       r12, 0x8000;
  lbz       r12, 0x1208(r12);
  cmpwi     r12, 0;
  beq       end;
  li        r0, 0;

end:
  blr;
    )
}
kmCall(0x80521408, GetNoDC);
kmCall(0x8053EF6C, GetNoDC);
kmCall(0x8053F0B4, GetNoDC);
kmCall(0x8053F124, GetNoDC);

void NoDCPatch() {
  DCHook = 0x00;
  if (RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_FROOM_NONHOST || RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_FROOM_HOST) {
    DCHook = 0x00FF0100;
  }
}
static PageLoadHook PatchNoDC(NoDCPatch);

extern "C" void ItemVanish(unsigned int r0, unsigned int r12) {
  if (Pulsar::System::sInstance->IsContext(Pulsar::PULSAR_MODE_OTT)) {
    if(r12 == 0) return;
    volatile unsigned int cmp = (r0 == 10);
    (void)cmp;
    return;
  }
}
kmCall(0x8079F748, ItemVanish);

} // namespace RetroRewind