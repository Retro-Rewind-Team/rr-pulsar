#ifndef _SETTINGS_
#define _SETTINGS_
#include <kamek.hpp>
#include <MarioKartWii/UI/Section/RKSYSRequester.hpp>
#include <PulsarSystem.hpp>
#include <Config.hpp>
#include <Settings/SettingsParam.hpp>
#include <Settings/SettingsBinary.hpp>
#include <Ghost/GhostManager.hpp>

namespace Pulsar {
namespace Ghosts {
class Mgr;
}
namespace UI {
class SettingsPanel;
class CustomItemPage;
}
namespace Settings {

struct TrophyEntry {
    u32 crc32;
    u8 variantIdx;
    bool hasTrophy[4];
};

struct TrophyFile {
    static const u32 magic = 'TRPH';
    static const u32 version = 1;

    Pulsar::SectionHeader header;
    u32 crc32;
    u8 variantIdx;
    u8 hasTrophy[4];
    u8 reserved[3];
};

class Hook : public DoFuncsHook {
    static DoFuncsHook* settingsHooks;

   public:
    Hook(Func& f) : DoFuncsHook(f, &settingsHooks) {}
    static void Exec() { DoFuncsHook::Exec(settingsHooks); }
};

class Mgr {
   private:
    static Mgr* sInstance;
    static void SaveTask(void*);
    void Init(const u16* totalTrophyCount, const char* settingsPath, const char* trophiesPath);
    int GetSettingsBinSize(u32 trackCount) const;
    char filePath[IOS::ipcMaxPath];
    char trophiesFilePath[IOS::ipcMaxPath];
    Binary* rawBin;

    TrophyEntry* FindTrackTrophy(u32 crc32, u8 variantIdx);
    const TrophyEntry* FindTrackTrophy(u32 crc32, u8 variantIdx) const;
    void InitTrophyEntries(const u16* totalTrophyCount);
    void LoadTrophiesFromFiles();
    void MigrateLegacyTrophies();
    bool LoadLegacyTrophies(TrophiesHolder*& holder) const;
    bool WriteTrophyFile(const TrophyEntry& trophy) const;
    bool ReadTrophyFile(TrophyEntry& trophy) const;
    void GetTrophyFolder(char* dest, u32 crc32, u8 variantIdx) const;
    void GetTrophyFilePath(char* dest, u32 crc32, u8 variantIdx) const;
    bool EnsureTrophyFoldersExist(u32 crc32, u8 variantIdx) const;
    void AdjustSections();
    void SetSettingValue(Type type, u32 setting, u8 value);
    void SetUserSettingValue(UserType type, u32 setting, u8 value);
    void AdjustSectionsSizes();
    Binary* CreateFromOld(const Binary* old);
    void Update() {
        Hook::Exec();
        this->RequestSave();
    }
    void RequestSave() { System::sInstance->taskThread->Request(&Mgr::SaveTask, nullptr, 0); }
    void RequestTrophiesSave() { System::sInstance->taskThread->Request(&Mgr::SaveTask, reinterpret_cast<void*>(1), 0); }
    void Save();
    void SaveTrophies();
    void AddTrophy(u32 crc32, u8 variantIdx, TTMode mode);
    u32 CountTrophiesInTrackRange(u32 firstTrackIdx, u32 trackCount, TTMode mode) const;
    u32 CountVariantsInTrackRange(u32 firstTrackIdx, u32 trackCount) const;
    void SetLastSelectedCup(PulsarCupId id) { this->rawBin->GetSection<MiscParams>().lastSelectedCup = id; }

   public:
    Mgr() : rawBin(nullptr), trophyEntries(nullptr), trophyEntryCount(0) {
        for (int i = 0; i < 4; ++i) this->trophyCount[i] = 0;
    }
    static Mgr& Get() { return *sInstance; }
    static const Mgr& GetConst() { return *sInstance; }
    static bool IsCreated() { return sInstance != nullptr; }

    bool HasTrophy(u32 crc32, u8 variantIdx, TTMode mode) const;
    bool HasTrophy(u32 crc32, TTMode mode) const;
    bool HasTrophy(PulsarId id, u8 variantIdx, TTMode mode) const;
    bool HasTrophy(PulsarId id, TTMode mode) const;
    bool HasTrophyForAllVariants(PulsarId id, TTMode mode) const;
    u16 GetTotalTrophyCount(TTMode mode) const { return totalTrophyCount[mode]; }
    u16 GetTotalTrophyCount(PulsarId id, TTMode mode) const;
    int GetTrophyCount(TTMode mode) const { return this->trophyCount[mode]; }
    int GetTrophyCount(PulsarId id, TTMode mode) const;
    PulsarCupId GetSavedSelectedCup() const { return this->rawBin->GetSection<MiscParams>().lastSelectedCup; }
    u32 GetCustomItems() const { return this->rawBin->GetSection<MiscParams>().customItemsBitfield; }
    void SetCustomItems(u32 val) { this->rawBin->GetSection<MiscParams>().customItemsBitfield = val; }

    // GP
    static u8 GetGPStatus(u32 idx, u32 cc) {
        Mgr* mgr = Mgr::sInstance;
        GPSection& gp = mgr->rawBin->GetSection<GPSection>();
        return gp.gpStatus[idx].gpCCStatus[cc];
    }
    static GPRank ComputeRankFromStatus(u8 gpStatus) {
        return static_cast<GPRank>(gpStatus >> 2);
    }
    static u32 ComputeTrophyFromStatus(u8 gpStatus) {
        return gpStatus & 0b11;
    }
    static void SaveGPResult(RKSYSRequester* requester, u32 r4, u32 r5, u32 r6, u32 r7, u32 r8, u32 r9, bool isNew);
    u8 GetSettingValue(Type type, u32 setting) const;
    u8 GetUserSettingValue(UserType type, u32 setting) const;
    static void Create();

   private:
    u16 totalTrophyCount[4];
    u16 trophyCount[4];
    TrophyEntry* trophyEntries;
    u32 trophyEntryCount;
    u32 pulsarPageCount;
    u32 userPageCount;

    friend class System;
    friend class UI::SettingsPanel;
    // Two ghosts functions which save the settings
    friend bool Ghosts::Mgr::SaveGhost(const RKSYS::LicenseLdbEntry& entry, u32 ldbPosition, bool isFlap);
    friend void Ghosts::Mgr::CreateAndSaveFiles(Ghosts::Mgr* manager);
    friend class UI::ExpGhostSelect;
};
}  // namespace Settings
}  // namespace Pulsar

#endif
