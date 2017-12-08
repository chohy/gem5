#ifndef __MEM_CACHE_TAGS_GLOBALTAGTABLE_HH__
#define __MEM_CACHE_TAGS_GLOBALTAGTABLE_HH__

#include <cassert>
#include <string>
#include <vector>

#include "base/callback.hh"
#include "base/random.hh"
#include "base/statistics.hh"
#include "debug/ChoTag.hh"
#include "debug/ChoTagEvent.hh"
#include "debug/SubarrayDist.hh"
#include "mem/cache/tags/base.hh"
#include "mem/cache/tags/subarray.hh"
#include "mem/cache/tags/tagtable.hh"
#include "mem/cache/base.hh"
#include "mem/cache/blk.hh"
#include "mem/packet.hh"
#include "params/GlobalTagTable.hh"
#include "sim/clocked_object.hh"

class GlobalTagTable : public ClockedObject
{
  public:
    /** Typedef the block type used in this tag store. */
    typedef CacheBlk BlkType;
    /** Typedef for the sub array type used in this tag store. */
    typedef SubArray<BlkType> SubArrayType;
    /**Typedef for the tag table entry. */
    typedef TableEntry<Addr, SubArrayType> EntryType;
    /** Typedef for the tag table type used in this tag store. */
    typedef TagTable<Addr, SubArrayType> TableType;

  protected:
    /** The block size of the cache. */
    const unsigned blkSize;
    /** The size of the cache */
    const unsigned size;
    /** Pointer to the parent cache. */
    BaseCache *cache;

    /** The number of the global tag table entries. */
    const unsigned numTagTableEntries;
    /** The number of blocks in the cache. */
    unsigned numBlocks;
    /** The number of delta bits */
    const int numDeltaBits;
    /** The number of sub arrays */
    const unsigned numSubArrays;
    /** The number of blocks in the sub array */
    const int subArraySize;

    /** The access latency of the global tag table. */
    const Cycles sharedTagAccessLatency;
    /** The access latency of the delta arrays. */
    const Cycles deltaAccessLatency;
    /** The access latency of the data arrays. */
    const Cycles dataAccessLatency;

    /**
     * The number of tags that need to be touched to meet the warmup
     * percentage.
     */
    int warmupBound;
    /** Marked true when the cache is warmed up. */
    bool warmedUp;

    /**The global tag table. */
    TableType *table;
    /**The sub arrays. */
    SubArrayType *subArrays;
    /** The cache blocks. */
    BlkType *blks;
    /** The data blocks, 1 per cache block. */
    uint8_t *dataBlks;

    /** The amount to shift the address to get the set. */
    int indexShift;
    /** The amount to shift the address to get the tag. */
    int tagShift;
    /** The amount to shift the address to get the shared tag. */
    int sharedTagShift;
    /** Mask out all bits that aren't part of the index. */
    unsigned indexMask;
    /** Mask out all bits that aren't part of the block offest. */
    unsigned blkMask;

    //Statistics
    /**
     * @addtogroup CacheStatistics
     */

    /** Number of replacements of valid blocks per thread. */
    Stats::Vector replacements;
    /** Per cycle average of the number of tags that hold valid data. */
    Stats::Average tagsInUse;

    /** The total numer of references to a block before it is replaced. */
    Stats::Scalar totalRefs;

    /**
     * The number of reference counts sampled. This is different from
     * replacements because we sample all the valid blocks
     * when the simulator exits.
     */
    Stats::Scalar sampledRefs;

    /**
     * Average number of references to a block before is was replaced.
     * @todo This should change to an average stat once we have them.
     */
    Stats::Formula avgRefs;

    /** The cycle that the warmup percentage was hit. */
    Stats::Scalar warmupCycle;

    /** Average occupancy of each requestor using the cache */
    Stats::AverageVector occupancies;

    /** Average occ % of each requestor using the cache */
    Stats::Formula avgOccs;

    /** Occupancy of each context/cpu using the cache */
    Stats::Vector occupanciesTaskId;

    /** Occupancy of each context/cpu using the cache */
    Stats::Vector2d ageTaskId;

