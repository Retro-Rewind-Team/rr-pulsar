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
#include <include/c_stdio.h>
#include <include/c_stdlib.h>
#include <include/c_string.h>
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
// `/patches` supports whole-file redirects and tagged archive-member overrides.
// Tagged files use `member.ext.ArchiveTag`; the tag maps to `ArchiveTag.szs`.
const char kModsRoot[] = "/patches";
const char kModsRootPrefix[] = "/patches/";
const u32 kMaxOverridesTotal = 4096;
const u32 kBRSAROverrideSlotCount = 1024;
const u32 kOverrideMaxGrowthOnSourceHeap = 0x100000;
const u32 kInvalidPoolOffset = 0xFFFFFFFFu;
const u16 kInvalidScratchIndex16 = 0xFFFFu;

enum OverrideEntryFlags {
    OVERRIDEENTRYFLAG_NONE = 0,
    OVERRIDEENTRYFLAG_HAS_SUBPATH = (1 << 0),
    OVERRIDEENTRYFLAG_IS_DELETE = (1 << 1)
};

struct TaggedOverrideEntry {
    u32 sourcePathOffset;
    u32 size;
    u16 tagId;
    u16 flags;
};

struct WholeFileOverrideEntry {
    u32 sourcePathOffset;
    u32 basenameHash;
};

enum BRSAROverrideType {
    BRSAROVERRIDE_INVALID = 0,
    BRSAROVERRIDE_BRWSD,
    BRSAROVERRIDE_BRBNK
};

struct BRSAROverrideSlot {
    u32 sourcePathOffset;
    u32 size;
    u32 fileDataSize;
    u32 waveDataOffset;
    u32 waveDataSize;
    u8 type;
    u8 layoutState;
    u8 reserved[2];
};

struct OverrideTagEntry {
    u32 nameOffset;
    u16 startIndex;
    u16 count;
};

// Built on first use and kept persistent because archive loads are a hot path.
struct OverrideDatabase {
    void* block;
    u32 blockSize;
    EGG::Heap* heap;

    char* stringPool;
    u32 stringPoolSize;
    u32 stringPoolUsed;

    OverrideTagEntry* tags;
    u32 tagCount;
    u32 tagCapacity;

    TaggedOverrideEntry* taggedEntries;
    u32 taggedCount;

    WholeFileOverrideEntry* wholeFileEntries;
    u32 wholeFileCount;

    BRSAROverrideSlot* brsarSlots;
    u32 brsarSlotCount;
    u32 brsarCount;
};

struct LooseOverrideScratch {
    u16* nodeOverrideIndex;
    u32 nodeOverrideCapacity;
    u32* entryAppliedBits;
    u32 entryAppliedCapacity;
    u16* basenameHashHeads16;
    u16* basenameHashNext16;
    s32* basenameHashHeads32;
    s32* basenameHashNext32;
    u32 basenameHashCapacity;
    bool useWideBasenameIndices;
    u32* repackOffsets;
    u32* repackSizes;
    u32* repackOriginalSizes;
    u32* repackOrder;
    u32 repackCapacity;
    EGG::Heap* heap;
};

struct ScanBuildState {
    OverrideDatabase* database;
    TaggedOverrideEntry* taggedEntries;
    u32 taggedCount;
    bool taggedTruncated;
    WholeFileOverrideEntry* wholeFileEntries;
    u32 wholeFileCount;
    bool wholeFileTruncated;
    BRSAROverrideSlot* brsarSlots;
    u32 brsarCount;
    bool brsarTruncated;
    u32 stringBytes;
    u32 tagStringBytes;
    u8* brsarSlotOccupied;
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

struct PendingStructuralAddCandidate {
    u16 parentDirIndex;
    u16 overrideIndex;
};

struct PendingStructuralAdd {
    u16 parentDirIndex;
    u16 overrideIndex;
    u32 pathOffset;
    u32 nameOffset;
};

struct StructuralChildRef {
    u32 oldNodeIndex;
    u32 addedFileIndex;
    const char* name;
    bool isAddedFile;
};

static OverrideDatabase sOverrideDatabase = {};
static OverrideDatabase* sActiveOverrideDatabase = &sOverrideDatabase;
static u8 sLoggedBRSARLayoutFailure[1024] = {};
static bool sOverrideIndicesAttempted = false;
static bool sHasWholeFileOverrides = false;
static bool sModsRootChecked = false;
static bool sModsRootPresent = false;
static char sModsRootPath[OVERRIDE_MAX_PATH] = "/patches";
static bool sOverrideCacheStateInitialized = false;
static bool sCachedLooseOverridesEnabled = false;
static char sCachedModFolder[OVERRIDE_MAX_PATH] = "";
static char sLastUIArchiveBase[32] = "";
static LooseOverrideScratch sLooseOverrideScratch = {};

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
    return strncmp(str, prefix, strlen(prefix)) == 0;
}

static const char* FindLastChar(const char* str, char needle) {
    if (str == nullptr) return nullptr;
    const char* last = nullptr;
    const char* cursor = str;
    while ((cursor = strchr(cursor, needle)) != nullptr) {
        last = cursor;
        ++cursor;
    }
    return last;
}

static const char* FindBasename(const char* path) {
    if (path == nullptr) return nullptr;
    const char* lastSlash = FindLastChar(path, '/');
    return lastSlash ? lastSlash + 1 : path;
}

static u32 MaxU32(u32 lhs, u32 rhs) {
    return lhs > rhs ? lhs : rhs;
}

static void SetOverrideResult(u32* outAppliedOverrides, u32 appliedOverrides, u32* outPatchedNodes,
                              u32 patchedNodes, u32* outMissingOverrides, u32 missingOverrides) {
    if (outAppliedOverrides != nullptr) *outAppliedOverrides = appliedOverrides;
    if (outPatchedNodes != nullptr) *outPatchedNodes = patchedNodes;
    if (outMissingOverrides != nullptr) *outMissingOverrides = missingOverrides;
}

struct HeapCandidate {
    EGG::Heap* heap;
    u32 reclaimedBytes;
};

static EGG::Heap* FindHeapWithSpace(const HeapCandidate* candidates, u32 count, u32 requiredSize) {
    if (candidates == nullptr) return nullptr;

    for (u32 i = 0; i < count; ++i) {
        EGG::Heap* heap = candidates[i].heap;
        if (heap == nullptr) continue;

        bool alreadyChecked = false;
        for (u32 j = 0; j < i; ++j) {
            if (candidates[j].heap == heap && candidates[j].reclaimedBytes >= candidates[i].reclaimedBytes) {
                alreadyChecked = true;
                break;
            }
        }
        if (alreadyChecked) continue;

        if (heap->getAllocatableSize(0x20) + candidates[i].reclaimedBytes >= requiredSize) {
            return heap;
        }
    }
    return nullptr;
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
    // Tags are lookup keys, so truncation is acceptable if the string stays terminated.
    dest[i] = '\0';
}

static void ToLowerInPlace(char* str) {
    if (str == nullptr) return;
    for (; *str != '\0'; ++str) {
        if (*str >= 'A' && *str <= 'Z') *str = static_cast<char>(*str - 'A' + 'a');
    }
}

static u32 ReadBE32(const void* data) {
    const u8* bytes = reinterpret_cast<const u8*>(data);
    return (static_cast<u32>(bytes[0]) << 24) | (static_cast<u32>(bytes[1]) << 16) |
           (static_cast<u32>(bytes[2]) << 8) | static_cast<u32>(bytes[3]);
}

static inline u32 Align32(u32 value) {
    return nw4r::ut::RoundUp(value, 0x20);
}

static s32 CompareWholeFileBasenames(const char* lhs, const char* rhs) {
    if (lhs == rhs) return 0;
    if (lhs == nullptr) return -1;
    if (rhs == nullptr) return 1;
    return strcmp(lhs, rhs);
}

static bool DecodeOverrideRelativePath(char* dest, u32 destSize, const char* src);
static bool TryParseArchiveTag(const char* relativePath, char* strippedName, u32 strippedNameSize,
                               char* archiveTagLower, u32 archiveTagLowerSize);
static bool ExtractTaggedOverrideMetadata(const char* relativePath, char* strippedName, u32 strippedNameSize,
                                          char* archiveTagLower, u32 archiveTagLowerSize, bool& outIsDelete);
static bool BuildOverridePathWithRoot(const char* root, const char* name, const char* tag, char* outPath, u32 outSize);

static u32 HashLowerCaseString(const char* name) {
    if (name == nullptr) return 0;
    u32 hash = 2166136261u;
    for (const char* cursor = name; *cursor != '\0'; ++cursor) {
        char c = *cursor;
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
        hash ^= static_cast<u8>(c);
        hash *= 16777619u;
    }
    return hash;
}

static const char* GetPooledString(const OverrideDatabase& database, u32 offset) {
    if (database.stringPool == nullptr || offset >= database.stringPoolUsed) return nullptr;
    return database.stringPool + offset;
}

static const char* GetRelativePath(u32 sourcePathOffset) {
    if (sActiveOverrideDatabase == nullptr) return nullptr;
    return GetPooledString(*sActiveOverrideDatabase, sourcePathOffset);
}

static bool BuildStoredOverridePath(u32 sourcePathOffset, char* outPath, u32 outSize) {
    const char* relativePath = GetRelativePath(sourcePathOffset);
    if (relativePath == nullptr) return false;
    return BuildOverridePathWithRoot(kModsRoot, relativePath, nullptr, outPath, outSize);
}

static bool DecodeStoredOverrideRelativePath(u32 sourcePathOffset, char* decodedPath, u32 decodedSize) {
    const char* relativePath = GetRelativePath(sourcePathOffset);
    if (relativePath == nullptr) return false;
    return DecodeOverrideRelativePath(decodedPath, decodedSize, relativePath);
}

static bool GetTaggedEntryMatchName(const TaggedOverrideEntry& entry, char* outName, u32 outNameSize) {
    if (outName == nullptr || outNameSize == 0) return false;

    char decodedPath[OVERRIDE_MAX_PATH];
    char archiveTagLower[OVERRIDE_MAX_NAME];
    bool isDelete = false;
    if (!DecodeStoredOverrideRelativePath(entry.sourcePathOffset, decodedPath, sizeof(decodedPath))) {
        outName[0] = '\0';
        return false;
    }
    return ExtractTaggedOverrideMetadata(decodedPath, outName, outNameSize, archiveTagLower, sizeof(archiveTagLower),
                                         isDelete);
}

static bool GetWholeFileEntryBasenameLower(const WholeFileOverrideEntry& entry, char* outBasename, u32 outSize) {
    if (outBasename == nullptr || outSize == 0) return false;

    const char* relativePath = GetRelativePath(entry.sourcePathOffset);
    if (relativePath == nullptr) {
        outBasename[0] = '\0';
        return false;
    }

    const char* basename = FindBasename(relativePath);
    if (basename == nullptr) {
        outBasename[0] = '\0';
        return false;
    }
    ToLowerCopy(outBasename, basename, outSize);
    return outBasename[0] != '\0';
}

static bool GetTagIdForName(OverrideDatabase& database, const char* tagName, u16& outTagId) {
    outTagId = 0;
    if (tagName == nullptr || tagName[0] == '\0' || database.tags == nullptr) return false;

    for (u32 i = 0; i < database.tagCount; ++i) {
        const char* existing = GetPooledString(database, database.tags[i].nameOffset);
        if (existing != nullptr && strcmp(existing, tagName) == 0) {
            outTagId = static_cast<u16>(i);
            return true;
        }
    }

    if (database.tagCount >= database.tagCapacity) return false;

    const u32 tagLen = static_cast<u32>(strlen(tagName)) + 1;
    if (database.stringPool == nullptr || database.stringPoolUsed + tagLen > database.stringPoolSize) return false;

    const u32 offset = database.stringPoolUsed;
    memcpy(database.stringPool + offset, tagName, tagLen);
    database.stringPoolUsed += tagLen;

    outTagId = static_cast<u16>(database.tagCount);
    database.tags[database.tagCount].nameOffset = offset;
    database.tags[database.tagCount].startIndex = 0;
    database.tags[database.tagCount].count = 0;
    ++database.tagCount;
    return true;
}

