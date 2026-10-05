#include <CustomCharacters/CustomCharacters.hpp>
#include <IO/SDIO.hpp>
#include <core/rvl/os/OSCache.hpp>

namespace Pulsar {
namespace CustomCharacters {

enum { AUTHOR_NAME_CONTROL_WORDS = (sizeof(CharaName) + sizeof(u32) - 1) / sizeof(u32) };

static u32 authorNameControlStorage[LOCAL_PLAYER_COUNT][AUTHOR_NAME_CONTROL_WORDS];
static bool authorNameControlConstructed[LOCAL_PLAYER_COUNT];
static bool authorNameControlLoaded[LOCAL_PLAYER_COUNT];
static bool loadingAuthorNameControl;
static CharaName *authorTextControl;
static u32 authorTextValue;
static CharaName *characterNameTextControl[LOCAL_PLAYER_COUNT];
static u32 characterNameTextValue[LOCAL_PLAYER_COUNT];
static bool characterNameTextOverridden[LOCAL_PLAYER_COUNT];

// Tico models are placement-new constructed by the vanilla driver model path.
TicoModel *CreateTicoModelHook(void *memory, DriverController *controller) {
    if (memory == nullptr) return nullptr;
    const Racedata *racedata = Racedata::sInstance;
    const u8 playerId = controller->GetPlayerIdx();
    const CharacterId character = racedata->racesScenario.players[playerId].characterId;
    const LooseVoiceInfo &info = GetLooseVoiceInfo(character, RaceSkinTable(playerId, character));
    if (info.hasFiles || info.silent) return nullptr;
    return new (memory) TicoModel(controller);
}
kmCall(0x807c8994, CreateTicoModelHook);

// Character select previews use the hovered button until section params catch up.
CharacterId PreviewCharacter(u8 hud) {
    if (hud >= LOCAL_PLAYER_COUNT) return MARIO;
    const SectionMgr *mgr = SectionMgr::sInstance;
    CharacterId character = hoveredCharacters[hud];
    if (mgr != nullptr && mgr->sectionParams != nullptr && !IsCharacter(character)) character = mgr->sectionParams->characters[hud];
    return character;
}

// Unpack up to two advertised skin tables from a remote player's SELECT packet.
void UpdateOnlineCharacterTablesFromAid(u8 aid, const u8 *playerIdToAid, u16 characterTables) {
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
    const SectionMgr *mgr = SectionMgr::sInstance;
    if (mgr != nullptr && mgr->sectionParams != nullptr) {
        localCount = static_cast<u8>(mgr->sectionParams->localPlayerCount);
    } else if (Racedata::sInstance != nullptr) {
        localCount = Racedata::sInstance->menusScenario.localPlayerCount;
    }
    if (localCount > 2) localCount = 2;

    u16 packed = 0;
    for (u8 hud = 0; hud < localCount; ++hud) {
        CharacterId character = hoveredCharacters[hud];
        if (mgr != nullptr && mgr->sectionParams != nullptr && hud < mgr->sectionParams->localPlayerCount) {
            character = mgr->sectionParams->characters[hud];
        } else if (Racedata::sInstance != nullptr && hud < Racedata::sInstance->menusScenario.localPlayerCount) {
            const u8 playerId = Racedata::sInstance->menusScenario.settings.hudPlayerIds[hud];
            if (playerId < ONLINE_PLAYER_COUNT) character = Racedata::sInstance->menusScenario.players[playerId].characterId;
        }
        packed |= static_cast<u16>((SelectedTable(character) & PACKET_MASK) << (hud * PACKET_BITS));
    }
    return packed;
}

// Skin input only runs while the character select page is the active top layer.
bool IsCharacterSelectActive() {
    const SectionMgr *mgr = SectionMgr::sInstance;
    if (mgr == nullptr || mgr->curSection == nullptr) return false;
    const Pages::CharacterSelect *page = mgr->curSection->Get<Pages::CharacterSelect>();
    return page != nullptr && mgr->curSection->GetTopLayerPage() == page && page->currentState == STATE_ACTIVE && !page->updateState;
}

bool SetRaceNameTextIfCustom(LayoutUIControl &control, const char *paneName, u8 playerId) {
    const Racedata *racedata = Racedata::sInstance;
    if (racedata == nullptr || playerId >= racedata->racesScenario.playerCount || playerId >= ONLINE_PLAYER_COUNT) return false;
    const CharacterId character = racedata->racesScenario.players[playerId].characterId;
    if (IsMiiCharacter(character)) return false;
    const u32 bmgId = SkinNameBmgId(character, RaceSkinTable(playerId, character));
    if (bmgId == 0) return false;
    SetCustomCharacterNameMessage(control, paneName, bmgId);
    return true;
}

// Race name controls store playerId at 0x178 in the vanilla layout control.
void SetRaceCharacterNameHook(LayoutUIControl *control, const char *paneName, u32 bmgId, const Text::Info *info) {
    if (control == nullptr) return;
    static const u32 PLAYER_ID_OFFSET = 0x178;
    const u32 playerId = *reinterpret_cast<const u32 *>(reinterpret_cast<const u8 *>(control) + PLAYER_ID_OFFSET);
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

void FillRaceResultNameHook(CtrlRaceResult *result, u8 playerId) {
    const Racedata *racedata = Racedata::sInstance;
    SectionMgr *sectionMgr = SectionMgr::sInstance;
    if (result == nullptr || racedata == nullptr || sectionMgr == nullptr || sectionMgr->sectionParams == nullptr || playerId >= racedata->racesScenario.playerCount) {
        return;
    }
    const RacedataScenario &scenario = racedata->racesScenario;
    const RacedataPlayer &player = scenario.players[playerId];
    const bool useMiiName = IsMiiCharacter(player.characterId) ||
                            ((scenario.settings.gamemode >= MODE_PRIVATE_VS && scenario.settings.gamemode <= MODE_PRIVATE_BATTLE) ||
                             scenario.localPlayerCount > 1) &&
                                player.playerType != PLAYER_CPU;
    if (useMiiName) {
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
void LoadRaceResultHook(CtrlRaceResult *result) {
    reinterpret_cast<void (*)(CtrlRaceResult *)>(kmRuntimeAddr(0x807f4e68))(result);

    nw4r::lyt::TextBox *name = static_cast<nw4r::lyt::TextBox *>(result->layout.GetPaneByName("player_name"));
    if (name == nullptr) return;

    name->AllocStringBuffer(32);
    Text::PaneHandler *handler = result->layout.GetTextPaneHandlerByName("player_name");
    if (handler != nullptr) {
        handler->~PaneHandler();
        new (handler) Text::PaneHandler;
        handler->Init(name);
    }
}
kmWritePointer(0x808d3f24, LoadRaceResultHook);

void UpdateCharacterSelectText(u8 hud) {
    if (!IsCharacterSelectActive()) {
        authorTextControl = nullptr;
        authorTextValue = 0;
        memset(characterNameTextControl, 0, sizeof(characterNameTextControl));
        memset(characterNameTextValue, 0, sizeof(characterNameTextValue));
        memset(characterNameTextOverridden, 0, sizeof(characterNameTextOverridden));
        return;
    }
    SectionMgr *mgr = SectionMgr::sInstance;
    if (mgr == nullptr || mgr->curSection == nullptr || hud >= LOCAL_PLAYER_COUNT) return;
    Pages::CharacterSelect *page = mgr->curSection->Get<Pages::CharacterSelect>();
    if (page == nullptr || page->names == nullptr) return;

    const CharacterId character = PreviewCharacter(hud);
    CharaName &name = page->names[hud];
    const u32 nameBmgId = SkinNameBmgId(character, SelectedTable(character));
    if (characterNameTextControl[hud] != &name || characterNameTextValue[hud] != nameBmgId) {
        if (nameBmgId != 0) {
            SetCustomCharacterNameMessage(name, nameBmgId);
            characterNameTextOverridden[hud] = true;
        } else if (characterNameTextOverridden[hud] && !IsMiiCharacter(character)) {
            CharacterId displayCharacter = StateCharacter(character);
            if (!IsCharacter(displayCharacter)) displayCharacter = character;
            if (IsCharacter(displayCharacter) && !IsMiiCharacter(displayCharacter)) {
                name.SetMessage(GetCharacterBMGId(displayCharacter, false), nullptr);
            }
            characterNameTextOverridden[hud] = false;
        }
        characterNameTextControl[hud] = &name;
        characterNameTextValue[hud] = nameBmgId;
    }

    if (!authorNameControlLoaded[hud]) return;
    CharaName *author = reinterpret_cast<CharaName *>(&authorNameControlStorage[hud][0]);
    if (SectionPlayerCount(mgr) > 1) {
        author->isHidden = true;
        if (authorTextControl == author) {
            authorTextControl = nullptr;
            authorTextValue = 0;
        }
        return;
    }
    const u8 table = SelectedTable(character);
    const u32 authorBmgId = IsCharacter(character) && table != TABLE_DEFAULT && table <= CUSTOM_TABLE_LIMIT && HasSkin(character, table)
                                ? (static_cast<u32>(character) << 16) | CUSTOM_CHARACTER_AUTHOR_BMG_START | table
                                : 0;
    if (authorTextControl == author && authorTextValue == authorBmgId) return;
    author->isHidden = authorBmgId == 0 || !SetCustomCharacterAuthorMessage(*author, authorBmgId);
    authorTextControl = author;
    authorTextValue = authorBmgId;
}

// Author text reuses a CharaName control attached under the vanilla name control.
void CharacterSelectNameLoadHook(ControlLoader *loader, const char *folderName, const char *ctrName, const char *variant, const char **animNames) {
    loader->Load(folderName, ctrName, variant, animNames);
    if (loadingAuthorNameControl || loader == nullptr || loader->layoutUIControl == nullptr) return;
    CharaName &name = *static_cast<CharaName *>(loader->layoutUIControl);
    const u32 hud = name.unknown_0x178;
    if (hud >= LOCAL_PLAYER_COUNT) return;
    characterNameTextControl[hud] = nullptr;
    characterNameTextValue[hud] = 0;
    characterNameTextOverridden[hud] = false;

    CharaName *author = reinterpret_cast<CharaName *>(&authorNameControlStorage[hud][0]);
    if (authorNameControlConstructed[hud]) {
        authorNameControlLoaded[hud] = false;
        if (authorTextControl == author) {
            authorTextControl = nullptr;
            authorTextValue = 0;
        }
    }
    new (author) CharaName;
    authorNameControlConstructed[hud] = true;
    author->unknown_0x178 = hud;
    name.InitControlGroup(1);
    name.AddControl(0, author);
    loadingAuthorNameControl = true;
    ControlLoader authorLoader(author);
    authorLoader.Load(folderName, ctrName, variant, nullptr);
    loadingAuthorNameControl = false;

    const char *panes[] = {"Window_00", "black_parts_t_00", "black_parts_t_01", "select_base", "border", "cc_prev_wh", "cc_next_wh",
                           "cc_prev_nc", "cc_next_nc", "cc_prev_cls", "cc_next_cls", "cc_prev_gc", "cc_next_gc"};
    for (u32 i = 0; i < ARRAY_COUNT(panes); ++i) {
        if (author->layout.GetPaneByName(panes[i]) != nullptr) author->SetPaneVisibility(panes[i], false);
    }
    for (u32 i = 0; i < ARRAY_COUNT(author->positionAndscale); ++i) {
        author->positionAndscale[i].position = name.positionAndscale[i].position;
        author->positionAndscale[i].position.y -= 14.5f;
        author->positionAndscale[i].scale.x *= 1.1f;
    }
    author->isHidden = true;
    authorNameControlLoaded[hud] = true;
}
kmCall(0x8083d9dc, CharacterSelectNameLoadHook);

void CharacterSelectHoverHook(Pages::CharacterSelect *page, CtrlMenuCharacterSelect::ButtonDriver *button, u32 buttonId, u8 hud) {
    if (hud < LOCAL_PLAYER_COUNT) hoveredCharacters[hud] = static_cast<CharacterId>(buttonId);
    page->OnButtonDriverSelect(button, buttonId, hud);
    if (hud < LOCAL_PLAYER_COUNT) characterNameTextControl[hud] = nullptr;
    UpdateCharacterSelectText(hud);
}
kmCall(0x807e2cf0, CharacterSelectHoverHook);
kmCall(0x807e304c, CharacterSelectHoverHook);
kmCall(0x807e34d0, CharacterSelectHoverHook);
kmCall(0x807e37b0, CharacterSelectHoverHook);
kmCall(0x807e3a88, CharacterSelectHoverHook);

// Menu BRRES selection can be forced back to vanilla during voting restore.
u8 ResolveMenuTable(CharacterId character) {
    if (forceDefaultMenuDriverBRRES) return TABLE_DEFAULT;
    if (ShouldForceDefaultVotingMenuTable()) return TABLE_DEFAULT;
    return SelectedTable(character);
}

bool BuildDriverPath(CharacterId character, u8 table, char *path, u32 pathSize) {
    const char *name = DriverBRRESName(character, table);
    if (name == nullptr) return false;
    const int written = snprintf(path, pathSize, "/Scene/Model/Driver/%s.brres", name);
    return written > 0 && static_cast<u32>(written) < pathSize;
}

bool OpenChannelCharacterFile(SDIO &sd, const char *discPath, char *resolvedPath, u32 resolvedPathSize) {
    if (!IsNewChannel() || discPath == nullptr) return false;

    char path[0x80];
    while (*discPath == '/') ++discPath;
    const char *basename = discPath;
    for (const char *cursor = discPath; *cursor != '\0'; ++cursor) {
        if (*cursor == '/') basename = cursor + 1;
    }
    const bool patchesEnabled = Settings::Mgr::IsCreated() &&
                                Settings::Mgr::Get().GetSettingValue(Pulsar::Settings::SETTING_LOOSEARCHIVEOVERRIDES) ==
                                    LOOSEARCHIVEOVERRIDES_ENABLED;
    for (u32 i = 0; i < 6; ++i) {
        int written = -1;
        switch (i) {
            case 0:
                if (strncmp(discPath, "Scene/Model/Driver/", 19) == 0) written = snprintf(path, sizeof(path), "/RetroRewind6/Character/Driver/%s", discPath + 19);
                break;
            case 1:
                if (strncmp(discPath, "Race/Map/", 9) == 0) written = snprintf(path, sizeof(path), "/RetroRewind6/Character/Map/%s", discPath + 9);
                break;
            case 2:
                if (strncmp(discPath, "sound/", 6) == 0) written = snprintf(path, sizeof(path), "/RetroRewind6/Character/Sound/%s", discPath + 6);
                break;
            case 3:
                if (strncmp(discPath, "Scene/Model/Kart/", 17) == 0) written = snprintf(path, sizeof(path), "/RetroRewind6/Character/Allkart/%s", discPath + 17);
                break;
            case 4:
                if (patchesEnabled) written = snprintf(path, sizeof(path), "/RetroRewind6/Patches/%s", discPath);
                break;
            case 5:
                if (patchesEnabled) written = snprintf(path, sizeof(path), "/RetroRewind6/Patches/%s", basename);
                break;
        }
        if (written <= 0 || static_cast<u32>(written) >= sizeof(path)) continue;
        if (!sd.OpenFile(path, FILE_MODE_READ)) continue;
        if (resolvedPath != nullptr && resolvedPathSize > 0) snprintf(resolvedPath, resolvedPathSize, "%s", path);
        return true;
    }
    return false;
}

bool DiscFileSize(const char *path, u32 &size) {
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

void *LoadFileToMainRAM(const char *path, EGG::Heap *heap, EGG::DvdRipper::EAllocDirection allocDirection, u32 *outSize) {
    if (!IsNewChannel()) {
        return EGG::DvdRipper::LoadToMainRAM(path, nullptr, heap, allocDirection, 0, nullptr, outSize);
    }
    if (outSize != nullptr) *outSize = 0;
    SDIO sd(IOType_SD, nullptr, nullptr);
    if (!OpenChannelCharacterFile(sd, path, nullptr, 0)) return nullptr;
    const s32 signedFileSize = sd.GetFileSize();
    if (signedFileSize <= 0 || static_cast<u32>(signedFileSize) > 0x7fffffe0) {
        sd.Close();
        return nullptr;
    }
    const u32 fileSize = static_cast<u32>(signedFileSize);
    const u32 allocSize = (fileSize + 0x20) & ~0x1f;
    void *buffer = EGG::Heap::alloc(allocSize, allocDirection == EGG::DvdRipper::ALLOC_FROM_TAIL ? -0x20 : 0x20, heap);
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
    if (allocSize > fileSize) memset(static_cast<u8 *>(buffer) + fileSize, 0, allocSize - fileSize);
    OS::DCStoreRange(buffer, allocSize);
    if (outSize != nullptr) *outSize = fileSize;
    return buffer;
}

}  // namespace CustomCharacters
}  // namespace Pulsar
