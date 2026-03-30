#include <Debug/Debug.hpp>
#include <PulsarSystem.hpp>
#include <IO/IO.hpp>
#include <core/rvl/OS/OS.hpp>
#include <include/c_stdio.h>

namespace Pulsar {
namespace {

const char kLogsFolder[] = "/logs";
const char kDefaultLogFileName[] = "Pulsar.log";
const u32 kMaxLogMessageLength = 1024;
const u32 kMaxLogLineLength = kMaxLogMessageLength + 2;

static bool sLogFolderReady = false;

static bool EnsureLogFolder(IO* io) {
    if (io == nullptr) return false;
    if (sLogFolderReady) return true;

    if (io->FolderExists(kLogsFolder) || io->CreateFolder(kLogsFolder)) {
        sLogFolderReady = true;
    }
    return sLogFolderReady;
}

static const char* GetLogFileStem(const char* modFolder) {
    if (modFolder == nullptr) return nullptr;

    while (*modFolder == '/') ++modFolder;
    if (*modFolder == '\0') return nullptr;

    const char* stem = modFolder;
    for (const char* cur = modFolder; *cur != '\0'; ++cur) {
        if (*cur == '/' && cur[1] != '\0') stem = cur + 1;
    }
    return stem;
}

static void BuildLogFilePath(char* outPath, u32 outSize) {
    if (outPath == nullptr || outSize == 0) return;

    const System* system = System::sInstance;
    const char* stem = nullptr;
    if (system != nullptr) {
        stem = GetLogFileStem(system->GetModFolder());
    }
    if (stem != nullptr && stem[0] != '\0') {
        const int written = snprintf(outPath, outSize, "%s/%s.log", kLogsFolder, stem);
        if (written > 0 && static_cast<u32>(written) < outSize) return;
    }

    snprintf(outPath, outSize, "%s/%s", kLogsFolder, kDefaultLogFileName);
    outPath[outSize - 1] = '\0';
}

static bool OpenLogFileForAppend(IO* io, const char* path) {
    if (io == nullptr || path == nullptr) return false;

    if (io->OpenFile(path, FILE_MODE_READ_WRITE)) {
        const s32 size = io->GetFileSize();
        io->Seek(size > 0 ? static_cast<u32>(size) : 0);
        return true;
    }

    if (!io->CreateAndOpen(path, FILE_MODE_READ_WRITE)) {
        return false;
    }

    io->Seek(0);
    return true;
}

static void WriteLogLine(IO* io, const char* message) {
    if (io == nullptr || message == nullptr || message[0] == '\0') return;

    char line[kMaxLogLineLength];
    u32 length = static_cast<u32>(strlen(message));
    if (length >= sizeof(line)) length = sizeof(line) - 1;
    memcpy(line, message, length);
    if (length == 0 || line[length - 1] != '\n') {
        if (length + 1 < sizeof(line)) {
            line[length++] = '\n';
        }
    }
    line[length] = '\0';
    io->Write(length, line);
}

}  // namespace

void Pulsar_LogMessage(const char* message) {
    if (message == nullptr || message[0] == '\0') return;
    OS::Report("%s", message);

    IO* io = IO::sInstance;
    if (!EnsureLogFolder(io)) return;

    char path[IOS::ipcMaxPath];
    BuildLogFilePath(path, sizeof(path));
    if (!OpenLogFileForAppend(io, path)) return;

    WriteLogLine(io, message);
    io->Close();
}

bool Pulsar_FlushLogs() {
    return true;
}

}  // namespace Pulsar
