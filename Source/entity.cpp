/**
 * @file entity.cpp
 * @brief Entity system implementation.
 */

#include <cmath>
#include <iostream>
#include <algorithm>

#include "AEEngine.h"
#include "entity.hpp"

 // For player-specific logic
#include "player.hpp"

namespace PS = PlayerSystem;

namespace Entity
{
    // Forward declarations
    Node* AStar(Node start, Node end);

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

        size_t flat = static_cast<size_t>(idx.y) * s_collisionMap->width + static_cast<size_t>(idx.x);
        return !s_collisionMap->solid[flat];
    }

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
        //, mesh(nullptr)
        //, texture(nullptr)
        //, drawScale{ 64.0f, 64.0f }
        //, drawMode(DrawMode::NONE)
    {}

    EntityBase::EntityBase(ID id_, EntityType type_, AEVec2 coord, AEVec2 nextCoord, Index currentIdx, Index nextIdx,
                           float spd, FaceDirection dir, MoveState st, bool act, bool AI, bool hold
                           /*, AEGfxVertexList* mesh_, AEGfxTexture* tex_,
                              AEVec2 scale_, DrawMode mode_*/)
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
        //, mesh(mesh_)
        //, texture(tex_)
        //, drawScale(scale_)
        //, drawMode(mode_)
    {}

    EntityBase::EntityBase(ID id_, EntityType type_, AEVec2 worldPos
                           /*, AEGfxVertexList* mesh_, AEGfxTexture* tex_,
                              AEVec2 scale_, DrawMode mode_*/)
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
        , active(true)
        , isAI(false)
        , holding(false)
        //, mesh(mesh_)
        //, texture(tex_)
        //, drawScale(scale_)
        //, drawMode(mode_)
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

    //AEGfxVertexList* EntityBase::GetMesh() const { return mesh; }
    //AEGfxTexture* EntityBase::GetTexture() const { return texture; }
    //AEVec2 EntityBase::GetDrawScale() const { return drawScale; }
    //DrawMode EntityBase::GetDrawMode() const { return drawMode; }

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

    //void EntityBase::SetMesh(AEGfxVertexList* m) { mesh = m; }
    //void EntityBase::SetTexture(AEGfxTexture* tex) { texture = tex; }
    //void EntityBase::SetDrawScale(AEVec2 scale) { drawScale = scale; }
    //void EntityBase::SetDrawMode(DrawMode mode) { drawMode = mode; }

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
        Node targetNode{ this->GetNextIndex(), 0, 0, 0, nullptr };

        this->ClearPath();

        Node* newPath = AStar(startNode, targetNode);
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

    static std::vector<Node> GetNeighbors(const Node& current)
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

    Node* AStar(Node start, Node end)
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

            for (Node neighbor : GetNeighbors(*current))
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

    void UpdatePositions()
    {
        PS::Update(dt);
    }

    void HandleCollisions()
    {
        if (!s_collisionMap || !s_collisionMap->ready) return;

        for (int i = 0; i < MAX_ENTITIES; ++i)
        {
            EntityBase* e = entities[i];
            if (!e || !e->IsActive()) continue;

            // AI entities rely on A* which already avoids solid tiles
            if (e->IsAI()) continue;

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

            if (blockedX) next.x = cur.x;
            if (blockedY) next.y = cur.y;

            if (blockedX && blockedY)
            {
                AEVec2 tileCentre = IndexToWorld(e->GetCurrentIndex());
                e->SetCoordinates(tileCentre);
                e->SetNextCoordinates(tileCentre);
            }
            else
            {
                e->SetNextCoordinates(next);
            }
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
                int rowA = a->GetCurrentIndex().y;
                int rowB = b->GetCurrentIndex().y;
                if (rowA != rowB) return rowA > rowB;
                return a->GetCurrentIndex().x < b->GetCurrentIndex().x;
            });

        // -- 3. Iterate and dispatch draw calls ------------------------------
        //    Currently dispatches to sub-system draw functions by EntityType.
        //
        //    [FUTURE] When DrawMode and the rendering members are uncommented,
        //    this loop will check e->GetDrawMode() and call the appropriate
        //    Sprite:: function directly:
        //
        //    switch (e->GetDrawMode())
        //    {
        //    case DrawMode::STATIC:
        //        BasicUtilities::Sprite::DrawSprite(
        //            e->GetMesh(), e->GetTexture(),
        //            e->GetCoordinates(), e->GetDrawScale());
        //        break;
        //
        //    case DrawMode::ANIM_GRID:
        //        // Requires AnimationState stored on the entity or sub-system.
        //        // BasicUtilities::Sprite::UpdateAnimation(animState, row, dur, rows, cols);
        //        // BasicUtilities::Sprite::DrawAnimation(
        //        //     e->GetMesh(), e->GetTexture(), animState,
        //        //     e->GetCoordinates(), e->GetDrawScale());
        //        break;
        //
        //    case DrawMode::ANIM_JSON:
        //        // Requires AnimationState + JMeta stored on entity or sub-system.
        //        // BasicUtilities::Sprite::UpdateAnimation(animState, meta, tagIndex);
        //        // BasicUtilities::Sprite::DrawAnimation(
        //        //     e->GetMesh(), e->GetTexture(), animState,
        //        //     e->GetCoordinates(), e->GetDrawScale());
        //        break;
        //
        //    case DrawMode::NONE:
        //    default:
        //        break;
        //    }

        for (int i = 0; i < renderCount; ++i)
        {
            EntityBase* e = renderList[i];

            switch (e->GetType())
            {
            case EntityType::PLAYER:
                PS::Draw();
                break;

            // Future: case EntityType::NPC: CustomerSystem::DrawSingle(...); break;
            // Future: case EntityType::PROP: PropSystem::DrawSingle(...); break;

            default:
                break;
            }
        }
    }

    //============================================================================
    //                 Entity system lifecycle
    //============================================================================

    void Load(const Collision::Map* map)
    {
        SetCollisionMap(map);
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
    }
} // namespace Entity