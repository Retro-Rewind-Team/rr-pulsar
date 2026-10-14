#include <MarioKartWii/Input/InputManager.hpp>
#include <MarioKartWii/Race/RaceInfo/RaceInfo.hpp>
#include <Settings/Settings.hpp>

namespace Pulsar {
namespace Race {

static bool autoAccelerating[4];
static bool accelerateHeld[4];
static void UpdateAutoAcceleration(Input::RealControllerHolder &holder, bool isPaused) {
    holder.Input::ControllerHolder::Update(isPaused);

    const u32 slot = &holder - Input::Manager::sInstance->realControllerHolders;
    const bool pressed = (holder.curController->inputState.buttonActions & 0x1) != 0;
    if (Raceinfo::sInstance == nullptr || Raceinfo::sInstance->stage != RACESTAGE_RACE || holder.blockInputs || Racedata::sInstance->racesScenario.settings.gamemode == MODE_TIME_TRIAL
      || System::sInstance->IsContext(PULSAR_MODE_OTT) || Settings::Mgr::Get().GetSettingValue(Settings::SETTING_AUTOACCELERATION) == 0) {
        autoAccelerating[slot] = false;
    } else if (!isPaused) {
        if (pressed && !accelerateHeld[slot])
            autoAccelerating[slot] = !autoAccelerating[slot];
        if (autoAccelerating[slot]) {
            holder.inputStates[0].buttonActions |= 0x1;
            // Controller mappings require physical acceleration before they emit hop/drift.
            if ((holder.inputStates[0].buttonActions & 0x2) != 0 && ((holder.inputStates[1].buttonActions & 0x2) == 0 || (holder.inputStates[1].buttonActions & 0x8) != 0))
                holder.inputStates[0].buttonActions |= 0x8;
        }
    }
    accelerateHeld[slot] = pressed;
}
kmCall(0x80521784, UpdateAutoAcceleration);

}  // namespace Race
}  // namespace Pulsar
