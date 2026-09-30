#include <Driver/CustomCharacters.hpp>
#include <MarioKartWii/Archive/ArchiveMgr.hpp>
#include <MarioKartWii/3D/Model/Menu/MenuDriverModel.hpp>
#include <MarioKartWii/3D/Model/Menu/MenuModelMgr.hpp>
#include <MarioKartWii/3D/Scn/ScnMgr.hpp>
#include <MarioKartWii/Driver/Toadette.hpp>
#include <MarioKartWii/Input/ControllerHolder.hpp>
#include <MarioKartWii/Scene/GameScene.hpp>
#include <MarioKartWii/UI/Page/Menu/CharacterSelect.hpp>
#include <MarioKartWii/Audio/RSARPlayer.hpp>
#include <MarioKartWii/UI/Section/SectionMgr.hpp>
#include <UI/UI.hpp>
#include <core/egg/mem/ExpHeap.hpp>
#include <core/rvl/PAD.hpp>
#include <core/rvl/WPAD.hpp>
#include <core/rvl/dvd/dvd.hpp>

namespace Pulsar {
namespace Driver {

bool characterTables[CHARACTER_COUNT][MAX_CUSTOM_CHARACTER_SLOTS + 1];

u8 selectedSlots[CHARACTER_COUNT];
static u8 loadedSlots[CHARACTER_COUNT];
static s8 cycleDirections[4];
static ModelDirector *originalModels[CHARACTER_COUNT];
static ModelTransformator *originalTransformators[CHARACTER_COUNT];
static bool originalWasVisible[CHARACTER_COUNT];
static ModelDirector *customModels[CHARACTER_COUNT];
static EGG::ExpHeap *customHeaps[CHARACTER_COUNT];

void CreateCharacterTable() {
    char path[0x80];

    for (u32 character = 0; character < CHARACTER_COUNT; ++character) {
        const CharacterId id = static_cast<CharacterId>(character);
        const char *name = ArchiveMgr::GetKartArchivePostfix(id);
        characterTables[character][0] = true;

        for (u32 slot = 1; slot <= MAX_CUSTOM_CHARACTER_SLOTS; ++slot) {
            snprintf(path, sizeof(path), "/Scene/Model/Driver/%s-%u.brres", name, slot);
            characterTables[character][slot] = DVD::ConvertPathToEntryNum(path) >= 0;
        }
    }
}

static s32 LoadKartArchive(char *path, u32 size, const char *format, const char *name) {
    u32 character = 0;
    while (character < CHARACTER_COUNT && strcmp(name, ArchiveMgr::GetKartArchivePostfix(static_cast<CharacterId>(character))) != 0) {
        ++character;
    }

    const u32 slot = character < CHARACTER_COUNT ? selectedSlots[character] : 0;
    const char *battleSuffix = strstr(format, "_BT") != nullptr ? "_BT" : "";
    if (slot != 0) {
        char archivePath[0x80];
        snprintf(archivePath, sizeof(archivePath), "/Scene/Model/Kart/%s-%u-allkart%s.szs", name, slot, battleSuffix);
        if (DVD::ConvertPathToEntryNum(archivePath) >= 0)
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

static void UnloadDriverBRRES(u32 character) {
    if (customModels[character] == nullptr) return;

    ModelDirector *model = customModels[character];
    ScnMgr *scnMgr = ScnMgr::sInstance[model->scnMgrIdx];
    if ((model->bitfield & 0x100000) && scnMgr != nullptr) {
        model->ToggleVisible(false);
        scnMgr->RemoveModelDirector(model);
    }

    MenuDriverModel *driverModel = &MenuModelMgr::sInstance->driverModels->models[character];
    if (character == TOADETTE) {
        ToadetteHair *hair = MenuModelMgr::sInstance->driverModels->bangs;
        hair->toadette = originalModels[character];
        hair->cb->toadette = originalModels[character];
        for (u32 i = 0; i < 2; ++i) {
            ModelCalcCBBoneLinked *callback = static_cast<ModelCalcCBBoneLinked *>(
                static_cast<EmptyModelCalcParent *>(hair->scnMdlEx[i]->scnObj->callback));
            callback->other = originalModels[character];
        }
    }
    driverModel->model = originalModels[character];
    driverModel->charSelTransformator = originalTransformators[character];
    if (scnMgr != nullptr) {
        driverModel->Init();
        originalModels[character]->ToggleVisible(originalWasVisible[character]);
    }

    customModels[character] = nullptr;
    loadedSlots[character] = 0;
    customHeaps[character]->freeAll();
    customHeaps[character]->destroy();
    customHeaps[character] = nullptr;
    originalModels[character] = nullptr;
    originalTransformators[character] = nullptr;
    originalWasVisible[character] = false;
}

bool LoadDriverBRRES(CharacterId characterId, u32 slot) {
    const u32 character = static_cast<u32>(characterId);
    if (character >= CHARACTER_COUNT || slot > MAX_CUSTOM_CHARACTER_SLOTS || !characterTables[character][slot]) return false;
    if (loadedSlots[character] == slot) return true;

    UnloadDriverBRRES(character);
    if (slot == 0) return true;

    MenuDriverModelMgr *manager = MenuModelMgr::sInstance->driverModels;
    MenuDriverModel *driverModel = &manager->models[character];
    if (driverModel->model == nullptr) return false;

    char path[0x80];
    snprintf(path, sizeof(path), "/Scene/Model/Driver/%s-%u.brres", ArchiveMgr::GetKartArchivePostfix(characterId), slot);
    const s32 entryNum = DVD::ConvertPathToEntryNum(path);
    DVD::FileInfo fileInfo = {};
    if (entryNum < 0 || !DVD::FastOpen(entryNum, &fileInfo)) return false;
    const u32 fileSize = fileInfo.length;
    DVD::Close(&fileInfo);

    GameScene *scene = const_cast<GameScene *>(GameScene::GetCurrent());
    // GameScene locks its dynamic heaps after setup, so briefly allow this child heap allocation.
    EGG::Heap *parentHeap = scene->structsHeaps.heaps[0];
    const u16 heapFlags = parentHeap->dameFlag;
    parentHeap->dameFlag &= ~1;
    EGG::ExpHeap *heap = EGG::ExpHeap::Create(fileSize + 0xe1000, parentHeap, 0);
    parentHeap->dameFlag = heapFlags;
    if (heap == nullptr) return false;

    EGG::Allocator *allocator = new (heap) EGG::Allocator(heap, 0x20);
    ScnMgr *scnMgr = ScnMgr::sInstance[0];
    EGG::Heap *oldHeap = scnMgr->curHeap;
    EGG::Allocator *oldAllocator = scnMgr->curAllocator;
    EGG::Allocator *oldMenuAllocator = menuAllocator;
    scnMgr->curHeap = heap;
    scnMgr->curAllocator = allocator;
    menuAllocator = allocator;

    ModelDirector *model = new (heap) ModelDirector(2, 0);
    g3d::ResFile brres;
    ModelDirector::RipAndBindBRRES(brres, path, heap, true);
    MenuModelBRRESHandle brresHandle;
    brresHandle.menuModelBRRES = brres;
    const bool loaded = brresHandle.LoadDriverModel(*model, characterId);

    scnMgr->curHeap = oldHeap;
    scnMgr->curAllocator = oldAllocator;
    menuAllocator = oldMenuAllocator;

    if (!loaded) {
        ScnMgr *scnMgr = ScnMgr::sInstance[model->scnMgrIdx];
        if ((model->bitfield & 0x100000) && scnMgr != nullptr) {
            model->ToggleVisible(false);
            scnMgr->RemoveModelDirector(model);
        }
        heap->freeAll();
        heap->destroy();
        return false;
    }

    if (originalModels[character] == nullptr) {
        originalModels[character] = driverModel->model;
        originalTransformators[character] = driverModel->charSelTransformator;
        originalWasVisible[character] = (originalModels[character]->bitfield & 0x200000) != 0;
    }

    originalModels[character]->ToggleVisible(false);
    customModels[character] = model;
    customHeaps[character] = heap;
    loadedSlots[character] = slot;
    driverModel->model = model;
    driverModel->charSelTransformator = model->modelTransformator;
    driverModel->Init();
    if (character == TOADETTE) {
        ToadetteHair *hair = manager->bangs;
        hair->toadette = model;
        hair->cb->toadette = model;
        for (u32 i = 0; i < 2; ++i) {
            ModelCalcCBBoneLinked *callback = static_cast<ModelCalcCBBoneLinked *>(
                static_cast<EmptyModelCalcParent *>(hair->scnMdlEx[i]->scnObj->callback));
            callback->other = model;
        }
    }
    return true;
}

static void PageBeforeControlUpdate(Page *page) {
    typedef void (*PageFunction)(Page *);
    PageFunction *vtable = *reinterpret_cast<PageFunction **>(page);
    vtable[18](page);
    if (page->pageId != PAGE_CHARACTER_SELECT) return;

    Pages::CharacterSelect *characterSelectPage = static_cast<Pages::CharacterSelect *>(page);
    memset(cycleDirections, 0, sizeof(cycleDirections));
    for (u32 player = 0; player < 4; ++player) {
        if ((characterSelectPage->localPlayerBitfield & (1 << player)) == 0) continue;
        Input::ControllerHolder *holder = SectionMgr::sInstance->pad.GetControllerHolder(player);
        if (holder == nullptr || holder->curController == nullptr) continue;

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
                previousButton = WPAD::WPAD_BUTTON_C;
                nextButton = WPAD::WPAD_BUTTON_Z;
                if (raw & WPAD::WPAD_BUTTON_C) holder->uiinputStates[0].buttonActions &= ~0x100;
                break;
            default:
                previousButton = WPAD::WPAD_BUTTON_B;
                nextButton = WPAD::WPAD_BUTTON_A;
                holder->uiinputStates[0].buttonActions &= ~0x3;
                if (raw & WPAD::WPAD_BUTTON_2) holder->uiinputStates[0].buttonActions |= 0x1;
                if (raw & WPAD::WPAD_BUTTON_1) holder->uiinputStates[0].buttonActions |= 0x2;
                break;
        }

        if ((pressed & (previousButton | nextButton)) == previousButton) {
            cycleDirections[player] = -1;
            Audio::RSARPlayer::PlaySoundById(SOUND_ID_LEFT_ARROW_PRESS, 0, 0);
        } else if ((pressed & (previousButton | nextButton)) == nextButton) {
            cycleDirections[player] = 1;
            Audio::RSARPlayer::PlaySoundById(SOUND_ID_RIGHT_ARROW_PRESS, 0, 0);
        }
    }
}
kmCall(0x806022fc, PageBeforeControlUpdate);

static void PageAfterControlUpdate(Page *page) {
    typedef void (*PageFunction)(Page *);
    PageFunction *vtable = *reinterpret_cast<PageFunction **>(page);
    vtable[19](page);
    if (page->pageId != PAGE_CHARACTER_SELECT) return;

    Pages::CharacterSelect *characterSelectPage = static_cast<Pages::CharacterSelect *>(page);
    bool changed[CHARACTER_COUNT] = {};
    for (u32 player = 0; player < 4; ++player) {
        const s8 direction = cycleDirections[player];
        cycleDirections[player] = 0;
        if (direction == 0 || (characterSelectPage->localPlayerBitfield & (1 << player)) == 0) continue;

        const u32 character = static_cast<u32>(characterSelectPage->models[player].curCharacter);
        if (character >= CHARACTER_COUNT || changed[character]) continue;
        changed[character] = true;

        u32 slot = selectedSlots[character];
        for (u32 tries = 0; tries <= MAX_CUSTOM_CHARACTER_SLOTS; ++tries) {
            if (direction < 0)
                slot = slot == 0 ? MAX_CUSTOM_CHARACTER_SLOTS : slot - 1;
            else
                slot = slot == MAX_CUSTOM_CHARACTER_SLOTS ? 0 : slot + 1;
            if (characterTables[character][slot]) break;
        }
        selectedSlots[character] = slot;
        if (!LoadDriverBRRES(static_cast<CharacterId>(character), slot)) selectedSlots[character] = 0;
    }

    for (u32 character = 0; character < CHARACTER_COUNT; ++character) {
        bool focused = false;
        for (u32 player = 0; player < 4; ++player) {
            if ((characterSelectPage->localPlayerBitfield & (1 << player)) != 0 &&
                static_cast<u32>(characterSelectPage->models[player].curCharacter) == character) {
                focused = true;
                break;
            }
        }

        const u32 slot = focused ? selectedSlots[character] : 0;
        if ((customModels[character] != nullptr && loadedSlots[character] != slot) ||
            (customModels[character] == nullptr && slot != 0)) {
            if (!LoadDriverBRRES(static_cast<CharacterId>(character), slot)) selectedSlots[character] = 0;
        }
    }

    for (u32 player = 0; player < 4; ++player) {
        if ((characterSelectPage->localPlayerBitfield & (1 << player)) == 0) continue;
        const u32 character = static_cast<u32>(characterSelectPage->models[player].curCharacter);
        if (changed[character] && character < 24)
            characterSelectPage->names[player].SetMessage(UI::GetCharacterNameBMGId(character, false, player));
    }
}
kmCall(0x80602318, PageAfterControlUpdate);

static void RequestDriverModel(MenuModelMgr *manager, u8 playerId, CharacterId characterId) {
    manager->RequestDriverModel(playerId, characterId);
    const u32 character = static_cast<u32>(characterId);
    if (manager->isActive && character < CHARACTER_COUNT && selectedSlots[character] != 0)
        LoadDriverBRRES(characterId, selectedSlots[character]);
}
kmCall(0x805f5604, RequestDriverModel);

static void MenuModelMgrDestroy() {
    for (u32 character = 0; character < CHARACTER_COUNT; ++character) UnloadDriverBRRES(character);
    MenuModelMgr::DestroyInstance();
}
kmCall(0x805552b0, MenuModelMgrDestroy);

}  // namespace Driver
}  // namespace Pulsar
