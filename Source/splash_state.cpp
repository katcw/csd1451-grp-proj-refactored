/*===========================================================================
@filename   game_state_manager.cpp
@author(s)  Kalen Tan (kalenchunwei.tan@digipen.edu.sg)
@course     CSD1451
@section    Section B

@brief
============================================================================*/

//============================================================================
// INCLUDES
//============================================================================
#include <iostream>
#include "AEEngine.h"
#include "AEGraphics.h"
#include "AEMtx33.h"
#include "splash_state.hpp"
#include "game_state_manager.hpp"
#include "game_state_list.hpp"
#include "utilities.hpp"

namespace
{
    AEGfxTexture* logoTexture = nullptr;
    AEGfxVertexList* logoMesh = nullptr;

    AEMtx33 logoScale = { 0 };

    enum SplashPhase { 
        FADE_IN, 
        HOLD, FADE_OUT, 
        COMPLETE 
    };

    SplashPhase currentPhase = FADE_IN;
    float phaseTimer = 0.0f;
    float alpha = 0.0f;

    const float FADE_IN_DURATION = 1.0f;
    const float HOLD_DURATION = 2.0f;
    const float FADE_OUT_DURATION = 1.0f;

    const float LOGO_WIDTH = 800.0f;
    const float LOGO_HEIGHT = 250.0f;
}

void Splash_Load()
{
    logoTexture = BasicUtilities::loadTexture("Assets/digipen_logo.png");
}

void Splash_Initialise()
{
    logoMesh = BasicUtilities::createSquareMesh();
    AEMtx33Scale(&logoScale, LOGO_WIDTH, LOGO_HEIGHT);

    currentPhase = FADE_IN;
    phaseTimer = 0.0f;
    alpha = 0.0f;
}

void Splash_Update()
{
    float deltaTime = static_cast<float>(AEFrameRateControllerGetFrameTime());
    phaseTimer += deltaTime;

    if (AEInputCheckTriggered(AEVK_SPACE)) nextState = GS_MAINMENU;

    switch (currentPhase)
    {
    case FADE_IN:
        alpha = phaseTimer / FADE_IN_DURATION;
        if (phaseTimer >= FADE_IN_DURATION)
        {
            alpha = 1.0f;
            currentPhase = HOLD;
            phaseTimer = 0.0f;
        }
        break;

    case HOLD:
        alpha = 1.0f;
        if (phaseTimer >= HOLD_DURATION)
        {
            currentPhase = FADE_OUT;
            phaseTimer = 0.0f;
        }
        break;

    case FADE_OUT:
        alpha = 1.0f - (phaseTimer / FADE_OUT_DURATION);
        if (phaseTimer >= FADE_OUT_DURATION)
        {
            alpha = 0.0f;
            currentPhase = COMPLETE;
            nextState = GS_MAINMENU;
        }
        break;

    case COMPLETE:
        nextState = GS_MAINMENU;
        break;
    }
}

void Splash_Draw()
{
    AEGfxSetBackgroundColor(0.85f, 0.84f, 0.80f);
    AEGfxSetRenderMode(AE_GFX_RM_TEXTURE);

    AEGfxSetColorToMultiply(1.0f, 1.0f, 1.0f, 1.0f);
    AEGfxSetColorToAdd(0.0f, 0.0f, 0.0f, 0.0f);
    AEGfxSetBlendMode(AE_GFX_BM_BLEND);
    AEGfxSetTransparency(alpha);
    AEGfxTextureSet(logoTexture, 0, 0);
    AEGfxSetTransform(logoScale.m);
    AEGfxMeshDraw(logoMesh, AE_GFX_MDM_TRIANGLES);
}

void Splash_Free()
{
    AEGfxMeshFree(logoMesh);
}

void Splash_Unload()
{
    AEGfxTextureUnload(logoTexture);
}
