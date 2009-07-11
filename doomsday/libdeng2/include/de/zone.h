/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 1999-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 * Copyright (c) 2006-2009 Daniel Swanson <danij@dengine.net>
 * Copyright (c) 1993-1996 by id Software, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef LIBDENG2_ZONE_H
#define LIBDENG2_ZONE_H

#include <de/deng.h>

#include <list>

/**
 * Define LIBDENG2_FAKE_MEMORY_ZONE to force all memory blocks to be allocated from
 * the real heap. Useful when debugging memory-related problems.
 */
//#define LIBDENG2_FAKE_MEMORY_ZONE

namespace de
{
    /**
     * The memory zone allocates raw blocks of memory with low overhead.
     * The zone is composed of multiple memory volumes. New volumes are allocated
     * when there is no space left on the existing ones, allowing the memory zone
     * to grow at runtime.
     *
     * When fast malloc mode is enabled, memory volumes aren't checked for purgable
     * blocks. If the rover block isn't suitable, a new empty volume is created
     * without further checking. This is suitable for cases where lots of blocks
     * are being allocated in a rapid sequence, with no frees in between (e.g.,
     * map setup).
     *
     * Block sequences. The MAP_STATIC purge tag has a special purpose.
     * It works like MAP so that it is purged on a per map basis, but
     * blocks allocated as MAP_STATIC should not be freed at any time when the
     * map is being used. Internally, the map-static blocks are linked into
     * sequences so that Z_Malloc knows to skip all of them efficiently. This is
     * possible because no block inside the sequence could be purged by Z_Malloc
     * anyway.
     *
     * There is never any space between memblocks, and there will never be
     * two contiguous free memblocks.
     *
     * The rover can be left pointing at a non-empty block.
     *
     * It is of no value to free a cachable block, because it will get
     * overwritten automatically if needed.
     *
     * @ingroup core
     */
    class PUBLIC_API Zone
    {
    public:
        class Batch;
        
        /// The specified memory address was outside the zone. @ingroup errors
        DEFINE_ERROR(ForeignError);
        
        /// Invalid purge tag. @ingroup errors
        DEFINE_ERROR(TagError);

        /// Invalid owner. @ingroup errors
        DEFINE_ERROR(OwnerError);    
        
        /// A problem in the structure of the memory zone was detected during
        /// verification. @ingroup errors
        DEFINE_ERROR(ConsistencyError);
        
        /// Purge tags indicate when/if a block can be freed.
        enum PurgeTag
        {
            UNDEFINED = 0,
            STATIC = 1,     ///< Static entire execution time.
            SOUND = 2,      ///< Static while playing.
            MUSIC = 3,      ///< Static while playing.
            REFRESH_TEXTURE = 11, // Textures/Flats/refresh.
            REFRESH_COLORMAP = 12,
            REFRESH_TRANSLATION = 13,
            REFRESH_SPRITE = 14, 
            PATCH = 15,
            MODEL = 16,
            SPRITE = 20,

            USER1 = 40,
            USER2,
            USER3,
            USER4,
            USER5,
            USER6,
            USER7,
            USER8,
            USER9,
            USER10,
            
            MAP = 50,   ///< Static until map exited (may still be freed during the map, though).
            MAP_STATIC = 52, ///< Not freed until map exited.

            // Tags >= 100 are purgable whenever needed.
            PURGE_LEVEL = 100,
            CACHE = 101
        };
        
    public:
        Zone();
        ~Zone();

        /**
         * Enables or disables fast malloc mode. Enable for added performance during
         * map setup. Disable fast mode during other times to save memory and reduce
         * fragmentation.
         *
         * @param enabled  true or false.
         */
        void enableFastMalloc(bool enabled = true);
        
        /**
         * Allocates a block of memory.
         * 
         * @param size  Size of the block to allocate.        
         * @param tag   Purge tag. Indicates when the block can be freed.
         * @param user  You can a NULL user if the tag is < PURGE_LEVEL.
         */
        void* allocate(dsize size, PurgeTag tag = STATIC, void* user = 0);

        /**
         * Allocates and clears a block of memory.
         * 
         * @param size  Size of the block to allocate.        
         * @param tag   Purge tag. Indicates when the block can be freed.
         * @param user  You can a NULL user if the tag is < PURGE_LEVEL.
         */
        void* allocateClear(dsize size, PurgeTag tag = STATIC, void* user = 0);

        /**
         * Resizes a block of memory.
         * Only resizes blocks with no user. If a block with a user is
         * reallocated, the user will lose its current block and be set to
         * NULL. Does not change the tag of existing blocks.
         *
         * @param ptr  Pointer to memory allocated from the zone.
         * @param newSize  New size for memory block.
         * @param tagForNewAlloc  Tag used when making a completely new allocation.
         */
        void* resize(void* ptr, dsize newSize, PurgeTag tagForNewAlloc = STATIC);

