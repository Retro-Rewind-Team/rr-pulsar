#include <MarioKartWii/UI/Page/Other/GlobeSearch.hpp>
#include <MarioKartWii/RKSYS/RKSYSMgr.hpp>
#include <Settings/UI/ExpWFCMainPage.hpp>
#include <UI/UI.hpp>
#include <Network/Ranking.hpp>
#include <UI/PlayerCount.hpp>
#include <PulsarSystem.hpp>

namespace Pulsar {
namespace UI {
// EXPANDED WFC, keeping WW button and just hiding it in case it is ever needed...

kmWrite32(0x8064b984, 0x60000000);  // nop the InitControl call in the init func
kmWrite24(0x80899a36, 'PUL');  // 8064ba38
kmWrite24(0x80899a5B, 'PUL');  // 8064ba90

void ExpWFCMain::OnInit() {
    this->InitControlGroup(11);
    WFCMainMenu::OnInit();
    this->AddControl(5, settingsButton, 0);

    this->settingsButton.Load(UI::buttonFolder, "Settings1P", "Settings", 1, 0, false);
    this->settingsButton.buttonId = 5;
    this->settingsButton.SetOnClickHandler(this->onSettingsClick, 0);
    this->settingsButton.SetOnSelectHandler(this->onButtonSelectHandler);

    this->AddControl(6, playerCount, 0);
    ControlLoader loader(&this->playerCount);
    loader.Load(UI::buttonFolder, "PlayerButton", "VRButton", nullptr);

    this->AddControl(10, rankInfo, 0);
    ControlLoader rankLoader(&this->rankInfo);
    rankLoader.Load(UI::buttonFolder, "RankButton", "VRButton", nullptr);

    this->AddControl(7, mainButton, 0);
    this->mainButton.Load(UI::buttonFolder, "MainButton", "ButtonMain", 1, 0, 0);
    this->mainButton.buttonId = 6;
    this->mainButton.SetMessage(BMG_MAIN_MODES);
    this->mainButton.SetOnClickHandler(this->onMainClick, 0);
    this->mainButton.SetOnSelectHandler(this->onButtonSelectHandler);

    this->AddControl(8, otherButton, 0);
    this->otherButton.Load(UI::buttonFolder, "OtherButton", "ButtonOther", 1, 0, 0);
    this->otherButton.buttonId = 7;
    this->otherButton.SetMessage(BMG_OTHER_MODES);
    this->otherButton.SetOnClickHandler(this->onOtherClick, 0);
    this->otherButton.SetOnSelectHandler(this->onButtonSelectHandler);

    this->AddControl(9, battleButton, 0);
    this->battleButton.Load(UI::buttonFolder, "BattleButton", "ButtonBattle", 1, 0, 0);
    this->battleButton.buttonId = 8;
    this->battleButton.SetMessage(BMG_BATTLE_MODES);
    this->battleButton.SetOnClickHandler(this->onBattleClick, 0);
    this->battleButton.SetOnSelectHandler(this->onButtonSelectHandler);

    this->regionalButton.manipulator.inaccessible = true;
    this->worldwideButton.manipulator.inaccessible = true;
    this->regionalButton.isHidden = true;
    this->worldwideButton.isHidden = true;

    this->topSettingsPage = SettingsPanel::id;

    // Set retro button as default selected
    this->mainButton.Select(0);

    // this->manipulatorManager.SetGlobalHandler(START_PRESS, this->onStartPress, false, false);
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
    ExpSection::GetSection()->GetPulPage<SettingsPanel>()->prevPageId = PAGE_WFC_MAIN;
    this->nextPageId = static_cast<PageId>(this->topSettingsPage);
    this->EndStateAnimated(0, pushButton.GetAnimationFrameSize());
}

void ExpWFCMain::ExtOnButtonSelect(PushButton& button, u32 hudSlotId) {
    if (button.buttonId == 5) {
        u32 bmgId = BMG_SETTINGS_BOTTOM + 1;
        if (this->topSettingsPage == PAGE_VS_TEAMS_VIEW)
            bmgId += 1;
        else if (this->topSettingsPage == PAGE_BATTLE_MODE_SELECT)
            bmgId += 2;
        this->bottomText.SetMessage(bmgId, 0);
    } else
        this->OnButtonSelect(button, hudSlotId);
}

void ExpWFCMain::BeforeControlUpdate() {
    WFCMainMenu::BeforeControlUpdate();

    int RR_numRetro, RR_numCT, RR_numRT;
    int RR_num200cc, RR_numOTT, RR_numIR;
    int BT_numRegulars, BT_numElim;
    int numRegulars;

    PlayerCount::GetNumbersMain(RR_numRetro, RR_numCT, RR_numRT);
    PlayerCount::GetNumbersOther(RR_num200cc, RR_numOTT, RR_numIR);
    PlayerCount::GetNumbersBT(BT_numRegulars, BT_numElim);
    PlayerCount::GetNumbersRegular(numRegulars);

    Text::Info info;
    info.intToPass[0] = RR_numRetro + RR_numCT + RR_numRT + RR_num200cc + RR_numOTT + RR_numIR + BT_numRegulars + BT_numElim + numRegulars;
    this->playerCount.SetTextBoxMessage("go", BMG_PLAYER_COUNT, &info);

    if (!Dolphin::IsEmulator()) {
        this->playerCount.SetPaneVisibility("capsul_null", false);
    }

    wchar_t rankBuf[48];
    rankBuf[0] = L'\0';
    Ranking::FormatRankMessage(rankBuf, sizeof(rankBuf) / sizeof(rankBuf[0]));
    Text::Info rankInfoTxt;
    rankInfoTxt.strings[0] = rankBuf;
    this->rankInfo.SetTextBoxMessage("go", UI::BMG_TEXT, &rankInfoTxt);
    if (!Dolphin::IsEmulator()) {
        this->rankInfo.SetPaneVisibility("capsul_null", false);
    }
}

// ExpWFCModeSel
kmWrite32(0x8064c284, 0x38800001);  // distance func

void ExpWFCModeSel::OnInit() {
    WFCModeSelect::OnInit();
    // this->manipulatorManager.SetGlobalHandler(START_PRESS, this->onStartPress, false, false);
}

u32 Pulsar::UI::ExpWFCModeSel::lastClickedButton = 0;

void ExpWFCModeSel::InitButton(ExpWFCModeSel& self) {
    self.InitControlGroup(13);

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
    self.ctButton.SetTextBoxMessage("go", BMG_VR_RATING, &info);
    self.regButton.SetTextBoxMessage("go", BMG_VR_RATING, &info);
    self.twoHundredButton.SetTextBoxMessage("go", BMG_VR_RATING, &info);
    self.ottButton.SetTextBoxMessage("go", BMG_VR_RATING, &info);
    self.itemRainButton.SetTextBoxMessage("go", BMG_VR_RATING, &info);
    self.RRbattleButton.SetTextBoxMessage("go", BMG_BR_RATING, &info);
    self.RRbattleButtonElim.SetTextBoxMessage("go", BMG_BR_RATING, &info);
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
    ClearModeContexts();

    if (id == ottButtonId) {
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
    const bool isHidden = search->searchType == 1 ? false : true;  // make the button visible if continental was clicked

    // Reset game mode if worldwide is selected
    if (isHidden) {
        ClearModeContexts();
        System::sInstance->netMgr.region = 0x0A;
        page->lastClickedButton = 0;  // Reset to VS button
    }

    page->vsButton.isHidden = isHidden;
    page->vsButton.manipulator.inaccessible = isHidden;
    page->ctButton.isHidden = isHidden;
    page->ctButton.manipulator.inaccessible = isHidden;
    page->regButton.isHidden = isHidden;
    page->regButton.manipulator.inaccessible = isHidden;
    page->twoHundredButton.isHidden = isHidden;
    page->twoHundredButton.manipulator.inaccessible = isHidden;
    page->ottButton.isHidden = isHidden;
    page->ottButton.manipulator.inaccessible = isHidden;
    page->itemRainButton.isHidden = isHidden;
    page->itemRainButton.manipulator.inaccessible = isHidden;
    page->RRbattleButton.isHidden = isHidden;
    page->RRbattleButton.manipulator.inaccessible = isHidden;
    page->RRbattleButtonElim.isHidden = isHidden;
    page->RRbattleButtonElim.manipulator.inaccessible = isHidden;

    page->vsButton.SetMessage(BMG_VS_BUTTON);

    if (!isHidden) {
        bool isMainMode = (ExpWFCMain::lastClickedMainMenuButton == 6);
        bool isBattleMode = (ExpWFCMain::lastClickedMainMenuButton == 8);
        bool isOtherMode = (ExpWFCMain::lastClickedMainMenuButton == 7);

        // Show retro buttons only in main mode
        page->vsButton.isHidden = isOtherMode || isBattleMode;
        page->vsButton.manipulator.inaccessible = isOtherMode || isBattleMode;
        page->ctButton.isHidden = isOtherMode || isBattleMode;
        page->ctButton.manipulator.inaccessible = isOtherMode || isBattleMode;
        page->regButton.isHidden = isOtherMode || isBattleMode;
        page->regButton.manipulator.inaccessible = isOtherMode || isBattleMode;

        // Show custom buttons only in other mode
        page->ottButton.isHidden = isMainMode || isBattleMode;
        page->ottButton.manipulator.inaccessible = isMainMode || isBattleMode;
        page->twoHundredButton.isHidden = isMainMode || isBattleMode;
        page->twoHundredButton.manipulator.inaccessible = isMainMode || isBattleMode;
        page->itemRainButton.isHidden = isMainMode || isBattleMode;
        page->itemRainButton.manipulator.inaccessible = isMainMode || isBattleMode;

        // Show battle buttons only in battle mode
        page->RRbattleButton.isHidden = isOtherMode || isMainMode;
        page->RRbattleButton.manipulator.inaccessible = isOtherMode || isMainMode;
        page->RRbattleButtonElim.isHidden = isOtherMode || isMainMode;
        page->RRbattleButtonElim.manipulator.inaccessible = isOtherMode || isMainMode;
    }

    page->battleButton.isHidden = true;
    page->battleButton.manipulator.inaccessible = true;

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
    if (System::sInstance->IsContext(PULSAR_MODE_OTT) && System::sInstance->IsContext(PULSAR_RETROS)) {
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
        button->Select(0);
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
    }

    else
        WFCModeSelect::OnModeButtonSelect(modeButton, hudSlotId);
}

void ExpWFCModeSel::BeforeControlUpdate() {
    WFCModeSelect::BeforeControlUpdate();

    int numRegulars;
    int RR_numRetro, RR_numCT, RR_numRT;
    int RR_num200cc, RR_numOTT, RR_numIR;
    int BT_numRegulars, BT_numELIM;
    PlayerCount::GetNumbersMain(RR_numRetro, RR_numCT, RR_numRT);
    PlayerCount::GetNumbersOther(RR_num200cc, RR_numOTT, RR_numIR);
    PlayerCount::GetNumbersBT(BT_numRegulars, BT_numELIM);
    PlayerCount::GetNumbersRegular(numRegulars);

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

        info.intToPass[0] = BT_numRegulars;
        this->RRbattleButton.SetTextBoxMessage("go", Pulsar::UI::BMG_PLAYER_COUNT, &info);

        info.intToPass[0] = BT_numELIM;
        this->RRbattleButtonElim.SetTextBoxMessage("go", Pulsar::UI::BMG_PLAYER_COUNT, &info);
    } else {
        info.intToPass[0] = numRegulars;
        this->vsButton.SetTextBoxMessage("go", Pulsar::UI::BMG_PLAYER_COUNT, &info);
    }

    RKSYS::Mgr* rksysMgr = RKSYS::Mgr::sInstance;
    u32 vr = 0;
    u32 br = 0;
    if (rksysMgr->curLicenseId >= 0) {
        RKSYS::LicenseMgr& license = rksysMgr->licenses[rksysMgr->curLicenseId];
        vr = license.vr.points;
        br = license.br.points;
    }

    info.intToPass[0] = vr;
    this->vrButton.SetTextBoxMessage("go", Pulsar::UI::BMG_VR_RATING, &info);
    if (ExpWFCMain::lastClickedMainMenuButton == 8) {
        info.intToPass[0] = br;
        this->vrButton.SetTextBoxMessage("go", Pulsar::UI::BMG_BR_RATING, &info);
    }

    if (!Dolphin::IsEmulator()) {
        this->vsButton.SetPaneVisibility("capsul_null", false);
        this->ottButton.SetPaneVisibility("capsul_null", false);
        this->twoHundredButton.SetPaneVisibility("capsul_null", false);
        this->ctButton.SetPaneVisibility("capsul_null", false);
        this->itemRainButton.SetPaneVisibility("capsul_null", false);
        this->regButton.SetPaneVisibility("capsul_null", false);
        this->RRbattleButton.SetPaneVisibility("capsul_null", false);
        this->RRbattleButtonElim.SetPaneVisibility("capsul_null", false);
    }
}

}  // namespace UI
}  // namespace Pulsar

// void PatchWFCMenu_LoadButton(PushButton* _this, const char* folderName, const char* ctrName, const char* variant, u32 localPlayerBitfield, u32 r8, bool inaccessible) {
//     _this->Load(folderName, "NewWifiMenuButton", variant, localPlayerBitfield, r8, inaccessible);
// }

// void PatchWFCMenu_AddCapsule(CtrlMenuInstructionText* _this, u32 bmgId, const Text::Info* info) {
//     Pages::WFCMainMenu* wfcMenu = SectionMgr::sInstance->curSection->Get<Pages::WFCMainMenu>();

//     Text::Info ninfo;
//     wfcMenu->regionalButton.SetPaneVisibility("capsul_null", true);

//     _this->SetMessage(bmgId, info);
// }

// kmCall(0x8064bc54, PatchWFCMenu_AddCapsule);
// kmCall(0x8064ba90, PatchWFCMenu_LoadButton);