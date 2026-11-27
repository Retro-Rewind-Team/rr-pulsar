#ifndef _PUL_GLOBALFUNCTION_
#define _PUL_GLOBALFUNCTION_

#include <kamek.hpp>
#include <MarioKartWii/System/Identifiers.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <MarioKartWii/Race/RaceData.hpp>

namespace Pulsar {

inline RKNet::RoomType GetRoomType() {
    const RKNet::Controller *controller = RKNet::Controller::sInstance;
    return controller != nullptr ? controller->roomType : RKNet::ROOMTYPE_NONE;
}

// Not in any online room (offline play)
inline bool IsOfflineRoom() {
    return GetRoomType() == RKNet::ROOMTYPE_NONE;
}

// In any online room (friend room, worldwide or regional)
inline bool IsOnlineRoom() {
    return GetRoomType() != RKNet::ROOMTYPE_NONE;
}

// Friend room, host or non-host
inline bool IsFriendRoom() {
    const RKNet::RoomType roomType = GetRoomType();
    return roomType == RKNet::ROOMTYPE_FROOM_HOST || roomType == RKNet::ROOMTYPE_FROOM_NONHOST;
}

// Friend room, local console is the host
inline bool IsFriendRoomHost() {
    return GetRoomType() == RKNet::ROOMTYPE_FROOM_HOST;
}

// Friend room or offline (anything that is not a public matchmaking room)
inline bool IsFriendRoomOrOffline() {
    return IsOfflineRoom() || IsFriendRoom();
}

// Worldwide room: VS, battle or joining
inline bool IsWorldwideRoom() {
    const RKNet::RoomType roomType = GetRoomType();
    return roomType == RKNet::ROOMTYPE_VS_WW || roomType == RKNet::ROOMTYPE_BT_WW || roomType == RKNet::ROOMTYPE_JOINING_WW;
}

// Worldwide VS room (VS or joining, no battle)
inline bool IsWorldwideVSRoom() {
    const RKNet::RoomType roomType = GetRoomType();
    return roomType == RKNet::ROOMTYPE_VS_WW || roomType == RKNet::ROOMTYPE_JOINING_WW;
}

// Regional room: VS, battle or joining
inline bool IsRegionalRoom() {
    const RKNet::RoomType roomType = GetRoomType();
    return roomType == RKNet::ROOMTYPE_VS_REGIONAL || roomType == RKNet::ROOMTYPE_BT_REGIONAL || roomType == RKNet::ROOMTYPE_JOINING_REGIONAL;
}

// Regional VS room (VS or joining, no battle)
inline bool IsRegionalVSRoom() {
    const RKNet::RoomType roomType = GetRoomType();
    return roomType == RKNet::ROOMTYPE_VS_REGIONAL || roomType == RKNet::ROOMTYPE_JOINING_REGIONAL;
}

// Public battle room (worldwide or regional)
inline bool IsPublicBattleRoom() {
    const RKNet::RoomType roomType = GetRoomType();
    return roomType == RKNet::ROOMTYPE_BT_WW || roomType == RKNet::ROOMTYPE_BT_REGIONAL;
}

// Public matchmaking room (worldwide or regional, any kind)
inline bool IsPublicRoom() {
    return IsWorldwideRoom() || IsRegionalRoom();
}

// WFC connection is active (not shut down); roomType may still be ROOMTYPE_NONE while in menus
inline bool IsOnlineConnected() {
    const RKNet::Controller *controller = RKNet::Controller::sInstance;
    return controller != nullptr && controller->connectionState != RKNet::CONNECTIONSTATE_SHUTDOWN;
}

// Time trial
inline bool IsTimeTrialMode(GameMode mode) {
    return mode == MODE_TIME_TRIAL;
}

// Time trial or ghost race
inline bool IsTimeTrialOrGhostMode(GameMode mode) {
    return mode == MODE_TIME_TRIAL || mode == MODE_GHOST_RACE;
}

// Any battle (offline, public or private)
inline bool IsBattleMode(GameMode mode) {
    return mode == MODE_BATTLE || mode == MODE_PUBLIC_BATTLE || mode == MODE_PRIVATE_BATTLE;
}

// Online battle (public or private)
inline bool IsOnlineBattleMode(GameMode mode) {
    return mode == MODE_PUBLIC_BATTLE || mode == MODE_PRIVATE_BATTLE;
}

// Any online mode (private VS, public VS, public battle, private battle)
inline bool IsOnlineMode(GameMode mode) {
    return mode >= MODE_PRIVATE_VS && mode <= MODE_PRIVATE_BATTLE;
}

// Friend room modes (private VS or private battle)
inline bool IsFriendRoomMode(GameMode mode) {
    return mode == MODE_PRIVATE_VS || mode == MODE_PRIVATE_BATTLE;
}

// Offline VS race (menu settings), requires the controller to exist
inline bool IsOfflineVS() {
    const RKNet::Controller *controller = RKNet::Controller::sInstance;
    return controller != nullptr && controller->roomType == RKNet::ROOMTYPE_NONE && Racedata::sInstance->menusScenario.settings.gamemode == MODE_VS_RACE;
}

inline bool IsOffline100cc() {
    return IsOfflineRoom() && (Racedata::sInstance->menusScenario.settings.modeFlags & 1);
}

}  // namespace Pulsar

#endif
