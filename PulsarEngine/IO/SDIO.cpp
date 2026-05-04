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
    int (*diropen)(dir_struct* dir, const char* path);
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
    this->CloseFolder();
    fileCount = 0;
    fileNames = nullptr;

    __sd_vtable->diropen(&dirData, path);
    strncpy(folderName, path, IOS::ipcMaxPath);
    folderName[IOS::ipcMaxPath - 1] = '\0';
    char filename[SD_MAX_FILENAME_LENGTH];

    fileNames = new (heap) IOS::IPCPath[maxFileCount];
    stat stat;
    memset(&stat, 0, sizeof(stat));
    memset(filename, 0, sizeof(filename));
    while (__sd_vtable->dirnext(&dirData, filename, &stat) == 0) {
        if (fileCount >= maxFileCount) {
            break;
        }
        if ((stat.st_mode & S_IFMT) == S_IFDIR) {
            // Skip directories
            memset(filename, 0, sizeof(filename));
            continue;
        }
        strncpy(fileNames[fileCount], filename, IOS::ipcMaxPath);
        fileNames[fileCount][IOS::ipcMaxPath - 1] = '\0';
        fileCount++;
        memset(&stat, 0, sizeof(stat));
        memset(filename, 0, sizeof(filename));
    }
}

void SDIO::CloseFolder() {
    if (fileNames) {
        delete[] (fileNames);
        __sd_vtable->dirclose(&dirData);
    }
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
