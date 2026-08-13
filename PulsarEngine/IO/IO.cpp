#include <kamek.hpp>
#include <PulsarSystem.hpp>
#include <IO/IO.hpp>
#include <IO/RiivoIO.hpp>
#include <IO/NANDIO.hpp>
#include <IO/SDIO.hpp>
#include <core/rvl/OS/OS.hpp>

namespace Pulsar {

IO *IO::sInstance = nullptr;

IO *IO::CreateInstance(IOType type, EGG::Heap *heap, EGG::TaskThread *const taskThread) {
    IO *io;

    switch (type) {
        case IOType_RIIVO:
            io = new (heap) RiivoIO(type, heap, taskThread);
            break;
        case IOType_ISO:
        case IOType_DOLPHIN:
            io = new (heap) NANDIO(type, heap, taskThread);
            break;
        case IOType_SD:
            io = new (heap) SDIO(type, heap, taskThread);
            break;
    }

    IO::sInstance = io;
    return io;
}

void IO::CreateFolderAsync(CreateRequest *request) {
    IO *io = IO::sInstance;
    io->CreateFolder(request->path);
    request->isFree = true;
}

s32 IO::ReadFolderFileFromPath(void *bufferIn, const char *path, u32 maxLength) {
    this->OpenFile(path, FILE_MODE_READ);
    const u32 size = this->GetFileSize();
    const u32 length = size <= maxLength ? size : maxLength;
    s32 ret = this->Read(length, bufferIn);
    this->Close();
    return ret;
}

}  // namespace Pulsar