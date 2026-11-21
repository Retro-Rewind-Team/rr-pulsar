/*
    PCP port mapping (RFC 6887, MAP, IPv4/UDP) as a non-blocking state machine.
*/

#include <kamek.hpp>
#include <core/rvl/OS/OS.hpp>
#include <core/rvl/ipc/ipc.hpp>
#include <Network/PCP/PCP.hpp>
#include <Network/PCP/PCPSocket.hpp>
#include <Dolphin/DolphinIOS.hpp>

// Lifetime asked for. Renewed at half time, so after a hard power-off the port stays open for
// two hours at most.
#ifndef PCP_REQUESTED_LIFETIME_SEC
#define PCP_REQUESTED_LIFETIME_SEC 7200
#endif

// UDP source port of the client (the port RFC 6887 reserves for PCP clients). (A9: nothing else
// in the game binds it.)
#ifndef PCP_CLIENT_PORT
#define PCP_CLIENT_PORT 5350
#endif

// 1 = read the own netmask from the address table (0x4003). It is then used to check that the
// gateway lies in the own subnet and for the fallback guesses (otherwise /24).
#ifndef PCP_USE_INTERFACE_OPT
#define PCP_USE_INTERFACE_OPT 0
#endif

// Fixed PCP server, host order (e.g. 0xC0A8B201 = 192.168.178.1). 0 = read the routing table.
#ifndef PCP_GATEWAY_OVERRIDE
#define PCP_GATEWAY_OVERRIDE 0
#endif

// 1 = send "::" as suggested external address, like the launcher.
#ifndef PCP_SUGGEST_V6_UNSPECIFIED
#define PCP_SUGGEST_V6_UNSPECIFIED 0
#endif

#ifndef PCP_DEFAULT_ENABLED
#define PCP_DEFAULT_ENABLED 1
#endif

// Logs state changes only, never per frame.
#ifndef PCP_LOG_ENABLED
#define PCP_LOG_ENABLED 1
#endif
#if PCP_LOG_ENABLED
#define PCP_LOG(...) OS::Report(__VA_ARGS__)
#else
#define PCP_LOG(...)
#endif

// printf arguments for an IPv4 address in host order: "%u.%u.%u.%u"
#define PCP_IP4(ip) (((ip) >> 24) & 0xff), (((ip) >> 16) & 0xff), (((ip) >> 8) & 0xff), ((ip) & 0xff)

namespace Pulsar {
namespace Network {
namespace PCP {

// Protocol
static const u16 SERVER_PORT = 5351;
static const u8 PCP_VERSION = 2;
static const u8 OPCODE_MAP = 1;
static const u8 PROTO_UDP = 17;
static const s32 MAP_SIZE = 60;    // 24 header + 36 MAP, request and response
static const u32 RX_SIZE = 0x460;  // >= 1100, the largest PCP message

// Timing
static const u32 MAX_LIFETIME_SEC = 86400;  // RFC 6887 section 15: longer leases count as 24 h
static const u32 MIN_RENEW_DELAY_SEC = 4;   // RFC 6887 section 11.2.1
static const u32 BACKOFF_FIRST_SEC = 30;    // no answer: 30 s, 60 s, ... 10 min
static const u32 BACKOFF_MAX_SEC = 600;
static const u32 SERVER_RETRY_MIN_SEC = 30;  // router refused: wait what it says, within these bounds
static const u32 SERVER_RETRY_MAX_SEC = 1800;
static const u32 SOCK_RETRY_SEC = 2;  // one quick retry after a passing SO error
static const u32 TICK_GAP_MS = 500;   // a longer pause between ticks -> check the socket before use

// Limits
static const u32 MAX_RX_PER_TICK = 2;
static const u32 MAX_SOCK_WAIT_TICKS = 30;  // ticks a transaction waits for a socket that says "later"
static const u32 SHUTDOWN_DELETE_COPIES = 2;  // unconfirmed delete at session end: UDP, send twice
static const u32 ROUTE_TABLE_MAX = 32;  // option 0x4005 answers 32 on every measured IOS and in Dolphin

enum Op {
    OP_MAP,
    OP_RENEW,
    OP_DELETE,
};

// Retransmission per request type: MAP 0.5/1/2 s per candidate, renewal 1/2/4/8 s, delete 0.5/1 s.
struct Retry {
    u32 attempts;
    u32 firstMs;
};
static const Retry RETRY[] = {{3, 500}, {4, 1000}, {2, 500}};  // indexed by Op

struct Context {
    // Members with a start value other than 0 come first (see the initializer below).
    State state;
    Error error;
    s32 pcpResult;  // -1 = none yet
    s32 sock;       // -1 = none
    u32 backoffSec;

    s32 soError;
    s32 rejectResult;
    u32 flags;
    bool fatal;

    // identity, read once
    bool haveSerno;
    u32 serno;
    u16 internalPort;
    u8 nonce[12];

    // socket
    bool sockNeedsCheck;  // look at the descriptor (SOGetSockName) before using it again
    u64 lastTick;
    u32 sockWait;
    bool quickRetryUsed;

    // addresses, host order
    u32 localIp;
    u32 gateway;  // PCP server that answered with our nonce; renewals and deletes go here
    u32 candidates[4];  // where the running MAP goes: override, default route, or the guesses
    u32 candidateCount;  // 0 = not resolved yet
    u32 candidateIdx;

    // running transaction
    Op op;
    u32 attempt;  // requests sent to the current target
    u32 sendOk;   // requests sent successfully in this transaction
    u32 rtoMs;
    u64 rtoDeadline;
    bool otherVersionSeen;  // a NAT-PMP / other PCP version answer was seen

