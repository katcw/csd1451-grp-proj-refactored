/*===========================================================================
@filename   game_state_manager.cpp
@author(s)  Kalen Tan (kalentanchunwei.tan@digipen.edu.sg)
@course     CSD1451
@section    Section B

@brief      
============================================================================*/

//============================================================================
// INCLUDES
//============================================================================
#include <iostream>
#include "game_state_list.hpp"
#include "game_state_manager.hpp"
#include "splash_state.hpp"
#include "main_menu.hpp"
#include "tutorial.hpp"

int currentState{}, previousState{}, nextState{};
FP fpLoad = nullptr, fpInitialise = nullptr, fpUpdate = nullptr,
fpDraw = nullptr, fpFree = nullptr, fpUnload = nullptr;

void GSM_Initialise(int startingState)
{
	currentState = previousState = nextState = startingState;
}

void GSM_Update()
{
	switch (currentState)
	{
	case GS_SPLASH:
		fpLoad = Splash_Load;
		fpInitialise = Splash_Initialise;
		fpUpdate = Splash_Update;
		fpDraw = Splash_Draw;
		fpFree = Splash_Free;
		fpUnload = Splash_Unload;
		break;
	case GS_MAINMENU:
		fpLoad = MainMenu_Load;
		fpInitialise = MainMenu_Initialise;
		fpUpdate = MainMenu_Update;
		fpDraw = MainMenu_Draw;
		fpFree = MainMenu_Free;
		fpUnload = MainMenu_Unload;
		break;
	// [!] No prototype for now, unused code from previous version so.. idk
	//	   what it's still doing here but ok 
	//case GS_PROTOTYPE:
	//	fpLoad = Prototype_Load;
	//	fpInitialise = Prototype_Initialise;
	//	fpUpdate = Prototype_Update;
	//	fpDraw = Prototype_Draw;
	//	fpFree = Prototype_Free;
	//	fpUnload = Prototype_Unload;

	case GS_TUTORIAL:
		fpLoad       = Tutorial_Load;
		fpInitialise = Tutorial_Initialise;
		fpUpdate     = Tutorial_Update;
		fpDraw       = Tutorial_Draw;
		fpFree       = Tutorial_Free;
		fpUnload     = Tutorial_Unload;
		break;
	case GS_CREDITS:
		break;
	case GS_EXIT:
		break;
	case GS_PLAY:
		break;
	}
}
