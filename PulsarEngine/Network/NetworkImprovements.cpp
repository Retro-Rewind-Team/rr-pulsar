#include <kamek.hpp>
#include <runtimeWrite.hpp>
#include <Network/WiiLink.hpp>

namespace Pulsar {
namespace Network {

kmWrite32(0x8011b6f4, 0x3803012C);  // li r0, 300
kmWrite32(0x8011b998, 0x388300C8);  // addi r4, r3, 200
kmWrite32(0x8011b99c, 0x3860000F);  // li r3, 15

kmWrite32(0x80657ea8, 0x28040009);
kmWrite32(0x8010a6c4, 0x28000190);  // cmplwi r0, 400
kmWrite32(0x8010a720, 0x2800001E);  // cmplwi r0, 30

}  // namespace Network
}  // namespace Pulsar