#include <UI/PlayerCount.hpp>
#include <Settings/Settings.hpp>
#include <Network/Json.hpp>
#include <Network/NHTTPHelper.hpp>
#include <core/rvl/DWC/NHTTP.hpp>
#include <core/rvl/NHTTP/NHTTP.hpp>

typedef void *ServerBrowser;

extern "C" int ServerBrowserLimitUpdateA(ServerBrowser sb, bool async,
                                         bool disconnectOnComplete,
                                         const unsigned char *basicFields,
                                         int numBasicFields,
                                         const char *serverFilter, int maxServers);
extern "C" void ServerBrowserFree(ServerBrowser sb);

extern "C" void qr2_register_keyA(int keyid, const char *key);

extern "C" int DWCi_QR2Startup(u32 profileID);
extern "C" void DWC_SetReportLevel(u32 level);
static float hookLocalTimer = 0.0f;
static bool hasRKNetRequestFinished = true;
static bool playerCountRequestActive = false;
static bool nhttpStarted = false;
static void *playerCountWorkBuf = nullptr;
static const u32 playerCountWorkBufSize = 0x20000;
static const char playerCountUrl[] = "http://rwfc.net/api/wfc/mkw_stats";

static bool IsCompetitiveMatchmakingEnabled() {
    const u8 timeoutSetting = Pulsar::Settings::Mgr::Get().GetSettingValue(Pulsar::Settings::SETTING_INFINITEMATCHMAKINGTIMEOUT);
    return timeoutSetting == Pulsar::MATCHMAKINGTIMEOUT_INFINITE;
}

static const char *GetCompetitiveServerFilter(const char *serverFilter, char *expandedFilter, u32 filterSize) {
    if (!IsCompetitiveMatchmakingEnabled() || serverFilter == nullptr) return serverFilter;

    const char *suspendNeedle = "dwc_suspend = 0";
    const char *suspendPos = strstr(serverFilter, suspendNeedle);
    if (suspendPos == nullptr || strstr(serverFilter, "dwc_hoststate") == nullptr || strstr(serverFilter, "dwc_pid !=") == nullptr) {
        return serverFilter;
    }

    const int prefixLength = suspendPos - serverFilter;
    const char *suffix = suspendPos + strlen(suspendNeedle);
    const int written = snprintf(
        expandedFilter,
        filterSize,
        "%.*s(dwc_suspend = 0 or numplayers < 11)%s",
        prefixLength,
        serverFilter,
        suffix);
    if (written <= 0 || written >= static_cast<int>(filterSize)) return serverFilter;

    return expandedFilter;
}

static int RR_numPlayers150cc = 0;
static int RR_numPlayersCT = 0;
static int RR_numPlayersRT = 0;

static int RR_numPlayers200cc = 0;
static int RR_numPlayersOTT = 0;
static int RR_numPlayersIR = 0;

static int BT_numPlayersRegular = 0;
static int BT_numPlayersELIM = 0;

static int RR_numPlayersRegular = 0;
static int RR_numPlayersOthers = 0;
static int RR_numPlayersTotal = 0;

void PlayerCount::GetNumbersMain(int &nRetro, int &nCT, int &nRT) {
    nRetro = RR_numPlayers150cc;
    nCT = RR_numPlayersCT;
    nRT = RR_numPlayersRT;
}

void PlayerCount::GetNumbersOther(int &n200, int &nOtt, int &nIR) {
    n200 = RR_numPlayers200cc;
    nOtt = RR_numPlayersOTT;
    nIR = RR_numPlayersIR;
}

void PlayerCount::GetNumbersBT(int &nBattle, int &nBattleELIM) {
    nBattle = BT_numPlayersRegular;
    nBattleELIM = BT_numPlayersELIM;
}

void PlayerCount::GetNumbersRegular(int &nRegular) {
    nRegular = RR_numPlayersRegular;
}

void PlayerCount::GetNumbersTotal(int &nTotal) {
    nTotal = RR_numPlayersTotal;
}

void PlayerCount::GetNumbersOthers(int &nOthers) {
    nOthers = RR_numPlayersOthers;
}

