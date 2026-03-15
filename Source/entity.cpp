/**
 * @file entity.cpp
 * @brief Entity system implementation.
 */

#include <cmath>
#include <iostream>
#include <algorithm>

#include "AEEngine.h"
#include "entity.hpp"

 // For child-specific logic
#include "player.hpp"
#include "customer.hpp"
#include "utilities.hpp"

namespace PS = PlayerSystem;

namespace Entity
{
    // Forward declarations
    Node* AStar(Node start, Node end, const EntityBase* self);

    // Pointer to currently active collision map used by pathfinding.
    // Set via SetCollisionMap() from level/tutor init after Map_Load.
    static const Collision::Map* s_collisionMap = nullptr;

    // Variable for delta time
	float dt = 0.0f;
}

namespace Entity
{
    /// Global array of entity pointers, initialized to nullptr
    EntityBase* entities[MAX_ENTITIES] = { nullptr };
    unsigned int entityCount = 0; // count of active entities

    // =========================================================================
    //     Render list - rebuilt every frame from the entity array, then sorted.
    //     The entity array itself is NEVER reordered.
    // =========================================================================
    static EntityBase* renderList[MAX_ENTITIES];
    static int         renderCount = 0;

    // =========================================================================
    //                          Utility helpers
    // =========================================================================

    AEVec2 IndexToWorld(Index idx);

    Index AdjacentIndex(Index idx, FaceDirection dir);

    bool IsTilePassable(Index idx);

    bool IsEntityBlocking(Index idx);

    bool IsEntityBlockingDynamic(Index idx, const EntityBase* self);

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
        , waitTimer(0.0f)
        , finalGoal{ -1, -1 }
        , hasFinalGoal(false)
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
        , waitTimer(0.0f)
        , finalGoal{ -1, -1 }
        , hasFinalGoal(false)
        , active(act)
        , isAI(AI)
        , holding(hold)
    {}

    EntityBase::EntityBase(ID id_, EntityType type_, AEVec2 worldPos)
        : id(id_)
        , type(type_)
        , position(worldPos)
        , nextCoordinates(worldPos)
        , velocity{ 0.0f, 0.0f }
        , current{ 0, 0 }
        , next{ 0, 0 }
        , speed(0.0f)
        , facing(FaceDirection::DOWN)
        , lastDirection(FaceDirection::DOWN)
        , moveState(MoveState::DEFAULT)
        , waitTimer(0.0f)
        , finalGoal{ -1, -1 }
        , hasFinalGoal(false)
        , active(true)
        , isAI(false)
        , holding(false)
    {
        // Derive grid index from world position using the active collision map
        UpdateIndex();
        next = current;
    }

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
    bool EntityBase::IsCoordsSame() const { return (position.x == nextCoordinates.x) && (position.y == nextCoordinates.y); }
    bool EntityBase::IsIndexSame() const { return (current.x == next.x) && (current.y == next.y); }

    void EntityBase::SetFinalGoal(Index idx) { finalGoal = idx; hasFinalGoal = true; }
    Index EntityBase::GetFinalGoal() const { return finalGoal; }
    bool EntityBase::HasFinalGoal() const { return hasFinalGoal; }
    void EntityBase::ClearFinalGoal() { hasFinalGoal = false; }
    float& EntityBase::RefWaitTimer() { return waitTimer; }


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
    void EntityBase::SetCurrentIndex(Index idx) { current = idx; }
    void EntityBase::SetNextIndex(Index idx) { next = idx; }
    void EntityBase::SetSpeed(float spd) { speed = spd; }
    void EntityBase::SetFaceDirection(FaceDirection dir) { lastDirection = dir; facing = dir; }
    void EntityBase::SetMoveState(MoveState state) { moveState = state; }
    void EntityBase::SetActive(bool act) { active = act; }
    void EntityBase::SetAI(bool AI) { isAI = AI; }
    void EntityBase::SetHolding(bool hold) { holding = hold; }

    // =========================================================================
    //                         Update functions (position, index, etc.)
    // =========================================================================

