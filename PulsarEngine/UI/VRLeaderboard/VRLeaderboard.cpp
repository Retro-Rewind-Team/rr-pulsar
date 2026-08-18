#include <UI/VRLeaderboard/VRLeaderboard.hpp>
#include <Network/Json.hpp>
#include <Network/NHTTPHelper.hpp>
#include <UI/UI.hpp>
#include <MarioKartWii/Archive/ArchiveMgr.hpp>
#include <MarioKartWii/Audio/RSARPlayer.hpp>
#include <MarioKartWii/File/BMG.hpp>
#include <MarioKartWii/RKNet/FriendMgr.hpp>
#include <MarioKartWii/UI/Section/SectionMgr.hpp>
#include <MarioKartWii/RKSYS/RKSYSMgr.hpp>
#include <MarioKartWii/RKSYS/LicenseMgr.hpp>
#include <MarioKartWii/System/Identifiers.hpp>
#include <MarioKartWii/Mii/Mii.hpp>
#include <core/RK/RKSystem.hpp>
#include <core/egg/mem/Heap.hpp>
#include <core/rvl/DWC/DWCAccount.hpp>
#include <core/rvl/NHTTP/NHTTP.hpp>
#include <core/rvl/DWC/NHTTP.hpp>
#include <core/rvl/OS/OS.hpp>
#include <core/nw4r/lyt/TextBox.hpp>
#include <include/c_stdio.h>
#include <include/c_string.h>
#include <hooks.hpp>
#include <Settings/Settings.hpp>
#include <Settings/SettingsParam.hpp>

kmWrite32(0x800c9980, 0x4800000c);  // b 0x800c998c

static void NHTTPConfigureHttpsForRequest(void *request) {
    if (request == nullptr) return;
    typedef s32 (*Fn)(void *, ...);
    (reinterpret_cast<Fn>(&NHTTPSetRootCADefault))(request);
    (reinterpret_cast<Fn>(&NHTTPSetVerifyOption))(request, 1);
}

