/*
 *  The contents of this file are subject to the Initial
 *  Developer's Public License Version 1.0 (the "License");
 *  you may not use this file except in compliance with the
 *  License. You may obtain a copy of the License at
 *  http://www.ibphoenix.com/main.nfs?a=ibphoenix&page=ibp_idpl.
 *
 *  Software distributed under the License is distributed AS IS,
 *  WITHOUT WARRANTY OF ANY KIND, either express or implied.
 *  See the License for the specific language governing rights
 *  and limitations under the License.
 *
 *  The Original Code was created by Dmitry Yemanov
 *  for the ScratchBird Open Source RDBMS project.
 *
 *  Copyright (c) 2006 Dmitry Yemanov <dimitr@users.sf.net>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#include "firebird.h"
#include "../common/gdsassert.h"
#include "../jrd/jrd.h"
#include "../jrd/req.h"

#include "../jrd/RecordBuffer.h"

const char* const SCRATCH = "fb_recbuf_";

using namespace Jrd;

RecordBuffer::RecordBuffer(MemoryPool& pool, const Format* format)
	: PermanentStorage(pool)
{
	record = FB_NEW_POOL(pool) Record(pool, format);
}

void RecordBuffer::reset()
{
	count = 0;
	space.reset();
}

offset_t RecordBuffer::store(const Record* new_record)
{
	const ULONG length = record->getLength();
	fb_assert(new_record->getLength() == length);

	if (!space)
		space = FB_NEW_POOL(getPool()) TempSpace(getPool(), SCRATCH);

	space->write(count * length, new_record->getData(), length);

	return count++;
}

bool RecordBuffer::fetch(offset_t position, Record* to_record)
{
	const ULONG length = record->getLength();
	fb_assert(to_record->getLength() == length);

	if (position >= count)
		return false;

	fb_assert(space.hasData());
	space->read(position * length, to_record->getData(), length);

	return true;
}

const Format* RecordBuffer::getFormat() const
{
	return record->getFormat();
}
