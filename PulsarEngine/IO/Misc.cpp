#include <kamek.hpp>
#include <MarioKartWii/Archive/ArchiveMgr.hpp>
#include <MarioKartWii/Scene/GameScene.hpp>
#include <Settings/Settings.hpp>
#include <PulsarSystem.hpp>

namespace Pulsar {

// Adds extra archives to Common, UI, and Driver holders for custom Pulsar assets
kmWrite32(0x8052a108, 0x38800003);  // Add one archive to CommonArchiveHolder
kmWrite32(0x8052a188, 0x38800004);  // Add one archive to UIArchiveHolder
kmWrite32(0x805550a8, 0x80a418d4);  // Load menu Driver.szs from archiveHeaps.heaps[0] (MEM1), matching GlobeScene

extern "C" void __nw__FUl(void*);
extern "C" void ArchiveHolder(void*);
extern "C" void DVDCreate(void*);
asmFunc CreateGenericArchiveHolder() {
    ASM(
        nofralloc;
        cmpwi r3, ARCHIVE_HOLDER_DRIVER;
        li r31, 0x1;
        bne + createArchive;
        li r31, 0x2;

        createArchive:
        li r3, 0x1c;
        lis r12, __nw__FUl@h;
        ori r12, r12, __nw__FUl@l;
        mtctr r12;
        bctrl;
        cmpwi r3, 0x0;
        beq + done;
        mr r4, r31;
        lis r12, ArchiveHolder@h;
        ori r12, r12, ArchiveHolder@l;
        mtctr r12;
        bctrl;

        done:
        or r31, r3, r3;
        lis r12, DVDCreate@h;
        ori r12, r12, DVDCreate@l;
        mtctr r12;
        bctr;)
}
kmBranch(0x8052a0d4, CreateGenericArchiveHolder);

void LoadAssetsFile(ArchiveFile* file, const char* path, EGG::Heap* decompressedHeap, bool isCompressed, s32 allocDirection,
                    EGG::Heap* archiveHeap, EGG::Archive::FileInfo* info) {
    const ArchiveMgr* archiveMgr = ArchiveMgr::sInstance;
    const ArchivesHolder* driverHolder = archiveMgr->archivesHolders[ARCHIVE_HOLDER_DRIVER];
    if (driverHolder != nullptr && driverHolder->archiveCount > 1 && file == &driverHolder->archives[1]) {
        path = "/DriverAssets.szs";
        const GameScene* scene = GameScene::GetCurrent();
        if (scene != nullptr && (scene->id == SCENE_ID_MENU || scene->id == SCENE_ID_GLOBE)) {
            decompressedHeap = scene->archiveHeaps.heaps[1];
            archiveHeap = scene->archiveHeaps.heaps[1];
        }
    }

    if (file == &archiveMgr->archivesHolders[ARCHIVE_HOLDER_UI]->archives[3]) {
        const char* fileType = "UI";
        Pulsar::Language currentLanguage = static_cast<Pulsar::Language>(Pulsar::Settings::Mgr::Get().GetUserSettingValue(static_cast<Pulsar::Settings::UserType>(Pulsar::Settings::SETTINGSTYPE_MISC), Pulsar::SCROLLER_LANGUAGE));

        bool isRaceScene = (GameScene::GetCurrent()->id == SCENE_ID_RACE);
        const char* baseType = isRaceScene ? "Race" : "UI";
        const char* langSuffix = "";
        switch (currentLanguage) {
            case Pulsar::LANGUAGE_JAPANESE:
                langSuffix = "_J";
                break;
            case Pulsar::LANGUAGE_FRENCH:
                langSuffix = "_F";
                break;
            case Pulsar::LANGUAGE_GERMAN:
                langSuffix = "_G";
                break;
            case Pulsar::LANGUAGE_DUTCH:
                langSuffix = "_D";
                break;
            case Pulsar::LANGUAGE_SPANISHUS:
                langSuffix = "_AS";
                break;
            case Pulsar::LANGUAGE_SPANISHEU:
                langSuffix = "_ES";
                break;
            case Pulsar::LANGUAGE_FINNISH:
                langSuffix = "_FI";
                break;
            case Pulsar::LANGUAGE_ITALIAN:
                langSuffix = "_I";
                break;
            case Pulsar::LANGUAGE_KOREAN:
                langSuffix = "_K";
                break;
            case Pulsar::LANGUAGE_RUSSIAN:
                langSuffix = "_R";
                break;
            case Pulsar::LANGUAGE_TURKISH:
                langSuffix = "_T";
                break;
            case Pulsar::LANGUAGE_CZECH:
                langSuffix = "_C";
                break;
            case Pulsar::LANGUAGE_ENGLISH:
                langSuffix = "";
                break;
            default:
                langSuffix = "";
                break;
        }
        char newPath[0x20];
        snprintf(newPath, 0x20, "%sAssets%s.szs", baseType, langSuffix);
        path = newPath;
    } else if (file == &archiveMgr->archivesHolders[ARCHIVE_HOLDER_COMMON]->archives[2])
        path = System::CommonAssets;
    else if (file == &archiveMgr->archivesHolders[ARCHIVE_HOLDER_UI]->archives[0])
        path = "/ReplacedAssets.szs";
    file->Load(path, decompressedHeap, isCompressed, allocDirection, archiveHeap, info);
    OS::Report("Loading archive file %s for archive %p\n", path, file);
}
kmCall(0x8052aa2c, LoadAssetsFile);

}  // namespace Pulsar