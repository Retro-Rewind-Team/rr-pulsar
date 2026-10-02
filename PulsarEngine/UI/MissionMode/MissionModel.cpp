#include <kamek.hpp>
#include <Driver/CustomCharacters.hpp>
#include <UI/MissionMode/MissionModel.hpp>
#include <Gamemodes/MissionMode/MissionMode.hpp>
#include <Gamemodes/MissionMode/MissionMusic.hpp>
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
static Pages::ModelRenderer *comboModelRenderer;
static CharacterId comboModelCharacter;
static KartId comboModelKart;
static bool savedMenuCombo;
static CharacterId savedCharacter;
static KartId savedKart;
static bool missionCharacterSlotApplied;
static CharacterId missionCharacter;
static u8 savedMissionCharacterSlot;
static bool missionLoadedCharacters[0x18];
static const u32 BACK_MODEL_CONTROL_OFFSET = 0x1C8;

class MissionDriftSelect : public Pages::DriftSelect {
public:
    void OnActivate() override {
        Pages::DriftSelect::OnActivate();
        if (!IsMissionMenuSection())
            return;
        if (this->extraControlNumber > 0)
            UpdateComboModel(this->modelPosition[0]);
    }

    void OnDeactivate() override {
        if (IsMissionMenuSection()) {
            HideComboModel();
            ResetDriverAnimation(0);
        }
        Pages::DriftSelect::OnDeactivate();
    }
};

}  // namespace

void ResetDriverAnimation(u8 hudSlotId) {
    if (MenuModelMgr::sInstance == nullptr || !MenuModelMgr::sInstance->isActive || MenuModelMgr::sInstance->driverModels == nullptr)
        return;

    MenuDriverModelMgr *driverModels = MenuModelMgr::sInstance->driverModels;
    if (hudSlotId >= driverModels->playerCount || driverModels->players[hudSlotId].playerModel == nullptr)
        return;

    MenuDriverModel *driver = driverModels->players[hudSlotId].playerModel;
    if (driver->state < MenuDriverModel::MENUDRIVERMODEL_STATE_ONKARTSELECT)
        return;

    if (driver->charSelTransformator == nullptr || reinterpret_cast<u32>(driver->charSelTransformator) < 0x1000)
        return;

    driver->SwitchState(hudSlotId, MenuDriverModel::MENUDRIVERMODEL_STATE_ONCHARSELECT);
}

void RestoreMenuDriverModel(CharacterId character) {
    if (MenuModelMgr::sInstance == nullptr || !MenuModelMgr::sInstance->isActive || MenuModelMgr::sInstance->driverModels == nullptr)
        return;

    MenuDriverModelMgr *driverModels = MenuModelMgr::sInstance->driverModels;
    if (driverModels->playerCount == 0)
        return;

    driverModels->SetPlayerCharacter(0, character);
    ResetDriverAnimation(0);
}

void ResetMissionDriverModels() {
    if (MenuModelMgr::sInstance == nullptr || !MenuModelMgr::sInstance->isActive || MenuModelMgr::sInstance->driverModels == nullptr)
        return;

    MenuDriverModelMgr *driverModels = MenuModelMgr::sInstance->driverModels;
    if (driverModels->models == nullptr)
        return;

    for (u32 character = 0; character < sizeof(missionLoadedCharacters); ++character) {
        if (!missionLoadedCharacters[character])
            continue;

        MenuDriverModel *driver = &driverModels->models[character];
        if (driver->model != nullptr && driver->model->modelTransformator != nullptr && reinterpret_cast<u32>(driver->model->modelTransformator) >= 0x1000) {
            driver->charSelTransformator = driver->model->modelTransformator;
            driver->onKartTransformator = nullptr;
            driver->Init();
        }
    }
    memset(missionLoadedCharacters, 0, sizeof(missionLoadedCharacters));
}

void Reset() {
    if (missionCharacterSlotApplied) {
        const u32 character = static_cast<u32>(missionCharacter);
        Driver::selectedSlots[character] = savedMissionCharacterSlot;
        if (MenuModelMgr::sInstance != nullptr && MenuModelMgr::sInstance->isActive && MenuModelMgr::sInstance->driverModels != nullptr)
            Driver::LoadDriverBRRES(missionCharacter, savedMissionCharacterSlot);
        missionCharacterSlotApplied = false;
    }
    scenarioLoaded = false;
    comboModelLoaded = false;

    if (SectionMgr::sInstance == nullptr || SectionMgr::sInstance->curSection == nullptr)
        return;

    Pages::ModelRenderer *renderer = static_cast<Pages::ModelRenderer *>(SectionMgr::sInstance->curSection->pages[PAGE_MODEL_RENDERER]);
    if (renderer != nullptr)
        renderer->params[0].isVisible = false;
}

