#include <Settings/UI/ExpMultiPlayer.hpp>
#include <Settings/UI/SettingsPanel.hpp>
#include <Settings/UI/SettingsPageSelect.hpp>
#include <UI/UI.hpp>

namespace Pulsar {
namespace UI {

ExpMultiPlayer::ExpMultiPlayer() {
    this->externControlCount += 1;
    this->onSettingsClickHandler.subject = this;
    this->onSettingsClickHandler.ptmf = &ExpMultiPlayer::OnSettingsButtonClick;
}

UIControl* ExpMultiPlayer::CreateExternalControl(u32 externControlId) {
    if (externControlId == this->externControlCount - 1) {
        PushButton* button = new (PushButton);
        this->AddControl(this->controlCount++, *button, 0);

        button->Load(UI::buttonFolder, "Settings1P", "Settings1P", this->activePlayerBitfield, 0, false);
        button->buttonId = externControlId;
        return button;
    }

    return this->Pages::MultiPlayer::CreateExternalControl(externControlId);
}

void ExpMultiPlayer::SetButtonHandlers(PushButton& button) {
    if (button.buttonId == this->externControlCount - 1) {
        button.SetOnClickHandler(this->onSettingsClickHandler, 0);
        button.SetOnSelectHandler(this->onButtonSelectHandler);
        button.SetOnDeselectHandler(this->onButtonDeselectHandler);
        return;
    }

    this->Pages::MultiPlayer::SetButtonHandlers(button);
}

void ExpMultiPlayer::OnExternalButtonSelect(PushButton& button, u32 hudSlotId) {
    if (button.buttonId == this->externControlCount - 1) {
        this->bottomText->SetMessage(BMG_SETTINGSBUTTON_BOTTOM);
        return;
    }

    // MultiPlayer::OnExternalButtonSelect crops movie panes for every external button.
    // The single-player settings BRCTR does not define the multiplayer movie panes,
    // so exclude the injected settings button from the vanilla loop temporarily.
    const u32 originalExternControlCount = this->externControlCount;
    this->externControlCount = originalExternControlCount - 1;
    this->Pages::MultiPlayer::OnExternalButtonSelect(button, hudSlotId);
    this->externControlCount = originalExternControlCount;
}

void ExpMultiPlayer::OnSettingsButtonClick(PushButton& button, u32 hudSlotId) {
    SettingsPageSelect* settingsPageSelect = ExpSection::GetSection()->GetPulPage<SettingsPageSelect>();
    SettingsPanel* settingsPanel = ExpSection::GetSection()->GetPulPage<SettingsPanel>();
    if (settingsPageSelect == nullptr || settingsPanel == nullptr) return;

    settingsPageSelect->prevPageId = PAGE_MULTIPLAYER_MENU;
    settingsPanel->prevPageId = PAGE_MULTIPLAYER_MENU;
    this->nextPageId = static_cast<PageId>(SettingsPageSelect::id);
    this->EndStateAnimated(0, button.GetAnimationFrameSize());
}

}  // namespace UI
}  // namespace Pulsar
