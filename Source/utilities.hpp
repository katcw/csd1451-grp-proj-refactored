/*===========================================================================
@filename   utilities.hpp
@author(s)  -
@course     CSD1451
@section    Section B

@brief		Contains helper function declarations and structs necessary for
			use later
============================================================================*/

#ifndef UTILITIES_HPP
#define UTILITIES_HPP

//============================================================================
// INCLUDES
//============================================================================
#include <iostream>
#include <fstream>
#include "entity.hpp"
#include <nlohmann/json.hpp>

// Alias for nlohmann::json
using json = nlohmann::json;

namespace BasicUtilities
{
	// Button struct to contain button data 
	struct Button
	{
		float	x, y; // Contains X and Y position values of button
		float	width, height; // Contains width and height of button
		float	normalScale; // Original scale of button
		float	hoverScale; // Hover / target scale of button
		float	currentScale; // Current scale of button (used for tracking)
		bool	isHovered; // State to check if button is being hovered on 

		bool isClicked(float mouseX, float mouseY) const
		{
			float halfWidth		= (width * currentScale) / 2.0f;
			float halfHeight	= (height * currentScale) / 2.0f;

			return (mouseX >= x - halfWidth && mouseX <= x + halfWidth &&
					mouseY >= y - halfHeight && mouseY <= y + halfHeight);
		}

		// Update function to apply hover ease effect on buttons  
		void updateHover(float mouseX, float mouseY, float deltaTime, float easeSpeed)
		{
			isHovered			=	isClicked(mouseX, mouseY);
			float targetScale	=	isHovered ? hoverScale : normalScale;
			currentScale		+=	(targetScale - currentScale) * easeSpeed * deltaTime;
		}
	};

    AEGfxTexture* loadTexture(const char* filepath);
    AEGfxVertexList* createSquareMesh(float uvHeight = 1.0f, float uvWidth = 1.0f, unsigned int color = 0xFFFFFFFF);
	void screenCoordsToWorldCoords(s32 screenX, s32 screenY, float& worldX, float& worldY);

	/******************************************************************************/
	/*
		Draws text centered on the given world-space position

		[!] worldX/Y define the CENTRE of the rendered text, that was the whole 
			point of this function
		[!] Blend mode is set in the function so don't set it before calling
	*/
	/******************************************************************************/
	void drawText(s8 fontId, const char* text, float worldX, float worldY,
	              float scale, float r, float g, float b, float alpha = 1.0f);

	/******************************************************************************/
	/*
		Draws word-wrapped text centred at (cx, topY). Lines are split at word
		boundaries so no line exceeds maxW world units. Each subsequent line is
		drawn one line-height * 1.4 below the previous.

		[!] Returns the Y position below the last rendered line so callers can
		    position additional elements (e.g. a prompt) directly below.
		[!] Blend mode is set internally — do not set it before calling.
	*/
	/******************************************************************************/
	float drawTextWrapped(s8 fontId, const char* text, float cx, float topY,
	                      float scale, float r, float g, float b, float maxW);

	/******************************************************************************/
	/*
		Draws a UI card (textured quad) at a world-space position
	*/
	/******************************************************************************/
	void drawUICard(AEGfxVertexList* mesh, AEGfxTexture* texture,
	                float worldX, float worldY,
	                float width, float height, float alpha = 1.0f);

	/******************************************************************************/
	/*
		Draws a tooltip bar in the style of the old-repo storage tooltip:
		  [ dark background ] [ icon ] [ label text ]

		cx, cy    = world-space centre of the bar
		iconTex   = icon texture; if nullptr, draws a coloured square (ir,ig,ib)
		ir,ig,ib  = fallback icon colour when iconTex == nullptr
		label     = text shown to the right of the icon
		scale     = text scale (default 0.55)
		bgAlpha   = background transparency (default 0.85)
	*/
	/******************************************************************************/
	void drawTooltip(AEGfxVertexList* mesh,
		AEGfxTexture* iconTex,
		float ir, float ig, float ib,
		const char* label,
		float cx, float cy,
		s8 fontId,
		float scale = 0.55f,
		float bgAlpha = 0.85f,
		float t = 1.f);

