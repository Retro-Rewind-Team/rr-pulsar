#ifndef _LOOSE_ARCHIVE_OVERRIDES_
#define _LOOSE_ARCHIVE_OVERRIDES_

#include <kamek.hpp>
#include <core/egg/mem/Heap.hpp>
#include <core/rvl/arc/arc.hpp>

namespace Pulsar {

// ============================================================================
// Constants
// ============================================================================

static const u32 OVERRIDE_MAX_PATH = 256;
static const u32 MAX_OVERRIDES = 256;
static const char* const OVERRIDE_ROOT = "/files/Mods";

// ============================================================================
// Data Structures
// ============================================================================

// Single override entry representing a file in /files/Mods
struct OverrideEntry {
    char fullPath[OVERRIDE_MAX_PATH];      // e.g., /files/Mods/blyt/file.brlyt.Award
    char relativePath[OVERRIDE_MAX_PATH];  // e.g., blyt/file.brlyt.Award
    u32 size;                              // File size in bytes
    u32 alignedSize;                       // RoundUp(size, 0x20)
};

// Collection of override entries for an archive
struct OverrideList {
    OverrideEntry* entries;
    u32 count;
    u32 capacity;
    u32 totalAlignedSize;  // Sum of all alignedSize values
};

// U8 archive node with explicit byte layout (avoids bitfield issues)
// Matches U8 file format: https://wiki.tockdom.com/wiki/U8_(File_Format)
struct U8Node {
    u8 type;              // 0 = file, 1 = directory
    u8 nameOffsetHi;      // High byte of 24-bit name offset
    u16 nameOffsetLo;     // Low 16 bits of name offset (big-endian)
    u32 dataOffset;       // File: offset from archive start; Dir: parent index
    u32 dataSize;         // File: file size; Dir: index of first node not in dir
    
    inline u32 GetNameOffset() const {
        return (static_cast<u32>(nameOffsetHi) << 16) | nameOffsetLo;
    }
    
    inline bool IsDirectory() const {
        return type != 0;
    }
};

// U8 archive header
struct U8Header {
    u32 magic;            // 0x55AA382D ('U.8-')
    u32 nodeOffset;       // Offset to first node (always 0x20)
    u32 nodeTableSize;    // Size of node table + string table
    u32 dataOffset;       // Offset to file data
    u8 reserved[16];
};

// ============================================================================
// API Functions
// ============================================================================

// Check if the override root directory exists
bool OverrideRootExists();

// Initialize an empty override list
void InitOverrideList(OverrideList& list);

// Build override list for a specific archive
// archiveBaseName: e.g., "Award" from "/UI/Award.szs"
// Returns true if overrides were found
bool BuildOverrideList(const char* archiveBaseName, OverrideList& out, EGG::Heap* heap);

// Free memory allocated for override list
void FreeOverrideList(OverrideList& list, EGG::Heap* heap);

// Apply U8 overrides to a decompressed archive buffer
// archiveBase: pointer to decompressed U8 data
// baseSize: original decompressed size (override data goes after this)
// list: overrides to apply
// heap: heap for temporary allocations (file reading)
void ApplyU8Overrides(void* archiveBase, u32 baseSize, const OverrideList& list, EGG::Heap* heap);

// Check if a whole-file override exists for the given path
// Returns override path if exists, nullptr otherwise
// Uses static buffer - not thread-safe
const char* CheckWholeFileOverride(const char* originalPath);

// Extract archive base name from path (without extension)
// e.g., "/UI/Award.szs" -> "Award"
void ExtractArchiveBaseName(const char* path, char* outName, u32 outSize);

// Find last occurrence of character in string (replacement for strrchr)
const char* StringFindLastChar(const char* str, char c);

// Extract filename (basename) from a path
// e.g., "/UI/Award.szs" -> "Award.szs"
const char* GetBasename(const char* path);

}  // namespace Pulsar

#endif