static bool AddRelativePathToPool(OverrideDatabase& database, const char* relativePath, u32& outOffset) {
    outOffset = 0;
    if (relativePath == nullptr || database.stringPool == nullptr) return false;

    const u32 pathLen = static_cast<u32>(strlen(relativePath)) + 1;
    if (database.stringPoolUsed + pathLen > database.stringPoolSize) return false;

    outOffset = database.stringPoolUsed;
    memcpy(database.stringPool + outOffset, relativePath, pathLen);
    database.stringPoolUsed += pathLen;
    return true;
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

    // Require `name.ext.Tag` so plain files like `Common.szs` stay whole-file redirects.

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

static bool IsSupportedBRSAROverrideTypeSuffix(const char* suffix, u8& outType) {
    outType = BRSAROVERRIDE_INVALID;
    if (suffix == nullptr) return false;

    if (strcmp(suffix, ".brwsd") == 0 || strcmp(suffix, ".rwsd") == 0) {
        outType = BRSAROVERRIDE_BRWSD;
        return true;
    }
    if (strcmp(suffix, ".brbnk") == 0 || strcmp(suffix, ".rbnk") == 0) {
        outType = BRSAROVERRIDE_BRBNK;
        return true;
    }
    return false;
}

static bool TryParseExactFileId(const char* stem, u32& outFileId) {
    outFileId = 0;
    if (stem == nullptr || stem[0] == '\0') return false;

    u32 value = 0;
    u32 index = 0;
    while (stem[index] >= '0' && stem[index] <= '9') {
        value = value * 10 + static_cast<u32>(stem[index] - '0');
        ++index;
    }
    if (index == 0) return false;
    if (stem[index] != '\0') return false;

    outFileId = value;
    return true;
}

static bool TryParseBRSAROverride(const char* relativePath, u32& outFileId, u8& outType) {
    outFileId = 0;
    outType = BRSAROVERRIDE_INVALID;
    if (relativePath == nullptr) return false;

    const char* filename = FindBasename(relativePath);
    if (filename == nullptr || filename[0] == '\0') return false;

    char lowerName[OVERRIDE_MAX_PATH];
    ToLowerCopy(lowerName, filename, sizeof(lowerName));

    const char* lastDot = FindLastChar(lowerName, '.');
    if (lastDot == nullptr) return false;

    u8 type = BRSAROVERRIDE_INVALID;
    if (!IsSupportedBRSAROverrideTypeSuffix(lastDot, type)) return false;

    const char* firstDot = strchr(lowerName, '.');
    if (firstDot == nullptr || firstDot == lowerName) return false;
    if (firstDot != lastDot && firstDot + 1 >= lastDot) {
        // Reject malformed names such as `<fileId>..brwsd`.
        return false;
    }

    const u32 idLen = static_cast<u32>(firstDot - lowerName);
    if (idLen == 0 || idLen >= sizeof(lowerName)) return false;

    char fileIdStem[OVERRIDE_MAX_PATH];
    memcpy(fileIdStem, lowerName, idLen);
    fileIdStem[idLen] = '\0';

    // `<fileId>.<type>` or `<fileId>.<anything>.<type>`
    if (!TryParseExactFileId(fileIdStem, outFileId)) return false;

    outType = type;
    return true;
}

static s32 CompareTaggedOverrideEntries(const TaggedOverrideEntry& lhs, const TaggedOverrideEntry& rhs) {
    if (lhs.tagId < rhs.tagId) return -1;
    if (lhs.tagId > rhs.tagId) return 1;

    char lhsName[OVERRIDE_MAX_PATH];
    char rhsName[OVERRIDE_MAX_PATH];
    const bool lhsParsed = GetTaggedEntryMatchName(lhs, lhsName, sizeof(lhsName));
    const bool rhsParsed = GetTaggedEntryMatchName(rhs, rhsName, sizeof(rhsName));
    if (!lhsParsed && !rhsParsed) return 0;
    if (!lhsParsed) return -1;
    if (!rhsParsed) return 1;
    return strcmp(lhsName, rhsName);
}

static int CompareTaggedOverrideEntriesForQSort(const void* lhs, const void* rhs) {
    return CompareTaggedOverrideEntries(*static_cast<const TaggedOverrideEntry*>(lhs),
                                        *static_cast<const TaggedOverrideEntry*>(rhs));
}

static void SortOverrideEntriesByArchiveTag(TaggedOverrideEntry* entries, u32 count) {
    if (entries == nullptr || count < 2) return;

    qsort(entries, count, sizeof(TaggedOverrideEntry), CompareTaggedOverrideEntriesForQSort);
}

static void BuildTaggedOverrideRanges(OverrideDatabase& database) {
    if (database.tags == nullptr || database.tagCount == 0) return;

    for (u32 i = 0; i < database.tagCount; ++i) {
        database.tags[i].startIndex = 0;
        database.tags[i].count = 0;
    }

    if (database.taggedEntries == nullptr || database.taggedCount == 0) return;

    for (u32 i = 0; i < database.taggedCount; ++i) {
        const u16 tagId = database.taggedEntries[i].tagId;
        if (tagId >= database.tagCount) continue;

        OverrideTagEntry& tag = database.tags[tagId];
        if (tag.count == 0) {
            tag.startIndex = static_cast<u16>(i);
        }
        ++tag.count;
    }
}

static bool FindArchiveTagId(const OverrideDatabase& database, const char* archiveBaseLower, u16& outTagId) {
    outTagId = 0;
    if (database.tags == nullptr || database.tagCount == 0 || archiveBaseLower == nullptr || archiveBaseLower[0] == '\0') {
        return false;
    }

    for (u32 i = 0; i < database.tagCount; ++i) {
        const char* tagName = GetPooledString(database, database.tags[i].nameOffset);
        if (tagName != nullptr && strcmp(tagName, archiveBaseLower) == 0) {
            outTagId = static_cast<u16>(i);
            return true;
        }
    }
    return false;
}

static bool FindArchiveTagRangeById(const OverrideDatabase& database, u16 tagId, u32& start, u32& end) {
    start = 0;
    end = 0;
    if (database.tags == nullptr || database.taggedEntries == nullptr || database.taggedCount == 0 ||
        tagId >= database.tagCount) {
        return false;
    }

    const OverrideTagEntry& tag = database.tags[tagId];
    if (tag.count == 0) return false;

    start = tag.startIndex;
    end = start + tag.count;
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

static u32 HashArchiveMemberBasename(const char* name) {
    if (name == nullptr) return 0;
    u32 hash = 2166136261u;
    for (const char* cursor = name; *cursor != '\0'; ++cursor) {
        hash ^= static_cast<u8>(*cursor);
        hash *= 16777619u;
    }
    return hash;
}

static u32 GetBasenameHashCapacity(u32 nodeCapacity) {
    u32 capacity = 8;
    u32 target = (nodeCapacity > 0x7FFFFFFFu) ? 0xFFFFFFFFu : (nodeCapacity * 2);
    if (target < 8) target = 8;
    while (capacity < target && capacity < 0x80000000u) {
        capacity <<= 1;
    }
    return capacity;
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

static bool StripDeleteSuffixInPlace(char* path) {
    if (path == nullptr) return false;
    const size_t len = strlen(path);
    static const char kDeleteSuffix[] = ".delete";
    const size_t suffixLen = sizeof(kDeleteSuffix) - 1;
    if (len < suffixLen) return false;

    char* tail = path + (len - suffixLen);
    if (!EndsWithIgnoreCase(path, kDeleteSuffix)) return false;

    tail[0] = '\0';
    return true;
}

static bool DecodeOverrideRelativePath(char* dest, u32 destSize, const char* src) {
    if (dest == nullptr || destSize == 0) return false;
    if (src == nullptr) {
        dest[0] = '\0';
        return true;
    }

    u32 writeIdx = 0;
    const char* cursor = src;

    // `[button][timg]icon.tpl.Channel` matches `button/timg/icon.tpl.Channel`.

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

static bool ExtractTaggedOverrideMetadata(const char* relativePath, char* strippedName, u32 strippedNameSize,
                                          char* archiveTagLower, u32 archiveTagLowerSize, bool& outIsDelete) {
    outIsDelete = false;
    if (!TryParseArchiveTag(relativePath, strippedName, strippedNameSize, archiveTagLower, archiveTagLowerSize)) {
        return false;
    }

    outIsDelete = StripDeleteSuffixInPlace(strippedName);
    return strippedName[0] != '\0';
}

static void SetModsRootPath(const char* path) {
    CopyPath(sModsRootPath, sizeof(sModsRootPath), path);
}

static void FreeOverrideDatabase(OverrideDatabase& database) {
    if (database.block != nullptr && database.heap != nullptr) {
        EGG::Heap::free(database.block, database.heap);
    }
    database = OverrideDatabase();
}

static u32 GetEntryAppliedWordCount(u32 entryCapacity) {
    return (entryCapacity + 31) >> 5;
}

static void ClearEntryAppliedBits(u32* entryAppliedBits, u32 entryCapacity) {
    if (entryAppliedBits == nullptr) return;
    memset(entryAppliedBits, 0, sizeof(u32) * GetEntryAppliedWordCount(entryCapacity));
}

static void MarkEntryApplied(u32* entryAppliedBits, u32 entryIndex) {
    if (entryAppliedBits == nullptr) return;
    entryAppliedBits[entryIndex >> 5] |= (1u << (entryIndex & 31));
}

static bool IsEntryApplied(const u32* entryAppliedBits, u32 entryIndex) {
    if (entryAppliedBits == nullptr) return false;
    return (entryAppliedBits[entryIndex >> 5] & (1u << (entryIndex & 31))) != 0;
}

static u32 CountAppliedEntries(const u32* entryAppliedBits, u32 entryCapacity) {
    u32 appliedCount = 0;
    for (u32 i = 0; i < entryCapacity; ++i) {
        if (IsEntryApplied(entryAppliedBits, i)) ++appliedCount;
    }
    return appliedCount;
}

static u32 GetLooseOverrideScratchFootprint(u32 nodeCapacity, u32 entryCapacity, u32 repackCapacity,
                                            u32 basenameHashCapacity, bool useWideBasenameIndices) {
    u32 footprint = 0;
    footprint += Align32(sizeof(u16) * nodeCapacity);
    footprint += Align32(sizeof(u32) * GetEntryAppliedWordCount(entryCapacity));
    if (useWideBasenameIndices) {
        footprint += Align32(sizeof(s32) * nodeCapacity);
        footprint += Align32(sizeof(s32) * basenameHashCapacity);
    } else {
        footprint += Align32(sizeof(u16) * nodeCapacity);
        footprint += Align32(sizeof(u16) * basenameHashCapacity);
    }
    if (repackCapacity > 0) {
        footprint += Align32(sizeof(u32) * repackCapacity) * 4;
    }
    return footprint;
}

static u32 GetLooseOverrideScratchFootprint(const LooseOverrideScratch& scratch) {
    return GetLooseOverrideScratchFootprint(scratch.nodeOverrideCapacity, scratch.entryAppliedCapacity,
                                            scratch.repackCapacity, scratch.basenameHashCapacity,
                                            scratch.useWideBasenameIndices);
}

static void FreeLooseOverrideScratch(LooseOverrideScratch& scratch) {
    if (scratch.heap != nullptr) {
        if (scratch.nodeOverrideIndex != nullptr) EGG::Heap::free(scratch.nodeOverrideIndex, scratch.heap);
        if (scratch.entryAppliedBits != nullptr) EGG::Heap::free(scratch.entryAppliedBits, scratch.heap);
        if (scratch.basenameHashHeads16 != nullptr) EGG::Heap::free(scratch.basenameHashHeads16, scratch.heap);
        if (scratch.basenameHashNext16 != nullptr) EGG::Heap::free(scratch.basenameHashNext16, scratch.heap);
        if (scratch.basenameHashHeads32 != nullptr) EGG::Heap::free(scratch.basenameHashHeads32, scratch.heap);
        if (scratch.basenameHashNext32 != nullptr) EGG::Heap::free(scratch.basenameHashNext32, scratch.heap);
        if (scratch.repackOffsets != nullptr) EGG::Heap::free(scratch.repackOffsets, scratch.heap);
        if (scratch.repackSizes != nullptr) EGG::Heap::free(scratch.repackSizes, scratch.heap);
        if (scratch.repackOriginalSizes != nullptr) EGG::Heap::free(scratch.repackOriginalSizes, scratch.heap);
        if (scratch.repackOrder != nullptr) EGG::Heap::free(scratch.repackOrder, scratch.heap);
    }
    scratch = LooseOverrideScratch();
}

static EGG::Heap* GetOverridesHeap();

static EGG::Heap* GetLooseOverrideScratchHeap(u32 requiredSize, EGG::Heap* fallbackHeap) {
    const u32 currentFootprint = GetLooseOverrideScratchFootprint(sLooseOverrideScratch);
    HeapCandidate candidates[5];
    candidates[0].heap = RKSystem::mInstance.EGGRootMEM2;
    candidates[0].reclaimedBytes = 0;
    candidates[1].heap = sLooseOverrideScratch.heap;
    // Scratch contents are transient per archive load, so growth can reclaim the old buffers first.
    candidates[1].reclaimedBytes = currentFootprint;
    candidates[2].heap = GetOverridesHeap();
    candidates[2].reclaimedBytes = 0;
    candidates[3].heap = RKSystem::mInstance.EGGRootMEM1;
    candidates[3].reclaimedBytes = 0;
    candidates[4].heap = fallbackHeap;
    candidates[4].reclaimedBytes = 0;
    return FindHeapWithSpace(candidates, 5, requiredSize);
}

static bool EnsureLooseOverrideScratchCapacity(u32 nodeCapacity, u32 entryCapacity, u32 repackCapacity,
                                               EGG::Heap* fallbackHeap) {
    if (nodeCapacity == 0 || entryCapacity == 0) return false;
    const u32 basenameHashCapacity = GetBasenameHashCapacity(nodeCapacity);
    const bool useWideBasenameIndices = sLooseOverrideScratch.useWideBasenameIndices || (nodeCapacity > 65534);
    if (sLooseOverrideScratch.nodeOverrideCapacity >= nodeCapacity &&
        sLooseOverrideScratch.entryAppliedCapacity >= entryCapacity &&
        sLooseOverrideScratch.basenameHashCapacity >= basenameHashCapacity &&
        sLooseOverrideScratch.repackCapacity >= repackCapacity &&
        sLooseOverrideScratch.useWideBasenameIndices == useWideBasenameIndices &&
        sLooseOverrideScratch.nodeOverrideIndex != nullptr && sLooseOverrideScratch.entryAppliedBits != nullptr &&
        ((useWideBasenameIndices && sLooseOverrideScratch.basenameHashHeads32 != nullptr &&
          sLooseOverrideScratch.basenameHashNext32 != nullptr) ||
         (!useWideBasenameIndices && sLooseOverrideScratch.basenameHashHeads16 != nullptr &&
          sLooseOverrideScratch.basenameHashNext16 != nullptr)) &&
        (repackCapacity == 0 ||
         (sLooseOverrideScratch.repackOffsets != nullptr && sLooseOverrideScratch.repackSizes != nullptr &&
          sLooseOverrideScratch.repackOriginalSizes != nullptr && sLooseOverrideScratch.repackOrder != nullptr))) {
        return true;
    }

    const u32 targetNodeCapacity = MaxU32(sLooseOverrideScratch.nodeOverrideCapacity, nodeCapacity);
    const u32 targetEntryCapacity = MaxU32(sLooseOverrideScratch.entryAppliedCapacity, entryCapacity);
    const u32 targetBasenameHashCapacity = MaxU32(sLooseOverrideScratch.basenameHashCapacity, basenameHashCapacity);
    const u32 targetRepackCapacity = MaxU32(sLooseOverrideScratch.repackCapacity, repackCapacity);
    const u32 requiredSize = GetLooseOverrideScratchFootprint(targetNodeCapacity, targetEntryCapacity,
                                                              targetRepackCapacity, targetBasenameHashCapacity,
                                                              useWideBasenameIndices);

    EGG::Heap* heap = GetLooseOverrideScratchHeap(requiredSize, fallbackHeap);
    if (heap == nullptr) {
        return false;
    }

    FreeLooseOverrideScratch(sLooseOverrideScratch);

    sLooseOverrideScratch.nodeOverrideIndex = EGG::Heap::alloc<u16>(sizeof(u16) * targetNodeCapacity, 0x20, heap);
    sLooseOverrideScratch.entryAppliedBits =
        EGG::Heap::alloc<u32>(sizeof(u32) * GetEntryAppliedWordCount(targetEntryCapacity), 0x20, heap);
    if (useWideBasenameIndices) {
        sLooseOverrideScratch.basenameHashHeads32 =
            EGG::Heap::alloc<s32>(sizeof(s32) * targetBasenameHashCapacity, 0x20, heap);
        sLooseOverrideScratch.basenameHashNext32 =
            EGG::Heap::alloc<s32>(sizeof(s32) * targetNodeCapacity, 0x20, heap);
    } else {
        sLooseOverrideScratch.basenameHashHeads16 =
            EGG::Heap::alloc<u16>(sizeof(u16) * targetBasenameHashCapacity, 0x20, heap);
        sLooseOverrideScratch.basenameHashNext16 =
            EGG::Heap::alloc<u16>(sizeof(u16) * targetNodeCapacity, 0x20, heap);
    }
    if (targetRepackCapacity > 0) {
        sLooseOverrideScratch.repackOffsets = EGG::Heap::alloc<u32>(sizeof(u32) * targetRepackCapacity, 0x20, heap);
        sLooseOverrideScratch.repackSizes = EGG::Heap::alloc<u32>(sizeof(u32) * targetRepackCapacity, 0x20, heap);
        sLooseOverrideScratch.repackOriginalSizes =
            EGG::Heap::alloc<u32>(sizeof(u32) * targetRepackCapacity, 0x20, heap);
        sLooseOverrideScratch.repackOrder = EGG::Heap::alloc<u32>(sizeof(u32) * targetRepackCapacity, 0x20, heap);
    }

    if (sLooseOverrideScratch.nodeOverrideIndex == nullptr || sLooseOverrideScratch.entryAppliedBits == nullptr ||
        (useWideBasenameIndices &&
         (sLooseOverrideScratch.basenameHashHeads32 == nullptr || sLooseOverrideScratch.basenameHashNext32 == nullptr)) ||
        (!useWideBasenameIndices &&
         (sLooseOverrideScratch.basenameHashHeads16 == nullptr || sLooseOverrideScratch.basenameHashNext16 == nullptr)) ||
        (targetRepackCapacity > 0 &&
         (sLooseOverrideScratch.repackOffsets == nullptr || sLooseOverrideScratch.repackSizes == nullptr ||
          sLooseOverrideScratch.repackOriginalSizes == nullptr || sLooseOverrideScratch.repackOrder == nullptr))) {
        FreeLooseOverrideScratch(sLooseOverrideScratch);
        return false;
    }

    sLooseOverrideScratch.nodeOverrideCapacity = targetNodeCapacity;
    sLooseOverrideScratch.entryAppliedCapacity = targetEntryCapacity;
    sLooseOverrideScratch.basenameHashCapacity = targetBasenameHashCapacity;
    sLooseOverrideScratch.useWideBasenameIndices = useWideBasenameIndices;
    sLooseOverrideScratch.repackCapacity = targetRepackCapacity;
    sLooseOverrideScratch.heap = heap;
    return true;
}

static void BuildArchiveBasenameLookup16(const U8Node* nodes, u32 nodeCount, char* stringTable, u16* bucketHeads,
                                         u32 bucketCount, u16* nextNode) {
    if (nodes == nullptr || stringTable == nullptr || bucketHeads == nullptr || nextNode == nullptr || bucketCount == 0) {
        return;
    }

    memset(bucketHeads, 0xFF, sizeof(u16) * bucketCount);
    memset(nextNode, 0xFF, sizeof(u16) * nodeCount);

    for (u32 nodeIdx = 1; nodeIdx < nodeCount; ++nodeIdx) {
        if (NodeIsDir(nodes[nodeIdx])) continue;
        const char* nodeName = stringTable + NodeNameOffset(nodes[nodeIdx]);
        if (nodeName == nullptr || nodeName[0] == '\0') continue;

        const u32 bucket = HashArchiveMemberBasename(nodeName) & (bucketCount - 1);
        nextNode[nodeIdx] = bucketHeads[bucket];
        bucketHeads[bucket] = static_cast<u16>(nodeIdx);
    }
}

static void BuildArchiveBasenameLookup32(const U8Node* nodes, u32 nodeCount, char* stringTable, s32* bucketHeads,
                                         u32 bucketCount, s32* nextNode) {
    if (nodes == nullptr || stringTable == nullptr || bucketHeads == nullptr || nextNode == nullptr || bucketCount == 0) {
        return;
    }

    memset(bucketHeads, 0xFF, sizeof(s32) * bucketCount);
    memset(nextNode, 0xFF, sizeof(s32) * nodeCount);

    for (u32 nodeIdx = 1; nodeIdx < nodeCount; ++nodeIdx) {
        if (NodeIsDir(nodes[nodeIdx])) continue;
        const char* nodeName = stringTable + NodeNameOffset(nodes[nodeIdx]);
        if (nodeName == nullptr || nodeName[0] == '\0') continue;

        const u32 bucket = HashArchiveMemberBasename(nodeName) & (bucketCount - 1);
        nextNode[nodeIdx] = bucketHeads[bucket];
        bucketHeads[bucket] = static_cast<s32>(nodeIdx);
    }
}

static u32 MatchArchiveBasenameOverride16(const U8Node* nodes, char* stringTable, const u16* bucketHeads,
                                          const u16* nextNode, u32 bucketCount, const char* basename, u16 entryIndex,
                                          u16* nodeOverrideIndex) {
    if (nodes == nullptr || stringTable == nullptr || bucketHeads == nullptr || nextNode == nullptr || bucketCount == 0 ||
        basename == nullptr || basename[0] == '\0' || nodeOverrideIndex == nullptr) {
        return 0;
    }

    const u32 bucket = HashArchiveMemberBasename(basename) & (bucketCount - 1);
    u32 matchCount = 0;
    for (u16 nodeIdx = bucketHeads[bucket]; nodeIdx != kInvalidScratchIndex16; nodeIdx = nextNode[nodeIdx]) {
        const char* nodeName = stringTable + NodeNameOffset(nodes[nodeIdx]);
        if (strcmp(nodeName, basename) != 0) continue;

        // Matching nodes stay fan-out capable: a single basename override still patches every sibling file node.
        nodeOverrideIndex[nodeIdx] = entryIndex;
        ++matchCount;
    }
    return matchCount;
}

static u32 MatchArchiveBasenameOverride32(const U8Node* nodes, char* stringTable, const s32* bucketHeads,
                                          const s32* nextNode, u32 bucketCount, const char* basename, u16 entryIndex,
                                          u16* nodeOverrideIndex) {
    if (nodes == nullptr || stringTable == nullptr || bucketHeads == nullptr || nextNode == nullptr || bucketCount == 0 ||
        basename == nullptr || basename[0] == '\0' || nodeOverrideIndex == nullptr) {
        return 0;
    }

    const u32 bucket = HashArchiveMemberBasename(basename) & (bucketCount - 1);
    u32 matchCount = 0;
    for (s32 nodeIdx = bucketHeads[bucket]; nodeIdx >= 0; nodeIdx = nextNode[nodeIdx]) {
        const char* nodeName = stringTable + NodeNameOffset(nodes[nodeIdx]);
        if (strcmp(nodeName, basename) != 0) continue;

        nodeOverrideIndex[nodeIdx] = entryIndex;
        ++matchCount;
    }
    return matchCount;
}

static void BuildArchiveFileSlotCapacities(const U8Node* nodes, u32 nodeCount, u32 archiveSize, u32* fileOrder,
                                           u32* slotCapacities) {
    if (nodes == nullptr || fileOrder == nullptr || slotCapacities == nullptr) return;

    memset(slotCapacities, 0, sizeof(u32) * nodeCount);
    u32 fileCount = 0;
    for (u32 nodeIdx = 1; nodeIdx < nodeCount; ++nodeIdx) {
        if (NodeIsDir(nodes[nodeIdx])) continue;
        fileOrder[fileCount++] = nodeIdx;
    }

    // U8 node order is not guaranteed to follow file payload order, so compute
    // in-place growth limits from a temporary dataOffset-sorted view.
    for (u32 i = 1; i < fileCount; ++i) {
        const u32 keyNode = fileOrder[i];
        const u32 keyOffset = nodes[keyNode].dataOffset;
        u32 insertIdx = i;
        while (insertIdx > 0) {
            const u32 prevNode = fileOrder[insertIdx - 1];
            if (nodes[prevNode].dataOffset <= keyOffset) break;
            fileOrder[insertIdx] = prevNode;
            --insertIdx;
        }
        fileOrder[insertIdx] = keyNode;
    }

    for (u32 i = 0; i < fileCount; ++i) {
        const u32 nodeIdx = fileOrder[i];
        const u32 currentOffset = nodes[nodeIdx].dataOffset;
        if (currentOffset >= archiveSize) continue;

        u32 slotEnd = archiveSize;
        if (i + 1 < fileCount) {
            const u32 nextOffset = nodes[fileOrder[i + 1]].dataOffset;
            if (nextOffset < currentOffset) continue;
            slotEnd = (nextOffset < archiveSize) ? nextOffset : archiveSize;
        }
        slotCapacities[nodeIdx] = slotEnd - currentOffset;
    }
}

static void ResetModsRootCache() {
    sModsRootChecked = false;
    sModsRootPresent = false;
    SetModsRootPath(kModsRoot);
}

static void InvalidateOverrideIndices() {
    FreeOverrideDatabase(sOverrideDatabase);
    sOverrideIndicesAttempted = false;
    sHasWholeFileOverrides = false;
    ResetModsRootCache();
}

static void GetCurrentModFolder(char* outPath, u32 outSize) {
    if (outPath == nullptr || outSize == 0) return;
    outPath[0] = '\0';

    const System* system = System::sInstance;
    if (system == nullptr) return;

    const char* modFolder = system->GetModFolder();
    if (modFolder == nullptr || modFolder[0] == '\0') return;
    CopyPath(outPath, outSize, modFolder);
}

static void RefreshOverrideCacheState() {
    char modFolder[OVERRIDE_MAX_PATH];
    GetCurrentModFolder(modFolder, sizeof(modFolder));
    const bool looseOverridesEnabled = AreLooseArchiveOverridesEnabled();

    if (!sOverrideCacheStateInitialized) {
        CopyPath(sCachedModFolder, sizeof(sCachedModFolder), modFolder);
        sCachedLooseOverridesEnabled = looseOverridesEnabled;
        sOverrideCacheStateInitialized = true;
        return;
    }

    if (sCachedLooseOverridesEnabled != looseOverridesEnabled || strcmp(sCachedModFolder, modFolder) != 0) {
        InvalidateOverrideIndices();
        CopyPath(sCachedModFolder, sizeof(sCachedModFolder), modFolder);
        sCachedLooseOverridesEnabled = looseOverridesEnabled;
    }
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
    // The index is persistent for the process lifetime, so prefer large root
    // heaps over transient/archive-specific heaps. Falling back to the system
    // heap is still better than rebuilding the index every archive load.
    HeapCandidate candidates[3];
    candidates[0].heap = RKSystem::mInstance.EGGRootMEM2;
    candidates[0].reclaimedBytes = 0;
    candidates[1].heap = RKSystem::mInstance.EGGRootMEM1;
    candidates[1].reclaimedBytes = 0;
    candidates[2].heap = GetOverridesHeap();
    candidates[2].reclaimedBytes = 0;
    return FindHeapWithSpace(candidates, 3, requiredSize);
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
    const u32 end = Align32(reinterpret_cast<u32>(addr) + size);
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

static bool ReadDVDFileRange(const char* path, void* dest, u32 size, u32 offset) {
    DVD::FileInfo info;
    if (!DVD::Open(path, &info)) return false;
    InvalidateRange(dest, size);
    const s32 read = DVD::ReadPrio(&info, dest, static_cast<s32>(size), static_cast<s32>(offset), 2);
    DVD::Close(&info);
    return read == static_cast<s32>(size);
}

static bool ReadOpenedDVDFileRange(DVD::FileInfo& info, void* dest, u32 size, u32 offset) {
    InvalidateRange(dest, size);
    const s32 read = DVD::ReadPrio(&info, dest, static_cast<s32>(size), static_cast<s32>(offset), 2);
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

static bool ModsRootExists() {
    if (sModsRootChecked) return sModsRootPresent;

    // Probe once; DVD uses `/patches`, SD/Dolphin can resolve to the mod folder.
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

static bool ReadOverrideFile(const TaggedOverrideEntry& entry, void* dest) {
    if (!ModsRootExists()) return false;
    char fullPath[OVERRIDE_MAX_PATH];
    if (!BuildStoredOverridePath(entry.sourcePathOffset, fullPath, sizeof(fullPath))) return false;
    return ReadDVDFile(fullPath, dest, entry.size);
}

static bool FillTaggedOverrideEntry(OverrideDatabase& database, TaggedOverrideEntry& entry, const char* relativePath,
                                    u32 size) {
    u32 sourcePathOffset = 0;
    if (!AddRelativePathToPool(database, relativePath, sourcePathOffset)) return false;

    char decodedPath[OVERRIDE_MAX_PATH];
    char strippedName[OVERRIDE_MAX_PATH];
    char archiveTagLower[OVERRIDE_MAX_NAME];
    bool isDelete = false;
    if (!DecodeOverrideRelativePath(decodedPath, sizeof(decodedPath), relativePath) ||
        !ExtractTaggedOverrideMetadata(decodedPath, strippedName, sizeof(strippedName), archiveTagLower,
                                       sizeof(archiveTagLower), isDelete)) {
        return false;
    }

    u16 tagId = 0;
    if (!GetTagIdForName(database, archiveTagLower, tagId)) return false;

    entry.sourcePathOffset = sourcePathOffset;
    entry.size = size;
    entry.tagId = tagId;
    entry.flags = (strchr(strippedName, '/') != nullptr) ? OVERRIDEENTRYFLAG_HAS_SUBPATH : OVERRIDEENTRYFLAG_NONE;
    if (isDelete) entry.flags |= OVERRIDEENTRYFLAG_IS_DELETE;
    return true;
}

static bool FillWholeFileOverrideEntry(OverrideDatabase& database, WholeFileOverrideEntry& entry,
                                       const char* relativePath) {
    const char* basename = FindBasename(relativePath);
    if (basename == nullptr || basename[0] == '\0') return false;

    u32 sourcePathOffset = 0;
    if (!AddRelativePathToPool(database, relativePath, sourcePathOffset)) return false;

    entry.sourcePathOffset = sourcePathOffset;
    entry.basenameHash = HashLowerCaseString(basename);
    return true;
}

struct BRSAROverrideLayout {
    u32 fileSize;
    u32 waveOffset;
    u32 waveSize;
};

static bool FillBRSAROverrideEntry(OverrideDatabase& database, BRSAROverrideSlot& entry, const char* relativePath,
                                   u8 type, u32 size) {
    u32 sourcePathOffset = 0;
    if (!AddRelativePathToPool(database, relativePath, sourcePathOffset)) return false;

    entry.sourcePathOffset = sourcePathOffset;
    entry.type = type;
    entry.size = size;
    entry.fileDataSize = 0;
    entry.waveDataOffset = 0;
    entry.waveDataSize = 0;
    entry.layoutState = 0;
    entry.reserved[0] = 0;
    entry.reserved[1] = 0;
    return true;
}

static bool CanAddEntry(u32 maxCount, u32& count, bool& truncated) {
    if (count >= maxCount) {
        truncated = true;
        return false;
    }
    return true;
}

struct ParsedScannedOverride {
    bool isTagged;
    bool isWholeFile;
    bool isBRSAR;
    u32 brsarFileId;
    u8 brsarType;
    char strippedName[OVERRIDE_MAX_PATH];
    char archiveTagLower[OVERRIDE_MAX_NAME];
    bool isDelete;
    const char* basename;
};

static bool ParseScannedOverride(const char* relativePath, ParsedScannedOverride& out) {
    memset(&out, 0, sizeof(out));
    if (relativePath == nullptr) return false;
    if (strlen(relativePath) >= OVERRIDE_MAX_PATH) return false;

    out.basename = FindBasename(relativePath);
    if (out.basename == nullptr || out.basename[0] == '\0') return false;

    if (TryParseBRSAROverride(relativePath, out.brsarFileId, out.brsarType)) {
        if (out.brsarFileId >= kBRSAROverrideSlotCount) return false;
        out.isBRSAR = true;
        return true;
    }

    out.isTagged = TryParseArchiveTag(relativePath, out.strippedName, sizeof(out.strippedName), out.archiveTagLower,
                                      sizeof(out.archiveTagLower));
    if (out.isTagged) {
        out.isDelete = StripDeleteSuffixInPlace(out.strippedName);
        if (out.strippedName[0] == '\0') return false;
        // Reject loose raw-file overrides for these resource types, even if they target an archive member.
        if (IsBlockedLooseRawOverrideExtension(out.strippedName)) return false;
        return true;
    }

    if (IsBlockedLooseRawOverrideExtension(out.basename)) return false;

    out.isWholeFile = true;
    return true;
}

static void AddParsedTaggedEntry(ScanBuildState& state, u32 maxCount, const char* relativePath, u32 size,
                                 const ParsedScannedOverride& parsed) {
    if (!CanAddEntry(maxCount, state.taggedCount, state.taggedTruncated)) return;
    if (state.taggedEntries != nullptr) {
        if (!FillTaggedOverrideEntry(*state.database, state.taggedEntries[state.taggedCount], relativePath, size)) {
            state.taggedTruncated = true;
            return;
        }
    }
    state.stringBytes += static_cast<u32>(strlen(relativePath)) + 1;
    state.tagStringBytes += static_cast<u32>(strlen(parsed.archiveTagLower)) + 1;
    ++state.taggedCount;
}

static s32 CompareBRSAROverrideCandidates(u8 lhsType, const char* lhsPath, u8 rhsType, const char* rhsPath) {
    if (lhsType < rhsType) return -1;
    if (lhsType > rhsType) return 1;
    if (lhsPath == nullptr || rhsPath == nullptr) return 0;
    return strcmp(lhsPath, rhsPath);
}

static void AddParsedWholeFileEntry(ScanBuildState& state, u32 maxCount, const char* relativePath) {
    if (!CanAddEntry(maxCount, state.wholeFileCount, state.wholeFileTruncated)) return;

    if (state.wholeFileEntries != nullptr) {
        if (!FillWholeFileOverrideEntry(*state.database, state.wholeFileEntries[state.wholeFileCount], relativePath)) {
            state.wholeFileTruncated = true;
            return;
        }
    }
    state.stringBytes += static_cast<u32>(strlen(relativePath)) + 1;
    ++state.wholeFileCount;
}

static void AddParsedBRSAROverrideEntry(ScanBuildState& state, u32 maxCount, const char* relativePath, u32 size,
                                        const ParsedScannedOverride& parsed) {
    const u32 fileId = parsed.brsarFileId;
    const u8 type = parsed.brsarType;
    bool isNewSlot = false;
    if (state.brsarSlots == nullptr) {
        if (state.brsarSlotOccupied == nullptr) return;
        if (state.brsarSlotOccupied[fileId] == 0) {
            if (!CanAddEntry(maxCount, state.brsarCount, state.brsarTruncated)) return;
            state.brsarSlotOccupied[fileId] = 1;
            ++state.brsarCount;
        }
    } else {
        BRSAROverrideSlot& slot = state.brsarSlots[fileId];
        isNewSlot = slot.sourcePathOffset == kInvalidPoolOffset;
        if (isNewSlot) {
            if (!CanAddEntry(maxCount, state.brsarCount, state.brsarTruncated)) return;
        } else {
            const char* existingPath = GetRelativePath(slot.sourcePathOffset);
            if (CompareBRSAROverrideCandidates(type, relativePath, slot.type, existingPath) >= 0) {
                state.stringBytes += static_cast<u32>(strlen(relativePath)) + 1;
                return;
            }
        }

        if (!FillBRSAROverrideEntry(*state.database, slot, relativePath, type, size)) {
            state.brsarTruncated = true;
            return;
        }
        if (isNewSlot) ++state.brsarCount;
    }

    state.stringBytes += static_cast<u32>(strlen(relativePath)) + 1;
}

static void AddScannedEntry(ScanBuildState& state, u32 maxTaggedCount, u32 maxWholeFileCount, u32 maxBRSARCount,
                            const char* relativePath, u32 size) {
    ParsedScannedOverride parsed;
    if (!ParseScannedOverride(relativePath, parsed)) return;

    if (parsed.isBRSAR) {
        AddParsedBRSAROverrideEntry(state, maxBRSARCount, relativePath, size, parsed);
    } else if (parsed.isTagged) {
        AddParsedTaggedEntry(state, maxTaggedCount, relativePath, size, parsed);
    } else if (parsed.isWholeFile) {
        AddParsedWholeFileEntry(state, maxWholeFileCount, relativePath);
    }
}

static bool IsScanBuildComplete(const ScanBuildState& state, u32 maxTaggedCount, u32 maxWholeFileCount,
                                u32 maxBRSARCount) {
    return state.taggedCount >= maxTaggedCount && state.wholeFileCount >= maxWholeFileCount &&
           state.brsarCount >= maxBRSARCount;
}

static bool FindModsDirInFST(u32& outIndex, u32& outEnd) {
    if (OS::BootInfo::mInstance.FSTLocation == nullptr) return false;

    const FSTEntry* entries = static_cast<const FSTEntry*>(OS::BootInfo::mInstance.FSTLocation);
    const u32 entryCount = entries[0].size;
    if (entryCount == 0) return false;
    return ResolveFSTDirByPath(kModsRoot, entryCount, outIndex, outEnd);
}

static void ScanModsDirDVD(ScanBuildState& state, u32 maxTaggedCount, u32 maxWholeFileCount, u32 maxBRSARCount) {
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


    // Walk the `/patches` FST subtree with a fixed stack and path buffer.
    for (u32 i = modsIndex + 1; i < modsEnd &&
                    !IsScanBuildComplete(state, maxTaggedCount, maxWholeFileCount, maxBRSARCount);
         ++i) {
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

        AddScannedEntry(state, maxTaggedCount, maxWholeFileCount, maxBRSARCount, relativePath, entry.size);
    }
}

static void ScanModsDirFromIO(IO& io, ScanBuildState& state, u32 maxTaggedCount, u32 maxWholeFileCount,
                              u32 maxBRSARCount) {
    char modsPath[OVERRIDE_MAX_PATH];
    if (!GetSDModsRootPath(modsPath, sizeof(modsPath))) return;
    if (!io.FolderExists(modsPath)) return;

    io.ReadFolder(modsPath);
    const u32 fileCount = io.GetFileCount();
    for (u32 i = 0; i < fileCount &&
                    !IsScanBuildComplete(state, maxTaggedCount, maxWholeFileCount, maxBRSARCount);
         ++i) {
        const char* fileName = io.GetFileName(i);
        if (fileName == nullptr || fileName[0] == '\0') continue;
        // SD scanning is flat; bracket prefixes encode archive subpaths.
        if (strlen(fileName) >= OVERRIDE_MAX_PATH) continue;

        char sdPath[OVERRIDE_MAX_PATH];
        io.GetFolderFilePath(sdPath, i);
        if (!io.OpenFile(sdPath, FILE_MODE_READ)) continue;

        const s32 fileSize = io.GetFileSize();
        io.Close();
        // Negative sizes indicate an IO failure, not a valid zero-length override.
        if (fileSize < 0) continue;

        AddScannedEntry(state, maxTaggedCount, maxWholeFileCount, maxBRSARCount, fileName, static_cast<u32>(fileSize));
    }
    io.CloseFolder();
}

static void ScanModsDirSD(ScanBuildState& state, u32 maxTaggedCount, u32 maxWholeFileCount, u32 maxBRSARCount) {
    IO* io = IO::sInstance;
    if (io == nullptr) return;
    if (!ShouldProbeSDModsPath()) return;

    if (io->type == IOType_SD) {
        ScanModsDirFromIO(*io, state, maxTaggedCount, maxWholeFileCount, maxBRSARCount);
        return;
    }

    System* system = System::sInstance;
    if (system == nullptr) return;

    SDIO sdIo(IOType_SD, system->heap, system->taskThread);
    ScanModsDirFromIO(sdIo, state, maxTaggedCount, maxWholeFileCount, maxBRSARCount);
}

static void ScanModsDir(ScanBuildState& state, u32 maxTaggedCount, u32 maxWholeFileCount, u32 maxBRSARCount) {
    if (!ModsRootExists()) return;

    IO* io = IO::sInstance;
    if (io != nullptr && ShouldProbeSDModsPath()) {
        // Prefer SD when available so loose files can change without rebuilding the disc image.
        ScanModsDirSD(state, maxTaggedCount, maxWholeFileCount, maxBRSARCount);
        return;
    }

    // Otherwise walk the baked-in `/patches` subtree from the DVD FST.
    ScanModsDirDVD(state, maxTaggedCount, maxWholeFileCount, maxBRSARCount);
}

static s32 CompareWholeFileEntries(const WholeFileOverrideEntry& lhs, const WholeFileOverrideEntry& rhs) {
    if (lhs.basenameHash < rhs.basenameHash) return -1;
    if (lhs.basenameHash > rhs.basenameHash) return 1;

    char lhsBasename[OVERRIDE_MAX_PATH];
    char rhsBasename[OVERRIDE_MAX_PATH];
    const bool lhsValid = GetWholeFileEntryBasenameLower(lhs, lhsBasename, sizeof(lhsBasename));
    const bool rhsValid = GetWholeFileEntryBasenameLower(rhs, rhsBasename, sizeof(rhsBasename));
    if (!lhsValid && !rhsValid) return 0;
    if (!lhsValid) return -1;
    if (!rhsValid) return 1;

    const s32 compare = CompareWholeFileBasenames(lhsBasename, rhsBasename);
    if (compare != 0) return compare;

    const char* lhsPath = GetRelativePath(lhs.sourcePathOffset);
    const char* rhsPath = GetRelativePath(rhs.sourcePathOffset);
    if (lhsPath == nullptr || rhsPath == nullptr) return 0;

    const size_t lhsLen = strlen(lhsPath);
    const size_t rhsLen = strlen(rhsPath);
    if (lhsLen < rhsLen) return -1;
    if (lhsLen > rhsLen) return 1;
    return strcmp(lhsPath, rhsPath);
}

static int CompareWholeFileEntriesForQSort(const void* lhs, const void* rhs) {
    return CompareWholeFileEntries(*static_cast<const WholeFileOverrideEntry*>(lhs),
                                   *static_cast<const WholeFileOverrideEntry*>(rhs));
}

static void SortWholeFileOverrideEntries(WholeFileOverrideEntry* entries, u32 count) {
    if (entries == nullptr || count < 2) return;

    qsort(entries, count, sizeof(WholeFileOverrideEntry), CompareWholeFileEntriesForQSort);
}

static const WholeFileOverrideEntry* FindWholeFileOverride(const OverrideDatabase& database, const char* basenameLower) {
    if (database.wholeFileEntries == nullptr || database.wholeFileCount == 0 || basenameLower == nullptr ||
        basenameLower[0] == '\0') {
        return nullptr;
    }

    const u32 basenameHash = HashLowerCaseString(basenameLower);
    u32 low = 0;
    u32 high = database.wholeFileCount;
    while (low < high) {
        const u32 mid = low + ((high - low) / 2);
        if (database.wholeFileEntries[mid].basenameHash < basenameHash) {
            low = mid + 1;
        } else {
            high = mid;
        }
    }

    while (low < database.wholeFileCount && database.wholeFileEntries[low].basenameHash == basenameHash) {
        char entryBasename[OVERRIDE_MAX_PATH];
        if (GetWholeFileEntryBasenameLower(database.wholeFileEntries[low], entryBasename, sizeof(entryBasename)) &&
            strcmp(entryBasename, basenameLower) == 0) {
            return &database.wholeFileEntries[low];
        }
        ++low;
    }
    return nullptr;
}

static BRSAROverrideSlot* FindBRSAROverrideSlot(u32 fileId) {
    if (sOverrideDatabase.brsarSlots == nullptr || sOverrideDatabase.brsarCount == 0 || fileId >= sOverrideDatabase.brsarSlotCount) {
        return nullptr;
    }

    BRSAROverrideSlot& slot = sOverrideDatabase.brsarSlots[fileId];
    if (slot.sourcePathOffset == kInvalidPoolOffset) return nullptr;
    return &slot;
}

static bool BRSAROverrideMagicMatches(u8 type, const u8* header) {
    if (header == nullptr) return false;
    if (type == BRSAROVERRIDE_BRWSD) {
        return memcmp(header, "RWSD", 4) == 0;
    }
    if (type == BRSAROVERRIDE_BRBNK) {
        return memcmp(header, "RBNK", 4) == 0;
    }
    return false;
}

static bool ShouldLogBRSARLayoutFailure(u32 fileId) {
    if (fileId >= kBRSAROverrideSlotCount || sLoggedBRSARLayoutFailure[fileId] != 0) return false;
    sLoggedBRSARLayoutFailure[fileId] = 1;
    return true;
}

static bool FindEmbeddedRWAROffset(DVD::FileInfo& info, const BRSAROverrideSlot& entry, u32 searchStart, u32& outOffset,
                                   u32& outSize) {
    outOffset = 0;
    outSize = 0;
    if (searchStart >= entry.size) return false;

    u8 exactHeader[0x20] __attribute__((aligned(32)));
    if (searchStart + sizeof(exactHeader) <= entry.size &&
        ReadOpenedDVDFileRange(info, exactHeader, sizeof(exactHeader), searchStart) &&
        memcmp(exactHeader, "RWAR", 4) == 0) {
        const u32 exactSize = ReadBE32(exactHeader + 8);
        if (exactSize >= 0x20 && searchStart + exactSize <= entry.size) {
            outOffset = searchStart;
            outSize = exactSize;
            return true;
        }
    }

    enum { kChunkSize = 0x800 };
    u8 chunk[kChunkSize] __attribute__((aligned(32)));
    u32 offset = Align32(searchStart + 0x20);
    while (offset + 0x20 <= entry.size) {
        u32 remaining = entry.size - offset;
        u32 readSize = remaining >= kChunkSize ? kChunkSize : remaining;
        readSize &= ~0x1F;
        if (readSize < 0x20) break;

        if (!ReadOpenedDVDFileRange(info, chunk, readSize, offset)) {
            offset += readSize;
            continue;
        }

        for (u32 chunkOffset = 0; chunkOffset + 0x20 <= readSize; chunkOffset += 0x20) {
            if (memcmp(chunk + chunkOffset, "RWAR", 4) != 0) continue;

            const u32 candidateSize = ReadBE32(chunk + chunkOffset + 8);
            const u32 candidateOffset = offset + chunkOffset;
            if (candidateSize >= 0x20 && candidateOffset + candidateSize <= entry.size) {
                outOffset = candidateOffset;
                outSize = candidateSize;
                return true;
            }
        }

        offset += readSize;
    }

    return false;
}

static bool TryGetBRSAROverrideLayout(u32 fileId, BRSAROverrideSlot& entry, BRSAROverrideLayout& outLayout) {
    outLayout.fileSize = 0;
    outLayout.waveOffset = 0;
    outLayout.waveSize = 0;

    if (entry.layoutState == 1) {
        outLayout.fileSize = entry.fileDataSize;
        outLayout.waveOffset = entry.waveDataOffset;
        outLayout.waveSize = entry.waveDataSize;
        return true;
    }
    if (entry.layoutState == 2) return false;

    const char* relativePath = GetRelativePath(entry.sourcePathOffset);
    char fullPath[OVERRIDE_MAX_PATH];
    if (relativePath == nullptr || !BuildStoredOverridePath(entry.sourcePathOffset, fullPath, sizeof(fullPath)) ||
        entry.size < 0x20) {
        if (ShouldLogBRSARLayoutFailure(fileId)) {
            OS::Report("[Pulsar] Loose BRSAR layout failed: fileId=%u path='%s' invalid file metadata size=0x%X\n",
                       fileId, relativePath != nullptr ? relativePath : "<missing>", entry.size);
        }
        return false;
    }

    DVD::FileInfo info;
    if (!DVD::Open(fullPath, &info)) {
        if (ShouldLogBRSARLayoutFailure(fileId)) {
            OS::Report("[Pulsar] Loose BRSAR layout failed: fileId=%u path='%s' header read failed\n", fileId,
                       relativePath);
        }
        return false;
    }

    u8 header[0x20] __attribute__((aligned(32)));
    const bool headerReadOk = ReadOpenedDVDFileRange(info, header, sizeof(header), 0);
    if (!headerReadOk) {
        DVD::Close(&info);
        if (ShouldLogBRSARLayoutFailure(fileId)) {
            OS::Report("[Pulsar] Loose BRSAR layout failed: fileId=%u path='%s' header read failed\n", fileId,
                       relativePath);
        }
        return false;
    }
    if (!BRSAROverrideMagicMatches(entry.type, header)) {
        DVD::Close(&info);
        if (ShouldLogBRSARLayoutFailure(fileId)) {
            OS::Report("[Pulsar] Loose BRSAR layout failed: fileId=%u path='%s' magic/type mismatch\n", fileId,
                       relativePath);
        }
        return false;
    }

    const u32 fileSize = ReadBE32(header + 8);
    if (fileSize < 0x20 || fileSize > entry.size) {
        DVD::Close(&info);
        entry.layoutState = 2;
        if (ShouldLogBRSARLayoutFailure(fileId)) {
            OS::Report("[Pulsar] Loose BRSAR layout failed: fileId=%u path='%s' invalid main size=0x%X entry=0x%X\n",
                       fileId, relativePath, fileSize, entry.size);
        }
        return false;
    }

    outLayout.fileSize = fileSize;

    const u32 searchStart = Align32(fileSize);
    u32 waveOffset = 0;
    u32 waveSize = 0;
    if (FindEmbeddedRWAROffset(info, entry, searchStart, waveOffset, waveSize)) {
        outLayout.waveOffset = waveOffset;
        outLayout.waveSize = waveSize;
    }

    DVD::Close(&info);

    entry.fileDataSize = outLayout.fileSize;
    entry.waveDataOffset = outLayout.waveOffset;
    entry.waveDataSize = outLayout.waveSize;
    entry.layoutState = 1;
    return true;
}

static u32 GetOverrideDatabaseFootprint(u32 taggedCount, u32 wholeFileCount, u32 brsarCount, u32 tagCapacity,
                                        u32 stringBytes) {
    u32 size = 0;
    size += Align32(sizeof(TaggedOverrideEntry) * taggedCount);
    size += Align32(sizeof(WholeFileOverrideEntry) * wholeFileCount);
    if (brsarCount > 0) {
        size += Align32(sizeof(BRSAROverrideSlot) * kBRSAROverrideSlotCount);
    }
    size += Align32(sizeof(OverrideTagEntry) * tagCapacity);
    size += Align32(stringBytes);
    return size;
}

static void InitializeOverrideDatabaseViews(OverrideDatabase& database, void* block, u32 blockSize, EGG::Heap* heap,
                                            u32 taggedCount, u32 wholeFileCount, u32 brsarCount, u32 tagCapacity,
                                            u32 stringBytes) {
    database.block = block;
    database.blockSize = blockSize;
    database.heap = heap;
    database.stringPool = nullptr;
    database.stringPoolSize = stringBytes;
    database.stringPoolUsed = 0;
    database.tags = nullptr;
    database.tagCount = 0;
    database.tagCapacity = tagCapacity;
    database.taggedEntries = nullptr;
    database.taggedCount = taggedCount;
    database.wholeFileEntries = nullptr;
    database.wholeFileCount = wholeFileCount;
    database.brsarSlots = nullptr;
    database.brsarSlotCount = 0;
    database.brsarCount = brsarCount;

    char* cursor = static_cast<char*>(block);
    if (taggedCount > 0) {
        database.taggedEntries = reinterpret_cast<TaggedOverrideEntry*>(cursor);
        cursor += Align32(sizeof(TaggedOverrideEntry) * taggedCount);
    }
    if (wholeFileCount > 0) {
        database.wholeFileEntries = reinterpret_cast<WholeFileOverrideEntry*>(cursor);
        cursor += Align32(sizeof(WholeFileOverrideEntry) * wholeFileCount);
    }
    if (brsarCount > 0) {
        database.brsarSlots = reinterpret_cast<BRSAROverrideSlot*>(cursor);
        database.brsarSlotCount = kBRSAROverrideSlotCount;
        cursor += Align32(sizeof(BRSAROverrideSlot) * kBRSAROverrideSlotCount);
    }
    if (tagCapacity > 0) {
        database.tags = reinterpret_cast<OverrideTagEntry*>(cursor);
        cursor += Align32(sizeof(OverrideTagEntry) * tagCapacity);
    }
    if (stringBytes > 0) {
        database.stringPool = cursor;
    }

    if (database.brsarSlots != nullptr) {
        memset(database.brsarSlots, 0, sizeof(BRSAROverrideSlot) * database.brsarSlotCount);
        for (u32 i = 0; i < database.brsarSlotCount; ++i) {
            database.brsarSlots[i].sourcePathOffset = kInvalidPoolOffset;
        }
    }
}

static void EnsureOverrideIndicesBuilt() {
    if (sOverrideIndicesAttempted) return;

    if (!ModsRootExists()) {
        FreeOverrideDatabase(sOverrideDatabase);
        sHasWholeFileOverrides = false;
        sOverrideIndicesAttempted = true;
        return;
    }

    sOverrideIndicesAttempted = true;


    // Count first, then allocate tightly and fill the persistent index.

    u8 brsarSlotOccupied[kBRSAROverrideSlotCount];
    memset(brsarSlotOccupied, 0, sizeof(brsarSlotOccupied));
    ScanBuildState countState = {nullptr, nullptr, 0, false, nullptr, 0, false, nullptr, 0, false, 0, 0,
                                 brsarSlotOccupied};
    ScanModsDir(countState, kMaxOverridesTotal, kMaxOverridesTotal, kMaxOverridesTotal);

    if (countState.taggedCount >= kMaxOverridesTotal) {
        countState.taggedTruncated = true;
    }
    if (countState.wholeFileCount >= kMaxOverridesTotal) {
        countState.wholeFileTruncated = true;
    }
    if (countState.brsarCount >= kMaxOverridesTotal) {
        countState.brsarTruncated = true;
    }
    if (countState.taggedTruncated) {
        OS::Report("[Pulsar] Loose tagged overrides truncated at %u entries (max %u)\n",
                   countState.taggedCount, kMaxOverridesTotal);
    }
    if (countState.wholeFileTruncated) {
        OS::Report("[Pulsar] Loose whole-file overrides truncated at %u entries (max %u)\n",
                   countState.wholeFileCount, kMaxOverridesTotal);
    }
    if (countState.brsarTruncated) {
        OS::Report("[Pulsar] Loose BRSAR overrides truncated at %u entries (max %u)\n",
                   countState.brsarCount, kMaxOverridesTotal);
    }

    if (countState.taggedCount == 0 && countState.wholeFileCount == 0 && countState.brsarCount == 0) {
        FreeOverrideDatabase(sOverrideDatabase);
        sHasWholeFileOverrides = false;
        return;
    }

    const u32 tagCapacity = countState.taggedCount;
    const u32 requiredSize = GetOverrideDatabaseFootprint(countState.taggedCount, countState.wholeFileCount,
                                                          countState.brsarCount, tagCapacity,
                                                          countState.stringBytes + countState.tagStringBytes);

    EGG::Heap* databaseHeap = GetPersistentOverrideHeap(requiredSize);
    if (databaseHeap == nullptr) {
        OS::Report("[Pulsar] Loose override database skipped: need 0x%X bytes, no persistent heap available\n",
                   requiredSize);
        FreeOverrideDatabase(sOverrideDatabase);
        sHasWholeFileOverrides = false;
        return;
    }

    void* databaseBlock = EGG::Heap::alloc(requiredSize, 0x20, databaseHeap);
    if (databaseBlock == nullptr) {
        OS::Report("[Pulsar] Loose override database allocation failed: size=0x%X\n", requiredSize);
        FreeOverrideDatabase(sOverrideDatabase);
        sHasWholeFileOverrides = false;
        return;
    }

    OverrideDatabase database = {};
    InitializeOverrideDatabaseViews(database, databaseBlock, requiredSize, databaseHeap, countState.taggedCount,
                                    countState.wholeFileCount, countState.brsarCount, tagCapacity,
                                    countState.stringBytes + countState.tagStringBytes);

    sActiveOverrideDatabase = &database;
    ScanBuildState fillState = {&database,
                                database.taggedEntries,
                                0,
                                false,
                                database.wholeFileEntries,
                                0,
                                false,
                                database.brsarSlots,
                                0,
                                false,
                                0,
                                0,
                                nullptr};
    ScanModsDir(fillState, database.taggedCount, database.wholeFileCount, database.brsarCount);
    database.taggedCount = fillState.taggedCount;
    database.wholeFileCount = fillState.wholeFileCount;
    database.brsarCount = fillState.brsarCount;

    SortOverrideEntriesByArchiveTag(database.taggedEntries, database.taggedCount);
    BuildTaggedOverrideRanges(database);
    SortWholeFileOverrideEntries(database.wholeFileEntries, database.wholeFileCount);

    FreeOverrideDatabase(sOverrideDatabase);
    sOverrideDatabase = database;
    sActiveOverrideDatabase = &sOverrideDatabase;
    sHasWholeFileOverrides = (database.wholeFileEntries != nullptr && database.wholeFileCount > 0);
}

static bool ResolveLooseBRSAROverride(u32 fileId, BRSAROverrideSlot*& outEntry, BRSAROverrideLayout& outLayout) {
    outEntry = nullptr;
    outLayout.fileSize = 0;
    outLayout.waveOffset = 0;
    outLayout.waveSize = 0;

    RefreshOverrideCacheState();
    if (!AreLooseArchiveOverridesEnabled()) return false;

    EnsureOverrideIndicesBuilt();
    outEntry = FindBRSAROverrideSlot(fileId);
    if (outEntry == nullptr) return false;
    return TryGetBRSAROverrideLayout(fileId, *outEntry, outLayout);
}

static bool IsFileExtensionSZS(const char* path) {
    return EndsWithIgnoreCase(path, ".szs");
}

// Redirect shared DVD path lookups so whole-file overrides also cover streams and non-SZS files.
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
    return Align32(dataStart);
}

static bool BuildStructuralAddedFiles(const PendingStructuralAddCandidate* candidates, u32 candidateCount,
                                      PendingStructuralAdd*& outAddedFiles, u32& outAddedFileCount,
                                      char*& outPathPool, u32& outPathPoolSize, u32& outAddedNameBytes,
                                      u32 oldStringTableSize, EGG::Heap* heap) {
    outAddedFiles = nullptr;
    outAddedFileCount = 0;
    outPathPool = nullptr;
    outPathPoolSize = 0;
    outAddedNameBytes = 0;
    if (candidateCount == 0) return true;
    if (heap == nullptr) return false;

    u32 uniqueCount = 0;
    u32 pathBytes = 0;
    u32 nameBytes = 0;
    char previousPath[OVERRIDE_MAX_PATH];
    previousPath[0] = '\0';
    u16 previousParent = kInvalidScratchIndex16;
    bool hasPrevious = false;

    for (u32 i = 0; i < candidateCount; ++i) {
        const PendingStructuralAddCandidate& candidate = candidates[i];
        char matchName[OVERRIDE_MAX_PATH];
        if (!GetTaggedEntryMatchName(sOverrideDatabase.taggedEntries[candidate.overrideIndex], matchName, sizeof(matchName)) ||
            matchName[0] == '\0') {
            continue;
        }

        const char* basename = FindBasename(matchName);
        if (basename == nullptr || basename[0] == '\0') continue;

        if (hasPrevious && previousParent == candidate.parentDirIndex && strcmp(previousPath, matchName) == 0) {
            continue;
        }

        pathBytes += static_cast<u32>(strlen(matchName)) + 1;
        nameBytes += static_cast<u32>(strlen(basename)) + 1;
        CopyPath(previousPath, sizeof(previousPath), matchName);
        previousParent = candidate.parentDirIndex;
        hasPrevious = true;
        ++uniqueCount;
    }

    if (uniqueCount == 0) return true;

    PendingStructuralAdd* addedFiles = EGG::Heap::alloc<PendingStructuralAdd>(sizeof(PendingStructuralAdd) * uniqueCount,
                                                                              0x20, heap);
    char* pathPool = EGG::Heap::alloc<char>(pathBytes, 0x20, heap);
    if (addedFiles == nullptr || pathPool == nullptr) {
        if (addedFiles != nullptr) EGG::Heap::free(addedFiles, heap);
        if (pathPool != nullptr) EGG::Heap::free(pathPool, heap);
        return false;
    }

    uniqueCount = 0;
    pathBytes = 0;
    nameBytes = 0;
    previousPath[0] = '\0';
    previousParent = kInvalidScratchIndex16;
    hasPrevious = false;

    for (u32 i = 0; i < candidateCount; ++i) {
        const PendingStructuralAddCandidate& candidate = candidates[i];
        char matchName[OVERRIDE_MAX_PATH];
        if (!GetTaggedEntryMatchName(sOverrideDatabase.taggedEntries[candidate.overrideIndex], matchName, sizeof(matchName)) ||
            matchName[0] == '\0') {
            continue;
        }

        const char* basename = FindBasename(matchName);
        if (basename == nullptr || basename[0] == '\0') continue;

        if (hasPrevious && previousParent == candidate.parentDirIndex && strcmp(previousPath, matchName) == 0) {
            addedFiles[uniqueCount - 1].overrideIndex = candidate.overrideIndex;
            continue;
        }

        const u32 fullLen = static_cast<u32>(strlen(matchName)) + 1;
        const u32 nameLen = static_cast<u32>(strlen(basename)) + 1;
        memcpy(pathPool + pathBytes, matchName, fullLen);

        addedFiles[uniqueCount].parentDirIndex = candidate.parentDirIndex;
        addedFiles[uniqueCount].overrideIndex = candidate.overrideIndex;
        addedFiles[uniqueCount].pathOffset = pathBytes;
        addedFiles[uniqueCount].nameOffset = oldStringTableSize + nameBytes;

        pathBytes += fullLen;
        nameBytes += nameLen;
        CopyPath(previousPath, sizeof(previousPath), matchName);
        previousParent = candidate.parentDirIndex;
        hasPrevious = true;
        ++uniqueCount;
    }

    outAddedFiles = addedFiles;
    outAddedFileCount = uniqueCount;
    outPathPool = pathPool;
    outPathPoolSize = pathBytes;
    outAddedNameBytes = nameBytes;
    return true;
}

struct StructuralRebuildContext {
    const U8Node* oldNodes;
    u32 oldNodeCount;
    const char* oldStringTable;
    const u16* nodeOverrideIndex;
    const u8* nodeDeleteFlags;
    const PendingStructuralAdd* addedFiles;
    u32 addedFileCount;
    const char* addedPathPool;
    u8* addedFileEmitted;
    const u8* dirKeepFlags;
    EGG::Heap* tempHeap;
    u32 childScratchCapacity;
    u8* oldArchiveBase;
    u8* newBuffer;
    U8Node* newNodes;
    char* newStringTable;
    u32 newStringTableSize;
    u32 nextStringOffset;
    u32 writeOffset;
    u32 nextNodeIndex;
    u32 patchedNodes;
    u32 rangeStart;
    u32* entryAppliedBits;
};

static bool AppendStructuralString(StructuralRebuildContext& context, const char* name, u32& outOffset) {
    if (name == nullptr) name = "";
    const u32 nameBytes = strlen(name) + 1;
    if (context.newStringTable == nullptr || context.nextStringOffset + nameBytes > context.newStringTableSize) {
        return false;
    }

    outOffset = context.nextStringOffset;
    memcpy(context.newStringTable + outOffset, name, nameBytes);
    context.nextStringOffset += nameBytes;
    return true;
}

static void ZeroStructuralFilePadding(u8* buffer, u32 offset, u32 size) {
    if (buffer == nullptr) return;
    const u32 paddedSize = Align32(size);
    if (paddedSize > size) {
        memset(buffer + offset + size, 0, paddedSize - size);
    }
}

static const char* GetStructuralAddedBasename(u32 addedIndex, const StructuralRebuildContext& context);

static bool EmitStructuralExistingFileNode(u32 oldNodeIdx, StructuralRebuildContext& context) {
    const U8Node& oldNode = context.oldNodes[oldNodeIdx];
    const u16 idx = context.nodeOverrideIndex[oldNodeIdx];

    const u32 newNodeIdx = context.nextNodeIndex++;
    U8Node& newNode = context.newNodes[newNodeIdx];
    u32 nameOffset = 0;
    if (!AppendStructuralString(context, context.oldStringTable + NodeNameOffset(oldNode), nameOffset)) {
        return false;
    }
    newNode.typeName = nameOffset;

    const u32 writeOffset = Align32(context.writeOffset);
    newNode.dataOffset = writeOffset;
    newNode.dataSize = (idx != kInvalidScratchIndex16) ? sOverrideDatabase.taggedEntries[idx].size : oldNode.dataSize;

    if (idx != kInvalidScratchIndex16) {
        const TaggedOverrideEntry& entry = sOverrideDatabase.taggedEntries[idx];
        if (!ReadOverrideFile(entry, context.newBuffer + writeOffset)) {
            return false;
        }
        MarkEntryApplied(context.entryAppliedBits, idx - context.rangeStart);
        ++context.patchedNodes;
    } else {
        memcpy(context.newBuffer + writeOffset, context.oldArchiveBase + oldNode.dataOffset, oldNode.dataSize);
    }

    const u32 paddedSize = Align32(newNode.dataSize);
    ZeroStructuralFilePadding(context.newBuffer, writeOffset, newNode.dataSize);
    context.writeOffset = writeOffset + paddedSize;
    return true;
}

static bool EmitStructuralAddedFileNode(u32 addedIndex, StructuralRebuildContext& context) {
    const PendingStructuralAdd& added = context.addedFiles[addedIndex];
    const TaggedOverrideEntry& entry = sOverrideDatabase.taggedEntries[added.overrideIndex];

    const u32 newNodeIdx = context.nextNodeIndex++;
    U8Node& newNode = context.newNodes[newNodeIdx];
    u32 nameOffset = 0;
    if (!AppendStructuralString(context, GetStructuralAddedBasename(addedIndex, context), nameOffset)) {
        return false;
    }
    newNode.typeName = nameOffset;

    const u32 writeOffset = Align32(context.writeOffset);
    newNode.dataOffset = writeOffset;
    newNode.dataSize = entry.size;

    if (!ReadOverrideFile(entry, context.newBuffer + writeOffset)) {
        return false;
    }

    const u32 paddedSize = Align32(newNode.dataSize);
    ZeroStructuralFilePadding(context.newBuffer, writeOffset, newNode.dataSize);
    context.writeOffset = writeOffset + paddedSize;

    MarkEntryApplied(context.entryAppliedBits, added.overrideIndex - context.rangeStart);
    ++context.patchedNodes;
    return true;
}

static const char* GetStructuralAddedPath(u32 addedIndex, const StructuralRebuildContext& context) {
    if (context.addedFiles == nullptr || context.addedPathPool == nullptr ||
        addedIndex >= context.addedFileCount) {
        return nullptr;
    }
    return context.addedPathPool + context.addedFiles[addedIndex].pathOffset;
}

static const char* GetStructuralAddedBasename(u32 addedIndex, const StructuralRebuildContext& context) {
    const char* path = GetStructuralAddedPath(addedIndex, context);
    if (path == nullptr) return nullptr;
    return FindBasename(path);
}

static bool StructuralDirectoryHasDirectAdditions(u32 oldDirIdx, const PendingStructuralAdd* addedFiles,
                                                  u32 addedFileCount) {
    for (u32 addedIdx = 0; addedIdx < addedFileCount; ++addedIdx) {
        if (addedFiles[addedIdx].parentDirIndex == oldDirIdx) return true;
    }
    return false;
}

static bool MarkStructuralKeptDirectories(u32 oldDirIdx, const U8Node* nodes, const u8* nodeDeleteFlags,
                                          const PendingStructuralAdd* addedFiles, u32 addedFileCount,
                                          u8* dirKeepFlags) {
    bool hasContent = (oldDirIdx == 0) || StructuralDirectoryHasDirectAdditions(oldDirIdx, addedFiles, addedFileCount);

    u32 childIdx = oldDirIdx + 1;
    const u32 dirEnd = nodes[oldDirIdx].dataSize;
    while (childIdx < dirEnd) {
        const U8Node& child = nodes[childIdx];
        if (NodeIsDir(child)) {
            if (MarkStructuralKeptDirectories(childIdx, nodes, nodeDeleteFlags, addedFiles, addedFileCount,
                                              dirKeepFlags)) {
                hasContent = true;
            }
            childIdx = child.dataSize;
            continue;
        }

        if (nodeDeleteFlags[childIdx] == 0) {
            hasContent = true;
        }
        ++childIdx;
    }

    dirKeepFlags[oldDirIdx] = hasContent ? 1 : 0;
    return hasContent;
}

static void MarkDeletedOverridesInSubtree(u32 oldDirIdx, StructuralRebuildContext& context) {
    const u32 dirEnd = context.oldNodes[oldDirIdx].dataSize;
    for (u32 nodeIdx = oldDirIdx + 1; nodeIdx < dirEnd; ++nodeIdx) {
        if (NodeIsDir(context.oldNodes[nodeIdx])) continue;
        if (context.nodeDeleteFlags[nodeIdx] == 0) continue;

        const u16 deleteIdx = context.nodeOverrideIndex[nodeIdx];
        if (deleteIdx == kInvalidScratchIndex16) continue;

        MarkEntryApplied(context.entryAppliedBits, deleteIdx - context.rangeStart);
        ++context.patchedNodes;
    }
}

static bool EmitStructuralDirectoryChildren(u32 oldDirIdx, u32 parentNewIdx, StructuralRebuildContext& context);

static bool EmitStructuralDirectoryNode(u32 oldDirIdx, u32 parentNewIdx, StructuralRebuildContext& context) {
    const u32 newDirIdx = context.nextNodeIndex++;
    U8Node& newDir = context.newNodes[newDirIdx];
    u32 nameOffset = 0;
    if (!AppendStructuralString(context, context.oldStringTable + NodeNameOffset(context.oldNodes[oldDirIdx]),
                                nameOffset)) {
        return false;
    }
    newDir.typeName = 0x01000000u | nameOffset;
    newDir.dataOffset = parentNewIdx;
    newDir.dataSize = newDirIdx + 1;

    if (!EmitStructuralDirectoryChildren(oldDirIdx, newDirIdx, context)) return false;

    newDir.dataSize = context.nextNodeIndex;
    return true;
}

static bool EmitStructuralDirectoryChildren(u32 oldDirIdx, u32 parentNewIdx, StructuralRebuildContext& context) {
    if (context.tempHeap == nullptr || context.childScratchCapacity == 0) return false;

    StructuralChildRef* children = EGG::Heap::alloc<StructuralChildRef>(
        sizeof(StructuralChildRef) * context.childScratchCapacity, 0x20, context.tempHeap);
    if (children == nullptr) return false;

    u32 childCount = 0;
    u32 childIdx = oldDirIdx + 1;
    const u32 dirEnd = context.oldNodes[oldDirIdx].dataSize;
    while (childIdx < dirEnd) {
        const U8Node& child = context.oldNodes[childIdx];
        const char* childName = context.oldStringTable + NodeNameOffset(child);
        if (NodeIsDir(child)) {
            if (context.dirKeepFlags != nullptr && context.dirKeepFlags[childIdx] == 0) {
                MarkDeletedOverridesInSubtree(childIdx, context);
                childIdx = child.dataSize;
                continue;
            }

            if (childCount >= context.childScratchCapacity) {
                EGG::Heap::free(children, context.tempHeap);
                return false;
            }
            children[childCount].oldNodeIndex = childIdx;
            children[childCount].addedFileIndex = 0;
            children[childCount].name = childName;
            children[childCount].isAddedFile = false;
            ++childCount;
            childIdx = child.dataSize;
            continue;
        }

        if (context.nodeDeleteFlags[childIdx] != 0) {
            const u16 deleteIdx = context.nodeOverrideIndex[childIdx];
            if (deleteIdx != kInvalidScratchIndex16) {
                MarkEntryApplied(context.entryAppliedBits, deleteIdx - context.rangeStart);
                ++context.patchedNodes;
            }
        } else {
            if (childCount >= context.childScratchCapacity) {
                EGG::Heap::free(children, context.tempHeap);
                return false;
            }
            children[childCount].oldNodeIndex = childIdx;
            children[childCount].addedFileIndex = 0;
            children[childCount].name = childName;
            children[childCount].isAddedFile = false;
            ++childCount;
        }
        ++childIdx;
    }

    for (u32 addedIdx = 0; addedIdx < context.addedFileCount; ++addedIdx) {
        if (context.addedFileEmitted != nullptr && context.addedFileEmitted[addedIdx] != 0) continue;
        if (context.addedFiles[addedIdx].parentDirIndex != oldDirIdx) continue;

        const char* addedName = GetStructuralAddedBasename(addedIdx, context);
        if (addedName == nullptr || addedName[0] == '\0') continue;
        if (childCount >= context.childScratchCapacity) {
            EGG::Heap::free(children, context.tempHeap);
            return false;
        }

        children[childCount].oldNodeIndex = 0;
        children[childCount].addedFileIndex = addedIdx;
        children[childCount].name = addedName;
        children[childCount].isAddedFile = true;
        ++childCount;
    }

    bool success = true;
    for (u32 sortedIdx = 0; sortedIdx < childCount; ++sortedIdx) {
        const StructuralChildRef childRef = children[sortedIdx];
        if (childRef.isAddedFile) {
            if (context.addedFileEmitted != nullptr) context.addedFileEmitted[childRef.addedFileIndex] = 1;
            if (!EmitStructuralAddedFileNode(childRef.addedFileIndex, context)) {
                success = false;
                break;
            }
            continue;
        }

        const U8Node& child = context.oldNodes[childRef.oldNodeIndex];
        if (NodeIsDir(child)) {
            if (!EmitStructuralDirectoryNode(childRef.oldNodeIndex, parentNewIdx, context)) {
                success = false;
                break;
            }
        } else if (!EmitStructuralExistingFileNode(childRef.oldNodeIndex, context)) {
            success = false;
            break;
        }
    }

    EGG::Heap::free(children, context.tempHeap);
    return success;
}

static bool EmitStructuralRootNode(u32 oldRootIdx, StructuralRebuildContext& context) {
    const u32 newRootIdx = context.nextNodeIndex++;
    U8Node& newRoot = context.newNodes[newRootIdx];
    u32 rootNameOffset = 0;
    if (!AppendStructuralString(context, context.oldStringTable + NodeNameOffset(context.oldNodes[oldRootIdx]),
                                rootNameOffset)) {
        return false;
    }
    newRoot.typeName = 0x01000000u | rootNameOffset;
    newRoot.dataOffset = context.oldNodes[oldRootIdx].dataOffset;
    newRoot.dataSize = newRootIdx + 1;

    if (!EmitStructuralDirectoryChildren(oldRootIdx, newRootIdx, context)) {
        return false;
    }

    newRoot.dataSize = context.nextNodeIndex;
    return true;
}

static u32 CountStructuralKeptOldNameBytes(u32 oldDirIdx, const U8Node* nodes, const char* stringTable,
                                           const u8* nodeDeleteFlags, const u8* dirKeepFlags) {
    if (nodes == nullptr || stringTable == nullptr) return 0;

    u32 total = 0;
    u32 childIdx = oldDirIdx + 1;
    const u32 dirEnd = nodes[oldDirIdx].dataSize;
    while (childIdx < dirEnd) {
        const U8Node& child = nodes[childIdx];
        if (NodeIsDir(child)) {
            if (dirKeepFlags == nullptr || dirKeepFlags[childIdx] != 0) {
                const char* name = stringTable + NodeNameOffset(child);
                total += strlen(name) + 1;
                total += CountStructuralKeptOldNameBytes(childIdx, nodes, stringTable, nodeDeleteFlags, dirKeepFlags);
            }
            childIdx = child.dataSize;
            continue;
        }

        if (nodeDeleteFlags == nullptr || nodeDeleteFlags[childIdx] == 0) {
            const char* name = stringTable + NodeNameOffset(child);
            total += strlen(name) + 1;
        }
        ++childIdx;
    }
    return total;
}

static void FreeStructuralRebuildTemps(PendingStructuralAdd* addedFiles, char* addedPathPool, u8* addedFileEmitted,
                                       u8* dirKeepFlags, EGG::Heap* heap) {
    if (addedFiles != nullptr) EGG::Heap::free(addedFiles, heap);
    if (addedPathPool != nullptr) EGG::Heap::free(addedPathPool, heap);
    if (addedFileEmitted != nullptr) EGG::Heap::free(addedFileEmitted, heap);
    if (dirKeepFlags != nullptr) EGG::Heap::free(dirKeepFlags, heap);
}

static void FreeStructuralMatchTemps(u8* nodeDeleteFlags, PendingStructuralAddCandidate* structuralAddCandidates,
                                     EGG::Heap* heap) {
    if (nodeDeleteFlags != nullptr) EGG::Heap::free(nodeDeleteFlags, heap);
    if (structuralAddCandidates != nullptr) EGG::Heap::free(structuralAddCandidates, heap);
}

static bool RebuildArchiveWithStructuralOverrides(const char* archiveBaseLower, u8*& archiveBase, u32& archiveSize,
                                                  EGG::Heap* sourceHeap, EGG::Heap*& archiveHeap, ARC::Header* header,
                                                  const U8Node* nodes, u32 nodeCount, const u16* nodeOverrideIndex,
                                                  const u8* nodeDeleteFlags,
                                                  const PendingStructuralAddCandidate* addCandidates,
                                                  u32 addCandidateCount, u32 rangeStart, u32 taggedCandidates,
                                                  u32* entryAppliedBits, u32 missingOverrides, u32* outAppliedOverrides,
                                                  u32* outPatchedNodes, u32* outMissingOverrides) {
    PendingStructuralAdd* addedFiles = nullptr;
    char* addedPathPool = nullptr;
    u8* addedFileEmitted = nullptr;
    u8* dirKeepFlags = nullptr;
    u32 addedPathPoolSize = 0;
    u32 addedNameBytes = 0;
    bool success = false;

    const u32 estimatedTempBytes =
        (header != nullptr ? header->combinedNodeSize : 0) +
        sizeof(StructuralChildRef) * (nodeCount + addCandidateCount) + 0x10000;
    HeapCandidate tempCandidates[4];
    tempCandidates[0].heap = RKSystem::mInstance.EGGRootMEM2;
    tempCandidates[0].reclaimedBytes = 0;
    tempCandidates[1].heap = RKSystem::mInstance.EGGRootMEM1;
    tempCandidates[1].reclaimedBytes = 0;
    tempCandidates[2].heap = GetOverridesHeap();
    tempCandidates[2].reclaimedBytes = 0;
    tempCandidates[3].heap = sourceHeap;
    tempCandidates[3].reclaimedBytes = 0;
    EGG::Heap* tempHeap = FindHeapWithSpace(tempCandidates, 4, estimatedTempBytes);
    if (tempHeap == nullptr) tempHeap = tempCandidates[2].heap;
    if (tempHeap == nullptr) tempHeap = sourceHeap;
    if (tempHeap == nullptr) {
        OS::Report("[Pulsar] Structural rebuild early fail '%s': no temp heap\n", archiveBaseLower);
        return false;
    }

    const u32 oldNodeBytes = sizeof(U8Node) * nodeCount;
    if (header == nullptr || header->combinedNodeSize < oldNodeBytes) {
        OS::Report("[Pulsar] Structural rebuild early fail '%s': bad header nodes=%u combined=0x%X\n",
                   archiveBaseLower, nodeCount, header != nullptr ? header->combinedNodeSize : 0);
        return false;
    }
    const u32 nodeOffset = header->nodeOffset;
    const u32 oldCombinedNodeSize = header->combinedNodeSize;
    const u32 oldStringTableSize = oldCombinedNodeSize - oldNodeBytes;
    const char* oldStringTable = reinterpret_cast<const char*>(nodes + nodeCount);
    if (!BuildStructuralAddedFiles(addCandidates, addCandidateCount, addedFiles, addCandidateCount, addedPathPool,
                                    addedPathPoolSize, addedNameBytes, oldStringTableSize, tempHeap)) {
        OS::Report("[Pulsar] Structural rebuild early fail '%s': added file list\n", archiveBaseLower);
        return false;
    }
    if (addCandidateCount > 0) {
        addedFileEmitted = EGG::Heap::alloc<u8>(addCandidateCount, 0x20, tempHeap);
        if (addedFileEmitted == nullptr) {
            OS::Report("[Pulsar] Structural rebuild early fail '%s': emitted flags count=%u\n", archiveBaseLower,
                       addCandidateCount);
            FreeStructuralRebuildTemps(addedFiles, addedPathPool, addedFileEmitted, dirKeepFlags, tempHeap);
            return false;
        }
        memset(addedFileEmitted, 0, addCandidateCount);
    }

    dirKeepFlags = EGG::Heap::alloc<u8>(nodeCount, 0x20, tempHeap);
    if (dirKeepFlags == nullptr) {
        OS::Report("[Pulsar] Structural rebuild early fail '%s': dir flags nodes=%u\n", archiveBaseLower, nodeCount);
        FreeStructuralRebuildTemps(addedFiles, addedPathPool, addedFileEmitted, dirKeepFlags, tempHeap);
        return false;
    }
    memset(dirKeepFlags, 0, nodeCount);
    MarkStructuralKeptDirectories(0, nodes, nodeDeleteFlags, addedFiles, addCandidateCount, dirKeepFlags);

    u32 deletedFileCount = 0;
    u32 deletedDirCount = 0;
    u32 totalDataSize = 0;
    for (u32 nodeIdx = 1; nodeIdx < nodeCount; ++nodeIdx) {
        if (NodeIsDir(nodes[nodeIdx])) {
            if (dirKeepFlags[nodeIdx] == 0) ++deletedDirCount;
            continue;
        }
        if (nodeDeleteFlags[nodeIdx] != 0) {
            ++deletedFileCount;
            continue;
        }

        u32 fileSize = nodes[nodeIdx].dataSize;
        const u16 idx = nodeOverrideIndex[nodeIdx];
        if (idx != kInvalidScratchIndex16) {
            fileSize = sOverrideDatabase.taggedEntries[idx].size;
        }
        totalDataSize += Align32(fileSize);
    }
    for (u32 addedIdx = 0; addedIdx < addCandidateCount; ++addedIdx) {
        totalDataSize += Align32(sOverrideDatabase.taggedEntries[addedFiles[addedIdx].overrideIndex].size);
    }

    const char* rootName = oldStringTable + NodeNameOffset(nodes[0]);
    const u32 newStringTableSize = strlen(rootName) + 1 +
                                   CountStructuralKeptOldNameBytes(0, nodes, oldStringTable, nodeDeleteFlags, dirKeepFlags) +
                                   addedNameBytes;
    const u32 newNodeCount = nodeCount - deletedFileCount - deletedDirCount + addCandidateCount;
    const u32 newCombinedNodeSize = sizeof(U8Node) * newNodeCount + newStringTableSize;
    const u32 newDataStart = Align32(nodeOffset + newCombinedNodeSize);
    const u32 newSize = Align32(newDataStart + totalDataSize);

    HeapCandidate candidates[4];
    candidates[0].heap = archiveHeap;
    candidates[0].reclaimedBytes = 0;
    candidates[1].heap = RKSystem::mInstance.EGGRootMEM2;
    candidates[1].reclaimedBytes = 0;
    candidates[2].heap = GetOverridesHeap();
    candidates[2].reclaimedBytes = 0;
    candidates[3].heap = RKSystem::mInstance.EGGRootMEM1;
    candidates[3].reclaimedBytes = 0;
    EGG::Heap* repackHeap = FindHeapWithSpace(candidates, 4, newSize);

    if (repackHeap == nullptr) {
        OS::Report("[Pulsar] Structural rebuild fail '%s': no separate heap for old=0x%X new=0x%X\n",
                   archiveBaseLower, archiveSize, newSize);
        FreeStructuralRebuildTemps(addedFiles, addedPathPool, addedFileEmitted, dirKeepFlags, tempHeap);
        return false;
    }

    u8* newBuffer = static_cast<u8*>(EGG::Heap::alloc(newSize, 0x20, repackHeap));
    if (newBuffer == nullptr) {
        OS::Report("[Pulsar] Structural loose override rebuild allocation failed for '%s': old=0x%X new=0x%X\n",
                   archiveBaseLower, archiveSize, newSize);
        FreeStructuralRebuildTemps(addedFiles, addedPathPool, addedFileEmitted, dirKeepFlags, tempHeap);
        return false;
    }

    memcpy(newBuffer, archiveBase, nodeOffset);

    ARC::Header* newHeader = reinterpret_cast<ARC::Header*>(newBuffer);
    newHeader->combinedNodeSize = newCombinedNodeSize;
    newHeader->fileOffset = newDataStart;
    if (nodeOffset > 0x10) {
        memset(newBuffer + 0x10, 0xCC, nodeOffset - 0x10);
    }

    U8Node* newNodes = reinterpret_cast<U8Node*>(newBuffer + newHeader->nodeOffset);
    char* newStringTable = reinterpret_cast<char*>(newNodes + newNodeCount);
    memset(newStringTable, 0, newStringTableSize);
    const u32 fstEnd = nodeOffset + newCombinedNodeSize;
    if (newDataStart > fstEnd) {
        memset(newBuffer + fstEnd, 0, newDataStart - fstEnd);
    }

    StructuralRebuildContext context;
    context.oldNodes = nodes;
    context.oldNodeCount = nodeCount;
    context.oldStringTable = oldStringTable;
    context.nodeOverrideIndex = nodeOverrideIndex;
    context.nodeDeleteFlags = nodeDeleteFlags;
    context.addedFiles = addedFiles;
    context.addedFileCount = addCandidateCount;
    context.addedPathPool = addedPathPool;
    context.addedFileEmitted = addedFileEmitted;
    context.dirKeepFlags = dirKeepFlags;
    context.tempHeap = tempHeap;
    context.childScratchCapacity = nodeCount + addCandidateCount;
    context.oldArchiveBase = archiveBase;
    context.newBuffer = newBuffer;
    context.newNodes = newNodes;
    context.newStringTable = newStringTable;
    context.newStringTableSize = newStringTableSize;
    context.nextStringOffset = 0;
    context.writeOffset = newDataStart;
    context.nextNodeIndex = 0;
    context.patchedNodes = 0;
    context.rangeStart = rangeStart;
    context.entryAppliedBits = entryAppliedBits;

    success = EmitStructuralRootNode(0, context) && context.nextNodeIndex == newNodeCount &&
              context.nextStringOffset == newStringTableSize;
    if (!success) {
        OS::Report("[Pulsar] Structural rebuild detail '%s': emitted=%u/%u strings=%u/%u write=0x%X/0x%X\n",
                   archiveBaseLower, context.nextNodeIndex, newNodeCount, context.nextStringOffset, newStringTableSize,
                   context.writeOffset, newSize);
    }
    if (success) {
        const u32 finalSize = Align32(context.writeOffset);
        if (finalSize < newSize) {
            memset(newBuffer + finalSize, 0, newSize - finalSize);
        }
        OS::DCStoreRange(newBuffer, finalSize);

        EGG::Heap::free(archiveBase, sourceHeap);
        archiveBase = newBuffer;
        archiveSize = finalSize;
        archiveHeap = repackHeap;

        const u32 appliedOverrides = CountAppliedEntries(entryAppliedBits, taggedCandidates);
        SetOverrideResult(outAppliedOverrides, appliedOverrides, outPatchedNodes, context.patchedNodes,
                          outMissingOverrides, missingOverrides);
        OS::Report("[Pulsar] Structural loose overrides applied for '%s': applied=%u patched=%u missing=%u\n",
                   archiveBaseLower, appliedOverrides, context.patchedNodes, missingOverrides);
    } else {
        EGG::Heap::free(newBuffer, repackHeap);
    }

    FreeStructuralRebuildTemps(addedFiles, addedPathPool, addedFileEmitted, dirKeepFlags, tempHeap);
    return success;
}

}  // namespace

bool IsModsPath(const char* path) {
    if (path == nullptr) return false;
    if (strcmp(path, kModsRoot) == 0) return true;
    if (StartsWith(path, kModsRootPrefix)) return true;


    // Also treat the resolved SD root as internal to avoid recursive redirects.

    const u32 rootLen = strlen(sModsRootPath);
    if (rootLen == 0) return false;
    if (strncmp(path, sModsRootPath, rootLen) != 0) return false;
    return path[rootLen] == '\0' || path[rootLen] == '/';
}

const char* ResolveWholeFileOverride(const char* path, char* resolvedPath, u32 resolvedSize, bool* outRedirected) {
    if (outRedirected != nullptr) *outRedirected = false;
    if (path == nullptr || resolvedPath == nullptr || resolvedSize == 0) return path;
    RefreshOverrideCacheState();
    if (!AreLooseArchiveOverridesEnabled()) return path;

    if (IsModsPath(path)) return path;
    // Do not redirect individual loose-file requests for these raw resources into `/patches`.
    // Tagged archive-member overrides for the same extensions are also rejected during index construction.
    if (IsBlockedLooseRawOverrideExtension(path)) return path;
    EnsureOverrideIndicesBuilt();
    if (!sHasWholeFileOverrides) return path;

    const char* base = FindBasename(path);
    if (base == nullptr || base[0] == '\0') return path;
    if (strlen(base) >= OVERRIDE_MAX_PATH) return path;

    char basenameLower[OVERRIDE_MAX_PATH];
    ToLowerCopy(basenameLower, base, sizeof(basenameLower));
    const WholeFileOverrideEntry* entry = FindWholeFileOverride(sOverrideDatabase, basenameLower);
    if (entry == nullptr) return path;
    if (!BuildStoredOverridePath(entry->sourcePathOffset, resolvedPath, resolvedSize)) return path;
    if (outRedirected != nullptr) *outRedirected = true;
    return resolvedPath;
}

bool HasStructuralLooseOverrides(const char* archiveBaseLower) {
    RefreshOverrideCacheState();
    if (!AreLooseArchiveOverridesEnabled()) return false;

    EnsureOverrideIndicesBuilt();
    if (sOverrideDatabase.taggedEntries == nullptr || sOverrideDatabase.taggedCount == 0) return false;
    if (archiveBaseLower == nullptr || archiveBaseLower[0] == '\0') return false;

    u16 tagId = 0;
    if (!FindArchiveTagId(sOverrideDatabase, archiveBaseLower, tagId)) return false;

    u32 rangeStart = 0;
    u32 rangeEnd = 0;
    if (!FindArchiveTagRangeById(sOverrideDatabase, tagId, rangeStart, rangeEnd)) return false;

    for (u32 i = rangeStart; i < rangeEnd; ++i) {
        const u16 flags = sOverrideDatabase.taggedEntries[i].flags;
        if ((flags & (OVERRIDEENTRYFLAG_IS_DELETE | OVERRIDEENTRYFLAG_HAS_SUBPATH)) != 0) {
            return true;
        }
    }
    return false;
}

bool ShouldApplyLooseOverrides(const char* path, char* archiveBaseLower, u32 archiveBaseLowerSize) {
    if (path == nullptr || archiveBaseLower == nullptr || archiveBaseLowerSize == 0) return false;
    RefreshOverrideCacheState();
    if (!AreLooseArchiveOverridesEnabled()) return false;
    // Loose files under the mods root are already redirected content, so never feed them back into archive patching.
    if (IsModsPath(path)) return false;
    // Tagged member overrides only target compressed archive loads.
    if (!IsFileExtensionSZS(path)) return false;


    // Cheap gate only; index lookup happens after decompression.

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

static u32 ApplyInPlaceLooseOverrides(u8* archiveBase, U8Node* nodes, u32 nodeCount, u16* nodeOverrideIndex,
                                      u32* fileSlotCapacities, u32* entryAppliedBits, u32 rangeStart) {
    if (archiveBase == nullptr || nodes == nullptr || nodeOverrideIndex == nullptr || fileSlotCapacities == nullptr ||
        entryAppliedBits == nullptr) {
        return 0;
    }

    u32 patchedNodes = 0;
    for (u32 nodeIdx = 1; nodeIdx < nodeCount; ++nodeIdx) {
        const u16 idx = nodeOverrideIndex[nodeIdx];
        if (idx == kInvalidScratchIndex16) continue;
        if (NodeIsDir(nodes[nodeIdx])) continue;

        const TaggedOverrideEntry& entry = sOverrideDatabase.taggedEntries[idx];
        if (entry.size > fileSlotCapacities[nodeIdx]) {
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
        MarkEntryApplied(entryAppliedBits, idx - rangeStart);
        ++patchedNodes;
    }

    if (patchedNodes > 0) {
        OS::DCStoreRange(nodes, nodeCount * sizeof(U8Node));
    }
    return patchedNodes;
}

static u32 CountInPlaceOversizedOverrides(const U8Node* nodes, u32 nodeCount, const u16* nodeOverrideIndex,
                                          const u32* fileSlotCapacities) {
    if (nodes == nullptr || nodeOverrideIndex == nullptr || fileSlotCapacities == nullptr) return 0;

    u32 oversizedNodes = 0;
    for (u32 nodeIdx = 1; nodeIdx < nodeCount; ++nodeIdx) {
        const u16 idx = nodeOverrideIndex[nodeIdx];
        if (idx == kInvalidScratchIndex16) continue;
        if (NodeIsDir(nodes[nodeIdx])) continue;
        if (sOverrideDatabase.taggedEntries[idx].size > fileSlotCapacities[nodeIdx]) {
            ++oversizedNodes;
        }
    }
    return oversizedNodes;
}

bool ApplyLooseOverrides(const char* archiveBaseLower, u8*& archiveBase, u32& archiveSize, EGG::Heap* sourceHeap,
                         EGG::Heap*& archiveHeap, u32* outAppliedOverrides, u32* outPatchedNodes,
                         u32* outMissingOverrides, const u8* compressedData) {
    SetOverrideResult(outAppliedOverrides, 0, outPatchedNodes, 0, outMissingOverrides, 0);
    RefreshOverrideCacheState();
    if (!AreLooseArchiveOverridesEnabled()) return false;


    // Resolve the tag bucket, match entries to U8 nodes, then patch in place or repack.
    // Applied override count can differ from patched nodes when a basename fans out.

    EnsureOverrideIndicesBuilt();
    if (sOverrideDatabase.taggedEntries == nullptr || sOverrideDatabase.taggedCount == 0) return false;
    if (archiveBase == nullptr || archiveBaseLower == nullptr || archiveBaseLower[0] == '\0') return false;
    if (sourceHeap == nullptr) {
        return false;
    }
    if (archiveHeap == nullptr) {
        archiveHeap = sourceHeap;
    }

    u16 tagId = 0;
    if (!FindArchiveTagId(sOverrideDatabase, archiveBaseLower, tagId)) {
        return false;
    }

    u32 rangeStart = 0;
    u32 rangeEnd = 0;
    if (!FindArchiveTagRangeById(sOverrideDatabase, tagId, rangeStart, rangeEnd)) {
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
    if (!EnsureLooseOverrideScratchCapacity(nodeCount, taggedCandidates, nodeCount, sourceHeap)) {
        return false;
    }
    // These buffers are reused across archive loads and only grow when a larger archive or candidate bucket appears.
    u16* nodeOverrideIndex = sLooseOverrideScratch.nodeOverrideIndex;
    u32* entryAppliedBits = sLooseOverrideScratch.entryAppliedBits;
    u16* basenameHashHeads16 = sLooseOverrideScratch.basenameHashHeads16;
    u16* basenameHashNext16 = sLooseOverrideScratch.basenameHashNext16;
    s32* basenameHashHeads32 = sLooseOverrideScratch.basenameHashHeads32;
    s32* basenameHashNext32 = sLooseOverrideScratch.basenameHashNext32;
    const bool useWideBasenameIndices = sLooseOverrideScratch.useWideBasenameIndices;
    const u32 basenameHashCapacity = sLooseOverrideScratch.basenameHashCapacity;
    u32* fileNodeOrder = sLooseOverrideScratch.repackOffsets;
    u32* fileSlotCapacities = sLooseOverrideScratch.repackSizes;
    u32* repackOffsets = sLooseOverrideScratch.repackOffsets;
    u32* repackSizes = sLooseOverrideScratch.repackSizes;
    u32* repackOriginalSizes = sLooseOverrideScratch.repackOriginalSizes;
    u32* repackOrder = sLooseOverrideScratch.repackOrder;

    memset(nodeOverrideIndex, 0xFF, sizeof(u16) * nodeCount);
    ClearEntryAppliedBits(entryAppliedBits, taggedCandidates);
    if (useWideBasenameIndices) {
        BuildArchiveBasenameLookup32(nodes, nodeCount, stringTable, basenameHashHeads32, basenameHashCapacity,
                                     basenameHashNext32);
    } else {
        BuildArchiveBasenameLookup16(nodes, nodeCount, stringTable, basenameHashHeads16, basenameHashCapacity,
                                     basenameHashNext16);
    }
    BuildArchiveFileSlotCapacities(nodes, nodeCount, archiveSize, fileNodeOrder, fileSlotCapacities);

    EGG::Heap* structuralTempHeap = GetOverridesHeap();
    if (structuralTempHeap == nullptr) structuralTempHeap = sourceHeap;
    u8* nodeDeleteFlags = nullptr;
    PendingStructuralAddCandidate* structuralAddCandidates = nullptr;
    if (structuralTempHeap != nullptr) {
        nodeDeleteFlags = EGG::Heap::alloc<u8>(nodeCount, 0x20, structuralTempHeap);
        structuralAddCandidates = EGG::Heap::alloc<PendingStructuralAddCandidate>(
            sizeof(PendingStructuralAddCandidate) * taggedCandidates, 0x20, structuralTempHeap);
    }
    if (nodeDeleteFlags == nullptr || structuralAddCandidates == nullptr) {
        FreeStructuralMatchTemps(nodeDeleteFlags, structuralAddCandidates, structuralTempHeap);
        return false;
    }
    memset(nodeDeleteFlags, 0, nodeCount);

    bool anyOverrides = false;
    bool hasStructuralChanges = false;
    u32 missingOverrides = 0;
    u32 structuralAddCandidateCount = 0;

    // Build the node -> override table. Basename-only entries fan out by design.
    for (u32 i = rangeStart; i < rangeEnd; ++i) {
        const TaggedOverrideEntry& entry = sOverrideDatabase.taggedEntries[i];
        char matchName[OVERRIDE_MAX_PATH];
        if (!GetTaggedEntryMatchName(entry, matchName, sizeof(matchName)) || matchName[0] == '\0') {
            continue;
        }
        const bool isDelete = (entry.flags & OVERRIDEENTRYFLAG_IS_DELETE) != 0;

        if ((entry.flags & OVERRIDEENTRYFLAG_HAS_SUBPATH) != 0) {
            s32 entryNum = ARC::ConvertPathToEntrynum(&handle, matchName);
            if (entryNum < 0) {
                if (!isDelete) {
                    const char* slash = FindLastChar(matchName, '/');
                    u16 parentDirIndex = 0;
                    bool canAddStructuralFile = true;
                    if (slash != nullptr) {
                        char parentPath[OVERRIDE_MAX_PATH];
                        const u32 parentLen = static_cast<u32>(slash - matchName);
                        if (parentLen == 0 || parentLen + 1 > sizeof(parentPath)) {
                            canAddStructuralFile = false;
                        } else {
                            memcpy(parentPath, matchName, parentLen);
                            parentPath[parentLen] = '\0';
                            const s32 parentEntryNum = ARC::ConvertPathToEntrynum(&handle, parentPath);
                            if (parentEntryNum < 0 || NodeIsDir(nodes[parentEntryNum]) == 0) {
                                canAddStructuralFile = false;
                            } else {
                                parentDirIndex = static_cast<u16>(parentEntryNum);
                            }
                        }
                    }

                    if (canAddStructuralFile && structuralAddCandidateCount < taggedCandidates) {
                        structuralAddCandidates[structuralAddCandidateCount].parentDirIndex = parentDirIndex;
                        structuralAddCandidates[structuralAddCandidateCount].overrideIndex = static_cast<u16>(i);
                        ++structuralAddCandidateCount;
                        anyOverrides = true;
                        hasStructuralChanges = true;
                        continue;
                    }
                }

                // Count it as missing so logs can tell "override exists on disk" from "archive actually contains that node".
                ++missingOverrides;
                continue;
            }
            if (NodeIsDir(nodes[entryNum])) {
                // A matching directory path is still unusable because only file payload nodes can be replaced.
                ++missingOverrides;
                continue;
            }
            nodeOverrideIndex[entryNum] = static_cast<u16>(i);
            nodeDeleteFlags[entryNum] = isDelete ? 1 : 0;
            if (isDelete) hasStructuralChanges = true;
            anyOverrides = true;
        } else {
            const u32 matchCount = useWideBasenameIndices
                                       ? MatchArchiveBasenameOverride32(nodes, stringTable, basenameHashHeads32,
                                                                        basenameHashNext32, basenameHashCapacity,
                                                                       matchName, static_cast<u16>(i), nodeOverrideIndex)
                                       : MatchArchiveBasenameOverride16(nodes, stringTable, basenameHashHeads16,
                                                                       basenameHashNext16, basenameHashCapacity,
                                                                       matchName, static_cast<u16>(i), nodeOverrideIndex);
            if (matchCount == 0) {
                ++missingOverrides;
                continue;
            }
            if (isDelete) {
                hasStructuralChanges = true;
                for (u32 nodeIdx = 1; nodeIdx < nodeCount; ++nodeIdx) {
                    if (nodeOverrideIndex[nodeIdx] == static_cast<u16>(i)) {
                        nodeDeleteFlags[nodeIdx] = 1;
                    }
                }
            } else {
                for (u32 nodeIdx = 1; nodeIdx < nodeCount; ++nodeIdx) {
                    if (nodeOverrideIndex[nodeIdx] == static_cast<u16>(i)) {
                        nodeDeleteFlags[nodeIdx] = 0;
                    }
                }
            }
            anyOverrides = true;
        }
    }

    if (!anyOverrides) {
        FreeStructuralMatchTemps(nodeDeleteFlags, structuralAddCandidates, structuralTempHeap);
        if (missingOverrides > 0) {
            OS::Report("[Pulsar] Loose overrides skipped for '%s': %u tagged file(s) did not match archive contents\n",
                       archiveBaseLower, missingOverrides);
        }
        SetOverrideResult(outAppliedOverrides, 0, outPatchedNodes, 0, outMissingOverrides, missingOverrides);
        return false;
    }

    if (hasStructuralChanges) {
        const bool rebuilt = RebuildArchiveWithStructuralOverrides(
            archiveBaseLower, archiveBase, archiveSize, sourceHeap, archiveHeap, header, nodes, nodeCount, nodeOverrideIndex,
            nodeDeleteFlags, structuralAddCandidates, structuralAddCandidateCount, rangeStart, taggedCandidates,
            entryAppliedBits, missingOverrides, outAppliedOverrides, outPatchedNodes, outMissingOverrides);
        if (!rebuilt) {
            OS::Report("[Pulsar] Structural loose override rebuild failed for '%s': candidates=%u missing=%u\n",
                       archiveBaseLower, structuralAddCandidateCount, missingOverrides);
        }
        FreeStructuralMatchTemps(nodeDeleteFlags, structuralAddCandidates, structuralTempHeap);
        return rebuilt;
    }

    FreeStructuralMatchTemps(nodeDeleteFlags, structuralAddCandidates, structuralTempHeap);

    bool needsRepack = false;
    for (u32 nodeIdx = 1; nodeIdx < nodeCount; ++nodeIdx) {
        const u16 idx = nodeOverrideIndex[nodeIdx];
        if (idx == kInvalidScratchIndex16) continue;
        if (NodeIsDir(nodes[nodeIdx])) continue;
        // In-place growth is safe as long as the replacement stays inside this
        // node's real byte slot up to the next file payload.
        if (sOverrideDatabase.taggedEntries[idx].size > fileSlotCapacities[nodeIdx]) {
            needsRepack = true;
            break;
        }
    }


    u32 patchedNodes = 0;
    if (!needsRepack) {

        // Fast path: overwrite payloads in place and zero-fill shrink leftovers.

        patchedNodes = ApplyInPlaceLooseOverrides(archiveBase, nodes, nodeCount, nodeOverrideIndex, fileSlotCapacities,
                                                  entryAppliedBits, rangeStart);

        const u32 appliedOverrides = CountAppliedEntries(entryAppliedBits, taggedCandidates);

        SetOverrideResult(outAppliedOverrides, appliedOverrides, outPatchedNodes, patchedNodes, outMissingOverrides,
                          missingOverrides);
        return appliedOverrides > 0;
    }

    const u32 dataStart = GetFileDataStart(header);
    // Repack keeps metadata and rewrites the payload region.
    if (dataStart == 0 || dataStart > archiveSize) {
        return false;
    }

    u32 totalDataSize = 0;
    for (u32 nodeIdx = 1; nodeIdx < nodeCount; ++nodeIdx) {
        if (NodeIsDir(nodes[nodeIdx])) continue;
        const u16 idx = nodeOverrideIndex[nodeIdx];
        u32 size = nodes[nodeIdx].dataSize;
        if (idx != kInvalidScratchIndex16) {
            // Reserve space for replacement sizes up front so the repack layout is fully known before copying.
            size = sOverrideDatabase.taggedEntries[idx].size;
        }
        totalDataSize += Align32(size);
    }

    const u32 originalArchiveSize = archiveSize;
    u32 newSize = dataStart + totalDataSize;
    newSize = Align32(newSize);

    const u32 growth = (newSize > archiveSize) ? (newSize - archiveSize) : 0;
    EGG::Heap* repackHeap = archiveHeap;

    EGG::Heap* candidates[3];
    candidates[0] = RKSystem::mInstance.EGGRootMEM2;
    candidates[1] = RKSystem::mInstance.EGGRootMEM1;
    candidates[2] = GetOverridesHeap();


    // Prefer a roomier heap; source-heap growth stays capped.

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
    bool repackPartialFailure = false;
    u32 repackOrderCount = 0;

    if (repackHeap == archiveHeap && allowSourceHeap && compressedData != nullptr) {

        // Same-heap repack is only safe when every file moves forward and no
        // override shrinks after later files have been relocated. In that narrow
        // case we can free the old archive, decompress the original SZS back into
        // a new larger buffer on the same heap, and then move files downward from
        // highest original offset to lowest without clobbering unread data.

        memset(repackOffsets, 0, sizeof(u32) * nodeCount);
        memset(repackSizes, 0, sizeof(u32) * nodeCount);
        memset(repackOriginalSizes, 0, sizeof(u32) * nodeCount);
        memset(repackOrder, 0, sizeof(u32) * nodeCount);
        u32 plannedOffset = dataStart;
        bool allOffsetsForward = true;
        bool hasShrinkOverride = false;
        for (u32 nodeIdx = 1; nodeIdx < nodeCount; ++nodeIdx) {
            if (NodeIsDir(nodes[nodeIdx])) continue;
            plannedOffset = Align32(plannedOffset);
            repackOffsets[nodeIdx] = plannedOffset;
            const u16 idx = nodeOverrideIndex[nodeIdx];
            repackOriginalSizes[nodeIdx] = nodes[nodeIdx].dataSize;
            const u32 plannedSize =
                (idx != kInvalidScratchIndex16) ? sOverrideDatabase.taggedEntries[idx].size : nodes[nodeIdx].dataSize;
            repackSizes[nodeIdx] = plannedSize;
            repackOrder[repackOrderCount++] = nodeIdx;
            if (plannedOffset < nodes[nodeIdx].dataOffset) {
                allOffsetsForward = false;
            }
            if (idx != kInvalidScratchIndex16 && plannedSize < nodes[nodeIdx].dataSize) {
                // Same-heap repack cannot safely recover from a failed shrink override after later files move.
                hasShrinkOverride = true;
            }
            plannedOffset += Align32(plannedSize);
        }
        useSameHeapRepack = allOffsetsForward && !hasShrinkOverride;
        if (!useSameHeapRepack) {
            repackOrderCount = 0;
        } else {
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
        if (archiveBase != nullptr) {
            ARC::Header* fallbackHeader = reinterpret_cast<ARC::Header*>(archiveBase);
            U8Node* fallbackNodes = reinterpret_cast<U8Node*>(archiveBase + fallbackHeader->nodeOffset);
            const u32 oversizedNodes =
                CountInPlaceOversizedOverrides(fallbackNodes, nodeCount, nodeOverrideIndex, fileSlotCapacities);
            if (oversizedNodes > 0) {
                OS::Report("[Pulsar] Loose override repack fallback rejected for '%s': oversized=%u missing=%u\n",
                           archiveBaseLower, oversizedNodes, missingOverrides);
                SetOverrideResult(outAppliedOverrides, 0, outPatchedNodes, 0, outMissingOverrides, missingOverrides);
                return false;
            }
            patchedNodes = ApplyInPlaceLooseOverrides(archiveBase, fallbackNodes, nodeCount, nodeOverrideIndex,
                                                      fileSlotCapacities, entryAppliedBits, rangeStart);

            const u32 appliedOverrides = CountAppliedEntries(entryAppliedBits, taggedCandidates);

            if (patchedNodes > 0) {
                OS::Report("[Pulsar] Loose override repack fallback used for '%s': applied=%u patched=%u oversized=%u missing=%u\n",
                           archiveBaseLower, appliedOverrides, patchedNodes, oversizedNodes, missingOverrides);
                SetOverrideResult(outAppliedOverrides, appliedOverrides, outPatchedNodes, patchedNodes,
                                  outMissingOverrides, missingOverrides);
                return true;
            }
        }
        SetOverrideResult(outAppliedOverrides, 0, outPatchedNodes, 0, outMissingOverrides, missingOverrides);
        return false;
    }

    if (!useSameHeapRepack) {
        // Copy the untouched metadata prefix now; file payloads get rewritten into their new aligned slots below.
        memcpy(newBuffer, archiveBase, dataStart);
    }
    ARC::Header* newHeader = reinterpret_cast<ARC::Header*>(newBuffer);
    newHeader->fileOffset = dataStart;
    U8Node* newNodes = reinterpret_cast<U8Node*>(newBuffer + newHeader->nodeOffset);

    u32 writeOffset = dataStart;
    if (useSameHeapRepack) {
        for (u32 orderIdx = 0; orderIdx < repackOrderCount; ++orderIdx) {
            const u32 nodeIdx = repackOrder[orderIdx];
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
            const u16 idx = nodeOverrideIndex[nodeIdx];
            if (idx == kInvalidScratchIndex16) continue;

            const TaggedOverrideEntry& entry = sOverrideDatabase.taggedEntries[idx];
            const u32 oldSize = repackOriginalSizes[nodeIdx];
            const u32 newOffset = repackOffsets[nodeIdx];

            if (!ReadOverrideFile(entry, newBuffer + newOffset)) {
                // Keep metadata internally consistent; the partial repack is discarded below.
                newNodes[nodeIdx].dataSize = oldSize;
                repackPartialFailure = true;
                continue;
            }
            MarkEntryApplied(entryAppliedBits, idx - rangeStart);
            ++patchedNodes;
        }

        writeOffset = dataStart + totalDataSize;
    } else {
        for (u32 nodeIdx = 1; nodeIdx < nodeCount; ++nodeIdx) {
            if (NodeIsDir(nodes[nodeIdx])) continue;

            const u16 idx = nodeOverrideIndex[nodeIdx];
            const u32 oldOffset = nodes[nodeIdx].dataOffset;
            const u32 oldSize = nodes[nodeIdx].dataSize;
            bool useOverride = (idx != kInvalidScratchIndex16);
            u32 newFileSize = useOverride ? sOverrideDatabase.taggedEntries[idx].size : oldSize;

            writeOffset = Align32(writeOffset);
            const u32 alignedSize = Align32(newFileSize);
            if (writeOffset + alignedSize > newSize) {
                const TaggedOverrideEntry& entry = sOverrideDatabase.taggedEntries[idx];
                const char* relativePath = GetRelativePath(entry.sourcePathOffset);
                OS::Report("[Pulsar] Loose override '%s' skipped in '%s': repack buffer too small for 0x%X bytes\n",
                           relativePath != nullptr ? relativePath : "<missing>", archiveBaseLower, newFileSize);
                // Recover by copying the original member instead of throwing away the entire repack.
                useOverride = false;
                newFileSize = oldSize;
                repackPartialFailure = true;
            }

            newNodes[nodeIdx].dataOffset = writeOffset;
            if (useOverride) {
                const TaggedOverrideEntry& entry = sOverrideDatabase.taggedEntries[idx];
                if (!ReadOverrideFile(entry, newBuffer + writeOffset)) {
                    // Keep the staging buffer parseable; the partial repack is discarded below.
                    useOverride = false;
                    newFileSize = oldSize;
                    repackPartialFailure = true;
                } else {
                    MarkEntryApplied(entryAppliedBits, idx - rangeStart);
                    ++patchedNodes;
                }
            }

            if (!useOverride) {
                memcpy(newBuffer + writeOffset, archiveBase + oldOffset, oldSize);
            }

            newNodes[nodeIdx].dataSize = newFileSize;
            const u32 paddedSize = Align32(newFileSize);
            if (paddedSize > newFileSize) {
                memset(newBuffer + writeOffset + newFileSize, 0, paddedSize - newFileSize);
            }
            writeOffset += paddedSize;
        }
    }

    const u32 appliedOverrides = CountAppliedEntries(entryAppliedBits, taggedCandidates);
    if (repackPartialFailure) {
        OS::Report("[Pulsar] Loose override repack incomplete for '%s': applied=%u patched=%u missing=%u\n",
                   archiveBaseLower, appliedOverrides, patchedNodes, missingOverrides);
        if (useSameHeapRepack && compressedData != nullptr) {
            EGG::Decomp::decodeSZS(const_cast<u8*>(compressedData), newBuffer);
            if (newSize > originalArchiveSize) {
                memset(newBuffer + originalArchiveSize, 0, newSize - originalArchiveSize);
            }
            archiveBase = newBuffer;
            archiveSize = originalArchiveSize;
            archiveHeap = repackHeap;
            OS::DCStoreRange(archiveBase, archiveSize);
        } else {
            EGG::Heap::free(newBuffer, repackHeap);
        }
        SetOverrideResult(outAppliedOverrides, appliedOverrides, outPatchedNodes, patchedNodes, outMissingOverrides,
                          missingOverrides);
        return false;
    }

    u32 finalSize = Align32(writeOffset);
    // Clamp to the allocated size so bad metadata cannot claim a larger archive than the buffer we own.
    if (finalSize > newSize) finalSize = newSize;
    if (!useSameHeapRepack && finalSize < newSize) {
        memset(newBuffer + finalSize, 0, newSize - finalSize);
    }
    OS::DCStoreRange(newBuffer, finalSize);

    if (!useSameHeapRepack) {
        EGG::Heap::free(archiveBase, sourceHeap);
    }
    archiveBase = newBuffer;
    archiveSize = finalSize;
    archiveHeap = repackHeap;

    SetOverrideResult(outAppliedOverrides, appliedOverrides, outPatchedNodes, patchedNodes, outMissingOverrides,
                      missingOverrides);
    return appliedOverrides > 0;
}

static void ArchiveFileLoadOverride(ArchiveFile* file, const char* path, EGG::Heap* mountHeap, bool isCompressed,
                                    s32 allocDirection, EGG::Heap* dumpHeap, EGG::Archive::FileInfo* info) {


    // Normalize MKW's UI fallback quirk: a localized UI request can be followed by suffix-only `.szs`.
    // Whole-file redirects happen before rip/decompress so plain loose replacements skip member patching.

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

bool AreLooseArchiveOverridesEnabledForDebug() {
    RefreshOverrideCacheState();
    return AreLooseArchiveOverridesEnabled();
}

bool GetLooseBRSAROverrideSizes(u32 fileId, u32& outFileSize, u32& outWaveDataSize) {
    outFileSize = 0;
    outWaveDataSize = 0;

    BRSAROverrideLayout layout;
    BRSAROverrideSlot* entry = nullptr;
    if (!ResolveLooseBRSAROverride(fileId, entry, layout)) return false;

    outFileSize = layout.fileSize;
    outWaveDataSize = layout.waveSize;
    return true;
}

static bool ReadLooseBRSAROverrideRange(u32 fileId, void* dest, u32 size, bool waveData) {
    if (dest == nullptr || size == 0) return false;

    BRSAROverrideLayout layout;
    BRSAROverrideSlot* entry = nullptr;
    if (!ResolveLooseBRSAROverride(fileId, entry, layout)) return false;

    const u32 readOffset = waveData ? layout.waveOffset : 0;
    const u32 expectedSize = waveData ? layout.waveSize : layout.fileSize;
    if (expectedSize != size || (waveData && readOffset == 0)) return false;

    char fullPath[OVERRIDE_MAX_PATH];
    if (!BuildStoredOverridePath(entry->sourcePathOffset, fullPath, sizeof(fullPath))) return false;
    return ReadDVDFileRange(fullPath, dest, size, readOffset);
}

bool ReadLooseBRSAROverrideFile(u32 fileId, void* dest, u32 size) {
    return ReadLooseBRSAROverrideRange(fileId, dest, size, false);
}

bool ReadLooseBRSAROverrideWaveData(u32 fileId, void* dest, u32 size) {
    return ReadLooseBRSAROverrideRange(fileId, dest, size, true);
}

u32 GetLooseArchiveOverrideFileCount() {
    RefreshOverrideCacheState();
    if (!AreLooseArchiveOverridesEnabled()) return 0;

    EnsureOverrideIndicesBuilt();
    return sOverrideDatabase.taggedCount + sOverrideDatabase.wholeFileCount + sOverrideDatabase.brsarCount;
}

}  // namespace IOOverrides
}  // namespace Pulsar
