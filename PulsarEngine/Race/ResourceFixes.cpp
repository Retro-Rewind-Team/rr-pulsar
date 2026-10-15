#include <kamek.hpp>
#include <core/nw4r/g3d/res/ResFile.hpp>
#include <MarioKartWii/3D/Model/ModelDirector.hpp>
#include <MarioKartWii/Scene/GameScene.hpp>
#include <core/rvl/gx/GX.hpp>

namespace Pulsar {

static bool SafeCheckRevision(nw4r::g3d::ResFile *file) {
    if (file == nullptr || file->data == nullptr) {
        return false;
    }
    return file->CheckRevision();
}
kmCall(0x8055b810, SafeCheckRevision);

static void SafeInit(nw4r::g3d::ResFile *file) {
    if (file == nullptr || file->data == nullptr) {
        return;
    }
    file->Init();
}
kmCall(0x8055b81c, SafeInit);

static bool SafeBind(nw4r::g3d::ResFile *file, nw4r::g3d::ResFile *rhs) {
    if (file == nullptr || file->data == nullptr || rhs == nullptr || rhs->data == nullptr) {
        return false;
    }
    return file->Bind(*rhs);
}
kmCall(0x8055b838, SafeBind);
kmCall(0x8055b854, SafeBind);

// GameScene::reinit+0x1C, "bl GameScene::Reset", before it frees the structs heaps: the outgoing scene's light textures
// still have EFB copies queued into them, so drain the GPU first, as EGG::SceneManager::destroyScene does.
static void ResetAndDrainGpu(GameScene &scene) {
    scene.Reset();
    GX::Flush();
    GX::DrawDone();
}
kmCall(0x8051b7cc, ResetAndDrainGpu);

}  // namespace Pulsar