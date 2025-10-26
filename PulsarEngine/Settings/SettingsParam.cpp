#include <kamek.hpp>
#include <PulsarSystem.hpp>
#include <Config.hpp>
#include <Settings/SettingsParam.hpp>

namespace Pulsar {

namespace Settings {

u8 Params::radioCount[Params::pageCount] = {
    6, 5, 2, 2, 3, 2, 3, 5, 5, 2, 2, 0
    // Add user radio count here

};
u8 Params::scrollerCount[Params::pageCount] = {0, 0, 1, 0, 0, 0, 3, 0, 0, 2, 0, 2};

u8 Params::buttonsPerPagePerRow[Params::pageCount][Params::maxRadioCount] =  // first row is PulsarSettingsType, 2nd is rowIdx of radio
    {
        {4, 2, 2, 2, 2, 4, 0, 0},
        {2, 2, 2, 3, 2, 0, 0, 0},
        {2, 2, 0, 0, 0, 0, 0, 0},
        {2, 2, 0, 0, 0, 0, 0, 0},
        {2, 3, 2, 0, 0, 0, 0, 0},
        {2, 2, 0, 0, 0, 0, 0, 0},
        {4, 3, 4, 0, 0, 0, 0, 0},
        {2, 2, 2, 2, 3, 0, 0, 0},
        {2, 2, 2, 2, 2, 0, 0, 0},
        {3, 2, 0, 0, 0, 0, 0, 0},
        {2, 2, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 0},
};

u8 Params::optionsPerPagePerScroller[Params::pageCount][Params::maxScrollerCount] =
    {
        {0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0},
        {5, 0, 0, 0, 0},
        {0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0},
        {7, 5, 4, 0, 0},
        {0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0},
        {4, 4, 0, 0, 0},
        {0, 0, 0, 0, 0},
        {2, 13, 0, 0, 0},
};

}  // namespace Settings
}  // namespace Pulsar