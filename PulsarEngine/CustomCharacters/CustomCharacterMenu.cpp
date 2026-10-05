#include <CustomCharacters/CustomCharacters.hpp>

namespace Pulsar {
namespace CustomCharacters {

static u16 heldToggleButtons[LOCAL_PLAYER_COUNT];

// Voting menus restore selected skin models after the vanilla random vote flow.
void RestoreVotingMenuDriverModels() {
    const SectionMgr *sectionMgr = SectionMgr::sInstance;
    if (sectionMgr == nullptr || sectionMgr->curSection == nullptr) return;
    const SectionId section = sectionMgr->curSection->sectionId;
    if (!IsVotingSection(section)) return;

    votingMenuTableSection = section;
    votingMenuTablesRestored = true;
    ApplySelectedNames();
    RefreshLocalOnlineCustomCharacterFlags();
    for (u8 character = 0; character < MENU_DRIVER_MODEL_COUNT; ++character) {
        const CharacterId characterId = static_cast<CharacterId>(character);
        if (SelectedTable(characterId) != TABLE_DEFAULT) RefreshMenuDriverModel(characterId);
    }

    if (sectionMgr->sectionParams == nullptr) return;
    const u8 count = SectionPlayerCount(sectionMgr);
    for (u8 hud = 0; hud < count; ++hud) {
        ReinitMenuDriverModelMgr(hud, sectionMgr->sectionParams->characters[hud]);
    }
}

bool RandomizeSelectedCharacterTable(CharacterId character) {
    if (!IsCharacter(StateCharacter(character))) return false;
    u8 valid[TABLE_COUNT];
    u8 count = 0;
    for (u8 table = 0; table < TABLE_COUNT; ++table) {
        if (HasSkin(character, table)) valid[count++] = table;
    }
    if (count == 0) return false;
    Random random;
    const bool changed = SetSelectedTable(character, valid[random.NextLimited<u8>(count)]);
    if (changed) ReinitMenuDriverModelMgr(0, character);
    return changed;
}

static bool CycleSkin(CharacterId character, int step) {
    if (!IsCharacter(StateCharacter(character))) return false;
    u8 table = SelectedTable(character);
    for (u8 i = 1; i < TABLE_COUNT; ++i) {
        table = step < 0 ? (table == 0 ? TABLE_COUNT - 1 : table - 1) : (table + 1 >= TABLE_COUNT ? TABLE_DEFAULT : table + 1);
        if (HasSkin(character, table) && SetSelectedTable(character, table)) return true;
    }
    return false;
}

void ResetCustomCharacterMenuState() {
    const SectionMgr *mgr = SectionMgr::sInstance;
    const SectionId section = mgr != nullptr && mgr->curSection != nullptr ? mgr->curSection->sectionId : SECTION_NONE;
    if (!IsVotingSection(section)) {
        votingMenuTableSection = SECTION_NONE;
        votingMenuTablesRestored = false;
    }
    if (!IsOnlineRoom(RKNet::Controller::sInstance)) ResetOnlineCustomCharacterFlags();
    ResetOfflineCpuSkinTablesForSection();
    SyncRawCachesToCurrentScene();
    if (mgr != nullptr && mgr->sectionParams != nullptr) {
        const u8 count = SectionPlayerCount(mgr);
        for (u8 hud = 0; hud < count; ++hud) hoveredCharacters[hud] = mgr->sectionParams->characters[hud];
    }
    ApplySelectedNames();
}
SectionLoadHook ResetCustomCharacterMenuStateHook(ResetCustomCharacterMenuState);

void MenuSceneSectionUpdateHook(SectionMgr *mgr) {
    const u8 count = SectionPlayerCount(mgr);
    if (!IsCharacterSelectActive() || mgr == nullptr || mgr->sectionParams == nullptr) {
        memset(heldToggleButtons, 0, sizeof(heldToggleButtons));
    } else {
        Pages::CharacterSelect *page = mgr->curSection->Get<Pages::CharacterSelect>();
        if (page != nullptr) {
            const char *hintPanes[] = {"cc_prev_wh", "cc_next_wh", "cc_prev_nc", "cc_next_nc", "cc_prev_cls", "cc_next_cls", "cc_prev_gc", "cc_next_gc"};
            const u8 *manipulator = reinterpret_cast<const u8 *>(&page->controlsManipulatorManager);
            for (u8 hud = 0; hud < count; ++hud) {
                Input::RealControllerHolder *holder = mgr->pad.padInfos[hud].controllerHolder;
                ControllerType type = GCN;
                if (holder != nullptr && holder->curController != nullptr) {
                    const ControllerType currentType = holder->curController->GetType();
                    if (currentType == WHEEL || currentType == NUNCHUCK || currentType == CLASSIC || currentType == GCN) type = currentType;
                }

                const bool canToggle = count <= 1 || manipulator[0xa4 + hud * 0x5c] != 0;
                if (page->names != nullptr) {
                    CharaName &name = page->names[hud];
                    for (u32 i = 0; i < ARRAY_COUNT(hintPanes); ++i) name.SetPaneVisibility(hintPanes[i], false);
                    if (canToggle) {
                        u32 offset = type == WHEEL ? 0 : type == NUNCHUCK ? 2
                                                     : type == CLASSIC    ? 4
                                                                          : 6;
                        name.SetPaneVisibility(hintPanes[offset], true);
                        name.SetPaneVisibility(hintPanes[offset + 1], true);
                    }
                }

                if (holder == nullptr || holder->curController == nullptr || !canToggle) {
                    heldToggleButtons[hud] = 0;
                    continue;
                }

                u16 prevButton;
                u16 nextButton;
                u16 prevAction = 0;
                u16 nextAction = 0;
                switch (type) {
                    case WHEEL:
                        prevButton = WPAD::WPAD_BUTTON_B;
                        nextButton = WPAD::WPAD_BUTTON_A;
                        prevAction = static_cast<u16>(1 << BACK_PRESS);
                        nextAction = static_cast<u16>(1 << FORWARD_PRESS);
                        break;
                    case NUNCHUCK:
                        prevButton = WPAD::WPAD_BUTTON_1;
                        nextButton = WPAD::WPAD_BUTTON_2;
                        prevAction = static_cast<u16>(1 << BACK_PRESS);
                        nextAction = static_cast<u16>(1 << FORWARD_PRESS);
                        break;
                    case CLASSIC:
                        prevButton = WPAD::WPAD_CL_TRIGGER_L;
                        nextButton = WPAD::WPAD_CL_TRIGGER_R;
                        break;
                    default:
                        prevButton = PAD::PAD_BUTTON_L;
                        nextButton = PAD::PAD_BUTTON_R;
                        break;
                }

                const u16 inputs = holder->inputStates[0].buttonRaw;
                const u16 buttons = static_cast<u16>(prevButton | nextButton);
                const u16 pressed = static_cast<u16>((inputs & buttons) & ~heldToggleButtons[hud]);
                heldToggleButtons[hud] = static_cast<u16>(inputs & buttons);
                if ((inputs & prevButton) != 0) {
                    holder->inputStates[0].buttonRaw &= static_cast<u16>(~prevButton);
                    holder->uiinputStates[0].rawButtons &= static_cast<u16>(~prevButton);
                    holder->uiinputStates[0].buttonActions &= static_cast<u16>(~prevAction);
                }
                if ((inputs & nextButton) != 0) {
                    holder->inputStates[0].buttonRaw &= static_cast<u16>(~nextButton);
                    holder->uiinputStates[0].rawButtons &= static_cast<u16>(~nextButton);
                    holder->uiinputStates[0].buttonActions &= static_cast<u16>(~nextAction);
                }

                const CharacterId character = PreviewCharacter(hud);
                if ((pressed & prevButton) != 0 && CycleSkin(character, -1)) {
                    ReinitMenuDriverModelMgr(hud, character);
                    Audio::RSARPlayer::PlaySoundById(SOUND_ID_LEFT_ARROW_PRESS, 0, 0);
                }
                if ((pressed & nextButton) != 0 && CycleSkin(character, 1)) {
                    ReinitMenuDriverModelMgr(hud, character);
                    Audio::RSARPlayer::PlaySoundById(SOUND_ID_RIGHT_ARROW_PRESS, 0, 0);
                }
            }
        }
    }
    for (u8 hud = 0; hud < count; ++hud) UpdateCharacterSelectText(hud);
    ApplyVoteRandomMessageBoxKartState();
    mgr->MenuUpdate();
    ApplyVoteRandomMessageBoxKartState();
}
kmCall(0x805552e8, MenuSceneSectionUpdateHook);
kmCall(0x80553b30, MenuSceneSectionUpdateHook);

}  // namespace CustomCharacters
}  // namespace Pulsar