    /** Occ % of each context/cpu using the cache */
    Stats::Formula percentOccsTaskId;

    /** Number of tag table entries consulted over all accesses. */
    Stats::Scalar tableEntryAccesses;
    /** Number of deltas consulted over all accesses. */
    Stats::Scalar deltaAccesses;
    /** Number of data blocks consulted over all accesses. */
    Stats::Scalar dataAccesses;

    /** Number of subarrays of entry. */
    Stats::Vector entrySubarrayCnt;
    /** Number of entry accesses of entry. */
    Stats::Vector entryAccessCnt;
    /** Number of block replacements of entry. */
    Stats::Vector entryReplacementCnt;

    /**
     * @}
     */

  public:
    /** Convenience typedef. */
    typedef GlobalTagTableParams Params;

    /** 
     * Construc and initialize this tag store.
     */
    GlobalTagTable(const Params *p);

    /**
     * Destructor
     */
    ~GlobalTagTable(){};

	/** 
	 * Set the parent cache back pointer.
	 * @param _cache Poiter to parent cache.
	 */
	void setCache(BaseCache *_cache);

	/**
	 * Register local statistics. 
	 */
	void regStats();

	/**
	 * Average in the reference count for valid blocks when the simulation
	 * exits.
	 */
	void cleanupRefs();

	/**
	 * Computes stats just prior to dump event
	 */
	void computeStats();

	/**
	 *iterated through all blocks and clear all locks
	 *Needed to clear all lock tracking at once
	 */
	void clearLocks();

	/**
	 * Print all tags used
	 */
	std::string print() const;

    /**
     * Visit each block in the tag store and apply a visitor to the
     * block.
     *
     * The visitor shuld be a function (or object that behaves like a
     * function) that takes a cache block reference as its parameter
     * and returns a bool. A bvisitor can request the traversal to be
     * stopped by returning false, returning true causes it to be
     * called for the next block in the tag stroe.
     *
     * \param visitor Visitor to call on each block.
     */
    void forEachBlk(CacheBlkVisitor &visitor) {
        for (unsigned i = 0; i < numSubArrays*subArraySize; ++i){
            if (!visitor(blks[i]))
                return;
        }
    }

    /**
     * Return the block size.
     * @return the block size.
     */
    unsigned
        getBlockSize() const
    {
        return blkSize;
    }


	int
	getSubArraySize() const
	{
		return subArraySize;
	}

	/**
	 * Invalidate the given blck
	 * @param blk The block to invalidate.
	 */
    void invalidateBlk(CacheBlk *blk)
    {
        assert(blk);
        assert(blk->isValid());
        tagsInUse--;
        assert(blk->srcMasterId < cache->system->maxMasters());
        occupancies[blk->srcMasterId]--;
        blk->srcMasterId = Request::invldMasterId;
        blk->task_id = ContextSwitchTaskId::Unknown;
        blk->tickInserted = curTick();
    }

	/**
	 * Invalidate the given global tag table entry.
	 * When entry is invalidated, the allocated sub arrays are
	 * invalidated, too.
	 * @param entry The gloal tag talble entry to invalidate.
	 */
    void invalidateEntry(EntryType *entry)
    {
        assert(entry);
        assert(entry->valid_bit);

		std::vector<SubArrayType*>::iterator it;
		while (!(entry->subArray.empty())) {
			it = entry->subArray.begin();
			(*it)->entry = NULL;
			entry->subArray.erase(it);
		}

        entry->valid_bit = false;
        entry->accessCnt = 0;
        entry->replacementCnt = 0;
        for (int i = 0; i < subArraySize; i++) {
            entry->MRUArray[0][i] = 0;
            entry->MRUArray[1][i] = 0;
        }
    }

