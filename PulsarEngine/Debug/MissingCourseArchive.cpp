#include <include/c_stdio.h>

#include <Debug/Debug.hpp>
#include <MarioKartWii/Archive/ArchiveMgr.hpp>
#include <SlotExpansion/CupsConfig.hpp>

namespace Pulsar {
namespace Debug {

CourseArchivesHolder* LoadCourseArchiveChecked(ArchiveMgr* archiveMgr, CourseId id, EGG::Heap* heap, bool isMultiplayer) {
    CourseArchivesHolder* holder = archiveMgr->LoadCourseArchive(id, heap, isMultiplayer);
    if (holder != nullptr && holder->HasArchives()) return holder;

    static char message[128];
    const CupsConfig* cupsConfig = CupsConfig::sInstance;
    const PulsarId winning = cupsConfig != nullptr ? cupsConfig->GetWinning() : PULSARID_NONE;
    if (cupsConfig != nullptr && winning != PULSARID_NONE && !CupsConfig::IsReg(winning)) {
        const char* fileName = cupsConfig->GetFileName(winning, cupsConfig->GetCurVariantIdx());
        if (fileName == nullptr || fileName[0] == '\0') fileName = cupsConfig->GetFileName(winning, 0);
        if (fileName != nullptr && fileName[0] != '\0') {
            snprintf(message, sizeof(message), "Fatal Error:\nMissing track file:\nRace/Course/%s.szs", fileName);
            FatalError(message);
        }
    }

    FatalError("Fatal Error:\nMissing track .szs file in Race/Course.");
    return holder;
}

kmCall(0x80553e10, LoadCourseArchiveChecked);

}  // namespace Debug
}  // namespace Pulsar
