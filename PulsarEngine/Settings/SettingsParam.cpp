#include <kamek.hpp>
#include <PulsarSystem.hpp>
#include <Config.hpp>
#include <Settings/SettingsParam.hpp>

namespace Pulsar {

namespace Settings {

u8 Params::radioCount[Params::pageCount] ={
    4, 6, 3, 5, 2, 8, 6, 0 //menu, race, host, OTT, KO, RR1, RRHost, RRLanguage
    //Add user radio count here

};
u8 Params::scrollerCount[Params::pageCount] ={ 1, 1, 1, 0, 2, 0, 2, 1 }; //menu, race, host, OTT, KO, RR1, RRHost, RRLanguage

u8 Params::buttonsPerPagePerRow[Params::pageCount][Params::maxRadioCount] = //first row is PulsarSettingsType, 2nd is rowIdx of radio
{
    { 2, 2, 3, 2, 0, 0, 0, 0 }, //Menu 
    { 2, 2, 2, 2, 3, 2, 0, 0 }, //Race
    { 2, 4, 2, 3, 0, 0, 0, 0 }, //Host
    { 2, 2, 2, 2, 2, 0, 0, 0 }, //OTT
    { 2, 2, 0, 0, 0, 0, 0, 0 }, //KO
    { 4, 2, 2, 3, 2, 2, 2, 2 }, //RR1
    { 3, 4, 2, 2, 2, 4, 0, 0 }, //RRHost
    { 0, 0, 0, 0, 0, 0, 0, 0 }, //RRLanguage
};

u8 Params::optionsPerPagePerScroller[Params::pageCount][Params::maxScrollerCount] =
{
    { 5, 0, 0, 0, 0}, //Menu 
    { 4, 0, 0, 0, 0}, //Race
    { 7, 0, 0, 0, 0}, //Host
    { 0, 0, 0, 0, 0}, //OTT
    { 4, 4, 0, 0, 0}, //KO
    { 3, 0, 0, 0, 0}, //RR1
    { 5, 4, 0, 0, 0}, //RRHost
    { 13, 0, 0, 0, 0}, //RRLanguage
};

}//namespace Settings
}//namespace Pulsar