        /**
         * Resizes a block of memory so that any new allocated memory is filled with zeroes.
         *
         * @param ptr  Pointer to memory allocated from the zone.
         * @param newSize  New size for memory block.
         * @param tagForNewAlloc  Tag used when making a completely new allocation.
         */
        void* resizeClear(void* ptr, dsize newSize, PurgeTag tagForNewAlloc = STATIC);

        /**
         * Free memory that was allocated with allocate().
         *
         * @param ptr  Pointer to memory allocated from the zone.
         */
        void free(void *ptr);

        /**
         * Free memory blocks in all volumes with a tag in the specified range.
         */
        void freeTags(PurgeTag lowTag, PurgeTag highTag = CACHE);

        /**
         * Sets the tag of a memory block.
         *
         * @param ptr  Pointer to memory allocated from the zone.
         */
        void setTag(void* ptr, PurgeTag tag);

        /**
         * Get the user of a memory block.
         *
         * @param ptr  Pointer to memory allocated from the zone.
         */
        void* getUser(void* ptr) const;
        
        /**
         * Sets the user of a memory block.
         *
         * @param ptr  Pointer to memory allocated from the zone.
         */
        void setUser(void* ptr, void* newUser);
        
        /**
         * Get the tag of a memory block.
         */
        PurgeTag getTag(void* ptr) const;
        
        /**
         * Calculate the amount of unused memory in all volumes combined.
         */
        dsize availableMemory() const;
        
        /**
         * Check all zone volumes for consistency.
         */
        void verify() const;
        
        /**
         * Constructs a new batch allocator. When the allocator is deleted, all
         * the memory blocks allocated by it are automatically freed.
         * The zone retains ownership of the allocator. Batches are deleted automatically
         * based on their purge tags.
         *
         * @see Zone::Batch
         * @see deleteBatch()
         *
         * @return  Reference to the allocator. 
         */
        Batch& newBatch(dsize sizeOfElement, duint batchSize, PurgeTag tag = STATIC);
        
        /**
         * Deletes a batch owned by the zone. Note that batches are freed automatically
         * according to the purge tags.
         */
        void deleteBatch(Batch& batch);

    public:
        /**
         * The Batch is an allocator utility that efficiently allocates a large
         * number of memory blocks. It should be used in performance-critical places
         * instead of separate calls to allocate().
         */
        class PUBLIC_API Batch
        {
        public:
            /**
             * Constructs a new block memory allocator. These are used instead of many
             * calls to Zone::allocate() when the number of required elements is unknown and
             * when linear allocation would be too slow.
             *
             * Memory is allocated as needed in blocks of @c batchSize elements. When
             * a new element is required we simply reserve a pointer in the previously
             * allocated block of elements or create a new block just in time.
             *
             * @param zone  Memory zone for the allocations.
             * @param sizeOfElement Required size of each element.
             * @param batchSize     Number of elements in each zblock of the set.
             *
             * @return  Pointer to the newly created batch.
             */
            Batch(Zone& zone, dsize sizeOfElement, duint batchSize, PurgeTag tag = STATIC);

            /**
             * Frees the entire batch.
             * All memory allocated is released for all elements in all blocks.
             */
            ~Batch();

            /**
             * Allocates a new memory block within the batch.
             *
             * @return  Pointer to memory block. Do not call Zone::free() on this.
             */
            void* allocate();
            
            /**
             * Returns the purge tag used by the allocator.
             */
            PurgeTag tag() const { return tag_; }

        private:
            /**
             * Allocate a new block of memory to be used for linear object allocations.
             */
            void addBlock();

        private:
            Zone& zone_;

            duint elementsPerBlock_;

            dsize elementSize_;

            /// All blocks in a blockset have the same tag.
            PurgeTag tag_; 

            duint count_;
            
            struct ZBlock;
            ZBlock* blocks_;
        };

    protected:
        class MemZone;
        class MemVolume;
        class MemBlock;
        class BlockSet;

        /**
         * Gets the memory block header of an allocated memory block.
         * 
         * @param ptr  Pointer to memory allocated from the zone.
         *
         * @return  Block header.
         */
        MemBlock* getBlock(void* ptr) const;
        
        /**
         * Create a new memory volume.  The new volume is added to the list of
         * memory volumes.
         *
         * @return  Pointer to the new volume.
         */
        MemVolume* newVolume(dsize volumeSize);

    private:
        MemVolume* volumeRoot_;

        /**
         * If false, Z_Malloc will free purgable blocks and aggressively look for
         * free memory blocks inside each memory volume before creating new volumes.
         * This leads to slower mallocing performance, but reduces memory fragmentation
         * as free and purgable blocks are utilized within the volumes. Fast mode is
         * enabled during map setup because a large number of mallocs will occur
         * during setup.
         */
        bool fastMalloc_;
        
        typedef std::list<Batch> Batches;
        Batches batches_;
    };
    
}

#endif /* LIBDENG2_ZONE_H */
