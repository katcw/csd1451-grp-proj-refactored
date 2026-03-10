/*===========================================================================
@filename   utilities.cpp
@author(s)  -
@course     CSD1451
@section    Section B

@brief      Contains helper function definitions
============================================================================*/

//============================================================================
// INCLUDES
//============================================================================
#include <iostream>
#include <cstdio>
#include <cstring>
#include "AEEngine.h"
#include "AEGraphics.h"
#include "AEMtx33.h"

namespace BasicUtilities
{
    AEGfxTexture* loadTexture(const char* filepath)
    {
        AEGfxTexture* returnedTexture = AEGfxTextureLoad(filepath);

        if (returnedTexture == nullptr)
        {
            std::cout << "ERROR! COULD NOT LOCATE " << filepath << '\n';
        }
        else
        {
            std::cout << "SUCCESS! " << filepath << " LOADED" << '\n';
        }

        return returnedTexture;
    }

    AEGfxVertexList* createSquareMesh(float uvHeight, float uvWidth, unsigned int color)
    {
        AEGfxMeshStart();
        AEGfxTriAdd(
            -0.5f, -0.5f, color, 0.0f, uvHeight,
            0.5f, -0.5f, color, uvWidth, uvHeight,
            -0.5f, 0.5f, color, 0.0f, 0.0f);
        AEGfxTriAdd(
            0.5f, -0.5f, color, uvWidth, uvHeight,
            0.5f, 0.5f, color, uvWidth, 0.0f,
            -0.5f, 0.5f, color, 0.0f, 0.0f);

        return AEGfxMeshEnd();
    }

    void screenCoordsToWorldCoords(s32 screenX, s32 screenY, float& worldX, float& worldY)
    {
        worldX = screenX - (AEGfxGetWindowWidth() / 2.0f);
        worldY = (AEGfxGetWindowHeight() / 2.0f) - screenY;
    }

    void drawText(s8 fontId, const char* text, float worldX, float worldY,
                  float scale, float r, float g, float b, float alpha)
    {
        // Convert world coordinates to normalised
        float normalX = worldX / (AEGfxGetWindowWidth()  / 2.0f);
        float normalY = worldY / (AEGfxGetWindowHeight() / 2.0f);

        // Measure the rendered text 
        // [!] Necessary to shift text to centre
        float textWidth = 0.f, textHeight = 0.f;
        AEGfxGetPrintSize(fontId, text, scale, &textWidth, &textHeight);

        // [!] AEGfxPrint anchors at the bottom-left; shift left and down by half extents to centre
        normalX -= textWidth  / 2.0f;
        normalY -= textHeight / 2.0f;

        AEGfxSetBlendMode(AE_GFX_BM_BLEND);
        AEGfxPrint(fontId, text, normalX, normalY, scale, r, g, b, alpha);
    }

    float drawTextWrapped(s8 fontId, const char* text, float cx, float topY,
                          float scale, float r, float g, float b, float maxW)
    {
        float dummy, lineH;
        AEGfxGetPrintSize(fontId, "A", scale, &dummy, &lineH);
        const float lineGap = lineH * (AEGfxGetWindowHeight() / 2.0f) * 1.4f;
        const float normalMaxW = maxW / (AEGfxGetWindowWidth() / 2.0f);

        char currentLine[256] = {};
        char testLine[256];
        float y = topY;

        const char* p = text;
        while (*p)
        {
            while (*p == ' ') ++p;                          // skip spaces
            if (!*p) break;

            const char* wordEnd = p;
            while (*wordEnd && *wordEnd != ' ') ++wordEnd;  // find word boundary

            if (currentLine[0])
                sprintf_s(testLine, sizeof(testLine), "%s %.*s",
                          currentLine, static_cast<int>(wordEnd - p), p);
            else
                sprintf_s(testLine, sizeof(testLine), "%.*s",
                          static_cast<int>(wordEnd - p), p);

            float w, h;
            AEGfxGetPrintSize(fontId, testLine, scale, &w, &h);

            if (w > normalMaxW && currentLine[0])  // overflow: flush current line, restart
            {
                drawText(fontId, currentLine, cx, y, scale, r, g, b, 1.0f);
                y -= lineGap;
                sprintf_s(currentLine, sizeof(currentLine), "%.*s",
                          static_cast<int>(wordEnd - p), p);
            }
            else
            {
                strcpy_s(currentLine, sizeof(currentLine), testLine);
            }
            p = wordEnd;
        }
        if (currentLine[0])
        {
            drawText(fontId, currentLine, cx, y, scale, r, g, b, 1.0f);
            y -= lineGap;
        }
        return y;
    }

    void drawUICard(AEGfxVertexList* mesh, AEGfxTexture* texture,
                    float worldX, float worldY,
                    float width, float height, float alpha)
    {
        AEMtx33 translate = { 0 }, scale = { 0 }, transform = { 0 };
        AEMtx33Trans(&translate, worldX, worldY);
        AEMtx33Scale(&scale, width, height);
        AEMtx33Concat(&transform, &translate, &scale);

        AEGfxSetRenderMode(AE_GFX_RM_TEXTURE);
        AEGfxSetColorToMultiply(1.0f, 1.0f, 1.0f, 1.0f);
        AEGfxSetColorToAdd(0.0f, 0.0f, 0.0f, 0.0f);
        AEGfxSetBlendMode(AE_GFX_BM_BLEND);
        AEGfxSetTransparency(alpha);
        AEGfxTextureSet(texture, 0, 0);
        AEGfxSetTransform(transform.m);
        AEGfxMeshDraw(mesh, AE_GFX_MDM_TRIANGLES);
    }
}