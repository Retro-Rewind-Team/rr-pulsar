#include <Driver/CustomCharacters.hpp>
#include <IO/LooseArchiveOverrides.hpp>
#include <Network/Settings/SelectionRestrictions.hpp>
#include <Settings/Settings.hpp>
#include <MarioKartWii/Archive/ArchiveMgr.hpp>
#include <MarioKartWii/GlobalFunctions.hpp>
#include <MarioKartWii/3D/Model/Menu/MenuDriverModel.hpp>
#include <MarioKartWii/3D/Model/Menu/MenuKartModel.hpp>
#include <MarioKartWii/3D/Model/Menu/MenuModelMgr.hpp>
#include <MarioKartWii/3D/Scn/ScnMgr.hpp>
#include <MarioKartWii/Driver/Toadette.hpp>
#include <MarioKartWii/Input/ControllerHolder.hpp>
#include <MarioKartWii/Scene/GameScene.hpp>
#include <MarioKartWii/UI/Page/Menu/CharacterSelect.hpp>
#include <MarioKartWii/Audio/RSARPlayer.hpp>
#include <MarioKartWii/UI/Section/SectionMgr.hpp>
#include <UI/UI.hpp>
#include <core/egg/DVD/DvdRipper.hpp>
#include <core/egg/mem/ExpHeap.hpp>
#include <core/rvl/PAD.hpp>
#include <core/rvl/WPAD.hpp>
#include <core/rvl/dvd/dvd.hpp>