    void EntityBase::ResolveDirection()
    {
        AEVec2 delta = { nextCoordinates.x - position.x, nextCoordinates.y - position.y };

        if (std::fabs(delta.x) >= std::fabs(delta.y))
        {
            if (delta.x > 0.f) lastDirection = FaceDirection::RIGHT;
            else if (delta.x < 0.f) lastDirection = FaceDirection::LEFT;
        }
        else
        {
            if (delta.y > 0.f) lastDirection = FaceDirection::UP;
            else if (delta.y < 0.f) lastDirection = FaceDirection::DOWN;
        }

        if (delta.x != 0.f || delta.y != 0.f)
        {
            facing = lastDirection;
            moveState = MoveState::WALKING;
        }
        else
        {
            moveState = MoveState::DEFAULT;
        }
    }

    void EntityBase::Move()
    {
        position = nextCoordinates;
        this->UpdateIndex();
    }

    void EntityBase::UpdateIndex()
    {
        if (!s_collisionMap) return;

        int col = static_cast<int>(std::floor((position.x - s_collisionMap->origin.x) / s_collisionMap->tileSize));
        int row = static_cast<int>(std::floor((position.y - s_collisionMap->origin.y) / s_collisionMap->tileSize));

        if (col < 0) col = 0;
        if (row < 0) row = 0;
        if (col >= s_collisionMap->width)  col = s_collisionMap->width - 1;
        if (row >= s_collisionMap->height) row = s_collisionMap->height - 1;

        current = { col, row };
    }

    // ------------------------------------------------------------------
    //             Path manipulation helpers
    // ------------------------------------------------------------------

    void EntityBase::ClearPath()
    {
        path.clear();
        path.shrink_to_fit();
    }

    bool EntityBase::IsPathEmpty() const
    {
        return path.empty();
    }

    const std::vector<Node>& EntityBase::GetPath() const
    {
        return path;
    }

    std::vector<Node>& EntityBase::RefPath()
    {
        return path;
    }

    void EntityBase::PushPathNode(const Node& n)
    {
        path.push_back(n);
    }

    bool EntityBase::SetPath()
    {
        Index startIdx = this->GetCurrentIndex();

        Node startNode{ startIdx, 0, 0, 0, nullptr };
        Node targetNode{ hasFinalGoal ? finalGoal : this->GetNextIndex(), 0, 0, 0, nullptr };

        this->ClearPath();

        Node* newPath = AStar(startNode, targetNode, this);
        if (newPath)
        {
            std::vector<Node> tmp;
            for (Node* p = newPath; p != nullptr; p = p->parent)
                tmp.push_back(*p);

            Node* p = newPath;
            while (p)
            {
                Node* toDelete = p;
                p = p->parent;
                delete toDelete;
            }

            for (auto it = tmp.rbegin(); it != tmp.rend(); ++it)
            {
                it->parent = nullptr;
                this->PushPathNode(*it);
            }

            return true;
        }

        return false;
    }

    bool EntityBase::ConsumePathNode()
    {
        SetCoordinates(GetNextCoordinates());
        SetCurrentIndex(GetNextIndex());
        path.pop_back();
        SetVelocity({ 0.0f, 0.0f });

        if (!path.empty())
        {
            Node nextTarget = path.back();
            SetNextIndex(nextTarget.index);
            SetNextCoordinates(IndexToWorld(nextTarget.index));
            moveState = MoveState::WALKING;
            return true;
        }

        moveState = MoveState::DEFAULT;
        return false;
    }

    bool EntityBase::FollowPath()
    {
        if (dt <= 0.0f) return false;
        if (path.empty()) return false;

        Node target = path.back();

        SetNextIndex(target.index);
        SetNextCoordinates(IndexToWorld(target.index));

        ResolveDirection();

        AEVec2 toTarget = { nextCoordinates.x - position.x, nextCoordinates.y - position.y };
        float dist = std::sqrtf(toTarget.x * toTarget.x + toTarget.y * toTarget.y);

        constexpr float ARRIVE_EPS = 1e-3f;
        if (dist <= ARRIVE_EPS)
        {
            ConsumePathNode();
            return true;
        }

        AEVec2 dir = { toTarget.x / dist, toTarget.y / dist };
        AEVec2 vel = { dir.x * speed, dir.y * speed };
        SetVelocity(vel);

        AEVec2 disp = { vel.x * dt, vel.y * dt };
        float dispLen = std::sqrtf(disp.x * disp.x + disp.y * disp.y);

        if (dispLen >= dist)
        {
            ConsumePathNode();
            ResolveDirection();
        }
        else
        {
            AEVec2 newPos = { position.x + disp.x, position.y + disp.y };
            SetCoordinates(newPos);
            moveState = MoveState::WALKING;
        }

        return true;
    }

