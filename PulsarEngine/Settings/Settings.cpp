#include <Settings/Settings.hpp>
#include <CustomCharacters.hpp>
#include <PulsarSystem.hpp>
#include <SlotExpansion/CupsConfig.hpp>
#include <IO/IO.hpp>

namespace Pulsar {
namespace Settings {

DoFuncsHook* Hook::settingsHooks = nullptr;

Mgr* Mgr::sInstance = nullptr;

static const char trophyFolderName[] = "Trophies";
static const char trophyFileName[] = "Trophy.pul";
static const char migratedLegacySuffix[] = ".migrated";

void Mgr::SaveTask(void* data) {
    if (data == nullptr) sInstance->Save();
    else if (data == reinterpret_cast<void*>(1)) sInstance->SaveTrophies();
    else sInstance->Save();
}

int Mgr::GetSettingsBinSize(u32 trackCount) const {
    u32 size = sizeof(BinaryHeader) + sizeof(u32) * (Binary::sectionCount - 1) + sizeof(PagesHolder) + sizeof(Page) * (this->pulsarPageCount + this->userPageCount - 1) + sizeof(MiscParams) + sizeof(TrophiesHolder) + sizeof(TrackTrophy) * (trackCount - 1) + sizeof(GPSection) + sizeof(GPCupStatus) * (trackCount / 4 - 1);
    return size;
}

void Mgr::Save() {
    IO* io = IO::sInstance;
    io->OpenFile(this->filePath, FILE_MODE_WRITE);
    io->Overwrite(this->rawBin->header.fileSize, this->rawBin);
    io->Close();
};

void Mgr::SaveTrophies() {
    if (this->trophyEntries == nullptr) return;
    for (u32 i = 0; i < this->trophyEntryCount; ++i) {
        TrophyEntry& trophy = this->trophyEntries[i];
        bool hasAnyTrophy = false;
        for (int mode = 0; mode < 4; ++mode) {
            if (trophy.hasTrophy[mode]) {
                hasAnyTrophy = true;
                break;
            }
        }
        if (hasAnyTrophy) this->WriteTrophyFile(trophy);
    }
}

void Mgr::Init(const u16* totalTrophyCount, const char* settingsPath, const char* trophiesPath) {
    this->pulsarPageCount = Settings::Params::pulsarPageCount;
    this->userPageCount = Settings::Params::userPageCount;

    strncpy(this->filePath, settingsPath, IOS::ipcMaxPath);;
    this->filePath[IOS::ipcMaxPath - 1] = '\0';
    strncpy(this->trophiesFilePath, trophiesPath, IOS::ipcMaxPath);
    this->trophiesFilePath[IOS::ipcMaxPath - 1] = '\0';

    const u32 trackCount = CupsConfig::sInstance->GetEffectiveTrackCount();
    const u32 size = this->GetSettingsBinSize(trackCount);
    System* system = System::sInstance;
    IO* io = IO::sInstance;

    Binary* buffer;
    bool ret = io->OpenFile(this->filePath, FILE_MODE_READ_WRITE);
    if (!ret) {
        io->CreateAndOpen(this->filePath, FILE_MODE_READ_WRITE);
    } else {
        alignas(0x20) BinaryHeader header;
        ret = io->Read(sizeof(BinaryHeader), &header) == sizeof(BinaryHeader);
        if (header.magic != Binary::binMagic)
            ret = false;
        else {
            buffer = io->Alloc<Binary>(header.fileSize);
            io->Seek(0);
            io->Read(header.fileSize, buffer);
            if (header.version != Binary::curVersion) {
                // Force a fresh RRGameSettings.pul when the binary version changes.
                delete buffer;
                ret = false;
            }
        }
    }
    if (!ret) {
        buffer = io->Alloc<Binary>(size);
        memset(buffer, 0, size);
        new (buffer) Binary(this->pulsarPageCount, this->userPageCount, trackCount);
    }
    io->Close();

    TrophiesHolder& trophies = buffer->GetSection<TrophiesHolder>();
    for (int i = 0; i < 4; ++i) {
        u32 curTotalCount = this->GetTotalTrophyCount(static_cast<TTMode>(i));
        if (trophies.trophyCount[i] > curTotalCount) trophies.trophyCount[i] = curTotalCount;
    }

    this->rawBin = buffer;
    this->AdjustSections();

    MiscParams& params = this->rawBin->GetSection<MiscParams>();
    if (params.customItemsBitfield == 0) {
        params.customItemsBitfield = 0x7FFFF;
    }

    u8& looseOverridesSetting =
        this->rawBin->GetSection<PagesHolder>().pages[SETTINGSTYPE_MISC].settings[RADIO_LOOSEARCHIVEOVERRIDES];
    if (looseOverridesSetting > LOOSEARCHIVEOVERRIDES_DISABLED) {
        looseOverridesSetting = LOOSEARCHIVEOVERRIDES_ENABLED;
    }

    this->InitTrophyEntries(totalTrophyCount);
    this->LoadTrophiesFromFiles();
    this->MigrateLegacyTrophies();

    io->OpenFile(this->filePath, FILE_MODE_WRITE);
    io->Overwrite(this->rawBin->header.fileSize, this->rawBin);
    io->Close();
}

void Mgr::GetTrophyFolder(char* dest, u32 crc32, u8 variantIdx) const {
    const char* modFolder = System::sInstance->GetModFolder();
    if (variantIdx == 0)
        snprintf(dest, IOS::ipcMaxPath, "%s/%s/%08x", modFolder, trophyFolderName, crc32);
    else
        snprintf(dest, IOS::ipcMaxPath, "%s/%s/%08x/%u", modFolder, trophyFolderName, crc32, variantIdx);
}

void Mgr::GetTrophyFilePath(char* dest, u32 crc32, u8 variantIdx) const {
    char folder[IOS::ipcMaxPath];
    this->GetTrophyFolder(folder, crc32, variantIdx);
    snprintf(dest, IOS::ipcMaxPath, "%s/%s", folder, trophyFileName);
}

bool Mgr::EnsureTrophyFoldersExist(u32 crc32, u8 variantIdx) const {
    IO* io = IO::sInstance;
    char rootFolder[IOS::ipcMaxPath];
    snprintf(rootFolder, IOS::ipcMaxPath, "%s/%s", System::sInstance->GetModFolder(), trophyFolderName);
    if (!io->FolderExists(rootFolder) && !io->CreateFolder(rootFolder)) return false;

    char baseFolder[IOS::ipcMaxPath];
    this->GetTrophyFolder(baseFolder, crc32, 0);
    if (!io->FolderExists(baseFolder) && !io->CreateFolder(baseFolder)) return false;

    if (variantIdx != 0) {
        char variantFolder[IOS::ipcMaxPath];
        this->GetTrophyFolder(variantFolder, crc32, variantIdx);
        if (!io->FolderExists(variantFolder) && !io->CreateFolder(variantFolder)) return false;
    }
    return true;
}

bool Mgr::WriteTrophyFile(const TrophyEntry& trophy) const {
    if (!this->EnsureTrophyFoldersExist(trophy.crc32, trophy.variantIdx)) return false;

    alignas(0x20) TrophyFile file;
    file.header.magic = TrophyFile::magic;
    file.header.version = TrophyFile::version;
    file.header.size = sizeof(TrophyFile);
    file.crc32 = trophy.crc32;
    file.variantIdx = trophy.variantIdx;
    memset(file.hasTrophy, 0, sizeof(file.hasTrophy));
    memset(file.reserved, 0, sizeof(file.reserved));
    for (int mode = 0; mode < 4; ++mode) {
        file.hasTrophy[mode] = trophy.hasTrophy[mode] ? 1 : 0;
    }

    char path[IOS::ipcMaxPath];
    this->GetTrophyFilePath(path, trophy.crc32, trophy.variantIdx);
    IO* io = IO::sInstance;
    bool ret = io->OpenFile(path, FILE_MODE_WRITE);
    if (!ret) ret = io->CreateAndOpen(path, FILE_MODE_WRITE);
    if (!ret) return false;
    io->Overwrite(sizeof(TrophyFile), &file);
    io->Close();
    return true;
}

bool Mgr::ReadTrophyFile(TrophyEntry& trophy) const {
    char path[IOS::ipcMaxPath];
    this->GetTrophyFilePath(path, trophy.crc32, trophy.variantIdx);

    IO* io = IO::sInstance;
    if (!io->OpenFile(path, FILE_MODE_READ)) return false;

    alignas(0x20) TrophyFile file;
    bool ret = io->Read(sizeof(TrophyFile), &file) == sizeof(TrophyFile) &&
               file.header.magic == TrophyFile::magic &&
               file.header.version == TrophyFile::version &&
               file.crc32 == trophy.crc32 &&
               file.variantIdx == trophy.variantIdx;
    io->Close();

    if (!ret) return false;
    for (int mode = 0; mode < 4; ++mode) {
        trophy.hasTrophy[mode] = file.hasTrophy[mode] != 0;
    }
    return true;
}

void Mgr::InitTrophyEntries(const u16* totalTrophyCount) {
    const CupsConfig* cups = CupsConfig::sInstance;
    const u32 regTrackCount = cups->HasRegs() ? 32 : 0;
    const u32 ctTrackCount = cups->GetRetroTrackCount() + cups->GetCTOnlyTrackCount();
    const u32 totalRaceTrackCount = regTrackCount + ctTrackCount;

    u32 trophyVariantEntryCount = 0;
    for (u32 i = 0; i < ctTrackCount; ++i) {
        const PulsarId id = static_cast<PulsarId>(PULSARID_FIRSTCT + i);
        trophyVariantEntryCount += cups->GetTrack(id).variantCount;
    }

    for (int mode = 0; mode < 4; ++mode) {
        this->totalTrophyCount[mode] = totalTrophyCount[mode];
        this->trophyCount[mode] = 0;
    }

    this->trophyEntryCount = totalRaceTrackCount + trophyVariantEntryCount;
    this->trophyEntries = new (System::sInstance->heap) TrophyEntry[this->trophyEntryCount];
    memset(this->trophyEntries, 0, sizeof(TrophyEntry) * this->trophyEntryCount);

    u32 entryIdx = 0;
    for (u32 i = 0; i < regTrackCount; ++i) {
        this->trophyEntries[entryIdx].crc32 = cups->GetCRC32(static_cast<PulsarId>(i));
        this->trophyEntries[entryIdx].variantIdx = 0;
        ++entryIdx;
    }

    for (u32 i = 0; i < ctTrackCount; ++i) {
        const PulsarId id = static_cast<PulsarId>(PULSARID_FIRSTCT + i);
        const Track& track = cups->GetTrack(id);

        this->trophyEntries[entryIdx].crc32 = track.crc32;
        this->trophyEntries[entryIdx].variantIdx = 0;
        ++entryIdx;

        for (u32 variantIdx = 1; variantIdx <= track.variantCount; ++variantIdx) {
            this->trophyEntries[entryIdx].crc32 = track.crc32;
            this->trophyEntries[entryIdx].variantIdx = static_cast<u8>(variantIdx);
            ++entryIdx;
        }
    }
}

void Mgr::LoadTrophiesFromFiles() {
    for (u32 i = 0; i < this->trophyEntryCount; ++i) {
        TrophyEntry& trophy = this->trophyEntries[i];
        if (!this->ReadTrophyFile(trophy)) continue;
        for (int mode = 0; mode < 4; ++mode) {
            if (trophy.hasTrophy[mode]) ++this->trophyCount[mode];
        }
    }
}

bool Mgr::LoadLegacyTrophies(TrophiesHolder*& holder) const {
    holder = nullptr;
    IO* io = IO::sInstance;
    if (!io->OpenFile(this->trophiesFilePath, FILE_MODE_READ)) return false;

    const s32 fileSize = io->GetFileSize();
    alignas(0x20) Pulsar::SectionHeader header;
    const bool ret = io->Read(sizeof(Pulsar::SectionHeader), &header) == sizeof(Pulsar::SectionHeader) &&
                     header.magic == TrophiesHolder::tropMagic &&
                     header.version == TrophiesHolder::version &&
                     header.size >= sizeof(TrophiesHolder) &&
                     fileSize >= 0 &&
                     header.size <= static_cast<u32>(fileSize) &&
                     (header.size - sizeof(TrophiesHolder)) % sizeof(TrackTrophy) == 0;
    if (!ret) {
        io->Close();
        return false;
    }

    io->Seek(0);
    holder = io->Alloc<TrophiesHolder>(header.size);
    if (holder == nullptr) {
        io->Close();
        return false;
    }
    const bool readOk = io->Read(header.size, holder) == static_cast<s32>(header.size);
    io->Close();
    if (!readOk) {
        delete holder;
        holder = nullptr;
        return false;
    }
    return true;
}

void Mgr::MigrateLegacyTrophies() {
    char migratedPath[IOS::ipcMaxPath];
    snprintf(migratedPath, IOS::ipcMaxPath, "%s%s", this->trophiesFilePath, migratedLegacySuffix);

    IO* io = IO::sInstance;
    if (io->OpenFile(migratedPath, FILE_MODE_READ)) {
        io->Close();
        return;
    }

    TrophiesHolder* legacy = nullptr;
    if (!this->LoadLegacyTrophies(legacy) || legacy == nullptr) return;

    const u32 legacyEntryCount = (legacy->header.size - sizeof(TrophiesHolder)) / sizeof(TrackTrophy) + 1;
    for (u32 i = 0; i < legacyEntryCount; ++i) {
        const TrackTrophy& src = legacy->trophies[i];
        TrophyEntry* dest = this->FindTrackTrophy(src.crc32, 0);
        if (dest == nullptr) continue;

        bool changed = false;
        for (int mode = 0; mode < 4; ++mode) {
            if (src.hastrophy[mode] && !dest->hasTrophy[mode]) {
                dest->hasTrophy[mode] = true;
                ++this->trophyCount[mode];
                changed = true;
            }
        }
        if (changed) {
            this->WriteTrophyFile(*dest);
        }
    }

    delete legacy;

    if (!io->RenameFile(this->trophiesFilePath, migratedPath)) {
        if (io->OpenFile(migratedPath, FILE_MODE_WRITE) || io->CreateAndOpen(migratedPath, FILE_MODE_WRITE)) {
            alignas(0x20) u32 marker = TrophiesHolder::tropMagic;
            io->Overwrite(sizeof(marker), &marker);
            io->Close();
        }
    }
}

TrophyEntry* Mgr::FindTrackTrophy(u32 crc32, u8 variantIdx) {
    for (u32 i = 0; i < this->trophyEntryCount; ++i) {
        TrophyEntry& trophy = this->trophyEntries[i];
        if (trophy.crc32 == crc32 && trophy.variantIdx == variantIdx) return &trophy;
    }
    return nullptr;
}

const TrophyEntry* Mgr::FindTrackTrophy(u32 crc32, u8 variantIdx) const {
    for (u32 i = 0; i < this->trophyEntryCount; ++i) {
        const TrophyEntry& trophy = this->trophyEntries[i];
        if (trophy.crc32 == crc32 && trophy.variantIdx == variantIdx) return &trophy;
    }
    return nullptr;
}

void Mgr::AddTrophy(u32 crc32, u8 variantIdx, TTMode mode) {
    TrophyEntry* trophy = this->FindTrackTrophy(crc32, variantIdx);
    if (trophy != nullptr && !trophy->hasTrophy[mode]) {
        ++this->trophyCount[mode];
        trophy->hasTrophy[mode] = true;
        this->RequestTrophiesSave();
    }
}

bool Mgr::HasTrophy(u32 crc32, u8 variantIdx, TTMode mode) const {
    const TrophyEntry* trophy = this->FindTrackTrophy(crc32, variantIdx);
    if (trophy != nullptr && trophy->hasTrophy[mode]) return true;
    return false;
}

bool Mgr::HasTrophy(u32 crc32, TTMode mode) const {
    return this->HasTrophy(crc32, 0, mode);
}

bool Mgr::HasTrophy(PulsarId id, u8 variantIdx, TTMode mode) const {
    return this->HasTrophy(CupsConfig::sInstance->GetCRC32(id), variantIdx, mode);
}

bool Mgr::HasTrophy(PulsarId id, TTMode mode) const {
    return this->HasTrophy(id, 0, mode);
}

bool Mgr::HasTrophyForAllVariants(PulsarId id, TTMode mode) const {
    if (!this->HasTrophy(id, 0, mode)) return false;
    if (CupsConfig::IsReg(id)) return true;

    const Track& track = CupsConfig::sInstance->GetTrack(id);
    for (u32 variantIdx = 1; variantIdx <= track.variantCount; ++variantIdx) {
        if (!this->HasTrophy(id, static_cast<u8>(variantIdx), mode)) return false;
    }
    return true;
}

u32 Mgr::CountTrophiesInTrackRange(u32 firstTrackIdx, u32 trackCount, TTMode mode) const {
    const CupsConfig* cups = CupsConfig::sInstance;
    u32 count = 0;
    for (u32 i = 0; i < trackCount; ++i) {
        const PulsarId id = static_cast<PulsarId>(PULSARID_FIRSTCT + firstTrackIdx + i);
        const Track& track = cups->GetTrack(id);
        for (u32 variantIdx = 0; variantIdx <= track.variantCount; ++variantIdx) {
            if (this->HasTrophy(track.crc32, static_cast<u8>(variantIdx), mode)) ++count;
        }
    }
    return count;
}

u16 Mgr::GetTotalTrophyCount(PulsarId id, TTMode mode) const {
    const CupsConfig* cups = CupsConfig::sInstance;
    if (CupsConfig::IsReg(id)) return this->GetTotalTrophyCount(mode);

    const u32 trackIdx = static_cast<u32>(id) - PULSARID_FIRSTCT;
    const u32 rtTrackCount = cups->GetRetroTrackCount();
    const u32 ctTrackCount = cups->GetCTOnlyTrackCount();
    u32 total = this->GetTotalTrophyCount(mode);

    if (trackIdx < rtTrackCount) {
        total = cups->retroTrophyCount[mode];
    } else if (trackIdx < rtTrackCount + ctTrackCount) {
        total = cups->ctOnlyTrophyCount[mode];
    }
    if (total > 0xFFFF) total = 0xFFFF;
    return static_cast<u16>(total);
}

int Mgr::GetTrophyCount(PulsarId id, TTMode mode) const {
    const CupsConfig* cups = CupsConfig::sInstance;
    if (CupsConfig::IsReg(id)) return this->GetTrophyCount(mode);

    const u32 trackIdx = static_cast<u32>(id) - PULSARID_FIRSTCT;
    const u32 rtTrackCount = cups->GetRetroTrackCount();
    const u32 ctTrackCount = cups->GetCTOnlyTrackCount();
    if (trackIdx < rtTrackCount) return this->CountTrophiesInTrackRange(0, rtTrackCount, mode);
    if (trackIdx < rtTrackCount + ctTrackCount) return this->CountTrophiesInTrackRange(rtTrackCount, ctTrackCount, mode);
    return this->GetTrophyCount(mode);
}

u8 Mgr::GetSettingValue(Type type, u32 setting) const {
    return this->rawBin->GetSection<PagesHolder>().pages[type].settings[setting];
}
u8 Mgr::GetUserSettingValue(UserType type, u32 setting) const {
    return this->rawBin->GetSection<PagesHolder>().pages[type].settings[setting];
}

void Mgr::SetSettingValue(Type type, u32 setting, u8 value) {
    this->rawBin->GetSection<PagesHolder>().pages[type].settings[setting] = value;
}
void Mgr::SetUserSettingValue(UserType type, u32 setting, u8 value) {
    u8& currentValue = this->rawBin->GetSection<PagesHolder>().pages[type].settings[setting];
    if (type == SETTINGSTYPE_MISC && setting == RADIO_LOOSEARCHIVEOVERRIDES && currentValue != value) {
        CustomCharacters::ResetAllCharacterTablesToDefault();
    }
    currentValue = value;
}

void Mgr::AdjustSections() {
    MiscParams& params = this->rawBin->GetSection<MiscParams>();
    TrophiesHolder& trophiesHolder = this->rawBin->GetSection<TrophiesHolder>();

    const CupsConfig* cupsConfig = CupsConfig::sInstance;
    const u32 oldTrackCount = params.trackCount;
    const u32 trackCount = cupsConfig->GetEffectiveTrackCount();

    EGG::Heap* heap = System::sInstance->heap;
    u16* missingCRCIndex = new (heap) u16[trackCount];  // 24
    memset(missingCRCIndex, 0xFFFF, sizeof(u16) * trackCount);  // if it's 0xFFFF, it's missing
    u16* toberemovedCRCIndex = new (heap) u16[oldTrackCount];  // 24
    memset(toberemovedCRCIndex, 0xFFFF, sizeof(u16) * oldTrackCount);

    TrackTrophy* trophies = trophiesHolder.trophies;
    for (int curNew = 0; curNew < trackCount; ++curNew) {
        for (int curOld = 0; curOld < oldTrackCount; ++curOld) {
            if (cupsConfig->GetCRC32(cupsConfig->ConvertTrack_IdxToPulsarId(curNew)) == trophies[curOld].crc32) {
                missingCRCIndex[curNew] = curOld;  // this new track crc32 is already in the file
                break;
            }
        }
    }

    for (int curOld = 0; curOld < oldTrackCount; ++curOld) {
        for (int curNew = 0; curNew < trackCount; ++curNew) {
            if (trophies[curOld].crc32 == cupsConfig->GetCRC32(cupsConfig->ConvertTrack_IdxToPulsarId(curNew))) {
                toberemovedCRCIndex[curOld] = curNew;  // this old track still exists
                break;
            }
        }
    }

    for (int curNew = 0; curNew < trackCount; ++curNew) {
        if (missingCRCIndex[curNew] == 0xFFFF) {
            for (int curOld = 0; curOld < oldTrackCount; ++curOld) {
                if (toberemovedCRCIndex[curOld] == 0xFFFF) {
                    missingCRCIndex[curNew] = curOld;  // found a spot to put the missing track in, reset that spot and use it for the new track
                    toberemovedCRCIndex[curOld] = 0;
                    trophies[curOld].crc32 = cupsConfig->GetCRC32(cupsConfig->ConvertTrack_IdxToPulsarId(curNew));
                    for (int mode = 0; mode < 4; ++mode) {
                        if (trophies[curOld].hastrophy[mode] == true) {
                            trophies[curOld].hastrophy[mode] = false;
                            trophiesHolder.trophyCount[mode]--;
                        }
                    }

                    break;
                }
            }
        }
    }
    this->AdjustSectionsSizes();
    if (oldTrackCount < trackCount) {  // the surplus of tracks is simply put continuously at the end of the file, which has been resized to fit the additional tracks
        trophies = this->rawBin->GetSection<TrophiesHolder>().trophies;
        u32 idx = oldTrackCount;
        for (int curNew = 0; curNew < trackCount; ++curNew) {  // 4032 4132
            if (missingCRCIndex[curNew] == 0xFFFF) {
                trophies[idx].crc32 = cupsConfig->GetCRC32(cupsConfig->ConvertTrack_IdxToPulsarId(curNew));
                for (int mode = 0; mode < 4; ++mode) trophies[idx].hastrophy[mode] = false;

                ++idx;
            }
        }
        this->SaveTrophies();
    } else if (oldTrackCount > trackCount) {
        for (int curOld = 0; curOld < oldTrackCount; ++curOld) {  // 4032 4132
            if (toberemovedCRCIndex[curOld] == 0xFFFF) {
                for (int mode = 0; mode < 4; ++mode) {
                    if (trophies[curOld].hastrophy[mode] == true) {
                        trophies[curOld].hastrophy[mode] = false;
                        trophiesHolder.trophyCount[mode]--;
                    }
                }
            }
        }
    }
    heap->free(missingCRCIndex);
    heap->free(toberemovedCRCIndex);
}

void Mgr::AdjustSectionsSizes() {
    Binary* oldBin = this->rawBin;
    PagesHolder& srcPages = oldBin->GetSection<PagesHolder>();
    MiscParams& srcParams = oldBin->GetSection<MiscParams>();
    TrophiesHolder& srcTrophiesHolder = oldBin->GetSection<TrophiesHolder>();
    GPSection& srcGp = oldBin->GetSection<GPSection>();

    u32 newTrackCount = CupsConfig::sInstance->GetEffectiveTrackCount();

    s32 pulsarPageDiff = this->pulsarPageCount - srcPages.pulsarPageCount;
    s32 userPageDiff = this->userPageCount - srcPages.userPageCount;

    s32 trackDiff = newTrackCount - srcParams.trackCount;
    s32 trophySizeDiff = sizeof(TrackTrophy) * (trackDiff);
    s32 gpSizeDiff = sizeof(GPCupStatus) * (trackDiff / 4);

    if (trophySizeDiff <= 0 && pulsarPageDiff <= 0 && userPageDiff <= 0) return;  // no modifications necessary
    if (pulsarPageDiff < 0) pulsarPageDiff = 0;  // if only one is negative (both would take the return above) then avoid a sum that is negative using this hacky trick
    if (userPageDiff < 0) userPageDiff = 0;

    s32 totalPageDiff = sizeof(Page) * (pulsarPageDiff + userPageDiff);
    u32 newSize = oldBin->header.fileSize +
                  totalPageDiff +  // added pages
                  // miscSizeDiff, nothing for now
                  trophySizeDiff +
                  gpSizeDiff;

    srcParams.trackCount = newTrackCount;

    Binary* buffer = IO::sInstance->Alloc<Binary>(newSize);

    // Copy the sections one by one, then change the offsets and the section sizes
    // HEADER
    memcpy(buffer, oldBin, oldBin->header.offsets[0]);  // copy header + page offset to section 0 = size of the header

    // PAGES
    // Pages offset should never be modified in this function
    PagesHolder& destPages = buffer->GetSection<PagesHolder>();
    memcpy(&destPages, &srcPages, srcPages.header.size - sizeof(Page) * srcPages.userPageCount);  // start by copying the pulsarPages (and the header)
    destPages.pulsarPageCount = this->pulsarPageCount;
    destPages.userPageCount = this->userPageCount;

    Page& destUserPages = destPages.pages[destPages.pulsarPageCount];  // start of the user Page array
    Page& srcUserPages = srcPages.pages[srcPages.pulsarPageCount];
    memcpy(&destUserPages, &srcUserPages, srcPages.userPageCount * sizeof(Page));

    // MISC, NOT modified for now
    buffer->header.offsets[MiscParams::index] += totalPageDiff;
    memcpy(&buffer->GetSection<MiscParams>(), &srcParams, srcParams.header.size);  // copy params

    // TROPHIES
    buffer->header.offsets[TrophiesHolder::index] += totalPageDiff;
    memcpy(&buffer->GetSection<TrophiesHolder>(), &srcTrophiesHolder, srcTrophiesHolder.header.size);  // copy trophies

    // GP
    buffer->header.offsets[GPSection::index] += totalPageDiff + trophySizeDiff;
    memcpy(&buffer->GetSection<GPSection>(), &srcGp, srcGp.header.size);

    // SIZES:
    buffer->GetSection<PagesHolder>().header.size += totalPageDiff;
    // buffer->GetSection<MiscParams>().header.size;
    buffer->GetSection<TrophiesHolder>().header.size += trophySizeDiff;
    buffer->GetSection<GPSection>().header.size += gpSizeDiff;

    buffer->header.fileSize = newSize;
    this->rawBin = buffer;
    delete oldBin;
}

Binary* Mgr::CreateFromOld(const Binary* old) {
    Binary* ret;
    const u32 version = old->header.version;
    if (version < 2)
        ret = nullptr;
    else {
        const PagesHolderV1* oldPages;
        const MiscParams* oldParams;
        const TrophiesHolder* oldTrophies;

        if (version == 2) {
            const BinaryHeaderV1& oldHeader = reinterpret_cast<const BinaryHeaderV1&>(old->header);
            oldPages = reinterpret_cast<const PagesHolderV1*>(ut::AddU32ToPtr(old, oldHeader.offsetToPages));
            oldParams = reinterpret_cast<const MiscParams*>(ut::AddU32ToPtr(old, oldHeader.offsetToMisc));
            oldTrophies = reinterpret_cast<const TrophiesHolder*>(ut::AddU32ToPtr(old, oldHeader.offsetToTrophies));
        } else {  // version 3
            oldPages = reinterpret_cast<const PagesHolderV1*>(&old->GetSection<PagesHolder>());  // since GetSection uses offset, this reinterpret_cast is completely safe
            oldParams = &old->GetSection<MiscParams>();
            oldTrophies = &old->GetSection<TrophiesHolder>();
        }
        const u32 pageCount = ut::Min(this->pulsarPageCount, oldPages->pageCount);  // we use the minimum here, it's fine if some settings are lost
        const u32 trackCount = oldParams->trackCount;  // we use the old track count to preserve all trophies
        ret = IO::sInstance->Alloc<Binary>(this->GetSettingsBinSize(trackCount));
        new (ret) Binary(pageCount, 0, trackCount);  // this didn't have userPageCount

        // PAGES, version 4 modifies the header and adds user pages so just copy the pulsar pages
        PagesHolder& pages = ret->GetSection<PagesHolder>();
        memcpy(&pages.pages[0], &oldPages->pages[0], pageCount * sizeof(Page));

        // MISC, unchanged from 2/3 to 4
        MiscParams& params = ret->GetSection<MiscParams>();
        memcpy(&params, oldParams, params.header.size);

        // TROPHIES, unchanged from 2/3 to 4
        TrophiesHolder& trophies = ret->GetSection<TrophiesHolder>();
        memcpy(&trophies, oldTrophies, trophies.header.size);

        // GP
        GPSection& gp = ret->GetSection<GPSection>();  // create GPSection
        if (version == 2) {
            const u32 cupCount = trackCount / 4;
            memset(&gp.gpStatus[0], 0xFF, sizeof(GPCupStatus) * cupCount);
        } else if (version == 3) {
            const GPSection& oldGp = old->GetSection<GPSection>();
            memcpy(&gp, &oldGp, oldGp.header.size);
        }
    }
    delete old;
    return ret;
}

void Mgr::SaveGPResult(RKSYSRequester* requester, u32 r4, u32 r5, u32 r6, u32 r7, u32 r8, u32 r9, bool isNew) {
    const PulsarCupId id = CupsConfig::sInstance->lastSelectedCup;
    if (!CupsConfig::IsRegCup(id)) {
        const u32 realCupId = CupsConfig::ConvertCup_PulsarIdToRealId(id);
        const GPRank rank = Racedata::sInstance->awardScenario.players[0].ComputeGPRank();
        register u32 trophy;
        asm(mr trophy, r31;);
        register u32 cc;
        asm(mr cc, r29;);

        Mgr* self = Mgr::sInstance;
        GPSection& gp = self->rawBin->GetSection<GPSection>();
        u8 newStatus = trophy | (rank << 2);

        const u8 oldStatus = gp.gpStatus[realCupId].gpCCStatus[cc];
        bool isNew = false;
        if (oldStatus == 0xFF)
            isNew = true;
        else {
            const GPRank oldRank = ComputeRankFromStatus(oldStatus);
            const u32 oldTrophy = ComputeTrophyFromStatus(oldStatus);

            if (trophy < oldTrophy)
                isNew = true;
            else if (trophy == oldTrophy && rank < oldRank)
                isNew = true;
        }
        if (isNew) {
            gp.gpStatus[realCupId].gpCCStatus[cc] = newStatus;
            self->RequestSave();
        }
    } else if (isNew)
        requester->NotifyNewLicenseContent();
}
kmWrite32(0x805bd1d4, 0x418200d8);
kmCall(0x805bd2ac, Mgr::SaveGPResult);

}  // namespace Settings
}  // namespace Pulsar
