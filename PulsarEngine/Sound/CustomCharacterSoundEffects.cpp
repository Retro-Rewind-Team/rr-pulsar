#include <Race/CustomCharacters.hpp>
#include <MarioKartWii/Archive/ArchiveMgr.hpp>
#include <MarioKartWii/Race/RaceData.hpp>
#include <core/rvl/dvd/dvd.hpp>
#include <include/c_stdio.h>

namespace Pulsar {
namespace Sound {

bool FindLooseSoundEffectPath(u32 fileId, const char *extension, char *path, u32 pathSize, u32 *outFileSize) {
    if (path == nullptr || pathSize == 0 || extension == nullptr) return false;
    path[0] = '\0';
    if (outFileSize != nullptr) *outFileSize = 0;

    Racedata *racedata = Racedata::sInstance;
    if (racedata == nullptr) return false;
    const RacedataScenario &scenario = racedata->racesScenario;
    const u8 localCount = scenario.localPlayerCount > 4 ? 4 : scenario.localPlayerCount;
    for (u8 hud = 0; hud < localCount; ++hud) {
        const u8 playerId = racedata->GetPlayerIdOfLocalPlayer(hud);
        if (playerId >= scenario.playerCount || playerId >= 12) continue;
        const CharacterId character = scenario.players[playerId].characterId;
        const u32 slot = Race::GetPlayerCustomCharacterSlot(playerId, character);
        if (slot == 0) continue;

        const int written = snprintf(path, pathSize, "/sound/%u.%s-%u.%s", fileId,
                                     ArchiveMgr::GetKartArchivePostfix(character), slot, extension);
        if (written <= 0 || static_cast<u32>(written) >= pathSize) continue;

        DVD::FileInfo info;
        if (DVD::Open(path, &info)) {
            if (outFileSize != nullptr) *outFileSize = static_cast<u32>(info.length);
            DVD::Close(&info);
            return true;
        }
    }
    path[0] = '\0';
    return false;
}

}  // namespace Sound
}  // namespace Pulsar
