#include <Dolphin/DolphinIOS.hpp>
#include <UI/UI.hpp>
#include <MarioKartWii/Race/RaceInfo/RaceInfo.hpp>
#include <SlotExpansion/UI/ExpansionUIMisc.hpp>
#include <SlotExpansion/CupsConfig.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <MarioKartWii/RKSYS/RKSYSMgr.hpp>
#include <MarioKartWii/Scene/GameScene.hpp>
#include <core/rvl/DWC/DWCAccount.hpp>
#include <Network/Rating/PlayerRating.hpp>
#include <CustomCharacters/CustomCharacters.hpp>
#include <MarioKartWii/Archive/ArchiveMgr.hpp>

namespace Discord {

static bool hasWrittenClientID = false;
static int frameCount = 0;
static u64 startTimeStamp = 0;
SectionId prevSectionId = SECTION_NONE;
static CharacterId charID = CHARACTER_NONE;

static char smallImageKey[256] = "";
static char smallImageText[64] = "";

static void SetSmallImage(const char *key, const char *text) {
    snprintf(smallImageKey, sizeof(smallImageKey), "%s", key);
    snprintf(smallImageText, sizeof(smallImageText), "%s", text);
}

void CleanBMGMessage(wchar_t *dest, const wchar_t *src);
void ConvertUTF16toUtf8(char *dest, const wchar_t *src, size_t max_len);

struct CustomSkinDiscordImage {
    CharacterId character;
    u8 table;
    const char *key;
};

// Developer mapping table for custom skin Discord images.
// Arranged in Character Selection Screen order (Light -> Medium -> Heavy -> Miis).
// Add entries here if you upload custom skin icons to the Discord Developer Portal.
// Example: { BABY_PEACH, 2, "custom_peach_skin" },
// If not specified here (or in BMG), presence cleanly falls back to the base character's image.
static const CustomSkinDiscordImage customSkinDiscordImages[] = {
    // --- Light ---
    // BABY_MARIO
    // BABY_LUIGI
    // TOAD
    // TOADETTE
    // BABY_PEACH
    // BABY_DAISY
    // KOOPA_TROOPA
    // DRY_BONES

    // --- Medium ---
    // MARIO
    // LUIGI
    // YOSHI
    // BIRDO
    // PEACH
    // DAISY
    // DIDDY_KONG
    // BOWSER_JR

    // --- Heavy ---
    // WARIO
    // WALUIGI
    // KING_BOO
    // ROSALINA
    // DONKEY_KONG
    // FUNKY_KONG
    // BOWSER
    // DRY_BOWSER

    // --- Miis ---
    // MII_A
    // MII_B
    // MII_C
    { CHARACTER_NONE, 0, nullptr }
};

// Returns default Discord asset key and display name in Character Selection Screen order.
static void GetDefaultCharacterInfo(CharacterId character, const char *&outKey, const char *&outName) {
    outKey = "";
    outName = "";
    switch (character) {
        // --- Light ---
        case BABY_MARIO:
            outKey = "bmario";
            outName = "Baby Mario";
            break;
        case BABY_LUIGI:
            outKey = "bluigi";
            outName = "Baby Luigi";
            break;
        case TOAD:
            outKey = "toad";
            outName = "Toad";
            break;
        case TOADETTE:
            outKey = "toadette";
            outName = "Toadette";
            break;
        case BABY_PEACH:
            outKey = "bpeach";
            outName = "Baby Peach";
            break;
        case BABY_DAISY:
            outKey = "bdaisy";
            outName = "Baby Daisy";
            break;
        case KOOPA_TROOPA:
            outKey = "koopa_troopa";
            outName = "Koopa Troopa";
            break;
        case DRY_BONES:
            outKey = "dry_bones";
            outName = "Dry Bones";
            break;

        // --- Medium ---
        case MARIO:
            outKey = "mario";
            outName = "Mario";
            break;
        case LUIGI:
            outKey = "luigi";
            outName = "Luigi";
            break;
        case YOSHI:
            outKey = "yoshi";
            outName = "Yoshi";
            break;
        case BIRDO:
            outKey = "birdo";
            outName = "Birdo";
            break;
        case PEACH:
        case PEACH_BIKER:
            outKey = "peach";
            outName = "Peach";
            break;
        case DAISY:
        case DAISY_BIKER:
            outKey = "daisy";
            outName = "Daisy";
            break;
        case DIDDY_KONG:
            outKey = "diddy";
            outName = "Diddy Kong";
            break;
        case BOWSER_JR:
            outKey = "bowser_jr";
            outName = "Bowser Jr";
            break;

        // --- Heavy ---
        case WARIO:
            outKey = "wario";
            outName = "Wario";
            break;
        case WALUIGI:
            outKey = "waluigi";
            outName = "Waluigi";
            break;
        case KING_BOO:
            outKey = "king_boo";
            outName = "King Boo";
            break;
        case ROSALINA:
        case ROSALINA_BIKER:
            outKey = "rosalina";
            outName = "Rosalina";
            break;
        case DONKEY_KONG:
            outKey = "dk";
            outName = "Donkey Kong";
            break;
        case FUNKY_KONG:
            outKey = "funky";
            outName = "Funky Kong";
            break;
        case BOWSER:
            outKey = "bowser";
            outName = "Bowser";
            break;
        case DRY_BOWSER:
            outKey = "dry_bowser";
            outName = "Dry Bowser";
            break;

        // --- Miis ---
        case MII_L_A_MALE:
        case MII_L_A_FEMALE:
        case MII_M_A_MALE:
        case MII_M_A_FEMALE:
        case MII_S_A_MALE:
        case MII_S_A_FEMALE:
            outKey = "mii_a";
            outName = "Mii (Outfit A)";
            break;
        case MII_L_B_MALE:
        case MII_L_B_FEMALE:
        case MII_M_B_MALE:
        case MII_M_B_FEMALE:
        case MII_S_B_MALE:
        case MII_S_B_FEMALE:
            outKey = "mii_b";
            outName = "Mii (Outfit B)";
            break;
        case MII_L_C_MALE:
        case MII_L_C_FEMALE:
        case MII_M_C_MALE:
        case MII_M_C_FEMALE:
        case MII_S_C_MALE:
        case MII_S_C_FEMALE:
            outKey = "mii_c";
            outName = "Mii (Outfit C)";
            break;
        default:
            break;
    }
}

static const wchar_t *ResolveSkinBmgMessage(u32 bmgId) {
    if (bmgId == 0) return nullptr;

    const wchar_t *msg = Pulsar::UI::GetCustomMsg(static_cast<s32>(bmgId));
    if (msg != nullptr && msg[0] != L'\0') {
        return msg;
    }

    const SectionMgr *sectionMgr = SectionMgr::sInstance;
    if (sectionMgr != nullptr && sectionMgr->systemBMG != nullptr) {
        const s32 msgId = sectionMgr->systemBMG->GetMsgId(static_cast<s32>(bmgId));
        if (msgId >= 0) {
            msg = sectionMgr->systemBMG->GetMsgByMsgId(msgId);
            if (msg != nullptr && msg[0] != L'\0') return msg;
        }
    }

    ArchiveMgr *archiveMgr = ArchiveMgr::sInstance;
    if (archiveMgr != nullptr && archiveMgr->archivesHolders[ARCHIVE_HOLDER_UI] != nullptr) {
        ArchivesHolder *uiHolder = archiveMgr->archivesHolders[ARCHIVE_HOLDER_UI];
        for (int i = static_cast<int>(uiHolder->archiveCount) - 1; i >= 0; --i) {
            void *file = uiHolder->archives[i].GetFile("message/Common.bmg", nullptr);
            if (file != nullptr) {
                BMGHolder holder;
                holder.Init(*reinterpret_cast<const BMGHeader *>(file));
                const s32 msgId = holder.GetMsgId(static_cast<s32>(bmgId));
                if (msgId >= 0) {
                    msg = holder.GetMsgByMsgId(msgId);
                    if (msg != nullptr && msg[0] != L'\0') return msg;
                }
            }
        }
    }

    return nullptr;
}

static const char *GetSkinDiscordImageKey(CharacterId character, u8 table, const char *defaultKey) {
    if (table == Pulsar::CustomCharacters::TABLE_DEFAULT) {
        return defaultKey;
    }

    const CharacterId stateChar = Pulsar::CustomCharacters::StateCharacter(character);

    // Check developer mapping table
    for (u32 i = 0; customSkinDiscordImages[i].character != CHARACTER_NONE; ++i) {
        if ((customSkinDiscordImages[i].character == character ||
             customSkinDiscordImages[i].character == stateChar) &&
            customSkinDiscordImages[i].table == table) {
            if (customSkinDiscordImages[i].key != nullptr && customSkinDiscordImages[i].key[0] != '\0') {
                return customSkinDiscordImages[i].key;
            }
        }
    }

    // Check if an asset key string is defined via BMG (0x8a00 | table)
    static char bmgImageKey[256];
    const u32 imageBmgId = (static_cast<u32>(character) << 16) | Pulsar::CustomCharacters::CUSTOM_CHARACTER_IMAGE_BMG_START | table;
    const wchar_t *imageMsg = ResolveSkinBmgMessage(imageBmgId);
    if (imageMsg != nullptr && imageMsg[0] != L'\0') {
        wchar_t cleanKey[0x100];
        CleanBMGMessage(cleanKey, imageMsg);
        ConvertUTF16toUtf8(bmgImageKey, cleanKey, sizeof(bmgImageKey));
        if (bmgImageKey[0] != '\0') {
            return bmgImageKey;
        }
    }

    // Fallback to default character image key
    return defaultKey;
}

static void SetCharacterSmallImage(CharacterId character, u8 table = Pulsar::CustomCharacters::TABLE_DEFAULT) {
    const char *defaultKey = "";
    const char *defaultName = "";
    GetDefaultCharacterInfo(character, defaultKey, defaultName);
    if (defaultKey[0] == '\0') {
        return;
    }

    const char *finalKey = defaultKey;
    const char *finalName = defaultName;
    char customNameUtf8[64];

    if (table != Pulsar::CustomCharacters::TABLE_DEFAULT) {
        finalKey = GetSkinDiscordImageKey(character, table, defaultKey);

        const u32 bmgId = Pulsar::CustomCharacters::SkinNameBmgId(character, table);
        if (bmgId != 0) {
            const wchar_t *msg = ResolveSkinBmgMessage(bmgId);
            if (msg != nullptr && msg[0] != L'\0') {
                wchar_t cleanName[0x100];
                CleanBMGMessage(cleanName, msg);
                ConvertUTF16toUtf8(customNameUtf8, cleanName, sizeof(customNameUtf8));
                if (customNameUtf8[0] != '\0') {
                    finalName = customNameUtf8;
                }
            }
        }
    }

    SetSmallImage(finalKey, finalName);
}

// Removes 00 1A escapes from the BMG text
void CleanBMGMessage(wchar_t *dest, const wchar_t *src) {
    int inc = 0;
    for (int i = 0; i < 0x100 && src[i] && inc + 1 < 0x100; i++) {
        if (src[i] == 0x001a) {
            u8 size = *(u8 *)(&src[i + 1]);
            i += (size / 2) - 1;
        } else {
            dest[inc] = src[i];
            inc++;
        }
    }
    dest[inc] = '\0';
}

void ConvertUTF16toUtf8(char *dest, const wchar_t *src, size_t max_len) {
    if (max_len == 0) return;

    size_t destIndex = 0;
    for (size_t i = 0;; i++) {
        wchar_t c = src[i];
        if (c == 0) {
            break;
        }
        if (c <= 0x007F) {
            if (destIndex + 1 >= max_len) break;
            dest[destIndex++] = (char)c;
        } else if (c <= 0x07FF) {
            if (destIndex + 2 >= max_len) break;
            dest[destIndex++] = 0xC0 | ((c >> 6) & 0x1F);
            dest[destIndex++] = 0x80 | (c & 0x3F);
        } else {
            if (destIndex + 3 >= max_len) break;
            dest[destIndex++] = 0xE0 | ((c >> 12) & 0x0F);
            dest[destIndex++] = 0x80 | ((c >> 6) & 0x3F);
            dest[destIndex++] = 0x80 | (c & 0x3F);
        }
    }
    dest[destIndex] = '\0';
}

static bool GetFirstLocalRacePlayer(u8 &outPlayerId, CharacterId &outCharacterId) {
    const GameScene *scene = GameScene::GetCurrent();
    Racedata *raceData = Racedata::sInstance;
    Raceinfo *raceInfo = Raceinfo::sInstance;
    if (scene == nullptr || scene->id != SCENE_ID_RACE || raceData == nullptr || raceInfo == nullptr ||
        !raceInfo->IsAtLeastStage(RACESTAGE_INTRO)) {
        return false;
    }

    const RacedataScenario &scenario = raceData->racesScenario;
    if (scenario.localPlayerCount > 0) {
        const u8 playerId = scenario.settings.hudPlayerIds[0];
        if (playerId < scenario.playerCount && scenario.players[playerId].playerType == PLAYER_REAL_LOCAL) {
            outPlayerId = playerId;
            outCharacterId = scenario.players[playerId].characterId;
            return true;
        }
    }

    for (u32 i = 0; i < scenario.playerCount; ++i) {
        if (scenario.players[i].playerType == PLAYER_REAL_LOCAL) {
            outPlayerId = static_cast<u8>(i);
            outCharacterId = scenario.players[i].characterId;
            return true;
        }
    }
    return false;
}

static CharacterId GetFirstLocalRaceCharacter() {
    u8 playerId = 0;
    CharacterId characterId = CHARACTER_NONE;
    if (GetFirstLocalRacePlayer(playerId, characterId)) {
        return characterId;
    }
    return CHARACTER_NONE;
}

void DiscordRichPresence(Section *_this) {
    _this->Update();
    if (!Dolphin::IsEmulator()) {
        return;
    }

    if (_this->sectionId == prevSectionId && ((frameCount++ % 900) != 0)) {
        return;
    }

    if (!hasWrittenClientID) {
        Dolphin::SetDiscordClient("1471316950004006963");
        hasWrittenClientID = true;
    }

    char *state = "";
    char *details = "In a Menu";
    char *largeImageText = "";
    int minPlayers = 0;
    int maxPlayers = 0;

    RKSYS::Mgr *rksysMgr = RKSYS::Mgr::sInstance;
    float vr = 0, br = 0;
    u64 fc = 0;

    smallImageKey[0] = '\0';
    smallImageText[0] = '\0';

    if (rksysMgr && rksysMgr->curLicenseId >= 0) {
        RKSYS::LicenseMgr &license = rksysMgr->licenses[rksysMgr->curLicenseId];
        vr = Pulsar::PointRating::GetUserVR(rksysMgr->curLicenseId);
        br = Pulsar::PointRating::GetUserBR(rksysMgr->curLicenseId);
        fc = DWC::CreateFriendKey(&license.dwcAccUserData);
    }

    if (fc) {
        u32 fcParts[3];
        for (int j = 0; j < 3; ++j) {
            fcParts[j] = fc % 10000;
            fc /= 10000;
        }

        char fcText[32];
        snprintf(fcText, 32, "Friend Code: %04u-%04u-%04u", fcParts[2], fcParts[1], fcParts[0]);
        largeImageText = fcText;
    }

    u8 localPlayerId = 0;
    CharacterId localCharId = CHARACTER_NONE;
    if (GetFirstLocalRacePlayer(localPlayerId, localCharId)) {
        charID = localCharId;
        const u8 table = Pulsar::CustomCharacters::RaceSkinTable(localPlayerId, localCharId);
        SetCharacterSmallImage(localCharId, table);
    } else {
        charID = CHARACTER_NONE;
    }

    if (_this->sectionId != prevSectionId) {
        Dolphin::GetSystemTime(startTimeStamp);
        prevSectionId = _this->sectionId;
    }

    wchar_t trackNameW[0x100];
    char trackName[0x100];

    memset(trackNameW, 0, 0x100);

    u32 bmgId = Pulsar::UI::GetCurTrackBMG();
    const wchar_t *msg = Pulsar::UI::GetCustomMsg(bmgId);
    if (msg && Raceinfo::sInstance && Raceinfo::sInstance->IsAtLeastStage(RACESTAGE_INTRO)) {
        CleanBMGMessage(trackNameW, msg);
        ConvertUTF16toUtf8(trackName, trackNameW, 32);
        state = trackName;
    }

    RKNet::Controller *controller = RKNet::Controller::sInstance;
    if (controller) {
        RKNet::ControllerSub &sub = controller->subs[controller->currentSub];
        maxPlayers = 12;
        minPlayers = sub.playerCount;
    }

    prevSectionId = _this->sectionId;
    switch (_this->sectionId) {
        case SECTION_GP:
            details = "In a Grand Prix";
            break;
        case SECTION_TT:
            details = "In Time Trials";
            break;
        case SECTION_P1VS:
            details = "In a 1P VS";
            break;
        case SECTION_P2VS:
            details = "In a 2P VS";
            break;
        case SECTION_P3VS:
            details = "In a 3P VS";
            break;
        case SECTION_P4VS:
            details = "In a 4P VS";
            break;
        case SECTION_P1TEAM_VS:
            details = "In a 1P Team VS";
            break;
        case SECTION_P2TEAM_VS:
            details = "In a 2P Team VS";
            break;
        case SECTION_P3TEAM_VS:
            details = "In a 3P Team VS";
            break;
        case SECTION_P4TEAM_VS:
            details = "In a 4P Team VS";
            break;
        case SECTION_P1BATTLE:
            details = "In a 1P Battle";
            break;
        case SECTION_P2BATTLE:
            details = "In a 2P Battle";
            break;
        case SECTION_P3BATTLE:
            details = "In a 3P Battle";
            break;
        case SECTION_P4BATTLE:
            details = "In a 4P Battle";
            break;
        case SECTION_MISSION_MODE:
            details = "In Mission Mode";
            break;
        case SECTION_TOURNAMENT:
            details = "In a Tournament";
            break;
        case SECTION_GP_REPLAY:
            details = "Watching a GP Replay";
            break;
        case SECTION_TT_REPLAY:
        case SECTION_WATCH_GHOST_FROM_CHANNEL:
        case SECTION_WATCH_GHOST_FROM_DOWNLOADS:
        case SECTION_WATCH_GHOST_FROM_MENU:
            details = "Watching a TT Replay";
            break;
        case SECTION_P1_WIFI:
        case SECTION_P1_WIFI_FROM_FROOM_RACE:
        case SECTION_P1_WIFI_FROM_FIND_FRIEND:
        case SECTION_P2_WIFI:
        case SECTION_P2_WIFI_FROM_FROOM_RACE:
        case SECTION_P2_WIFI_FROM_FIND_FRIEND:
            details = "In a WiFi menu";
            break;
        case SECTION_P1_WIFI_VS_VOTING:
        case SECTION_P2_WIFI_VS_VOTING:
            details = "Voting for a WiFi VS";
            break;
        case SECTION_P1_WIFI_BATTLE_VOTING:
        case SECTION_P2_WIFI_BATTLE_VOTING:
            details = "Voting for a WiFi Battle";
            break;
        case SECTION_P1_WIFI_FROOM_VS_VOTING:
        case SECTION_P2_WIFI_FROOM_VS_VOTING:
            details = "Voting for a VS in a froom";
            break;
        case SECTION_P1_WIFI_FROOM_TEAMVS_VOTING:
        case SECTION_P2_WIFI_FROOM_TEAMVS_VOTING:
            details = "Voting for a Team VS in a froom";
            break;
        case SECTION_P1_WIFI_FROOM_BALLOON_VOTING:
        case SECTION_P2_WIFI_FROOM_BALLOON_VOTING:
            details = "Voting for a Balloon Battle in a froom";
            break;
        case SECTION_P1_WIFI_FROOM_COIN_VOTING:
        case SECTION_P2_WIFI_FROOM_COIN_VOTING:
            details = "Voting for a Coin Runners in a froom";
            break;
        case SECTION_P1_WIFI_VS:
        case SECTION_P2_WIFI_VS:
            details = "Racing in a WiFi VS";
            break;
        case SECTION_P1_WIFI_BT:
        case SECTION_P2_WIFI_BT:
            details = "Racing in a WiFi Battle";
            break;
        case SECTION_P1_WIFI_FRIEND_VS:
        case SECTION_P2_WIFI_FRIEND_VS:
            details = "Racing in a WiFi Friend VS";
            break;
        case SECTION_P1_WIFI_FRIEND_TEAMVS:
        case SECTION_P2_WIFI_FRIEND_TEAMVS:
            details = "Racing in a WiFi Friend Team VS";
            break;
        case SECTION_P1_WIFI_FRIEND_BALLOON:
        case SECTION_P2_WIFI_FRIEND_BALLOON:
            details = "Racing in a WiFi Friend Balloon Battle";
            break;
        case SECTION_P1_WIFI_FRIEND_COIN:
        case SECTION_P2_WIFI_FRIEND_COIN:
            details = "Racing in a WiFi Friend Coin Runners";
            break;
        case SECTION_P1_WIFI_VS_LIVEVIEW:
        case SECTION_P2_WIFI_VS_LIVEVIEW:
            details = "Spectating a WiFi VS";
            break;
        case SECTION_P1_WIFI_BT_LIVEVIEW:
        case SECTION_P2_WIFI_BT_LIVEVIEW:
            details = "Spectating a WiFi Battle";
            break;
        default:
            state = "";
            break;
    }

    if (_this->sectionId >= SECTION_P1_WIFI && _this->sectionId <= SECTION_P2_WIFI_FRIEND_COIN) {
        char newDetails[0x100];
        int vrScaled = (int)(vr * 100.0f + 0.5f);
        int brScaled = (int)(br * 100.0f + 0.5f);
        snprintf(newDetails, 0x100, "%s (VR: %d BR: %d)", details, vrScaled, brScaled);
        details = newDetails;
    }

    Dolphin::SetDiscordPresence(
        details,
        state,
        "image_logo",
        largeImageText,
        smallImageKey,
        smallImageText,
        startTimeStamp,
        0,
        minPlayers,
        maxPlayers);
}

kmCall(0x80635540, DiscordRichPresence);

}  // namespace Discord
