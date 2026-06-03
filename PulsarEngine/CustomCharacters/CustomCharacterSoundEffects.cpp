#include <CustomCharacters/CustomCharacters.hpp>

namespace Pulsar {
namespace CustomCharacters {

namespace {
static bool BuildLooseSoundEffectPath(u32 fileId, const char* postfix, const char* extension, char* path, u32 pathSize) {
    if (postfix == nullptr || postfix[0] == '\0' || extension == nullptr || path == nullptr || pathSize == 0) return false;
    const int written = snprintf(path, pathSize, "/sound/%u.%s.%s", fileId, postfix, extension);
    return written > 0 && static_cast<u32>(written) < pathSize;
}

}  // namespace

bool FindLooseSoundEffectPath(u32 fileId, const char* extension, char* path, u32 pathSize, u32* outFileSize) {
    if (path == nullptr || pathSize == 0) return false;
    path[0] = '\0';
    if (outFileSize != nullptr) *outFileSize = 0;

    const Racedata* racedata = Racedata::sInstance;
    if (racedata == nullptr) return false;

    const RacedataScenario& scenario = racedata->racesScenario;
    const u8 localCount = scenario.localPlayerCount > LOCAL_PLAYER_COUNT ? LOCAL_PLAYER_COUNT : scenario.localPlayerCount;
    for (u8 hud = 0; hud < localCount && hud < LOCAL_PLAYER_COUNT; ++hud) {
        const u8 playerId = racedata->GetPlayerIdOfLocalPlayer(hud);
        if (playerId >= scenario.playerCount || playerId >= ONLINE_PLAYER_COUNT) continue;

        const CharacterId character = scenario.players[playerId].characterId;
        const u8 table = RaceSkinTable(playerId, character);
        if (table == TABLE_DEFAULT) continue;

        const char* postfix = GeneratedCustomPostfix(character, table);
        if (postfix == nullptr) continue;

        if (!BuildLooseSoundEffectPath(fileId, postfix, extension, path, pathSize)) continue;

        u32 size = 0;
        if (DiscFileSize(path, size) && size != 0) {
            if (outFileSize != nullptr) *outFileSize = size;
            return true;
        }
    }

    path[0] = '\0';
    return false;
}

}  // namespace CustomCharacters
}  // namespace Pulsar
