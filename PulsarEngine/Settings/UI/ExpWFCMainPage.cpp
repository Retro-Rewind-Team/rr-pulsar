#include <MarioKartWii/UI/Page/Other/GlobeSearch.hpp>
#include <MarioKartWii/UI/Page/Other/Message.hpp>
#include <MarioKartWii/RKSYS/RKSYSMgr.hpp>
#include <MarioKartWii/UI/Text/Text.hpp>
#include <Settings/UI/ExpWFCMainPage.hpp>
#include <Settings/UI/SettingsPageSelect.hpp>
#include <UI/UI.hpp>
#include <Network/Ranking.hpp>
#include <UI/PlayerCount.hpp>
#include <PulsarSystem.hpp>
#include <Network/Mogi.hpp>
#include <Network/Rating/PlayerRating.hpp>
#include <Network/Rating/RatingSync.hpp>
#include <Network/ServerDateTime.hpp>
#include <include/c_wchar.h>

namespace Pulsar {
namespace UI {

static wchar_t s_rankDetailsBuffer[512];

static void FormatMMRValue(float mmr, wchar_t* buffer, u32 bufferSize) {
    int scaled = (int)(mmr * 100.0f + 0.5f);
    swprintf(buffer, bufferSize, L"%d", scaled);
}

static void FormatMMRDelta(float delta, wchar_t* buffer, u32 bufferSize) {
    int scaled = (int)(delta * 100.0f + (delta >= 0.0f ? 0.5f : -0.5f));
    if (scaled < 0)
        swprintf(buffer, bufferSize, L"%d", scaled);
    else
        swprintf(buffer, bufferSize, L"+%d", scaled);
}

static void ShowPendingLoginMMRChange(ExpWFCMain& page) {
    Section* section = SectionMgr::sInstance->curSection;
    if (section == nullptr || section->GetTopLayerPage() != &page) return;

    Pages::MessageBoxTransparent* messageBox = section->Get<Pages::MessageBoxTransparent>();
    if (messageBox == nullptr) return;

    float oldMMR;
    float newMMR;
    if (!PointRating::GetPendingLoginMMRChange(oldMMR, newMMR)) return;

    wchar_t oldText[32];
    wchar_t deltaText[32];
    wchar_t newText[32];
    FormatMMRValue(oldMMR, oldText, sizeof(oldText) / sizeof(oldText[0]));
    FormatMMRDelta(newMMR - oldMMR, deltaText, sizeof(deltaText) / sizeof(deltaText[0]));
    FormatMMRValue(newMMR, newText, sizeof(newText) / sizeof(newText[0]));
    Text::Info info;
    info.strings[0] = oldText;
    info.strings[1] = deltaText;
    info.strings[2] = newText;
    messageBox->Reset();
    messageBox->SetMessageWindowText(UI::BMG_MOGI_MMR_CHANGE, &info);
    page.AddPageLayer(PAGE_MESSAGE_BOX_TRANSPARENT, 0);
    PointRating::ClearPendingLoginMMRChange();
}

static void ApplyVRMultiplierHighlight(PushButton& button, bool hasMultiplier) {
    nw4r::lyt::TextBox* textBox = reinterpret_cast<nw4r::lyt::TextBox*>(button.layout.GetPaneByName("go"));
    if (textBox != nullptr) {
        nw4r::ut::Color color = hasMultiplier ? nw4r::ut::Color(0, 255, 0, 255) : nw4r::ut::Color(255, 255, 255, 255);
        textBox->color1[0] = color;
        textBox->color1[1] = color;
    }
}

static void FormatRatingLabel(float rating, const wchar_t* suffix, wchar_t* buffer, u32 bufferSize) {
    wchar_t digits[32];
    PointRating::FormatRatingDigits(rating, digits, sizeof(digits) / sizeof(digits[0]));
    swprintf(buffer, bufferSize, (rating >= 1000.0f) ? L"%ls%ls\uF06D" : L"%ls%ls", digits, suffix);
}

static void SetButtonHidden(PushButton& button, bool hidden) {
    button.isHidden = hidden;
    button.manipulator.inaccessible = hidden;
}

// Expanded WFC main menu. The vanilla worldwide/regional buttons stay loaded because
// the original menu code still expects them to exist.

kmWrite32(0x8064b984, 0x60000000);  // nop the InitControl call in the init func
kmWrite24(0x80899a36, 'PUL');  // 8064ba38
kmWrite24(0x80899a5B, 'PUL');  // 8064ba90

void ExpWFCMain::OnInit() {
    this->InitControlGroup(13);
    WFCMainMenu::OnInit();
    this->AddControl(6, settingsButton, 0);

    this->settingsButton.Load(UI::buttonFolder, "Settings1P", "Settings", 1, 0, false);
    this->settingsButton.buttonId = 5;
    this->settingsButton.SetOnClickHandler(this->onSettingsClick, 0);
    this->settingsButton.SetOnSelectHandler(this->onButtonSelectHandler);

    this->AddControl(7, playerCount, 0);
    ControlLoader loader(&this->playerCount);
    loader.Load(UI::buttonFolder, "PlayerButton", "VRButton", nullptr);

    this->AddControl(8, rankInfo, 0);
    ControlLoader rankLoader(&this->rankInfo);
    rankLoader.Load(UI::buttonFolder, "RankButton", "VRButton", nullptr);

    this->AddControl(9, mainButton, 0);
    this->mainButton.Load(UI::buttonFolder, "MainButton", "ButtonMain", 1, 0, 0);
    this->mainButton.buttonId = 6;
    this->mainButton.SetMessage(BMG_MAIN_MODES);
    this->mainButton.SetOnClickHandler(this->onMainClick, 0);
    this->mainButton.SetOnSelectHandler(this->onButtonSelectHandler);

    this->AddControl(10, otherButton, 0);
    this->otherButton.Load(UI::buttonFolder, "OtherButton", "ButtonOther", 1, 0, 0);
    this->otherButton.buttonId = 7;
    this->otherButton.SetMessage(BMG_OTHER_MODES);
    this->otherButton.SetOnClickHandler(this->onOtherClick, 0);
    this->otherButton.SetOnSelectHandler(this->onButtonSelectHandler);

    this->AddControl(11, battleButton, 0);
    this->battleButton.Load(UI::buttonFolder, "BattleButton", "ButtonBattle", 1, 0, 0);
    this->battleButton.buttonId = 8;
    this->battleButton.SetMessage(BMG_BATTLE_MODES);
    this->battleButton.SetOnClickHandler(this->onBattleClick, 0);
    this->battleButton.SetOnSelectHandler(this->onButtonSelectHandler);

    this->AddControl(12, leaderboardButton, 0);
    this->leaderboardButton.Load(UI::buttonFolder, "Settings1P", "Leaderboard", 1, 0, 0);
    this->leaderboardButton.buttonId = 9;
    this->leaderboardButton.SetMessage(BMG_VR_LEADERBOARD_BUTTON);
    this->leaderboardButton.SetOnClickHandler(this->onLeaderboardClick, 0);
    this->leaderboardButton.SetOnSelectHandler(this->onButtonSelectHandler);

    SetButtonHidden(this->regionalButton, true);
    SetButtonHidden(this->worldwideButton, true);

    this->topSettingsPage = SettingsPageSelect::id;

    this->mainButton.Select(0);

    this->manipulatorManager.SetGlobalHandler(START_PRESS, this->onStartPress, false, false);
}

u32 Pulsar::UI::ExpWFCMain::lastClickedMainMenuButton = 6;
void ExpWFCMain::OnMainButtonClick(PushButton& pushButton, u32 hudSlotId) {
    ExpWFCMain::lastClickedMainMenuButton = 6;  // retros
    this->OnRegionalButtonClick(pushButton, hudSlotId);
}

void ExpWFCMain::OnOtherButtonClick(PushButton& pushButton, u32 hudSlotId) {
    ExpWFCMain::lastClickedMainMenuButton = 7;  // customs
    this->OnRegionalButtonClick(pushButton, hudSlotId);
}

void ExpWFCMain::OnBattleButtonClick(PushButton& pushButton, u32 hudSlotId) {
    ExpWFCMain::lastClickedMainMenuButton = 8;  // battle
    this->OnRegionalButtonClick(pushButton, hudSlotId);
}

void ExpWFCMain::OnSettingsButtonClick(PushButton& pushButton, u32 r5) {
    ExpSection::GetSection()->GetPulPage<SettingsPageSelect>()->prevPageId = PAGE_WFC_MAIN;
    ExpSection::GetSection()->GetPulPage<SettingsPanel>()->prevPageId = PAGE_WFC_MAIN;
    this->nextPageId = static_cast<PageId>(this->topSettingsPage);
    this->EndStateAnimated(0, pushButton.GetAnimationFrameSize());
}

void ExpWFCMain::OnLeaderboardButtonClick(PushButton& pushButton, u32 hudSlotId) {
    this->nextPageId = static_cast<PageId>(PULPAGE_VRLEADERBOARD);
    this->EndStateAnimated(0, pushButton.GetAnimationFrameSize());
}

void ExpWFCMain::ExtOnStartPress(u32) {
    Pages::MessageBoxTransparent* messageBox = SectionMgr::sInstance->curSection->Get<Pages::MessageBoxTransparent>();
    if (messageBox == nullptr) return;

    s_rankDetailsBuffer[0] = L'\0';
    Ranking::FormatRankDetailsMessage(s_rankDetailsBuffer, sizeof(s_rankDetailsBuffer) / sizeof(s_rankDetailsBuffer[0]));

    Text::Info info;
    info.strings[0] = s_rankDetailsBuffer;
    messageBox->Reset();
    messageBox->SetMessageWindowText(UI::BMG_TEXT, &info);
    this->AddPageLayer(PAGE_MESSAGE_BOX_TRANSPARENT, 0);
}

void ExpWFCMain::ExtOnButtonSelect(PushButton& button, u32 hudSlotId) {
    if (button.buttonId != 5) {
        this->OnButtonSelect(button, hudSlotId);
    }
    this->bottomText.SetMessage(BMG_RANKING_TEXT, 0);
}

void ExpWFCMain::BeforeControlUpdate() {
    WFCMainMenu::BeforeControlUpdate();
    if (this->selectMainButtonOnResume) {
        this->selectMainButtonOnResume = false;
        this->mainButton.Select(0);
    }
    ShowPendingLoginMMRChange(*this);

    int RR_numRetro, RR_numCT, RR_numRT;
    int RR_num200cc, RR_numOTT, RR_numIR;
    int BT_numRegulars, BT_numElim;
    int numRegulars;
    int numMogi;

    PlayerCount::GetNumbersMain(RR_numRetro, RR_numCT, RR_numRT);
    PlayerCount::GetNumbersOther(RR_num200cc, RR_numOTT, RR_numIR);
    PlayerCount::GetNumbersBT(BT_numRegulars, BT_numElim);
    PlayerCount::GetNumbersRegular(numRegulars);
    PlayerCount::GetNumbersMogi(numMogi);

    Text::Info info;
    info.intToPass[0] = RR_numRetro + RR_numCT + RR_numRT + RR_num200cc + RR_numOTT + RR_numIR + BT_numRegulars + BT_numElim + numRegulars + numMogi;
    this->playerCount.SetTextBoxMessage("go", BMG_PLAYER_COUNT, &info);

    wchar_t rankBuf[48];
    rankBuf[0] = L'\0';
    Ranking::FormatRankMessage(rankBuf, sizeof(rankBuf) / sizeof(rankBuf[0]));
    Text::Info rankInfoTxt;
    rankInfoTxt.strings[0] = rankBuf;
    this->rankInfo.SetTextBoxMessage("go", UI::BMG_TEXT, &rankInfoTxt);
    this->rankInfo.SetPaneVisibility("capsul_null", true);
}

void ExpWFCMain::OnResume() {
    this->selectMainButtonOnResume = true;
}

// ExpWFCModeSel
kmWrite32(0x8064c284, 0x38800001);  // distance func

void ExpWFCModeSel::OnInit() {
    WFCModeSelect::OnInit();
}

u32 Pulsar::UI::ExpWFCModeSel::lastClickedButton = 0;

void ExpWFCModeSel::InitButton(ExpWFCModeSel& self) {
    self.InitControlGroup(14);

    self.AddControl(5, self.ctButton, 0);
    self.ctButton.Load(UI::buttonFolder, "WifiMenuModeSelect", "CTButton", 1, 0, 0);
    self.ctButton.buttonId = ctButtonId;
    self.ctButton.SetMessage(BMG_CT_BUTTON);
    self.ctButton.SetOnClickHandler(self.onModeButtonClickHandler, 0);
    self.ctButton.SetOnSelectHandler(self.onButtonSelectHandler);

    self.AddControl(6, self.regButton, 0);
    self.regButton.Load(UI::buttonFolder, "WifiMenuModeSelect", "RegButton", 1, 0, 0);
    self.regButton.buttonId = regButtonId;
    self.regButton.SetMessage(BMG_REGULAR_BUTTON);
    self.regButton.SetOnClickHandler(self.onModeButtonClickHandler, 0);
    self.regButton.SetOnSelectHandler(self.onButtonSelectHandler);

    self.AddControl(13, self.mogiButton, 0);
    self.mogiButton.Load(UI::buttonFolder, "WifiMenuModeSelect", "CompButton", 1, 0, 0);
    self.mogiButton.buttonId = mogiButtonId;
    self.mogiButton.SetMessage(UI::BMG_MOGI_BUTTON);
    self.mogiButton.SetOnClickHandler(self.onModeButtonClickHandler, 0);
    self.mogiButton.SetOnSelectHandler(self.onButtonSelectHandler);

    self.AddControl(7, self.twoHundredButton, 0);
    self.twoHundredButton.Load(UI::buttonFolder, "WifiMenuModeSelect", "200Button", 1, 0, 0);
    self.twoHundredButton.buttonId = twoHundredButtonId;
    self.twoHundredButton.SetMessage(BMG_200_BUTTON);
    self.twoHundredButton.SetOnClickHandler(self.onModeButtonClickHandler, 0);
    self.twoHundredButton.SetOnSelectHandler(self.onButtonSelectHandler);

    self.AddControl(8, self.ottButton, 0);
    self.ottButton.Load(UI::buttonFolder, "WifiMenuModeSelect", "OTTButton", 1, 0, 0);
    self.ottButton.buttonId = ottButtonId;
    self.ottButton.SetMessage(BMG_OTT_BUTTON);
    self.ottButton.SetOnClickHandler(self.onModeButtonClickHandler, 0);
    self.ottButton.SetOnSelectHandler(self.onButtonSelectHandler);

    self.AddControl(9, self.itemRainButton, 0);
    self.itemRainButton.Load(UI::buttonFolder, "WifiMenuModeSelect", "ItemRainButton", 1, 0, 0);
    self.itemRainButton.buttonId = itemRainButtonId;
    self.itemRainButton.SetMessage(BMG_ITEM_RAIN_BUTTON);
    self.itemRainButton.SetOnClickHandler(self.onModeButtonClickHandler, 0);
    self.itemRainButton.SetOnSelectHandler(self.onButtonSelectHandler);

    self.AddControl(11, self.RRbattleButton, 0);
    self.RRbattleButton.Load(UI::buttonFolder, "WifiMenuModeSelect", "BattleButton", 1, 0, 0);
    self.RRbattleButton.buttonId = RRbattleButtonId;
    self.RRbattleButton.SetMessage(BMG_BATTLE_BUTTON);
    self.RRbattleButton.SetOnClickHandler(self.onModeButtonClickHandler, 0);
    self.RRbattleButton.SetOnSelectHandler(self.onButtonSelectHandler);

    self.AddControl(12, self.RRbattleButtonElim, 0);
    self.RRbattleButtonElim.Load(UI::buttonFolder, "WifiMenuModeSelect", "BattleButtonElim", 1, 0, 0);
    self.RRbattleButtonElim.buttonId = RRbattleButtonIdElim;
    self.RRbattleButtonElim.SetMessage(BMG_BATTLE_BUTTON_ELIM);
    self.RRbattleButtonElim.SetOnClickHandler(self.onModeButtonClickHandler, 0);
    self.RRbattleButtonElim.SetOnSelectHandler(self.onButtonSelectHandler);

    self.AddControl(10, self.vrButton, 0);
    ControlLoader loader(&self.vrButton);
    loader.Load(UI::buttonFolder, "VRButton", "VRButton", nullptr);

    Text::Info info;
    RKSYS::Mgr* rksysMgr = RKSYS::Mgr::sInstance;
    float vr = 0.0f;
    float br = 0.0f;
    if (rksysMgr->curLicenseId >= 0) {
        vr = PointRating::GetUserVR(rksysMgr->curLicenseId);
        br = PointRating::GetUserBR(rksysMgr->curLicenseId);
    }

    wchar_t buffer[64];
    FormatRatingLabel(vr, L"VR", buffer, sizeof(buffer) / sizeof(buffer[0]));
    info.strings[0] = buffer;

    self.ctButton.SetTextBoxMessage("go", UI::BMG_TEXT, &info);
    self.regButton.SetTextBoxMessage("go", UI::BMG_TEXT, &info);
    self.twoHundredButton.SetTextBoxMessage("go", UI::BMG_TEXT, &info);
    self.ottButton.SetTextBoxMessage("go", UI::BMG_TEXT, &info);
    self.itemRainButton.SetTextBoxMessage("go", UI::BMG_TEXT, &info);

    if (ExpWFCMain::lastClickedMainMenuButton == 8) {
        FormatRatingLabel(br, L"BR", buffer, sizeof(buffer) / sizeof(buffer[0]));
        info.strings[0] = buffer;
        self.RRbattleButton.SetTextBoxMessage("go", UI::BMG_TEXT, &info);
        self.RRbattleButtonElim.SetTextBoxMessage("go", UI::BMG_TEXT, &info);
    } else {
        self.RRbattleButton.SetTextBoxMessage("go", UI::BMG_TEXT, &info);
        self.RRbattleButtonElim.SetTextBoxMessage("go", UI::BMG_TEXT, &info);
    }
}
kmCall(0x8064c294, ExpWFCModeSel::InitButton);

void ExpWFCModeSel::ClearModeContexts() {
    const u32 modeContexts[] = {
        PULSAR_RETROS,
        PULSAR_CTS,
        PULSAR_MODE_OTT,
        PULSAR_200_WW,
        PULSAR_ITEMMODERAIN,
    };

    const u32 numContexts = sizeof(modeContexts) / sizeof(modeContexts[0]);
    for (u32 i = 0; i < numContexts; ++i) {
        u32 context = modeContexts[i];
        System::sInstance->context &= ~(1 << context);
    }
}

void ExpWFCModeSel::OnModeButtonClick(PushButton& modeButton, u32 hudSlotId) {
    const u32 id = modeButton.buttonId;
    Mogi::SetEnabled(id == mogiButtonId);
    ClearModeContexts();

    if (Mogi::IsEnabled()) {
        System::sInstance->netMgr.region = Mogi::REGION;
    } else if (id == ottButtonId) {
        System::sInstance->netMgr.region = 0x0B;
    } else if (id == twoHundredButtonId) {
        System::sInstance->netMgr.region = 0x0C;
    } else if (id == itemRainButtonId) {
        System::sInstance->netMgr.region = 0x0D;
    } else if (id == ctButtonId) {
        System::sInstance->netMgr.region = 0x14;
    } else if (id == regButtonId) {
        System::sInstance->netMgr.region = 0x15;
    } else if (id == RRbattleButtonId) {
        System::sInstance->netMgr.region = 0x0E;
        WFCModeSelect::OnModeButtonClick(this->battleButton, hudSlotId);
    } else if (id == RRbattleButtonIdElim) {
        System::sInstance->netMgr.region = 0x0F;
        WFCModeSelect::OnModeButtonClick(this->battleButton, hudSlotId);
    } else {
        System::sInstance->netMgr.region = 0x0A;
    }

    // Update contexts based on the region number
    System::sInstance->UpdateContextWrapper();

    this->lastClickedButton = id;
    WFCModeSelect::OnModeButtonClick(modeButton, hudSlotId);
}

void ExpWFCModeSel::OnActivatePatch() {
    register ExpWFCModeSel* page;
    asm(mr page, r29;);
    register Pages::GlobeSearch* search;
    asm(mr search, r30;);
    const bool isHidden = search->searchType != 1;

    if (isHidden) {
        ClearModeContexts();
        if (!Mogi::IsEnabled()) System::sInstance->netMgr.region = 0x0A;
        page->lastClickedButton = 0;
    }

    SetButtonHidden(page->vsButton, isHidden);
    SetButtonHidden(page->ctButton, isHidden);
    SetButtonHidden(page->regButton, isHidden);
    SetButtonHidden(page->twoHundredButton, isHidden);
    SetButtonHidden(page->ottButton, isHidden);
    SetButtonHidden(page->itemRainButton, isHidden);
    SetButtonHidden(page->RRbattleButton, isHidden);
    SetButtonHidden(page->RRbattleButtonElim, isHidden);
    SetButtonHidden(page->mogiButton, isHidden);

    page->vsButton.SetMessage(BMG_VS_BUTTON);

    if (!isHidden) {
        const bool isMainMode = ExpWFCMain::lastClickedMainMenuButton == 6;
        const bool isBattleMode = ExpWFCMain::lastClickedMainMenuButton == 8;
        const bool isOtherMode = ExpWFCMain::lastClickedMainMenuButton == 7;

        SetButtonHidden(page->vsButton, isOtherMode || isBattleMode);
        SetButtonHidden(page->ctButton, isOtherMode || isBattleMode);
        SetButtonHidden(page->regButton, isOtherMode || isBattleMode);
        SetButtonHidden(page->mogiButton, isOtherMode || isBattleMode);

        SetButtonHidden(page->ottButton, isMainMode || isBattleMode);
        SetButtonHidden(page->twoHundredButton, isMainMode || isBattleMode);
        SetButtonHidden(page->itemRainButton, isMainMode || isBattleMode);

        SetButtonHidden(page->RRbattleButton, isOtherMode || isMainMode);
        SetButtonHidden(page->RRbattleButtonElim, isOtherMode || isMainMode);
    }

    SetButtonHidden(page->battleButton, true);

    Text::Info info;
    RKSYS::Mgr* rksysMgr = RKSYS::Mgr::sInstance;
    u32 vr = 0;
    u32 br = 0;
    if (rksysMgr->curLicenseId >= 0) {
        RKSYS::LicenseMgr& license = rksysMgr->licenses[rksysMgr->curLicenseId];
        vr = license.vr.points;
        br = license.br.points;
    }
    info.intToPass[0] = vr;
    if (ExpWFCMain::lastClickedMainMenuButton == 8) {
        info.intToPass[0] = br;
    }
    page->vsButton.SetTextBoxMessage("go", BMG_VR_RATING, &info);

    page->nextPage = PAGE_NONE;
    PushButton* button = &page->vsButton;
    PushButton* BTbutton = &page->RRbattleButton;
    PushButton* TWObutton = &page->twoHundredButton;
    u32 bmgId = UI::BMG_RACE_WITH11P;
    page->lastClickedButton = 0;
    const u32 gamemode = Racedata::sInstance->racesScenario.settings.gamemode;

    // Determine which button should be selected based on current context
    if (Mogi::IsEnabled()) {
        page->lastClickedButton = mogiButtonId;
        button = &page->mogiButton;
        bmgId = BMG_MOGI_BOTTOM;
    } else if (System::sInstance->IsContext(PULSAR_MODE_OTT) && System::sInstance->IsContext(PULSAR_RETROS)) {
        page->lastClickedButton = ottButtonId;
        button = &page->ottButton;
        bmgId = UI::BMG_OTT_WW_BOTTOM;
    } else if (System::sInstance->IsContext(PULSAR_200_WW) && System::sInstance->IsContext(PULSAR_RETROS)) {
        page->lastClickedButton = twoHundredButtonId;
        button = &page->twoHundredButton;
        bmgId = UI::BMG_200_WW_BOTTOM;
    } else if (System::sInstance->IsContext(PULSAR_ITEMMODERAIN) && System::sInstance->IsContext(PULSAR_RETROS)) {
        page->lastClickedButton = itemRainButtonId;
        button = &page->itemRainButton;
        bmgId = UI::BMG_ITEM_RAIN_WW_BOTTOM;
    } else if (System::sInstance->IsContext(PULSAR_CTS) && !System::sInstance->IsContext(PULSAR_MODE_OTT) && !System::sInstance->IsContext(PULSAR_200_WW)) {
        page->lastClickedButton = ctButtonId;
        button = &page->ctButton;
        bmgId = UI::BMG_RACE_WITH11P;
    } else if (System::sInstance->IsContext(PULSAR_REGS)) {
        page->lastClickedButton = regButtonId;
        button = &page->regButton;
        bmgId = UI::BMG_RACE_WITH11P;
    } else if (System::sInstance->netMgr.region == 0x0E && gamemode == MODE_PUBLIC_BATTLE) {
        page->lastClickedButton = RRbattleButtonId;
        button = &page->RRbattleButton;
        bmgId = UI::BMG_BATTLE_WW_BOTTOM;
    } else if (System::sInstance->netMgr.region == 0x0F && System::sInstance->IsContext(PULSAR_ELIMINATION)) {
        page->lastClickedButton = RRbattleButtonIdElim;
        button = &page->RRbattleButtonElim;
        bmgId = UI::BMG_BATTLE_WW_BOTTOM_ELIM;
    } else if (page->lastClickedButton == 2) {
        button = &page->battleButton;
        bmgId = UI::BMG_BATTLE_WITH6P;
    }

    page->bottomText.SetMessage(bmgId);
    button->Select(0);
    if (ExpWFCMain::lastClickedMainMenuButton == 8) {
        BTbutton->Select(0);
    } else if (ExpWFCMain::lastClickedMainMenuButton == 7) {
        TWObutton->Select(0);
    } else if (ExpWFCMain::lastClickedMainMenuButton == 6) {
        if (Mogi::IsEnabled())
            page->mogiButton.Select(0);
        else
            page->vsButton.Select(0);
    }
}
kmCall(0x8064c5f0, ExpWFCModeSel::OnActivatePatch);

void ExpWFCModeSel::OnModeButtonSelect(PushButton& modeButton, u32 hudSlotId) {
    if (modeButton.buttonId == ottButtonId) {
        this->bottomText.SetMessage(BMG_OTT_WW_BOTTOM);
    } else if (modeButton.buttonId == twoHundredButtonId) {
        this->bottomText.SetMessage(BMG_200_WW_BOTTOM);
    } else if (modeButton.buttonId == itemRainButtonId) {
        this->bottomText.SetMessage(BMG_ITEM_RAIN_WW_BOTTOM);
    } else if (modeButton.buttonId == ctButtonId) {
        this->bottomText.SetMessage(BMG_RACE_WITH11P);
    } else if (modeButton.buttonId == regButtonId) {
        this->bottomText.SetMessage(BMG_RACE_WITH11P);
    } else if (modeButton.buttonId == RRbattleButtonId) {
        this->bottomText.SetMessage(BMG_BATTLE_WW_BOTTOM);
    } else if (modeButton.buttonId == RRbattleButtonIdElim) {
        this->bottomText.SetMessage(BMG_BATTLE_WW_BOTTOM_ELIM);
    } else if (modeButton.buttonId == mogiButtonId) {
        this->bottomText.SetMessage(BMG_MOGI_BOTTOM);
    }

    else
        WFCModeSelect::OnModeButtonSelect(modeButton, hudSlotId);
}

void ExpWFCModeSel::BeforeControlUpdate() {
    WFCModeSelect::BeforeControlUpdate();

    int numRegulars;
    int numMogi;
    int RR_numRetro, RR_numCT, RR_numRT;
    int RR_num200cc, RR_numOTT, RR_numIR;
    int BT_numRegulars, BT_numELIM;
    PlayerCount::GetNumbersMain(RR_numRetro, RR_numCT, RR_numRT);
    PlayerCount::GetNumbersOther(RR_num200cc, RR_numOTT, RR_numIR);
    PlayerCount::GetNumbersBT(BT_numRegulars, BT_numELIM);
    PlayerCount::GetNumbersRegular(numRegulars);
    PlayerCount::GetNumbersMogi(numMogi);

    Pages::GlobeSearch* globeSearch = SectionMgr::sInstance->curSection->Get<Pages::GlobeSearch>();

    Text::Info info;
    if (globeSearch->searchType == 1) {
        info.intToPass[0] = RR_numIR;
        this->itemRainButton.SetTextBoxMessage("go", Pulsar::UI::BMG_PLAYER_COUNT, &info);

        info.intToPass[0] = RR_numOTT;
        this->ottButton.SetTextBoxMessage("go", Pulsar::UI::BMG_PLAYER_COUNT, &info);

        info.intToPass[0] = RR_num200cc;
        this->twoHundredButton.SetTextBoxMessage("go", Pulsar::UI::BMG_PLAYER_COUNT, &info);

        info.intToPass[0] = RR_numRetro;
        this->vsButton.SetTextBoxMessage("go", Pulsar::UI::BMG_PLAYER_COUNT, &info);

        info.intToPass[0] = RR_numCT;
        this->ctButton.SetTextBoxMessage("go", Pulsar::UI::BMG_PLAYER_COUNT, &info);

        info.intToPass[0] = RR_numRT;
        this->regButton.SetTextBoxMessage("go", Pulsar::UI::BMG_PLAYER_COUNT, &info);

        info.intToPass[0] = numMogi;
        this->mogiButton.SetTextBoxMessage("go", Pulsar::UI::BMG_PLAYER_COUNT, &info);

        info.intToPass[0] = BT_numRegulars;
        this->RRbattleButton.SetTextBoxMessage("go", Pulsar::UI::BMG_PLAYER_COUNT, &info);

        info.intToPass[0] = BT_numELIM;
        this->RRbattleButtonElim.SetTextBoxMessage("go", Pulsar::UI::BMG_PLAYER_COUNT, &info);
    } else {
        info.intToPass[0] = numRegulars;
        this->vsButton.SetTextBoxMessage("go", Pulsar::UI::BMG_PLAYER_COUNT, &info);
    }

    RKSYS::Mgr* rksysMgr = RKSYS::Mgr::sInstance;
    float vr = 0.0f;
    float br = 0.0f;
    if (rksysMgr->curLicenseId >= 0) {
        vr = PointRating::GetUserVR(rksysMgr->curLicenseId);
        br = PointRating::GetUserBR(rksysMgr->curLicenseId);
    }

    wchar_t buffer[64];
    FormatRatingLabel(vr, L"VR", buffer, sizeof(buffer) / sizeof(buffer[0]));
    info.strings[0] = buffer;

    this->vrButton.SetTextBoxMessage("go", Pulsar::UI::BMG_TEXT, &info);
    if (ExpWFCMain::lastClickedMainMenuButton == 8) {
        FormatRatingLabel(br, L"BR", buffer, sizeof(buffer) / sizeof(buffer[0]));
        info.strings[0] = buffer;
        this->vrButton.SetTextBoxMessage("go", Pulsar::UI::BMG_TEXT, &info);
    }

    ApplyVRMultiplierHighlight(this->twoHundredButton, PointRating::IsWeekendMultiplierActiveForRegion(0x0C));
    ApplyVRMultiplierHighlight(this->ottButton, PointRating::IsWeekendMultiplierActiveForRegion(0x0B));
    ApplyVRMultiplierHighlight(this->itemRainButton, PointRating::IsWeekendMultiplierActiveForRegion(0x0D) || PointRating::IsItemRainEventActive());
}

}  // namespace UI
}  // namespace Pulsar
