#include <kamek.hpp>
#include <IO/LooseArchiveOverrides.hpp>
#include <core/nw4r/snd.hpp>

namespace Pulsar {
namespace Sound {

typedef bool (*ReadFileInfoFn)(const nw4r::snd::SoundArchive*, u32, nw4r::snd::SoundArchive::FileInfo*);
typedef bool (*ReadFilePosFn)(const nw4r::snd::SoundArchive*, u32, u32, nw4r::snd::SoundArchive::FilePos*);
typedef bool (*ReadGroupItemInfoFn)(const nw4r::snd::SoundArchive*, u32, u32, nw4r::snd::SoundArchive::GroupItemInfo*);

static const ReadFileInfoFn sReadFileInfo = reinterpret_cast<ReadFileInfoFn>(0x8009dff0);
static const ReadFilePosFn sReadFilePos = reinterpret_cast<ReadFilePosFn>(0x8009e000);
static const ReadGroupItemInfoFn sReadGroupItemInfo = reinterpret_cast<ReadGroupItemInfoFn>(0x8009dfd0);

static const void* FindGroupFileAddress(nw4r::snd::SoundArchivePlayer* player, u32 fileId, bool waveData) {
    if (player == nullptr || player->soundArchive == nullptr) return nullptr;

    nw4r::snd::SoundArchive::FileInfo fileInfo;
    if (!sReadFileInfo(player->soundArchive, fileId, &fileInfo)) return nullptr;

    for (u32 index = 0; index < fileInfo.filePosCount; ++index) {
        nw4r::snd::SoundArchive::FilePos filePos;
        if (!sReadFilePos(player->soundArchive, fileId, index, &filePos)) continue;

        u32 groupBase = 0;
        if (player->groupTable != nullptr && filePos.groupId < player->groupTable[0]) {
            const u32 groupTableIndex = filePos.groupId * 2 + (waveData ? 2 : 1);
            groupBase = player->groupTable[groupTableIndex];
        }
        if (groupBase == 0) continue;

        nw4r::snd::SoundArchive::GroupItemInfo groupItemInfo;
        if (!sReadGroupItemInfo(player->soundArchive, filePos.groupId, filePos.groupIndex, &groupItemInfo)) continue;

        return reinterpret_cast<const void*>(groupBase + (waveData ? groupItemInfo.waveDataOffset : groupItemInfo.offset));
    }

    return nullptr;
}

static const void* GetFileAddressWithLooseBRSAROverride(nw4r::snd::SoundArchivePlayer* player, u32 fileId) {
    const void* fileData = nullptr;
    u32 fileDataSize = 0;
    const void* waveData = nullptr;
    u32 waveDataSize = 0;
    if (IOOverrides::ResolveLooseBRSAROverride(fileId, fileData, fileDataSize, waveData, waveDataSize)) {
        return fileData;
    }

    const void* ret = nullptr;
    if (player != nullptr && player->fileManager != nullptr) {
        ret = player->fileManager->GetFileAddress(fileId);
    }
    if (ret != nullptr) return ret;
    if (player == nullptr || player->soundArchive == nullptr) return nullptr;

    ret = player->soundArchive->detail_GetFileAddress(fileId);
    if (ret != nullptr) return ret;

    return FindGroupFileAddress(player, fileId, false);
}
kmBranch(0x800a1560, GetFileAddressWithLooseBRSAROverride);

static const void* GetFileWaveDataAddressWithLooseBRSAROverride(nw4r::snd::SoundArchivePlayer* player, u32 fileId) {
    const void* fileData = nullptr;
    u32 fileDataSize = 0;
    const void* waveData = nullptr;
    u32 waveDataSize = 0;
    if (IOOverrides::ResolveLooseBRSAROverride(fileId, fileData, fileDataSize, waveData, waveDataSize) &&
        waveData != nullptr && waveDataSize > 0) {
        return waveData;
    }

    if (player == nullptr || player->soundArchive == nullptr) return nullptr;

    const void* ret = player->soundArchive->detail_GetWaveDataFileAddress(fileId);
    if (ret != nullptr) return ret;

    if (player->fileManager != nullptr) {
        ret = player->fileManager->GetFileWaveDataAddress(fileId);
        if (ret != nullptr) return ret;
    }

    return FindGroupFileAddress(player, fileId, true);
}
kmBranch(0x800a16b0, GetFileWaveDataAddressWithLooseBRSAROverride);

}  // namespace Sound
}  // namespace Pulsar
