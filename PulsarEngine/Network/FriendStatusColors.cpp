#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <MarioKartWii/UI/Page/Other/FriendList.hpp>
#include <MarioKartWii/UI/Page/Other/WFCMenu.hpp>
#include <Network/FriendStatusColors.hpp>
#include <UI/UI.hpp>

namespace Pulsar {
namespace Network {

ut::Color GetFriendColor(u32 friendIdx) {
    RKNet::SearchType type = RKNet::Controller::sInstance->GetFriendSearchType(friendIdx);
    const u8 friendRegion = RKNet::Controller::sInstance->friends[friendIdx].statusData.regionId;

    switch (type) {
        case RKNet::SEARCH_TYPE_VS_WW:
            return 0xffff00ff;
        case RKNet::SEARCH_TYPE_BT_WW:
            return 0xffa000ff;
        case RKNet::SEARCH_TYPE_BT_REGIONAL:
            return 0x00ffffff;
        case RKNet::SEARCH_TYPE_VS_REGIONAL:
            switch (friendRegion) {
                case 0x0B:  // Online TT
                    return 0xffa000ff;
                case 0x0C:  // 200cc
                    return 0x8000ffff;
                case 0x0D:  // Item Rain
                    return 0xff0000ff;
                case 0x14:  // Custom tracks
                    return 0x0050ffff;
                case 0x15:  // Regular tracks
                    return 0xffff00ff;
                default:  // Retro tracks and other regionals
                    return 0x00ff00ff;
            }
        default:
            return 0xffff00ff;
    }
}

static u8 friendStatusButtonUsedIdx;
asmFunc FriendStatusUsedIdx() {
    ASM(
        nofralloc;
        mr r30, r28;
        lis r12, friendStatusButtonUsedIdx @ha;
        stb r29, friendStatusButtonUsedIdx @l(r12);
        blr;
    )
}
kmCall(0x8064b548, FriendStatusUsedIdx);
kmCall(0x8064de54, FriendStatusUsedIdx);  // after race

void WifiMenuButtonColor(LayoutUIControl &control, u32 friendIdx) {
    ut::Color color = GetFriendColor(friendIdx);
    lyt::TextBox *text = reinterpret_cast<lyt::TextBox *>(control.layout.GetPaneByName("text_meet"));
    text->color1[0] = color;
    UI::ResetMatColor(text, 0xa0a0a000);
    UI::UnbindRLMC(text->material);
    lyt::Picture *smiley = reinterpret_cast<lyt::Picture *>(control.layout.GetPaneByName("nikoyellow_waku"));
    smiley->vertexColours[0] = color;
    smiley->vertexColours[1] = color;
    smiley->vertexColours[2] = color;
    smiley->vertexColours[3] = color;
    UI::ResetMatColor(smiley, 0);
    UI::UnbindRLMC(smiley->material);
    smiley = reinterpret_cast<lyt::Picture *>(control.layout.GetPaneByName("nikoyellow_light"));
    smiley->vertexColours[0] = color;
    smiley->vertexColours[1] = color;
    smiley->vertexColours[2] = color;
    smiley->vertexColours[3] = color;
    UI::ResetMatColor(smiley, 0xa0a0a000);
    UI::UnbindRLMC(smiley->material);
    lyt::Material *border = reinterpret_cast<lyt::Window *>(control.layout.GetPaneByName("Window_00"))->frames->material;
    border->tevColours[0].r = color.r;
    border->tevColours[0].g = color.g;
    border->tevColours[0].b = color.b;
    border->tevColours[0].a = 0;
    UI::UnbindRLMC(border);
}

void FriendStatusButtonColor(AnimationGroup &group, u32 idx, float frame) {
    group.PlayAnimationAtFrame(idx, frame);
    if (idx == 1) {  // MEET anm
        register LayoutUIControl *button;
        asm(mr button, r27;);
        WifiMenuButtonColor(*button, friendStatusButtonUsedIdx);
    }
}
kmCall(0x8064b5e8, FriendStatusButtonColor);
kmCall(0x8064de98, FriendStatusButtonColor);  // after race
kmWrite32(0x8064b560, 0x60000000);

void FriendButtonWindowColor(FriendButton &button) {
    ut::Color color = GetFriendColor(button.friendIdx);
    lyt::Picture *smiley = reinterpret_cast<lyt::Picture *>(button.layout.GetPaneByName("nikoyellow_waku"));
    smiley->vertexColours[0] = color;
    smiley->vertexColours[1] = color;
    smiley->vertexColours[2] = color;
    smiley->vertexColours[3] = color;
    UI::ResetMatColor(smiley, 0);
    UI::UnbindRLMC(smiley->material);
    smiley = reinterpret_cast<lyt::Picture *>(button.layout.GetPaneByName("nikoyellow_base"));
    smiley->vertexColours[0] = color;
    smiley->vertexColours[1] = color;
    smiley->vertexColours[2] = color;
    smiley->vertexColours[3] = color;
    UI::UnbindRLMC(smiley->material);
    // UI::ResetMatColor(smiley); don't do that as this is a light
    lyt::Material *border = reinterpret_cast<lyt::Window *>(button.layout.GetPaneByName("Window_00"))->frames->material;
    border->tevColours[0].r = color.r;
    border->tevColours[0].g = color.g;
    border->tevColours[0].b = color.b;
    border->tevColours[0].a = 0;
    UI::UnbindRLMC(border);
}

void FriendButtonColorOnActivate(AnimationGroup &group, u32 idx, float frame) {
    group.PlayAnimationAtFrame(idx, frame);
    register FriendButton *button;
    asm(mr button, r30;);
    if (button->status == 4)
        FriendButtonWindowColor(*button);
}
bool FriendButtonColorOnUpdate(FriendButton &button, u32 idx, float frame) {
    register u32 status;
    asm(mr status, r31;);
    if (status == 4)
        FriendButtonWindowColor(button);
    return button.IsSelected();
}
kmCall(0x805d3b40, FriendButtonColorOnActivate);
kmCall(0x805d3d1c, FriendButtonColorOnUpdate);

kmWrite32(0x805d60e4, 0x7fe3fb78);  // nop to preserve the page
kmWrite32(0x805d61a0, 0x7fc3f378);
void JoinButtonColor(Pages::FriendInfo &info, u32 status) {
    info.joinButton.SetStatus(status);
    if (status >= 0x15 && status <= 0x18) {
        WifiMenuButtonColor(info.joinButton, info.selectedFriendIdx);
    }
};
kmCall(0x805d60e8, JoinButtonColor);
kmCall(0x805d61a8, JoinButtonColor);

void SetGlobeMsgColor(Pages::Globe::MessageWindow &msg, ut::Color color) {
    static const char *suffixes[5] = {"c", "normal_l", "normal_r", "l", "r"};
    char paneName[20];
    for (int i = 0; i < 5; ++i) {
        snprintf(paneName, 20, "window_%s", suffixes[i]);
        lyt::Picture *picture = reinterpret_cast<lyt::Picture *>(msg.layout.GetPaneByName(paneName));
        picture->vertexColours[0] = color;
        picture->vertexColours[1] = color;
        picture->vertexColours[2] = color;
        picture->vertexColours[3] = color;
        if (color == -1)
            continue;
        UI::ResetMatColor(picture, 0);
        UI::UnbindRLMC(picture->material);
    }
}

}  // namespace Network
}  // namespace Pulsar
