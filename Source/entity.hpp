/*===========================================================================
@filename   entity.hpp
@author(s)  -
@course     CSD1451
@section    Section B

@brief      Shared base class for all movable characters (player, NPCs, etc.),
            along with enums for facing direction and movement state.
            Header-only — other character classes inherit from EntityBase
            to get these common fields for free.
============================================================================*/

#ifndef ENTITY_HPP
#define ENTITY_HPP

#include "AEEngine.h"
#include <vector>
#include "collision.hpp"

 // ============================================================================
//                          Definitions and aliases
// ============================================================================
#define MAX_ENTITIES 100           ///< Maximum number of entities stored in the global array
using ID = unsigned int;           ///< Type alias for entity IDs

///TEMP
#define TILE_SIZE 50.0f

// =============================================================================
//                              Entity namespace
// =============================================================================

namespace Entity
{
	/**
	 * @brief Facing direction used for movement and animation.
	 */
	enum class FaceDirection
	{
		LEFT,			///< Facing left
		RIGHT,			///< Facing right
		UP,				///< Facing up / back
		DOWN,			///< Facing down / front
		LAST			///< Sentinel / no change
	};

	/**
	 * @brief Movement state used to drive animations and movement logic.
	 */
	enum class MoveState
	{
		WALKING,		///< Entity is walking (moving slowly)
		RUNNING,		///< Entity is running (moving fast)
		IDLE,			///< Entity is idle (not moving)
		DEFAULT			///< Default / unspecified
	};

	/**
	 * @brief Entity type used for categorization and logic branching.
	 */
	enum class EntityType
	{
		PLAYER,			///< Player-controlled entity
		NPC,			///< Non-player character
		ITEM,			///< Item or pickup
		PROP,			///< Static prop or obstacle
		OTHER			///< Other / uncategorized
	};

	/**
	 * @brief Integer grid index for pathfinding and tile-based logic.
	 */
	struct Index
	{
		int x;			///< grid X index
		int y;			///< grid Y index
	};

	/**
	* @brief Node for pathfinding algorithms, containing grid index and cost values.
	*/
	struct Node
	{
		Index index;		///< Grid index of this node
		int gCost;			///< Cost from start to this node
		int hCost;			///< Heuristic cost from this node to target
		int fCost;			///< Total cost (gCost + hCost)
		Node* parent;		///< Pointer to parent node for path reconstruction
	};

	// =========================================================================
	//                          Utility helpers
	// =========================================================================

	/**
	 * @brief Converts a grid index to world-space tile centre using the active collision map.
	 * @param[in] idx Grid index to convert.
	 * @return World position at the centre of the tile. Returns (0,0) if no map is loaded.
	 */
	AEVec2 IndexToWorld(Index idx);

	/**
	 * @brief Returns the adjacent grid index one step in the given direction.
	 * @param[in] idx  Starting grid index.
	 * @param[in] dir  Direction to step.
	 * @return The neighbouring index. Returns idx unchanged if dir is LAST.
	 */
	Index AdjacentIndex(Index idx, FaceDirection dir);

	/**
	 * @brief Tests whether a grid index is within map bounds and is not solid.
	 * @param[in] idx Grid index to test.
	 * @return true if the tile is passable, false if solid or out of bounds.
	 */
	bool IsTilePassable(Index idx);

	/******************************************************************************/ 
	/*
	@brief  Base class shared by all movable characters (player, NPCs, etc.).
	        Any character class that inherits from this automatically receives
	        all of the data members listed below. Only add members here when
	        they are needed by every type of character — keep character-specific
	        data in the character's own class instead.

	@member position   Where the character currently is in the game world (x, y).
	                   (0, 0) by default.

	@member velocity   How fast the character is moving and in which direction
	                   this frame (x, y). Applied each update to move position.
	                   (0, 0) by default.

	@member speed      The character's maximum movement speed. Used to scale
	                   velocity when movement input is applied. 0 by default.

	@member facing     The direction the character is currently looking.
	                   Used by the animation and draw systems to pick the
	                   correct sprite. Defaults to RIGHT.

	@member moveState  Whether the character is standing still or walking.
	                   Used by the animation system to switch between idle
	                   and walk animations. Defaults to IDLE.
	*/
	/******************************************************************************/
	class EntityBase
	{
	protected:
		ID                 id;                              ///< Unique identifier ( Runtime initialized )
		EntityType         type{ EntityType::OTHER };       ///< Entity type for categorization