    // =========================================================================
    //                         Entity management functions
    // =========================================================================

    int FindFreeEntitySlot()
    {
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

        if (entities[index] != nullptr)
        {
            std::cout << "[WARNING] AddEntityAt: slot " << index
                      << " occupied (id=" << entities[index]->GetId()
                      << "). Searching for next free slot...\n";

            int fallback = FindFreeEntitySlot();
            if (fallback == -1)
            {
                std::cout << "[WARNING] AddEntityAt: no free slots available. Entity not added.\n";
                return false;
            }

            std::cout << "[WARNING] AddEntityAt: using fallback slot " << fallback << ".\n";
            entities[fallback] = e;
            if (e->GetId() == 0)
                e->SetId(static_cast<ID>(fallback));
            ++entityCount;
            return true;
        }

        entities[index] = e;
        if (e->GetId() == 0)
            e->SetId(static_cast<ID>(index));
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
    //                          A* pathfinding
    // =============================================================================

    int Heuristic(const Node& a, const Node& b)
    {
        return std::abs(a.index.x - b.index.x) + std::abs(a.index.y - b.index.y);
    }

    static std::vector<Node> GetNeighbors(const Node& current, const EntityBase* self)
    {
        std::vector<Node> out;

        if (!s_collisionMap || !s_collisionMap->ready)
            return out;

        Index candidates[4] = {
            { current.index.x + 1, current.index.y },
            { current.index.x - 1, current.index.y },
            { current.index.x,     current.index.y + 1 },
            { current.index.x,     current.index.y - 1 }
        };

        for (const Index& idx : candidates)
        {
            if (!IsTilePassable(idx)) continue;
            if (IsEntityBlockingDynamic(idx, self)) continue;

            Node n;
            n.index = idx;
            n.gCost = n.hCost = n.fCost = 0;
            n.parent = nullptr;
            out.push_back(n);
        }

        return out;
    }

    Node* LowestFCost(std::vector<Node*>& open)
    {
        Node* best = open[0];
        for (Node* n : open)
            if (n->fCost < best->fCost)
                best = n;
        return best;
    }

    bool InClosed(const Node& node, const std::vector<Node*>& closed)
    {
        for (const Node* n : closed)
            if (n->index.x == node.index.x && n->index.y == node.index.y)
                return true;
        return false;
    }

    void RemoveFromOpen(std::vector<Node*>& open, Node* node)
    {
        for (auto it = open.begin(); it != open.end(); ++it)
        {
            if (*it == node)
            {
                open.erase(it);
                return;
            }
        }
    }

    Node* AStar(Node start, Node end, const EntityBase* self)
    {
        constexpr size_t MAX_SEARCH_NODES = 2000;
        std::vector<Node*> open;
        std::vector<Node*> closed;

        Node* startPtr = new Node(start);
        startPtr->parent = nullptr;
        startPtr->gCost = 0;
        startPtr->hCost = Heuristic(*startPtr, end);
        startPtr->fCost = startPtr->gCost + startPtr->hCost;
        open.push_back(startPtr);

        auto findInOpen = [&](const Index& idx) -> Node*
        {
            for (Node* n : open)
                if (n->index.x == idx.x && n->index.y == idx.y)
                    return n;
            return nullptr;
        };

        size_t iterations = 0;
        while (!open.empty())
        {
            if (++iterations > MAX_SEARCH_NODES)
            {
                for (Node* n : open) delete n;
                for (Node* n : closed) delete n;
                return nullptr;
            }

            Node* current = LowestFCost(open);

            if (current->index.x == end.index.x && current->index.y == end.index.y)
            {
                Node* result = nullptr;
                for (Node* p = current; p != nullptr; p = p->parent)
                {
                    Node* copy = new Node(*p);
                    copy->parent = result;
                    result = copy;
                }

                for (Node* n : open) delete n;
                for (Node* n : closed) delete n;
                return result;
            }

            RemoveFromOpen(open, current);
            closed.push_back(current);

            for (Node neighbor : GetNeighbors(*current, self))
            {
                if (InClosed(neighbor, closed))
                    continue;

                int tentativeG = current->gCost + 1;

                Node* inOpen = findInOpen(neighbor.index);
                if (inOpen == nullptr)
                {
                    Node* newNode = new Node(neighbor);
                    newNode->parent = current;
                    newNode->gCost = tentativeG;
                    newNode->hCost = Heuristic(*newNode, end);
                    newNode->fCost = newNode->gCost + newNode->hCost;
                    open.push_back(newNode);
                }
                else if (tentativeG < inOpen->gCost)
                {
                    inOpen->parent = current;
                    inOpen->gCost = tentativeG;
                    inOpen->hCost = Heuristic(*inOpen, end);
                    inOpen->fCost = inOpen->gCost + inOpen->hCost;
                }
            }
        }

        for (Node* n : open) delete n;
        for (Node* n : closed) delete n;
        return nullptr;
    }

    void SetCollisionMap(const Collision::Map* map)
    {
        s_collisionMap = map;
    }

    // =============================================================================
    //                  Entity system pipeline stages
    // =============================================================================

    void UpdateAI()
    {
        constexpr float WAIT_TIME_BEFORE_REPATH = 1.0f;

        for (int i = 0; i < MAX_ENTITIES; i++)
        {
            EntityBase* e = entities[i];
            if (!e || !e->IsActive() || !e->IsAI()) continue;

            if (e->IsPathEmpty()) 
            {
                e->SetMoveState(MoveState::IDLE);
                continue; 
            }

            Index nextTarget = e->GetPath().back().index;
            if (IsEntityBlockingDynamic(nextTarget, e))
            {
                e->RefWaitTimer() += dt;
                e->SetMoveState(MoveState::IDLE);

                // Wait exceeded threshold - try to repath around!
                if (e->RefWaitTimer() >= WAIT_TIME_BEFORE_REPATH)
                {
                    e->SetPath(); 
                    e->RefWaitTimer() = 0.0f;
                }
            }
            else
            {
                e->RefWaitTimer() = 0.0f;
                e->FollowPath();
            }
        }
    }

    void UpdatePositions()
    {
        PS::Update(dt);
        UpdateAI();
    }

    void HandleCollisions()
    {
        if (!s_collisionMap || !s_collisionMap->ready) return;

        for (int i = 0; i < MAX_ENTITIES; ++i)
        {
            EntityBase* e = entities[i];
            if (!e || !e->IsActive()) continue;

            // Skip static props — they don't move
            if (e->GetType() == EntityType::PROP) continue;

            if (e->IsCoordsSame()) continue;

            AEVec2 cur  = e->GetCoordinates();
            AEVec2 next = e->GetNextCoordinates();

            float hw = 25.0f;
            float hh = 50.0f;

            if (e->GetType() == EntityType::PLAYER)
            {
                hw = PlayerSystem::HALF_W;
                hh = PlayerSystem::HALF_H;
            }

            bool blockedX = Collision::Map_IsSolidAABB(*s_collisionMap, next.x, cur.y, hw, hh);
            bool blockedY = Collision::Map_IsSolidAABB(*s_collisionMap, blockedX ? cur.x : next.x, next.y, hw, hh);

            // Entity vs Entity AABB Check for smooth local avoidance
            for (int j = 0; j < MAX_ENTITIES; ++j)
            {
                EntityBase* other = entities[j];
                if (!other || !other->IsActive() || other == e) continue;

                float ohw = 25.0f;
                float ohh = 50.0f;
                if (other->GetType() == EntityType::PLAYER) {
                    ohw = PlayerSystem::HALF_W;
                    ohh = PlayerSystem::HALF_H;
                }

                auto CheckAABB = [](float cx1, float cy1, float hw1, float hh1,
                                    float cx2, float cy2, float hw2, float hh2) {
                    Collision::AABB a = { {cx1 - hw1, cy1 - hh1}, {cx1 + hw1, cy1 + hh1} };
                    Collision::AABB b = { {cx2 - hw2, cy2 - hh2}, {cx2 + hw2, cy2 + hh2} };
                    return Collision::IsSolid_Static(a, b);
                };

                // Test X local sliding
                if (CheckAABB(next.x, cur.y, hw, hh, other->GetCoordinates().x, other->GetCoordinates().y, ohw, ohh))
                    blockedX = true;

                // Test Y local sliding
                if (CheckAABB(blockedX ? cur.x : next.x, next.y, hw, hh, other->GetCoordinates().x, other->GetCoordinates().y, ohw, ohh))
                    blockedY = true;
            }

            if (blockedX) next.x = cur.x;
            if (blockedY) next.y = cur.y;

            e->SetNextCoordinates(next);
        }
    }

    void UpdateTransformations()
    {
        for (int i = 0; i < MAX_ENTITIES; ++i)
        {
            EntityBase* e = entities[i];
            if (!e || !e->IsActive()) continue;

            // Skip static props — they never move
            if (e->GetType() == EntityType::PROP) continue;

            e->UpdateIndex();
        }
    }

    // =============================================================================
    //                          Render queue
    // =============================================================================

    void RenderQueue()
    {
        // -- 1. Rebuild render list from the entity array ----------------------
        //    The entity array is the source of truth and is never reordered.
        //    We copy active pointers into a separate list for sorting.
        renderCount = 0;

        for (int i = 0; i < MAX_ENTITIES; ++i)
        {
            if (entities[i] && entities[i]->IsActive())
                renderList[renderCount++] = entities[i];
        }

        // -- 2. Sort by grid row index -----------------------------------------
        //    Higher row index (higher world-Y) is drawn FIRST so that entities
        //    closer to the camera (lower Y) overlap those behind them.
        //    Ties broken by grid column (left to right).
        std::sort(renderList, renderList + renderCount,
            [](const EntityBase* a, const EntityBase* b)
            {
                return a->GetCoordinates().y > b->GetCoordinates().y;
            });

        for (int i = 0; i < renderCount; ++i)
        {
            EntityBase* e = renderList[i];

            switch (e->GetType())
            {
            case EntityType::PLAYER:
                PS::Draw();
                break;
            default:
                break;
            }
        }
    }

    //============================================================================
	// 					     Utilities
    //============================================================================

    AEVec2 IndexToWorld(Index idx)
    {
        if (s_collisionMap && s_collisionMap->ready)
        {
            float ts = s_collisionMap->tileSize;
            return {
                s_collisionMap->origin.x + idx.x * ts + ts * 0.5f,
                s_collisionMap->origin.y + idx.y * ts + ts * 0.5f
            };
        }
        return { idx.x * TILE_SIZE + TILE_SIZE * 0.5f,
                 idx.y * TILE_SIZE + TILE_SIZE * 0.5f };
    }

    Index AdjacentIndex(Index idx, FaceDirection dir)
    {
        switch (dir)
        {
        case FaceDirection::LEFT:  return { idx.x - 1, idx.y };
        case FaceDirection::RIGHT: return { idx.x + 1, idx.y };
        case FaceDirection::UP:    return { idx.x, idx.y + 1 };
        case FaceDirection::DOWN:  return { idx.x, idx.y - 1 };
        default:                   return idx;
        }
    }

    bool IsTilePassable(Index idx)
    {
        if (!s_collisionMap || !s_collisionMap->ready) return false;

        if (idx.x < 0 || idx.y < 0) return false;
        if (idx.x >= s_collisionMap->width || idx.y >= s_collisionMap->height) return false;

        size_t flat = idx.y * s_collisionMap->width + idx.x;

        if (s_collisionMap->solid[flat])
            return false;

        if (IsEntityBlocking(idx))
            return false;

        return true;
    }

    bool IsEntityBlocking(Index idx)
    {
        for (int i = 0; i < MAX_ENTITIES; i++)
        {
            EntityBase* e = entities[i];
            if (!e || !e->IsActive()) continue;

            if (e->GetType() == EntityType::PROP)
            {
                if (e->GetCurrentIndex().x == idx.x &&
                    e->GetCurrentIndex().y == idx.y)
                    return true;
            }
        }
        return false;
    }

    bool IsEntityBlockingDynamic(Index idx, const EntityBase* self)
    {
        for (int i = 0; i < MAX_ENTITIES; i++)
        {
            EntityBase* e = entities[i];
            if (!e || !e->IsActive() || e == self) continue;

            // Check if they are currently occupying the grid or actively transitioning to it
            if (e->GetCurrentIndex().x == idx.x && e->GetCurrentIndex().y == idx.y) return true;
            if (e->GetNextIndex().x == idx.x && e->GetNextIndex().y == idx.y) return true;
        }
        return false;
    }

    AEGfxVertexList* pathLineMesh = nullptr;
    const bool SHOW_PATH_DEBUG = true;

    void drawDebugLine(AEGfxVertexList* mesh, const AEVec2& from, const AEVec2& to, float thickness, float r, float g, float b)
    {
        if (!mesh) return;

        float dx = to.x - from.x;
        float dy = to.y - from.y;
        float length = std::sqrt(dx * dx + dy * dy);
        if (length < 1e-4f) return;

        float angle = std::atan2(dy, dx);
        float midX = 0.5f * (from.x + to.x);
        float midY = 0.5f * (from.y + to.y);

        AEMtx33 s = { 0 }, rot = { 0 }, t = { 0 }, tmp = { 0 }, m = { 0 };
        AEMtx33Scale(&s, length, thickness);
        AEMtx33Rot(&rot, angle);
        AEMtx33Trans(&t, midX, midY);
        AEMtx33Concat(&tmp, &rot, &s);
        AEMtx33Concat(&m, &t, &tmp);
        AEGfxSetTransform(m.m);

        AEGfxSetRenderMode(AE_GFX_RM_COLOR);
        AEGfxSetColorToMultiply(r, g, b, 1.0f);
        AEGfxSetBlendMode(AE_GFX_BM_BLEND);
        AEGfxMeshDraw(mesh, AE_GFX_MDM_TRIANGLES);
    }

    //============================================================================
    //                 Entity system lifecycle
    //============================================================================

    void Load(const Collision::Map* map)
    {
        SetCollisionMap(map);
        pathLineMesh = BasicUtilities::createSquareMesh(1.0f, 1.0f, 0x88FFFFFF);
        PS::Load();
    }

    void Init()
    {
        renderCount = 0;
    }

    void Update()
    {
        dt = static_cast<float>(AEFrameRateControllerGetFrameTime());

        UpdatePositions();
        HandleCollisions();
        UpdateTransformations();
    }

    void Draw()
    {
        RenderQueue();

        if (SHOW_PATH_DEBUG && pathLineMesh)
        {
            for (int i = 0; i < MAX_ENTITIES; ++i)
            {
                EntityBase* e = entities[i];
                if (!e || !e->IsActive() || e->IsPathEmpty()) continue;

                // Color lines differ by AI / Player
                float r = e->IsAI() ? 1.0f : 1.0f;
                float g = e->IsAI() ? 0.3f : 1.0f;
                float b = e->IsAI() ? 0.3f : 0.0f;

                const auto& path = e->GetPath();
                AEVec2 prev = e->GetCoordinates();

                for (int j = static_cast<int>(path.size()) - 1; j >= 0; --j)
                {
                    const Node& n = path[static_cast<size_t>(j)];
                    AEVec2 center = IndexToWorld(n.index);
                    drawDebugLine(pathLineMesh, prev, center, 6.0f, r, g, b);
                    prev = center;
                }
            }
        }
    }

    void Free()
    {
        PS::Free();
        renderCount = 0;
    }

    void Unload()
    {
        PS::Unload();
        ClearEntities();
        renderCount = 0;
        if (pathLineMesh)
        {
            AEGfxMeshFree(pathLineMesh);
            pathLineMesh = nullptr;
        }
    }
} // namespace Entity