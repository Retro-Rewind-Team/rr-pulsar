#ifndef _PUL_REGION_
#define _PUL_REGION_
#include <kamek.hpp>

namespace Pulsar {
namespace Region {
struct Subregion {
    u32 nameBmg;
    u16 longitude;
    u16 latitude;
    u8 state;
};
struct Country {
    u32 nameBmg;
    u16 longitude;
    u16 latitude;
    u8 state;
    u8 lineRegion;
    u16 subregionOffset;
    u8 subregionCount;
};

static const u32 countryCount = 254;
const Country *GetCountry(u32 id);
u32 GetCountryChoiceCount();
u8 GetCountryAt(u32 listIndex);
u32 GetListIndex(u8 country);
u8 GetSelectedCountry();
u8 GetSelectedSubregion();
const Subregion *GetSubregion(u8 country, u8 state);
const Subregion *GetSubregionAt(u8 country, u32 index);
u32 GetSubregionCount(u8 country);
u8 GetWiiCountry();
u8 GetEffectiveCountry();
u8 GetLineRegion(u8 country);
bool GetLocation(u32 &location, u16 &longitude, u16 &latitude);
void GetPreview(u8 country, u8 state, u16 &longitude, u16 &latitude);
}  // namespace Region
}  // namespace Pulsar
#endif
