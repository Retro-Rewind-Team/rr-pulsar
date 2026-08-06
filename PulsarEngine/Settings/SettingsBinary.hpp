#ifndef _SETTINGS_BINARY_
#define _SETTINGS_BINARY_
#include <kamek.hpp>
#include <PulsarSystem.hpp>
#include <Config.hpp>

namespace Pulsar {
namespace Settings {

enum SectionIndexes {
    SECTION_SETTINGS,
    SECTION_MISC,
    SECTION_TROPHIES,
    SECTION_GP
};

struct TrackTrophy {
    u32 crc32;
    bool hastrophy[4];
};  // 0x8

struct SettingsHolder {
    static const u32 magic = 'SETT';
    static const u32 version = 1;
    static const u32 index = SECTION_SETTINGS;
    static const u32 valueCapacity = (SETTING_COUNT + 3) & ~3;
    Pulsar::SectionHeader header;
    u8 values[valueCapacity];
};
static_assert(sizeof(SettingsHolder) == 0x4c, "SettingsHolder layout changed");

struct MiscParams {
    static const u32 miscMagic = 'MISC';
    static const u32 version = 1;
    static const u32 index = SECTION_MISC;
    Pulsar::SectionHeader header;
    u32 customItemsBitfield;
    u32 reserved[18];  // 0xc
    u8 rankingBadge;  // 0x58, 0 is the normal ranking badge
    u8 rankingBadgePadding[3];
    PulsarCupId lastSelectedCup;  // 0x5c
    u32 trackCount;  // 0x60
};
static_assert(sizeof(MiscParams) == 0x64, "MiscParams layout changed");

struct TrophiesHolder {
    static const u32 tropMagic = 'TROP';
    static const u32 version = 1;
    static const u32 index = SECTION_TROPHIES;
    Pulsar::SectionHeader header;
    u16 trophyCount[4];
    TrackTrophy trophies[1];
};

struct GPCupStatus {
    u8 gpCCStatus[4];  // one per CC
};
struct GPSection {
    static const u32 gpMagic = 'GP\0\0';
    static const u32 version = 1;
    static const u32 index = SECTION_GP;

    // 2 bits for trophy
    // 4 bits for rank, 0 to 7
    // 3 2 1 a b c d e

    Pulsar::SectionHeader header;
    GPCupStatus gpStatus[1];  // one per cup
};

struct BinaryHeader {
    u32 magic;
    u32 version;
    u32 fileSize;
    u32 sectionCount;
    u32 offsets[1];  // 0x10
};

class alignas(0x20) Binary {
    static const u32 binMagic = 'PULP';
    static const u32 sectionCount = 4;
    static const u32 curVersion = 7;

    Binary(u32 trackCount);

    template <typename T>
    inline T& GetSection() {
        return *reinterpret_cast<T*>(ut::AddU32ToPtr(this, this->header.offsets[T::index]));
    }
    template <typename T>
    inline const T& GetSection() const {
        return *reinterpret_cast<const T*>(ut::AddU32ToPtr(this, this->header.offsets[T::index]));
    }
    template <class T>
    bool CheckSection(const T& t) {
        if (t.header.magic != T::magic) return false;
        return true;
    }

    BinaryHeader header;
    SettingsHolder settings;
    friend class Mgr;
};

}  // namespace Settings
}  // namespace Pulsar

#endif
