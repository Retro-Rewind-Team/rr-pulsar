#ifndef _PUL_CUSTOMENGINECLASSPAGE_
#define _PUL_CUSTOMENGINECLASSPAGE_

#include <MarioKartWii/UI/Page/Other/RegisterFriend.hpp>
#include <UI/UI.hpp>

namespace Pulsar {
namespace UI {

class CustomEngineClassPage : public Pages::RegisterFriend {
public:
    static const PulPageId id = PULPAGE_CUSTOMENGINECLASS;

    PageId GetNextPage() const override;
    void OnInit() override;
    void OnActivate() override;

private:
    void OnDigitClick(PushButton &digit, u32 hudSlotId);
    void OnBackSpaceClick(PushButton &backSpace, u32 hudSlotId);
    void OnOkButtonClick(PushButton &button, u32 hudSlotId);
    void OnBackButtonClick(CtrlMenuBackButton &button, u32 hudSlotId);
    void OnBackPress(u32 hudSlotId);
    u32 GetEngineClass();
    void UpdateOkButton();

    PtmfHolder_2A<CustomEngineClassPage, void, PushButton &, u32> onCustomDigitClickHandler;
    PtmfHolder_2A<CustomEngineClassPage, void, PushButton &, u32> onCustomBackSpaceClickHandler;
    PtmfHolder_2A<CustomEngineClassPage, void, PushButton &, u32> onCustomOkButtonClickHandler;
    PtmfHolder_2A<CustomEngineClassPage, void, CtrlMenuBackButton &, u32> onCustomBackButtonClickHandler;
    PtmfHolder_1A<CustomEngineClassPage, void, u32> onCustomBackPressHandler;
};

}  // namespace UI
}  // namespace Pulsar

#endif
