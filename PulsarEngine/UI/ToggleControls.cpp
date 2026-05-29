#include <UI/ToggleControls.hpp>

namespace Pulsar {
namespace UI {

static const char* anims[7] = {"Choice", "ChoiceOff", "ChoiceOffToOn", "ChoiceOn", "ChoiceOnToOff", nullptr, nullptr};

void ToggleButton::ToggleState(bool state) {
    if (this->state != state) {
        u32 id = this->state ? 0 : 2;
        this->animator.GetAnimationGroupById(4).PlayAnimationAtFrame(id, 0.0f);  // choice
        this->state = !this->state;
    }
}

void ToggleButton::OnClick(u32, u32) {
    AnimationGroup& choiceGroup = this->animator.GetAnimationGroupById(4);  // choice group, off, offtoon, on, ontooff
    const u32 curAnimation = choiceGroup.curAnimation;
    const bool curState = this->state;
    this->state = !this->state;
    if ((curState && curAnimation == 1) || (!curState && curAnimation == 3)) {
        float frameSize = static_cast<float>(choiceGroup.animations[curAnimation].transform->GetFrameSize());
        choiceGroup.PlayAnimationAtPercent(1, 1.0f - 75.0f / frameSize);
    } else if (curState && curAnimation == 2)
        choiceGroup.PlayAnimationAtFrame(3, 0.0f);
    else if (!curState && curAnimation == 0)
        choiceGroup.PlayAnimationAtFrame(1, 0.0f);
}

void ToggleButton::Load(u32 localPlayerBitfield, const char* folderName, const char* ctrName, const char* variant) {
    this->LoadWithAnims(anims, folderName, ctrName, variant, localPlayerBitfield, 0);
    AnimationGroup& choiceGroup = this->animator.GetAnimationGroupById(4);  // choice, pattern and colours on click
    choiceGroup.PlayAnimationAtFrame(this->state ? 2 : 0, 0.0f);
}

}  // namespace UI
}  // namespace Pulsar
