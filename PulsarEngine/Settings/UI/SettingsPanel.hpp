#ifndef _PUL_SETTINGSPAGE_
#define _PUL_SETTINGSPAGE_

#include <kamek.hpp>
#include <core/nw4r/ut/Misc.hpp>
#include <MarioKartWii/UI/Section/SectionMgr.hpp>
#include <MarioKartWii/UI/Ctrl/PushButton.hpp>
#include <MarioKartWii/UI/Page/Menu/Menu.hpp>
#include <MarioKartWii/UI/Page/Menu/VSSettings.hpp>
#include <MarioKartWii/UI/Ctrl/CountDown.hpp>
#include <Settings/SettingsParam.hpp>
#include <UI/UI.hpp>

namespace Pulsar {
namespace UI {

class SettingsPanel : public Pages::MenuInteractable {
   public:
    static const PulPageId id = PULPAGE_SETTINGS;

    SettingsPanel();
    ~SettingsPanel() override;
    void SetPage(Settings::SettingsPageId id);
    static void StartVotingPreview(Settings::SettingsPageId firstPage);
    static bool IsVotingPreviewActive();
    static void SetVotingPreviewPage(Settings::SettingsPageId page);
    static void ApplyVotingPreviewHostSettings();

    void OnInit() override;
    void OnActivate() override;
    const ut::detail::RuntimeTypeInfo* GetRuntimeTypeInfo() const override;
    void OnExternalButtonSelect(PushButton& button, u32 r5) override;
    int GetActivePlayerBitfield() const override;
    int GetPlayerBitfield() const override;
    ManipulatorManager& GetManipulatorManager() override;
    UIControl* CreateExternalControl(u32 id) override;
    UIControl* CreateControl(u32 id) override;
    void SetButtonHandlers(PushButton& pushButton) override;
    void OnBackPress(u32 hudSlotId);
    void OnBackButtonClick(PushButton& button, u32 hudSlotId);
    void BeforeControlUpdate() override;

    Settings::SettingsPageId settingsPageId;

   private:
    void LoadCurrentValues();
    void SaveSettings(bool writeFile);
    bool HasModifiedMiscSettings() const;
    void LoadMainMenuAndSaveSettings(PushButton& button);
    void LoadPrevMenuAndSaveSettings(PushButton& button);
    void OnSaveButtonClick(PushButton& button, u32 hudSlotId);
    void OnRadioButtonClick(RadioButtonControl& radioButtonControl, u32 r5, u32 optionId);
    void OnRadioButtonChange(RadioButtonControl& radioButtonControl, u32 r5, u32 optionId);
    void OnUpDownClick(UpDownControl& upDownControl, u32 hudSlotId);
    void OnUpDownSelect(UpDownControl& upDownControl, u32 hudSlotId);
    void OnTextChange(TextUpDownValueControl::TextControl& text, u32 optionId);

    u32 GetTextId(const TextUpDownValueControl::TextControl& text) const {
        TextUpDownValueControl* valueControl = static_cast<TextUpDownValueControl*>(text.parentGroup->parentControl);
        return static_cast<int>(reinterpret_cast<u32>(valueControl) - reinterpret_cast<u32>(&this->textUpDown[0])) /
               sizeof(TextUpDownValueControl);
    }

    RadioButtonControl radioButtonControls[Settings::Params::maxRadioCount];
    PtmfHolder_3A<SettingsPanel, void, RadioButtonControl&, u32, u32> onRadioButtonClickHandler;
    PtmfHolder_3A<SettingsPanel, void, RadioButtonControl&, u32, u32> onRadioButtonChangeHandler;
    UpDownControl upDownControls[Settings::Params::maxScrollerCount];
    TextUpDownValueControl textUpDown[Settings::Params::maxScrollerCount];
    PtmfHolder_2A<SettingsPanel, void, UpDownControl&, u32> onUpDownClickHandler;
    PtmfHolder_2A<SettingsPanel, void, UpDownControl&, u32> onUpDownSelectHandler;
    PtmfHolder_2A<SettingsPanel, void, TextUpDownValueControl::TextControl&, u32> onTextChangeHandler;
    PtmfHolder_2A<SettingsPanel, void, PushButton&, u32> onBackButtonClickHandler;

    u8 radioValues[Settings::Params::maxRadioCount];
    u8 scrollerValues[Settings::Params::maxScrollerCount];

   public:
    CountDown* timer;
    s32 rouletteCounter;
    CountDownTimerControl countdownControl;
    int frameCounter;
    CountDown countdown;
};

}  // namespace UI
}  // namespace Pulsar

#endif
