/**
 * Direct port from mkw-sp Checkpoint Display
 *
 * Licensed under MIT. (See LICENSE_mkw-sp)
 *
 * Copyright 2021-2023 Pablo Stebler

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 *
 * https://github.com/mkw-sp/mkw-sp/blob/main/payload/sp/3d/Checkpoints.cc
 */

#include <kamek.hpp>
#include <runtimeWrite.hpp>
#include <Gamemodes/PracticeMode/TTPractice.hpp>
#include <Gamemodes/PracticeMode/TTPracticeCheckpoints.hpp>
#include <Settings/Settings.hpp>
#include <MarioKartWii/3D/Camera/Camera.hpp>
#include <MarioKartWii/3D/Scn/Renderer.hpp>
#include <MarioKartWii/3D/Scn/ScnMgr.hpp>
#include <MarioKartWii/KMP/KMPManager.hpp>
#include <core/rvl/gx/GX.hpp>

namespace Pulsar {
namespace TTPractice {
namespace Checkpoints {

static const float CHECKPOINT_TOP_OFFSET = 5000.0f;
static const float CHECKPOINT_BOTTOM_OFFSET = -5000.0f;

kmRuntimeUse(0x805625a8);  // ScnMgr::DrawModelsImpl
kmRuntimeUse(0x809c1848);  // Renderer::cur

struct CheckpointVertex {
    Vec3 pos;
    bool translucent;
};

typedef void (*ScnMgrDrawModelsImplFn)(ScnMgr* mgr, GameScreen* screen);

static ScnMgrDrawModelsImplFn GetScnMgrDrawModelsImpl() {
    static const ScnMgrDrawModelsImplFn function = reinterpret_cast<ScnMgrDrawModelsImplFn>(kmRuntimeAddr(0x805625a8));
    return function;
}

DisplayMode GetDisplayMode() {
    if (!Settings::Mgr::IsCreated()) return DISPLAY_DISABLED;

    const u8 setting =
        Settings::Mgr::Get().GetUserSettingValue(Settings::SETTINGSTYPE_TTPRACTICE, RADIO_TTPRACTICE_CHECKPOINTDISPLAY);
    if (setting == TTPRACTICE_CHECKPOINTDISPLAY_KEY_ONLY) return DISPLAY_KEY_ONLY;
    if (setting == TTPRACTICE_CHECKPOINTDISPLAY_ALL) return DISPLAY_ALL;
    return DISPLAY_DISABLED;
}

static KMP::Holder<CKPT>* GetCheckpointHolder(KMP::Manager& kmp, u16 idx) {
    if (kmp.ckptSection == nullptr || idx >= kmp.ckptSection->pointCount) return nullptr;
    return kmp.ckptSection->holdersArray[idx];
}

static KMP::Holder<JGPT>* GetJugemPointHolder(KMP::Manager& kmp, u16 idx) {
    if (kmp.jgptSection == nullptr || idx >= kmp.jgptSection->pointCount) return nullptr;
    return kmp.jgptSection->holdersArray[idx];
}

static GX::Color MakeCheckpointColor(u8 r, u8 g, u8 b, u8 a) {
    GX::Color color;
    color.r = r;
    color.g = g;
    color.b = b;
    color.a = a;
    return color;
}

static GX::Color MakeKeyCheckpointColor() {
    return MakeCheckpointColor(0xb0, 0x40, 0xff, 75);
}

static GX::Color MakeRegularCheckpointColor() {
    return MakeCheckpointColor(0x20, 0x8c, 0xff, 75);
}

static void SetupCheckpointGX(const EGG::Matrix34f& viewMtx) {
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
    GX::SetZMode(true, GX::GX_LEQUAL, false);
    GX::SetCullMode(GX::GX_CULL_NONE);
    GX::LoadPosMtxImm(viewMtx.element, 0);
    GX::SetCurrentMtx(0);
}

static void SubmitCheckpointVertex(const CheckpointVertex& vertex, GX::Color color) {
    if (vertex.translucent) color.a = 0x60;
    GX_Position3f32(vertex.pos.x, vertex.pos.y, vertex.pos.z);
    GX_Color4u8(color.r, color.g, color.b, color.a);
}

static void SetCheckpointVertex(CheckpointVertex& vertex, float x, float y, float z, bool translucent) {
    vertex.pos.x = x;
    vertex.pos.y = y;
    vertex.pos.z = z;
    vertex.translucent = translucent;
}

static void DrawCheckpointPlane(KMP::Manager& kmp, const CKPT& checkpoint, GX::Color color) {
    KMP::Holder<JGPT>* jugemPoint = GetJugemPointHolder(kmp, checkpoint.respawn);
    if (jugemPoint == nullptr || jugemPoint->raw == nullptr) return;

    const float top = jugemPoint->raw->position.y + CHECKPOINT_TOP_OFFSET;
    const float bottom = jugemPoint->raw->position.y + CHECKPOINT_BOTTOM_OFFSET;

    CheckpointVertex face[4];
    SetCheckpointVertex(face[0], checkpoint.leftPoint.x, top, checkpoint.leftPoint.z, false);
    SetCheckpointVertex(face[1], checkpoint.leftPoint.x, bottom, checkpoint.leftPoint.z, false);
    SetCheckpointVertex(face[2], checkpoint.rightPoint.x, bottom, checkpoint.rightPoint.z, false);
    SetCheckpointVertex(face[3], checkpoint.rightPoint.x, top, checkpoint.rightPoint.z, false);

    GX::Begin(GX::GX_QUADS, GX::GX_VTXFMT0, 4);
    for (u32 vert = 0; vert < 4; ++vert) SubmitCheckpointVertex(face[vert], color);
    GXEnd();

    color.a = 0xff;
    GX::SetLineWidth(8, GX::GX_TO_ZERO);
    GX::Begin(GX::GX_LINESTRIP, GX::GX_VTXFMT0, 5);
    for (u32 vert = 0; vert < 5; ++vert) {
        const CheckpointVertex& vertex = face[vert % 4];
        GX_Position3f32(vertex.pos.x, vertex.pos.y, vertex.pos.z);
        GX_Color4u8(color.r, color.g, color.b, color.a);
    }
    GXEnd();
}

static void DrawCheckpointPath(KMP::Manager& kmp, const CKPH& checkPath, DisplayMode displayMode) {
    const u16 start = checkPath.start;
    const u16 end = static_cast<u16>(checkPath.start + checkPath.length);
    if (kmp.ckptSection == nullptr) return;

    for (u16 i = start; i < end && i < kmp.ckptSection->pointCount; ++i) {
        KMP::Holder<CKPT>* holder = GetCheckpointHolder(kmp, i);
        if (holder == nullptr || holder->raw == nullptr) continue;

        const CKPT& checkpoint = *holder->raw;
        const bool isKeyCheckpoint = checkpoint.type != 0xff;
        if (!isKeyCheckpoint && displayMode != DISPLAY_ALL) continue;

        GX::Color color;
        if (isKeyCheckpoint) {
            color = MakeKeyCheckpointColor();
        } else {
            color = MakeRegularCheckpointColor();
        }

        DrawCheckpointPlane(kmp, checkpoint, color);
    }
}

static GameScreen* GetActiveModelScreen(GameScreen* screen) {
    if (screen != nullptr) return screen;
    Renderer* const* currentRenderer = reinterpret_cast<Renderer* const*>(kmRuntimeAddr(0x809c1848));
    if (currentRenderer[0] == nullptr) return nullptr;
    return &currentRenderer[0]->screen;
}

static void DrawPracticeCheckpoints(GameScreen* screen) {
    const DisplayMode displayMode = GetDisplayMode();
    if (displayMode == DISPLAY_DISABLED) return;
    if (!IsEnabled() || screen == nullptr || screen->perspectiveCam == nullptr) return;

    KMP::Manager* kmp = KMP::Manager::sInstance;
    if (kmp == nullptr || kmp->ckptSection == nullptr || kmp->ckphSection == nullptr || kmp->jgptSection == nullptr) return;

    SetupCheckpointGX(screen->perspectiveCam->GetViewMatrix());
    for (u16 i = 0; i < kmp->ckphSection->pointCount; ++i) {
        KMP::Holder<CKPH>* holder = kmp->ckphSection->holdersArray[i];
        if (holder == nullptr || holder->raw == nullptr) continue;
        DrawCheckpointPath(*kmp, *holder->raw, displayMode);
    }
}

static void DrawPracticeModelsAndCheckpoints(ScnMgr* mgr, GameScreen* screen) {
    GetScnMgrDrawModelsImpl()(mgr, screen);
    DrawPracticeCheckpoints(GetActiveModelScreen(screen));
}
kmCall(0x805b1d40, DrawPracticeModelsAndCheckpoints);

}  // namespace Checkpoints
}  // namespace TTPractice
}  // namespace Pulsar
