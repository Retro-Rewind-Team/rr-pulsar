#include <RetroRewind.hpp>
#include <MarioKartWii/UI/Ctrl/Menu/CtrlMenuCharacterSelect.hpp>
#include <MarioKartWii/UI/Page/Menu/CharacterSelect.hpp>
#include <MarioKartWii/GlobalFunctions.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>

// Original code from VP, adapted to Pulsar 2.0.
namespace RetroRewind {
namespace UI {

static bool IsFriendRoom() {
    const RKNet::RoomType roomType = RKNet::Controller::sInstance->roomType;
    return roomType == RKNet::ROOMTYPE_FROOM_HOST || roomType == RKNet::ROOMTYPE_FROOM_NONHOST;
}

System::WeightClass GetMiiWeightClass(Mii &mii) {
    CharacterId charId = GetMiiCharacterId(mii);
    if (charId < MII_M_A_MALE) {
        return System::LIGHTWEIGHT;
    } else if (charId < MII_L_A_MALE) {
        return System::MEDIUMWEIGHT;
    }
    return System::HEAVYWEIGHT;
}

void EnableButtons(CtrlMenuCharacterSelect &charSelect) {
    for (u8 i = 0; i < 42; i++) {
        CtrlMenuCharacterSelect::ButtonDriver *buttonDriver = charSelect.GetButtonDriver(static_cast<CharacterId>(i));
        if (buttonDriver != 0) {
            buttonDriver->SetPicturePane("chara", GetCharacterIconPaneName(static_cast<CharacterId>(i)));
            buttonDriver->SetPicturePane("chara_shadow", GetCharacterIconPaneName(static_cast<CharacterId>(i)));
            buttonDriver->SetPicturePane("chara_light_01", GetCharacterIconPaneName(static_cast<CharacterId>(i)));
            buttonDriver->SetPicturePane("chara_light_02", GetCharacterIconPaneName(static_cast<CharacterId>(i)));
            buttonDriver->SetPicturePane("chara_c_down", GetCharacterIconPaneName(static_cast<CharacterId>(i)));
            buttonDriver->manipulator.inaccessible = false;
        }
    }
}

void DisableButton(CtrlMenuCharacterSelect::ButtonDriver *button) {
    button->SetPicturePane("chara", "cha_26_hatena");
    button->SetPicturePane("chara_shadow", "cha_26_hatena");
    button->SetPicturePane("chara_light_01", "cha_26_hatena");
    button->SetPicturePane("chara_light_02", "cha_26_hatena");
    button->SetPicturePane("chara_c_down", "cha_26_hatena");

    button->manipulator.inaccessible = true;
}

void RestrictCharacterSelection(PushButton *button, u32 hudSlotId) {
    Pages::CharacterSelect *page = SectionMgr::sInstance->curSection->Get<Pages::CharacterSelect>();
    CtrlMenuCharacterSelect &charSelect = page->ctrlMenuCharSelect;
    SectionId curSection = SectionMgr::sInstance->curSection->sectionId;
    Pulsar::CharacterRestriction charRestrictLight = Pulsar::CHAR_DEFAULTSELECTION;
    Pulsar::CharacterRestriction charRestrictMid = Pulsar::CHAR_DEFAULTSELECTION;
    Pulsar::CharacterRestriction charRestrictHeavy = Pulsar::CHAR_DEFAULTSELECTION;
    if (IsFriendRoom()) {
        charRestrictLight = System::sInstance->IsContext(Pulsar::PULSAR_CHARRESTRICTLIGHT) ? Pulsar::CHAR_LIGHTONLY : Pulsar::CHAR_DEFAULTSELECTION;
        charRestrictMid = System::sInstance->IsContext(Pulsar::PULSAR_CHARRESTRICTMID) ? Pulsar::CHAR_MEDIUMONLY : Pulsar::CHAR_DEFAULTSELECTION;
        charRestrictHeavy = System::sInstance->IsContext(Pulsar::PULSAR_CHARRESTRICTHEAVY) ? Pulsar::CHAR_HEAVYONLY : Pulsar::CHAR_DEFAULTSELECTION;
    }
    CtrlMenuCharacterSelect::ButtonDriver *driverButtons = charSelect.driverButtonsArray;
    System::WeightClass miiWeight = GetMiiWeightClass(page->localPlayerMiis[0]);

    EnableButtons(charSelect);

    if (charRestrictLight != Pulsar::CHAR_DEFAULTSELECTION || charRestrictMid != Pulsar::CHAR_DEFAULTSELECTION || charRestrictHeavy != Pulsar::CHAR_DEFAULTSELECTION) {
        for (int i = System::BUTTON_BABY_MARIO; i < System::BUTTON_MII_A; i++) {
            bool restrictButton = false;

            if (charRestrictLight == Pulsar::CHAR_LIGHTONLY) {
                restrictButton = (i >= System::BUTTON_MARIO && i < System::BUTTON_MII_A);
            }
            if (charRestrictMid == Pulsar::CHAR_MEDIUMONLY) {
                restrictButton = (i >= System::BUTTON_BABY_MARIO && i < System::BUTTON_MARIO) ||
                                 (i >= System::BUTTON_WARIO && i < System::BUTTON_MII_A);
            }
            if (charRestrictHeavy == Pulsar::CHAR_HEAVYONLY) {
                restrictButton = (i >= System::BUTTON_BABY_MARIO && i < System::BUTTON_WARIO);
            }

            if (restrictButton) {
                DisableButton(&driverButtons[i]);
            }
        }
    }

    if (curSection == SECTION_P2_WIFI ||
        curSection == SECTION_P2_WIFI_FROOM_VS_VOTING ||
        curSection == SECTION_P2_WIFI_FROOM_TEAMVS_VOTING ||
        curSection == SECTION_P2_WIFI_FROOM_BALLOON_VOTING ||
        curSection == SECTION_P2_WIFI_FROOM_COIN_VOTING ||
        (charRestrictLight == Pulsar::CHAR_LIGHTONLY && miiWeight != System::LIGHTWEIGHT) ||
        (charRestrictMid == Pulsar::CHAR_MEDIUMONLY && miiWeight != System::MEDIUMWEIGHT) ||
        (charRestrictHeavy == Pulsar::CHAR_HEAVYONLY && miiWeight != System::HEAVYWEIGHT)) {
        DisableButton(&driverButtons[System::BUTTON_MII_A]);
        DisableButton(&driverButtons[System::BUTTON_MII_B]);
    }

    bool currentAccessible = !button->manipulator.inaccessible;

    if (!currentAccessible) {
        CtrlMenuCharacterSelect::ButtonDriver *newButton = nullptr;
        for (int i = System::BUTTON_BABY_MARIO; i < System::BUTTON_MII_A; i++) {
            if (!driverButtons[i].manipulator.inaccessible) {
                newButton = &driverButtons[i];
                break;
            }
        }

        if (newButton) {
            button->HandleDeselect(hudSlotId, -1);
            newButton->SelectInitial(hudSlotId);
            newButton->SetButtonColours(hudSlotId);
            page->OnButtonDriverSelect(newButton, newButton->buttonId, hudSlotId);
        }
    }
}
kmCall(0x807e33a8, RestrictCharacterSelection);
}  // namespace UI
}  // namespace RetroRewind
