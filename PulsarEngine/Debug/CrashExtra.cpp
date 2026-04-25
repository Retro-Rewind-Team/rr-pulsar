#include <include/c_stdio.h>
#include <include/c_string.h>

#include <Debug/CrashExtra.hpp>
#include <PulsarSystem.hpp>
#include <IO/LooseArchiveOverrides.hpp>
#include <SlotExpansion/CupsConfig.hpp>
#include <CustomCharacters.hpp>
#include <MarioKartWii/UI/Section/SectionMgr.hpp>

namespace Pulsar {
namespace Debug {
namespace {

const char* GetVanillaTrackSzs(CourseId courseId) {
    switch (courseId) {
        case LUIGI_CIRCUIT: return "beginner_course.szs";
        case MOO_MOO_MEADOWS: return "farm_course.szs";
        case MUSHROOM_GORGE: return "kinoko_course.szs";
        case TOADS_FACTORY: return "factory_course.szs";
        case MARIO_CIRCUIT: return "castle_course.szs";
        case COCONUT_MALL: return "shopping_course.szs";
        case DK_SUMMIT: return "boardcross_course.szs";
        case WARIOS_GOLD_MINE: return "truck_course.szs";
        case DAISY_CIRCUIT: return "senior_course.szs";
        case KOOPA_CAPE: return "water_course.szs";
        case MAPLE_TREEWAY: return "treehouse_course.szs";
        case GRUMBLE_VOLCANO: return "volcano_course.szs";
        case DRY_DRY_RUINS: return "desert_course.szs";
        case MOONVIEW_HIGHWAY: return "ridgehighway_course.szs";
        case BOWSERS_CASTLE: return "koopa_course.szs";
        case RAINBOW_ROAD: return "rainbow_course.szs";
        case GCN_PEACH_BEACH: return "old_peach_gc.szs";
        case DS_YOSHI_FALLS: return "old_falls_ds.szs";
        case SNES_GHOST_VALLEY_2: return "old_obake_sfc.szs";
        case N64_MARIO_RACEWAY: return "old_mario_64.szs";
        case N64_SHERBET_LAND: return "old_sherbet_64.szs";
        case GBA_SHY_GUY_BEACH: return "old_heyho_gba.szs";
        case DS_DELFINO_SQUARE: return "old_town_ds.szs";
        case GCN_WALUIGI_STADIUM: return "old_waluigi_gc.szs";
        case DS_DESERT_HILLS: return "old_desert_ds.szs";
        case GBA_BOWSER_CASTLE_3: return "old_koopa_gba.szs";
        case N64_DKS_JUNGLE_PARKWAY: return "old_donkey_64.szs";
        case GCN_MARIO_CIRCUIT: return "old_mario_gc.szs";
        case SNES_MARIO_CIRCUIT_3: return "old_mario_sfc.szs";
        case DS_PEACH_GARDENS: return "old_garden_ds.szs";
        case GCN_DK_MOUNTAIN: return "old_donkey_gc.szs";
        case N64_BOWSERS_CASTLE: return "old_koopa_64.szs";
        case DELFINO_PIER: return "venice_battle.szs";
        case BLOCK_PLAZA: return "block_battle.szs";
        case CHAIN_CHOMP_WHEEL: return "casino_battle.szs";
        case FUNKY_STADIUM: return "skate_battle.szs";
        case THWOMP_DESERT: return "sand_battle.szs";
        case GCN_COOKIE_LAND: return "old_CookieLand_gc.szs";
        case DS_TWILIGHT_HOUSE: return "old_House_ds.szs";
        case SNES_BATTLE_COURSE_4: return "old_battle4_sfc.szs";
        case GBA_BATTLE_COURSE_3: return "old_battle3_gba.szs";
        case N64_SKYSCRAPER: return "old_matenro_64.szs";
        case GALAXY_COLOSSEUM: return "ring_mission.szs";
        case WINNING_DEMO: return "winningrun_demo.szs";
        case LOSING_DEMO: return "loser_demo.szs";
        case DRAW_DEMO: return "draw_demo.szs";
        default: return nullptr;
    }
}

void CopyTrackSzs(char* dest, u32 size, const char* src) {
    if (dest == nullptr || size == 0) return;
    dest[0] = '\0';
    if (src == nullptr || src[0] == '\0') return;
    snprintf(dest, size, "%s", src);
}

void PopulateLastTrackSzs(CrashExtra& extra) {
    CupsConfig* cupsConfig = CupsConfig::sInstance;
    if (cupsConfig == nullptr) return;

    const PulsarId winning = cupsConfig->GetWinning();
    if (winning != PULSARID_NONE) {
        if (!CupsConfig::IsReg(winning)) {
            const char* fileName = cupsConfig->GetFileName(winning, cupsConfig->GetCurVariantIdx());
            if (fileName == nullptr || fileName[0] == '\0') fileName = cupsConfig->GetFileName(winning, 0);
            if (fileName != nullptr && fileName[0] != '\0') {
                snprintf(extra.lastTrackSzs, sizeof(extra.lastTrackSzs), "%s.szs", fileName);
                return;
            }
        } else {
            const char* fileName = GetVanillaTrackSzs(CupsConfig::ConvertTrack_PulsarIdToRealId(winning));
            if (fileName != nullptr) {
                CopyTrackSzs(extra.lastTrackSzs, sizeof(extra.lastTrackSzs), fileName);
                return;
            }
        }
    }

    if (Racedata::sInstance == nullptr) return;
    const char* fallback = GetVanillaTrackSzs(Racedata::sInstance->racesScenario.settings.courseId);
    CopyTrackSzs(extra.lastTrackSzs, sizeof(extra.lastTrackSzs), fallback);
}

}  // namespace

void PopulateCrashExtra(ExceptionFile& exception) {
    CrashExtra& extra = exception.extra;

    SectionMgr* sectionMgr = SectionMgr::sInstance;
    if (sectionMgr != nullptr && sectionMgr->curSection != nullptr) {
        extra.sectionId = static_cast<s32>(sectionMgr->curSection->sectionId);
        Page* topPage = sectionMgr->curSection->GetTopLayerPage();
        if (topPage != nullptr) extra.pageId = static_cast<s32>(topPage->pageId);
    }

    const System* system = System::sInstance;
    if (system != nullptr) {
        extra.context = system->context;
        extra.context2 = system->context2;
    }

    if (IOOverrides::AreLooseArchiveOverridesEnabledForDebug()) {
        extra.flags |= EXCEPTION_FLAG_LOOSE_ARCHIVE_OVERRIDES_ENABLED;
    }
    if (CustomCharacters::IsCustomCharacterTableActive()) {
        extra.flags |= EXCEPTION_FLAG_CUSTOM_CHARACTER_ENABLED;
    }
    extra.looseOverrideFileCount = IOOverrides::GetLooseArchiveOverrideFileCount();
    const u8 myStuffValue = *reinterpret_cast<volatile u8*>(0x80001200);
    if (myStuffValue == 0x01) extra.myStuffState = EXCEPTION_MYSTUFF_ENABLED;
    else if (myStuffValue == 0x02) extra.myStuffState = EXCEPTION_MYSTUFF_MUSIC_ONLY;
    else extra.myStuffState = EXCEPTION_MYSTUFF_DISABLED;

    PopulateLastTrackSzs(extra);
}

}  // namespace Debug
}  // namespace Pulsar
