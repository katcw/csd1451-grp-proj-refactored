/*===========================================================================
@filename   main.cpp
@author(s)  - 
@course     CSD1451
@section    Section B

@brief      Main application entry point for initializing AEEngine,
            and running the game state manager loop 
============================================================================*/

//============================================================================
// INCLUDES
//============================================================================
#include <crtdbg.h>
#include "AEEngine.h"
#include "game_state_manager.hpp"
//============================================================================

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR    lpCmdLine,
    _In_ int       nCmdShow)
{
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);

    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    AESysInit(hInstance, nCmdShow, 1600, 900, 1, 60, false, NULL);
    AESysSetWindowTitle("My Game");
    AESysReset();

    GSM_Initialise(GS_SPLASH);

    while (currentState != GS_EXIT)
    {
        if (currentState == GS_RESTART)
        {
            currentState = previousState;
            nextState = previousState;
        }
        else
        {
            GSM_Update();
            fpLoad();
        }

        fpInitialise();

        while (currentState == nextState)
        {
            AESysFrameStart();

            float dt = (float)AEFrameRateControllerGetFrameTime();
            /*TimeOfDayUpdate(dt);*/
            UNREFERENCED_PARAMETER(dt);

            fpUpdate();
            fpDraw();

            AESysFrameEnd();
            if (AESysDoesWindowExist() == false)
                nextState = GS_EXIT;
        }

        fpFree();

        if (nextState != GS_RESTART)
        {
            fpUnload();
        }
        previousState = currentState;
        currentState = nextState;
    }

    AESysExit();
    return 0;
}

