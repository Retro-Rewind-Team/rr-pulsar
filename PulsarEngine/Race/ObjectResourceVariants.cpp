#include <kamek.hpp>
#include <include/c_stdio.h>
#include <MarioKartWii/3D/Model/ModelDirector.hpp>
#include <MarioKartWii/Archive/ArchiveMgr.hpp>
#include <MarioKartWii/KMP/GOBJ.hpp>
#include <MarioKartWii/KMP/KMPManager.hpp>
#include <MarioKartWii/System/Random.hpp>
#include <MarioKartWii/Objects/Object.hpp>

namespace Pulsar {
namespace Race {

static const u32 GET_BRRES_NAME_VTABLE_OFFSET = 0x34;
static const u32 GET_SUBFILE_NAME_VTABLE_OFFSET = 0x38;
static const u32 VARIANT_NAME_BUFFER_COUNT = 4;
static const u32 VARIANT_NAME_BUFFER_SIZE = 0x40;
static const char* FALLBACK_EMPTY_RESOURCE_NAME = "-";

enum VariantNameType {
    VARIANT_NAME_BRRES,
    VARIANT_NAME_KCL
};

typedef const char* (*ObjectNameGetter)(Object*);

struct ObjectGobjView {
    u8 padding[0xa0];
    const void* gobjLink;
};

static char sVariantNameBuffers[VARIANT_NAME_BUFFER_COUNT][VARIANT_NAME_BUFFER_SIZE];
static u32 sNextVariantNameBufferIdx = 0;

static char* GetNextVariantNameBuffer() {
    char* nameBuffer = sVariantNameBuffers[sNextVariantNameBufferIdx];
    sNextVariantNameBufferIdx = (sNextVariantNameBufferIdx + 1) % VARIANT_NAME_BUFFER_COUNT;
    return nameBuffer;
}

static const char* CallOriginalObjectNameGetter(Object* object, u32 vtableOffset) {
    if (object == nullptr) return nullptr;

    const u32* vtable = *reinterpret_cast<const u32* const*>(object);
    ObjectNameGetter getter = reinterpret_cast<ObjectNameGetter>(vtable[vtableOffset / 4]);
    return getter(object);
}

static const GOBJ* GetObjectGobj(const Object& object) {
    const ObjectGobjView& view = reinterpret_cast<const ObjectGobjView&>(object);
    if (view.gobjLink == nullptr) return nullptr;
    return *reinterpret_cast<GOBJ* const*>(view.gobjLink);
}

static bool TryGetObjectHolderIndex(const Object& object, u16& holderIdx) {
    const KMP::Manager* kmp = KMP::Manager::sInstance;
    if (kmp == nullptr || kmp->gobjSection == nullptr || kmp->gobjSection->holdersArray == nullptr) return false;

    const u16 gobjCount = kmp->gobjSection->pointCount;
    if (gobjCount == 0) return false;

    if (object.holderIdx < gobjCount) {
        holderIdx = static_cast<u16>(object.holderIdx);
        return true;
    }

    const GOBJ* gobj = GetObjectGobj(object);
    if (gobj == nullptr) return false;

    for (u16 i = 0; i < gobjCount; ++i) {
        const KMP::Holder<GOBJ>* holder = kmp->gobjSection->holdersArray[i];
        if (holder != nullptr && holder->raw == gobj) {
            holderIdx = i;
            return true;
        }
    }
    return false;
}

static u32 GetObjectVariantIndex(const Object& object) {
    const KMP::Manager* kmp = KMP::Manager::sInstance;
    if (kmp == nullptr || kmp->gobjSection == nullptr || kmp->gobjSection->holdersArray == nullptr) return 0;

    u16 holderIdx = 0;
    if (!TryGetObjectHolderIndex(object, holderIdx)) return 0;

    const KMP::Holder<GOBJ>* holder = kmp->gobjSection->holdersArray[holderIdx];
    if (holder == nullptr || holder->raw == nullptr) return 0;

    const u16 objectId = holder->raw->objID;
    u32 variantIndex = 0;

    for (u16 i = 0; i < holderIdx; ++i) {
        const KMP::Holder<GOBJ>* previousHolder = kmp->gobjSection->holdersArray[i];
        if (previousHolder != nullptr && previousHolder->raw != nullptr && previousHolder->raw->objID == objectId) {
            ++variantIndex;
        }
    }
    return variantIndex;
}

static bool DoesVariantResourceExist(const char* variantName, VariantNameType type) {
    if (variantName == nullptr || variantName[0] == '\0') return false;

    char fileName[VARIANT_NAME_BUFFER_SIZE];
    if (type == VARIANT_NAME_BRRES) {
        if (snprintf(fileName, sizeof(fileName), "%s.brres", variantName) <= 0) return false;
        return ModelDirector::BRRESExists(ARCHIVE_HOLDER_COURSE, fileName);
    }

    if (snprintf(fileName, sizeof(fileName), "%s.kcl", variantName) <= 0) return false;
    if (ArchiveMgr::sInstance == nullptr) return false;
    return ArchiveMgr::sInstance->GetFile(ARCHIVE_HOLDER_COURSE, fileName, nullptr) != nullptr;
}

static bool IsDigit(char value) {
    return value >= '0' && value <= '9';
}

static const char* GetVariantNameIfAvailable(Object* object, const char* baseName, VariantNameType type) {
    if (baseName == nullptr) return FALLBACK_EMPTY_RESOURCE_NAME;
    if (object == nullptr) return baseName;
    if (baseName[0] == '\0') return baseName;
    if (baseName[0] == '-' && baseName[1] == '\0') return baseName;

    const u32 variantIndex = GetObjectVariantIndex(*object);

    const u32 nameLen = static_cast<u32>(strlen(baseName));
    u32 digitStart = nameLen;
    while (digitStart > 0 && IsDigit(baseName[digitStart - 1])) --digitStart;
    const bool hasNumericSuffix = (digitStart > 1 && digitStart < nameLen && baseName[digitStart - 1] == '_');

    char* variantName = GetNextVariantNameBuffer();
    int writeCount = 0;

    if (hasNumericSuffix) {
        const u32 stemLen = digitStart - 1;  // trim trailing "_<number>"
        if (stemLen > 0) {
            writeCount = snprintf(variantName, VARIANT_NAME_BUFFER_SIZE, "%.*s_%u", stemLen, baseName, variantIndex);
        }
    } else {
        writeCount = snprintf(variantName, VARIANT_NAME_BUFFER_SIZE, "%s_%u", baseName, variantIndex);
    }

    if (writeCount <= 0 || writeCount >= static_cast<int>(VARIANT_NAME_BUFFER_SIZE)) return baseName;
    if (DoesVariantResourceExist(variantName, type)) return variantName;
    return baseName;
}

static const char* GetVariantBRRESName(Object* object) {
    const char* baseName = CallOriginalObjectNameGetter(object, GET_BRRES_NAME_VTABLE_OFFSET);
    return GetVariantNameIfAvailable(object, baseName, VARIANT_NAME_BRRES);
}
kmCall(0x8081fd68, GetVariantBRRESName);  // Object::LoadGraphics getResourcesName -> "%s.brres"

static const char* GetVariantKCLName(Object* object) {
    const char* baseName = CallOriginalObjectNameGetter(object, GET_SUBFILE_NAME_VTABLE_OFFSET);
    return GetVariantNameIfAvailable(object, baseName, VARIANT_NAME_KCL);
}
kmCall(0x8081aa84, GetVariantKCLName);  // GeoObjectKCL::LoadCollision getKclName -> "%s.kcl"

}  // namespace Race
}  // namespace Pulsar