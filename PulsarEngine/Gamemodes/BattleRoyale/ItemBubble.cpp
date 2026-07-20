#include <kamek.hpp>
#include <Gamemodes/BattleRoyale/BattleRoyale.hpp>
#include <MarioKartWii/3D/Camera/CameraMgr.hpp>
#include <MarioKartWii/Item/Obj/ItemObj.hpp>
#include <MarioKartWii/Race/RaceData.hpp>
#include <MarioKartWii/System/Identifiers.hpp>
#include <MarioKartWii/3D/Model/ModelDirector.hpp>
#include <MarioKartWii/3D/Model/ModelTransformator.hpp>
#include <MarioKartWii/3D/Model/AnmHolder.hpp>
#include <UI/ExtendedTeamSelect/ExtendedTeamManager.hpp>

namespace Pulsar {
namespace Extra {

static bool ShouldUseBattleRoyaleItemBubbles() {
    return BattleRoyale::ShouldApplyBattleRoyale();
}

static bool AreOnSameItemBubbleTeam(u8 firstPlayerId, u8 secondPlayerId) {
    const Racedata* racedata = Racedata::sInstance;
    if (racedata == nullptr) return false;
    const RacedataScenario& scenario = racedata->racesScenario;
    if (firstPlayerId >= scenario.playerCount || secondPlayerId >= scenario.playerCount) return false;
    if (firstPlayerId == secondPlayerId) return true;

    const System* system = System::sInstance;
    UI::ExtendedTeamManager* extendedTeamMgr = UI::ExtendedTeamManager::sInstance;
    if (system != nullptr && system->IsContext(PULSAR_EXTENDEDTEAMS) && extendedTeamMgr != nullptr) {
        const UI::ExtendedTeamID firstTeam = extendedTeamMgr->GetPlayerTeam(firstPlayerId);
        return firstTeam != UI::TEAM_COUNT && firstTeam == extendedTeamMgr->GetPlayerTeam(secondPlayerId);
    }

    const Team firstTeam = scenario.players[firstPlayerId].team;
    return firstTeam != TEAM_NONE && firstTeam == scenario.players[secondPlayerId].team;
}

static bool IsOnAnyLocalItemBubbleTeam(u8 playerId) {
    const Racedata* racedata = Racedata::sInstance;
    if (racedata == nullptr) return false;

    const RacedataScenario& scenario = racedata->racesScenario;
    for (u8 localPlayer = 0; localPlayer < scenario.localPlayerCount; ++localPlayer) {
        const u8 localPlayerId = static_cast<u8>(racedata->GetPlayerIdOfLocalPlayer(localPlayer));
        if (AreOnSameItemBubbleTeam(playerId, localPlayerId)) return true;
    }
    return false;
}

static bool IsVanillaFourPlayerItemLightScreen(u8 playerId) {
    const Racedata* racedata = Racedata::sInstance;
    if (racedata == nullptr) return false;

    const RacedataScenario& scenario = racedata->racesScenario;
    if (playerId >= scenario.playerCount) return false;

    const PlayerType type = scenario.players[playerId].playerType;
    const u32 relativeType = static_cast<u32>(type - PLAYER_CPU);
    return relativeType >= 5 || ((1 << relativeType) & 0x19) == 0;
}

static void SetupVanillaItemLightAnimation(Item::Obj* obj) {
    if (obj->item_light == nullptr) return;

    const Racedata* racedata = Racedata::sInstance;
    if (racedata == nullptr) return;

    const RacedataScenario& scenario = racedata->racesScenario;
    const u8 screenCount = scenario.screenCount;
    if (obj->bitfield78 & 0x10000) {
        for (u32 i = 0; i < screenCount; ++i)
            obj->item_light->DisableScreen(i);
        return;
    }

    const u8 playerId = obj->playerUsedItemId;
    if (playerId >= scenario.playerCount) return;

    const Team team = scenario.players[playerId].team;
    ModelTransformator* transformator = obj->item_light->modelTransformator;
    if (transformator != nullptr) {
        transformator->PlayAnmNoBlend(team != TEAM_RED, 0.0f, 1.0f);
        transformator->PlayAnmNoBlend(2, 0.0f, 1.0f);
    }

    if (scenario.settings.gametype != GAMETYPE_ONLINE_SPECTATOR) {
        RaceCameraMgr* cameraMgr = RaceCameraMgr::sInstance;
        if (cameraMgr == nullptr || cameraMgr->sortedCameras == nullptr) return;

        for (u32 i = 0; i < screenCount; ++i) {
            RaceCamera* camera = cameraMgr->sortedCameras[i];
            if (camera != nullptr && camera->playerId < scenario.playerCount && scenario.players[camera->playerId].team == team)
                obj->item_light->EnableScreen(i);
            else
                obj->item_light->DisableScreen(i);
        }

        if (screenCount == 4 && IsVanillaFourPlayerItemLightScreen(2) && !IsVanillaFourPlayerItemLightScreen(3))
            obj->item_light->DisableScreen(3);
    }
}

static void LoadGraphicsAndItemLight(Item::Obj* obj, const char* mdlName, const char* shadowSrc,
                                     Item::Obj::AnmParam* anmParam,
                                     nw4r::g3d::ScnMdl::BufferOption option,
                                     u32 directorBitfield) {
    obj->LoadGraphicsImplicitBRRESNoFunc(mdlName, shadowSrc, anmParam, option, directorBitfield);
    if (ShouldUseBattleRoyaleItemBubbles()) obj->LoadItemLight();
}
kmCall(0x807af01c, LoadGraphicsAndItemLight);  // ObjKouraGreen non-teams path
kmCall(0x807a40d8, LoadGraphicsAndItemLight);  // ObjBanana non-teams path
kmCall(0x807ab3f4, LoadGraphicsAndItemLight);  // ObjKouraRed non-teams path

static void SetupItemLightAnimation(Item::Obj* obj) {
    if (!ShouldUseBattleRoyaleItemBubbles()) {
        SetupVanillaItemLightAnimation(obj);
        return;
    }

    if (obj->item_light == nullptr) return;

    const Racedata* racedata = Racedata::sInstance;
    if (racedata == nullptr) return;

    const RacedataScenario& scenario = racedata->racesScenario;
    const u8 screenCount = scenario.screenCount;

    // Hide on every screen first.
    for (u32 i = 0; i < screenCount; ++i)
        obj->item_light->DisableScreen(i);

    if (obj->bitfield78 & 0x10000) return;  // item is held/tethered - keep hidden

    // Show the bubble on every local screen whose player shares the item's team.
    const u8 playerId = obj->playerUsedItemId;
    if (playerId >= scenario.playerCount) return;

    // Play blue CLR animation (index 1) and the CHR loop animation (index 2).
    ModelTransformator* transformator = obj->item_light->modelTransformator;
    if (transformator == nullptr) return;

    bool showBubble = false;
    for (u8 localPlayer = 0; localPlayer < scenario.localPlayerCount; ++localPlayer) {
        const u8 localPlayerId = static_cast<u8>(racedata->GetPlayerIdOfLocalPlayer(localPlayer));
        if (!AreOnSameItemBubbleTeam(playerId, localPlayerId)) continue;

        const u8 hudSlot = racedata->GetHudSlotId(localPlayerId);
        if (hudSlot >= screenCount) continue;
        obj->item_light->EnableScreen(hudSlot);
        showBubble = true;
    }

    if (!showBubble) return;
    transformator->PlayAnmNoBlend(1, 0.0f, 1.0f);  // item_light_blue (CLR, slot 1)
    transformator->PlayAnmNoBlend(2, 0.0f, 1.0f);  // item_light      (CHR, slot 2)
}
kmBranch(0x8079e38c, SetupItemLightAnimation);

static void SpawnSetupAndFixFIBTexture(Item::Obj* obj, int objId) {
    // Call the original Item::Obj::Set which internally calls SpawnModel - this makes the
    // FIB visible and freezes the animation at the default (red) frame.
    obj->Set(static_cast<ItemObjId>(objId));

    if (!ShouldUseBattleRoyaleItemBubbles()) return;

    // Only post-process fake item boxes.
    if (obj->itemObjId != OBJ_FAKE_ITEM_BOX) return;

    // Use the friendly texture for items owned by a local player's teammate.
    const u8 playerId = obj->playerUsedItemId;
    if (playerId >= 12 || !IsOnAnyLocalItemBubbleTeam(playerId)) return;

    ModelDirector* mdl = obj->modelDirector;
    if (mdl == nullptr) return;
    ModelTransformator* transformator = mdl->modelTransformator;
    if (transformator == nullptr) return;

    // Freeze PAT and CLR animations at frame 1.0 (blue team texture).
    AnmHolder* patHolder = transformator->GetAnmHolderByType(ANMTYPE_TEXPAT);
    AnmHolder* clrHolder = transformator->GetAnmHolderByType(ANMTYPE_CLR);
    if (patHolder != nullptr) patHolder->UpdateRateAndSetFrame(1.0f);
    if (clrHolder != nullptr) clrHolder->UpdateRateAndSetFrame(1.0f);
}
kmCall(0x8079e590, SpawnSetupAndFixFIBTexture);  // bl FUN_8079e5f4 inside ItemObj_spawn

}  // namespace Extra
}  // namespace Pulsar
