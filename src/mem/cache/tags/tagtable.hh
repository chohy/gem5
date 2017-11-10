/**
 * Author: Haeyoon Cho
 */

#ifndef __TAGTABLE_HH__
#define __TAGTABLE_HH__

#include <cassert>
#include <vector>

template <typename TagType, typename SubArrayType>
class TableEntry {
public:
    bool valid_bit;
    TagType shared_tag;
    std::vector<SubArrayType*> subArray;
    int *MRUArray[2];
    unsigned subArraySize;
	//unsigned subArrayMask;
    unsigned accessCnt;
    unsigned replacementCnt;

    TableEntry() {};
    ~TableEntry() {};

    void Initialize(unsigned _subArraySize)
    {
        valid_bit = false;
        //subArrayMask = 0;
        accessCnt = 0;
        replacementCnt = 0;
        subArraySize = _subArraySize;
        MRUArray[0] = new int[subArraySize];
        MRUArray[1] = new int[subArraySize];

        //initiate MRUArray.
        for (int i = 0; i < subArraySize; i++) {
            MRUArray[0][i] = 0;
            MRUArray[1][i] = 0;
        }
    };

};

/**
 * It imitates "mem/cache/tags/cacheset.hh"
 * as a tag table.
 */
template <typename TagType, typename SubArrayType>
class TagTable {
  public:
    unsigned numTagTableEntries;

    TableEntry<TagType, SubArrayType> *entries;

  public:
	/** Construct and initialize this tag table. */
	TagTable(int numEntries, unsigned subArraySize) : numTagTableEntries(numEntries)
	{
		entries = new TableEntry<TagType, SubArrayType>[numTagTableEntries];
        for (int i = 0; i < numTagTableEntries; i++)
            entries[i].Initialize(subArraySize);
	}
	/** Destructor */
	~TagTable()
	{
		delete[] entries;
	}

    TableEntry<TagType, SubArrayType>* findEntry(TagType shared_tag) const;
};

template <typename TagType, typename SubArrayType>
TableEntry<TagType, SubArrayType>*
TagTable<TagType, SubArrayType>::findEntry(TagType shared_tag) const
{
    for (unsigned i = 0; i < numTagTableEntries; i++) {
        //cout<<shared_tag<<", "<<entries[i].shared_tag<<endl;
        if (shared_tag == entries[i].shared_tag && entries[i].valid_bit == true)
            return &entries[i];
    }
    return NULL;
}
#endif // __TAGTABLE_HH__
