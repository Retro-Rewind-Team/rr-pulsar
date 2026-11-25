#ifndef _PULSAR_RATING_MULTI_
#define _PULSAR_RATING_MULTI_

#include <types.hpp>
#include <core/GS/GP/GPTypes.hpp>
#include <core/rvl/DWC/DWCLogin.hpp>

namespace Pulsar {
namespace Network {

#ifdef BETA
const char multiplierUrl[] = "http://rwfc.net/api/multiplier?channel=beta";
#else
const char multiplierUrl[] = "http://rwfc.net/api/multiplier?channel=stable";
#endif

extern float ratingMultiplier;
extern u32 loginGeneration;
extern DWC::LoginCallback originalLoginCallback;

struct MultiplierRequest {
    char buffer[0x1000];
    u32 generation;
};

float ApplyRatingMultiplier(float change);

}  // namespace Network
}  // namespace Pulsar

#endif
