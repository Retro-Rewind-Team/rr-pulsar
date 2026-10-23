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
#include <Race/CustomCharacters.hpp>
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

// Icons exported from Retro Rewind 6.13.4; skins without distinct icons use the existing fallback.
// Source paths and pack contributor credits: https://github.com/DarthMDev/rr-character-icons
// Thanks to nerfko for the original hosted URL list that inspired these mappings.
// Arranged in Character Selection Screen order (Light -> Medium -> Heavy).
#define DISCORD_SKIN_IMAGE(file) "https://raw.githubusercontent.com/DarthMDev/rr-character-icons/627eb8314a6e0e50b278462fb65e2af20c2e0f58/icons/" file

static const CustomSkinDiscordImage customSkinDiscordImages[] = {
    // BABY_MARIO
    {BABY_MARIO, 1, DISCORD_SKIN_IMAGE("bmr-1.png")},  // Baby Wario
    {BABY_MARIO, 3, DISCORD_SKIN_IMAGE("bmr-3.png")},  // Coin Coffer
    {BABY_MARIO, 4, DISCORD_SKIN_IMAGE("bmr-4.png")},  // Shy Guy
    {BABY_MARIO, 5, DISCORD_SKIN_IMAGE("bmr-5.png")},  // Paper Mario
    {BABY_MARIO, 6, DISCORD_SKIN_IMAGE("bmr-6.png")},  // Cheep Cheep
    // BABY_LUIGI
    {BABY_LUIGI, 1, DISCORD_SKIN_IMAGE("blg-1.png")},  // Baby Waluigi
    {BABY_LUIGI, 2, DISCORD_SKIN_IMAGE("blg-2.png")},  // Professor E. Gadd
    {BABY_LUIGI, 3, DISCORD_SKIN_IMAGE("blg-3.png")},  // Spike
    {BABY_LUIGI, 4, DISCORD_SKIN_IMAGE("blg-4.png")},  // Fuzzy
    {BABY_LUIGI, 5, DISCORD_SKIN_IMAGE("blg-5.png")},  // Virus Trio
    // TOAD
    {TOAD, 1, DISCORD_SKIN_IMAGE("ko-1.png")},  // Captain Toad
    {TOAD, 3, DISCORD_SKIN_IMAGE("ko-3.png")},  // Swoop
    {TOAD, 4, DISCORD_SKIN_IMAGE("ko-4.png")},  // Toadsworth
    {TOAD, 5, DISCORD_SKIN_IMAGE("ko-5.png")},  // Tuxie
    // TOADETTE
    {TOADETTE, 3, DISCORD_SKIN_IMAGE("kk-3.png")},  // Blooper
    {TOADETTE, 4, DISCORD_SKIN_IMAGE("kk-4.png")},  // Para-Biddybud
    // BABY_PEACH
    {BABY_PEACH, 2, DISCORD_SKIN_IMAGE("bpc-2.png")},  // Pikachu
    {BABY_PEACH, 3, DISCORD_SKIN_IMAGE("bpc-3.png")},  // Lemmy
    {BABY_PEACH, 4, DISCORD_SKIN_IMAGE("bpc-4.png")},  // Kirby
    // BABY_DAISY
    {BABY_DAISY, 2, DISCORD_SKIN_IMAGE("bds-2.png")},  // Rocky Wrench
    {BABY_DAISY, 3, DISCORD_SKIN_IMAGE("bds-3.png")},  // Connie
    {BABY_DAISY, 4, DISCORD_SKIN_IMAGE("bds-4.png")},  // Talking Flower
    // KOOPA_TROOPA
    {KOOPA_TROOPA, 1, DISCORD_SKIN_IMAGE("nk-1.png")},  // Koopa Paratroopa
    {KOOPA_TROOPA, 2, DISCORD_SKIN_IMAGE("nk-2.png")},  // Lakitu
    {KOOPA_TROOPA, 5, DISCORD_SKIN_IMAGE("nk-5.png")},  // Sidestepper
    // DRY_BONES
    {DRY_BONES, 3, DISCORD_SKIN_IMAGE("ka-3.png")},  // Goomba
    {DRY_BONES, 4, DISCORD_SKIN_IMAGE("ka-4.png")},  // Peepa
    // MARIO
    {MARIO, 3, DISCORD_SKIN_IMAGE("mr-3.png")},  // Snowman
    {MARIO, 4, DISCORD_SKIN_IMAGE("mr-4.png")},  // Mario (SM64)
    {MARIO, 5, DISCORD_SKIN_IMAGE("mr-5.png")},  // Mario (SNES)
    // LUIGI
    {LUIGI, 3, DISCORD_SKIN_IMAGE("lg-3.png")},  // Monty Mole
    {LUIGI, 4, DISCORD_SKIN_IMAGE("lg-4.png")},  // Sonic
    {LUIGI, 5, DISCORD_SKIN_IMAGE("lg-5.png")},  // Don-Chan
    // YOSHI
    {YOSHI, 3, DISCORD_SKIN_IMAGE("ys-3.png")},  // Piranha Plant
    // BIRDO
    {BIRDO, 2, DISCORD_SKIN_IMAGE("ca-2.png")},  // Mega Man
    {BIRDO, 3, DISCORD_SKIN_IMAGE("ca-3.png")},  // Dolphin
    {BIRDO, 4, DISCORD_SKIN_IMAGE("ca-4.png")},  // Shadow
    // PEACH
    {PEACH, 5, DISCORD_SKIN_IMAGE("pc-5.png")},  // Shantae
    // DAISY
    {DAISY, 3, DISCORD_SKIN_IMAGE("ds-3.png")},  // Daisy (Swimwear)
    {DAISY, 4, DISCORD_SKIN_IMAGE("ds-4.png")},  // Ludwig
    {DAISY, 5, DISCORD_SKIN_IMAGE("ds-5.png")},  // Bill Board
    // DIDDY_KONG
    {DIDDY_KONG, 2, DISCORD_SKIN_IMAGE("dd-2.png")},  // Dixie Kong
    {DIDDY_KONG, 3, DISCORD_SKIN_IMAGE("dd-3.png")},  // The Chimp
    {DIDDY_KONG, 4, DISCORD_SKIN_IMAGE("dd-4.png")},  // Iggy
    {DIDDY_KONG, 5, DISCORD_SKIN_IMAGE("dd-5.png")},  // Fox McCloud
    // BOWSER_JR
    {BOWSER_JR, 2, DISCORD_SKIN_IMAGE("jr-2.png")},  // Hammer Bro
    {BOWSER_JR, 3, DISCORD_SKIN_IMAGE("jr-3.png")},  // Kamek
    {BOWSER_JR, 5, DISCORD_SKIN_IMAGE("jr-5.png")},  // Nabbit
    {BOWSER_JR, 6, DISCORD_SKIN_IMAGE("jr-6.png")},  // Ant Trooper
    // WARIO
    {WARIO, 3, DISCORD_SKIN_IMAGE("wr-3.png")},  // King Bob-omb
    {WARIO, 5, DISCORD_SKIN_IMAGE("wr-5.png")},  // Morton
    {WARIO, 6, DISCORD_SKIN_IMAGE("wr-6.png")},  // Snooza Koopa
    // WALUIGI
    {WALUIGI, 3, DISCORD_SKIN_IMAGE("wl-3.png")},  // Wiggler
    {WALUIGI, 5, DISCORD_SKIN_IMAGE("wl-5.png")},  // Captain Falcon
    // KING_BOO
    {KING_BOO, 2, DISCORD_SKIN_IMAGE("kt-2.png")},  // Gooper Blooper
    {KING_BOO, 3, DISCORD_SKIN_IMAGE("kt-3.png")},  // Goku
    // ROSALINA
    {ROSALINA, 3, DISCORD_SKIN_IMAGE("rs-3.png")},  // Pauline
    {ROSALINA, 5, DISCORD_SKIN_IMAGE("rs-5.png")},  // Honey Queen
    // DONKEY_KONG
    {DONKEY_KONG, 3, DISCORD_SKIN_IMAGE("dk-3.png")},  // Kritter
    {DONKEY_KONG, 4, DISCORD_SKIN_IMAGE("dk-4.png")},  // Pokey
    {DONKEY_KONG, 5, DISCORD_SKIN_IMAGE("dk-5.png")},  // King K. Rool
    {DONKEY_KONG, 6, DISCORD_SKIN_IMAGE("dk-6.png")},  // Pianta
    // FUNKY_KONG
    {FUNKY_KONG, 2, DISCORD_SKIN_IMAGE("fk-2.png")},  // Chain Chomp
    {FUNKY_KONG, 5, DISCORD_SKIN_IMAGE("fk-5.png")},  // R.O.B.
    {FUNKY_KONG, 6, DISCORD_SKIN_IMAGE("fk-6.png")},  // Jaxi
    {FUNKY_KONG, 7, DISCORD_SKIN_IMAGE("fk-7.png")},  // Donkey Kong Jr.
    // BOWSER
    {BOWSER, 3, DISCORD_SKIN_IMAGE("kp-3.png")},  // Chargin' Chuck
    {BOWSER, 4, DISCORD_SKIN_IMAGE("kp-4.png")},  // Boom Boom
    // DRY_BOWSER
    {DRY_BOWSER, 2, DISCORD_SKIN_IMAGE("bk-2.png")},  // Lubba
    {DRY_BOWSER, 3, DISCORD_SKIN_IMAGE("bk-3.png")},  // Petey Piranha
    {DRY_BOWSER, 4, DISCORD_SKIN_IMAGE("bk-4.png")},  // Plessie
    {CHARACTER_NONE, 0, nullptr},
};

