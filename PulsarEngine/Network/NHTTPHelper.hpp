#ifndef _PUL_NHTTP_HELPER_
#define _PUL_NHTTP_HELPER_

#include <types.hpp>

namespace Pulsar {
namespace Network {

void* NHTTPAlloc(u32 size, s32 align);
void NHTTPFree(void* ptr);
bool PrepareNHTTPRequest();
void MarkNHTTPRequestActive();
void FinishNHTTPRequest();

}  // namespace Network
}  // namespace Pulsar

#endif
