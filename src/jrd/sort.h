/*
 *	PROGRAM:	JRD Sort
 *	MODULE:		sort.h
 *	DESCRIPTION:	Sort package definitions
 *
 * The contents of this file are subject to the Interbase Public
 * License Version 1.0 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy
 * of the License at http://www.Inprise.com/IPL.html
 *
 * Software distributed under the License is distributed on an
 * "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, either express
 * or implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code was created by Inprise Corporation
 * and its predecessors. Portions created by Inprise Corporation are
 * Copyright (C) Inprise Corporation.
 *
 * All Rights Reserved.
 * Contributor(s): ______________________________________.
 */

#ifndef JRD_SORT_H
#define JRD_SORT_H

#include "../include/fb_blk.h"
#include "../common/DecFloat.h"
#include "../jrd/TempSpace.h"
#include "../jrd/align.h"

namespace Jrd {

// Forward declaration
class Attachment;
class Sort;
class SortOwner;
struct merge_control;

// SORTP is used throughout sort.c as a pointer into arrays of
// longwords(32 bits).
// Use this definition whenever doing pointer arithmetic, as
// ScratchBird variables (eg. scb->m_longs) are in 32 - bit longwords.

typedef ULONG SORTP;

// since the first part of the record contains a back_pointer, whose
// size depends on the platform (ie 16, 32 or 64 bits.).
// This pointer data_type is defined by platform specific sort_ptr_t.

typedef IPTR sort_ptr_t;

#define PREV_RUN_RECORD(record) (((SORTP*) record - m_longs))
#define NEXT_RUN_RECORD(record) (((SORTP*) record + m_longs))

// a macro to goto the key_id part of a particular record.
// Pls. refer to the SR structure in sort.h for an explanation of the record structure.

#define KEYOF(record) ((SORTP*)(((SR*)record)->sr_sort_record.sort_record_key))

// macro to point to the next record for sorting.
// still using m_longs as we cannot do record++ *
#define NEXT_RECORD(record) ((SR*)((SORTP*) record - m_longs))

// structure containing the key and data part of the sort record,
// the back pointer is not written to the disk, this is how the records
// look in the run files.

struct sort_record
{
	ULONG sort_record_key[1];
	/* Sorting key.  Mangled by diddle_key to
	   compare using ULONG word compares (size
	   is rounded upwards if necessary).
	   Min length of 1 ULONG, max indeterminate
	   For current sort, length is stored as
	   m_key_length.
	   Keys are created by BTR_* routines as
	   sequences of ULONG - this representation
	   cannot be easily changed. */

/*  sort_record_data  is here to explain the sort record.
    To get to the data part of a record add scb->m_key_length to a pointer
    pointing to the start of the sort_record_key.

    ULONG       sort_record_data [1];
                                   Data values, not part of key,
                                   that are pumped through sort.
                                   Min length of 0, max indeterminate,
                                   byte data, but starts on ULONG boundary and
                                   rounded up to ULONG size
                                   Sizeof sr_data array would be
                                   (m_longs - m_key_length) * sizeof(ULONG) -
                                                 sizeof(sr_bckptr)
*/

};

const ULONG MAX_SORT_RECORD = 1024 * 1024;	// 1MB

// the record struct actually contains the keyids etc, and the back_pointer
// which points to the sort_record structure.
typedef struct sr
{
	sort_record** sr_bckptr;	// Pointer back to sort list entry
	union {
		sort_record sr_sort_record;
		FB_UINT64 dummy_alignment_force;
	};
} SR;

// m_longs includes the size of sr_bckptr.

/* The sort memory pool is laid out as follows during sorting:

struct sort_memory
{
        struct sr       *records [X1];
        ULONG           empty [X2];
        struct sr       data [X1];
};

We pack items into sort_memory, inserting the first pointer into
records [0], and the first data value into data[X1-1];  Continuing
until we are out of records to sort or memory.
(eg: X1 * (sizeof(struct sr*) + scb->m_longs) + X2 * sizeof(ULONG) == MAX_MEMORY
*/


// skd_dtype

const int SKD_long			= 1;
const int SKD_ulong			= 2;
const int SKD_short			= 3;
const int SKD_ushort		= 4;
const int SKD_text			= 5;
const int SKD_float			= 6;
const int SKD_double		= 7;
const int SKD_quad			= 8;
const int SKD_timestamp		= 9;
const int SKD_bytes			= 10;
const int SKD_varying		= 11;		// non-international
const int SKD_cstring		= 12;		// non-international
const int SKD_sql_time		= 13;
const int SKD_sql_date		= 14;
const int SKD_int64			= 15;
const int SKD_dec64			= 16;
const int SKD_dec128		= 17;
const int SKD_sql_time_tz	= 18;
const int SKD_timestamp_tz	= 19;
const int SKD_int128		= 20;
const int SKD_inet			= 21;
const int SKD_cidr			= 22;
const int SKD_macaddr		= 23;

// skd_flags
const UCHAR SKD_ascending		= 0;	// default initializer
const UCHAR SKD_descending		= 1;
const UCHAR SKD_binary			= 2;
const UCHAR SKD_separate_data	= 4;

// Sort key definition block

struct sort_key_def
{
	UCHAR	skd_dtype;			// Data type
	UCHAR	skd_flags;			// Flags

private:
	USHORT	skd_length;			// Length if string
	ULONG	skd_offset;			// Offset from beginning

public:
	ULONG	skd_vary_offset;	// Offset to varying/cstring length