	// Ticks t toward 1 when active, toward 0 when not. Clamps to [0,1].
	// Call once per frame. speed=6 gives ~0.17s transition.
	inline void tickEase(float& t, bool active, float dt, float speed = 6.f)
	{
		t += active ? dt * speed : -dt * speed;
		t = t < 0.f ? 0.f : t > 1.f ? 1.f : t;
	}

	// Smoothstep — apply to t before passing to any draw function.
	inline float smoothstep(float t)
	{
		return t * t * (3.f - 2.f * t);
	}

	void Draw_UI_Element(AEGfxVertexList* mesh, AEGfxTexture* texture, AEMtx33& transform);

	// ----------------------------------------------------------------------
	//						      JSON File Loader
	// ----------------------------------------------------------------------

	/**
	 * @brief Loads and parses a JSON file from disk.
	 */
	json LoadJsonFile(const char* filename);

	//============================================================================
	// Sprite utilities
	//============================================================================
	namespace Sprite {
		/**
		 * @brief Frame data from Aseprite JSON export.
		 */
		struct JFrame {
			std::string filename{};
			float x{}, y{}, width{}, height{};
			float duration{ 200.0f };
		};

		/**
		 * @brief Animation tag defining frame range and playback direction.
		 */
		struct JAnimationTag {
			std::string name{};
			int from{ 0 };
			int to{ 3 };
			std::string direction{};
		};

		/**
		 * @brief Complete sprite metadata from Aseprite JSON export.
		 */
		struct JMeta {
			std::vector<JFrame>        frames{};
			std::vector<JAnimationTag> frameTags{};
			int                        sheetWidth{ 0 };
			int                        sheetHeight{ 0 };
			std::string                imageFile{};
		};

		/**
		 * @brief Maximum number of animations/textures per sprite.
		 *
		 * @details This constant defines the fixed size of the arrays in SpriteData.
		 */
		static constexpr u32 MAX_SPRITE_ANIMS = 12;

		/**
		 * @brief Container for Sprite data with fixed-size arrays.
		 *
		 * @details Holds arrays for JSON metadata and texture pointers, along with a count of used entries.
		 */
		struct SpriteData
		{
			JMeta jsonList[MAX_SPRITE_ANIMS];				///< Array of JSON metadata
			AEGfxTexture* textureList[MAX_SPRITE_ANIMS];	///< Array of texture pointers
			u32                          count;             ///< Number of entries used

			/**
			 * @brief Default constructor - initializes all textures to nullptr.
			 */
			SpriteData() : count(0)
			{
				for (u32 i = 0; i < MAX_SPRITE_ANIMS; ++i)
				{
					textureList[i] = nullptr;
				}
			}

			/**
			 * @brief Frees all loaded textures and resets count.
			 */
			void Free()
			{
				for (u32 i = 0; i < count; ++i)
				{
					if (textureList[i] != nullptr)
					{
						AEGfxTextureUnload(textureList[i]);
						textureList[i] = nullptr;
					}
				}
				count = 0;
			}

			/**
			 * @brief Returns true if the given index has a valid (non-null) texture loaded.
			 * @param[in] index Slot index to check (0 to MAX_SPRITE_ANIMS-1).
			 * @return true if texture pointer at index is non-null, false otherwise.
			 */
			bool HasTexture(u32 index) const
			{
				return index < MAX_SPRITE_ANIMS && textureList[index] != nullptr;
			}

			/**
			 * @brief Returns true if the given index has valid JSON metadata loaded.
			 * @param[in] index Slot index to check (0 to MAX_SPRITE_ANIMS-1).
			 * @return true if frames vector at index is non-empty, false otherwise.
			 */
			bool HasMeta(u32 index) const
			{
				return index < MAX_SPRITE_ANIMS && !jsonList[index].frames.empty();
			}
		};

		/**
		 * @brief Tracks animation frame timing and UV texture offsets.
		 *
		 * @details Used to maintain the current frame index, accumulated time for frame changes,
		 */
		struct AnimationState
		{
			float timer = 0.0f;
			u32 currentIndex = 0;
			float uvOffsetX = 0.0f;
			float uvOffsetY = 0.0f;
		};

		/**
		 * @brief Updates animation state for grid-based spritesheets.
		 *
		 * @param state AnimationState to update (timer, index, UV offsets)
		 * @param row Row index in spritesheet (0 to rows-1)
		 * @param frameDuration Seconds per frame (e.g., 0.2f = 5 FPS)
		 * @param rows Total rows in spritesheet
		 * @param cols Total columns in spritesheet
		 */
		void UpdateAnimation(AnimationState& state, u32 row, float frameDuration = 0.2f, u32 rows = 4, u32 cols = 4);

