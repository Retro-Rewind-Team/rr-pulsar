#include <kamek.hpp>
#include <PulsarSystem.hpp>
#include <IO/IO.hpp>
#include <IO/RiivoIO.hpp>
#include <IO/SDIO.hpp>
#include <IO/LooseArchiveOverrides.hpp>
#include <MarioKartWii/Archive/ArchiveFile.hpp>
#include <core/egg/Archive.hpp>
#include <core/RK/RKSystem.hpp>
#include <core/egg/DVD/DvdRipper.hpp>
#include <core/egg/mem/Heap.hpp>
#include <core/nw4r/ut/Misc.hpp>
#include <core/rvl/arc/arc.hpp>
#include <core/rvl/devfs/isfs.hpp>
#include <core/rvl/dvd/dvd.hpp>
#include <core/rvl/OS/OSBootInfo.hpp>
#include <core/rvl/os/OS.hpp>
#include <core/rvl/os/OSCache.hpp>

namespace Pulsar {
class IO;
}
typedef Pulsar::IO PulsarIOClass;

namespace Pulsar {
namespace IOOverrides {

#ifndef PULSAR_OVERRIDE_LOGGING
#define PULSAR_OVERRIDE_LOGGING 1
#endif

#if PULSAR_OVERRIDE_LOGGING
#define OVERRIDE_LOG(...) Pulsar_Log("[Pulsar][Overrides] " __VA_ARGS__)
#define OVERRIDE_WARN(...) Pulsar_Log("[Pulsar][Overrides][Warn] " __VA_ARGS__)
#else
#define OVERRIDE_LOG(...)
#define OVERRIDE_WARN(...)
#endif

namespace {

const char kModsRoot[] = "/mods";
const char kModsRootAlt[] = "/Mods";
const char kModsRootPrefix[] = "/mods/";
const char kModsRootAltPrefix[] = "/Mods/";
const char kOverrideDiagBuild[] = "override-diag-20260330b";
const u32 kMaxOverridesTotal = 1024;
const u32 kOverrideRepackGrowthFallback = 0x100000;
const u32 kOverrideMaxGrowthOnSourceHeap = 0x100000;

struct OverrideEntry {
    char fullPath[OVERRIDE_MAX_PATH];
    char relativePath[OVERRIDE_MAX_PATH];
    char strippedName[OVERRIDE_MAX_PATH];
    char archiveTagLower[OVERRIDE_MAX_NAME];
    bool hasSubpath;
    bool isTagged;
    u32 size;
};

struct ModIndex {
    OverrideEntry* entries;
    u32 count;
};

struct U8Node {
    u32 typeName;
    u32 dataOffset;
    u32 dataSize;
};

struct FSTEntry {
    u32 typeName;
    u32 offset;
    u32 size;
};

struct DVDOverrideMatch {
    u32 nodeIndex;
    u32 size;
    char path[OVERRIDE_MAX_PATH];
};

struct sd_vtable {
    int (*open)(void* file_struct, const char* path, int flags);
    int (*close)(int fd);
    int (*read)(int fd, void* ptr, size_t len);
    int (*write)(int fd, const void* ptr, size_t len);
    int (*rename)(const char* oldName, const char* newName);
    int (*stat)(const char* path, void* statbuf);
    int (*mkdir)(const char* path);
    int (*diropen)(dir_struct* dir, const char* path);
    int (*dirnext)(dir_struct* dir, char* outFilename, void* filestatbuf);
    int (*dirclose)(dir_struct* dir);
    int (*seek)(int fd, int pos, int direction);
    int (*errno)();
};

const sd_vtable* sSdVtable = reinterpret_cast<sd_vtable*>(0x81782e00);

#define S_IFDIR 0040000
#define S_IFMT 0170000

static ModIndex sModIndex = {nullptr, 0};
static bool sModIndexAttempted = false;
static bool sModsRootChecked = false;
static bool sModsRootPresent = false;
static bool sOverrideSourceChecked = false;
static bool sLoggedDiagBuild = false;
static bool sLoggedFSTSummary = false;
static char sModsRootPath[OVERRIDE_MAX_PATH] = "/mods";

enum OverrideSource {
    OVERRIDE_SOURCE_NONE = 0,
    OVERRIDE_SOURCE_RIIVO = 1,
    OVERRIDE_SOURCE_SD = 2,
    OVERRIDE_SOURCE_ISFS = 3,
    OVERRIDE_SOURCE_DVD = 4
};

static OverrideSource sOverrideSource = OVERRIDE_SOURCE_NONE;

static void LogDiagBuildOnce() {
    if (sLoggedDiagBuild) return;
    sLoggedDiagBuild = true;
    OVERRIDE_LOG("Diagnostics build=%s\n", kOverrideDiagBuild);
}

static bool StartsWith(const char* str, const char* prefix) {
    if (str == nullptr || prefix == nullptr) return false;
    while (*prefix != '\0') {
        if (*str != *prefix) return false;
        ++str;
        ++prefix;
    }
    return true;
}

static bool EndsWithIgnoreCase(const char* str, const char* suffix) {
    if (str == nullptr || suffix == nullptr) return false;
    const size_t strLen = strlen(str);
    const size_t suffixLen = strlen(suffix);
    if (suffixLen > strLen) return false;
    const char* tail = str + (strLen - suffixLen);
    for (size_t i = 0; i < suffixLen; ++i) {
        char a = tail[i];
        char b = suffix[i];
        if (a >= 'A' && a <= 'Z') a = static_cast<char>(a - 'A' + 'a');
        if (b >= 'A' && b <= 'Z') b = static_cast<char>(b - 'A' + 'a');
        if (a != b) return false;
    }
    return true;
}

static const char* FindBasename(const char* path) {
    if (path == nullptr) return nullptr;
    const char* lastSlash = nullptr;
    for (const char* p = path; *p != '\0'; ++p) {
        if (*p == '/') lastSlash = p;
    }
    return lastSlash ? lastSlash + 1 : path;
}

static const char* FindLastChar(const char* str, char needle) {
    if (str == nullptr) return nullptr;
    const char* last = nullptr;
    for (const char* p = str; *p != '\0'; ++p) {
        if (*p == needle) last = p;
    }
    return last;
}

static void ToLowerCopy(char* dest, const char* src, u32 destSize) {
    if (dest == nullptr || destSize == 0) return;
    if (src == nullptr) {
        dest[0] = '\0';
        return;
    }
    u32 i = 0;
    for (; src[i] != '\0' && i + 1 < destSize; ++i) {
        char c = src[i];
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
        dest[i] = c;
    }
    dest[i] = '\0';
}

static void ToLowerInPlace(char* str) {
    if (str == nullptr) return;
    for (; *str != '\0'; ++str) {
        if (*str >= 'A' && *str <= 'Z') *str = static_cast<char>(*str - 'A' + 'a');
    }
}

static bool NodeIsDir(const U8Node& node) {
    return (node.typeName >> 24) != 0;
}

static u32 NodeNameOffset(const U8Node& node) {
    return node.typeName & 0x00FFFFFF;
}

static bool FSTEntryIsDir(const FSTEntry& entry) {
    return (entry.typeName & 0xFF000000) != 0;
}

static u32 FSTNameOffset(const FSTEntry& entry) {
    return entry.typeName & 0x00FFFFFF;
}

static bool IsDotEntry(const char* name) {
    if (name == nullptr) return false;
    if (name[0] == '.' && name[1] == '\0') return true;
    if (name[0] == '.' && name[1] == '.' && name[2] == '\0') return true;
    return false;
}

static void CopyPath(char* dest, u32 destSize, const char* src) {
    if (dest == nullptr || destSize == 0) return;
    if (src == nullptr) {
        dest[0] = '\0';
        return;
    }
    strncpy(dest, src, destSize);
    dest[destSize - 1] = '\0';
}

static void SetModsRootPath(const char* path) {
    CopyPath(sModsRootPath, sizeof(sModsRootPath), path);
}

static bool BuildChildPaths(char* childAbs, u32 childAbsSize, char* childRel, u32 childRelSize,
                            const char* absPath, const char* relPath, const char* name) {
    if (childAbs == nullptr || childRel == nullptr || absPath == nullptr || relPath == nullptr || name == nullptr) return false;

    int relWritten = 0;
    if (relPath[0] != '\0') {
        relWritten = snprintf(childRel, childRelSize, "%s/%s", relPath, name);
    } else {
        relWritten = snprintf(childRel, childRelSize, "%s", name);
    }

    const int absWritten = snprintf(childAbs, childAbsSize, "%s/%s", absPath, name);
    if (relWritten <= 0 || absWritten <= 0) return false;
    if (static_cast<u32>(relWritten) >= childRelSize || static_cast<u32>(absWritten) >= childAbsSize) return false;
    return true;
}

static bool AppendPath(char* path, u32 pathSize, u32& pathLen, const char* name) {
    if (path == nullptr || name == nullptr || pathSize == 0) return false;
    int written = 0;
    if (pathLen == 0) {
        written = snprintf(path, pathSize, "%s", name);
    } else {
        written = snprintf(path + pathLen, pathSize - pathLen, "/%s", name);
    }
    if (written <= 0) return false;
    if (pathLen + static_cast<u32>(written) >= pathSize) return false;
    pathLen += static_cast<u32>(written);
    return true;
}

static EGG::Heap* GetOverridesHeap() {
    System* system = System::sInstance;
    if (system == nullptr) return 0;
    return static_cast<EGG::Heap*>(system->heap);
}

static bool ModsRootExists();
static OverrideSource GetOverrideSource(IOType ioType);
static bool FindModsDirInFST(u32& outIndex, u32& outEnd, const char*& outRootPath);

static bool ResolveFSTDirByPath(const char* path, u32 entryCount, u32& outIndex, u32& outEnd) {
    if (path == nullptr || path[0] == '\0') return false;
    const s32 entryNum = DVD::ConvertPathToEntryNum(path);
    OVERRIDE_LOG("FST path lookup: %s -> entry=%d\n", path, entryNum);
    if (entryNum < 0) return false;
    if (static_cast<u32>(entryNum) >= entryCount) {
        OVERRIDE_WARN("FST path lookup out of bounds: %s -> entry=%d count=%u\n", path, entryNum, entryCount);
        return false;
    }
    const FSTEntry* entries = static_cast<const FSTEntry*>(OS::BootInfo::mInstance.FSTLocation);
    if (!FSTEntryIsDir(entries[entryNum])) {
        OVERRIDE_WARN("FST path lookup is not a dir: %s -> entry=%d\n", path, entryNum);
        return false;
    }
    outIndex = static_cast<u32>(entryNum);
    outEnd = entries[outIndex].size;
    OVERRIDE_LOG("FST path resolved: %s -> index=%u end=%u\n", path, outIndex, outEnd);
    return true;
}

static void LogFSTProbeSummary(const FSTEntry* entries, u32 entryCount, const char* stringTable) {
    if (sLoggedFSTSummary) return;
    sLoggedFSTSummary = true;
    OVERRIDE_LOG("FST probe: location=%08x entryCount=%u\n", reinterpret_cast<u32>(entries), entryCount);

    u32 rootLogged = 0;
    u32 namedMatches = 0;
    for (u32 i = 1; i < entryCount; ++i) {
        const FSTEntry& entry = entries[i];
        const char* name = stringTable + FSTNameOffset(entry);
        if (name == nullptr) continue;

        if (FSTEntryIsDir(entry) && entry.offset == 0 && rootLogged < 24) {
            OVERRIDE_LOG("FST root dir[%u]: index=%u name=%s end=%u\n", rootLogged, i, name, entry.size);
            ++rootLogged;
        }

        if (strcmp(name, "mods") == 0 || strcmp(name, "Mods") == 0) {
            OVERRIDE_LOG("FST named entry[%u]: index=%u name=%s isDir=%d offset=%u size=%u\n",
                         namedMatches, i, name, FSTEntryIsDir(entry) ? 1 : 0, entry.offset, entry.size);
            ++namedMatches;
        }
    }

    OVERRIDE_LOG("FST probe summary: rootDirsLogged=%u namedModsMatches=%u\n", rootLogged, namedMatches);
}

static bool OpenRiivoDir(const char* path, s32* outDeviceFd, s32* outDirFd) {
    if (outDeviceFd == nullptr || outDirFd == nullptr) return false;
    s32 riivoFd = PulsarIOClass::OpenFix("file", IOS::MODE_NONE);
    if (riivoFd < 0) return false;
    s32 dirFd = IOS::IOCtl(riivoFd, static_cast<IOS::IOCtlType>(RIIVO_IOCTL_OPENDIR), (void*)path, strlen(path) + 1, nullptr, 0);
    if (dirFd < 0) {
        IOS::Close(riivoFd);
        return false;
    }
    *outDeviceFd = riivoFd;
    *outDirFd = dirFd;
    return true;
}

static void CloseRiivoDir(s32 deviceFd, s32 dirFd) {
    IOS::IOCtl(deviceFd, static_cast<IOS::IOCtlType>(RIIVO_IOCTL_CLOSEDIR), (void*)&dirFd, sizeof(s32), nullptr, 0);
    IOS::Close(deviceFd);
}

static bool RiivoDirExists(const char* path) {
    s32 deviceFd = -1;
    s32 dirFd = -1;
    if (!OpenRiivoDir(path, &deviceFd, &dirFd)) return false;
    CloseRiivoDir(deviceFd, dirFd);
    return true;
}

static bool RiivoFileExists(const char* path) {
    char fullPath[riivoMaxPath];
    const int written = snprintf(fullPath, sizeof(fullPath), "file%s", path);
    if (written <= 0 || static_cast<size_t>(written) >= sizeof(fullPath)) return false;
    s32 fd = PulsarIOClass::OpenFix(fullPath, static_cast<IOS::Mode>(RIIVO_MODE_READ));
    if (fd < 0) return false;
    IOS::Close(fd);
    return true;
}

static bool SDFileExists(const char* path) {
    if (sSdVtable == nullptr || sSdVtable->open == nullptr || sSdVtable->close == nullptr) return false;
    file_struct file;
    if (sSdVtable->open(&file, path, O_RDONLY) == -1) return false;
    const int fd = reinterpret_cast<int>(&file);
    sSdVtable->close(fd);
    return true;
}

static bool SDDirExists(const char* path) {
    if (sSdVtable == nullptr || sSdVtable->stat == nullptr) return false;
    stat st;
    if (sSdVtable->stat(path, &st) != 0) return false;
    return (st.st_mode & S_IFMT) == S_IFDIR;
}

static bool ISFSFileExists(const char* path) {
    char tmp[OVERRIDE_MAX_PATH];
    strncpy(tmp, path, sizeof(tmp));
    tmp[sizeof(tmp) - 1] = '\0';
    s32 fd = ISFS::Open(tmp, ISFS::MODE_READ);
    if (fd < 0) return false;
    ISFS::Close(fd);
    return true;
}

static bool ISFSDirExists(const char* path) {
    u32 count = 0;
    return ISFS::ReadDir(path, nullptr, &count) >= 0;
}

static bool ISFSDirHasEntries(const char* path) {
    u32 count = 0;
    return ISFS::ReadDir(path, nullptr, &count) >= 0 && count > 0;
}

static bool ReadDVDFile(const char* path, void* dest, u32 size) {
    DVD::FileInfo info;
    if (!DVD::Open(path, &info)) return false;
    const s32 read = DVD::ReadPrio(&info, dest, static_cast<s32>(size), 0, 2);
    DVD::Close(&info);
    return read == static_cast<s32>(size);
}

static bool BuildOverridePathWithRoot(const char* root, const char* name, const char* tag, char* outPath, u32 outSize) {
    if (root == nullptr || name == nullptr || outPath == nullptr || outSize == 0) return false;
    int written = 0;
    if (tag != nullptr && tag[0] != '\0') {
        written = snprintf(outPath, outSize, "%s/%s.%s", root, name, tag);
    } else {
        written = snprintf(outPath, outSize, "%s/%s", root, name);
    }
    if (written <= 0 || static_cast<u32>(written) >= outSize) return false;
    return true;
}

static bool BuildModsRootFromModFolder(const char* prefix, const char* modFolder, const char* leaf, char* outPath, u32 outSize) {
    if (modFolder == nullptr || leaf == nullptr || outPath == nullptr || outSize == 0) return false;
    const int written = snprintf(outPath, outSize, "%s%s/%s", prefix, modFolder, leaf);
    if (written <= 0 || static_cast<u32>(written) >= outSize) return false;
    return true;
}

static bool TryRiivoModsRootCandidate(const char* path) {
    if (path == nullptr || path[0] == '\0') return false;
    const bool exists = RiivoDirExists(path);
    OVERRIDE_LOG("Riivo mods candidate: %s -> %s\n", path, exists ? "hit" : "miss");
    if (exists) {
        SetModsRootPath(path);
    }
    return exists;
}

static bool ResolveRiivoModsRoot() {
    System* system = System::sInstance;
    const char* modFolder = nullptr;
    if (system != nullptr) modFolder = system->GetModFolder();

    char path[OVERRIDE_MAX_PATH];
    if (modFolder != nullptr) {
        if (BuildModsRootFromModFolder("", modFolder, "Mods", path, sizeof(path)) && TryRiivoModsRootCandidate(path)) return true;
        if (BuildModsRootFromModFolder("", modFolder, "mods", path, sizeof(path)) && TryRiivoModsRootCandidate(path)) return true;
        if (BuildModsRootFromModFolder("/mnt/sd", modFolder, "Mods", path, sizeof(path)) && TryRiivoModsRootCandidate(path)) return true;
        if (BuildModsRootFromModFolder("/mnt/sd", modFolder, "mods", path, sizeof(path)) && TryRiivoModsRootCandidate(path)) return true;
        if (BuildModsRootFromModFolder("/mnt/usb", modFolder, "Mods", path, sizeof(path)) && TryRiivoModsRootCandidate(path)) return true;
        if (BuildModsRootFromModFolder("/mnt/usb", modFolder, "mods", path, sizeof(path)) && TryRiivoModsRootCandidate(path)) return true;
    }

    if (TryRiivoModsRootCandidate(kModsRoot)) return true;
    if (TryRiivoModsRootCandidate(kModsRootAlt)) return true;
    return false;
}

static bool ResolveSDModsRoot() {
    System* system = System::sInstance;
    const char* modFolder = nullptr;
    if (system != nullptr) modFolder = system->GetModFolder();

    char path[OVERRIDE_MAX_PATH];
    if (modFolder != nullptr) {
        if (BuildModsRootFromModFolder("", modFolder, "Mods", path, sizeof(path)) && SDDirExists(path)) {
            SetModsRootPath(path);
            return true;
        }
        if (BuildModsRootFromModFolder("", modFolder, "mods", path, sizeof(path)) && SDDirExists(path)) {
            SetModsRootPath(path);
            return true;
        }
    }

    if (SDDirExists(kModsRoot)) {
        SetModsRootPath(kModsRoot);
        return true;
    }
    if (SDDirExists(kModsRootAlt)) {
        SetModsRootPath(kModsRootAlt);
        return true;
    }
    return false;
}

static bool ResolveISFSModsRoot() {
    System* system = System::sInstance;
    const char* modFolder = nullptr;
    if (system != nullptr) modFolder = system->GetModFolder();

    char path[OVERRIDE_MAX_PATH];
    if (modFolder != nullptr) {
        if (BuildModsRootFromModFolder("/shared2/Pulsar", modFolder, "Mods", path, sizeof(path)) && ISFSDirHasEntries(path)) {
            SetModsRootPath(path);
            return true;
        }
        if (BuildModsRootFromModFolder("/shared2/Pulsar", modFolder, "mods", path, sizeof(path)) && ISFSDirHasEntries(path)) {
            SetModsRootPath(path);
            return true;
        }
    }

    if (ISFSDirHasEntries("/shared2/Pulsar/mods")) {
        SetModsRootPath("/shared2/Pulsar/mods");
        return true;
    }
    if (ISFSDirHasEntries("/shared2/Pulsar/Mods")) {
        SetModsRootPath("/shared2/Pulsar/Mods");
        return true;
    }
    return false;
}

static bool ModsRootExists() {
    if (sModsRootChecked) return sModsRootPresent;

    sModsRootChecked = true;
    sModsRootPresent = false;
    LogDiagBuildOnce();

    PulsarIOClass* io = PulsarIOClass::sInstance;
    const int ioType = io ? io->type : -1;

    OVERRIDE_LOG("Mods root probe: ioType=%d primary=%s alt=%s\n", ioType, kModsRoot, kModsRootAlt);
    sOverrideSource = GetOverrideSource(io ? io->type : IOType_RIIVO);

    sModsRootPresent = (sOverrideSource != OVERRIDE_SOURCE_NONE);

    if (sModsRootPresent) {
        OVERRIDE_LOG("Mods root set: %s (source=%d)\n", sModsRootPath, sOverrideSource);
    } else {
        OVERRIDE_WARN("Mods root missing for ioType=%d (overrides disabled).\n", ioType);
    }

    return sModsRootPresent;
}

static OverrideSource GetOverrideSource(IOType ioType) {
    if (sOverrideSourceChecked) return sOverrideSource;
    sOverrideSourceChecked = true;
    sOverrideSource = OVERRIDE_SOURCE_NONE;
    SetModsRootPath(kModsRoot);

    u32 modsIndex = 0;
    u32 modsEnd = 0;
    const char* rootPath = kModsRoot;
    if (FindModsDirInFST(modsIndex, modsEnd, rootPath)) {
        SetModsRootPath(rootPath);
        sOverrideSource = OVERRIDE_SOURCE_DVD;
        OVERRIDE_LOG("Override source selected: DVD (ioType=%d root=%s)\n", ioType, sModsRootPath);
        return sOverrideSource;
    }

    if (ResolveRiivoModsRoot()) {
        sOverrideSource = OVERRIDE_SOURCE_RIIVO;
        OVERRIDE_LOG("Override source selected: Riivolution (ioType=%d root=%s)\n", ioType, sModsRootPath);
        return sOverrideSource;
    }

    if (ioType == IOType_SD && ResolveSDModsRoot()) {
        sOverrideSource = OVERRIDE_SOURCE_SD;
        OVERRIDE_LOG("Override source selected: SD (ioType=%d root=%s)\n", ioType, sModsRootPath);
        return sOverrideSource;
    }

    if ((ioType == IOType_DOLPHIN || ioType == IOType_ISO) && ResolveISFSModsRoot()) {
        sOverrideSource = OVERRIDE_SOURCE_ISFS;
        OVERRIDE_LOG("Override source selected: ISFS (ioType=%d root=%s)\n", ioType, sModsRootPath);
        return sOverrideSource;
    }

    OVERRIDE_WARN("No override source found (ioType=%d).\n", ioType);
    return sOverrideSource;
}

static bool ReadRiivoFile(const OverrideEntry& entry, void* dest) {
    char fullPath[riivoMaxPath];
    const int written = snprintf(fullPath, sizeof(fullPath), "file%s", entry.fullPath);
    if (written <= 0 || static_cast<size_t>(written) >= sizeof(fullPath)) return false;
    s32 fd = PulsarIOClass::OpenFix(fullPath, static_cast<IOS::Mode>(RIIVO_MODE_READ));
    if (fd < 0) return false;

    u32 totalRead = 0;
    while (totalRead < entry.size) {
        const s32 readNow = IOS::Read(fd, reinterpret_cast<u8*>(dest) + totalRead, entry.size - totalRead);
        if (readNow <= 0) break;
        totalRead += static_cast<u32>(readNow);
    }
    IOS::Close(fd);
    return totalRead == entry.size;
}

static bool ReadSDFile(const OverrideEntry& entry, void* dest) {
    if (sSdVtable == nullptr || sSdVtable->open == nullptr || sSdVtable->read == nullptr || sSdVtable->close == nullptr) return false;
    file_struct file;
    if (sSdVtable->open(&file, entry.fullPath, O_RDONLY) == -1) return false;
    const int fd = reinterpret_cast<int>(&file);

    u32 totalRead = 0;
    while (totalRead < entry.size) {
        const int readNow = sSdVtable->read(fd, reinterpret_cast<u8*>(dest) + totalRead, entry.size - totalRead);
        if (readNow <= 0) break;
        totalRead += static_cast<u32>(readNow);
    }
    sSdVtable->close(fd);
    return totalRead == entry.size;
}

static bool ReadISFSFile(const OverrideEntry& entry, void* dest) {
    char tmp[OVERRIDE_MAX_PATH];
    strncpy(tmp, entry.fullPath, sizeof(tmp));
    tmp[sizeof(tmp) - 1] = '\0';
    s32 fd = ISFS::Open(tmp, ISFS::MODE_READ);
    if (fd < 0) return false;

    u32 totalRead = 0;
    while (totalRead < entry.size) {
        s32 readNow = ISFS::Read(fd, reinterpret_cast<u8*>(dest) + totalRead, entry.size - totalRead);
        if (readNow <= 0) break;
        totalRead += static_cast<u32>(readNow);
    }

    ISFS::Close(fd);
    return totalRead == entry.size;
}

static bool ReadOverrideFile(const OverrideEntry& entry, void* dest) {
    if (!ModsRootExists()) return false;

    switch (sOverrideSource) {
        case OVERRIDE_SOURCE_RIIVO:
            return ReadRiivoFile(entry, dest);
        case OVERRIDE_SOURCE_SD:
            return ReadSDFile(entry, dest);
        case OVERRIDE_SOURCE_ISFS:
            return ReadISFSFile(entry, dest);
        case OVERRIDE_SOURCE_DVD:
            return ReadDVDFile(entry.fullPath, dest, entry.size);
        default:
            break;
    }
    return false;
}

static u32 GetSDFileSize(const char* path) {
    if (sSdVtable == nullptr || sSdVtable->open == nullptr || sSdVtable->close == nullptr) return 0;
    file_struct file;
    if (sSdVtable->open(&file, path, O_RDONLY) == -1) return 0;
    const u32 size = file.filesize;
    const int fd = reinterpret_cast<int>(&file);
    sSdVtable->close(fd);
    return size;
}

static u32 GetISFSFileSize(const char* path) {
    char tmp[OVERRIDE_MAX_PATH];
    strncpy(tmp, path, sizeof(tmp));
    tmp[sizeof(tmp) - 1] = '\0';
    s32 fd = ISFS::Open(tmp, ISFS::MODE_READ);
    if (fd < 0) return 0;
    IOS::FileStats stats;
    u32 size = 0;
    if (ISFS::GetFileStats(fd, &stats) >= 0) {
        size = stats.size;
    }
    ISFS::Close(fd);
    return size;
}

static void FillOverrideEntry(OverrideEntry& entry, const char* fullPath, const char* relativePath, u32 size) {
    strncpy(entry.fullPath, fullPath, sizeof(entry.fullPath));
    entry.fullPath[sizeof(entry.fullPath) - 1] = '\0';
    strncpy(entry.relativePath, relativePath, sizeof(entry.relativePath));
    entry.relativePath[sizeof(entry.relativePath) - 1] = '\0';

    entry.hasSubpath = (strchr(relativePath, '/') != nullptr);
    entry.isTagged = false;
    entry.archiveTagLower[0] = '\0';

    const char* filename = FindLastChar(relativePath, '/');
    if (filename != nullptr) {
        filename++;
    } else {
        filename = relativePath;
    }

    const char* lastDot = FindLastChar(filename, '.');
    if (lastDot != nullptr && lastDot[1] != '\0') {
        entry.isTagged = true;
        ToLowerCopy(entry.archiveTagLower, lastDot + 1, sizeof(entry.archiveTagLower));
        const u32 prefixLen = static_cast<u32>(lastDot - relativePath);
        if (prefixLen < sizeof(entry.strippedName)) {
            memcpy(entry.strippedName, relativePath, prefixLen);
            entry.strippedName[prefixLen] = '\0';
        } else {
            entry.strippedName[0] = '\0';
        }
    } else {
        strncpy(entry.strippedName, relativePath, sizeof(entry.strippedName));
        entry.strippedName[sizeof(entry.strippedName) - 1] = '\0';
    }

    entry.size = size;
}

static bool CanAddEntry(OverrideEntry* entries, u32 maxCount, u32& count, bool& truncated) {
    if (count >= maxCount) {
        truncated = true;
        return false;
    }
    if (entries != nullptr && count >= maxCount) {
        truncated = true;
        return false;
    }
    return true;
}

static void AddEntry(OverrideEntry* entries, u32 maxCount, u32& count, bool& truncated,
                     const char* fullPath, const char* relativePath, u32 size) {
    if (!CanAddEntry(entries, maxCount, count, truncated)) return;
    if (fullPath == nullptr || relativePath == nullptr) return;
    if (strlen(fullPath) >= OVERRIDE_MAX_PATH || strlen(relativePath) >= OVERRIDE_MAX_PATH) {
        OVERRIDE_WARN("Override path too long, skipping: %s\n", fullPath ? fullPath : "<null>");
        return;
    }
    if (entries != nullptr) {
        FillOverrideEntry(entries[count], fullPath, relativePath, size);
    }
    ++count;
}

static void ScanModsDirRiivo(const char* absPath, const char* relPath, OverrideEntry* entries, u32 maxCount, u32& count, bool& truncated) {
    s32 deviceFd = -1;
    s32 dirFd = -1;
    if (!OpenRiivoDir(absPath, &deviceFd, &dirFd)) {
        OVERRIDE_WARN("Riivo scan open failed: %s\n", absPath);
        return;
    }
    OVERRIDE_LOG("Riivo scan open: path=%s deviceFd=%d dirFd=%d\n", absPath, deviceFd, dirFd);

    alignas(0x20) IOS::IOCtlvRequest request[3];
    alignas(0x20) char fileName[riivoMaxPath];
    alignas(0x20) RiivoStats stats;
    u32 listed = 0;

    while (count < maxCount) {
        request[0].address = &dirFd;
        request[0].size = 4;
        request[1].address = &fileName;
        request[1].size = riivoMaxPath;
        request[2].address = &stats;
        request[2].size = sizeof(RiivoStats);
        s32 ret = IOS::IOCtlv(deviceFd, static_cast<IOS::IOCtlType>(RIIVO_IOCTL_NEXTDIR), 1, 2, request);
        if (ret != 0) {
            OVERRIDE_LOG("Riivo NEXTDIR stop: path=%s ret=%d listed=%u\n", absPath, ret, listed);
            break;
        }
        if (IsDotEntry(fileName)) continue;
        ++listed;

        const bool isDir = (stats.mode & S_IFDIR) == S_IFDIR;
        if (listed <= 8) {
            OVERRIDE_LOG("Riivo entry[%u]: parent=%s name=%s dir=%d size=%u mode=%08x\n",
                         listed - 1, absPath, fileName, isDir ? 1 : 0, static_cast<u32>(stats.size),
                         static_cast<u32>(stats.mode));
        }

        char childAbs[OVERRIDE_MAX_PATH];
        char childRel[OVERRIDE_MAX_PATH];
        if (!BuildChildPaths(childAbs, sizeof(childAbs), childRel, sizeof(childRel), absPath, relPath, fileName)) {
            OVERRIDE_WARN("Override path too long under %s, skipping %s.\n", absPath, fileName);
            continue;
        }

        if (isDir) {
            ScanModsDirRiivo(childAbs, childRel, entries, maxCount, count, truncated);
            if (truncated) break;
        } else {
            AddEntry(entries, maxCount, count, truncated, childAbs, childRel, static_cast<u32>(stats.size));
            if (truncated) break;
        }
    }

    OVERRIDE_LOG("Riivo scan summary: path=%s listed=%u totalCount=%u truncated=%d\n",
                 absPath, listed, count, truncated ? 1 : 0);
    CloseRiivoDir(deviceFd, dirFd);
}

static void ScanModsDirSD(const char* absPath, const char* relPath, OverrideEntry* entries, u32 maxCount, u32& count, bool& truncated) {
    if (sSdVtable == nullptr || sSdVtable->diropen == nullptr || sSdVtable->dirnext == nullptr || sSdVtable->dirclose == nullptr) return;
    dir_struct dir;
    if (sSdVtable->diropen(&dir, absPath) != 0) return;

    char filename[768];
    stat st;
    while (count < maxCount && sSdVtable->dirnext(&dir, filename, &st) == 0) {
        if (IsDotEntry(filename)) continue;
        const bool isDir = ((st.st_mode & S_IFMT) == S_IFDIR);

        char childAbs[OVERRIDE_MAX_PATH];
        char childRel[OVERRIDE_MAX_PATH];
        if (!BuildChildPaths(childAbs, sizeof(childAbs), childRel, sizeof(childRel), absPath, relPath, filename)) {
            OVERRIDE_WARN("Override path too long under %s, skipping %s.\n", absPath, filename);
            continue;
        }

        if (isDir) {
            ScanModsDirSD(childAbs, childRel, entries, maxCount, count, truncated);
            if (truncated) break;
        } else {
            const u32 size = GetSDFileSize(childAbs);
            AddEntry(entries, maxCount, count, truncated, childAbs, childRel, size);
            if (truncated) break;
        }
    }

    sSdVtable->dirclose(&dir);
}

static void ScanModsDirISFS(const char* absPath, const char* relPath, OverrideEntry* entries, u32 maxCount, u32& count, bool& truncated) {
    u32 entryCount = 0;
    s32 ret = ISFS::ReadDir(absPath, nullptr, &entryCount);
    if (ret < 0 || entryCount == 0) return;

    EGG::Heap* heap = GetOverridesHeap();
    if (heap == nullptr) return;

    char* buffer = new (heap, 0x20) char[255 * (entryCount + 1)];
    if (buffer == nullptr) return;

    ret = ISFS::ReadDir(absPath, buffer, &entryCount);
    if (ret < 0) {
        EGG::Heap::free(buffer, heap);
        return;
    }

    char* cursor = buffer;
    while (cursor[0] != '\0' && count < maxCount) {
        const u32 length = strlen(cursor);
        if (length == 0) break;
        if (length > 255) break;
        if (IsDotEntry(cursor)) {
            cursor += length + 1;
            continue;
        }

        char childAbs[OVERRIDE_MAX_PATH];
        char childRel[OVERRIDE_MAX_PATH];
        if (!BuildChildPaths(childAbs, sizeof(childAbs), childRel, sizeof(childRel), absPath, relPath, cursor)) {
            OVERRIDE_WARN("Override path too long under %s, skipping %s.\n", absPath, cursor);
            cursor += length + 1;
            continue;
        }

        u32 subCount = 0;
        const bool isDir = ISFS::ReadDir(childAbs, nullptr, &subCount) >= 0;
        if (isDir) {
            ScanModsDirISFS(childAbs, childRel, entries, maxCount, count, truncated);
            if (truncated) break;
        } else {
            const u32 size = GetISFSFileSize(childAbs);
            AddEntry(entries, maxCount, count, truncated, childAbs, childRel, size);
            if (truncated) break;
        }

        cursor += length + 1;
    }

    EGG::Heap::free(buffer, heap);
}

static bool FindModsDirInFST(u32& outIndex, u32& outEnd, const char*& outRootPath) {
    if (OS::BootInfo::mInstance.FSTLocation == nullptr) {
        OVERRIDE_WARN("FST location unavailable; cannot scan /mods.\n");
        return false;
    }

    const FSTEntry* entries = static_cast<const FSTEntry*>(OS::BootInfo::mInstance.FSTLocation);
    const u32 entryCount = entries[0].size;
    if (entryCount == 0) {
        OVERRIDE_WARN("FST entry count is zero.\n");
        return false;
    }

    const char* stringTable = reinterpret_cast<const char*>(entries) + (entryCount * sizeof(FSTEntry));
    LogFSTProbeSummary(entries, entryCount, stringTable);

    if (ResolveFSTDirByPath(kModsRoot, entryCount, outIndex, outEnd)) {
        outRootPath = kModsRoot;
        return true;
    }
    if (ResolveFSTDirByPath(kModsRootAlt, entryCount, outIndex, outEnd)) {
        outRootPath = kModsRootAlt;
        return true;
    }

    for (u32 i = 1; i < entryCount; ++i) {
        const FSTEntry& entry = entries[i];
        if (!FSTEntryIsDir(entry)) continue;
        if (entry.offset != 0) continue;  // only root children
        const char* name = stringTable + FSTNameOffset(entry);
        if (name == nullptr) continue;
        if (strcmp(name, "mods") == 0) {
            outIndex = i;
            outEnd = entry.size;
            outRootPath = kModsRoot;
            OVERRIDE_LOG("FST matched root /mods: index=%u end=%u\n", outIndex, outEnd);
            return true;
        }
    }

    for (u32 i = 1; i < entryCount; ++i) {
        const FSTEntry& entry = entries[i];
        if (!FSTEntryIsDir(entry)) continue;
        const char* name = stringTable + FSTNameOffset(entry);
        if (name == nullptr) continue;
        if (strcmp(name, "mods") == 0) {
            outIndex = i;
            outEnd = entry.size;
            outRootPath = kModsRoot;
            OVERRIDE_WARN("FST fallback matched non-root mods dir: index=%u parent=%u end=%u\n",
                          outIndex, entry.offset, outEnd);
            return true;
        }
    }

    for (u32 i = 1; i < entryCount; ++i) {
        const FSTEntry& entry = entries[i];
        if (!FSTEntryIsDir(entry)) continue;
        const char* name = stringTable + FSTNameOffset(entry);
        if (name == nullptr) continue;
        if (strcmp(name, "Mods") == 0) {
            outIndex = i;
            outEnd = entry.size;
            outRootPath = kModsRootAlt;
            OVERRIDE_WARN("FST fallback matched non-root Mods dir: index=%u parent=%u end=%u\n",
                          outIndex, entry.offset, outEnd);
            return true;
        }
    }

    OVERRIDE_WARN("FST did not expose root /mods or /Mods.\n");
    return false;
}

static void ScanModsDirDVD(OverrideEntry* entries, u32 maxCount, u32& count, bool& truncated) {
    u32 modsIndex = 0;
    u32 modsEnd = 0;
    const char* rootPath = kModsRoot;
    if (!FindModsDirInFST(modsIndex, modsEnd, rootPath)) {
        OVERRIDE_WARN("No /mods directory found in FST.\n");
        return;
    }

    SetModsRootPath(rootPath);
    sModsRootPresent = true;

    const FSTEntry* fst = static_cast<const FSTEntry*>(OS::BootInfo::mInstance.FSTLocation);
    const u32 entryCount = fst[0].size;
    if (modsIndex >= entryCount || modsEnd > entryCount || modsEnd <= modsIndex) {
        OVERRIDE_WARN("FST /mods bounds invalid (index=%u end=%u count=%u).\n", modsIndex, modsEnd, entryCount);
        return;
    }
    const char* stringTable = reinterpret_cast<const char*>(fst) + (entryCount * sizeof(FSTEntry));

    struct DirStackEntry {
        u32 endIndex;
        u32 prevLen;
    };

    DirStackEntry stack[32];
    u32 depth = 0;
    char relPath[OVERRIDE_MAX_PATH];
    u32 relLen = 0;
    relPath[0] = '\0';

    for (u32 i = modsIndex + 1; i < modsEnd && count < maxCount; ++i) {
        while (depth > 0 && i >= stack[depth - 1].endIndex) {
            relLen = stack[depth - 1].prevLen;
            relPath[relLen] = '\0';
            --depth;
        }

        const FSTEntry& entry = fst[i];
        const char* name = stringTable + FSTNameOffset(entry);
        if (name == nullptr || name[0] == '\0') continue;

        if (FSTEntryIsDir(entry)) {
            if (depth >= 32) {
                OVERRIDE_WARN("FST /mods depth exceeded; skipping %s.\n", name);
                continue;
            }
            const u32 prevLen = relLen;
            if (!AppendPath(relPath, sizeof(relPath), relLen, name)) {
                OVERRIDE_WARN("Override path too long under %s, skipping %s.\n", rootPath, name);
                relLen = prevLen;
                relPath[relLen] = '\0';
                continue;
            }
            stack[depth].endIndex = entry.size;
            stack[depth].prevLen = prevLen;
            ++depth;
            continue;
        }

        char relativePath[OVERRIDE_MAX_PATH];
        int relWritten = 0;
        if (relLen > 0) {
            relWritten = snprintf(relativePath, sizeof(relativePath), "%s/%s", relPath, name);
        } else {
            relWritten = snprintf(relativePath, sizeof(relativePath), "%s", name);
        }
        if (relWritten <= 0 || static_cast<u32>(relWritten) >= sizeof(relativePath)) {
            OVERRIDE_WARN("Override path too long under %s, skipping %s.\n", rootPath, name);
            continue;
        }

        char fullPath[OVERRIDE_MAX_PATH];
        if (!BuildOverridePathWithRoot(rootPath, relativePath, nullptr, fullPath, sizeof(fullPath))) {
            OVERRIDE_WARN("Override path truncated for %s/%s.\n", rootPath, relativePath);
            continue;
        }

        AddEntry(entries, maxCount, count, truncated, fullPath, relativePath, entry.size);
        if (truncated) break;
    }
}

static void ScanModsDir(const char* absPath, const char* relPath, OverrideEntry* entries, u32 maxCount, u32& count, bool& truncated) {
    if (!ModsRootExists()) return;

    switch (sOverrideSource) {
        case OVERRIDE_SOURCE_RIIVO:
            ScanModsDirRiivo(absPath, relPath, entries, maxCount, count, truncated);
            break;
        case OVERRIDE_SOURCE_SD:
            ScanModsDirSD(absPath, relPath, entries, maxCount, count, truncated);
            break;
        case OVERRIDE_SOURCE_ISFS:
            ScanModsDirISFS(absPath, relPath, entries, maxCount, count, truncated);
            break;
        case OVERRIDE_SOURCE_DVD:
            ScanModsDirDVD(entries, maxCount, count, truncated);
            break;
        default:
            break;
    }
}

static void EnsureModIndexBuilt() {
    if (sModIndexAttempted) return;

    if (!ModsRootExists()) {
        sModIndex.entries = nullptr;
        sModIndex.count = 0;
        sModIndexAttempted = true;
        return;
    }

    EGG::Heap* heap = GetOverridesHeap();
    if (heap == nullptr) {
        OVERRIDE_WARN("Mod index build skipped (heap unavailable).\n");
        sModIndex.entries = nullptr;
        sModIndex.count = 0;
        return;
    }

    sModIndexAttempted = true;

    OVERRIDE_LOG("Mod index build start: source=%d root=%s\n", sOverrideSource, sModsRootPath);
    u32 count = 0;
    bool truncated = false;
    ScanModsDir(sModsRootPath, "", nullptr, kMaxOverridesTotal, count, truncated);
    if (count == 0 && sOverrideSource == OVERRIDE_SOURCE_RIIVO) {
        u32 modsIndex = 0;
        u32 modsEnd = 0;
        const char* rootPath = kModsRoot;
        if (FindModsDirInFST(modsIndex, modsEnd, rootPath)) {
            OVERRIDE_WARN("Riivo /mods enumeration returned zero entries; retrying via DVD/FST at %s.\n", rootPath);
            SetModsRootPath(rootPath);
            sOverrideSource = OVERRIDE_SOURCE_DVD;
            OVERRIDE_LOG("Mod index retry: source=%d root=%s\n", sOverrideSource, sModsRootPath);
            ScanModsDirDVD(nullptr, kMaxOverridesTotal, count, truncated);
        }
    }
    if (count >= kMaxOverridesTotal) {
        truncated = true;
    }
    if (count == 0) {
        sModIndex.entries = nullptr;
        sModIndex.count = 0;
        OVERRIDE_LOG("No overrides found under %s.\n", sModsRootPath);
        return;
    }

    OverrideEntry* entries = EGG::Heap::alloc<OverrideEntry>(sizeof(OverrideEntry) * count, 0x20, heap);
    if (entries == nullptr) {
        OVERRIDE_WARN("Mod index allocation failed (count=%u).\n", count);
        sModIndex.entries = nullptr;
        sModIndex.count = 0;
        return;
    }

    u32 filled = 0;
    bool truncatedFill = false;
    ScanModsDir(sModsRootPath, "", entries, count, filled, truncatedFill);

    sModIndex.entries = entries;
    sModIndex.count = filled;

    if (truncated || truncatedFill) {
        OVERRIDE_WARN("Mod index truncated at %u entries (max=%u).\n", filled, kMaxOverridesTotal);
    }
    u32 taggedCount = 0;
    for (u32 i = 0; i < filled; ++i) {
        if (entries[i].isTagged) ++taggedCount;
    }
    OVERRIDE_LOG("Mod index built: %u overrides (tagged=%u).\n", filled, taggedCount);
    if (taggedCount == 0) {
        OVERRIDE_WARN("No tagged overrides found under %s. Use <file>.<archiveBase> (e.g. foo.tpl.title).\n",
                      sModsRootPath);
    }
}

static bool IsFileExtensionSZS(const char* path) {
    return EndsWithIgnoreCase(path, ".szs");
}

static u32 GetFileDataStart(const ARC::Header* header) {
    if (header == nullptr) return 0;
    const u32 metaEnd = header->nodeOffset + header->combinedNodeSize;
    u32 dataStart = header->fileOffset;
    if (dataStart < metaEnd) dataStart = metaEnd;
    return nw4r::ut::RoundUp(dataStart, 0x20);
}

}  // namespace

bool IsModsPath(const char* path) {
    if (path == nullptr) return false;
    if (strcmp(path, kModsRoot) == 0 || strcmp(path, kModsRootAlt) == 0) return true;
    if (StartsWith(path, kModsRootPrefix) || StartsWith(path, kModsRootAltPrefix)) return true;

    const u32 rootLen = strlen(sModsRootPath);
    if (rootLen == 0) return false;
    if (strncmp(path, sModsRootPath, rootLen) != 0) return false;
    return path[rootLen] == '\0' || path[rootLen] == '/';
}

const char* ResolveWholeFileOverride(const char* path, char* resolvedPath, u32 resolvedSize, bool* outRedirected) {
    if (outRedirected != nullptr) *outRedirected = false;
    if (path == nullptr || resolvedPath == nullptr || resolvedSize == 0) return path;

    if (IsModsPath(path)) return path;
    if (!ModsRootExists()) return path;

    const char* base = FindBasename(path);
    if (base == nullptr || base[0] == '\0') return path;

    bool exists = false;
    switch (sOverrideSource) {
        case OVERRIDE_SOURCE_RIIVO: {
            const int written = snprintf(resolvedPath, resolvedSize, "%s/%s", sModsRootPath, base);
            if (written <= 0 || static_cast<u32>(written) >= resolvedSize) {
                OVERRIDE_WARN("Override path truncated for %s (buffer=%u).\n", path, resolvedSize);
                return path;
            }
            exists = RiivoFileExists(resolvedPath);
            break;
        }
        case OVERRIDE_SOURCE_SD: {
            const int written = snprintf(resolvedPath, resolvedSize, "%s/%s", sModsRootPath, base);
            if (written <= 0 || static_cast<u32>(written) >= resolvedSize) {
                OVERRIDE_WARN("Override path truncated for %s (buffer=%u).\n", path, resolvedSize);
                return path;
            }
            exists = SDFileExists(resolvedPath);
            break;
        }
        case OVERRIDE_SOURCE_ISFS: {
            const int written = snprintf(resolvedPath, resolvedSize, "%s/%s", sModsRootPath, base);
            if (written <= 0 || static_cast<u32>(written) >= resolvedSize) {
                OVERRIDE_WARN("Override path truncated for %s (buffer=%u).\n", path, resolvedSize);
                return path;
            }
            exists = ISFSFileExists(resolvedPath);
            break;
        }
        case OVERRIDE_SOURCE_DVD: {
            if (!sModIndexAttempted) {
                EnsureModIndexBuilt();
            }
            if (sModIndex.entries != nullptr && sModIndex.count > 0) {
                for (u32 i = 0; i < sModIndex.count; ++i) {
                    if (strcmp(sModIndex.entries[i].relativePath, base) == 0) {
                        if (BuildOverridePathWithRoot(sModsRootPath, base, nullptr, resolvedPath, resolvedSize)) {
                            exists = true;
                        }
                        break;
                    }
                }
            }
            break;
        }
        default:
            break;
    }

    if (exists) {
        if (outRedirected != nullptr) *outRedirected = true;
        OVERRIDE_LOG("Whole-file override: %s -> %s\n", path, resolvedPath);
        return resolvedPath;
    }

    return path;
}

bool ShouldApplyLooseOverrides(const char* path, char* archiveBaseLower, u32 archiveBaseLowerSize) {
    if (path == nullptr || archiveBaseLower == nullptr || archiveBaseLowerSize == 0) return false;
    if (IsModsPath(path)) return false;
    if (!IsFileExtensionSZS(path)) return false;

    const char* base = FindBasename(path);
    if (base == nullptr) return false;

    const size_t baseLen = strlen(base);
    if (baseLen <= 4) return false;
    const size_t nameLen = baseLen - 4;
    if (nameLen == 0) return false;

    const size_t copyLen = (nameLen + 1 < archiveBaseLowerSize) ? nameLen : archiveBaseLowerSize - 1;
    memcpy(archiveBaseLower, base, copyLen);
    archiveBaseLower[copyLen] = '\0';
    ToLowerInPlace(archiveBaseLower);
    return true;
}

bool ApplyLooseOverrides(const char* archiveBaseLower, u8*& archiveBase, u32& archiveSize, EGG::Heap* sourceHeap,
                         EGG::Heap*& archiveHeap, u32* outAppliedOverrides, u32* outPatchedNodes,
                         u32* outMissingOverrides) {
    if (outAppliedOverrides != nullptr) *outAppliedOverrides = 0;
    if (outPatchedNodes != nullptr) *outPatchedNodes = 0;
    if (outMissingOverrides != nullptr) *outMissingOverrides = 0;

    EnsureModIndexBuilt();
    if (sModIndex.entries == nullptr || sModIndex.count == 0) return false;
    if (archiveBase == nullptr || archiveBaseLower == nullptr || archiveBaseLower[0] == '\0') return false;
    if (sourceHeap == nullptr) {
        OVERRIDE_WARN("Override heap unavailable for %s.\n", archiveBaseLower);
        return false;
    }
    if (archiveHeap == nullptr) {
        archiveHeap = sourceHeap;
    }

    ARC::Handle handle;
    if (!ARC::InitHandle(archiveBase, &handle)) {
        OVERRIDE_WARN("ARC::InitHandle failed for archive %s.\n", archiveBaseLower);
        return false;
    }

    ARC::Header* header = reinterpret_cast<ARC::Header*>(archiveBase);
    U8Node* nodes = reinterpret_cast<U8Node*>(archiveBase + header->nodeOffset);
    const u32 nodeCount = nodes[0].dataSize;
    if (nodeCount == 0) {
        OVERRIDE_WARN("U8 node table empty for archive %s.\n", archiveBaseLower);
        return false;
    }

    char* stringTable = reinterpret_cast<char*>(nodes + nodeCount);
    s32* nodeOverrideIndex = EGG::Heap::alloc<s32>(sizeof(s32) * nodeCount, 0x20, sourceHeap);
    u8* entryApplied = EGG::Heap::alloc<u8>(sizeof(u8) * sModIndex.count, 0x20, sourceHeap);
    if (nodeOverrideIndex == nullptr || entryApplied == nullptr) {
        OVERRIDE_WARN("Override temp alloc failed (nodes=%u entries=%u).\n", nodeCount, sModIndex.count);
        if (nodeOverrideIndex != nullptr) EGG::Heap::free(nodeOverrideIndex, sourceHeap);
        if (entryApplied != nullptr) EGG::Heap::free(entryApplied, sourceHeap);
        return false;
    }

    memset(nodeOverrideIndex, 0xFF, sizeof(s32) * nodeCount);
    memset(entryApplied, 0, sizeof(u8) * sModIndex.count);

    bool anyOverrides = false;
    u32 missingOverrides = 0;
    u32 skippedOverrides = 0;

    for (u32 i = 0; i < sModIndex.count; ++i) {
        const OverrideEntry& entry = sModIndex.entries[i];
        const bool tagMatches = entry.isTagged && (strcmp(entry.archiveTagLower, archiveBaseLower) == 0);
        if (!tagMatches) {
            ++skippedOverrides;
            continue;
        }

        const char* matchName = entry.strippedName;
        if (matchName[0] == '\0') {
            OVERRIDE_WARN("Skip empty match name for override %s.\n", entry.fullPath);
            continue;
        }

        if (entry.hasSubpath) {
            s32 entryNum = ARC::ConvertPathToEntrynum(&handle, matchName);
            if (entryNum < 0) {
                OVERRIDE_WARN("Missing U8 path: %s (archive=%s).\n", matchName, archiveBaseLower);
                ++missingOverrides;
                continue;
            }
            if (NodeIsDir(nodes[entryNum])) {
                OVERRIDE_WARN("Override path is directory: %s (archive=%s).\n", matchName, archiveBaseLower);
                ++missingOverrides;
                continue;
            }
            nodeOverrideIndex[entryNum] = static_cast<s32>(i);
            anyOverrides = true;
        } else {
            u32 matchCount = 0;
            for (u32 nodeIdx = 1; nodeIdx < nodeCount; ++nodeIdx) {
                if (NodeIsDir(nodes[nodeIdx])) continue;
                const char* nodeName = stringTable + NodeNameOffset(nodes[nodeIdx]);
                if (strcmp(nodeName, matchName) == 0) {
                    nodeOverrideIndex[nodeIdx] = static_cast<s32>(i);
                    ++matchCount;
                }
            }
            if (matchCount == 0) {
                OVERRIDE_WARN("No matching filename in archive %s for %s.\n", archiveBaseLower, matchName);
                ++missingOverrides;
                continue;
            }
            anyOverrides = true;
        }
    }

    if (!anyOverrides) {
        if (outMissingOverrides != nullptr) *outMissingOverrides = missingOverrides;
        if (skippedOverrides > 0) {
            OVERRIDE_LOG("Ignored %u overrides without matching tag for %s.\n", skippedOverrides, archiveBaseLower);
        }
        EGG::Heap::free(nodeOverrideIndex, sourceHeap);
        EGG::Heap::free(entryApplied, sourceHeap);
        return false;
    }

    bool needsRepack = false;
    for (u32 nodeIdx = 1; nodeIdx < nodeCount; ++nodeIdx) {
        const s32 idx = nodeOverrideIndex[nodeIdx];
        if (idx < 0) continue;
        if (NodeIsDir(nodes[nodeIdx])) continue;
        if (sModIndex.entries[idx].size > nodes[nodeIdx].dataSize) {
            needsRepack = true;
            break;
        }
    }

    u32 patchedNodes = 0;
    if (!needsRepack) {
        for (u32 nodeIdx = 1; nodeIdx < nodeCount; ++nodeIdx) {
            const s32 idx = nodeOverrideIndex[nodeIdx];
            if (idx < 0) continue;
            if (NodeIsDir(nodes[nodeIdx])) continue;

            const OverrideEntry& entry = sModIndex.entries[idx];
            if (entry.size > nodes[nodeIdx].dataSize) {
                OVERRIDE_WARN("Override too large for in-place: %s (size=%u, nodeSize=%u, archive=%s).\n",
                              entry.fullPath, entry.size, nodes[nodeIdx].dataSize, archiveBaseLower);
                continue;
            }

            void* dest = archiveBase + nodes[nodeIdx].dataOffset;
            if (!ReadOverrideFile(entry, dest)) {
                OVERRIDE_WARN("Failed to read override file: %s\n", entry.fullPath);
                continue;
            }
            nodes[nodeIdx].dataSize = entry.size;
            OS::DCStoreRange(dest, entry.size);
            entryApplied[idx] = 1;
            ++patchedNodes;
        }

        if (patchedNodes > 0) {
            OS::DCStoreRange(nodes, nodeCount * sizeof(U8Node));
        }

        u32 appliedOverrides = 0;
        for (u32 i = 0; i < sModIndex.count; ++i) {
            if (entryApplied[i]) ++appliedOverrides;
        }

        if (outAppliedOverrides != nullptr) *outAppliedOverrides = appliedOverrides;
        if (outPatchedNodes != nullptr) *outPatchedNodes = patchedNodes;
        if (outMissingOverrides != nullptr) *outMissingOverrides = missingOverrides;

        OVERRIDE_LOG("Applied overrides to %s: files=%u nodes=%u missing=%u (in-place).\n",
                     archiveBaseLower, appliedOverrides, patchedNodes, missingOverrides);
        if (skippedOverrides > 0) {
            OVERRIDE_LOG("Ignored %u overrides without matching tag for %s.\n", skippedOverrides, archiveBaseLower);
        }

        EGG::Heap::free(nodeOverrideIndex, sourceHeap);
        EGG::Heap::free(entryApplied, sourceHeap);
        return appliedOverrides > 0;
    }

    const u32 dataStart = GetFileDataStart(header);
    if (dataStart == 0 || dataStart > archiveSize) {
        OVERRIDE_WARN("Archive data start invalid for %s (start=0x%X size=0x%X).\n",
                      archiveBaseLower, dataStart, archiveSize);
        EGG::Heap::free(nodeOverrideIndex, sourceHeap);
        EGG::Heap::free(entryApplied, sourceHeap);
        return false;
    }

    u32 totalDataSize = 0;
    for (u32 nodeIdx = 1; nodeIdx < nodeCount; ++nodeIdx) {
        if (NodeIsDir(nodes[nodeIdx])) continue;
        const s32 idx = nodeOverrideIndex[nodeIdx];
        u32 size = nodes[nodeIdx].dataSize;
        if (idx >= 0) {
            size = sModIndex.entries[idx].size;
        }
        totalDataSize += nw4r::ut::RoundUp(size, 0x20);
    }

    u32 newSize = dataStart + totalDataSize;
    newSize = nw4r::ut::RoundUp(newSize, 0x20);

    const u32 growth = (newSize > archiveSize) ? (newSize - archiveSize) : 0;
    EGG::Heap* repackHeap = archiveHeap;

    EGG::Heap* candidates[3];
    candidates[0] = RKSystem::mInstance.EGGRootMEM2;
    candidates[1] = RKSystem::mInstance.EGGRootMEM1;
    candidates[2] = GetOverridesHeap();

    for (u32 i = 0; i < 3; ++i) {
        EGG::Heap* candidate = candidates[i];
        if (candidate == nullptr || candidate == archiveHeap) continue;
        const u32 available = candidate->getAllocatableSize(0x20);
        if (available < newSize) continue;
        repackHeap = candidate;
        break;
    }

    const bool allowSourceHeap = (growth <= kOverrideMaxGrowthOnSourceHeap);
    bool triedSourceHeap = false;
    u8* newBuffer = nullptr;

    if (repackHeap == archiveHeap && !allowSourceHeap) {
        OVERRIDE_WARN("Repack skipped on source heap (growth=0x%X, limit=0x%X) for %s.\n",
                      growth, kOverrideMaxGrowthOnSourceHeap, archiveBaseLower);
    } else {
        newBuffer = static_cast<u8*>(EGG::Heap::alloc(newSize, 0x20, repackHeap));
    }
    if (newBuffer == nullptr && repackHeap != archiveHeap) {
        OVERRIDE_WARN("Override repack alloc failed on fallback heap (size=0x%X).\n", newSize);
        if (allowSourceHeap) {
            repackHeap = archiveHeap;
            newBuffer = static_cast<u8*>(EGG::Heap::alloc(newSize, 0x20, repackHeap));
            triedSourceHeap = true;
        }
    }
    if (newBuffer == nullptr && repackHeap == archiveHeap && !triedSourceHeap && allowSourceHeap) {
        newBuffer = static_cast<u8*>(EGG::Heap::alloc(newSize, 0x20, repackHeap));
        triedSourceHeap = true;
    }
    if (newBuffer == nullptr) {
        OVERRIDE_WARN("Override repack alloc failed (size=0x%X). Falling back to in-place.\n", newSize);
        for (u32 nodeIdx = 1; nodeIdx < nodeCount; ++nodeIdx) {
            const s32 idx = nodeOverrideIndex[nodeIdx];
            if (idx < 0) continue;
            if (NodeIsDir(nodes[nodeIdx])) continue;
            const OverrideEntry& entry = sModIndex.entries[idx];
            if (entry.size > nodes[nodeIdx].dataSize) {
                OVERRIDE_WARN("Override too large, skipped: %s (size=%u, nodeSize=%u, archive=%s).\n",
                              entry.fullPath, entry.size, nodes[nodeIdx].dataSize, archiveBaseLower);
                continue;
            }
            void* dest = archiveBase + nodes[nodeIdx].dataOffset;
            if (!ReadOverrideFile(entry, dest)) {
                OVERRIDE_WARN("Failed to read override file: %s\n", entry.fullPath);
                continue;
            }
            nodes[nodeIdx].dataSize = entry.size;
            OS::DCStoreRange(dest, entry.size);
            entryApplied[idx] = 1;
            ++patchedNodes;
        }

        if (patchedNodes > 0) {
            OS::DCStoreRange(nodes, nodeCount * sizeof(U8Node));
        }

        u32 appliedOverrides = 0;
        for (u32 i = 0; i < sModIndex.count; ++i) {
            if (entryApplied[i]) ++appliedOverrides;
        }

        if (outAppliedOverrides != nullptr) *outAppliedOverrides = appliedOverrides;
        if (outPatchedNodes != nullptr) *outPatchedNodes = patchedNodes;
        if (outMissingOverrides != nullptr) *outMissingOverrides = missingOverrides;

        OVERRIDE_LOG("Applied overrides to %s: files=%u nodes=%u missing=%u (partial).\n",
                     archiveBaseLower, appliedOverrides, patchedNodes, missingOverrides);
        if (skippedOverrides > 0) {
            OVERRIDE_LOG("Ignored %u overrides without matching tag for %s.\n", skippedOverrides, archiveBaseLower);
        }

        EGG::Heap::free(nodeOverrideIndex, sourceHeap);
        EGG::Heap::free(entryApplied, sourceHeap);
        return appliedOverrides > 0;
    }

    memcpy(newBuffer, archiveBase, dataStart);
    ARC::Header* newHeader = reinterpret_cast<ARC::Header*>(newBuffer);
    newHeader->fileOffset = dataStart;
    U8Node* newNodes = reinterpret_cast<U8Node*>(newBuffer + newHeader->nodeOffset);

    u32 writeOffset = dataStart;
    for (u32 nodeIdx = 1; nodeIdx < nodeCount; ++nodeIdx) {
        if (NodeIsDir(nodes[nodeIdx])) continue;

        const s32 idx = nodeOverrideIndex[nodeIdx];
        const u32 oldOffset = nodes[nodeIdx].dataOffset;
        const u32 oldSize = nodes[nodeIdx].dataSize;
        bool useOverride = (idx >= 0);
        u32 newFileSize = useOverride ? sModIndex.entries[idx].size : oldSize;

        writeOffset = nw4r::ut::RoundUp(writeOffset, 0x20);
        const u32 alignedSize = nw4r::ut::RoundUp(newFileSize, 0x20);
        if (writeOffset + alignedSize > newSize) {
            OVERRIDE_WARN("Repack overflow for %s; keeping original data for node %u.\n", archiveBaseLower, nodeIdx);
            useOverride = false;
            newFileSize = oldSize;
        }

        newNodes[nodeIdx].dataOffset = writeOffset;
        if (useOverride) {
            const OverrideEntry& entry = sModIndex.entries[idx];
            if (!ReadOverrideFile(entry, newBuffer + writeOffset)) {
                OVERRIDE_WARN("Failed to read override file: %s\n", entry.fullPath);
                useOverride = false;
                newFileSize = oldSize;
            } else {
                entryApplied[idx] = 1;
                ++patchedNodes;
            }
        }

        if (!useOverride) {
            memcpy(newBuffer + writeOffset, archiveBase + oldOffset, oldSize);
        }

        newNodes[nodeIdx].dataSize = newFileSize;
        writeOffset += nw4r::ut::RoundUp(newFileSize, 0x20);
    }

    u32 finalSize = nw4r::ut::RoundUp(writeOffset, 0x20);
    if (finalSize > newSize) finalSize = newSize;
    OS::DCStoreRange(newBuffer, finalSize);

    if (repackHeap != archiveHeap) {
        OVERRIDE_LOG("Repack heap switched for %s (growth=0x%X).\n", archiveBaseLower, growth);
    }

    EGG::Heap::free(archiveBase, sourceHeap);
    archiveBase = newBuffer;
    archiveSize = finalSize;
    archiveHeap = repackHeap;

    u32 appliedOverrides = 0;
    for (u32 i = 0; i < sModIndex.count; ++i) {
        if (entryApplied[i]) ++appliedOverrides;
    }

    if (outAppliedOverrides != nullptr) *outAppliedOverrides = appliedOverrides;
    if (outPatchedNodes != nullptr) *outPatchedNodes = patchedNodes;
    if (outMissingOverrides != nullptr) *outMissingOverrides = missingOverrides;

    OVERRIDE_LOG("Applied overrides to %s: files=%u nodes=%u missing=%u size=0x%X.\n",
                 archiveBaseLower, appliedOverrides, patchedNodes, missingOverrides, archiveSize);
    if (skippedOverrides > 0) {
        OVERRIDE_LOG("Ignored %u overrides without matching tag for %s.\n", skippedOverrides, archiveBaseLower);
    }

    EGG::Heap::free(nodeOverrideIndex, sourceHeap);
    EGG::Heap::free(entryApplied, sourceHeap);
    return appliedOverrides > 0;
}

static void ArchiveFileLoadOverride(ArchiveFile* file, const char* path, EGG::Heap* mountHeap, bool isCompressed,
                                    s32 allocDirection, EGG::Heap* dumpHeap, EGG::Archive::FileInfo* info) {
    char resolvedPath[OVERRIDE_MAX_PATH];
    bool redirected = false;
    const char* finalPath = ResolveWholeFileOverride(path, resolvedPath, sizeof(resolvedPath), &redirected);

    if ((isCompressed == 0) || (dumpHeap == nullptr)) {
        dumpHeap = mountHeap;
    }

    if (file->status == ARCHIVE_STATUS_NONE) {
        bool ripped = false;
        EGG::DvdRipper::EAllocDirection ripAlloc = EGG::DvdRipper::ALLOC_FROM_HEAD;
        s32 align = -8;
        if (isCompressed == 0) {
            align = allocDirection;
        }
        if (align < 0) {
            ripAlloc = EGG::DvdRipper::ALLOC_FROM_TAIL;
        }

        void* rippedData = EGG::DvdRipper::LoadToMainRAM(finalPath, nullptr, dumpHeap, ripAlloc, 0, nullptr,
                                                         &file->compressedArchiveSize);
        file->compressedArchive = rippedData;

        if (file->compressedArchiveSize == 0 || rippedData == nullptr) {
            file->compressedArchiveSize = 0;
            if (redirected) {
                OVERRIDE_WARN("Whole-file override load failed: %s -> %s\n", path, finalPath);
            }
        } else {
            file->dumpHeap = dumpHeap;
            ripped = true;
        }

        file->status = ripped ? ARCHIVE_STATUS_DUMPED : ARCHIVE_STATUS_NONE;
    }

    if (file->status >= ARCHIVE_STATUS_DUMPED) {
        if (isCompressed == 0) {
            file->rawArchive = file->compressedArchive;
            file->archiveSize = file->compressedArchiveSize;
            file->archiveHeap = file->dumpHeap;
            file->compressedArchive = nullptr;
            file->compressedArchiveSize = 0;
            file->dumpHeap = nullptr;
            file->status = ARCHIVE_STATUS_DECOMPRESSED;
        } else {
            file->Decompress(finalPath, mountHeap, info);
            if (file->compressedArchive != nullptr && file->dumpHeap != nullptr) {
                EGG::Heap::free(file->compressedArchive, file->dumpHeap);
                file->compressedArchive = nullptr;
                file->compressedArchiveSize = 0;
                file->dumpHeap = nullptr;
            }
        }

        EGG::Archive* mounted = nullptr;
        if (file->rawArchive != nullptr) {
            mounted = EGG::Archive::Mount(file->rawArchive, mountHeap, 4);
        }
        file->archive = mounted;
        file->status = mounted ? ARCHIVE_STATUS_MOUNTED : ARCHIVE_STATUS_NONE;
    }
}
kmBranch(0x80518e10, ArchiveFileLoadOverride);

}  // namespace IOOverrides
}  // namespace Pulsar
