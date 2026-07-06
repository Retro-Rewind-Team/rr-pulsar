#include <kamek.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>

namespace Pulsar {
namespace Network {

#ifdef PHANTOM_RACER_REPRO

static u8 GetUnusedAid(u32 availableAids, u8 localAid) {
    for (u8 aid = 0; aid < 12; ++aid) {
        if (aid == localAid) continue;
        if ((availableAids & (1 << aid)) == 0) return aid;
    }
    return 0xff;
}

static int FindRemotePeerPlayerId(const RKNet::Controller& controller, const RKNet::ControllerSub& sub) {
    for (int playerId = 11; playerId >= 0; --playerId) {
        const u8 aid = controller.aidsBelongingToPlayerIds[playerId];
        if (aid < 12 && aid != sub.localAid && aid != sub.hostAid) return playerId;
    }
    return -1;
}

static int FindHostPlayerId(const RKNet::Controller& controller, const RKNet::ControllerSub& sub) {
    for (int playerId = 11; playerId >= 0; --playerId) {
        if (controller.aidsBelongingToPlayerIds[playerId] == sub.hostAid) return playerId;
    }
    return -1;
}

static void UpdateAidMapWithLocalPhantomRepro(RKNet::Controller* controller) {
    if (controller == nullptr) return;
    controller->UpdateAidsBelongingToPlayerIds();

    const RKNet::ControllerSub& sub = controller->subs[controller->currentSub];
    if (sub.localAid == sub.hostAid) return;
    if (sub.playerCount < 2) return;

    const u8 unusedAid = GetUnusedAid(sub.availableAids, sub.localAid);
    if (unusedAid >= 12) return;

    int playerId = FindRemotePeerPlayerId(*controller, sub);
    if (playerId < 0) playerId = FindHostPlayerId(*controller, sub);
    if (playerId < 0) return;

    controller->aidsBelongingToPlayerIds[playerId] = unusedAid;
}
kmCall(0x80650e48, UpdateAidMapWithLocalPhantomRepro);

#endif

}  // namespace Network
}  // namespace Pulsar
