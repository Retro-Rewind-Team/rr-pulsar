#include <IO/IOSIO.hpp>

namespace Pulsar {

// FILE
#pragma suppress_warnings on

bool IOSIO::OpenFileDirectly(const char *path, u32 mode) {
    if (type == IOType_ISO) return -1;
    this->fd = IO::OpenFix(path, static_cast<IOS::Mode>(mode));
    return this->fd >= 0;
}

#pragma suppress_warnings reset

s32 IOSIO::Read(u32 size, void *bufferIn) {
    if (this->fd < 0) return 0;
    return IOS::Read(this->fd, bufferIn, size);
}

s32 IOSIO::Write(u32 length, const void *buffer) {
    if (this->fd < 0) return -1;
    return IOS::Write(this->fd, buffer, length);
}

s32 IOSIO::Overwrite(u32 length, const void *buffer) {
    if (this->fd < 0) return -1;
    IOS::Seek(this->fd, 0, IOS::SEEK_START);
    return IOS::Write(this->fd, buffer, length);
}

void IOSIO::Close() {
    if (this->fd >= 0) IOS::Close(this->fd);
    this->fd = -1;
    this->fileSize = -1;
}

s32 IOSIO::GetFileSize() {
    if (this->fileSize < 0 && this->fd >= 0) {
        s32 size = IOS::Seek(this->fd, 0, IOS::SEEK_END);
        if (size >= 0) {
            this->fileSize = size;
            IOS::Seek(this->fd, 0, IOS::SEEK_START);
        }
    }
    return this->fileSize;
}

void IOSIO::Seek(u32 offset) {
    IOS::Seek(this->fd, offset, IOS::SEEK_START);
}

// FOLDER
void IOSIO::CloseFolder() {
    isBusy = false;
    this->Close();
    if (this->fileNames != nullptr) delete[] (this->fileNames);
    this->fileNames = nullptr;
    this->folderName[0] = '\0';
    this->fileCount = 0;
}

}  // namespace Pulsar
