#ifndef __EXTENDEDTEAMS_CTRL_RESULT_HPP
#define __EXTENDEDTEAMS_CTRL_RESULT_HPP

#include <MarioKartWii/UI/Ctrl/CtrlRace/CtrlRaceResultTeam.hpp>
#include <UI/ExtendedTeamSelect/ExtendedTeamSelect.hpp>

namespace Pulsar {
namespace UI {

class CtrlRaceResultExtendedTeams : public LayoutUIControl {
public:
    void InitSelf() override;
    void OnUpdate() override;
    const char *GetClassName() const override;

    void Load(ExtendedTeamID teamID, int numTeams, int teamIdx);
    bool IsResultAnimDone() const;

    ExtendedTeamID teamId;
    CtrlRaceResultTeam::Player players[6];

    LayoutUIControl items[6];
    LayoutUIControl *resultTeamPoint;

    Text::Info textInfo;
    float currentScore;
    u32 teamScore;
};

}  // namespace UI
}  // namespace Pulsar

#endif