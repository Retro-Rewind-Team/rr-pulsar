#include <Network/FroomCPUs.hpp>
#include <Network/PacketExpansion.hpp>
#include <PulsarSystem.hpp>
#include <Driver/CustomCharacters.hpp>
#include <Race/CustomCharacters.hpp>
#include <MarioKartWii/GlobalFunctions.hpp>
#include <MarioKartWii/Race/RaceData.hpp>
#include <MarioKartWii/System/Random.hpp>
#include <MarioKartWii/RKNet/PacketMgr.hpp>
#include <MarioKartWii/Race/RaceInfo/RaceInfo.hpp>
#include <MarioKartWii/Kart/KartFunctions.hpp>
#include <MarioKartWii/AI/AIManager.hpp>
#include <MarioKartWii/Race/RaceBalloon.hpp>
#include <Gamemodes/BattleRoyale/BattleRoyale.hpp>
#include <UI/ExtendedTeamSelect/ExtendedTeamManager.hpp>
#include <core/rvl/DWC/DWCTransport.hpp>

namespace Pulsar {
namespace FroomCPUs {

Roster roster;
u8 playerSlots[12] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
u32 lastSelectId;

static_assert(sizeof(CPUHeader) <= Network::PulRH1SizeFroomCPUs, "CPU RH1 buffer capacity");

RKNet::RACEDATAPacket racePackets[12];
Progress progress[12];
u32 packetFrames[12];
u16 validPackets;

void WriteSELECT(Roster &dest, u32 selectId) {
    memset(&dest, 0, sizeof(dest));
    if (!System::sInstance->IsContext(PULSAR_FROOMCPUS))
        return;
    const RKNet::Controller *controller = RKNet::Controller::sInstance;
    const RKNet::ControllerSub &sub = controller->subs[controller->currentSub];
    if (sub.localAid != sub.hostAid)
        return;

    if (selectId != lastSelectId) {
        // Zero CPUs is a valid initial field; GP knockout must not refill it later.
        if (roster.count == 0 && (lastSelectId == 0 || !System::sInstance->IsContext(PULSAR_MODE_KO))) {
            memset(&roster, 0, sizeof(roster));
            Random random;
            roster.count = 12 - sub.playerCount;
            for (u32 i = 0; i < roster.count; ++i) {
                Entry &entry = roster.entries[i];
                entry.character = random.NextLimited(24);
                entry.kart = CharacterIdToKartIdByIdx(static_cast<CharacterId>(entry.character), random.NextLimited(12));
                entry.previousPosition = sub.playerCount + i + 1;
                entry.team = i & 1;
                u32 slots = 0;
                for (u32 slot = 0; slot <= Driver::MAX_CUSTOM_CHARACTER_SLOTS; ++slot) {
                    if (Driver::characterTables[entry.character][slot])
                        ++slots;
                }
                if (slots != 0) {
                    u32 selected = random.NextLimited(slots);
                    for (u32 slot = 0; slot <= Driver::MAX_CUSTOM_CHARACTER_SLOTS; ++slot) {
                        if (Driver::characterTables[entry.character][slot]) {
                            if (selected == 0) {
                                entry.characterSlot = slot;
                                break;
                            }
                            --selected;
                        }
                    }
                }
            }
        }
        lastSelectId = selectId;
    }
    memcpy(&dest, &roster, sizeof(dest));
}

void ReadSELECT(const Roster &src, u32 aid) {
    if (!System::sInstance->IsContext(PULSAR_FROOMCPUS))
        return;
    const RKNet::Controller *controller = RKNet::Controller::sInstance;
    const RKNet::ControllerSub &sub = controller->subs[controller->currentSub];
    if (aid != sub.hostAid || sub.localAid == sub.hostAid || src.count > 11)
        return;
    for (u32 i = 0; i < src.count; ++i) {
        if (src.entries[i].character >= 24 || src.entries[i].kart > PHANTOM || src.entries[i].characterSlot > Driver::MAX_CUSTOM_CHARACTER_SLOTS
          || GetCharacterWeightClass(static_cast<CharacterId>(src.entries[i].character)) != GetKartWeightClass(static_cast<KartId>(src.entries[i].kart)))
            return;
    }
    memcpy(&roster, &src, sizeof(roster));
}

void PrepareRace(RacedataScenario &scenario) {
    memset(playerSlots, 0xFF, sizeof(playerSlots));
    RKNet::Controller *controller = RKNet::Controller::sInstance;
    if (controller == nullptr || (controller->roomType != RKNet::ROOMTYPE_FROOM_HOST && controller->roomType != RKNet::ROOMTYPE_FROOM_NONHOST))
        return;
    if (scenario.settings.gametype == GAMETYPE_ONLINE_SPECTATOR && !System::sInstance->IsContext(PULSAR_VR)
      && (scenario.settings.gamemode == MODE_PRIVATE_VS || scenario.settings.gamemode == MODE_PUBLIC_VS)) {
        const RKNet::ControllerSub &sub = controller->subs[controller->currentSub];
        const RKNet::PacketHolder<Network::PulRH1> *holder = controller->GetReceivedPacketHolder<Network::PulRH1>(sub.hostAid);
        if (holder != nullptr && holder->packetSize >= Network::PulRH1SizeFroomCPUs) {
            System::sInstance->context2 |= 1 << PULSAR_FROOMCPUS;
            System::sInstance->netMgr.hostContext2 |= 1 << PULSAR_FROOMCPUS;
            ReadSELECT(holder->packet->cpuRoster, sub.hostAid);
        }
    }
    if (!System::sInstance->IsContext(PULSAR_FROOMCPUS))
        return;
    validPackets = 0;
    memset(racePackets, 0, sizeof(racePackets));
    memset(progress, 0, sizeof(progress));
    memset(packetFrames, 0, sizeof(packetFrames));
    for (u32 playerId = 0; playerId < 12; ++playerId) {
        if (scenario.players[playerId].playerType == PLAYER_CPU)
            scenario.players[playerId].playerType = PLAYER_NONE;
    }
    const RKNet::ControllerSub &sub = controller->subs[controller->currentSub];
    u32 playerId = 0;
    while (playerId < 12 && scenario.players[playerId].playerType != PLAYER_NONE) ++playerId;
    for (u32 i = 0; i < roster.count && playerId < 12; ++i) {
        Entry &entry = roster.entries[i];
        // Only GP knockout carries CPU eliminations between races.
        if (System::sInstance->IsContext(PULSAR_MODE_KO) && entry.status >= 2)
            continue;
        entry.status = 0;
        RacedataPlayer &player = scenario.players[playerId];
        player.playerType = PLAYER_CPU;
        player.characterId = static_cast<CharacterId>(entry.character);
        player.kartId = static_cast<KartId>(entry.kart);
        player.hudSlotId = -1;
        player.realControllerChannel = -1;
        player.controllerType = CPU;
        player.rating.points = 5000;
        player.team = static_cast<Team>(entry.team);
        player.previousScore = entry.score;
        player.score = entry.score;
        player.prevFinishPos = entry.previousPosition;
        player.finishPos = entry.previousPosition;
        Race::racePlayerSlots[playerId] = entry.characterSlot;
        playerSlots[playerId] = i;
        controller->aidsBelongingToPlayerIds[playerId] = sub.hostAid;
        controller->disconnectedPlayerIds &= ~(1 << playerId);
        if (UI::ExtendedTeamManager::IsActivated()) {
            UI::ExtendedTeamManager::sInstance->SetPlayerIndexes(playerId, playerId, sub.hostAid, 2 + i);
            UI::ExtendedTeamManager::sInstance->SetPlayerTeam(playerId, static_cast<UI::ExtendedTeamID>(entry.team));
        }
        ++playerId;
    }
    scenario.settings.cpuMode = CPU_HARD;
    if (playerId != 0) {
        controller->subs[0].availableAids |= 1 << sub.hostAid;
        controller->subs[1].availableAids |= 1 << sub.hostAid;
    }
    scenario.ComputePlayerCounts(&scenario.playerCount, &scenario.screenCount, &scenario.localPlayerCount);
}

void SaveResults() {
    if (!System::sInstance->IsContext(PULSAR_FROOMCPUS))
        return;
    const RKNet::Controller *controller = RKNet::Controller::sInstance;
    const RKNet::ControllerSub &sub = controller->subs[controller->currentSub];
    if (sub.localAid != sub.hostAid)
        return;
    const RacedataScenario &scenario = Racedata::sInstance->menusScenario;
    for (u32 playerId = 0; playerId < scenario.playerCount; ++playerId) {
        if (playerId >= 12 || playerSlots[playerId] >= roster.count)
            continue;
        Entry &entry = roster.entries[playerSlots[playerId]];
        entry.score = scenario.players[playerId].score;
        entry.previousPosition = scenario.players[playerId].finishPos;
    }
}

static void InitKartNetworkType() {
    Kart::ResetPlayerTypeFlags();
    register u32 playerId;
    asm(mr playerId, r28;);
    if (System::sInstance->IsContext(PULSAR_FROOMCPUS) && playerId < 12 && playerSlots[playerId] < roster.count) {
        const RKNet::Controller *controller = RKNet::Controller::sInstance;
        const RKNet::ControllerSub &sub = controller->subs[controller->currentSub];
        // Use the native factory's region-specific flags without changing CPU identity.
        Racedata::sInstance->racesScenario.players[playerId].playerType = sub.localAid == sub.hostAid ? PLAYER_REAL_LOCAL : PLAYER_REAL_ONLINE;
    }
}
kmCall(0x8058f5d8, InitKartNetworkType);

static u32 SetKartNetworkType() {
    register u32 playerId;
    asm(mr playerId, r28;);
    if (System::sInstance->IsContext(PULSAR_FROOMCPUS) && playerId < 12 && playerSlots[playerId] < roster.count)
        Racedata::sInstance->racesScenario.players[playerId].playerType = PLAYER_CPU;
    return 8;  // Replaced allocation size at Kart::Manager::SetupPlayer + 0x8c.
}
kmCall(0x8058f640, SetKartNetworkType);

static void InitCPUAI(KartAIController &ai) {
    const u32 playerId = ai.GetPlayerIdx();
    RacedataPlayer &player = Racedata::sInstance->racesScenario.players[playerId];
    const PlayerType type = player.playerType;
    if (System::sInstance->IsContext(PULSAR_FROOMCPUS) && playerId < 12 && playerSlots[playerId] < roster.count) {
        const RKNet::Controller *controller = RKNet::Controller::sInstance;
        const RKNet::ControllerSub &sub = controller->subs[controller->currentSub];
        if (sub.localAid != sub.hostAid)
            player.playerType = PLAYER_REAL_ONLINE;
    }
    ai.Init();
    player.playerType = type;
}
kmCall(0x80596000, InitCPUAI);

static void UpdateCPUAI(KartAIController &ai) {
    const u32 playerId = ai.GetPlayerIdx();
    if (System::sInstance->IsContext(PULSAR_FROOMCPUS) && playerId < 12 && playerSlots[playerId] < roster.count) {
        const RKNet::Controller *controller = RKNet::Controller::sInstance;
        const RKNet::ControllerSub &sub = controller->subs[controller->currentSub];
        if (sub.localAid != sub.hostAid)
            return;
    }
    ai.Update();
}
kmCall(0x80596778, UpdateCPUAI);

static void SetCPUDifficulty(AI::Manager &manager) {
    manager.SetCCAndDifficulty();
    if (System::sInstance->IsContext(PULSAR_FROOMCPUS))
        manager.difficulty = CPU_HARD;
}
kmCall(0x80738fd0, SetCPUDifficulty);

static void ExportRaceData(RKNet::PacketMgr &mgr, const RKNet::RACEDATAPacket &packet, u32 size, u32 playerId) {
    if (!System::sInstance->IsContext(PULSAR_FROOMCPUS) || playerId >= 12 || playerSlots[playerId] >= roster.count) {
        mgr.ExportRACEDATA(packet, size, playerId);
        return;
    }
    const RaceinfoPlayer &player = *Raceinfo::sInstance->players[playerId];
    Progress state;
    state.checkpoint = player.checkpoint;
    memcpy(state.completion, &player.raceCompletion, sizeof(state.completion));
    state.respawn = player.respawn;
    state.battleScore = player.battleScore;
    state.lap = player.currentLap;
    state.maxLap = player.maxLap;
    state.kcp = player.currentKCP;
    state.maxKCP = player.maxKCP;
    state.flags = player.stateFlags;
    // Raceinfo initializes this timer as active at 0:00, before the race starts.
    state.finished = (player.stateFlags & 2) != 0;
    state.minutes = player.raceFinishTime->minutes;
    state.seconds = player.raceFinishTime->seconds;
    state.milliseconds = player.raceFinishTime->milliseconds;
    state.balloons = RaceBalloonManager::sInstance == nullptr ? 0xFF : RaceBalloonManager::sInstance->players[playerId].count;
    state.receivedFinishMask = 0;
    state.disconnectedMask = RKNet::Controller::sInstance->disconnectedPlayerIds;
    const RacedataScenario &scenario = Racedata::sInstance->racesScenario;
    if (scenario.settings.gamemode == MODE_PRIVATE_VS || scenario.settings.gamemode == MODE_PUBLIC_VS) {
        const GMDataOnlineVS &mode = *static_cast<const GMDataOnlineVS *>(Raceinfo::sInstance->gamemodeData);
        state.receivedFinishMask = mode.players[playerId].receivedFinishMask;
    }
    const int interrupts = OS::DisableInterrupts();
    memcpy(&racePackets[playerId], &packet, sizeof(packet));
    memcpy(&progress[playerId], &state, sizeof(state));
    packetFrames[playerId] = Raceinfo::sInstance->raceFrames;
    validPackets |= 1 << playerId;
    OS::RestoreInterrupts(interrupts);
}
kmCall(0x8058cb14, ExportRaceData);

static RKNet::RACEDATAPacket &ImportRaceData(RKNet::PacketMgr &mgr, u8 playerId) {
    return (System::sInstance->IsContext(PULSAR_FROOMCPUS) && playerId < 12 && playerSlots[playerId] < roster.count) ? racePackets[playerId] : mgr.GetRACEDATA(playerId);
}
kmCall(0x8058a1d0, ImportRaceData);

static u32 GetCPUFrame(const RKNet::PacketMgr &mgr, u32 playerId) {
    return (System::sInstance->IsContext(PULSAR_FROOMCPUS) && playerId < 12 && playerSlots[playerId] < roster.count) ? packetFrames[playerId] : mgr.GetPlayerRH1Timer(playerId);
}
kmCall(0x80589ab4, GetCPUFrame);
kmCall(0x80589ad8, GetCPUFrame);

static BOOL SendRaceAndCPUs(u8 aid, const void *buffer, int size) {
    const BOOL result = DWC::SendUnreliable(aid, buffer, size);
    if (!result || !System::sInstance->IsContext(PULSAR_FROOMCPUS) || RKNet::ITEMHandler::sInstance == nullptr)
        return result;
    const RKNet::Controller *controller = RKNet::Controller::sInstance;
    const RKNet::ControllerSub &sub = controller->subs[controller->currentSub];
    if (sub.localAid != sub.hostAid)
        return result;

    RacePacket packet;
    memset(&packet, 0, sizeof(packet));
    packet.header.magic = static_cast<const RKNet::RACEPacketHeader *>(buffer)->magic;
    packet.header.sizes[0] = sizeof(packet.header);
    packet.header.sizes[1] = sizeof(packet.rh1);
    packet.header.sizes[4] = sizeof(packet.racedata);
    packet.header.sizes[6] = sizeof(packet.items);
    packet.rh1.unknown_0x17 = 0xFC;
    packet.rh1.selectId = Racedata::sInstance->racesScenario.settings.selectId;
    u32 slot = 0;
    for (u32 playerId = 0; playerId <= 12; ++playerId) {
        if (playerId < 12) {
            if ((playerId >= 12 || playerSlots[playerId] >= roster.count) || !(validPackets & (1 << playerId)))
                continue;
            const int interrupts = OS::DisableInterrupts();
            packet.rh1.playerIds[slot] = playerId;
            packet.rh1.timer = packetFrames[playerId];
            memcpy(&packet.rh1.progress[slot], &progress[playerId], sizeof(Progress));
            memcpy(&packet.racedata[slot], &racePackets[playerId], sizeof(RKNet::RACEDATAPacket));
            memcpy(&packet.items[slot], &RKNet::ITEMHandler::sInstance->receivedPackets[playerId], sizeof(RKNet::ITEMPacket));
            // Every CPU shares the host's item-limit decision. Echo each remote
            // human's lottery timer so real players can complete their handshake.
            packet.items[slot].acknowledge = 0;
            u32 remoteSlot = 0;
            for (u32 i = 0; i < Racedata::sInstance->racesScenario.playerCount && remoteSlot < 2; ++i) {
                if ((i >= 12 || playerSlots[i] >= roster.count) && controller->aidsBelongingToPlayerIds[i] == aid) {
                    const RKNet::ITEMPacket &human = RKNet::ITEMHandler::sInstance->receivedPackets[i];
                    if (remoteSlot == 0)
                        packet.items[slot].acknowledgeTimer1 = human.timer;
                    else
                        packet.items[slot].acknowledgeTimer2 = human.timer;
                    if (human.mode == 1 && RKNet::ITEMHandler::sInstance->itemStatus[i] == 1)
                        packet.items[slot].acknowledge |= 1 << remoteSlot;
                    ++remoteSlot;
                }
            }
            OS::RestoreInterrupts(interrupts);
            ++slot;
        }
        if (slot == 2 || (slot != 0 && playerId == 12)) {
            if (slot == 1)
                packet.rh1.playerIds[1] = 0xFF;
            packet.header.crc32 = 0;
            packet.header.crc32 = OS::CalcCRC32(&packet, sizeof(packet));
            DWC::SendUnreliable(aid, &packet, sizeof(packet));
            slot = 0;
        }
    }
    return result;
}
kmCall(0x8065803c, SendRaceAndCPUs);
kmCall(0x80657f54, SendRaceAndCPUs);

static u32 CheckRacePacket(RKNet::RACEPacketHeader &header, u32 size) {
    register u32 expectedCRC;
    register u32 aid;
    asm(mr expectedCRC, r22; mr aid, r27;);
    const u32 crc = OS::CalcCRC32(&header, size);
    if (crc != expectedCRC)
        return crc;

    // Return a mismatched CRC for CPU packets so the native splitter skips them.
    if (size != sizeof(RacePacket) || header.sizes[0] != sizeof(header) || header.sizes[1] != sizeof(CPUHeader))
        return crc;
    const RacePacket &packet = reinterpret_cast<const RacePacket &>(header);
    if (packet.rh1.unknown_0x17 != 0xFC)
        return crc;
    // The room outlives its race item handler; late CPU packets are still ours to consume.
    if (!System::sInstance->IsContext(PULSAR_FROOMCPUS) || RKNet::ITEMHandler::sInstance == nullptr)
        return ~crc;
    const RKNet::Controller *controller = RKNet::Controller::sInstance;
    const RKNet::ControllerSub &sub = controller->subs[controller->currentSub];
    if (aid != sub.hostAid || sub.localAid == sub.hostAid || header.sizes[4] != sizeof(packet.racedata) || header.sizes[6] != sizeof(packet.items))
        return ~crc;
    // SELECT IDs are generated per console. Match the sender's race, not ours.
    const RKNet::PacketHolder<Network::PulRH1> *hostRH1 = controller->GetReceivedPacketHolder<Network::PulRH1>(sub.hostAid);
    if (packet.rh1.selectId != hostRH1->packet->selectId)
        return ~crc;
    for (u32 slot = 0; slot < 2; ++slot) {
        const u32 playerId = packet.rh1.playerIds[slot];
        if ((playerId >= 12 || playerSlots[playerId] >= roster.count) || ((validPackets & (1 << playerId)) && static_cast<s32>(packet.rh1.timer - packetFrames[playerId]) <= 0))
            continue;
        memcpy(&racePackets[playerId], &packet.racedata[slot], sizeof(RKNet::RACEDATAPacket));
        memcpy(&progress[playerId], &packet.rh1.progress[slot], sizeof(Progress));
        memcpy(&RKNet::ITEMHandler::sInstance->receivedPackets[playerId], &packet.items[slot], sizeof(RKNet::ITEMPacket));
        packetFrames[playerId] = packet.rh1.timer;
        validPackets |= 1 << playerId;
    }
    return ~crc;
}
kmCall(0x80659ab4, CheckRacePacket);

static void ApplyProgress() {
    if (!System::sInstance->IsContext(PULSAR_FROOMCPUS))
        return;
    const RKNet::Controller *controller = RKNet::Controller::sInstance;
    const RKNet::ControllerSub &sub = controller->subs[controller->currentSub];
    const System *system = System::sInstance;
    Raceinfo &raceinfo = *Raceinfo::sInstance;
    const RacedataScenario &scenario = Racedata::sInstance->racesScenario;
    if (sub.localAid == sub.hostAid) {
        RKNet::ITEMHandler &handler = *RKNet::ITEMHandler::sInstance;
        for (u32 playerId = 0; playerId < Racedata::sInstance->racesScenario.playerCount; ++playerId) {
            if (playerId >= 12 || playerSlots[playerId] >= roster.count)
                continue;
            RKNet::ITEMPacket &packet = handler.receivedPackets[playerId];
            packet.acknowledge = 0;
            for (u32 slot = 0; slot < sub.localPlayerCount; ++slot) {
                const u32 humanId = controller->GetLocalPlayerId(slot);
                const RKNet::ITEMPacket &human = handler.toSendPackets[slot];
                if (slot == 0)
                    packet.acknowledgeTimer1 = human.timer;
                else
                    packet.acknowledgeTimer2 = human.timer;
                if (human.mode == 1 && handler.itemStatus[humanId] == 1)
                    packet.acknowledge |= 1 << slot;
            }
        }
        if (raceinfo.stage == RACESTAGE_RACE && !system->IsContext(PULSAR_MODE_LAPKO) && !system->IsContext(PULSAR_MODE_BATTLEROYALE)
          && (scenario.settings.gamemode == MODE_PRIVATE_VS || scenario.settings.gamemode == MODE_PUBLIC_VS)) {
            bool hasRealPlayer = false;
            for (u32 playerId = 0; playerId < scenario.playerCount; ++playerId) {
                if (playerId < 12 && playerSlots[playerId] < roster.count)
                    continue;
                hasRealPlayer = true;
                if (!(raceinfo.players[playerId]->stateFlags & 0x12))
                    return;
            }
            if (hasRealPlayer) {
                for (u32 playerId = 0; playerId < scenario.playerCount; ++playerId) {
                    RaceinfoPlayer &player = *raceinfo.players[playerId];
                    if ((playerId < 12 && playerSlots[playerId] < roster.count) && !(player.stateFlags & 0x12))
                        player.EndRace(4, true);
                }
            }
        }
        return;
    }
}
RaceFrameHook applyProgress(ApplyProgress);

static void UpdateRacePlayer(RaceinfoPlayer &player) {
    if (!System::sInstance->IsContext(PULSAR_FROOMCPUS) || player.id >= 12 || playerSlots[player.id] >= roster.count) {
        player.Update();
        return;
    }
    const RKNet::Controller *controller = RKNet::Controller::sInstance;
    const RKNet::ControllerSub &sub = controller->subs[controller->currentSub];
    if (sub.localAid == sub.hostAid) {
        player.Update();
        const RacedataScenario &scenario = Racedata::sInstance->racesScenario;
        if (scenario.settings.gamemode == MODE_PRIVATE_VS || scenario.settings.gamemode == MODE_PUBLIC_VS) {
            GMDataOnlineVS &mode = *static_cast<GMDataOnlineVS *>(Raceinfo::sInstance->gamemodeData);
            GMDataOnlineVSPlayer &cpu = mode.players[player.id];
            cpu.raceFinishTime = *player.raceFinishTime;
            cpu.raceFinishTime.isActive = (player.stateFlags & 2) != 0;
            cpu.receivedFinishMask = 0;
            // A CPU shares the host's knowledge of finish times. Native RH2
            // still needs its acknowledgement when resolving each real racer.
            for (u32 i = 0; i < scenario.playerCount; ++i) {
                if ((Raceinfo::sInstance->players[i]->stateFlags & 2) || mode.players[i].raceFinishTime.isActive)
                    cpu.receivedFinishMask |= 1 << i;
            }
            cpu.acknowledgedFinishMask = (player.stateFlags & 2) ? 1 << player.id : 0;
            cpu.disconnectedMask = controller->disconnectedPlayerIds;
        }
        return;
    }
    if (!(validPackets & (1 << player.id)))
        return;
    Progress state;
    const int interrupts = OS::DisableInterrupts();
    memcpy(&state, &progress[player.id], sizeof(state));
    OS::RestoreInterrupts(interrupts);
    player.checkpoint = state.checkpoint;
    memcpy(&player.raceCompletion, state.completion, sizeof(state.completion));
    player.respawn = state.respawn;
    player.battleScore = state.battleScore;
    player.currentLap = state.lap;
    player.maxLap = state.maxLap;
    player.currentKCP = state.kcp;
    player.maxKCP = state.maxKCP;
    RaceBalloonManager *balloons = RaceBalloonManager::sInstance;
    if (balloons != nullptr && state.balloons <= 5) {
        const u8 current = balloons->players[player.id].count;
        if (current < state.balloons)
            BattleRoyale::AddBalloons(balloons, player.id, state.balloons - current);
        else if (current > state.balloons)
            balloons->Remove(player.id, 1, 1, 0, current - state.balloons, 0);
    }
    if (state.finished) {
        Timer time;
        time.minutes = state.minutes;
        time.seconds = state.seconds;
        time.milliseconds = state.milliseconds;
        time.isActive = true;
        if (!(player.stateFlags & 2))
            player.EndRace(time, false, 0);
        else
            *player.raceFinishTime = time;
    }
    player.stateFlags = state.flags;
    const RacedataScenario &scenario = Racedata::sInstance->racesScenario;
    if (scenario.settings.gamemode == MODE_PRIVATE_VS || scenario.settings.gamemode == MODE_PUBLIC_VS) {
        GMDataOnlineVS &mode = *static_cast<GMDataOnlineVS *>(Raceinfo::sInstance->gamemodeData);
        GMDataOnlineVSPlayer &cpu = mode.players[player.id];
        cpu.raceFinishTime = *player.raceFinishTime;
        cpu.raceFinishTime.isActive = state.finished;
        cpu.receivedFinishMask = state.receivedFinishMask;
        cpu.acknowledgedFinishMask = state.finished ? 1 << player.id : 0;
        cpu.disconnectedMask = state.disconnectedMask;
    }
}
kmCall(0x8053320c, UpdateRacePlayer);

static u32 GetItemSlot(const RKNet::PacketMgr &, u32 playerId) {
    // CPU ITEM records occupy their existing per-player receive slots. Returning
    // the equivalent offset lets native item setters reuse those records safely.
    if (System::sInstance->IsContext(PULSAR_FROOMCPUS) && playerId < 12 && playerSlots[playerId] < roster.count)
        return 2 + playerId;
    const RKNet::Controller *controller = RKNet::Controller::sInstance;
    for (u32 slot = 0; slot < 2; ++slot) {
        if (static_cast<u32>(controller->GetLocalPlayerId(slot)) == playerId)
            return slot;
    }
    return -1;
}
kmBranch(0x80654918, GetItemSlot);

static void DecideCPUItem(RKNet::ITEMHandler &handler, u8 playerId, ItemId item) {
    if (!System::sInstance->IsContext(PULSAR_FROOMCPUS) || playerId >= 12 || playerSlots[playerId] >= roster.count) {
        handler.SetItem(playerId, item);
        return;
    }
    RKNet::ITEMPacket &packet = handler.receivedPackets[playerId];
    packet.timer = Raceinfo::sInstance->raceFrames >> 3;
    packet.storedItem = Item::Manager::IsThereCapacityForItem(item) ? item : ITEM_NONE;
    packet.mode = 2;  // The host finalizes CPU lotteries; clients do not run them.
}
kmCall(0x807ba210, DecideCPUItem);
kmCall(0x80654730, DecideCPUItem);

static void InitItemPlayer(Item::Player &player, u8 playerId) {
    RacedataPlayer &entry = Racedata::sInstance->racesScenario.players[playerId];
    const RKNet::Controller *controller = RKNet::Controller::sInstance;
    const RKNet::ControllerSub &sub = controller->subs[controller->currentSub];
    const PlayerType type = entry.playerType;
    if (System::sInstance->IsContext(PULSAR_FROOMCPUS) && playerId < 12 && playerSlots[playerId] < roster.count && sub.localAid != sub.hostAid)
        entry.playerType = PLAYER_REAL_ONLINE;
    player.Init(playerId);
    entry.playerType = type;
}
kmCall(0x807993dc, InitItemPlayer);

static void UpdateItems(Item::Manager &manager) {
    manager.Update();
    if (!System::sInstance->IsContext(PULSAR_FROOMCPUS))
        return;
    const RKNet::Controller *controller = RKNet::Controller::sInstance;
    const RKNet::ControllerSub &sub = controller->subs[controller->currentSub];
    if (sub.localAid != sub.hostAid)
        return;
    RKNet::ITEMHandler &handler = *RKNet::ITEMHandler::sInstance;
    for (u32 playerId = 0; playerId < manager.playerCount; ++playerId) {
        if (playerId < 12 && playerSlots[playerId] < roster.count) {
            const Item::Player &player = manager.players[playerId];
            RKNet::ITEMPacket &packet = handler.receivedPackets[playerId];
            if (player.inventory.currentItemId != ITEM_NONE) {
                packet.storedItem = player.inventory.currentItemId;
                packet.mode = 7 - player.inventory.currentItemCount;
            } else if (player.roulette.isTheRouletteSpinning == 0) {
                packet.storedItem = ITEM_NONE;
                packet.mode = 0;
            }
            handler.UpdateDraggedItem(playerId);
        }
    }
}
kmCall(0x80554c68, UpdateItems);

}  // namespace FroomCPUs
}  // namespace Pulsar