void SaveMenuCombo() {
    comboModelLoaded = false;

    if (savedMenuCombo || SectionMgr::sInstance == nullptr || SectionMgr::sInstance->sectionParams == nullptr)
        return;

    SectionParams *params = SectionMgr::sInstance->sectionParams;
    savedCharacter = params->characters[0];
    savedKart = params->karts[0];
    savedMenuCombo = true;
}

void RestoreMenuCombo() {
    if (!savedMenuCombo || SectionMgr::sInstance == nullptr || SectionMgr::sInstance->sectionParams == nullptr)
        return;

    SectionParams *params = SectionMgr::sInstance->sectionParams;
    params->characters[0] = savedCharacter;
    params->karts[0] = savedKart;
    params->combos[0].selCharacter = savedCharacter;
    params->combos[0].selKart = savedKart;

    if (Racedata::sInstance != nullptr) {
        Racedata::sInstance->menusScenario.players[0].characterId = savedCharacter;
        Racedata::sInstance->menusScenario.players[0].kartId = savedKart;
    }

    if (SectionMgr::sInstance->curSection != nullptr) {
        Pages::ModelRenderer *renderer = static_cast<Pages::ModelRenderer *>(SectionMgr::sInstance->curSection->pages[PAGE_MODEL_RENDERER]);
        if (renderer != nullptr) {
            renderer->params[0].character = savedCharacter;
            renderer->params[0].kart = savedKart;
            renderer->params[0].isVisible = false;
        }
    }
    ResetMissionDriverModels();
    RestoreMenuDriverModel(savedCharacter);
    savedMenuCombo = false;
}

void SetScenarioLoaded(bool loaded) {
    scenarioLoaded = loaded;
    comboModelLoaded = false;
    if (!loaded || Racedata::sInstance == nullptr || !::Pulsar::MissionMode::IsMissionScenario(Racedata::sInstance->menusScenario))
        return;

    const CharacterId characterId = Racedata::sInstance->menusScenario.players[0].characterId;
    const u32 character = static_cast<u32>(characterId);
    if (character >= Driver::CHARACTER_COUNT)
        return;

    const u8 configuredSlot = ::Pulsar::MissionMode::GetMissionCharacterTable(Racedata::sInstance->menusScenario, 0);
    if (configuredSlot == ::Pulsar::MissionMode::MISSION_CHARACTER_TABLE_UNSET)
        return;

    missionCharacter = characterId;
    savedMissionCharacterSlot = Driver::selectedSlots[character];
    missionCharacterSlotApplied = true;
    Driver::selectedSlots[character] = configuredSlot <= Driver::MAX_CUSTOM_CHARACTER_SLOTS && Driver::characterTables[character][configuredSlot] ? configuredSlot : 0;
    if (MenuModelMgr::sInstance != nullptr && MenuModelMgr::sInstance->isActive && MenuModelMgr::sInstance->driverModels != nullptr)
        Driver::LoadDriverBRRES(characterId, Driver::selectedSlots[character]);
}

bool IsMissionMenuSection() {
    if (!scenarioLoaded || Racedata::sInstance == nullptr || Racedata::sInstance->menusScenario.settings.gamemode != MODE_MISSION_TOURNAMENT || SectionMgr::sInstance == nullptr
      || SectionMgr::sInstance->curSection == nullptr)
        return false;

    const SectionId sectionId = SectionMgr::sInstance->curSection->sectionId;
    return sectionId == SECTION_SINGLE_P_FROM_MENU || sectionId == SECTION_SINGLE_P_TT_CHANGE_CHARA || sectionId == SECTION_SINGLE_P_VS_NEXT_RACE || sectionId == SECTION_SINGLE_P_MR_CHOOSE_MISSION;
}

