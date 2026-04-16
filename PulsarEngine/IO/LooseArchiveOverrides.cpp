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
 * the Retro Rewind team and/ or patchzy.
 */

#include <kamek.hpp>
#include <PulsarSystem.hpp>
#include <RetroRewindChannel.hpp>
#include <IO/LooseArchiveOverrides.hpp>
#include <IO/SDIO.hpp>
#include <Settings/Settings.hpp>
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
/*
 * LooseArchiveOverrides is split into two separate behaviors that share the
 * same `/patches` root:
 *
 * 1. Whole-file redirects:
 *    `ResolveWholeFileOverride()` swaps an original path like
 *    `/Scene/UI/Common.szs` to `/patches/Common.szs` before loading if a loose
 *    replacement exists. This is to mimic the original my-stuff folder behavior
 *
 * 2. Tagged archive-member overrides:
 *    files named `member.ext.ArchiveTag` are indexed once, then matched against
 *    a decompressed archive named `ArchiveTag.szs`.(for example) The filename before the last
 *    dot becomes the target node path inside the archive.
 *
 * The implementation is Wii-safe: fixed-size buffers,
 * one persistent array for the index, and a sorted-by-tag layout so runtime
 * archive loads only touch the slice relevant to the current archive.
 */
//main patch folder
const char kModsRoot[] = "/patches";
const char kModsRootPrefix[] = "/patches/";
//max mods allowed, can be increased maybe but for now if someone has more than 1024 overrides they deserve to have some not work ;/
const u32 kMaxOverridesTotal = 1024;
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


// Cached loose-override index. This is built once on first use and then kept in
// a persistent heap because archive loads can happen frequently and often on
// hot paths such as UI transitions. It might be worth figuring out if this is really cheaper though
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

static bool AreLooseArchiveOverridesEnabled() {
    if (!Settings::Mgr::IsCreated()) {
        // Settings not initialized yet, assume disabled to avoid unsafe behavior.
        return false;
    }
    return Settings::Mgr::Get().GetUserSettingValue(Settings::SETTINGSTYPE_MISC, RADIO_LOOSEARCHIVEOVERRIDES) ==
           LOOSEARCHIVEOVERRIDES_ENABLED;
}

