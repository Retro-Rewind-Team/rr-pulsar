#ifndef _PULSAR_MOGI_HPP_
#define _PULSAR_MOGI_HPP_

#include <kamek.hpp>
#include <MarioKartWii/Race/Racedata.hpp>

namespace Pulsar {
namespace Mogi {

static const u8 REGION = 0x16;

bool IsEnabled();
void SetEnabled(bool enabled);
bool IsActive();
bool IsTeamFormat();
bool IsPublicRoom();
bool CanStartRace();
u32 GetLobbySeed();
u8 GetTeamForPlayer(u8 playerIdx);
void PrepareHostRoom(u32& hostContext2, u8& raceCount);
void ApplyHostRoom(u32 hostContext2);
void OnFinalRace(const RacedataScenario& scenario);
void ProcessPendingDisconnect();

}  // namespace Mogi
}  // namespace Pulsar

#endif