#undef DISCORD_SKIN_IMAGE

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
    if (bmgId == 0)
        return nullptr;

    const wchar_t *msg = Pulsar::UI::GetCustomMsg(static_cast<s32>(bmgId));
    if (msg != nullptr && msg[0] != L'\0') {
        return msg;
    }

    const SectionMgr *sectionMgr = SectionMgr::sInstance;
    if (sectionMgr != nullptr && sectionMgr->systemBMG != nullptr) {
        const s32 msgId = sectionMgr->systemBMG->GetMsgId(static_cast<s32>(bmgId));
        if (msgId >= 0) {
            msg = sectionMgr->systemBMG->GetMsgByMsgId(msgId);
            if (msg != nullptr && msg[0] != L'\0')
                return msg;
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
                    if (msg != nullptr && msg[0] != L'\0')
                        return msg;
                }
            }
        }
    }

    return nullptr;
}

static const char *GetSkinDiscordImageKey(CharacterId character, u8 table, const char *defaultKey) {
    if (table == 0) {
        return defaultKey;
    }

    // Kart and bike variants share the same hosted skin images.
    CharacterId imageCharacter = character;
    switch (character) {
        case PEACH_BIKER:
            imageCharacter = PEACH;
            break;
        case DAISY_BIKER:
            imageCharacter = DAISY;
            break;
        case ROSALINA_BIKER:
            imageCharacter = ROSALINA;
            break;
        default:
            break;
    }

    // Check the hosted skin images
    for (u32 i = 0; customSkinDiscordImages[i].character != CHARACTER_NONE; ++i) {
        if (customSkinDiscordImages[i].character == imageCharacter && customSkinDiscordImages[i].table == table) {
            if (customSkinDiscordImages[i].key != nullptr && customSkinDiscordImages[i].key[0] != '\0') {
                return customSkinDiscordImages[i].key;
            }
        }
    }

    // Check if an asset key string is defined via BMG (0x8a00 | table)
    static char bmgImageKey[256];
    const u32 imageBmgId = (static_cast<u32>(character) << 16) | Pulsar::UI::BMG_CUSTOM_CHARACTER_IMAGE_START | table;
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

static void SetCharacterSmallImage(CharacterId character, u8 table = 0) {
    const char *defaultKey = "";
    const char *defaultName = "";
    GetDefaultCharacterInfo(character, defaultKey, defaultName);
    if (defaultKey[0] == '\0') {
        return;
    }

    const char *finalKey = defaultKey;
    const char *finalName = defaultName;
    char customNameUtf8[64];

    if (table != 0) {
        finalKey = GetSkinDiscordImageKey(character, table, defaultKey);

        const u32 bmgId = (static_cast<u32>(character) << 16) | Pulsar::UI::BMG_CUSTOM_CHARACTER_NAME_START | table;
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
    if (max_len == 0)
        return;

    size_t destIndex = 0;
    for (size_t i = 0;; i++) {
        wchar_t c = src[i];
        if (c == 0) {
            break;
        }
        if (c <= 0x007F) {
            if (destIndex + 1 >= max_len)
                break;
            dest[destIndex++] = (char)c;
        } else if (c <= 0x07FF) {
            if (destIndex + 2 >= max_len)
                break;
            dest[destIndex++] = 0xC0 | ((c >> 6) & 0x1F);
            dest[destIndex++] = 0x80 | (c & 0x3F);
        } else {
            if (destIndex + 3 >= max_len)
                break;
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
    if (scene == nullptr || scene->id != SCENE_ID_RACE || raceData == nullptr || raceInfo == nullptr || !raceInfo->IsAtLeastStage(RACESTAGE_INTRO)) {
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
        const u32 slot = Pulsar::Race::GetPlayerCustomCharacterSlot(localPlayerId, localCharId);
        SetCharacterSmallImage(localCharId, slot);
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

    Dolphin::SetDiscordPresence(details, state, "image_logo", largeImageText, smallImageKey, smallImageText, startTimeStamp, 0, minPlayers, maxPlayers);
}

kmCall(0x80635540, DiscordRichPresence);

}  // namespace Discord
