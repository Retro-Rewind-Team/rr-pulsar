#ifndef _LOADER_
#define _LOADER_
#include <core/rvl/dvd/dvd.hpp>
#include <core/RK/RKSystem.hpp>

struct LoaderParams;

typedef void (*OSReport_t)(const char* str, ...);
typedef void (*OSFatal_t)(u32* fg, u32* bg, const char* str, ...);
typedef int (*DVDConvertPathToEntrynum_t)(const char* path);
typedef bool (*DVDFastOpen_t)(int entrynum, DVD::FileInfo* fileInfo);
typedef int (*DVDReadPrio_t)(DVD::FileInfo* fileInfo, void* buffer, int length, int offset, int unk);
typedef bool (*DVDClose_t)(DVD::FileInfo* fileInfo);
typedef int (*sprintf_t)(char* str, const char* format, ...);
typedef void (*NETSHA1Init_t)(void* ctx);
typedef void (*NETSHA1Update_t)(void* ctx, const void* data, u32 length);
typedef void (*NETSHA1GetDigest_t)(void* ctx, void* digest);

enum Region {
    PAL = 0,
    NTSC_U = 1,
    NTSC_J = 2,
    NTSC_K = 3
};

struct LoaderParams {
    OSReport_t OSReport;
    OSFatal_t OSFatal;
    DVDConvertPathToEntrynum_t DVDConvertPathToEntrynum;
    DVDFastOpen_t DVDFastOpen;
    DVDReadPrio_t DVDReadPrio;
    DVDClose_t DVDClose;
    sprintf_t sprintf;
    RKSystem* rkSystem;
    NETSHA1Init_t NETSHA1Init;
    NETSHA1Update_t NETSHA1Update;
    NETSHA1GetDigest_t NETSHA1GetDigest;
    Region region;
    u32 relStart;
};

void LoadKamekBinaryFromDisc(LoaderParams* funcs);
#endif