static bool GetOnlineCount(const char *json, const char *objectName, u32 &online) {
    char objectPattern[32];
    snprintf(objectPattern, sizeof(objectPattern), "\"%s\"", objectName);

    const char *objectNamePos = strstr(json, objectPattern);
    if (objectNamePos == nullptr) return false;

    const char *objectStart = strchr(objectNamePos + strlen(objectPattern), '{');
    if (objectStart == nullptr) return false;

    const char *objectEnd = strchr(objectStart + 1, '}');
    if (objectEnd == nullptr) return false;

    const char *onlinePos = strstr(objectStart, "\"online\"");
    if (onlinePos == nullptr || onlinePos >= objectEnd) return false;

    const char *colon = strchr(onlinePos + sizeof("\"online\"") - 1, ':');
    if (colon == nullptr || colon >= objectEnd) return false;

    const char *valuePos = colon + 1;
    return Pulsar::Network::Json::ParseU32(valuePos, objectEnd, online);
}

static int GetRegionOnlineCount(const char *json, const char *region) {
    u32 online = 0;
    GetOnlineCount(json, region, online);
    return static_cast<int>(online);
}

static bool ParsePlayerCountResponse(const char *json) {
    if (json == nullptr) return false;

    u32 totalPlayers = 0;
    if (!GetOnlineCount(json, "global", totalPlayers)) return false;

    RR_numPlayers150cc = GetRegionOnlineCount(json, "vs_10");
    RR_numPlayers200cc = GetRegionOnlineCount(json, "vs_12");
    RR_numPlayersOTT = GetRegionOnlineCount(json, "vs_11");
    RR_numPlayersIR = GetRegionOnlineCount(json, "vs_13");
    RR_numPlayersCT = GetRegionOnlineCount(json, "vs_20");
    RR_numPlayersRT = GetRegionOnlineCount(json, "vs_21");
    RR_numPlayersRegular = GetRegionOnlineCount(json, "vs");
    BT_numPlayersRegular = GetRegionOnlineCount(json, "bt_14");
    BT_numPlayersELIM = GetRegionOnlineCount(json, "bt_15");
    RR_numPlayersOthers = GetRegionOnlineCount(json, "unknown");
    RR_numPlayersTotal = static_cast<int>(totalPlayers);
    return true;
}

static void OnPlayerCountReceived(s32 result, void *response, void *userdata) {
    Pulsar::Network::FinishNHTTPRequest();
    playerCountRequestActive = false;

    if (response == nullptr) return;
    if (result != 0) {
        NHTTPDestroyResponse(response);
        return;
    }

    char *body = nullptr;
    const int bodyLen = NHTTP::GetBodyAll(reinterpret_cast<NHTTP::Res *>(response), &body);
    if (body == nullptr || bodyLen <= 0) {
        NHTTPDestroyResponse(response);
        return;
    }

    const u32 responseBufSize = static_cast<u32>(bodyLen) + 1;
    char *responseBuf = reinterpret_cast<char *>(Pulsar::Network::NHTTPAlloc(responseBufSize, 4));
    if (responseBuf == nullptr) {
        NHTTPDestroyResponse(response);
        return;
    }

    memcpy(responseBuf, body, bodyLen);
    responseBuf[bodyLen] = '\0';
    NHTTPDestroyResponse(response);

    ParsePlayerCountResponse(responseBuf);
    Pulsar::Network::NHTTPFree(responseBuf);
}

static void StartPlayerCountRequest() {
    if (playerCountRequestActive) return;
    if (!Pulsar::Network::PreparePersistentNHTTPRequest(nhttpStarted)) return;

    if (playerCountWorkBuf == nullptr) {
        playerCountWorkBuf = Pulsar::Network::NHTTPAlloc(playerCountWorkBufSize, 0x20);
        if (playerCountWorkBuf == nullptr) return;
    }
    memset(playerCountWorkBuf, 0, playerCountWorkBufSize);

    void *request = NHTTPCreateRequest(playerCountUrl, 0, playerCountWorkBuf, playerCountWorkBufSize,
                                       reinterpret_cast<void *>(&OnPlayerCountReceived), nullptr);
    if (request == nullptr) return;

    playerCountRequestActive = true;
    const s32 sendRet = NHTTPSendRequestAsync(request);
    if (sendRet < 0) {
        nhttpStarted = false;
        playerCountRequestActive = false;
        return;
    }

    Pulsar::Network::MarkNHTTPRequestActive();
}

