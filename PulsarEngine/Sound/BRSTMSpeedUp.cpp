#include <kamek.hpp>
#include <runtimeWrite.hpp>
#include <MarioKartWii/Race/RaceInfo/RaceInfo.hpp>
#include <MarioKartWii/Audio/RSARPlayer.hpp>
#include <MarioKartWii/Audio/RaceMgr.hpp>
#include <MarioKartWii/Audio/Actors/KartActor.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <MarioKartWii/UI/Ctrl/CtrlRace/CtrlRaceGhostDiffTime.hpp>
#include <Settings/Settings.hpp>

/*Music speedup:
When the player reaches the final lap (if the track has >1 laps) and if the setting is set, the music will
speedup instead of transitioning to the _f file. The jingle will still play.
*/

namespace Pulsar {
namespace Sound {

using namespace nw4r;
static const Audio::RaceState RACE_STATE_FINAL_LAP_JINGLE = static_cast<Audio::RaceState>(0x6);
static const u8 INVALID_HUD_SLOT_ID = 0xFF;
static u8 finalLapSpeedupHudSlot = INVALID_HUD_SLOT_ID;

static void MusicSpeedup(Audio::RaceRSARPlayer* rsarSoundPlayer, u32 jingle, u8 hudSlotId) {
    u8 isSpeedUp = Settings::Mgr::Get().GetUserSettingValue(Settings::SETTINGSTYPE_SOUND, RADIO_MUSICSPEEDUP);
    Audio::RaceMgr* raceAudioMgr = Audio::RaceMgr::sInstance;
    Raceinfo* raceInfo = Raceinfo::sInstance;
    const u8 maxLap = raceAudioMgr->maxLap;
    const u8 curLap = raceAudioMgr->lap;
    const RacedataSettings& raceDataSettings = Racedata::sInstance->racesScenario.settings;
    RaceinfoPlayer* hudPlayer = nullptr;
    if (raceInfo != nullptr && raceInfo->players != nullptr) {
        hudPlayer = raceInfo->players[raceDataSettings.hudPlayerIds[hudSlotId]];
    }
    const bool isNewLapTrigger = (maxLap != curLap);
    const bool hudPlayerReachedFinalLap = hudPlayer != nullptr && hudPlayer->currentLap >= raceDataSettings.lapCount;
    const bool isFirstFinalLapTrigger = isNewLapTrigger && raceAudioMgr->raceState == Audio::RACE_STATE_NORMAL && hudPlayerReachedFinalLap;
    if (raceAudioMgr->raceState == Audio::RACE_STATE_NORMAL && maxLap != raceDataSettings.lapCount) {
        finalLapSpeedupHudSlot = INVALID_HUD_SLOT_ID;
    }
    if (maxLap == 1) return;
    if (maxLap == raceDataSettings.lapCount) {
        register Audio::KartActor* kartActor;
        asm(mr kartActor, r29;);
        snd::detail::BasicSound& sound = kartActor->soundArchivePlayer->soundPlayerArray[0].soundList.GetFront();
        if (isSpeedUp == SPEEDUP_ENABLED || sound.soundId == SOUND_ID_GALAXY_COLOSSEUM) {
            if (isFirstFinalLapTrigger) {
                finalLapSpeedupHudSlot = hudSlotId;
                raceAudioMgr->raceState = RACE_STATE_FINAL_LAP_JINGLE;
                rsarSoundPlayer->PlaySound(SOUND_ID_FINAL_LAP, hudSlotId);
            }
            if (finalLapSpeedupHudSlot == hudSlotId && raceInfo != nullptr && raceInfo->players != nullptr) {
                const Timer& raceTimer = raceInfo->timerMgr->timers[0];
                const Timer& playerTimer = raceInfo->players[raceDataSettings.hudPlayerIds[finalLapSpeedupHudSlot]]->lapSplits[maxLap - 2];
                const Timer difference = CtrlRaceGhostDiffTime::SubtractTimers(raceTimer, playerTimer);
                if (difference.minutes < 1 && difference.seconds < 5) {
                    sound.ambientParam.pitch += 0.0002f;
                }
            }
        } else if (isNewLapTrigger && (raceAudioMgr->raceState == Audio::RACE_STATE_NORMAL || raceAudioMgr->raceState == RACE_STATE_FINAL_LAP_JINGLE)) {
            raceAudioMgr->SetRaceState(Audio::RACE_STATE_FAST);
        }
    } else if (isNewLapTrigger) {
        rsarSoundPlayer->PlaySound(SOUND_ID_NORMAL_LAP, hudSlotId);
    }
    return;
}
kmCall(0x8070b2f8, MusicSpeedup);
kmWrite32(0x8070b2c0, 0x60000000);
kmWrite32(0x8070b2d4, 0x60000000);

kmRuntimeUse(0x807125d4);
static void RaceSoundManager_CheckRaceState(void* raceSoundManager) {
    const Racedata* racedata = Racedata::sInstance;
    const RKNet::Controller* rknet = RKNet::Controller::sInstance;
    Audio::RaceMgr* raceAudioMgr = Audio::RaceMgr::sInstance;
    if (racedata != nullptr && rknet != nullptr && raceAudioMgr != nullptr && rknet->roomType != RKNet::ROOMTYPE_NONE) {
        const RacedataSettings& settings = racedata->racesScenario.settings;
        if (settings.lapCount == 1 && raceAudioMgr->raceState == Audio::RACE_STATE_NORMAL) {
            const u8 localPlayerId = raceAudioMgr->playerIdFirstLocalPlayer;
            Raceinfo* raceInfo = Raceinfo::sInstance;
            if (raceInfo != nullptr && raceInfo->players != nullptr && localPlayerId < 12) {
                RaceinfoPlayer* player = raceInfo->players[localPlayerId];
                if (player != nullptr && player->raceFinishTime != nullptr && player->raceFinishTime->isActive) {
                    raceAudioMgr->raceState = RACE_STATE_FINAL_LAP_JINGLE;
                }
            }
        }
    }

    reinterpret_cast<void (*)(void*)>(kmRuntimeAddr(0x807125d4))(raceSoundManager);
}
kmCall(0x80710f84, RaceSoundManager_CheckRaceState);

}  // namespace Sound
}  // namespace Pulsar
