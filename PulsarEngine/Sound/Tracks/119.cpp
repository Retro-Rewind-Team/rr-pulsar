#include <kamek.hpp>
#include <MarioKartWii/Audio/RaceMgr.hpp>
#include <MarioKartWii/Audio/RSARPlayer.hpp>
#include <MarioKartWii/Audio/SinglePlayer.hpp>
#include <MarioKartWii/Audio/AudioManager.hpp>
#include <MarioKartWii/Race/RaceData.hpp>
#include <MarioKartWii/Race/RaceInfo/RaceInfo.hpp>
#include <Settings/Settings.hpp>
#include <SlotExpansion/CupsConfig.hpp>
#include <core/nw4r/snd/BasicSound.hpp>
#include <core/nw4r/snd/StrmSound.hpp>
#include <core/nw4r/snd/SoundArchivePlayer.hpp>
#include <core/nw4r/snd/SoundStartable.hpp>

namespace Pulsar {
namespace Sound {

static const float TRACK_119_TIER_1_THRESHOLD = 1.699f;
static const float TRACK_119_TIER_2_THRESHOLD = 1.732f;
static const float TRACK_119_NORMAL_LAP_JINGLE_THRESHOLD = 1.341f;
static const Audio::RaceState RACE_STATE_FINAL_LAP_MUSIC = static_cast<Audio::RaceState>(0x6);

static u8 track119MusicTier = 0;
static u32 track119ActiveStreamStartSample = 0;
static bool track119Loaded = false;
static bool track119LoadedInitialized = false;
static bool track119NormalLapJinglePlayed = false;
static bool track119Tier2ReloadPending = false;

bool Has119TieredBRSTM(u8 tier);

static bool IsCTMusicEnabled() {
    return Settings::Mgr::Get().GetUserSettingValue(Settings::SETTINGSTYPE_SOUND, RADIO_CTMUSIC) == CTMUSIC_ENABLED;
}

static bool StringsEqual(const char* lhs, const char* rhs) {
    if (lhs == nullptr || rhs == nullptr) return false;
    while (*lhs != '\0' && *rhs != '\0') {
        if (*lhs != *rhs) return false;
        ++lhs;
        ++rhs;
    }
    return *lhs == *rhs;
}

static bool Is119FileName(const char* fileName) {
    return StringsEqual(fileName, "119") || StringsEqual(fileName, "119.szs");
}

bool Is119Loaded() {
    const CupsConfig* cupsConfig = CupsConfig::sInstance;
    if (cupsConfig == nullptr) return false;

    const PulsarId track = cupsConfig->GetWinning();
    if (CupsConfig::IsReg(track)) return false;

    const u8 variantIdx = cupsConfig->GetCurVariantIdx();
    const char* fileName = cupsConfig->GetFileName(track, variantIdx);
    if (fileName == nullptr || fileName[0] == '\0') {
        fileName = cupsConfig->GetFileName(track, 0);
    }
    return Is119FileName(fileName);
}

static u8 Get119MusicTier(float raceCompletion) {
    if (raceCompletion < TRACK_119_TIER_1_THRESHOLD) return 0;
    if (raceCompletion < TRACK_119_TIER_2_THRESHOLD) return 1;
    return 2;
}

u8 Get119RacePercentageMusicTier() {
    return track119Loaded ? track119MusicTier : 0;
}

static void Reset119MusicState() {
    track119MusicTier = 0;
    track119ActiveStreamStartSample = 0;
    track119Loaded = false;
    track119LoadedInitialized = false;
    track119NormalLapJinglePlayed = false;
    track119Tier2ReloadPending = false;
}

static Audio::SinglePlayer* GetSinglePlayer() {
    Audio::SinglePlayer* singlePlayer = Audio::SinglePlayer::sInstance;
    if (singlePlayer == nullptr || singlePlayer->activeHandle == nullptr) return nullptr;
    return singlePlayer;
}

static u32 GetActiveSinglePlayerSoundId() {
    Audio::SinglePlayer* singlePlayer = GetSinglePlayer();
    if (singlePlayer == nullptr || singlePlayer->activeHandle->basicSound == nullptr) {
        return 0;
    }
    return singlePlayer->activeHandle->basicSound->soundId;
}

static u32 GetActiveRaceMusicSampleOffset() {
    Audio::SinglePlayer* singlePlayer = GetSinglePlayer();
    if (singlePlayer == nullptr || singlePlayer->activeHandle->basicSound == nullptr) return track119ActiveStreamStartSample;

    const nw4r::snd::detail::BasicSound* basicSound = singlePlayer->activeHandle->basicSound;
    const nw4r::snd::detail::StrmSound* strmSound = static_cast<const nw4r::snd::detail::StrmSound*>(basicSound);
    const u32 sampleRate = strmSound->strmPlayer.strmInfo.sampleRate;
    return track119ActiveStreamStartSample + (basicSound->updateCounter * sampleRate) / 60;
}

static void ReloadMainRaceMusic(u32 soundId, u32 sampleOffset = 0) {
    if (soundId == 0) return;

    Audio::SinglePlayer* singlePlayer = Audio::SinglePlayer::sInstance;
    Audio::Manager* audioMgr = Audio::Manager::sInstance;
    if (singlePlayer == nullptr || audioMgr == nullptr || audioMgr->soundArchivePlayer == nullptr) return;

    singlePlayer->canNotCancel = false;
    singlePlayer->canNotPrepareOther = false;

    nw4r::snd::SoundStartable::StartInfo startInfo;
    startInfo.enableFlag = nw4r::snd::SoundStartable::StartInfo::ENABLE_START_OFFSET;
    startInfo.startOffsetType = nw4r::snd::SoundStartable::StartInfo::START_OFFSET_TYPE_SAMPLE;
    startInfo.startOffset = sampleOffset;

    Audio::Handle* handle = singlePlayer->GetFreeHandle();
    if (handle == nullptr) return;
    if (audioMgr->soundArchivePlayer->detail_PrepareSound(handle, soundId, &startInfo) !=
        nw4r::snd::SoundStartable::START_SUCCESS) {
        return;
    }

    singlePlayer->preparedHandle = handle;
    singlePlayer->StopSound();
    singlePlayer->PlayPreparedSound(0);
    singlePlayer->StopInactiveSounds();
    track119ActiveStreamStartSample = sampleOffset;
}

static void ReloadActiveRaceMusic(bool preserveSampleOffset = false) {
    const u32 sampleOffset = preserveSampleOffset ? GetActiveRaceMusicSampleOffset() : 0;
    ReloadMainRaceMusic(GetActiveSinglePlayerSoundId(), sampleOffset);
}

static bool UpdatePendingTier2Reload(const Audio::RaceMgr& raceAudioMgr) {
    if (!track119Tier2ReloadPending || raceAudioMgr.raceState != RACE_STATE_FINAL_LAP_MUSIC) return false;

    track119Tier2ReloadPending = false;
    ReloadActiveRaceMusic();
    return true;
}

static u8 GetHudSlotIdForPlayer(u8 playerId) {
    const Racedata* racedata = Racedata::sInstance;
    if (racedata == nullptr) return 0;
    return racedata->GetHudSlotId(playerId);
}

static void Play119NormalLapJingle(u8 playerId) {
    Audio::RaceRSARPlayer* rsarPlayer = static_cast<Audio::RaceRSARPlayer*>(Audio::RSARPlayer::sInstance);
    if (rsarPlayer == nullptr) return;

    const u8 hudSlotId = GetHudSlotIdForPlayer(playerId);
    rsarPlayer->PlaySound(SOUND_ID_NORMAL_LAP, hudSlotId);
}

static void Play119TierChangeJingle(Audio::RaceMgr& raceAudioMgr, u8 tier, u8 playerId) {
    if (tier >= 2) {
        if (raceAudioMgr.raceState == RACE_STATE_FINAL_LAP_MUSIC) {
            raceAudioMgr.raceState = Audio::RACE_STATE_NORMAL;
        }
        raceAudioMgr.SetRaceState(Audio::RACE_STATE_FAST);
        return;
    }

    Play119NormalLapJingle(playerId);
}

void Update119RacePercentageMusic() {
    if (!IsCTMusicEnabled()) {
        Reset119MusicState();
        return;
    }

    Audio::RaceMgr* raceAudioMgr = Audio::RaceMgr::sInstance;
    const Raceinfo* raceInfo = Raceinfo::sInstance;
    if (raceAudioMgr == nullptr || raceInfo == nullptr || raceInfo->players == nullptr) return;
    if (raceInfo->timerMgr == nullptr || !raceInfo->timerMgr->hasRaceStarted) {
        Reset119MusicState();
        return;
    }

    if (!track119LoadedInitialized) {
        track119Loaded = Is119Loaded();
        track119LoadedInitialized = true;
    }
    if (!track119Loaded) {
        track119MusicTier = 0;
        return;
    }

    if (UpdatePendingTier2Reload(*raceAudioMgr)) return;

    const u8 playerId = raceAudioMgr->playerIdFirstLocalPlayer;
    if (playerId >= 12 || raceInfo->players[playerId] == nullptr) return;

    const float raceCompletion = raceInfo->players[playerId]->raceCompletion;
    if (!track119NormalLapJinglePlayed && raceCompletion >= TRACK_119_NORMAL_LAP_JINGLE_THRESHOLD) {
        track119NormalLapJinglePlayed = true;
        Play119NormalLapJingle(playerId);
    }

    u8 nextTier = Get119MusicTier(raceCompletion);
    if (nextTier < track119MusicTier) nextTier = track119MusicTier;
    if (nextTier == track119MusicTier) return;

    if (nextTier >= 2 && !Has119TieredBRSTM(nextTier)) return;

    track119MusicTier = nextTier;
    track119Tier2ReloadPending = false;
    if (nextTier == 0) {
        ReloadActiveRaceMusic();
    } else if (nextTier < 2) {
        ReloadActiveRaceMusic(true);
    } else {
        track119Tier2ReloadPending = true;
        Play119TierChangeJingle(*raceAudioMgr, nextTier, playerId);
    }
}

static RaceLoadHook Reset119MusicStateOnRaceLoad(Reset119MusicState);

}
}
