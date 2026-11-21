#ifndef _PUL_PCP_
#define _PUL_PCP_

#include <kamek.hpp>

namespace Pulsar {
namespace Network {
namespace PCP {

enum State {
    STATE_DISABLED = 0,  // switched off, or fatal error (see Status::error)
    STATE_IDLE,          // nothing requested yet
    STATE_PROBING,       // MAP request under way, no mapping yet
    STATE_ACTIVE,        // mapping granted, waiting for the renewal time
    STATE_RENEWING,      // mapping granted, renewal under way
    STATE_RELEASING,     // delete (lifetime 0) under way
    STATE_BACKOFF,       // last attempt failed, waiting for the retry time
};

// Own error classes. The router's result code is kept separately in Status::pcpResult.
enum Error {
    ERR_NONE = 0,
    ERR_HOOK = 1,           // entry hook not installed, Tick() is never called
    ERR_SERNO = 2,          // setting.txt unreadable or without SERNO: internal port unknown
    ERR_NO_LOCAL_IP = 3,    // SOGetHostID gave no usable address
    ERR_SOCKET = 4,         // socket/bind failed for another reason, see Status::soError
    ERR_SEND = 5,           // not a single request could be sent, see Status::soError
    ERR_NO_RESPONSE = 6,    // nobody answered on port 5351 (no PCP in the router, or switched off)
    ERR_NO_PCP_V2 = 7,      // the gateway answered, but only NAT-PMP / another PCP version
    ERR_SERVER_RESULT = 8,  // router answered with a result code != 0, see Status::pcpResult
    ERR_LEASE_LOST = 9,     // renewal failed until the lease ran out
    ERR_SO_DOWN = 10,       // socket library not started or restarting
    ERR_PORT_IN_USE = 11,   // somebody else holds the PCP client port
    ERR_NO_GATEWAY = 12,    // no override, no default route and no address to guess from; nothing sent
};

enum Flags {
    FLAG_EXT_PORT_DIFFERS = 1 << 0,     // external port != internal port (normal behind CGNAT)
    FLAG_EXT_ADDR_NOT_PUBLIC = 1 << 1,  // external address is private/shared: CGNAT or double NAT upstream
    FLAG_LEASE_SHORTENED = 1 << 2,      // router granted less than requested
    FLAG_LEASE_NOT_EXTENDED = 1 << 3,   // router answers a renewal with the remaining time
    FLAG_EXT_PORT_CHANGED = 1 << 4,     // external port changed during the session (sticky)
    FLAG_NAT_BEFORE_SERVER = 1 << 5,    // ADDRESS_MISMATCH: a NAT sits between console and PCP server
    FLAG_FOREIGN_NONCE = 1 << 6,        // NOT_AUTHORIZED: port probably mapped under another nonce (launcher?)
    FLAG_RELEASE_UNCONFIRMED = 1 << 7,  // delete sent but not confirmed (or not sent at all)
    FLAG_SOCKET_REPLACED = 1 << 8,      // our socket vanished (SOCleanup) and was reopened (sticky)
    FLAG_NONBLOCK_FAILED = 1 << 9,      // SOFcntl failed; harmless, receiving is poll-gated
    FLAG_EXT_ADDR_UNKNOWN = 1 << 10,    // external address in the response is not IPv4-mapped
    FLAG_IFOPT_REJECTED = 1 << 11,      // PCP_USE_INTERFACE_OPT: address table implausible, netmask unknown
    FLAG_GATEWAY_UNCHECKED = 1 << 12,   // gateway from the routing table used without subnet check
    FLAG_GATEWAY_GUESSED = 1 << 13,     // routing table unusable (reason in the log): addresses guessed
};

struct Status {
    State state;
    Error error;
    s32 pcpResult;      // last RFC 6887 result code with a matching nonce, -1 = none yet
    s32 soError;        // last negative return value of an SO call, 0 = none
    bool active;        // a mapping is currently granted
    u16 internalPort;   // 22000 + SERNO % 1000, 0 = unknown
    u16 externalPort;   // assigned by the router, 0 = none
    u32 externalIp;     // IPv4, host order, 0 = unknown
    u32 gatewayIp;      // PCP server that answered, 0 = none yet
    u32 localIp;
    u32 lifetime;       // seconds granted by the last successful response
    u32 secondsLeft;    // until the lease runs out (computed in GetStatus)
    u32 flags;          // Flags
    u32 serno;
    s32 rejectResult;   // result code of the last refusal, 0 after a success or session end
};

// Drives the state machine. Call once per iteration of the network loop.
void Tick();

// The session ends while the socket library is STILL usable (entry of DWC_ShutdownFriendsMatch):
// sends the delete without waiting for the answer and closes the socket.
void OnSessionEnd();

// The session is gone and the socket library must not be touched any more. No SO call; the
// mapping is left to expire. The descriptor is checked before it is ever used again.
void OnSessionLost();

// Runtime switch (settings). Disabling deletes an active mapping (with confirmation).
void SetEnabled(bool enabled);
bool IsEnabled();

// 0 = automatic (routing table, then guesses). Otherwise this is the only PCP server asked.
// Takes effect at the next MAP attempt.
void SetGatewayOverride(u32 ipv4);

// Thread-safe snapshot for a status display.
void GetStatus(Status &out);

// Reduced view for the UI.
enum UiState {
    UI_OFF = 0,
    UI_NEGOTIATING,
    UI_ACTIVE,
    UI_REJECTED,   // router refused, result in UiStatus::result
    UI_FAILED,
    UI_NO_SERVER,  // no PCP server answered
};

struct UiStatus {
    UiState state;
    Error error;
    s32 result;  // UI_REJECTED: the router's result code, otherwise 0
    u32 flags;
};

void GetUiStatus(UiStatus &out);

// RFC 6887 name of a result code ("SUCCESS", "NOT_AUTHORIZED", ...).
const char *GetResultName(s32 pcpResult);

// Called by PCPHook.cpp if Tick() can never run.
void ReportHookFailure();

// Reads only bytes [0, len) of table, and only after checking 0 < len <= room.
// Entries: 24 bytes, big endian {dest, netmask, gateway, flags, u64}.
enum RouteCode {
    ROUTE_OK = 0,
    ROUTE_CALL_FAILED,   // ret < 0: the buffer was not looked at
    ROUTE_BAD_LENGTH,    // len <= 0, len > room, or not a multiple of 24
    ROUTE_NOT_ANSWERED,  // len == room and every byte 0: a Dolphin without case 0x4006
    ROUTE_NO_DEFAULT,    // no entry with dest 0, netmask 0, gateway != 0 and (flags & 1)
    ROUTE_IMPLAUSIBLE,   // default route(s) present, none plausible
};
struct RouteResult {
    RouteCode code;
    u32 gateway;      // chosen gateway, 0 unless ROUTE_OK
    u32 entries;      // whole entries in the answer
    u32 defaults;     // number of default routes
    u32 index;        // entry of the chosen route
    u32 flags;        // its flags
    u32 rejected;     // ROUTE_IMPLAUSIBLE: gateway of the first refused default route
    const char *why;  // ROUTE_IMPLAUSIBLE: reason for that refusal, otherwise nullptr
};
// netmask 0 = unknown: subnet checks skipped. The first default route that passes the checks wins.
// Refused: 0.x, 127.x, 224.0.0.0 and above, the own address, and with a netmask: outside the own
// subnet, or its network or broadcast address.
RouteResult ParseRouteTable(const void *table, s32 room, s32 ret, s32 len, u32 localIp, u32 netmask);

}  // namespace PCP
}  // namespace Network
}  // namespace Pulsar

#endif
