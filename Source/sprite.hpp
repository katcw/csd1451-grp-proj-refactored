/*===========================================================================
@filename   sprite.hpp
@author(s)  -
@course     CSD1451
@section    Section B

@brief      Self-contained sprite animation module using Aseprite JSON exports.
            Provides JSON data structures, a sprite-data container, per-entity
            animation state, and functions for loading, updating, and drawing
            animated sprites.
============================================================================*/

//============================================================================
// HEADER GUARD
//============================================================================
#ifndef SPRITE_HPP
#define SPRITE_HPP

//============================================================================
// INCLUDES
//============================================================================
#include <vector>
#include <string>
#include "AEEngine.h"
#include "nlohmann/json.hpp"

//============================================================================
// NAMESPACE
//============================================================================
namespace Sprite
{
	//========================================================================
	// JSON DATA STRUCTURES (Aseprite export format)
	//========================================================================

	/******************************************************************************/
	/*
		Data for a single animation frame from Aseprite JSON export,
		x/y/width/height are pixel coordinates within the spritesheet
		
		[!] Duration is in milliseconds
	*/
	/******************************************************************************/
	struct JFrame
	{
		std::string filename;
		float x        = 0.f;
		float y        = 0.f;
		float width    = 0.f;
		float height   = 0.f;
		float duration = 200.f; // !!! milliseconds
	};

	/******************************************************************************/
	/*
		Named animation tag from an Aseprite JSON export
		
		[?] from/to are inclusive global frame indices into the frames array
	*/
	/******************************************************************************/
	struct JAnimationTag
	{
		std::string name;
		int         from      = 0;
		int         to        = 0;
		std::string direction;
	};

	/******************************************************************************/
	/*
		Complete sprite metadata parsed from an Aseprite JSON export

		[?] frames: all frame data in global order.
		[?] frameTags: named animation clips (e.g. "Left", "Right", "Up", "Back")
		[?] sheetWidth/sheetHeight: pixel dimensions of the full spritesheet
	*/
	/******************************************************************************/
	struct JMeta
	{
		std::vector<JFrame>        frames;
		std::vector<JAnimationTag> frameTags;
		int sheetWidth  = 0;
		int sheetHeight = 0;
	};

	//========================================================================
	// SPRITE DATA CONTAINER
	//========================================================================

	static constexpr unsigned int MAX_ANIM_SLOTS = 4;

	/******************************************************************************/
	/*
		Holds up to MAX_ANIM_SLOTS texture + metadata pairs for a single character
		Typical layout:
			[0] Idle 
			[1] Walking
			[2] IdleCarry
			[3] WalkCarry
	*/
	/******************************************************************************/
	struct SpriteData
	{
		AEGfxTexture* textures[MAX_ANIM_SLOTS] = {};
		JMeta         metas[MAX_ANIM_SLOTS]    = {};
		unsigned int  count                    = 0;

		/******************************************************************************/
		/*
			Unloads all textures and resets container
		*/
		/******************************************************************************/
		void Free();
	};

	//========================================================================
	// PER-ENTITY ANIMATION STATE
	//========================================================================

	/******************************************************************************/
	/*
		Tracks the current animation frame, accumulated time, and UV offsets
		for a single entity's active animation clip
	*/
	/******************************************************************************/
	struct AnimationState
	{
		float        timer        = 0.f;
		unsigned int currentIndex = 0;
		float        uvOffsetX    = 0.f;
		float        uvOffsetY    = 0.f;
	};

	//========================================================================
	// FUNCTION DECLARATIONS
	//========================================================================

	/******************************************************************************/
	/*
		Opens and parses a JSON file from disk using nlohmann
	*/
	/******************************************************************************/
	nlohmann::json LoadJsonFile(const char* filepath);

	/******************************************************************************/
	/*
		Parses an Aseprite JSON export object into a JMeta struct
	*/
	/******************************************************************************/
	JMeta LoadMeta(const nlohmann::json& j);

	/******************************************************************************/
	/*
		Loads a texture and its Aseprite JSON metadata into a specific slot of
		the given SpriteData
	*/
	/******************************************************************************/
	bool LoadEntry(SpriteData& data, unsigned int slot,
	               const char* texPath, const char* jsonPath);

	/******************************************************************************/
	/*
		Advances the animation state using per-frame durations from the JSON meta
		tagIndex selects the animation clip (e.g. 0=Left, 1=Right, 2=Up, 3=Back)
		Loops within the tag's from/to range
	*/
	/******************************************************************************/
	void UpdateAnimation(AnimationState& state, const JMeta& meta,
	                     unsigned int tagIndex = 0);

	/******************************************************************************/
	/*
		Draws an animated sprite at the given world-space center position and scale,
		the mesh must have UV coordinates matching one frame of the spritesheet
	*/
	/******************************************************************************/
	void DrawAnimation(AEGfxVertexList* mesh, AEGfxTexture* texture,
	                   const AnimationState& state,
	                   AEVec2 position, AEVec2 scale);

} 

#endif 