		/**
		 * @brief Updates animation state using JSON meta data (Aseprite export).
		 *
		 * @param state AnimationState to update (timer, index, UV offsets)
		 * @param meta JMeta containing frames, tags, and sheet dimensions
		 * @param tagIndex Index of animation tag to use (e.g., 0=left, 1=right)
		 * @param customDuration Override duration in seconds (-1.0f uses JSON duration)
		 */
		void UpdateAnimation(AnimationState& state,
			JMeta meta,
			u32 tagIndex = 0,
			float customDuration = -1.0f);

		/**
		 * @brief Draws an animated sprite with UV offset from AnimationState.
		 *
		 * @param mesh Pointer to vertex list mesh (must not be nullptr)
		 * @param texture Pointer to spritesheet texture (must not be nullptr)
		 * @param state AnimationState containing current UV offsets
		 * @param position World position {x, y} for sprite center
		 * @param scale Sprite size {width, height} in world units
		 */
		void DrawAnimation(AEGfxVertexList* mesh, AEGfxTexture* texture, const AnimationState& state,
			AEVec2 position = { 0.0f, 0.0f },
			AEVec2 scale = { 100.f, 100.f });

		/**
		 * @brief Draws a static sprite (no animation, UV at 0,0).
		 *
		 * @param mesh Pointer to vertex list mesh
		 * @param texture Pointer to texture to display
		 * @param position World position {x, y} for sprite center
		 * @param scale Sprite size {width, height} in world units
		 */
		void DrawSprite(AEGfxVertexList* mesh, AEGfxTexture* texture,
			AEVec2 position = { 0.0f, 0.0f },
			AEVec2 scale = { 100.f, 100.f });

		/**
		 * @brief Loads a sprite texture and its corresponding JSON
		 *        metadata into the playerSpriteData structure.
		 *
		 * @param[in] index Index in the sprite data arrays to load into (0 to MAX_SPRITE_ANIMS-1)
		 * @param[in] texturePath File path to the sprite texture (e.g., "Assets/Player/Player_Idle.png")
		 * @param[in] jsonPath File path to the sprite JSON metadata (e.g., "Assets/Player/Player_Idle.json")
		 *
		 * @return True if successfully loaded both texture and JSON, false if index out of range or load failure
		 */
		bool LoadEntry(SpriteData& list, u32 index, const char* texturePath, const char* jsonPath);

		/**
		 * @brief Loads a sprite entry with 4 animations (idle/walk x hold/no-hold) based on a base name.
		 *
		 * @param name Base name for the sprite (e.g., "MC" for "MC_Idle.png", "MC_Walk.png", etc.)
		 * @param list SpriteData structure to load the textures and metadata into
		 *
		 * @details This function constructs file paths for 4 animations based on the provided name:
		 *          - Idle:   "{name}_Idle.png" and "{name}_Idle.json"
		 *          - Walk:   "{name}_Walk.png" and "{name}_Walk.json"
		 *          - IdleHold: "{name}_IdleHold.png" and "{name}_IdleHold.json"
		 *          - WalkHold: "{name}_WalkHold.png" and "{name}_WalkHold.json"
		 *
		 * @note The function assumes a specific naming convention for the files. It will attempt to load
		 *       all 4 entries and will print debug information about success or failure of each load.
		 */
		void LoadEntry4(const char* name, SpriteData& list);

		/**
		 * @brief Selects texture based on entity movement state.
		 *
		 * @param state Current movement state of the entity
		 * @param isHolding Whether the entity is holding an item
		 * @param animArray Array of textures [0]=idle, [1]=walk, [2]=idleHold, [3]=walkHold
		 * @return Pointer to appropriate texture for the state
		 *
		 * @note Falls back to non-holding texture if holding texture is nullptr
		 */
		AEGfxTexture* GetTextureForState(Entity::MoveState state, bool isHolding, AEGfxTexture* animArray[]);

	}

	void Draw_UI_Element(AEGfxVertexList* mesh, AEGfxTexture* texture, AEMtx33& transform);
}

#endif // UTILITIES_HPP