bool hasQR2Initialized = false;
int hook_QR2Startup(u32 id) {
    int res = DWCi_QR2Startup(id);

    qr2_register_keyA(0x32, "dwc_pid");
    qr2_register_keyA(0x33, "dwc_mtype");
    qr2_register_keyA(0x34, "dwc_mver");
    qr2_register_keyA(0x35, "dwc_eval");
    qr2_register_keyA(0x36, "dwc_groupid");
    qr2_register_keyA(0x37, "dwc_hoststate");
    qr2_register_keyA(0x38, "dwc_suspend");
    qr2_register_keyA(0x64, "rk");
    qr2_register_keyA(0x65, "ev");
    qr2_register_keyA(0x66, "eb");
    qr2_register_keyA(0x67, "p");

    hasQR2Initialized = true;

    return res;
}

int hook_ServerBrowserLimitUpdateA(ServerBrowser sb, bool async,
                                   bool disconnectOnComplete,
                                   const unsigned char *basicFields,
                                   int numBasicFields,
                                   const char *serverFilter, int maxServers) {
    hasRKNetRequestFinished = false;

    unsigned char newFields[64];
    int newNumFields = 0;

    if (basicFields && numBasicFields > 0) {
        newNumFields = numBasicFields;
        if (newNumFields > 60) newNumFields = 60;
        for (int i = 0; i < newNumFields; i++) {
            newFields[i] = basicFields[i];
        }
    }

    bool hasEV = false;
    bool hasEB = false;
    for (int i = 0; i < newNumFields; i++) {
        if (newFields[i] == 0x65) hasEV = true;
        if (newFields[i] == 0x66) hasEB = true;
    }

    if (!hasEV) newFields[newNumFields++] = 0x65;
    if (!hasEB) newFields[newNumFields++] = 0x66;

    char expandedFilter[0x100];
    const char *updatedServerFilter = GetCompetitiveServerFilter(serverFilter, expandedFilter, sizeof(expandedFilter));

    int res = ServerBrowserLimitUpdateA(
        sb,
        async,
        disconnectOnComplete,
        newFields,
        newNumFields,
        updatedServerFilter,
        maxServers);

    return res;
}

void hook_ServerBrowserFree(ServerBrowser sb) {
    hasRKNetRequestFinished = true;
    ServerBrowserFree(sb);
}

void hook_DWC_SetReportLevel(u32 level) {
#ifndef PROD
    DWC_SetReportLevel(0xffffffff);
#else
    DWC_SetReportLevel(level);
#endif
}

void hook_Section_calc(Section *_this) {
    _this->UpdateLayers();

    hookLocalTimer += 1.0f / 60.0f;

    bool isConnectionIdle = RKNet::Controller::sInstance &&
                            RKNet::Controller::sInstance->GetConnectionState() == RKNet::CONNECTIONSTATE_IDLE;

    if (hasQR2Initialized && !playerCountRequestActive && hasRKNetRequestFinished && hookLocalTimer >= 5.0f &&
        SectionMgr::sInstance->curSection->pages[Pages::Globe::id] && DWC::MatchControl::sInstance && isConnectionIdle) {
        hookLocalTimer = 0.0f;
        StartPlayerCountRequest();
    }
}

kmCall(0x800d0584, hook_QR2Startup);
kmCall(0x800d413c, hook_QR2Startup);
kmCall(0x800d5484, hook_QR2Startup);
kmCall(0x800d56bc, hook_QR2Startup);
kmCall(0x800d605c, hook_QR2Startup);
kmCall(0x800d62c0, hook_QR2Startup);

kmCall(0x800db908, hook_ServerBrowserLimitUpdateA);
kmCall(0x800d1058, hook_ServerBrowserFree);
kmCall(0x800d839c, hook_ServerBrowserFree);
kmCall(0x800db46c, hook_ServerBrowserFree);
kmCall(0x80658be8, hook_DWC_SetReportLevel);
kmCall(0x80622514, hook_Section_calc);
