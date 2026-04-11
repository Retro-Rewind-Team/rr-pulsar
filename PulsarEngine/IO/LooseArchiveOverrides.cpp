/*
 * This feature was developed by patchzy as part of the Retro Rewind project.
 *
 * Copyright (C) Retro Rewind.
 *
 * Licensed under the GNU General Public License v3.0 only.
 * You may not use, modify, or distribute this code except in compliance with
 * that license.
 *
 * Use of this system in a non-GPLv3 project requires prior permission from
 * the Retro Rewind team and patchzy.
 */

#include <kamek.hpp>
#include <PulsarSystem.hpp>
#include <RetroRewindChannel.hpp>
#include <IO/LooseArchiveOverrides.hpp>
#include <IO/SDIO.hpp>
#include <MarioKartWii/Archive/ArchiveFile.hpp>
#include <core/egg/Archive.hpp>
#include <core/egg/Decomp.hpp>
#include <core/RK/RKSystem.hpp>
#include <core/egg/DVD/DvdRipper.hpp>
#include <core/egg/mem/Heap.hpp>
#include <core/nw4r/ut/Misc.hpp>
#include <core/rvl/arc/arc.hpp>
#include <core/rvl/dvd/dvd.hpp>
#include <core/rvl/OS/OSBootInfo.hpp>
#include <core/rvl/os/OS.hpp>
#include <core/rvl/os/OSCache.hpp>