	/**
	 * Access block and update replacement data. May not succeed, in which case
	 * NULL pointer is returned. This has all the implications of a cache
	 * access and should only be used as such. Return the access latency as a
	 * side effect.
	 * @param addr The address to find.
	 * @param is_secure True if the target memory space is secure.
	 * @param lat The access latency.
	 * @param context_src
	 * @return Pointer to the cache block if found.
	 */
    CacheBlk* accessBlock(Addr addr, bool is_secure, Cycles &lat,
        int context_src)
    {
		Addr tag = extractTag(addr);
		Addr index = extractIndex(addr);
		Addr shared_tag = extractSharedTag(addr);
		BlkType *blk = NULL;

		EntryType *entry = table->findEntry(shared_tag);
        tableEntryAccesses += numTagTableEntries;

		if (entry == NULL) {
			//Maybe here need stats update.
			lat = sharedTagAccessLatency;
			return blk;
		}
        entry->accessCnt += 1;
        deltaAccesses += entry->subArray.size();

        lat += deltaAccessLatency;
		std::vector<SubArrayType*>::iterator it;
		for (it = entry->subArray.begin(); it < entry->subArray.end(); it++) {
			blk = (*it)->findBlk(index, tag, is_secure);

			if (blk) {
                if (blk->whenReady > curTick()
                    && cache->ticksToCycles(blk->whenReady - curTick())
                    > (lat + dataAccessLatency))
                    lat = cache->ticksToCycles(blk->whenReady - curTick());
                else lat += dataAccessLatency;

                //MRU block update
                int position = std::distance(entry->subArray.begin(), it);
                if (position != entry->MRUArray[0][index]) {
                    entry->MRUArray[1][index] = entry->MRUArray[0][index];
                    entry->MRUArray[0][index] = position;
                }
                
                blk->refCount += 1;
                dataAccesses += 1;

                return blk;
			}
		}

		return NULL;
    }

	/**
	 * Finds the given address in the cache, do not update replacement data.
	 * i.e. This is a no-side-effect find of a block.
	 * @param addr The address to find.
	 * @param is_secrue Ture if the target memory space is secure.
	 * @return Pointer to the cache block if found.
	 */
	CacheBlk* findBlock(Addr addr, bool is_secure) const
	{
		Addr tag = extractTag(addr);
		Addr index = extractIndex(addr);
		BlkType *blk = NULL;

		EntryType* entry = table->findEntry(extractSharedTag(addr));
		if (entry == NULL)
			return NULL;

		std::vector<SubArrayType*>::iterator it;
		for (it = entry->subArray.begin(); it < entry->subArray.end(); it++) {
			blk = (*it)->findBlk(index, tag, is_secure);
			if (blk)
				return blk;
		}

		return NULL;
	}

	/**
	* Finds the tag table entry with given address in the cache, do not update
	* replacement data.
	* i.e. This is a no-side-effect find of a block.
	* @param addr The address to find.
	* @param is_secrue Ture if the target memory space is secure.
	* @return Pointer to the cache block if found.
	*/
	EntryType* findEntry(Addr addr) const
	{
		return table->findEntry(extractSharedTag(addr));
	}

	/**
	 * Find an invalid block to evict for the address provided.
	 * If there are no invalid blocks, this will return the block
	 * in two case.
	 * If the tag table entry has less than or equal to 2 sub arrays,
	 * this will return the least-recently used position.
	 * Otherwise, this will return the block randomly
	 * except two most recently used blocks.
	 * @param addr The addr to a find a replacement candidate for.
	 * @return The candidate block.
	 */
	CacheBlk* findVictimBlk(Addr addr)
	{
		BlkType *blk = NULL;
		SubArrayType *subarray = NULL;
		EntryType *entry = table->findEntry(extractSharedTag(addr));
		assert(entry != NULL);

		int way = entry->subArray.size();
        Addr index = extractIndex(addr);
        assert(way > 0);

        for (int i = 0; i < way; i++) {
            subarray = entry->subArray.at(i);
            blk = subarray->blks[index];
            if (!blk->isValid())
                return blk;
        }

		if (way == 1) {
			subarray = entry->subArray.at(0);
			blk = subarray->blks[index];
			return blk;
		}
		else if (way == 2) {
			subarray = entry->subArray.at(entry->MRUArray[1][index]);
            blk = subarray->blks[index];
			return blk;
		}
		else {
            int idx;
            do idx = random_mt.random<int>(0, way - 1);
            while (idx != entry->MRUArray[0][index] && idx != entry->MRUArray[1][index]);

			assert((idx-1) < way);
			//assert(idx > 1);
			subarray = entry->subArray.at(idx);
			blk = subarray->blks[index];
			return blk;
		}
	}

