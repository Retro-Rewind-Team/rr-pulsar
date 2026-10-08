#ifndef _PULSAR_TTPRACTICE_
#define _PULSAR_TTPRACTICE_

#include <kamek.hpp>
#include <MarioKartWii/System/Identifiers.hpp>

namespace Pulsar {
namespace TTPractice {

void SetPracticeMode(bool enabled);
bool IsPracticeMode();
ItemId GetStartingItem(u32 hudSlotId);
bool IsEnabled();
bool AreItemBoxesEnabled();
bool IsObjectFreezeEnabled();

}  // namespace TTPractice
}  // namespace Pulsar

#endif
