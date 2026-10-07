#include <kamek.hpp>
#include <MarioKartWii/Audio/RaceMgr.hpp>
#include <MarioKartWii/Audio/RSARPlayer.hpp>
#include <MarioKartWii/Audio/SinglePlayer.hpp>
#include <MarioKartWii/KMP/KMPManager.hpp>
#include <MarioKartWii/Lakitu/LakituPlayer.hpp>
#include <MarioKartWii/3D/Model/ModelDirector.hpp>
#include <MarioKartWii/Race/RaceData.hpp>
#include <MarioKartWii/Race/RaceInfo/RaceInfo.hpp>
#include <MarioKartWii/RKNet/PacketMgr.hpp>
#include <Network/PacketExpansion.hpp>
#include <Settings/Settings.hpp>
#include <core/nw4r/snd/BasicSound.hpp>

namespace Pulsar {
namespace Sound {

static const Audio::RaceState RACE_STATE_FINAL_LAP_MUSIC = static_cast<Audio::RaceState>(0x6);

static u16 checkpointMusicIndex = 1;
static u8 pendingCheckpointMusic = 0;
static u16 checkpointFinalLapPlayers = 0;
static u32 triggeredMusicCheckpoints[12][8];
static u16 pendingCheckpointLapSigns[12];

bool HasCheckpointBRSTM(u16 index);

u16 GetCheckpointMusicIndex() {
    return checkpointMusicIndex;
}

bool IsCheckpointFinalLap() {
    return (checkpointFinalLapPlayers & (1u << Audio::RaceMgr::sInstance->playerIdFirstLocalPlayer)) != 0;
}

static void ResetCheckpointMusicState() {
    checkpointMusicIndex = 1;
    pendingCheckpointMusic = 0;
    checkpointFinalLapPlayers = 0;
    memset(triggeredMusicCheckpoints, 0, sizeof(triggeredMusicCheckpoints));
    memset(pendingCheckpointLapSigns, 0, sizeof(pendingCheckpointLapSigns));
}

static u32 GetActiveSinglePlayerSoundId() {
    Audio::SinglePlayer *singlePlayer = Audio::SinglePlayer::sInstance;
    if (singlePlayer == nullptr || singlePlayer->activeHandle == nullptr || singlePlayer->activeHandle->basicSound == nullptr) {
        return 0;
    }
    return singlePlayer->activeHandle->basicSound->soundId;
}

static void ReloadMainRaceMusic(u32 soundId) {
    if (soundId == 0)
        return;

    Audio::SinglePlayer *singlePlayer = Audio::SinglePlayer::sInstance;
    if (singlePlayer == nullptr)
        return;

    singlePlayer->canNotCancel = false;
    singlePlayer->canNotPrepareOther = false;
    singlePlayer->StopInactiveSounds();
    singlePlayer->PrepareSound(soundId, false);
    singlePlayer->StopSound();
    singlePlayer->PlayPreparedSound(0);
    singlePlayer->StopInactiveSounds();
}

static void ReloadActiveRaceMusic() {
    ReloadMainRaceMusic(GetActiveSinglePlayerSoundId());
}

KMP::Holder<CKPT> *UpdateMusicCheckpoint(RaceinfoPlayer *player, u16 cpId, bool isRemote, float completion) {
    const u16 previous = player->checkpoint;
    KMP::Holder<CKPT> *checkpoint = player->UpdateCheckPoint(cpId, isRemote, completion);
    const KMP::CKPTSection &section = *KMP::Manager::sInstance->ckptSection;
    if (section.rawKMPBlock->header.flag == 0x5252 && Raceinfo::sInstance->timerMgr->hasRaceStarted && !(player->stateFlags & 0x30)) {
        const KMP::Holder<CKPT> &previousCheckpoint = *section.holdersArray[previous];
        for (u16 i = 0; i < previousCheckpoint.nextCKPTCount; ++i) {
            if (previousCheckpoint.next[i].holder != checkpoint)
                continue;
            u32 &triggered = triggeredMusicCheckpoints[player->id][cpId / 32];
            if (triggered & (1u << (cpId % 32)))
                break;
            triggered |= 1u << (cpId % 32);
            // RR CKPT trailer: one byte per checkpoint; bit 0 advances music, bit 1 starts the final lap, bit 2 plays the lap jingle.
            const u8 action = reinterpret_cast<const u8 *>(&section.rawKMPBlock->entries)[section.pointCount * sizeof(CKPT) + cpId];
            if ((action & 2) && !isRemote) {
                // Preserve key checkpoint progress. The normal EndLap path will finish this player at the next lap checkpoint.
                player->currentLap = Racedata::sInstance->racesScenario.settings.lapCount;
                player->maxLap = player->currentLap;
                checkpointFinalLapPlayers |= 1u << player->id;
                pendingCheckpointLapSigns[player->id] = 0xffff;
            }
            if (player->id == Audio::RaceMgr::sInstance->playerIdFirstLocalPlayer) {
                pendingCheckpointMusic = action;
            }
            break;
        }
    }
    return checkpoint;
}
kmCall(0x805354d8, UpdateMusicCheckpoint);

void SendCheckpointFinalLap(RKNet::PacketHolder<Network::PulRH2> &holder, const Network::PulRH2 *packet, u32 len) {
    holder.Copy(packet, len);
    holder.packet->checkpointFinalLapPlayers = 0;
    const RacedataScenario &scenario = Racedata::sInstance->racesScenario;
    for (u8 i = 0; i < scenario.playerCount; ++i) {
        if (scenario.players[i].playerType == PLAYER_REAL_LOCAL)
            holder.packet->checkpointFinalLapPlayers |= checkpointFinalLapPlayers & (1u << i);
    }
    if (holder.packet->checkpointFinalLapPlayers != 0)
        holder.packetSize = sizeof(Network::PulRH2);
}
kmCall(0x80653c88, SendCheckpointFinalLap);

RKNet::RACEHEADER2Packet &ReceiveCheckpointFinalLap(RKNet::PacketMgr &mgr, u8 playerId) {
    RKNet::RACEHEADER2Packet &packet = mgr.GetRH2(playerId);
    const RKNet::Controller &controller = *RKNet::Controller::sInstance;
    const u8 aid = controller.aidsBelongingToPlayerIds[playerId];
    const RKNet::PacketHolder<Network::PulRH2> &holder = *controller.splitReceivedRACEPackets[controller.lastReceivedBufferUsed[aid][2]][aid]->GetPacketHolder<Network::PulRH2>();
    RaceinfoPlayer &player = *Raceinfo::sInstance->players[playerId];
    // Only the owner's RH2 can advance a remote racer; the native RH2 finish-time path remains authoritative.
    if (holder.packetSize >= sizeof(Network::PulRH2) && (holder.packet->checkpointFinalLapPlayers & (1u << playerId)) && !(checkpointFinalLapPlayers & (1u << playerId))
      && !(player.stateFlags & 0x30)) {
        const u8 lapCount = Racedata::sInstance->racesScenario.settings.lapCount;
        player.raceCompletion += lapCount - player.currentLap;
        player.raceCompletionMax = player.raceCompletion;
        player.currentLap = lapCount;
        player.maxLap = lapCount;
        checkpointFinalLapPlayers |= 1u << playerId;
    }
    return packet;
}
kmCall(0x8053e4b8, ReceiveCheckpointFinalLap);

void UpdateCheckpointMusic() {
    const u8 action = pendingCheckpointMusic;
    pendingCheckpointMusic = 0;
    if (action == 0)
        return;

    Audio::RaceMgr &raceAudioMgr = *Audio::RaceMgr::sInstance;
    if (action & 2) {
        if (raceAudioMgr.raceState == RACE_STATE_FINAL_LAP_MUSIC)
            raceAudioMgr.raceState = Audio::RACE_STATE_NORMAL;
        if (raceAudioMgr.raceState == Audio::RACE_STATE_NORMAL)
            raceAudioMgr.SetRaceState(Audio::RACE_STATE_FAST);
    } else if ((action & 1) && Settings::Mgr::Get().GetSettingValue(Pulsar::Settings::SETTING_CTMUSIC) == CTMUSIC_ENABLED && HasCheckpointBRSTM(checkpointMusicIndex + 1)) {
        ++checkpointMusicIndex;
        pendingCheckpointLapSigns[raceAudioMgr.playerIdFirstLocalPlayer] = checkpointMusicIndex;
        if (action & 4) {
            static_cast<Audio::RaceRSARPlayer *>(Audio::RSARPlayer::sInstance)->PlaySound(SOUND_ID_NORMAL_LAP, Racedata::sInstance->GetHudSlotId(raceAudioMgr.playerIdFirstLocalPlayer));
        }
        ReloadActiveRaceMusic();
    }
}

void EnableCheckpointLapSign(Lakitu::EnableDisplayLapAction *action) {
    action->Lakitu::EnableDisplayLapAction::EnableAction();
    if (pendingCheckpointLapSigns[action->playerId] != 0)
        action->isEnabled = true;
}
kmWritePointer(0x808c989c, EnableCheckpointLapSign);

void StartCheckpointLapSign(Lakitu::Player *lakitu) {
    lakitu->Lakitu::Player::OnStartLap();
    const u8 playerId = lakitu->GetPlayerIdx();
    const u16 sign = pendingCheckpointLapSigns[playerId];
    pendingCheckpointLapSigns[playerId] = 0;
    if (sign == 0xffff) {
        lakitu->SetModelsVisibility(true, 4, 7);
    } else if (sign != 0) {
        // Music stage numbers must not select the final-lap model merely because they equal the race's lap count.
        lakitu->SetModelsVisibility(true, 3, 7);
        lakitu->lapModel->modelTransformator->GetAnmHolderByType(ANMTYPE_TEXPAT)->UpdateRateAndSetFrame(static_cast<float>(sign - 2));
    }
}
kmWritePointer(0x808c96e0, StartCheckpointLapSign);

static RaceLoadHook ResetCheckpointMusicStateOnRaceLoad(ResetCheckpointMusicState);

}  // namespace Sound
}  // namespace Pulsar
