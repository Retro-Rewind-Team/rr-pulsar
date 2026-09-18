#include <Settings/Region.hpp>
#include <Settings/Settings.hpp>
#include <core/System/SystemManager.hpp>
#include <MarioKartWii/RKNet/USER.hpp>

namespace Pulsar {
namespace Region {
static const Subregion subregions[] = {
    {0x9002e, 0x6166, 0x1901, 24},
    {0x90053, 0x5cb6, 0x17e1, 41},
    {0x90003, 0x6480, 0x1e9e, 3},
    {0x9001c, 0x634f, 0x1933, 15},
    {0x90034, 0x6085, 0x18e5, 27},
    {0x90061, 0x5acc, 0x12a2, 48},
    {0x90037, 0x605f, 0x18a8, 28},
    {0x90000, 0x6363, 0x1960, 2},
    {0x901c4, 0xd6cb, 0xe729, 3},
    {0x901cc, 0xd25c, 0xe9ac, 7},
    {0x901da, 0xcf11, 0xe89e, 14},
    {0x901ea, 0xd4d6, 0xe982, 22},
    {0x90231, 0xe49d, 0xf6c5, 7},
    {0x901be, 0xddee, 0xf4c7, 2},
    {0x90240, 0xe0c3, 0xf1d7, 13},
    {0x90246, 0xdcfb, 0xedee, 16},
    {0x9024b, 0xe142, 0xefb8, 18},
    {0x90252, 0xdb98, 0xeaa5, 20},
    {0x90258, 0xdd77, 0xec63, 23},
    {0x9025a, 0xdeda, 0xef44, 24},
    {0x90289, 0xaf50, 0x2614, 3},
    {0x9028b, 0xa846, 0x2270, 4},
    {0x90287, 0xca23, 0x2030, 2},
    {0x902ad, 0xcd5d, 0x2148, 10},
    {0x904e3, 0xade4, 0x1738, 4},
    {0x901be, 0xb981, 0x0dd1, 2},
    {0x90502, 0xb685, 0x0eb2, 15},
    {0x9050f, 0xb8aa, 0x1240, 20},
    {0x90519, 0xc136, 0x0d27, 24},
    {0x9052a, 0xc046, 0x0ee8, 32},
    {0x9067e, 0xc938, 0x1ba8, 2},  // District of Columbia
    {0x83370, 0xa06a, 0x2975, 3},  // Alaska
    {0x83371, 0xc2a2, 0x1704, 4},  // Alabama
    {0x83372, 0xbe60, 0x18b5, 5},  // Arkansas
    {0x90689, 0xb04e, 0x17c9, 6},  // Arizona
    {0x9068b, 0xa99b, 0x1b6f, 7},  // California
    {0x90690, 0xb559, 0x1c42, 8},  // Colorado
    {0x83373, 0xcc50, 0x1db2, 9},  // Connecticut
    {0x83374, 0xca4c, 0x1bd8, 10},  // Delaware
    {0x90696, 0xc412, 0x15a5, 11},  // Florida
    {0x90699, 0xc3fe, 0x17ff, 12},  // Georgia
    {0x9069c, 0x8fbf, 0x0f26, 13},  // Hawaii
    {0x83375, 0xbd70, 0x1d95, 14},  // Iowa
    {0x83376, 0xad5e, 0x1f03, 15},  // Idaho
    {0x906a4, 0xc041, 0x1c4d, 16},  // Illinois
    {0x83377, 0xc2bc, 0x1c47, 17},  // Indiana
    {0x83378, 0xbbf7, 0x1bc4, 18},  // Kansas
    {0x83379, 0xc3a6, 0x1b2a, 19},  // Kentucky
    {0x8337a, 0xbf2e, 0x15a7, 20},  // Louisiana
    {0x906b0, 0xcd78, 0x1e1f, 21},  // Massachusetts
    {0x8337b, 0xc99b, 0x1bb7, 22},  // Maryland
    {0x8337c, 0xce61, 0x1f82, 23},  // Maine
    {0x8337d, 0xc3e0, 0x1e63, 24},  // Michigan
    {0x8337e, 0xbdcd, 0x1ff5, 25},  // Minnesota
    {0x8337f, 0xbe75, 0x1b6e, 26},  // Missouri
    {0x83380, 0xbfdf, 0x16f7, 27},  // Mississippi
    {0x83381, 0xb055, 0x2121, 28},  // Montana
    {0x83382, 0xc815, 0x1970, 29},  // North Carolina
    {0x83383, 0xb855, 0x2149, 30},  // North Dakota
    {0x83384, 0xbb43, 0x1d03, 31},  // Nebraska
    {0x83385, 0xcd21, 0x1eb9, 32},  // New Hampshire
    {0x83386, 0xcada, 0x1c99, 33},  // New Jersey
    {0x83387, 0xb4ab, 0x1960, 34},  // New Mexico
    {0x83388, 0xaad6, 0x1bd9, 35},  // Nevada
    {0x906de, 0xcb8e, 0x1e54, 36},  // New York
    {0x906e1, 0xc4fb, 0x1c6a, 37},  // Ohio
    {0x83389, 0xbaa8, 0x1938, 38},  // Oklahoma
    {0x8338a, 0xa883, 0x1ff5, 39},  // Oregon
    {0x906e8, 0xc954, 0x1ca3, 40},  // Pennsylvania
    {0x8338b, 0xcd38, 0x1dbd, 41},  // Rhode Island
    {0x8338c, 0xc661, 0x182d, 42},  // South Carolina
    {0x8338d, 0xb8a4, 0x1f8d, 43},  // South Dakota
    {0x8338e, 0xc24a, 0x19b7, 44},  // Tennessee
    {0x906fe, 0xba7f, 0x1585, 45},  // Texas
    {0x8338f, 0xb06f, 0x1cfc, 46},  // Utah
    {0x83390, 0xc8eb, 0x1ab4, 47},  // Virginia
    {0x83391, 0xcc64, 0x1f79, 48},  // Vermont
    {0x90707, 0xa89b, 0x2173, 49},  // Washington
    {0x83392, 0xc06e, 0x1ea1, 50},  // Wisconsin
    {0x83393, 0xc5f4, 0x1b45, 51},  // West Virginia
    {0x83394, 0xb577, 0x1d41, 52},  // Wyoming
    {0x9098f, 0x69fc, 0xe6dd, 2},
    {0x90995, 0x6b8c, 0xe7e7, 3},
    {0x9099c, 0x5d0b, 0xf729, 4},
    {0x909a3, 0x6cd1, 0xec7b, 5},
    {0x909a5, 0x6291, 0xe72c, 6},
    {0x909ac, 0x68bd, 0xe188, 7},
    {0x90660, 0x6716, 0xe51d, 8},
    {0x909b2, 0x5264, 0xe94b, 9},
    {0x90c3c, 0x0581, 0x228c, 3},
    {0x90c58, 0xfece, 0x2236, 8},
    {0x90c63, 0x0636, 0x1dcf, 11},
    {0x90c37, 0x01ab, 0x22bd, 2},
    {0x90c96, 0x03d2, 0x1ec9, 22},
    {0x90c9a, 0x036f, 0x2089, 23},
    {0x90cb4, 0x0686, 0x22ae, 4},
    {0x90cb8, 0x0837, 0x223a, 5},
    {0x90cab, 0x0988, 0x2559, 2},
    {0x90cc6, 0x071a, 0x2614, 8},
    {0x90cb0, 0x05db, 0x239b, 3},
    {0x90cd9, 0x04d1, 0x2470, 11},
    {0x90cea, 0x09c4, 0x244a, 14},
    {0x90e31, 0x0a22, 0x1d0a, 16},
    {0x90de7, 0x08e1, 0x1dca, 2},
    {0x90dfd, 0x0688, 0x2055, 6},
    {0x90e4b, 0x0679, 0x1be3, 21},
    {0x90e45, 0x097f, 0x1b1b, 20},
    {0x90e1a, 0x0800, 0x1f21, 11},
    {0x90e0b, 0x08c5, 0x204e, 8},
    {0x90fd8, 0x7c46, 0xe5c4, 3},
    {0x90fdd, 0x7ac4, 0xe10c, 5},
    {0x90fe0, 0x7939, 0xdf60, 6},
    {0x90ff2, 0x7ca2, 0xe523, 14},
    {0x90fd5, 0x7c4a, 0xe29f, 2},
    {0x911d0, 0xfbbc, 0x1a95, 3},
    {0x91220, 0xfe1a, 0x1e78, 17},
    {0x911e9, 0xf507, 0x13ff, 7},
    {0x91202, 0x018b, 0x1d6d, 11},
    {0x911cc, 0xfd5e, 0x1cbd, 2},
    {0x91209, 0xffbe, 0x1c11, 12},
    {0x9138a, 0xffeb, 0x24a0, 2},
    {0x9139d, 0xffe9, 0x24a0, 7},
    {0x91396, 0xfbcd, 0x26d1, 6},
    {0x90164, 0xfdb9, 0x27ca, 4},
    {0x90197, 0xfdbe, 0x249b, 5},
    {0x919c5, 0x5bc1, 0x18f2, 3},
    {0x919da, 0x5a4f, 0x1aaa, 9},
    {0x919cd, 0x5a0c, 0x1aaa, 5},
    {0x919fe, 0x59fa, 0x17d5, 17},
    {0x919c0, 0x5a4f, 0x1aaa, 2},
    {0x91d71, 0x52cc, 0x1c62, 2},
    {0x91d82, 0x54d5, 0x128c, 7},
    {0x91d86, 0x506d, 0x106d, 9},
    {0x91d7a, 0x5661, 0x1638, 4},
    {0x91daa, 0x4a09, 0x15c8, 25},
    {0x91dae, 0x556a, 0x158b, 27},
    {0x9201a, 0x36e5, 0x1456, 2},
    {0x92030, 0x33a3, 0x1061, 8},
    {0x92045, 0x372b, 0x093b, 17},
    {0x92036, 0x36b2, 0x0608, 12},
    {0x9203d, 0x33c9, 0x0d7e, 14},
    {0x92054, 0x3917, 0x094d, 23},
    {0x92071, 0x398a, 0x1317, 33},
    {0x92058, 0x3ed7, 0x100c, 25},
};

static const Country countries[] = {
    {0x90063, 0x6363, 0x1960, 2, 0, 0, 8},
    {0x900dd, 0x6aaa, 0xc93f, 2, 4, 8, 0},
    {0x900eb, 0xcf75, 0x08a2, 2, 1, 8, 0},
    {0x90100, 0xd6dd, 0xdb3e, 2, 1, 8, 0},
    {0x90164, 0xfdbc, 0x27c9, 2, 2, 8, 0},
    {0x90197, 0xfdbd, 0x249c, 2, 2, 8, 0},
    {0x9019c, 0xd32b, 0x0cd2, 1, 1, 8, 0},
    {0x901a0, 0xd32b, 0x0cf4, 1, 1, 8, 0},
    {0x901b7, 0xd405, 0x0c2b, 2, 1, 8, 0},
    {0x901f7, 0xd647, 0xe768, 2, 1, 8, 4},
    {0x901fc, 0xce33, 0x08e6, 1, 1, 12, 0},
    {0x901fe, 0xc8ff, 0x11d6, 1, 1, 12, 0},
    {0x90201, 0xd59c, 0x0950, 1, 1, 12, 0},
    {0x90206, 0xc0e1, 0x0c44, 2, 1, 12, 0},
    {0x90224, 0xcf8a, 0xf445, 2, 1, 12, 0},
    {0x90268, 0xddee, 0xf4c7, 2, 1, 12, 8},
    {0x90280, 0xd20c, 0x0d1b, 2, 1, 20, 0},
    {0x902c0, 0xca23, 0x2030, 2, 1, 20, 4},
    {0x902cd, 0xc620, 0x0db7, 2, 1, 24, 0},
    {0x90306, 0xcdc0, 0xe837, 2, 1, 24, 0},
    {0x90355, 0xcb40, 0x0305, 2, 1, 24, 0},
    {0x90367, 0xc436, 0x0710, 2, 1, 24, 0},
    {0x90369, 0xd457, 0x0ae1, 1, 1, 24, 0},
    {0x903aa, 0xce4c, 0x0d21, 2, 1, 24, 0},
    {0x903e3, 0xc82e, 0xffd9, 2, 1, 24, 0},
    {0x90400, 0xc092, 0x09bf, 2, 1, 24, 0},
    {0x90411, 0xdacb, 0x0382, 2, 1, 24, 0},
    {0x90418, 0xd417, 0x0891, 1, 1, 24, 0},
    {0x9041c, 0xd41d, 0x0b60, 1, 1, 24, 0},
    {0x90420, 0xbfa1, 0x0a65, 2, 1, 24, 0},
    {0x9046e, 0xd6a4, 0x04d5, 2, 1, 24, 0},
    {0x9048b, 0xcc90, 0x0d2e, 2, 1, 24, 0},
    {0x904b4, 0xc1fb, 0x0a06, 2, 1, 24, 0},
    {0x904d8, 0xc963, 0x0ccc, 2, 1, 24, 0},
    {0x904dd, 0xd491, 0x0a61, 1, 1, 24, 0},
    {0x90506, 0xb981, 0x0dd1, 2, 1, 24, 6},
    {0x90532, 0xd3c2, 0x0be0, 1, 1, 30, 0},
    {0x90534, 0xcefc, 0x089d, 1, 1, 30, 0},
    {0x9055e, 0xc2a8, 0x08a3, 2, 1, 30, 0},
    {0x90562, 0xc772, 0x0660, 2, 1, 30, 0},
    {0x9059d, 0xd6ff, 0xee09, 2, 1, 30, 0},
    {0x905d4, 0xc936, 0xf76f, 2, 1, 30, 0},
    {0x90601, 0xd367, 0x0c4d, 2, 1, 30, 0},
    {0x90608, 0xd4a0, 0x09f4, 1, 1, 30, 0},
    {0x9062d, 0xd477, 0x095b, 2, 1, 30, 0},
    {0x90648, 0xd8c6, 0x0425, 2, 1, 30, 0},
    {0x90662, 0xd442, 0x0792, 2, 1, 30, 0},
    {0x90677, 0xcd6a, 0x0f42, 2, 1, 30, 0},
    {0x90732, 0xc938, 0x1ba8, 2, 1, 30, 51},
    {0x9075d, 0xd80f, 0xe737, 2, 1, 44, 0},
    {0x9072b, 0xd1d4, 0x0d0b, 1, 1, 44, 0},
    {0x9078f, 0xd06b, 0x0777, 2, 1, 44, 0},
    {0x907b7, 0x1fa7, 0x1c92, 2, 2, 44, 0},
    {0x907dc, 0x139a, 0x2654, 2, 2, 44, 0},
    {0x90699, 0x1fd8, 0x1daa, 2, 2, 44, 0},
    {0x9085c, 0x0f0d, 0x1e57, 2, 2, 44, 0},
    {0x907e7, 0x1d2a, 0x1e94, 2, 2, 44, 0},
    {0x908c2, 0x213e, 0x1c50, 2, 2, 44, 0},
    {0x908f1, 0x17b9, 0x1903, 2, 2, 44, 0},
    {0x90914, 0x1f44, 0x1e07, 2, 2, 44, 0},
    {0x90944, 0x1514, 0x214f, 2, 2, 44, 0},
    {0x90949, 0x0e2c, 0x2abc, 1, 2, 44, 0},
    {0x9095d, 0xfb31, 0x2c18, 2, 2, 44, 0},
    {0x9098a, 0x0e19, 0x1d63, 2, 2, 44, 0},
    {0x909e3, 0x69fc, 0xe6dd, 2, 3, 81, 8},
    {0x90a16, 0x0ba4, 0x2247, 2, 2, 52, 0},
    {0x90a30, 0x031b, 0x2427, 2, 2, 52, 0},
    {0x90a4c, 0x0d17, 0x1f2f, 2, 2, 52, 0},
    {0x90a68, 0x126b, 0xee79, 2, 2, 52, 0},
    {0x90ad7, 0x1092, 0x1e5e, 2, 2, 52, 0},
    {0x90af8, 0x0b5a, 0x2092, 2, 2, 52, 0},
    {0x90b12, 0x17bb, 0x1901, 2, 2, 52, 0},
    {0x90b77, 0x0a43, 0x239b, 2, 2, 52, 0},
    {0x90bd6, 0x08ed, 0x279a, 2, 2, 52, 0},
    {0x90c0a, 0x1196, 0x2a43, 2, 2, 52, 0},
    {0x90c32, 0x11bb, 0x2ac9, 2, 2, 52, 0},
    {0x90ca6, 0x01ab, 0x22bd, 2, 2, 89, 6},
    {0x90d00, 0x0988, 0x2559, 2, 2, 95, 7},
    {0x90d5b, 0x10de, 0x1b01, 2, 2, 65, 0},
    {0x90d8a, 0x0d91, 0x21c5, 2, 2, 65, 0},
    {0x90dc6, 0xf06f, 0x2d9b, 2, 2, 65, 0},
    {0x90de1, 0xfb8d, 0x25ee, 2, 2, 65, 0},
    {0x90e52, 0x08e1, 0x1dca, 2, 2, 102, 7},
    {0x90e9e, 0x1120, 0x287f, 2, 2, 72, 0},
    {0x90eba, 0x1388, 0xeb27, 2, 2, 72, 0},
    {0x90ec2, 0x06c5, 0x2185, 2, 2, 72, 0},
    {0x90edb, 0x11fa, 0x26e2, 2, 2, 72, 0},
    {0x90ee1, 0x045b, 0x2347, 2, 2, 72, 0},
    {0x90f0e, 0x0f40, 0x1dde, 2, 2, 72, 0},
    {0x90f15, 0x0a52, 0x1987, 2, 2, 72, 0},
    {0x90f54, 0x0db3, 0x1e2d, 2, 2, 72, 0},
    {0x90f78, 0x1728, 0xed89, 2, 2, 72, 0},
    {0x90f97, 0x0c25, 0xeff7, 2, 2, 72, 0},
    {0x90fce, 0x0379, 0x253d, 2, 2, 72, 0},
    {0x91005, 0x7c4a, 0xe29f, 2, 3, 109, 5},
    {0x91037, 0x079d, 0x2a9f, 2, 2, 77, 0},
    {0x91099, 0x0ef0, 0x2526, 2, 2, 77, 0},
    {0x910b7, 0xf980, 0x1b89, 2, 2, 77, 0},
    {0x91112, 0x128c, 0x1f98, 2, 2, 77, 0},
    {0x91149, 0x1abf, 0x27a8, 2, 2, 77, 0},
    {0x9115e, 0x0e91, 0x1fe1, 2, 2, 77, 0},
    {0x91178, 0x0c2d, 0x223d, 2, 2, 77, 0},
    {0x91194, 0x0a50, 0x20bf, 2, 2, 77, 0},
    {0x911c5, 0x13f2, 0xed69, 2, 2, 77, 0},
    {0x9122a, 0xfd5e, 0x1cbd, 2, 2, 114, 6},
    {0x91239, 0x1624, 0xed49, 2, 2, 83, 0},
    {0x912ab, 0x0cda, 0x2a28, 2, 2, 83, 0},
    {0x91312, 0x0546, 0x2163, 2, 2, 83, 0},
    {0x91383, 0x175c, 0x1c64, 2, 2, 83, 0},
    {0x91405, 0xffeb, 0x24a0, 2, 2, 120, 5},
    {0x91448, 0x141c, 0xf50a, 2, 2, 88, 0},
    {0x91475, 0x1614, 0xf354, 2, 2, 88, 0},
    {0x9148f, 0x2378, 0x1cb9, 2, 2, 88, 0},
    {0x914d6, 0xf49b, 0x0cd9, 2, 2, 88, 0},
    {0x9151d, 0xfa50, 0x08fc, 2, 2, 88, 0},
    {0x91545, 0x0182, 0x099b, 2, 2, 88, 0},
    {0x915ad, 0x0ab4, 0x08a1, 2, 2, 88, 0},
    {0x91623, 0x1727, 0x0b05, 2, 2, 88, 0},
    {0x9164a, 0x1bae, 0x0ae5, 2, 2, 88, 0},
    {0x91650, 0x1eae, 0x083d, 2, 2, 88, 0},
    {0x916cd, 0x203c, 0x0172, 2, 2, 88, 0},
    {0x916d1, 0x0114, 0x1e38, 1, 2, 88, 0},
    {0x913ed, 0xfc33, 0x19b1, 1, 2, 88, 0},
    {0x913d1, 0xfe33, 0x232b, 1, 2, 88, 0},
    {0x91390, 0xfcd0, 0x2681, 1, 2, 88, 0},
    {0x913d7, 0xfe80, 0x22fa, 1, 2, 88, 0},
    {0x916da, 0x0546, 0x1f19, 1, 2, 88, 0},
    {0x91771, 0x5667, 0x11cd, 2, 4, 88, 0},
    {0x917c6, 0x4a9c, 0x083a, 2, 4, 88, 0},
    {0x91833, 0x48f5, 0x0cc6, 2, 4, 88, 0},
    {0x918a7, 0x4c07, 0x2213, 2, 4, 88, 0},
    {0x91914, 0x4456, 0x0e0b, 2, 4, 88, 0},
    {0x91943, 0x3c77, 0x137f, 2, 4, 88, 0},
    {0x9197c, 0x4b46, 0x0ef4, 2, 4, 88, 0},
    {0x919b9, 0x5969, 0x1bbf, 2, 5, 88, 0},
    {0x91a03, 0x5a4f, 0x1aaa, 2, 5, 125, 5},
    {0x91a37, 0x4046, 0x10e6, 2, 4, 93, 0},
    {0x91a9f, 0x3fbd, 0x1389, 2, 4, 93, 0},
    {0x91ab8, 0x51bc, 0x037a, 2, 4, 93, 0},
    {0x91ace, 0x3445, 0x02f8, 2, 4, 93, 0},
    {0x91af7, 0x38c6, 0x04ee, 2, 4, 93, 0},
    {0x91b3b, 0x594a, 0xf9e9, 2, 4, 93, 0},
    {0x913e6, 0x3379, 0xfad1, 1, 4, 93, 0},
    {0x91b42, 0x5147, 0x0ff9, 1, 4, 93, 0},
    {0x91b45, 0x50c8, 0x0fcc, 1, 4, 93, 0},
    {0x90ff4, 0x8e63, 0xf0ec, 2, 3, 93, 0},
    {0x90ffb, 0x872c, 0xf274, 1, 3, 93, 0},
    {0x909dc, 0x7771, 0xeb58, 1, 3, 93, 0},
    {0x9071e, 0x679f, 0x0ad0, 2, 3, 93, 0},
    {0x90715, 0x869d, 0xf5d9, 2, 3, 93, 0},
    {0x9071c, 0x66ee, 0x0995, 1, 3, 93, 0},
    {0x91c1e, 0x4bf7, 0xfb9c, 2, 1, 93, 0},
    {0x91c23, 0x49da, 0x00eb, 1, 1, 93, 0},
    {0x91cd0, 0x477a, 0x09c7, 2, 1, 93, 0},
    {0x91d24, 0x5608, 0x0a62, 2, 1, 93, 0},
    {0x91d4c, 0x4851, 0x0240, 2, 1, 93, 0},
    {0x91d5b, 0xd34f, 0x0cba, 2, 1, 93, 0},
    {0x91d5f, 0xd325, 0x0cda, 1, 1, 93, 0},
    {0x91d6a, 0xd80e, 0x2143, 2, 1, 93, 0},
    {0x91dc4, 0x52cc, 0x1c62, 2, 4, 130, 6},
    {0x91e72, 0x3131, 0x188d, 2, 2, 99, 0},
    {0x91ee9, 0x32cc, 0x2462, 2, 2, 99, 0},
    {0x91f20, 0x350e, 0x1e7d, 2, 2, 99, 0},
    {0x91f49, 0x33f4, 0x17f5, 2, 2, 99, 0},
    {0x91f66, 0x30e9, 0x1b67, 2, 2, 99, 0},
    {0x91f92, 0x2981, 0x1af9, 2, 2, 99, 0},
    {0x91fee, 0x3141, 0x1d5e, 2, 2, 99, 0},
    {0x92013, 0x26a9, 0x1166, 2, 1, 99, 0},
    {0x92079, 0x36e5, 0x1456, 2, 2, 136, 8},
    {0x920f8, 0x1638, 0x155e, 2, 1, 107, 0},
    {0x92119, 0x29aa, 0x10ca, 2, 1, 107, 0},
    {0x92140, 0x24a5, 0x11fb, 2, 1, 107, 0},
    {0x9216c, 0x221e, 0x14e2, 2, 1, 107, 0},
    {0x921a4, 0x2142, 0x1185, 2, 1, 107, 0},
    {0x921e1, 0x19d0, 0x17d2, 2, 1, 107, 0},
    {0x92201, 0x23f8, 0x12a4, 2, 1, 107, 0},
    {0x9225a, 0x198d, 0x16b8, 2, 1, 107, 0},
    {0x9227b, 0x248b, 0x1961, 2, 2, 107, 0},
    {0x922ec, 0x1f8f, 0x17b4, 2, 2, 107, 0},
    {0x9231e, 0x190a, 0x1699, 2, 2, 107, 0},
    {0x92358, 0x1940, 0x1818, 2, 2, 107, 0},
    {0x9236c, 0x1907, 0x16af, 2, 2, 107, 0},
    {0x92402, 0x1f6f, 0x0aea, 2, 2, 107, 0},
    {0x92406, 0x08d7, 0x1f3d, 1, 2, 107, 0},
    {0x92409, 0x08da, 0x1dcc, 1, 2, 107, 0},
    {0x913db, 0xd1ef, 0x16f8, 1, 1, 107, 0},
    {0x92430, 0x95a4, 0xf388, 2, 3, 107, 0},
    {0x90c9f, 0x276e, 0xf128, 1, 2, 107, 0},
    {0x90ca4, 0x2029, 0xf6ea, 1, 2, 107, 0},
    {0x92444, 0x765e, 0xf029, 2, 3, 107, 0},
    {0x92450, 0x82b7, 0xf68e, 2, 3, 107, 0},
    {0x92491, 0x0518, 0x0648, 2, 2, 107, 0},
    {0x924d5, 0x0969, 0xf9b8, 2, 2, 107, 0},
    {0x924f2, 0xffdc, 0x03f2, 2, 2, 107, 0},
    {0x92502, 0x00de, 0x045c, 2, 2, 107, 0},
    {0x9251e, 0x01da, 0x049e, 2, 2, 107, 0},
    {0x92542, 0xfee9, 0x08c9, 2, 2, 107, 0},
    {0x92567, 0xfc3f, 0x04d8, 2, 2, 107, 0},
    {0x9258a, 0xf852, 0x047d, 2, 2, 107, 0},
    {0x92598, 0xf697, 0x0608, 2, 2, 107, 0},
    {0x925ac, 0xf642, 0x06c4, 2, 2, 107, 0},
    {0x925b9, 0xf4ef, 0x086d, 2, 2, 107, 0},
    {0x925da, 0xf398, 0x0a72, 2, 2, 107, 0},
    {0x925e8, 0xf437, 0x0991, 2, 2, 107, 0},
    {0x925fe, 0xef47, 0x0aa1, 2, 2, 107, 0},
    {0x913f7, 0xfbf0, 0xf4ac, 2, 2, 107, 0},
    {0x926fa, 0x1481, 0x2170, 2, 2, 107, 0},
    {0x927b5, 0x15b4, 0x23e0, 2, 2, 107, 0},
    {0x927e9, 0x0830, 0x02bf, 2, 2, 107, 0},
    {0x92832, 0x0d33, 0x031c, 2, 2, 107, 0},
    {0x928b2, 0x0ae5, 0xfced, 2, 2, 107, 0},
    {0x928e4, 0x0adf, 0xfcf8, 2, 2, 107, 0},
    {0x928f7, 0x063d, 0x02ab, 2, 2, 107, 0},
    {0x92919, 0x06b8, 0x0046, 2, 2, 107, 0},
    {0x92924, 0x04c9, 0x003d, 2, 2, 107, 0},
    {0x92a5a, 0x022c, 0x1a22, 2, 2, 107, 0},
    {0x92a94, 0x1b8c, 0x066b, 2, 2, 107, 0},
    {0x92aa9, 0x0961, 0x1762, 2, 2, 107, 0},
    {0x92ada, 0xfb13, 0x1827, 2, 2, 107, 0},
    {0x92aee, 0x1678, 0x0372, 2, 2, 107, 0},
    {0x92b9d, 0x073d, 0x1a2c, 2, 2, 107, 0},
    {0x92bbb, 0xf87d, 0x1299, 2, 2, 107, 0},
    {0x92be6, 0xc570, 0x1073, 2, 1, 107, 0},
    {0x92bed, 0x1543, 0xfd8f, 2, 2, 107, 0},
    {0x92bf7, 0x1ec2, 0xf7af, 2, 2, 107, 0},
    {0x92c1d, 0x1a2e, 0xff16, 2, 2, 107, 0},
    {0x92c77, 0x21ca, 0xf28a, 2, 2, 107, 0},
    {0x92c80, 0x1806, 0xf60f, 2, 2, 107, 0},
    {0x92c83, 0x28e4, 0xf1aa, 1, 2, 107, 0},
    {0x92c8d, 0x1560, 0xfe9f, 2, 2, 107, 0},
    {0x92c90, 0x276e, 0xfcb8, 1, 2, 107, 0},
    {0x92d3b, 0x196a, 0xfb9d, 2, 2, 107, 0},
    {0x92d49, 0x172b, 0x0039, 2, 2, 107, 0},
    {0x92d67, 0x2773, 0xf0d3, 2, 4, 107, 0},
    {0x913f0, 0xa37d, 0xee2d, 1, 3, 107, 0},
    {0x913df, 0xcf8f, 0xcff4, 2, 4, 107, 0},
    {0x913fe, 0xe60c, 0xd967, 1, 4, 107, 0},
    {0x92da4, 0x7080, 0x04f3, 2, 3, 107, 0},
    {0x92dbb, 0x7ee4, 0xf31a, 2, 3, 107, 0},
    {0x92dd5, 0x7b01, 0x00f2, 2, 3, 107, 0},
    {0x92de5, 0x79d0, 0x050b, 2, 3, 107, 0},
    {0x92dec, 0x76b3, 0xff9d, 1, 3, 107, 0},
    {0x92e0e, 0x5fbb, 0x0555, 2, 3, 107, 0},
    {0x92e26, 0x68a3, 0xf943, 2, 3, 107, 0},
    {0x92e31, 0x85de, 0xf62a, 2, 3, 107, 0},
    {0x92e5f, 0x71bf, 0xf94b, 2, 3, 107, 0},
    {0x90ffd, 0x8556, 0xf9ec, 2, 3, 107, 0},
    {0x92e77, 0x836a, 0xf0f9, 2, 3, 107, 0},
    {0x92e8c, 0x7f6e, 0xf9f1, 2, 3, 107, 0},
    {0x92e9c, 0x77b1, 0xf364, 2, 3, 107, 0},
    {0x909c0, 0x4b21, 0xf895, 1, 3, 107, 0},
    {0x909c7, 0x44db, 0xf756, 2, 3, 107, 0},
    {0x90712, 0xd106, 0x0d16, 1, 1, 107, 0},
    {0x90bd1, 0xdb36, 0x2da2, 2, 2, 107, 0},
};

static const u8 countryOrder[] = {
    0,
    161,
    64,
    216,
    122,
    193,
    9,
    10,
    53,
    65,
    66,
    113,
    12,
    176,
    137,
    13,
    54,
    67,
    14,
    196,
    138,
    15,
    68,
    69,
    16,
    139,
    70,
    197,
    224,
    205,
    129,
    209,
    18,
    210,
    117,
    20,
    160,
    21,
    225,
    212,
    22,
    198,
    71,
    223,
    72,
    73,
    211,
    74,
    120,
    23,
    24,
    25,
    170,
    26,
    213,
    119,
    75,
    106,
    217,
    239,
    76,
    77,
    214,
    204,
    55,
    78,
    194,
    79,
    28,
    30,
    201,
    202,
    31,
    32,
    185,
    33,
    80,
    81,
    169,
    152,
    178,
    179,
    82,
    180,
    83,
    34,
    1,
    177,
    162,
    226,
    240,
    173,
    163,
    130,
    84,
    181,
    85,
    199,
    218,
    86,
    87,
    88,
    227,
    228,
    156,
    140,
    115,
    90,
    241,
    114,
    229,
    36,
    238,
    207,
    127,
    131,
    91,
    219,
    92,
    132,
    93,
    242,
    133,
    94,
    95,
    39,
    116,
    192,
    135,
    89,
    96,
    171,
    164,
    243,
    182,
    40,
    244,
    41,
    42,
    155,
    97,
    98,
    172,
    99,
    100,
    230,
    43,
    44,
    45,
    245,
    184,
    215,
    174,
    203,
    101,
    231,
    200,
    153,
    102,
    103,
    246,
    121,
    104,
    136,
    220,
    105,
    141,
    118,
    46,
    107,
    108,
    175,
    165,
    232,
    154,
    142,
    195,
    248,
    47,
    221,
    109,
    166,
    249,
    233,
    208,
    168,
    110,
    49,
    50,
    167,
    250,
    52,
    134,
    183,
    111,
    112,
};

static const u16 subregionOrder[] = {
    0,
    1,
    2,
    3,
    4,
    5,
    6,
    7,
    8,
    9,
    10,
    11,
    12,
    13,
    14,
    15,
    16,
    17,
    18,
    19,
    20,
    21,
    22,
    23,
    24,
    25,
    26,
    27,
    28,
    29,
    30,
    31,
    32,
    33,
    34,
    35,
    36,
    37,
    38,
    39,
    40,
    41,
    42,
    43,
    44,
    45,
    46,
    47,
    48,
    49,
    50,
    51,
    52,
    53,
    54,
    55,
    56,
    57,
    58,
    59,
    60,
    61,
    62,
    63,
    64,
    65,
    66,
    67,
    68,
    69,
    70,
    71,
    72,
    73,
    74,
    75,
    76,
    77,
    78,
    79,
    80,
    81,
    82,
    83,
    84,
    85,
    86,
    87,
    88,
    89,
    90,
    91,
    92,
    93,
    94,
    95,
    96,
    97,
    98,
    99,
    100,
    101,
    102,
    103,
    104,
    105,
    106,
    107,
    108,
    109,
    110,
    111,
    112,
    113,
    114,
    115,
    116,
    117,
    118,
    119,
    120,
    121,
    122,
    123,
    124,
    125,
    126,
    127,
    128,
    129,
    130,
    131,
    132,
    133,
    134,
    135,
    136,
    137,
    138,
    139,
    140,
    141,
    142,
    143,
};

const Country *GetCountry(u32 id) {
    if (id >= 1 && id <= countryCount) return &countries[id - 1];
    return nullptr;
}

u32 GetCountryChoiceCount() {
    return sizeof(countryOrder) / sizeof(countryOrder[0]);
}

u8 GetCountryAt(u32 listIndex) {
    return listIndex < GetCountryChoiceCount() ? countryOrder[listIndex] : 0;
}

u32 GetListIndex(u8 country) {
    for (u32 i = 0; i < GetCountryChoiceCount(); ++i)
        if (countryOrder[i] == country) return i;
    return 0;
}

u8 GetSelectedCountry() {
    if (!Settings::Mgr::IsCreated()) return 0;
    const u8 country = Settings::Mgr::Get().GetDisplayCountry();
    return GetListIndex(country) == 0 ? 0 : country;
}

u32 GetSubregionCount(u8 country) {
    const Country *entry = GetCountry(country);
    return entry == nullptr ? 0 : entry->subregionCount;
}

const Subregion *GetSubregionAt(u8 country, u32 index) {
    const Country *entry = GetCountry(country);
    if (entry == nullptr || index >= entry->subregionCount) return nullptr;
    return &subregions[subregionOrder[entry->subregionOffset + index]];
}

const Subregion *GetSubregion(u8 country, u8 state) {
    const Country *parent = GetCountry(country);
    if (parent == nullptr) return nullptr;
    for (u32 i = 0; i < parent->subregionCount; ++i) {
        const Subregion *entry = &subregions[parent->subregionOffset + i];
        if (entry->state == state) return entry;
    }
    return nullptr;
}

u8 GetSelectedSubregion() {
    if (!Settings::Mgr::IsCreated()) return 0;
    const u8 state = Settings::Mgr::Get().GetDisplaySubregion();
    return GetSubregion(GetSelectedCountry(), state) == nullptr ? 0 : state;
}

u8 GetWiiCountry() {
    const SystemManager *system = SystemManager::sInstance;
    if (system == nullptr) return 0;
    const u8 country = system->simpleAddr.id >> 24;
    return GetCountry(country) == nullptr ? 0 : country;
}

u8 GetEffectiveCountry() {
    const u8 selected = GetSelectedCountry();
    return selected != 0 ? selected : GetWiiCountry();
}

u8 GetLineRegion(u8 country) {
    const Country *entry = GetCountry(country);
    return entry == nullptr ? 7 : entry->lineRegion;
}

bool GetLocation(u32 &location, u16 &longitude, u16 &latitude) {
    const Country *entry = GetCountry(GetSelectedCountry());
    if (entry == nullptr) return false;
    const Subregion *subregion = GetSubregion(GetSelectedCountry(), GetSelectedSubregion());
    location = (static_cast<u32>(GetSelectedCountry()) << 24) |
               (static_cast<u32>(subregion == nullptr ? entry->state : subregion->state) << 16);
    longitude = subregion == nullptr ? entry->longitude : subregion->longitude;
    latitude = subregion == nullptr ? entry->latitude : subregion->latitude;
    return true;
}

void GetPreview(u8 country, u8 state, u16 &longitude, u16 &latitude) {
    const Country *entry = GetCountry(country);
    const Subregion *subregion = GetSubregion(country, state);
    if (entry != nullptr) {
        longitude = subregion == nullptr ? entry->longitude : subregion->longitude;
        latitude = subregion == nullptr ? entry->latitude : subregion->latitude;
    } else {
        longitude = 0;
        latitude = 0;
        if (SystemManager::sInstance != nullptr) {
            SystemManager::sInstance->GetLongitude(longitude, true);
            SystemManager::sInstance->GetLatitude(latitude, true);
        }
    }
}

// These tail-call sites service USER packets, friends, ghosts and the local
// globe. Keep SystemManager::regionId intact: the save validation and network
// matchmaking region are independent of the player's display location.
static bool ReadCountry(const SystemManager &system, u32 &dest, bool showFlag) {
    u32 location;
    u16 longitude, latitude;
    if (!GetLocation(location, longitude, latitude)) return system.GetCountry(dest, showFlag);
    dest = location;
    return true;
}
static bool ReadLongitude(const SystemManager &system, u16 &dest, bool showFlag) {
    u32 location;
    u16 longitude, latitude;
    if (!GetLocation(location, longitude, latitude)) return system.GetLongitude(dest, showFlag);
    dest = longitude;
    return true;
}
static bool ReadLatitude(const SystemManager &system, u16 &dest, bool showFlag) {
    u32 location;
    u16 longitude, latitude;
    if (!GetLocation(location, longitude, latitude)) return system.GetLatitude(dest, showFlag);
    dest = latitude;
    return true;
}
kmBranch(0x8054a9dc, ReadCountry);
kmBranch(0x8054aa04, ReadLatitude);
kmBranch(0x8054aa2c, ReadLongitude);

static void CreateSendPacket(RKNet::USERHandler &handler) {
    handler.CreateSendPacket();
    handler.toSendPacket.regionId = GetLineRegion(GetEffectiveCountry());
}
kmCall(0x806628b0, CreateSendPacket);

}  // namespace Region
}  // namespace Pulsar