		// Core spatial state (protected so derived classes and systems can access)
		AEVec2             position{ 0.0f, 0.0f };          ///< Current world position (pixels)
		AEVec2             nextCoordinates{ 0.0f, 0.0f };	///< Target / next position (pixels)
		AEVec2             velocity{ 0.0f, 0.0f };			///< Current frame velocity (pixels/sec)

		// Grid indices
		Index              current{ 0, 0 };					///< Current grid index
		Index              next{ 0, 0 };					///< Next grid index
		std::vector<Node>  path;							///< Current path of grid indices for pathfinding

		// Movement / animation state
		float              speed{ 0.0f };							///< Movement speed (pixels per second)
		FaceDirection      facing{ FaceDirection::RIGHT };			///< Facing used by animation/draw
		FaceDirection      lastDirection{ FaceDirection::DOWN };	///< Last facing direction (default down)
		MoveState          moveState{ MoveState::DEFAULT };			///< Current movement state

		// AI tracking
		float              waitTimer{ 0.0f };               ///< Timer for blocked AI wait/repath logic
		Index              finalGoal{ -1, -1 };             ///< Reserved end goal for AI pathing
		bool               hasFinalGoal{ false };           ///< If a path target has been set

		// Misc flags
		bool			   active{ true };					///< Whether the entity is active (can be used for soft deletion)
		bool 			   isAI{ false };					///< Whether the entity is AI-controlled
		bool               holding{ false };                ///< Whether entity holds an item (TO BE REMOVED)

		// Rendering
		//AEGfxVertexList*   mesh{ nullptr };				///< Mesh pointer for rendering (commented out — enable when render queue is ready)

		// ------------------------------------------------------------------
		//                    Private path helpers
		// ------------------------------------------------------------------

		/**
		 * @brief Snaps entity to the current target node, pops it from the path,
		 *        and prepares the next node if one exists.
		 *
		 * @details Used internally by FollowPath() to avoid duplicating the
		 *          snap-and-advance logic in multiple branches.
		 *
		 * @return true if more nodes remain in the path, false if path is now empty.
		 */
		bool ConsumePathNode();

	public:
		// ------------------------------------------------------------------
		//                     Constructors & Destructor
		// ------------------------------------------------------------------

		/**
		 * @brief Default constructor.
		 *
		 * @details Leaves the entity in a safe default state. Use setters or a
		 *          parameterized constructor to configure the instance.
		 */
		EntityBase();

		/**
		 * @brief Full constructor initializing every member.
		 *
		 * @param[in] id_      Unique identifier for this entity
		 * @param[in] type     Entity type for categorization.
		 * @param[in] coord    Initial world coordinates.
		 * @param[in] nextCoord Initial next/target coordinates.
		 * @param[in] currentIdx Initial grid index.
		 * @param[in] nextIdx  Initial next grid index.
		 * @param[in] spd      Movement speed in pixels per second.
		 * @param[in] dir      Initial facing direction.
		 * @param[in] st       Initial movement state.
		 * @param[in] act      Whether the entity starts active.
		 * @param[in] AI       Whether the entity is AI-controlled.
		 * @param[in] hold     Initial holding state.
		 * @param[in] mesh_    Optional mesh pointer for rendering (default nullptr).
		 */
		EntityBase(ID id_, EntityType type, AEVec2 coord, AEVec2 nextCoord, Index currentIdx, Index nextIdx,
			float spd, FaceDirection dir, MoveState st, bool act = true, bool AI = false, bool hold = false
		/*, AEGfxVertexList* mesh_ = nullptr*/);

		/**
		 * @brief Static prop constructor. Only requires position and type.
		 *        Grid index is computed from position. Speed/velocity/facing are zeroed.
		 *
		 * @param[in] id_      Unique identifier.
		 * @param[in] type_    Entity type (typically PROP).
		 * @param[in] worldPos World position — grid index is derived automatically.
		 */
		EntityBase(ID id_, EntityType type_, AEVec2 worldPos /*, AEGfxVertexList* mesh_ = nullptr*/);

		/**
		 * @brief Virtual destructor to allow safe deletion via base pointer.
		 */
		virtual ~EntityBase();

		// ------------------------------------------------------------------
		//                              Getters
		// ------------------------------------------------------------------

