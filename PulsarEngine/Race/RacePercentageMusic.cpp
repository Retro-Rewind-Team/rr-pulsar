#include <kamek.hpp>
#include <MarioKartWii/Audio/RaceMgr.hpp>
#include <MarioKartWii/Audio/RSARPlayer.hpp>
#include <MarioKartWii/Audio/SinglePlayer.hpp>
#include <MarioKartWii/Race/RaceData.hpp>
#include <MarioKartWii/Race/RaceInfo/RaceInfo.hpp>
#include <SlotExpansion/CupsConfig.hpp>
#include <core/nw4r/snd/BasicSound.hpp>

namespace Pulsar {
namespace Sound {

static const float SW2RR_NORMAL_THRESHOLD = 1.090f;
static const float SW2RR_TIER_1_THRESHOLD = 1.168f;
static const float SW2RR_TIER_2_THRESHOLD = 1.480f;

static u8 sw2rrMusicTier = 0;

bool HasSW2RRTieredBRSTM(u8 tier);

static bool StringsEqual(const char* lhs, const char* rhs) {
    if (lhs == nullptr || rhs == nullptr) return false;
    while (*lhs != '\0' && *rhs != '\0') {
        if (*lhs != *rhs) return false;
        ++lhs;
        ++rhs;
    }
    return *lhs == *rhs;
}

static bool IsSW2RRFileName(const char* fileName) {
    return StringsEqual(fileName, "sw2RR") || StringsEqual(fileName, "SW2RR") ||
        StringsEqual(fileName, "sw2RR.szs") || StringsEqual(fileName, "SW2RR.szs");
}

static bool IsSW2RRLoaded() {
    const CupsConfig* cupsConfig = CupsConfig::sInstance;
    if (cupsConfig == nullptr) return false;

    const PulsarId track = cupsConfig->GetWinning();
    if (CupsConfig::IsReg(track)) return false;

    const u8 variantIdx = cupsConfig->GetCurVariantIdx();
    const char* fileName = cupsConfig->GetFileName(track, variantIdx);
    if (fileName == nullptr || fileName[0] == '\0') {
        fileName = cupsConfig->GetFileName(track, 0);
    }
    return IsSW2RRFileName(fileName);
}

static u8 GetSW2RRMusicTier(float raceCompletion) {
    if (raceCompletion < SW2RR_NORMAL_THRESHOLD) return 0;
    if (raceCompletion < SW2RR_TIER_1_THRESHOLD) return 1;
    if (raceCompletion < SW2RR_TIER_2_THRESHOLD) return 2;
    return 3;
}

u8 GetSW2RRRacePercentageMusicTier() {
    return IsSW2RRLoaded() ? sw2rrMusicTier : 0;
}

static void ReloadMainRaceMusic() {
    Audio::SinglePlayer* singlePlayer = Audio::SinglePlayer::sInstance;
    if (singlePlayer == nullptr) return;

    u32 soundId = 0;
    if (singlePlayer->activeHandle != nullptr && singlePlayer->activeHandle->basicSound != nullptr) {
        soundId = singlePlayer->activeHandle->basicSound->soundId;
    }
    if (soundId == 0) return;

    singlePlayer->canNotCancel = false;
    singlePlayer->canNotPrepareOther = false;
    singlePlayer->PrepareSound(soundId, false);
    singlePlayer->StopSound();
    singlePlayer->PlayPreparedSound(0);
    singlePlayer->StopInactiveSounds();

}

static u8 GetHudSlotIdForPlayer(u8 playerId) {
    const Racedata* racedata = Racedata::sInstance;
    if (racedata == nullptr) return 0;
    return racedata->GetHudSlotId(playerId);
}

static void PlaySW2RRTierChangeJingle(Audio::RaceMgr& raceAudioMgr, u8 tier, u8 playerId) {
    if (tier >= 3) {
        raceAudioMgr.SetRaceState(Audio::RACE_STATE_FAST);
        return;
    }

    Audio::RaceRSARPlayer* rsarPlayer = static_cast<Audio::RaceRSARPlayer*>(Audio::RSARPlayer::sInstance);
    if (rsarPlayer == nullptr) return;

    const u8 hudSlotId = GetHudSlotIdForPlayer(playerId);
    rsarPlayer->PlaySound(SOUND_ID_NORMAL_LAP, hudSlotId);
}

void UpdateSW2RRRacePercentageMusic() {
    if (!IsSW2RRLoaded()) {
        sw2rrMusicTier = 0;
        return;
    }

    Audio::RaceMgr* raceAudioMgr = Audio::RaceMgr::sInstance;
    const Raceinfo* raceInfo = Raceinfo::sInstance;
    if (raceAudioMgr == nullptr || raceInfo == nullptr || raceInfo->players == nullptr) return;
    if (raceInfo->timerMgr == nullptr || !raceInfo->timerMgr->hasRaceStarted) return;

    const u8 playerId = raceAudioMgr->playerIdFirstLocalPlayer;
    if (playerId >= 12 || raceInfo->players[playerId] == nullptr) return;

    const u8 nextTier = GetSW2RRMusicTier(raceInfo->players[playerId]->raceCompletion);
    if (nextTier == sw2rrMusicTier) return;

    if (nextTier != 0 && !HasSW2RRTieredBRSTM(nextTier)) {
        return;
    }

    sw2rrMusicTier = nextTier;
    if (nextTier == 0) {
        ReloadMainRaceMusic();
    } else if (nextTier < 3) {
        PlaySW2RRTierChangeJingle(*raceAudioMgr, nextTier, playerId);
        ReloadMainRaceMusic();
    } else {
        PlaySW2RRTierChangeJingle(*raceAudioMgr, nextTier, playerId);
    }
}

}  // namespace Sound
}  // namespace Pulsar
