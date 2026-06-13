#ifndef _PULSAR_
#define _PULSAR_

#include <kamek.hpp>
#include <core/egg/mem/ExpHeap.hpp>
#include <MarioKartWii/System/Identifiers.hpp>
#include <MarioKartWii/UI/Text/Text.hpp>
#include <Extensions/LECODE/LECODEMgr.hpp>
#include <Debug/Debug.hpp>
#include <IO/IO.hpp>
#include <Info.hpp>
#include <Config.hpp>
#include <Network/Network.hpp>
#include <Network/MatchCommand.hpp>
#include <Gamemodes/OnlineTT/OnlineTT.hpp>

namespace Pulsar {
namespace KO {
class Mgr;
}  // namespace KO

namespace LapKO {
class Mgr;
}  // namespace LapKO

class ConfigFile;

enum Context {
    PULSAR_CT = 0,
    PULSAR_200,
    PULSAR_200_WW,
    PULSAR_FEATHER,
    PULSAR_UMTS,
    PULSAR_SMTS,
    PULSAR_KOFINAL,
    PULSAR_MEGATC,
    PULSAR_MODE_OTT,
    PULSAR_MODE_KO,
    PULSAR_MODE_LAPKO,
    PULSAR_CHARRESTRICTLIGHT,
    PULSAR_CHARRESTRICTMID,
    PULSAR_CHARRESTRICTHEAVY,
    PULSAR_KARTRESTRICT,
    PULSAR_BIKERESTRICT,
    PULSAR_500,
    PULSAR_THUNDERCLOUD,
    PULSAR_REGS,
    PULSAR_RETROS,
    PULSAR_CTS,
    PULSAR_ALL,
    PULSAR_CHANGECOMBO,
    PULSAR_EXTENDEDTEAMS,
    PULSAR_FFA,
    PULSAR_ELIMINATION,
    PULSAR_STARTRETROS,
    PULSAR_STARTCTS,
    PULSAR_STARTREGS,
    PULSAR_START200,
    PULSAR_STARTOTT,
    PULSAR_STARTITEMRAIN,
};

enum Context2 {
    PULSAR_MIIHEADS,
    PULSAR_TRANSMISSIONINSIDE,
    PULSAR_TRANSMISSIONOUTSIDE,
    PULSAR_TRANSMISSIONVANILLA,
    PULSAR_ITEMMODERANDOM,
    PULSAR_ITEMMODEBLAST,
    PULSAR_ITEMMODERAIN,
    PULSAR_ITEMMODESTORM,
    PULSAR_HAW,
    PULSAR_RANKING,
    PULSAR_VR,
    PULSAR_ALLITEMSCANLAND,
    PULSAR_ITEMBOXRESPAWN,
    PULSAR_ITEMMODENONE,
    PULSAR_MODE_BATTLEROYALE,
    PULSAR_KOPERRACE_2,
    PULSAR_KOPERRACE_3,
    PULSAR_KOPERRACE_4,
    PULSAR_MODE_TTPRACTICE
};

class System {
   protected:
    System();

   public:
    // System functions
    void Init(const ConfigFile& confRT, const ConfigFile& confCT, const ConfigFile& confBT,
              u32 rtReadBytes, u32 ctReadBytes, u32 btReadBytes);
    void InitInstances(const ConfigFile& conf, IOType type);
    void InitIO(IOType type) const;
    void InitCups(const ConfigFile& conf);
    void InitSettings(const u16* totalTrophyCount) const;
    void UpdateContext();
    static void UpdateContextWrapper();
    static void ClearOttContext();

   protected:
    // Virtual
    virtual void AfterInit() {};

   public:
    static System* sInstance;

    virtual void SetUserInfo(Network::ResvInfo::UserInfo& userInfo) {};
    virtual bool CheckUserInfo(const Network::ResvInfo::UserInfo& userInfo) { return true; };
    const Info& GetInfo() const { return this->info; }

    bool IsContext(Context context) const { return (this->context & (1 << context)) != 0; }
    bool IsContext(Context2 context2) const { return (this->context2 & (1 << context2)) != 0; }
    static s32 OnSceneEnter(Random& random);

    const char* GetModFolder() const { return modFolderName; }
    static void CreateSystem();

    // Network
    static asmFunc GetRaceCount();

    // Modes
    static asmFunc GetNonTTGhostPlayersCount();

    // BMG
    const BMGHolder& GetBMG() const { return customBmgs; }
    const BMGHolder& GetBMGCT() const { return customBmgsCT; }
    const BMGHolder& GetBMGBT() const { return customBmgsBT; }

    // VARIABLES
    EGG::ExpHeap* const heap;  // 0x4
    EGG::TaskThread* const taskThread;  // 0x8
    // Constants

   public:
    char modFolderName[IOS::ipcMaxFileName + 1];  // 0xC
    u8 padding[2];
    Info info;  // 0x1c
    u32 context;
    u32 context2;

   public:
    // Updated from ROOM packets when the host starts a GP.
    Network::Mgr netMgr;

    TTMode ttMode;

    // LECODE data
    LECODE::Mgr lecodeMgr;

    // Modes
    KO::Mgr* koMgr;
    LapKO::Mgr* lapKoMgr;
    bool ottHideNames;
    OTT::Mgr ottMgr;
    u8 nonTTGhostPlayersCount;  // because a ghost can be added in vs, racedata's playercount is not reliable

   private:
    // Custom BMGS
    BMGHolder customBmgs;
    BMGHeader* rawBmg;
    BMGHolder customBmgsCT;
    BMGHeader* rawBmgCT;
    BMGHolder customBmgsBT;
    BMGHeader* rawBmgBT;

   public:
    // string pool
    static const char pulsarString[];
    static const char CommonAssets[];
    static const char breff[];
    static const char breft[];
    static const char* ttModeFolders[];

    struct Inherit {
        typedef System* (*CreateFunc)();
        Inherit(CreateFunc func) {
            create = func;
            inherit = this;
        }
        CreateFunc create;
    };
    static Inherit* inherit;
    friend class Info;
};
}  // namespace Pulsar

#endif
