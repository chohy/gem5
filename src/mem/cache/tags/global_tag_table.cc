/**
 * Author: Haeyoon Cho
 */

#include "base/intmath.hh"
#include "cpu/smt.hh" // maxThreadsPerCPU
#include "mem/cache/tags/global_tag_table.hh"
#include "mem/cache/base.hh"

GlobalTagTable::GlobalTagTable(const Params *p)
	:ClockedObject(p), blkSize(p->block_size), size(p->size),
	numTagTableEntries(p->num_tag_table_entries),
	numDeltaBits(p->num_delta_bits),
	numSubArrays(p->num_subarray),
        subArraySize(p->size/p->num_subarray),
        sharedTagAccessLatency(p->shared_tag_access_latency),
	deltaAccessLatency(p->delta_access_latency),
	dataAccessLatency(p->data_access_latency)
{
	// Check parameters
	if (blkSize < 4 || !isPowerOf2(blkSize)) {
		fatal("Block size must be at least 4 and a pwoer of 2");
	}
	if (numTagTableEntries > numSubArrays) {
		fatal("# of sub arrays must be greater than or equal to\
			   # of global tag table entries or more");
	}
	if (numDeltaBits <= 0 ) {
		fatal("# of delta bits must be non-zero");
	}
	if (subArraySize <= 0 || !isPowerOf2(subArraySize)) {
		fatal("The sub array size must be non-zero and a power of 2");
	}

	blkMask = blkSize - 1;
	indexShift = floorLog2(blkSize);
	indexMask = subArraySize - 1;
	tagShift = indexShift + floorLog2(subArraySize);
	sharedTagShift = tagShift + numDeltaBits;

	/** @todo Make warmup percentag a parameter. */
	warmupBound = numSubArrays * subArraySize;

	table = new TableType(numTagTableEntries);
	subArrays = new SubArrayType[numSubArrays];
	blks = new BlkType[numSubArrays * subArraySize];
	// allocate data storage in one bing chunk.
	numBlocks = numSubArrays * subArraySize;
	dataBlks = new uint8_t[numBlocks * blkSize];

	unsigned blkIndex = 0;    // index into blks array.
	for (unsigned i = 0; i < numSubArrays; i++)
	{
		subArrays[i].blks = new BlkType*[subArraySize];

		// link in the data blocks
		for (unsigned j = 0; j < subArraySize; j++)
		{
			BlkType *blk = &blks[blkIndex];
			blk->data = &dataBlks[blkSize*blkIndex];
			blkIndex++;

			// invalidate new cache block
			blk->invalidate();

			blk->tag = j;
			blk->whenReady = 0;
			blk->isTouched = false;
			blk->size = blkSize;
			subArrays[i].blks[j] = blk;
			blk->set = j;
		}
	}
}

void
GlobalTagTable::setCache(BaseCache *_cache)
{
	assert(!cache);
	cache = _cache;
}

