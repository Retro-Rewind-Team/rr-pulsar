#ifndef _PUL_MATCH_COMMAND_
#define _PUL_MATCH_COMMAND_
#include <core/rvl/DWC/DWCMatch.hpp>
#include <core/rvl/ipc/ipc.hpp>

namespace Pulsar {
namespace Network {

struct ResvInfo {
    struct UserInfo {
        UserInfo() {
            info[0] = -1;
            info[1] = -1;
            info[2] = -1;
            info[3] = -1;
        }
        u32 info[4];
    };
    ResvInfo() {
        padding[0] = 0;
        padding[1] = 0;
    }
    u32 roomKey;
    char modFolderName[IOS::ipcMaxFileName];
    u8 statusData;
    u8 padding[2];
    UserInfo userInfo;
};
static_assert(sizeof(ResvInfo) == 0x24, "ResvInfo size");

struct ResvPacket : DWC::Reservation {
    ResvPacket(const DWC::Reservation& src);
    ResvInfo pulInfo;
};
static_assert(sizeof(ResvPacket) == 0x48, "ResvPacket size");

}  // namespace Network
}  // namespace Pulsar

#endif
