#include <UI/VRLeaderboard/VRLeaderboard.hpp>

#include <Network/WiiLink.hpp>
#include <UI/UI.hpp>

#include <MarioKartWii/Archive/ArchiveMgr.hpp>
#include <MarioKartWii/Audio/RSARPlayer.hpp>
#include <MarioKartWii/File/BMG.hpp>
#include <MarioKartWii/RKNet/FriendMgr.hpp>
#include <MarioKartWii/UI/Section/SectionMgr.hpp>
#include <MarioKartWii/RKSYS/RKSYSMgr.hpp>
#include <MarioKartWii/RKSYS/LicenseMgr.hpp>
#include <MarioKartWii/System/Friend.hpp>
#include <MarioKartWii/System/Identifiers.hpp>
#include <MarioKartWii/Mii/Mii.hpp>
#include <core/RK/RKSystem.hpp>
#include <core/egg/mem/Heap.hpp>
#include <core/rvl/DWC/DWCAccount.hpp>
#include <core/rvl/NHTTP/NHTTP.hpp>
#include <core/rvl/OS/OS.hpp>
#include <core/nw4r/lyt/TextBox.hpp>
#include <include/c_stdio.h>
#include <include/c_string.h>
#include <hooks.hpp>

kmWrite32(0x800c9980, 0x4800000c);  // b 0x800c998c

void* NHTTPCreateRequest(const char* url, int param_2, void* buffer, u32 length, void* callback, void* userdata);
s32 NHTTPSendRequestAsync(void* request);
s32 NHTTPDestroyResponse(void* response);
s32 NHTTPSetRootCADefault();
s32 NHTTPSetVerifyOption(u32 option);
s32 NHTTPStartup(void* alloc, void* free, u32 param_3);

static void NHTTPConfigureHttpsForRequest(void* request) {
    if (request == nullptr) return;
    typedef s32 (*Fn)(void*, ...);
    (reinterpret_cast<Fn>(&NHTTPSetRootCADefault))(request);
    (reinterpret_cast<Fn>(&NHTTPSetVerifyOption))(request, 1);
}

