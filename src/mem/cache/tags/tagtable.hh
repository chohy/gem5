/**
 * Author: Haeyoon Cho
 */

#ifndef __TAGTABLE_HH__
#define __TAGTABLE_HH__

#include <cassert>

template <typename TagType, typename SubArrayType>
class tableEntry {
public:
    bool valid_bit;
    TagType shared_tag;
    SubArrayType *subArray;
    unsigned accessCnt;
    unsigned replacementCnt;

    tableEntry()
    {
        valid_bit = false;
        ref = 0;
        accessCnt = 0;
        replacementCnt = 0;
    }
    ~tableEntry() {};

    const tableEntry<TagType, SubArrayType>& operator=(const tableEntry<TagType, subArrayType>& rhs)
    {
        valid_bit = rhs.valid_bit;
        shared_tag = rhs.shared_tag;
        subArray = rhs.subArray;
        accessCnt = rhs.accessCnt;
        replacementCnt = rhs.repacementCnt;
        return *this;
    }
};

/**
 * It is imitated "mem/cache/tags/cacheset.hh"
 * as a tag table.
 */
template <typename TagType, typename SubArrayType>
class TagTable {
  public:
    unsigned numTagTableEntry;

    tableEntry<TagType, SubArrayType> *entries;

  public:
    tableEntry<TagType, SubArrayType>* findEntry(TagType shared_tag) const;
    void moveToHead(tableEntry<TagType, SubArrayType> *entry);
    void moveToTail(tableEntry<TagType, SubArrayType> *entry);
    void sortTable();
};

template <typename TagType, typename SubArrayType>
tableEntry<TagType, SubArrayType>*
TagTable<TagType, SubArrayType>::findEntry(TagType shared_tag) const
{
    for (unsigned i = 0; i < numTagTableEntry; i++) {
        cout<<shared_tag<<", "<<entries[i].shared_tag<<endl;
        if (shared_tag == entries[i].shared_tag && entries[i].valid_bit == true)
            return &entries[i];
    }
    return NULL;
}

template <typename TagType, typename SubArrayType>
void
TagTable<TagType, SubArrayType>::moveToHead(tableEntry<TagType, subArrayType> *entry)
{
    if (entries[0] == entry)
        return;

    unsigned i=0;
    tableEntry<TagType, SubArrayType> *next = entry;

    do {
        assert(i < numTagTableEntry);
        tableEntry<TagType, SubArrayType> *tmp = entries[i];
        entries[i] = next;
        next = tmp;
        ++i;
    } while (next != entry);
}

template <typename TagType, typename SubArrayType>
void
TagTable<TagType, SubArrayType>::moveToTail(tableEntry<TagType, subArrayType> *entry)
{
    if (entries[numTagTableEntry-1] == entry)
        return;

    unsigned i = numTagTableEntry - 1;
    tableEntry<TagType, SubArrayType> *next = entry;

    do {
        assert(i < numTagTableEntry);
        tableEntry<TagType, SubArrayType> *tmp = entries[i];
        entries[i] = next;
        next = tmp;
        --i;
    } while (next != entry);
}

template <typename TagType, typename SubArrayType>
void
TagTable<TagType, SubArrayType>::sortTable()
{
    for (unsigned i=1; i < numTagTableEntry; i++) {
        for (unsigned j = 0; j < i; j++) {
            if (entries[i].accessCnt > entries[j].accessCnt) {
                tableEntry<TagType, SubArrayType> tmp = entries[j];
                entries[j] = entries[i];
                entries[i] = tmp;
            }
        }
    }
}
#endif // __TAGTABLE_HH__
