#include <core/nw4r/ut/Misc.hpp>
#include <MarioKartWii/GlobalFunctions.hpp>
#include <MarioKartWii/UI/Page/Other/SELECTStageMgr.hpp>
#include <UI/ChangeCombo/ChangeCombo.hpp>
#include <PulsarSystem.hpp>
#include <Gamemodes/KO/KOMgr.hpp>
#include <RetroRewind.hpp>
#include <MarioKartWii/UI/Ctrl/Menu/CtrlMenuCharacterSelect.hpp>
#include <MarioKartWii/UI/Page/Menu/CharacterSelect.hpp>
#include <MarioKartWii/GlobalFunctions.hpp>
#include <Settings/UI/SettingsPanel.hpp>
#include <MarioKartWii/UI/Page/Other/VR.hpp>
#include <MarioKartWii/UI/Page/Other/SELECTStageMgr.hpp>
#include <MarioKartWii/UI/Ctrl/CountDown.hpp>

namespace Pulsar {
namespace UI {

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
    const RKNet::Controller* controller = RKNet::Controller::sInstance;
    const System* system = System::sInstance;

    const Section* curSection = SectionMgr::sInstance->curSection;
    Pages::SELECTStageMgr* selectStageMgr = curSection->Get<Pages::SELECTStageMgr>();
    CountDown* timer = &selectStageMgr->countdown;

    bool isKOd = false;
    if (system->IsContext(PULSAR_MODE_KO) && system->koMgr->isSpectating) isKOd = true;
    if (System::sInstance->IsContext(PULSAR_MODE_OTT) && system->IsContext(PULSAR_CHANGECOMBO) == OTTSETTING_COMBO_ENABLED) isKOd = true;
    if (System::sInstance->IsContext(PULSAR_MODE_OTT) && ((RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_VS_REGIONAL) || (RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_JOINING_REGIONAL))) isKOd = true;

    bool isRandomHidden = false;
    if (Settings::Mgr::Get().GetUserSettingValue(Settings::SETTINGSTYPE_ONLINE, RADIO_ONLINERANDOMBUTTON) == RANDOMBUTTON_DISABLED) isRandomHidden = true;

    this->AddControl(0xF, this->randomComboButton, 0);
    this->randomComboButton.isHidden = isKOd || isRandomHidden;
    this->randomComboButton.Load(UI::buttonFolder, "PULiMemberConfirmButton", "Random", 1, 0, isKOd || isRandomHidden);
    this->randomComboButton.SetOnClickHandler(this->onRandomComboClick, 0);

    this->AddControl(0x10, this->changeComboButton, 0);
    this->changeComboButton.isHidden = isKOd;
    this->changeComboButton.Load(UI::buttonFolder, "PULiMemberConfirmButton", "Change", 1, 0, isKOd);
    this->changeComboButton.SetOnClickHandler(this->onChangeComboClick, 0);
    // this->changeComboButton.manipulator.SetAction(START_PRESS, this->changeComboButton.onClickHandlerObj, 0);

    this->AddControl(0x11, settingsButton, 0);
    this->settingsButton.Load(UI::buttonFolder, "SettingsVR", "Settings", 1, 0, hideSettings);
    this->settingsButton.buttonId = 5;
    this->settingsButton.SetOnClickHandler(this->onSettingsClick, 0);
    this->settingsButton.SetOnSelectHandler(this->onButtonSelectHandler);
    this->topSettingsPage = SettingsPanel::id;

    // Share timer with settings panel
    SettingsPanel* settingsPanel = ExpSection::GetSection()->GetPulPage<SettingsPanel>();
    settingsPanel->timer = timer;

    Pages::CharacterSelect* charPage = curSection->Get<Pages::CharacterSelect>();
    charPage->timer = timer;
    charPage->ctrlMenuCharSelect.timer = timer;

    Pages::KartSelect* kartPage = curSection->Get<Pages::KartSelect>();
    if (kartPage != nullptr) kartPage->timer = timer;

    Pages::BattleKartSelect* kartBattlePage = curSection->Get<Pages::BattleKartSelect>();
    if (kartBattlePage != nullptr) kartBattlePage->timer = timer;

    Pages::MultiKartSelect* multiKartPage = curSection->Get<Pages::MultiKartSelect>();
    if (multiKartPage != nullptr) multiKartPage->timer = timer;

    Pages::DriftSelect* driftPage = curSection->Get<Pages::DriftSelect>();
    if (driftPage != nullptr) driftPage->timer = timer;

