#ifndef _PUL_UI_MISSIONMODEL_
#define _PUL_UI_MISSIONMODEL_

#include <kamek.hpp>
#include <UI/UI.hpp>
#include <MarioKartWii/UI/Page/Page.hpp>

class NoteModelControl;

namespace Pulsar {
namespace UI {
namespace MissionModel {

void Reset();
void SaveMenuCombo();
void RestoreMenuCombo();
void SetScenarioLoaded(bool loaded);
bool IsMissionMenuSection();
void ResetDriverAnimation(u8 hudSlotId);
void RequestBackgroundModel();
void CreateModelPage(ExpSection& section);
void UpdateComboModel(NoteModelControl& model);
bool LoadComboModel(NoteModelControl& model);
void HideComboModel();
Page* CreateDriftSelectPage();

}
}
}

#endif
