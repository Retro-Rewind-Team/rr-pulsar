#include <CustomCharacters/CustomCharacters.hpp>
#include <Network/FriendRoomCPUs.hpp>

namespace Pulsar {
namespace CustomCharacters {

struct RawBRRES {
    EGG::ExpHeap *heap;
    void *file;
    bool failed;
    bool bound;
};

struct RawTPL {
    bool failed;
};

struct ReloadedMenuDriverModel {
    EGG::ExpHeap *heap;
    ModelDirector *model;
    ToadetteHair *hair;
};

static RawBRRES rawBRRES[TABLE_COUNT][CHARACTER_COUNT];
static RawBRRES looseMiiCBRRES[MII_C_COUNT];
static RawTPL looseMinimapTPL[TABLE_COUNT][CHARACTER_COUNT];
static const GameScene *rawCacheSceneOwner;
static ReloadedMenuDriverModel reloadedMenuDriverModels[MENU_DRIVER_MODEL_COUNT];
static const GameScene *reloadedMenuDriverModelSceneOwner;
static MenuDriverModel *reloadedMenuDriverModelOwner;

static void UnlockHeap(EGG::Heap *heap) {
    if (heap != nullptr) heap->dameFlag &= ~0x1;
}

static bool IsInHeap(const EGG::ExpHeap *heap, const void *ptr) {
    if (heap == nullptr || ptr == nullptr || heap->rvlHeap == nullptr) return false;
    const u8 *start = reinterpret_cast<const u8 *>(heap->rvlHeap->startAddr);
    const u8 *end = reinterpret_cast<const u8 *>(heap->rvlHeap->endAddr);
    const u8 *address = reinterpret_cast<const u8 *>(ptr);
    return start != nullptr && end != nullptr && start < end && address >= start && address < end;
}

static void DetachHeapListNodes(nw4r::ut::List *list, const EGG::ExpHeap *heap) {
    for (void *node = nw4r::ut::List_GetNext(list, nullptr); node != nullptr;) {
        void *next = nw4r::ut::List_GetNext(list, node);
        if (IsInHeap(heap, node)) nw4r::ut::List_Remove(list, node);
        node = next;
    }
}

static void DetachHeapFromScnMgrs(const EGG::ExpHeap *heap) {
    if (heap == nullptr) return;
    ScnMgr *const *mgrs = ScnMgr::sInstance;
    for (u32 i = 0; i < 2; ++i) {
        ScnMgr *mgr = mgrs[i];
        if (mgr == nullptr) continue;
        DetachHeapListNodes(&mgr->modelDirectors, heap);
        DetachHeapListNodes(&mgr->screenSpecificModelDirectors, heap);
        DetachHeapListNodes(&mgr->scnGroupExHolderList, heap);
        DetachHeapListNodes(&mgr->hardcodedMatNamesModelDirectors, heap);
    }
}

static void DestroyHeap(EGG::ExpHeap *&heap) {
    if (heap == nullptr) return;
    DetachHeapFromScnMgrs(heap);
    UnlockHeap(heap);
    heap->destroy();
    heap = nullptr;
}

static void ClearRawCache(RawBRRES &cache, bool destroyHeap) {
    if (destroyHeap)
        DestroyHeap(cache.heap);
    else
        cache.heap = nullptr;
    cache.file = nullptr;
    cache.failed = false;
    cache.bound = false;
}

void SyncRawCachesToCurrentScene() {
    const GameScene *const scene = GameScene::GetCurrent();
    if (rawCacheSceneOwner == scene) return;
    const SectionMgr *mgr = SectionMgr::sInstance;
    if (mgr != nullptr && ((mgr->curSection != nullptr && IsGameplaySection(mgr->curSection->sectionId)) ||
                           (mgr->nextSectionId != SECTION_NONE && IsGameplaySection(mgr->nextSectionId)))) {
        return;
    }
    for (u32 table = 0; table < TABLE_COUNT; ++table) {
        for (u32 character = 0; character < CHARACTER_COUNT; ++character) ClearRawCache(rawBRRES[table][character], true);
    }
    for (u32 i = 0; i < MII_C_COUNT; ++i) ClearRawCache(looseMiiCBRRES[i], true);
    rawCacheSceneOwner = scene;
}

// Loose BRRES files are loaded once, then bound into each model holder.
bool LoadRawBRRES(void *holder, RawBRRES &cache, const char *path) {
    if (cache.failed) return false;
    if (cache.file == nullptr) {
        u32 fileSize = 0;
        if (!DiscFileSize(path, fileSize)) {
            cache.failed = true;
            return false;
        }
        GameScene *scene = const_cast<GameScene *>(GameScene::GetCurrent());
        if (scene == nullptr) {
            cache.failed = true;
            return false;
        }
        const u32 heapSize = ((fileSize + 0x1f) & ~0x1f) + 0x2000;
        static const u32 PARENT_RESERVE = 0x200000;
        EGG::Heap *heaps[] = {scene->structsHeaps.heaps[1], scene->structsHeaps.heaps[0]};
        EGG::Heap *parent = nullptr;
        u32 bestSize = 0;
        for (u32 i = 0; i < ARRAY_COUNT(heaps); ++i) {
            if (heaps[i] == nullptr) continue;
            UnlockHeap(heaps[i]);
            const u32 size = heaps[i]->getAllocatableSize(0x20);
            if (size >= heapSize + PARENT_RESERVE) {
                parent = heaps[i];
                break;
            }
            if (size > bestSize) {
                parent = heaps[i];
                bestSize = size;
            }
        }
        if (bestSize < heapSize && (parent == nullptr || parent->getAllocatableSize(0x20) < heapSize)) parent = nullptr;
        if (parent == nullptr) {
            cache.failed = true;
            return false;
        }
        cache.heap = EGG::ExpHeap::Create(static_cast<int>(heapSize), parent, 0);
        if (cache.heap == nullptr) {
            cache.failed = true;
            return false;
        }
        u32 loadedSize = 0;
        cache.file = LoadFileToMainRAM(path, cache.heap, EGG::DvdRipper::ALLOC_FROM_HEAD, &loadedSize);
        if (cache.file == nullptr || loadedSize == 0) {
            ClearRawCache(cache, true);
            cache.failed = true;
            return false;
        }
    }
    if ((reinterpret_cast<u32>(cache.file) & 0x1f) != 0) {
        cache.failed = true;
        return false;
    }
    nw4r::g3d::ResFile &resFile = *reinterpret_cast<nw4r::g3d::ResFile *>(reinterpret_cast<u8 *>(holder) + 4);
    resFile.data = reinterpret_cast<nw4r::g3d::ResFileData *>(cache.file);
    if (!cache.bound) {
        ModelDirector::BindBRRESImpl(resFile, path, nullptr, 0);
        cache.bound = true;
    }
    return true;
}

bool LoadRawBRRESIntoHeap(void *holder, EGG::Heap *heap, const char *path, u32 fileSize) {
    if (holder == nullptr || heap == nullptr || path == nullptr || fileSize == 0) return false;
    u32 loadedSize = 0;
    void *file = LoadFileToMainRAM(path, heap, EGG::DvdRipper::ALLOC_FROM_HEAD, &loadedSize);
    if (file == nullptr || loadedSize == 0) return false;
    if ((reinterpret_cast<u32>(file) & 0x1f) != 0) {
        heap->free(file);
        return false;
    }

    nw4r::g3d::ResFile &resFile = *reinterpret_cast<nw4r::g3d::ResFile *>(reinterpret_cast<u8 *>(holder) + 4);
    resFile.data = reinterpret_cast<nw4r::g3d::ResFileData *>(file);
    ModelDirector::BindBRRESImpl(resFile, path, nullptr, 0);
    return true;
}

// Mii outfit C files are separate loose BRRES overrides.
u8 MiiCIndex(CharacterId character) {
    switch (character) {
        case MII_S_C_MALE:
            return 0;
        case MII_S_C_FEMALE:
            return 1;
        case MII_M_C_MALE:
            return 2;
        case MII_M_C_FEMALE:
            return 3;
        case MII_L_C_MALE:
            return 4;
        case MII_L_C_FEMALE:
            return 5;
        default:
            return MII_C_COUNT;
    }
}

bool TryLoadCustomMenuBRRES(void *holder, CharacterId character) {
    const CharacterId menuCharacter = MenuBRRESCharacter(character);
    const u8 table = ResolveMenuTable(menuCharacter);
    if (table == TABLE_DEFAULT || table >= TABLE_COUNT || !HasSkin(menuCharacter, table)) return false;
    char path[0x60];
    if (!BuildDriverPath(menuCharacter, table, path, sizeof(path))) return false;
    return LoadRawBRRES(holder, rawBRRES[table][menuCharacter], path);
}

bool TryLoadLooseMiiCBRRES(void *holder, CharacterId character) {
    const u8 idx = MiiCIndex(character);
    if (idx >= MII_C_COUNT) return false;
    char path[0x60];
    if (!BuildDriverPath(character, TABLE_DEFAULT, path, sizeof(path))) return false;
    return LoadRawBRRES(holder, looseMiiCBRRES[idx], path);
}

bool TryLoadCustomMenuBRRESIntoHeap(void *holder, CharacterId character, EGG::Heap *heap, bool &fileExists) {
    fileExists = false;
    const CharacterId menuCharacter = MenuBRRESCharacter(character);
    const u8 table = ResolveMenuTable(menuCharacter);
    if (table == TABLE_DEFAULT || table >= TABLE_COUNT || !HasSkin(menuCharacter, table)) return false;
    char path[0x60];
    if (!BuildDriverPath(menuCharacter, table, path, sizeof(path))) return false;
    u32 fileSize = 0;
    if (!DiscFileSize(path, fileSize)) {
        rawBRRES[table][menuCharacter].failed = true;
        return false;
    }
    fileExists = true;
    return LoadRawBRRESIntoHeap(holder, heap, path, fileSize);
}

bool TryLoadLooseMiiCBRRESIntoHeap(void *holder, CharacterId character, EGG::Heap *heap, bool &fileExists) {
    fileExists = false;
    const u8 idx = MiiCIndex(character);
    if (idx >= MII_C_COUNT) return false;
    char path[0x60];
    if (!BuildDriverPath(character, TABLE_DEFAULT, path, sizeof(path))) return false;
    u32 fileSize = 0;
    if (!DiscFileSize(path, fileSize)) {
        looseMiiCBRRES[idx].failed = true;
        return false;
    }
    fileExists = true;
    return LoadRawBRRESIntoHeap(holder, heap, path, fileSize);
}

// Menu driver BRRES loads prefer selected loose skins, then loose Mii C, then vanilla.
u32 LoadMenuDriverBRRESHook(void *holder, CharacterId character) {
    SyncRawCachesToCurrentScene();
    ScnMgr *const *mgrs = ScnMgr::sInstance;
    ScnMgr *scnMgr = mgrs[0];
    EGG::Heap *heap = scnMgr != nullptr ? scnMgr->curHeap : static_cast<EGG::Heap *>(nullptr);
    bool fileExists = false;
    if (TryLoadCustomMenuBRRESIntoHeap(holder, character, heap, fileExists)) return 1;
    if (fileExists) return 0;
    if (TryLoadLooseMiiCBRRESIntoHeap(holder, character, heap, fileExists)) return 1;
    if (fileExists) return 0;
    if (TryLoadCustomMenuBRRES(holder, character) || TryLoadLooseMiiCBRRES(holder, character)) return 1;
    return static_cast<MenuModelBRRESHandle *>(holder)->BindDriverBRRES(character);
}
kmCall(0x80830368, LoadMenuDriverBRRESHook);
kmCall(0x80831234, LoadMenuDriverBRRESHook);
kmCall(0x8083183c, LoadMenuDriverBRRESHook);

bool LoadMenuDriverBRRESForReload(void *holder, CharacterId character, EGG::ExpHeap *heap) {
    bool fileExists = false;
    if (TryLoadCustomMenuBRRESIntoHeap(holder, character, heap, fileExists)) return true;
    if (fileExists) return false;
    if (TryLoadLooseMiiCBRRESIntoHeap(holder, character, heap, fileExists)) return true;
    if (fileExists) return false;
    return static_cast<MenuModelBRRESHandle *>(holder)->BindDriverBRRES(character);
}

// Loose minimap icons are optional TPL files matched to the selected skin table.
TPLPalettePtr LoadLooseMinimapTPL(CharacterId character, u8 table) {
    static const u32 TPL_VERSION_NUMBER = 0x0020af30;
    if (table == TABLE_DEFAULT || table >= TABLE_COUNT || !IsCharacter(character)) return nullptr;
    RawTPL &cache = looseMinimapTPL[table][character];
    if (cache.failed) return nullptr;

    char path[0x60];
    const char *postfix = GeneratedCustomPostfix(character, table);
    const int written = postfix != nullptr ? snprintf(path, sizeof(path), "/Race/Map/%s.tpl", postfix) : -1;
    if (written <= 0 || static_cast<u32>(written) >= sizeof(path)) {
        cache.failed = true;
        return nullptr;
    }
    u32 fileSize = 0;
    if (!DiscFileSize(path, fileSize)) {
        cache.failed = true;
        return nullptr;
    }
    GameScene *scene = const_cast<GameScene *>(GameScene::GetCurrent());
    if (scene == nullptr) {
        cache.failed = true;
        return nullptr;
    }
    EGG::Heap *heaps[] = {scene->structsHeaps.heaps[0], scene->structsHeaps.heaps[1], scene->mainMEMHeap, scene->otherMEMHeap};
    EGG::Heap *heap = nullptr;
    for (u32 i = 0; i < ARRAY_COUNT(heaps); ++i) {
        if (heaps[i] == nullptr) continue;
        UnlockHeap(heaps[i]);
        if (heaps[i]->getAllocatableSize(0x20) >= fileSize + 0x1000) {
            heap = heaps[i];
            break;
        }
    }
    if (heap == nullptr) {
        cache.failed = true;
        return nullptr;
    }
    u32 loadedSize = 0;
    TPLPalettePtr palette = static_cast<TPLPalettePtr>(LoadFileToMainRAM(path, heap, EGG::DvdRipper::ALLOC_FROM_HEAD, &loadedSize));
    if (palette == nullptr || loadedSize == 0 || palette->versionNumber != TPL_VERSION_NUMBER) {
        cache.failed = true;
        return nullptr;
    }
    return palette;
}

void InitMinimapCharacterHook(CtrlRace2DMapCharacter *control) {
    const Racedata *racedata = Racedata::sInstance;
    if (control != nullptr && racedata != nullptr && control->playerId < racedata->racesScenario.playerCount) {
        const u8 playerId = control->playerId;
        const CharacterId character = racedata->racesScenario.players[playerId].characterId;

        if (Network::IsFriendRoomCPU(playerId)) {
            const char *pane = GetCharacterIconPaneName(character);
            if (pane != nullptr) {
                control->SetPicturePane("chara_0_0", pane);
                control->SetPicturePane("chara_shadow_0_0", pane);
                control->SetPicturePane("chara_shadow_0_1", pane);
            }
        }

        TPLPalettePtr tpl = LoadLooseMinimapTPL(character, RaceSkinTable(playerId, character));
        if (tpl != nullptr) {
            nw4r::lyt::Pane *panes[] = {control->charaPane, control->charaShadow0Pane, control->charaShadow1Pane};
            for (u32 i = 0; i < ARRAY_COUNT(panes); ++i) {
                if (panes[i] == nullptr) continue;
                nw4r::lyt::Material *material = panes[i]->GetMaterial();
                if (material != nullptr) material->GetTexMapAry()->ReplaceImage(tpl);
            }
        }
    }
    control->CtrlRaceBase::InitSelf();
}
kmCall(0x807eb22c, InitMinimapCharacterHook);

// Kart archive loading reads the temporarily swapped character name postfix.
ArchivesHolder *LoadKartArchiveHook(ArchiveMgr *archiveMgr, u8 playerId, KartId kart, CharacterId character, u32 color, u32 type,
                                    EGG::Heap *decompressedHeap, EGG::Heap *archiveHeap) {
    const char **entry = nullptr;
    if (IsCharacter(character)) entry = characterNames + character;
    const char *oldName = nullptr;
    if (entry != nullptr) oldName = *entry;
    if (entry != nullptr) {
        const char *name = GeneratedCustomPostfix(character, RaceSkinTable(playerId, character));
        *entry = name != nullptr ? name : GetDefaultCharacterPostfix(character);
    }
    ArchivesHolder *holder = archiveMgr->LoadKartArchive(playerId, kart, character, color, type, decompressedHeap, archiveHeap);
    if (entry != nullptr) *entry = oldName;
    return holder;
}
kmCall(0x805540f4, LoadKartArchiveHook);

ArchivesHolder *LoadBackupKartArchiveHook(ArchiveMgr *archiveMgr, u8 playerId, KartId kart, CharacterId character, u32 color, u32 type,
                                          EGG::Heap *decompressedHeap, EGG::Heap *archiveHeap) {
    const char **entry = nullptr;
    if (IsCharacter(character)) entry = characterNames + character;
    const char *oldName = nullptr;
    if (entry != nullptr) oldName = *entry;
    if (entry != nullptr) {
        const char *name = GeneratedCustomPostfix(character, RaceSkinTable(playerId, character));
        *entry = name != nullptr ? name : GetDefaultCharacterPostfix(character);
    }
    ArchivesHolder *holder = archiveMgr->LoadKartArchiveHolder2(playerId, kart, character, color, 0, decompressedHeap, archiveHeap);
    if (entry != nullptr) *entry = oldName;
    return holder;
}
kmCall(0x80554198, LoadBackupKartArchiveHook);

kmRuntimeUse(0x8056be20);
void SetModelColorsIfReady(void *starAnm, void *drawMdl) {
    if (drawMdl == nullptr) return;
    const u8 *model = static_cast<const u8 *>(drawMdl);
    if (*reinterpret_cast<void *const *>(model + 0x10) == nullptr) return;
    for (u32 i = 0; i < 2; ++i) {
        if (*reinterpret_cast<void *const *>(model + 0x14 + i * sizeof(void *)) == nullptr) return;
    }
    reinterpret_cast<void (*)(void *, void *)>(kmRuntimeAddr(0x8056be20))(starAnm, drawMdl);
}
kmCall(0x80592e24, SetModelColorsIfReady);
kmCall(0x80592e40, SetModelColorsIfReady);

struct MenuKartArchiveLoader {
    void *vtable;
    EGG::Heap *mountHeap;
    EGG::Heap *dumpHeap;
    u32 state;
    CharacterId character;
    u32 gamemode;
};

bool RequestLoadKartArchivesImmediate(ArchiveMgr *archiveMgr, u8 hudSlotId, CharacterId character, u32 gamemode) {
    if (archiveMgr == nullptr || hudSlotId >= LOCAL_PLAYER_COUNT) return false;
    MenuKartArchiveLoader *loader = reinterpret_cast<MenuKartArchiveLoader *>(&archiveMgr->allkartsModelsLoaders[hudSlotId]);
    if (loader->mountHeap == nullptr || loader->state == 1) return false;
    loader->character = character;
    loader->gamemode = gamemode;
    loader->state = 1;

    const char **entry = nullptr;
    if (IsCharacter(character)) entry = characterNames + character;
    const char *oldName = nullptr;
    if (entry != nullptr) oldName = *entry;
    if (entry != nullptr) {
        const char *postfix = GeneratedCustomPostfix(character, SelectedTable(character));
        if (postfix != nullptr) {
            char path[0x60];
            const int written = snprintf(path, sizeof(path), "/Scene/Model/Kart/%s-allkart%s.szs", postfix, gamemode == 2 ? "" : "_BT");
            u32 fileSize = 0;
            if (written <= 0 || static_cast<u32>(written) >= sizeof(path) || !DiscFileSize(path, fileSize)) postfix = nullptr;
        }
        if (postfix == nullptr) postfix = GetDefaultCharacterPostfix(character);
        if (postfix != nullptr) *entry = postfix;
    }
    ArchiveMgr::LoadKartArchiveAsync(hudSlotId);
    if (entry != nullptr) *entry = oldName;
    return true;
}
kmCall(0x805f5658, RequestLoadKartArchivesImmediate);
kmCall(0x805f5798, RequestLoadKartArchivesImmediate);
kmCall(0x805f592c, RequestLoadKartArchivesImmediate);

const char *GetMenuDriverBRRESNameHook(u32 character) {
    const CharacterId id = static_cast<CharacterId>(character);
    const u8 table = ResolveMenuTable(id);
    if (IsCharacter(id) && table < TABLE_COUNT && rawBRRES[table][id].failed) return ArchiveMgr::GetKartArchivePostfix(id);
    const char *name = DriverBRRESName(id, table);
    if (name != nullptr) return name;
    return ArchiveMgr::GetKartArchivePostfix(id);
}
kmCall(0x8081e4a0, GetMenuDriverBRRESNameHook);

void DetachListNodeIfPresent(nw4r::ut::List *list, void *target) {
    if (list == nullptr || target == nullptr) return;
    for (void *node = nw4r::ut::List_GetNext(list, nullptr); node != nullptr;) {
        void *next = nw4r::ut::List_GetNext(list, node);
        if (node == target) nw4r::ut::List_Remove(list, node);
        node = next;
    }
}

// Reloaded menu models must leave all scene lists before their heap is destroyed.
void DetachModelDirectorFromScnMgrs(ModelDirector *model) {
    if (model == nullptr) return;
    ScnMgr *const *mgrs = ScnMgr::sInstance;
    for (u32 i = 0; i < 2; ++i) {
        ScnMgr *mgr = mgrs[i];
        if (mgr == nullptr) continue;
        DetachListNodeIfPresent(&mgr->modelDirectors, model);
        DetachListNodeIfPresent(&mgr->screenSpecificModelDirectors, model);
        DetachListNodeIfPresent(&mgr->hardcodedMatNamesModelDirectors, model);
    }
}

void RemoveInitializedModelDirector(ModelDirector *model) {
    if (model == nullptr) return;
    if ((model->bitfield & 0x100000) != 0) model->ToggleVisible(false);
    DetachModelDirectorFromScnMgrs(model);
}

void DestroyModelDirector(ModelDirector *model) {
    if (model == nullptr) return;
    RemoveInitializedModelDirector(model);
    delete model;
}

void ForgetReloadedMenuDriverModelHeaps() {
    memset(reloadedMenuDriverModels, 0, sizeof(reloadedMenuDriverModels));
    reloadedMenuDriverModelOwner = nullptr;
}

void DestroyReloadedMenuDriverModel(u8 idx, ModelDirector **modelSlot) {
    if (idx >= MENU_DRIVER_MODEL_COUNT) return;
    ReloadedMenuDriverModel &reloaded = reloadedMenuDriverModels[idx];
    EGG::ExpHeap *&heap = reloaded.heap;
    ModelDirector *model = reloaded.model;
    if (model == nullptr && modelSlot != nullptr) model = *modelSlot;
    ToadetteHair *hair = reloaded.hair;

    if (heap == nullptr) {
        DestroyModelDirector(hair);
        DestroyModelDirector(model);
        if (modelSlot != nullptr && *modelSlot == model) *modelSlot = nullptr;
        reloaded.hair = nullptr;
        reloaded.model = nullptr;
        return;
    }

    if (hair != nullptr && IsInHeap(heap, hair)) DestroyModelDirector(hair);
    if (model != nullptr && IsInHeap(heap, model)) DestroyModelDirector(model);
    if (modelSlot != nullptr && *modelSlot == model) *modelSlot = nullptr;
    reloaded.hair = nullptr;
    reloaded.model = nullptr;
    DestroyHeap(heap);
}

void DestroyAllReloadedMenuDriverModels() {
    for (u8 i = 0; i < MENU_DRIVER_MODEL_COUNT; ++i) DestroyReloadedMenuDriverModel(i, nullptr);
}

void SyncReloadedMenuDriverModelHeaps(MenuDriverModel *models) {
    const GameScene *scene = GameScene::GetCurrent();
    if (reloadedMenuDriverModelSceneOwner != scene) {
        ForgetReloadedMenuDriverModelHeaps();
        reloadedMenuDriverModelSceneOwner = scene;
    }

    if (reloadedMenuDriverModelOwner != models) {
        if (reloadedMenuDriverModelOwner != nullptr) DestroyAllReloadedMenuDriverModels();
        ForgetReloadedMenuDriverModelHeaps();
        reloadedMenuDriverModelOwner = models;
    }
}

void CleanupReloadedMenuDriverModels() {
    DestroyAllReloadedMenuDriverModels();
    ForgetReloadedMenuDriverModelHeaps();
    reloadedMenuDriverModelSceneOwner = nullptr;
}

void DestroyMenuModelMgrInstanceHook() {
    MenuModelMgr *mgr = MenuModelMgr::sInstance;
    if (mgr == nullptr) return;
    CleanupReloadedMenuDriverModels();
    MenuModelMgr::sInstance = nullptr;

    const u32 vtable = *reinterpret_cast<u32 *>(reinterpret_cast<u8 *>(mgr) + 0x10);
    typedef void (*Dtor)(MenuModelMgr *, s32);
    reinterpret_cast<Dtor>(*reinterpret_cast<u32 *>(vtable + 0x8))(mgr, 1);
}
kmBranch(0x8059e04c, DestroyMenuModelMgrInstanceHook);

EGG::Allocator **MenuAllocatorSlot() { return &menuAllocator; }

EGG::Allocator *CreateScnObjAllocator(EGG::Heap *parent) {
    if (parent == nullptr) return nullptr;
    void *buf = operator new(0x1c, parent, 4);
    if (buf == nullptr) return nullptr;
    return new (buf) EGG::Allocator(parent, 0x20);
}

// Menu model reloads use a fresh heap so failed custom loads can be discarded.
EGG::ExpHeap *CreateMenuDriverModelHeap(GameScene &scene) {
    static const u32 MIN_HEAP_SIZE = 0x60000;
    static const u32 PARENT_RESERVE = 0x20000;
    EGG::Heap *parents[] = {scene.structsHeaps.heaps[0], scene.structsHeaps.heaps[1]};
    EGG::Heap *best = nullptr;
    u32 bestSize = 0;
    for (u32 i = 0; i < ARRAY_COUNT(parents); ++i) {
        EGG::Heap *parent = parents[i];
        if (parent == nullptr) continue;
        UnlockHeap(parent);
        const u32 freeSize = parent->getAllocatableSize(0x20);
        if (freeSize > bestSize) {
            best = parent;
            bestSize = freeSize;
        }
    }
    if (best == nullptr || bestSize <= MIN_HEAP_SIZE + PARENT_RESERVE) return nullptr;
    return EGG::ExpHeap::Create(static_cast<int>(bestSize - PARENT_RESERVE), best, 0);
}

void UnlockMenuModelHeaps(MenuModelMgr &modelMgr) {
    UnlockHeap(modelMgr.otherHeap);
    UnlockHeap(modelMgr.heap);

    GameScene *scene = const_cast<GameScene *>(GameScene::GetCurrent());
    if (scene == nullptr) return;
    for (u32 i = 0; i < ARRAY_COUNT(scene->structsHeaps.heaps); ++i) UnlockHeap(scene->structsHeaps.heaps[i]);
    UnlockHeap(scene->mainMEMHeap);
    UnlockHeap(scene->otherMEMHeap);
}

// These accessors name the vanilla MenuDriverModel fields used by reload hooks.
ModelDirector **MenuDriverModelDirectorSlot(MenuDriverModel *models, u8 idx) {
    static const u32 MENU_DRIVER_MODEL_SIZE = 0x28;
    static const u32 MENU_MODEL_DIRECTOR_OFFSET = 0x4;
    return reinterpret_cast<ModelDirector **>(reinterpret_cast<u8 *>(models) + idx * MENU_DRIVER_MODEL_SIZE + MENU_MODEL_DIRECTOR_OFFSET);
}

MenuDriverModel *MenuDriverModelSlot(MenuDriverModel *models, u8 idx) {
    static const u32 MENU_DRIVER_MODEL_SIZE = 0x28;
    return reinterpret_cast<MenuDriverModel *>(reinterpret_cast<u8 *>(models) + idx * MENU_DRIVER_MODEL_SIZE);
}

ModelTransformator **MenuDriverModelCharSelTransformatorSlot(MenuDriverModel &model) {
    static const u32 MENU_DRIVER_MODEL_CHAR_SEL_TRANSFORMATOR_OFFSET = 0xc;
    return reinterpret_cast<ModelTransformator **>(reinterpret_cast<u8 *>(&model) + MENU_DRIVER_MODEL_CHAR_SEL_TRANSFORMATOR_OFFSET);
}

ModelTransformator **MenuDriverModelOnKartTransformatorSlot(MenuDriverModel &model) {
    static const u32 MENU_DRIVER_MODEL_ON_KART_TRANSFORMATOR_OFFSET = 0x10;
    return reinterpret_cast<ModelTransformator **>(reinterpret_cast<u8 *>(&model) + MENU_DRIVER_MODEL_ON_KART_TRANSFORMATOR_OFFSET);
}

u32 &MenuDriverModelIdSlot(MenuDriverModel &model) {
    static const u32 MENU_DRIVER_MODEL_ID_OFFSET = 0x18;
    return *reinterpret_cast<u32 *>(reinterpret_cast<u8 *>(&model) + MENU_DRIVER_MODEL_ID_OFFSET);
}

KartId &MenuDriverModelVehicleSlot(MenuDriverModel &model) {
    static const u32 MENU_DRIVER_MODEL_VEHICLE_OFFSET = 0x1c;
    return *reinterpret_cast<KartId *>(reinterpret_cast<u8 *>(&model) + MENU_DRIVER_MODEL_VEHICLE_OFFSET);
}

void ConstructMenuModelBRRESHandle(void *handle) {
    new (handle) MenuModelBRRESHandle;
}

void DestroyMenuModelBRRESHandle(void *handle) {
    static_cast<MenuModelBRRESHandle *>(handle)->~MenuModelBRRESHandle();
}

bool LoadMenuDriverModel(void *handle, ModelDirector *model, CharacterId character) {
    return static_cast<MenuModelBRRESHandle *>(handle)->LoadDriverModel(*model, character);
}

ToadetteHair *LoadMenuDriverToadetteHair(void *handle, EGG::ExpHeap *heap, ModelDirector *model) {
    if (heap == nullptr || model == nullptr) return nullptr;
    return new (heap, 4) ToadetteHair(static_cast<MenuModelBRRESHandle *>(handle)->menuModelBRRES, model, 1);
}

bool IsLoadedMenuDriverModelReady(const ModelDirector *model) {
    return model != nullptr && (model->bitfield & 0x100000) != 0 && model->scnMdlEx[0] != nullptr &&
           model->scnMdlEx[0]->scnObj != nullptr && model->scnMdlEx[1] != nullptr && model->scnMdlEx[1]->scnObj != nullptr;
}

// Build a replacement menu model in an isolated heap and restore scene allocators.
bool LoadReloadedMenuDriverModel(GameScene &scene, ScnMgr &scnMgr, CharacterId character, ModelDirector *&newModel,
                                 ToadetteHair *&newHair, EGG::ExpHeap *&newHeap) {
    newModel = nullptr;
    newHair = nullptr;
    newHeap = nullptr;

    EGG::ExpHeap *modelHeap = CreateMenuDriverModelHeap(scene);
    if (modelHeap == nullptr) return false;

    u32 handle[2];
    ConstructMenuModelBRRESHandle(handle);
    const bool brresLoaded = LoadMenuDriverBRRESForReload(handle, character, modelHeap);
    if (!brresLoaded) {
        DestroyMenuModelBRRESHandle(handle);
        DestroyHeap(modelHeap);
        return false;
    }

    EGG::Allocator *freshAllocator = CreateScnObjAllocator(modelHeap);
    if (freshAllocator == nullptr) {
        DestroyMenuModelBRRESHandle(handle);
        DestroyHeap(modelHeap);
        return false;
    }

    EGG::Allocator **menuAllocSlot = MenuAllocatorSlot();
    EGG::Heap *savedHeap = scnMgr.curHeap;
    EGG::Allocator *savedAllocator = scnMgr.curAllocator;
    EGG::Allocator *savedMenuAllocator = *menuAllocSlot;
    scnMgr.curHeap = modelHeap;
    scnMgr.curAllocator = freshAllocator;
    *menuAllocSlot = freshAllocator;

    ModelDirector *model = new (modelHeap, 4) ModelDirector(2, 0);
    const bool loaded = model != nullptr && LoadMenuDriverModel(handle, model, character) && IsLoadedMenuDriverModelReady(model);
    ToadetteHair *hair = nullptr;
    bool hairLoaded = true;
    if (loaded && character == TOADETTE) {
        hair = LoadMenuDriverToadetteHair(handle, modelHeap, model);
        hairLoaded = IsLoadedMenuDriverModelReady(hair);
    }

    scnMgr.curHeap = savedHeap;
    scnMgr.curAllocator = savedAllocator;
    *menuAllocSlot = savedMenuAllocator;
    DestroyMenuModelBRRESHandle(handle);

    if (!loaded || !hairLoaded) {
        DestroyModelDirector(hair);
        DestroyModelDirector(model);
        DestroyHeap(modelHeap);
        return false;
    }

    UnlockHeap(modelHeap);
    modelHeap->adjust();
    newModel = model;
    newHair = hair;
    newHeap = modelHeap;
    return true;
}

bool LoadDefaultReloadedMenuDriverModel(GameScene &scene, ScnMgr &scnMgr, CharacterId character, ModelDirector *&newModel,
                                        ToadetteHair *&newHair, EGG::ExpHeap *&newHeap) {
    forceDefaultMenuDriverBRRES = true;
    const bool loaded = LoadReloadedMenuDriverModel(scene, scnMgr, character, newModel, newHair, newHeap);
    forceDefaultMenuDriverBRRES = false;
    return loaded;
}

void DestroyOldMenuDriverModelForReload(u8 idx, ModelDirector **modelSlot, ModelDirector *oldModel, ToadetteHair **hairSlot,
                                        ToadetteHair *oldHair) {
    if (reloadedMenuDriverModels[idx].heap != nullptr) {
        DestroyReloadedMenuDriverModel(idx, modelSlot);
        if (hairSlot != nullptr && *hairSlot == oldHair) *hairSlot = nullptr;
        return;
    }

    DestroyModelDirector(oldHair);
    DestroyModelDirector(oldModel);
    if (modelSlot != nullptr && *modelSlot == oldModel) *modelSlot = nullptr;
    if (hairSlot != nullptr && *hairSlot == oldHair) *hairSlot = nullptr;
}

void ResetReloadedMenuDriverModel(MenuDriverModel &menuModel, CharacterId character) {
    *MenuDriverModelCharSelTransformatorSlot(menuModel) = menuModel.model->modelTransformator;
    *MenuDriverModelOnKartTransformatorSlot(menuModel) = nullptr;
    MenuDriverModelIdSlot(menuModel) = static_cast<u32>(character);
}

// Replace one character-select model while preserving the surrounding menu manager.
bool ReloadMenuDriverModel(MenuDriverModelMgr &driverMgr, CharacterId character) {
    if (character < 0 || character >= MENU_DRIVER_MODEL_COUNT || driverMgr.models == nullptr) return false;
    const u8 idx = static_cast<u8>(character);
    MenuDriverModel *menuModel = MenuDriverModelSlot(driverMgr.models, idx);
    ModelDirector **modelSlot = MenuDriverModelDirectorSlot(driverMgr.models, idx);
    ToadetteHair **hairSlot = character == TOADETTE ? &driverMgr.bangs : static_cast<ToadetteHair **>(nullptr);

    GameScene *scene = const_cast<GameScene *>(GameScene::GetCurrent());
    if (scene == nullptr) return false;
    SyncReloadedMenuDriverModelHeaps(driverMgr.models);

    ScnMgr *const *mgrs = ScnMgr::sInstance;
    ScnMgr *scnMgr = mgrs[0];
    if (scnMgr == nullptr) return false;

    ModelDirector *oldModel = *modelSlot;
    ToadetteHair *oldHair = hairSlot != nullptr ? *hairSlot : static_cast<ToadetteHair *>(nullptr);

    ModelDirector *newModel = nullptr;
    ToadetteHair *newHair = nullptr;
    EGG::ExpHeap *newHeap = nullptr;
    if (!LoadReloadedMenuDriverModel(*scene, *scnMgr, character, newModel, newHair, newHeap)) {
        if (!LoadDefaultReloadedMenuDriverModel(*scene, *scnMgr, character, newModel, newHair, newHeap)) {
            return false;
        }
    }

    DestroyOldMenuDriverModelForReload(idx, modelSlot, oldModel, hairSlot, oldHair);

    *modelSlot = newModel;
    if (hairSlot != nullptr) *hairSlot = newHair;
    ReloadedMenuDriverModel &reloaded = reloadedMenuDriverModels[idx];
    reloaded.heap = newHeap;
    reloaded.model = newModel;
    reloaded.hair = newHair;
    ResetReloadedMenuDriverModel(*menuModel, character);
    menuModel->Init();
    return true;
}

// The scn roots are double buffered (ScnMgr::SwitchScnRoot flips them every
// draw): the root drawn this frame was already calculated last frame, before
// the reloaded model existed, and would display it in bind pose for one frame.
// Attach the animations and recalculate that root now, then attach them to the
// root the upcoming ScnMgr::CalcMain calculates. Only correct right before
// CalcMain, so this must stay on the queued reload path below.
void PoseReloadedMenuDriverModel(CharacterId character) {
    ReloadedMenuDriverModel &reloaded = reloadedMenuDriverModels[character];
    ModelDirector *model = reloaded.model;
    ToadetteHair *hair = reloaded.hair;
    ScnMgr *scnMgr = ScnMgr::sInstance[0];
    if (model == nullptr || scnMgr == nullptr) return;

    const u32 curRootIdx = scnMgr->curScnRootIdx;
    const u32 drawnRootIdx = curRootIdx ^ 1;
    model->Update(drawnRootIdx);
    if (hair != nullptr) hair->Update(drawnRootIdx);
    scnMgr->curScnRootIdx = drawnRootIdx;
    scnMgr->curScnRoot = scnMgr->scnRoots[drawnRootIdx];
    scnMgr->UpdateScnRoot(0);
    scnMgr->curScnRootIdx = curRootIdx;
    scnMgr->curScnRoot = scnMgr->scnRoots[curRootIdx];
    model->Update(curRootIdx);
    if (hair != nullptr) hair->Update(curRootIdx);
}

// In menus the section update runs after ScnMgr::CalcMain, so a model reloaded
// there could never be posed in time for its first draw. Skin changes are
// therefore only queued here and executed right before the next CalcMain.
bool pendingMenuDriverModelReloads[LOCAL_PLAYER_COUNT];
CharacterId pendingMenuDriverModelCharacters[LOCAL_PLAYER_COUNT];

void ReinitMenuDriverModelMgr(u8 hud, CharacterId character) {
    MenuModelMgr *modelMgr = MenuModelMgr::sInstance;
    if (modelMgr == nullptr || !modelMgr->isActive || modelMgr->driverModels == nullptr) return;
    if (hud >= LOCAL_PLAYER_COUNT || hud >= modelMgr->playerCount) return;

    UnlockMenuModelHeaps(*modelMgr);
    if (!ReloadMenuDriverModel(*modelMgr->driverModels, character)) return;
    PoseReloadedMenuDriverModel(character);
    modelMgr->driverModels->SetPlayerCharacter(hud, character);

    SectionMgr *sectionMgr = SectionMgr::sInstance;
    if (sectionMgr == nullptr || sectionMgr->curSection == nullptr) return;
    Pages::CharacterSelect *page = sectionMgr->curSection->Get<Pages::CharacterSelect>();
    if (page != nullptr && page->models != nullptr) page->models[hud].RequestModel(character);
}

// These are the three ScnMgr::CalcMain call sites of GameScene::calc, covering
// every scene calc path a menu can take.
void ProcessMenuDriverModelReloadsAndCalc() {
    for (u8 hud = 0; hud < LOCAL_PLAYER_COUNT; ++hud) {
        if (!pendingMenuDriverModelReloads[hud]) continue;
        pendingMenuDriverModelReloads[hud] = false;
        ReinitMenuDriverModelMgr(hud, pendingMenuDriverModelCharacters[hud]);
    }
    ScnMgr::CalcMain();
}
kmCall(0x8051b444, ProcessMenuDriverModelReloadsAndCalc);
kmCall(0x8051b4dc, ProcessMenuDriverModelReloadsAndCalc);
kmCall(0x8051b610, ProcessMenuDriverModelReloadsAndCalc);

void RefreshMenuDriverModel(CharacterId character) {
    MenuModelMgr *modelMgr = MenuModelMgr::sInstance;
    if (modelMgr == nullptr || !modelMgr->isActive || modelMgr->driverModels == nullptr) return;

    UnlockMenuModelHeaps(*modelMgr);
    ReloadMenuDriverModel(*modelMgr->driverModels, character);
}

KartId SelectedMenuKartForHud(u8 hud) {
    const SectionMgr *sectionMgr = SectionMgr::sInstance;
    if (sectionMgr != nullptr && sectionMgr->sectionParams != nullptr && hud < sectionMgr->sectionParams->localPlayerCount) {
        return sectionMgr->sectionParams->karts[hud];
    }
    const Racedata *racedata = Racedata::sInstance;
    if (racedata != nullptr && hud < racedata->menusScenario.localPlayerCount) {
        const u8 playerId = racedata->menusScenario.settings.hudPlayerIds[hud];
        if (playerId < ONLINE_PLAYER_COUNT) return racedata->menusScenario.players[playerId].kartId;
    }
    return STANDARD_KART_S;
}

// The random-vote message box shows drivers on the currently selected kart.
void ApplyVoteRandomMessageBoxKartState() {
    SectionMgr *sectionMgr = SectionMgr::sInstance;
    if (sectionMgr == nullptr || sectionMgr->curSection == nullptr) {
        voteRandomMessageBoxKartStateApplied = false;
        return;
    }
    const Page *topPage = sectionMgr->curSection->GetTopLayerPage();
    if (!IsVotingSection(sectionMgr->curSection->sectionId) || topPage == nullptr || topPage->pageId != PAGE_VOTERANDOM_MESSAGE_BOX) {
        voteRandomMessageBoxKartStateApplied = false;
        return;
    }
    if (voteRandomMessageBoxKartStateApplied) return;

    MenuModelMgr *modelMgr = MenuModelMgr::sInstance;
    if (modelMgr == nullptr || !modelMgr->isActive || modelMgr->driverModels == nullptr) return;
    const u8 count = SectionPlayerCount(sectionMgr);
    for (u8 hud = 0; hud < count && hud < modelMgr->playerCount; ++hud) {
        MenuDriverModel *model = modelMgr->driverModels->players[hud].playerModel;
        if (model == nullptr) continue;
        MenuDriverModelVehicleSlot(*model) = SelectedMenuKartForHud(hud);
        // PrepareDriverOnKartAnms saves the active transformator as the character-select transformator.
        model->SwitchState(hud, MenuDriverModel::MENUDRIVERMODEL_STATE_ONCHARSELECT);
        modelMgr->driverModels->PrepareDriverOnKartAnms(hud);
        model->SwitchState(hud, static_cast<MenuDriverModel::State>(3));
    }
    voteRandomMessageBoxKartStateApplied = true;
}

// Award models use separate animations and must always load from the vanilla BRRES.
const char *GetAwardDriverBRRESName(CharacterId character) {
    return GetDefaultCharacterPostfix(character);
}
kmCall(0x80789668, GetAwardDriverBRRESName);

}  // namespace CustomCharacters
}  // namespace Pulsar
