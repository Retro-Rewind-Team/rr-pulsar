#ifndef _PULSAR_PLAYER_RATING_
#define _PULSAR_PLAYER_RATING_

#include <types.hpp>

namespace Pulsar {
namespace Network {

u32 GetPlayerRating(u8 aid, bool isBattle, u32 fallback);

}  // namespace Network
}  // namespace Pulsar

#endif
