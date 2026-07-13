#ifndef _PULSAR_NETWORK_PHANTOMRACER_
#define _PULSAR_NETWORK_PHANTOMRACER_

#include <kamek.hpp>
#include <MarioKartWii/UI/Page/Other/SELECTStageMgr.hpp>

namespace Pulsar {
namespace Network {

void MarkPhantomAid(u32 aid);
void ClearPhantomAid(u32 aid);
void AppendPhantomSelectInfos(Pages::SELECTStageMgr& stageMgr);

}  // namespace Network
}  // namespace Pulsar

#endif