	/**
	 * Find an invalid tag table entry.
	 * If there are no invalid entry, this will return the entry
	 * which is a least accessed entry.
	 * @return The candidate entry.
	 */
	EntryType* findVictimEntry()
	{
		// Find a invalid entry.
		for (int i = 0; i < numTagTableEntries; i++){
			if (table->entries[i].valid_bit == false)
				return &(table->entries[i]);
		}

		EntryType *entry = &(table->entries[0]);
		for (int i = 1; i < numTagTableEntries; i++){
			if (entry->accessCnt > table->entries[i].accessCnt)
				entry = &(table->entries[i]);
		}

		return entry;
	}

	/**
	 * Insert the new block into the cache.
	 * @param pkt Packet holding the address to update.
	 * @param blk The block to update.
	 */
	void insertBlock(PacketPtr pkt, CacheBlk *blk)
	{
		Addr addr = pkt->getAddr();
		MasterID master_id = pkt->req->masterId();
		uint32_t task_id = pkt->req->taskId();

		if (!blk->isTouched) {
			tagsInUse++;
			blk->isTouched = true;
			if (!warmedUp && tagsInUse.value() >= warmupBound) {
				warmedUp = true;
				warmupCycle = curTick();
			}
		}

        EntryType *entry = findEntry(pkt->getAddr());
		// If we're replacing a block that was previously valid update
		// stats for it. This can't be done in findBlock() because a
		// found block might not acually be replaced there if the
		// coherence protocol says it can't be.
		if (blk->isValid()) {
			replacements[0]++;
			totalRefs += blk->refCount;
			++sampledRefs;
			blk->refCount = 0;

			// deal with evicted block
			assert(blk->srcMasterId < cache->system->maxMasters());
			occupancies[blk->srcMasterId]--;

			blk->invalidate();

			entry->replacementCnt += 1;
		}

		blk->isTouched = true;

		// Set tag for new block. Caller is responsible for setting status.
		blk->tag = extractTag(addr);

		// deal with what we are bringing in
		assert(master_id < cache->system->maxMasters());
		occupancies[master_id]++;
		blk->srcMasterId = master_id;
		blk->task_id = task_id;
		blk->tickInserted = curTick();

		// We only need to write into one tag and one data block.
		deltaAccesses += 1;
		dataAccesses += 1;

        //MRU block update
        Addr index = extractIndex(addr);
        int position = 0;
        for (int i = 0; i < entry->subArray.size(); i++) {
            if (entry->subArray.at(i)->findBlk(index, extractTag(addr),
                pkt->isSecure())) {
                position = i;
                break;
            }
        }
        if (position != entry->MRUArray[0][index]) {
            entry->MRUArray[1][index] = entry->MRUArray[0][index];
            entry->MRUArray[0][index] = position;
        }
	}

	/**
	* Insert the new global tag table entry.
	* @param addr The address to update.
	* @param entry The entry to update.
	*/
	void insertEntry(Addr addr, EntryType *entry)
	{
		assert(!table->findEntry(extractSharedTag(addr)));
		assert(entry->valid_bit == false);

        tableEntryAccesses += 1;

		entry->valid_bit = true;

		// Set shared tag for new entry. Caller is responsible for setting status.
		entry->shared_tag = extractSharedTag(addr);
	}

	/**
	 * Allocate new sub array to the tag entry.
	 * @param entry The entry to be allocated.
	 * @return true if the allocation is successed.
	 */
	bool allocateSubArray(EntryType *entry) {
		int valid_entry_cnt = 0;
		int valid_subarray_cnt = 0;

		for (int i = 0; i < numTagTableEntries; i++) {
			if (table->entries[i].valid_bit == true) valid_entry_cnt++;
		}

		for (int i = 0; i < numSubArrays; i++) {
			if (subArrays[i].entry != NULL) valid_subarray_cnt++;
		}

		if ((numSubArrays - valid_subarray_cnt) <= (numTagTableEntries - valid_entry_cnt))
			return false;
		
		for (int i = 0; i < numSubArrays; i++) {
			if (subArrays[i].entry == NULL) {
				entry->subArray.push_back(&subArrays[i]);
				subArrays[i].entry = entry;
                
                DPRINTF(ChoTag, "Entry %s: %s sub arrays, Remain: %s\n",
                    table->getEntryIdx(entry), entry->subArray.size(),
                    numSubArrays-table->getAllocatedSubarrayNum());
				return true;
			}
		}
		//There is no availabe sub array.
		return false;
	}

