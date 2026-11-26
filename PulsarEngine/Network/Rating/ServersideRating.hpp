#ifndef _PULSAR_SERVERSIDE_RATING_
#define _PULSAR_SERVERSIDE_RATING_

#include <types.hpp>

namespace Pulsar {
namespace Network {

struct RatingResponse {
    volatile int status;
    u32 vr;
    u32 br;
};

struct RatingRequest {
    char buffer[0x400];
    RatingResponse *response;
};

void StoreRatingUponConnection(int profileId);

}  // namespace Network
}  // namespace Pulsar

#endif
