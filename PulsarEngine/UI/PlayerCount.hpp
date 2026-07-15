#ifndef __PLAYER_COUNT_HPP
#define __PLAYER_COUNT_HPP

#include <PulsarSystem.hpp>
#include <Settings/UI/ExpWFCMainPage.hpp>
#include <MarioKartWii/UI/Page/Other/Globe.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <MarioKartWii/UI/Section/SectionMgr.hpp>
#include <MarioKartWii/UI/Page/Other/WFCMenu.hpp>
#include <core/rvl/DWC/DWCMatch.hpp>
#include <core/GS/GSAvailable.hpp>
#include <MarioKartWii/RKSYS/RKSYSMgr.hpp>
#include <Dolphin/DolphinIOS.hpp>

namespace PlayerCount {

void GetNumbersMain(int& nRetro, int& nCT, int& nRT);
void GetNumbersOther(int& n200, int& nOtt, int& nIR);
void GetNumbersBT(int& nBattle, int& nBattleELIM);
void GetNumbersRegular(int& nRegular);
void GetNumbersTotal(int& nTotal);
void GetNumbersOthers(int& nOthers);

}  // namespace PlayerCount

#endif  // __PLAYER_COUNT_HPP