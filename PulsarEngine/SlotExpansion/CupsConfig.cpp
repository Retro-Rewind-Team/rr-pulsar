#include <MarioKartWii/Audio/Race/AudioItemAlterationMgr.hpp>
#include <MarioKartWii/Audio/RaceMgr.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <MarioKartWii/GlobalFunctions.hpp>
#include <MarioKartWii/UI/Ctrl/PushButton.hpp>
#include <Settings/UI/ExpWFCMainPage.hpp>
#include <SlotExpansion/CupsConfig.hpp>
#include <Settings/Settings.hpp>
#include <PulsarSystem.hpp>

namespace Pulsar {

CupsConfig* CupsConfig::sInstance = nullptr;

CupsConfig::CupsConfig(const CupsHolder& rawCups) : regsMode(rawCups.regsMode),
                                                    // Cup actions initialization
                                                    hasOddCups(false),
                                                    winningCourse(PULSARID_NONE),
                                                    selectedCourse(PULSARID_FIRSTREG),
                                                    lastSelectedCup(PULSARCUPID_FIRSTREG),
                                                    lastSelectedCupButtonIdx(0),
                                                    isAlphabeticalLayout(false) {
    if (regsMode != 1) {
        lastSelectedCup = PULSARCUPID_FIRSTCT;
        selectedCourse = PULSARID_FIRSTCT;
    }
    hasRegs = regsMode > 0;

    u32 count = rawCups.ctsCupCount;
    if (count & 1) {
        ++count;
        hasOddCups = true;
    }
    definedCTsCupCount = count;
    ctsCupCount = count;
    for (int i = 0; i < 4; ++i) trophyCount[i] = rawCups.trophyCount[i];

    u16 ctsCount = count * 4;

    mainTracks = new Track[ctsCount];
    variants = new Variant[rawCups.totalVariantCount];
    variantsOffs = new u16[ctsCount];
    alphabeticalArray = new u16[ctsCount];
    invertedAlphabeticalArray = new u16[ctsCount];

    memcpy(mainTracks, &rawCups.tracks, sizeof(Track) * ctsCount);
    memcpy(variants, (reinterpret_cast<const u8*>(&rawCups.tracks) + sizeof(Track) * ctsCount), sizeof(Variant) * rawCups.totalVariantCount);

    for (int i = 0; i < 184; ++i) {
        alphabeticalArray[i] = i;
        invertedAlphabeticalArray[i] = i;
    }

    const u16* originalAlphabeticalArray = reinterpret_cast<const u16*>(reinterpret_cast<const u8*>(&rawCups.tracks) + sizeof(Track) * ctsCount);

    u16 lastTrackIndices[80];
    for (int i = 0; i < 80; ++i) {
        lastTrackIndices[i] = i + 184;
    }
    for (int i = 0; i < 80; ++i) {
        for (int j = i + 1; j < 80; ++j) {
            u16 iPos = 0xFFFF;
            u16 jPos = 0xFFFF;

            for (int k = 0; k < ctsCount; ++k) {
                if (originalAlphabeticalArray[k] == lastTrackIndices[i]) iPos = k;
                if (originalAlphabeticalArray[k] == lastTrackIndices[j]) jPos = k;
            }
            if (iPos > jPos) {
                u16 temp = lastTrackIndices[i];
                lastTrackIndices[i] = lastTrackIndices[j];
                lastTrackIndices[j] = temp;
            }
        }
    }
    for (int i = 0; i < 80; ++i) {
        alphabeticalArray[184 + i] = lastTrackIndices[i];
    }
    for (int i = 184 + 80; i < ctsCount; ++i) {
        alphabeticalArray[i] = i;
    }

    u16 cumulativeVarCount = 0;
    for (int i = 0; i < ctsCount; ++i) {
        if (i >= 184) {
            invertedAlphabeticalArray[alphabeticalArray[i]] = i;
        }
        variantsOffs[i] = cumulativeVarCount * sizeof(Variant);
        cumulativeVarCount += mainTracks[i].variantCount;
    }
}

// Converts trackID to track slot using table
CourseId CupsConfig::GetCorrectTrackSlot() const {
    const CourseId realId = ConvertTrack_PulsarIdToRealId(this->winningCourse);
    if (IsReg(this->winningCourse))
        return realId;
    else
        return static_cast<CourseId>(this->cur.slot);
}

// MusicSlot
inline int CupsConfig::GetCorrectMusicSlot() const {
    register const Audio::RaceMgr* mgr;
    asm(mr mgr, r30;);
    CourseId realId = mgr->courseId;
    if (realId <= 0x1F) {  //! battle
        realId = ConvertTrack_PulsarIdToRealId(this->winningCourse);
        if (!IsReg(this->winningCourse)) realId = static_cast<CourseId>(this->cur.musicSlot);
    }
    int ret = Audio::ItemAlterationMgr::courseToSoundIdTable[realId];
    register Audio::RaceState futureState;
    asm(mr futureState, r31;);
    if (futureState == Audio::RACE_STATE_FAST && ret == SOUND_ID_GALAXY_COLOSSEUM) ret = SOUND_ID_GALAXY_COLOSSEUM - 1;
    return ret;
}

int CupsConfig::GetCRC32(PulsarId pulsarId) const {
    if (IsReg(pulsarId))
        return RegsCRC32[pulsarId];
    else
        return this->GetTrack(pulsarId).crc32;
}

void CupsConfig::GetTrackGhostFolder(char* dest, PulsarId pulsarId) const {
    const u32 crc32 = this->GetCRC32(pulsarId);
    const char* modFolder = System::sInstance->GetModFolder();
    if (IsReg(pulsarId))
        snprintf(dest, IOS::ipcMaxPath, "%s/Ghosts/%s", modFolder, &crc32);
    else
        snprintf(dest, IOS::ipcMaxPath, "%s/Ghosts/%08x", modFolder, crc32);
}

u32 CupsConfig::RandomizeVariant(PulsarId id) const {
    u32 variantIdx = 0;
    if (!IsReg(id)) {
        const Track& track = GetTrack(id);
        Random random;
        variantIdx = random.NextLimited(track.variantCount + 1);
    }
    return variantIdx;
}

void CupsConfig::SetWinning(PulsarId id, u32 variantIdx) {
    if (!IsReg(id)) {
        const Track& track = GetTrack(id);
        cur.crc32 = track.crc32;
        const GameMode mode = Racedata::sInstance->menusScenario.settings.gamemode;
        if (mode == MODE_TIME_TRIAL || mode == MODE_GHOST_RACE)
            variantIdx = 0;
        else if (variantIdx == 0xFF)
            variantIdx = RandomizeVariant(id);

        u8 slot;
        u8 musicSlot;
        if (variantIdx == 0) {
            slot = track.slot;
            musicSlot = track.musicSlot;
        } else {
            const Variant& variant = variants[this->variantsOffs[ConvertTrack_PulsarIdToRealId(id)] + variantIdx - 1];
            slot = variant.slot;
            musicSlot = variant.musicSlot;
        }
        cur.slot = slot;
        cur.musicSlot = musicSlot;
    }

    this->winningCourse = id;
    this->curVariantIdx = variantIdx;
}

void CupsConfig::ToggleCTs(bool enabled) {
    u32 count;
    bool isRegs = false;
    const RacedataSettings& racedataSettings = Racedata::sInstance->menusScenario.settings;
    const GameMode mode = racedataSettings.gamemode;
    u32 isBattle = (mode == MODE_BATTLE || mode == MODE_PUBLIC_BATTLE || mode == MODE_PRIVATE_BATTLE);
    if ((RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_FROOM_HOST || RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_FROOM_NONHOST) && !isBattle) {
        if (System::sInstance->IsContext(PULSAR_REGS)) isRegs = TRACKSELECTION_REGS;
    }
    if (RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_VS_WW || RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_BT_WW || RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_JOINING_WW) {
        isRegs = TRACKSELECTION_REGS;
    }
    if ((RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_VS_REGIONAL || RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_JOINING_REGIONAL) && System::sInstance->netMgr.region == 0x15) {
        isRegs = TRACKSELECTION_REGS;
    }
    if (isRegs == TRACKSELECTION_REGS) {
        if (lastSelectedCup > 7) {
            hasRegs = true;
            selectedCourse = PULSARID_FIRSTREG;
            lastSelectedCup = PULSARCUPID_FIRSTREG;  // CT cup -> regs
            lastSelectedCupButtonIdx = 0;
        }
        count = 0;
    } else if (isRegs == false) {
        count = definedCTsCupCount;
        hasRegs = false;
    } else {
        count = definedCTsCupCount;
        hasRegs = (RKNet::Controller::sInstance->roomType != RKNet::ROOMTYPE_VS_REGIONAL) &&
                  (RKNet::Controller::sInstance->roomType != RKNet::ROOMTYPE_JOINING_REGIONAL);
    }
    ctsCupCount = count;
}

void CupsConfig::SetLayout() {
    CupsConfig::sInstance->isAlphabeticalLayout = Settings::Mgr::Get().GetUserSettingValue(Settings::SETTINGSTYPE_MENU, RADIO_LAYOUT) == LAYOUT_ALPHABETICAL;
}
Settings::Hook CTLayout(CupsConfig::SetLayout);

void CupsConfig::GetExpertPath(char* dest, PulsarId id, TTMode mode) const {
    if (this->IsReg(id)) {
        const u32 crc32 = this->GetCRC32(id);
        snprintf(dest, IOS::ipcMaxPath, "/Experts/%s_%s.rkg", &crc32, System::ttModeFolders[mode]);
    } else {
        snprintf(dest, IOS::ipcMaxPath, "/Experts/%d_%s.rkg", this->ConvertTrack_PulsarIdToRealId(id), System::ttModeFolders[mode]);
    }
}

PulsarId CupsConfig::RandomizeTrack() const {
    const Settings::Mgr& settings = Settings::Mgr::Get();
    const RacedataSettings& racedataSettings = Racedata::sInstance->menusScenario.settings;
    const GameMode mode = racedataSettings.gamemode;
    u32 isBattle = (mode == MODE_BATTLE || mode == MODE_PUBLIC_BATTLE || mode == MODE_PRIVATE_BATTLE);
    Random random;
    u32 pulsarId;
    u32 isRetroOnly = TRACKSELECTION_ALL;
    u32 isCTOnly = TRACKSELECTION_ALL;
    u32 isRegsOnly = TRACKSELECTION_ALL;
    if (RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_FROOM_HOST || RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_FROOM_NONHOST) {
        if (System::sInstance->IsContext(PULSAR_RETROS)) isRetroOnly = TRACKSELECTION_RETROS;
        if (System::sInstance->IsContext(PULSAR_CTS)) isCTOnly = TRACKSELECTION_CTS;
        if (System::sInstance->IsContext(PULSAR_REGS)) isRegsOnly = TRACKSELECTION_REGS;
    }
    if (RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_NONE) {
        if (settings.GetUserSettingValue(Settings::SETTINGSTYPE_FROOM1, SCROLLER_TRACKSELECTION) == TRACKSELECTION_RETROS) isRetroOnly = TRACKSELECTION_RETROS;
        if (settings.GetUserSettingValue(Settings::SETTINGSTYPE_FROOM1, SCROLLER_TRACKSELECTION) == TRACKSELECTION_CTS) isCTOnly = TRACKSELECTION_CTS;
        if (settings.GetUserSettingValue(Settings::SETTINGSTYPE_FROOM1, SCROLLER_TRACKSELECTION) == TRACKSELECTION_REGS) isRegsOnly = TRACKSELECTION_REGS;
    }
    if (RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_JOINING_REGIONAL || RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_VS_REGIONAL) {
        if (System::sInstance->netMgr.region == 0x0A || System::sInstance->netMgr.region == 0x0B || System::sInstance->netMgr.region == 0x0C || System::sInstance->netMgr.region == 0x0D) isRetroOnly = TRACKSELECTION_RETROS;
        if (System::sInstance->netMgr.region == 0x14) isCTOnly = TRACKSELECTION_CTS;
    }
    if (isRetroOnly == TRACKSELECTION_RETROS && isRegsOnly != TRACKSELECTION_REGS && !isBattle)
        pulsarId = random.NextLimited(184) + 0x100;
    else if (isCTOnly == TRACKSELECTION_CTS && isRegsOnly != TRACKSELECTION_REGS && !isBattle)
        pulsarId = random.NextLimited(80) + 0x100 + 184;
    else if (isRegsOnly == TRACKSELECTION_REGS && !isBattle)
        pulsarId = random.NextLimited(32);
    else if (isBattle)
        pulsarId = random.NextLimited(40) + 0x100 + 264;
    else if (this->HasRegs()) {
        pulsarId = random.NextLimited(this->GetCtsTrackCount() + 32);
        if (pulsarId > 31) pulsarId += (0x100 - 32);
    } else
        pulsarId = random.NextLimited(264) + 0x100;
    return static_cast<PulsarId>(pulsarId);
}

/*
PulsarCupId CupsDef::GetNextCupId(PulsarCupId pulsarId, s32 direction) const {
    const u32 idx = ConvertCup_PulsarIdToIdx(pulsarId); //40 -> 8
    const u32 count = this->GetTotalCupCount(); //0xa
    const u32 min = count < 8 ? 8 : 0; //0
    const u32 nextIdx = ((idx + direction + count) % count) + min; //6
    if(this->hasRegs && nextIdx < 8) return static_cast<PulsarCupId>(nextIdx);
    else return
        if(IsRegCup(pulsarId) && nextIdx >= 8) return static_cast<PulsarCupId>(nextIdx + 0x38 + count);
    return ConvertCup_IdxToPulsarId(nextIdx);
}
*/

PulsarCupId CupsConfig::GetNextCupId(PulsarCupId pulsarId, s32 direction) const {
    const Settings::Mgr& settings = Settings::Mgr::Get();
    const u32 idx = ConvertCup_PulsarIdToIdx(pulsarId);
    const RacedataSettings& racedataSettings = Racedata::sInstance->menusScenario.settings;
    const GameMode mode = racedataSettings.gamemode;
    u32 isBattle = (mode == MODE_BATTLE || mode == MODE_PUBLIC_BATTLE || mode == MODE_PRIVATE_BATTLE);
    u32 isRetroOnly = TRACKSELECTION_ALL;
    u32 isCTOnly = TRACKSELECTION_ALL;
    u32 isRegsOnly = TRACKSELECTION_ALL;
    if (RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_FROOM_HOST || RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_FROOM_NONHOST) {
        if (System::sInstance->IsContext(PULSAR_RETROS)) isRetroOnly = TRACKSELECTION_RETROS;
        if (System::sInstance->IsContext(PULSAR_CTS)) isCTOnly = TRACKSELECTION_CTS;
        if (System::sInstance->IsContext(PULSAR_REGS)) isRegsOnly = TRACKSELECTION_REGS;
    }
    if (RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_JOINING_REGIONAL || RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_VS_REGIONAL) {
        if (System::sInstance->netMgr.region == 0x0A || System::sInstance->netMgr.region == 0x0B || System::sInstance->netMgr.region == 0x0C || System::sInstance->netMgr.region == 0x0D) isRetroOnly = TRACKSELECTION_RETROS;
        if (System::sInstance->netMgr.region == 0x14) isCTOnly = TRACKSELECTION_CTS;
    }
    if (isRetroOnly == TRACKSELECTION_RETROS && isRegsOnly != TRACKSELECTION_REGS && !isBattle) {
        const u32 countRetro = 46;
        const u32 minRetro = countRetro < 8 ? 8 : 0;
        const u32 nextIdxRetro = ((idx + direction + countRetro) % countRetro) + minRetro;
        if (!this->hasRegs && nextIdxRetro < 8) return static_cast<PulsarCupId>(nextIdxRetro + countRetro + 0x38);
        return ConvertCup_IdxToPulsarId(nextIdxRetro);
    } else if (isCTOnly == TRACKSELECTION_CTS && isRegsOnly != TRACKSELECTION_REGS && !isBattle) {
        const u32 countCT = 20;
        const u32 lastCupIndex = this->GetTotalCupCount() - 1;
        const u32 startIdx = 54;
        const u32 nextIdxCT = startIdx + ((idx - startIdx + direction + countCT) % countCT);
        if (!this->hasRegs && nextIdxCT < 8) return static_cast<PulsarCupId>(nextIdxCT + countCT + 0x38);
        return ConvertCup_IdxToPulsarId(nextIdxCT);
    } else if (isBattle) {
        const u32 countBT = 10;
        const u32 lastCupIndex = this->GetTotalCupCount() - 1;
        const u32 startIdx = 74;
        const u32 nextIdxBT = startIdx + ((idx - startIdx + direction + countBT) % countBT);
        if (!this->hasRegs && nextIdxBT < 8) return static_cast<PulsarCupId>(nextIdxBT + countBT + 0x38);
        return ConvertCup_IdxToPulsarId(nextIdxBT);
    } else {
        const u32 count = 66;
        const u32 min = count < 8 ? 8 : 0;
        const u32 nextIdx = ((idx + direction + count) % count) + min;
        if (!this->hasRegs && nextIdx < 8) return static_cast<PulsarCupId>(nextIdx + count + 0x38);
        return ConvertCup_IdxToPulsarId(nextIdx);
    }
}

void CupsConfig::SaveSelectedCourse(const PushButton& courseButton) {
    this->selectedCourse = ConvertTrack_PulsarCupToTrack(this->lastSelectedCup, courseButton.buttonId);  // FIX HERE
    this->SetWinning(selectedCourse);
}

static int GetCorrectMusicSlotWrapper() {
    return CupsConfig::sInstance->GetCorrectMusicSlot();
}
kmCall(0x80711fd8, GetCorrectMusicSlotWrapper);
kmCall(0x8071206c, GetCorrectMusicSlotWrapper);

u32 CupsConfig::ConvertCup_PulsarIdToRealId(PulsarCupId pulsarCupId) {
    if (IsRegCup(pulsarCupId)) {
        if ((pulsarCupId & 1) == 0)
            return pulsarCupId / 2;
        else
            return (pulsarCupId + 7) / 2;
    } else
        return pulsarCupId - 0x40;
}

u32 CupsConfig::ConvertCup_PulsarIdToIdx(PulsarCupId pulsarCupId) {
    u32 idx = pulsarCupId;
    if (!IsRegCup(pulsarCupId)) idx = pulsarCupId - 0x38;
    return idx;
}

PulsarCupId CupsConfig::ConvertCup_IdxToPulsarId(u32 cupIdx) {
    if (!IsRegCup(static_cast<PulsarCupId>(cupIdx))) {
        return static_cast<PulsarCupId>(cupIdx + 0x38);
    } else
        return static_cast<PulsarCupId>(cupIdx);
}

CourseId CupsConfig::ConvertTrack_PulsarIdToRealId(PulsarId pulsarId) {
    if (IsReg(pulsarId)) {
        if (pulsarId < 32)
            return static_cast<CourseId>(idToCourseId[pulsarId]);
        else
            return static_cast<CourseId>(pulsarId);  // battle
    } else
        return static_cast<CourseId>(pulsarId - 0x100);
}

PulsarId CupsConfig::ConvertTrack_RealIdToPulsarId(CourseId id) {
    if (id < 32)
        for (int i = 0; i < 32; ++i)
            if (id == idToCourseId[i]) return static_cast<PulsarId>(i);
    return static_cast<PulsarId>(id);
}

PulsarId CupsConfig::ConvertTrack_IdxToPulsarId(u32 idx) const {
    if (!this->HasRegs()) {
        idx += 0x100;
    } else if (idx > 31)
        idx += 0x100 - 32;
    return static_cast<PulsarId>(idx);
}

const u8 CupsConfig::idToCourseId[32] = {
    0x08, 0x01, 0x02, 0x04,  // mushroom cup
    0x10, 0x14, 0x19, 0x1A,  // shell cup
    0x00, 0x05, 0x06, 0x07,  // flower cup
    0x1B, 0x1F, 0x17, 0x12,
    0x09, 0x0F, 0x0B, 0x03,
    0x15, 0x1E, 0x1D, 0x11,
    0x0E, 0x0A, 0x0C, 0x0D,
    0x18, 0x16, 0x13, 0x1C

};

const u32 CupsConfig::RegsCRC32[] = {
    0x4C430000,  // LC
    0x4D4D4D00,  // MMM
    0x4D470000,  // MG
    0x54460000,  // TF

    0x72504200,  // rPB
    0x72594600,  // rYF
    0x72475600,  // rGV
    0x724D5200,  // rMR

    0x4D430000,  // MC
    0x434D0000,  // CM
    0x444B5300,  // DKS
    0x57474D00,  // WGM

    0x72534C00,  // rSL
    0x53474200,  // SGB
    0x72445300,  // rDS
    0x72575300,  // rWS

    0x44430000,  // DC
    0x4B430000,  // KC
    0x4D540000,  // MT
    0x47560000,  // GV

    0x72444800,  // rDH
    0x42433300,  // BC3
    0x72444B00,  // rDK
    0x724D4300,  // rMC

    0x44445200,  // DDR
    0x4D480000,  // MH
    0x42430000,  // BC
    0x52520000,  // RR

    0x4D433300,  // MC3
    0x72504700,  // rPG
    0x444B4D00,  // DKM
    0x72424300  // rBC
};

}  // namespace Pulsar