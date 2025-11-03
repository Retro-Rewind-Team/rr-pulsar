#include <Gamemodes/Battle/BattleElimination.hpp>
#include <Gamemodes/LapKO/LapKOMgr.hpp>
#include <MarioKartWii/UI/Ctrl/CtrlRace/CtrlRaceBase.hpp>
#include <MarioKartWii/UI/Layout/ControlLoader.hpp>
#include <MarioKartWii/UI/Page/RaceHUD/RaceHUD.hpp>
#include <MarioKartWii/UI/Section/SectionMgr.hpp>
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

static const wchar_t* CopyNameSafe(const wchar_t* src, size_t srcMax, wchar_t* dst, size_t dstLen) {
    if (src == nullptr || dst == nullptr || dstLen == 0) return nullptr;
    size_t i = 0;
    for (; i + 1 < dstLen && i < srcMax; ++i) {
        const wchar_t ch = src[i];
        if (ch == L'\0') break;
        dst[i] = ch;
    }
    if (i == 0) {
        if (dstLen > 0) dst[0] = L'\0';
        return nullptr;
    }
    dst[i] = L'\0';
    return dst;
}

static const wchar_t* CopyCharacterNameFromBMG(const BMGHolder& holder, s32 bmgId, wchar_t* scratch, size_t length) {
    if (scratch == nullptr || length <= 1) return nullptr;
    if (holder.bmgFile == nullptr) return nullptr;
    const s32 msgId = holder.GetMsgId(bmgId);
    const wchar_t* source = holder.GetMsgByMsgId(msgId);
    return CopyNameSafe(source, length - 1, scratch, length);
}

class CtrlRaceLapKOElimMessage : public CtrlRaceBase {
   public:
    static u32 Count();
    static void Create(Page& page, u32 index, u32 count);
    void Load(u8 hudSlotId);
    void OnUpdate() override;

   private:
    void UpdateMessage(const u8* playerIds, u8 count);
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
    const bool lapKoDisplay = system->IsContext(PULSAR_MODE_LAPKO) && system->IsContext(PULSAR_ELIMINATION);
    const bool battleDisplay = ::Pulsar::BattleElim::ShouldApplyBattleElimination();
    if (!lapKoDisplay && !battleDisplay) return 0;
    const Racedata* racedata = Racedata::sInstance;
    const RacedataScenario& scenario = racedata->racesScenario;
    u32 localCount = scenario.localPlayerCount;
    if (localCount == 0) localCount = 1;
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
    const RacedataScenario& scenario = Racedata::sInstance->menusScenario;
    const GameMode mode = scenario.settings.gamemode;
    const bool lapKoContext = (system->IsContext(PULSAR_MODE_LAPKO) && system->IsContext(PULSAR_ELIMINATION) && system->lapKoMgr != nullptr);
    const bool battleContext = ::Pulsar::BattleElim::ShouldApplyBattleElimination();

    u16 timer = 0;
    u8 eliminationCount = 0;
    u8 playerIds[4] = {0xFF, 0xFF, 0xFF, 0xFF};

    if (lapKoContext && (mode == MODE_PRIVATE_VS || mode == MODE_VS_RACE)) {
        const Mgr& mgr = *system->lapKoMgr;
        timer = mgr.GetEliminationDisplayTimer();
        eliminationCount = mgr.GetRecentEliminationCount();
        for (u8 idx = 0; idx < eliminationCount && idx < 4; ++idx) {
            playerIds[idx] = mgr.GetRecentEliminationId(idx);
        }
    } else if (battleContext && (mode == MODE_PUBLIC_BATTLE || mode == MODE_PRIVATE_BATTLE || mode == MODE_BATTLE)) {
        timer = ::Pulsar::BattleElim::GetEliminationDisplayTimer();
        eliminationCount = ::Pulsar::BattleElim::GetRecentEliminationCount();
        for (u8 idx = 0; idx < eliminationCount && idx < 4; ++idx) {
            playerIds[idx] = ::Pulsar::BattleElim::GetRecentEliminationId(idx);
        }
    } else {
        this->Show(false);
        this->lastDisplayTimer = 0;
        this->soundPlayedThisDisplay = false;
        return;
    }

    if (timer == 0 || eliminationCount == 0) {
        this->Show(false);
        this->lastDisplayTimer = 0;
        this->soundPlayedThisDisplay = false;
        return;
    }

    this->Show(true);
    if (timer != this->lastDisplayTimer) {
        this->UpdateMessage(playerIds, eliminationCount);
        this->lastDisplayTimer = timer;
    }

