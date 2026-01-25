#include <IO/LooseArchiveOverrides.hpp>
#include <IO/IO.hpp>
#include <IO/RiivoIO.hpp>
#include <IO/SDIO.hpp>
#include <core/nw4r/ut/Misc.hpp>
#include <core/rvl/arc/arc.hpp>
#include <core/rvl/devfs/isfs.hpp>
#include <core/rvl/ipc/ipc.hpp>
#include <core/rvl/os/OS.hpp>
#include <core/rvl/os/OSCache.hpp>
#include <include/c_stdio.h>
#include <include/c_string.h>

namespace Pulsar {
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

extern const sd_vtable* __sd_vtable;
}  // namespace Pulsar

namespace Pulsar {
namespace IOOverrides {
namespace {

const char kOverrideRoot[] = "/files/Mods";
const char kOverrideRootNoSlash[] = "files/Mods";
const u32 kOverrideAlignment = 0x20;
const u32 kMaxOverrides = 256;
const u32 kMaxPath = OVERRIDE_MAX_PATH;

struct U8Node {
    u32 typeName;
    u32 dataOffset;
    u32 dataSize;
};

static char ToLowerAscii(char c) {
    if (c >= 'A' && c <= 'Z') {
        return static_cast<char>(c + ('a' - 'A'));
    }
    return c;
}

static bool EqualsIgnoreCase(const char* lhs, const char* rhs) {
    if (!lhs || !rhs) {
        return false;
    }
    while (*lhs && *rhs) {
        if (ToLowerAscii(*lhs) != ToLowerAscii(*rhs)) {
            return false;
        }
        ++lhs;
        ++rhs;
    }
    return *lhs == '\0' && *rhs == '\0';
}

static bool EndsWithIgnoreCase(const char* value, const char* suffix) {
    if (!value || !suffix) {
        return false;
    }
    const size_t valueLen = strlen(value);
    const size_t suffixLen = strlen(suffix);
    if (suffixLen > valueLen) {
        return false;
    }
    const char* start = value + (valueLen - suffixLen);
    return EqualsIgnoreCase(start, suffix);
}

static bool StartsWith(const char* value, const char* prefix) {
    if (!value || !prefix) {
        return false;
    }
    const size_t prefixLen = strlen(prefix);
    return strncmp(value, prefix, prefixLen) == 0;
}

static const char* FindBaseName(const char* path) {
    if (!path) {
        return "";
    }
    const char* last = path;
    for (const char* cur = path; *cur; ++cur) {
        if (*cur == '/') {
            last = cur + 1;
        }
    }
    return last;
}

static const char* FindLastChar(const char* value, char target) {
    if (!value) {
        return 0;
    }
    const char* last = 0;
    for (const char* cur = value; *cur; ++cur) {
        if (*cur == target) {
            last = cur;
        }
    }
    return last;
}

static bool IsDirNode(const U8Node& node);

static bool NameEqualsSegment(const char* name, const char* segStart, size_t segLen) {
    if (!name || !segStart) {
        return false;
    }
    const size_t nameLen = strlen(name);
    return nameLen == segLen && strncmp(name, segStart, segLen) == 0;
}

static s32 FindChildInDir(U8Node* nodes, u32 nodeCount, char* stringTable, u32 dirIndex, u32 dirEnd,
                          const char* segStart, size_t segLen) {
    if (!nodes || !stringTable || dirIndex >= nodeCount || dirEnd > nodeCount) {
        return -1;
    }
    u32 i = dirIndex + 1;
    while (i < dirEnd) {
        const U8Node& node = nodes[i];
        const char* name = stringTable + (node.typeName & 0x00FFFFFF);
        if (NameEqualsSegment(name, segStart, segLen)) {
            return static_cast<s32>(i);
        }
        if (IsDirNode(node)) {
            i = node.dataSize;
        } else {
            ++i;
        }
    }
    return -1;
}

static s32 FindU8EntryByPath(U8Node* nodes, u32 nodeCount, char* stringTable, const char* path) {
    if (!nodes || !stringTable || !path || nodeCount == 0) {
        return -1;
    }
    const char* cursor = path;
    if (*cursor == '/') {
        ++cursor;
    }
    u32 dirIndex = 0;
    u32 dirEnd = nodes[0].dataSize;
    while (*cursor) {
        const char* segStart = cursor;
        while (*cursor && *cursor != '/') {
            ++cursor;
        }
        const size_t segLen = static_cast<size_t>(cursor - segStart);
        if (segLen == 0) {
            if (*cursor == '/') {
                ++cursor;
                continue;
            }
            break;
        }
        const s32 childIndex = FindChildInDir(nodes, nodeCount, stringTable, dirIndex, dirEnd, segStart, segLen);
        if (childIndex < 0) {
            return -1;
        }
        if (*cursor == '\0') {
            return childIndex;
        }
        if (!IsDirNode(nodes[childIndex])) {
            return -1;
        }
        dirIndex = static_cast<u32>(childIndex);
        dirEnd = nodes[dirIndex].dataSize;
        while (*cursor == '/') {
            ++cursor;
        }
    }
    return static_cast<s32>(dirIndex);
}

static bool IsDirNode(const U8Node& node) {
    return (node.typeName >> 24) != 0;
}

static bool BuildArchiveBaseName(const char* archivePath, char* outName, u32 outSize) {
    if (!archivePath || !outName || outSize == 0) {
        return false;
    }
    const char* base = FindBaseName(archivePath);
    const size_t baseLen = strlen(base);
    if (baseLen + 1 > outSize) {
        return false;
    }
    strncpy(outName, base, outSize);
    outName[outSize - 1] = '\0';
    char* dot = const_cast<char*>(FindLastChar(outName, '.'));
    if (dot) {
        *dot = '\0';
    }
    return true;
}

static bool GetFullOverridePath(const char* filename, char* outPath, u32 outSize) {
    if (!filename || !outPath || outSize == 0) {
        return false;
    }
    const int written = snprintf(outPath, outSize, "%s/%s", kOverrideRoot, filename);
    if (written <= 0 || static_cast<u32>(written) >= outSize) {
        return false;
    }
    return true;
}

static bool GetFileSizeFromIO(const char* path, u32& outSize) {
    IO* io = IO::sInstance;
    if (!io) {
        OS::Report("[Pulsar][Mods] IO instance missing; cannot read %s\n", path ? path : "<null>");
        return false;
    }
    if (!io->OpenFile(path, FILE_MODE_READ)) {
        return false;
    }
    const s32 size = io->GetFileSize();
    io->Close();
    if (size <= 0) {
        return false;
    }
    outSize = static_cast<u32>(size);
    return true;
}

static bool ReadFileIntoBuffer(const char* path, void* buffer, u32 size) {
    IO* io = IO::sInstance;
    if (!io) {
        OS::Report("[Pulsar][Mods] IO instance missing; cannot read %s\n", path ? path : "<null>");
        return false;
    }
    if (!io->OpenFile(path, FILE_MODE_READ)) {
        OS::Report("[Pulsar][Mods] Failed to open override file %s\n", path);
        return false;
    }
    u32 totalRead = 0;
    while (totalRead < size) {
        const u32 remaining = size - totalRead;
        s32 read = io->Read(remaining, reinterpret_cast<u8*>(buffer) + totalRead);
        if (read <= 0) {
            io->Close();
            OS::Report("[Pulsar][Mods] Read failed for %s (read %d, expected %u)\n", path, read, size);
            return false;
        }
        totalRead += static_cast<u32>(read);
    }
    io->Close();
    return true;
}

static bool AddOverrideEntry(OverrideList& list, const char* fullPath, const char* relativePath) {
    if (list.count >= kMaxOverrides) {
        OS::Report("[Pulsar][Mods] Override cap reached (%u). Skipping %s\n", kMaxOverrides, fullPath);
        return false;
    }
    u32 size = 0;
    if (!GetFileSizeFromIO(fullPath, size)) {
        OS::Report("[Pulsar][Mods] Failed to read size for override %s\n", fullPath);
        return false;
    }

    OverrideEntry& entry = list.entries[list.count];
    strncpy(entry.fullPath, fullPath, kMaxPath);
    entry.fullPath[kMaxPath - 1] = '\0';
    strncpy(entry.relativePath, relativePath, kMaxPath);
    entry.relativePath[kMaxPath - 1] = '\0';
    entry.size = size;
    ++list.count;
    return true;
}

static bool BuildChildPath(char* outPath, u32 outSize, const char* base, const char* name) {
    const int written = snprintf(outPath, outSize, "%s/%s", base, name);
    return written > 0 && static_cast<u32>(written) < outSize;
}

static bool BuildChildRelativePath(char* outPath, u32 outSize, const char* baseRel, const char* name) {
    if (!baseRel || baseRel[0] == '\0') {
        const int written = snprintf(outPath, outSize, "%s", name);
        return written > 0 && static_cast<u32>(written) < outSize;
    }
    return BuildChildPath(outPath, outSize, baseRel, name);
}

static void ScanRiivoDirInternal(s32 riivoFd, const char* basePath, const char* baseRel, OverrideList& list) {
    if (list.count >= kMaxOverrides) {
        return;
    }
    alignas(0x20) IOS::IOCtlvRequest request[3];
    alignas(0x20) s32 folderFd = IOS::IOCtl(riivoFd, static_cast<IOS::IOCtlType>(RIIVO_IOCTL_OPENDIR),
                                            (void*)basePath, strlen(basePath) + 1, 0, 0);
    if (folderFd < 0) {
        OS::Report("[Pulsar][Mods] Riivo opendir failed for %s (%d)\n", basePath, folderFd);
        return;
    }

    alignas(0x20) char fileName[riivoMaxPath];
    alignas(0x20) RiivoStats stats;
    while (list.count < kMaxOverrides) {
        request[0].address = &folderFd;
        request[0].size = sizeof(folderFd);
        request[1].address = &fileName;
        request[1].size = riivoMaxPath;
        request[2].address = &stats;
        request[2].size = sizeof(RiivoStats);
        const s32 ret = IOS::IOCtlv(riivoFd, static_cast<IOS::IOCtlType>(RIIVO_IOCTL_NEXTDIR), 1, 2, request);
        if (ret != 0) {
            break;
        }
        if (fileName[0] == '\0') {
            continue;
        }
        if (strcmp(fileName, ".") == 0 || strcmp(fileName, "..") == 0) {
            continue;
        }

        const bool isDir = (stats.mode & S_IFDIR) == S_IFDIR;
        char childPath[kMaxPath];
        char childRel[kMaxPath];
        if (!BuildChildPath(childPath, sizeof(childPath), basePath, fileName) ||
            !BuildChildRelativePath(childRel, sizeof(childRel), baseRel, fileName)) {
            OS::Report("[Pulsar][Mods] Riivo path too long for %s/%s\n", basePath, fileName);
            continue;
        }

        if (isDir) {
            ScanRiivoDirInternal(riivoFd, childPath, childRel, list);
        } else {
            AddOverrideEntry(list, childPath, childRel);
        }
    }

    IOS::IOCtl(riivoFd, static_cast<IOS::IOCtlType>(RIIVO_IOCTL_CLOSEDIR), (void*)&folderFd, sizeof(folderFd), 0, 0);
}

#define SD_S_IFDIR 0040000
#define SD_S_IFMT 0170000
#define SD_MAX_FILENAME_LENGTH 768

static void ScanSDDirInternal(const char* basePath, const char* baseRel, OverrideList& list) {
    if (list.count >= kMaxOverrides) {
        return;
    }
    dir_struct dirData;
    if (__sd_vtable->diropen(&dirData, basePath) != 0) {
        OS::Report("[Pulsar][Mods] SD diropen failed for %s\n", basePath);
        return;
    }

    char fileName[SD_MAX_FILENAME_LENGTH];
    stat fileStat;
    while (list.count < kMaxOverrides) {
        if (__sd_vtable->dirnext(&dirData, fileName, &fileStat) != 0) {
            break;
        }
        if (fileName[0] == '\0') {
            continue;
        }
        if (strcmp(fileName, ".") == 0 || strcmp(fileName, "..") == 0) {
            continue;
        }

        const bool isDir = (fileStat.st_mode & SD_S_IFMT) == SD_S_IFDIR;
        char childPath[kMaxPath];
        char childRel[kMaxPath];
        if (!BuildChildPath(childPath, sizeof(childPath), basePath, fileName) ||
            !BuildChildRelativePath(childRel, sizeof(childRel), baseRel, fileName)) {
            OS::Report("[Pulsar][Mods] SD path too long for %s/%s\n", basePath, fileName);
            continue;
        }

        if (isDir) {
            ScanSDDirInternal(childPath, childRel, list);
        } else {
            AddOverrideEntry(list, childPath, childRel);
        }
    }

    __sd_vtable->dirclose(&dirData);
}

static bool BuildNandRealPath(const char* path, char* outPath, u32 outSize) {
    const int written = snprintf(outPath, outSize, "%s%s", "/shared2/Pulsar", path);
    return written > 0 && static_cast<u32>(written) < outSize;
}

static void ScanNandDirInternal(const char* basePath, const char* baseRel, OverrideList& list, EGG::Heap* heap) {
    if (list.count >= kMaxOverrides) {
        return;
    }

    char realPath[kMaxPath];
    if (!BuildNandRealPath(basePath, realPath, sizeof(realPath))) {
        OS::Report("[Pulsar][Mods] NAND path too long for %s\n", basePath);
        return;
    }

    u32 count = 0;
    s32 error = ISFS::ReadDir(realPath, 0, &count);
    if (error < 0) {
        OS::Report("[Pulsar][Mods] NAND ReadDir failed for %s (%d)\n", realPath, error);
        return;
    }

    const u32 bufferSize = 255 * (count + 1);
    char* nameBuffer = static_cast<char*>(EGG::Heap::alloc(bufferSize, 0x20, heap));
    if (!nameBuffer) {
        OS::Report("[Pulsar][Mods] NAND ReadDir alloc failed (%u)\n", bufferSize);
        return;
    }
    memset(nameBuffer, 0, bufferSize);
    error = ISFS::ReadDir(realPath, nameBuffer, &count);
    if (error < 0) {
        OS::Report("[Pulsar][Mods] NAND ReadDir failed (data) for %s (%d)\n", realPath, error);
        EGG::Heap::free(nameBuffer, heap);
        return;
    }

    char* cursor = nameBuffer;
    while (*cursor && list.count < kMaxOverrides) {
        const u32 nameLen = strlen(cursor);
        if (nameLen == 0) {
            break;
        }
        if (strcmp(cursor, ".") == 0 || strcmp(cursor, "..") == 0) {
            cursor += nameLen + 1;
            continue;
        }

        char childPath[kMaxPath];
        char childRel[kMaxPath];
        if (!BuildChildPath(childPath, sizeof(childPath), basePath, cursor) ||
            !BuildChildRelativePath(childRel, sizeof(childRel), baseRel, cursor)) {
            OS::Report("[Pulsar][Mods] NAND path too long for %s/%s\n", basePath, cursor);
            cursor += nameLen + 1;
            continue;
        }

        char childRealPath[kMaxPath];
        bool isDir = false;
        if (BuildNandRealPath(childPath, childRealPath, sizeof(childRealPath))) {
            u32 childCount = 0;
            isDir = ISFS::ReadDir(childRealPath, 0, &childCount) >= 0;
        }

        if (isDir) {
            ScanNandDirInternal(childPath, childRel, list, heap);
        } else {
            AddOverrideEntry(list, childPath, childRel);
        }

        cursor += nameLen + 1;
    }

    EGG::Heap::free(nameBuffer, heap);
}

struct MatchInfo {
    char matchPath[kMaxPath];
    const char* matchName;
    bool hasSubdir;
    bool tagged;
};

static bool BuildMatchInfo(const OverrideEntry& entry, const char* archiveBase, MatchInfo& outInfo) {
    const char* relPath = entry.relativePath;
    if (!relPath || relPath[0] == '\0') {
        return false;
    }
    const char* lastSlash = FindLastChar(relPath, '/');
    const char* filename = lastSlash ? lastSlash + 1 : relPath;
    const char* dirPart = lastSlash ? relPath : 0;

    outInfo.tagged = false;
    strncpy(outInfo.matchPath, relPath, kMaxPath);
    outInfo.matchPath[kMaxPath - 1] = '\0';

    if (archiveBase && archiveBase[0] != '\0') {
        const size_t filenameLen = strlen(filename);
        const size_t tagLen = strlen(archiveBase);
        if (filenameLen > tagLen + 1) {
            const char* suffixStart = filename + (filenameLen - tagLen);
            if (suffixStart > filename && *(suffixStart - 1) == '.' && EqualsIgnoreCase(suffixStart, archiveBase)) {
                outInfo.tagged = true;
                char trimmedName[kMaxPath];
                const size_t trimmedLen = filenameLen - (tagLen + 1);
                if (trimmedLen >= sizeof(trimmedName)) {
                    return false;
                }
                memcpy(trimmedName, filename, trimmedLen);
                trimmedName[trimmedLen] = '\0';
                if (dirPart) {
                    const size_t dirLen = static_cast<size_t>(lastSlash - relPath);
                    if (dirLen + 1 + trimmedLen + 1 > sizeof(outInfo.matchPath)) {
                        return false;
                    }
                    memcpy(outInfo.matchPath, relPath, dirLen);
                    outInfo.matchPath[dirLen] = '/';
                    memcpy(outInfo.matchPath + dirLen + 1, trimmedName, trimmedLen);
                    outInfo.matchPath[dirLen + 1 + trimmedLen] = '\0';
                } else {
                    strncpy(outInfo.matchPath, trimmedName, kMaxPath);
                    outInfo.matchPath[kMaxPath - 1] = '\0';
                }
            }
        }
    }

    outInfo.matchName = FindBaseName(outInfo.matchPath);
    outInfo.hasSubdir = strchr(outInfo.matchPath, '/') != 0;
    return true;
}

static bool ApplyOverrideToArchive(U8Node* nodes, u32 nodeCount, char* stringTable, void* archiveBase,
                                   const OverrideEntry& entry, const char* archiveBaseName, u32& writeOffset,
                                   u32& outNodesPatched) {
    MatchInfo info;
    if (!BuildMatchInfo(entry, archiveBaseName, info)) {
        return false;
    }

    u32 localPatched = 0;
    s32 entryNum = -1;
    if (info.hasSubdir) {
        const char* internalPath = info.matchPath;
        if (internalPath[0] == '/') {
            internalPath++;
        }
        entryNum = FindU8EntryByPath(nodes, nodeCount, stringTable, internalPath);
        if (entryNum < 0 || static_cast<u32>(entryNum) >= nodeCount) {
            OS::Report("[Pulsar][Mods] Override path not found: %s (archive %s)\n", internalPath, archiveBaseName);
            return false;
        } else {
            U8Node& node = nodes[entryNum];
            if (IsDirNode(node)) {
                OS::Report("[Pulsar][Mods] Override %s points at directory %s\n", entry.fullPath, internalPath);
                return false;
            }
        }
    } else {
        bool found = false;
        for (u32 i = 1; i < nodeCount; ++i) {
            if (IsDirNode(nodes[i])) {
                continue;
            }
            const char* nodeName = stringTable + (nodes[i].typeName & 0x00FFFFFF);
            if (strcmp(nodeName, info.matchName) == 0) {
                found = true;
                break;
            }
        }
        if (!found) {
            OS::Report("[Pulsar][Mods] No filename match for %s in %s\n", info.matchName, archiveBaseName);
            return false;
        }
    }

    const u32 alignedSize = nw4r::ut::RoundUp(entry.size, kOverrideAlignment);
    u8* dest = reinterpret_cast<u8*>(archiveBase) + writeOffset;
    if (!ReadFileIntoBuffer(entry.fullPath, dest, entry.size)) {
        OS::Report("[Pulsar][Mods] Failed to read override data for %s\n", entry.fullPath);
        return false;
    }

    if (info.hasSubdir) {
        U8Node& node = nodes[entryNum];
        node.dataOffset = writeOffset;
        node.dataSize = entry.size;
        ++localPatched;
    } else {
        for (u32 i = 1; i < nodeCount; ++i) {
            U8Node& node = nodes[i];
            if (IsDirNode(node)) {
                continue;
            }
            const char* nodeName = stringTable + (node.typeName & 0x00FFFFFF);
            if (strcmp(nodeName, info.matchName) == 0) {
                node.dataOffset = writeOffset;
                node.dataSize = entry.size;
                ++localPatched;
            }
        }
    }

    if (localPatched > 0) {
        OS::DCStoreRange(dest, alignedSize);
        OS::DCStoreRange(nodes, nodeCount * sizeof(U8Node));
        writeOffset += alignedSize;
        outNodesPatched += localPatched;
        OS::Report("[Pulsar][Mods] Applied override %s (%u bytes) to %u node(s)\n",
                   entry.fullPath, entry.size, localPatched);
        return true;
    }

    OS::Report("[Pulsar][Mods] Override %s had no targets in %s\n", entry.fullPath, archiveBaseName);
    return false;
}

}  // namespace

const char* GetOverrideRoot() {
    return kOverrideRoot;
}

bool IsModsPath(const char* path) {
    if (!path) {
        return false;
    }
    if (StartsWith(path, kOverrideRoot)) {
        return true;
    }
    return StartsWith(path, kOverrideRootNoSlash);
}

bool IsSZSPath(const char* path) {
    if (!path) {
        return false;
    }
    return EndsWithIgnoreCase(path, ".szs");
}

bool BuildOverrideList(OverrideList& list, EGG::Heap* heap) {
    list.entries = 0;
    list.count = 0;

    IO* io = IO::sInstance;
    if (!io) {
        OS::Report("[Pulsar][Mods] IO instance missing; cannot scan overrides\n");
        return false;
    }

    if (!io->FolderExists(kOverrideRoot)) {
        OS::Report("[Pulsar][Mods] Override root missing: %s\n", kOverrideRoot);
        return false;
    }

    list.entries = static_cast<OverrideEntry*>(EGG::Heap::alloc(sizeof(OverrideEntry) * kMaxOverrides, 0x20, heap));
    if (!list.entries) {
        OS::Report("[Pulsar][Mods] Override list alloc failed (%u entries)\n", kMaxOverrides);
        return false;
    }

    OS::Report("[Pulsar][Mods] Scanning overrides under %s (IO type %d)\n", kOverrideRoot, io->type);

    switch (io->type) {
        case IOType_RIIVO: {
            const s32 riivoFd = IO::OpenFix("file", IOS::MODE_NONE);
            if (riivoFd < 0) {
                OS::Report("[Pulsar][Mods] Riivo device open failed (%d)\n", riivoFd);
                break;
            }
            ScanRiivoDirInternal(riivoFd, kOverrideRoot, "", list);
            IOS::Close(riivoFd);
            break;
        }
        case IOType_SD:
            ScanSDDirInternal(kOverrideRoot, "", list);
            break;
        case IOType_ISO:
        case IOType_DOLPHIN:
        default:
            ScanNandDirInternal(kOverrideRoot, "", list, heap);
            break;
    }

    OS::Report("[Pulsar][Mods] Override scan complete. Files: %u\n", list.count);
    return list.count > 0;
}

void FreeOverrideList(OverrideList& list, EGG::Heap* heap) {
    if (list.entries) {
        EGG::Heap::free(list.entries, heap);
        list.entries = 0;
    }
    list.count = 0;
}

bool TryLoadWholeArchiveOverride(const char* archivePath, EGG::Heap* heap, void*& outData, u32& outSize) {
    outData = 0;
    outSize = 0;

    if (!archivePath || !IsSZSPath(archivePath)) {
        return false;
    }
    if (IsModsPath(archivePath)) {
        return false;
    }

    const char* baseName = FindBaseName(archivePath);
    char overridePath[kMaxPath];
    if (!GetFullOverridePath(baseName, overridePath, sizeof(overridePath))) {
        return false;
    }

    u32 fileSize = 0;
    if (!GetFileSizeFromIO(overridePath, fileSize)) {
        return false;
    }

    void* buffer = EGG::Heap::alloc(fileSize, 0x20, heap);
    if (!buffer) {
        OS::Report("[Pulsar][Mods] Override alloc failed for %s (0x%X)\n", overridePath, fileSize);
        return false;
    }

    if (!ReadFileIntoBuffer(overridePath, buffer, fileSize)) {
        EGG::Heap::free(buffer, heap);
        return false;
    }

    outData = buffer;
    outSize = fileSize;
    OS::Report("[Pulsar][Mods] Whole archive override loaded: %s (0x%X)\n", overridePath, fileSize);
    return true;
}

u32 CalculateOverrideExpansion(const OverrideList& list, const char* archivePath, u32 baseSize, u32& outMatchCount) {
    outMatchCount = 0;
    if (!archivePath || list.count == 0) {
        return 0;
    }

    char archiveBase[kMaxPath];
    if (!BuildArchiveBaseName(archivePath, archiveBase, sizeof(archiveBase))) {
        return 0;
    }

    u32 total = 0;
    for (u32 i = 0; i < list.count; ++i) {
        const OverrideEntry& entry = list.entries[i];
        MatchInfo info;
        if (!BuildMatchInfo(entry, archiveBase, info)) {
            continue;
        }
        total += nw4r::ut::RoundUp(entry.size, kOverrideAlignment);
        ++outMatchCount;
    }

    if (outMatchCount == 0) {
        return 0;
    }

    const u32 basePad = nw4r::ut::RoundUp(baseSize, kOverrideAlignment) - baseSize;
    return total + basePad;
}

u32 ApplyArchiveOverrides(void* archiveBase, u32 baseSize, const char* archivePath, const OverrideList& list,
                          u32& outOverrideFilesApplied, u32& outNodesPatched) {
    outOverrideFilesApplied = 0;
    outNodesPatched = 0;
    if (!archiveBase || !archivePath || list.count == 0) {
        return 0;
    }

    char archiveBaseName[kMaxPath];
    if (!BuildArchiveBaseName(archivePath, archiveBaseName, sizeof(archiveBaseName))) {
        return 0;
    }

    ARC::Header* header = reinterpret_cast<ARC::Header*>(archiveBase);
    U8Node* nodes = reinterpret_cast<U8Node*>(reinterpret_cast<u8*>(archiveBase) + header->nodeOffset);
    const u32 nodeCount = nodes[0].dataSize;
    char* stringTable = reinterpret_cast<char*>(nodes + nodeCount);

    u32 writeOffset = nw4r::ut::RoundUp(baseSize, kOverrideAlignment);
    for (u32 i = 0; i < list.count; ++i) {
        const OverrideEntry& entry = list.entries[i];
        const bool applied = ApplyOverrideToArchive(nodes, nodeCount, stringTable, archiveBase, entry, archiveBaseName,
                                                    writeOffset, outNodesPatched);
        if (applied) {
            ++outOverrideFilesApplied;
        }
    }

    return writeOffset - baseSize;
}

}  // namespace IOOverrides
}  // namespace Pulsar