		/**
		 * @brief Get unique id.
		 * @return Entity id (0 means unassigned).
		 */
		ID GetId() const;

		/**
		* @brief Get entity type.
		* @return EntityType enum value representing the entity's type.
		*/
		EntityType GetType() const;

		/**
		 * @brief Returns the current world coordinates.
		 * @return AEVec2 current position (x, y).
		 */
		AEVec2 GetCoordinates() const;

		/**
		 * @brief Returns the queued/target coordinates (next).
		 * @return AEVec2 next/target position.
		 */
		AEVec2 GetNextCoordinates() const;

		/**
		 * @brief Returns the current velocity.
		 * @return AEVec2 velocity (x, y) in pixels per second.
		 */
		AEVec2 GetVelocity() const;

		/**
		 * @brief Returns the current grid index.
		 * @return Index representing current tile index.
		 */
		Index GetCurrentIndex() const;

		/**
		 * @brief Returns the next grid index.
		 * @return Index representing the next tile index.
		 */
		Index GetNextIndex() const;

		/**
		 * @brief Returns the movement speed.
		 * @return Speed in pixels per second.
		 */
		float GetSpeed() const;

		/**
		 * @brief Returns the last facing direction.
		 * @return FaceDirection enum value.
		 */
		FaceDirection GetLastDirection() const;

		/**
		 * @brief Returns the current movement state.
		 * @return MoveState enum value.
		 */
		MoveState GetMoveState() const;

		/**
		* @brief Returns whether the entity is active.
		* @return true if active, false otherwise.
		*/
		bool IsActive() const;

		/**
		 * @brief Query whether the entity is holding an item.
		 * @return true when holding an item, false otherwise.
		 */
		bool IsHolding() const;

		/**
		* @brief Returns whether the entity is AI-controlled.
		* @return true if entity is AI-controlled, false otherwise.
		*/
		bool IsAI() const;

		/**
		* @brief Returns whether current and next coordinates are the same
		* @return true if coordinates are the same, false otherwise
		*/
		bool IsCoordsSame() const;

		/**
		* @brief Returns if current and next index are the same
		* @return true if Indexes are the same, false otherwise
		*/
		bool IsIndexSame() const;

		// AI GETTERS / SETTERS

		/**
		* @brief Sets the final goal index for AI pathfinding.
		 * @param[in] idx Grid index representing the final destination for pathfinding.
		 * @details This is used by AI to remember their ultimate target, even if they need to temporarily deviate or wait.
		 *          Setting this does not automatically compute a path — call ComputePath() after setting a new goal.
		*/
		void SetFinalGoal(Index idx);

		/**
		* @brief Gets the final goal index for AI pathfinding.
		*/
		Index GetFinalGoal() const;

		/**
		* @brief Checks if a final goal has been set for AI pathfinding.
		 * @return true if a final goal index has been set, false otherwise.
		 * @details This can be used by AI logic to determine whether the entity is currently pursuing a target or just wandering.
		*/
		bool HasFinalGoal() const;

		/**
		* @brief Clears the final goal index for AI pathfinding and resets the hasFinalGoal flag.
		 * @post After calling this, HasFinalGoal() will return false and GetFinalGoal() will return an invalid index (-1, -1).
		 * @details Use this when an AI reaches its target or needs to abandon its current goal for any reason.
		 *          This does not automatically clear the current path or next index — call ClearPath() if you also want to reset those.
		*/
		void ClearFinalGoal();

		/**
		* @brief Reference to the AI wait timer.
		*/
		float& RefWaitTimer();

		// ------------------------------------------------------------------
		//                     Reference Getters (For AI)
		// ------------------------------------------------------------------

		/**
		* @brief Reference to the current coordinates.
		*/
		AEVec2& RefCoordinates();

		/**
		* @brief Reference to the X component of the current coordinates.
		* 
		* @return Reference to the X component of the current coordinates, allowing direct modification.
		*/
		float& RefX();

		/**
		* @brief Reference to the Y component of the current coordinates.
		* 
		* @return Reference to the Y component of the current coordinates, allowing direct modification.
		*/
		float& RefY();

		/**
		* @brief Reference to the X component of the next coordinates.
		* 
		* @return Reference to the X component of the next coordinates, allowing direct modification.
		*/
		float& RefNextX();

		/**
		* @brief Reference to the Y component of the next coordinates.
		* 
		* @return Reference to the Y component of the next coordinates, allowing direct modification.
		*/
		float& RefNextY();

