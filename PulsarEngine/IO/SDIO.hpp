#ifndef _SDIO_
#define _SDIO_

#include <IO/IO.hpp>

namespace Pulsar {

#define O_RDONLY 0
#define O_WRONLY 1
#define O_RDWR 2
#define O_APPEND 0x0008
#define O_CREAT 0x0200

struct file_struct {
    u32 filesize;
    u8 _unused[76];
};

struct dir_struct {
    u8 _unused[836];
};

struct stat {
    u8 _unused[8];
    u32 st_mode;
    u8 _unused2[76];
};

// Should be in sync with the assertions in runtime-ext
// size_assert(file_struct, 80);
// size_assert(dir_struct, 836);
// size_assert(stat, 88);

class SDIO : public IO {
   public:
    SDIO(IOType type, EGG::Heap* heap, EGG::TaskThread* taskThread) : IO(type, heap, taskThread) {
        offset_assert(stat, st_mode, 8);
        offset_assert(file_struct, filesize, 0);
        fileNames = nullptr;
    }

    bool OpenFile(const char* path, u32 mode) override;
    bool CreateAndOpen(const char* path, u32 mode) override;
    bool RenameFile(const char* oldPath, const char* newPath) const override;

    bool FolderExists(const char* path) const override;
    bool CreateFolder(const char* path) override;
    void ReadFolder(const char* path) override;
    void CloseFolder() override;

    s32 GetFileSize() override;

    s32 Read(u32 size, void* bufferIn) override;
    void Seek(u32 offset) override;
    s32 Write(u32 length, const void* buffer) override;
    s32 Overwrite(u32 length, const void* buffer) override;
    void Close() override;

   private:
    file_struct fileData;
    dir_struct dirData;

    int fd() const;
};
}  // namespace Pulsar

#endif
