#include <Gamemodes/LapKO/LapKOMgr.hpp>
#include <MarioKartWii/UI/Ctrl/CtrlRace/CtrlRaceBase.hpp>
#include <MarioKartWii/UI/Layout/ControlLoader.hpp>
#include <MarioKartWii/UI/Page/RaceHUD/RaceHUD.hpp>
#include <MarioKartWii/Race/RaceData.hpp>
#include <MarioKartWii/GlobalFunctions.hpp>
#include <MarioKartWii/Mii/Mii.hpp>
#include <MarioKartWii/UI/Text/Text.hpp>
#include <PulsarSystem.hpp>
#include <UI/CtrlRaceBase/CustomCtrlRaceBase.hpp>
#include <UI/UI.hpp>

#include <include/c_wchar.h>

namespace Pulsar {
namespace LapKO {

static const u16 kEliminationDisplayDuration = 180;

extern "C" void fun_playSound(void*);
extern "C" void ptr_menuPageOrSomething(void*);
asmFunc playElimSound() {
    ASM(
        nofralloc;
        mflr r11;
        stwu sp, -0x80(sp);
        stmw r3, 0x8(sp);
        lis r11, ptr_menuPageOrSomething @ha;
        lwz r3, ptr_menuPageOrSomething @l(r11);
        li r4, 0xDD;
        lis r12, fun_playSound @h;
        ori r12, r12, fun_playSound @l;
        mtctr r12;
        bctrl;
        lmw r3, 0x8(sp);
        addi sp, sp, 0x80;
        mtlr r11;
        blr;)
}

// Safely copy a wchar_t name from a fixed-size source buffer to dst.
// - Stops at the first null in src or at srcMax, and ensures dst is null-terminated.
// - Returns dst if at least one character was copied, otherwise nullptr.
static const wchar_t* CopyNameSafe(const wchar_t* src, size_t srcMax, wchar_t* dst, size_t dstLen) {
    if (src == nullptr || dst == nullptr || dstLen == 0) return nullptr;
    size_t i = 0;
    for (; i + 1 < dstLen && i < srcMax; ++i) {
        const wchar_t ch = src[i];
        if (ch == L'\0') break;
        dst[i] = ch;
    }
    if (i == 0) {
        // Nothing copied (empty string)
        if (dstLen > 0) dst[0] = L'\0';
        return nullptr;
    }
    dst[i] = L'\0';
    return dst;
}

class CtrlRaceLapKOElimMessage : public CtrlRaceBase {
   public:
    static u32 Count();
    static void Create(Page& page, u32 index, u32 count);

    void Load(u8 hudSlotId);
    void OnUpdate() override;

   private:
    void UpdateMessage(const Mgr& mgr);
    void Show(bool visible);
    const wchar_t* GetPlayerDisplayName(u8 playerId, wchar_t* scratch, size_t length) const;

