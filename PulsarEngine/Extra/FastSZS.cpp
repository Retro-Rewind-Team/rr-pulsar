// FastSZS logic developed by Rambo

#include <kamek.hpp>

namespace Pulsar {
namespace Extra {

extern "C" u32 decodeSZSAsm(void* dst, const void* src);

u32 Decomp_decodeSZS_hook(const void* src, void* dest) {
    return decodeSZSAsm(dest, src);
}

kmCall(0x80218be8, Decomp_decodeSZS_hook);
kmCall(0x80519560, Decomp_decodeSZS_hook);
kmCall(0x8051d350, Decomp_decodeSZS_hook);
kmCall(0x8051d644, Decomp_decodeSZS_hook);

}  // namespace Extra
}  // namespace Pulsar