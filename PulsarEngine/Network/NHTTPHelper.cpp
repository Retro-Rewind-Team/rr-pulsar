#include <Network/NHTTPHelper.hpp>
#include <core/RK/RKSystem.hpp>
#include <core/egg/mem/Heap.hpp>
#include <core/rvl/DWC/NHTTP.hpp>

namespace Pulsar {
namespace Network {

static u32 s_activeRequestCount = 0;

void* NHTTPAlloc(u32 size, s32 align) {
    EGG::Heap* heap = RKSystem::mInstance.EGGSystem;
    if (heap == nullptr) return nullptr;
    if (align < 4) align = 4;
    return EGG::Heap::alloc(size, align, heap);
}

void NHTTPFree(void* ptr) {
    if (ptr == nullptr) return;
    EGG::Heap* heap = RKSystem::mInstance.EGGSystem;
    if (heap != nullptr) EGG::Heap::free(ptr, heap);
}

bool PrepareNHTTPRequest() {
    if (s_activeRequestCount != 0) return true;

    const s32 startupRet = NHTTPStartup(reinterpret_cast<void*>(&NHTTPAlloc),
                                        reinterpret_cast<void*>(&NHTTPFree),
                                        0x11);
    if (startupRet < 0) return false;

    return true;
}

void MarkNHTTPRequestActive() {
    ++s_activeRequestCount;
}

void FinishNHTTPRequest() {
    if (s_activeRequestCount != 0) --s_activeRequestCount;
}

}  // namespace Network
}  // namespace Pulsar
