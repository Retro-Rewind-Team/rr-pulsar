#ifndef _CUSTOMCHARACTERS_
#define _CUSTOMCHARACTERS_

#include <kamek.hpp>
#include <MarioKartWii/System/Identifiers.hpp>

namespace Pulsar {
namespace Driver {

enum {
    CHARACTER_COUNT = 0x18,
    MAX_CUSTOM_CHARACTER_SLOTS = 100
};

extern bool characterTables[CHARACTER_COUNT][MAX_CUSTOM_CHARACTER_SLOTS + 1];
extern u8 selectedSlots[CHARACTER_COUNT];

void CreateCharacterTable();
bool LoadDriverBRRES(CharacterId character, u32 slot);

}  // namespace Driver
}  // namespace Pulsar

#endif
