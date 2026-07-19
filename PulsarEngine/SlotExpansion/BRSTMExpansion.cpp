#include <kamek.hpp>
#include <MarioKartWii/Audio/AudioManager.hpp>
#include <MarioKartWii/Race/RaceData.hpp>
#include <MarioKartWii/UI/Section/SectionMgr.hpp>
#include <Gamemodes/MissionMode/MissionMode.hpp>
#include <Gamemodes/MissionMode/MissionMusic.hpp>
#include <Sound/MiscSound.hpp>
#include <SlotExpansion/CupsConfig.hpp>
#include <SlotExpansion/UI/ExpansionUIMisc.hpp>
#include <RetroRewind.hpp>

namespace Pulsar {
namespace Sound {

static char pulPath[0x100];

u8 GetSW2RRRacePercentageMusicTier();
bool IsSW2RRLoaded();

static bool ResolveKCMenuMusicPath(const SectionId section, const char*& extFilePath) {
    if (section >= SECTION_MAIN_MENU_FROM_BOOT && section <= SECTION_MAIN_MENU_FROM_LICENSE) {
        extFilePath = titleMusicFile;
        return true;
    }
    if (section >= SECTION_P1_WIFI && section <= SECTION_P2_WIFI_FROOM_COIN_VOTING) {
        if (IsWifiLobbySection(section))
            extFilePath = wifilobbyMusicFile;
        else
            extFilePath = wifiMusicFile;
        return true;
    }
    if ((section >= SECTION_SINGLE_P_FROM_MENU && section <= SECTION_SINGLE_P_LIST_RACE_GHOST) || section == SECTION_LOCAL_MULTIPLAYER) {
        extFilePath = offlineMusicFile;
        return true;
    }

    return false;
}

static bool CheckBRSTMPath(const char* path) {
    return DVD::ConvertPathToEntryNum(path) >= 0;
}

static bool StringEndsWith(const char* str, const char* suffix) {
    if (str == nullptr || suffix == nullptr) return false;

    const char* strEnd = str;
    while (*strEnd != '\0') ++strEnd;

    const char* suffixEnd = suffix;
    while (*suffixEnd != '\0') ++suffixEnd;

    while (suffixEnd != suffix) {
        if (strEnd == str) return false;
        --strEnd;
        --suffixEnd;
        if (*strEnd != *suffixEnd) return false;
    }
    return true;
}

static bool ResolveSW2RRFanfareGP1Path(const nw4r::snd::DVDSoundArchive* archive, const char*& extFilePath) {
    if (archive == nullptr || !IsSW2RRLoaded() || !StringEndsWith(extFilePath, "/o_FanfareGP1_32.brstm")) return false;

    snprintf(pulPath, sizeof(pulPath), "%sstrm/o_FanfareRRGP1_32.brstm", archive->extFileRoot);
    if (!CheckBRSTMPath(pulPath)) return false;

    extFilePath = pulPath;
    return true;
}

s32 CheckBRSTMRoot(const char* root, PulsarId id, const char* lapSpecifier,
                   const char* racePercentageSpecifier = "") {
    const CupsConfig* cupsConfig = CupsConfig::sInstance;
    const u8 variantIdx = cupsConfig->GetCurVariantIdx();
    const char* creatorName = cupsConfig->GetFileName(id, variantIdx);
    if (creatorName != nullptr) {
        snprintf(pulPath, 0x100, "%sstrm/%s%s%s.brstm", root, creatorName, lapSpecifier, racePercentageSpecifier);
        if (CheckBRSTMPath(pulPath)) return 0;
    }
    if (variantIdx != 0) {
        creatorName = cupsConfig->GetFileName(id, 0);
        if (creatorName != nullptr) {
            snprintf(pulPath, 0x100, "%sstrm/%s%s%s.brstm", root, creatorName, lapSpecifier, racePercentageSpecifier);
            if (CheckBRSTMPath(pulPath)) return 0;
        }
    }
    char trackName[0x100];
    UI::GetTrackBMG(trackName, id);
    snprintf(pulPath, 0x100, "%sstrm/%s%s%s.brstm", root, trackName, lapSpecifier, racePercentageSpecifier);
    if (CheckBRSTMPath(pulPath)) return 0;

    snprintf(pulPath, 0x50, "%sstrm/%d%s%s.brstm", root, CupsConfig::ConvertTrack_PulsarIdToRealId(id), lapSpecifier,
             racePercentageSpecifier);
    if (CheckBRSTMPath(pulPath)) return 0;
    return -1;
}

s32 CheckBRSTM(const nw4r::snd::DVDSoundArchive* archive, PulsarId id, const char* lapSpecifier,
               const char* racePercentageSpecifier = "") {
    return CheckBRSTMRoot(archive->extFileRoot, id, lapSpecifier, racePercentageSpecifier);
}

static const char* GetSW2RRRacePercentageSpecifier() {
    switch (GetSW2RRRacePercentageMusicTier()) {
        case 1: return "-1";
        case 2: return "-2";
        case 3: return "-3";
        default: return "";
    }
}

bool HasSW2RRTieredBRSTM(u8 tier) {
    if (tier == 0 || tier > 3) return true;

    const CupsConfig* cupsConfig = CupsConfig::sInstance;
    if (cupsConfig == nullptr) return false;

    const PulsarId track = cupsConfig->GetWinning();
    if (CupsConfig::IsReg(track)) return false;

    char racePercentageSpecifier[3];
    snprintf(racePercentageSpecifier, sizeof(racePercentageSpecifier), "-%u", tier);

    return CheckBRSTMRoot("/sound/", track, "_n", racePercentageSpecifier) >= 0;
}

nw4r::ut::FileStream* MusicSlotsExpand(nw4r::snd::DVDSoundArchive* archive, void* buffer, int size,
                                       const char* extFilePath, u32 r7, u32 length) {
    const Pulsar::CTMusic isBRSTMOn = static_cast<Pulsar::CTMusic>(Pulsar::Settings::Mgr::Get().GetUserSettingValue(
        static_cast<Pulsar::Settings::UserType>(Pulsar::Settings::SETTINGSTYPE_SOUND), Pulsar::RADIO_CTMUSIC));
    const char firstChar = extFilePath[0xC];
    const CupsConfig* cupsConfig = CupsConfig::sInstance;
    const PulsarId track = cupsConfig->GetWinning();
    register SoundIDs toPlayId;
    asm(mr toPlayId, r20;);

    ResolveSW2RRFanfareGP1Path(archive, extFilePath);

    if (toPlayId == SOUND_ID_KC) {
        const SectionId section = SectionMgr::sInstance->curSection->sectionId;
        if (ResolveKCMenuMusicPath(section, extFilePath)) {
            return archive->OpenExtStream(buffer, size, extFilePath, 0, length);
        }
    }

    if (firstChar == 'n' || firstChar == 'S' || firstChar == 'r') {
        if (Pulsar::MissionMode::ResolveMissionMusicPath(archive->extFileRoot, extFilePath)) {
            return archive->OpenExtStream(buffer, size, extFilePath, 0, length);
        }

        CourseId missionMusicSlot;
        const bool hasMissionNativeMusic = Pulsar::MissionMode::GetMissionMusicSlotOverride(missionMusicSlot);
        if (hasMissionNativeMusic || isBRSTMOn != Pulsar::CTMUSIC_ENABLED)
            return archive->OpenExtStream(buffer, size, extFilePath, 0, length);

        if (!CupsConfig::IsReg(track)) {
            register u32 strLength;
            asm(mr strLength, r28;);
            const char finalChar = extFilePath[strLength];
            const bool isFinalLap = finalChar == 'f' || finalChar == 'F';

            const char* racePercentageSpecifier = GetSW2RRRacePercentageSpecifier();
            const bool hasRacePercentageSpecifier = racePercentageSpecifier[0] != '\0';

            if (isFinalLap && hasRacePercentageSpecifier && CheckBRSTM(archive, track, "_final", racePercentageSpecifier) >= 0) {
                extFilePath = pulPath;
            } else if (hasRacePercentageSpecifier && CheckBRSTM(archive, track, "_n", racePercentageSpecifier) >= 0) {
                extFilePath = pulPath;
                if (isFinalLap) {
                    Audio::Manager::sInstance->soundArchivePlayer->soundPlayerArray->soundList.GetFront().ambientParam.pitch = 1.1f;
                }
            } else if (isFinalLap && CheckBRSTM(archive, track, "_final") >= 0) {
                extFilePath = pulPath;
            } else if (CheckBRSTM(archive, track, "_n") >= 0) {
                extFilePath = pulPath;
                if (isFinalLap) {
                    Audio::Manager::sInstance->soundArchivePlayer->soundPlayerArray->soundList.GetFront().ambientParam.pitch = 1.1f;
                }
            }
        }
    }
    return archive->OpenExtStream(buffer, size, extFilePath, 0, length);
}
kmCall(0x8009e0e4, MusicSlotsExpand);

}
}
