/*
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

#include "firebird.h"
#include "../jrd/jrd.h"
#include "../jrd/exe.h"
#include "../jrd/btr.h"
#include "../jrd/req.h"
#include "../jrd/btr_proto.h"
#include "../jrd/cch_proto.h"
#include "../jrd/cmp_proto.h"
#include "../jrd/evl_proto.h"
#include "../jrd/met_proto.h"
#include "../jrd/vio_proto.h"
#include "../jrd/rlck_proto.h"

#include "RecordSource.h"

using namespace ScratchBird;
using namespace Jrd;

// ------------------------------------
// Data access: index driven table scan
// ------------------------------------

IndexTableScan::IndexTableScan(CompilerScratch* csb, const string& alias,
							   StreamType stream, jrd_rel* relation,
							   InversionNode* index, USHORT length,
							   double selectivity)
	: RecordStream(csb, stream),
	  m_alias(csb->csb_pool, alias), m_relation(relation), m_index(index),
	  m_inversion(NULL), m_condition(NULL), m_length(length), m_offset(0)
{
	fb_assert(m_index);

	// Reserve one excess byte for the upper key - in case when length of
	// upper key at retrieval is greater than declared index key length.
	// See also comments at openStream().
	FB_SIZE_T size = sizeof(Impure) + 2u * m_length + 1u;
	size = FB_ALIGN(size, FB_ALIGNMENT);
	m_offset = size;
	size += sizeof(index_desc);

	m_impure = csb->allocImpure(FB_ALIGNMENT, static_cast<ULONG>(size));
	m_cardinality = csb->csb_rpt[stream].csb_cardinality * selectivity;
}

void IndexTableScan::internalOpen(thread_db* tdbb) const
{
	Request* const request = tdbb->getRequest();
	Impure* const impure = request->getImpure<Impure>(m_impure);

	impure->irsb_flags = irsb_first | irsb_open;

	record_param* const rpb = &request->req_rpb[m_stream];
	RLCK_reserve_relation(tdbb, request->req_transaction, m_relation, false);

	rpb->rpb_number.setValue(BOF_NUMBER);

	fb_assert(!impure->irsb_nav_lower);
	impure->irsb_nav_current_lower = impure->irsb_nav_lower = FB_NEW_POOL(*tdbb->getDefaultPool()) temporary_key;

	fb_assert(!impure->irsb_nav_upper);
	impure->irsb_nav_current_upper = impure->irsb_nav_upper = FB_NEW_POOL(*tdbb->getDefaultPool()) temporary_key;
}

void IndexTableScan::close(thread_db* tdbb) const
{
	Request* const request = tdbb->getRequest();

	invalidateRecords(request);

	Impure* const impure = request->getImpure<Impure>(m_impure);

	if (impure->irsb_flags & irsb_open)
	{
		impure->irsb_flags &= ~irsb_open;

		if (m_recursive)
		{
			if (impure->irsb_nav_bitmap)
			{
				delete *impure->irsb_nav_bitmap;
				*impure->irsb_nav_bitmap = NULL;
			}

			delete impure->irsb_nav_records_visited;
			impure->irsb_nav_records_visited = NULL;
		}

		if (impure->irsb_nav_btr_gc_lock)
		{
#ifdef DEBUG_LCK_LIST
			if (!impure->irsb_nav_page && impure->irsb_nav_btr_gc_lock->isActive())
				gds__log("DEBUG_LCK_LIST: irsb_nav_btr_gc_lock->isActive() && !irsb_nav_page");
#endif
			if (impure->irsb_nav_btr_gc_lock->isActive())
				impure->irsb_nav_btr_gc_lock->enablePageGC(tdbb);

			delete impure->irsb_nav_btr_gc_lock;
			impure->irsb_nav_btr_gc_lock = NULL;
		}

		impure->irsb_nav_page = 0;

		if (impure->irsb_nav_lower)
		{
			delete impure->irsb_nav_lower;
			impure->irsb_nav_current_lower = impure->irsb_nav_lower = NULL;
		}

		if (impure->irsb_nav_upper)
		{
			delete impure->irsb_nav_upper;
			impure->irsb_nav_current_upper = impure->irsb_nav_upper = NULL;
		}

		if (impure->irsb_iterator)
		{
			delete impure->irsb_iterator;
			impure->irsb_iterator = NULL;
		}
	}
#ifdef DEBUG_LCK_LIST
	// paranoid check
	else if (impure->irsb_nav_btr_gc_lock)
	{
		gds__log("DEBUG_LCK_LIST: irsb_nav_btr_gc_lock && !(irsb_flags & irsb_open)");

		if (impure->irsb_nav_btr_gc_lock->isActive())
			impure->irsb_nav_btr_gc_lock->enablePageGC(tdbb);

		delete impure->irsb_nav_btr_gc_lock;
		impure->irsb_nav_btr_gc_lock = NULL;
		impure->irsb_nav_page = 0;
	}
#endif
}

bool IndexTableScan::internalGetRecord(thread_db* tdbb) const
{
	JRD_reschedule(tdbb);

	Request* const request = tdbb->getRequest();
	record_param* const rpb = &request->req_rpb[m_stream];
	Impure* const impure = request->getImpure<Impure>(m_impure);

	if (!(impure->irsb_flags & irsb_open))
	{
		rpb->rpb_number.setValid(false);
		return false;
	}

	if (impure->irsb_flags & irsb_first)
	{
		impure->irsb_flags &= ~irsb_first;
		setPage(tdbb, impure, NULL);

		impure->irsb_iterator = m_index->retrieval->irb_list ?
			FB_NEW_POOL(*tdbb->getDefaultPool()) IndexScanListIterator(tdbb, m_index->retrieval) :
			nullptr;

		USHORT dummy = 0;		// exclude upper/lower bits are not used here,
								// i.e. additional forced include not needed
		if (!BTR_make_bounds(tdbb, m_index->retrieval, impure->irsb_iterator,
							 impure->irsb_nav_lower, impure->irsb_nav_upper, dummy))
		{
			rpb->rpb_number.setValid(false);
			return false;
		}
	}

	// If this is the first time, start at the beginning
	if (!impure->irsb_nav_page)
	{
		// initialize for a retrieval
		if (!setupBitmaps(tdbb, impure))
		{
			rpb->rpb_number.setValid(false);
			return false;
		}
	}

	index_desc* const idx = (index_desc*) ((SCHAR*) impure + m_offset);
	const bool descending = (idx->idx_flags & idx_descending);

	// find the last fetched position from the index
	const USHORT pageSpaceID = m_relation->getPages(tdbb)->rel_pg_space_id;
	win window(pageSpaceID, impure->irsb_nav_page);

	const IndexRetrieval* const retrieval = m_index->retrieval;
	const USHORT flags = retrieval->irb_generic & (irb_descending | irb_partial | irb_starting);
	const bool ignoreNulls =
		(retrieval->irb_generic & irb_ignore_null_value_key) && (idx->idx_count == 1);

	do
	{
		UCHAR* nextPointer = getPosition(tdbb, impure, &window);
		if (!nextPointer)
		{
			rpb->rpb_number.setValid(false);
			return false;
		}

		temporary_key key;
		memcpy(key.key_data, impure->irsb_nav_data, impure->irsb_nav_length);

		// set the upper (or lower) limit for navigational retrieval
		temporary_key upper;
		if (retrieval->irb_upper_count)
		{
			upper.key_length = impure->irsb_nav_upper_length;
			memcpy(upper.key_data, impure->irsb_nav_data + m_length, upper.key_length);
		}

		IndexKey recordKey(tdbb, m_relation, idx);

		// Find the next interesting node. If necessary, skip to the next page.
		RecordNumber number;
		IndexNode node;
		while (true)
		{
			Ods::btree_page* page = (Ods::btree_page*) window.win_buffer;

			UCHAR* pointer = nextPointer;
			if (pointer)
			{
				node.readNode(pointer, true);
				number = node.recordNumber;
			}

			if (node.isEndLevel)
				break;

			if (node.isEndBucket)
			{
				page = (Ods::btree_page*) CCH_HANDOFF(tdbb, &window, page->btr_sibling, LCK_read, pag_index);
				nextPointer = page->btr_nodes + page->btr_jump_size;
				continue;
			}

			// If we're walking in a descending index and we need to ignore NULLs
			// then stop at the first NULL we see (only for single segment!)
			if (descending && ignoreNulls &&
				node.prefix == 0 && node.length >= 1 && node.data[0] == 255)
			{
				break;
			}

			// Build the current key value from the prefix and current node data.
			memcpy(key.key_data + node.prefix, node.data, node.length);
			key.key_length = node.length + node.prefix;

			// Make sure we haven't hit the upper (or lower) limit.
			if (retrieval->irb_upper_count &&
				compareKeys(idx, key.key_data, key.key_length, &upper, flags) > 0)
			{
				const auto nextLower = impure->irsb_nav_current_lower;
				const auto nextUpper = impure->irsb_nav_current_upper;

				if (impure->irsb_iterator && impure->irsb_iterator->getNext(tdbb, nextLower, nextUpper))
				{
					if (retrieval->irb_generic & irb_root_list_scan)
					{
						CCH_RELEASE(tdbb, &window);
						page = BTR_find_page(tdbb, retrieval, &window, idx, nextLower, nextUpper);
						setPage(tdbb, impure, &window);
					}

					// If END_BUCKET is reached BTR_find_leaf will return NULL
					while (!(nextPointer = BTR_find_leaf(page, nextLower, nullptr, nullptr,
						(idx->idx_flags & idx_descending),
						(retrieval->irb_generic & (irb_starting | irb_partial)))))
					{
						page = (Ods::btree_page*) CCH_HANDOFF(tdbb, &window, page->btr_sibling, LCK_read, pag_index);
					}

					// Update the local keys
					key.key_length = nextLower->key_length;
					memcpy(key.key_data, nextLower->key_data, key.key_length);
					upper.key_length = nextUpper->key_length;
					memcpy(upper.key_data, nextUpper->key_data, upper.key_length);

					// Update the keys in the impure area
					impure->irsb_nav_length = key.key_length;
					impure->irsb_nav_upper_length = MIN(m_length + 1, upper.key_length);
					memcpy(impure->irsb_nav_data + m_length, upper.key_data, impure->irsb_nav_upper_length);

					continue;
				}

				break;
			}

			const bool skipNullKey = (ignoreNulls && !descending && !key.key_length);

			// Skip this record if:
			// 1) NULL key is found and we were asked to ignore them, or
			// 2) there is an inversion tree for this index and this record
			//    is not in the bitmap for the inversion, or
			// 3) the record has already been visited

			if (skipNullKey ||
				(!(impure->irsb_flags & irsb_mustread) &&
				(!impure->irsb_nav_bitmap ||
					!RecordBitmap::test(*impure->irsb_nav_bitmap, number.getValue()))) ||
				RecordBitmap::test(impure->irsb_nav_records_visited, number.getValue()))
			{
				nextPointer = node.readNode(pointer, true);
				continue;
			}

			// reset the current navigational position in the index
			rpb->rpb_number = number;
			setPosition(tdbb, impure, rpb, &window, pointer, key);

			CCH_RELEASE(tdbb, &window);

			if (VIO_get(tdbb, rpb, request->req_transaction, request->req_pool))
			{
				if (const auto result = recordKey.compose(rpb->rpb_record))
				{
					IndexErrorContext context(m_relation, idx);
					context.raise(tdbb, result, rpb->rpb_record);
				}

				if (!compareKeys(idx, key.key_data, key.key_length, recordKey, 0))
				{
					// mark in the navigational bitmap that we have visited this record
					RBM_SET(tdbb->getDefaultPool(), &impure->irsb_nav_records_visited,
							rpb->rpb_number.getValue());

					rpb->rpb_number.setValid(true);
					return true;
				}
			}

			nextPointer = getPosition(tdbb, impure, &window);
			if (!nextPointer)
			{
				rpb->rpb_number.setValid(false);
				return false;
			}
		}

		CCH_RELEASE(tdbb, &window);

		advanceStream(tdbb, impure, &window);
	} while (true);

	// bof or eof must have been set at this point
	rpb->rpb_number.setValid(false);
	return false;
}

void IndexTableScan::getLegacyPlan(thread_db* tdbb, string& plan, unsigned level) const
{
	if (!level)
		plan += "(";

	plan += printName(tdbb, m_alias) + " ORDER ";
	string index;
	printLegacyInversion(tdbb, m_index, index);
	plan += index;

	if (m_inversion)
	{
		plan += " INDEX (";
		string indices;
		printLegacyInversion(tdbb, m_inversion, indices);
		plan += indices + ")";
	}

	if (!level)
		plan += ")";
}

void IndexTableScan::internalGetPlan(thread_db* tdbb, PlanEntry& planEntry, unsigned level, bool recurse) const
{
	planEntry.className = "IndexTableScan";

	planEntry.lines.add().text = "Table " +
		printName(tdbb, m_relation->rel_name.toQuotedString(), m_alias) + " Access By ID";
	printOptInfo(planEntry.lines);

	printInversion(tdbb, m_index, planEntry.lines, true, 1, true);

	planEntry.objectType = m_relation->getObjectType();
	planEntry.objectName = m_relation->rel_name;

	if (m_alias.hasData() && m_alias != string(m_relation->rel_name.object))
		planEntry.alias = m_alias;

	if (m_inversion)
		printInversion(tdbb, m_inversion, planEntry.lines, true, 2, false);
}

int IndexTableScan::compareKeys(const index_desc* idx,
								const UCHAR* key_string1,
								USHORT length1,
								const temporary_key* key2,
								USHORT flags) const
{
	const UCHAR* string1 = key_string1;
	const UCHAR* string2 = key2->key_data;
	const USHORT length2 = key2->key_length;

	USHORT l = MIN(length1, length2);
	if (l)
	{
		do
		{
			if (*string1++ != *string2++)
			{
				return (string1[-1] < string2[-1]) ? -1 : 1;
			}
		} while (--l);
	}

	// if the keys are identical, return 0
	if (length1 == length2)
		return 0;

	// do a partial key search; if the index is a compound key,
	// check to see if the segments are matching--for character
	// fields, do partial matches within a segment if irb_starting
	// is specified, for other types do only matches on the entire segment

	if ((flags & (irb_partial | irb_starting)) && (length1 > length2))
	{
		// figure out what segment we're on; if it's a
		// character segment we've matched the partial string
		const UCHAR* segment = NULL;
		USHORT segnum = 0;

		if (idx->idx_count > 1)
		{
			segment = key_string1 + ((length2 - 1) / (Ods::STUFF_COUNT + 1)) * (Ods::STUFF_COUNT + 1);
			segnum = idx->idx_count - (UCHAR) ((flags & irb_descending) ? ((*segment) ^ -1) : *segment);
		}

		// If it's a string type key, and we're allowing "starting with" fuzzy
		// type matching, we're done
		if (flags & irb_starting)
		{
			const index_desc::idx_repeat* const tail = idx->idx_rpt + segnum;

			if (tail->idx_itype == idx_string ||
				tail->idx_itype == idx_byte_array ||
				tail->idx_itype == idx_metadata ||
				tail->idx_itype >= idx_first_intl_string)
			{
				return 0;
			}
		}

		if (idx->idx_count > 1)
		{
			// If we search for NULLs at the beginning then we're done if the first
			// segment isn't the first one possible
			if (length2 == 0)
			{
				if (segnum != 0)
					return 0;
			}

			// if we've exhausted the segment, we've found a match
			USHORT remainder = length2 % (Ods::STUFF_COUNT + 1);
			if (!remainder && (*string1 != *segment))
			{
				return 0;
			}

			// if the rest of the key segment is 0's, we've found a match
			if (remainder)
			{
				for (remainder = Ods::STUFF_COUNT + 1 - remainder; remainder; remainder--)
				{
					if (*string1++)
						break;
				}

				if (!remainder)
					return 0;
			}
		}
	}

	if (flags & irb_descending)
	{
		return (length1 < length2) ? 1 : -1;
	}

	return (length1 < length2) ? -1 : 1;
}

bool IndexTableScan::findSavedNode(thread_db* tdbb, Impure* impure, win* window, UCHAR** return_pointer) const
{
	const IndexRetrieval* const retrieval = m_index->retrieval;
	const USHORT flags = retrieval->irb_generic & irb_descending;

	index_desc* const idx = (index_desc*) ((SCHAR*) impure + m_offset);
	Ods::btree_page* page = (Ods::btree_page*) CCH_FETCH(tdbb, window, LCK_read, pag_index);

	// the outer loop goes through all the sibling pages
	// looking for the node (in case the page has split);
	// the inner loop goes through the nodes on each page
	temporary_key key;
	IndexNode node;
	while (true)
	{
		UCHAR* pointer = page->btr_nodes + page->btr_jump_size;
		const UCHAR* const endPointer = ((UCHAR*) page + page->btr_length);
		while (pointer < endPointer)
		{
			pointer = node.readNode(pointer, true);

			if (node.isEndLevel)
			{
				*return_pointer = node.nodePointer;
				return false;
			}

			if (node.isEndBucket)
			{
				page = (Ods::btree_page*) CCH_HANDOFF(tdbb, window, page->btr_sibling,
													  LCK_read, pag_index);
				break;
			}

			// maintain the running key value and compare it with the stored value
			memcpy(key.key_data + node.prefix, node.data, node.length);
			key.key_length = node.length + node.prefix;
			const int result =
				compareKeys(idx, impure->irsb_nav_data, impure->irsb_nav_length, &key, flags);

			// if the keys are equal, return this node even if it is just a duplicate node;
			// duplicate nodes that have already been returned will be filtered out at a
			// higher level by checking the sparse bit map of records already returned, and
			// duplicate nodes that were inserted before the stored node can be
			// safely returned since their position in the index is arbitrary
			if (!result)
			{
				*return_pointer = node.nodePointer;
				return node.recordNumber == impure->irsb_nav_number;
			}

			// if the stored key is less than the current key, then the stored key
			// has been deleted from the index and we should just return the next
			// key after it
			if (result < 0)
			{
				*return_pointer = node.nodePointer;
				return false;
			}

		}
	}
}

void IndexTableScan::advanceStream(thread_db* tdbb, Impure* impure, win* window) const
{
	impure->irsb_nav_current_lower = impure->irsb_nav_current_lower->key_next.get();
	impure->irsb_nav_current_upper = impure->irsb_nav_current_upper->key_next.get();
	setPage(tdbb, impure, NULL);
	window->win_page = 0;
}

UCHAR* IndexTableScan::getPosition(thread_db* tdbb, Impure* impure, win* window) const
{
	while (impure->irsb_nav_current_lower)
	{
		UCHAR* position = getStreamPosition(tdbb, impure, window);

		if (position)
			return position;

		advanceStream(tdbb, impure, window);
	}

	return NULL;
}

UCHAR* IndexTableScan::getStreamPosition(thread_db* tdbb, Impure* impure, win* window) const
{
	if (!window->win_page.getPageNum())
		return openStream(tdbb, impure, window);

	// Re-fetch page and get incarnation counter
	Ods::btree_page* page = (Ods::btree_page*) CCH_FETCH(tdbb, window, LCK_read, pag_index);

	UCHAR* pointer = NULL;
	const SLONG incarnation = CCH_get_incarnation(window);
	IndexNode node;
	if (incarnation == impure->irsb_nav_incarnation)
	{
		pointer = (UCHAR*) page + impure->irsb_nav_offset;

		// The new way of doing things is to have the current
		// nav_offset be the last node fetched.
		return node.readNode(pointer, true);
	}

	// Page is presumably changed.  If there is a previous
	// stored position in the page, go back to it if possible.
	CCH_RELEASE(tdbb, window);
	if (!impure->irsb_nav_page)
		return openStream(tdbb, impure, window);

	const bool found = findSavedNode(tdbb, impure, window, &pointer);
	page = (Ods::btree_page*) window->win_buffer;
	if (pointer)
	{
		// seek to the next node only if we found the actual node on page;
		// if we did not find it, we are already at the next node
		// (such as when the node is garbage collected)
		return found ? node.readNode(pointer, true) : pointer;
	}

	return page->btr_nodes + page->btr_jump_size;
}

UCHAR* IndexTableScan::openStream(thread_db* tdbb, Impure* impure, win* window) const
{
	temporary_key* lower = impure->irsb_nav_current_lower;
	temporary_key* upper = impure->irsb_nav_current_upper;

	setPage(tdbb, impure, NULL);
	impure->irsb_nav_length = 0;

	// Find the starting leaf page
	const IndexRetrieval* const retrieval = m_index->retrieval;
	index_desc* const idx = (index_desc*) ((SCHAR*) impure + m_offset);

	Ods::btree_page* page = BTR_find_page(tdbb, retrieval, window, idx, lower, upper);
	setPage(tdbb, impure, window);

	// find the upper limit for the search
	temporary_key* limit_ptr = NULL;
	if (retrieval->irb_upper_count)
	{
		// If upper key length is greater than declared key length, we need
		// one "excess" byte for correct comparison. Without it there could
		// be false equality hits.
		impure->irsb_nav_upper_length = MIN(m_length + 1, upper->key_length);
		memcpy(impure->irsb_nav_data + m_length, upper->key_data, impure->irsb_nav_upper_length);
	}

	if (retrieval->irb_lower_count)
		limit_ptr = lower;

	// If there is a starting descriptor, search down index to starting position.
	// This may involve sibling buckets if splits are in progress.  If there
	// isn't a starting descriptor, walk down the left side of the index.

	if (limit_ptr)
	{
		UCHAR* pointer = NULL;
		// If END_BUCKET is reached BTR_find_leaf will return NULL
		while (!(pointer = BTR_find_leaf(page, limit_ptr, impure->irsb_nav_data, NULL,
							(idx->idx_flags & idx_descending),
							(retrieval->irb_generic & (irb_starting | irb_partial)))))
		{
			page = (Ods::btree_page*) CCH_HANDOFF(tdbb, window, page->btr_sibling,
				  LCK_read, pag_index);
		}

		IndexNode node;
		node.readNode(pointer, true);

		impure->irsb_nav_length = node.prefix + node.length;
		return pointer;
	}

	return page->btr_nodes + page->btr_jump_size;
}

void IndexTableScan::setPage(thread_db* tdbb, Impure* impure, win* window) const
{
	const ULONG newPage = window ? window->win_page.getPageNum() : 0;

	if (impure->irsb_nav_page != newPage)
	{
		if (impure->irsb_nav_page)
		{
			fb_assert(impure->irsb_nav_btr_gc_lock);
			impure->irsb_nav_btr_gc_lock->enablePageGC(tdbb);
		}

		if (newPage)
		{
			if (!impure->irsb_nav_btr_gc_lock)
			{
				impure->irsb_nav_btr_gc_lock =
#ifdef DEBUG_LCK_LIST
					FB_NEW_RPT(*tdbb->getDefaultPool(), 0) BtrPageGCLock(tdbb, tdbb->getDefaultPool());
#else
					FB_NEW_RPT(*tdbb->getDefaultPool(), 0) BtrPageGCLock(tdbb);
#endif
			}

			impure->irsb_nav_btr_gc_lock->disablePageGC(tdbb, window->win_page);
		}

		impure->irsb_nav_page = newPage;

		// clear position as page was changed
		impure->irsb_nav_incarnation = 0;
		impure->irsb_nav_offset = 0;
	}
}

void IndexTableScan::setPosition(thread_db* tdbb,
								 Impure* impure,
								 record_param* rpb,
								 win* window,
								 const UCHAR* pointer,
								 const temporary_key& key) const
{
	// We can actually set position without having a data page
	// fetched; if not, just set the incarnation to the lowest possible
	// Note, setPage could clear position (incarnation and offset).

	setPage(tdbb, impure, window);
	impure->irsb_nav_incarnation = CCH_get_incarnation(window);
	impure->irsb_nav_number = rpb->rpb_number;

	// save the current key value
	fb_assert(key.key_length <= m_length);
	impure->irsb_nav_length = key.key_length;
	memcpy(impure->irsb_nav_data, key.key_data, key.key_length);

	// setup the offsets into the current index page
	impure->irsb_nav_offset = pointer - (UCHAR*) window->win_buffer;
}

bool IndexTableScan::setupBitmaps(thread_db* tdbb, Impure* impure) const
{
	// Start a bitmap which tells us we have already visited
	// this record; this is to handle the case where there is more
	// than one leaf node reference to the same record number; the
	// bitmap allows us to filter out the multiple references.
	RecordBitmap::reset(impure->irsb_nav_records_visited);
	impure->irsb_flags |= irsb_mustread;

	// the first time we open the stream, compute a bitmap
	// for the inversion tree -- this may cause problems for
	// read-committed transactions since they will get one
	// view of the database when the stream is opened
	if (m_inversion)
	{
		if (!m_condition || !m_condition->execute(tdbb, tdbb->getRequest()))
		{
			impure->irsb_flags &= ~irsb_mustread;
			// There is no need to reset or release the bitmap, it is
			// done in EVL_bitmap()
			impure->irsb_nav_bitmap = EVL_bitmap(tdbb, m_inversion, NULL);
			return (*impure->irsb_nav_bitmap != NULL);
		}
	}

	return true;
}