void
GlobalTagTable::regStats()
{
	using namespace Stats;
	replacements
		.init(maxThreadsPerCPU)
		.name(name() + ".replacements")
		.desc("number of replacements")
		.flags(total)
		;

	tagsInUse
		.name(name() + ".tagsinuse")
		.desc("Cycle average of tags in use")
		;

	totalRefs
		.name(name() + ".total_refs")
		.desc("Total number of references to valid blocks.")
		;

	sampledRefs
		.name(name() + ".sampled_refs")
		.desc("Sample count of references to valid blocks.")
		;

	avgRefs
		.name(name() + ".avg_refs")
		.desc("Average number of references to valid blocks.")
		;

	avgRefs = totalRefs / sampledRefs;

	warmupCycle
		.name(name() + ".warmup_cycle")
		.desc("Cycle when the warmup percentage was hit.")
		;

	occupancies
		.init(cache->system->maxMasters())
		.name(name() + ".occ_blocks")
		.desc("Average occupied blocks per requestor")
		.flags(nozero | nonan)
		;
	for (int i = 0; i < cache->system->maxMasters(); i++) {
		occupancies.subname(i, cache->system->getMasterName(i));
	}

	avgOccs
		.name(name() + ".occ_percent")
		.desc("Average percentage of cache occupancy")
		.flags(nozero | total)
		;
	for (int i = 0; i < cache->system->maxMasters(); i++) {
		avgOccs.subname(i, cache->system->getMasterName(i));
	}

	avgOccs = occupancies / Stats::constant(numBlocks);

	occupanciesTaskId
		.init(ContextSwitchTaskId::NumTaskId)
		.name(name() + ".occ_task_id_blocks")
		.desc("Occupied blocks per task id")
		.flags(nozero | nonan)
		;

	ageTaskId
		.init(ContextSwitchTaskId::NumTaskId, 5)
		.name(name() + ".age_task_id_blocks")
		.desc("Occupied blocks per task id")
		.flags(nozero | nonan)
		;

	percentOccsTaskId
		.name(name() + ".occ_task_id_percent")
		.desc("Percentage of cache occupancy per task id")
		.flags(nozero)
		;

	percentOccsTaskId = occupanciesTaskId / Stats::constant(numBlocks);

	tableEntryAccesses
		.name(name() + ".table_entry_accesses")
		.desc("Number of tag table entry accessess")
		;

	tagAccesses
		.name(name() + ".tag_accesses")
		.desc("Number of tag accesses")
		;

	dataAccesses
		.name(name() + ".data_accesses")
		.desc("Number of data accesses")
		;

	registerDumpCallback(new GlobalTagTableDumpCallback(this));
	registerExitCallback(new GlobalTagTableCallback(this));
}

void
GlobalTagTable::cleanupRefs()
{
	for (unsigned i = 0; i < numBlocks; i++){
		if (blks[i].isValid()){
			totalRefs += blks[i].refCount;
			++sampledRefs;
		}
	}
}

void
GlobalTagTable::computeStats()
{
	for (unsigned i = 0; i < ContextSwitchTaskId::NumTaskId; ++i) {
		occupanciesTaskId[i] = 0;
		for (unsigned j = 0; j < 5; ++j) {
			ageTaskId[i][j] = 0;
		}
	}

	for (unsigned i = 0; i < numBlocks; ++i) {
		if (blks[i].isValid()) {
			assert(blks[i].task_id < ContextSwitchTaskId::NumTaskId);
			occupanciesTaskId[blks[i].task_id]++;
			assert(blks[i].tickInserted <= curTick());
			Tick age = curTick() - blks[i].tickInserted;

			int age_index;
			if (age / SimClock::Int::us < 10) { // <10us
				age_index = 0;
			}
			else if (age / SimClock::Int::us < 100) { // <100us
				age_index = 1;
			}
			else if (age / SimClock::Int::ms < 1) { // <1ms
				age_index = 2;
			}
			else if (age / SimClock::Int::ms < 10) { // <10ms
				age_index = 3;
			}
			else
				age_index = 4; // >10ms

			ageTaskId[blks[i].task_id][age_index]++;
		}
	}
}

void
GlobalTagTable::clearLocks()
{
	for (int i = 0; i < numBlocks; i++){
		blks[i].clearLoadLocks();
	}
}

std::string
GlobalTagTable::print() const
{
	std::string cache_state;
	for (unsigned i = 0; i < numSubArrays; ++i) {
		// link in the data blocks
		for (unsigned j = 0; j < subArraySize; ++j) {
			BlkType *blk = subArrays[i].blks[j];
			if (blk->isValid())
				cache_state += csprintf("\tset: %d block: %d %s\n", i, j,
				blk->print());
		}
	}
	if (cache_state.empty())
		cache_state = "no valid tags\n";
	return cache_state;
}

GlobalTagTable*
GlobalTagTableParams::create()
{
    return new GlobalTagTable(this);
}
