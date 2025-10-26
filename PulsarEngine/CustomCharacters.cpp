#include <RetroRewind.hpp>
#include <Settings/Settings.hpp>
#include <MarioKartWii/System/Identifiers.hpp>
#include <MarioKartWii/GlobalFunctions.hpp>

namespace Pulsar {

void SetCharacter() {
    u32 character = static_cast<Pulsar::Transmission>(Pulsar::Settings::Mgr::Get().GetUserSettingValue(static_cast<Pulsar::Settings::UserType>(Pulsar::Settings::SETTINGSTYPE_MISC), Pulsar::SCROLLER_CUSTOMCHARACTER));
    CUSTOM_DRIVER = 'D';
    CUSTOM_BABY_MARIO = 'bm';
    CUSTOM_TOAD = 'ko';
    CUSTOM_MARIO = 'mr';
    CUSTOM_YOSHI = 'ys';
    CUSTOM_WARIO = 'wr';
    CUSTOM_KING_BOO = 'kt';
    CUSTOM_BABY_LUIGI = 'bl';
    CUSTOM_TOADETTE = 'kk';
    CUSTOM_LUIGI = 'lg';
    CUSTOM_BIRDO = 'ca';
    CUSTOM_WALUIGI = 'wl';
    CUSTOM_ROSALINA = 'rs';
    CUSTOM_ROSALINA_MENU = 'rs';
    CUSTOM_BABY_PEACH = 'bp';
    CUSTOM_KOOPA_TROOPA = 'nk';
    CUSTOM_PEACH = 'pc';
    CUSTOM_PEACH_MENU = 'pc';
    CUSTOM_DIDDY_KONG = 'dd';
    CUSTOM_DONKEY_KONG = 'dk';
    CUSTOM_FUNKY_KONG = 'fk';
    CUSTOM_BABY_DAISY = 'bd';
    CUSTOM_DRY_BONES = 'ka';
    CUSTOM_DAISY = 'ds';
    CUSTOM_DAISY_MENU = 'ds';
    CUSTOM_BOWSER_JR = 'jr';
    CUSTOM_BOWSER = 'kp';
    CUSTOM_DRY_BOWSER = 'bk';
    if (character == Pulsar::CUSTOMCHARACTER_ENABLED && GetLocalPlayerCount() == 1) {
        CUSTOM_DRIVER = 'R';
        CUSTOM_BABY_MARIO = 'km';
        CUSTOM_TOAD = 'ct';
        CUSTOM_MARIO = 'sm';
        CUSTOM_YOSHI = 'ky';
        CUSTOM_WARIO = 'hw';
        CUSTOM_KING_BOO = 'kb';
        CUSTOM_BABY_LUIGI = 'cl';
        CUSTOM_TOADETTE = 'et';
        CUSTOM_LUIGI = 'cl';
        CUSTOM_BIRDO = 'rb';
        CUSTOM_WALUIGI = 'vw';
        CUSTOM_ROSALINA = 'ar';
        CUSTOM_ROSALINA_MENU = 'ar';
        CUSTOM_BABY_PEACH = 'cp';
        CUSTOM_KOOPA_TROOPA = 'pk';
        CUSTOM_PEACH = 'ap';
        CUSTOM_PEACH_MENU = 'ap';
        CUSTOM_DIDDY_KONG = 'ad';
        CUSTOM_DONKEY_KONG = 'gd';
        CUSTOM_FUNKY_KONG = 'ck';
        CUSTOM_BABY_DAISY = 'rd';
        CUSTOM_DRY_BONES = 'bb';
        CUSTOM_DAISY = 'sd';
        CUSTOM_DAISY_MENU = 'sd';
        CUSTOM_BOWSER_JR = 'pj';
        CUSTOM_BOWSER = 'db';
        CUSTOM_DRY_BOWSER = 'gk';
    }
}
static PageLoadHook SetCharacterHook(SetCharacter);

}  // namespace Pulsar
