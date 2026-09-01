#include <kamek.hpp>
#include <MarioKartWii/UI/Page/Menu/GPClassSelect.hpp>
#include <RetroRewindChannel.hpp>

namespace Pulsar {

void LoadGrandPrixClassMovie(Pages::Menu *page, char **, bool isVisible) {
    if (IsNewChannel()) return;
    static char *movie = "thp/button/class.thp";
    page->LoadMovies(&movie, isVisible);
}
kmCall(0x8083F870, LoadGrandPrixClassMovie);

void InitializeReversedGPClassButtons(Pages::GPClassSelect *page) {
    page->Pages::Menu::OnControlsInitialized();
    const u32 count = page->externControlCount;
    const u32 start = count == 4 ? 0 : 1;
    for (u32 i = 0; i < count; ++i) {
        PushButton &button = *page->externControls[i];
        const u32 visibleClassId = page->externControlCount == 4 ? 3 - i : 2 - i;
        button.buttonId = visibleClassId < 2 ? visibleClassId + 4 : visibleClassId;
        button.SetMessage(button.buttonId + 0xBBE);
        if (button.IsSelected()) static_cast<Pages::Menu *>(page)->OnExternalButtonSelect(button, 0);
    }
}
kmWritePointer(0x808D9418, InitializeReversedGPClassButtons);

void ToggleGPVehicleClasses(Pages::GPClassSelect *page, u32 hudSlotId) {
    const u32 count = page->externControlCount;
    if (count < 3) return;
    static const u32 allVehicleIds[4] = {3, 2, 5, 4};
    const u32 start = count == 4 ? 0 : 1;
    const u32 *const ids = allVehicleIds;
    for (u32 i = 0; i < count; ++i) {
        PushButton &button = *page->externControls[i];
        button.buttonId = ids[start + i];
        button.SetMessage(button.buttonId + 0xBBE);
        if (button.IsSelected()) static_cast<Pages::Menu *>(page)->OnExternalButtonSelect(button, hudSlotId);
    }
}
kmWritePointer(0x808D9414, ToggleGPVehicleClasses);

}  // namespace Pulsar