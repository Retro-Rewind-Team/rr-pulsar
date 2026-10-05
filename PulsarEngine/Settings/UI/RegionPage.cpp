#include <Settings/UI/RegionPage.hpp>
#include <Settings/UI/SettingsPageSelect.hpp>
#include <Settings/Region.hpp>
#include <Settings/Settings.hpp>
#include <MarioKartWii/3D/GlobeMgr.hpp>
#include <MarioKartWii/UI/Layout/ControlLoader.hpp>
#include <MarioKartWii/UI/Page/Menu/Menu.hpp>

namespace Pulsar {
namespace UI {
static_assert(sizeof(GlobeMgr) == 0x40, "GlobeMgr ABI changed");
static_assert(sizeof(EarthModel) == 0x110, "EarthModel ABI changed");
static_assert(sizeof(GlobeMii) == 0x1a8, "GlobeMii ABI changed");
static bool regionScene = false;
static bool returnToSettings = false;
static SectionId returnSection = SECTION_SINGLE_P_FROM_MENU;
static Settings::SettingsContext returnContext = Settings::SETTINGS_CONTEXT_OFFLINE;

bool RegionPage::IsOpen() {
    const SectionMgr *manager = SectionMgr::sInstance;
    return regionScene && manager != nullptr && manager->curSection != nullptr &&
           manager->curSection->sectionId == SECTION_P1_WIFI;
}

void RegionPage::Open(Pages::Menu &source, PushButton &button) {
    returnSection = SectionMgr::sInstance->curSection->sectionId;
    regionScene = true;
    source.ChangeSectionById(SECTION_P1_WIFI, button);
}

bool RegionPage::CreatePages(ExpSection &section) {
    if (!regionScene || section.sectionId != SECTION_P1_WIFI) return false;
    ExpSection::CreateAndInitPage(section, id);
    return true;
}

static void AddInitialLayers(ExpSection &section, SectionId sectionId) {
    if (RegionPage::IsOpen()) {
        ExpSection::AddPageLayer(section, RegionPage::id);
        return;
    }
    section.AddInitialLayers(sectionId);
    if (returnToSettings && sectionId == returnSection) {
        returnToSettings = false;
        SettingsPageSelect *settings = section.GetPulPage<SettingsPageSelect>();
        if (settings != nullptr) {
            const PageId previous = sectionId == SECTION_OPTIONS ? PAGE_OPTIONS : sectionId == SECTION_LOCAL_MULTIPLAYER ? PAGE_MULTIPLAYER_MENU
                                                                                                                         : PAGE_SINGLE_PLAYER_MENU;
            settings->SetContext(returnContext, previous);
            section.RemoveTopLayerPage();
            ExpSection::AddPageLayer(section, SettingsPageSelect::id);
        }
    }
}
kmCall(0x8062213c, AddInitialLayers);

RegionPage::RegionPage()
    : firstRow(0), frames(0), previewCountry(0), previewSubregion(0), subregionPage(false), miiDisplayed(false), ready(false), leaving(false) {
    onPrevious.subject = this;
    onPrevious.ptmf = &RegionPage::OnPrevious;
    onNext.subject = this;
    onNext.ptmf = &RegionPage::OnNext;
    onClick.subject = this;
    onClick.ptmf = &RegionPage::OnClick;
    onSelect.subject = this;
    onSelect.ptmf = &RegionPage::OnSelect;
    onDeselect.subject = this;
    onDeselect.ptmf = &RegionPage::OnDeselect;
    onBack.subject = this;
    onBack.ptmf = &RegionPage::OnBack;
    manager.Init(1, false);
    manager.SetDistanceFunc(1);
    SetManipulatorManager(manager);
    manager.SetGlobalHandler(BACK_PRESS, onBack, false, false);
}

void RegionPage::OnInit() {
    InitControlGroup(rowCount + 4);

    AddControl(0, titleText, 0);
    titleText.Load(false);
    titleText.SetMessage(BMG_REGION_BUTTON);

    for (u32 i = 0; i < rowCount; ++i) {
        PushButton &button = buttons[i];
        AddControl(i + 1, button, 0);
        char variant[16];
        snprintf(variant, sizeof(variant), "Button%d", i);
        button.Load(buttonFolder, "RegionButton", variant, 1, 0, false);
        button.buttonId = i;
        button.SetOnClickHandler(onClick, 0);
        button.SetOnSelectHandler(onSelect);
        button.SetOnDeselectHandler(onDeselect);
    }

    AddControl(rowCount + 1, backButton, 0);
    backButton.Load(buttonFolder, "Back", "ButtonBack", 1, 0, false);
    backButton.buttonId = rowCount;
    backButton.SetOnClickHandler(onClick, 0);

    AddControl(rowCount + 2, arrows, 0);
    arrows.SetLeftArrowHandler(onPrevious);
    arrows.SetRightArrowHandler(onNext);
    arrows.Load(buttonFolder, "RegionArrowR", "CountryR", "RegionArrowL", "CountryL", 1, false, false);

    AddControl(rowCount + 3, miiName, 0);
    ControlLoader loader(&miiName);
    loader.Load("globe", "NameWindow", "Name", nullptr);
    miiName.ResetTextBoxMessage("user_id");
    miiName.ResetTextBoxMessage("user_id_shadow");
    miiName.SetPaneVisibility("flag_null", false);
    miiName.isHidden = true;
}

void RegionPage::OnActivate() {
    leaving = false;
    ready = false;
    miiDisplayed = false;
    miiName.isHidden = true;
    frames = 0;
    subregionPage = false;
    previewCountry = Region::GetSelectedCountry();
    previewSubregion = Region::GetSelectedSubregion();
    const u32 index = Region::GetListIndex(previewCountry);
    firstRow = index / rowCount * rowCount;
    RefreshRows();
    buttons[index - firstRow].SelectInitial(0);
}

void RegionPage::OnDeactivate() {
    ready = false;
    miiDisplayed = false;
}

u32 RegionPage::GetChoiceCount() const {
    if (!subregionPage) return Region::GetCountryChoiceCount();
    const u32 count = Region::GetSubregionCount(previewCountry);
    return count == 0 ? 0 : count + 1;
}

void RegionPage::RefreshRows() {
    const u32 count = GetChoiceCount();
    for (u32 row = 0; row < rowCount; ++row) {
        PushButton &button = buttons[row];
        const u32 index = firstRow + row;
        const bool hidden = index >= count;
        button.isHidden = hidden;
        button.manipulator.inaccessible = hidden;
        if (hidden) continue;

        if (!subregionPage) {
            const u8 country = Region::GetCountryAt(index);
            if (country == 0)
                button.SetMessage(BMG_REGION_USE_WII_LOCATION);
            else
                button.SetMessage(Region::GetCountry(country)->nameBmg);
        } else if (index == 0) {
            button.SetMessage(BMG_REGION_DEFAULT_LOCATION);
        } else {
            button.SetMessage(Region::GetSubregionAt(previewCountry, index - 1)->nameBmg);
        }
    }

    const bool paging = count > rowCount;
    arrows.isHidden = !paging;
    arrows.leftArrow.isHidden = !paging;
    arrows.rightArrow.isHidden = !paging;
    arrows.leftArrow.manipulator.inaccessible = !paging;
    arrows.rightArrow.manipulator.inaccessible = !paging;
}

void RegionPage::Preview() {
    if (!ready || GlobeMgr::sInstance == nullptr) return;
    GlobeMgr *globe = GlobeMgr::sInstance;
    miiName.isHidden = true;
    globe->earthmodel->isMiiShown = false;
    globe->ResetGlobeMii();
    SectionMgr *sectionMgr = SectionMgr::sInstance;
    if (sectionMgr != nullptr && sectionMgr->sectionParams != nullptr &&
        sectionMgr->sectionParams->localPlayerMiis.miiCount != 0) {
        Mii *mii = sectionMgr->sectionParams->localPlayerMiis.GetMii(0);
        if (mii != nullptr) globe->SetMii(*mii);
    }
    u16 longitude, latitude;
    Region::GetPreview(previewCountry, previewSubregion, longitude, latitude);
    globe->SetPosition(1, static_cast<s16>(latitude) * (360.0f / 65536.0f),
                       static_cast<s16>(longitude) * (360.0f / 65536.0f));
    miiDisplayed = false;
    UpdateMiiNameAndFlag();
}

void RegionPage::UpdateMiiNameAndFlag() {
    SectionMgr *sectionMgr = SectionMgr::sInstance;
    if (sectionMgr != nullptr && sectionMgr->sectionParams != nullptr &&
        sectionMgr->sectionParams->localPlayerMiis.miiCount != 0) {
        Mii *mii = sectionMgr->sectionParams->localPlayerMiis.GetMii(0);
        if (mii != nullptr) {
            Text::Info info;
            info.miis[0] = mii;
            miiName.SetTextBoxMessage("mii_name", BMG_MII_NAME, &info);
            miiName.SetTextBoxMessage("shadow", BMG_MII_NAME, &info);
        }
    }

    const u8 country = previewCountry == 0 ? Region::GetWiiCountry() : previewCountry;
    char flagPane[4];
    snprintf(flagPane, sizeof(flagPane), "%03u", country);
    const bool hasFlag = miiName.PicturePaneExists(flagPane);
    if (hasFlag) {
        miiName.SetPicturePane("flag", flagPane);
        miiName.SetPicturePane("flag_shadow", flagPane);
    }
    miiName.SetPaneVisibility("flag_null", hasFlag);

    const Region::Country *entry = Region::GetCountry(country);
    if (entry != nullptr) {
        miiName.SetTextBoxMessage("user_id", entry->nameBmg);
        miiName.SetTextBoxMessage("user_id_shadow", entry->nameBmg);
    } else {
        miiName.ResetTextBoxMessage("user_id");
        miiName.ResetTextBoxMessage("user_id_shadow");
    }
}

void RegionPage::OpenSubregions(u8 country) {
    for (u32 i = 0; i < rowCount; ++i) {
        if (buttons[i].IsSelected()) buttons[i].HandleDeselect(0, -1);
    }
    previewCountry = country;
    previewSubregion = 0;
    subregionPage = true;
    firstRow = 0;
    RefreshRows();
    buttons[0].Select(0);
    Preview();
}

void RegionPage::SaveAndExit(u8 country, u8 subregion) {
    if (leaving) return;
    Settings::Mgr::Get().SetDisplayLocation(country, subregion);
    previewCountry = country;
    previewSubregion = subregion;
    leaving = true;
    regionScene = false;
    returnToSettings = true;
    ChangeSectionBySceneChange(returnSection, 0, 0.0f);
}

void RegionPage::OnClick(PushButton &button, u32) {
    if (leaving) return;
    const u32 selected = button.buttonId;
    if (selected == rowCount) {
        OnBack(0);
        return;
    }
    if (selected >= rowCount) return;

    const u32 index = firstRow + selected;
    if (index >= GetChoiceCount()) return;

    if (!subregionPage) {
        const u8 country = Region::GetCountryAt(index);
        if (country == 0 || Region::GetSubregionCount(country) == 0)
            SaveAndExit(country, 0);
        else
            OpenSubregions(country);
        return;
    }

    if (index == 0) {
        SaveAndExit(previewCountry, 0);
        return;
    }
    const Region::Subregion *subregion = Region::GetSubregionAt(previewCountry, index - 1);
    if (subregion != nullptr) SaveAndExit(previewCountry, subregion->state);
}

void RegionPage::OnSelect(PushButton &button, u32) {
    if (leaving || button.buttonId >= rowCount) return;
    const u32 index = firstRow + button.buttonId;
    if (index >= GetChoiceCount()) return;

    if (!subregionPage) {
        previewCountry = Region::GetCountryAt(index);
        previewSubregion = 0;
    } else if (index == 0) {
        previewSubregion = 0;
    } else {
        const Region::Subregion *subregion = Region::GetSubregionAt(previewCountry, index - 1);
        if (subregion == nullptr) return;
        previewSubregion = subregion->state;
    }
    Preview();
}

void RegionPage::OnPrevious(SheetSelectControl &, u32) {
    ChangeListPage(false);
}

void RegionPage::OnNext(SheetSelectControl &, u32) {
    ChangeListPage(true);
}

void RegionPage::ChangeListPage(bool next) {
    if (leaving) return;
    const u32 pageCount = (GetChoiceCount() + rowCount - 1) / rowCount;
    if (pageCount < 2) return;
    firstRow = ((firstRow / rowCount + (next ? 1 : pageCount - 1)) % pageCount) * rowCount;
    RefreshRows();
    buttons[0].Select(0);
}

void RegionPage::OnBack(u32) {
    if (leaving) return;
    if (subregionPage) {
        for (u32 i = 0; i < rowCount; ++i) {
            if (buttons[i].IsSelected()) buttons[i].HandleDeselect(0, -1);
        }
        subregionPage = false;
        previewSubregion = 0;
        const u32 index = Region::GetListIndex(previewCountry);
        firstRow = index / rowCount * rowCount;
        RefreshRows();
        buttons[index - firstRow].Select(0);
        Preview();
        return;
    }

    leaving = true;
    regionScene = false;
    returnToSettings = true;
    ChangeSectionBySceneChange(returnSection, 0, 0.0f);
}

void RegionPage::BeforeControlUpdate() {
    ++frames;
}

void RegionPage::ComposeGlobe() {
    GlobeMgr *globe = GlobeMgr::sInstance;
    if (leaving || globe == nullptr || globe->earthmodel == nullptr) return;

    if (!ready && frames > 2) {
        ready = true;
        Preview();
    }

    // SetPosition temporarily hides the Mii while the globe rotates. The stock
    // globe page waits for this flag before displaying it at the new location.
    if (ready && !miiDisplayed && globe->IsMiiShown()) {
        globe->DisplayMii();
        miiName.isHidden = false;
        miiDisplayed = true;
    }
}

static void UpdateGlobe(GlobeMgr &globe) {
    globe.Update();
    if (RegionPage::IsOpen()) {
        RegionPage *page = ExpSection::GetSection()->GetPulPage<RegionPage>();
        if (page != nullptr) page->ComposeGlobe();
    }
}
kmCall(0x80553b3c, UpdateGlobe);

}  // namespace UI
}  // namespace Pulsar
