#include <kamek.hpp>
#include <MarioKartWii/Driver/DriverController.hpp>
#include <MarioKartWii/Kart/KartBRRESHandle.hpp>
#include <MarioKartWii/Kart/KartMovement.hpp>
#include <MarioKartWii/Kart/KartPointers.hpp>
#include <MarioKartWii/Kart/KartPhysics.hpp>
#include <MarioKartWii/Race/RaceInfo/RaceInfo.hpp>

using nw4r::g3d::ResFile;

namespace Pulsar {
namespace Race {

enum CustomAnimation { SHOCK_HIT,
                       STAR_USE,
                       MEGA_USE,
                       WAIT_BEFORE_START,
                       SHOCK_DODGE_STAR,
                       ANIMATION_COUNT };
enum AnimationType { MODEL_ANIMATION,
                     TEXTURE_ANIMATION,
                     TEXTURE_PATTERN_ANIMATION,
                     TYPE_COUNT };

const char *const animationNames[ANIMATION_COUNT] = {
    "shockHit", "starUse", "megaUse", "waitBeforeStart", "shockDodgeStar"};

struct CustomAnimations {
    DriverController *model;
    u16 ids[ANIMATION_COUNT][TYPE_COUNT];
    u16 lengths[ANIMATION_COUNT];
    u16 dodgeFrames;
    u16 frames;
    u8 trigger;
    u8 active;
    u8 used;
    u8 arm;
    u8 leg;
};

CustomAnimations animations[12];
void LoadCustomAnimations(DriverController *model, ModelDirector **primary, ModelDirector **lod,
                          Kart::BRRESHandle &handle, bool translucent) {
    model->CreateModelDirectors(primary, lod, handle, translucent);
    ResFile &file = handle.brres;
    u8 playerId = model->GetPlayerIdx();
    CustomAnimations &custom = animations[playerId];
    custom.model = nullptr;
    custom.dodgeFrames = 0;
    custom.frames = 0;
    custom.trigger = 0;
    custom.active = 0;
    custom.used = 0;
    for (int animation = 0; animation < ANIMATION_COUNT; ++animation) {
        custom.lengths[animation] = 0;
        for (int type = 0; type < TYPE_COUNT; ++type) custom.ids[animation][type] = 0xffff;
    }
    if (model->isCpu) return;  // CPU models have no driver animation list.

    custom.model = model;
    ModelTransformator *manager = model->driverModel->modelTransformator;

    for (int animation = 0; animation < ANIMATION_COUNT; ++animation) {
        const char *name = animationNames[animation];
        for (int type = 0; type < TYPE_COUNT; ++type) {
            bool exists = type == MODEL_ANIMATION ? file.GetResAnmChr(name).data != nullptr : type == TEXTURE_ANIMATION ? file.GetResAnmTexSrt(name).data != nullptr
                                                                                                                        : file.GetResAnmTexPat(name).data != nullptr;
            if (!exists) continue;

            // The original loader appends all 41 game animations and their PAT0/SRT0 tracks first.
            u16 id = manager->anmHolderList.count;
            (*primary)->LinkAnimation(id, file, name, type == MODEL_ANIMATION ? ANMTYPE_CHR : type == TEXTURE_ANIMATION ? ANMTYPE_TEXSRT
                                                                                                                        : ANMTYPE_TEXPAT,
                                      type == MODEL_ANIMATION, nullptr, ARCHIVE_HOLDER_KART, 0);
            custom.ids[animation][type] = id;
            u16 length = type == MODEL_ANIMATION ? file.GetResAnmChr(name).data->fileInfo.frameCount : type == TEXTURE_ANIMATION ? file.GetResAnmTexSrt(name).data->fileInfo.frameCount
                                                                                                                                 : file.GetResAnmTexPat(name).data->fileInfo.frameCount;
            if (length > custom.lengths[animation]) custom.lengths[animation] = length;
        }
    }
}
kmCall(0x805778dc, LoadCustomAnimations);

void UpdateCustomAnimations(DriverController *model) {
    u8 playerId = model->GetPlayerIdx();
    CustomAnimations &custom = animations[playerId];
    u8 *bytes = reinterpret_cast<u8 *>(model);
    bool customPose = custom.model == model && custom.active && custom.frames &&
                      custom.ids[custom.active - 1][MODEL_ANIMATION] != 0xffff;
    u8 ik = bytes[0x144];
    u8 head = bytes[0x147];
    if (customPose) {
        // The vanilla IK pass moves hands and feet to the kart after CHR0 supplies their pose.
        bytes[0x144] = 0;
        bytes[0x147] = 0;
    }
    model->Update();
    if (customPose) {
        bytes[0x144] = ik;
        bytes[0x147] = head;
    }
    if (custom.model != model) return;

    Kart::Status *status = model->pointers->kartStatus;
    if (!(status->bitfield2 & 0x80)) custom.used &= ~(1 << SHOCK_HIT);
    if (!(status->bitfield1 & 0x80000000)) custom.used &= ~(1 << STAR_USE);
    if (!(status->bitfield2 & 0x8000)) custom.used &= ~(1 << MEGA_USE);
    if (Raceinfo::sInstance->stage > RACESTAGE_COUNTDOWN)
        custom.used &= ~(1 << WAIT_BEFORE_START);
    u8 trigger = 0;
    if (status->bitfield2 & 0x80)
        trigger = SHOCK_HIT + 1;
    else if (custom.dodgeFrames)
        trigger = SHOCK_DODGE_STAR + 1;
    else if (status->bitfield1 & 0x80000000)
        trigger = STAR_USE + 1;
    else if (status->bitfield2 & 0x8000)
        trigger = MEGA_USE + 1;
    else if (Raceinfo::sInstance->stage <= RACESTAGE_COUNTDOWN)
        trigger = WAIT_BEFORE_START + 1;
    if (trigger && custom.ids[trigger - 1][MODEL_ANIMATION] == 0xffff &&
        custom.ids[trigger - 1][TEXTURE_ANIMATION] == 0xffff &&
        custom.ids[trigger - 1][TEXTURE_PATTERN_ANIMATION] == 0xffff) trigger = 0;

    ModelTransformator *manager = model->driverModel->modelTransformator;
    if (custom.active && (custom.active != trigger || !custom.frames)) {
        bytes[0x14a] = custom.arm;
        bytes[0x14b] = custom.leg;
        u16 current = model->currentAnimation;
        if (current < 41) {
            if (custom.ids[custom.active - 1][MODEL_ANIMATION] != 0xffff)
                manager->PlayAnmNoBlend(current, 0.0f, 1.0f);
            u8 pat = bytes[0x175 + current];
            u8 srt = bytes[0x19e + current];
            if (custom.ids[custom.active - 1][TEXTURE_PATTERN_ANIMATION] != 0xffff) {
                if (pat != 0xff)
                    manager->PlayAnmNoBlend(pat, 0.0f, 1.0f);
                else
                    manager->DetachHolderFromParent(ANMTYPE_TEXPAT);
            }
            if (custom.ids[custom.active - 1][TEXTURE_ANIMATION] != 0xffff) {
                if (srt != 0xff)
                    manager->PlayAnmNoBlend(srt, 0.0f, 1.0f);
                else
                    manager->DetachHolderFromParent(ANMTYPE_TEXSRT);
            }
        }
        custom.active = 0;
    }

    if (trigger && trigger != custom.trigger &&
        (trigger == SHOCK_DODGE_STAR + 1 || trigger == STAR_USE + 1 ||
         trigger == MEGA_USE + 1 || !(custom.used & (1 << (trigger - 1))))) {
        custom.active = trigger;
        custom.frames = custom.lengths[trigger - 1];
        custom.used |= 1 << (trigger - 1);
        // The driver's bone callback uses these flags to attach arms and legs to the vehicle.
        custom.arm = bytes[0x14a];
        custom.leg = bytes[0x14b];
        bytes[0x14a] = 0;
        bytes[0x14b] = 0;
    }
    if (custom.active && custom.frames) {
        for (int type = 0; type < TYPE_COUNT; ++type) {
            u16 id = custom.ids[custom.active - 1][type];
            int managerType = type == MODEL_ANIMATION ? 0 : type == TEXTURE_ANIMATION ? 2
                                                                                      : 3;
            AnmHolder *node = manager->activeAnms[managerType];
            if (id == 0xffff) continue;
            float frame = custom.lengths[custom.active - 1] - custom.frames;
            if (trigger != custom.trigger || !node || node->idx != id ||
                custom.frames == custom.lengths[custom.active - 1] ||
                (manager->bitfieldActiveAnmTypes & (1 << managerType)))
                manager->PlayAnmNoBlend(id, frame, 1.0f);
            else {
                node->SetFrame(frame);
                node->SetUpdateRate(1.0f);
            }
        }
        --custom.frames;
        if (!custom.frames && (custom.active == STAR_USE + 1 || custom.active == MEGA_USE + 1))
            custom.frames = custom.lengths[custom.active - 1];
    }
    custom.trigger = trigger;
    if (custom.dodgeFrames) --custom.dodgeFrames;
}
kmCall(0x8058eee8, UpdateCustomAnimations);

bool PlayShockDodgeStarAnimation(Kart::Movement *movement, int timer, int param3, int param4) {
    bool starActive = (movement->pointers->kartStatus->bitfield1 & 0x80000000) != 0;
    bool hit = movement->ApplyLightningEffect(timer, param3, param4);
    if (starActive && !hit) {
        CustomAnimations &custom = animations[movement->GetPlayerIdx()];
        if (custom.model && (custom.ids[SHOCK_DODGE_STAR][MODEL_ANIMATION] != 0xffff || custom.ids[SHOCK_DODGE_STAR][TEXTURE_ANIMATION] != 0xffff || custom.ids[SHOCK_DODGE_STAR][TEXTURE_PATTERN_ANIMATION] != 0xffff))
            custom.dodgeFrames = custom.lengths[SHOCK_DODGE_STAR];
    }
    return hit;
}
kmCall(0x805804b4, PlayShockDodgeStarAnimation);
kmCall(0x80580504, PlayShockDodgeStarAnimation);
kmCall(0x80580584, PlayShockDodgeStarAnimation);
kmCall(0x805805d4, PlayShockDodgeStarAnimation);
kmCall(0x805806fc, PlayShockDodgeStarAnimation);
kmCall(0x8058074c, PlayShockDodgeStarAnimation);

void HideShockSpin(Kart::PhysicsHolder *physics, Quat *rotation) {
    Kart::Link *action = reinterpret_cast<Kart::Link *>(reinterpret_cast<u8 *>(rotation) - 0xb0);
    u32 actionId = *reinterpret_cast<u32 *>(reinterpret_cast<u8 *>(action) + 0x1c);
    CustomAnimations &custom = animations[action->GetPlayerIdx()];
    if ((actionId == 10 || actionId == 15 || actionId == 17) && custom.model && (custom.ids[SHOCK_HIT][MODEL_ANIMATION] != 0xffff || custom.ids[SHOCK_HIT][TEXTURE_ANIMATION] != 0xffff || custom.ids[SHOCK_HIT][TEXTURE_PATTERN_ANIMATION] != 0xffff)) {
        Quat identity;
        identity.Set(1.0f, 0.0f, 0.0f, 0.0f);
        physics->AddInstantaneousExtraRot(identity);
    } else
        physics->AddInstantaneousExtraRot(*rotation);
}
kmCall(0x8056835c, HideShockSpin);

}  // namespace Race
}  // namespace Pulsar