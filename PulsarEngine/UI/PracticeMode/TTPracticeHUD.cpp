#include <kamek.hpp>
#include <Gamemodes/PracticeMode/TTPractice.hpp>
#include <MarioKartWii/Item/ItemManager.hpp>
#include <MarioKartWii/Item/ItemPlayer.hpp>
#include <MarioKartWii/Race/RaceData.hpp>
#include <MarioKartWii/UI/Ctrl/CtrlRace/CtrlRaceItemWindow.hpp>
#include <core/rvl/gx/GX.hpp>

namespace Pulsar {
namespace TTPractice {

static const u16 GOLDEN_MUSHROOM_TIMER_FRAMES = 480;
static const u16 GOLDEN_MUSHROOM_WARNING_FRAMES = 120;

// X/Y offsets are relative to the item window's HUD position.
static const float GOLDEN_TIMER_BAR_EDGE_EXTENSION = 4.0f;
static const float GOLDEN_TIMER_BAR_HEIGHT = 3.0f;
static const float GOLDEN_TIMER_BAR_Y_OFFSET = -20.5f;
static const u32 GOLDEN_TIMER_BAR_POSITION_INDEX = 1;
static const u8 GOLDEN_TIMER_BAR_YELLOW_GREEN = 220;
static const u8 GOLDEN_TIMER_BAR_ALPHA = 220;

struct GoldenTimerBar {
    float x;
    float y;
    float width;
    float height;
    u8 red;
    u8 green;
    u8 blue;
    u8 alpha;
};

static bool IsPracticeTimeTrial() {
    const Racedata* racedata = Racedata::sInstance;
    return IsPracticeMode() && racedata != nullptr && racedata->racesScenario.settings.gamemode == MODE_TIME_TRIAL;
}

static void SetupGoldenTimerGX() {
    GX::ClearVtxDesc();
    GX::SetVtxDesc(GX::GX_VA_POS, GX::GX_DIRECT);
    GX::SetVtxDesc(GX::GX_VA_CLR0, GX::GX_DIRECT);
    GX::SetVtxAttrFmt(GX::GX_VTXFMT0, GX::GX_VA_POS, GX::GX_POS_XYZ, GX::GX_F32, 0);
    GX::SetVtxAttrFmt(GX::GX_VTXFMT0, GX::GX_VA_CLR0, GX::GX_CLR_RGBA, GX::GX_RGBA8, 0);
    GX::SetNumTexGens(0);
    GX::SetNumChans(1);
    GX::SetChanCtrl(GX::GX_COLOR0A0, false, GX::GX_SRC_REG, GX::GX_SRC_VTX, GX::GX_LIGHT_NULL, GX::GX_DF_NONE, GX::GX_AF_NONE);
    GX::SetTevOrder(GX::GX_TEVSTAGE0, GX::GX_TEXCOORD_NULL, GX::GX_TEXMAP_NULL, GX::GX_COLOR0A0);
    GX::SetTevOp(GX::GX_TEVSTAGE0, GX::GX_PASSCLR);
    GX::SetNumTevStages(1);
    GX::SetBlendMode(GX::GX_BM_BLEND, GX::GX_BL_SRCALPHA, GX::GX_BL_INVSRCALPHA, GX::GX_LO_CLEAR);
    GX::SetZMode(false, GX::GX_ALWAYS, false);

    Mtx identity = {
        {1.0f, 0.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 1.0f, 0.0f},
    };
    GX::LoadPosMtxImm(identity, 0);
    GX::SetCurrentMtx(0);
}

static void DrawGoldenTimerQuad(const GoldenTimerBar& bar) {
    SetupGoldenTimerGX();

    GX::Begin(GX::GX_QUADS, GX::GX_VTXFMT0, 4);
    GX_Position3f32(bar.x, bar.y, 0.0f);
    GX_Color4u8(bar.red, bar.green, bar.blue, bar.alpha);
    GX_Position3f32(bar.x + bar.width, bar.y, 0.0f);
    GX_Color4u8(bar.red, bar.green, bar.blue, bar.alpha);
    GX_Position3f32(bar.x + bar.width, bar.y - bar.height, 0.0f);
    GX_Color4u8(bar.red, bar.green, bar.blue, bar.alpha);
    GX_Position3f32(bar.x, bar.y - bar.height, 0.0f);
    GX_Color4u8(bar.red, bar.green, bar.blue, bar.alpha);
    GXEnd();
}

static bool TryBuildGoldenTimerBar(CtrlRaceItemWindow& itemWindow, GoldenTimerBar& bar) {
    if (!IsPracticeTimeTrial()) return false;

    Item::Manager* manager = Item::Manager::sInstance;
    if (manager == nullptr) return false;

    const u8 playerId = itemWindow.GetPlayerId();
    if (playerId >= 12) return false;

    Item::Player& player = manager->players[playerId];
    const Item::PlayerInventory& inventory = player.inventory;
    if (inventory.currentItemId != GOLDEN_MUSHROOM || !inventory.hasGolden || inventory.goldenTimer == 0) return false;

    const float remaining = inventory.goldenTimer > GOLDEN_MUSHROOM_TIMER_FRAMES
                                ? 1.0f
                                : static_cast<float>(inventory.goldenTimer) / static_cast<float>(GOLDEN_MUSHROOM_TIMER_FRAMES);

    nw4r::lyt::Pane* itemWindowPane = itemWindow.GetPane();
    if (itemWindowPane == nullptr) return false;

    const PositionAndScale& itemWindowPosition = itemWindow.positionAndscale[GOLDEN_TIMER_BAR_POSITION_INDEX];
    const float barScaleX = itemWindowPosition.scale.x;
    const float barScaleY = itemWindowPosition.scale.z;
    const float fullWidth = (itemWindowPane->size.x + GOLDEN_TIMER_BAR_EDGE_EXTENSION * 2.0f) * barScaleX;
    if (fullWidth <= 0.0f) return false;

    bar.x = itemWindowPosition.position.x - fullWidth * 0.5f;
    bar.y = itemWindowPosition.position.y + GOLDEN_TIMER_BAR_Y_OFFSET * barScaleY;
    bar.width = fullWidth * remaining;
    bar.height = GOLDEN_TIMER_BAR_HEIGHT * barScaleY;
    bar.red = 255;
    bar.green = inventory.goldenTimer <= GOLDEN_MUSHROOM_WARNING_FRAMES ? 0 : GOLDEN_TIMER_BAR_YELLOW_GREEN;
    bar.blue = 0;
    bar.alpha = GOLDEN_TIMER_BAR_ALPHA;
    return true;
}

static void DrawGoldenMushroomTimer(CtrlRaceItemWindow& itemWindow) {
    GoldenTimerBar bar;
    if (TryBuildGoldenTimerBar(itemWindow, bar)) DrawGoldenTimerQuad(bar);
}

static void DrawItemWindowWithGoldenTimer(CtrlRaceItemWindow& itemWindow, u32 curZIdx) {
    if (!itemWindow.IsInactive()) DrawGoldenMushroomTimer(itemWindow);
    itemWindow.LayoutUIControl::Draw(curZIdx);
}
kmWritePointer(0x808d3cdc, DrawItemWindowWithGoldenTimer);

}  // namespace TTPractice
}  // namespace Pulsar