namespace Pulsar {
namespace UI {

static void BMGHolderLoadWithFallback(BMGHolder* self, const char* name) {
    if (self == nullptr) return;

    self->bmgFile = nullptr;
    self->info = nullptr;
    self->data = nullptr;
    self->str1Block = nullptr;
    self->messageIds = nullptr;

    if (name == nullptr) return;
    ArchiveMgr* archiveMgr = ArchiveMgr::sInstance;
    if (archiveMgr == nullptr) return;

    char path[96];
    snprintf(path, sizeof(path), "message/%s.bmg", name);

    void* file = archiveMgr->GetFile(ARCHIVE_HOLDER_UI, path, nullptr);
    if (file == nullptr) {
        file = archiveMgr->GetFile(ARCHIVE_HOLDER_UI, "message/Common.bmg", nullptr);
    }
    if (file == nullptr) return;

    self->Init(*reinterpret_cast<const BMGHeader*>(file));
}
kmBranch(0x805f8b90, BMGHolderLoadWithFallback);

static void* NHTTPAllocFromEggHeap(u32 size, s32 align) {
    EGG::Heap* heap = RKSystem::mInstance.EGGSystem;
    if (heap == nullptr) return nullptr;
    if (align < 4) align = 4;
    return EGG::Heap::alloc(size, align, heap);
}

static void NHTTPFreeFromEggHeap(void* ptr) {
    if (ptr == nullptr) return;
    EGG::Heap* heap = RKSystem::mInstance.EGGSystem;
    if (heap != nullptr) EGG::Heap::free(ptr, heap);
}
kmBranch(0x800ed69c, NHTTPAllocFromEggHeap);
kmBranch(0x800ed6b4, NHTTPFreeFromEggHeap);

VRLeaderboardPage::FetchState VRLeaderboardPage::s_fetchState = VRLeaderboardPage::FETCH_IDLE;
bool VRLeaderboardPage::s_hasApplied = false;
VRLeaderboardPage::Entry VRLeaderboardPage::s_entries[VRLeaderboardPage::kMaxEntries];
char VRLeaderboardPage::s_responseBuf[65536];

static wchar_t s_statusText[128];
static wchar_t s_rowTextLoading[] = L"Loading...";
static wchar_t s_rowTextDash[] = L"----";
static wchar_t s_rowLabelVR[] = L"VR";
static wchar_t s_rowLabelScroll[] = L"Use left and right on the D-Pad to navigate pages.";
static wchar_t s_rowBlank[] = L"";
static wchar_t s_positionText[8];
static int s_lastLoggedState = -1;
static u64 s_requestStartTime = 0;
static bool s_nhttpStarted = false;
static const u32 s_nhttpWorkBufSize = 0x20000;
static u32 s_requestGeneration = 0;
static u64 s_currentUserFriendCode = 0;
static u32 s_entrySoundFrameCounter = 0;

static const u32 s_requestTimeoutMs = 45000;

struct NHTTPRequestCtx {
    u32 generation;
    void* workBuf;
    u32 workBufSize;
};

static bool HasPane(const LayoutUIControl& control, const char* paneName) {
    if (paneName == nullptr) return false;
    return control.layout.GetPaneByName(paneName) != nullptr;
}

static void SetTextBoxIfPresent(LayoutUIControl& control, const char* paneName, u32 bmgId, Text::Info* info) {
    if (HasPane(control, paneName)) control.SetTextBoxMessage(paneName, bmgId, info);
}

static void SetPaneVisibleIfPresent(LayoutUIControl& control, const char* paneName, bool visible) {
    if (HasPane(control, paneName)) control.SetPaneVisibility(paneName, visible);
}

static void CopyAsciiToWide(wchar_t* dst, size_t dstLen, const char* src) {
    if (dst == nullptr || dstLen == 0) return;
    if (src == nullptr) {
        dst[0] = L'\0';
        return;
    }
    size_t out = 0;
    for (; out + 1 < dstLen && src[out] != '\0'; ++out) {
        const unsigned char c = static_cast<unsigned char>(src[out]);
        dst[out] = (c < 0x80) ? static_cast<wchar_t>(c) : L'?';
    }
    dst[out] = L'\0';
}

static const char* FindStr(const char* haystack, const char* needle) {
    if (haystack == nullptr || needle == nullptr) return nullptr;
    const size_t needleLen = strlen(needle);
    if (needleLen == 0) return haystack;
    for (const char* p = haystack; *p != '\0'; ++p) {
        if (strncmp(p, needle, needleLen) == 0) return p;
    }
    return nullptr;
}

static const char* SkipWhitespace(const char* p) {
    while (p != nullptr && (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t')) ++p;
    return p;
}

static unsigned char ParseJsonEscape(const char*& p) {
    const unsigned char esc = static_cast<unsigned char>(*p++);
    if (esc == '\0') return '?';
    switch (esc) {
        case '"':
        case '\\':
        case '/':
            return esc;
        case 'b':
            return '\b';
        case 'f':
            return '\f';
        case 'n':
            return '\n';
        case 'r':
            return '\r';
        case 't':
            return '\t';
        case 'u':
            for (int i = 0; i < 4 && *p != '\0'; ++i) ++p;
            return '?';
        default:
            return '?';
    }
}

static const char* ParseJsonStringIntoWide(const char* p, wchar_t* out, size_t outLen) {
    if (out == nullptr || outLen == 0) return nullptr;
    out[0] = L'\0';
    p = SkipWhitespace(p);
    if (p == nullptr || *p != '"') return nullptr;
    ++p;

    size_t o = 0;
    while (*p != '\0' && *p != '"') {
        unsigned char c = static_cast<unsigned char>(*p++);
        if (c == '\\') c = ParseJsonEscape(p);
        if (o + 1 < outLen) out[o++] = (c < 0x80) ? static_cast<wchar_t>(c) : L'?';
    }
    if (*p == '"') ++p;
    out[o] = L'\0';
    return p;
}

template <typename T>
static const char* ParseJsonUInt(const char* p, T& out) {
    out = 0;
    p = SkipWhitespace(p);
    if (p == nullptr || *p == '-' || *p < '0' || *p > '9') return nullptr;
    while (*p >= '0' && *p <= '9') {
        out = out * 10 + static_cast<T>(*p - '0');
        ++p;
    }
    return p;
}

static const char* ParseJsonU32(const char* p, u32& out) {
    return ParseJsonUInt(p, out);
}

static const char* ParseJsonU64(const char* p, u64& out) {
    return ParseJsonUInt(p, out);
}

static inline int MinInt(int a, int b) { return (a < b) ? a : b; }

static const char* ParseJsonStringIntoAscii(const char* p, char* out, size_t outLen) {
    if (out == nullptr || outLen == 0) return nullptr;
    out[0] = '\0';
    p = SkipWhitespace(p);
    if (p == nullptr || *p != '"') return nullptr;
    ++p;

    size_t o = 0;
    while (*p != '\0' && *p != '"') {
        unsigned char c = static_cast<unsigned char>(*p++);
        if (c == '\\') c = ParseJsonEscape(p);
        if (o + 1 < outLen) out[o++] = static_cast<char>(c);
    }
    if (*p == '"') ++p;
    out[o] = '\0';
    return p;
}

static int Base64CharValue(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    if (c == '=') return -2;  // padding
    return -1;
}

static int DecodeBase64(const char* in, u8* out, int outCap) {
    if (in == nullptr || out == nullptr || outCap <= 0) return 0;

    int outLen = 0;
    int buf[4];
    int bufCount = 0;

    for (const char* p = in; *p != '\0'; ++p) {
        const char c = *p;
        if (c == ' ' || c == '\n' || c == '\r' || c == '\t') continue;

        const int v = Base64CharValue(c);
        if (v == -1) continue;
        buf[bufCount++] = v;
        if (bufCount != 4) continue;

        const int v0 = buf[0];
        const int v1 = buf[1];
        const int v2 = buf[2];
        const int v3 = buf[3];

        if (v0 < 0 || v1 < 0) break;
        const u32 triple = (static_cast<u32>(v0) << 18) | (static_cast<u32>(v1) << 12) |
                           (static_cast<u32>((v2 < 0) ? 0 : v2) << 6) | (static_cast<u32>((v3 < 0) ? 0 : v3));

        if (outLen < outCap) out[outLen++] = static_cast<u8>((triple >> 16) & 0xff);
        if (v2 != -2) {
            if (outLen < outCap) out[outLen++] = static_cast<u8>((triple >> 8) & 0xff);
        }
        if (v3 != -2) {
            if (outLen < outCap) out[outLen++] = static_cast<u8>(triple & 0xff);
        }

        bufCount = 0;
        if (v2 == -2 || v3 == -2) break;
    }

    return outLen;
}

static void ExtractMiiNameFromStoreData(const RFL::StoreData* storeData, wchar_t* outName, size_t outNameLen) {
    if (outName == nullptr || outNameLen == 0 || storeData == nullptr) {
        if (outName != nullptr && outNameLen > 0) outName[0] = L'\0';
        return;
    }

    size_t o = 0;
    for (int i = 0; i < 10 && o + 1 < outNameLen; ++i) {
        const u16 code = storeData->miiName[i];
        if (code == 0) break;
        outName[o++] = static_cast<wchar_t>(code);
    }
    outName[o] = L'\0';
}

static bool ExtractMiiNameFromStoreDataBase64(const char* b64, wchar_t* outName, size_t outNameLen) {
    if (outName == nullptr || outNameLen == 0 || b64 == nullptr || b64[0] == '\0') return false;

    RFL::StoreData storeData;
    if (DecodeBase64(b64, reinterpret_cast<u8*>(&storeData), static_cast<int>(sizeof(storeData))) < 0x16) return false;

    ExtractMiiNameFromStoreData(&storeData, outName, outNameLen);
    return outName[0] != L'\0';
}

static const char* FindStrInRange(const char* start, const char* end, const char* needle) {
    if (start == nullptr || end == nullptr || needle == nullptr) return nullptr;
    const size_t needleLen = strlen(needle);
    if (needleLen == 0) return start;
    for (const char* p = start; p + needleLen <= end; ++p) {
        if (strncmp(p, needle, needleLen) == 0) return p;
    }
    return nullptr;
}

static const char* FindMatchingObjectEnd(const char* objStart) {
    if (objStart == nullptr || *objStart != '{') return nullptr;
    int depth = 0;
    bool inString = false;
    bool escape = false;
    for (const char* p = objStart; *p != '\0'; ++p) {
        const char c = *p;
        if (inString) {
            if (escape) {
                escape = false;
                continue;
            }
            if (c == '\\') {
                escape = true;
                continue;
            }
            if (c == '"') inString = false;
            continue;
        }
        if (c == '"') {
            inString = true;
            continue;
        }
        if (c == '{') {
            ++depth;
        } else if (c == '}') {
            --depth;
            if (depth == 0) return p;
        }
    }
    return nullptr;
}

static bool IsFriendCodeInLicenseFriends(u64 friendCode) {
    if (friendCode == 0) return false;
    RKSYS::Mgr* rksysMgr = RKSYS::Mgr::sInstance;
    if (rksysMgr == nullptr || rksysMgr->curLicenseId < 0 || rksysMgr->curLicenseId >= 4) return false;

    RKSYS::LicenseFriends& licenseFriends = rksysMgr->licenses[rksysMgr->curLicenseId].GetFriends();
    for (u32 i = 0; i < 30; ++i) {
        if (licenseFriends.friends[i].friendCode == friendCode) return true;
    }
    return false;
}

VRLeaderboardPage::VRLeaderboardPage() {
    nextPageId = PAGE_NONE;
    curPage = 0;

    onBackButtonClickHandler.subject = this;
    onBackButtonClickHandler.ptmf = &VRLeaderboardPage::OnBackButtonClick;
    onBackPressHandler.subject = this;
    onBackPressHandler.ptmf = &VRLeaderboardPage::OnBackPress;

    titleText = new CtrlMenuPageTitleText;
    bottomText = new CtrlMenuInstructionText;
    backButton = new CtrlMenuBackButton;
    for (int i = 0; i < kRowsPerPage; ++i) {
        rows[i] = new LayoutUIControl;
    }
    miiGroup = new MiiGroup;
    controlsManipulatorManager = new ControlsManipulatorManager;

    controlsManipulatorManager->Init(1, false);
    this->SetManipulatorManager(*controlsManipulatorManager);
    controlsManipulatorManager->SetGlobalHandler(BACK_PRESS, onBackPressHandler, false, false);
}

VRLeaderboardPage::~VRLeaderboardPage() {
    delete titleText;
    delete bottomText;
    delete backButton;
    for (int i = 0; i < kRowsPerPage; ++i) {
        delete rows[i];
    }
    delete miiGroup;
    delete controlsManipulatorManager;
}

PageId VRLeaderboardPage::GetNextPage() const {
    return this->nextPageId;
}

void VRLeaderboardPage::OnInit() {
    this->InitControlGroup(13);

    miiGroup->Init(kRowsPerPage, 0x4, nullptr);

    this->AddControl(0, *titleText, 0);
    titleText->Load(0);

    this->AddControl(1, *bottomText, 0);
    bottomText->Load();

    this->AddControl(2, *backButton, 0);
    backButton->Load(UI::buttonFolder, "Back", "ButtonBack", 1, 0, false);
    backButton->SetOnClickHandler(onBackButtonClickHandler, 0);

    for (int i = 0; i < kRowsPerPage; ++i) {
        this->AddControl(3 + i, *rows[i], 0);

        ControlLoader loader(rows[i]);
        char variant[8];
        snprintf(variant, sizeof(variant), "rank%d", i + 1);
        static const char* noAnims[] = {nullptr};
        loader.Load("result", "ResultVS", variant, noAnims);

        SetPaneVisibleIfPresent(*rows[i], "handle_text", false);
        SetPaneVisibleIfPresent(*rows[i], "time", false);
    }

    ResetRowsToLoading();
}

void VRLeaderboardPage::OnActivate() {
    this->nextPageId = PAGE_NONE;
    this->curPage = 0;
    s_hasApplied = false;
    s_fetchState = FETCH_IDLE;
    s_nhttpStarted = false;
    ResetRowsToLoading();
    // OS::Report("[VRLeaderboard] OnActivate\n");
    // Play leaderboard loading sound effect
    this->PlaySound(SOUND_ID_BUTTON_SELECT, -1);
    StartFetch(this);
}

void VRLeaderboardPage::BeforeEntranceAnimations() {
    this->nextPageId = PAGE_NONE;
    backButton->SelectInitial(0);
}

void VRLeaderboardPage::OnUpdate() {
    if (s_fetchState != s_lastLoggedState) {
        // OS::Report("[VRLeaderboard] state=%d\n", static_cast<int>(s_fetchState));
        s_lastLoggedState = s_fetchState;
    }

    if (s_fetchState == FETCH_REQUESTING) {
        const u64 now = OS::GetTime();
        const u32 elapsedMs = OS::TicksToMilliseconds(now - s_requestStartTime);
        if (elapsedMs > s_requestTimeoutMs) {
            // OS::Report("[VRLeaderboard] timeout after %ums\n", elapsedMs);
            s_fetchState = FETCH_ERROR;
            s_hasApplied = false;
        }
    }

    if (s_fetchState == FETCH_READY && !s_hasApplied) {
        s_entrySoundFrameCounter = 0;  // Reset frame counter when applying results
        ApplyResults();
        s_hasApplied = true;
    } else if (s_fetchState == FETCH_ERROR && !s_hasApplied) {
        ApplyError("Failed to load leaderboard");
        s_hasApplied = true;
    }

    // Play staggered sounds for entries after results are applied (like VS Leaderboard)
    if (s_fetchState == FETCH_READY && s_hasApplied && s_entrySoundFrameCounter < kRowsPerPage) {
        const u32 targetFrame = s_entrySoundFrameCounter * 2;  // 2 frames per entry
        if (this->curStateDuration >= targetFrame) {
            Audio::RSARPlayer::PlaySoundById(SOUND_ID_SMALL_HIGH_NOTE, 0, this);
            ++s_entrySoundFrameCounter;
        }
    }

    if (s_fetchState == FETCH_READY && s_hasApplied) {
        const Input::RealControllerHolder* controllerHolder = nullptr;
        if (SectionMgr::sInstance != nullptr) controllerHolder = SectionMgr::sInstance->pad.padInfos[0].controllerHolder;
        if (controllerHolder != nullptr && controllerHolder->curController != nullptr) {
            const ControllerType controllerType = controllerHolder->curController->GetType();
            const u16 inputs = controllerHolder->inputStates[0].buttonRaw;
            const u16 newInputs = (inputs & ~controllerHolder->inputStates[1].buttonRaw);

            bool pageChanged = false;
            bool pageWentLeft = false;
            u16 leftButton = 0;
            u16 rightButton = 0;

            if (controllerType == CLASSIC) {
                leftButton = WPAD::WPAD_CL_TRIGGER_L | WPAD::WPAD_CL_BUTTON_LEFT;
                rightButton = WPAD::WPAD_CL_TRIGGER_R | WPAD::WPAD_CL_BUTTON_RIGHT;
            } else if (controllerType == WHEEL) {
                leftButton = WPAD::WPAD_BUTTON_UP;
                rightButton = WPAD::WPAD_BUTTON_DOWN;
            } else if (controllerType == NUNCHUCK) {
                leftButton = WPAD::WPAD_BUTTON_LEFT;
                rightButton = WPAD::WPAD_BUTTON_RIGHT;
            } else {
                leftButton = PAD::PAD_BUTTON_L | PAD::PAD_BUTTON_LEFT;
                rightButton = PAD::PAD_BUTTON_R | PAD::PAD_BUTTON_RIGHT;
            }

            if ((newInputs & leftButton) != 0 && curPage > 0) {
                --curPage;
                pageChanged = true;
                pageWentLeft = true;
            } else if ((newInputs & rightButton) != 0 && curPage + 1 < kPageCount) {
                ++curPage;
                pageChanged = true;
                pageWentLeft = false;
            }

            if (pageChanged) {
                this->PlaySound(pageWentLeft ? SOUND_ID_LEFT_ARROW_PRESS : SOUND_ID_RIGHT_ARROW_PRESS, -1);
                ApplyResults();
            }
        }
    }
}

void VRLeaderboardPage::OnBackPress(u32 /*hudSlotId*/) {
    this->nextPageId = PAGE_WFC_MAIN;
    this->EndStateAnimated(1, 0.0f);
}

void VRLeaderboardPage::OnBackButtonClick(PushButton& button, u32 /*hudSlotId*/) {
    this->nextPageId = PAGE_WFC_MAIN;
    this->EndStateAnimated(1, button.GetAnimationFrameSize());
}

void VRLeaderboardPage::ResetRowsToLoading() {
    for (int i = 0; i < kRowsPerPage; ++i) {
        Text::Info nameInfo;
        nameInfo.strings[0] = s_rowTextLoading;
        SetTextBoxIfPresent(*rows[i], "player_name", UI::BMG_TEXT, &nameInfo);

        Text::Info valueInfo;
        valueInfo.strings[0] = s_rowBlank;
        SetTextBoxIfPresent(*rows[i], "total_score", UI::BMG_TEXT, &valueInfo);
        SetTextBoxIfPresent(*rows[i], "total_point", UI::BMG_TEXT, &valueInfo);

        Text::Info labelInfo;
        labelInfo.strings[0] = s_rowBlank;
        SetTextBoxIfPresent(*rows[i], "position", UI::BMG_TEXT, &labelInfo);

        SetPaneVisibleIfPresent(*rows[i], "chara_icon", false);
        SetPaneVisibleIfPresent(*rows[i], "chara_icon_sha", false);
    }
}

void VRLeaderboardPage::ApplyResults() {
    const int base = static_cast<int>(curPage) * kRowsPerPage;
    for (int i = 0; i < kRowsPerPage; ++i) {
        const int idx = base + i;
        if (idx < 0 || idx >= kMaxEntries) continue;
        swprintf(s_positionText, sizeof(s_positionText) / sizeof(s_positionText[0]), L"#%d", idx + 1);
        swprintf(s_entries[idx].line, sizeof(s_entries[idx].line) / sizeof(s_entries[idx].line[0]), L"%ls", s_entries[idx].name);

        Text::Info nameInfo;
        nameInfo.strings[0] = s_entries[idx].line;
        SetTextBoxIfPresent(*rows[i], "player_name", UI::BMG_TEXT, &nameInfo);

        Text::Info posInfo;
        posInfo.strings[0] = s_positionText;
        SetTextBoxIfPresent(*rows[i], "position", UI::BMG_TEXT, &posInfo);

        swprintf(s_statusText, sizeof(s_statusText) / sizeof(s_statusText[0]), L"%u", s_entries[idx].vr);
        Text::Info valueInfo;
        valueInfo.strings[0] = s_statusText;
        SetTextBoxIfPresent(*rows[i], "total_score", UI::BMG_TEXT, &valueInfo);

        Text::Info labelInfo;
        labelInfo.strings[0] = s_rowLabelVR;
        SetTextBoxIfPresent(*rows[i], "total_point", UI::BMG_TEXT, &labelInfo);

        // Determine color: gold for current user, green for friends, white for others
        const bool isCurrentUser = (s_currentUserFriendCode != 0 && s_entries[idx].friendCode != 0 &&
                                    s_currentUserFriendCode == s_entries[idx].friendCode);
        bool isFriend = false;
        if (!isCurrentUser && s_entries[idx].friendCode != 0) {
            // Check DWC friends list via FriendMgr
            RKNet::FriendMgr* friendMgr = RKNet::FriendMgr::sInstance;
            if (friendMgr != nullptr && friendMgr->IsAvailable()) {
                const s32 friendIdx = friendMgr->GetFriendIdx(s_entries[idx].friendCode);
                isFriend = (friendIdx >= 0);
            }

            // Also check all licenses' friends lists from save file
            if (!isFriend) {
                isFriend = IsFriendCodeInLicenseFriends(s_entries[idx].friendCode);
            }
        }

        nw4r::ut::Color textColor;
        if (isCurrentUser) {
            // Gold color: RGB(255, 215, 0)
            textColor = nw4r::ut::Color(255, 215, 0, 255);
        } else if (isFriend) {
            // Green color: RGB(0, 255, 0)
            textColor = nw4r::ut::Color(0, 255, 0, 255);
        } else {
            // White (default)
            textColor = nw4r::ut::Color(255, 255, 255, 255);
        }

        // Apply text color to all text boxes
        const char* textBoxNames[] = {"player_name", "position", "total_score", "total_point"};
        for (int j = 0; j < 4; ++j) {
            nw4r::lyt::TextBox* textBox = reinterpret_cast<nw4r::lyt::TextBox*>(rows[i]->layout.GetPaneByName(textBoxNames[j]));
            if (textBox != nullptr) {
                textBox->color1[0] = textColor;
                textBox->color1[1] = textColor;
            }
        }

        miiGroup->LoadMii(i, &s_entries[idx].miiData);
        rows[i]->SetMiiPane("chara_icon", *miiGroup, i, 2);
        rows[i]->SetMiiPane("chara_icon_sha", *miiGroup, i, 2);
        SetPaneVisibleIfPresent(*rows[i], "chara_icon", true);
        SetPaneVisibleIfPresent(*rows[i], "chara_icon_sha", true);
    }

    Text::Info info;
    info.strings[0] = s_rowLabelScroll;
    bottomText->SetMessage(UI::BMG_TEXT, &info);
}

void VRLeaderboardPage::ApplyError(const char* msg) {
    CopyAsciiToWide(s_statusText, sizeof(s_statusText) / sizeof(s_statusText[0]), msg);

    Text::Info info;
    info.strings[0] = s_statusText;
    bottomText->SetMessage(UI::BMG_TEXT, &info);

    for (int i = 0; i < kRowsPerPage; ++i) {
        Text::Info info;
        info.strings[0] = s_rowTextDash;
        SetTextBoxIfPresent(*rows[i], "player_name", UI::BMG_TEXT, &info);

        Text::Info valueInfo;
        valueInfo.strings[0] = s_rowBlank;
        SetTextBoxIfPresent(*rows[i], "total_score", UI::BMG_TEXT, &valueInfo);
        SetTextBoxIfPresent(*rows[i], "total_point", UI::BMG_TEXT, &valueInfo);

        Text::Info labelInfo;
        labelInfo.strings[0] = s_rowBlank;
        SetTextBoxIfPresent(*rows[i], "position", UI::BMG_TEXT, &labelInfo);

        SetPaneVisibleIfPresent(*rows[i], "chara_icon", false);
        SetPaneVisibleIfPresent(*rows[i], "chara_icon_sha", false);
    }
}

void VRLeaderboardPage::StartFetch(VRLeaderboardPage* /*page*/) {
    if (s_fetchState == FETCH_REQUESTING) return;
    s_fetchState = FETCH_REQUESTING;
    s_hasApplied = false;
    s_lastLoggedState = -1;
    s_requestStartTime = OS::GetTime();
    ++s_requestGeneration;

    // Get current user's friend code
    s_currentUserFriendCode = 0;
    RKSYS::Mgr* rksysMgr = RKSYS::Mgr::sInstance;
    if (rksysMgr != nullptr && rksysMgr->curLicenseId >= 0) {
        RKSYS::LicenseMgr& license = rksysMgr->licenses[rksysMgr->curLicenseId];
        s_currentUserFriendCode = DWC::CreateFriendKey(&license.dwcAccUserData);
        // OS::Report("[VRLeaderboard] Current user friend code: %llu\n", s_currentUserFriendCode);
    }

    memset(s_entries, 0, sizeof(s_entries));
    memset(s_responseBuf, 0, sizeof(s_responseBuf));

    if (!s_nhttpStarted) {
        const s32 startupRet = NHTTPStartup(reinterpret_cast<void*>(&NHTTPAllocFromEggHeap),
                                            reinterpret_cast<void*>(&NHTTPFreeFromEggHeap),
                                            0x11);
        // OS::Report("[VRLeaderboard] NHTTPStartup ret=%d\n", startupRet);
        if (startupRet < 0) {
            s_fetchState = FETCH_ERROR;
            return;
        }
        s_nhttpStarted = true;
    }

    NHTTPRequestCtx* ctx = reinterpret_cast<NHTTPRequestCtx*>(NHTTPAllocFromEggHeap(sizeof(NHTTPRequestCtx), 0x20));
    if (ctx == nullptr) {
        s_fetchState = FETCH_ERROR;
        return;
    }
    ctx->generation = s_requestGeneration;
    ctx->workBufSize = s_nhttpWorkBufSize;
    ctx->workBuf = NHTTPAllocFromEggHeap(ctx->workBufSize, 0x20);
    // OS::Report("[VRLeaderboard] workBuf=%p workBufSize=%u gen=%u\n", ctx->workBuf, ctx->workBufSize, ctx->generation);
    if (ctx->workBuf == nullptr) {
        NHTTPFreeFromEggHeap(ctx);
        s_fetchState = FETCH_ERROR;
        return;
    }
    memset(ctx->workBuf, 0, ctx->workBufSize);

    char url[256];
    snprintf(url, sizeof(url), "http://%s:8000/api/leaderboard/top/no-mii/50", WWFC_DOMAIN);

    // OS::Report("[VRLeaderboard] fetching %s\n", url);
    void* request = NHTTPCreateRequest(url, 0, ctx->workBuf, ctx->workBufSize,
                                       reinterpret_cast<void*>(&VRLeaderboardPage::OnLeaderboardReceived),
                                       ctx);
    if (request == nullptr) {
        // OS::Report("[VRLeaderboard] NHTTPCreateRequest failed\n");
        if (ctx->workBuf != nullptr) NHTTPFreeFromEggHeap(ctx->workBuf);
        NHTTPFreeFromEggHeap(ctx);
        s_fetchState = FETCH_ERROR;
        return;
    }

    if (strncmp(url, "https://", 8) == 0) {
        NHTTPConfigureHttpsForRequest(request);
    }
    const s32 sendRet = NHTTPSendRequestAsync(request);
    // OS::Report("[VRLeaderboard] NHTTPSendRequestAsync req=%p ret=%d\n", request, sendRet);
    if (sendRet != 0) {
        s_nhttpStarted = false;
        if (ctx->workBuf != nullptr) NHTTPFreeFromEggHeap(ctx->workBuf);
        NHTTPFreeFromEggHeap(ctx);
        s_fetchState = FETCH_ERROR;
    }
}

void VRLeaderboardPage::OnLeaderboardReceived(s32 result, void* response, void* userdata) {
    NHTTPRequestCtx* ctx = reinterpret_cast<NHTTPRequestCtx*>(userdata);
    // OS::Report("[VRLeaderboard] callback result=%d response=%p\n", result, response);
    if (ctx != nullptr) {
        // OS::Report("[VRLeaderboard] callback gen=%u curGen=%u\n", ctx->generation, s_requestGeneration);
    }
    if (response == nullptr) {
        s_fetchState = FETCH_ERROR;
        if (ctx != nullptr) {
            if (ctx->workBuf != nullptr) NHTTPFreeFromEggHeap(ctx->workBuf);
            NHTTPFreeFromEggHeap(ctx);
        }
        return;
    }
    if (ctx != nullptr && ctx->generation != s_requestGeneration) {
        // OS::Report("[VRLeaderboard] ignoring stale response gen=%u curGen=%u\n", ctx->generation, s_requestGeneration);
        NHTTPDestroyResponse(response);
        if (ctx->workBuf != nullptr) NHTTPFreeFromEggHeap(ctx->workBuf);
        NHTTPFreeFromEggHeap(ctx);
        return;
    }

    if (result != 0) {
        NHTTPDestroyResponse(response);
        s_fetchState = FETCH_ERROR;
        if (ctx != nullptr) {
            if (ctx->workBuf != nullptr) NHTTPFreeFromEggHeap(ctx->workBuf);
            NHTTPFreeFromEggHeap(ctx);
        }
        return;
    }

    char* body = nullptr;
    int bodyLen = NHTTP::GetBodyAll(reinterpret_cast<NHTTP::Res*>(response), &body);
    // OS::Report("[VRLeaderboard] body=%p bodyLen=%d\n", body, bodyLen);
    if (body == nullptr || bodyLen <= 0) {
        NHTTPDestroyResponse(response);
        s_fetchState = FETCH_ERROR;
        return;
    }
    // OS::Report("[VRLeaderboard] bodyText=%.*s\n", MinInt(bodyLen, 512), body);

    int copyLen = bodyLen;
    if (copyLen > static_cast<int>(sizeof(s_responseBuf) - 1)) copyLen = sizeof(s_responseBuf) - 1;
    memcpy(s_responseBuf, body, copyLen);
    s_responseBuf[copyLen] = '\0';

    NHTTPDestroyResponse(response);
    if (ctx != nullptr) {
        if (ctx->workBuf != nullptr) NHTTPFreeFromEggHeap(ctx->workBuf);
        NHTTPFreeFromEggHeap(ctx);
    }

    int parsed = ParseResponse(s_responseBuf, s_entries, kMaxEntries);
    // OS::Report("[VRLeaderboard] parsed=%d\n", parsed);
    if (parsed <= 0) {
        s_fetchState = FETCH_ERROR;
        return;
    }

    OverrideOwnMiiData(s_entries, parsed, s_currentUserFriendCode);

    for (int i = parsed; i < kMaxEntries; ++i) {
        CopyAsciiToWide(s_entries[i].name, sizeof(s_entries[i].name) / sizeof(s_entries[i].name[0]), "----");
        s_entries[i].vr = 0;
        s_entries[i].friendCode = 0;
    }
    s_fetchState = FETCH_READY;
    s_hasApplied = false;
}

int VRLeaderboardPage::ParseResponse(const char* json, Entry* outEntries, int maxEntries) {
    if (json == nullptr || outEntries == nullptr || maxEntries <= 0) return 0;

    const char* p = FindStr(json, "[");
    if (p == nullptr) return 0;

    ++p;

    int count = 0;

    while (*p != '\0' && count < maxEntries) {
        while (*p != '\0' && *p != '{' && *p != ']') ++p;
        if (*p == ']' || *p == '\0') break;

        const char* objStart = p;
        const char* objEnd = FindMatchingObjectEnd(objStart);
        if (objEnd == nullptr) break;

        outEntries[count].name[0] = L'\0';
        outEntries[count].vr = 0;
        outEntries[count].friendCode = 0;
        memset(&outEntries[count].miiData, 0, sizeof(outEntries[count].miiData));

        const char* miiKey = FindStrInRange(objStart, objEnd, "\"miiData\"");
        const char* nameKey = FindStrInRange(objStart, objEnd, "\"name\"");
        const char* vrKey = FindStrInRange(objStart, objEnd, "\"vr\"");
        const char* friendCodeKey = FindStrInRange(objStart, objEnd, "\"friendCode\"");
        if (friendCodeKey == nullptr) {
            friendCodeKey = FindStrInRange(objStart, objEnd, "\"friend_code\"");
        }

        if (miiKey != nullptr) {
            const char* colon = FindStrInRange(miiKey, objEnd, ":");
            if (colon != nullptr) {
                char miiB64[192];
                const char* after = ParseJsonStringIntoAscii(colon + 1, miiB64, sizeof(miiB64));
                (void)after;
                DecodeBase64(miiB64, reinterpret_cast<u8*>(&outEntries[count].miiData), sizeof(outEntries[count].miiData));
                ExtractMiiNameFromStoreData(&outEntries[count].miiData, outEntries[count].name,
                                            sizeof(outEntries[count].name) / sizeof(outEntries[count].name[0]));
            }
        }

        if (outEntries[count].name[0] == L'\0' && nameKey != nullptr) {
            const char* colon = FindStrInRange(nameKey, objEnd, ":");
            if (colon != nullptr) {
                (void)ParseJsonStringIntoWide(colon + 1, outEntries[count].name,
                                              sizeof(outEntries[count].name) / sizeof(outEntries[count].name[0]));
            }
        }

        if (vrKey != nullptr) {
            const char* colon = FindStrInRange(vrKey, objEnd, ":");
            if (colon != nullptr) {
                u32 vrValue = 0;
                (void)ParseJsonU32(colon + 1, vrValue);
                outEntries[count].vr = vrValue;
            }
        }

        if (friendCodeKey != nullptr) {
            const char* colon = FindStrInRange(friendCodeKey, objEnd, ":");
            if (colon != nullptr) {
                colon = SkipWhitespace(colon + 1);
                if (colon != nullptr && *colon == '"') {
                    char fcStr[32];
                    ParseJsonStringIntoAscii(colon, fcStr, sizeof(fcStr));
                    u64 friendCodeValue = 0;
                    for (const char* p = fcStr; *p != '\0'; ++p) {
                        if (*p >= '0' && *p <= '9') friendCodeValue = friendCodeValue * 10 + static_cast<u64>(*p - '0');
                    }
                    outEntries[count].friendCode = friendCodeValue;
                } else {
                    ParseJsonU64(colon, outEntries[count].friendCode);
                }
            }
        }

        if (outEntries[count].name[0] != L'\0') {
            ++count;
        }

        p = objEnd + 1;
    }
    return count;
}

void VRLeaderboardPage::OverrideOwnMiiData(Entry* entries, int entryCount, u64 ownFriendCode) {
    if (entries == nullptr || entryCount <= 0 || ownFriendCode == 0) return;

    RKSYS::Mgr* rksysMgr = RKSYS::Mgr::sInstance;
    if (rksysMgr == nullptr || rksysMgr->curLicenseId < 0 || rksysMgr->curLicenseId >= 4) return;

    RKSYS::LicenseMgr& license = rksysMgr->licenses[rksysMgr->curLicenseId];

    for (int i = 0; i < entryCount; ++i) {
        if (entries[i].friendCode == ownFriendCode) {
            Mii::ComputeRFLStoreData(entries[i].miiData, &license.createID);
            ExtractMiiNameFromStoreData(&entries[i].miiData, entries[i].name,
                                        sizeof(entries[i].name) / sizeof(entries[i].name[0]));
        }
    }
}

}  // namespace UI
}  // namespace Pulsar