    // mapping
    bool active;
    u16 externalPort;
    u8 externalAddr[16];
    u32 externalIp;
    u32 lifetime;
    u64 expiresAt;
    u64 renewAt;
    u64 retryAt;
};

static Context ctx = {STATE_IDLE, ERR_NONE, -1, -1, BACKOFF_FIRST_SEC};

static volatile bool sEnableRequest = PCP_DEFAULT_ENABLED;
static volatile u32 sGatewayOverride = PCP_GATEWAY_OVERRIDE;
static volatile bool sBusy = false;
static volatile bool sPendingSessionLost = false;

// Snapshot for GetStatus(), written by Publish() at the end of every tick.
static Status sPublished;
static u64 sPublishedExpiresAt = 0;
static bool sHavePublished = false;

// IPC buffers, aligned to 0x20 like everything the SO library hands to IOS.
static u8 sTx[0x40] __attribute((aligned(0x20)));
static u8 sRx[RX_SIZE] __attribute((aligned(0x20)));
static u8 sSetting[0x100] __attribute((aligned(0x20)));
static SO::RouteEntry sRoutes[ROUTE_TABLE_MAX] __attribute((aligned(0x20)));
// The own ioctlv of the routing table call (Dolphin only).
static u32 sIn[8] __attribute((aligned(0x20)));   // {level, option}
static u32 sLen[8] __attribute((aligned(0x20)));  // in: room, out: bytes written
static IOS::IOCtlvRequest sVec[4] __attribute((aligned(0x20)));

// ---------------------------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------------------------

static u64 Now() {
    return static_cast<u64>(OS::GetTime());
}
static u64 SecToTicks(u32 sec) {
    return static_cast<u64>(OS::GetTimerClock()) * sec;
}
static u64 MsToTicks(u32 ms) {
    return static_cast<u64>(OS::GetTimerClock() / 1000) * ms;
}

static u32 Clamp(u32 value, u32 lo, u32 hi) {
    return value < lo ? lo : (value > hi ? hi : value);
}

static u16 ReadU16(const u8 *p) {
    return static_cast<u16>((p[0] << 8) | p[1]);
}
static u32 ReadU32(const u8 *p) {
    return (static_cast<u32>(p[0]) << 24) | (static_cast<u32>(p[1]) << 16) | (static_cast<u32>(p[2]) << 8) | p[3];
}
static void WriteU16(u8 *p, u16 value) {
    p[0] = static_cast<u8>(value >> 8);
    p[1] = static_cast<u8>(value);
}
static void WriteU32(u8 *p, u32 value) {
    p[0] = static_cast<u8>(value >> 24);
    p[1] = static_cast<u8>(value >> 16);
    p[2] = static_cast<u8>(value >> 8);
    p[3] = static_cast<u8>(value);
}

static SO::SockAddrIn MakeAddr(u32 ip, u16 port) {
    SO::SockAddrIn a;
    a.len = sizeof(a);  // the SO library wants the length set, also for output parameters
    a.family = static_cast<u8>(SO::PF_INET);
    a.port = port;
    a.addr = ip;
    return a;
}

// Tick() and OnSessionEnd() must never run into each other. Interrupts are only off for the
// test-and-set, never across an SO call.
static bool TryEnter() {
    const int irq = OS::DisableInterrupts();
    const bool ok = !sBusy;
    if (ok)
        sBusy = true;
    OS::RestoreInterrupts(irq);
    return ok;
}
static void Leave() {
    sBusy = false;
}

static void Publish() {
    Status s;
    memset(&s, 0, sizeof(s));
    s.state = ctx.state;
    s.error = ctx.error;
    s.pcpResult = ctx.pcpResult;
    s.soError = ctx.soError;
    s.active = ctx.active;
    s.internalPort = ctx.internalPort;
    s.externalPort = ctx.active ? ctx.externalPort : 0;
    s.externalIp = ctx.active ? ctx.externalIp : 0;
    s.gatewayIp = ctx.gateway;
    s.localIp = ctx.localIp;
    s.lifetime = ctx.active ? ctx.lifetime : 0;
    s.flags = ctx.flags;
    s.serno = ctx.serno;
    s.rejectResult = ctx.rejectResult;

    const int irq = OS::DisableInterrupts();
    sPublished = s;
    sPublishedExpiresAt = ctx.active ? ctx.expiresAt : 0;
    sHavePublished = true;
    OS::RestoreInterrupts(irq);
}

// Reads SERNO from the encrypted setting.txt. Only file access, once per run.
static bool ReadSerno(u32 &serno) {
    const s32 fd = IOS::Open("/title/00000001/00000002/data/setting.txt", IOS::MODE_READ);
    if (fd < 0)
        return false;
    const s32 read = IOS::Read(fd, sSetting, sizeof(sSetting));
    IOS::Close(fd);
    if (read != static_cast<s32>(sizeof(sSetting)))
        return false;

    u32 key = 0x73B5DBFA;
    for (u32 i = 0; i < sizeof(sSetting); ++i) {
        sSetting[i] ^= static_cast<u8>(key);
        key = (key << 1) | (key >> 31);
    }

    for (u32 i = 0; i + 6 <= sizeof(sSetting); ++i) {
        if (memcmp(&sSetting[i], "SERNO=", 6) != 0)
            continue;
        u32 value = 0;
        u32 digits = 0;
        for (u32 j = i + 6; j < sizeof(sSetting) && sSetting[j] >= '0' && sSetting[j] <= '9'; ++j) {
            value = value * 10 + (sSetting[j] - '0');
            ++digits;
        }
        if (digits == 0)
            return false;
        serno = value;
        return true;
    }
    return false;
}

// Generator of rr_pcp_make_nonce, seeded only with values that never change on this console.
static void MakeNonce(u8 *nonce, u32 serno, u16 port) {
    u32 state = serno ^ ((static_cast<u32>(port) << 16) | port) ^ 0x50435052;
    if (state == 0)
        state = 0x9e3779b9;
    for (u32 i = 0; i < 12; ++i) {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        nonce[i] = static_cast<u8>(state);
    }
}

// ---------------------------------------------------------------------------------------------
// Socket
// ---------------------------------------------------------------------------------------------

// What a negative SO return value means for us (enum SOError, so.h).
enum SoClass {
    SOC_OK,
    SOC_AGAIN,      // nothing to do right now, not an error
    SOC_TRANSIENT,  // passing condition of the library, says nothing about our socket
    SOC_GONE,       // the descriptor is certainly not a socket (any more)
    SOC_OTHER,      // e.g. SO_EINVAL (library not started, or a bad argument), SO_EFATAL, SO_ENOTCONN
};
static SoClass Classify(s32 ret) {
    if (ret >= 0)
        return SOC_OK;
    switch (ret) {
        case SO::E_AGAIN:
        case SO::E_INTR:
            return SOC_AGAIN;
        case SO::E_NOMEM:  // every SO call allocates its IPC buffer
        case SO::E_NOBUFS:
        case SO::E_BUSY:  // another thread is inside SOStartup/SOCleanup
            return SOC_TRANSIENT;
        case SO::E_BADF:
        case SO::E_NOTSOCK:
        case SO::E_NETRESET:  // library terminated
            return SOC_GONE;
        default:
            return SOC_OTHER;
    }
}
static bool IsPassing(s32 ret) {
    const SoClass c = Classify(ret);
    return c == SOC_AGAIN || c == SOC_TRANSIENT;
}

// Records a failed SO call on our socket. Unless the reason is a passing one, the descriptor is
// looked at again before its next use.
static void RecordSoError(s32 ret) {
    ctx.soError = ret;
    if (!IsPassing(ret))
        ctx.sockNeedsCheck = true;
}

enum SockCheck {
    SOCK_OURS,   // confirmed: still bound to our client port
    SOCK_GONE,   // certainly not ours any more, number forgotten (never closed)
    SOCK_LATER,  // could not be asked right now
    SOCK_DOUBT,  // unclear answer: number kept but not used, a new socket may be tried
};

static void ForgetSocket() {
    ctx.sock = -1;
    ctx.flags |= FLAG_SOCKET_REPLACED;
}

static SockCheck CheckSocket() {
    if (ctx.sock < 0)
        return SOCK_GONE;
    if (!ctx.sockNeedsCheck)
        return SOCK_OURS;

    SO::SockAddrIn name = MakeAddr(0, 0);
    name.family = 0;
    const s32 ret = SO::GetSockName(ctx.sock, &name);
    if (ret >= 0) {
        if (name.family == SO::PF_INET && name.port == PCP_CLIENT_PORT) {
            ctx.sockNeedsCheck = false;
            return SOCK_OURS;
        }
        PCP_LOG("PCP: descriptor %d now belongs to somebody else (port %u)\n", ctx.sock, name.port);
        ForgetSocket();
        return SOCK_GONE;
    }

    ctx.soError = ret;
    switch (Classify(ret)) {
        case SOC_AGAIN:
        case SOC_TRANSIENT:
            return SOCK_LATER;
        case SOC_GONE:
            PCP_LOG("PCP: socket %d is gone (ret %d)\n", ctx.sock, ret);
            ForgetSocket();
            return SOCK_GONE;
        default:
            return SOCK_DOUBT;
    }
}

// ERR_NONE: ctx.sock is usable. Otherwise "later" tells whether the reason is a passing one.
static Error EnsureSocket(bool &later) {
    later = false;
    const SockCheck check = CheckSocket();
    if (check == SOCK_OURS)
        return ERR_NONE;
    if (check == SOCK_LATER) {
        later = true;
        return ERR_SOCKET;
    }

    const s32 s = SO::Socket(SO::PF_INET, SO::SOCK_DGRAM, 0);
    if (s < 0) {
        ctx.soError = s;
        if (s == SO::E_INVAL || s == SO::E_NETRESET || s == SO::E_FATAL)
            return ERR_SO_DOWN;  // same arguments as the game's call, so this means "not started"
        later = IsPassing(s);
        return ERR_SOCKET;
    }

    // Receiving is gated by a poll anyway, so a failure here is only recorded.
    const s32 fl = SO::Fcntl(s, SO::F_GETFL, 0);
    if (fl < 0 || SO::Fcntl(s, SO::F_SETFL, fl | SO::O_NONBLOCK) < 0)
        ctx.flags |= FLAG_NONBLOCK_FAILED;
    else
        ctx.flags &= ~FLAG_NONBLOCK_FAILED;

    const SO::SockAddrIn local = MakeAddr(0, PCP_CLIENT_PORT);  // 0 = INADDR_ANY
    const s32 ret = SO::Bind(s, &local);
    if (ret < 0) {
        ctx.soError = ret;
        SO::Close(s);  // created above, certainly ours
        if (ret == SO::E_ADDRINUSE)
            return ERR_PORT_IN_USE;
        later = IsPassing(ret);
        return ERR_SOCKET;
    }

    if (check == SOCK_DOUBT) {
        // The port was free, so the old descriptor was not our socket any more. Not closed.
        PCP_LOG("PCP: socket %d replaced by %d (last error %d)\n", ctx.sock, s, ctx.soError);
        ctx.flags |= FLAG_SOCKET_REPLACED;
    }
    ctx.sock = s;
    ctx.sockNeedsCheck = false;
    return ERR_NONE;
}

// Gateway: override, else the default route of the routing table, else guesses

// A possible next hop: not 0.x, not 127.x, not multicast or above (includes 255.255.255.255).
static bool IsUnicast(u32 ip) {
    const u32 a = ip >> 24;
    return a != 0 && a != 127 && a < 224;
}

// nullptr = plausible gateway for a host with address localIp (netmask 0 = unknown).
static const char *GatewayFault(u32 gw, u32 localIp, u32 netmask) {
    if (!IsUnicast(gw))
        return "not a unicast address";
    if (gw == localIp)
        return "own address";
    if (netmask != 0) {
        const u32 host = gw & ~netmask;
        if ((gw & netmask) != (localIp & netmask))
            return "outside own subnet";
        if (host == 0 || host == ~netmask)
            return "network or broadcast address of own subnet";
    }
    return nullptr;
}

RouteResult ParseRouteTable(const void *table, s32 room, s32 ret, s32 len, u32 localIp, u32 netmask) {
    RouteResult r;
    memset(&r, 0, sizeof(r));
    r.code = ROUTE_OK;
    r.why = nullptr;

    const s32 entrySize = static_cast<s32>(sizeof(SO::RouteEntry));
    if (ret < 0) {
        r.code = ROUTE_CALL_FAILED;  // the buffer may hold anything: not looked at
        return r;
    }
    if (table == nullptr || room <= 0 || len <= 0 || len > room || len % entrySize != 0) {
        r.code = ROUTE_BAD_LENGTH;
        return r;
    }
    const u8 *b = static_cast<const u8 *>(table);
    if (len == room) {
        // Length untouched and nothing written: a Dolphin without case 0x4006.
        s32 i = 0;
        while (i < len && b[i] == 0) ++i;
        if (i == len) {
            r.code = ROUTE_NOT_ANSWERED;
            return r;
        }
    }

    // The first PLAUSIBLE default route wins. Its position means nothing (hardware lists it last,
    // Dolphin first).
    r.entries = static_cast<u32>(len / entrySize);
    bool chosen = false;
    for (u32 i = 0; i < r.entries; ++i) {
        const u8 *e = b + i * entrySize;
        const u32 dest = ReadU32(e);
        const u32 mask = ReadU32(e + 4);
        const u32 gw = ReadU32(e + 8);
        const u32 flags = ReadU32(e + 12);
        if (dest != 0 || mask != 0 || gw == 0 || (flags & 1) == 0)
            continue;  // not a default route
        ++r.defaults;
        if (chosen)
            continue;  // only counted
        const char *fault = GatewayFault(gw, localIp, netmask);
        if (fault == nullptr) {
            chosen = true;
            r.gateway = gw;
            r.index = i;
            r.flags = flags;
        } else if (r.why == nullptr) {
            r.rejected = gw;
            r.why = fault;
        }
    }
    if (chosen) {
        r.rejected = 0;  // an earlier refusal only matters if nothing was chosen
        r.why = nullptr;
    } else {
        r.code = r.defaults == 0 ? ROUTE_NO_DEFAULT : ROUTE_IMPLAUSIBLE;
    }
    return r;
}

#if PCP_USE_INTERFACE_OPT
// Netmask of the own address from the address table (0x4003), 0 = unknown. Same call as the
// game's getlocalhost(). Only believed if address, netmask and broadcast fit each other.
static u32 ReadNetmask() {
    static SO::IPAddrEntry sTable[5] __attribute((aligned(0x20)));
    memset(sTable, 0, sizeof(sTable));
    s32 len = sizeof(sTable);
    const s32 entrySize = static_cast<s32>(sizeof(SO::IPAddrEntry));
    const s32 ret = SO::GetInterfaceOpt(nullptr, SO::SOL_CONFIG, SO::CONFIG_IP_ADDR_TABLE, sTable, &len);
    if (ret == 0 && len > 0 && len <= static_cast<s32>(sizeof(sTable)) && len % entrySize == 0) {
        for (s32 i = 0; i < len / entrySize; ++i) {
            const u32 addr = ReadU32(sTable[i].addr);
            const u32 netmask = ReadU32(sTable[i].netmask);
            const u32 bcast = ReadU32(sTable[i].bcastAddr);
            const u32 inv = ~netmask;
            const bool contiguous = (inv & (inv + 1)) == 0;
            const bool sane = inv >= 0x3 && inv <= 0x00FFFFFF;  // between /8 and /30
            if (addr == ctx.localIp && contiguous && sane && bcast == (addr | inv)) {
                ctx.flags &= ~FLAG_IFOPT_REJECTED;
                return netmask;
            }
        }
    } else if (ret < 0) {
        ctx.soError = ret;
    }
    ctx.flags |= FLAG_IFOPT_REJECTED;
    return 0;
}
#endif

// Own handle on /dev/net/ip/top, Dolphin only. Opened once and NEVER closed: in Dolphin all
// handles share one device object, and closing ours stops the replies to all of the game's
// socket calls. After a failed open nothing is retried; lookups fall back to guessing.
static s32 TopHandle() {
    static s32 sFd = -1;
    static bool sTried = false;
    if (!sTried) {
        sTried = true;
        sFd = IOS::Open(SO::NET_IP_TOP_PATH, IOS::MODE_NONE);
        if (sFd >= 0) {
            PCP_LOG("PCP: gateway: own handle %d on %s, kept open for the whole run (Dolphin)\n", sFd, SO::NET_IP_TOP_PATH);
        } else {
            PCP_LOG("PCP: gateway: open of %s failed (%d), not tried again in this run\n", SO::NET_IP_TOP_PATH, sFd);
        }
    }
    return sFd;
}

// Reads the routing table (0x4006) into sRoutes. Returns the call's result; len = bytes written.
//   Console: through the game's SOGetInterfaceOpt.
//   Dolphin: own ioctlv with a zeroed buffer. Dolphin starts writing at the offset given by the
//     second word of the buffer, and the game's wrapper passes an unzeroed heap block (so -28).
static s32 CallRouteTable(s32 &len) {
    memset(sRoutes, 0, sizeof(sRoutes));
    len = sizeof(sRoutes);
    if (!Dolphin::IsEmulator())
        return SO::GetInterfaceOpt(nullptr, SO::SOL_CONFIG, SO::CONFIG_ROUTE_TABLE, sRoutes, &len);

    const s32 fd = TopHandle();
    if (fd < 0)
        return fd;
    memset(sIn, 0, sizeof(sIn));
    memset(sLen, 0, sizeof(sLen));
    sIn[0] = static_cast<u32>(SO::SOL_CONFIG);
    sIn[1] = static_cast<u32>(SO::CONFIG_ROUTE_TABLE);
    sLen[0] = sizeof(sRoutes);
    sVec[0].address = sIn;
    sVec[0].size = 8;
    sVec[1].address = sRoutes;
    sVec[1].size = sizeof(sRoutes);
    sVec[2].address = sLen;
    sVec[2].size = 4;
    // No cache maintenance needed: Dolphin only.
    const s32 ret = IOS::IOCtlv(fd, static_cast<IOS::IOCtlType>(SO::IOCTLV_GET_INTERFACE_OPT), 1, 2, sVec);
    len = static_cast<s32>(sLen[0]);
    return ret;
}

static void AddCandidate(u32 ip) {
    const u32 room = sizeof(ctx.candidates) / sizeof(ctx.candidates[0]);
    if (ip == 0 || ip == ctx.localIp)
        return;
    for (u32 i = 0; i < ctx.candidateCount; ++i)
        if (ctx.candidates[i] == ip)
            return;
    if (ctx.candidateCount < room)
        ctx.candidates[ctx.candidateCount++] = ip;
}

static inline u32 CandidateOrZero(u32 i) {  // for the log line only
    return i < ctx.candidateCount ? ctx.candidates[i] : 0;
}

static void LogRouteFailure(const RouteResult &r, s32 ret, s32 len, u32 mask, const char *via) {
    switch (r.code) {
        case ROUTE_CALL_FAILED:
            PCP_LOG("PCP: gateway: route table call failed (%s, ret %d)\n", via, ret);
            break;
        case ROUTE_BAD_LENGTH:
            PCP_LOG("PCP: gateway: route table invalid (%s, ret %d, length %d, room %d, entry size 24)\n", via, ret, len, static_cast<s32>(sizeof(sRoutes)));
            break;
        case ROUTE_NOT_ANSWERED:
            PCP_LOG("PCP: gateway: route table invalid (%s, length unchanged, all %d bytes zero: option 0x4006 not answered)\n", via, static_cast<s32>(sizeof(sRoutes)));
            break;
        case ROUTE_NO_DEFAULT:
            PCP_LOG("PCP: gateway: no default route (dest 0, netmask 0, gateway != 0, flags & 1) in %u entries (%s)\n", r.entries, via);
            break;
        default:
            PCP_LOG("PCP: gateway: default route %u.%u.%u.%u rejected, %s (local %u.%u.%u.%u, netmask %08x, default routes %u of %u entries, %s)\n", PCP_IP4(r.rejected), r.why,
                    PCP_IP4(ctx.localIp), mask, r.defaults, r.entries, via);
            break;
    }
}

// Fills ctx.candidates for the next MAP attempt. Runs on a tick of its own. false: no candidate.
static bool ResolveGateway() {
    ctx.candidateCount = 0;
    ctx.candidateIdx = 0;
    ctx.flags &= ~(FLAG_GATEWAY_UNCHECKED | FLAG_GATEWAY_GUESSED);

    // 1. Override: the only address asked, the table is not read.
    const u32 fixed = sGatewayOverride;
    if (fixed != 0 && fixed != ctx.localIp) {
        AddCandidate(fixed);
        PCP_LOG("PCP: gateway %u.%u.%u.%u (override, route table not read)\n", PCP_IP4(fixed));
        return true;
    }
    if (fixed != 0) {
        PCP_LOG("PCP: gateway: override %u.%u.%u.%u is the own address, ignored\n", PCP_IP4(fixed));
    }

    // 2. Default route of the routing table.
    u32 mask = 0;
#if PCP_USE_INTERFACE_OPT
    mask = ReadNetmask();
#endif
    s32 len = 0;
    const s32 ret = CallRouteTable(len);
    const RouteResult r = ParseRouteTable(sRoutes, sizeof(sRoutes), ret, len, ctx.localIp, mask);
    const char *via = Dolphin::IsEmulator() ? "own ioctlv, Dolphin" : "SOGetInterfaceOpt";
    (void)via;  // only used by PCP_LOG

    if (r.code == ROUTE_OK) {
        AddCandidate(r.gateway);
        if (mask == 0)
            ctx.flags |= FLAG_GATEWAY_UNCHECKED;
        PCP_LOG("PCP: gateway %u.%u.%u.%u from route table (%s, flags %08x, entry %u of %u, default routes %u, %s)\n", PCP_IP4(r.gateway), via, r.flags, r.index, r.entries,
                r.defaults, mask != 0 ? "subnet checked" : (PCP_USE_INTERFACE_OPT ? "subnet NOT checked: address table refused" : "subnet NOT checked: PCP_USE_INTERFACE_OPT 0"));
        return true;
    }
    if (r.code == ROUTE_CALL_FAILED)
        ctx.soError = ret;
    LogRouteFailure(r, ret, len, mask, via);

    // 3. Guess: last server that answered, then first and last host of the subnet (/24 without
    //    a known netmask).
    const u32 m = mask != 0 ? mask : 0xFFFFFF00;
    const u32 net = ctx.localIp & m;
    AddCandidate(ctx.gateway);
    AddCandidate(net | 1);
    AddCandidate((net | ~m) - 1);
    if (ctx.candidateCount == 0)
        return false;
    ctx.flags |= FLAG_GATEWAY_GUESSED;
    PCP_LOG("PCP: gateway: guessing: %u.%u.%u.%u %u.%u.%u.%u %u.%u.%u.%u\n", PCP_IP4(CandidateOrZero(0)), PCP_IP4(CandidateOrZero(1)), PCP_IP4(CandidateOrZero(2)));
    return true;
}

// State transitions

static void EnterBackoff(u64 now, Error error, u32 seconds) {
    ctx.error = error;
    ctx.state = STATE_BACKOFF;
    ctx.retryAt = now + SecToTicks(seconds);
    PCP_LOG("PCP: failed, error %d result %d so %d, retry in %u s\n", error, ctx.pcpResult, ctx.soError, seconds);
}

// Failure without a retry time from the router: 30 s, 60 s, ... up to 10 min.
static void EnterBackoffProgressive(u64 now, Error error) {
    const u32 seconds = ctx.backoffSec;
    ctx.backoffSec = seconds * 2 > BACKOFF_MAX_SEC ? BACKOFF_MAX_SEC : seconds * 2;
    EnterBackoff(now, error, seconds);
}

static void DropMapping() {
    ctx.active = false;
    ctx.externalPort = 0;
    ctx.externalIp = 0;
    ctx.lifetime = 0;
    ctx.flags &= ~(FLAG_EXT_PORT_DIFFERS | FLAG_EXT_ADDR_NOT_PUBLIC | FLAG_LEASE_SHORTENED | FLAG_LEASE_NOT_EXTENDED | FLAG_EXT_ADDR_UNKNOWN);
}

static void StartTransaction(Op op, u64 now) {
    static const State states[] = {STATE_PROBING, STATE_RENEWING, STATE_RELEASING};  // by Op
    ctx.op = op;
    ctx.state = states[op];
    ctx.attempt = 0;
    ctx.sendOk = 0;
    ctx.sockWait = 0;
    ctx.otherVersionSeen = false;
    ctx.rtoDeadline = now;  // first request goes out on the next tick
    ctx.sockNeedsCheck = true;  // one look at the descriptor per transaction
    if (op == OP_MAP) {
        ctx.pcpResult = -1;  // "no result yet" must not look like an earlier SUCCESS (0)
        ctx.candidateCount = 0;  // the gateway is resolved on the next tick
        ctx.candidateIdx = 0;
    }
}

// Starts a new MAP attempt: identity (once), local address, socket.
static void Begin(u64 now) {
    if (!ctx.haveSerno) {
        u32 serno = 0;
        if (!ReadSerno(serno)) {
            ctx.fatal = true;
            ctx.error = ERR_SERNO;
            ctx.state = STATE_DISABLED;
            PCP_LOG("PCP: no SERNO, disabled\n");
            return;
        }
        ctx.haveSerno = true;
        ctx.serno = serno;
        ctx.internalPort = static_cast<u16>(22000 + serno % 1000);
        MakeNonce(ctx.nonce, serno, ctx.internalPort);
    }

    const u32 ip = static_cast<u32>(SO::GetHostID());  // 0 = no address yet (A4)
    if (ip == 0 || (ip >> 24) == 127) {
        EnterBackoffProgressive(now, ERR_NO_LOCAL_IP);
        return;
    }
    ctx.localIp = ip;

    bool later;
    const Error sockError = EnsureSocket(later);
    if (sockError != ERR_NONE) {
        // A passing error gets one quick retry, then the normal 30 s, 60 s, ...
        if (later && !ctx.quickRetryUsed) {
            ctx.quickRetryUsed = true;
            EnterBackoff(now, sockError, SOCK_RETRY_SEC);
        } else {
            EnterBackoffProgressive(now, sockError);
        }
        return;
    }
    StartTransaction(OP_MAP, now);
}

static void StartRenew(u64 now) {
    // A new address from DHCP: the old mapping points to the old one, so map from scratch.
    const u32 ip = static_cast<u32>(SO::GetHostID());
    if (ip != 0 && ip != ctx.localIp) {
        DropMapping();
        Begin(now);
        return;
    }
    StartTransaction(OP_RENEW, now);
}

static void RenewFailed(u64 now, Error error) {
    const u64 minDelay = SecToTicks(MIN_RENEW_DELAY_SEC);
    if (now + minDelay < ctx.expiresAt) {
        // Lease still running: stay active and try again after half of what is left.
        const u64 half = (ctx.expiresAt - now) / 2;
        ctx.renewAt = now + (half < minDelay ? minDelay : half);
        ctx.error = error;
        ctx.state = STATE_ACTIVE;
        return;
    }
    DropMapping();
    EnterBackoffProgressive(now, ERR_LEASE_LOST);
}

static void FinishRelease(bool confirmed) {
    DropMapping();
    if (confirmed)
        ctx.flags &= ~FLAG_RELEASE_UNCONFIRMED;
    else
        ctx.flags |= FLAG_RELEASE_UNCONFIRMED;
    ctx.state = sEnableRequest ? STATE_IDLE : STATE_DISABLED;
    PCP_LOG("PCP: released (%s)\n", confirmed ? "confirmed" : "unconfirmed");
}

// The running transaction failed. retrySec: wait time for a new MAP, 0 = progressive backoff.
static void FailTransaction(u64 now, Error error, u32 retrySec) {
    switch (ctx.op) {
        case OP_MAP:
            DropMapping();
            if (retrySec != 0)
                EnterBackoff(now, error, retrySec);
            else
                EnterBackoffProgressive(now, error);
            break;
        case OP_RENEW:
            RenewFailed(now, error);
            break;
        case OP_DELETE:
            FinishRelease(false);
            break;
    }
}

// The session is over (or the socket layer is gone). The descriptor number is kept: whoever uses
// it next checks it first, so a socket that survived is reused instead of leaked.
static void ForgetSession() {
    ctx.sockNeedsCheck = true;
    DropMapping();
    ctx.rejectResult = 0;
    ctx.backoffSec = BACKOFF_FIRST_SEC;
    ctx.quickRetryUsed = false;
    if (!ctx.fatal)
        ctx.state = sEnableRequest ? STATE_IDLE : STATE_DISABLED;
}

// Requests

static u32 CurrentTarget() {
    return ctx.op == OP_MAP ? ctx.candidates[ctx.candidateIdx] : ctx.gateway;
}

// MAP request, RFC 6887 figure 2 (header) + figure 9 (MAP).
static void BuildRequest(Op op) {
    u8 *p = sTx;
    memset(p, 0, MAP_SIZE);
    p[0] = PCP_VERSION;
    p[1] = OPCODE_MAP;
    WriteU32(p + 4, op == OP_DELETE ? 0 : PCP_REQUESTED_LIFETIME_SEC);
    p[18] = 0xff;  // client address ::ffff:a.b.c.d
    p[19] = 0xff;
    WriteU32(p + 20, ctx.localIp);
    memcpy(p + 24, ctx.nonce, 12);
    p[36] = PROTO_UDP;
    WriteU16(p + 40, ctx.internalPort);

    if (ctx.active) {
        // Renewal / delete: name what we have, so the router keeps (or removes) exactly that.
        WriteU16(p + 42, ctx.externalPort);
        memcpy(p + 44, ctx.externalAddr, 16);
    } else {
        // New mapping: external port 0 and address ::ffff:0.0.0.0 = no preference. Behind CGNAT
        // a specific wish may be refused; the port that counts is the one in the response.
#if !PCP_SUGGEST_V6_UNSPECIFIED
        p[54] = 0xff;
        p[55] = 0xff;
#endif
    }
}

static s32 SendRequest(Op op, u32 target) {
    BuildRequest(op);
    const SO::SockAddrIn to = MakeAddr(target, SERVER_PORT);
    return SO::SendTo(ctx.sock, sTx, MAP_SIZE, 0, &to);
}

// Delete without waiting for the answer, for when no further tick can be expected.
static void BestEffortDelete() {
    u32 target = 0;
    if (ctx.active || ctx.state == STATE_RELEASING)
        target = ctx.gateway;
    else if (ctx.state == STATE_PROBING && ctx.sendOk != 0)
        target = ctx.candidates[ctx.candidateIdx];  // a mapping may exist although no answer came
    if (target == 0)
        return;

    ctx.flags |= FLAG_RELEASE_UNCONFIRMED;
    if (CheckSocket() != SOCK_OURS)
        return;
    for (u32 i = 0; i < SHUTDOWN_DELETE_COPIES; ++i) {
        const s32 ret = SendRequest(OP_DELETE, target);
        if (ret < 0)
            ctx.soError = ret;
    }
}

// Responses

// Private, shared (CGNAT, RFC 6598), link-local, 0.x or loopback.
static bool IsNotPublic(u32 ip) {
    const u32 a = ip >> 24;
    const u32 b = (ip >> 16) & 0xff;
    return a == 10 || (a == 172 && b >= 16 && b <= 31) || (a == 192 && b == 168) || (a == 100 && b >= 64 && b <= 127) || (a == 169 && b == 254) || a == 0 || a == 127;
}

static void OnMapped(u64 now, u32 server, u32 lifetime, u16 externalPort, const u8 *externalAddr) {
    static const u8 V4_MAPPED_PREFIX[12] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xff, 0xff};
    const bool wasActive = ctx.active;
    const u64 oldExpiresAt = ctx.expiresAt;
    const u16 oldExternalPort = ctx.externalPort;

