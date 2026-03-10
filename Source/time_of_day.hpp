/*===========================================================================
@filename   time_of_day.hpp
@brief      Generic time-of-day system. Simulates an in-game day (07:00–18:00)
            in real time. Provides background colour interpolation across three
            keyframes and a screen-space clock HUD. Drop these calls into any
            game state:
                TimeOfDay::Init()                       — on state initialise
                TimeOfDay::Update(dt)                   — every frame
                TimeOfDay::GetBackgroundColor(r, g, b)  — before AEGfxSetBackgroundColor
                TimeOfDay::Draw(fontId)                 — after world draw (HUD)
============================================================================*/
#pragma once
#include "AEEngine.h"

namespace TimeOfDay
{
    // Lifecycle
    void Init();
    void Update(float dt);

    // HUD: renders clock string in screen-space (unaffected by camera)
    void Draw(s8 fontId);

    // Queries
    bool IsRunning();
    bool HasEnded();
    int  GetHours();
    int  GetMinutes();
    void GetClockString(char* buf, int bufSize);

    // Returns interpolated background colour for the current time of day
    void GetBackgroundColor(float& r, float& g, float& b);
}
