#include <RetroRewind.hpp>

namespace Languages {

void RenameFont() {
    // Default 'F'
    FontRename = 0x46;

    Pulsar::Language currentLanguage = static_cast<Pulsar::Language>(
        Pulsar::Settings::Mgr::Get().GetUserSettingValue(
            static_cast<Pulsar::Settings::UserType>(Pulsar::Settings::SETTINGSTYPE_MISC),
            Pulsar::SCROLLER_LANGUAGE));

    switch (currentLanguage) {
        case Pulsar::LANGUAGE_KOREAN:
            FontRename = 0x4B;  // 'K'
            break;
        default:
            // Keep default value
            break;
    }
}
BootHook FontHook(RenameFont, 4);

// Rename The 'S' in Scene/UI/X.szs
void RenameRace() {
    RaceRename = 0x53;

    Pulsar::Language currentLanguage = static_cast<Pulsar::Language>(
        Pulsar::Settings::Mgr::Get().GetUserSettingValue(
            static_cast<Pulsar::Settings::UserType>(Pulsar::Settings::SETTINGSTYPE_MISC),
            Pulsar::SCROLLER_LANGUAGE));

    switch (currentLanguage) {
        case Pulsar::LANGUAGE_JAPANESE:
            RaceRename = 0x4A;  // 'J'
            break;
        case Pulsar::LANGUAGE_FRENCH:
            RaceRename = 0x46;  // 'F'
            break;
        case Pulsar::LANGUAGE_GERMAN:
            RaceRename = 0x47;  // 'G'
            break;
        case Pulsar::LANGUAGE_DUTCH:
            RaceRename = 0x44;  // 'D'
            break;
        case Pulsar::LANGUAGE_SPANISHUS:
            RaceRename = 0x55;  // 'U'
            break;
        case Pulsar::LANGUAGE_SPANISHEU:
            RaceRename = 0x45;  // 'E'
            break;
        case Pulsar::LANGUAGE_FINNISH:
            RaceRename = 0x4E;  // 'N'
            break;
        case Pulsar::LANGUAGE_ITALIAN:
            RaceRename = 0x49;  // 'I'
            break;
        case Pulsar::LANGUAGE_KOREAN:
            RaceRename = 0x4B;  // 'K'
            break;
        case Pulsar::LANGUAGE_RUSSIAN:
            RaceRename = 0x41;  // 'A'
            break;
        case Pulsar::LANGUAGE_TURKISH:
            RaceRename = 0x54;  // 'T'
            break;
        case Pulsar::LANGUAGE_CZECH:
            RaceRename = 0x43;  // 'C'
            break;
        default:
            // Keep default value
            break;
    }
}
BootHook RaceHook(RenameRace, 4);

void RenameAward() {
    AwardRename = 0x53;

    Pulsar::Language currentLanguage = static_cast<Pulsar::Language>(
        Pulsar::Settings::Mgr::Get().GetUserSettingValue(
            static_cast<Pulsar::Settings::UserType>(Pulsar::Settings::SETTINGSTYPE_MISC),
            Pulsar::SCROLLER_LANGUAGE));

    switch (currentLanguage) {
        case Pulsar::LANGUAGE_JAPANESE:
            AwardRename = 0x4A;  // 'J'
            break;
        case Pulsar::LANGUAGE_FRENCH:
            AwardRename = 0x46;  // 'F'
            break;
        case Pulsar::LANGUAGE_GERMAN:
            AwardRename = 0x47;  // 'G'
            break;
        case Pulsar::LANGUAGE_DUTCH:
            AwardRename = 0x44;  // 'D'
            break;
        case Pulsar::LANGUAGE_SPANISHUS:
            AwardRename = 0x55;  // 'U'
            break;
        case Pulsar::LANGUAGE_SPANISHEU:
            AwardRename = 0x45;  // 'E'
            break;
        case Pulsar::LANGUAGE_FINNISH:
            AwardRename = 0x4E;  // 'N'
            break;
        case Pulsar::LANGUAGE_ITALIAN:
            AwardRename = 0x49;  // 'I'
            break;
        case Pulsar::LANGUAGE_KOREAN:
            AwardRename = 0x4B;  // 'K'
            break;
        case Pulsar::LANGUAGE_RUSSIAN:
            AwardRename = 0x41;  // 'A'
            break;
        case Pulsar::LANGUAGE_TURKISH:
            AwardRename = 0x54;  // 'T'
            break;
        case Pulsar::LANGUAGE_CZECH:
            AwardRename = 0x43;  // 'C'
            break;
        default:
            // Keep default value
            break;
    }
}
BootHook AwardHook(RenameAward, 4);

// Rename the 'R' in Race/Common.szs
void RenameCommon() {
    CommonRename = 0x52;

    Pulsar::Language currentLanguage = static_cast<Pulsar::Language>(
        Pulsar::Settings::Mgr::Get().GetUserSettingValue(
            static_cast<Pulsar::Settings::UserType>(Pulsar::Settings::SETTINGSTYPE_MISC),
            Pulsar::SCROLLER_LANGUAGE));

    switch (currentLanguage) {
        case Pulsar::LANGUAGE_JAPANESE:
            CommonRename = 0x4A;  // 'J'
            break;
        case Pulsar::LANGUAGE_FRENCH:
            CommonRename = 0x46;  // 'F'
            break;
        case Pulsar::LANGUAGE_GERMAN:
            CommonRename = 0x47;  // 'G'
            break;
        case Pulsar::LANGUAGE_DUTCH:
            CommonRename = 0x44;  // 'D'
            break;
        case Pulsar::LANGUAGE_SPANISHUS:
            CommonRename = 0x55;  // 'U'
            break;
        case Pulsar::LANGUAGE_SPANISHEU:
            CommonRename = 0x45;  // 'E'
            break;
        case Pulsar::LANGUAGE_FINNISH:
            CommonRename = 0x4E;  // 'N'
            break;
        case Pulsar::LANGUAGE_ITALIAN:
            CommonRename = 0x49;  // 'I'
            break;
        case Pulsar::LANGUAGE_KOREAN:
            CommonRename = 0x4B;  // 'K'
            break;
        case Pulsar::LANGUAGE_RUSSIAN:
            CommonRename = 0x41;  // 'A'
            break;
        case Pulsar::LANGUAGE_TURKISH:
            CommonRename = 0x54;  // 'T'
            break;
        case Pulsar::LANGUAGE_CZECH:
            CommonRename = 0x43;  // 'C'
            break;
        default:
            // Keep default value
            break;
    }
}
BootHook CommonHook(RenameCommon, 4);

}  // namespace Languages