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

static bool ParseHexKey(const char* text, u32& value) {
    value = 0;
    if (text == nullptr || *text == '\0') return false;
    while (*text != '\0') {
        char c = *text++;
        if (c == '\r' || c == '\n') break;
        value <<= 4;
        if (c >= '0' && c <= '9')
            value |= static_cast<u32>(c - '0');
        else if (c >= 'A' && c <= 'F')
            value |= static_cast<u32>(c - 'A' + 10);
        else if (c >= 'a' && c <= 'f')
            value |= static_cast<u32>(c - 'a' + 10);
        else
            return false;
    }
    return true;
}

static void TrimLine(char* line) {
    if (line == nullptr) return;
    char* start = line;
    while (*start == ' ' || *start == '\t') ++start;
    if (start != line) {
        const size_t len = strlen(start);
        memmove(line, start, len + 1);
    }
    size_t length = strlen(line);
    while (length > 0) {
        char c = line[length - 1];
        if (c == '\r' || c == '\n' || c == ' ' || c == '\t') {
            line[length - 1] = '\0';
            --length;
        } else
            break;
    }
}

static char* DuplicateFileName(const char* src) {
    if (src == nullptr || *src == '\0') return nullptr;
    size_t len = strlen(src);
    if (len == 0) return nullptr;
    const size_t maxLen = IOS::ipcMaxFileName;
    if (len > maxLen) len = maxLen;
    char* copy = new char[maxLen + 1];
    memcpy(copy, src, len);
    copy[len] = '\0';
    return copy;
}

CupsConfig* CupsConfig::sInstance = nullptr;