static bool EndsWithIgnoreCase(const char* str, const char* suffix) {
    if (str == nullptr || suffix == nullptr) return false;
    const size_t strLen = strlen(str);
    const size_t suffixLen = strlen(suffix);
    // Reject impossible matches early so callers can use this as a cheap extension filter.
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

static bool IsBlockedLooseRawOverrideExtension(const char* path) {
    return EndsWithIgnoreCase(path, ".kcl") || EndsWithIgnoreCase(path, ".kmp") || EndsWithIgnoreCase(path, ".slt");
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
    // Tags are only lookup keys, so truncation is acceptable as long as the string stays terminated.
    dest[i] = '\0';
}

static void ToLowerInPlace(char* str) {
    if (str == nullptr) return;
    for (; *str != '\0'; ++str) {
        if (*str >= 'A' && *str <= 'Z') *str = static_cast<char>(*str - 'A' + 'a');
    }
}

static s32 CompareArchiveTags(const char* lhs, const char* rhs) {
    if (lhs == rhs) return 0;
    if (lhs == nullptr) return -1;
    if (rhs == nullptr) return 1;
    return strcmp(lhs, rhs);
}

static bool TryParseArchiveTag(const char* relativePath, char* strippedName, u32 strippedNameSize,
                               char* archiveTagLower, u32 archiveTagLowerSize) {
    if (strippedName != nullptr && strippedNameSize > 0) strippedName[0] = '\0';
    if (archiveTagLower != nullptr && archiveTagLowerSize > 0) archiveTagLower[0] = '\0';
    if (relativePath == nullptr || strippedName == nullptr || strippedNameSize == 0 ||
        archiveTagLower == nullptr || archiveTagLowerSize == 0) {
        return false;
    }

    const char* filename = FindLastChar(relativePath, '/');
    if (filename != nullptr) {
        ++filename;
    } else {
        filename = relativePath;
    }

    const char* lastDot = FindLastChar(filename, '.');
    if (lastDot == nullptr || lastDot == filename || lastDot[1] == '\0') {
        return false;
    }

    const char* extDot = nullptr;
    for (const char* p = filename; p < lastDot; ++p) {
        if (*p == '.') extDot = p;
    }

    // Loose archive-member overrides intentionally require at least two dots:
    // `name.ext.Tag`

    // `Tag` is the archive basename, and `name.ext` remains the target node
    // path once any bracket-prefix path encoding has been expanded. This keeps
    // ordinary single-extension files such as `Common.szs` in the whole-file
    // redirect lane instead of accidentally treating `szs` as an archive tag.

    if (extDot == nullptr || extDot == filename || extDot + 1 >= lastDot) {
        return false;
    }

    const u32 prefixLen = static_cast<u32>(lastDot - relativePath);
    // Refuse to index an entry if its decoded member path would have to be truncated.
    if (prefixLen + 1 > strippedNameSize) {
        return false;
    }

    memcpy(strippedName, relativePath, prefixLen);
    strippedName[prefixLen] = '\0';
    ToLowerCopy(archiveTagLower, lastDot + 1, archiveTagLowerSize);
    return true;
}

static void SortOverrideEntriesByArchiveTag(OverrideEntry* entries, u32 count) {
    if (entries == nullptr || count < 2) return;


    // The index is ordered with a tiny insertion sort during its one-time build step.
    // Entries are grouped by archive tag first, then by stripped member path so
    // binary range lookup can hand ApplyLooseOverrides() a contiguous bucket for
    // the current archive. The secondary key keeps the ordering deterministic for
    // debugging and duplicate-resolution behavior.

    for (u32 i = 1; i < count; ++i) {
        const OverrideEntry key = entries[i];
        u32 insertIdx = i;
        while (insertIdx > 0) {
            const OverrideEntry& prev = entries[insertIdx - 1];
            const s32 compare = CompareArchiveTags(prev.archiveTagLower, key.archiveTagLower);
            if (compare < 0) break;
            if (compare == 0 && strcmp(prev.strippedName, key.strippedName) <= 0) break;
            entries[insertIdx] = prev;
            --insertIdx;
        }
        entries[insertIdx] = key;
    }
}

static bool FindArchiveTagRange(const ModIndex& index, const char* archiveBaseLower, u32& start, u32& end) {
    start = 0;
    end = 0;
    if (index.entries == nullptr || index.count == 0 || archiveBaseLower == nullptr || archiveBaseLower[0] == '\0') {
        return false;
    }

    // Standard lower/upper-bound search over the sorted index. Returning a
    // half-open range keeps the runtime loop branch-light and avoids rescanning
    // unrelated loose overrides for every archive load.

    u32 low = 0;
    u32 high = index.count;
    while (low < high) {
        const u32 mid = low + ((high - low) / 2);
        const s32 compare = CompareArchiveTags(index.entries[mid].archiveTagLower, archiveBaseLower);
        if (compare < 0) {
            low = mid + 1;
        } else {
            high = mid;
        }
    }
    // No exact match here means this archive has no bucket in the prebuilt index.
    if (low >= index.count || strcmp(index.entries[low].archiveTagLower, archiveBaseLower) != 0) {
        return false;
    }

    start = low;
    high = index.count;
    while (low < high) {
        const u32 mid = low + ((high - low) / 2);
        const s32 compare = CompareArchiveTags(index.entries[mid].archiveTagLower, archiveBaseLower);
        if (compare <= 0) {
            low = mid + 1;
        } else {
            high = mid;
        }
    }

    end = low;
    return end > start;
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
    // Downstream path logic assumes explicit NUL termination after every bounded copy.
    dest[destSize - 1] = '\0';
}

static bool DecodeOverrideRelativePath(char* dest, u32 destSize, const char* src) {
    if (dest == nullptr || destSize == 0) return false;
    if (src == nullptr) {
        dest[0] = '\0';
        return true;
    }

    u32 writeIdx = 0;
    const char* cursor = src;

    // Loose files live in a flat directory on disk, so nested archive paths use
    // a bracket prefix encoding. The decoded form is only used for archive node
    // matching; the original filename on disk remains untouched.

    // Example:
    // `[button][timg]icon.tpl.Channel` -> `button/timg/icon.tpl.Channel`

    while (*cursor == '[') {
        const char* close = strchr(cursor + 1, ']');
        if (close == nullptr || close == cursor + 1) {
            break;
        }

        const u32 segmentLen = static_cast<u32>(close - (cursor + 1));
        if (writeIdx + segmentLen + 1 >= destSize) {
            dest[0] = '\0';
            return false;
        }

        memcpy(dest + writeIdx, cursor + 1, segmentLen);
        writeIdx += segmentLen;
        dest[writeIdx++] = '/';
        cursor = close + 1;
    }

    for (; *cursor != '\0'; ++cursor) {
        if (writeIdx + 1 >= destSize) {
            dest[0] = '\0';
            return false;
        }
        dest[writeIdx++] = *cursor;
    }

    dest[writeIdx] = '\0';
    return true;
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
    // The traversal stack restores `pathLen`, so partial writes would corrupt later path reconstruction.
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

    // The index is persistent for the process lifetime, so prefer large root
    // heaps over transient/archive-specific heaps. Falling back to the system
    // heap is still better than rebuilding the index every archive load.

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
    // Hardware SD can use the active IO backend directly; Dolphin channel mode needs an explicit SD probe.
    if (io->type == IOType_SD) return true;
    if (io->type == IOType_DOLPHIN && IsNewChannel()) return true;
    return false;
}

static bool GetSDModsRootPath(char* outPath, u32 outSize) {
    if (outPath == nullptr || outSize == 0) return false;

    const System* system = System::sInstance;
    if (system == nullptr) return false;

    const char* modFolder = system->GetModFolder();
    // No mod folder means there is no external loose-override root to resolve.
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
        // Dolphin channel mode is not backed by the main IO object, so probe through a temporary SDIO instance.
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
    // The DVD scan walks a directory range directly, so `/patches` must resolve to a directory entry.
    if (!FSTEntryIsDir(entries[entryNum])) {
        return false;
    }
    outIndex = static_cast<u32>(entryNum);
    outEnd = entries[outIndex].size;
    return true;
}

static void InvalidateRange(void* addr, u32 size) {
    if (addr == nullptr || size == 0) return;
    const u32 start = reinterpret_cast<u32>(addr) & ~0x1F;
    const u32 end = nw4r::ut::RoundUp(reinterpret_cast<u32>(addr) + size, 0x20);
    OS::DCInvalidateRange(reinterpret_cast<void*>(start), end - start);
}

static bool ReadDVDFile(const char* path, void* dest, u32 size) {
    DVD::FileInfo info;
    if (!DVD::Open(path, &info)) return false;
    // External media reads can bypass the CPU cache; invalidate before the DMA-style read.
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

    // Probe once, then cache both presence and the root path flavor we resolved.
    // DVD builds use `/patches` directly; SD/Dolphin setups may resolve to the
    // external mod folder path instead.

    sModsRootChecked = true;
    SetModsRootPath(kModsRoot);
    u32 modsIndex = 0;
    u32 modsEnd = 0;
    sModsRootPresent = FindModsDirInFST(modsIndex, modsEnd);
    if (!sModsRootPresent) {
        // Disc FST lookup is preferred; SD probing is only the fallback path.
        sModsRootPresent = ModsRootExistsOnSD();
    }
    return sModsRootPresent;
}

static bool ReadOverrideFile(const OverrideEntry& entry, void* dest) {
    if (!ModsRootExists()) return false;
    // The index already cached the final logical path and size, so patch reads become direct file copies.
    return ReadDVDFile(entry.fullPath, dest, entry.size);
}

static void FillOverrideEntry(OverrideEntry& entry, const char* fullPath, const char* relativePath, u32 size) {
    strncpy(entry.fullPath, fullPath, sizeof(entry.fullPath));
    entry.fullPath[sizeof(entry.fullPath) - 1] = '\0';

    if (!DecodeOverrideRelativePath(entry.relativePath, sizeof(entry.relativePath), relativePath)) {
        // Keep the entry structurally valid even if path decoding failed; later matching will naturally reject it.
        entry.relativePath[0] = '\0';
    }


    // `strippedName` is the archive lookup key:
    // - `button/timg/icon.tpl` for nested overrides
    // - `icon.tpl` for basename-only overrides that may match multiple nodes

    // `archiveTagLower` is the archive bucket key, derived from the final suffix
    // in `name.ext.ArchiveTag`.

    entry.hasSubpath = (strchr(entry.relativePath, '/') != nullptr);
    entry.isTagged = TryParseArchiveTag(entry.relativePath, entry.strippedName, sizeof(entry.strippedName),
                                        entry.archiveTagLower, sizeof(entry.archiveTagLower));
    if (!entry.isTagged) {
        strncpy(entry.strippedName, entry.relativePath, sizeof(entry.strippedName));
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
    if (fullPath == nullptr || relativePath == nullptr) return;
    // All persistent storage is fixed-size, so oversized filenames are intentionally dropped.
    if (strlen(fullPath) >= OVERRIDE_MAX_PATH || strlen(relativePath) >= OVERRIDE_MAX_PATH) {
        return;
    }

    // The index intentionally contains only tagged archive-member overrides.
    // Plain loose files like `Common.szs` are served by ResolveWholeFileOverride()
    // and do not need to consume index slots or per-archive lookup time here.

    char strippedName[OVERRIDE_MAX_PATH];
    char archiveTagLower[OVERRIDE_MAX_NAME];
    if (!TryParseArchiveTag(relativePath, strippedName, sizeof(strippedName),
                            archiveTagLower, sizeof(archiveTagLower))) {
        return;
    }
    // Reject loose raw-file overrides for these resource types, even if they target an archive member.
    if (IsBlockedLooseRawOverrideExtension(strippedName)) return;
    if (!CanAddEntry(entries, maxCount, count, truncated)) return;
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
    // Abort on malformed directory bounds before walking raw FST indices.
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


    // Walk the disc FST subtree rooted at `/patches` without allocating a
    // recursive directory structure. The manual stack mirrors the nested FST
    // directory ranges so we can rebuild relative paths on the fly.

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
                // The scan stays non-recursive and fixed-memory; over-deep trees are skipped rather than overflowing.
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
        // SD scanning is intentionally flat; bracket prefixes encode any desired archive subpath.
        if (strlen(fileName) >= OVERRIDE_MAX_PATH) continue;

        char sdPath[OVERRIDE_MAX_PATH];
        io.GetFolderFilePath(sdPath, i);
        if (!io.OpenFile(sdPath, FILE_MODE_READ)) continue;

        const s32 fileSize = io.GetFileSize();
        io.Close();
        // Negative sizes indicate an IO failure, not a valid zero-length override.
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
        // Prefer SD when available so loose files can change without rebuilding the disc image.
        ScanModsDirSD(entries, maxCount, count, truncated);
        return;
    }

    // Otherwise walk the baked-in `/patches` subtree from the DVD FST.
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


    // Two-pass build:

    // Pass 1 counts valid tagged overrides so we can allocate a tightly sized
    // persistent array instead of keeping slack memory around forever.

    // Pass 2 fills and sorts the array by archive tag.

    // This makes archive-time work predictable: one binary lookup and one scan
    // over the relevant tag bucket.

    u32 count = 0;
    bool truncated = false;
    ScanModsDir(nullptr, kMaxOverridesTotal, count, truncated);
    if (count >= kMaxOverridesTotal) {
        truncated = true;
    }
    if (truncated) {
        OS::Report("[Pulsar] Loose overrides truncated at %u entries (max %u)\n", count, kMaxOverridesTotal);
    }
    if (count == 0) {
        // Once this is discovered, future archive loads can skip the feature with a single pointer check.
        sModIndex.entries = nullptr;
        sModIndex.count = 0;
        return;
    }

    const u32 requiredSize = sizeof(OverrideEntry) * count;
    EGG::Heap* heap = GetPersistentOverrideHeap(requiredSize);
    if (heap == nullptr) {
        OS::Report("[Pulsar] Loose overrides index allocation skipped: need 0x%X bytes, no persistent heap available\n",
                   requiredSize);
        sModIndex.entries = nullptr;
        sModIndex.count = 0;
        return;
    }

    OverrideEntry* entries = EGG::Heap::alloc<OverrideEntry>(requiredSize, 0x20, heap);
    if (entries == nullptr) {
        OS::Report("[Pulsar] Loose overrides index allocation failed: size=0x%X\n", requiredSize);
        sModIndex.entries = nullptr;
        sModIndex.count = 0;
        return;
    }

    u32 filled = 0;
    bool truncatedFill = false;
    ScanModsDir(entries, count, filled, truncatedFill);
    truncated = truncated || truncatedFill;
    // The sorted layout is what makes per-archive binary range lookup possible later.
    SortOverrideEntriesByArchiveTag(entries, filled);

    sModIndex.entries = entries;
    sModIndex.count = filled;
}

static bool IsFileExtensionSZS(const char* path) {
    return EndsWithIgnoreCase(path, ".szs");
}


// Ghidra references for DVDConvertPathToEntrynum (8015df4c) show the path-based DVD
// loaders that bypass ArchiveFile entirely:
// - 800910b4: nw4r::snd::DVDSoundArchive::Open
// - 8009130c: nw4r::snd::DVDSoundArchive::OpenExtStream
// - 8015e2d8: DVD::Open
// - 80222500: EGG::DvdFile::open(const char*)
// - 8052a914: system archive path lookup
// Redirecting those shared lookup sites makes whole-file loose overrides apply
// to BRSTM streams and other non-SZS files without changing their loaders.
static s32 ConvertPathToEntryNumWithLooseOverride(const char* path) {
    if (path == nullptr) return -1;

    char resolvedPath[OVERRIDE_MAX_PATH];
    const char* finalPath = ResolveWholeFileOverride(path, resolvedPath, sizeof(resolvedPath), nullptr);
    return DVD::ConvertPathToEntryNum(finalPath);
}
kmCall(0x800910b4, ConvertPathToEntryNumWithLooseOverride);
kmCall(0x8009130c, ConvertPathToEntryNumWithLooseOverride);
kmCall(0x8015e2d8, ConvertPathToEntryNumWithLooseOverride);
kmCall(0x80222500, ConvertPathToEntryNumWithLooseOverride);
kmCall(0x8052a914, ConvertPathToEntryNumWithLooseOverride);

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


    // The logical `/patches` prefix is always treated as internal to the feature,
    // but `sModsRootPath` may point at an SD-resolved location instead. Both need
    // to be recognized so redirected reads do not recursively redirect again.

    const u32 rootLen = strlen(sModsRootPath);
    if (rootLen == 0) return false;
    if (strncmp(path, sModsRootPath, rootLen) != 0) return false;
    return path[rootLen] == '\0' || path[rootLen] == '/';
}