    if (lifetime > MAX_LIFETIME_SEC)
        lifetime = MAX_LIFETIME_SEC;

    ctx.gateway = server;
    ctx.active = true;
    ctx.externalPort = externalPort;
    memcpy(ctx.externalAddr, externalAddr, 16);
    ctx.lifetime = lifetime;
    ctx.expiresAt = now + SecToTicks(lifetime);
    ctx.error = ERR_NONE;
    ctx.rejectResult = 0;
    ctx.backoffSec = BACKOFF_FIRST_SEC;
    ctx.quickRetryUsed = false;
    ctx.state = STATE_ACTIVE;

    // Flags describe the current mapping; FLAG_EXT_PORT_CHANGED and FLAG_SOCKET_REPLACED are sticky.
    u32 flags = ctx.flags & ~(FLAG_EXT_PORT_DIFFERS | FLAG_EXT_ADDR_NOT_PUBLIC | FLAG_EXT_ADDR_UNKNOWN | FLAG_LEASE_SHORTENED | FLAG_LEASE_NOT_EXTENDED | FLAG_NAT_BEFORE_SERVER
                              | FLAG_FOREIGN_NONCE | FLAG_RELEASE_UNCONFIRMED);
    if (externalPort != ctx.internalPort)
        flags |= FLAG_EXT_PORT_DIFFERS;
    if (wasActive && externalPort != oldExternalPort)
        flags |= FLAG_EXT_PORT_CHANGED;
    if (lifetime < PCP_REQUESTED_LIFETIME_SEC)
        flags |= FLAG_LEASE_SHORTENED;

