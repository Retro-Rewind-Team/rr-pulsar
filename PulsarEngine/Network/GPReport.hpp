/*
    Based on wfc-patcher-wii
    Written by mkwcat

    Copyright (c) 2023-2025 WiiLink
    SPDX-License-Identifier: gpl-2.0-or-later
*/

#ifndef _GPREPORT_
#define _GPREPORT_
#include <types.hpp>

namespace Pulsar {
namespace Network {

void Report(const char* key, const char* string);
void ReportU32(const char* key, u32 uint);

}  // namespace Network
}  // namespace Pulsar
#endif
