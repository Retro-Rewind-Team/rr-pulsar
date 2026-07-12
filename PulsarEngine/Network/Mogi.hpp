#ifndef _PULSAR_MOGI_HPP_
#define _PULSAR_MOGI_HPP_

#include <kamek.hpp>
#include <MarioKartWii/Race/Racedata.hpp>

namespace Pulsar {
namespace Mogi {

static const u8 REGION = 0x16;
static const u16 MMR_PACKET_MAGIC = 0x4D4D;

bool IsEnabled();
void SetEnabled(bool enabled);
bool IsActive();
bool IsTeamFormat();
bool IsPublicRoom();
bool CanStartRace();
u32 GetLobbySeed();
u8 GetTeamForPlayer(u8 playerIdx);
void SetTeamForPlayer(u8 playerIdx, u8 team);
u8 GetRaceCount();
void FillMMRPacket(u16& player0, u16& player1);
void ReceiveMMRPacket(u8 aid, u16 player0, u16 player1);
u16 GetRemoteMMR(u8 aid, u8 playerIdOnConsole);
void PrepareHostRoom(u32& hostContext2, u8& raceCount);
void ApplyHostRoom(u32 hostContext2);
void OnFinalRace(const RacedataScenario& scenario);
void ProcessPendingDisconnect();

}  // namespace Mogi
}  // namespace Pulsar

#endif