	USHORT getSkdLength() const { return skd_length; }

	void setSkdLength(UCHAR dtype, USHORT dscLength)
	{
		skd_dtype = dtype;

		switch (dtype)
		{
		case SKD_dec64:
			fb_assert(dscLength == sizeof(ScratchBird::Decimal64));
			skd_length = ScratchBird::Decimal64::getKeyLength();
			break;
		case SKD_dec128:
			fb_assert(dscLength == sizeof(ScratchBird::Decimal128));
			skd_length = ScratchBird::Decimal128::getKeyLength();
			break;
		default:
			skd_length = dscLength;
		}
	}

	ULONG getSkdOffset() const { return skd_offset; }

	void setSkdOffset(const sort_key_def* prev = nullptr, dsc* desc = nullptr)
	{
		skd_offset = 0;
		if (prev)
		{
			skd_offset = prev->skd_offset + prev->skd_length;
#ifndef WORDS_BIGENDIAN
			skd_offset = ROUNDUP(skd_offset, sizeof(SLONG));
#else
			if (desc && desc->dsc_dtype >= dtype_aligned)
				skd_offset = FB_ALIGN(skd_offset, type_alignments[desc->dsc_dtype]);
#endif
		}
	}
};


// Run/merge common block header

struct run_merge_hdr
{
	SSHORT			rmh_type;
	merge_control*	rmh_parent;
};

// rmh_type

const int RMH_TYPE_RUN	= 0;
const int RMH_TYPE_MRG	= 1;
const int RMH_TYPE_SORT = 2;


// Run control block

struct run_control
{
	run_merge_hdr	run_header;
	run_control*	run_next;			// Next (actually last) run
	ULONG			run_records;		// Records (remaining) in run
	USHORT			run_depth;			// Number of "elementary" runs
	FB_UINT64		run_seek;			// Offset in file of run
	FB_UINT64		run_size;			// Length of run in work file
	sort_record*	run_record;			// Next record in run
	UCHAR*			run_buffer;			// Run buffer
	UCHAR*			run_end_buffer;		// End of buffer
	bool			run_buff_alloc;		// Allocated buffer flag
	bool			run_buff_cache;		// run buffer is already in cache
	FB_UINT64		run_mem_seek;		// position of run's buffer in in-memory part of sort file
	ULONG			run_mem_size;		// size of run's buffer in in-memory part of sort file
};

// Merge control block

struct merge_control
{
	run_merge_hdr	mrg_header;
	sort_record*	mrg_record_a;
	run_merge_hdr*	mrg_stream_a;
	sort_record*	mrg_record_b;
	run_merge_hdr*	mrg_stream_b;
};

// Sort control block, for partitioned sort

struct sort_control
{
	run_merge_hdr	srt_header;
	Sort*	srt_sort;
};


// Sort class

typedef bool (*FPTR_REJECT_DUP_CALLBACK)(const UCHAR*, const UCHAR*, void*);

// flags as set in m_flags

const int scb_sorted		= 1;	// stream has been sorted
const int scb_reuse_buffer	= 2;	// reuse buffer if possible

class Sort
{
	friend class PartitionedSort;
public:
	Sort(Database*, SortOwner*,
		 ULONG, FB_SIZE_T, FB_SIZE_T, const sort_key_def*,
		 FPTR_REJECT_DUP_CALLBACK, void*, FB_UINT64 = 0);
	~Sort();

	void get(Jrd::thread_db*, ULONG**);
	void put(Jrd::thread_db*, ULONG**);
	void sort(Jrd::thread_db*);