const char* ResolveWholeFileOverride(const char* path, char* resolvedPath, u32 resolvedSize, bool* outRedirected) {
    if (outRedirected != nullptr) *outRedirected = false;
    if (path == nullptr || resolvedPath == nullptr || resolvedSize == 0) return path;
    if (!AreLooseArchiveOverridesEnabled()) return path;

    if (IsModsPath(path)) return path;
    if (!ModsRootExists()) return path;
    // Do not redirect individual loose-file requests for these raw resources into `/patches`.
    // Tagged archive-member overrides for the same extensions are also rejected during index construction.
    if (IsBlockedLooseRawOverrideExtension(path)) return path;

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
    if (!AreLooseArchiveOverridesEnabled()) return false;
    // Loose files under the mods root are already redirected content, so never feed them back into archive patching.
    if (IsModsPath(path)) return false;
    // Tagged member overrides only target compressed archive loads.
    if (!IsFileExtensionSZS(path)) return false;


    // This is only a cheap gate. It does not inspect the index yet. it simply
    // answers "is this a non-mods `.szs` request, and if so what bucket key would
    // its tagged member overrides live under?"

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
                         u32* outMissingOverrides, const u8* compressedData) {
    if (outAppliedOverrides != nullptr) *outAppliedOverrides = 0;
    if (outPatchedNodes != nullptr) *outPatchedNodes = 0;
    if (outMissingOverrides != nullptr) *outMissingOverrides = 0;
    if (!AreLooseArchiveOverridesEnabled()) return false;


    // Runtime flow:
    // 1. Pull the prebuilt bucket for this archive tag.
    // 2. Resolve each override to concrete U8 node indices.
    // 3. Patch in place when every replacement fits inside the original node.
    // 4. Otherwise repack the archive into a larger buffer and rewrite offsets.

    // `outAppliedOverrides` counts unique loose files that were successfully
    // consumed, while `outPatchedNodes` counts archive nodes rewritten. Those can
    // differ when one basename-only override matches multiple nodes.

    EnsureModIndexBuilt();
    if (sModIndex.entries == nullptr || sModIndex.count == 0) return false;
    if (archiveBase == nullptr || archiveBaseLower == nullptr || archiveBaseLower[0] == '\0') return false;
    if (sourceHeap == nullptr) {
        return false;
    }
    if (archiveHeap == nullptr) {
        archiveHeap = sourceHeap;
    }

    u32 rangeStart = 0;
    u32 rangeEnd = 0;
    if (!FindArchiveTagRange(sModIndex, archiveBaseLower, rangeStart, rangeEnd)) {
        return false;
    }
    const u32 taggedCandidates = rangeEnd - rangeStart;

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
    u8* entryApplied = EGG::Heap::alloc<u8>(sizeof(u8) * taggedCandidates, 0x20, tempHeap);
    if (nodeOverrideIndex == nullptr || entryApplied == nullptr) {
        // `nodeOverrideIndex` maps archive nodes to loose entries; `entryApplied` deduplicates reporting per loose file.
        if (nodeOverrideIndex != nullptr) EGG::Heap::free(nodeOverrideIndex, tempHeap);
        if (entryApplied != nullptr) EGG::Heap::free(entryApplied, tempHeap);
        return false;
    }

    memset(nodeOverrideIndex, 0xFF, sizeof(s32) * nodeCount);
    memset(entryApplied, 0, sizeof(u8) * taggedCandidates);

    bool anyOverrides = false;
    u32 missingOverrides = 0;

    //Build a node -> override table for this archive.

    //Nested paths (`button/timg/icon.tpl`) resolve directly through ARC path
    //lookup. Basename-only entries intentionally fan out to every file node with
    //that name, which is useful for archives that duplicate assets in multiple
    //folders but still want one shared loose override.
    for (u32 i = rangeStart; i < rangeEnd; ++i) {
        const OverrideEntry& entry = sModIndex.entries[i];
        const char* matchName = entry.strippedName;
        if (matchName[0] == '\0') {
            continue;
        }

        if (entry.hasSubpath) {
            s32 entryNum = ARC::ConvertPathToEntrynum(&handle, matchName);
            if (entryNum < 0) {
                // Count it as missing so logs can tell "override exists on disk" from "archive actually contains that node".
                ++missingOverrides;
                continue;
            }
            if (NodeIsDir(nodes[entryNum])) {
                // A matching directory path is still unusable because only file payload nodes can be replaced.
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
                    // Basename-only overrides intentionally fan out across every sibling with the same leaf filename.
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
        if (missingOverrides > 0) {
            OS::Report("[Pulsar] Loose overrides skipped for '%s': %u tagged file(s) did not match archive contents\n",
                       archiveBaseLower, missingOverrides);
        }
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
        // Any growth would invalidate later file offsets, so one oversized replacement forces the repack path.
        if (sModIndex.entries[idx].size > nodes[nodeIdx].dataSize) {
            needsRepack = true;
            break;
        }
    }


    u32 patchedNodes = 0;
    if (!needsRepack) {

        // Fast path: overwrite file payloads in place.

        // Smaller replacements are zero-filled to erase stale bytes from the old
        // payload, then the node size is updated so consumers see the trimmed file.
        // This avoids rebuilding the archive metadata when offsets stay valid.
        // This is also why any documentation should mention that 
        // replacing files with smaller loose overrides is safer for stability than larger ones, 
        // which always require repacking.

        for (u32 nodeIdx = 1; nodeIdx < nodeCount; ++nodeIdx) {
            const s32 idx = nodeOverrideIndex[nodeIdx];
            if (idx < 0) continue;
            if (NodeIsDir(nodes[nodeIdx])) continue;

            const OverrideEntry& entry = sModIndex.entries[idx];
            if (entry.size > nodes[nodeIdx].dataSize) {
                // Oversized writes are intentionally left to the repack path, never to in-place patching.
                continue;
            }


            void* dest = archiveBase + nodes[nodeIdx].dataOffset;
            // zappelin patches le game
            if (!ReadOverrideFile(entry, dest)) {
                continue;
            }
            if (entry.size < nodes[nodeIdx].dataSize) {
                memset(reinterpret_cast<u8*>(dest) + entry.size, 0, nodes[nodeIdx].dataSize - entry.size);
            }
            nodes[nodeIdx].dataSize = entry.size;
            OS::DCStoreRange(dest, entry.size);
            entryApplied[idx - rangeStart] = 1;
            ++patchedNodes;
        }

        if (patchedNodes > 0) {
            OS::DCStoreRange(nodes, nodeCount * sizeof(U8Node));
        }

        u32 appliedOverrides = 0;
        for (u32 i = 0; i < taggedCandidates; ++i) {
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
    // Repacking preserves the metadata prefix and rebuilds only the payload region starting here.
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
            // Reserve space for replacement sizes up front so the repack layout is fully known before copying.
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


    // Repack path: prefer moving to a roomier heap first so large overrides do
    // not starve the original archive heap. Growing the source heap is capped to
    // keep one oversized override from destabilizing the caller's allocation pool.

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
    u32* repackOriginalSizes = nullptr;
    u32* repackOrder = nullptr;
    u32 repackOrderCount = 0;

    if (repackHeap == archiveHeap && allowSourceHeap && compressedData != nullptr) {

        // Same-heap repack is only safe when every file moves forward and no
        // override shrinks after later files have been relocated. In that narrow
        // case we can free the old archive, decompress the original SZS back into
        // a new larger buffer on the same heap, and then move files downward from
        // highest original offset to lowest without clobbering unread data.

        repackOffsets = EGG::Heap::alloc<u32>(sizeof(u32) * nodeCount, 0x20, tempHeap);
        repackSizes = EGG::Heap::alloc<u32>(sizeof(u32) * nodeCount, 0x20, tempHeap);
        repackOriginalSizes = EGG::Heap::alloc<u32>(sizeof(u32) * nodeCount, 0x20, tempHeap);
        repackOrder = EGG::Heap::alloc<u32>(sizeof(u32) * nodeCount, 0x20, tempHeap);
        if (repackOffsets != nullptr && repackSizes != nullptr && repackOriginalSizes != nullptr && repackOrder != nullptr) {
            memset(repackOffsets, 0, sizeof(u32) * nodeCount);
            memset(repackSizes, 0, sizeof(u32) * nodeCount);
            memset(repackOriginalSizes, 0, sizeof(u32) * nodeCount);
            memset(repackOrder, 0, sizeof(u32) * nodeCount);
            u32 plannedOffset = dataStart;
            bool allOffsetsForward = true;
            bool hasShrinkOverride = false;
            for (u32 nodeIdx = 1; nodeIdx < nodeCount; ++nodeIdx) {
                if (NodeIsDir(nodes[nodeIdx])) continue;
                plannedOffset = nw4r::ut::RoundUp(plannedOffset, 0x20);
                repackOffsets[nodeIdx] = plannedOffset;
                const s32 idx = nodeOverrideIndex[nodeIdx];
                repackOriginalSizes[nodeIdx] = nodes[nodeIdx].dataSize;
                const u32 plannedSize = (idx >= 0) ? sModIndex.entries[idx].size : nodes[nodeIdx].dataSize;
                repackSizes[nodeIdx] = plannedSize;
                repackOrder[repackOrderCount++] = nodeIdx;
                if (plannedOffset < nodes[nodeIdx].dataOffset) {
                    allOffsetsForward = false;
                }
                if (idx >= 0 && plannedSize < nodes[nodeIdx].dataSize) {
                    // Same-heap repack cannot safely recover from a failed shrink override after later files move.
                    hasShrinkOverride = true;
                }
                plannedOffset += nw4r::ut::RoundUp(plannedSize, 0x20);
            }
            useSameHeapRepack = allOffsetsForward && !hasShrinkOverride;
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
            if (repackOriginalSizes != nullptr) EGG::Heap::free(repackOriginalSizes, tempHeap);
            if (repackOrder != nullptr) EGG::Heap::free(repackOrder, tempHeap);
            repackOffsets = nullptr;
            repackSizes = nullptr;
            repackOriginalSizes = nullptr;
            repackOrder = nullptr;
            repackOrderCount = 0;
        }
    }

    if (useSameHeapRepack) {
        // Free first, then recreate from compressed data; the old decompressed buffer has no headroom for growth.
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
        OS::Report("[Pulsar] Loose override repack allocation failed for '%s': old=0x%X new=0x%X growth=0x%X%s\n",
                   archiveBaseLower, archiveSize, newSize, growth, allowSourceHeap ? "" : " source-heap growth capped");
        if (archiveBase == nullptr && compressedData != nullptr) {
            // Same-heap repack may already have released the old archive, so rebuild the original before bailing out.
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
        if (repackOriginalSizes != nullptr) EGG::Heap::free(repackOriginalSizes, tempHeap);
        if (repackOrder != nullptr) EGG::Heap::free(repackOrder, tempHeap);
        EGG::Heap::free(nodeOverrideIndex, tempHeap);
        EGG::Heap::free(entryApplied, tempHeap);
        return false;
    }

    if (!useSameHeapRepack) {
        // Copy the untouched metadata prefix now; file payloads get rewritten into their new aligned slots below.
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
                // Same-heap relocation can overlap source and destination ranges, so `memmove` is required.
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
            const u32 oldSize = repackOriginalSizes[nodeIdx];
            const u32 newOffset = repackOffsets[nodeIdx];

            if (!ReadOverrideFile(entry, newBuffer + newOffset)) {
                // If the loose file is unreadable, keep the relocated original payload size for this node.
                newNodes[nodeIdx].dataSize = oldSize;
                continue;
            }
            entryApplied[idx - rangeStart] = 1;
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
                const OverrideEntry& entry = sModIndex.entries[idx];
                OS::Report("[Pulsar] Loose override '%s' skipped in '%s': repack buffer too small for 0x%X bytes\n",
                           entry.relativePath, archiveBaseLower, newFileSize);
                // Recover by copying the original member instead of throwing away the entire repack.
                useOverride = false;
                newFileSize = oldSize;
            }

            newNodes[nodeIdx].dataOffset = writeOffset;
            if (useOverride) {
                const OverrideEntry& entry = sModIndex.entries[idx];
                if (!ReadOverrideFile(entry, newBuffer + writeOffset)) {
                    // One broken loose file should not invalidate the rest of the archive rebuild.
                    useOverride = false;
                    newFileSize = oldSize;
                } else {
                    entryApplied[idx - rangeStart] = 1;
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
    // Clamp to the allocated size so bad metadata cannot claim a larger archive than the buffer we own.
    if (finalSize > newSize) finalSize = newSize;
    OS::DCStoreRange(newBuffer, finalSize);

    if (!useSameHeapRepack) {
        EGG::Heap::free(archiveBase, sourceHeap);
    }
    archiveBase = newBuffer;
    archiveSize = finalSize;
    archiveHeap = repackHeap;

    u32 appliedOverrides = 0;
    for (u32 i = 0; i < taggedCandidates; ++i) {
        if (entryApplied[i]) ++appliedOverrides;
    }

    if (outAppliedOverrides != nullptr) *outAppliedOverrides = appliedOverrides;
    if (outPatchedNodes != nullptr) *outPatchedNodes = patchedNodes;
    if (outMissingOverrides != nullptr) *outMissingOverrides = missingOverrides;
    if (missingOverrides > 0) {
        OS::Report("[Pulsar] Loose overrides for '%s': applied=%u patched=%u missing=%u\n", archiveBaseLower,
                   appliedOverrides, patchedNodes, missingOverrides);
    }

    if (repackOffsets != nullptr) EGG::Heap::free(repackOffsets, tempHeap);
    if (repackSizes != nullptr) EGG::Heap::free(repackSizes, tempHeap);
    if (repackOriginalSizes != nullptr) EGG::Heap::free(repackOriginalSizes, tempHeap);
    if (repackOrder != nullptr) EGG::Heap::free(repackOrder, tempHeap);
    EGG::Heap::free(nodeOverrideIndex, tempHeap);
    EGG::Heap::free(entryApplied, tempHeap);
    return appliedOverrides > 0;
}

static void ArchiveFileLoadOverride(ArchiveFile* file, const char* path, EGG::Heap* mountHeap, bool isCompressed,
                                    s32 allocDirection, EGG::Heap* dumpHeap, EGG::Archive::FileInfo* info) {


    // It first normalizes a quirk of MKW's UI archive naming where a localized
    // request can be followed by a fallback `.szs` request using only the suffix.
    // After that, whole-file redirection happens before the game rips/decompresses
    // the archive, so plain loose replacements bypass the more expensive tagged
    // member patch pipeline entirely.

    // System::ResourceManager::loadUI at 0x80540680 is the entry point that kicks UI archive loading for scene archive slot 2.
    // GameSource/MarioKartWii/Archive/ArchiveMgr.hpp:75. <-- this function
    // System::UIArchivesHolder::Reset at 0x8052a2fc is the part that creates the UI language fallback setup.
    // it calls the base reset, then writes LOCALIZED_SZS[language] into suffix slot 1. The base reset leaves slot 0 as plain ".szs".
    // ArchivesHolder::LoadArchives at 0x8052a954 is where the filenames are actually formed.
    // it effectively does snprintf(fullname, "%s%s", filename, this->suffixes[i]);, 
    // so the game builds both base + "_E.szs" and base + ".szs" from the same UI base name.

    // ArchivesHolder::GetFile at 0x8052a760 searches mounted archives from the last slot down to the first
    // so the localized UI archive is checked first and the plain .szs archive is the fallback.

    char normalizedPath[OVERRIDE_MAX_PATH];
    const char* requestedPath = path;
    if (path != nullptr) {
        const char* base = FindBasename(path);
        if (base != nullptr && strcmp(base, ".szs") == 0 && StartsWith(sLastUIArchiveBase, "/Scene/UI/")) {
            // MKW sometimes follows a localized request with a suffix-only `.szs` request; restore the cached basename.
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
                // Cache only short locale/style suffix forms such as `_E`; longer names are treated as real basenames.
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
            file->Decompress(requestedPath, mountHeap, info);
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