kmRuntimeUse(0x805f2e84);
void RequestBackgroundModel() {
    ExpSection *section = ExpSection::GetSection();
    if (section != nullptr && section->pages[PAGE_BACKMODEL] != nullptr) {
        typedef void (*RequestModelFn)(void *, BackModelType);
        const RequestModelFn requestModel = reinterpret_cast<RequestModelFn>(kmRuntimeAddr(0x805f2e84));
        requestModel(reinterpret_cast<u8 *>(section->pages[PAGE_BACKMODEL]) + BACK_MODEL_CONTROL_OFFSET, BACKMODEL_BALOON);
        return;
    }

    if (MenuModelMgr::sInstance != nullptr)
        MenuModelMgr::sInstance->RequestBackModel(BACKMODEL_BALOON);
}

void CreateModelPage(ExpSection &section) {
    if (section.pages[PAGE_MODEL_RENDERER] == nullptr)
        section.CreateAndInitPage(section, PAGE_MODEL_RENDERER);

    Pages::ModelRenderer *renderer = section.Get<Pages::ModelRenderer>();
    if (renderer != comboModelRenderer) {
        comboModelRenderer = renderer;
        comboModelLoaded = false;
    }
}

void UpdateComboModel(NoteModelControl &model) {
    if (!IsMissionMenuSection()) {
        model.isHidden = true;
        return;
    }

    ExpSection *section = ExpSection::GetSection();
    if (section == nullptr) {
        model.isHidden = true;
        return;
    }
    Pages::ModelRenderer *renderer = section->Get<Pages::ModelRenderer>();
    if (renderer == nullptr) {
        model.isHidden = true;
        return;
    }

    const RacedataPlayer &player = Racedata::sInstance->menusScenario.players[0];
    const s32 characterId = static_cast<s32>(player.characterId);
    const s32 kartId = static_cast<s32>(player.kartId);
    if (characterId < static_cast<s32>(MARIO) || characterId > static_cast<s32>(ROSALINA_BIKER) || kartId < static_cast<s32>(STANDARD_KART_S) || kartId > static_cast<s32>(PHANTOM)) {
        renderer->params[0].isVisible = false;
        model.isHidden = true;
        return;
    }

    if (SectionMgr::sInstance->sectionParams != nullptr) {
        SectionMgr::sInstance->sectionParams->characters[0] = player.characterId;
        SectionMgr::sInstance->sectionParams->karts[0] = player.kartId;
    }
    renderer->params[0].character = player.characterId;
    renderer->params[0].kart = player.kartId;
    renderer->params[0].isVisible = true;
    model.isHidden = false;
}

bool LoadComboModel(NoteModelControl &model) {
    if (!IsMissionMenuSection()) {
        model.isHidden = true;
        return false;
    }

    UpdateComboModel(model);
    if (model.isHidden)
        return false;

    ExpSection *section = ExpSection::GetSection();
    if (section == nullptr || Racedata::sInstance == nullptr)
        return false;

    Pages::ModelRenderer *renderer = section->Get<Pages::ModelRenderer>();
    if (renderer == nullptr)
        return false;

    const RacedataPlayer &player = Racedata::sInstance->menusScenario.players[0];
    if (comboModelLoaded && comboModelRenderer == renderer && comboModelCharacter == player.characterId && comboModelKart == player.kartId)
        return true;

    ResetDriverAnimation(0);
    renderer->params[0].Reset();
    renderer->params[0].character = player.characterId;
    renderer->params[0].kart = player.kartId;
    renderer->LoadKartModelsByCharacter(0, player.characterId);
    renderer->RequestCharacterModel(0, player.characterId);
    renderer->RequestKartModel(0, player.kartId);
    if (player.characterId >= MARIO && player.characterId <= ROSALINA_BIKER)
        missionLoadedCharacters[static_cast<u32>(player.characterId)] = true;
    comboModelRenderer = renderer;
    comboModelCharacter = player.characterId;
    comboModelKart = player.kartId;
    comboModelLoaded = true;
    return true;
}

void HideComboModel() {
    if (!IsMissionMenuSection())
        return;

    ExpSection *section = ExpSection::GetSection();
    Pages::ModelRenderer *renderer = section->Get<Pages::ModelRenderer>();
    if (renderer != nullptr)
        renderer->params[0].isVisible = false;
}

Page *CreateDriftSelectPage() {
    return new MissionDriftSelect();
}

}  // namespace MissionModel
}  // namespace UI
}  // namespace Pulsar