	/**
	 * Generate the tag from the given address.
	 * @param addr The address to get the tag from.
	 * @return The tag of the address.
	 */
	Addr extractTag(Addr addr) const
	{
		return (addr >> tagShift);
	}

    /**
     * Calculate the block offset of an address.
     * @param addr the address to get the offset of.
     * @return the block offset.
     */
    int extractBlkOffset(Addr addr) const
    {
        return (addr & (Addr)(blkSize-1));
    }

        /**
	* Generate the index from the given addrss.
	* @param addr The address to get the tag from.
	* @return The index of the addrss.
	*/
	Addr extractIndex(Addr addr) const
	{
		return ((addr >> indexShift) & indexMask);
	}
	/**
	 * Generate the shared tag from the given addrss.
	 * @param addr The address to get the tag from.
	 * @return The shared tag of the addrss.
	 */
	Addr extractSharedTag(Addr addr) const
	{
		return addr >> sharedTagShift;
	}

	/**
	 * Align an address to the block size.
	 * @param addr the address to align.
	 * @return The block address.
	 */
	Addr blkAlign(Addr addr) const
	{
		return (addr & ~(Addr)blkMask);
	}

	/**
	 * Regenerate the block address from the tag.
	 * @param tag The tag of the block.
	 * @param index The index of the block.
	 * @return The block address.
	 */
	Addr regenerateBlkAddr(Addr tag, unsigned index) const
	{
		return ((tag << tagShift) | ((Addr)index << indexShift));
	}

    class CntResetEvent : public Event
    {
      private:
        GlobalTagTable *gtt;

      public:
        CntResetEvent(GlobalTagTable *_gtt)
            : Event(Default_Pri, AutoDelete), gtt(_gtt)
        {}

        /** Process */
        void process()
        {
            for (int i = 0; i < gtt->numTagTableEntries; i++) {
                DPRINTF(SubarrayDist,
                    "Entry %d: %d subarrays, %d accesses, %d replacements\n", i,
                    gtt->table->entries[i].subArray.size(),
                    gtt->table->entries[i].accessCnt,
                    gtt->table->entries[i].replacementCnt);

                gtt->entrySubarrayCnt[i] = gtt->table->entries[i].subArray.size();
                gtt->entryAccessCnt[i] = gtt->table->entries[i].accessCnt;
                gtt->entryReplacementCnt[i] = gtt->table->entries[i].replacementCnt;
                
                gtt->table->entries[i].accessCnt = 0;
                gtt->table->entries[i].replacementCnt = 0;
            }

            /*
            for (int i = 0; i < gtt->numTagTableEntries; i++) {
                gtt->table->entries[i].accessCnt = 0;
                gtt->table->entries[i].replacementCnt = 0;
            }
            */

            DPRINTF(ChoTagEvent, "cntResetEvent occured\n");
            
            
            gtt->schedule(gtt->cntResetEvent, curTick() + 100000000);
        }
    };

    CntResetEvent cntResetEvent;
};

class GlobalTagTableCallback : public Callback
{
	GlobalTagTable *tags;
  public:
	GlobalTagTableCallback(GlobalTagTable *t) : tags(t) {}
	void process() { tags->cleanupRefs(); };
};

class GlobalTagTableDumpCallback : public Callback
{
	GlobalTagTable *tags;
public:
	GlobalTagTableDumpCallback(GlobalTagTable *t) : tags(t) {}
	void process() { tags->computeStats(); };
};

#endif // __MEM_CACHE_TAGS_GLOBALTAGTABLE_HH__
