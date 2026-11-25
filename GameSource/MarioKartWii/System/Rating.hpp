#ifndef _RATING_
#define _RATING_
#include <kamek.hpp>

class Rating {
public:
    Rating();  // 805444d0
    virtual ~Rating();  // 8052da10 vtable 808b32a0
    void AddPoints(s32 points);  // 8052d270
    static float CalcPoints(s32 difference, bool won);  // 8052d068
    static float EvaluateCurve(float sample);  // 8052cef8

    u32 points;
};  // 0x8
#endif