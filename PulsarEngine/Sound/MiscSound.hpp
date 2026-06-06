#ifndef _MISCSOUND_
#define _MISCSOUND_

#include <kamek.hpp>
#include <core/nw4r/snd.hpp>
#include <MarioKartWii/Audio/SinglePlayer.hpp>
#include <MarioKartWii/UI/Section/SectionMgr.hpp>

namespace Pulsar {
namespace Sound {

snd::SoundStartable::StartResult PlayExtBRSEQ(snd::SoundStartable& startable, Audio::Handle& handle, const char* fileName, const char* labelName, bool hold);
const char wifilobbyMusicFile[] = "/sound/strm/wifi_lobby_bg.brstm";
const char wifiMusicFile[] = "/sound/strm/wifi_globe_bg.brstm";
const char offlineMusicFile[] = "/sound/strm/offline_bg.brstm";
const char titleMusicFile[] = "/sound/strm/title_bg.brstm";

// Returns true when the player is in an online lobby/voting section
// (i.e. connected to a room), as opposed to the initial wifi menu screens.
static inline bool IsWifiLobbySection(SectionId sectionId) {
    switch (sectionId) {
        case SECTION_P1_WIFI_VS_VOTING:
        case SECTION_P1_WIFI_BATTLE_VOTING:
        case SECTION_P2_WIFI_VS_VOTING:
        case SECTION_P2_WIFI_BATTLE_VOTING:
            return true;
        default:
            return sectionId >= SECTION_P1_WIFI_FROOM_VS_VOTING && sectionId <= SECTION_P2_WIFI_FROOM_COIN_VOTING;
    }
}

}  // namespace Sound
}  // namespace Pulsar

#endif