/*===========================================================================
@filename   game_state_list.hpp
@author(s)  Kalen Tan (kalenchunwei.tan@digipen.edu.sg)
@course     CSD1451
@section    Section B

@brief      Contains enumerators for all states in the game
============================================================================*/
#pragma once

#include <iostream>

enum gameState
{
	GS_SPLASH = 0,
	GS_MAINMENU,
	GS_CREDITS,
	GS_EXIT,
	GS_PLAY,
	GS_TUTORIAL,
	GS_RESTART
};
