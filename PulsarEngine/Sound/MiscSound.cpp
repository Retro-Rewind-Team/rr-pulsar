#include <kamek.hpp>
#include <MarioKartWii/Audio/AudioManager.hpp>
#include <MarioKartWii/Audio/RSARPlayer.hpp>
#include <MarioKartWii/Audio/SinglePlayer.hpp>
#include <MarioKartWii/Audio/Other/AudioStreamsMgr.hpp>
#include <MarioKartWii/UI/Section/SectionMgr.hpp>
#include <core/nw4r/snd/BasicSound.hpp>
#include <Sound/MiscSound.hpp>
#include <Settings/Settings.hpp>
#include <runtimeWrite.hpp>

namespace Pulsar {
namespace Sound {
using namespace nw4r;

static bool IsOnlineRaceSection(SectionId sectionId) {
    switch (sectionId) {
        case SECTION_P1_WIFI_VS:
        case SECTION_P2_WIFI_VS:
        case SECTION_P1_WIFI_FRIEND_VS:
        case SECTION_P1_WIFI_FRIEND_TEAMVS:
        case SECTION_P2_WIFI_FRIEND_VS:
        case SECTION_P2_WIFI_FRIEND_TEAMVS:
        case SECTION_P1_WIFI_VS_LIVEVIEW:
        case SECTION_P2_WIFI_VS_LIVEVIEW:
        case SECTION_P1_WIFI_BT:
        case SECTION_P2_WIFI_BT:
        case SECTION_P1_WIFI_FRIEND_BALLOON:
        case SECTION_P1_WIFI_FRIEND_COIN:
        case SECTION_P2_WIFI_FRIEND_BALLOON:
        case SECTION_P2_WIFI_FRIEND_COIN:
        case SECTION_P1_WIFI_BT_LIVEVIEW:
        case SECTION_P2_WIFI_BT_LIVEVIEW:
            return true;
        default:
            return false;
    }
}

static bool ShouldRefreshWifiMenuMusic(u32 soundId) {
    if (soundId != SOUND_ID_WIFI_MUSIC) return false;

    const u8 musicSetting = Settings::Mgr::Get().GetUserSettingValue(Settings::SETTINGSTYPE_SOUND, RADIO_MUSIC);
    if (musicSetting != MUSIC_DISABLE_RACE) return false;

    const SectionMgr* sectionMgr = SectionMgr::sInstance;
    if (sectionMgr == nullptr) return false;

    return IsOnlineRaceSection(sectionMgr->prevSectionId);
}

// RaceAudioMgr SetRaceState patch that skips the entire func, effectively disabling the mgr
static void DisableRaceMusic(Audio::SinglePlayer& singlePlayer, u32 soundId, s16 delay) {
    const bool isEnabled = Settings::Mgr::Get().GetUserSettingValue(Settings::SETTINGSTYPE_SOUND, RADIO_MUSIC) == MUSIC_DEFAULT;
    if (isEnabled) singlePlayer.PlaySound(soundId, delay);
}
kmCall(0x80711fcc, DisableRaceMusic);  // RaceMgr::SetRaceState
kmCall(0x80711df4, DisableRaceMusic);  // RaceMgr::SetRaceState
kmCall(0x80712198, DisableRaceMusic);  // RaceMgr::SetRaceState
kmCall(0x80711074, DisableRaceMusic);  // RaceMgr::SetRaceState
kmCall(0x8064a398, DisableRaceMusic);  // wifi waiting, hook at Page::LiveViewWaiting
kmCall(0x8064a340, DisableRaceMusic);  // wifi waiting

static void PreventPrepareRaceMusic(u32 unused, Audio::Handle* handle, u32 soundId) {
    const bool isEnabled = Settings::Mgr::Get().GetUserSettingValue(Settings::SETTINGSTYPE_SOUND, RADIO_MUSIC) == MUSIC_DEFAULT;
    if (isEnabled) Audio::Manager::sInstance->PrepareSound(handle, soundId);
}
kmCall(0x806f8eb4, PreventPrepareRaceMusic);

static void DisableMenuMusic(Audio::SinglePlayer& singlePlayer, u32 soundId, s16 delay) {
    const bool isEnabled = Settings::Mgr::Get().GetUserSettingValue(Settings::SETTINGSTYPE_SOUND, RADIO_MUSIC) != MUSIC_DISABLE_ALL;
    if (isEnabled) singlePlayer.PlaySound(soundId, delay);
}
kmCall(0x806fa64c, DisableMenuMusic);

static void DisableAndChangeBGMusic(Audio::SinglePlayer& singlePlayer, u32 soundId) {
    const bool isEnabled = Settings::Mgr::Get().GetUserSettingValue(Settings::SETTINGSTYPE_SOUND, RADIO_MUSIC) != MUSIC_DISABLE_ALL;
    if (isEnabled) {
        const bool shouldRefreshWifiMusic = ShouldRefreshWifiMenuMusic(soundId);
        const char* customBGPath = nullptr;
        if (soundId == SOUND_ID_TITLE)
            customBGPath = titleMusicFile;
        else if (soundId == SOUND_ID_OFFLINE_MENUS)
            customBGPath = offlineMusicFile;
        else if (soundId == SOUND_ID_WIFI_MUSIC) {
            const SectionId curSection = SectionMgr::sInstance->curSection->sectionId;
            customBGPath = IsWifiLobbySection(curSection) ? wifilobbyMusicFile : wifiMusicFile;
        }
        if (customBGPath != nullptr) {
            DVD::FileInfo info;
            BOOL ret = DVD::Open(customBGPath, &info);
            if (ret) {
                DVD::Close(&info);
                if (info.length > 0) {
                    soundId = SOUND_ID_KC;
                    singlePlayer.StopInactiveSounds();
                    if (shouldRefreshWifiMusic) {
                        singlePlayer.StopSound();
                        singlePlayer.PrepareSound(soundId, true);
                    } else if (customBGPath != nullptr) {
                        singlePlayer.PrepareSound(soundId, false);  // needed so that the streamsMgr has its internal handle set
                    }
                }
            }
        }
        singlePlayer.PlaySound(soundId, 0);
        if (customBGPath != nullptr) singlePlayer.StopInactiveSounds();
    }
}
kmCall(0x806fa664, DisableAndChangeBGMusic);
kmWrite32(0x8085a674, 0x60000000);  // no use in preparing the TT jingle if you're going to the change character menu
static snd::SoundArchive::SoundType PatchPrepareStreamsBG(snd::SoundArchive& archive, u32 soundId) {
    if (soundId == SOUND_ID_KC) {
        const SectionId section = SectionMgr::sInstance->curSection->sectionId;

        u8 type = 0;
        if (section >= SECTION_SINGLE_P_FROM_MENU && section <= SECTION_SINGLE_P_LIST_RACE_GHOST || section == SECTION_LOCAL_MULTIPLAYER)
            type = 1;
        else if (section >= SECTION_P1_WIFI && section <= SECTION_P2_WIFI_FROOM_COIN_VOTING)
            type = 2;
        if (type != 0) {
            register Audio::StreamsMgr* streams;
            asm(mr streams, r30;);
            snd::StrmSoundHandle strmHandle(streams->curHandle);
            for (int i = 0; i < 4; ++i) {
                float volume = 1.0f;
                streams->streamsVolume[i].curValue = volume;
                strmHandle.SetTrackVolume(1 << i, volume);
            }
            return snd::SoundArchive::SOUND_TYPE_INVALID;
        }
    }
    return archive.GetSoundType(soundId);
}
kmCall(0x806fa2fc, PatchPrepareStreamsBG);

static void ToggleMenuMusic() {
    const bool isEnabled = Settings::Mgr::Get().GetUserSettingValue(Settings::SETTINGSTYPE_SOUND, RADIO_MUSIC) != MUSIC_DISABLE_ALL;
    Audio::SinglePlayer* singlePlayer = Audio::SinglePlayer::sInstance;
    if (isEnabled)
        singlePlayer->PlayBGSound(2);
    else
        singlePlayer->StopSound();
}
Settings::Hook ToggleMenuMusicHook(ToggleMenuMusic);

static float CheckFanfare(const Audio::SinglePlayer& singlePlayer) {
    const bool isEnabled = Settings::Mgr::Get().GetUserSettingValue(Settings::SETTINGSTYPE_SOUND, RADIO_MUSIC) == MUSIC_DEFAULT;
    if (isEnabled)
        return singlePlayer.GetFanfareLength();
    else
        return -1.0f;
}
kmCall(0x80857860, CheckFanfare);

static u8 specialItemReceiveSoundEnabled = 1;
static u8 specialItemReceiveSoundPitchPending = 0;

static void RefreshSpecialItemReceiveSoundSetting() {
    specialItemReceiveSoundEnabled =
        Settings::Mgr::Get().GetUserSettingValue(Settings::SETTINGSTYPE_SOUND, RADIO_SPECIALITEMRECEIVE) == SPECIALITEMRECEIVE_ENABLED;
}
Settings::Hook RefreshSpecialItemReceiveSoundSettingHook(RefreshSpecialItemReceiveSoundSetting);
RaceLoadHook RefreshSpecialItemReceiveSoundSettingRaceHook(RefreshSpecialItemReceiveSoundSetting);

// Play Special Item Recieve Sound [_Ro]
static asmFunc UseThundercloudReceiveSoundForSpecialItems() {
    ASM(
        nofralloc;
        li r4, 0xE3;

        stwu r1, -0x10(r1);
        stw r11, 0x8(r1);
        li r0, 0;
        lis r11, specialItemReceiveSoundPitchPending @ha;
        stb r0, specialItemReceiveSoundPitchPending @l(r11);

        lis r11, specialItemReceiveSoundEnabled @ha;
        lbz r11, specialItemReceiveSoundEnabled @l(r11);
        cmpwi r11, 0;
        lwz r11, 0x8(r1);
        addi r1, r1, 0x10;
        beq - end;

        subi r0, r28, 8;
        cmplwi r0, 3;
        ble - custom;
        cmpwi r28, 0xF;
        bne - end;

        custom :;
        stwu r1, -0x10(r1);
        stw r11, 0x8(r1);
        li r0, 1;
        lis r11, specialItemReceiveSoundPitchPending @ha;
        stb r0, specialItemReceiveSoundPitchPending @l(r11);
        lwz r11, 0x8(r1);
        addi r1, r1, 0x10;

        end :;
        blr;);
}
kmCall(0x8079814c, UseThundercloudReceiveSoundForSpecialItems);

kmRuntimeUse(0x8071497c);
static bool StartItemReceiveSoundWithPitch(Audio::RSARPlayer* rsarPlayer, u32 soundId, u32 localPlayerNum) {
    typedef bool (*PlaySound)(Audio::RSARPlayer*, u32, u32);
    const bool ret = reinterpret_cast<PlaySound>(kmRuntimeAddr(0x8071497c))(rsarPlayer, soundId, localPlayerNum);

    if (ret && specialItemReceiveSoundPitchPending != 0 && soundId == 0xE3 && Audio::RSARPlayer::seqSoundHandle.basicSound != nullptr) {
        Audio::RSARPlayer::seqSoundHandle.basicSound->SetPitch(1.15f);
    }
    specialItemReceiveSoundPitchPending = 0;
    return ret;
}
kmCall(0x80798160, StartItemReceiveSoundWithPitch);

snd::SoundStartable::StartResult PlayExtBRSEQ(snd::SoundStartable& startable, Audio::Handle& handle, const char* fileName, const char* labelName, bool hold) {
    snd::SoundStartable::StartInfo startInfo;
    startInfo.seqSoundInfo.startLocationLabel = labelName;
    startInfo.enableFlag |= snd::SoundStartable::StartInfo::ENABLE_SEQ_SOUND_INFO;

    void* file = ArchiveMgr::sInstance->GetFile(ARCHIVE_HOLDER_COMMON, fileName);
    if (file != nullptr) {
        startInfo.seqSoundInfo.seqDataAddress = file;
        if (hold)
            return startable.detail_HoldSound(&handle, SOUND_ID_MINITURBO, &startInfo);
        else
            return startable.detail_StartSound(&handle, SOUND_ID_MINITURBO, &startInfo);
    }
    return snd::SoundStartable::START_ERR_USER;
}

// disable TF music delay
kmWrite16(0x80711FE8, 0x00004800);
kmWrite16(0x80712024, 0x00004800);
kmWrite16(0x8071207C, 0x00004800);
kmWrite16(0x807120B8, 0x00004800);

// disable tf music reset
kmWrite32(0x80719920, 0x48000010);

// disable THP and demo by blring SinglePlayer::CalcTitlePageNext
kmWrite32(0x806fa738, 0x4e800020);
}  // namespace Sound
}  // namespace Pulsar
