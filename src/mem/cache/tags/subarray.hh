/**
 * Author: Haeyoon Cho
 */

#ifndef __SUBARRAY_HH__
#define __SUBARRAY_HH__

#include <cassert>

#include "mem/cache/blk.hh"

/**
 * A sub array for the global tag table.
 */
template <class BlkType>
class SubArray
{
  public:
    /** Cache blocks in this sub array. */
    BlkType **blks;

	/**
	 * Find a block matching the tag in this sub array.
	 * @param index The index to fine.
	 * @param tag The tag to fine.
	 * @param is_secure True if the target memory space is secure.
	 * @return Pointer to the block if found.
	 */
	BlkType* findBlk(Addr index, Addr tag, bool is_secure) const;
	void invalidate();
};

template <class BlkType>
BlkType*
SubArray<BlkType>::findBlk(Addr index, Addr tag, bool is_secure) const
{
	if (blks[index]->tag == tag)
		return blks[index];
	else
		return NULL;
}

/*
template <class BlkType>
void
SubArray<BlkType>::invalidate()
{
	int size = sizeof(blks) / sizeof(BlkType*);
	for (int i = 0; i < size; i++){
		if (blks[i]->isValid()){
			occupancies[blk->srcMasterId]--;
			blk->srcMasterId = Request::invldMasterId;
			blk->task_id = ContextSwitchTaskId::Unknown;
			blk->tickInserted = curTick();

			blks[i]->invalidate();
		}

	}
}
*/

#endif //__SUBARRAY_HH__