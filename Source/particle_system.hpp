/*===========================================================================
@filename   particle_system.hpp
@author(s)  -
@course     CSD1451
@section    Section B

@brief      Each Particle is one object instance (active flag = soft deletion). 
            Callers emit particles via typed helpers (e.g. EmitWater). The 
            system owns no textures — it uses a coloured mesh baked at load 
            time.
============================================================================*/
#pragma once

#include "AEEngine.h"

namespace ParticleSystem
{
    constexpr int MAX_PARTICLES                 = 256; // Max particle instances that can be present at once

    // One game-object instance
    struct Particle
    {
        bool   active                           = false;
        AEVec2 pos                              = {};
        AEVec2 vel                              = {};
        float  lifetime                         = 0.0f;
        float  maxLifetime                      = 1.0f;
        float  size                             = 6.0f;
        float  r                                = 1.0f;
        float  g                                = 1.0f;
        float  b                                = 1.0f; // Tint colour
    };

    struct State
    {
        Particle         pool[MAX_PARTICLES]    = {};
        AEGfxVertexList* mesh                   = nullptr; // Particle mesh
        float            emitTimer              = 0.f; // Rate limit for emitter
    };

    
    void Load  (State& s); // Create particle mesh
    void Update(State& s, float dt);
    void Draw  (State const& s);
    void Free  (State& s); // Destroy mesh

    /******************************************************************************/
    /*
    @brief  Spawns a radial burst of water-droplet particles from a world position
            (typically the pot centre). Particles spray in random directions and
            curve downward under gravity.

    @param  s       The current state of which function is being called
    @param  pos     World coordinate space centre to emit from (emitter)
    */
    /******************************************************************************/
    void EmitWater(State& s, AEVec2 pos);
}