    if (memcmp(externalAddr, V4_MAPPED_PREFIX, 12) == 0) {
        ctx.externalIp = ReadU32(externalAddr + 12);
        // The router's WAN address is not public: another NAT (CGNAT or a second router) is in front.
        if (IsNotPublic(ctx.externalIp))
            flags |= FLAG_EXT_ADDR_NOT_PUBLIC;
    } else {
        ctx.externalIp = 0;
        flags |= FLAG_EXT_ADDR_UNKNOWN;
    }

    // Some routers answer a renewal with the time that is left instead of a new lifetime. Asking
    // again before the end cannot extend anything, so the next request goes out right after the
    // end, where the router creates the mapping anew.
    const bool notExtended = wasActive && ctx.op == OP_RENEW && ctx.expiresAt <= oldExpiresAt + SecToTicks(2);
    if (notExtended) {
        flags |= FLAG_LEASE_NOT_EXTENDED;
        ctx.renewAt = ctx.expiresAt + SecToTicks(1);
    } else {
        const u32 delay = lifetime / 2;
        ctx.renewAt = now + SecToTicks(delay < MIN_RENEW_DELAY_SEC ? MIN_RENEW_DELAY_SEC : delay);
    }
    ctx.flags = flags;

    PCP_LOG("PCP: %s, port %u -> external %u, lifetime %u s, flags %x\n", wasActive ? "renewed" : "mapped", ctx.internalPort, externalPort, lifetime, flags);
}

static void OnServerError(u64 now, u32 server, u32 result, u32 retryAfter) {
    ctx.gateway = server;  // whoever answers with our nonce is the PCP server, also when it refuses
    ctx.rejectResult = static_cast<s32>(result);
    PCP_LOG("PCP: router %u.%u.%u.%u answered result %u (%s)\n", PCP_IP4(server), result, GetResultName(static_cast<s32>(result)));
    if (result == 12)  // ADDRESS_MISMATCH
        ctx.flags |= FLAG_NAT_BEFORE_SERVER;
    if (result == 2)  // NOT_AUTHORIZED
        ctx.flags |= FLAG_FOREIGN_NONCE;
    // For errors the lifetime field says how long to wait before asking again.
    FailTransaction(now, ERR_SERVER_RESULT, Clamp(retryAfter, SERVER_RETRY_MIN_SEC, SERVER_RETRY_MAX_SEC));
}

// Only addresses this transaction has actually sent to.
static bool IsExpectedServer(u32 addr) {
    if (ctx.op != OP_MAP)
        return addr == ctx.gateway;
    for (u32 i = 0; i <= ctx.candidateIdx && i < ctx.candidateCount; ++i)
        if (ctx.candidates[i] == addr)
            return true;
    return false;
}

// true if the datagram ended the running transaction.
static bool HandleDatagram(u64 now, const u8 *p, s32 size, const SO::SockAddrIn &from) {
    if (from.port != SERVER_PORT || !IsExpectedServer(from.addr))
        return false;

    // Too short or another version: how a NAT-PMP-only or PCPv1 gateway answers. No nonce, so it
    // does not end the transaction; it only names the reason if the transaction times out.
    if (size < MAP_SIZE || p[0] != PCP_VERSION) {
        if (size >= 4 && (p[1] & 0x80) != 0)
            ctx.otherVersionSeen = true;
        return false;
    }
    if ((p[1] & 0x80) == 0 || (p[1] & 0x7f) != OPCODE_MAP)
        return false;

    // Nothing else is trusted before the nonce matches (error responses echo it too).
    if (memcmp(p + 24, ctx.nonce, 12) != 0)
        return false;

    const u32 result = p[3];
    const u32 lifetime = ReadU32(p + 4);
    ctx.pcpResult = static_cast<s32>(result);
    if (result != 0) {
        OnServerError(now, from.addr, result, lifetime);
        return true;
    }

    if (p[36] != PROTO_UDP || ReadU16(p + 40) != ctx.internalPort)
        return false;
    const u16 externalPort = ReadU16(p + 42);

    if (ctx.op == OP_DELETE) {
        if (lifetime != 0)
            return false;  // late answer to an earlier MAP
        FinishRelease(true);
        return true;
    }
    if (lifetime == 0 || externalPort == 0)
        return false;  // late answer to an earlier delete, or nothing assigned

    OnMapped(now, from.addr, lifetime, externalPort, p + 44);
    return true;
}

// Transaction: receive, time out, (re)send. Per tick at most MAX_RX_PER_TICK polls + receives
// and one send.
// Polls and receives without waiting. true if a datagram ended the transaction.

static bool ReceiveAnswers(u64 now) {
    for (u32 i = 0; i < MAX_RX_PER_TICK; ++i) {
        SO::PollFD pfd;
        pfd.fd = ctx.sock;
        pfd.events = SO::POLLRDNORM;
        pfd.revents = 0;
        const s32 ready = SO::Poll(&pfd, 1, 0);
        if (ready < 0) {
            if (Classify(ready) != SOC_AGAIN)
                RecordSoError(ready);
            return false;
        }
        if (ready == 0)
            return false;
        if ((pfd.revents & (SO::POLLERR | SO::POLLHUP | SO::POLLNVAL)) != 0) {
            ctx.sockNeedsCheck = true;
            return false;
        }
        if ((pfd.revents & SO::POLLRDNORM) == 0)
            return false;

        SO::SockAddrIn from = MakeAddr(0, 0);
        from.family = 0;
        const s32 size = SO::RecvFrom(ctx.sock, sRx, sizeof(sRx), SO::RECV_FLAGS, &from);
        if (size < 0) {
            // SO_EAGAIN after a positive poll: the datagram is not there after all. Not an error.
            if (Classify(size) != SOC_AGAIN)
                RecordSoError(size);
            return false;
        }
        if (HandleDatagram(now, sRx, size, from))
            return true;
    }
    return false;
}

static void TransactionTimedOut(u64 now) {
    if (ctx.op == OP_MAP && ctx.candidateIdx + 1 < ctx.candidateCount) {
        // Next guess. Late answers of the earlier ones are still accepted.
        ++ctx.candidateIdx;
        ctx.attempt = 0;
        ctx.rtoDeadline = now;
        return;
    }
    const Error error = ctx.sendOk == 0 ? ERR_SEND : (ctx.otherVersionSeen ? ERR_NO_PCP_V2 : ERR_NO_RESPONSE);
    FailTransaction(now, error, 0);
}

static void TickTransaction(u64 now) {
    if (ctx.op == OP_MAP && ctx.candidateCount == 0) {
        // First tick of a MAP attempt: find the PCP server. Nothing is sent before.
        if (!ResolveGateway())
            EnterBackoffProgressive(now, ERR_NO_GATEWAY);
        return;
    }

    bool later;
    const Error sockError = EnsureSocket(later);
    if (sockError != ERR_NONE) {
        // A passing condition: ask again on the next ticks, but not for ever.
        if (later && ++ctx.sockWait <= MAX_SOCK_WAIT_TICKS)
            return;
        FailTransaction(now, sockError, 0);
        return;
    }
    ctx.sockWait = 0;

    if (ctx.sendOk != 0 && ReceiveAnswers(now))
        return;
    if (now < ctx.rtoDeadline)
        return;

    const Retry &retry = RETRY[ctx.op];
    if (ctx.attempt >= retry.attempts) {
        TransactionTimedOut(now);
        return;
    }

    // A failed send counts as an attempt as well.
    const s32 ret = SendRequest(ctx.op, CurrentTarget());
    if (ret < 0)
        RecordSoError(ret);
    else
        ++ctx.sendOk;
    ctx.rtoMs = ctx.attempt == 0 ? retry.firstMs : ctx.rtoMs * 2;
    ctx.rtoDeadline = now + MsToTicks(ctx.rtoMs);
    ++ctx.attempt;
}

// Public

void Tick() {
    if (!TryEnter())
        return;

    const u64 now = Now();
    if (ctx.lastTick != 0 && now - ctx.lastTick > MsToTicks(TICK_GAP_MS))
        ctx.sockNeedsCheck = true;
    ctx.lastTick = now;

    if (sPendingSessionLost) {
        sPendingSessionLost = false;
        const bool hadMapping = ctx.active;
        ForgetSession();
        if (hadMapping)
            ctx.flags |= FLAG_RELEASE_UNCONFIRMED;
    }

    const bool enabled = sEnableRequest;
    if (!ctx.fatal) {
        switch (ctx.state) {
            case STATE_DISABLED:
                if (enabled)
                    ctx.state = STATE_IDLE;
                break;
            case STATE_IDLE:
                if (enabled)
                    Begin(now);
                else
                    ctx.state = STATE_DISABLED;
                break;
            case STATE_BACKOFF:
                if (!enabled)
                    ctx.state = STATE_DISABLED;
                else if (now >= ctx.retryAt)
                    Begin(now);
                break;
            case STATE_PROBING:
                if (enabled) {
                    TickTransaction(now);
                } else {
                    BestEffortDelete();
                    DropMapping();
                    ctx.state = STATE_DISABLED;
                }
                break;
            case STATE_ACTIVE:
                if (!enabled)
                    StartTransaction(OP_DELETE, now);
                else if (now >= ctx.renewAt)
                    StartRenew(now);
                break;
            case STATE_RENEWING:
                if (enabled)
                    TickTransaction(now);
                else
                    StartTransaction(OP_DELETE, now);
                break;
            case STATE_RELEASING:
                TickTransaction(now);
                break;
        }
    }

    Publish();
    Leave();
}

void OnSessionEnd() {
    if (!TryEnter()) {
        sPendingSessionLost = true;
        return;
    }
    ctx.sockNeedsCheck = true;  // never trust the remembered descriptor here without a look
    if (ctx.state == STATE_PROBING || ctx.state == STATE_ACTIVE || ctx.state == STATE_RENEWING || ctx.state == STATE_RELEASING)
        BestEffortDelete();
    if (CheckSocket() == SOCK_OURS) {
        SO::Close(ctx.sock);
        ctx.sock = -1;
    }
    ForgetSession();
    PCP_LOG("PCP: session end\n");
    Publish();
    Leave();
}

void OnSessionLost() {
    if (!TryEnter()) {
        sPendingSessionLost = true;
        return;
    }
    const bool hadMapping = ctx.active;
    ForgetSession();
    if (hadMapping)
        ctx.flags |= FLAG_RELEASE_UNCONFIRMED;
    Publish();
    Leave();
}

void SetEnabled(bool enabled) {
    sEnableRequest = enabled;
}

bool IsEnabled() {
    return sEnableRequest;
}

void SetGatewayOverride(u32 ipv4) {
    sGatewayOverride = ipv4;
}

void GetStatus(Status &out) {
    const int irq = OS::DisableInterrupts();
    const bool valid = sHavePublished;
    out = sPublished;
    const u64 expiresAt = sPublishedExpiresAt;
    OS::RestoreInterrupts(irq);

    if (!valid) {  // no tick yet
        memset(&out, 0, sizeof(out));
        out.state = sEnableRequest ? STATE_IDLE : STATE_DISABLED;
        out.pcpResult = -1;
        return;
    }
    const u64 now = Now();
    if (out.active && expiresAt > now)
        out.secondsLeft = static_cast<u32>((expiresAt - now) / static_cast<u64>(OS::GetTimerClock()));
}

void GetUiStatus(UiStatus &out) {
    Status s;
    GetStatus(s);
    out.error = s.error;
    out.flags = s.flags;
    out.result = 0;
    if (s.active)
        out.state = UI_ACTIVE;
    else if (s.state == STATE_DISABLED && !sEnableRequest)
        out.state = UI_OFF;
    else if (s.error == ERR_SERVER_RESULT && s.rejectResult > 0) {
        out.state = UI_REJECTED;
        out.result = s.rejectResult;
    } else if (s.error == ERR_NO_RESPONSE || s.error == ERR_NO_PCP_V2)
        out.state = UI_NO_SERVER;
    else if (s.error != ERR_NONE)
        out.state = UI_FAILED;
    else
        out.state = UI_NEGOTIATING;
}

const char *GetResultName(s32 pcpResult) {
    // RFC 6887 section 7.4.
    static const char *const NAMES[] = {
        "SUCCESS",
        "UNSUPP_VERSION",
        "NOT_AUTHORIZED",
        "MALFORMED_REQUEST",
        "UNSUPP_OPCODE",
        "UNSUPP_OPTION",
        "MALFORMED_OPTION",
        "NETWORK_FAILURE",
        "NO_RESOURCES",
        "UNSUPP_PROTOCOL",
        "USER_EX_QUOTA",
        "CANNOT_PROVIDE_EXTERNAL",
        "ADDRESS_MISMATCH",
        "EXCESSIVE_REMOTE_PEERS",
    };
    if (pcpResult < 0)
        return "NO_RESULT";
    if (pcpResult >= static_cast<s32>(sizeof(NAMES) / sizeof(NAMES[0])))
        return "UNKNOWN";
    return NAMES[pcpResult];
}

void ReportHookFailure() {
    ctx.fatal = true;
    ctx.error = ERR_HOOK;
    ctx.state = STATE_DISABLED;
    Publish();
}

}  // namespace PCP
}  // namespace Network
}  // namespace Pulsar
