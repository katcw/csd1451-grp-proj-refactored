/*===========================================================================
@filename   particle_system.cpp
@brief      See particle_system.hpp.
============================================================================*/
#include "particle_system.hpp"
#include "utilities.hpp"
#include "AEGraphics.h"
#include "AEMtx33.h"
#include <cstdlib>  // rand()
#include <cmath>

namespace ParticleSystem
{
    static const float GRAVITY = 80.f; // downward acceleration (world units/s²)

    //========================================================================
    // LIFECYCLE
    //========================================================================

    void Load(State& s)
    {
        // White mesh — colour is applied per-particle via SetColorToMultiply
        s.mesh = BasicUtilities::createSquareMesh(1.f, 1.f, 0xFFFFFFFF);
    }

    void Update(State& s, float dt)
    {
        for (int i = 0; i < MAX_PARTICLES; ++i)
        {
            if (!s.pool[i].active) continue;
            Particle& p = s.pool[i];

            p.pos.x   += p.vel.x * dt;
            p.pos.y   += p.vel.y * dt;
            p.vel.y   -= GRAVITY  * dt; // arc under gravity

            p.lifetime -= dt;
            if (p.lifetime <= 0.f) p.active = false;
        }
    }

    void Draw(State const& s)
    {
        if (!s.mesh) return;

        AEGfxSetRenderMode(AE_GFX_RM_COLOR);
        AEGfxSetColorToAdd(0.f, 0.f, 0.f, 0.f);
        AEGfxSetBlendMode(AE_GFX_BM_BLEND);

        AEMtx33 sclMtx, trsMtx, mtx;

        for (int i = 0; i < MAX_PARTICLES; ++i)
        {
            if (!s.pool[i].active) continue;
            const Particle& p = s.pool[i];

            float fade = p.lifetime / p.maxLifetime;
            AEGfxSetColorToMultiply(p.r, p.g, p.b, fade);
            AEGfxSetTransparency(fade);

            AEMtx33Scale(&sclMtx, p.size, p.size);
            AEMtx33Trans(&trsMtx, p.pos.x, p.pos.y);
            AEMtx33Concat(&mtx, &trsMtx, &sclMtx);
            AEGfxSetTransform(mtx.m);
            AEGfxMeshDraw(s.mesh, AE_GFX_MDM_TRIANGLES);
        }
    }

    void Free(State& s)
    {
        if (s.mesh) { AEGfxMeshFree(s.mesh); s.mesh = nullptr; }
        for (int i = 0; i < MAX_PARTICLES; ++i) s.pool[i] = Particle{};
        s.emitTimer = 0.f;
    }

    //========================================================================
    // EMITTERS
    //========================================================================

    void EmitWater(State& s, AEVec2 pos)
    {
        const int BURST = 3;
        int spawned = 0;

        for (int i = 0; i < MAX_PARTICLES && spawned < BURST; ++i)
        {
            if (s.pool[i].active) continue;

            Particle& p = s.pool[i];
            p.active = true;

            // Emit from pot centre
            p.pos.x = pos.x;
            p.pos.y = pos.y;

            // Random direction radial spray; gravity curves them downward in Update
            float angle = (float)(rand() % 360) * (3.14159f / 180.f);
            float speed = (float)(rand() % 40 + 20);   // 20–60 units/s
            p.vel.x = cosf(angle) * speed;
            p.vel.y = sinf(angle) * speed;

            p.lifetime    = 0.30f + (rand() % 20) * 0.01f;  // 0.30–0.50 s
            p.maxLifetime = p.lifetime;
            p.size        = 3.f + (float)(rand() % 4);        // 3–6 units
            p.r           = 0.3f;
            p.g           = 0.7f;
            p.b           = 1.f;

            ++spawned;
        }
    }

} // namespace ParticleSystem