namespace Pulsar {
namespace Driver {

bool characterTables[CHARACTER_COUNT][MAX_CUSTOM_CHARACTER_SLOTS + 1];

u8 selectedSlots[4][CHARACTER_COUNT];
static u8 loadedSlots[4];
static s8 cycleDirections[4];
enum {
    AUTHOR_NAME_CONTROL_WORDS = (sizeof(CharaName) + sizeof(u32) - 1) / sizeof(u32)
};
static u32 authorNameControlStorage[4][AUTHOR_NAME_CONTROL_WORDS];
static bool authorNameControlLoaded[4];
static bool loadingAuthorNameControl;
static u32 authorTextBmgIds[4];
static MenuDriverModel *customModels[4];
static EGG::ExpHeap *customHeaps[4];
static ToadetteHair *customHair[4];

void CreateCharacterTable() {
    char path[0x80];

    for (u32 character = 0; character < CHARACTER_COUNT; ++character) {
        const CharacterId id = static_cast<CharacterId>(character);
        const char *name = ArchiveMgr::GetKartArchivePostfix(id);
        characterTables[character][0] = true;

        for (u32 slot = 1; slot <= MAX_CUSTOM_CHARACTER_SLOTS; ++slot) {
            snprintf(path, sizeof(path), "/Scene/Model/Driver/%s-%u.brres", name, slot);
            characterTables[character][slot] = IOOverrides::ConvertPathToEntryNumWithLooseOverride(path) >= 0;
        }
        for (u32 player = 0; player < 4; ++player) {
            if (!characterTables[character][selectedSlots[player][character]])
                selectedSlots[player][character] = 0;
        }
    }
}
static Settings::Hook CreateCharacterTableHook(CreateCharacterTable);

static s32 LoadKartArchive(char *path, u32 size, const char *format, const char *name, const ArchivesHolder *archive) {
    const u32 player = archive - ArchiveMgr::sInstance->kartModelsHolders;
    u32 character = 0;
    while (character < CHARACTER_COUNT && strcmp(name, ArchiveMgr::GetKartArchivePostfix(static_cast<CharacterId>(character))) != 0) {
        ++character;
    }

    const u32 slot = character < CHARACTER_COUNT && !Restrictions::IsCharacterRestrictionConfigActive() && !Restrictions::IsVehicleRestrictionConfigActive()
      ? selectedSlots[player][character] : 0;
    const char *battleSuffix = strstr(format, "_BT") != nullptr ? "_BT" : "";
    if (slot != 0) {
        char archivePath[0x80];
        snprintf(archivePath, sizeof(archivePath), "/Scene/Model/Kart/%s-%u-allkart%s.szs", name, slot, battleSuffix);
        if (IOOverrides::ConvertPathToEntryNumWithLooseOverride(archivePath) >= 0)
            return snprintf(path, size, "Scene/Model/Kart/%s-%u-allkart%s", name, slot, battleSuffix);
    }

    return snprintf(path, size, "Scene/Model/Kart/%s-allkart%s", name, battleSuffix);
}
kmCall(0x80541160, LoadKartArchive);
kmCall(0x805411a0, LoadKartArchive);
kmCall(0x80541f60, LoadKartArchive);
kmCall(0x80541fa0, LoadKartArchive);
kmCall(0x80542140, LoadKartArchive);
kmCall(0x80542180, LoadKartArchive);

// Pass the per-player archive in r7 at each verified snprintf call site.
kmWrite32(0x8054115c, 0x7fe7fb78);  // mr r7, r31
kmWrite32(0x8054119c, 0x7fe7fb78);  // mr r7, r31
kmWrite32(0x80541f5c, 0x7fc7f378);  // mr r7, r30
kmWrite32(0x80541f9c, 0x7fc7f378);  // mr r7, r30
kmWrite32(0x8054213c, 0x7fc7f378);  // mr r7, r30
kmWrite32(0x8054217c, 0x7fc7f378);  // mr r7, r30

static void UnloadDriverBRRES(u32 player) {
    if (customModels[player] == nullptr)
        return;

    MenuDriverModelMgr *manager = MenuModelMgr::sInstance->driverModels;
    if (manager->players[player].playerModel == customModels[player])
        manager->SetPlayerCharacter(player, manager->players[player].id);
    MenuKartModelMgr *kartModels = MenuModelMgr::sInstance->kartModels;
    for (u32 hud = 0; hud < kartModels->playerCount; ++hud) {
        if (!kartModels->players[hud].isLocked)
            continue;
        for (u32 kart = 0; kart < kartModels->modelCount; ++kart) {
            if (kartModels->players[hud].modelsArray[kart].driverModel == customModels[player])
                kartModels->players[hud].modelsArray[kart].driverModel = &manager->models[customModels[player]->id];
        }
    }
    ModelDirector *model = customModels[player]->model;
    ScnMgr *scnMgr = ScnMgr::sInstance[model->scnMgrIdx];
    if ((model->bitfield & 0x100000) && scnMgr != nullptr) {
        model->ToggleVisible(false);
        scnMgr->RemoveModelDirector(model);
    }
    if (customHair[player] != nullptr) {
        customHair[player]->ToggleVisible(false);
        ScnMgr::sInstance[customHair[player]->scnMgrIdx]->RemoveModelDirector(customHair[player]);
        customHair[player] = nullptr;
    }
    customModels[player] = nullptr;
    loadedSlots[player] = 0;
    customHeaps[player]->destroy();
    customHeaps[player] = nullptr;
}

bool LoadDriverBRRES(u8 player, CharacterId characterId, u32 slot) {
    if (Restrictions::IsCharacterRestrictionConfigActive() || Restrictions::IsVehicleRestrictionConfigActive())
        slot = 0;
    const u32 character = static_cast<u32>(characterId);
    if (character >= CHARACTER_COUNT || slot > MAX_CUSTOM_CHARACTER_SLOTS || !characterTables[character][slot])
        return false;
    if (customModels[player] != nullptr && customModels[player]->id == character && loadedSlots[player] == slot)
        return true;

    UnloadDriverBRRES(player);
    if (slot == 0 && SectionMgr::sInstance->sectionParams->localPlayerCount == 1)
        return true;

    char path[0x80];
    u32 fileSize = 0;
    if (slot != 0) {
        snprintf(path, sizeof(path), "/Scene/Model/Driver/%s-%u.brres", ArchiveMgr::GetKartArchivePostfix(characterId), slot);
        const s32 entryNum = IOOverrides::ConvertPathToEntryNumWithLooseOverride(path);
        DVD::FileInfo fileInfo = {};
        if (entryNum < 0 || !DVD::FastOpen(entryNum, &fileInfo))
            return false;
        fileSize = fileInfo.length;
        DVD::Close(&fileInfo);
    }

    GameScene *scene = const_cast<GameScene *>(GameScene::GetCurrent());
    // GameScene locks its dynamic heaps after setup, so briefly allow this child heap allocation.
    EGG::Heap *parentHeap = scene->structsHeaps.heaps[0];
    if (scene->structsHeaps.heaps[1]->getAllocatableSize(4) > parentHeap->getAllocatableSize(4))
        parentHeap = scene->structsHeaps.heaps[1];
    const u16 heapFlags = parentHeap->dameFlag;
    parentHeap->dameFlag &= ~1;
    EGG::ExpHeap *heap = EGG::ExpHeap::Create(fileSize + 0xe1000, parentHeap, 0);
    parentHeap->dameFlag = heapFlags;
    if (heap == nullptr)
        return false;

    EGG::Allocator *allocator = new (heap) EGG::Allocator(heap, 0x20);
    ScnMgr *scnMgr = ScnMgr::sInstance[0];
    EGG::Heap *oldHeap = scnMgr->curHeap;
    EGG::Allocator *oldAllocator = scnMgr->curAllocator;
    EGG::Allocator *oldMenuAllocator = menuAllocator;
    scnMgr->curHeap = heap;
    scnMgr->curAllocator = allocator;
    menuAllocator = allocator;
    EGG::Heap *oldCurrentHeap = heap->BecomeCurrentHeap();
    MenuDriverModel *driverModel = new (heap) MenuDriverModel;
    ModelDirector *model = driverModel->model;
    MenuModelBRRESHandle brresHandle;
    bool bound;
    if (slot == 0) {
        // BindDriverBRRES also resolves the stock menu versions of Peach, Daisy and Rosalina.
        bound = brresHandle.BindDriverBRRES(characterId);
    } else {
        brresHandle.menuModelBRRES.data = static_cast<g3d::ResFileData *>(EGG::DvdRipper::LoadToMainRAM(path, nullptr, heap, EGG::DvdRipper::ALLOC_FROM_HEAD, 0, nullptr, nullptr));
        bound = brresHandle.menuModelBRRES.data != nullptr;
        if (bound)
            ModelDirector::BindBRRESImpl(brresHandle.menuModelBRRES, path, nullptr, 0);
    }
    const bool loaded = bound && brresHandle.LoadDriverModel(*model, characterId);
    if (loaded && character == TOADETTE && ModelDirector::MdlExists("hair", brresHandle.menuModelBRRES))
        customHair[player] = new (heap) ToadetteHair(brresHandle.menuModelBRRES, model, 1);
    oldCurrentHeap->BecomeCurrentHeap();
    scnMgr->curHeap = oldHeap;
    scnMgr->curAllocator = oldAllocator;
    menuAllocator = oldMenuAllocator;

    if (!loaded) {
        ScnMgr *scnMgr = ScnMgr::sInstance[model->scnMgrIdx];
        if ((model->bitfield & 0x100000) && scnMgr != nullptr) {
            model->ToggleVisible(false);
            scnMgr->RemoveModelDirector(model);
        }
        heap->destroy();
        return false;
    }

    customModels[player] = driverModel;
    customHeaps[player] = heap;
    loadedSlots[player] = slot;
    driverModel->id = character;
    driverModel->state = MenuDriverModel::MENUDRIVERMODEL_STATE_IDLE;
    driverModel->charSelTransformator = model->modelTransformator;
    driverModel->onKartTransformator = nullptr;
    driverModel->Init();
    model->Update(scnMgr->curScnRootIdx);
    if (customHair[player] != nullptr)
        customHair[player]->Update(scnMgr->curScnRootIdx);
    heap->adjust();
    return true;
}

// Stock character buttons are exclusive; Miis already allow multiple players.
kmWrite32(0x807e29fc, 0x39000000);  // li r8, 0

static void UpdateDriverModels(MenuDriverModelMgr *manager) {
    for (u32 player = 0; player < manager->playerCount; ++player) {
        if (customModels[player] != nullptr) {
            customModels[player]->Update();
            customModels[player]->ToggleVisible(false);
        }
        if (customHair[player] != nullptr)
            customHair[player]->ToggleVisible(false);
    }
    manager->Update();
    for (u32 player = 0; player < manager->playerCount; ++player) {
        if (customHair[player] != nullptr && manager->players[player].playerModel == customModels[player]) {
            customHair[player]->ToggleVisible(true);
            ModelTransformator *transformator = customModels[player]->model->modelTransformator;
            bool lockHair = false;
            if (customModels[player]->state == MenuDriverModel::MENUDRIVERMODEL_STATE_ONCHARSELECT && transformator->IsTypeActive(ANMTYPE_CHR)) {
                const AnmHolder *anm = transformator->GetAnmHolderByType(ANMTYPE_CHR);
                lockHair = anm->GetFrameCount() <= anm->GetFrame();
            }
            customHair[player]->ToggleLock(lockHair);
        }
    }
}
kmCall(0x8059e4cc, UpdateDriverModels);

static void DrawDriverModels(MenuDriverModelMgr *manager, u8 playerId) {
    for (u32 player = 0; player < manager->playerCount; ++player) {
        if (customModels[player] != nullptr)
            customModels[player]->ToggleTransparent(false);
        if (customHair[player] != nullptr)
            customHair[player]->ToggleTransparent(false);
    }
    manager->Draw(playerId);
    if (manager->players[playerId].playerModel == customModels[playerId]) {
        // The stock renderer otherwise shows its second Toadette hair on any duplicate model.
        if (manager->secondBangs != nullptr)
            manager->secondBangs->ToggleTransparent(false);
        if (customHair[playerId] != nullptr)
            customHair[playerId]->ToggleTransparent(manager->players[playerId].isVisible);
    }
}
kmCall(0x8059e528, DrawDriverModels);

static void PageCheckActions(Page *page) {
    if (page->pageId != PAGE_CHARACTER_SELECT) {
        page->CheckActions();
        return;
    }

    Pages::CharacterSelect *characterSelectPage = static_cast<Pages::CharacterSelect *>(page);
    memset(cycleDirections, 0, sizeof(cycleDirections));
    for (u32 player = 0; player < 4; ++player) {
        if ((characterSelectPage->playerBitfield & (1 << player)) == 0)
            continue;
        Input::ControllerHolder *holder = SectionMgr::sInstance->pad.GetControllerHolder(player);
        if (holder == nullptr || holder->curController == nullptr)
            continue;

        const u16 raw = holder->uiinputStates[0].rawButtons;
        const u16 pressed = raw & ~holder->uiinputStates[1].rawButtons;
        u16 previousButton = 0;
        u16 nextButton = 0;
        switch (holder->curController->GetType()) {
            case GCN:
                previousButton = PAD::PAD_BUTTON_L;
                nextButton = PAD::PAD_BUTTON_R;
                break;
            case CLASSIC:
                previousButton = WPAD::WPAD_CL_TRIGGER_L;
                nextButton = WPAD::WPAD_CL_TRIGGER_R;
                break;
            case NUNCHUCK:
                previousButton = WPAD::WPAD_BUTTON_1;
                nextButton = WPAD::WPAD_BUTTON_2;
                holder->uiinputStates[0].buttonActions &= ~0x3;
                if (raw & WPAD::WPAD_BUTTON_A)
                    holder->uiinputStates[0].buttonActions |= 0x1;
                if (raw & WPAD::WPAD_BUTTON_B)
                    holder->uiinputStates[0].buttonActions |= 0x2;
                break;
            default:
                previousButton = WPAD::WPAD_BUTTON_B;
                nextButton = WPAD::WPAD_BUTTON_A;
                holder->uiinputStates[0].buttonActions &= ~0x3;
                if (raw & WPAD::WPAD_BUTTON_2)
                    holder->uiinputStates[0].buttonActions |= 0x1;
                if (raw & WPAD::WPAD_BUTTON_1)
                    holder->uiinputStates[0].buttonActions |= 0x2;
                break;
        }

        if ((pressed & (previousButton | nextButton)) == previousButton) {
            cycleDirections[player] = -1;
        } else if ((pressed & (previousButton | nextButton)) == nextButton) {
            cycleDirections[player] = 1;
        }
    }
    // Remap skin inputs before the manipulator can dispatch Forward or Back.
    page->CheckActions();
}
kmCall(0x806225dc, PageCheckActions);

static void PageAfterControlUpdate(Page *page) {
    typedef void (*PageFunction)(Page *);
    PageFunction *vtable = *reinterpret_cast<PageFunction **>(page);
    vtable[19](page);
    if (page->pageId != PAGE_CHARACTER_SELECT)
        return;

    Pages::CharacterSelect *characterSelectPage = static_cast<Pages::CharacterSelect *>(page);
    for (u32 player = 0; player < 4; ++player) {
        const s8 direction = cycleDirections[player];
        cycleDirections[player] = 0;
        if (direction == 0 || (characterSelectPage->playerBitfield & (1 << player)) == 0)
            continue;

        const u32 character = static_cast<u32>(characterSelectPage->models[player].curCharacter);
        if (character >= CHARACTER_COUNT)
            continue;
        if (characterSelectPage->controlsManipulatorManager.inaccessible || !characterSelectPage->controlsManipulatorManager.holders[player].info.enabled)
            continue;

        u32 slot = selectedSlots[player][character];
        for (u32 tries = 0; tries <= MAX_CUSTOM_CHARACTER_SLOTS; ++tries) {
            if (direction < 0)
                slot = slot == 0 ? MAX_CUSTOM_CHARACTER_SLOTS : slot - 1;
            else
                slot = slot == MAX_CUSTOM_CHARACTER_SLOTS ? 0 : slot + 1;
            if (characterTables[character][slot])
                break;
        }
        if (slot == selectedSlots[player][character])
            continue;
        selectedSlots[player][character] = slot;
        if (!LoadDriverBRRES(player, static_cast<CharacterId>(character), slot))
            selectedSlots[player][character] = 0;
        else
            Audio::RSARPlayer::PlaySoundById(direction < 0 ? SOUND_ID_LEFT_ARROW_PRESS : SOUND_ID_RIGHT_ARROW_PRESS, 0, 0);
        if (player == 0) {
            UI::SetCharacterSelectIcon(
              characterSelectPage->ctrlMenuCharSelect.GetButtonDriver(static_cast<CharacterId>(character)), "chara_c_down", GetCharacterIconPaneName(static_cast<CharacterId>(character)));
        }
    }

    for (u32 player = 0; player < 4; ++player) {
        if ((characterSelectPage->playerBitfield & (1 << player)) == 0)
            continue;
        const ControllerType controllerType = SectionPad::GetType(SectionMgr::sInstance->pad.GetCurrentID(player));
        characterSelectPage->names[player].SetPaneVisibility("cc_prev_gc", controllerType == GCN);
        characterSelectPage->names[player].SetPaneVisibility("cc_next_gc", controllerType == GCN);
        characterSelectPage->names[player].SetPaneVisibility("cc_prev_cls", controllerType == CLASSIC);
        characterSelectPage->names[player].SetPaneVisibility("cc_next_cls", controllerType == CLASSIC);
        characterSelectPage->names[player].SetPaneVisibility("cc_prev_nc", controllerType == NUNCHUCK);
        characterSelectPage->names[player].SetPaneVisibility("cc_next_nc", controllerType == NUNCHUCK);
        characterSelectPage->names[player].SetPaneVisibility("cc_prev_wh", controllerType == WHEEL);
        characterSelectPage->names[player].SetPaneVisibility("cc_next_wh", controllerType == WHEEL);
        const u32 character = static_cast<u32>(characterSelectPage->models[player].curCharacter);
        if (character >= CHARACTER_COUNT)
            continue;
        MenuDriverModelMgr *manager = MenuModelMgr::sInstance->driverModels;
        if (!LoadDriverBRRES(player, static_cast<CharacterId>(character), selectedSlots[player][character]))
            selectedSlots[player][character] = 0;
        else if (customModels[player] != nullptr)
            manager->players[player].playerModel = customModels[player];
        characterSelectPage->names[player].SetMessage(UI::GetCharacterNameBMGId(character, false, 12 + player));
    }

    for (u32 player = 0; player < 4; ++player) {
        if ((characterSelectPage->playerBitfield & (1 << player)) == 0 || !authorNameControlLoaded[player])
            continue;
        CharaName *author = reinterpret_cast<CharaName *>(&authorNameControlStorage[player][0]);
        if (characterSelectPage->localPlayerCount > 1) {
            author->isHidden = true;
            authorTextBmgIds[player] = 0;
            continue;
        }
        const u32 character = static_cast<u32>(characterSelectPage->models[player].curCharacter);
        const u32 authorBmgId = character < CHARACTER_COUNT ? UI::GetCharacterAuthorBMGId(character, selectedSlots[player][character]) : 0;
        if (authorTextBmgIds[player] == authorBmgId)
            continue;
        author->isHidden = authorBmgId == 0 || !UI::SetCustomCharacterAuthorMessage(*author, authorBmgId);
        authorTextBmgIds[player] = authorBmgId;
    }
}
kmCall(0x80602318, PageAfterControlUpdate);

static void CharacterSelectName(ControlLoader *loader, const char *folderName, const char *ctrName, const char *variant, const char **animNames) {
    loader->Load(folderName, ctrName, variant, animNames);
    if (loadingAuthorNameControl || Restrictions::IsCharacterRestrictionConfigActive())
        return;
    CharaName &name = *static_cast<CharaName *>(loader->layoutUIControl);
    const u32 hud = name.unknown_0x178;
    if (hud >= 4)
        return;

    CharaName *author = reinterpret_cast<CharaName *>(&authorNameControlStorage[hud][0]);
    authorNameControlLoaded[hud] = false;
    authorTextBmgIds[hud] = 0;
    new (author) CharaName;
    author->unknown_0x178 = hud;
    name.InitControlGroup(1);
    name.AddControl(0, author);
    loadingAuthorNameControl = true;
    ControlLoader authorLoader(author);
    authorLoader.Load(folderName, ctrName, variant, nullptr);
    loadingAuthorNameControl = false;

    const char *panes[] = {
        "Window_00", "black_parts_t_00", "black_parts_t_01", "select_base", "border", "cc_prev_wh", "cc_next_wh", "cc_prev_nc", "cc_next_nc", "cc_prev_cls", "cc_next_cls", "cc_prev_gc", "cc_next_gc"};
    for (u32 i = 0; i < sizeof(panes) / sizeof(panes[0]); ++i) {
        if (author->layout.GetPaneByName(panes[i]) != nullptr)
            author->SetPaneVisibility(panes[i], false);
    }
    for (u32 i = 0; i < sizeof(author->positionAndscale) / sizeof(author->positionAndscale[0]); ++i) {
        author->positionAndscale[i].position = name.positionAndscale[i].position;
        author->positionAndscale[i].position.y -= 14.5f;
        author->positionAndscale[i].scale.x *= 1.1f;
    }
    author->isHidden = true;
    authorNameControlLoaded[hud] = true;
}
kmCall(0x8083d9dc, CharacterSelectName);

static void RequestDriverModel(MenuModelMgr *manager, u8 playerId, CharacterId characterId) {
    const u32 character = static_cast<u32>(characterId);
    manager->RequestDriverModel(playerId, characterId);
    if (!manager->isActive || playerId >= SectionMgr::sInstance->sectionParams->localPlayerCount)
        return;
    if (character < CHARACTER_COUNT) {
        if (LoadDriverBRRES(playerId, characterId, selectedSlots[playerId][character]) && customModels[playerId] != nullptr)
            manager->driverModels->players[playerId].playerModel = customModels[playerId];
    } else {
        UnloadDriverBRRES(playerId);
    }
}
kmCall(0x805f5604, RequestDriverModel);
kmCall(0x805f5918, RequestDriverModel);
kmBranch(0x805f5704, RequestDriverModel);

static void ResetScnMgr() {
    for (u32 player = 0; player < 4; ++player) UnloadDriverBRRES(player);
    ScnMgr::Reset();
}
kmCall(0x8051b118, ResetScnMgr);

}  // namespace Driver
}  // namespace Pulsar
