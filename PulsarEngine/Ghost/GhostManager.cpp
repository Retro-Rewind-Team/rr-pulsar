#include <Ghost/GhostManager.hpp>
#include <Settings/Settings.hpp>
#include <IO/IO.hpp>
#include <SlotExpansion/CupsConfig.hpp>
#include <MarioKartWii/Kart/KartManager.hpp>
#include <UI/TransmissionSelect/TransmissionSelect.hpp>

namespace Pulsar {
namespace Ghosts {
Mgr* Mgr::sInstance = nullptr;

Mgr::RKGCallback Mgr::cb = nullptr;

char Mgr::folderPath[IOS::ipcMaxPath] = "";

Mgr* Mgr::CreateInstance() {
    Mgr* self = Mgr::sInstance;
    if (self == nullptr) {
        self = new (System::sInstance->heap, 0x20) Mgr;
        Mgr::sInstance = self;
    }
    self->Reset();
    return self;
}

void Mgr::DestroyInstance() {
    delete (Mgr::sInstance);
    Mgr::sInstance = nullptr;
}

void Mgr::Init(PulsarId id, u8 variantIdx) {
    this->Reset();
    this->pulsarId = id;
    this->variantIdx = variantIdx;
    IO* io = IO::sInstance;
    const System* system = System::sInstance;
    const CupsConfig* cupsConfig = CupsConfig::sInstance;
    const TTMode ttMode = system->ttMode;
    
    if (variantIdx > 0) {
        char parentPath[IOS::ipcMaxPath];
        cupsConfig->GetTrackGhostFolder(parentPath, id, 0);
        if (!io->FolderExists(parentPath)) io->CreateFolder(parentPath);
    }
    cupsConfig->GetTrackGhostFolder(folderPath, id, variantIdx);

    bool exists = io->FolderExists(folderPath);
    if (!exists) io->CreateFolder(folderPath);
    char folderModePath[IOS::ipcMaxPath];
    snprintf(folderModePath, IOS::ipcMaxPath, "%s/%s", folderPath, System::ttModeFolders[ttMode]);
    exists = io->FolderExists(folderModePath);
    if (!exists)
        io->CreateFolder(folderModePath);
    else
        io->ReadFolder(folderModePath);

    new (&this->leaderboard) Leaderboard(folderPath, id, true);
    RKG* decompressed = new (system->heap) RKG;
    this->files = new (system->heap) GhostData[1 + io->GetFileCount()];

    s32 expertCRC32 = -1;
    DVD::FileInfo info;
    char expertName[IOS::ipcMaxPath];
    cupsConfig->GetExpertPath(expertName, id, ttMode, variantIdx);
    this->expertEntryNum = DVD::ConvertPathToEntryNum(expertName);
    if (this->expertEntryNum >= 0) {
        DVD::FastOpen(this->expertEntryNum, &info);

        GhostData& curData = this->files[0];
        this->rkg.ClearBuffer();

        DVD::ReadPrio(&info, &this->rkg, info.length, 0, 2);
        if (this->rkg.CheckValidity()) {
            curData.Init(rkg);
            expertCRC32 = this->GetRKGcrc32(this->rkg);
            if (this->cb != nullptr) {
                rkg.DecompressTo(*decompressed);
                this->cb(*decompressed, IS_LOADING_LEADERBOARDS, 0);
            }
            curData.courseId = static_cast<CourseId>(id);
            if (curData.type == EXPERT_STAFF_GHOST) {
                this->expertGhost.minutes = rkg.header.minutes;
                this->expertGhost.seconds = rkg.header.seconds;
                this->expertGhost.milliseconds = rkg.header.milliseconds;
                this->expertGhost.isActive = true;
            }
            curData.padding = expertFileIdx;
            if (ttMode <= TTMODE_200 && this->leaderboard.GetFavGhost(ttMode)[0] == '?') this->favGhostFileIndex[ttMode] = expertFileIdx;
        }
        DVD::Close(&info);
    }
    u32 counter = this->HasExpert();  // starts at 1 if the expert for this track exists

    for (int i = 0; i < io->GetFileCount(); ++i) {
        this->rkg.ClearBuffer();
        GhostData& curData = this->files[counter];
        s32 ret = io->ReadFolderFile(&this->rkg, i, sizeof(RKG));
        if (ret > 0 && this->rkg.CheckValidity() && this->GetRKGcrc32(this->rkg) != expertCRC32) {
            curData.Init(rkg);
            if (this->cb != nullptr) {
                rkg.DecompressTo(*decompressed);
                this->cb(*decompressed, IS_LOADING_LEADERBOARDS, counter);
            }
            curData.courseId = static_cast<CourseId>(id);
            curData.padding = i;
            if (ttMode <= TTMODE_200 && this->favGhostFileIndex[ttMode] == 0xFF) {
                if (strcmp(this->leaderboard.GetFavGhost(ttMode), Mgr::GetGhostFileName(i)) == 0) {
                    this->favGhostFileIndex[ttMode] = i;
                }
            }
            ++counter;
        }
    }
    this->rkgCount = counter;
    delete decompressed;
}

void Mgr::Reset() {
    IO::sInstance->CloseFolder();
    this->pulsarId = PULSARID_NONE;
    this->variantIdx = 0;
    this->lastUsedSlot = 0;
    this->expertGhost.isActive = false;
    mainGhostIndex = 0xFF;
    for (int i = 0; i < 3; ++i) selGhostsIndex[i] = 0xFF;
    this->favGhostFileIndex[0] = 0xFF;
    this->favGhostFileIndex[1] = 0xFF;
    new (&this->expertGhost) Timer;
    delete[] this->files;
    this->files = nullptr;
    Racedata* racedata = Racedata::sInstance;
    racedata->menusScenario.players[1].playerType = PLAYER_NONE;
    racedata->menusScenario.players[2].playerType = PLAYER_NONE;
    racedata->menusScenario.players[3].playerType = PLAYER_NONE;
}

void Mgr::SaveLeaderboard() {
    char folderPath[IOS::ipcMaxPath];
    CupsConfig::sInstance->GetTrackGhostFolder(folderPath, this->pulsarId, this->variantIdx);
    this->leaderboard.Save(folderPath);
}

// Enables ghost for loading when GhostSelect's ToggleButton is pressed to true and when Challenge/Watch is pressed
bool Mgr::EnableGhost(const GhostListEntry& entry, bool isMain) {
    const u32 index = entry.padding[0];
    const bool exists = index != 0xFF;
    if (exists) {
        if (isMain)
            this->mainGhostIndex = index;
        else {
            this->selGhostsIndex[this->lastUsedSlot] = index;
            this->lastUsedSlot = (this->lastUsedSlot + 1) % 3;
        }
    }
    return exists;
}

void Mgr::DisableGhost(const GhostListEntry& entry) {
    const u32 index = entry.padding[0];
    for (int i = 0; i < 3; ++i) {
        if (this->selGhostsIndex[i] == index) {
            this->lastUsedSlot = i;
            this->selGhostsIndex[i] = 0xFF;
        }
    }
}

// Loads and checks validity of a RKG
bool Mgr::LoadGhost(RKG& rkg, u32 fileIndex) const {
    rkg.ClearBuffer();
    if (fileIndex == expertFileIdx && this->HasExpert()) {
        DVD::FileInfo info;
        DVD::FastOpen(this->expertEntryNum, &info);
        DVD::ReadPrio(&info, &rkg, info.length, 0, 2);
        DVD::Close(&info);
    } else
        IO::sInstance->ReadFolderFile(&rkg, fileIndex, sizeof(RKG));
    return rkg.CheckValidity();
}

void Mgr::LoadAllGhosts(u32 maxGhosts, bool isGhostRace) {
    u8 position = 1;
    for (int i = 0; i < maxGhosts; ++i) {
        if (this->selGhostsIndex[i] != 0xFF) {
            if (this->LoadGhost(this->rkg, this->GetGhostData(this->selGhostsIndex[i]).padding)) {
                Racedata* racedata = Racedata::sInstance;
                RKG& dest = racedata->ghosts[position];
                if (this->rkg.header.compressed) {
                    this->rkg.DecompressTo(dest);
                } else
                    memcpy(&dest, &this->rkg, sizeof(RKG));
                if (this->cb != nullptr) {
                    this->cb(dest, IS_SETTING_RACE, i);
                }

                racedata->menusScenario.players[position + isGhostRace].playerType = PLAYER_GHOST;
                SectionMgr::sInstance->sectionParams->playerMiis.LoadMii(position + isGhostRace, &dest.header.miiData);
                ++position;
            }
        }
    }
}

bool Mgr::SaveGhost(const RKSYS::LicenseLdbEntry& entry, u32 ldbPosition, bool isFlap) {
    if (!areGhostsSaving) return false;
    if (isFlap) this->leaderboard.Update(ENTRY_FLAP, this->entry, -1);
    GhostData data;
    data.Fill(0);
    RKG buffer;
    buffer.ClearBuffer();

    bool gotTrophy = false;
    buffer.header.unknown_3 = Pulsar::UI::GetSelectedTransmission(0);

    if (data.CreateRKG(buffer) && buffer.CompressTo(this->rkg)) {
        if (this->cb != nullptr) {
            this->cb(buffer, IS_SAVING_GHOST, -1);
        }
        u32 crc32 = Mgr::GetRKGcrc32(this->rkg);
        if (ldbPosition <= ENTRY_10TH) this->leaderboard.Update(ldbPosition, entry, crc32);
        const System* system = System::sInstance;
        system->taskThread->Request(&Mgr::CreateAndSaveFiles, this, 0);

        const Timer& expert = this->GetExpert();
        if (expert.isActive && expert > entry.timer && system->GetInfo().HasTrophies()) {
            gotTrophy = true;
            Settings::Mgr::sInstance->AddTrophy(CupsConfig::sInstance->GetCRC32(this->GetPulsarId()), this->GetVariantIdx(), system->ttMode);
            this->leaderboard.AddTrophy();
        }
    }
    return gotTrophy;
}

void Mgr::CreateAndSaveFiles(Mgr* self) {
    char path[IOS::ipcMaxPath];
    const RKG& rkg = self->rkg;
    s8 repeatCount = self->leaderboard.GetRepeatCount(rkg);

    IO* io = IO::sInstance;
    const u32 minutes = rkg.header.minutes;
    const u32 seconds = rkg.header.seconds;
    u32 milliseconds = rkg.header.milliseconds;
    const char* format = "%s/%s/%01dm%02ds%03d.rkg";
    char letter = '?';
    if (repeatCount > 1) {
        format = "%s/%s/%01dm%02ds%02d%c.rkg";
        letter += repeatCount;
        milliseconds = milliseconds / 10;
    }
    const TTMode ttMode = System::sInstance->ttMode;
    snprintf(path, IOS::ipcMaxPath, format, folderPath, System::ttModeFolders[ttMode], minutes, seconds, milliseconds, letter);

    io->CreateAndOpen(path, FILE_MODE_WRITE);
    io->Overwrite(GetRKGLength(rkg), &rkg);
    io->Close();

    self->SaveLeaderboard();
    Settings::Mgr::sInstance->SaveTrophies();

    char prevGhostFile[IOS::ipcMaxPath];
    prevGhostFile[0] = '\0';
    u32 prevFileIndex = expertFileIdx;
    if (self->mainGhostIndex != 0xFF && self->mainGhostIndex < self->rkgCount) {
        prevFileIndex = self->files[self->mainGhostIndex].padding;
        if (prevFileIndex != expertFileIdx) {
            snprintf(prevGhostFile, IOS::ipcMaxPath, "%s/%s/%s", folderPath, System::ttModeFolders[ttMode], io->GetFileName(prevFileIndex));
        }
    }

    self->Init(CupsConfig::sInstance->GetWinning(), CupsConfig::sInstance->GetCurVariantIdx());

    if (prevFileIndex == expertFileIdx)
        self->mainGhostIndex = 0;
    else if (prevGhostFile[0] != '\0') {
        for (int i = 1; i < self->rkgCount; ++i) {
            char curFileName[IOS::ipcMaxPath];
            snprintf(curFileName, IOS::ipcMaxPath, "%s/%s/%s", folderPath, System::ttModeFolders[System::sInstance->ttMode], io->GetFileName(self->files[i].padding));
            if (strcmp(prevGhostFile, curFileName) == 0) {
                self->mainGhostIndex = i;
                break;
            }
        }
    }

    SectionMgr::sInstance->sectionParams->isNewTime = true;
}

void Mgr::InsertCustomGroupToList(GhostList* list, CourseId) {
    Mgr* self = Mgr::sInstance;
    const CupsConfig* cupsConfig = CupsConfig::sInstance;
    self->Init(cupsConfig->GetWinning(), cupsConfig->GetCurVariantIdx());
    u32 index = 0;
    const u32 rkgCount = IO::sInstance->GetFileCount();
    for (int i = 0; i < rkgCount; ++i) {
        if (index == 37) break;
        const GhostData& data = self->GetGhostData(i);
        if (data.isValid) {
            list->entries[index].data = &data;
            list->entries[index].index = index;
            list->entries[index].padding[0] = i;
            ++index;
        }
    }
    if (self->expertGhost.isActive) {
        const GhostData& data = self->GetGhostData(rkgCount);
        if (data.isValid) {
            list->entries[index].data = &data;
            list->entries[index].index = index;
            list->entries[index].padding[0] = rkgCount;
            ++index;
        }
    }
    list->count = index;
    for (int i = list->count; i < 38; ++i) list->entries[i].index = 0xFF;
    qsort(list, list->count, sizeof(GhostListEntry), reinterpret_cast<int (*)(const void*, const void*)>(&GhostList::CompareEntries));
}
kmCall(0x806394f0, Mgr::InsertCustomGroupToList);

static s32 PlayCorrectFinishAnim(RKSYS::LicenseMgr*, const Timer& timer, CourseId courseId) {
    return Mgr::GetInstance()->GetLeaderboard().GetPosition(timer);
}
kmCall(0x80856fec, PlayCorrectFinishAnim);

static int IncreaseRacedataSize() {
    return 0xC3F0;
}
kmCall(0x8052fe78, IncreaseRacedataSize);
kmWrite32(0x80531f44, 0x4800001c);  // make it so the game will only use the first rkg buffer for normal ghost usage

// Patch needed since we now have 4 rkgs which are used in order
static bool RacedataCheckCorrectRKG() {
    u8 offset = 0;
    u32 index;
    if (System::sInstance->IsContext(PULSAR_MODE_OTT))
        index = 0;
    else {
        register u32 id;
        asm(mr id, r27;);
        if (Racedata::sInstance->menusScenario.players[0].playerType != PLAYER_GHOST) offset = 1;
        index = id - offset;
    }
    return Racedata::sInstance->ghosts[index].CheckValidity();
}
kmCall(0x8052f5c8, RacedataCheckCorrectRKG);

static void GhostHeaderGetCorrectRKG(GhostData& header) {
    u8 offset = 0;
    Racedata* racedata = Racedata::sInstance;
    RacedataScenario& scenario = racedata->menusScenario;
    u32 index;
    if (System::sInstance->IsContext(PULSAR_MODE_OTT))
        index = 0;
    else {
        register u32 id;
        asm(mr id, r27;);
        if (scenario.players[0].playerType != PLAYER_GHOST) offset = 1;
        scenario.players[id].hudSlotId = id;
        scenario.settings.hudPlayerIds[id] = id;
        index = id - offset;
    }
    header.Init(racedata->ghosts[index]);
    header.courseId = scenario.settings.courseId;
}
kmCall(0x8052f5e4, GhostHeaderGetCorrectRKG);

void Mgr::LoadCorrectMainGhost(Pages::GhostManager& ghostManager, u8 r4) {
    Mgr* self = Mgr::sInstance;
    self->LoadGhost(*ghostManager.rkgPointer, self->GetGhostData(self->mainGhostIndex).padding);
    if (ghostManager.state == SAVED_GHOST_RACE_FROM_MENU) ghostManager.state = STAFF_GHOST_RACE_FROM_MENU;
}
kmCall(0x805e158c, Mgr::LoadCorrectMainGhost);

void Mgr::ExtendSetupGhostRace(Pages::GhostManager& ghostManager, bool isStaffGhost, bool replaceGhostMiiByPlayer, bool disablePlayerMii) {
    ghostManager.SetupGhostRace(true, replaceGhostMiiByPlayer, disablePlayerMii);
    Mgr::sInstance->LoadAllGhosts(2, true);
}
kmCall(0x805e13ac, Mgr::ExtendSetupGhostRace);
kmCall(0x805e13e4, Mgr::ExtendSetupGhostRace);
kmCall(0x805e141c, Mgr::ExtendSetupGhostRace);
kmCall(0x805e149c, Mgr::ExtendSetupGhostRace);
kmCall(0x805e14c8, Mgr::ExtendSetupGhostRace);
kmCall(0x805e14f4, Mgr::ExtendSetupGhostRace);

void Mgr::ExtendSetupGhostReplay(Pages::GhostManager& ghostManager, bool isStaffGhosts) {
    ghostManager.SetupGhostReplay(true);
    Mgr::sInstance->LoadAllGhosts(3, false);
}
kmCall(0x805e144c, Mgr::ExtendSetupGhostReplay);
kmCall(0x805e1518, Mgr::ExtendSetupGhostReplay);

static void SetCorrectGhostRaceSlot(const GhostList& list, s32 entryIdx) {
    list.SetSectionParamsGhostValues(entryIdx);
    if (entryIdx >= 0 && entryIdx < list.count) {
        const CourseId slot = CupsConfig::sInstance->GetCorrectTrackSlot();
        SectionMgr::sInstance->sectionParams->courseId = slot;
    }
}
kmCall(0x805c7b6c, SetCorrectGhostRaceSlot);
kmCall(0x805c7d2c, SetCorrectGhostRaceSlot);
kmCall(0x80639e54, SetCorrectGhostRaceSlot);
kmCall(0x80639fb0, SetCorrectGhostRaceSlot);

kmWrite32(0x8085b260, 0x60000000);  // nop the check that replaces the currently raced ghost ONLY if it's a new best time and your pb
}  // namespace Ghosts
}  // namespace Pulsar
