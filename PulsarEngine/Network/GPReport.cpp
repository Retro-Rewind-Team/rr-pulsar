#include <types.hpp>
#include <include/c_stdio.h>
#include <core/GS/GP/GPTypes.hpp>
#include <core/GS/GP/GPUtility.hpp>
#include <core/rvl/DWC/DWCMatch.hpp>

namespace Pulsar {
namespace Network {

void Report(const char* key, const char* string) {
    DWC::MatchControl* matchControl = DWC::MatchControl::sInstance;
    if (matchControl == nullptr) {
        return;
    }

    GP::Connection** connection = matchControl->gpConnection;
    if (connection == nullptr || *connection == nullptr) {
        return;
    }

    GP::IConnection* iconnection = reinterpret_cast<GP::IConnection*>(*connection);

    GP::gpiAppendStringToBuffer(
        connection, &iconnection->outputBuffer, "\\wl:report\\\\");
    GP::gpiAppendStringToBuffer(
        connection, &iconnection->outputBuffer, key);
    GP::gpiAppendStringToBuffer(
        connection, &iconnection->outputBuffer, "\\");
    GP::gpiAppendStringToBuffer(
        connection, &iconnection->outputBuffer, string);
    GP::gpiAppendStringToBuffer(
        connection, &iconnection->outputBuffer, "\\final\\");
}

void ReportU32(const char* key, u32 uint) {
    char buffer[sizeof("4294967295")];

    if (snprintf(buffer, sizeof(buffer), "%lu", uint) < 0) {
        return;
    }

    Report(key, buffer);
}

}  // namespace Network
}  // namespace Pulsar
