#include <kamek.hpp>
#include <UI/MissionMode/MissionModel.hpp>
#include <Gamemodes/MissionMode/MissionMode.hpp>
#include <MarioKartWii/3D/Model/Menu/MenuModelMgr.hpp>
#include <MarioKartWii/3D/Model/Menu/MenuDriverModel.hpp>
#include <MarioKartWii/Race/RaceData.hpp>
#include <MarioKartWii/UI/Page/Menu/DriftSelect.hpp>
#include <MarioKartWii/UI/Page/Other/ModelRenderer.hpp>
#include <MarioKartWii/UI/Ctrl/ModelControl.hpp>
#include <runtimeWrite.hpp>

namespace Pulsar {
namespace UI {
namespace MissionModel {

namespace {

static bool scenarioLoaded;
static bool comboModelLoaded;
static const u32 BACK_MODEL_CONTROL_OFFSET = 0x1C8;

class MissionDriftSelect : public Pages::DriftSelect {
   public:
    void OnActivate() override {
        Pages::DriftSelect::OnActivate();
        if (!IsMissionMenuSection()) return;
        if (this->extraControlNumber > 0) UpdateComboModel(this->modelPosition[0]);
    }

    void OnDeactivate() override {
        if (IsMissionMenuSection()) {
            HideComboModel();
            ResetDriverAnimation(0);
        }
        Pages::DriftSelect::OnDeactivate();
    }
};

}

void Reset() {
    scenarioLoaded = false;
    comboModelLoaded = false;
}

void SetScenarioLoaded(bool loaded) {
    scenarioLoaded = loaded;
    if (!loaded) comboModelLoaded = false;
}

bool IsMissionMenuSection() {
    if (!scenarioLoaded || Racedata::sInstance == nullptr ||
        Racedata::sInstance->menusScenario.settings.gamemode != MODE_MISSION_TOURNAMENT ||
        SectionMgr::sInstance == nullptr || SectionMgr::sInstance->curSection == nullptr)
        return false;

    const SectionId sectionId = SectionMgr::sInstance->curSection->sectionId;
    return sectionId == SECTION_SINGLE_P_FROM_MENU || sectionId == SECTION_SINGLE_P_MR_CHOOSE_MISSION;
}

void ResetDriverAnimation(u8 hudSlotId) {
    if (MenuModelMgr::sInstance == nullptr || !MenuModelMgr::sInstance->isActive ||
        MenuModelMgr::sInstance->driverModels == nullptr) return;

    MenuDriverModelMgr* driverModels = MenuModelMgr::sInstance->driverModels;
    if (hudSlotId >= driverModels->playerCount || driverModels->players[hudSlotId].playerModel == nullptr) return;

    MenuDriverModel* driver = driverModels->players[hudSlotId].playerModel;
    if (driver->state >= MenuDriverModel::MENUDRIVERMODEL_STATE_ONKARTSELECT) {
        if (driver->charSelTransformator == nullptr) return;
        driver->SwitchState(hudSlotId, MenuDriverModel::MENUDRIVERMODEL_STATE_ONCHARSELECT);
    }
}

kmRuntimeUse(0x805f2e84);
void RequestBackgroundModel() {
    ExpSection* section = ExpSection::GetSection();
    if (section != nullptr && section->pages[PAGE_BACKMODEL] != nullptr) {
        typedef void (*RequestModelFn)(void*, BackModelType);
        const RequestModelFn requestModel = reinterpret_cast<RequestModelFn>(kmRuntimeAddr(0x805f2e84));
        requestModel(reinterpret_cast<u8*>(section->pages[PAGE_BACKMODEL]) + BACK_MODEL_CONTROL_OFFSET,
                     BACKMODEL_BALOON);
        return;
    }

    if (MenuModelMgr::sInstance != nullptr) MenuModelMgr::sInstance->RequestBackModel(BACKMODEL_BALOON);
}

void CreateModelPage(ExpSection& section) {
    if (section.pages[PAGE_MODEL_RENDERER] == nullptr) section.CreateAndInitPage(section, PAGE_MODEL_RENDERER);
}

void UpdateComboModel(NoteModelControl& model) {
    if (!IsMissionMenuSection()) {
        model.isHidden = true;
        return;
    }

    ExpSection* section = ExpSection::GetSection();
    if (section == nullptr) {
        model.isHidden = true;
        return;
    }
    Pages::ModelRenderer* renderer = section->Get<Pages::ModelRenderer>();
    if (renderer == nullptr) {
        model.isHidden = true;
        return;
    }

    const RacedataPlayer& player = Racedata::sInstance->menusScenario.players[0];
    const s32 characterId = static_cast<s32>(player.characterId);
    const s32 kartId = static_cast<s32>(player.kartId);
    if (characterId < static_cast<s32>(MARIO) || characterId > static_cast<s32>(ROSALINA_BIKER) ||
        kartId < static_cast<s32>(STANDARD_KART_S) || kartId > static_cast<s32>(PHANTOM)) {
        renderer->params[0].isVisible = false;
        model.isHidden = true;
        return;
    }

    if (SectionMgr::sInstance->sectionParams != nullptr) {
        SectionMgr::sInstance->sectionParams->characters[0] = player.characterId;
        SectionMgr::sInstance->sectionParams->karts[0] = player.kartId;
    }
    renderer->params[0].isVisible = true;
    model.isHidden = false;
}

bool LoadComboModel(NoteModelControl& model) {
    if (!IsMissionMenuSection()) {
        model.isHidden = true;
        return false;
    }

    UpdateComboModel(model);
    if (model.isHidden || comboModelLoaded) return false;

    ExpSection* section = ExpSection::GetSection();
    if (section == nullptr || Racedata::sInstance == nullptr) return false;

    Pages::ModelRenderer* renderer = section->Get<Pages::ModelRenderer>();
    if (renderer == nullptr) return false;

    const RacedataPlayer& player = Racedata::sInstance->menusScenario.players[0];
    ResetDriverAnimation(0);
    renderer->LoadKartModelsByCharacter(0, player.characterId);
    renderer->RequestCharacterModel(0, player.characterId);
    renderer->RequestKartModel(0, player.kartId);
    comboModelLoaded = true;
    return true;
}

void HideComboModel() {
    if (!IsMissionMenuSection()) return;

    ExpSection* section = ExpSection::GetSection();
    Pages::ModelRenderer* renderer = section->Get<Pages::ModelRenderer>();
    if (renderer != nullptr) renderer->params[0].isVisible = false;
}

Page* CreateDriftSelectPage() { return new MissionDriftSelect(); }

}
}
}