    nw4r::lyt::Pane* root;
    nw4r::lyt::TextBox* textBox;
    u16 lastDisplayTimer;
    bool soundPlayedThisDisplay;
};

static UI::CustomCtrlBuilder sLapKOElimMessageBuilder(
    CtrlRaceLapKOElimMessage::Count, CtrlRaceLapKOElimMessage::Create);

u32 CtrlRaceLapKOElimMessage::Count() {
    const System* system = System::sInstance;
    if (system == nullptr) return 0;
    if (!system->IsContext(PULSAR_MODE_LAPKO)) return 0;
    if (system->lapKoMgr == nullptr) return 0;

    const Racedata* racedata = Racedata::sInstance;
    if (racedata == nullptr) return 0;

    const RacedataScenario& scenario = racedata->racesScenario;
    u32 localCount = scenario.localPlayerCount;
    if (localCount == 0) localCount = 1;  // cover spectator view / online
    return localCount;
}

void CtrlRaceLapKOElimMessage::Create(Page& page, u32 index, u32 count) {
    for (u32 i = 0; i < count; ++i) {
        CtrlRaceLapKOElimMessage* control = new (CtrlRaceLapKOElimMessage);
        page.AddControl(index + i, *control, 0);
        control->Load(static_cast<u8>(i));
    }
}

void CtrlRaceLapKOElimMessage::Load(u8 hudSlot) {
    this->hudSlotId = hudSlot;
    ControlLoader loader(this);
    loader.Load(UI::raceFolder, "CTInfo", "CTInfo", nullptr);
    this->root = this->layout.GetPaneByName("root");
    if (this->root == nullptr) {
        this->root = this->rootPane;
    }
    this->textBox = static_cast<nw4r::lyt::TextBox*>(this->layout.GetPaneByName("TextBox_00"));
    this->lastDisplayTimer = 0;
    this->soundPlayedThisDisplay = false;
    this->Show(false);
}

void CtrlRaceLapKOElimMessage::OnUpdate() {
    this->UpdatePausePosition();

    const System* system = System::sInstance;
    if (system == nullptr || !system->IsContext(PULSAR_MODE_LAPKO) || system->lapKoMgr == nullptr) {
        this->Show(false);
        return;
    }

    const Mgr& mgr = *system->lapKoMgr;
    const u16 timer = mgr.GetEliminationDisplayTimer();
    if (timer == 0 || mgr.GetRecentEliminationCount() == 0) {
        this->Show(false);
        this->lastDisplayTimer = 0;
        this->soundPlayedThisDisplay = false;  // reset so next display will play sound
        return;
    }

    this->Show(true);
    if (timer != this->lastDisplayTimer) {
        this->UpdateMessage(mgr);
        this->lastDisplayTimer = timer;
    }

    if (this->root != nullptr) {
        const float fadeFraction = static_cast<float>(timer) / static_cast<float>(kEliminationDisplayDuration);
        const u8 alpha = static_cast<u8>((fadeFraction > 1.0f ? 1.0f : fadeFraction) * 255.0f);
        this->root->alpha = alpha;
    }
}

void CtrlRaceLapKOElimMessage::UpdateMessage(const Mgr& mgr) {
    if (this->textBox == nullptr) return;

    wchar_t buffer[128];
    buffer[0] = L'\0';
    const size_t bufferLen = sizeof(buffer) / sizeof(buffer[0]);
    int written = ::swprintf(buffer, bufferLen, L"\nEliminated: ");
    if (written < 0) {
        written = 0;
        buffer[0] = L'\0';
    }

    wchar_t nameScratch[64];
    const u8 count = mgr.GetRecentEliminationCount();
    for (u8 idx = 0; idx < count; ++idx) {
        const u8 playerId = mgr.GetRecentEliminationId(idx);
        const wchar_t* displayName = this->GetPlayerDisplayName(playerId, nameScratch, sizeof(nameScratch) / sizeof(nameScratch[0]));
        if (displayName == nullptr) continue;
        size_t remaining = (written >= 0) ? bufferLen - static_cast<size_t>(written) : bufferLen;
        if (remaining <= 1) break;

        if (idx > 0) {
            if (remaining <= 3) break;
            const int res = ::swprintf(buffer + written, remaining, L", ");
            if (res > 0) {
                written += res;
                remaining = bufferLen - static_cast<size_t>(written);
            }
        }

        if (remaining <= 1) break;
        const int res = ::swprintf(buffer + written, remaining, L"%ls", displayName);
        if (res > 0) written += res;
    }

    // Play the elimination sound the first time this message is established for the
    // current display. Subsequent updates while the display is active won't replay it.
    if (!this->soundPlayedThisDisplay) {
        playElimSound();
        this->soundPlayedThisDisplay = true;
    }

    Text::Info info;
    info.strings[0] = buffer;
    this->SetMessage(UI::BMG_TEXT, &info);
}

void CtrlRaceLapKOElimMessage::Show(bool visible) {
    // Visibility is handled via the pane flag's bit 0x01 in nw4r::lyt::Pane; however
    // LayoutUIControl exposes SetPaneVisibility by name. Since we have direct pointers,
    // simply toggle alpha and rely on animations/layout visibility.
    if (this->root != nullptr) this->root->alpha = visible ? 255 : 0;
    if (this->textBox != nullptr) this->textBox->alpha = visible ? 255 : 0;
}

const wchar_t* CtrlRaceLapKOElimMessage::GetPlayerDisplayName(u8 playerId, wchar_t* scratch, size_t length) const {
    if (playerId >= 12 || scratch == nullptr || length == 0) return nullptr;
    const Racedata* racedata = Racedata::sInstance;
    if (racedata == nullptr) return nullptr;

    const RacedataScenario& scenario = racedata->racesScenario;
    const RacedataPlayer& player = scenario.players[playerId];

    scratch[0] = L'\0';
    // Prefer the resolved Mii name from AdditionalInfo (up to 10-11 wide chars)
    if (player.mii.isLoaded) {
        // RFL::AdditionalInfo::name is 11 wchar_t according to headers
        if (player.mii.info.name[0] != L'\0') {
            const wchar_t* copied = CopyNameSafe(player.mii.info.name, 11, scratch, length);
            if (copied != nullptr) return copied;
        }
        // Fallback to the raw stored miiName (10 wchar_t)
        if (player.mii.rawStoreMii.miiName[0] != L'\0') {
            const wchar_t* copied = CopyNameSafe(player.mii.rawStoreMii.miiName, 10, scratch, length);
            if (copied != nullptr) return copied;
        }
    }

    const wchar_t* bmgName = UI::GetCustomMsg(GetCharacterBMGId(player.characterId, true));
    if (bmgName != nullptr) {
        ::wcsncpy(scratch, bmgName, length - 1);
        scratch[length - 1] = L'\0';
        return scratch;
    }

    return L"Player";
}

}  // namespace LapKO
}  // namespace Pulsar
