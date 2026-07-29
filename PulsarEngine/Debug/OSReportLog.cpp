#include <kamek.hpp>
#include <PulsarSystem.hpp>
#include <IO/IO.hpp>
#include <IO/SDIO.hpp>
#include <core/rvl/OS/OS.hpp>
#include <include/c_stdarg.h>
#include <include/c_stdio.h>
#include <include/c_string.h>

#ifdef OS_LOGS
namespace Pulsar {
namespace Debug {

const char* OS_REPORT_LOG_PATH = "/RetroRewind6/OSReport.txt";
const u32 LOG_BUFFER_SIZE = 0x4000;
const u32 LOG_FLUSH_CHUNK_SIZE = 0x400;

char sLogBuffer[LOG_BUFFER_SIZE];
u32 sLogWritePos = 0;
u32 sLogReadPos = 0;
bool sWritingOSReportLog = false;

u32 PendingLogBytes() {
    if (sLogWritePos >= sLogReadPos) return sLogWritePos - sLogReadPos;
    return LOG_BUFFER_SIZE - sLogReadPos + sLogWritePos;
}

void QueueOSReportLog(const char* text, u32 length) {
    if (text == nullptr || length == 0) return;

    for (u32 i = 0; i < length; ++i) {
        const u32 next = (sLogWritePos + 1) % LOG_BUFFER_SIZE;
        if (next == sLogReadPos) sLogReadPos = (sLogReadPos + 1) % LOG_BUFFER_SIZE;
        sLogBuffer[sLogWritePos] = text[i];
        sLogWritePos = next;
    }
}

void FlushOSReportLog() {
    if (System::sInstance == nullptr || sWritingOSReportLog || PendingLogBytes() == 0) return;

    sWritingOSReportLog = true;

    SDIO sd(IOType_SD, nullptr, nullptr);
    IO* logIo = &sd;
    if (IO::sInstance != nullptr && IO::sInstance->type == IOType_DOLPHIN) {
        logIo = IO::sInstance;
    }

    if (!logIo->OpenFile(OS_REPORT_LOG_PATH, IOS::MODE_WRITE)) {
        logIo->CreateFolder("/RetroRewind6");
        if (!logIo->CreateAndOpen(OS_REPORT_LOG_PATH, IOS::MODE_WRITE)) {
            sWritingOSReportLog = false;
            return;
        }
    }

    const s32 size = logIo->GetFileSize();
    if (size > 0) logIo->Seek(static_cast<u32>(size));

    u32 remaining = PendingLogBytes();
    if (remaining > LOG_FLUSH_CHUNK_SIZE) remaining = LOG_FLUSH_CHUNK_SIZE;

    while (remaining != 0) {
        u32 chunk = LOG_BUFFER_SIZE - sLogReadPos;
        if (chunk > remaining) chunk = remaining;

        const s32 wrote = logIo->Write(chunk, &sLogBuffer[sLogReadPos]);
        if (wrote <= 0) break;

        sLogReadPos = (sLogReadPos + static_cast<u32>(wrote)) % LOG_BUFFER_SIZE;
        remaining -= static_cast<u32>(wrote);
    }

    logIo->Close();

    sWritingOSReportLog = false;
}

int OSReportLogHook(const char* format, ...) {
    va_list args;
    va_start(args, format);
    const int printed = vprintf(format, args);

    char buffer[0x400];
    va_start(args, format);
    int written = vsnprintf(buffer, sizeof(buffer), format, args);

    if (written > 0) {
        u32 length = static_cast<u32>(written);
        if (length >= sizeof(buffer)) length = sizeof(buffer) - 1;
        QueueOSReportLog(buffer, length);
    }

    return printed;
}
kmBranch(0x801A25D0, OSReportLogHook);
static FrameLoadHook FlushOSReportLogHook(FlushOSReportLog);

}  // namespace Debug
}  // namespace Pulsar

#endif
