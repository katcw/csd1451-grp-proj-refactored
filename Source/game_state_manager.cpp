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
#include "level1.hpp"
#include "level2.hpp"
#include "rune_minigame.hpp"
#include "merchant.hpp"
//============================================================================
// GLOBAL STATE
//============================================================================
int currentState{}, previousState{}, nextState{};

FP fpLoad = nullptr;
FP fpInitialise = nullptr;
FP fpUpdate = nullptr;
FP fpDraw = nullptr;
FP fpFree = nullptr;
FP fpUnload = nullptr;

// Shared run-wide merchant bonuses
RunBonuses gRunBonuses{};

//============================================================================
// GAME STATE MANAGER
//============================================================================
void GSM_Initialise(int startingState)
{
	currentState = previousState = nextState = startingState;

	// Fresh run start
	gRunBonuses = RunBonuses{};
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
		// case GS_PROTOTYPE:
		//     fpLoad = Prototype_Load;
		//     fpInitialise = Prototype_Initialise;
		//     fpUpdate = Prototype_Update;
		//     fpDraw = Prototype_Draw;
		//     fpFree = Prototype_Free;
		//     fpUnload = Prototype_Unload;
		//     break;

	case GS_TUTORIAL:
		fpLoad = Tutorial_Load;
		fpInitialise = Tutorial_Initialise;
		fpUpdate = Tutorial_Update;
		fpDraw = Tutorial_Draw;
		fpFree = Tutorial_Free;
		fpUnload = Tutorial_Unload;
		break;

	case GS_LEVEL1:
		fpLoad = Level1_Load;
		fpInitialise = Level1_Initialise;
		fpUpdate = Level1_Update;
		fpDraw = Level1_Draw;
		fpFree = Level1_Free;
		fpUnload = Level1_Unload;
		break;

	case GS_LEVEL2:
		fpLoad = Level2_Load;
		fpInitialise = Level2_Initialise;
		fpUpdate = Level2_Update;
		fpDraw = Level2_Draw;
		fpFree = Level2_Free;
		fpUnload = Level2_Unload;
		break;

	case GS_CREDITS:
		break;

	case GS_EXIT:
		break;

	case GS_RUNE_TEST:
		fpLoad = RuneMinigame::Load;
		fpInitialise = RuneMinigame::Init;
		fpUpdate = RuneMinigame::Update;
		fpDraw = RuneMinigame::Draw;
		fpFree = RuneMinigame::Free;
		fpUnload = RuneMinigame::Unload;
		break;

	case GS_PLAY:
		break;
	}
}