#include <CustomCharacters/CustomCharacters.hpp>

namespace Pulsar {
namespace CustomCharacters {

// Voting menus restore selected skin models after the vanilla random vote flow.
void RestoreVotingMenuDriverModels() {
    const SectionId section = CurrentSectionId();
    if (!IsVotingSection(section)) return;

    votingMenuTableSection = section;
    votingMenuTablesRestored = true;
    ApplySelectedNames();
    RefreshLocalOnlineCustomCharacterFlags();

    const SectionMgr* sectionMgr = SectionMgr::sInstance;
    if (sectionMgr == nullptr || sectionMgr->sectionParams == nullptr) return;
    const u8 count = SectionPlayerCount(sectionMgr);
    for (u8 hud = 0; hud < count; ++hud) {
        ReinitMenuDriverModelMgr(hud, sectionMgr->sectionParams->characters[hud]);
    }
}

// Random selection chooses from the vanilla table plus installed custom skins.
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

// Section loads refresh menu-visible state and clear stale per-section caches.
void ResetCustomCharacterMenuState() {
    if (!IsVotingSection(CurrentSectionId())) {
        votingMenuTableSection = SECTION_NONE;
        votingMenuTablesRestored = false;
    }
    if (!IsOnlineRoom(RKNet::Controller::sInstance)) ResetOnlineCustomCharacterFlags();
    ResetOfflineCpuSkinTablesForSection();
    SyncRawCachesToCurrentScene();
    CacheHoveredFromSection();
    ApplySelectedNames();
}
SectionLoadHook ResetCustomCharacterMenuStateHook(ResetCustomCharacterMenuState);

// Hover updates both the preview model and the custom name/author labels.
void CharacterSelectHoverHook(Pages::CharacterSelect* page, CtrlMenuCharacterSelect::ButtonDriver* button, u32 buttonId, u8 hud) {
    if (hud < LOCAL_PLAYER_COUNT) hoveredCharacters[hud] = static_cast<CharacterId>(buttonId);
    page->OnButtonDriverSelect(button, buttonId, hud);
    if (hud < LOCAL_PLAYER_COUNT) characterNameTextControl[hud] = nullptr;
    UpdateCharacterSelectNameText(page, hud);
    UpdateCharacterSelectAuthorText(page, hud);
}
kmCall(0x807e2cf0, CharacterSelectHoverHook);
kmCall(0x807e304c, CharacterSelectHoverHook);
kmCall(0x807e34d0, CharacterSelectHoverHook);
kmCall(0x807e37b0, CharacterSelectHoverHook);
kmCall(0x807e3a88, CharacterSelectHoverHook);

// Hint panes show the skin-cycle buttons for the active controller type.
ControllerType ControllerForHud(const SectionMgr& mgr, u8 hud) {
    if (hud >= LOCAL_PLAYER_COUNT) return GCN;
    const Input::RealControllerHolder* holder = mgr.pad.padInfos[hud].controllerHolder;
    if (holder == nullptr || holder->curController == nullptr) return GCN;
    const ControllerType type = holder->curController->GetType();
    return type == WHEEL || type == NUNCHUCK || type == CLASSIC || type == GCN ? type : GCN;
}

void SetHintPanes(CharaName& name, ControllerType type, bool visible) {
    const char* panes[] = {"cc_prev_wh", "cc_next_wh", "cc_prev_nc", "cc_next_nc", "cc_prev_cls", "cc_next_cls", "cc_prev_gc", "cc_next_gc"};
    for (u32 i = 0; i < ARRAY_COUNT(panes); ++i) name.SetPaneVisibility(panes[i], false);
    if (!visible) return;
    u32 offset = 6;
    if (type == WHEEL)
        offset = 0;
    else if (type == NUNCHUCK)
        offset = 2;
    else if (type == CLASSIC)
        offset = 4;
    name.SetPaneVisibility(panes[offset], true);
    name.SetPaneVisibility(panes[offset + 1], true);
}

void UpdateHintPanes() {
    if (!IsCharacterSelectActive()) return;
    SectionMgr* mgr = SectionMgr::sInstance;
    if (mgr == nullptr || mgr->sectionParams == nullptr) return;
    Pages::CharacterSelect* page = mgr->curSection->Get<Pages::CharacterSelect>();
    if (page == nullptr || page->names == nullptr) return;
    const u8 count = SectionPlayerCount(mgr);
    for (u8 hud = 0; hud < count; ++hud) SetHintPanes(page->names[hud], ControllerForHud(*mgr, hud), true);
}

// Map each controller type to previous/next skin buttons and consumed UI actions.
void ToggleInputs(ControllerType type, u16& prevButton, u16& nextButton, u16& prevAction, u16& nextAction) {
    prevAction = 0;
    nextAction = 0;
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
        case GCN:
        default:
            prevButton = PAD::PAD_BUTTON_L;
            nextButton = PAD::PAD_BUTTON_R;
            break;
    }
}

void EatButton(Input::RealControllerHolder& holder, u16 button, u16 action) {
    holder.inputStates[0].buttonRaw &= static_cast<u16>(~button);
    holder.uiinputStates[0].rawButtons &= static_cast<u16>(~button);
    holder.uiinputStates[0].buttonActions &= static_cast<u16>(~action);
}

// Character select allows each local player to cycle installed skin tables.
bool ProcessSkinInput() {
    if (!IsCharacterSelectActive()) {
        memset(heldToggleButtons, 0, sizeof(heldToggleButtons));
        return false;
    }
    SectionMgr* mgr = SectionMgr::sInstance;
    if (mgr == nullptr || mgr->sectionParams == nullptr) {
        memset(heldToggleButtons, 0, sizeof(heldToggleButtons));
        return false;
    }

    bool changed = false;
    const u8 count = SectionPlayerCount(mgr);
    for (u8 hud = 0; hud < count; ++hud) {
        Input::RealControllerHolder* holder = mgr->pad.padInfos[hud].controllerHolder;
        if (holder == nullptr || holder->curController == nullptr) {
            heldToggleButtons[hud] = 0;
            continue;
        }

        u16 prevButton = 0;
        u16 nextButton = 0;
        u16 prevAction = 0;
        u16 nextAction = 0;
        ToggleInputs(ControllerForHud(*mgr, hud), prevButton, nextButton, prevAction, nextAction);
        const u16 inputs = holder->inputStates[0].buttonRaw;
        const u16 pressed = static_cast<u16>((inputs & (prevButton | nextButton)) & ~heldToggleButtons[hud]);
        heldToggleButtons[hud] = static_cast<u16>(inputs & (prevButton | nextButton));
        if ((inputs & prevButton) != 0) EatButton(*holder, prevButton, prevAction);
        if ((inputs & nextButton) != 0) EatButton(*holder, nextButton, nextAction);
        if ((pressed & prevButton) != 0) {
            const CharacterId character = PreviewCharacter(hud);
            if (CycleSkin(character, -1)) {
                ReinitMenuDriverModelMgr(hud, character);
                Audio::RSARPlayer::PlaySoundById(SOUND_ID_LEFT_ARROW_PRESS, 0, 0);
                changed = true;
            }
        }
        if ((pressed & nextButton) != 0) {
            const CharacterId character = PreviewCharacter(hud);
            if (CycleSkin(character, 1)) {
                ReinitMenuDriverModelMgr(hud, character);
                Audio::RSARPlayer::PlaySoundById(SOUND_ID_RIGHT_ARROW_PRESS, 0, 0);
                changed = true;
            }
        }
    }
    return changed;
}

// Menu updates keep hints, labels, and random-vote kart previews in sync.
void MenuSceneSectionUpdateHook(SectionMgr* mgr) {
    UpdateHintPanes();
    ProcessSkinInput();
    const u8 count = SectionPlayerCount(mgr);
    for (u8 hud = 0; hud < count; ++hud) UpdateCurrentCharacterSelectAuthorText(hud);
    ApplyVoteRandomMessageBoxKartState();
    mgr->MenuUpdate();
    ApplyVoteRandomMessageBoxKartState();
}
kmCall(0x805552e8, MenuSceneSectionUpdateHook);
kmCall(0x80553b30, MenuSceneSectionUpdateHook);

}  // namespace CustomCharacters
}  // namespace Pulsar
