#include <Gamemodes/MissionMode/MissionMusic.hpp>
#include <Gamemodes/MissionMode/MissionMode.hpp>
#include <MarioKartWii/Audio/Race/AudioItemAlterationMgr.hpp>
#include <MarioKartWii/Race/RaceData.hpp>
#include <core/rvl/dvd/dvd.hpp>
#include <core/System/SystemManager.hpp>
#include <SlotExpansion/CupsConfig.hpp>

namespace Pulsar {
namespace MissionMode {

namespace {

static const char MISSION_MUSIC_FILE[] = "Binaries/ConfigMR.pul";
static const char MISSION_RUN_MUSIC_FILE[] = "/sound/strm/MissionRun.brstm";
static const char MISSION_BOSS_MUSIC_FILE[] = "/sound/strm/MissionBoss.brstm";
static const u32 MAX_MISSION_MUSIC_ENTRIES = 0x100;
static const u32 MAX_MUSIC_NAME_LENGTH = 0x50;
static const u32 MISSION_MUSIC_CONFIG_MAGIC = 0x4D524346;
static const u32 MISSION_MUSIC_CONFIG_VERSION = 1;
static const u32 MISSION_MUSIC_CONFIG_HEADER_SIZE = 0x10;
static const u32 MISSION_MUSIC_CONFIG_ENTRY_SIZE = MAX_MUSIC_NAME_LENGTH;

static bool associationsLoaded;
static bool hasAssociation[MAX_MISSION_MUSIC_ENTRIES];
static char associationNames[MAX_MISSION_MUSIC_ENTRIES][MAX_MUSIC_NAME_LENGTH];
static char resolvedPath[0x100];
static u32 cachedMissionId = MAX_MISSION_MUSIC_ENTRIES;
static bool cachedTrackFound;
static CourseId cachedMusicSlot;

static const u32 NATIVE_MUSIC_SLOT_COUNT =
    sizeof(Audio::ItemAlterationMgr::courseToSoundIdTable) / sizeof(Audio::ItemAlterationMgr::courseToSoundIdTable[0]);

static bool IsSafeMusicName(const char* name) {
    if (name == nullptr || *name == '\0') return false;
    if (strstr(name, "..") != nullptr) return false;
    if (strchr(name, '/') != nullptr || strchr(name, '\\') != nullptr || strchr(name, ':') != nullptr) return false;
    return true;
}

static bool CopyMusicName(char* dest, const char* source) {
    if (!IsSafeMusicName(source)) return false;
    const size_t length = strlen(source);
    if (length >= MAX_MUSIC_NAME_LENGTH) return false;
    memcpy(dest, source, length + 1);
    const size_t extensionLength = 4;
    if (length > extensionLength && dest[length - extensionLength] == '.' &&
        (dest[length - 3] == 's' || dest[length - 3] == 'S') &&
        (dest[length - 2] == 'z' || dest[length - 2] == 'Z') &&
        (dest[length - 1] == 's' || dest[length - 1] == 'S'))
        dest[length - extensionLength] = '\0';
    return dest[0] != '\0';
}

static u32 ReadBigEndian32(const u8* data) {
    return (static_cast<u32>(data[0]) << 24) | (static_cast<u32>(data[1]) << 16) |
           (static_cast<u32>(data[2]) << 8) | static_cast<u32>(data[3]);
}

static void LoadAssociations() {
    if (associationsLoaded) return;
    associationsLoaded = true;

    u32 fileSize = 0;
    char* file = static_cast<char*>(SystemManager::RipFromDisc(MISSION_MUSIC_FILE, nullptr, &fileSize));
    if (file == nullptr || fileSize < MISSION_MUSIC_CONFIG_HEADER_SIZE) return;

    const u8* data = reinterpret_cast<const u8*>(file);
    const u32 magic = ReadBigEndian32(data);
    const u32 version = ReadBigEndian32(data + 0x04);
    const u32 entryCount = ReadBigEndian32(data + 0x08);
    const u32 entrySize = ReadBigEndian32(data + 0x0C);
    if (magic != MISSION_MUSIC_CONFIG_MAGIC || version != MISSION_MUSIC_CONFIG_VERSION ||
        entrySize != MISSION_MUSIC_CONFIG_ENTRY_SIZE || entryCount > MAX_MISSION_MUSIC_ENTRIES ||
        entryCount > (fileSize - MISSION_MUSIC_CONFIG_HEADER_SIZE) / entrySize) return;

    for (u32 missionId = 0; missionId < entryCount; ++missionId) {
        const u8* entry = data + MISSION_MUSIC_CONFIG_HEADER_SIZE + missionId * entrySize;
        char name[MAX_MUSIC_NAME_LENGTH];
        u32 nameLength = 0;
        while (nameLength < entrySize && entry[nameLength] != '\0') ++nameLength;
        if (nameLength == 0 || nameLength >= sizeof(name)) continue;
        memcpy(name, entry, nameLength);
        name[nameLength] = '\0';
        if (CopyMusicName(associationNames[missionId], name)) hasAssociation[missionId] = true;
    }

}

static bool CheckPath(const char* path) {
    return DVD::ConvertPathToEntryNum(path) >= 0;
}

static bool StringsEqualIgnoreCase(const char* lhs, const char* rhs) {
    if (lhs == nullptr || rhs == nullptr) return false;
    while (*lhs != '\0' && *rhs != '\0') {
        char left = *lhs++;
        char right = *rhs++;
        if (left >= 'A' && left <= 'Z') left = static_cast<char>(left + ('a' - 'A'));
        if (right >= 'A' && right <= 'Z') right = static_cast<char>(right + ('a' - 'A'));
        if (left != right) return false;
    }
    return *lhs == '\0' && *rhs == '\0';
}

static bool MusicNamesMatch(const char* configuredName, const char* trackName) {
    if (StringsEqualIgnoreCase(configuredName, trackName)) return true;
    if (trackName == nullptr) return false;

    const size_t length = strlen(trackName);
    if (length <= 4 || strcmp(trackName + length - 4, ".szs") != 0) return false;

    char trackNameWithoutExtension[MAX_MUSIC_NAME_LENGTH];
    if (length - 4 >= sizeof(trackNameWithoutExtension)) return false;
    memcpy(trackNameWithoutExtension, trackName, length - 4);
    trackNameWithoutExtension[length - 4] = '\0';
    return StringsEqualIgnoreCase(configuredName, trackNameWithoutExtension);
}

static bool FindConfiguredMusicSlot(CourseId& musicSlot) {
    if (CupsConfig::sInstance == nullptr || Racedata::sInstance == nullptr) return false;
    const RacedataScenario& scenario = Racedata::sInstance->racesScenario;
    if (!IsMissionScenario(scenario) || IsMissionBossObjective(scenario) || IsMissionScoreObjective(scenario)) return false;

    const u32 missionId = scenario.settings.raceNumber;
    if (missionId >= MAX_MISSION_MUSIC_ENTRIES || !hasAssociation[missionId]) return false;

    if (cachedMissionId == missionId) {
        if (!cachedTrackFound) return false;
        musicSlot = cachedMusicSlot;
        return true;
    }
    cachedMissionId = missionId;
    cachedTrackFound = false;

    const CupsConfig* cupsConfig = CupsConfig::sInstance;
    const u32 trackCount = static_cast<u32>(cupsConfig->GetCtsTrackCount());
    for (u32 i = 0; i < trackCount; ++i) {
        const PulsarId candidate = static_cast<PulsarId>(PULSARID_FIRSTCT + i);
        if (!MusicNamesMatch(associationNames[missionId], cupsConfig->GetFileName(candidate, 0))) continue;

        const Track& track = cupsConfig->GetTrack(candidate);
        if (track.musicSlot >= NATIVE_MUSIC_SLOT_COUNT) return false;
        cachedTrackFound = true;
        cachedMusicSlot = static_cast<CourseId>(track.musicSlot);
        musicSlot = cachedMusicSlot;
        return true;
    }
    return false;
}

static bool ResolveForcedMusic(const RacedataScenario& scenario, const char*& extFilePath) {
    const char* path = nullptr;
    if (IsMissionBossObjective(scenario))
        path = MISSION_BOSS_MUSIC_FILE;
    else if (IsMissionScoreObjective(scenario))
        path = MISSION_RUN_MUSIC_FILE;
    if (path == nullptr || !CheckPath(path)) return false;
    extFilePath = path;
    return true;
}

}

bool ResolveMissionMusicPath(const char* brstmRoot, const char*& extFilePath) {
    if (Racedata::sInstance == nullptr) return false;
    const RacedataScenario& scenario = Racedata::sInstance->racesScenario;
    if (!IsMissionScenario(scenario)) return false;

    const u32 missionId = scenario.settings.raceNumber;
    if (ResolveForcedMusic(scenario, extFilePath)) return true;

    LoadAssociations();
    if (missionId >= MAX_MISSION_MUSIC_ENTRIES || !hasAssociation[missionId]) return false;

    const char* root = brstmRoot != nullptr ? brstmRoot : "/sound/";
    snprintf(resolvedPath, sizeof(resolvedPath), "%sstrm/%s_n.brstm", root, associationNames[missionId]);
    if (!CheckPath(resolvedPath)) return false;
    extFilePath = resolvedPath;
    return true;
}

bool GetMissionMusicSlotOverride(CourseId& musicSlot) {
    if (Racedata::sInstance == nullptr) return false;
    const RacedataScenario& scenario = Racedata::sInstance->racesScenario;
    if (!IsMissionScenario(scenario)) return false;

    LoadAssociations();
    return FindConfiguredMusicSlot(musicSlot);
}

}
}
