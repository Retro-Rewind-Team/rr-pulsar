#include <Driver/CustomCharacters.hpp>
#include <Ghost/GhostManager.hpp>
#include <MarioKartWii/GlobalFunctions.hpp>
#include <MarioKartWii/UI/Ctrl/CtrlRace/CtrlRaceResult.hpp>
#include <MarioKartWii/Race/RaceData.hpp>
#include <Race/CustomCharacters.hpp>
#include <MarioKartWii/UI/Ctrl/CtrlRace/CtrlRace2DMap.hpp>
#include <MarioKartWii/UI/Ctrl/PushButton.hpp>
#include <UI/UI.hpp>

namespace Pulsar {
namespace UI {

void SetCharacterSelectIcon(LayoutUIControl *control, const char *paneName, const char *picturePane) {
    control->SetPicturePane(paneName, picturePane);
    const CharacterId character = static_cast<CharacterId>(static_cast<PushButton *>(control)->buttonId);
    if (static_cast<u32>(character) >= Driver::CHARACTER_COUNT)
        return;

    // Restore the stock textures before applying a skin, including when cycling back to slot zero.
    control->SetPicturePane("chara", picturePane);
    control->SetPicturePane("chara_shadow", picturePane);
    control->SetPicturePane("chara_light_01", picturePane);
    control->SetPicturePane("chara_light_02", picturePane);
    Race::LoadCustomCharacterIcon(
      character, Driver::selectedSlots[character], control->layout.GetPaneByName("chara"), control->layout.GetPaneByName("chara_shadow"), control->layout.GetPaneByName("chara_light_01"));
    *control->layout.GetPaneByName("chara_light_02")->GetMaterial()->GetTexMapAry() = *control->layout.GetPaneByName("chara")->GetMaterial()->GetTexMapAry();
    *control->layout.GetPaneByName("chara_c_down")->GetMaterial()->GetTexMapAry() = *control->layout.GetPaneByName("chara")->GetMaterial()->GetTexMapAry();
}
kmCall(0x807e2b38, SetCharacterSelectIcon);

u32 GetCharacterNameBMGId(u32 character, bool useGenericMiiName, u32 playerId, bool isAward) {
    if (character < Driver::CHARACTER_COUNT
      && (playerId >= 12 || Racedata::sInstance->racesScenario.settings.gamemode < MODE_PRIVATE_VS || Racedata::sInstance->racesScenario.settings.gamemode > MODE_PRIVATE_BATTLE)) {
        const u32 slot = Race::GetPlayerCustomCharacterSlot(playerId, static_cast<CharacterId>(character), isAward);
        if (slot != 0) {
            const u32 customBmgId = (character << 16) | BMG_CUSTOM_CHARACTER_NAME_START | slot;
            const wchar_t *customName = GetCustomMsg(customBmgId);
            if (customName != nullptr && customName[0] != L'\0')
                return customBmgId;
        }
    }
    return GetCharacterBMGId(static_cast<CharacterId>(character), useGenericMiiName);
}

u32 GetCharacterAuthorBMGId(u32 character, u32 slot) {
    if (character >= Driver::CHARACTER_COUNT || slot == 0 || slot > Driver::MAX_CUSTOM_CHARACTER_SLOTS)
        return 0;
    return (character << 16) | BMG_CUSTOM_CHARACTER_AUTHOR_START | slot;
}

bool SetCustomCharacterAuthorMessage(LayoutUIControl &control, u32 bmgId) {
    const wchar_t *author = GetCustomMsg(bmgId);
    if (author == nullptr || author[0] == L'\0')
        return false;
    control.SetMessage(bmgId, nullptr);
    return true;
}

static u32 GetNameBalloonCharacterNameBMGId(u32 character, bool useGenericMiiName) {
    register u32 playerId;
    asm(mr playerId, r30;);
    return GetCharacterNameBMGId(character, useGenericMiiName, playerId);
}
kmCall(0x807f056c, GetNameBalloonCharacterNameBMGId);
kmCall(0x807f0694, GetNameBalloonCharacterNameBMGId);

static u32 GetRaceResultCharacterNameBMGId(u32 character, bool useGenericMiiName) {
    register u32 playerId;
    asm(mr playerId, r31;);
    return GetCharacterNameBMGId(character, useGenericMiiName, playerId);
}
kmCall(0x807f53cc, GetRaceResultCharacterNameBMGId);

static u32 GetTeamResultCharacterNameBMGId(u32 character, bool useGenericMiiName) {
    register u32 playerId;
    asm(mr playerId, r19;);
    return GetCharacterNameBMGId(character, useGenericMiiName, playerId);
}
kmCall(0x807f6dfc, GetTeamResultCharacterNameBMGId);

static u32 GetAwardResultCharacterNameBMGId(u32 character, bool useGenericMiiName) {
    register u32 playerId;
    asm(mr playerId, r28;);
    return GetCharacterNameBMGId(character, useGenericMiiName, playerId, true);
}
kmCall(0x805bbd70, GetAwardResultCharacterNameBMGId);

static void SetRaceResultCharacterIcon(LayoutUIControl *control, const char *paneName, const char *picturePane) {
    register u32 playerId;
    asm(mr playerId, r30;);
    control->SetPicturePane(paneName, picturePane);
    const CharacterId character = Racedata::sInstance->racesScenario.players[playerId].characterId;
    Race::LoadCustomCharacterIcon(character, Race::GetPlayerCustomCharacterSlot(playerId, character), control->layout.GetPaneByName("chara_icon"), control->layout.GetPaneByName("chara_icon_sha"));
}
kmCall(0x807f60e4, SetRaceResultCharacterIcon);

static void SetTeamResultCharacterIcon(LayoutUIControl *control, const char *paneName, const char *picturePane) {
    register u32 playerId;
    asm(mr playerId, r19;);
    control->SetPicturePane(paneName, picturePane);
    const CharacterId character = Racedata::sInstance->racesScenario.players[playerId].characterId;
    Race::LoadCustomCharacterIcon(character, Race::GetPlayerCustomCharacterSlot(playerId, character), control->layout.GetPaneByName("chara_icon"), control->layout.GetPaneByName("chara_icon_sha"));
}
kmCall(0x807f6ec8, SetTeamResultCharacterIcon);

static void SetAwardResultCharacterIcon(LayoutUIControl *control, const char *paneName, const char *picturePane) {
    register u32 playerId;
    asm(mr playerId, r28;);
    control->SetPicturePane(paneName, picturePane);
    const CharacterId character = Racedata::sInstance->awardScenario.players[playerId].characterId;
    Race::LoadCustomCharacterIcon(
      character, Race::GetPlayerCustomCharacterSlot(playerId, character, true), control->layout.GetPaneByName("chara_icon"), control->layout.GetPaneByName("chara_icon_sha"));
}
kmCall(0x805bbca4, SetAwardResultCharacterIcon);

static void FillTTLeaderboardCharacterIcon(CtrlRaceResult *control, CharacterId character) {
    control->FillCharacter(character);
    const Ghosts::PULLdbEntry &entry = Ghosts::Mgr::GetInstance()->GetLeaderboard().GetPulEntry(static_cast<Ghosts::EntryLaps>(control->id - 1));
    if (entry.isActive) {
        Race::LoadCustomCharacterIcon(character, entry.customCharacterSlot, control->layout.GetPaneByName("chara_icon"), control->layout.GetPaneByName("chara_icon_sha"));
    }
}
kmCall(0x8085d950, FillTTLeaderboardCharacterIcon);
kmCall(0x8085dbdc, FillTTLeaderboardCharacterIcon);

static void SetGhostInfoCharacterIcon(LayoutUIControl *control, const char *paneName, const char *picturePane) {
    register const GhostData *data;
    asm(mr data, r30;);
    control->SetPicturePane(paneName, picturePane);
    if (control->parentGroup->parentPage->pageId == PAGE_GHOST_SELECT) {
        Race::LoadCustomCharacterIcon(data->characterId, data->unknown_0xc9[0], control->layout.GetPaneByName(paneName), nullptr);
    }
}
kmCall(0x805e2bdc, SetGhostInfoCharacterIcon);

static void LoadMinimapIcon(CtrlRace2DMapCharacter *control) {
    control->CtrlRaceBase::InitSelf();
    const CharacterId character = Racedata::sInstance->racesScenario.players[control->playerId].characterId;
    Race::LoadCustomCharacterIcon(character, Race::GetPlayerCustomCharacterSlot(control->playerId, character), control->charaPane, control->charaShadow0Pane, control->charaShadow1Pane, true);
}
kmCall(0x807eb22c, LoadMinimapIcon);

}  // namespace UI
}  // namespace Pulsar