namespace Pulsar {
namespace IOOverrides {

namespace {
//main patch folder
const char kModsRoot[] = "/patches";
const char kModsRootPrefix[] = "/patches/";
//max mods allowed, can be increased maybe but for now if someone has more than 1024 overrides they deserve to have some not work ;/
const u32 kMaxOverridesTotal = 1024;
//only let the rebuilt archive grow by up to 1 MiB on the source heap before preferring another heap
const u32 kOverrideMaxGrowthOnSourceHeap = 0x100000;

typedef void (*DCInvalidateRangeFunc)(void* addr, u32 size);
static DCInvalidateRangeFunc sDCInvalidateRange = reinterpret_cast<DCInvalidateRangeFunc>(0x801a1600);

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

static ModIndex sModIndex = {nullptr, 0};
static bool sModIndexAttempted = false;
static bool sModsRootChecked = false;
static bool sModsRootPresent = false;
static char sModsRootPath[OVERRIDE_MAX_PATH] = "/patches";
static char sLastUIArchiveBase[32] = "";

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

static EGG::Heap* GetPersistentOverrideHeap(u32 requiredSize) {
    EGG::Heap* candidates[3];
    candidates[0] = RKSystem::mInstance.EGGRootMEM2;
    candidates[1] = RKSystem::mInstance.EGGRootMEM1;
    candidates[2] = GetOverridesHeap();

    for (u32 i = 0; i < 3; ++i) {
        EGG::Heap* heap = candidates[i];
        if (heap == nullptr) continue;
        const u32 available = heap->getAllocatableSize(0x20);
        if (available >= requiredSize) return heap;
    }
    return nullptr;
}

static bool ModsRootExists();
static bool FindModsDirInFST(u32& outIndex, u32& outEnd);
static bool ShouldProbeSDModsPath() {
    IO* io = IO::sInstance;
    if (io == nullptr) return false;
    if (io->type == IOType_SD) return true;
    if (io->type == IOType_DOLPHIN && IsNewChannel()) return true;
    return false;
}

static bool GetSDModsRootPath(char* outPath, u32 outSize) {
    if (outPath == nullptr || outSize == 0) return false;

    const System* system = System::sInstance;
    if (system == nullptr) return false;

    const char* modFolder = system->GetModFolder();
    if (modFolder == nullptr || modFolder[0] == '\0') return false;

    const int written = snprintf(outPath, outSize, "%s/Patches", modFolder);
    if (written <= 0 || static_cast<u32>(written) >= outSize) return false;
    return true;
}

static bool ModsRootExistsOnSD() {
    IO* io = IO::sInstance;
    if (io == nullptr) return false;
    if (!ShouldProbeSDModsPath()) return false;

    char modsPath[OVERRIDE_MAX_PATH];
    if (!GetSDModsRootPath(modsPath, sizeof(modsPath))) return false;
    bool exists = false;
    if (io->type == IOType_SD) {
        exists = io->FolderExists(modsPath);
    } else {
        System* system = System::sInstance;
        if (system == nullptr) return false;
        SDIO sdIo(IOType_SD, system->heap, system->taskThread);
        exists = sdIo.FolderExists(modsPath);
    }
    return exists;
}

static bool ResolveFSTDirByPath(const char* path, u32 entryCount, u32& outIndex, u32& outEnd) {
    if (path == nullptr || path[0] == '\0') return false;
    const s32 entryNum = DVD::ConvertPathToEntryNum(path);
    if (entryNum < 0) return false;
    if (static_cast<u32>(entryNum) >= entryCount) {
        return false;
    }
    const FSTEntry* entries = static_cast<const FSTEntry*>(OS::BootInfo::mInstance.FSTLocation);
    if (!FSTEntryIsDir(entries[entryNum])) {
        return false;
    }
    outIndex = static_cast<u32>(entryNum);
    outEnd = entries[outIndex].size;
    return true;
}

static void InvalidateRange(void* addr, u32 size) {
    if (sDCInvalidateRange == nullptr || addr == nullptr || size == 0) return;
    const u32 start = reinterpret_cast<u32>(addr) & ~0x1F;
    const u32 end = nw4r::ut::RoundUp(reinterpret_cast<u32>(addr) + size, 0x20);
    sDCInvalidateRange(reinterpret_cast<void*>(start), end - start);
}

static bool ReadDVDFile(const char* path, void* dest, u32 size) {
    DVD::FileInfo info;
    if (!DVD::Open(path, &info)) return false;
    InvalidateRange(dest, size);
    const s32 read = DVD::ReadPrio(&info, dest, static_cast<s32>(size), 0, 2);
    DVD::Close(&info);
    return read == static_cast<s32>(size);
}

static bool DVDFileExists(const char* path) {
    DVD::FileInfo info;
    if (!DVD::Open(path, &info)) return false;
    DVD::Close(&info);
    return true;
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

static bool ModsRootExists() {
    if (sModsRootChecked) return sModsRootPresent;

    sModsRootChecked = true;
    SetModsRootPath(kModsRoot);
    u32 modsIndex = 0;
    u32 modsEnd = 0;
    sModsRootPresent = FindModsDirInFST(modsIndex, modsEnd);
    if (!sModsRootPresent) {
        sModsRootPresent = ModsRootExistsOnSD();
    }
    return sModsRootPresent;
}

static bool ReadOverrideFile(const OverrideEntry& entry, void* dest) {
    if (!ModsRootExists()) return false;
    return ReadDVDFile(entry.fullPath, dest, entry.size);
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
        return;
    }
    if (entries != nullptr) {
        FillOverrideEntry(entries[count], fullPath, relativePath, size);
    }
    ++count;
}

static bool FindModsDirInFST(u32& outIndex, u32& outEnd) {
    if (OS::BootInfo::mInstance.FSTLocation == nullptr) return false;

    const FSTEntry* entries = static_cast<const FSTEntry*>(OS::BootInfo::mInstance.FSTLocation);
    const u32 entryCount = entries[0].size;
    if (entryCount == 0) return false;
    return ResolveFSTDirByPath(kModsRoot, entryCount, outIndex, outEnd);
}

static void ScanModsDirDVD(OverrideEntry* entries, u32 maxCount, u32& count, bool& truncated) {
    u32 modsIndex = 0;
    u32 modsEnd = 0;
    if (!FindModsDirInFST(modsIndex, modsEnd)) return;

    SetModsRootPath(kModsRoot);
    sModsRootPresent = true;

    const FSTEntry* fst = static_cast<const FSTEntry*>(OS::BootInfo::mInstance.FSTLocation);
    const u32 entryCount = fst[0].size;
    if (modsIndex >= entryCount || modsEnd > entryCount || modsEnd <= modsIndex) {
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
                continue;
            }
            const u32 prevLen = relLen;
            if (!AppendPath(relPath, sizeof(relPath), relLen, name)) {
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
            continue;
        }

        char fullPath[OVERRIDE_MAX_PATH];
        if (!BuildOverridePathWithRoot(kModsRoot, relativePath, nullptr, fullPath, sizeof(fullPath))) {
            continue;
        }

        AddEntry(entries, maxCount, count, truncated, fullPath, relativePath, entry.size);
        if (truncated) break;
    }
}

static void ScanModsDirFromIO(IO& io, OverrideEntry* entries, u32 maxCount, u32& count, bool& truncated) {
    char modsPath[OVERRIDE_MAX_PATH];
    if (!GetSDModsRootPath(modsPath, sizeof(modsPath))) return;
    if (!io.FolderExists(modsPath)) return;

    io.ReadFolder(modsPath);
    const u32 fileCount = io.GetFileCount();
    for (u32 i = 0; i < fileCount && count < maxCount; ++i) {
        const char* fileName = io.GetFileName(i);
        if (fileName == nullptr || fileName[0] == '\0') continue;
        if (strlen(fileName) >= OVERRIDE_MAX_PATH) continue;

        char sdPath[OVERRIDE_MAX_PATH];
        io.GetFolderFilePath(sdPath, i);
        if (!io.OpenFile(sdPath, FILE_MODE_READ)) continue;

        const s32 fileSize = io.GetFileSize();
        io.Close();
        if (fileSize < 0) continue;

        char fullPath[OVERRIDE_MAX_PATH];
        if (!BuildOverridePathWithRoot(kModsRoot, fileName, nullptr, fullPath, sizeof(fullPath))) continue;

        AddEntry(entries, maxCount, count, truncated, fullPath, fileName, static_cast<u32>(fileSize));
        if (truncated) break;
    }
    io.CloseFolder();
}

static void ScanModsDirSD(OverrideEntry* entries, u32 maxCount, u32& count, bool& truncated) {
    IO* io = IO::sInstance;
    if (io == nullptr) return;
    if (!ShouldProbeSDModsPath()) return;

    if (io->type == IOType_SD) {
        ScanModsDirFromIO(*io, entries, maxCount, count, truncated);
        return;
    }

    System* system = System::sInstance;
    if (system == nullptr) return;

    SDIO sdIo(IOType_SD, system->heap, system->taskThread);
    ScanModsDirFromIO(sdIo, entries, maxCount, count, truncated);
}

static void ScanModsDir(OverrideEntry* entries, u32 maxCount, u32& count, bool& truncated) {
    if (!ModsRootExists()) return;

    IO* io = IO::sInstance;
    if (io != nullptr && ShouldProbeSDModsPath()) {
        ScanModsDirSD(entries, maxCount, count, truncated);
        return;
    }

    ScanModsDirDVD(entries, maxCount, count, truncated);
}

static void EnsureModIndexBuilt() {
    if (sModIndexAttempted) return;

    if (!ModsRootExists()) {
        sModIndex.entries = nullptr;
        sModIndex.count = 0;
        sModIndexAttempted = true;
        return;
    }

    sModIndexAttempted = true;

    u32 count = 0;
    bool truncated = false;
    ScanModsDir(nullptr, kMaxOverridesTotal, count, truncated);
    if (count >= kMaxOverridesTotal) {
        truncated = true;
    }
    if (count == 0) {
        sModIndex.entries = nullptr;
        sModIndex.count = 0;
        return;
    }

    const u32 requiredSize = sizeof(OverrideEntry) * count;
    EGG::Heap* heap = GetPersistentOverrideHeap(requiredSize);
    if (heap == nullptr) {
        sModIndex.entries = nullptr;
        sModIndex.count = 0;
        return;
    }

    OverrideEntry* entries = EGG::Heap::alloc<OverrideEntry>(requiredSize, 0x20, heap);
    if (entries == nullptr) {
        sModIndex.entries = nullptr;
        sModIndex.count = 0;
        return;
    }

    u32 filled = 0;
    bool truncatedFill = false;
    ScanModsDir(entries, count, filled, truncatedFill);
    truncated = truncated || truncatedFill;

    sModIndex.entries = entries;
    sModIndex.count = filled;
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
    if (strcmp(path, kModsRoot) == 0) return true;
    if (StartsWith(path, kModsRootPrefix)) return true;

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

    const int written = snprintf(resolvedPath, resolvedSize, "%s/%s", sModsRootPath, base);
    if (written <= 0 || static_cast<u32>(written) >= resolvedSize) {
        return path;
    }

    if (DVDFileExists(resolvedPath)) {
        if (outRedirected != nullptr) *outRedirected = true;
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
    if (strcmp(archiveBaseLower, "menusingle") != 0 && strcmp(archiveBaseLower, "menusingle_e") != 0) {
        return false;
    }
    return true;
}

bool ApplyLooseOverrides(const char* archiveBaseLower, u8*& archiveBase, u32& archiveSize, EGG::Heap* sourceHeap,
                         EGG::Heap*& archiveHeap, u32* outAppliedOverrides, u32* outPatchedNodes,
                         u32* outMissingOverrides, const u8* compressedData) {
    if (outAppliedOverrides != nullptr) *outAppliedOverrides = 0;
    if (outPatchedNodes != nullptr) *outPatchedNodes = 0;
    if (outMissingOverrides != nullptr) *outMissingOverrides = 0;

    EnsureModIndexBuilt();
    if (sModIndex.entries == nullptr || sModIndex.count == 0) return false;
    if (archiveBase == nullptr || archiveBaseLower == nullptr || archiveBaseLower[0] == '\0') return false;
    if (sourceHeap == nullptr) {
        return false;
    }
    if (archiveHeap == nullptr) {
        archiveHeap = sourceHeap;
    }


    u32 taggedCandidates = 0;
    for (u32 i = 0; i < sModIndex.count; ++i) {
        const OverrideEntry& entry = sModIndex.entries[i];
        if (entry.isTagged && strcmp(entry.archiveTagLower, archiveBaseLower) == 0) {
            ++taggedCandidates;
        }
    }
    if (taggedCandidates == 0) {
        return false;
    }

    ARC::Handle handle;
    if (!ARC::InitHandle(archiveBase, &handle)) {
        return false;
    }

    ARC::Header* header = reinterpret_cast<ARC::Header*>(archiveBase);
    U8Node* nodes = reinterpret_cast<U8Node*>(archiveBase + header->nodeOffset);
    const u32 nodeCount = nodes[0].dataSize;
    if (nodeCount == 0) {
        return false;
    }

    char* stringTable = reinterpret_cast<char*>(nodes + nodeCount);
    EGG::Heap* tempHeap = GetOverridesHeap();
    if (tempHeap == nullptr) tempHeap = sourceHeap;
    s32* nodeOverrideIndex = EGG::Heap::alloc<s32>(sizeof(s32) * nodeCount, 0x20, tempHeap);
    u8* entryApplied = EGG::Heap::alloc<u8>(sizeof(u8) * sModIndex.count, 0x20, tempHeap);
    if (nodeOverrideIndex == nullptr || entryApplied == nullptr) {
        if (nodeOverrideIndex != nullptr) EGG::Heap::free(nodeOverrideIndex, tempHeap);
        if (entryApplied != nullptr) EGG::Heap::free(entryApplied, tempHeap);
        return false;
    }

    memset(nodeOverrideIndex, 0xFF, sizeof(s32) * nodeCount);
    memset(entryApplied, 0, sizeof(u8) * sModIndex.count);

    bool anyOverrides = false;
    u32 missingOverrides = 0;

    for (u32 i = 0; i < sModIndex.count; ++i) {
        const OverrideEntry& entry = sModIndex.entries[i];
        const bool tagMatches = entry.isTagged && (strcmp(entry.archiveTagLower, archiveBaseLower) == 0);
        if (!tagMatches) {
            continue;
        }

        const char* matchName = entry.strippedName;
        if (matchName[0] == '\0') {
            continue;
        }

        if (entry.hasSubpath) {
            s32 entryNum = ARC::ConvertPathToEntrynum(&handle, matchName);
            if (entryNum < 0) {
                ++missingOverrides;
                continue;
            }
            if (NodeIsDir(nodes[entryNum])) {
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
                ++missingOverrides;
                continue;
            }
            anyOverrides = true;
        }
    }

    if (!anyOverrides) {
        if (outMissingOverrides != nullptr) *outMissingOverrides = missingOverrides;
        EGG::Heap::free(nodeOverrideIndex, tempHeap);
        EGG::Heap::free(entryApplied, tempHeap);
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
                continue;
            }


            void* dest = archiveBase + nodes[nodeIdx].dataOffset;
            if (!ReadOverrideFile(entry, dest)) {
                continue;
            }
            if (entry.size < nodes[nodeIdx].dataSize) {
                memset(reinterpret_cast<u8*>(dest) + entry.size, 0, nodes[nodeIdx].dataSize - entry.size);
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

        EGG::Heap::free(nodeOverrideIndex, tempHeap);
        EGG::Heap::free(entryApplied, tempHeap);
        return appliedOverrides > 0;
    }

    const u32 dataStart = GetFileDataStart(header);
    if (dataStart == 0 || dataStart > archiveSize) {
        EGG::Heap::free(nodeOverrideIndex, tempHeap);
        EGG::Heap::free(entryApplied, tempHeap);
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

    const u32 originalArchiveSize = archiveSize;
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
    bool useSameHeapRepack = false;
    u32* repackOffsets = nullptr;
    u32* repackSizes = nullptr;
    u32* repackOrder = nullptr;
    u32 repackOrderCount = 0;

    if (repackHeap == archiveHeap && allowSourceHeap && compressedData != nullptr) {
        repackOffsets = EGG::Heap::alloc<u32>(sizeof(u32) * nodeCount, 0x20, tempHeap);
        repackSizes = EGG::Heap::alloc<u32>(sizeof(u32) * nodeCount, 0x20, tempHeap);
        repackOrder = EGG::Heap::alloc<u32>(sizeof(u32) * nodeCount, 0x20, tempHeap);
        if (repackOffsets != nullptr && repackSizes != nullptr && repackOrder != nullptr) {
            memset(repackOffsets, 0, sizeof(u32) * nodeCount);
            memset(repackSizes, 0, sizeof(u32) * nodeCount);
            memset(repackOrder, 0, sizeof(u32) * nodeCount);
            u32 plannedOffset = dataStart;
            bool allOffsetsForward = true;
            for (u32 nodeIdx = 1; nodeIdx < nodeCount; ++nodeIdx) {
                if (NodeIsDir(nodes[nodeIdx])) continue;
                plannedOffset = nw4r::ut::RoundUp(plannedOffset, 0x20);
                repackOffsets[nodeIdx] = plannedOffset;
                const s32 idx = nodeOverrideIndex[nodeIdx];
                const u32 plannedSize = (idx >= 0) ? sModIndex.entries[idx].size : nodes[nodeIdx].dataSize;
                repackSizes[nodeIdx] = plannedSize;
                repackOrder[repackOrderCount++] = nodeIdx;
                if (plannedOffset < nodes[nodeIdx].dataOffset) {
                    allOffsetsForward = false;
                }
                plannedOffset += nw4r::ut::RoundUp(plannedSize, 0x20);
            }
            useSameHeapRepack = allOffsetsForward;
            if (useSameHeapRepack) {
                // Same-heap repack must move files from the highest original offset downward.
                for (u32 i = 1; i < repackOrderCount; ++i) {
                    const u32 keyNode = repackOrder[i];
                    const u32 keyOffset = nodes[keyNode].dataOffset;
                    u32 j = i;
                    while (j > 0) {
                        const u32 prevNode = repackOrder[j - 1];
                        if (nodes[prevNode].dataOffset >= keyOffset) break;
                        repackOrder[j] = prevNode;
                        --j;
                    }
                    repackOrder[j] = keyNode;
                }
            }
        }
        if (!useSameHeapRepack) {
            if (repackOffsets != nullptr) EGG::Heap::free(repackOffsets, tempHeap);
            if (repackSizes != nullptr) EGG::Heap::free(repackSizes, tempHeap);
            if (repackOrder != nullptr) EGG::Heap::free(repackOrder, tempHeap);
            repackOffsets = nullptr;
            repackSizes = nullptr;
            repackOrder = nullptr;
            repackOrderCount = 0;
        }
    }

    if (useSameHeapRepack) {
        EGG::Heap::free(archiveBase, sourceHeap);
        archiveBase = nullptr;
        newBuffer = static_cast<u8*>(EGG::Heap::alloc(newSize, 0x20, repackHeap));
        triedSourceHeap = true;
        if (newBuffer != nullptr) {
            EGG::Decomp::decodeSZS(const_cast<u8*>(compressedData), newBuffer);
            if (newSize > originalArchiveSize) {
                memset(newBuffer + originalArchiveSize, 0, newSize - originalArchiveSize);
            }
        }
    } else if (repackHeap != archiveHeap || allowSourceHeap) {
        newBuffer = static_cast<u8*>(EGG::Heap::alloc(newSize, 0x20, repackHeap));
    }
    if (newBuffer == nullptr && repackHeap != archiveHeap) {
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
        if (archiveBase == nullptr && compressedData != nullptr) {
            archiveBase = static_cast<u8*>(EGG::Heap::alloc(originalArchiveSize, 0x20, sourceHeap));
            if (archiveBase != nullptr) {
                EGG::Decomp::decodeSZS(const_cast<u8*>(compressedData), archiveBase);
                archiveHeap = sourceHeap;
                archiveSize = originalArchiveSize;
            }
        }
        if (outMissingOverrides != nullptr) *outMissingOverrides = missingOverrides;

        if (repackOffsets != nullptr) EGG::Heap::free(repackOffsets, tempHeap);
        if (repackSizes != nullptr) EGG::Heap::free(repackSizes, tempHeap);
        if (repackOrder != nullptr) EGG::Heap::free(repackOrder, tempHeap);
        EGG::Heap::free(nodeOverrideIndex, tempHeap);
        EGG::Heap::free(entryApplied, tempHeap);
        return false;
    }

    if (!useSameHeapRepack) {
        memset(newBuffer, 0, newSize);
        memcpy(newBuffer, archiveBase, dataStart);
    }
    ARC::Header* newHeader = reinterpret_cast<ARC::Header*>(newBuffer);
    newHeader->fileOffset = dataStart;
    U8Node* newNodes = reinterpret_cast<U8Node*>(newBuffer + newHeader->nodeOffset);

    u32 writeOffset = dataStart;
    if (useSameHeapRepack) {
        for (u32 orderIdx = 0; orderIdx < repackOrderCount; ++orderIdx) {
            const u32 nodeIdx = repackOrder[orderIdx];

            const s32 idx = nodeOverrideIndex[nodeIdx];
            const u32 oldOffset = newNodes[nodeIdx].dataOffset;
            const u32 oldSize = newNodes[nodeIdx].dataSize;
            const u32 newFileSize = repackSizes[nodeIdx];
            const u32 newOffset = repackOffsets[nodeIdx];

            if (newOffset != oldOffset) {
                memmove(newBuffer + newOffset, newBuffer + oldOffset, oldSize);
            }

            newNodes[nodeIdx].dataOffset = newOffset;
            newNodes[nodeIdx].dataSize = newFileSize;
        }

        // Persist moved source data before invalidating cache lines for external reads.
        OS::DCStoreRange(newBuffer, newSize);

        for (u32 orderIdx = 0; orderIdx < repackOrderCount; ++orderIdx) {
            const u32 nodeIdx = repackOrder[orderIdx];
            const s32 idx = nodeOverrideIndex[nodeIdx];
            if (idx < 0) continue;

            const OverrideEntry& entry = sModIndex.entries[idx];
            const u32 oldSize = nodes[nodeIdx].dataSize;
            const u32 newOffset = repackOffsets[nodeIdx];
            if (!ReadOverrideFile(entry, newBuffer + newOffset)) {
                newNodes[nodeIdx].dataSize = oldSize;
                continue;
            }
            entryApplied[idx] = 1;
            ++patchedNodes;
        }

        writeOffset = dataStart + totalDataSize;
    } else {
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
                useOverride = false;
                newFileSize = oldSize;
            }

            newNodes[nodeIdx].dataOffset = writeOffset;
            if (useOverride) {
                const OverrideEntry& entry = sModIndex.entries[idx];
                if (!ReadOverrideFile(entry, newBuffer + writeOffset)) {
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
    }

    u32 finalSize = nw4r::ut::RoundUp(writeOffset, 0x20);
    if (finalSize > newSize) finalSize = newSize;
    OS::DCStoreRange(newBuffer, finalSize);

    if (!useSameHeapRepack) {
        EGG::Heap::free(archiveBase, sourceHeap);
    }
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

    if (repackOffsets != nullptr) EGG::Heap::free(repackOffsets, tempHeap);
    if (repackSizes != nullptr) EGG::Heap::free(repackSizes, tempHeap);
    if (repackOrder != nullptr) EGG::Heap::free(repackOrder, tempHeap);
    EGG::Heap::free(nodeOverrideIndex, tempHeap);
    EGG::Heap::free(entryApplied, tempHeap);
    return appliedOverrides > 0;
}

static void ArchiveFileLoadOverride(ArchiveFile* file, const char* path, EGG::Heap* mountHeap, bool isCompressed,
                                    s32 allocDirection, EGG::Heap* dumpHeap, EGG::Archive::FileInfo* info) {
    char normalizedPath[OVERRIDE_MAX_PATH];
    const char* requestedPath = path;
    if (path != nullptr) {
        const char* base = FindBasename(path);
        if (base != nullptr && strcmp(base, ".szs") == 0 && StartsWith(sLastUIArchiveBase, "/Scene/UI/")) {
            const int written = snprintf(normalizedPath, sizeof(normalizedPath), "%s.szs", sLastUIArchiveBase);
            if (written > 0 && static_cast<u32>(written) < sizeof(normalizedPath)) {
                requestedPath = normalizedPath;
            }
            sLastUIArchiveBase[0] = '\0';
        } else if (StartsWith(path, "/Scene/UI/") && EndsWithIgnoreCase(path, ".szs")) {
            const char* dot = FindLastChar(path, '.');
            const char* underscore = dot;
            while (underscore != nullptr && underscore > path && underscore[-1] != '_' && underscore[-1] != '/') {
                --underscore;
            }
            if (dot != nullptr && underscore != nullptr && underscore > path && underscore[-1] == '_') {
                const u32 suffixLen = static_cast<u32>(dot - underscore);
                const u32 prefixLen = static_cast<u32>((underscore - 1) - path);
                if (suffixLen > 0 && suffixLen <= 3 && prefixLen + 1 <= sizeof(sLastUIArchiveBase)) {
                    memcpy(sLastUIArchiveBase, path, prefixLen);
                    sLastUIArchiveBase[prefixLen] = '\0';
                } else {
                    sLastUIArchiveBase[0] = '\0';
                }
            } else {
                sLastUIArchiveBase[0] = '\0';
            }
        }
    }
    char resolvedPath[OVERRIDE_MAX_PATH];
    const char* finalPath = ResolveWholeFileOverride(requestedPath, resolvedPath, sizeof(resolvedPath), nullptr);


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
