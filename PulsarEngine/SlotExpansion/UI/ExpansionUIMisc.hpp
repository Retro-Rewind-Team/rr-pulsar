#ifndef _EXPANSIONUIMISC_
#define _EXPANSIONUIMISC_
#include <kamek.hpp>
#include <MarioKartWii/System/Identifiers.hpp>
#include <MarioKartWii/UI/Ctrl/PushButton.hpp>
#include <MarioKartWii/UI/Page/Other/Votes.hpp>
#include <PulsarSystem.hpp>
#include <UI/UI.hpp>

namespace Pulsar {
namespace UI {
int GetTrackBMGId(PulsarId pulsarId, bool useCommonName);
int GetTrackVariantBMGId(PulsarId pulsarId, u8 variantIdx);
bool IsTrackBlocked(PulsarId id);
void SetCourseButtonMessage(PushButton& button, u32 bmgId, PulsarId trackId, u32 buttonIdx);
void SetVoteControlMessage(VoteControl& vote, u32 bmgId, PulsarId courseVote, u32 playerId);
void ApplyBlockedColorToString(wchar_t* dest, const wchar_t* src, u32 maxLen);

inline void GetTrackBMG(char* dest, PulsarId id) {
    const wchar_t* name = UI::GetCustomMsg(GetTrackBMGId(id, false));
    wchar_t polish[0x102];

    const wchar_t* token = wcschr(name, L'\x1A');
    const wchar_t* finalString = token != nullptr ? polish : name;

    wchar_t* cur = polish;
    const wchar_t* pos = name;
    while (token != nullptr) {
        wcsncpy(cur, pos, token - pos);
        cur = cur + (token - pos);
        cur[0] = '\0';
        const u8* escapeSequence = reinterpret_cast<const u8*>(token);
        u8 length = escapeSequence[2];
        pos = reinterpret_cast<const wchar_t*>(escapeSequence + length);
        token = wcschr(pos, L'\x1A');
    }
    snprintf(dest, 0x100, "%ls", finalString);
}
int GetTrackBMGByRowIdx(u32 cupTrackIdx);
int GetCurTrackBMG();
}  // namespace UI
}  // namespace Pulsar
#endif