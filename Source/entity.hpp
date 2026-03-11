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

// ============================================================================
//                          Definitions and aliases
// ============================================================================
#define MAX_ENTITIES 100           ///< Maximum number of entities stored in the global array
using ID = unsigned int;           ///< Type alias for entity IDs

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
	 * @brief Integer grid index used for tile / map lookups.
	 */
	struct Index
	{
		int x;			///< grid X index
		int y;			///< grid Y index
	};

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

		// Movement / animation state
		float              speed{ 0.0f };							///< Movement speed (pixels per second)
		FaceDirection      facing{ FaceDirection::RIGHT };			///< Facing used by animation/draw
		FaceDirection      lastDirection{ FaceDirection::DOWN };	///< Last facing direction (default down)
		MoveState          moveState{ MoveState::DEFAULT };			///< Current movement state

		// Misc flags
		bool			   active{ true };					///< Whether the entity is active (can be used for soft deletion)
		bool 			   isAI{ false };					///< Whether the entity is AI-controlled
		bool               holding{ false };                ///< Whether entity holds an item

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
		 * @param[in] spd      Movement speed in ction.
		 * @param[in] st       Initial movement state.
		 * @param[in] hold     Initial holding state.
		 */
		EntityBase(ID id_, EntityType type, AEVec2 coord, AEVec2 nextCoord, Index currentIdx, Index nextIdx,
			float spd, FaceDirection dir, MoveState st, bool act = true, bool AI = false, bool hold = false);

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
		* @brief Resolves facing direction based on the delta between current and next coordinates.
		* 
		* @details This can be used by AI or movement systems to automatically update the facing direction based on movement.
		*/
		void ResolveDirection();

		// ------------------------------------------------------------------
		//                     Reference Getters (For AI)
		// ------------------------------------------------------------------

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

		FaceDirection& RefFaceDirection();

		// ------------------------------------------------------------------
		//                              Setters
		// ------------------------------------------------------------------

		/**
		 * @brief Set entity unique id.
		 * @param[in] id_ New id value (0 = unassigned).
		 */
		void SetId(ID id_);

		/**
		* @brief Sets the entity type.
		* @param[in] type EntityType enum value representing the new type.
		*/
		void SetType(EntityType type);

		/**
		 * @brief Sets the coordinates.
		 * @param[in] coord coordinates (x, y).
		 */
		void SetCoordinates(AEVec2 coord);

		/**
		 * @brief Sets the queued/target coordinates.
		 * @param[in] coord Next coordinates (x, y).
		 */
		void SetNextCoordinates(AEVec2 coord);

		void SetVelocity(AEVec2 vel);

		/**
		* @brief Sets the current world coordinates.
		* @details This sets both `position` and `nextCoordinates` to the same value,
		*/
		void Move();

		/**
		 * @brief Sets the current grid index.
		 * @param[in] idx New current index.
		 */
		void SetCurrentIndex(Index idx);

		/**
		 * @brief Sets the next grid index.
		 * @param[in] idx New next index.
		 */
		void SetNextIndex(Index idx);

		/**
		 * @brief Sets the entity's movement speed.
		 * @param[in] spd Speed in pixels per second.
		 */
		void SetSpeed(float spd);

		/**
		 * @brief Sets the facing direction.
		 * @param[in] dir New facing direction.
		 */
		void SetFaceDirection(FaceDirection dir);

		/**
		 * @brief Sets the movement state.
		 * @param[in] state New movement state.
		 */
		void SetMoveState(MoveState state);

		/**
		 * @brief Sets whether the entity is holding an item.
		 * @param[in] hold true if holding, false otherwise.
		 */
		void SetHolding(bool hold);

		/**
		* @brief Sets whether the entity is active.
		* @param[in] act true to set active, false to set inactive.
		*/
		void SetActive(bool act);

		/**
		* @brief Sets whether entity is AI.
		* @param[in] ai true if entity is AI-controlled, false otherwise.
		*/
		void SetAI(bool ai);
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
	 * @note Ownership: the global array becomes responsible for deleting the pointer.
	 */
	bool AddEntity(EntityBase* e);

	/**
	* @brief Adds an entity pointer at a specific index if the slot is free.
	* @param[in] e Pointer to an EntityBase (usually a derived instance).
	* @param[in] index Desired slot index (0..MAX_ENTITIES-1).
	* @return true if entity was added at the specified index, false if index is out of range or slot is occupied.
	*/
	bool AddEntityAt(EntityBase* e, int index);

	/**
	 * @brief Removes and deletes the entity at the given index.
	 * @param[in] index Slot index to remove (0..MAX_ENTITIES-1).
	 * @return true on successful removal and deletion, false on invalid index or empty slot.
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
	 * @details This may include loading textures, meshes, or other assets
	 *          shared by multiple entities. Called once when the system is initialized.
	 */
	void Load();

	/**
	 * @brief Initializes the entity system and creates initial entities.
	 * @details Sets up any necessary data structures and creates initial entities
	 *          (e.g., player). Called once after Load().
	 */
	void Init();

	/**
	 * @brief Updates all entities in the system.
	 * @details This may include moving entities, handling input, or other
	 *          per-frame logic. Called every frame.
	 */
	void Update();

	/**
	 * @brief Draws all entities in the system.
	 * @details Renders entities to the screen. Called every frame after Update().
	 */
	void Draw();

	/**
	 * @brief Frees resources used by the entity system.
	 * @details This may include freeing textures, meshes, or other assets.
	 *          Called once when the system is being shut down or when returning
	 *          to a main menu.
	 */
	void Free();

	/**
	* @brief Unloads the entity system, clearing all entities and freeing resources.
	* @details Calls ClearEntities() to delete all entity instances and sets pointers to nullptr.
	*/
	void Unload();
}

#endif // !ENTITY_HPP