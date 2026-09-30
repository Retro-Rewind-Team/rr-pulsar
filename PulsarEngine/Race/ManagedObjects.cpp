#include <kamek.hpp>
#include <MarioKartWii/System/Random.hpp>
#include <MarioKartWii/Objects/Object.hpp>
#include <MarioKartWii/Objects/ObjectsMgr.hpp>

namespace Pulsar {
namespace Race {

// The game only creates this container for a small set of stock courses. A
// custom course can nevertheless contain a collidable object that requests
// managed-object registration. In that case the stock code dereferences the
// stale course-specific pointer.
struct ManagedObjectsStorage {
    Object *objects[0x3c];
    u32 managedObjCount;
};

static bool IsWiiMemoryAddress(const void *pointer) {
    const u32 address = reinterpret_cast<u32>(pointer);
    return (address >= 0x80000000 && address < 0x81800000) ||
           (address >= 0x90000000 && address < 0x94000000);
}

static void RegisterManagedObject(ObjectsMgr *mgr, Object *object) {
    if (mgr == nullptr || object == nullptr) return;

    ManagedObjectsStorage *storage = reinterpret_cast<ManagedObjectsStorage *>(mgr->managedObjects);
    if (!IsWiiMemoryAddress(storage)) {
        EGG::Heap *heap = EGG::Heap::current;
        if (heap == nullptr) return;

        storage = new (heap) ManagedObjectsStorage();
        if (storage == nullptr) return;
        mgr->managedObjects = reinterpret_cast<ManagedObjects *>(storage);
    }

    if (storage->managedObjCount >= 0x3c) return;

    mgr->managedObjects->RegisterObject(object);
}
kmBranch(0x8082b3a0, RegisterManagedObject);

}  // namespace Race
}  // namespace Pulsar