		/**
		* @brief Reference to the active state of the entity.
		* 
		* @return Reference to the active boolean, allowing direct modification.
		* @details This can be used by AI or other systems to deactivate entities without removing them from the global array.
		*/
		bool& RefActive();

		/**
		* @brief Reference to the facing direction of the entity.
		*/
		FaceDirection& RefFaceDirection();

		// ------------------------------------------------------------------
		//                              Setters
		// ------------------------------------------------------------------

		/**
		 * @brief Assign a unique identifier to this entity.
		 * @param[in] id_ New ID to assign. Passing 0 indicates "unassigned".
		 */
		void SetId(ID id_);

		/**
		 * @brief Set the entity's categorical type (player, NPC, item, etc.).
		 * @param[in] type New EntityType value.
		 */
		void SetType(EntityType type);

		/**
		 * @brief Set the queued/target world coordinates for the entity.
		 * @param[in] coord Target coordinates (x, y) in world pixels.
		 */
		void SetNextCoordinates(AEVec2 coord);

		/**
		 * @brief Immediately set the entity's current world coordinates.
		 * @param[in] coord New world position (x, y) in pixels.
		 */
		void SetCoordinates(AEVec2 coord);

		/**
		 * @brief Set the current frame velocity for the entity.
		 * @param[in] vel Velocity vector (pixels per second).
		 */
		void SetVelocity(AEVec2 vel);

		/**
		 * @brief Update the entity's current grid index.
		 * @param[in] idx New grid index representing the current tile.
		 */
		void SetCurrentIndex(Index idx);

		/**
		 * @brief Update the entity's next grid index (planned target tile).
		 * @param[in] idx Grid index representing the next tile to move toward.
		 */
		void SetNextIndex(Index idx);

		/**
		 * @brief Set the entity's movement speed.
		 * @param[in] spd Speed in pixels per second.
		 */
		void SetSpeed(float spd);

		/**
		 * @brief Set the entity's facing direction.
		 * @param[in] dir New facing direction.
		 * @details Also updates lastDirection so animations remember the most recent orientation.
		 */
		void SetFaceDirection(FaceDirection dir);

		/**
		 * @brief Set the entity's movement state (walking, idle, etc.).
		 * @param[in] state New MoveState value.
		 */
		void SetMoveState(MoveState state);

		/**
		 * @brief Set whether the entity is currently holding an item.
		 * @param[in] hold True when the entity holds an item, false otherwise.
		 */
		void SetHolding(bool hold);

		/**
		 * @brief Activate or deactivate the entity.
		 * @param[in] act True to mark as active, false to mark as inactive.
		 */
		void SetActive(bool act);

		/**
		 * @brief Mark the entity as AI-controlled or player-controlled.
		 * @param[in] ai True when controlled by AI, false for player control.
		 */
		void SetAI(bool ai);

		// =========================================================================
		//                  Update functions (position, index, etc.)
		// =========================================================================

		/**
		* @brief Resolves facing direction based on the delta between current and next coordinates.
		* @details Used by AI or movement systems to automatically update facing direction based on movement.
		*/
		void ResolveDirection();

		/**
		 * @brief Commit the queued next coordinates as the current position and update the grid index.
		 */
		void Move();

		/**
		* @brief Update the entity's current grid index based on its world coordinates and the collision map.
		* @details Should be called after moving the entity to keep the grid index in sync with its position.
		*/
		void UpdateIndex();

		// ------------------------------------------------------------------
		//                    Path manipulation helpers
		// ------------------------------------------------------------------

		/**
		 * @brief Clears the path and releases memory used by the container.
		 * @post After calling this, IsPathEmpty() will return true.
		 */
		void ClearPath();

		/**
		 * @brief Query whether the current path is empty.
		 * @return true if no nodes are stored in the path, false otherwise.
		 */
		bool IsPathEmpty() const;

		/**
		 * @brief Returns a const reference to the internal path vector.
		 * @note Use this to inspect the planned nodes without modifying them.
		 */
		const std::vector<Node>& GetPath() const;

		/**
		 * @brief Returns a mutable reference to the internal path vector.
		 * @note Allows path modification by external systems.
		 */
		std::vector<Node>& RefPath();

