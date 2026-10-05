#ifndef _PUL_RESTRICTION_PAGES_
#define _PUL_RESTRICTION_PAGES_

#include <kamek.hpp>
#include <MarioKartWii/UI/Page/Menu/CharacterSelect.hpp>
#include <MarioKartWii/UI/Page/Menu/KartSelect.hpp>
#include <UI/UI.hpp>

namespace Pulsar {
namespace UI {

class CharacterRestrictionPage : public Pages::CharacterSelect {
public:
    static const PulPageId id = PULPAGE_CHARACTERRESTRICTION;

    CharacterRestrictionPage();
    void OnInit() override;
    void OnActivate() override;
    void OnDeactivate() override;
    void AfterControlUpdate() override;

private:
    void OnRestrictionButtonClick(PushButton &button, u32 hudSlotId);
    void OnRestrictionButtonSelect(PushButton &button, u32 hudSlotId);
    void OnRestrictionButtonDeselect(PushButton &button, u32 hudSlotId);
    void OnRestrictionBackPress(u32 hudSlotId);
    void OnRestrictionBackButtonClick(PushButton &button, u32 hudSlotId) { OnRestrictionBackPress(hudSlotId); }
    void BindButtons();
    void UpdateButtonVisuals();

    PtmfHolder_2A<CharacterRestrictionPage, void, PushButton &, u32> restrictionButtonClickHandler;
    PtmfHolder_2A<CharacterRestrictionPage, void, PushButton &, u32> restrictionButtonSelectHandler;
    PtmfHolder_2A<CharacterRestrictionPage, void, PushButton &, u32> restrictionButtonDeselectHandler;
    PtmfHolder_2A<CharacterRestrictionPage, void, PushButton &, u32> restrictionBackButtonClickHandler;
    PtmfHolder_1A<CharacterRestrictionPage, void, u32> restrictionBackPressHandler;
};

class VehicleRestrictionWeightPage : public Pages::MenuInteractable {
public:
    static const PulPageId id = PULPAGE_VEHICLERESTRICTIONWEIGHT;

    VehicleRestrictionWeightPage();
    void OnInit() override;
    void OnActivate() override;
    UIControl *CreateControl(u32 id) override;
    void SetButtonHandlers(PushButton &button) override;
    int GetActivePlayerBitfield() const override { return activePlayerBitfield; }
    int GetPlayerBitfield() const override { return playerBitfield; }
    ManipulatorManager &GetManipulatorManager() override { return controlsManipulatorManager; }

private:
    void OnButtonClick(PushButton &button, u32 hudSlotId);
    void OnButtonSelect(PushButton &button, u32 hudSlotId);
    void OnBackPress(u32 hudSlotId);
    void OnBackButtonClick(PushButton &button, u32 hudSlotId);

    PushButton buttons[3];
    PtmfHolder_2A<VehicleRestrictionWeightPage, void, PushButton &, u32> onBackButtonClickHandler;
};

class VehicleRestrictionPage : public Pages::KartSelect {
public:
    static const PulPageId id = PULPAGE_VEHICLERESTRICTION;

    VehicleRestrictionPage();
    void OnInit() override;
    void OnActivate() override;
    void OnDeactivate() override;
    void SetButtonHandlers(PushButton &button) override;
    void SetWeight(u32 newWeight) { weight = newWeight; }

private:
    void OnRestrictionButtonClick(PushButton &button, u32 hudSlotId);
    void OnRestrictionButtonSelect(PushButton &button, u32 hudSlotId);
    void OnRestrictionBackPress(u32 hudSlotId);
    void OnRestrictionBackButtonClick(PushButton &button, u32 hudSlotId) { OnRestrictionBackPress(hudSlotId); }
    void BindButtons();
    void UpdateButtonVisuals();
    void RestoreCharacter();

    u32 weight;
    CharacterId savedCharacter;
    CharacterId savedComboCharacter;
    bool hasSavedCharacter;
    PtmfHolder_2A<VehicleRestrictionPage, void, PushButton &, u32> restrictionButtonClickHandler;
    PtmfHolder_2A<VehicleRestrictionPage, void, PushButton &, u32> restrictionButtonSelectHandler;
    PtmfHolder_2A<VehicleRestrictionPage, void, PushButton &, u32> restrictionBackButtonClickHandler;
    PtmfHolder_1A<VehicleRestrictionPage, void, u32> restrictionBackPressHandler;
};

}  // namespace UI
}  // namespace Pulsar

#endif