namespace Pulsar {
namespace UI {

struct VRLeaderboardText {
    const wchar_t *loading;
    const wchar_t *error;
};

static const VRLeaderboardText &GetVRLeaderboardText() {
    static const VRLeaderboardText texts[] = {
        {L"Loading...", L"Load failed."},
        {L"\u8AAD\u8FBC\u4E2D...", L"\u8AAD\u8FBC\u5931\u6557\u3002"},
        {L"Chargement...", L"\u00C9chec du chargement."},
        {L"Laden...", L"Laden fehlgeschlagen."},
        {L"Laden...", L"Laden mislukt."},
        {L"Cargando...", L"Error al cargar."},
        {L"Cargando...", L"Error al cargar."},
        {L"Ladataan...", L"Lataus ep\u00E4onnistui."},
        {L"Caricamento...", L"Caricamento fallito."},
        {L"\uBD88\uB7EC\uC624\uB294 \uC911...", L"\uBD88\uB7EC\uC624\uAE30 \uC2E4\uD328"},
        {L"\u0417\u0430\u0433\u0440\u0443\u0437\u043A\u0430...", L"\u0421\u0431\u043E\u0439 \u0437\u0430\u0433\u0440\u0443\u0437\u043A\u0438."},
        {L"Y\u00FCkleniyor...", L"Y\u00FCkleme hatas\u0131."},
        {L"Na\u010D\u00EDt\u00E1n\u00ED...", L"Na\u010Dten\u00ED selhalo."},
    };

    u32 idx = static_cast<u32>(Settings::Mgr::Get().GetSettingValue(Pulsar::Settings::SETTING_LANGUAGE));
    if (idx >= (sizeof(texts) / sizeof(texts[0]))) idx = LANGUAGE_ENGLISH;
    return texts[idx];
}

static void BMGHolderLoadWithFallback(BMGHolder *self, const char *name) {
    if (self == nullptr) return;

    self->bmgFile = nullptr;
    self->info = nullptr;
    self->data = nullptr;
    self->str1Block = nullptr;
    self->messageIds = nullptr;

    if (name == nullptr) return;
    ArchiveMgr *archiveMgr = ArchiveMgr::sInstance;
    if (archiveMgr == nullptr) return;

    char path[96];
    snprintf(path, sizeof(path), "message/%s.bmg", name);

    void *file = archiveMgr->GetFile(ARCHIVE_HOLDER_UI, path, nullptr);
    if (file == nullptr) {
        file = archiveMgr->GetFile(ARCHIVE_HOLDER_UI, "message/Common.bmg", nullptr);
    }
    if (file == nullptr) return;

    self->Init(*reinterpret_cast<const BMGHeader *>(file));
}
kmBranch(0x805f8b90, BMGHolderLoadWithFallback);

VRLeaderboardPage::FetchState VRLeaderboardPage::s_fetchState = VRLeaderboardPage::FETCH_IDLE;
bool VRLeaderboardPage::s_hasApplied = false;
VRLeaderboardPage::Entry *VRLeaderboardPage::s_entries = nullptr;

static wchar_t s_rowTextDash[] = L"----";
static wchar_t s_rowLabelVR[] = L"VR";
static wchar_t s_rowBlank[] = L"";
static u64 s_requestStartTime = 0;
static bool s_nhttpStarted = false;
static const u32 s_nhttpWorkBufSize = 0x20000;
static u32 s_requestGeneration = 0;
static u64 s_currentUserFriendCode = 0;
static u32 s_entrySoundFrameCounter = 0;
static u32 s_loadedAPIPage = 0;
static s32 s_loadedEntryCount = 0;

static const u32 s_requestTimeoutMs = 45000;

struct NHTTPRequestCtx {
    u32 generation;
    u32 apiPage;
};

// This page only issues a new request after the prior one has completed, so a single
// persistent request context/work buffer avoids lifetime bugs without affecting boot.
static NHTTPRequestCtx s_requestCtx;
static void *s_requestWorkBuf = nullptr;
static char s_requestUrl[256];

static u32 GetAPIPageForInGamePage(u32 inGamePage) {
    return inGamePage / VRLeaderboardPage::kPagesPerAPIFetch + 1;
}

static void SetLeaderboardRowTextColor(LayoutUIControl &row, const nw4r::ut::Color &textColor) {
    const char *textBoxNames[] = {"player_name", "position", "total_score", "total_point"};
    for (int j = 0; j < 4; ++j) {
        nw4r::lyt::TextBox *textBox = reinterpret_cast<nw4r::lyt::TextBox *>(row.layout.GetPaneByName(textBoxNames[j]));
        if (textBox != nullptr) {
            textBox->color1[0] = textColor;
            textBox->color1[1] = textColor;
        }
    }
}

static void SetTextBoxIfPresent(LayoutUIControl &control, const char *paneName, u32 bmgId,
                                const Text::Info *info) {
    if (control.layout.GetPaneByName(paneName) != nullptr) control.SetTextBoxMessage(paneName, bmgId, info);
}

static void SetPaneVisibleIfPresent(LayoutUIControl &control, const char *paneName, bool visible) {
    if (control.layout.GetPaneByName(paneName) != nullptr) control.SetPaneVisibility(paneName, visible);
}

static void ClearLeaderboardRow(LayoutUIControl &row, wchar_t *nameText) {
    Text::Info nameInfo;
    nameInfo.strings[0] = nameText;
    SetTextBoxIfPresent(row, "player_name", UI::BMG_TEXT, &nameInfo);

    Text::Info blankInfo;
    blankInfo.strings[0] = s_rowBlank;
    SetTextBoxIfPresent(row, "position", UI::BMG_TEXT, &blankInfo);
    SetTextBoxIfPresent(row, "total_score", UI::BMG_TEXT, &blankInfo);
    SetTextBoxIfPresent(row, "total_point", UI::BMG_TEXT, &blankInfo);

    SetLeaderboardRowTextColor(row, nw4r::ut::Color(255, 255, 255, 255));
    SetPaneVisibleIfPresent(row, "chara_icon", false);
    SetPaneVisibleIfPresent(row, "chara_icon_sha", false);
}

static unsigned char ParseJsonEscape(const char *&p) {
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

static const char *ParseJsonStringIntoWide(const char *p, wchar_t *out, size_t outLen) {
    if (out == nullptr || outLen == 0) return nullptr;
    out[0] = L'\0';
    p = Network::Json::SkipWhitespace(p);
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

static const char *ParseJsonStringIntoAscii(const char *p, char *out, size_t outLen) {
    if (out == nullptr || outLen == 0) return nullptr;
    out[0] = '\0';
    p = Network::Json::SkipWhitespace(p);
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

static int DecodeBase64(const char *in, u8 *out, int outCap) {
    if (in == nullptr || out == nullptr || outCap <= 0) return 0;

    int outLen = 0;
    int buf[4];
    int bufCount = 0;

    for (const char *p = in; *p != '\0'; ++p) {
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

static void ExtractMiiNameFromStoreData(const RFL::StoreData *storeData, wchar_t *outName, size_t outNameLen) {
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

static const char *FindStrInRange(const char *start, const char *end, const char *needle) {
    if (start == nullptr || end == nullptr || needle == nullptr) return nullptr;
    const size_t needleLen = strlen(needle);
    if (needleLen == 0) return start;
    for (const char *p = start; p + needleLen <= end; ++p) {
        if (strncmp(p, needle, needleLen) == 0) return p;
    }
    return nullptr;
}

static const char *FindMatchingObjectEnd(const char *objStart) {
    if (objStart == nullptr || *objStart != '{') return nullptr;
    int depth = 0;
    bool inString = false;
    bool escape = false;
    for (const char *p = objStart; *p != '\0'; ++p) {
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
    RKSYS::Mgr *rksysMgr = RKSYS::Mgr::sInstance;
    if (rksysMgr == nullptr || rksysMgr->curLicenseId < 0 || rksysMgr->curLicenseId >= 4) return false;

    RKSYS::LicenseFriends &licenseFriends = rksysMgr->licenses[rksysMgr->curLicenseId].GetFriends();
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
        static const char *noAnims[] = {nullptr};
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
    s_loadedAPIPage = 0;
    s_loadedEntryCount = 0;
    ResetRowsToLoading();

    if (s_entries == nullptr) {
        EGG::Heap *heap = RKSystem::mInstance.EGGSystem;
        if (heap != nullptr) {
            s_entries = new (heap, 0x20) Entry[kMaxEntries];
        }
    }
    if (s_entries == nullptr) {
        s_fetchState = FETCH_ERROR;
        return;
    }

    this->PlaySound(SOUND_ID_BUTTON_SELECT, -1);
    StartFetch(this);
}

void VRLeaderboardPage::OnDeactivate() {
    ++s_requestGeneration;
    s_fetchState = FETCH_IDLE;
    s_hasApplied = false;
    s_loadedAPIPage = 0;
    s_loadedEntryCount = 0;
    s_currentUserFriendCode = 0;
    s_entrySoundFrameCounter = 0;

    delete[] s_entries;
    s_entries = nullptr;
}

void VRLeaderboardPage::BeforeEntranceAnimations() {
    this->nextPageId = PAGE_NONE;
    backButton->SelectInitial(0);
}

void VRLeaderboardPage::OnUpdate() {
    if (s_fetchState == FETCH_REQUESTING) {
        const u64 now = OS::GetTime();
        const u32 elapsedMs = OS::TicksToMilliseconds(now - s_requestStartTime);
        if (elapsedMs > s_requestTimeoutMs) {
            s_fetchState = FETCH_ERROR;
            s_hasApplied = false;
        }
    }

    if (s_fetchState == FETCH_READY && !s_hasApplied) {
        s_entrySoundFrameCounter = 0;
        ApplyResults();
        s_hasApplied = true;
    } else if (s_fetchState == FETCH_ERROR && !s_hasApplied) {
        ApplyError();
        s_hasApplied = true;
    }

    if (s_fetchState == FETCH_READY && s_hasApplied && s_entrySoundFrameCounter < kRowsPerPage) {
        const u32 targetFrame = s_entrySoundFrameCounter * 2;
        if (this->curStateDuration >= targetFrame) {
            Audio::RSARPlayer::PlaySoundById(SOUND_ID_SMALL_HIGH_NOTE, 0, this);
            ++s_entrySoundFrameCounter;
        }
    }

    if (s_fetchState != FETCH_READY || !s_hasApplied) return;
    if (SectionMgr::sInstance == nullptr) return;

    const Input::RealControllerHolder *controllerHolder = SectionMgr::sInstance->pad.padInfos[0].controllerHolder;
    if (controllerHolder == nullptr || controllerHolder->curController == nullptr) return;

    const ControllerType controllerType = controllerHolder->curController->GetType();
    const u16 inputs = controllerHolder->inputStates[0].buttonRaw;
    const u16 newInputs = inputs & ~controllerHolder->inputStates[1].buttonRaw;

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

    bool pageWentLeft;
    if ((newInputs & leftButton) != 0 && curPage > 0) {
        --curPage;
        pageWentLeft = true;
    } else if ((newInputs & rightButton) != 0 && curPage + 1 < kPageCount) {
        ++curPage;
        pageWentLeft = false;
    } else {
        return;
    }

    this->PlaySound(pageWentLeft ? SOUND_ID_LEFT_ARROW_PRESS : SOUND_ID_RIGHT_ARROW_PRESS, -1);
    if (GetAPIPageForInGamePage(curPage) == s_loadedAPIPage) {
        ApplyResults();
    } else {
        ResetRowsToLoading();
        StartFetch(this);
    }
}

void VRLeaderboardPage::OnBackPress(u32 /*hudSlotId*/) {
    this->nextPageId = PAGE_WFC_MAIN;
    this->EndStateAnimated(1, 0.0f);
}

void VRLeaderboardPage::OnBackButtonClick(PushButton &button, u32 /*hudSlotId*/) {
    this->nextPageId = PAGE_WFC_MAIN;
    this->EndStateAnimated(1, button.GetAnimationFrameSize());
}

void VRLeaderboardPage::ResetRowsToLoading() {
    const VRLeaderboardText &text = GetVRLeaderboardText();
    Text::Info bottomInfo;
    bottomInfo.strings[0] = const_cast<wchar_t *>(text.loading);
    bottomText->SetMessage(UI::BMG_TEXT, &bottomInfo);

    for (int i = 0; i < kRowsPerPage; ++i) {
        ClearLeaderboardRow(*rows[i], const_cast<wchar_t *>(text.loading));
    }
}

void VRLeaderboardPage::ApplyResults() {
    if (s_entries == nullptr) {
        ApplyError();
        return;
    }

    const int base = static_cast<int>(curPage % kPagesPerAPIFetch) * kRowsPerPage;
    for (int i = 0; i < kRowsPerPage; ++i) {
        const int idx = base + i;
        if (idx >= s_loadedEntryCount) {
            ClearLeaderboardRow(*rows[i], s_rowTextDash);
            continue;
        }

        const u32 rank = s_entries[idx].rank != 0 ? s_entries[idx].rank : static_cast<u32>(curPage) * kRowsPerPage + i + 1;
        wchar_t positionText[8];
        swprintf(positionText, sizeof(positionText) / sizeof(positionText[0]), L"#%u", rank);

        Text::Info nameInfo;
        nameInfo.strings[0] = s_entries[idx].name;
        SetTextBoxIfPresent(*rows[i], "player_name", UI::BMG_TEXT, &nameInfo);

        Text::Info posInfo;
        posInfo.strings[0] = positionText;
        SetTextBoxIfPresent(*rows[i], "position", UI::BMG_TEXT, &posInfo);

        wchar_t vrText[16];
        swprintf(vrText, sizeof(vrText) / sizeof(vrText[0]), L"%u", s_entries[idx].vr);
        Text::Info valueInfo;
        valueInfo.strings[0] = vrText;
        SetTextBoxIfPresent(*rows[i], "total_score", UI::BMG_TEXT, &valueInfo);

        Text::Info labelInfo;
        labelInfo.strings[0] = s_rowLabelVR;
        SetTextBoxIfPresent(*rows[i], "total_point", UI::BMG_TEXT, &labelInfo);

        const bool isCurrentUser = (s_currentUserFriendCode != 0 && s_entries[idx].friendCode != 0 &&
                                    s_currentUserFriendCode == s_entries[idx].friendCode);
        bool isFriend = false;
        if (!isCurrentUser && s_entries[idx].friendCode != 0) {
            RKNet::FriendMgr *friendMgr = RKNet::FriendMgr::sInstance;
            if (friendMgr != nullptr && friendMgr->IsAvailable()) {
                const s32 friendIdx = friendMgr->GetFriendIdx(s_entries[idx].friendCode);
                isFriend = (friendIdx >= 0);
            }

            if (!isFriend) {
                isFriend = IsFriendCodeInLicenseFriends(s_entries[idx].friendCode);
            }
        }

        nw4r::ut::Color textColor;
        if (isCurrentUser) {
            textColor = nw4r::ut::Color(255, 215, 0, 255);
        } else if (isFriend) {
            textColor = nw4r::ut::Color(0, 255, 0, 255);
        } else {
            textColor = nw4r::ut::Color(255, 255, 255, 255);
        }

        SetLeaderboardRowTextColor(*rows[i], textColor);

        miiGroup->LoadMii(i, &s_entries[idx].miiData);
        rows[i]->SetMiiPane("chara_icon", *miiGroup, i, 2);
        rows[i]->SetMiiPane("chara_icon_sha", *miiGroup, i, 2);
        SetPaneVisibleIfPresent(*rows[i], "chara_icon", true);
        SetPaneVisibleIfPresent(*rows[i], "chara_icon_sha", true);
    }

    wchar_t pageText[16];
    swprintf(pageText, sizeof(pageText) / sizeof(pageText[0]), L"< %d/%d >", static_cast<int>(curPage) + 1,
             kPageCount);
    Text::Info info;
    info.strings[0] = pageText;
    bottomText->SetMessage(UI::BMG_TEXT, &info);
}

void VRLeaderboardPage::ApplyError() {
    Text::Info bottomInfo;
    bottomInfo.strings[0] = const_cast<wchar_t *>(GetVRLeaderboardText().error);
    bottomText->SetMessage(UI::BMG_TEXT, &bottomInfo);

    for (int i = 0; i < kRowsPerPage; ++i) {
        ClearLeaderboardRow(*rows[i], s_rowTextDash);
    }
}

void VRLeaderboardPage::StartFetch(VRLeaderboardPage *page) {
    if (s_fetchState == FETCH_REQUESTING) return;
    if (page == nullptr || s_entries == nullptr) {
        s_fetchState = FETCH_ERROR;
        return;
    }

    const u32 apiPage = GetAPIPageForInGamePage(page->curPage);
    s_fetchState = FETCH_REQUESTING;
    s_hasApplied = false;
    s_requestStartTime = OS::GetTime();
    ++s_requestGeneration;

    s_currentUserFriendCode = 0;
    RKSYS::Mgr *rksysMgr = RKSYS::Mgr::sInstance;
    if (rksysMgr != nullptr && rksysMgr->curLicenseId >= 0) {
        RKSYS::LicenseMgr &license = rksysMgr->licenses[rksysMgr->curLicenseId];
        s_currentUserFriendCode = DWC::CreateFriendKey(&license.dwcAccUserData);
    }

    memset(s_entries, 0, sizeof(Entry) * kMaxEntries);

    if (!Network::PreparePersistentNHTTPRequest(s_nhttpStarted)) {
        s_fetchState = FETCH_ERROR;
        return;
    }

    NHTTPRequestCtx *ctx = &s_requestCtx;
    ctx->generation = s_requestGeneration;
    ctx->apiPage = apiPage;
    if (s_requestWorkBuf == nullptr) {
        s_requestWorkBuf = Network::NHTTPAlloc(s_nhttpWorkBufSize, 0x20);
        if (s_requestWorkBuf == nullptr) {
            s_fetchState = FETCH_ERROR;
            return;
        }
    }
    memset(s_requestWorkBuf, 0, s_nhttpWorkBufSize);

    snprintf(s_requestUrl, sizeof(s_requestUrl), "http://rwfc.net/api/leaderboard/in-game?page=%u", apiPage);

    void *request = NHTTPCreateRequest(s_requestUrl, 0, s_requestWorkBuf, s_nhttpWorkBufSize,
                                       reinterpret_cast<void *>(&VRLeaderboardPage::OnLeaderboardReceived),
                                       ctx);
    if (request == nullptr) {
        s_fetchState = FETCH_ERROR;
        return;
    }

    if (strncmp(s_requestUrl, "https://", 8) == 0) {
        NHTTPConfigureHttpsForRequest(request);
    }
    const s32 sendRet = NHTTPSendRequestAsync(request);
    if (sendRet < 0) {
        s_nhttpStarted = false;
        s_fetchState = FETCH_ERROR;
        return;
    }
    Network::MarkNHTTPRequestActive();
}

void VRLeaderboardPage::OnLeaderboardReceived(s32 result, void *response, void *userdata) {
    Network::FinishNHTTPRequest();
    NHTTPRequestCtx *ctx = reinterpret_cast<NHTTPRequestCtx *>(userdata);

    if (ctx == nullptr || ctx->generation != s_requestGeneration) {
        if (response != nullptr) NHTTPDestroyResponse(response);
        return;
    }

    if (response == nullptr) {
        s_fetchState = FETCH_ERROR;
        return;
    }
    if (s_entries == nullptr) {
        NHTTPDestroyResponse(response);
        s_fetchState = FETCH_ERROR;
        return;
    }

    if (result != 0) {
        NHTTPDestroyResponse(response);
        s_fetchState = FETCH_ERROR;
        return;
    }

    char *body = nullptr;
    int bodyLen = NHTTP::GetBodyAll(reinterpret_cast<NHTTP::Res *>(response), &body);
    if (body == nullptr || bodyLen <= 0) {
        NHTTPDestroyResponse(response);
        s_fetchState = FETCH_ERROR;
        return;
    }

    const u32 responseBufSize = static_cast<u32>(bodyLen) + 1;
    char *responseBuf = reinterpret_cast<char *>(Network::NHTTPAlloc(responseBufSize, 4));
    if (responseBuf == nullptr) {
        NHTTPDestroyResponse(response);
        s_fetchState = FETCH_ERROR;
        return;
    }

    memcpy(responseBuf, body, bodyLen);
    responseBuf[bodyLen] = '\0';

    NHTTPDestroyResponse(response);
    s_loadedAPIPage = ctx->apiPage;

    const int parsed = ParseResponse(responseBuf, s_entries, kMaxEntries);
    Network::NHTTPFree(responseBuf);
    if (parsed <= 0) {
        s_fetchState = FETCH_ERROR;
        s_loadedEntryCount = 0;
        return;
    }

    OverrideOwnMiiData(s_entries, parsed, s_currentUserFriendCode);

    s_loadedEntryCount = parsed;
    s_fetchState = FETCH_READY;
    s_hasApplied = false;
}

int VRLeaderboardPage::ParseResponse(const char *json, Entry *outEntries, int maxEntries) {
    if (json == nullptr || outEntries == nullptr || maxEntries <= 0) return 0;

    const char *p = strchr(json, '[');
    if (p == nullptr) return 0;

    ++p;

    int count = 0;

    while (*p != '\0' && count < maxEntries) {
        while (*p != '\0' && *p != '{' && *p != ']') ++p;
        if (*p == ']' || *p == '\0') break;

        const char *objStart = p;
        const char *objEnd = FindMatchingObjectEnd(objStart);
        if (objEnd == nullptr) break;

        outEntries[count].name[0] = L'\0';
        outEntries[count].vr = 0;
        outEntries[count].rank = 0;
        outEntries[count].friendCode = 0;
        memset(&outEntries[count].miiData, 0, sizeof(outEntries[count].miiData));

        const char *miiKey = FindStrInRange(objStart, objEnd, "\"miiData\"");
        const char *nameKey = FindStrInRange(objStart, objEnd, "\"name\"");
        const char *vrKey = FindStrInRange(objStart, objEnd, "\"vr\"");
        const char *rankKey = FindStrInRange(objStart, objEnd, "\"rank\"");
        const char *friendCodeKey = FindStrInRange(objStart, objEnd, "\"friendCode\"");
        if (friendCodeKey == nullptr) {
            friendCodeKey = FindStrInRange(objStart, objEnd, "\"friend_code\"");
        }

        if (miiKey != nullptr) {
            const char *colon = FindStrInRange(miiKey, objEnd, ":");
            if (colon != nullptr) {
                char miiB64[192];
                (void)ParseJsonStringIntoAscii(colon + 1, miiB64, sizeof(miiB64));
                DecodeBase64(miiB64, reinterpret_cast<u8 *>(&outEntries[count].miiData), sizeof(outEntries[count].miiData));
                ExtractMiiNameFromStoreData(&outEntries[count].miiData, outEntries[count].name,
                                            sizeof(outEntries[count].name) / sizeof(outEntries[count].name[0]));
            }
        }

        if (outEntries[count].name[0] == L'\0' && nameKey != nullptr) {
            const char *colon = FindStrInRange(nameKey, objEnd, ":");
            if (colon != nullptr) {
                (void)ParseJsonStringIntoWide(colon + 1, outEntries[count].name,
                                              sizeof(outEntries[count].name) / sizeof(outEntries[count].name[0]));
            }
        }

        if (vrKey != nullptr) {
            const char *colon = FindStrInRange(vrKey, objEnd, ":");
            if (colon != nullptr) {
                Network::Json::ParseU32(colon + 1, outEntries[count].vr);
            }
        }

        if (rankKey != nullptr) {
            const char *colon = FindStrInRange(rankKey, objEnd, ":");
            if (colon != nullptr) {
                Network::Json::ParseU32(colon + 1, outEntries[count].rank);
            }
        }

        if (friendCodeKey != nullptr) {
            const char *colon = FindStrInRange(friendCodeKey, objEnd, ":");
            if (colon != nullptr) {
                colon = Network::Json::SkipWhitespace(colon + 1);
                if (colon != nullptr && *colon == '"') {
                    char fcStr[32];
                    ParseJsonStringIntoAscii(colon, fcStr, sizeof(fcStr));
                    u64 friendCodeValue = 0;
                    for (const char *fc = fcStr; *fc != '\0'; ++fc) {
                        if (*fc >= '0' && *fc <= '9') {
                            friendCodeValue = friendCodeValue * 10 + static_cast<u64>(*fc - '0');
                        }
                    }
                    outEntries[count].friendCode = friendCodeValue;
                } else {
                    Network::Json::ParseU64(colon, outEntries[count].friendCode);
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

void VRLeaderboardPage::OverrideOwnMiiData(Entry *entries, int entryCount, u64 ownFriendCode) {
    if (entries == nullptr || entryCount <= 0 || ownFriendCode == 0) return;

    RKSYS::Mgr *rksysMgr = RKSYS::Mgr::sInstance;
    if (rksysMgr == nullptr || rksysMgr->curLicenseId < 0 || rksysMgr->curLicenseId >= 4) return;

    RKSYS::LicenseMgr &license = rksysMgr->licenses[rksysMgr->curLicenseId];

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