		/**
		 * @brief Computes a new A* path from the entity's current index to its next index.
		 * @details Clears any existing path before computing. The resulting path is stored
		 *          internally with back() == next step so FollowPath() can consume it.
		 * @return true if a valid path was found, false otherwise (path left empty).
		 */
		bool SetPath();

		/**
		 * @brief Append a node to the end of the path.
		 * @param[in] n Node to push onto the path.
		 */
		void PushPathNode(const Node& n);

		/**
		* @brief Moves the entity along its stored path for the current frame.
		*
		* @details Computes the displacement toward the next path node using the entity's
		*          speed and the given delta time. Snaps to the node and advances to the
		*          next one if the displacement would overshoot.
		*
		* @param[in] dt Frame delta time in seconds.
		* @return true if the entity moved or consumed a node, false if there was no path or dt was invalid.
		*/
		bool FollowPath();
	};

	//=============================================================================
	//					     Extern declarations for systems
	//=============================================================================

	/**
	 * @brief Global entity pointer array.
	 *
	 * @details Each slot stores a pointer to an EntityBase or a derived
	 *          instance. Systems may add/remove entities using helper
	 *          functions. Slots not in use are nullptr.
	 *
	 * @note The array takes ownership of pointers passed to AddEntity().
	 */
	extern EntityBase* entities[MAX_ENTITIES];

	/**
	 * @brief Attempts to add an entity pointer into the first free slot.
	 * @param[in] e Pointer to an EntityBase (usually a derived instance).
	 * @return true if entity was added, false if no free slot was available.
	 */
	bool AddEntity(EntityBase* e);

	/**
	* @brief Adds an entity pointer at a specific index if the slot is free.
	* @param[in] e Pointer to an EntityBase (usually a derived instance).
	* @param[in] index Desired slot index (0..MAX_ENTITIES-1).
	* @return true if entity was added at the specified index, false otherwise.
	*/
	bool AddEntityAt(EntityBase* e, int index);

	/**
	 * @brief Removes and deletes the entity at the given index.
	 * @param[in] index Slot index to remove (0..MAX_ENTITIES-1).
	 */
	void RemoveEntity(int index);

	/**
	 * @brief Deletes and clears all entities stored in the global array.
	 * @post All entries in `entities` will be set to nullptr.
	 */
	void ClearEntities();

	/**
	 * @brief Finds the first free slot index in the entities array.
	 * @return Index of a free slot (0..MAX_ENTITIES-1) or -1 if none available.
	 */
	int FindFreeEntitySlot();

	//=============================================================================
	//					     Entity system functions
	//=============================================================================

	/**
	 * @brief Loads resources needed by the entity system.
	 */
	void Load(const Collision::Map* map = {});

	/**
	 * @brief Initializes the entity system and creates initial entities.
	 */
	void Init();

	/**
	 * @brief Updates all entities in the system. Called every frame.
	 */
	void Update();

	/**
	 * @brief Draws all entities in the system. Called every frame after Update().
	 */
	void Draw();

	/**
	 * @brief Frees per-frame resources used by the entity system.
	 */
	void Free();

	/**
	* @brief Unloads the entity system, clearing all entities and freeing resources.
	*/
	void Unload();

	// =========================================================================
	//                  Entity system pipeline stages
	// =========================================================================

	/**
	 * @brief Calls each sub-system's movement logic (e.g. PS::Update).
	 *        Each system computes nextCoordinates/velocity but does NOT commit position.
	 * @param[in] dt Frame delta time in seconds.
	 */
	void UpdatePositions();

	/**
	 * @brief Iterates active entities and validates their intended nextCoordinates
	 *        against the tile map and prop AABBs. Reverts blocked axes.
	 */
	void HandleCollisions();

	/**
	 * @brief Commits movement for all active entities: position = nextCoordinates,
	 *        updates grid index, and resolves facing direction.
	 */
	void UpdateTransformations();

	// =========================================================================
	//                          Render queue
	// =========================================================================

	/**
	 * @brief Builds a sorted draw list of all active entities (lowest Y first)
	 *        and invokes each sub-system's draw for that entity.
	 *
	 * @details Sorting by grid row index (descending Y = lower row = drawn first)
	 *          is cheaper than sorting by float world-Y and produces equivalent
	 *          results on a uniform tile grid.
	 *
	 *          Call this from Entity::Draw(). Level code should call Entity::Draw()
	 *          at the correct point in its own draw order.
	 */
	void RenderQueue();
}

#endif // !ENTITY_HPP