CupsConfig::CupsConfig(const CupsHolder& rawCups) : regsMode(rawCups.regsMode),
                                                    // Cup actions initialization
                                                    hasRegs(false),
                                                    hasOddCups(false),
                                                    winningCourse(PULSARID_NONE),
                                                    selectedCourse(PULSARID_FIRSTREG),
                                                    curVariantIdx(0),
                                                    pendingVariantIdx(0),
                                                    hasPendingVariant(false),
                                                    lastSelectedCup(PULSARCUPID_FIRSTREG),
                                                    lastSelectedCupButtonIdx(0),
                                                    isAlphabeticalLayout(false) {
    totalVariantCount = rawCups.totalVariantCount;
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
    trackFileNames = new char*[ctsCount];
    memset(trackFileNames, 0, sizeof(char*) * ctsCount);
    trackNameBmgIds = new u32[ctsCount];
    memset(trackNameBmgIds, 0, sizeof(u32) * ctsCount);
    variantFileNames = nullptr;
    variantNameBmgIds = nullptr;
    if (totalVariantCount != 0) {
        variantFileNames = new char*[totalVariantCount];
        memset(variantFileNames, 0, sizeof(char*) * totalVariantCount);
        variantNameBmgIds = new u32[totalVariantCount];
        memset(variantNameBmgIds, 0, sizeof(u32) * totalVariantCount);
    }

    memcpy(mainTracks, &rawCups.tracks, sizeof(Track) * ctsCount);
    memcpy(variants, (reinterpret_cast<const u8*>(&rawCups.tracks) + sizeof(Track) * ctsCount), sizeof(Variant) * rawCups.totalVariantCount);

    for (int i = 0; i < 172; ++i) {
        alphabeticalArray[i] = i;
        invertedAlphabeticalArray[i] = i;
    }

    const u16* originalAlphabeticalArray = reinterpret_cast<const u16*>(reinterpret_cast<const u8*>(&rawCups.tracks) + sizeof(Track) * ctsCount);

    u16 lastTrackIndices[88];
    for (int i = 0; i < 88; ++i) {
        lastTrackIndices[i] = i + 172;
    }
    for (int i = 0; i < 88; ++i) {
        for (int j = i + 1; j < 88; ++j) {
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
    for (int i = 0; i < 88; ++i) {
        alphabeticalArray[172 + i] = lastTrackIndices[i];
    }
    for (int i = 172 + 88; i < ctsCount; ++i) {
        alphabeticalArray[i] = i;
    }

    u16 cumulativeVarCount = 0;
    for (int i = 0; i < ctsCount; ++i) {
        if (i >= 172) {
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

void CupsConfig::GetTrackGhostFolder(char* dest, PulsarId pulsarId, u8 variantIdx) const {
    const u32 crc32 = this->GetCRC32(pulsarId);
    const char* modFolder = System::sInstance->GetModFolder();
    if (IsReg(pulsarId))
        snprintf(dest, IOS::ipcMaxPath, "%s/Ghosts/%s", modFolder, &crc32);
    else if (variantIdx == 0)
        snprintf(dest, IOS::ipcMaxPath, "%s/Ghosts/%08x", modFolder, crc32);
    else
        snprintf(dest, IOS::ipcMaxPath, "%s/Ghosts/%08x/v%d", modFolder, crc32, variantIdx);
}

void CupsConfig::LoadFileNames(const char* buffer, u32 length) {
    if (buffer == nullptr || length == 0 || this->GetCtsTrackCount() == 0) return;
    char* temp = new char[length + 1];
    memcpy(temp, buffer, length);
    temp[length] = '\0';
    char* cursor = temp;
    if (length >= 3 && static_cast<u8>(cursor[0]) == 0xEF && static_cast<u8>(cursor[1]) == 0xBB && static_cast<u8>(cursor[2]) == 0xBF)
        cursor += 3;
    if (length < 4 || cursor[0] != 'F' || cursor[1] != 'I' || cursor[2] != 'L' || cursor[3] != 'E') {
        delete[] temp;
        return;
    }
    char* lineBreak = strchr(cursor, '\n');
    if (lineBreak == nullptr) {
        delete[] temp;
        return;
    }
    cursor = lineBreak + 1;
    while (*cursor != '\0') {
        char* lineStart = cursor;
        char* next = strchr(cursor, '\n');
        if (next != nullptr) {
            *next = '\0';
            cursor = next + 1;
        } else {
            cursor += strlen(cursor);
        }
        TrimLine(lineStart);
        if (*lineStart == '\0') continue;
        char* equals = strchr(lineStart, '=');
        if (equals == nullptr) continue;
        *equals = '\0';
        char* keyStr = lineStart;
        char* valueStr = equals + 1;
        TrimLine(keyStr);
        TrimLine(valueStr);
        if (*keyStr == '\0' || *valueStr == '\0') continue;
        u32 key = 0;
        if (!ParseHexKey(keyStr, key)) continue;
        char* pipe = strchr(valueStr, '|');
        u32 bmgId = 0;
        if (pipe != nullptr) {
            char* bmgValue = pipe + 1;
            *pipe = '\0';
            TrimLine(valueStr);
            TrimLine(bmgValue);
            ParseHexKey(bmgValue, bmgId);
        } else {
            TrimLine(valueStr);
        }
        const u32 trackIdx = key & 0x0FFF;
        const u32 variantIdx = key >> 12;
        this->RegisterFileName(trackIdx, variantIdx, valueStr, bmgId);
    }
    delete[] temp;
}

void CupsConfig::RegisterFileName(u32 trackIdx, u32 variantIdx, const char* name, u32 bmgId) {
    if (trackIdx >= static_cast<u32>(this->GetCtsTrackCount()) || name == nullptr || *name == '\0') return;
    char* stored = DuplicateFileName(name);
    if (stored == nullptr) return;
    if (variantIdx == 0) {
        if (trackFileNames[trackIdx] != nullptr) delete[] trackFileNames[trackIdx];
        trackFileNames[trackIdx] = stored;
        trackNameBmgIds[trackIdx] = bmgId;
        return;
    }
    if (variantIdx - 1 >= this->mainTracks[trackIdx].variantCount || this->variantFileNames == nullptr) {
        delete[] stored;
        return;
    }
    const u32 base = this->variantsOffs[trackIdx] / sizeof(Variant);
    const u32 variantArrayIdx = base + (variantIdx - 1);
    if (variantArrayIdx >= this->totalVariantCount) {
        delete[] stored;
        return;
    }
    if (variantFileNames[variantArrayIdx] != nullptr) delete[] variantFileNames[variantArrayIdx];
    variantFileNames[variantArrayIdx] = stored;
    if (variantNameBmgIds != nullptr) variantNameBmgIds[variantArrayIdx] = bmgId;
}

void CupsConfig::RegisterVariantBmg(u32 trackIdx, u32 variantIdx, u32 bmgId) {
    if (trackIdx >= static_cast<u32>(this->GetCtsTrackCount())) return;
    if (variantIdx == 0) {
        trackNameBmgIds[trackIdx] = bmgId;
        return;
    }
    if (variantNameBmgIds == nullptr) return;
    if (variantIdx - 1 >= this->mainTracks[trackIdx].variantCount) return;
    const u32 base = this->variantsOffs[trackIdx] / sizeof(Variant);
    const u32 variantArrayIdx = base + (variantIdx - 1);
    if (variantArrayIdx >= this->totalVariantCount) return;
    variantNameBmgIds[variantArrayIdx] = bmgId;
}

u32 CupsConfig::GetVariantNameBmgId(PulsarId pulsarId, u8 variantIdx) const {
    if (IsReg(pulsarId)) return 0;
    const u32 trackIdx = ConvertTrack_PulsarIdToRealId(pulsarId);
    if (trackIdx >= static_cast<u32>(this->GetCtsTrackCount())) return 0;
    if (variantIdx == 0) {
        return trackNameBmgIds[trackIdx];
    }
    if (variantNameBmgIds == nullptr) return 0;
    if (variantIdx - 1 >= this->mainTracks[trackIdx].variantCount) return 0;
    const u32 base = this->variantsOffs[trackIdx] / sizeof(Variant);
    const u32 variantArrayIdx = base + (variantIdx - 1);
    if (variantArrayIdx >= this->totalVariantCount) return 0;
    return variantNameBmgIds[variantArrayIdx];
}

const char* CupsConfig::GetFileName(PulsarId id, u8 variantIdx) const {
    if (IsReg(id)) return nullptr;
    const u32 trackIdx = ConvertTrack_PulsarIdToRealId(id);
    if (trackIdx >= static_cast<u32>(this->GetCtsTrackCount())) return nullptr;
    if (variantIdx == 0) return trackFileNames[trackIdx];
    if (variantIdx - 1 >= this->mainTracks[trackIdx].variantCount || this->variantFileNames == nullptr) return nullptr;
    const u32 base = this->variantsOffs[trackIdx] / sizeof(Variant);
    const u32 variantArrayIdx = base + (variantIdx - 1);
    if (variantArrayIdx >= this->totalVariantCount) return nullptr;
    return variantFileNames[variantArrayIdx];
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

void CupsConfig::SetPendingVariant(u8 variantIdx) {
    this->pendingVariantIdx = variantIdx;
    this->hasPendingVariant = true;
}

void CupsConfig::ClearPendingVariant() {
    this->pendingVariantIdx = 0;
    this->hasPendingVariant = false;
}

void CupsConfig::SetWinning(PulsarId id, u32 variantIdx) {
    if (variantIdx == 0xFF) variantIdx = 0;

    if (!IsReg(id)) {
        const Track& track = GetTrack(id);
        cur.crc32 = track.crc32;
        const GameMode mode = Racedata::sInstance->menusScenario.settings.gamemode;

        u8 slot;
        u8 musicSlot;
        if (variantIdx == 0) {
            slot = track.slot;
            musicSlot = track.musicSlot;
        } else {
            const u32 base = this->variantsOffs[ConvertTrack_PulsarIdToRealId(id)] / sizeof(Variant);
            const Variant& variant = this->variants[base + (variantIdx - 1)];
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
    if ((RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_FROOM_HOST || RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_FROOM_NONHOST || RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_NONE) && !isBattle) {
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

void CupsConfig::GetExpertPath(char* dest, PulsarId id, TTMode mode, u8 variantIdx) const {
    if (this->IsReg(id)) {
        const u32 crc32 = this->GetCRC32(id);
        snprintf(dest, IOS::ipcMaxPath, "/Experts/%s_%s.rkg", &crc32, System::ttModeFolders[mode]);
    } else if (variantIdx == 0) {
        snprintf(dest, IOS::ipcMaxPath, "/Experts/%d_%s.rkg", this->ConvertTrack_PulsarIdToRealId(id), System::ttModeFolders[mode]);
    } else {
        snprintf(dest, IOS::ipcMaxPath, "/Experts/%d_v%d_%s.rkg", this->ConvertTrack_PulsarIdToRealId(id), variantIdx, System::ttModeFolders[mode]);
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
    if (RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_FROOM_HOST || RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_FROOM_NONHOST || RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_NONE) {
        if (System::sInstance->IsContext(PULSAR_RETROS)) isRetroOnly = TRACKSELECTION_RETROS;
        if (System::sInstance->IsContext(PULSAR_CTS)) isCTOnly = TRACKSELECTION_CTS;
        if (System::sInstance->IsContext(PULSAR_REGS)) isRegsOnly = TRACKSELECTION_REGS;
    }
    if (RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_JOINING_REGIONAL || RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_VS_REGIONAL) {
        if (System::sInstance->netMgr.region == 0x0A || System::sInstance->netMgr.region == 0x0B || System::sInstance->netMgr.region == 0x0C || System::sInstance->netMgr.region == 0x0D) isRetroOnly = TRACKSELECTION_RETROS;
        if (System::sInstance->netMgr.region == 0x14) isCTOnly = TRACKSELECTION_CTS;
    }
    if (isRetroOnly == TRACKSELECTION_RETROS && isRegsOnly != TRACKSELECTION_REGS && !isBattle)
        pulsarId = random.NextLimited(172) + 0x100;
    else if (isCTOnly == TRACKSELECTION_CTS && isRegsOnly != TRACKSELECTION_REGS && !isBattle)
        pulsarId = random.NextLimited(88) + 0x100 + 172;
    else if (isRegsOnly == TRACKSELECTION_REGS && !isBattle)
        pulsarId = random.NextLimited(32);
    else if (isBattle)
        pulsarId = random.NextLimited(40) + 0x100 + 260;
    else if (this->HasRegs()) {
        pulsarId = random.NextLimited(this->GetCtsTrackCount() + 32);
        if (pulsarId > 31) pulsarId += (0x100 - 32);
    } else
        pulsarId = random.NextLimited(260) + 0x100;
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
        const u32 countRetro = 43;
        const u32 minRetro = countRetro < 8 ? 8 : 0;
        const u32 nextIdxRetro = ((idx + direction + countRetro) % countRetro) + minRetro;
        if (!this->hasRegs && nextIdxRetro < 8) return static_cast<PulsarCupId>(nextIdxRetro + countRetro + 0x38);
        return ConvertCup_IdxToPulsarId(nextIdxRetro);
    } else if (isCTOnly == TRACKSELECTION_CTS && isRegsOnly != TRACKSELECTION_REGS && !isBattle) {
        const u32 countCT = 22;
        const u32 lastCupIndex = this->GetTotalCupCount() - 1;
        const u32 startIdx = 51;
        const u32 nextIdxCT = startIdx + ((idx - startIdx + direction + countCT) % countCT);
        if (!this->hasRegs && nextIdxCT < 8) return static_cast<PulsarCupId>(nextIdxCT + countCT + 0x38);
        return ConvertCup_IdxToPulsarId(nextIdxCT);
    } else if (isBattle) {
        const u32 countBT = 10;
        const u32 lastCupIndex = this->GetTotalCupCount() - 1;
        const u32 startIdx = 73;
        const u32 nextIdxBT = startIdx + ((idx - startIdx + direction + countBT) % countBT);
        if (!this->hasRegs && nextIdxBT < 8) return static_cast<PulsarCupId>(nextIdxBT + countBT + 0x38);
        return ConvertCup_IdxToPulsarId(nextIdxBT);
    } else {
        const u32 count = 64;
        const u32 min = count < 8 ? 8 : 0;
        const u32 nextIdx = ((idx + direction + count) % count) + min;
        if (!this->hasRegs && nextIdx < 8) return static_cast<PulsarCupId>(nextIdx + count + 0x38);
        return ConvertCup_IdxToPulsarId(nextIdx);
    }
}

void CupsConfig::SaveSelectedCourse(const PushButton& courseButton) {
    this->selectedCourse = ConvertTrack_PulsarCupToTrack(this->lastSelectedCup, courseButton.buttonId);  // FIX HERE
    u32 variantIdx = 0;
    if (this->hasPendingVariant) {
        variantIdx = this->pendingVariantIdx;
        this->hasPendingVariant = false;
    }
    this->SetWinning(selectedCourse, variantIdx);
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