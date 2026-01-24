#include <kamek.hpp>
#include <PulsarSystem.hpp>
#include <IO/LooseArchiveOverrides.hpp>
#include <IO/IO.hpp>
#include <IO/RiivoIO.hpp>
#include <IO/SDIO.hpp>
#include <MarioKartWii/Archive/ArchiveFile.hpp>
#include <core/rvl/os/OS.hpp>
#include <core/rvl/os/OSCache.hpp>
#include <core/nw4r/ut/Misc.hpp>

namespace Pulsar {

// ============================================================================
// Helper Functions
// ============================================================================

const char* StringFindLastChar(const char* str, char c) {
    if (str == nullptr) return nullptr;
    const char* last = nullptr;
    while (*str) {
        if (*str == c) last = str;
        str++;
    }
    // Check null terminator if c is 0
    if (c == '\0') return str;
    return last;
}

const char* GetBasename(const char* path) {
    if (path == nullptr) return nullptr;
    const char* lastSlash = StringFindLastChar(path, '/');
    return lastSlash ? lastSlash + 1 : path;
}

void ExtractArchiveBaseName(const char* path, char* outName, u32 outSize) {
    if (path == nullptr || outName == nullptr || outSize == 0) {
        if (outName && outSize > 0) outName[0] = '\0';
        return;
    }
    
    const char* basename = GetBasename(path);
    
    // Find the last '.' to strip extension
    const char* dot = StringFindLastChar(basename, '.');
    u32 len = dot ? static_cast<u32>(dot - basename) : strlen(basename);
    
    if (len >= outSize) len = outSize - 1;
    strncpy(outName, basename, len);
    outName[len] = '\0';
}

// Case-insensitive string comparison
static int StrCaseCmp(const char* a, const char* b) {
    while (*a && *b) {
        char ca = *a;
        char cb = *b;
        // Simple ASCII lowercase
        if (ca >= 'A' && ca <= 'Z') ca += 'a' - 'A';
        if (cb >= 'A' && cb <= 'Z') cb += 'a' - 'A';
        if (ca != cb) return ca - cb;
        a++;
        b++;
    }
    return *a - *b;
}

// Case-insensitive string ends with check
static bool StrEndsWith(const char* str, const char* suffix) {
    u32 strLen = strlen(str);
    u32 suffixLen = strlen(suffix);
    if (suffixLen > strLen) return false;
    return StrCaseCmp(str + strLen - suffixLen, suffix) == 0;
}

// ============================================================================
// Override List Management
// ============================================================================

void InitOverrideList(OverrideList& list) {
    list.entries = nullptr;
    list.count = 0;
    list.capacity = 0;
    list.totalAlignedSize = 0;
}

void FreeOverrideList(OverrideList& list, EGG::Heap* heap) {
    if (list.entries != nullptr) {
        EGG::Heap::free(list.entries, heap);
        list.entries = nullptr;
    }
    list.count = 0;
    list.capacity = 0;
    list.totalAlignedSize = 0;
}

static bool AddOverrideEntry(OverrideList& list, const char* fullPath, const char* relativePath, 
                             u32 fileSize, EGG::Heap* heap) {
    if (list.count >= MAX_OVERRIDES) {
        OS::Report("[Pulsar] Override limit reached (%d), skipping: %s\n", MAX_OVERRIDES, relativePath);
        return false;
    }
    
    // Allocate/expand entries array if needed
    if (list.entries == nullptr) {
        list.capacity = 32;
        list.entries = static_cast<OverrideEntry*>(
            EGG::Heap::alloc(sizeof(OverrideEntry) * list.capacity, 0x20, heap));
        if (list.entries == nullptr) {
            OS::Report("[Pulsar] Failed to allocate override entry array\n");
            return false;
        }
    } else if (list.count >= list.capacity) {
        // Expand array
        u32 newCapacity = list.capacity * 2;
        if (newCapacity > MAX_OVERRIDES) newCapacity = MAX_OVERRIDES;
        OverrideEntry* newEntries = static_cast<OverrideEntry*>(
            EGG::Heap::alloc(sizeof(OverrideEntry) * newCapacity, 0x20, heap));
        if (newEntries == nullptr) {
            OS::Report("[Pulsar] Failed to expand override entry array\n");
            return false;
        }
        memcpy(newEntries, list.entries, sizeof(OverrideEntry) * list.count);
        EGG::Heap::free(list.entries, heap);
        list.entries = newEntries;
        list.capacity = newCapacity;
    }
    
    OverrideEntry& entry = list.entries[list.count];
    strncpy(entry.fullPath, fullPath, OVERRIDE_MAX_PATH - 1);
    entry.fullPath[OVERRIDE_MAX_PATH - 1] = '\0';
    strncpy(entry.relativePath, relativePath, OVERRIDE_MAX_PATH - 1);
    entry.relativePath[OVERRIDE_MAX_PATH - 1] = '\0';
    entry.size = fileSize;
    entry.alignedSize = nw4r::ut::RoundUp(fileSize, 0x20);
    
    list.totalAlignedSize += entry.alignedSize;
    list.count++;
    return true;
}

// ============================================================================
// Directory Scanning (IO-type specific)
// ============================================================================

// Forward declaration for recursive scanning
static void ScanDirectoryRecursive(const char* dirPath, const char* relativePath, 
                                   OverrideList& list, EGG::Heap* heap);

// RIIVO implementation
static void ScanDirectoryRiivo(const char* dirPath, const char* relativePath, 
                               OverrideList& list, EGG::Heap* heap) {
    s32 riivo_fd = IO::OpenFix("file", IOS::MODE_NONE);
    if (riivo_fd < 0) return;
    
    alignas(0x20) s32 folder_fd = IOS::IOCtl(riivo_fd, static_cast<IOS::IOCtlType>(RIIVO_IOCTL_OPENDIR),
                                              (void*)dirPath, strlen(dirPath) + 1, nullptr, 0);
    
    if (folder_fd < 0) {
        IOS::Close(riivo_fd);
        return;
    }
    
    alignas(0x20) IOS::IOCtlvRequest request[3];
    alignas(0x20) char fileName[riivoMaxPath];
    alignas(0x20) RiivoStats stats;
    
    while (list.count < MAX_OVERRIDES) {
        request[0].address = &folder_fd;
        request[0].size = 4;
        request[1].address = &fileName;
        request[1].size = riivoMaxPath;
        request[2].address = &stats;
        request[2].size = sizeof(RiivoStats);
        
        s32 ret = IOS::IOCtlv(riivo_fd, static_cast<IOS::IOCtlType>(RIIVO_IOCTL_NEXTDIR), 1, 2, request);
        if (ret != 0) break;
        
        // Skip . and ..
        if (fileName[0] == '.' && (fileName[1] == '\0' || 
            (fileName[1] == '.' && fileName[2] == '\0'))) {
            continue;
        }
        
        // Build full path and relative path
        char fullPath[OVERRIDE_MAX_PATH];
        char newRelPath[OVERRIDE_MAX_PATH];
        snprintf(fullPath, OVERRIDE_MAX_PATH, "%s/%s", dirPath, fileName);
        
        if (relativePath[0] != '\0') {
            snprintf(newRelPath, OVERRIDE_MAX_PATH, "%s/%s", relativePath, fileName);
        } else {
            strncpy(newRelPath, fileName, OVERRIDE_MAX_PATH - 1);
            newRelPath[OVERRIDE_MAX_PATH - 1] = '\0';
        }
        
        if ((stats.mode & S_IFDIR) == S_IFDIR) {
            // Recurse into directory
            ScanDirectoryRecursive(fullPath, newRelPath, list, heap);
        } else {
            // Add file entry
            AddOverrideEntry(list, fullPath, newRelPath, static_cast<u32>(stats.size), heap);
        }
    }
    
    IOS::IOCtl(riivo_fd, static_cast<IOS::IOCtlType>(RIIVO_IOCTL_CLOSEDIR), 
               (void*)&folder_fd, sizeof(s32), nullptr, 0);
    IOS::Close(riivo_fd);
}

// SD implementation - declare sd_vtable extern
extern const struct sd_vtable* __sd_vtable;

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

#define SD_S_IFDIR 0040000
#define SD_S_IFMT 0170000

static void ScanDirectorySD(const char* dirPath, const char* relativePath, 
                            OverrideList& list, EGG::Heap* heap) {
    if (__sd_vtable == nullptr) return;
    
    dir_struct dirData;
    if (__sd_vtable->diropen(&dirData, dirPath) != 0) return;
    
    char fileName[768];  // SD_MAX_FILENAME_LENGTH
    stat statBuf;
    
    while (list.count < MAX_OVERRIDES) {
        if (__sd_vtable->dirnext(&dirData, fileName, &statBuf) != 0) break;
        
        // Skip . and ..
        if (fileName[0] == '.' && (fileName[1] == '\0' || 
            (fileName[1] == '.' && fileName[2] == '\0'))) {
            continue;
        }
        
        // Build paths
        char fullPath[OVERRIDE_MAX_PATH];
        char newRelPath[OVERRIDE_MAX_PATH];
        snprintf(fullPath, OVERRIDE_MAX_PATH, "%s/%s", dirPath, fileName);
        
        if (relativePath[0] != '\0') {
            snprintf(newRelPath, OVERRIDE_MAX_PATH, "%s/%s", relativePath, fileName);
        } else {
            strncpy(newRelPath, fileName, OVERRIDE_MAX_PATH - 1);
            newRelPath[OVERRIDE_MAX_PATH - 1] = '\0';
        }
        
        if ((statBuf.st_mode & SD_S_IFMT) == SD_S_IFDIR) {
            // Recurse into directory
            ScanDirectoryRecursive(fullPath, newRelPath, list, heap);
        } else {
            // Get file size via stat
            AddOverrideEntry(list, fullPath, newRelPath, 0, heap);  // Size will be read later
        }
    }
    
    __sd_vtable->dirclose(&dirData);
}

static void ScanDirectoryRecursive(const char* dirPath, const char* relativePath, 
                                   OverrideList& list, EGG::Heap* heap) {
    IO* io = IO::sInstance;
    if (io == nullptr) return;
    
    switch (io->type) {
        case IOType_RIIVO:
            ScanDirectoryRiivo(dirPath, relativePath, list, heap);
            break;
        case IOType_SD:
            ScanDirectorySD(dirPath, relativePath, list, heap);
            break;
        default:
            // ISO/DOLPHIN/NAND not supported for now
            break;
    }
}

// ============================================================================
// Override Root Check
// ============================================================================

bool OverrideRootExists() {
    IO* io = IO::sInstance;
    if (io == nullptr) return false;
    return io->FolderExists(OVERRIDE_ROOT);
}

// ============================================================================
// Build Override List
// ============================================================================

// Parse archive tag from filename
// Example: "congratulations.brlyt.Award" -> tag is "Award", returns true
// If no tag, returns false
static bool ParseArchiveTag(const char* relativePath, const char* archiveBase, 
                            char* strippedPath, u32 strippedPathSize) {
    const char* basename = GetBasename(relativePath);
    if (basename == nullptr) return false;
    
    // Look for .<archiveBase> suffix
    char tagSuffix[64];
    snprintf(tagSuffix, sizeof(tagSuffix), ".%s", archiveBase);
    
    if (StrEndsWith(basename, tagSuffix)) {
        // Tagged for this archive - strip the suffix
        u32 baseLen = strlen(basename);
        u32 suffixLen = strlen(tagSuffix);
        
        // Copy relativePath without the tag suffix
        u32 relLen = strlen(relativePath);
        u32 copyLen = relLen - suffixLen;
        if (copyLen >= strippedPathSize) copyLen = strippedPathSize - 1;
        strncpy(strippedPath, relativePath, copyLen);
        strippedPath[copyLen] = '\0';
        return true;
    }
    
    // Check if it's tagged for a DIFFERENT archive
    // Look for pattern: filename ends with .<something> where <something> is not an extension
    const char* lastDot = StringFindLastChar(basename, '.');
    if (lastDot != nullptr) {
        // Check if what's after the dot looks like an archive name (capitalized, no extension chars)
        const char* afterDot = lastDot + 1;
        if (strlen(afterDot) > 0 && afterDot[0] >= 'A' && afterDot[0] <= 'Z') {
            // Likely tagged for another archive - exclude from this one
            // But we need to be careful: ".brlyt" is an extension, not a tag
            // Heuristic: if it matches known extensions, it's not a tag
            if (StrCaseCmp(afterDot, "brlyt") != 0 && 
                StrCaseCmp(afterDot, "brres") != 0 &&
                StrCaseCmp(afterDot, "brctr") != 0 &&
                StrCaseCmp(afterDot, "bmg") != 0 &&
                StrCaseCmp(afterDot, "tpl") != 0 &&
                StrCaseCmp(afterDot, "szs") != 0) {
                // This appears to be tagged for a different archive
                return false;
            }
        }
    }
    
    // Not tagged - copy path as-is
    strncpy(strippedPath, relativePath, strippedPathSize - 1);
    strippedPath[strippedPathSize - 1] = '\0';
    return true;  // Include in override list (untagged = applies to all)
}

bool BuildOverrideList(const char* archiveBaseName, OverrideList& out, EGG::Heap* heap) {
    InitOverrideList(out);
    
    if (!OverrideRootExists()) {
        return false;
    }
    
    // First, scan the entire /files/Mods directory
    OverrideList allFiles;
    InitOverrideList(allFiles);
    ScanDirectoryRecursive(OVERRIDE_ROOT, "", allFiles, heap);
    
    if (allFiles.count == 0) {
        return false;
    }
    
    // Filter overrides that apply to this archive
    for (u32 i = 0; i < allFiles.count; i++) {
        const OverrideEntry& entry = allFiles.entries[i];
        char strippedPath[OVERRIDE_MAX_PATH];
        
        if (ParseArchiveTag(entry.relativePath, archiveBaseName, strippedPath, OVERRIDE_MAX_PATH)) {
            // This override applies to the current archive
            // Re-add with the stripped internal path
            OverrideEntry filtered;
            strncpy(filtered.fullPath, entry.fullPath, OVERRIDE_MAX_PATH);
            strncpy(filtered.relativePath, strippedPath, OVERRIDE_MAX_PATH);
            filtered.size = entry.size;
            filtered.alignedSize = entry.alignedSize;
            
            AddOverrideEntry(out, filtered.fullPath, filtered.relativePath, filtered.size, heap);
        }
    }
    
    FreeOverrideList(allFiles, heap);
    
    if (out.count > 0) {
        OS::Report("[Pulsar] Found %d override(s) for archive '%s'\n", out.count, archiveBaseName);
    }
    
    return out.count > 0;
}

// ============================================================================
// Whole-File Override Check
// ============================================================================

static char sOverridePathBuffer[OVERRIDE_MAX_PATH];

const char* CheckWholeFileOverride(const char* originalPath) {
    if (originalPath == nullptr) return nullptr;
    
    IO* io = IO::sInstance;
    if (io == nullptr) return nullptr;
    
    const char* basename = GetBasename(originalPath);
    if (basename == nullptr) return nullptr;
    
    snprintf(sOverridePathBuffer, OVERRIDE_MAX_PATH, "%s/%s", OVERRIDE_ROOT, basename);
    
    // Check if file exists - try to open it
    if (io->OpenFile(sOverridePathBuffer, FILE_MODE_READ)) {
        io->Close();
        OS::Report("[Pulsar] Whole-file override: %s -> %s\n", originalPath, sOverridePathBuffer);
        return sOverridePathBuffer;
    }
    
    return nullptr;
}

// ============================================================================
// U8 Archive Patching
// ============================================================================

// Find matching U8 node by filename (for filename-only matches)
static s32 FindNodeByFilename(U8Node* nodes, u32 nodeCount, const char* stringTable, 
                               const char* filename, u32 startIdx = 1) {
    for (u32 i = startIdx; i < nodeCount; i++) {
        if (nodes[i].IsDirectory()) continue;
        
        const char* nodeName = stringTable + nodes[i].GetNameOffset();
        if (StrCaseCmp(nodeName, filename) == 0) {
            return static_cast<s32>(i);
        }
    }
    return -1;
}

// Find matching U8 node by full internal path
static s32 FindNodeByPath(void* archiveBase, const char* internalPath) {
    // Use ARC functions for path-based lookup
    ARC::Handle handle;
    if (!ARC::InitHandle(archiveBase, &handle)) {
        return -1;
    }
    
    return ARC::ConvertPathToEntrynum(&handle, internalPath);
}

void ApplyU8Overrides(void* archiveBase, u32 baseSize, const OverrideList& list, EGG::Heap* heap) {
    if (archiveBase == nullptr || list.count == 0) return;
    
    U8Header* header = static_cast<U8Header*>(archiveBase);
    
    // Verify U8 magic
    if (header->magic != 0x55AA382D) {
        OS::Report("[Pulsar] Invalid U8 magic: 0x%08X\n", header->magic);
        return;
    }
    
    // Get node table and string table
    U8Node* nodes = reinterpret_cast<U8Node*>(static_cast<u8*>(archiveBase) + header->nodeOffset);
    u32 nodeCount = nodes[0].dataSize;  // Root node's dataSize is total node count
    const char* stringTable = reinterpret_cast<const char*>(nodes + nodeCount);
    
    // Write offset starts after original archive data
    u32 writeOffset = 0;
    
    IO* io = IO::sInstance;
    if (io == nullptr) return;
    
    for (u32 i = 0; i < list.count; i++) {
        const OverrideEntry& entry = list.entries[i];
        
        // Determine if this is a path match or filename match
        bool hasPath = (strchr(entry.relativePath, '/') != nullptr);
        
        s32 nodeIdx = -1;
        
        if (hasPath) {
            // Full path match - try to find exact internal path
            // The internal path might need '/' prepended
            char internalPath[OVERRIDE_MAX_PATH];
            if (entry.relativePath[0] != '/') {
                snprintf(internalPath, OVERRIDE_MAX_PATH, "/%s", entry.relativePath);
            } else {
                strncpy(internalPath, entry.relativePath, OVERRIDE_MAX_PATH);
            }
            nodeIdx = FindNodeByPath(archiveBase, internalPath);
        } else {
            // Filename-only match - find ALL matching nodes
            // For now, just find first match (can be extended for multiple)
            nodeIdx = FindNodeByFilename(nodes, nodeCount, stringTable, entry.relativePath);
        }
        
        if (nodeIdx < 0) {
            OS::Report("[Pulsar] No match for override: %s\n", entry.relativePath);
            continue;
        }
        
        // Read override file into buffer
        if (!io->OpenFile(entry.fullPath, FILE_MODE_READ)) {
            OS::Report("[Pulsar] Failed to open override: %s\n", entry.fullPath);
            continue;
        }
        
        u32 fileSize = io->GetFileSize();
        if (fileSize == 0) {
            io->Close();
            continue;
        }
        
        // Write data to position after original archive
        u8* writePos = static_cast<u8*>(archiveBase) + baseSize + writeOffset;
        io->Read(fileSize, writePos);
        io->Close();
        
        // Patch node entry
        U8Node& node = nodes[nodeIdx];
        u32 newOffset = baseSize + writeOffset;
        
        // Convert to big-endian offset from archive start
        node.dataOffset = newOffset;
        node.dataSize = fileSize;
        
        OS::Report("[Pulsar] Patched node %d: offset=0x%X, size=0x%X (%s)\n", 
                   nodeIdx, newOffset, fileSize, entry.relativePath);
        
        // Advance write offset (aligned)
        writeOffset += nw4r::ut::RoundUp(fileSize, 0x20);
    }
    
    // Flush cache for modified regions
    OS::DCStoreRange(nodes, sizeof(U8Node) * nodeCount);  // Node table
    if (writeOffset > 0) {
        OS::DCStoreRange(static_cast<u8*>(archiveBase) + baseSize, writeOffset);  // Appended data
    }
}

// ============================================================================
// Whole-File Override Hooks
// ============================================================================

// Hook for ArchiveFile::Dump to redirect path for whole-file overrides
// Original: void Dump(const char* path, EGG::Heap* heap, s32 allocDirection); // 805190f0
static void DumpWithOverride(ArchiveFile* file, const char* path, EGG::Heap* heap, s32 allocDirection) {
    const char* overridePath = CheckWholeFileOverride(path);
    if (overridePath != nullptr) {
        // Call original Dump with override path
        file->TryDump(overridePath, heap, allocDirection);
        if (file->compressedArchive != nullptr) {
            file->status = ARCHIVE_STATUS_DUMPED;
            return;
        }
        // Override failed, fall back to original
        OS::Report("[Pulsar] Whole-file override failed, using original: %s\n", path);
    }
    
    // Call original behavior
    file->TryDump(path, heap, allocDirection);
    if (file->compressedArchive != nullptr) {
        file->status = ARCHIVE_STATUS_DUMPED;
    }
}
kmBranch(0x805190f0, DumpWithOverride);

}  // namespace Pulsar

