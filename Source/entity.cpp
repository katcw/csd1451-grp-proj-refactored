/**
 * @file entity.cpp
 * @brief Entity system implementation.
 */

#include <cmath>
#include <iostream>

#include "AEEngine.h"
#include "entity.hpp"

 // For player-specific logic
#include "player.hpp"

namespace PS = PlayerSystem;

namespace Entity
{
    /// Global array of entity pointers, initialized to nullptr
    EntityBase* entities[MAX_ENTITIES] = { nullptr };
    unsigned int entityCount = 0; // count of active entities

    // =========================================================================
    //                        EntityBase class definitions
    // =========================================================================

    // -------------------------------------------------------------------------
    //                         Constructors & Destructor
    // -------------------------------------------------------------------------

    EntityBase::EntityBase()
        : id(0)
        , type(EntityType::OTHER)
        , position{ 0.0f, 0.0f }
        , nextCoordinates{ 0.0f, 0.0f }
        , velocity{ 0.0f, 0.0f }
        , current{ 0, 0 }
        , next{ 0, 0 }
        , speed(0.0f)
        , facing(FaceDirection::RIGHT)
        , lastDirection(FaceDirection::DOWN)
        , moveState(MoveState::DEFAULT)
        , active(true)
        , isAI(false)
        , holding(false)
    {}

    EntityBase::EntityBase(ID id_, EntityType type_, AEVec2 coord, AEVec2 nextCoord, Index currentIdx, Index nextIdx,
                           float spd, FaceDirection dir, MoveState st, bool act, bool AI, bool hold)
        : id(id_)
        , type(type_)
        , position(coord)
        , nextCoordinates(nextCoord)
        , velocity{ 0.0f, 0.0f }
        , current(currentIdx)
        , next(nextIdx)
        , speed(spd)
        , facing(dir)
        , lastDirection(dir)
        , moveState(st)
        , active(act)
        , isAI(AI)
        , holding(hold)
    {}

    EntityBase::~EntityBase() {}

    // -------------------------------------------------------------------------
    //                              Getters
    // -------------------------------------------------------------------------

    ID EntityBase::GetId() const { return id; }
    EntityType EntityBase::GetType() const { return type; }
    AEVec2 EntityBase::GetCoordinates() const { return position; }
    AEVec2 EntityBase::GetNextCoordinates() const { return nextCoordinates; }
	AEVec2 EntityBase::GetVelocity() const { return velocity; }
    Index EntityBase::GetCurrentIndex() const { return current; }
    Index EntityBase::GetNextIndex() const { return next; }
    float EntityBase::GetSpeed() const { return speed; }
    FaceDirection EntityBase::GetLastDirection() const { return lastDirection; }
    MoveState EntityBase::GetMoveState() const { return moveState; }
    bool EntityBase::IsActive() const { return active; }
    bool EntityBase::IsAI() const { return isAI; }
    bool EntityBase::IsHolding() const { return holding; }

    void EntityBase::ResolveDirection()
    {
        AEVec2 delta = { nextCoordinates.x - position.x, nextCoordinates.y - position.y };

        if (std::fabs(delta.x) > std::fabs(delta.y))
            lastDirection = (delta.x > 0.f) ? FaceDirection::RIGHT : FaceDirection::LEFT;
        else if (std::fabs(delta.y) > std::fabs(delta.x))
            lastDirection = (delta.y > 0.f) ? FaceDirection::UP : FaceDirection::DOWN;

        if (delta.x != 0.f || delta.y != 0.f)
            moveState = MoveState::WALKING;
        else
            moveState = MoveState::DEFAULT;
    }

    // -------------------------------------------------------------------------
    //                         Reference Getters
    // -------------------------------------------------------------------------

	AEVec2& EntityBase::RefCoordinates() { return position; }
    float& EntityBase::RefX() { return position.x; }
    float& EntityBase::RefY() { return position.y; }
    float& EntityBase::RefNextX() { return nextCoordinates.x; }
    float& EntityBase::RefNextY() { return nextCoordinates.y; }
    bool&  EntityBase::RefActive() { return active; }
	FaceDirection& EntityBase::RefFaceDirection() { return facing; }

    // -------------------------------------------------------------------------
    //                              Setters
    // -------------------------------------------------------------------------

    void EntityBase::SetId(ID id_) { id = id_; }
    void EntityBase::SetType(EntityType t) { type = t; }
    void EntityBase::SetNextCoordinates(AEVec2 coord) { nextCoordinates = coord; }
    void EntityBase::SetCoordinates(AEVec2 coord) { position = coord; }
	void EntityBase::SetVelocity(AEVec2 vel) { velocity = vel; }
    void EntityBase::Move() { position = nextCoordinates; }
    void EntityBase::SetCurrentIndex(Index idx) { current = idx; }
    void EntityBase::SetNextIndex(Index idx) { next = idx; }
    void EntityBase::SetSpeed(float spd) { speed = spd; }
    void EntityBase::SetFaceDirection(FaceDirection dir) { lastDirection = dir; facing = dir; }
    void EntityBase::SetMoveState(MoveState state) { moveState = state; }
    void EntityBase::SetActive(bool act) { active = act; }
    void EntityBase::SetAI(bool AI) { isAI = AI; }
    void EntityBase::SetHolding(bool hold) { holding = hold; }

    // =========================================================================
    //                         Entity management functions
    // =========================================================================

    int FindFreeEntitySlot()
    {
        // reserve slot 0 (commonly used for player in this project)
        for (int i = 1; i < MAX_ENTITIES; ++i)
            if (entities[i] == nullptr) return i;
        return -1;
    }

    bool AddEntity(EntityBase* e)
    {
        int slot = FindFreeEntitySlot();
        if (slot == -1 || !e) return false;
        entities[slot] = e;
        if (e->GetId() == 0)
            e->SetId(static_cast<ID>(slot));
        ++entityCount;
        return true;
    }

    bool AddEntityAt(EntityBase* e, int index)
    {
        if (!e || index < 0 || index >= MAX_ENTITIES) return false;
        if (entities[index]) return false;
        entities[index] = e;
        ++entityCount;
        return true;
    }

    void RemoveEntity(int index)
    {
        if (index < 0 || index >= MAX_ENTITIES) return;
        if (entities[index] != nullptr)
        {
            delete entities[index];
            entities[index] = nullptr;
            --entityCount;
        }
    }

    void ClearEntities()
    {
        for (int i = 0; i < MAX_ENTITIES; ++i)
        {
            delete entities[i];
            entities[i] = nullptr;
        }
        entityCount = 0;
    }

    // =============================================================================
    //                          Additional helper functions
    // =============================================================================

    void UpdatePositions()
    {
    }

    void HandleCollisions()
    {
    }

    void UpdateTransformations()
    {
    }

    void SnapToGrid(EntityBase* e)
    {
		(void)e; // placeholder to avoid unused parameter warning
    }

    //============================================================================
    //                 Entity system lifecycle (minimal implementations)
    //============================================================================

    void Load()
    {
        PS::Load();
    }

    void Init()
    {
		//PS::Init({ 0.f, 0.f }); // Initialize player at (0, 0) or spawn point from map
    }

    void Update()
    {
		//UpdatePositions();
        HandleCollisions();
		UpdateTransformations();
    }

    void Draw()
    {
		//PS::Draw();
    }

    void Free()
    {
        PS::Free();
    }

    void Unload()
    {
        PS::Unload();
        ClearEntities();
    }
} // namespace Entity