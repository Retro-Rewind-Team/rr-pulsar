#include <RetroRewind.hpp>
#include <Settings/Settings.hpp>

namespace Pulsar {
namespace UI {

static u8 hudR = 255;
static u8 hudG = 255;
static u8 hudB = 255;

static const u8 hudColors[12][3] = {
    {255, 255, 255},  // White
    {60, 60, 60},  // Black
    {255, 0, 0},  // Red
    {255, 165, 0},  // Orange
    {255, 255, 0},  // Yellow
    {0, 255, 0},  // Green
    {0, 0, 255},  // Blue
    {128, 0, 128},  // Purple
    {255, 192, 203},  // Pink
    {255, 0, 255},  // Magenta
    {0, 255, 255},  // Cyan
    {0, 128, 128}  // Teal
};

void UpdateHUDColor() {
    u8 setting = Settings::Mgr::Get().GetUserSettingValue(Settings::SETTINGSTYPE_MENU, SCROLL_HUDCOLOR);
    if (setting >= 12) setting = 0;
    hudR = hudColors[setting][0];
    hudG = hudColors[setting][1];
    hudB = hudColors[setting][2];
}

void GetHUDColor(void* self, RGBA16* c0, RGBA16* c1) {
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
kmBranch(0x805f0440, GetHUDColor);

void GetHUDBaseColor(void* self, RGBA16* c) {
    UpdateHUDColor();
    c->red = hudR;
    c->green = hudG;
    c->blue = hudB;
    c->alpha = 0x0A;
}
kmBranch(0x805f04d8, GetHUDBaseColor);

void GetHUDRaceColor(nw4r::lyt::Pane* _this, u32 idx, nw4r::ut::Color color) {
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