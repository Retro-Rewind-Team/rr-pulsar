#include <CustomCharacters/CustomCharacters.hpp>
#include <IO/SDIO.hpp>

namespace Pulsar {
namespace CustomCharacters {

// Tico models are placement-new constructed by the vanilla driver model path.
TicoModel* CreateTicoModelHook(void* memory, DriverController* controller) {
    if (memory == nullptr) return nullptr;
    const Racedata* racedata = Racedata::sInstance;
    const u8 playerId = controller->GetPlayerIdx();
    const CharacterId character = racedata->racesScenario.players[playerId].characterId;
    const LooseVoiceInfo& info = GetLooseVoiceInfo(character, RaceSkinTable(playerId, character));
    if (info.hasFiles || info.silent) return nullptr;
    return new (memory) TicoModel(controller);
}
kmCall(0x807c8994, CreateTicoModelHook);

// Temporarily swap the vanilla postfix so archive loads find the selected skin.
const char** BeginNameSwap(u8 playerId, CharacterId character, const char*& oldName) {
    const char** entry = CharacterNameEntry(character);
    oldName = nullptr;
    if (entry == nullptr) return nullptr;
    oldName = *entry;
    const char* name = GeneratedCustomPostfix(character, RaceSkinTable(playerId, character));
    if (name == nullptr) name = GetDefaultCharacterPostfix(character);
    *entry = name;
    return entry;
}

// Character select previews use the hovered button until section params catch up.
CharacterId PreviewCharacter(u8 hud) {
    if (hud >= LOCAL_PLAYER_COUNT) return MARIO;
    const SectionMgr* mgr = SectionMgr::sInstance;
    CharacterId character = hoveredCharacters[hud];
    if (mgr != nullptr && mgr->sectionParams != nullptr && !IsCharacter(character)) character = mgr->sectionParams->characters[hud];
    return character;
}

CharacterId SelectedCharacterForHud(u8 hud) {
    if (hud >= LOCAL_PLAYER_COUNT) return MARIO;
    const SectionMgr* mgr = SectionMgr::sInstance;
    if (mgr != nullptr && mgr->sectionParams != nullptr && hud < mgr->sectionParams->localPlayerCount) {
        return mgr->sectionParams->characters[hud];
    }
    const Racedata* racedata = Racedata::sInstance;
    if (racedata != nullptr && hud < racedata->menusScenario.localPlayerCount) {
        const u8 playerId = racedata->menusScenario.settings.hudPlayerIds[hud];
        if (playerId < ONLINE_PLAYER_COUNT) return racedata->menusScenario.players[playerId].characterId;
    }
    return hoveredCharacters[hud];
}

// Unpack up to two advertised skin tables from a remote player's SELECT packet.
void UpdateOnlineCharacterTablesFromAid(u8 aid, const u8* playerIdToAid, u16 characterTables) {
    if (playerIdToAid == nullptr) return;
    if (IsLocalMultiplayer()) {
        ResetOnlineCustomCharacterFlags();
        return;
    }
    u8 hud = 0;
    for (u8 playerId = 0; playerId < ONLINE_PLAYER_COUNT; ++playerId) {
        if (playerIdToAid[playerId] != aid) continue;
        const u8 table = hud < 2 ? static_cast<u8>((characterTables >> (hud * PACKET_BITS)) & PACKET_MASK) : TABLE_DEFAULT;
        onlineCharacterTables[playerId] = table < TABLE_COUNT ? table : TABLE_DEFAULT;
        ++hud;
    }
}

// Pack local selected skin tables into the SELECT packet extension field.
u16 GetLocalOnlineCharacterTables() {
    if (IsLocalMultiplayer()) return 0;
    u8 localCount = GetLocalPlayerCount();
    const SectionMgr* mgr = SectionMgr::sInstance;
    if (mgr != nullptr && mgr->sectionParams != nullptr) {
        localCount = static_cast<u8>(mgr->sectionParams->localPlayerCount);
    } else if (Racedata::sInstance != nullptr) {
        localCount = Racedata::sInstance->menusScenario.localPlayerCount;
    }
    if (localCount > 2) localCount = 2;

    u16 packed = 0;
    for (u8 hud = 0; hud < localCount; ++hud) {
        packed |= static_cast<u16>((SelectedTable(SelectedCharacterForHud(hud)) & PACKET_MASK) << (hud * PACKET_BITS));
    }
    return packed;
}

bool ShouldUseCustomCharacterForPlayer(u8 playerId) {
    return !IsLocalMultiplayer() && playerId < ONLINE_PLAYER_COUNT && onlineCharacterTables[playerId] != TABLE_DEFAULT;
}

// Skin input only runs while the character select page is the active top layer.
bool IsCharacterSelectActive() {
    const SectionMgr* mgr = SectionMgr::sInstance;
    if (mgr == nullptr || mgr->curSection == nullptr) return false;
    const Pages::CharacterSelect* page = mgr->curSection->Get<Pages::CharacterSelect>();
    return page != nullptr && mgr->curSection->GetTopLayerPage() == page && page->currentState == STATE_ACTIVE && !page->updateState;
}

void CacheHoveredFromSection() {
    const SectionMgr* mgr = SectionMgr::sInstance;
    if (mgr == nullptr || mgr->sectionParams == nullptr) return;
    const u8 count = SectionPlayerCount(mgr);
    for (u8 hud = 0; hud < count; ++hud) hoveredCharacters[hud] = mgr->sectionParams->characters[hud];
}

u32 PreviewAuthorBmgId(u8 hud) {
    if (hud >= LOCAL_PLAYER_COUNT) return 0;
    const CharacterId character = PreviewCharacter(hud);
    return SkinAuthorBmgId(character, SelectedTable(character));
}

u32 PreviewNameBmgId(u8 hud) {
    if (hud >= LOCAL_PLAYER_COUNT) return 0;
    const CharacterId character = PreviewCharacter(hud);
    return SkinNameBmgId(character, SelectedTable(character));
}

bool IsOnlineRaceMode(GameMode mode) {
    return mode >= MODE_PRIVATE_VS && mode <= MODE_PRIVATE_BATTLE;
}

u32 RaceNameBmgId(u8 playerId) {
    const Racedata* racedata = Racedata::sInstance;
    if (racedata == nullptr || playerId >= racedata->racesScenario.playerCount || playerId >= ONLINE_PLAYER_COUNT) return 0;
    const CharacterId character = racedata->racesScenario.players[playerId].characterId;
    if (IsMiiCharacter(character)) return 0;
    return SkinNameBmgId(character, RaceSkinTable(playerId, character));
}

bool SetRaceNameTextIfCustom(LayoutUIControl& control, const char* paneName, u8 playerId) {
    const u32 bmgId = RaceNameBmgId(playerId);
    if (bmgId == 0) return false;
    return SetCustomCharacterNameMessage(control, paneName, bmgId);
}

// Race name controls store playerId at 0x178 in the vanilla layout control.
void SetRaceCharacterNameHook(LayoutUIControl* control, const char* paneName, u32 bmgId, const Text::Info* info) {
    if (control == nullptr) return;
    static const u32 PLAYER_ID_OFFSET = 0x178;
    const u32 playerId = *reinterpret_cast<const u32*>(reinterpret_cast<const u8*>(control) + PLAYER_ID_OFFSET);
    if (playerId >= ONLINE_PLAYER_COUNT) {
        control->SetTextBoxMessage(paneName, bmgId, info);
        return;
    }
    if (!SetRaceNameTextIfCustom(*control, paneName, static_cast<u8>(playerId))) {
        control->SetTextBoxMessage(paneName, bmgId, info);
    }
}
kmCall(0x807f0580, SetRaceCharacterNameHook);
kmCall(0x807f06b0, SetRaceCharacterNameHook);

bool RaceResultUsesMiiName(const RacedataScenario& scenario, u8 playerId) {
    const RacedataPlayer& player = scenario.players[playerId];
    if (IsMiiCharacter(player.characterId)) return true;
    return (IsOnlineRaceMode(scenario.settings.gamemode) || scenario.localPlayerCount > 1) && player.playerType != PLAYER_CPU;
}

void FillRaceResultNameHook(CtrlRaceResult* result, u8 playerId) {
    const Racedata* racedata = Racedata::sInstance;
    SectionMgr* sectionMgr = SectionMgr::sInstance;
    if (result == nullptr || racedata == nullptr || sectionMgr == nullptr || sectionMgr->sectionParams == nullptr || playerId >= racedata->racesScenario.playerCount) {
        return;
    }
    const RacedataScenario& scenario = racedata->racesScenario;
    const RacedataPlayer& player = scenario.players[playerId];
    if (RaceResultUsesMiiName(scenario, playerId)) {
        Text::Info info;
        info.miis[0] = sectionMgr->sectionParams->playerMiis.GetMii(playerId);
        result->SetTextBoxMessage("player_name", UI::BMG_MII_NAME, &info);
    } else if (!SetRaceNameTextIfCustom(*result, "player_name", playerId)) {
        result->SetTextBoxMessage("player_name", GetCharacterBMGId(player.characterId, true), nullptr);
    }
    result->ResetTextBoxMessage("time");
}
kmBranch(0x807f52f4, FillRaceResultNameHook);

kmRuntimeUse(0x807f4e68);
// Custom race result names need a larger text buffer than the vanilla control.
void LoadRaceResultHook(CtrlRaceResult* result) {
    reinterpret_cast<void (*)(CtrlRaceResult*)>(kmRuntimeAddr(0x807f4e68))(result);

    nw4r::lyt::TextBox* name = static_cast<nw4r::lyt::TextBox*>(result->layout.GetPaneByName("player_name"));
    if (name == nullptr) return;

    name->AllocStringBuffer(32);
    Text::PaneHandler* handler = result->layout.GetTextPaneHandlerByName("player_name");
    if (handler != nullptr) {
        handler->~PaneHandler();
        new (handler) Text::PaneHandler;
        handler->Init(name);
    }
}
kmWritePointer(0x808d3f24, LoadRaceResultHook);

void ResetCharacterSelectNameTextCache() {
    for (u8 hud = 0; hud < LOCAL_PLAYER_COUNT; ++hud) {
        characterNameTextControl[hud] = nullptr;
        characterNameTextValue[hud] = 0;
        characterNameTextOverridden[hud] = false;
    }
}

void RestoreCharacterSelectNameText(CharaName& name, CharacterId character) {
    if (IsMiiCharacter(character)) return;
    CharacterId displayCharacter = StateCharacter(character);
    if (!IsCharacter(displayCharacter)) displayCharacter = character;
    if (!IsCharacter(displayCharacter) || IsMiiCharacter(displayCharacter)) return;
    name.SetMessage(GetCharacterBMGId(displayCharacter, false), nullptr);
}

void UpdateCharacterSelectNameText(Pages::CharacterSelect* page, u8 hud) {
    if (page == nullptr || page->names == nullptr || hud >= LOCAL_PLAYER_COUNT) return;
    CharaName& name = page->names[hud];
    const u32 bmgId = PreviewNameBmgId(hud);
    if (characterNameTextControl[hud] == &name && characterNameTextValue[hud] == bmgId) return;
    if (bmgId != 0) {
        SetCustomCharacterNameMessage(name, bmgId);
        characterNameTextOverridden[hud] = true;
    } else if (characterNameTextOverridden[hud]) {
        RestoreCharacterSelectNameText(name, PreviewCharacter(hud));
        characterNameTextOverridden[hud] = false;
    }
    characterNameTextControl[hud] = &name;
    characterNameTextValue[hud] = bmgId;
}

CharaName* GetAuthorNameControl(u8 hud) {
    if (hud >= LOCAL_PLAYER_COUNT || !authorNameControlLoaded[hud]) return nullptr;
    return reinterpret_cast<CharaName*>(&authorNameControlStorage[hud][0]);
}

bool ShouldHideCharacterSelectAuthorText() {
    return SectionPlayerCount(SectionMgr::sInstance) > 1;
}

void UpdateCharacterSelectAuthorText(Pages::CharacterSelect* page, u8 hud) {
    if (page == nullptr || page->names == nullptr) return;
    CharaName* authorControl = GetAuthorNameControl(hud);
    if (authorControl == nullptr) return;
    if (ShouldHideCharacterSelectAuthorText()) {
        authorControl->isHidden = true;
        if (authorTextControl == authorControl) {
            authorTextControl = nullptr;
            authorTextValue = 0;
        }
        return;
    }
    const u32 bmgId = PreviewAuthorBmgId(hud);
    if (authorTextControl == authorControl && authorTextValue == bmgId) return;
    if (bmgId == 0) {
        authorControl->isHidden = true;
    } else {
        authorControl->isHidden = !SetCustomCharacterAuthorMessage(*authorControl, bmgId);
    }
    authorTextControl = authorControl;
    authorTextValue = bmgId;
}

void UpdateCurrentCharacterSelectAuthorText(u8 hud) {
    if (!IsCharacterSelectActive()) {
        authorTextControl = nullptr;
        authorTextValue = 0;
        ResetCharacterSelectNameTextCache();
        return;
    }
    SectionMgr* mgr = SectionMgr::sInstance;
    if (mgr == nullptr || mgr->curSection == nullptr) return;
    UpdateCharacterSelectNameText(mgr->curSection->Get<Pages::CharacterSelect>(), hud);
    UpdateCharacterSelectAuthorText(mgr->curSection->Get<Pages::CharacterSelect>(), hud);
}

void SetPaneVisibleIfPresent(LayoutUIControl& control, const char* paneName, bool visible) {
    if (control.layout.GetPaneByName(paneName) != nullptr) control.SetPaneVisibility(paneName, visible);
}

void HideAuthorNameDecoration(LayoutUIControl& control) {
    const char* panes[] = {"Window_00",
                           "black_parts_t_00",
                           "black_parts_t_01",
                           "select_base",
                           "border",
                           "cc_prev_wh",
                           "cc_next_wh",
                           "cc_prev_nc",
                           "cc_next_nc",
                           "cc_prev_cls",
                           "cc_next_cls",
                           "cc_prev_gc",
                           "cc_next_gc"};
    for (u32 i = 0; i < ARRAY_COUNT(panes); ++i) SetPaneVisibleIfPresent(control, panes[i], false);
}

void PositionAuthorNameControl(LayoutUIControl& control, const LayoutUIControl& characterNameControl) {
    for (u32 i = 0; i < ARRAY_COUNT(control.positionAndscale); ++i) {
        control.positionAndscale[i].position = characterNameControl.positionAndscale[i].position;
        control.positionAndscale[i].position.y -= 14.5f;
        control.positionAndscale[i].scale.x *= 1.1f;
    }
}

CharaName* ConstructCharaName(CharaName* name) {
    return new (name) CharaName;
}

// Author text reuses a CharaName control attached under the vanilla name control.
void AttachAuthorNameControl(CharaName& name, const char* folderName, const char* ctrName, const char* variant) {
    const u32 hud = name.unknown_0x178;
    if (hud >= LOCAL_PLAYER_COUNT) return;
    characterNameTextControl[hud] = nullptr;
    characterNameTextValue[hud] = 0;
    characterNameTextOverridden[hud] = false;

    CharaName* author = reinterpret_cast<CharaName*>(&authorNameControlStorage[hud][0]);
    if (authorNameControlConstructed[hud]) {
        authorNameControlLoaded[hud] = false;
        if (authorTextControl == author) {
            authorTextControl = nullptr;
            authorTextValue = 0;
        }
    }
    ConstructCharaName(author);
    authorNameControlConstructed[hud] = true;
    author->unknown_0x178 = hud;
    name.InitControlGroup(1);
    name.AddControl(0, author);
    loadingAuthorNameControl = true;
    ControlLoader loader(author);
    loader.Load(folderName, ctrName, variant, nullptr);
    loadingAuthorNameControl = false;

    HideAuthorNameDecoration(*author);
    PositionAuthorNameControl(*author, name);
    author->isHidden = true;
    authorNameControlLoaded[hud] = true;
}

void CharacterSelectNameLoadHook(ControlLoader* loader, const char* folderName, const char* ctrName, const char* variant, const char** animNames) {
    loader->Load(folderName, ctrName, variant, animNames);
    if (loadingAuthorNameControl || loader == nullptr || loader->layoutUIControl == nullptr) return;
    AttachAuthorNameControl(*static_cast<CharaName*>(loader->layoutUIControl), folderName, ctrName, variant);
}
kmCall(0x8083d9dc, CharacterSelectNameLoadHook);

// Some vanilla heaps are marked no-alloc after setup; loose assets reopen them.
void UnlockHeap(EGG::Heap* heap) {
    if (heap != nullptr) heap->dameFlag &= ~0x1;
}

// Heap ownership checks prevent scene lists from keeping freed model pointers.
bool IsInHeap(const EGG::ExpHeap* heap, const void* ptr) {
    if (heap == nullptr || ptr == nullptr || heap->rvlHeap == nullptr) return false;
    const u8* start = reinterpret_cast<const u8*>(heap->rvlHeap->startAddr);
    const u8* end = reinterpret_cast<const u8*>(heap->rvlHeap->endAddr);
    const u8* address = reinterpret_cast<const u8*>(ptr);
    return start != nullptr && end != nullptr && start < end && address >= start && address < end;
}

void DetachHeapListNodes(nw4r::ut::List* list, const EGG::ExpHeap* heap) {
    for (void* node = nw4r::ut::List_GetNext(list, nullptr); node != nullptr;) {
        void* next = nw4r::ut::List_GetNext(list, node);
        if (IsInHeap(heap, node)) nw4r::ut::List_Remove(list, node);
        node = next;
    }
}

void DetachHeapFromScnMgrs(const EGG::ExpHeap* heap) {
    if (heap == nullptr) return;
    ScnMgr* const* mgrs = ScnMgr::sInstance;
    for (u32 i = 0; i < 2; ++i) {
        ScnMgr* mgr = mgrs[i];
        if (mgr == nullptr) continue;
        DetachHeapListNodes(&mgr->modelDirectors, heap);
        DetachHeapListNodes(&mgr->screenSpecificModelDirectors, heap);
        DetachHeapListNodes(&mgr->scnGroupExHolderList, heap);
        DetachHeapListNodes(&mgr->hardcodedMatNamesModelDirectors, heap);
    }
}

// Destroy loose heaps only after detaching every scene-list node they own.
void DestroyHeap(EGG::ExpHeap*& heap) {
    if (heap == nullptr) return;
    DetachHeapFromScnMgrs(heap);
    UnlockHeap(heap);
    heap->destroy();
    heap = nullptr;
}

bool IsGameplaySectionLoading() {
    const SectionMgr* mgr = SectionMgr::sInstance;
    if (mgr == nullptr) return false;
    if (mgr->curSection != nullptr && IsGameplaySection(mgr->curSection->sectionId)) return true;
    return mgr->nextSectionId != SECTION_NONE && IsGameplaySection(mgr->nextSectionId);
}

void ClearRawCache(RawBRRES& cache, bool destroyHeap) {
    if (destroyHeap)
        DestroyHeap(cache.heap);
    else
        cache.heap = nullptr;
    cache.file = nullptr;
    cache.failed = false;
    cache.bound = false;
}

// Gameplay section transitions may still reference models from the current scene.
bool ClearRawCaches(bool destroyHeap) {
    if (destroyHeap && IsGameplaySectionLoading()) return false;
    for (u32 table = 0; table < TABLE_COUNT; ++table) {
        for (u32 character = 0; character < CHARACTER_COUNT; ++character) ClearRawCache(rawBRRES[table][character], destroyHeap);
    }
    for (u32 i = 0; i < MII_C_COUNT; ++i) ClearRawCache(looseMiiCBRRES[i], destroyHeap);
    return true;
}

void SyncRawCachesToCurrentScene() {
    const GameScene* const scene = GameScene::GetCurrent();
    if (rawCacheSceneOwner == scene) return;
    if (ClearRawCaches(true)) {
        rawCacheSceneOwner = scene;
    }
}

// Menu BRRES selection can be forced back to vanilla during voting restore.
u8 ResolveMenuTable(CharacterId character) {
    if (forceDefaultMenuDriverBRRES) return TABLE_DEFAULT;
    if (ShouldForceDefaultVotingMenuTable()) return TABLE_DEFAULT;
    return SelectedTable(character);
}

u32 AlignUp(u32 value, u32 alignment) {
    return (value + alignment - 1) & ~(alignment - 1);
}

bool BuildDriverPath(CharacterId character, u8 table, char* path, u32 pathSize) {
    const char* name = DriverBRRESName(character, table);
    if (name == nullptr) return false;
    const int written = snprintf(path, pathSize, "/Scene/Model/Driver/%s.brres", name);
    return written > 0 && static_cast<u32>(written) < pathSize;
}

const char* PathBasename(const char* path) {
    if (path == nullptr) return nullptr;
    const char* basename = path;
    for (const char* cursor = path; *cursor != '\0'; ++cursor) {
        if (*cursor == '/') basename = cursor + 1;
    }
    return basename;
}

bool ShouldUsePatchCharacterFiles() {
    if (!Settings::Mgr::IsCreated()) return false;
    return Settings::Mgr::Get().GetUserSettingValue(Settings::SETTINGSTYPE_MISC, RADIO_LOOSEARCHIVEOVERRIDES) ==
           LOOSEARCHIVEOVERRIDES_ENABLED;
}

bool BuildChannelSdPath(const char* discPath, u32 candidate, char* outPath, u32 outSize) {
    if (discPath == nullptr || outPath == nullptr || outSize == 0) return false;
    while (*discPath == '/') ++discPath;
    const char* basename = PathBasename(discPath);

    int written = -1;
    switch (candidate) {
        case 0:
            if (strncmp(discPath, "Scene/Model/Driver/", 19) == 0) {
                written = snprintf(outPath, outSize, "/RetroRewind6/Character/Driver/%s", discPath + 19);
            }
            break;
        case 1:
            if (strncmp(discPath, "Race/Map/", 9) == 0) {
                written = snprintf(outPath, outSize, "/RetroRewind6/Character/Map/%s", discPath + 9);
            }
            break;
        case 2:
            if (strncmp(discPath, "sound/", 6) == 0) {
                written = snprintf(outPath, outSize, "/RetroRewind6/Character/Sound/%s", discPath + 6);
            }
            break;
        case 3:
            if (strncmp(discPath, "Scene/Model/Kart/", 17) == 0) {
                written = snprintf(outPath, outSize, "/RetroRewind6/Character/Allkart/%s", discPath + 17);
            }
            break;
        case 4:
            if (!ShouldUsePatchCharacterFiles()) return false;
            written = snprintf(outPath, outSize, "/RetroRewind6/Patches/%s", discPath);
            break;
        case 5:
            if (!ShouldUsePatchCharacterFiles()) return false;
            if (basename != nullptr) written = snprintf(outPath, outSize, "/RetroRewind6/Patches/%s", basename);
            break;
        default:
            return false;
    }
    return written > 0 && static_cast<u32>(written) < outSize;
}

bool OpenChannelCharacterFile(SDIO& sd, const char* discPath, char* resolvedPath, u32 resolvedPathSize) {
    if (!IsNewChannel()) return false;

    char path[0x80];
    for (u32 i = 0; i < 6; ++i) {
        if (!BuildChannelSdPath(discPath, i, path, sizeof(path))) continue;
        if (!sd.OpenFile(path, FILE_MODE_READ)) continue;
        if (resolvedPath != nullptr && resolvedPathSize > 0) snprintf(resolvedPath, resolvedPathSize, "%s", path);
        return true;
    }
    return false;
}

bool DiscFileSize(const char* path, u32& size) {
    if (!IsNewChannel()) {
        DVD::FileInfo info;
        if (!DVD::Open(path, &info)) {
            size = 0;
            return false;
        }
        size = info.length;
        DVD::Close(&info);
        return size != 0;
    }

    SDIO sd(IOType_SD, nullptr, nullptr);
    if (!OpenChannelCharacterFile(sd, path, nullptr, 0)) {
        size = 0;
        return false;
    }
    const s32 fileSize = sd.GetFileSize();
    sd.Close();
    if (fileSize <= 0) {
        size = 0;
        return false;
    }
    size = static_cast<u32>(fileSize);
    return size != 0;
}

void* LoadChannelFileToMainRAM(const char* path, EGG::Heap* heap, EGG::DvdRipper::EAllocDirection allocDirection, u32* outSize) {
    if (outSize != nullptr) *outSize = 0;
    SDIO sd(IOType_SD, nullptr, nullptr);
    char resolvedPath[0x80];
    if (!OpenChannelCharacterFile(sd, path, resolvedPath, sizeof(resolvedPath))) return nullptr;

    const s32 signedFileSize = sd.GetFileSize();
    if (signedFileSize <= 0 || static_cast<u32>(signedFileSize) > 0x7fffffe0) {
        sd.Close();
        return nullptr;
    }
    const u32 fileSize = static_cast<u32>(signedFileSize);
    if (outSize != nullptr) *outSize = fileSize;

    const u32 allocSize = AlignUp(fileSize + 1, 0x20);
    void* buffer = EGG::Heap::alloc(allocSize, allocDirection == EGG::DvdRipper::ALLOC_FROM_TAIL ? -0x20 : 0x20, heap);
    if (buffer == nullptr) {
        sd.Close();
        return nullptr;
    }

    const s32 read = sd.Read(fileSize, buffer);
    sd.Close();
    if (read != static_cast<s32>(fileSize)) {
        EGG::Heap::free(buffer, heap);
        return nullptr;
    }
    if (allocSize > fileSize) memset(static_cast<u8*>(buffer) + fileSize, 0, allocSize - fileSize);
    return buffer;
}

void* LoadFileToMainRAM(const char* path, EGG::Heap* heap, EGG::DvdRipper::EAllocDirection allocDirection, u32* outSize) {
    if (!IsNewChannel()) {
        return EGG::DvdRipper::LoadToMainRAM(path, nullptr, heap, allocDirection, 0, nullptr, outSize);
    }
    return LoadChannelFileToMainRAM(path, heap, allocDirection, outSize);
}

}  // namespace CustomCharacters
}  // namespace Pulsar
