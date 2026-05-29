/**
 * Direct port from mkw-sp Input Viewer
 *
 * Licensed under MIT. (See LICENSE_mkw-sp)
 *
 * Copyright 2021-2023 Pablo Stebler

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 *
 */

#ifndef _PULINPUTVIEWER_
#define _PULINPUTVIEWER_
#include <kamek.hpp>
#include <UI/CtrlRaceBase/CustomCtrlRaceBase.hpp>

// Displays the Input Viewer
namespace Pulsar {
namespace UI {
class CtrlRaceInputViewer : public CtrlRaceBase {
    enum DpadState {
        DpadState_Off,
        DpadState_Up,
        DpadState_Down,
        DpadState_Left,
        DpadState_Right,
        DpadState_Count  // Invalid
    };

    enum AccelState {
        AccelState_Off,
        AccelState_Pressed,
        AccelState_Count  // Invalid
    };

    enum Trigger {
        Trigger_L,
        Trigger_R,
        Trigger_BD,  // Brake drift
        Trigger_Count  // Invalid
    };

    enum TriggerState {
        TriggerState_Off,
        TriggerState_Pressed,
        TriggerState_Count  // Invalid
    };

   public:
    CtrlRaceInputViewer() {
        m_dpadState = DpadState_Off;
        m_dpadTimer = 0;
        m_accelState = AccelState_Off;
        m_triggerStates[Trigger_L] = TriggerState_Off;
        m_triggerStates[Trigger_R] = TriggerState_Off;
        m_triggerStates[Trigger_BD] = TriggerState_Off;
        m_stickState = Vec2(0.0f, 0.0f);
    }

    void Init() override;
    void OnUpdate() override;

    static u32 Count();
    static void Create(Page& page, u32 index, u32 count);

   private:
    void Load(const char* variant, u8 id);

    void setDpad(DpadState state);
    void setAccel(AccelState state);
    void setTrigger(Trigger trigger, TriggerState state);
    void setStick(Vec2 state);

   private:
    nw4r::lyt::Pane* m_dpadPanes[(int)DpadState_Count];
    nw4r::lyt::Pane* m_accelPanes[(int)AccelState_Count];
    nw4r::lyt::Pane* m_triggerPanes[(int)Trigger_Count][(int)TriggerState_Count];
    nw4r::lyt::Pane* m_stickPane;
    nw4r::math::VEC3 m_stickOrigin;

    DpadState m_dpadState;
    s8 m_dpadTimer;  // Keeps short D-pad taps visible for a few frames.
    AccelState m_accelState;
    TriggerState m_triggerStates[(int)Trigger_Count];
    Vec2 m_stickState;

    u32 m_playerId;

    static const s8 DPAD_HOLD_FOR_N_FRAMES;

   private:
    static const char* DpadStateToName(DpadState state) {
        switch (state) {
            case DpadState_Up:
                return "Up";
            case DpadState_Down:
                return "Down";
            case DpadState_Left:
                return "Left";
            case DpadState_Right:
                return "Right";
            default:
                return "Off";
        }
    }

    static const char* AccelStateToName(AccelState state) {
        switch (state) {
            case AccelState_Pressed:
                return "Pressed";
            default:
                return "Off";
        }
    }

    static const char* TriggerToName(Trigger trigger) {
        switch (trigger) {
            case Trigger_L:
                return "L";
            case Trigger_R:
                return "R";
            case Trigger_BD:
                return "BD";
            default:
                return "Off";
        }
    }

    static const char* TriggerStateToName(TriggerState state) {
        switch (state) {
            case TriggerState_Pressed:
                return "Pressed";
            default:
                return "Off";
        }
    }
};
}  // namespace UI
}  // namespace Pulsar

#endif
