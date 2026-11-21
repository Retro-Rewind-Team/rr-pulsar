#ifndef _PUL_PCP_SOCKET_
#define _PUL_PCP_SOCKET_

#include <kamek.hpp>
#include <runtimeWrite.hpp>

namespace Pulsar {
namespace Network {
namespace SO {

// Structures

struct SockAddrIn {
    u8 len;
    u8 family;
    u16 port;
    u32 addr;
};

// so.h:217-221 (SOPollFD): three ints.
struct PollFD {
    s32 fd;
    s32 events;
    s32 revents;
};

// One entry of the address table (option 0x4003). 12 bytes per entry: main.dol, getlocalhost()
// at 0x800f1d2c. Field order {ip, netmask, broadcast}: ipdiag and Dolphin (A6).
// Only used with PCP_USE_INTERFACE_OPT 1.
struct IPAddrEntry {
    u8 addr[4];
    u8 netmask[4];
    u8 bcastAddr[4];
};

// One entry of the routing table (option 0x4006), 24 bytes, big endian. The game never asks for
// it. Layout agrees in ipdiag, wiibrew (/dev/net/ip/top), Dolphin (IP/Top.cpp, case 0x4006) and
// libogc (if_configex). Default route: dest 0, netmask 0, gateway != 0, flags & 1.
// Bytes instead of u32/u64 members, so the struct has no padding on any compiler.
struct RouteEntry {
    u8 dest[4];
    u8 netmask[4];
    u8 gateway[4];
    u8 flags[4];  // bit 0 set on routes via a gateway
    u8 tick[8];   // hardware: a counter, Dolphin: 0; not used
};

typedef char PollFD_size_must_be_12[(sizeof(PollFD) == 12) ? 1 : -1];
typedef char SockAddrIn_size_must_be_8[(sizeof(SockAddrIn) == 8) ? 1 : -1];
typedef char IPAddrEntry_size_must_be_12[(sizeof(IPAddrEntry) == 12) ? 1 : -1];
typedef char RouteEntry_size_must_be_24[(sizeof(RouteEntry) == 24) ? 1 : -1];

// Constants

static const s32 PF_INET = 2;           // so.h:271
static const s32 SOCK_DGRAM = 2;        // so.h:273
static const s32 POLLRDNORM = 0x0001;   // so.h:258
static const s32 POLLERR = 0x0020;      // so.h:263, output only
static const s32 POLLHUP = 0x0040;      // so.h:264, output only
static const s32 POLLNVAL = 0x0080;     // so.h:265, output only
static const s32 SOL_CONFIG = 0xFFFE;   // so.h:123

// SOFcntl commands. Used by name in gsPlat:29-36; values from main.dol (SetSockBlocking 0x800f1bc8).
static const s32 F_GETFL = 3;
static const s32 F_SETFL = 4;
static const s32 O_NONBLOCK = 0x04;

// Flags for SORecvFrom: the game passes 4 on every receive (gsRev:93, gsRev:101). That 4 means
// "do not wait" is not confirmed; receiving only happens after SOPoll reported data anyway.
static const s32 RECV_FLAGS = 4;

// Interface options (level SOL_CONFIG). Address table: main.dol, getlocalhost() 0x800f1d2c.
// Routing table: wiibrew, ipdiag and Dolphin (0x4005 = number of entries, 32 everywhere).
static const s32 CONFIG_IP_ADDR_NUMBER = 0x4002;
static const s32 CONFIG_IP_ADDR_TABLE = 0x4003;
static const s32 CONFIG_ROUTE_TABLE_SIZE = 0x4005;
static const s32 CONFIG_ROUTE_TABLE = 0x4006;

// The IPC request behind SOGetInterfaceOpt: IOS_Ioctlv(fd, 0x1C, 1 in, 2 io) with
// in[0] = {level, option}, io[0] = answer, io[1] = answer length (in: room, out: written).
// Same in the forecast decomp (soBasic.c), libogc and Dolphin. PCP.cpp sends it itself in Dolphin,
// because the game's wrapper hands IOS an unzeroed heap block instead of the caller's buffer.
static const char NET_IP_TOP_PATH[] = "/dev/net/ip/top";
static const s32 IOCTLV_GET_INTERFACE_OPT = 0x1C;

// enum SOError (so.h:275-357), only the values PCP.cpp looks at.
static const s32 E_FATAL = static_cast<s32>(0x80000000);  // so.h:277
static const s32 E_ADDRINUSE = -3;   // so.h:280
static const s32 E_AGAIN = -6;       // so.h:283 (= SO_EWOULDBLOCK, so.h:355)
static const s32 E_BADF = -8;        // so.h:285
static const s32 E_BUSY = -10;       // so.h:287
static const s32 E_INTR = -27;       // so.h:304
static const s32 E_INVAL = -28;      // so.h:305
static const s32 E_NETRESET = -39;   // so.h:316
static const s32 E_NOBUFS = -42;     // so.h:319
static const s32 E_NOMEM = -49;      // so.h:326
static const s32 E_NOTCONN = -56;    // so.h:333
static const s32 E_NOTSOCK = -59;    // so.h:336

// Functions
// Parameter lists: so.h:232-254. Return value < 0 is an error (gsRev:30-35).

typedef s32 (*SocketFn)(s32 pf, s32 type, s32 protocol);  // the game passes protocol 0 (gsRev:38)
typedef s32 (*CloseFn)(s32 s);
typedef s32 (*BindFn)(s32 s, const void *sockAddr);
typedef s32 (*GetSockNameFn)(s32 s, void *sockAddr);
typedef s32 (*SendToFn)(s32 s, const void *buf, s32 len, s32 flags, const void *sockTo);
typedef s32 (*RecvFromFn)(s32 s, void *buf, s32 len, s32 flags, void *sockFrom);
// Variadic like so.h:246; main.dol confirms the callee checks cr1 for float arguments.
typedef s32 (*FcntlFn)(s32 s, s32 cmd, ...);
// so.h:248, timeout is one 64 bit value in r5:r6 (main.dol). Always called with 0.
typedef s32 (*PollFn)(PollFD *fds, u32 nfds, s64 timeout);
// so.h:189: own IPv4 address, 0 while there is none yet.
typedef s32 (*GetHostIDFn)();
// so.h:253: interface NULL as the game passes it, optlen in/out in bytes, 0 = success.
typedef s32 (*GetInterfaceOptFn)(void *iface, s32 level, s32 optname, void *optval, s32 *optlen);

// PAL addresses: each checked as a function start in main.dol.
kmRuntimeUse(0x801ecff4);  // SOSocket
kmRuntimeUse(0x801ed0e4);  // SOClose
kmRuntimeUse(0x801ed188);  // SOBind
kmRuntimeUse(0x801ed358);  // SOGetSockName
kmRuntimeUse(0x801ed4a0);  // SOSendTo
kmRuntimeUse(0x801ed454);  // SORecvFrom
kmRuntimeUse(0x801ed4ec);  // SOFcntl
kmRuntimeUse(0x801ed6d0);  // SOPoll
kmRuntimeUse(0x801ede88);  // SOGetHostID
kmRuntimeUse(0x801ee48c);  // SOGetInterfaceOpt

static const SocketFn Socket = (SocketFn)kmRuntimeAddr(0x801ecff4);
static const CloseFn Close = (CloseFn)kmRuntimeAddr(0x801ed0e4);
static const BindFn Bind = (BindFn)kmRuntimeAddr(0x801ed188);
static const GetSockNameFn GetSockName = (GetSockNameFn)kmRuntimeAddr(0x801ed358);
static const SendToFn SendTo = (SendToFn)kmRuntimeAddr(0x801ed4a0);
static const RecvFromFn RecvFrom = (RecvFromFn)kmRuntimeAddr(0x801ed454);
static const FcntlFn Fcntl = (FcntlFn)kmRuntimeAddr(0x801ed4ec);
static const PollFn Poll = (PollFn)kmRuntimeAddr(0x801ed6d0);
static const GetHostIDFn GetHostID = (GetHostIDFn)kmRuntimeAddr(0x801ede88);
static const GetInterfaceOptFn GetInterfaceOpt = (GetInterfaceOptFn)kmRuntimeAddr(0x801ee48c);

}  // namespace SO
}  // namespace Network
}  // namespace Pulsar

#endif
