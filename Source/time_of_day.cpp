/*===========================================================================
@filename   time_of_day.cpp
@brief      See time_of_day.hpp. Ported from the original repo's
            time_of_day.cpp; wrapped in a namespace and extended with
            background colour interpolation and a clock HUD draw.
============================================================================*/
#include "time_of_day.hpp"
#include "utilities.hpp"
#include <cstdio>

namespace TimeOfDay
{
    //========================================================================
    // SETTINGS
    //========================================================================
    constexpr float REAL_DURATION_SEC = 10.0f;        // 5 real minutes = full day
    constexpr float START_TIME_SEC    = 7.f  * 3600.f; // 07:00
    constexpr float END_TIME_SEC      = 18.f * 3600.f; // 18:00
    constexpr float SIM_DURATION_SEC  = END_TIME_SEC - START_TIME_SEC;
    constexpr float SPEED             = SIM_DURATION_SEC / REAL_DURATION_SEC;

    // Clock HUD position (screen-space; 0,0 = centre, ±800 X, ±450 Y)
    constexpr float HUD_X = 620.f;
    constexpr float HUD_Y = 390.f;

    // Background colour at t=0 (07:00 dawn) linearly fades to t=1 (18:00 night)
    // dawn  → (0.85, 0.84, 0.80)  beige
    // night → (0.30, 0.30, 0.40)  dark blue-grey

    //========================================================================
    // STATE
    //========================================================================
    static float gSimTimeSec = START_TIME_SEC;
    static bool  gRunning    = true;

    //========================================================================
    // LIFECYCLE
    //========================================================================
    void Init()
    {
        gSimTimeSec = START_TIME_SEC;
        gRunning    = true;
    }

    void Update(float dt)
    {
        if (!gRunning) return;

        gSimTimeSec += dt * SPEED;
        if (gSimTimeSec >= END_TIME_SEC)
        {
            gSimTimeSec = END_TIME_SEC;
            gRunning    = false;
        }
    }

    //========================================================================
    // HUD
    //========================================================================
    void Draw(s8 fontId)
    {
        char buf[8];
        GetClockString(buf, sizeof(buf));
        // White text; alpha 1 — screen-space so camera has no effect
        BasicUtilities::drawText(fontId, buf, HUD_X, HUD_Y, 1.f, 1.f, 1.f, 1.f);
    }

    //========================================================================
    // QUERIES
    //========================================================================
    bool IsRunning() { return gRunning; }
    bool HasEnded()  { return !gRunning; }

    int GetHours()   { return static_cast<int>(gSimTimeSec) / 3600; }
    int GetMinutes() { return (static_cast<int>(gSimTimeSec) / 60) % 60; }

    void GetClockString(char* buf, int bufSize)
    {
        if (!buf || bufSize <= 0) return;
        sprintf_s(buf, bufSize, "%02d:%02d", GetHours(), GetMinutes());
    }

    void GetBackgroundColor(float& r, float& g, float& b)
    {
        float t = (gSimTimeSec - START_TIME_SEC) / SIM_DURATION_SEC; // [0, 1]
        r = 0.85f - (0.55f * t);
        g = 0.84f - (0.54f * t);
        b = 0.80f - (0.40f * t);
    }
}
