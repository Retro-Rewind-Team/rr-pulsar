#include <kamek.hpp>
#include <MarioKartWii/Audio/AudioManager.hpp>
#include <MarioKartWii/Audio/RaceMgr.hpp>
#include <MarioKartWii/Race/RaceInfo/RaceInfo.hpp>
#include <MarioKartWii/UI/Section/SectionMgr.hpp>
#include <Sound/MiscSound.hpp>
#include <IO/LooseArchiveOverrides.hpp>
#include <SlotExpansion/CupsConfig.hpp>
#include <SlotExpansion/UI/ExpansionUIMisc.hpp>
#include <RetroRewind.hpp>

namespace Pulsar {
namespace Sound {

static char pulPath[0x100];

u16 GetCheckpointMusicIndex();
bool IsCheckpointFinalLap();

static bool ResolveKCMenuMusicPath(const SectionId section, const char *&extFilePath) {
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

static bool CheckBRSTMPath(const char *path) {
    return IOOverrides::ConvertPathToEntryNumWithLooseOverride(path) >= 0;
}

s32 CheckBRSTMRoot(const char *root, PulsarId id, const char *lapSpecifier, const char *musicSpecifier = "") {
    const CupsConfig *cupsConfig = CupsConfig::sInstance;
    const u8 variantIdx = cupsConfig->GetCurVariantIdx();
    const char *creatorName = cupsConfig->GetFileName(id, variantIdx);
    if (creatorName != nullptr) {
        snprintf(pulPath, 0x100, "%sstrm/%s%s%s.brstm", root, creatorName, lapSpecifier, musicSpecifier);
        if (CheckBRSTMPath(pulPath))
            return 0;
    }
    if (variantIdx != 0) {
        creatorName = cupsConfig->GetFileName(id, 0);
        if (creatorName != nullptr) {
            snprintf(pulPath, 0x100, "%sstrm/%s%s%s.brstm", root, creatorName, lapSpecifier, musicSpecifier);
            if (CheckBRSTMPath(pulPath))
                return 0;
        }
    }
    char trackName[0x100];
    UI::GetTrackBMG(trackName, id);
    snprintf(pulPath, 0x100, "%sstrm/%s%s%s.brstm", root, trackName, lapSpecifier, musicSpecifier);
    if (CheckBRSTMPath(pulPath))
        return 0;

    snprintf(pulPath, 0x50, "%sstrm/%d%s%s.brstm", root, CupsConfig::ConvertTrack_PulsarIdToRealId(id), lapSpecifier, musicSpecifier);
    if (CheckBRSTMPath(pulPath))
        return 0;
    return -1;
}

static bool ResolveTrackFanfareGP1Path(const nw4r::snd::DVDSoundArchive *archive, SoundIDs soundId, const char *&extFilePath) {
    // The stream hook also runs during awards; reject non-winning sounds before accessing race state.
    if (soundId != SOUND_ID_1STPLACE_FINISH_RESULTS && soundId != SOUND_ID_1STPLACE_FINISH_FANFARE && soundId != SOUND_ID_VS_1STPLACE_FINISH_RESULTS && soundId != SOUND_ID_MISSION_BOSS_WIN_FANFARE)
        return false;
    const Audio::RaceMgr *raceAudioMgr = Audio::RaceMgr::sInstance;
    const PulsarId track = CupsConfig::sInstance->GetWinning();
    if (raceAudioMgr == nullptr || CupsConfig::IsReg(track) || Raceinfo::sInstance->players[raceAudioMgr->playerIdFirstLocalPlayer]->position != 1)
        return false;

    if (CheckBRSTMRoot(archive->extFileRoot, track, "_o_FanfareGP1_32", "") < 0)
        return false;
    extFilePath = pulPath;
    return true;
}

s32 CheckBRSTM(const nw4r::snd::DVDSoundArchive *archive, PulsarId id, const char *lapSpecifier, const char *musicSpecifier = "") {
    if (CheckBRSTMRoot(archive->extFileRoot, id, lapSpecifier, musicSpecifier) >= 0)
        return 0;
    return CheckBRSTMRoot(archive->extFileRoot, id, lapSpecifier[1] == 'n' ? "_N" : "_F", musicSpecifier);
}

bool HasCheckpointBRSTM(u16 index) {
    const CupsConfig *cupsConfig = CupsConfig::sInstance;
    if (cupsConfig == nullptr)
        return false;

    const PulsarId track = cupsConfig->GetWinning();
    if (CupsConfig::IsReg(track))
        return false;

    char musicSpecifier[4];
    snprintf(musicSpecifier, sizeof(musicSpecifier), "%u", index);

    return CheckBRSTMRoot("/sound/", track, "_n", musicSpecifier) >= 0 || CheckBRSTMRoot("/sound/", track, "_N", musicSpecifier) >= 0;
}

nw4r::ut::FileStream *MusicSlotsExpand(nw4r::snd::DVDSoundArchive *archive, void *buffer, int size, const char *extFilePath, u32 r7, u32 length) {
    const Pulsar::CTMusic isBRSTMOn = static_cast<Pulsar::CTMusic>(Pulsar::Settings::Mgr::Get().GetSettingValue(Pulsar::Settings::SETTING_CTMUSIC));
    const char firstChar = extFilePath[0xC];
    const CupsConfig *cupsConfig = CupsConfig::sInstance;
    const PulsarId track = cupsConfig->GetWinning();
    register SoundIDs toPlayId;
    asm(mr toPlayId, r20;);

    if (isBRSTMOn == Pulsar::CTMUSIC_ENABLED && ResolveTrackFanfareGP1Path(archive, toPlayId, extFilePath))
        return archive->OpenExtStream(buffer, size, extFilePath, 0, length);

    if (toPlayId == SOUND_ID_KC) {
        const SectionId section = SectionMgr::sInstance->curSection->sectionId;
        if (ResolveKCMenuMusicPath(section, extFilePath)) {
            return archive->OpenExtStream(buffer, size, extFilePath, 0, length);
        }
    }
    if ((firstChar == 'n' || firstChar == 'S' || firstChar == 'r') && isBRSTMOn == Pulsar::CTMUSIC_ENABLED) {
        if (!CupsConfig::IsReg(track)) {
            register u32 strLength;
            asm(mr strLength, r28;);
            const char finalChar = extFilePath[strLength];
            const bool isFinalLap = finalChar == 'f' || finalChar == 'F' || IsCheckpointFinalLap();
            char musicSpecifier[4] = "";
            const u16 musicIndex = GetCheckpointMusicIndex();
            if (musicIndex > 1)
                snprintf(musicSpecifier, sizeof(musicSpecifier), "%u", musicIndex);

            if (isFinalLap && CheckBRSTM(archive, track, "_final") >= 0) {
                extFilePath = pulPath;
            } else if (CheckBRSTM(archive, track, "_n", musicSpecifier) >= 0) {
                extFilePath = pulPath;
                if (isFinalLap) {
                    Audio::Manager::sInstance->soundArchivePlayer->soundPlayerArray->soundList.GetFront().ambientParam.pitch = 1.06f;
                }
            } else if (CheckBRSTM(archive, track, "_n") >= 0) {
                extFilePath = pulPath;
                if (isFinalLap) {
                    Audio::Manager::sInstance->soundArchivePlayer->soundPlayerArray->soundList.GetFront().ambientParam.pitch = 1.06f;
                }
            }
        }
    }
    return archive->OpenExtStream(buffer, size, extFilePath, 0, length);
}
kmCall(0x8009e0e4, MusicSlotsExpand);

}  // namespace Sound
}  // namespace Pulsar
