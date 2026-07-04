#include <kamek.hpp>
#include <MarioKartWii/Audio/RaceMgr.hpp>
#include <MarioKartWii/Audio/RSARPlayer.hpp>
#include <MarioKartWii/Audio/SinglePlayer.hpp>
#include <MarioKartWii/Race/RaceData.hpp>
#include <MarioKartWii/Race/RaceInfo/RaceInfo.hpp>
#include <Settings/Settings.hpp>
#include <SlotExpansion/CupsConfig.hpp>
#include <core/nw4r/ut/BinaryFileFormat.hpp>
#include <core/nw4r/snd/DVDSoundArchive.hpp>
#include <core/nw4r/snd/BasicSound.hpp>

namespace Pulsar {
namespace Sound {

static const float SW2RR_NORMAL_THRESHOLD = 1.08975f;
static const float SW2RR_TIER_1_THRESHOLD = 1.16675f;
static const float SW2RR_TIER_2_THRESHOLD = 1.480f;
static const Audio::RaceState RACE_STATE_FINAL_LAP_MUSIC = static_cast<Audio::RaceState>(0x6);

static u8 sw2rrMusicTier = 0;
static bool sw2rrLoaded = false;
static bool sw2rrLoadedInitialized = false;
static bool sw2rrTier3ReloadPending = false;
static char sw2rrFanfarePath[0x100];

bool HasSW2RRTieredBRSTM(u8 tier);

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

static bool IsSW2RRFileName(const char* fileName) {
    return StringsEqual(fileName, "sw2RR") || StringsEqual(fileName, "SW2RR") ||
        StringsEqual(fileName, "sw2RR.szs") || StringsEqual(fileName, "SW2RR.szs");
}

bool IsSW2RRLoaded() {
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

static const char* GetSW2RRFanfareName(const char* extFilePath) {
    if (extFilePath == nullptr) return nullptr;

    const char* fileName = extFilePath;
    for (const char* cursor = extFilePath; *cursor != '\0'; ++cursor) {
        if (*cursor == '/') fileName = cursor + 1;
    }

    if (StringsEqual(fileName, "o_FanfareGP1_32.brstm")) return "o_FanfareSW2RRGP1_32.brstm";
    if (StringsEqual(fileName, "o_FanfareGP2_32.brstm")) return "o_FanfareSW2RRGP2_32.brstm";
    if (StringsEqual(fileName, "o_FanfareGPdame_32.brstm")) return "o_FanfareSW2RRGPdame_32.brstm";
    return nullptr;
}

bool ResolveSW2RRFanfarePath(const nw4r::snd::DVDSoundArchive* archive, const char*& extFilePath) {
    const char* sw2rrFanfare = GetSW2RRFanfareName(extFilePath);
    if (archive == nullptr || sw2rrFanfare == nullptr || !IsSW2RRLoaded()) return false;

    snprintf(sw2rrFanfarePath, sizeof(sw2rrFanfarePath), "%sstrm/%s", archive->extFileRoot, sw2rrFanfare);
    extFilePath = sw2rrFanfarePath;
    return true;
}

static u8 GetSW2RRMusicTier(float raceCompletion) {
    if (raceCompletion < SW2RR_NORMAL_THRESHOLD) return 0;
    if (raceCompletion < SW2RR_TIER_1_THRESHOLD) return 1;
    if (raceCompletion < SW2RR_TIER_2_THRESHOLD) return 2;
    return 3;
}

u8 GetSW2RRRacePercentageMusicTier() {
    return sw2rrLoaded ? sw2rrMusicTier : 0;
}

static void ResetSW2RRMusicState() {
    sw2rrMusicTier = 0;
    sw2rrLoaded = false;
    sw2rrLoadedInitialized = false;
    sw2rrTier3ReloadPending = false;
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

static bool UpdatePendingTier3Reload(const Audio::RaceMgr& raceAudioMgr) {
    if (!sw2rrTier3ReloadPending || raceAudioMgr.raceState != RACE_STATE_FINAL_LAP_MUSIC) return false;

    sw2rrTier3ReloadPending = false;
    ReloadActiveRaceMusic();
    return true;
}

static u8 GetHudSlotIdForPlayer(u8 playerId) {
    const Racedata* racedata = Racedata::sInstance;
    if (racedata == nullptr) return 0;
    return racedata->GetHudSlotId(playerId);
}

static void PlaySW2RRTierChangeJingle(Audio::RaceMgr& raceAudioMgr, u8 tier, u8 playerId) {
    if (tier >= 3) {
        if (raceAudioMgr.raceState == RACE_STATE_FINAL_LAP_MUSIC) {
            raceAudioMgr.raceState = Audio::RACE_STATE_NORMAL;
        }
        raceAudioMgr.SetRaceState(Audio::RACE_STATE_FAST);
        return;
    }

    Audio::RaceRSARPlayer* rsarPlayer = static_cast<Audio::RaceRSARPlayer*>(Audio::RSARPlayer::sInstance);
    if (rsarPlayer == nullptr) return;

    const u8 hudSlotId = GetHudSlotIdForPlayer(playerId);
    rsarPlayer->PlaySound(SOUND_ID_NORMAL_LAP, hudSlotId);
}

void UpdateSW2RRRacePercentageMusic() {
    if (!IsCTMusicEnabled()) {
        ResetSW2RRMusicState();
        return;
    }

    Audio::RaceMgr* raceAudioMgr = Audio::RaceMgr::sInstance;
    const Raceinfo* raceInfo = Raceinfo::sInstance;
    if (raceAudioMgr == nullptr || raceInfo == nullptr || raceInfo->players == nullptr) return;
    if (raceInfo->timerMgr == nullptr || !raceInfo->timerMgr->hasRaceStarted) {
        ResetSW2RRMusicState();
        return;
    }

    if (!sw2rrLoadedInitialized) {
        sw2rrLoaded = IsSW2RRLoaded();
        sw2rrLoadedInitialized = true;
    }
    if (!sw2rrLoaded) {
        sw2rrMusicTier = 0;
        return;
    }

    if (UpdatePendingTier3Reload(*raceAudioMgr)) return;

    const u8 playerId = raceAudioMgr->playerIdFirstLocalPlayer;
    if (playerId >= 12 || raceInfo->players[playerId] == nullptr) return;

    u8 nextTier = GetSW2RRMusicTier(raceInfo->players[playerId]->raceCompletion);
    if (nextTier < sw2rrMusicTier) nextTier = sw2rrMusicTier;
    if (nextTier == sw2rrMusicTier) return;

    if (nextTier != 0 && !HasSW2RRTieredBRSTM(nextTier)) return;

    sw2rrMusicTier = nextTier;
    sw2rrTier3ReloadPending = false;
    if (nextTier == 0) {
        ReloadActiveRaceMusic();
    } else if (nextTier < 3) {
        PlaySW2RRTierChangeJingle(*raceAudioMgr, nextTier, playerId);
        ReloadActiveRaceMusic();
    } else {
        sw2rrTier3ReloadPending = true;
        PlaySW2RRTierChangeJingle(*raceAudioMgr, nextTier, playerId);
    }
}

static RaceLoadHook ResetSW2RRMusicStateOnRaceLoad(ResetSW2RRMusicState);

}
}
