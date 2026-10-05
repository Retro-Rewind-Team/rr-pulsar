#ifndef _PUL_REGION_PAGE_
#define _PUL_REGION_PAGE_
#include <UI/UI.hpp>
#include <MarioKartWii/UI/Ctrl/PushButton.hpp>
#include <MarioKartWii/UI/Ctrl/SheetSelect.hpp>
#include <MarioKartWii/UI/Ctrl/Menu/CtrlMenuText.hpp>
#include <MarioKartWii/UI/Page/Menu/Menu.hpp>

namespace Pulsar {
namespace UI {
class RegionPage : public Page {
public:
    static const PulPageId id = PULPAGE_REGION;
    static const u32 rowCount = 5;
    RegionPage();
    void OnInit() override;
    void OnActivate() override;
    void OnDeactivate() override;
    void BeforeControlUpdate() override;
    static void Open(Pages::Menu &source, PushButton &button);
    static bool IsOpen();
    static bool CreatePages(ExpSection &section);
    void ComposeGlobe();

private:
    void RefreshRows();
    void Preview();
    void UpdateMiiNameAndFlag();
    void OpenSubregions(u8 country);
    void SaveAndExit(u8 country, u8 subregion);
    u32 GetChoiceCount() const;
    void OnClick(PushButton &button, u32 hudSlotId);
    void OnSelect(PushButton &button, u32 hudSlotId);
    void OnDeselect(PushButton &, u32) {}
    void OnBack(u32 hudSlotId);
    void OnPrevious(SheetSelectControl &, u32);
    void OnNext(SheetSelectControl &, u32);
    void ChangeListPage(bool next);
    ControlsManipulatorManager manager;
    CtrlMenuPageTitleText titleText;
    PushButton buttons[rowCount];
    CtrlMenuBackButton backButton;
    SheetSelectControl arrows;
    LayoutUIControl miiName;
    PtmfHolder_2A<RegionPage, void, SheetSelectControl &, u32> onPrevious;
    PtmfHolder_2A<RegionPage, void, SheetSelectControl &, u32> onNext;
    PtmfHolder_2A<RegionPage, void, PushButton &, u32> onClick;
    PtmfHolder_2A<RegionPage, void, PushButton &, u32> onSelect;
    PtmfHolder_2A<RegionPage, void, PushButton &, u32> onDeselect;
    PtmfHolder_1A<RegionPage, void, u32> onBack;
    u32 firstRow;
    u32 frames;
    u8 previewCountry;
    u8 previewSubregion;
    bool subregionPage;
    bool miiDisplayed;
    bool ready;
    bool leaving;
};
}  // namespace UI
}  // namespace Pulsar
#endif