    Pages::MultiDriftSelect* multiDriftPage = curSection->Get<Pages::MultiDriftSelect>();
    if (multiDriftPage != nullptr) {
        multiDriftPage->nextSectionOnButtonClick = SECTION_NONE;
        multiDriftPage->timer = timer;
    }
}

static void RandomizeCombo() {
    Random random;
    const SectionMgr* sectionMgr = SectionMgr::sInstance;
    bool charRestrictLight = Pulsar::CHAR_DEFAULTSELECTION;
    bool charRestrictMid = Pulsar::CHAR_DEFAULTSELECTION;
    bool charRestrictHeavy = Pulsar::CHAR_DEFAULTSELECTION;
    bool kartRest = Pulsar::KART_DEFAULTSELECTION;
    bool bikeRest = Pulsar::KART_DEFAULTSELECTION;
    if (RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_FROOM_HOST || RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_FROOM_NONHOST) {
        charRestrictLight = System::sInstance->IsContext(Pulsar::PULSAR_CHARRESTRICTLIGHT) ? Pulsar::CHAR_LIGHTONLY : Pulsar::CHAR_DEFAULTSELECTION;
        charRestrictMid = System::sInstance->IsContext(Pulsar::PULSAR_CHARRESTRICTMID) ? Pulsar::CHAR_MEDIUMONLY : Pulsar::CHAR_DEFAULTSELECTION;
        charRestrictHeavy = System::sInstance->IsContext(Pulsar::PULSAR_CHARRESTRICTHEAVY) ? Pulsar::CHAR_HEAVYONLY : Pulsar::CHAR_DEFAULTSELECTION;
        kartRest = System::sInstance->IsContext(Pulsar::PULSAR_KARTRESTRICT) ? Pulsar::KART_KARTONLY : Pulsar::KART_DEFAULTSELECTION;
        bikeRest = System::sInstance->IsContext(Pulsar::PULSAR_BIKERESTRICT) ? Pulsar::KART_BIKEONLY : Pulsar::KART_DEFAULTSELECTION;
    }
    const Section* section = sectionMgr->curSection;
    SectionParams* sectionParams = sectionMgr->sectionParams;
    for (int hudId = 0; hudId < sectionParams->localPlayerCount; ++hudId) {
        CharacterId character = random.NextLimited<CharacterId>(24);
        if (charRestrictLight == CHAR_LIGHTONLY) {
            character = static_cast<CharacterId>(CtrlMenuCharacterSelect::buttonIdToCharacterId[static_cast<RetroRewind::System::CharButtonId>(random.NextLimited<u8>(8))]);
        }
        if (charRestrictMid == CHAR_MEDIUMONLY) {
            character = static_cast<CharacterId>(CtrlMenuCharacterSelect::buttonIdToCharacterId[static_cast<RetroRewind::System::CharButtonId>(random.NextLimited<u8>(8) + 8)]);
        }
        if (charRestrictHeavy == CHAR_HEAVYONLY) {
            character = static_cast<CharacterId>(CtrlMenuCharacterSelect::buttonIdToCharacterId[static_cast<RetroRewind::System::CharButtonId>(random.NextLimited<u8>(8) + 16)]);
        }
        u8 kartCount = 12;
        if (kartRest == KART_KARTONLY) {
            kartCount = 6;  // Set options for kart only
        }
        if (bikeRest == KART_BIKEONLY) {
            kartCount = 6;  // Set options for bike only
        }
        const u32 randomizedKartPos = random.NextLimited(kartCount);
        const KartId kart = kartsSortedByWeight[GetCharacterWeightClass(character)][randomizedKartPos];

        sectionParams->characters[hudId] = character;
        sectionParams->karts[hudId] = kart;
        sectionParams->combos[hudId].selCharacter = character;
        sectionParams->combos[hudId].selKart = kart;

        ExpCharacterSelect* charSelect = section->Get<ExpCharacterSelect>();  // guaranteed to exist on this page
        charSelect->randomizedCharIdx[hudId] = character;
        charSelect->rolledCharIdx[hudId] = character;
        charSelect->rouletteCounter = ExpVR::randomDuration;
        charSelect->ctrlMenuCharSelect.selectedCharacter = character;
        charSelect->controlsManipulatorManager.inaccessible = true;
        ExpBattleKartSelect* battleKartSelect = section->Get<ExpBattleKartSelect>();
        if (battleKartSelect != nullptr) {
            battleKartSelect->selectedKart = random.NextLimited(2);
            battleKartSelect->controlsManipulatorManager.inaccessible = true;
        }

        ExpKartSelect* kartSelect = section->Get<ExpKartSelect>();
        if (kartSelect != nullptr) {
            kartSelect->rouletteCounter = ExpVR::randomDuration;
            kartSelect->randomizedKartPos = randomizedKartPos;
            kartSelect->rolledKartPos = randomizedKartPos;
            kartSelect->controlsManipulatorManager.inaccessible = true;
        }

        ExpMultiKartSelect* multiKartSelect = section->Get<ExpMultiKartSelect>();
        if (multiKartSelect != nullptr) {
            multiKartSelect->rouletteCounter = ExpVR::randomDuration;
            multiKartSelect->rolledKartPos[0] = randomizedKartPos;
            u32 options = 12;
            if (kartRest == KART_KARTONLY) {
                options = 6;  // Set options for kart only
            }
            if (bikeRest == KART_BIKEONLY) {
                options = 6;  // Set options for bike only
            }
            if (IsBattle()) options = 2;
            multiKartSelect->rolledKartPos[1] = random.NextLimited(options);
            multiKartSelect->controlsManipulatorManager.inaccessible = true;
        }
    }
}

void ExpVR::RandomizeComboVR(PushButton& randomComboButton, u32 hudSlotId) {
    this->comboButtonState = 1;
    this->EndStateAnimated(0, randomComboButton.GetAnimationFrameSize());
    RandomizeCombo();
}

void ExpVR::ChangeCombo(PushButton& changeComboButton, u32 hudSlotId) {
    this->comboButtonState = 2;
    this->EndStateAnimated(0, changeComboButton.GetAnimationFrameSize());
}

void ExpVR::OnSettingsButtonClick(PushButton& button, u32 hudSlotId) {
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
    SettingsPanel* settingsPanel = ExpSection::GetSection()->GetPulPage<SettingsPanel>();
    settingsPanel->prevPageId = PAGE_VR;
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
        const System* system = System::sInstance;
        bool isKOd = false;
        if (system->IsContext(PULSAR_MODE_KO) && system->koMgr->isSpectating) isKOd = true;
        if (system->IsContext(PULSAR_MODE_OTT) && system->IsContext(PULSAR_CHANGECOMBO) == OTTSETTING_COMBO_ENABLED) isKOd = true;
        if (System::sInstance->IsContext(PULSAR_MODE_OTT) && ((RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_VS_REGIONAL) || (RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_JOINING_REGIONAL))) isKOd = true;

        bool isRandomHidden = false;
        if (Settings::Mgr::Get().GetUserSettingValue(Settings::SETTINGSTYPE_ONLINE, RADIO_ONLINERANDOMBUTTON) == RANDOMBUTTON_DISABLED) isRandomHidden = true;

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

void ExpVR::OnResume() {
    if (this->areControlsHidden) this->areControlsHidden = false;
    this->shouldRestoreControls = true;
    VR::OnResume();
}

void ExpVR::ExtOnButtonSelect(PushButton& button, u32 hudSlotId) {
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

static void AddChangeComboPages(Section* section, PageId id) {
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

// 58 59 5e 5f 60 61
// P1
kmCall(0x8062e09c, AddChangeComboPages);  // 0x58 can't do this more efficiently because supporting page 0x7F breaks kart images
kmCall(0x8062e7e0, AddChangeComboPages);  // 0x60
kmCall(0x8062e870, AddChangeComboPages);  // 0x61
kmCall(0x8062e0e4, AddChangeComboPages);  // 0x59
kmCall(0x8062e900, AddChangeComboPages);  // 0x62
kmCall(0x8062e990, AddChangeComboPages);  // 0x63
// P2
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

void ExpCharacterSelect::BeforeControlUpdate() {
    // CtrlMenuCharacterSelect::ButtonDriver* array = this->ctrlMenuCharSelect.driverButtonsArray;

    const s32 roulette = this->rouletteCounter;
    bool charRestrictLight = Pulsar::CHAR_DEFAULTSELECTION;
    bool charRestrictMid = Pulsar::CHAR_DEFAULTSELECTION;
    bool charRestrictHeavy = Pulsar::CHAR_DEFAULTSELECTION;
    if (RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_FROOM_HOST || RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_FROOM_NONHOST) {
        charRestrictLight = System::sInstance->IsContext(Pulsar::PULSAR_CHARRESTRICTLIGHT) ? Pulsar::CHAR_LIGHTONLY : Pulsar::CHAR_DEFAULTSELECTION;
        charRestrictMid = System::sInstance->IsContext(Pulsar::PULSAR_CHARRESTRICTMID) ? Pulsar::CHAR_MEDIUMONLY : Pulsar::CHAR_DEFAULTSELECTION;
        charRestrictHeavy = System::sInstance->IsContext(Pulsar::PULSAR_CHARRESTRICTHEAVY) ? Pulsar::CHAR_HEAVYONLY : Pulsar::CHAR_DEFAULTSELECTION;
    }
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
            while (this->rolledCharIdx[hudId] == prevChar) {
                this->rolledCharIdx[hudId] = static_cast<CharacterId>(random.NextLimited(24));
                if (charRestrictLight == CHAR_LIGHTONLY) {
                    this->rolledCharIdx[hudId] = static_cast<CharacterId>(CtrlMenuCharacterSelect::buttonIdToCharacterId[static_cast<RetroRewind::System::CharButtonId>(random.NextLimited<u8>(8))]);
                }
                if (charRestrictMid == CHAR_MEDIUMONLY) {
                    this->rolledCharIdx[hudId] = static_cast<CharacterId>(CtrlMenuCharacterSelect::buttonIdToCharacterId[static_cast<RetroRewind::System::CharButtonId>(random.NextLimited<u8>(8) + 8)]);
                }
                if (charRestrictHeavy == CHAR_HEAVYONLY) {
                    this->rolledCharIdx[hudId] = static_cast<CharacterId>(CtrlMenuCharacterSelect::buttonIdToCharacterId[static_cast<RetroRewind::System::CharButtonId>(random.NextLimited<u8>(8) + 16)]);
                }
            }
        if (isGoodFrame) {
            this->ctrlMenuCharSelect.GetButtonDriver(prevChar)->HandleDeselect(hudId, -1);
            CtrlMenuCharacterSelect::ButtonDriver* nextButton = this->ctrlMenuCharSelect.GetButtonDriver(rolledCharIdx[hudId]);
            nextButton->HandleSelect(hudId, -1);
            nextButton->Select(0);
            // array[prevChar].HandleDeselect(0, -1);
            // array[this->rolledCharIdx].HandleSelect(0, -1);

        } else if (roulette == 0) {
            if (this->buttonCooldown == 0) {
                this->ctrlMenuCharSelect.GetButtonDriver(randomizedCharIdx[hudId])->HandleClick(hudId, -1);
                if (Settings::Mgr::Get().GetUserSettingValue(Settings::SETTINGSTYPE_MENU, RADIO_FASTMENUS) == FASTMENUS_ENABLED)
                    this->buttonCooldown = 30;
                else
                    this->buttonCooldown = 150;
            }
        }
    }

    // array[this->randomizedCharIdx].HandleClick(0, -1);
}
// store correct buttons in sectionParams

ExpBattleKartSelect::ExpBattleKartSelect() : selectedKart(-1) {}

void ExpBattleKartSelect::BeforeControlUpdate() {
    const s32 kart = this->selectedKart;
    if (kart >= 0 && this->currentState == 0x4) {
        this->controlsManipulatorManager.inaccessible = true;
        this->selectedKart = -1;
        PushButton* otherButton = this->controlGroup.GetControl<PushButton>(kart ^ 1);
        PushButton* kartButton = this->controlGroup.GetControl<PushButton>(kart);
        otherButton->HandleDeselect(0, -1);
        kartButton->HandleSelect(0, -1);
        kartButton->Select(0);
        kartButton->HandleClick(0, -1);
    }
}

ExpKartSelect::ExpKartSelect() : randomizedKartPos(-1), rolledKartPos(-1), rouletteCounter(-1) {}

void ExpKartSelect::BeforeControlUpdate() {
    s32 roulette = this->rouletteCounter;
    bool kartRest = Pulsar::KART_DEFAULTSELECTION;
    bool bikeRest = Pulsar::KART_DEFAULTSELECTION;
    if (RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_FROOM_HOST || RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_FROOM_NONHOST) {
        kartRest = System::sInstance->IsContext(Pulsar::PULSAR_KARTRESTRICT) ? Pulsar::KART_KARTONLY : Pulsar::KART_DEFAULTSELECTION;
        bikeRest = System::sInstance->IsContext(Pulsar::PULSAR_BIKERESTRICT) ? Pulsar::KART_BIKEONLY : Pulsar::KART_DEFAULTSELECTION;
    }
    if (roulette > 0) {
        this->controlsManipulatorManager.inaccessible = true;
        Random random;
        const u32 prevRoll = this->rolledKartPos;
        ButtonMachine* prevButton = this->GetKartButton(prevRoll);
        prevButton->HandleDeselect(0, -1);

        u32 nextRoll = prevRoll;
        const bool isGoodFrame = roulette % 4 == 1;
        u8 kartCount = 12;
        if (kartRest == KART_KARTONLY) {
            kartCount = 6;  // Set options for kart only
        }
        if (bikeRest == KART_BIKEONLY) {
            kartCount = 6;  // Set options for bike only
        }
        if (roulette == 1)
            nextRoll = this->randomizedKartPos;
        else if (isGoodFrame)
            while (nextRoll == prevRoll) nextRoll = random.NextLimited(kartCount);
        if (isGoodFrame) {
            ButtonMachine* nextButton = this->GetKartButton(nextRoll);
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

ButtonMachine* ExpKartSelect::GetKartButton(u32 idx) const {
    bool kartRest = Pulsar::KART_DEFAULTSELECTION;
    bool bikeRest = Pulsar::KART_DEFAULTSELECTION;
    if (RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_FROOM_HOST || RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_FROOM_NONHOST) {
        kartRest = System::sInstance->IsContext(Pulsar::PULSAR_KARTRESTRICT) ? Pulsar::KART_KARTONLY : Pulsar::KART_DEFAULTSELECTION;
        bikeRest = System::sInstance->IsContext(Pulsar::PULSAR_BIKERESTRICT) ? Pulsar::KART_BIKEONLY : Pulsar::KART_DEFAULTSELECTION;
    }
    u8 buttonsPerRow = 2;
    if (kartRest == KART_KARTONLY) {
        buttonsPerRow = 1;  // Set options for kart only
    }
    if (bikeRest == KART_BIKEONLY) {
        buttonsPerRow = 1;  // Set options for bike only
    }
    const UIControl* globalButtonHolder = this->controlGroup.GetControl(buttonsPerRow);  // holds the 6 controls (6 rows) that each hold a pair of buttons
    return globalButtonHolder->childrenGroup.GetControl(idx / buttonsPerRow)->childrenGroup.GetControl<ButtonMachine>(idx % buttonsPerRow);
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

void DriftSelectBeforeControlUpdate(Pages::DriftSelect* driftSelect) {
    ExpCharacterSelect* charSelect = SectionMgr::sInstance->curSection->Get<ExpCharacterSelect>();
    if (charSelect->rouletteCounter != -1 && driftSelect->currentState == 0x4) {
        driftSelect->controlsManipulatorManager.inaccessible = false;
        charSelect->rouletteCounter = -1;
    }
}
kmWritePointer(0x808D9DF8, DriftSelectBeforeControlUpdate);

void MultiDriftSelectBeforeControlUpdate(Pages::MultiDriftSelect* multiDriftSelect) {
    SectionMgr* sectionMgr = SectionMgr::sInstance;
    ExpCharacterSelect* charSelect = sectionMgr->curSection->Get<ExpCharacterSelect>();
    if (charSelect->rouletteCounter != -1 && multiDriftSelect->currentState == 0x4) {
        multiDriftSelect->controlsManipulatorManager.inaccessible = false;
        charSelect->rouletteCounter = -1;
    }
}
kmWritePointer(0x808D9C10, MultiDriftSelectBeforeControlUpdate);

void AddCharSelectLayer(Pages::SELECTStageMgr& page, PageId id, u32 animDirection) {
    const System* system = System::sInstance;
    const ExpVR* votingPage = SectionMgr::sInstance->curSection->Get<ExpVR>();  // always present when 0x90 is present
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

void SettingsPanel::BeforeControlUpdate() {
    SectionId id = SectionMgr::sInstance->curSection->sectionId;
    bool isVotingSection = (id >= SECTION_P1_WIFI_FROOM_VS_VOTING && id <= SECTION_P2_WIFI_FROOM_COIN_VOTING) || (id == SECTION_P1_WIFI_VS_VOTING);
    if (isVotingSection) {
        Pages::SELECTStageMgr* selectStageMgr = SectionMgr::sInstance->curSection->Get<Pages::SELECTStageMgr>();
        CountDown* timer = &selectStageMgr->countdown;
        if (timer->countdown <= 0) {
            this->OnBackPress(0);
        }
    }
}

}  // namespace UI
}  // namespace Pulsar
