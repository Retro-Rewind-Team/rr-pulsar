#include <kamek.hpp>
#include <MarioKartWii/Archive/ArchiveMgr.hpp>
#include <MarioKartWii/Scene/GameScene.hpp>
#include <Settings/Settings.hpp>
#include <PulsarSystem.hpp>

namespace Pulsar {

// Adds a 3rd UI (menu or race) and a 3rd common to the relevant archive holders, which will contain custom pulsar assets
kmWrite32(0x8052a108, 0x38800003);  // Add one archive to CommonArchiveHolder
kmWrite32(0x8052a188, 0x38800004);  // Add one archive to UIArchiveHolder
void LoadAssetsFile(ArchiveFile* file, const char* path, EGG::Heap* decompressedHeap, bool isCompressed, s32 allocDirection,
                    EGG::Heap* archiveHeap, EGG::Archive::FileInfo* info) {
    const ArchiveMgr* archiveMgr = ArchiveMgr::sInstance;
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
}
kmCall(0x8052aa2c, LoadAssetsFile);

}  // namespace Pulsar