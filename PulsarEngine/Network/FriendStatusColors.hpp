#ifndef _FRIENDSTATUSCOLORS_
#define _FRIENDSTATUSCOLORS_

#include <MarioKartWii/UI/Page/Other/Globe.hpp>

namespace Pulsar {
namespace Network {

ut::Color GetFriendColor(u32 friendIdx);
void SetGlobeMsgColor(Pages::Globe::MessageWindow &msg, ut::Color color);

}  // namespace Network
}  // namespace Pulsar

#endif