	bool isSorted() const
	{
		return m_flags & scb_sorted;
	}

	static FB_UINT64 readBlock(TempSpace* space, FB_UINT64 seek, UCHAR* address, ULONG length)
	{
		const size_t bytes = space->read(seek, address, length);
		fb_assert(bytes == length);
		return seek + bytes;
	}

	static FB_UINT64 writeBlock(TempSpace* space, FB_UINT64 seek, UCHAR* address, ULONG length)
	{
		const size_t bytes = space->write(seek, address, length);
		fb_assert(bytes == length);
		return seek + bytes;
	}

private:
	void allocateBuffer(MemoryPool&);
	void releaseBuffer();

	void diddleKey(UCHAR*, bool, bool);
	sort_record* getMerge(merge_control*);
	sort_record* getRecord();
	ULONG allocate(ULONG, ULONG, bool);
	void init();
	void mergeRuns(USHORT);
	ULONG order();
	void orderAndSave(Jrd::thread_db*);
	void putRun(Jrd::thread_db*);
	void sortBuffer(Jrd::thread_db*);
	void sortRunsBySeek(int);

#ifdef DEV_BUILD
	void checkFile(const run_control*);
#endif

	static void quick(SLONG, SORTP**, ULONG);

	Database* m_dbb;							// Database
	SortOwner* m_owner;							// Sort owner
	UCHAR* m_memory;							// ALLOC: Memory for sort
	UCHAR* m_end_memory;						// End of memory
	ULONG m_size_memory;						// Bytes allocated
	SR* m_last_record;							// Address of last record
	sort_record** m_first_pointer;				// Memory for sort
	sort_record** m_next_pointer;				// Address for next pointer
	ULONG m_longs;								// Length of record in longwords
	ULONG m_key_length;							// Key length
	ULONG m_unique_length;						// Unique key length, used when duplicates eliminated
	FB_UINT64 m_records;						// Number of records
	FB_UINT64 m_max_records;					// Maximum number of records to store, assigned but unused.
	TempSpace* m_space;							// temporary space for scratch file
	run_control* m_runs;						// ALLOC: Run on scratch file, if any
	merge_control* m_merge;						// Top level merge block
	run_control* m_free_runs;					// ALLOC: Currently unused run blocks
	ULONG m_flags;								// see flag bits below
	FPTR_REJECT_DUP_CALLBACK m_dup_callback;	// Duplicate handling callback
	void* m_dup_callback_arg;					// Duplicate handling callback arg
	merge_control* m_merge_pool;				// ALLOC: pool of merge_control blocks

	ULONG m_min_alloc_size;						// MIN and MAX values
	ULONG m_max_alloc_size;						// for the run buffer size

	ScratchBird::Array<sort_key_def> m_description;
};


class PartitionedSort
{
public:
	PartitionedSort(Database*, SortOwner*);
	~PartitionedSort();

	void get(Jrd::thread_db*, ULONG**);

	void addPartition(Sort* sort)
	{
		sort_control item;
		item.srt_header.rmh_type = RMH_TYPE_SORT;
		item.srt_header.rmh_parent = NULL;
		item.srt_sort = sort;

		m_parts.add(item);
	}

	void buildMergeTree();

private:
	sort_record* getMerge();

	SortOwner* m_owner;
	ScratchBird::HalfStaticArray<sort_control, 8> m_parts;
	ScratchBird::HalfStaticArray<merge_control, 8> m_nodes;	// nodes of merge tree
	merge_control* m_merge;				// root of merge tree
};


class SortOwner
{
public:
	SortOwner(MemoryPool& p, Database* database)
		: pool(p), dbb(database), sorts(p), buffers(p)
	{}

	~SortOwner()
	{
		unlinkAll();
	}

	void unlinkAll();

	void linkSort(Sort* scb)
	{
		fb_assert(scb);

		if (!sorts.exist(scb))
		{
			sorts.add(scb);
		}
	}

	void unlinkSort(Sort* scb)
	{
		fb_assert(scb);

		FB_SIZE_T pos;
		if (sorts.find(scb, pos))
		{
			sorts.remove(pos);
		}
	}

	MemoryPool& getPool() const
	{
		return pool;
	}

	UCHAR* allocateBuffer();
	void releaseBuffer(UCHAR*);

private:
	MemoryPool& pool;
	Database* const dbb;
	ScratchBird::SortedArray<Sort*> sorts;
	ScratchBird::HalfStaticArray<UCHAR*, 4> buffers;
};

} //namespace Jrd

#endif // JRD_SORT_H
