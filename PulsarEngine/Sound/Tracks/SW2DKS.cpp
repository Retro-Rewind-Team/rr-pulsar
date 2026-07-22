#include <kamek.hpp>
#include <MarioKartWii/Audio/RaceMgr.hpp>
#include <MarioKartWii/Audio/RSARPlayer.hpp>
#include <MarioKartWii/Audio/SinglePlayer.hpp>
#include <MarioKartWii/Race/RaceData.hpp>
#include <MarioKartWii/Race/RaceInfo/RaceInfo.hpp>
#include <Settings/Settings.hpp>
#include <SlotExpansion/CupsConfig.hpp>
#include <core/nw4r/snd/BasicSound.hpp>

namespace Pulsar {
namespace Sound {

static const float SW2DKS_TIER_1_THRESHOLD = 1.159f;
static const float SW2DKS_TIER_2_THRESHOLD = 1.236f;
static const float SW2DKS_TIER_3_THRESHOLD = 1.314f;
static const float SW2DKS_TIER_4_THRESHOLD = 1.389f;
static const float SW2DKS_TIER_5_THRESHOLD = 1.480f;
static const Audio::RaceState RACE_STATE_FINAL_LAP_MUSIC = static_cast<Audio::RaceState>(0x6);

static u8 sw2dksMusicTier = 0;
static bool sw2dksLoaded = false;
static bool sw2dksLoadedInitialized = false;
static bool sw2dksTier5ReloadPending = false;

bool HasSW2DKSTieredBRSTM(u8 tier);

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

static bool IsSW2DKSFileName(const char* fileName) {
    return StringsEqual(fileName, "sw2DKS") || StringsEqual(fileName, "SW2DKS") ||
        StringsEqual(fileName, "sw2DKS.szs") || StringsEqual(fileName, "SW2DKS.szs");
}

bool IsSW2DKSLoaded() {
    const CupsConfig* cupsConfig = CupsConfig::sInstance;
    if (cupsConfig == nullptr) return false;

    const PulsarId track = cupsConfig->GetWinning();
    if (CupsConfig::IsReg(track)) return false;

    const u8 variantIdx = cupsConfig->GetCurVariantIdx();
    const char* fileName = cupsConfig->GetFileName(track, variantIdx);
    if (fileName == nullptr || fileName[0] == '\0') {
        fileName = cupsConfig->GetFileName(track, 0);
    }
    return IsSW2DKSFileName(fileName);
}

static u8 GetSW2DKSMusicTier(float raceCompletion) {
    if (raceCompletion < SW2DKS_TIER_1_THRESHOLD) return 0;
    if (raceCompletion < SW2DKS_TIER_2_THRESHOLD) return 1;
    if (raceCompletion < SW2DKS_TIER_3_THRESHOLD) return 2;
    if (raceCompletion < SW2DKS_TIER_4_THRESHOLD) return 3;
    if (raceCompletion < SW2DKS_TIER_5_THRESHOLD) return 4;
    return 5;
}

u8 GetSW2DKSRacePercentageMusicTier() {
    return sw2dksLoaded ? sw2dksMusicTier : 0;
}

static void ResetSW2DKSMusicState() {
    sw2dksMusicTier = 0;
    sw2dksLoaded = false;
    sw2dksLoadedInitialized = false;
    sw2dksTier5ReloadPending = false;
}

static u32 GetActiveSinglePlayerSoundId() {
    Audio::SinglePlayer* singlePlayer = Audio::SinglePlayer::sInstance;
    if (singlePlayer == nullptr || singlePlayer->activeHandle == nullptr ||
        singlePlayer->activeHandle->basicSound == nullptr) {
        return 0;
    }
    return singlePlayer->activeHandle->basicSound->soundId;
}

static void ReloadMainRaceMusic(u32 soundId) {
    if (soundId == 0) return;

    Audio::SinglePlayer* singlePlayer = Audio::SinglePlayer::sInstance;
    if (singlePlayer == nullptr) return;

    singlePlayer->canNotCancel = false;
    singlePlayer->canNotPrepareOther = false;
    singlePlayer->PrepareSound(soundId, false);
    singlePlayer->StopSound();
    singlePlayer->PlayPreparedSound(0);
    singlePlayer->StopInactiveSounds();
}

static void ReloadActiveRaceMusic() {
    ReloadMainRaceMusic(GetActiveSinglePlayerSoundId());
}

static bool UpdatePendingTier5Reload(const Audio::RaceMgr& raceAudioMgr) {
    if (!sw2dksTier5ReloadPending || raceAudioMgr.raceState != RACE_STATE_FINAL_LAP_MUSIC) return false;

    sw2dksTier5ReloadPending = false;
    ReloadActiveRaceMusic();
    return true;
}

static u8 GetHudSlotIdForPlayer(u8 playerId) {
    const Racedata* racedata = Racedata::sInstance;
    if (racedata == nullptr) return 0;
    return racedata->GetHudSlotId(playerId);
}

static void PlaySW2DKSNormalLapJingle(u8 playerId) {
    Audio::RaceRSARPlayer* rsarPlayer = static_cast<Audio::RaceRSARPlayer*>(Audio::RSARPlayer::sInstance);
    if (rsarPlayer == nullptr) return;

    const u8 hudSlotId = GetHudSlotIdForPlayer(playerId);
    rsarPlayer->PlaySound(SOUND_ID_NORMAL_LAP, hudSlotId);
}

static void PlaySW2DKSTierChangeJingle(Audio::RaceMgr& raceAudioMgr, u8 tier, u8 playerId) {
    if (tier >= 5) {
        if (raceAudioMgr.raceState == RACE_STATE_FINAL_LAP_MUSIC) {
            raceAudioMgr.raceState = Audio::RACE_STATE_NORMAL;
        }
        raceAudioMgr.SetRaceState(Audio::RACE_STATE_FAST);
        return;
    }

    PlaySW2DKSNormalLapJingle(playerId);
}

void UpdateSW2DKSRacePercentageMusic() {
    if (!IsCTMusicEnabled()) {
        ResetSW2DKSMusicState();
        return;
    }

    Audio::RaceMgr* raceAudioMgr = Audio::RaceMgr::sInstance;
    const Raceinfo* raceInfo = Raceinfo::sInstance;
    if (raceAudioMgr == nullptr || raceInfo == nullptr || raceInfo->players == nullptr) return;
    if (raceInfo->timerMgr == nullptr || !raceInfo->timerMgr->hasRaceStarted) {
        ResetSW2DKSMusicState();
        return;
    }

    if (!sw2dksLoadedInitialized) {
        sw2dksLoaded = IsSW2DKSLoaded();
        sw2dksLoadedInitialized = true;
    }
    if (!sw2dksLoaded) {
        sw2dksMusicTier = 0;
        return;
    }

    if (UpdatePendingTier5Reload(*raceAudioMgr)) return;

    const u8 playerId = raceAudioMgr->playerIdFirstLocalPlayer;
    if (playerId >= 12 || raceInfo->players[playerId] == nullptr) return;

    u8 nextTier = GetSW2DKSMusicTier(raceInfo->players[playerId]->raceCompletion);
    if (nextTier < sw2dksMusicTier) nextTier = sw2dksMusicTier;
    if (nextTier == sw2dksMusicTier) return;

    if (nextTier != 0 && !HasSW2DKSTieredBRSTM(nextTier)) return;

    sw2dksMusicTier = nextTier;
    sw2dksTier5ReloadPending = false;
    if (nextTier == 0) {
        ReloadActiveRaceMusic();
    } else if (nextTier < 5) {
        PlaySW2DKSTierChangeJingle(*raceAudioMgr, nextTier, playerId);
        ReloadActiveRaceMusic();
    } else {
        sw2dksTier5ReloadPending = true;
        PlaySW2DKSTierChangeJingle(*raceAudioMgr, nextTier, playerId);
    }
}

static RaceLoadHook ResetSW2DKSMusicStateOnRaceLoad(ResetSW2DKSMusicState);

}
}