    if (this->root != nullptr) {
        const float fadeFraction = static_cast<float>(timer) / static_cast<float>(kEliminationDisplayDuration);
        const u8 alpha = static_cast<u8>((fadeFraction > 1.0f ? 1.0f : fadeFraction) * 255.0f);
        this->root->alpha = alpha;
    }
}

void CtrlRaceLapKOElimMessage::UpdateMessage(const u8* playerIds, u8 count) {
    if (this->textBox == nullptr || playerIds == nullptr) return;
    wchar_t buffer[128];
    buffer[0] = L'\0';
    const size_t bufferLen = sizeof(buffer) / sizeof(buffer[0]);
    int written = 0;
    wchar_t nameScratch[64];
    const u8 limitedCount = (count > 4) ? static_cast<u8>(4) : count;
    u8 addedNames = 0;
    for (u8 idx = 0; idx < limitedCount; ++idx) {
        const u8 playerId = playerIds[idx];
        const wchar_t* displayName = this->GetPlayerDisplayName(playerId, nameScratch, sizeof(nameScratch) / sizeof(nameScratch[0]));
        if (displayName == nullptr) continue;
        size_t remaining = (written >= 0) ? bufferLen - static_cast<size_t>(written) : bufferLen;
        if (remaining <= 1) break;
        if (addedNames == 0) {
            const int res = ::swprintf(buffer + written, remaining, L"\n%ls", displayName);
            if (res > 0) {
                written += res;
                ++addedNames;
            }
        } else {
            const int res = ::swprintf(buffer + written, remaining, L", %ls", displayName);
            if (res > 0) {
                written += res;
                ++addedNames;
            }
        }
    }
    if (addedNames > 0) {
        size_t remaining = (written >= 0) ? bufferLen - static_cast<size_t>(written) : bufferLen;
        if (remaining > 1) {
            if (addedNames == 1) {
                const int res = ::swprintf(buffer + written, remaining, L" has been eliminated!");
                if (res > 0) written += res;
            } else {
                const int res = ::swprintf(buffer + written, remaining, L" have been eliminated!");
                if (res > 0) written += res;
            }
        }
    }
    if (!this->soundPlayedThisDisplay) {
        playElimSound();
        this->soundPlayedThisDisplay = true;
    }
    Text::Info info;
    info.strings[0] = buffer;
    this->SetMessage(UI::BMG_TEXT, &info);
}

void CtrlRaceLapKOElimMessage::Show(bool visible) {
    if (this->root != nullptr) this->root->alpha = visible ? 255 : 0;
    if (this->textBox != nullptr) this->textBox->alpha = visible ? 255 : 0;
}

const wchar_t* CtrlRaceLapKOElimMessage::GetPlayerDisplayName(u8 playerId, wchar_t* scratch, size_t length) const {
    if (playerId >= 12 || scratch == nullptr || length == 0) return nullptr;
    const Racedata* racedata = Racedata::sInstance;

    const RacedataScenario& scenario = racedata->racesScenario;
    const RacedataPlayer& player = scenario.players[playerId];

    scratch[0] = L'\0';
    if (player.mii.isLoaded) {
        if (player.mii.info.name[0] != L'\0') {
            const wchar_t* copied = CopyNameSafe(player.mii.info.name, 11, scratch, length);
            if (copied != nullptr) return copied;
        }
        if (player.mii.rawStoreMii.miiName[0] != L'\0') {
            const wchar_t* copied = CopyNameSafe(player.mii.rawStoreMii.miiName, 10, scratch, length);
            if (copied != nullptr) return copied;
        }
    }

    const u32 characterBmgId = GetCharacterBMGId(player.characterId, true);
    const wchar_t* bmgName = nullptr;
    if (characterBmgId != 0) {
        bmgName = UI::GetCustomMsg(static_cast<s32>(characterBmgId));
        if (CopyCharacterNameFromBMG(this->curFileBmgs, static_cast<s32>(characterBmgId), scratch, length) != nullptr) return scratch;
        if (CopyCharacterNameFromBMG(this->commonBmgs, static_cast<s32>(characterBmgId), scratch, length) != nullptr) return scratch;

        const SectionMgr* sectionMgr = SectionMgr::sInstance;
        if (sectionMgr != nullptr && sectionMgr->systemBMG != nullptr) {
            if (CopyCharacterNameFromBMG(*sectionMgr->systemBMG, static_cast<s32>(characterBmgId), scratch, length) != nullptr) return scratch;
        }

        if (bmgName != nullptr) {
            ::wcsncpy(scratch, bmgName, length - 1);
            scratch[length - 1] = L'\0';
            return scratch;
        }
    }

    return L"Player";
}

}  // namespace LapKO
}  // namespace Pulsar
