#ifndef _PULSAR_MOGI_HPP_
#define _PULSAR_MOGI_HPP_

#include <kamek.hpp>
#include <MarioKartWii/Race/Racedata.hpp>

namespace Pulsar {
namespace Mogi {

static const u8 REGION = 0x16;
static const u8 REGION_CT = 0x17;
static const u8 REGION_REG = 0x18;
static const u16 MMR_PACKET_MAGIC = 0x4D4D;

bool IsEnabled();
void SetEnabled(bool enabled);
void UpdateRoomState();
bool IsActive();
bool IsTeamFormat();
bool IsFormatVoteActive();
bool IsFormatVoteResolved();
void FinishFormatVote();
bool IsPublicRoom();
u32 GetLobbySeed();
u8 GetTeamForPlayer(u8 playerIdx);
void FillMMRPacket(u16& player0, u16& player1);
void ReceiveMMRPacket(u8 aid, u16 player0, u16 player1);
u16 GetRemoteMMR(u8 aid, u8 playerIdOnConsole);
void FillFormatVotePacket(u8& state, u8& format);
void ReceiveFormatVotePacket(u8 aid, u8 state, u8 format);
void CastFormatVote(u8 format);
void OnFormatVoteTimeout();
void OnPlayerDisconnect(u8 aid);
void ReceivePlayerScores(u8 aid, u16 player0, u16 player1);
u16 GetMissingTeamScore(u8 team, bool previous);
void PrepareHostRoom(u32& hostContext2, u8& raceCount);
void ApplyHostRoom(u32 hostContext2);
void OnDisconnect();
void OnFinalResults();
void OnResultsDisplayed();
void ProcessPendingDisconnect();

}  // namespace Mogi
}  // namespace Pulsar

#endif
