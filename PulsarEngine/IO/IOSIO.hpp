#ifndef _IOSIO_
#define _IOSIO_

#include <IO/IO.hpp>
#include <kamek.hpp>

namespace Pulsar {

class IOSIO : public IO {
   public:
    IOSIO(IOType type, EGG::Heap* heap, EGG::TaskThread* taskThread) : isBusy(false), fd(-1), fileSize(-1), IO(type, heap, taskThread) {
        filePath[0] = '\0';
    }

    s32 GetFileSize();

    void CloseFolder() override;

    bool OpenFileDirectly(const char* path, u32 mode);
    s32 Read(u32 size, void* bufferIn) override;
    void Seek(u32 offset) override;
    s32 Write(u32 length, const void* buffer) override;
    s32 Overwrite(u32 length, const void* buffer) override;
    void Close() override;

   protected:
    bool isBusy;
    s32 fd;
    s32 fileSize;
    char filePath[IOS::ipcMaxPath];
};

}  // namespace Pulsar

#endif
