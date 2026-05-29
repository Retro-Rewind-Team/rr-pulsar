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

namespace Discord {

static bool hasWrittenClientID = false;
static int frameCount = 0;
static u64 startTimeStamp = 0;
SectionId prevSectionId = SECTION_NONE;
static CharacterId charID = CHARACTER_NONE;

static char smallImageKey[32] = "";
static char smallImageText[32] = "";

static void SetSmallImage(const char* key, const char* text) {
    snprintf(smallImageKey, sizeof(smallImageKey), "%s", key);
    snprintf(smallImageText, sizeof(smallImageText), "%s", text);
}

static void SetCharacterSmallImage(CharacterId character) {
    switch (character) {
        case BABY_MARIO:
            SetSmallImage("bmario", "Baby Mario");
            break;
        case BABY_LUIGI:
            SetSmallImage("bluigi", "Baby Luigi");
            break;
        case BABY_PEACH:
            SetSmallImage("bpeach", "Baby Peach");
            break;
        case BABY_DAISY:
            SetSmallImage("bdaisy", "Baby Daisy");
            break;
        case TOAD:
            SetSmallImage("toad", "Toad");
            break;
        case TOADETTE:
            SetSmallImage("toadette", "Toadette");
            break;
        case KOOPA_TROOPA:
            SetSmallImage("koopa_troopa", "Koopa Troopa");
            break;
        case DRY_BONES:
            SetSmallImage("dry_bones", "Dry Bones");
            break;
        case MARIO:
            SetSmallImage("mario", "Mario");
            break;
        case LUIGI:
            SetSmallImage("luigi", "Luigi");
            break;
        case PEACH:
        case PEACH_BIKER:
            SetSmallImage("peach", "Peach");
            break;
        case DAISY:
        case DAISY_BIKER:
            SetSmallImage("daisy", "Daisy");
            break;
        case YOSHI:
            SetSmallImage("yoshi", "Yoshi");
            break;
        case BIRDO:
            SetSmallImage("birdo", "Birdo");
            break;
        case DIDDY_KONG:
            SetSmallImage("diddy", "Diddy Kong");
            break;
        case BOWSER_JR:
            SetSmallImage("bowser_jr", "Bowser Jr");
            break;
        case WARIO:
            SetSmallImage("wario", "Wario");
            break;
        case WALUIGI:
            SetSmallImage("waluigi", "Waluigi");
            break;
        case DONKEY_KONG:
            SetSmallImage("dk", "Donkey Kong");
            break;
        case BOWSER:
            SetSmallImage("bowser", "Bowser");
            break;
        case KING_BOO:
            SetSmallImage("king_boo", "King Boo");
            break;
        case ROSALINA:
        case ROSALINA_BIKER:
            SetSmallImage("rosalina", "Rosalina");
            break;
        case FUNKY_KONG:
            SetSmallImage("funky", "Funky Kong");
            break;
        case DRY_BOWSER:
            SetSmallImage("dry_bowser", "Dry Bowser");
            break;
        case MII_L_A_MALE:
        case MII_L_A_FEMALE:
        case MII_M_A_MALE:
        case MII_M_A_FEMALE:
        case MII_S_A_MALE:
        case MII_S_A_FEMALE:
            SetSmallImage("mii_a", "Mii (Outfit A)");
            break;
        case MII_L_B_MALE:
        case MII_L_B_FEMALE:
        case MII_M_B_MALE:
        case MII_M_B_FEMALE:
        case MII_S_B_MALE:
        case MII_S_B_FEMALE:
            SetSmallImage("mii_b", "Mii (Outfit B)");
            break;
        default:
            break;
    }
}

// Removes 00 1A escapes from the BMG text
void CleanBMGMessage(wchar_t* dest, const wchar_t* src) {
    int inc = 0;
    for (int i = 0; i < 0x100 && src[i] && inc + 1 < 0x100; i++) {
        if (src[i] == 0x001a) {
            u8 size = *(u8*)(&src[i + 1]);
            i += (size / 2) - 1;
        } else {
            dest[inc] = src[i];
            inc++;
        }
    }
    dest[inc] = '\0';
}

void ConvertUTF16toUtf8(char* dest, const wchar_t* src, size_t max_len) {
    if (max_len == 0) return;

    size_t destIndex = 0;
    for (size_t i = 0; ; i++) {
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

static CharacterId GetFirstLocalRaceCharacter() {
    const GameScene* scene = GameScene::GetCurrent();
    Racedata* raceData = Racedata::sInstance;
    Raceinfo* raceInfo = Raceinfo::sInstance;
    if (scene == nullptr || scene->id != SCENE_ID_RACE || raceData == nullptr || raceInfo == nullptr ||
        !raceInfo->IsAtLeastStage(RACESTAGE_INTRO)) {
        return CHARACTER_NONE;
    }

    const RacedataScenario& scenario = raceData->racesScenario;
    if (scenario.localPlayerCount > 0) {
        const u8 playerId = scenario.settings.hudPlayerIds[0];
        if (playerId < scenario.playerCount && scenario.players[playerId].playerType == PLAYER_REAL_LOCAL) {
            return scenario.players[playerId].characterId;
        }
    }

    for (u32 i = 0; i < scenario.playerCount; ++i) {
        if (scenario.players[i].playerType == PLAYER_REAL_LOCAL) {
            return scenario.players[i].characterId;
        }
    }
    return CHARACTER_NONE;
}

void DiscordRichPresence(Section* _this) {
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

    char* state = "";
    char* details = "In a Menu";
    char* largeImageText = "";
    int minPlayers = 0;
    int maxPlayers = 0;

    RKSYS::Mgr* rksysMgr = RKSYS::Mgr::sInstance;
    float vr = 0, br = 0;
    u64 fc = 0;

    smallImageKey[0] = '\0';
    smallImageText[0] = '\0';

    if (rksysMgr && rksysMgr->curLicenseId >= 0) {
        RKSYS::LicenseMgr& license = rksysMgr->licenses[rksysMgr->curLicenseId];
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

    charID = GetFirstLocalRaceCharacter();
    if (charID != CHARACTER_NONE) {
        SetCharacterSmallImage(charID);
    }

    if (_this->sectionId != prevSectionId) {
        Dolphin::GetSystemTime(startTimeStamp);
        prevSectionId = _this->sectionId;
    }

    wchar_t trackNameW[0x100];
    char trackName[0x100];

    memset(trackNameW, 0, 0x100);

    u32 bmgId = Pulsar::UI::GetCurTrackBMG();
    const wchar_t* msg = Pulsar::UI::GetCustomMsg(bmgId);
    if (msg && Raceinfo::sInstance && Raceinfo::sInstance->IsAtLeastStage(RACESTAGE_INTRO)) {
        CleanBMGMessage(trackNameW, msg);
        ConvertUTF16toUtf8(trackName, trackNameW, 32);
        state = trackName;
    }

    RKNet::Controller* controller = RKNet::Controller::sInstance;
    if (controller) {
        RKNet::ControllerSub& sub = controller->subs[controller->currentSub];
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
