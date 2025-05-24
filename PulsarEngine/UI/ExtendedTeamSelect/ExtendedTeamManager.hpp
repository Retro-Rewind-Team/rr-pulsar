#ifndef __EXTENDED_TEAMSELECT_HPP__
#define __EXTENDED_TEAMSELECT_HPP__

#include <UI/UI.hpp>
#include <PulsarSystem.hpp>
#include <MarioKartWii/UI/Ctrl/CountDown.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <MarioKartWii/RKNet/ROOM.hpp>

namespace Pulsar
{
    namespace UI
    {

        enum ExtendedTeamID
        {
            TEAM_RED,
            TEAM_ORANGE,
            TEAM_YELLOW,
            TEAM_GREEN,
            TEAM_BLUE,
            TEAM_PURPLE,
            TEAM_COUNT
        };
        struct ExtendedTeamPlayer
        {
            u32 playerIdx;
            u32 miiIdx;
            u32 aid;
            u32 playerIdOnConsole;
            u32 active;
            u32 done;
            ExtendedTeamID team;
        };

        class ExtendedTeamManager
        {
        public:
            enum ExtendedROOMMessageType
            {
                MSG_TYPE_START_RACE = 0x81,
                MSG_TYPE_UPDATE_TEAMS = 0x82,
                MSG_TYPE_PING = 0x83,
                MSG_TYPE_ACK_START_RACE = 0x84
            };

            enum Status
            {
                STATUS_NONE,
                STATUS_WAITING_PRE,
                STATUS_SELECTING,
                STATUS_WAITING_POST,
                STATUS_DONE
            };

            static ExtendedTeamManager *sInstance;
            static void CreateInstance(ExtendedTeamManager *obj);
            static void DestroyInstance();

            ExtendedTeamManager();

            void SetPlayerTeam(u32 idx, ExtendedTeamID team)
            {
                this->players[idx].team = team;
            }

            void SetPlayerIndexes(u32 idx, u32 miiIdx, u32 aid, u32 playerIdOnConsole)
            {
                this->players[idx].playerIdx = idx;
                this->players[idx].miiIdx = miiIdx;
                this->players[idx].aid = aid;
                this->players[idx].playerIdOnConsole = playerIdOnConsole;
            }

            ExtendedTeamID GetPlayerTeamDeprecated(u32 idx)
            {
                return this->players[idx].team;
            }

            ExtendedTeamID GetPlayerTeam(u32 idx)
            {
                for (int i = 0; i < 12; i++)
                {
                    if (this->players[i].playerIdx == idx)
                    {
                        return this->players[i].team;
                    }
                }
                return TEAM_COUNT;
            }

            ExtendedTeamID GetPlayerTeamByAID(u8 aid, u8 playerIdOnConsole)
            {
                for (int i = 0; i < 12; i++)
                {
                    if (this->players[i].aid == aid && this->players[i].playerIdOnConsole == playerIdOnConsole)
                    {
                        return this->players[i].team;
                    }
                }
                return TEAM_COUNT;
            }

            u8 GetPlayerAID(u32 idx, u8 playerIdOnConsole)
            {
                return this->players[idx].aid;
            }

            const ExtendedTeamPlayer *GetPlayerInfo() const
            {
                return this->players;
            }

            void SendStartRacePacket();    // Host
            void SendUpdateTeamsPacket();  // Host
            void SendPingPacket();         // Non-Host
            void SendAckStartRacePacket(); // Non-Host

            void Update();
            void Reset();
            void ResetPlayers();

            void VotePageSync();

            Status GetStatus()
            {
                return this->status;
            }

            bool IsInactiveStatus()
            {
                return this->status == STATUS_NONE;
            }

            bool IsWaitingStatus()
            {
                return this->status == STATUS_WAITING_PRE || this->status == STATUS_WAITING_POST;
            }

            bool IsSelectingStatus()
            {
                return this->status == STATUS_SELECTING;
            }

            bool IsDoneStatus()
            {
                return this->status == STATUS_DONE;
            }

            void SetActiveStatusForAID(u8 aid)
            {
                for (int i = 0; i < 12; i++)
                {
                    if (this->players[i].aid == aid)
                    {
                        this->players[i].active = 1;
                    }
                }
            }

            void SetDoneStatusForAID(u8 aid)
            {
                for (int i = 0; i < 12; i++)
                {
                    if (this->players[i].aid == aid)
                    {
                        this->players[i].done = 1;
                    }
                }
            }

            void SetStatusExternal(Status status)
            {
                this->status = status;
            }

            static bool IsActivated()
            {
                RKNet::RoomType roomType = RKNet::Controller::sInstance->roomType;
                return System::sInstance->IsContext(PULSAR_EXTENDEDTEAMS) && Racedata::sInstance->menusScenario.settings.gamemode == MODE_PRIVATE_VS && (roomType == RKNet::ROOMTYPE_FROOM_HOST || roomType == RKNet::ROOMTYPE_FROOM_NONHOST);
            }

        private:
            bool AreAllOtherPlayersActive(u8 localAid);
            bool AreAllOtherPlayersDone(u8 localAid);

            ExtendedTeamPlayer players[12];

            bool isHost;

            Status status;

        public:
            CountDown waitingTimer;
            CountDown lastUpdateTimer;

            u32 hasFriendRoomStarted;
        };

    } // namespace UI
} // namespace Pulsar

#endif