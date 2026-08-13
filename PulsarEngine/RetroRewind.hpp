#include <kamek.hpp>
#include <PulsarSystem.hpp>
#include <Settings/Settings.hpp>
#include <MarioKartWii/GlobalFunctions.hpp>
#include <MarioKartWii/System/Identifiers.hpp>

extern u32 FPSPatchHook;
extern u32 PredictionHook;
extern u32 ItemRainOnlineFixHook;
extern u32 BloomHook;
extern u8 FontRename;
extern u8 RaceRename;
extern u8 CommonRename;
extern u8 AwardRename;

extern u16 CUSTOM_MARIO;
extern u16 CUSTOM_BABY_PEACH;
extern u16 CUSTOM_WALUIGI;
extern u16 CUSTOM_BOWSER;
extern u16 CUSTOM_BABY_DAISY;
extern u16 CUSTOM_DRY_BONES;
extern u16 CUSTOM_BABY_MARIO;
extern u16 CUSTOM_LUIGI;
extern u16 CUSTOM_TOAD;
extern u16 CUSTOM_DONKEY_KONG;
extern u16 CUSTOM_YOSHI;
extern u16 CUSTOM_WARIO;
extern u16 CUSTOM_BABY_LUIGI;
extern u16 CUSTOM_TOADETTE;
extern u16 CUSTOM_KOOPA_TROOPA;
extern u16 CUSTOM_DAISY;
extern u16 CUSTOM_PEACH;
extern u16 CUSTOM_BIRDO;
extern u16 CUSTOM_DIDDY_KONG;
extern u16 CUSTOM_KING_BOO;
extern u16 CUSTOM_BOWSER_JR;
extern u16 CUSTOM_DRY_BOWSER;
extern u16 CUSTOM_FUNKY_KONG;
extern u16 CUSTOM_ROSALINA;
extern u16 CUSTOM_PEACH_MENU;
extern u16 CUSTOM_DAISY_MENU;
extern u16 CUSTOM_ROSALINA_MENU;
extern u8 CUSTOM_DRIVER;

namespace RetroRewind {
class System : public Pulsar::System {
   public:
    static bool Is500cc();

    enum WeightClass {
        LIGHTWEIGHT,
        MEDIUMWEIGHT,
        HEAVYWEIGHT,
        MIIS,
        ALLWEIGHT
    };

    enum CharButtonId {
        BUTTON_BABY_MARIO,
        BUTTON_BABY_LUIGI,
        BUTTON_TOAD,
        BUTTON_TOADETTE,
        BUTTON_BABY_PEACH,
        BUTTON_BABY_DAISY,
        BUTTON_KOOPA_TROOPA,
        BUTTON_DRY_BONES,
        BUTTON_MARIO,
        BUTTON_LUIGI,
        BUTTON_YOSHI,
        BUTTON_BIRDO,
        BUTTON_PEACH,
        BUTTON_DAISY,
        BUTTON_DIDDY_KONG,
        BUTTON_BOWSER_JR,
        BUTTON_WARIO,
        BUTTON_WALUIGI,
        BUTTON_KING_BOO,
        BUTTON_ROSALINA,
        BUTTON_DONKEY_KONG,
        BUTTON_FUNKY_KONG,
        BUTTON_BOWSER,
        BUTTON_DRY_BOWSER,
        BUTTON_MII_A,
        BUTTON_MII_B
    };

    WeightClass weight;
    static Pulsar::System *Create();  // My Create function, needs to return Pulsar
    static WeightClass GetWeightClass(CharacterId);
};
}  // namespace RetroRewind