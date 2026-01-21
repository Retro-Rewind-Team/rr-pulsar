#include <kamek.hpp>
#include <core/nw4r/g3d/res/ResFile.hpp>
#include <MarioKartWii/3D/Model/ModelDirector.hpp>

namespace Pulsar {

static bool SafeCheckRevision(nw4r::g3d::ResFile* file) {
    if (file == nullptr || file->data == nullptr) {
        return false;
    }
    return file->CheckRevision();
}
kmCall(0x8055b810, SafeCheckRevision);

static void SafeInit(nw4r::g3d::ResFile* file) {
    if (file == nullptr || file->data == nullptr) {
        return;
    }
    file->Init();
}
kmCall(0x8055b81c, SafeInit);

static bool SafeBind(nw4r::g3d::ResFile* file, nw4r::g3d::ResFile* rhs) {
    if (file == nullptr || file->data == nullptr || rhs == nullptr || rhs->data == nullptr) {
        return false;
    }
    return file->Bind(*rhs);
}
kmCall(0x8055b838, SafeBind);
kmCall(0x8055b854, SafeBind);

} // namespace Pulsar