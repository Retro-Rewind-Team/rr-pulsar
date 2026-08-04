#include <RetroRewind.hpp>
#include <Settings/Settings.hpp>
#include <MarioKartWii/UI/Ctrl/Manipulator.hpp>

namespace Pulsar {
namespace UI {

static u8 hudR = 255;
static u8 hudG = 255;
static u8 hudB = 255;

static const u8 hudColors[12][3] = {
    {255, 255, 255},  // White
    {60, 60, 60},  // Black
    {232, 46, 46},  // Red
    {245, 129, 47},  // Orange
    {243, 232, 37},  // Yellow
    {169, 255, 69},  // Green
    {64, 99, 227},  // Blue
    {96, 38, 158},  // Purple
    {255, 192, 203},  // Pink
    {255, 0, 255},  // Magenta
    {36, 224, 255},  // Cyan
    {0, 128, 128}  // Teal
};

void UpdateHUDColor() {
    u8 setting = Settings::Mgr::Get().GetUserSettingValue(Settings::SETTINGSTYPE_MENU, SCROLL_HUDCOLOR);
    if (setting >= 12) setting = 0;
    hudR = hudColors[setting][0];
    hudG = hudColors[setting][1];
    hudB = hudColors[setting][2];
}

// Vanilla maps local player slots 0..3 to palette entries 1..4. Keep those
// pairings intact so local multiplayer HUD controls remain distinguishable.
static const RGBA16 localPlayerHUDColors[4][2] = {
    {{255, 255, 0, 255}, {210, 170, 0, 255}},
    {{0, 111, 255, 255}, {19, 229, 255, 255}},
    {{255, 0, 0, 255}, {255, 79, 255, 255}},
    {{0, 186, 0, 255}, {76, 255, 130, 255}},
};

static bool SetLocalPlayerHUDColors(u32 playerId, RGBA16* c0, RGBA16* c1) {
    if (playerId < 1 || playerId > 4) return false;

    *c0 = localPlayerHUDColors[playerId - 1][0];
    *c1 = localPlayerHUDColors[playerId - 1][1];
    return true;
}

void GetHUDColor(const ControlManipulator* self, RGBA16* c0, RGBA16* c1) {
    if (self != nullptr && SetLocalPlayerHUDColors(self->allowedPlayerId, c0, c1)) return;

    UpdateHUDColor();
    c0->red = hudR;
    c0->green = hudG;
    c0->blue = hudB;
    c0->alpha = 0xFD;
    c1->red = hudR;
    c1->green = hudG;
    c1->blue = hudB;
    c1->alpha = 0xFD;
}
kmBranch(0x805f03dc, GetHUDColor);

void GetHUDSlotColor(u8 hudSlotId, RGBA16* c0, RGBA16* c1) {
    if (GetLocalPlayerCount() > 1 && SetLocalPlayerHUDColors(hudSlotId + 1, c0, c1)) return;
    GetHUDColor(nullptr, c0, c1);
}
kmBranch(0x805f0440, GetHUDSlotColor);

void GetHUDBaseColor(void* self, RGBA16* c) {
    UpdateHUDColor();
    c->red = 0;
    c->green = 0;
    c->blue = 0;
    c->alpha = 0x46;
}
kmBranch(0x805f04d8, GetHUDBaseColor);

void GetHUDRaceColor(nw4r::lyt::Pane* _this, u32 idx, nw4r::ut::Color color) {
    if (GetLocalPlayerCount() > 1) {
        _this->SetVtxColor(idx, color);
        return;
    }

    UpdateHUDColor();
    if (idx < 2) {
        color.r = hudR;
        color.g = hudG;
        color.b = hudB;
        color.a = 0xFD;
    } else {
        color.r = hudR > 20 ? hudR - 20 : 0;
        color.g = hudG > 20 ? hudG - 20 : 0;
        color.b = hudB > 20 ? hudB - 20 : 0;
        color.a = 0xFD;
    }
    _this->SetVtxColor(idx, color);
}
kmCall(0x807ec1dc, GetHUDRaceColor);

}  // namespace UI
}  // namespace Pulsar