#ifndef _KAMEK_RUNTIME_WRITE_
#define _KAMEK_RUNTIME_WRITE_
#include <types.hpp>

#define kmRuntimeUse(addrHex) extern "C" char __kAutoMap_##addrHex
#define kmRuntimeAddr(addrHex) ((u32) & __kAutoMap_##addrHex)

#endif
