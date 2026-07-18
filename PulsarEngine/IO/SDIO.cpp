#include <IO/SDIO.hpp>
#include <core/rvl/ipc/ipc.hpp>

namespace Pulsar {

#define S_IFDIR 0040000 /* st_mode is directory */
#define S_IFMT 0170000 /* st_mode filetype mask */
#define SD_MAX_FILENAME_LENGTH 768 /* filename length limit imposed by sd driver */
#define EEXIST 17 /* errno code for 'File already exists' */

struct sd_vtable {
    int (*open)(void* file_struct, const char* path, int flags);
    int (*close)(int fd);
    int (*read)(int fd, void* ptr, size_t len);
    int (*write)(int fd, const void* ptr, size_t len);
    int (*rename)(const char* oldName, const char* newName);
    int (*stat)(const char* path, void* statbuf);
    int (*mkdir)(const char* path);
    dir_struct* (*diropen)(dir_struct* dir, const char* path);
    int (*dirnext)(dir_struct* dir, char* outFilename, void* filestatbuf);
    int (*dirclose)(dir_struct* dir);
    int (*seek)(int fd, int pos, int direction);
    int (*errno)();
};

const sd_vtable* __sd_vtable = reinterpret_cast<sd_vtable*>(0x81782e00);

u32 ios_mode_to_sd_mode(u32 mode) {
    switch (mode) {
        case IOS::MODE_WRITE:
            return O_WRONLY;
        case IOS::MODE_READ_WRITE:
            return O_RDWR;
        case IOS::MODE_NONE:
        case IOS::MODE_READ:
        default:
            return O_RDONLY;
    }
}

bool SDIO::OpenFile(const char* path, u32 mode) {
    return __sd_vtable->open(&fileData, path, ios_mode_to_sd_mode(mode)) != -1;
}

bool SDIO::CreateAndOpen(const char* path, u32 mode) {
    return __sd_vtable->open(&fileData, path, ios_mode_to_sd_mode(mode) | O_CREAT) != -1;
}

bool SDIO::RenameFile(const char* oldPath, const char* newPath) const {
    return __sd_vtable->rename(oldPath, newPath) == 0;
}

bool SDIO::FolderExists(const char* path) const {
    stat stat;
    if (__sd_vtable->stat(path, &stat) != 0) {
        return false;
    }
    return (stat.st_mode & S_IFMT) == S_IFDIR;
}

bool SDIO::CreateFolder(const char* path) {
    this->Bind(path);
    int res = __sd_vtable->mkdir(path);
    return res == 0 || __sd_vtable->errno() == EEXIST;
}

void SDIO::ReadFolder(const char* path) {
    if (!this->OpenFolderStream(path)) return;

    fileNames = new (heap) IOS::IPCPath[maxFileCount];
    if (fileNames == nullptr) {
        this->CloseFolderStream();
        return;
    }

    bool isDirectory = false;
    while (fileCount < maxFileCount &&
           this->ReadFolderEntry(fileNames[fileCount], IOS::ipcMaxPath, isDirectory)) {
        if (!isDirectory) ++fileCount;
    }
    this->CloseFolderStream();
}

bool SDIO::OpenFolderStream(const char* path) {
    this->CloseFolder();
    if (path == nullptr || __sd_vtable->diropen(&dirData, path) == nullptr) return false;

    snprintf(folderName, IOS::ipcMaxPath, "%s", path);
    isFolderOpen = true;
    return true;
}

bool SDIO::ReadFolderEntry(char* outFilename, u32 outFilenameSize, bool& outIsDirectory) {
    if (!isFolderOpen || outFilename == nullptr || outFilenameSize == 0) return false;

    char filename[SD_MAX_FILENAME_LENGTH];
    stat entryStat;
    while (true) {
        memset(&entryStat, 0, sizeof(entryStat));
        memset(filename, 0, sizeof(filename));
        if (__sd_vtable->dirnext(&dirData, filename, &entryStat) != 0) return false;

        const int written = snprintf(outFilename, outFilenameSize, "%s", filename);
        if (written <= 0 || static_cast<u32>(written) >= outFilenameSize) continue;

        outIsDirectory = (entryStat.st_mode & S_IFMT) == S_IFDIR;
        return true;
    }
}

void SDIO::CloseFolderStream() {
    if (!isFolderOpen) return;
    __sd_vtable->dirclose(&dirData);
    isFolderOpen = false;
}

void SDIO::CloseFolder() {
    if (fileNames) {
        delete[] (fileNames);
    }
    this->CloseFolderStream();
    fileNames = nullptr;
    folderName[0] = '\0';
    fileCount = 0;
}

s32 SDIO::GetFileSize() {
    return fileData.filesize;
}

int SDIO::fd() const {
    // The fd is always simply the address of the FILE_STRUCT.
    return reinterpret_cast<int>(&fileData);
}

s32 SDIO::Read(u32 size, void* bufferIn) {
    return __sd_vtable->read(fd(), bufferIn, size);
}

void SDIO::Seek(u32 offset) {
    __sd_vtable->seek(fd(), offset, 0);
}

s32 SDIO::Write(u32 length, const void* buffer) {
    return __sd_vtable->write(fd(), buffer, length);
}

s32 SDIO::Overwrite(u32 length, const void* buffer) {
    Seek(0);
    return __sd_vtable->write(fd(), buffer, length);
}

void SDIO::Close() {
    __sd_vtable->close(fd());
}

}  // namespace Pulsar
