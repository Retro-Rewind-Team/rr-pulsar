#ifndef _KARTACTION_
#define _KARTACTION_

#include <MarioKartWii/Kart/KartLink.hpp>

namespace Kart {

class Action : public Link {
   public:
    void StartAction3(u32 playerObjIdx);  // 80568718 star hit
    void StartAction6(u32 playerObjIdx);  // 80569024 bullet hit
    void StartAction13(u32 playerObjIdx);  // 80569818 mega hit
};

}  // namespace Kart

#endif
