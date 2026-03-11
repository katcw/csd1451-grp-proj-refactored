/*===========================================================================
@filename   utilities.hpp
@author(s)  -
@course     CSD1451
@section    Section B

@brief		Contains helper function declarations and structs necessary for
			use later
============================================================================*/

//============================================================================
// INCLUDES
//============================================================================
#include <iostream>

namespace BasicUtilities
{
	//============================================================================
	// STRUCTS
	//============================================================================
	struct Button
	{
		float x, y;
		float width, height;
		float normalScale, hoverScale, currentScale;
		bool isHovered;

		/******************************************************************************/
		/*
			Checks if the given mouse position is inside the button's bounds.
			Takes into account the button's current scale for accurate detection
			Returns true if the mouse is hovering over the button, false otherwise
		*/
		/******************************************************************************/
		bool isClicked(float mouseX, float mouseY)
		{
			float halfWidth = (width * currentScale) / 2.0f;
			float halfHeight = (height * currentScale) / 2.0f;

			return (mouseX >= x - halfWidth && mouseX <= x + halfWidth &&
				mouseY >= y - halfHeight && mouseY <= y + halfHeight);
		}

		/******************************************************************************/
		/*
			Updates the button's scale based on whether the mouse is hovering over it,
			and smoothly transitions between normal and hover scale with easing
		*/
		/******************************************************************************/
		void updateHover(float mouseX, float mouseY, float deltaTime, float easeSpeed)
		{
			isHovered = isClicked(mouseX, mouseY);
			float targetScale = isHovered ? hoverScale : normalScale;
			currentScale += (targetScale - currentScale) * easeSpeed * deltaTime;
		}
	};

	//============================================================================
	// FUNCTION DECLARATIONS
	//============================================================================
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
		float scale  = 0.55f,
		float bgAlpha = 0.85f,
		float t      = 1.f);   

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
}
