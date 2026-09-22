#include <core/nw4r/ut/Misc.hpp>
#include <MarioKartWii/GlobalFunctions.hpp>
#include <MarioKartWii/UI/Page/Other/SELECTStageMgr.hpp>
#include <UI/ChangeCombo/ChangeCombo.hpp>
#include <PulsarSystem.hpp>
#include <Gamemodes/KO/KOMgr.hpp>
#include <RetroRewind.hpp>
#include <MarioKartWii/UI/Ctrl/Menu/CtrlMenuCharacterSelect.hpp>
#include <MarioKartWii/UI/Page/Menu/CharacterSelect.hpp>
#include <Settings/UI/SettingsPanel.hpp>
#include <MarioKartWii/UI/Page/Other/VR.hpp>
#include <MarioKartWii/UI/Ctrl/CountDown.hpp>
#include <Settings/UI/SettingsPageSelect.hpp>
#include <CustomCharacters/CustomCharacters.hpp>
#include <Network/Rating/PlayerRating.hpp>
#include <Network/Settings/SelectionRestrictions.hpp>
#include <MarioKartWii/RKSYS/RKSYSMgr.hpp>

namespace Pulsar {
namespace UI {

static bool IsFriendRoom() {
    const RKNet::RoomType roomType = RKNet::Controller::sInstance->roomType;
    return roomType == RKNet::ROOMTYPE_FROOM_HOST || roomType == RKNet::ROOMTYPE_FROOM_NONHOST;
}

static bool IsRegionalRoom() {
    const RKNet::RoomType roomType = RKNet::Controller::sInstance->roomType;
    return roomType == RKNet::ROOMTYPE_VS_REGIONAL || roomType == RKNet::ROOMTYPE_JOINING_REGIONAL;
}

static bool ShouldHideComboButtons(const System &system) {
    if (system.IsContext(PULSAR_MODE_KO) && system.koMgr != nullptr && system.koMgr->isSpectating) return true;
    if (system.IsContext(PULSAR_MODE_OTT) && system.IsContext(PULSAR_CHANGECOMBO) == OTTSETTING_COMBO_ENABLED) return true;
    return system.IsContext(PULSAR_MODE_OTT) && IsRegionalRoom();
}

static CharacterId GetCharacterForSlot(u32 slot, u32 hudSlotId) {
    if (slot < 24) return static_cast<CharacterId>(CtrlMenuCharacterSelect::buttonIdToCharacterId[slot]);

    SectionParams *params = SectionMgr::sInstance->sectionParams;
    if (params == nullptr || hudSlotId >= params->localPlayerMiis.miiCount) return CHARACTER_NONE;
    Mii *mii = params->localPlayerMiis.GetMii(hudSlotId);
    if (mii == nullptr) return CHARACTER_NONE;

    CharacterId character = GetMiiCharacterId(*mii);
    if (character < MII_S_A_MALE || character > MII_L_C_FEMALE) return character;
    u32 groupStart = MII_S_A_MALE;
    if (character >= MII_L_A_MALE)
        groupStart = MII_L_A_MALE;
    else if (character >= MII_M_A_MALE)
        groupStart = MII_M_A_MALE;
    const u32 gender = (static_cast<u32>(character) - groupStart) & 1;
    return static_cast<CharacterId>(groupStart + gender + (slot - 24) * 2);
}

static CharacterId GetRandomEnabledCharacter(Random &random, u32 hudSlotId, CharacterId avoid) {
    CharacterId choices[Restrictions::CHARACTER_SLOT_COUNT];
    u32 count = 0;
    const u32 mask = Restrictions::GetCharacterMask();
    for (u32 slot = 0; slot < Restrictions::CHARACTER_SLOT_COUNT; ++slot) {
        if (((mask >> slot) & 1) == 0) continue;
        const CharacterId character = GetCharacterForSlot(slot, hudSlotId);
        if (character != CHARACTER_NONE && character != avoid) choices[count++] = character;
    }
    if (count == 0) return avoid;
    return choices[random.NextLimited(count)];
}

static u32 GetRandomEnabledVehiclePosition(Random &random, u32 weight, u32 avoid = 12) {
    u8 choices[Restrictions::VEHICLES_PER_WEIGHT];
    u32 count = 0;
    const u16 mask = Restrictions::GetVehicleMask(weight);
    for (u32 position = 0; position < Restrictions::VEHICLES_PER_WEIGHT; ++position) {
        if (((mask >> position) & 1) != 0 && position != avoid) choices[count++] = position;
    }
    if (count == 0) return avoid < Restrictions::VEHICLES_PER_WEIGHT ? avoid : Restrictions::GetFirstEnabledVehiclePosition(weight);
    return choices[random.NextLimited(count)];
}

static u32 GetEnabledVehicleOrdinal(u32 weight, u32 position) {
    const u16 mask = Restrictions::GetVehicleMask(weight);
    u32 ordinal = 0;
    for (u32 i = 0; i < position; ++i) ordinal += (mask >> i) & 1;
    return ordinal;
}

static const u32 settingsPreviewPageCapacity = 6;
static Settings::SettingsPageId s_settingsPreviewPages[settingsPreviewPageCapacity];
static u32 s_settingsPreviewSheetCount = 0;
static u32 s_settingsPreviewSheetIdx = 0;
static bool s_settingsPreviewComplete = false;
static bool s_settingsPreviewShownThisRoom = false;

static bool IsFroomVotingSection(SectionId sectionId) {
    return sectionId >= SECTION_P1_WIFI_FROOM_VS_VOTING && sectionId <= SECTION_P2_WIFI_FROOM_COIN_VOTING;
}

static void ResetSettingsPreview() {
    s_settingsPreviewSheetCount = 0;
    s_settingsPreviewSheetIdx = 0;
    s_settingsPreviewComplete = false;
}

void ResetFroomSettingsPreviewShown() {
    s_settingsPreviewShownThisRoom = false;
    ResetSettingsPreview();
}

static void BuildSettingsPreviewSheets() {
    const System *system = System::sInstance;
    const Network::Mgr &netMgr = system->netMgr;
    const SectionId sectionId = SectionMgr::sInstance->curSection->sectionId;
    const bool isBattle = sectionId == SECTION_P1_WIFI_FROOM_BALLOON_VOTING ||
                          sectionId == SECTION_P2_WIFI_FROOM_BALLOON_VOTING ||
                          sectionId == SECTION_P1_WIFI_FROOM_COIN_VOTING ||
                          sectionId == SECTION_P2_WIFI_FROOM_COIN_VOTING;
    s_settingsPreviewSheetCount = Settings::Params::BuildHostRulePages(
        s_settingsPreviewPages, isBattle,
        (netMgr.hostContext & (1 << PULSAR_MODE_KO)) || (netMgr.hostContext & (1 << PULSAR_MODE_LAPKO)),
        netMgr.hostContext & (1 << PULSAR_MODE_OTT), netMgr.hostContext2 & (1 << PULSAR_MODE_BATTLEROYALE),
        netMgr.hostContext & (1 << PULSAR_EXTENDEDTEAMS));
}

static bool TryPushNextSettingsPreview(Pages::SELECTStageMgr &page) {
    if (!System::sInstance->netMgr.hasHostSettingsPreview) return false;
    if (s_settingsPreviewShownThisRoom) return false;
    if (s_settingsPreviewComplete) return false;
    if (s_settingsPreviewSheetCount == 0) BuildSettingsPreviewSheets();

    if (s_settingsPreviewSheetIdx >= s_settingsPreviewSheetCount) {
        s_settingsPreviewComplete = true;
        s_settingsPreviewShownThisRoom = true;
        return false;
    }

    const Settings::SettingsPageId settingsPage = s_settingsPreviewPages[s_settingsPreviewSheetIdx++];
    SettingsPanel::StartVotingPreview(settingsPage);
    page.AddPageLayer(static_cast<PageId>(SettingsPanel::id), 0);
    return true;
}

bool AdvanceFroomSettingsPreview(Settings::SettingsPageId &settingsPage) {
    if (s_settingsPreviewShownThisRoom) return false;
    if (s_settingsPreviewComplete) return false;
    if (s_settingsPreviewSheetCount == 0) BuildSettingsPreviewSheets();

    if (s_settingsPreviewSheetIdx >= s_settingsPreviewSheetCount) {
        s_settingsPreviewComplete = true;
        s_settingsPreviewShownThisRoom = true;
        return false;
    }

    settingsPage = s_settingsPreviewPages[s_settingsPreviewSheetIdx++];
    return true;
}

static void SELECTStageMgrOnActivate(Pages::SELECTStageMgr *page) {
    ResetSettingsPreview();
    page->Pages::SELECTStageMgr::OnActivate();
}
kmWritePointer(0x808C06CC, SELECTStageMgrOnActivate);

static void SELECTStageMgrOnResume(Pages::SELECTStageMgr *page) {
    if (page->status == Pages::SELECTStageMgr::STATUS_WAITING && IsFroomVotingSection(SectionMgr::sInstance->curSection->sectionId)) {
        if (TryPushNextSettingsPreview(*page)) return;
    }

    page->Pages::SELECTStageMgr::OnResume();
}
kmWritePointer(0x808C06F0, SELECTStageMgrOnResume);

kmWrite32(0x806508d4, 0x60000000);  // Add VR screen outside of 1st race in frooms

ExpVR::ExpVR() : comboButtonState(0) {
    this->onRandomComboClick.subject = this;
    this->onRandomComboClick.ptmf = &ExpVR::RandomizeComboVR;
    this->onChangeComboClick.subject = this;
    this->onChangeComboClick.ptmf = &ExpVR::ChangeCombo;
    this->onSettingsClick.subject = this;
    this->onSettingsClick.ptmf = &ExpVR::OnSettingsButtonClick;
    this->onButtonSelectHandler.subject = this;
    this->onButtonSelectHandler.ptmf = &ExpVR::ExtOnButtonSelect;
    this->shouldRestoreControls = false;
}

kmWrite32(0x8064a61c, 0x60000000);  // nop initControlGroup

kmWrite24(0x808998b3, 'PUL');  // WifiMemberConfirmButton -> PULiMemberConfirmButton
void ExpVR::OnInit() {
    this->InitControlGroup(0x12);
    VR::OnInit();
    bool hideSettings = false;
    const System *system = System::sInstance;

    const Section *curSection = SectionMgr::sInstance->curSection;
    Pages::SELECTStageMgr *selectStageMgr = curSection->Get<Pages::SELECTStageMgr>();
    CountDown *timer = &selectStageMgr->countdown;

    bool isKOd = false;
    if (system->IsContext(PULSAR_MODE_KO) && system->koMgr->isSpectating) isKOd = true;
    if (System::sInstance->IsContext(PULSAR_MODE_OTT) && system->IsContext(PULSAR_CHANGECOMBO) == OTTSETTING_COMBO_ENABLED) isKOd = true;
    if (System::sInstance->IsContext(PULSAR_MODE_OTT) && ((RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_VS_REGIONAL) || (RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_JOINING_REGIONAL))) isKOd = true;

    bool isRandomHidden = false;
    if (Settings::Mgr::Get().GetSettingValue(Pulsar::Settings::SETTING_ONLINERANDOMBUTTON) == RANDOMBUTTON_DISABLED) isRandomHidden = true;
    if (Restrictions::AreOnlyMiisEnabled()) isRandomHidden = true;

    this->AddControl(0xF, this->randomComboButton, 0);
    this->randomComboButton.isHidden = isKOd || isRandomHidden;
    this->randomComboButton.Load(UI::buttonFolder, "PULiMemberConfirmButton", "Random", 1, 0, isKOd || isRandomHidden);
    this->randomComboButton.SetOnClickHandler(this->onRandomComboClick, 0);

    this->AddControl(0x10, this->changeComboButton, 0);
    this->changeComboButton.isHidden = isKOd;
    this->changeComboButton.Load(UI::buttonFolder, "PULiMemberConfirmButton", "Change", 1, 0, isKOd);
    this->changeComboButton.SetOnClickHandler(this->onChangeComboClick, 0);

    this->AddControl(0x11, settingsButton, 0);
    this->settingsButton.Load(UI::buttonFolder, "SettingsVR", "Settings", 1, 0, hideSettings);
    this->settingsButton.buttonId = 5;
    this->settingsButton.SetOnClickHandler(this->onSettingsClick, 0);
    this->settingsButton.SetOnSelectHandler(this->onButtonSelectHandler);
    this->topSettingsPage = SettingsPageSelect::id;

    SettingsPanel *settingsPanel = ExpSection::GetSection()->GetPulPage<SettingsPanel>();
    settingsPanel->timer = timer;

    Pages::CharacterSelect *charPage = curSection->Get<Pages::CharacterSelect>();
    charPage->timer = timer;
    charPage->ctrlMenuCharSelect.timer = timer;

    Pages::KartSelect *kartPage = curSection->Get<Pages::KartSelect>();
    if (kartPage != nullptr) kartPage->timer = timer;

    Pages::BattleKartSelect *kartBattlePage = curSection->Get<Pages::BattleKartSelect>();
    if (kartBattlePage != nullptr) kartBattlePage->timer = timer;

    Pages::MultiKartSelect *multiKartPage = curSection->Get<Pages::MultiKartSelect>();
    if (multiKartPage != nullptr) multiKartPage->timer = timer;

    Pages::DriftSelect *driftPage = curSection->Get<Pages::DriftSelect>();
    if (driftPage != nullptr) driftPage->timer = timer;

    Pages::MultiDriftSelect *multiDriftPage = curSection->Get<Pages::MultiDriftSelect>();
    if (multiDriftPage != nullptr) {
        multiDriftPage->nextSectionOnButtonClick = SECTION_NONE;
        multiDriftPage->timer = timer;
    }
}

static void RandomizeCombo() {
    if (Restrictions::AreOnlyMiisEnabled()) return;
    Random random;
    const SectionMgr *sectionMgr = SectionMgr::sInstance;
    const Section *section = sectionMgr->curSection;
    SectionParams *sectionParams = sectionMgr->sectionParams;
    for (int hudId = 0; hudId < sectionParams->localPlayerCount; ++hudId) {
        const CharacterId character = GetRandomEnabledCharacter(random, hudId, CHARACTER_NONE);
        if (character == CHARACTER_NONE) continue;
        const u32 weight = GetCharacterWeightClass(character);
        const u32 randomizedKartPos = GetRandomEnabledVehiclePosition(random, weight);
        const KartId kart = kartsSortedByWeight[weight][randomizedKartPos];

        sectionParams->characters[hudId] = character;
        sectionParams->karts[hudId] = kart;
        sectionParams->combos[hudId].selCharacter = character;
        sectionParams->combos[hudId].selKart = kart;
        CustomCharacters::RandomizeSelectedCharacterTable(character);

        ExpCharacterSelect *charSelect = section->Get<ExpCharacterSelect>();  // guaranteed to exist on this page
        charSelect->randomizedCharIdx[hudId] = character;
        charSelect->rolledCharIdx[hudId] = character;
        charSelect->rouletteCounter = ExpVR::randomDuration;
        charSelect->ctrlMenuCharSelect.selectedCharacter = character;
        charSelect->controlsManipulatorManager.inaccessible = true;
        ExpBattleKartSelect *battleKartSelect = section->Get<ExpBattleKartSelect>();
        if (battleKartSelect != nullptr) {
            battleKartSelect->selectedKart = random.NextLimited(2);
            battleKartSelect->controlsManipulatorManager.inaccessible = true;
        }

        ExpKartSelect *kartSelect = section->Get<ExpKartSelect>();
        if (kartSelect != nullptr) {
            kartSelect->rouletteCounter = ExpVR::randomDuration;
            kartSelect->randomizedKartPos = randomizedKartPos;
            kartSelect->rolledKartPos = randomizedKartPos;
            kartSelect->controlsManipulatorManager.inaccessible = true;
        }

        ExpMultiKartSelect *multiKartSelect = section->Get<ExpMultiKartSelect>();
        if (multiKartSelect != nullptr) {
            multiKartSelect->rouletteCounter = ExpVR::randomDuration;
            multiKartSelect->rolledKartPos[hudId] = IsBattle() ? random.NextLimited(2) : GetEnabledVehicleOrdinal(weight, randomizedKartPos);
            multiKartSelect->controlsManipulatorManager.inaccessible = true;
        }
    }
}

void ExpVR::RandomizeComboVR(PushButton &randomComboButton, u32 hudSlotId) {
    if (Restrictions::AreOnlyMiisEnabled()) return;
    this->comboButtonState = 1;
    this->EndStateAnimated(0, randomComboButton.GetAnimationFrameSize());
    RandomizeCombo();
}

void ExpVR::ChangeCombo(PushButton &changeComboButton, u32 hudSlotId) {
    this->comboButtonState = 2;
    this->EndStateAnimated(0, changeComboButton.GetAnimationFrameSize());
}

void ExpVR::OnSettingsButtonClick(PushButton &button, u32 hudSlotId) {
    this->savedOkHidden = this->okButton.isHidden;
    this->savedBackHidden = this->ctrlMenuBackButton.isHidden;
    this->savedBottomHidden = this->ctrlMenuBottomMessage.isHidden;
    this->savedRandomHidden = this->randomComboButton.isHidden;
    this->savedChangeHidden = this->changeComboButton.isHidden;
    this->savedSettingsHidden = this->settingsButton.isHidden;
    for (int i = 0; i < 12; ++i) this->savedVRControlsHidden[i] = this->vrControls[i].isHidden;
    this->areControlsHidden = true;
    this->okButton.isHidden = true;
    this->ctrlMenuBackButton.isHidden = true;
    this->ctrlMenuBottomMessage.isHidden = true;
    for (int i = 0; i < 12; ++i) this->vrControls[i].isHidden = true;
    this->randomComboButton.isHidden = true;
    this->changeComboButton.isHidden = true;
    this->settingsButton.isHidden = true;
    SettingsPanel *settingsPanel = ExpSection::GetSection()->GetPulPage<SettingsPanel>();
    settingsPanel->prevPageId = PAGE_NONE;
    SettingsPageSelect *settingsPageSelect = ExpSection::GetSection()->GetPulPage<SettingsPageSelect>();
    settingsPageSelect->SetContext(Settings::SETTINGS_CONTEXT_VOTING, PAGE_NONE);
    this->AddPageLayer(static_cast<PageId>(this->topSettingsPage), 0);
}

void ExpVR::AfterControlUpdate() {
    VR::AfterControlUpdate();

    const bool hidden = this->areControlsHidden;

    this->okButton.isHidden = hidden;
    this->ctrlMenuBackButton.isHidden = hidden;
    this->ctrlMenuBottomMessage.isHidden = hidden;
    if (hidden) {
        for (int i = 0; i < 12; ++i) this->vrControls[i].isHidden = true;
    }

    if (hidden) {
        this->randomComboButton.isHidden = true;
        this->changeComboButton.isHidden = true;
        this->settingsButton.isHidden = true;
    } else {
        const System *system = System::sInstance;
        const bool isKOd = ShouldHideComboButtons(*system);

        const bool isRandomHidden =
            Settings::Mgr::Get().GetSettingValue(Pulsar::Settings::SETTING_ONLINERANDOMBUTTON) == RANDOMBUTTON_DISABLED ||
            Restrictions::AreOnlyMiisEnabled();

        this->randomComboButton.isHidden = isKOd || isRandomHidden;
        this->changeComboButton.isHidden = isKOd;
        this->settingsButton.isHidden = false;

        if (this->shouldRestoreControls) {
            this->okButton.isHidden = this->savedOkHidden;
            this->ctrlMenuBackButton.isHidden = this->savedBackHidden;
            this->ctrlMenuBottomMessage.isHidden = this->savedBottomHidden;
            this->randomComboButton.isHidden = this->savedRandomHidden;
            this->changeComboButton.isHidden = this->savedChangeHidden;
            this->settingsButton.isHidden = this->savedSettingsHidden;
            for (int i = 0; i < 12; ++i) this->vrControls[i].isHidden = this->savedVRControlsHidden[i];
            this->shouldRestoreControls = false;
        }
    }
}

void ExpVR::BeforeExitAnimations() {
    VR::BeforeExitAnimations();
}

void ExpVR::OnDeactivate() {
    VR::OnDeactivate();
    CustomCharacters::RestoreVotingMenuDriverModels();
}

void ExpVR::OnResume() {
    if (this->areControlsHidden) this->areControlsHidden = false;
    this->shouldRestoreControls = true;
    VR::OnResume();
}

void ExpVR::ExtOnButtonSelect(PushButton &button, u32 hudSlotId) {
    if (button.buttonId == 5) {
        u32 bmgId = BMG_SETTINGS_BOTTOM + 1;
        if (this->topSettingsPage == PAGE_VS_TEAMS_VIEW)
            bmgId += 1;
        else if (this->topSettingsPage == PAGE_BATTLE_MODE_SELECT)
            bmgId += 2;
    } else {
        this->OnButtonClick(button, hudSlotId);
    }
}

static void AddChangeComboPages(Section *section, PageId id) {
    section->CreateAndInitPage(static_cast<PageId>(SettingsPanel::id));
    section->CreateAndInitPage(id);
    section->CreateAndInitPage(PAGE_CHARACTER_SELECT);
    bool isBattle = IsBattle();
    PageId kartPage = PAGE_KART_SELECT;
    PageId driftPage = PAGE_DRIFT_SELECT;
    if (SectionMgr::sInstance->sectionParams->localPlayerCount == 2) {
        kartPage = PAGE_MULTIPLAYER_KART_SELECT;
        driftPage = PAGE_MULTIPLAYER_DRIFT_SELECT;
    } else if (isBattle)
        kartPage = PAGE_BATTLE_KART_SELECT;
    section->CreateAndInitPage(kartPage);
    section->CreateAndInitPage(driftPage);
}

kmCall(0x8062e09c, AddChangeComboPages);  // 0x58 can't do this more efficiently because supporting page 0x7F breaks kart images
kmCall(0x8062e7e0, AddChangeComboPages);  // 0x60
kmCall(0x8062e870, AddChangeComboPages);  // 0x61
kmCall(0x8062e0e4, AddChangeComboPages);  // 0x59
kmCall(0x8062e900, AddChangeComboPages);  // 0x62
kmCall(0x8062e990, AddChangeComboPages);  // 0x63
kmCall(0x8062e708, AddChangeComboPages);  // 0x5e
kmCall(0x8062e798, AddChangeComboPages);  // 0x5f
kmCall(0x8062ea68, AddChangeComboPages);  // 0x64
kmCall(0x8062eaf8, AddChangeComboPages);  // 0x65
kmCall(0x8062eb88, AddChangeComboPages);  // 0x66
kmCall(0x8062ec18, AddChangeComboPages);  // 0x67

ExpCharacterSelect::ExpCharacterSelect() : rouletteCounter(-1) {
    randomizedCharIdx[0] = CHARACTER_NONE;
    randomizedCharIdx[1] = CHARACTER_NONE;
    rolledCharIdx[0] = CHARACTER_NONE;
    rolledCharIdx[1] = CHARACTER_NONE;
}

void ExpCharacterSelect::OnActivate() {
    Pages::CharacterSelect::OnActivate();

    CtrlMenuCharacterSelect::ButtonDriver *buttons = ctrlMenuCharSelect.driverButtonsArray;
    if (buttons == nullptr) return;

    const u32 playerBitfield = GetPlayerBitfield();
    const u32 miiCSlot = RetroRewind::System::BUTTON_MII_C;
    CtrlMenuCharacterSelect::ButtonDriver &miiCButton = buttons[miiCSlot];
    const CharacterId miiCCharacter = GetCharacterForSlot(miiCSlot, 0);
    if (miiCCharacter != CHARACTER_NONE) miiCButton.buttonId = miiCCharacter;

    bool miiCUnlocked = Restrictions::IsOnlyMiiOutfitCEnabled();
    RKSYS::Mgr *rksys = RKSYS::Mgr::sInstance;
    if (!miiCUnlocked && rksys != nullptr) miiCUnlocked = PointRating::GetUserVR(rksys->curLicenseId) >= 300.0f;

    CtrlMenuCharacterSelect::ButtonDriver *defaultButton = nullptr;
    if (!Restrictions::IsCharacterRestrictionEnabled()) {
        miiCButton.SetPlayerBitfield(miiCUnlocked ? playerBitfield : 0);
        if (!miiCUnlocked) {
            miiCButton.SetPicturePane("chara", "cha_26_hatena");
            miiCButton.SetPicturePane("chara_shadow", "cha_26_hatena");
            miiCButton.SetPicturePane("chara_light_01", "cha_26_hatena");
            miiCButton.SetPicturePane("chara_light_02", "cha_26_hatena");
            miiCButton.SetPicturePane("chara_c_down", "cha_26_hatena");
            if (miiCButton.IsSelected()) defaultButton = &buttons[RetroRewind::System::BUTTON_MARIO];
        }
    } else {
        for (u32 slot = 0; slot < Restrictions::CHARACTER_SLOT_COUNT; ++slot) {
            CtrlMenuCharacterSelect::ButtonDriver &button = buttons[slot];
            const bool enabled = Restrictions::IsCharacterSlotEnabled(slot) && (slot != miiCSlot || miiCUnlocked);
            if (enabled) {
                button.SetPlayerBitfield(playerBitfield);
                if (defaultButton == nullptr) defaultButton = &button;
                continue;
            }

            button.SetPlayerBitfield(0);
            button.SetPicturePane("chara", "cha_26_hatena");
            button.SetPicturePane("chara_shadow", "cha_26_hatena");
            button.SetPicturePane("chara_light_01", "cha_26_hatena");
            button.SetPicturePane("chara_light_02", "cha_26_hatena");
            button.SetPicturePane("chara_c_down", "cha_26_hatena");
        }
    }

    if (defaultButton == nullptr) return;
    for (u32 slot = 0; slot < Restrictions::CHARACTER_SLOT_COUNT; ++slot) {
        CtrlMenuCharacterSelect::ButtonDriver &button = buttons[slot];
        if (&button != defaultButton && button.IsSelected()) button.HandleDeselect(0, -1);
    }
    defaultButton->Select(0);
    defaultButton->SetButtonColours(0);
    OnButtonDriverSelect(defaultButton, defaultButton->buttonId, 0);
}

void ExpCharacterSelect::BeforeControlUpdate() {
    const s32 roulette = this->rouletteCounter;
    if (roulette > 0) {
        --this->rouletteCounter;
        this->controlsManipulatorManager.inaccessible = true;
    }
    if (this->buttonCooldown > 0) {
        --this->buttonCooldown;
    }
    for (int hudId = 0; hudId < SectionMgr::sInstance->sectionParams->localPlayerCount; ++hudId) {
        CharacterId prevChar = this->rolledCharIdx[hudId];
        Random random;
        const bool isGoodFrame = roulette % 4 == 1;
        if (roulette == 1)
            this->rolledCharIdx[hudId] = this->randomizedCharIdx[hudId];
        else if (isGoodFrame)
            this->rolledCharIdx[hudId] = GetRandomEnabledCharacter(random, hudId, prevChar);
        if (isGoodFrame) {
            this->ctrlMenuCharSelect.GetButtonDriver(prevChar)->HandleDeselect(hudId, -1);
            CtrlMenuCharacterSelect::ButtonDriver *nextButton = this->ctrlMenuCharSelect.GetButtonDriver(rolledCharIdx[hudId]);
            nextButton->HandleSelect(hudId, -1);
            nextButton->Select(0);

        } else if (roulette == 0) {
            if (this->buttonCooldown == 0) {
                this->ctrlMenuCharSelect.GetButtonDriver(randomizedCharIdx[hudId])->HandleClick(hudId, -1);
                if (Settings::Mgr::Get().GetSettingValue(Pulsar::Settings::SETTING_FASTMENUS) == FASTMENUS_ENABLED)
                    this->buttonCooldown = 30;
                else
                    this->buttonCooldown = 150;
            }
        }
    }
}

ExpBattleKartSelect::ExpBattleKartSelect() : selectedKart(-1) {}

void ExpBattleKartSelect::BeforeControlUpdate() {
    const s32 kart = this->selectedKart;
    if (kart >= 0 && this->currentState == 0x4) {
        this->controlsManipulatorManager.inaccessible = true;
        this->selectedKart = -1;
        PushButton *otherButton = this->controlGroup.GetControl<PushButton>(kart ^ 1);
        PushButton *kartButton = this->controlGroup.GetControl<PushButton>(kart);
        otherButton->HandleDeselect(0, -1);
        kartButton->HandleSelect(0, -1);
        kartButton->Select(0);
        kartButton->HandleClick(0, -1);
    }
}

ExpKartSelect::ExpKartSelect() : randomizedKartPos(-1), rolledKartPos(-1), rouletteCounter(-1) {}

void ExpKartSelect::OnActivate() {
    Pages::KartSelect::OnActivate();
    if (!Restrictions::IsVehicleRestrictionEnabled()) return;

    const u32 weight = GetCharacterWeightClass(SectionMgr::sInstance->sectionParams->characters[0]);
    const u32 position = Restrictions::GetFirstEnabledVehiclePosition(weight);
    ButtonMachine *button = GetButtonMachineById(static_cast<u8>(kartsSortedByWeight[weight][position]));
    if (button == nullptr) return;
    button->SelectInitial(0);
    button->HandleSelect(0, -1);
    OnExternalButtonSelect(*button, 0);
}

void ExpKartSelect::BeforeControlUpdate() {
    s32 roulette = this->rouletteCounter;
    if (roulette > 0) {
        this->controlsManipulatorManager.inaccessible = true;
        Random random;
        const u32 prevRoll = this->rolledKartPos;
        ButtonMachine *prevButton = this->GetKartButton(prevRoll);
        prevButton->HandleDeselect(0, -1);

        u32 nextRoll = prevRoll;
        const bool isGoodFrame = roulette % 4 == 1;
        if (roulette == 1)
            nextRoll = this->randomizedKartPos;
        else if (isGoodFrame)
            nextRoll = GetRandomEnabledVehiclePosition(random, GetCharacterWeightClass(SectionMgr::sInstance->sectionParams->characters[0]), prevRoll);
        if (isGoodFrame) {
            ButtonMachine *nextButton = this->GetKartButton(nextRoll);
            nextButton->HandleSelect(0, -1);
            nextButton->Select(0);
            this->rolledKartPos = nextRoll;
        }
        this->rouletteCounter--;
    } else if (roulette == 0) {
        this->rouletteCounter = -1;
        this->GetKartButton(this->randomizedKartPos)->HandleClick(0, -1);
    }
}

ButtonMachine *ExpKartSelect::GetKartButton(u32 idx) const {
    const u32 weight = GetCharacterWeightClass(SectionMgr::sInstance->sectionParams->characters[0]);
    return const_cast<ExpKartSelect *>(this)->GetButtonMachineById(static_cast<u8>(kartsSortedByWeight[weight][idx]));
}

ExpMultiKartSelect::ExpMultiKartSelect() : rouletteCounter(-1) {
    rolledKartPos[0] = -1;
    rolledKartPos[1] = -1;
}

void ExpMultiKartSelect::BeforeControlUpdate() {
    Random random;
    const s32 roulette = this->rouletteCounter;
    if (roulette > 0) {
        this->rouletteCounter--;
        this->controlsManipulatorManager.inaccessible = true;
    }
    for (int hudId = 0; hudId < SectionMgr::sInstance->sectionParams->localPlayerCount; ++hudId) {
        if (roulette == ExpVR::randomDuration) this->arrows[hudId].SelectInitial(this->rolledKartPos[hudId]);
        if (roulette > 8) {
            const bool isGoodFrame = roulette % 4 == 1;
            if (isGoodFrame) {
                if (random.NextLimited(2) == 0)
                    this->arrows[hudId].HandleRightPress(hudId, -1);
                else
                    this->arrows[hudId].HandleLeftPress(hudId, 0);
            }
        } else if (roulette == 0) {
            this->arrows[hudId].HandleClick(hudId, -1);
            this->nextPageId = PAGE_VOTE;
            this->EndStateAnimated(0, 0.0f);
        }
    }
}

void StopRandomComboRoulette() {
    ExpCharacterSelect *charSelect = SectionMgr::sInstance->curSection->Get<ExpCharacterSelect>();
    if (charSelect != nullptr && charSelect->rouletteCounter != -1) {
        charSelect->rouletteCounter = -1;
    }
}

void DriftSelectBeforeControlUpdate(Pages::DriftSelect *driftSelect) {
    ExpCharacterSelect *charSelect = SectionMgr::sInstance->curSection->Get<ExpCharacterSelect>();
    if (charSelect->rouletteCounter != -1 && driftSelect->currentState == 0x4) {
        driftSelect->controlsManipulatorManager.inaccessible = false;
        StopRandomComboRoulette();
    }
}
kmWritePointer(0x808D9DF8, DriftSelectBeforeControlUpdate);

void MultiDriftSelectBeforeControlUpdate(Pages::MultiDriftSelect *multiDriftSelect) {
    SectionMgr *sectionMgr = SectionMgr::sInstance;
    ExpCharacterSelect *charSelect = sectionMgr->curSection->Get<ExpCharacterSelect>();
    if (charSelect->rouletteCounter != -1 && multiDriftSelect->currentState == 0x4) {
        multiDriftSelect->controlsManipulatorManager.inaccessible = false;
        StopRandomComboRoulette();
    }
}
kmWritePointer(0x808D9C10, MultiDriftSelectBeforeControlUpdate);

void AddCharSelectLayer(Pages::SELECTStageMgr &page, PageId id, u32 animDirection) {
    const System *system = System::sInstance;
    const ExpVR *votingPage = SectionMgr::sInstance->curSection->Get<ExpVR>();  // always present when 0x90 is present
    if (system->IsContext(PULSAR_MODE_KO) && system->koMgr->isSpectating) {
        id = PAGE_VOTE;
        page.status = Pages::SELECTStageMgr::STATUS_VOTES_PAGE;
    } else if (votingPage->comboButtonState != 0)
        id = PAGE_CHARACTER_SELECT;
    page.AddPageLayer(id, animDirection);
}
kmCall(0x806509d0, AddCharSelectLayer);

asmFunc LoadCorrectPageAfterDrift() {  // r0 has gamemode
    ASM(
        nofralloc;
        cmpwi r0, MODE_PUBLIC_BATTLE;
        beq - isBattle;
        cmpwi r0, MODE_PRIVATE_BATTLE;
        bne + end;
        isBattle : li r0, 3;
        end : cmpwi r0, 3;
        blr;)
}
kmCall(0x8084e670, LoadCorrectPageAfterDrift);

}  // namespace UI
}  // namespace Pulsar
