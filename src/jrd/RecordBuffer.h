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

#ifndef JRD_RECORD_BUFFER_H
#define JRD_RECORD_BUFFER_H

#include "../common/classes/alloc.h"
#include "../common/classes/auto.h"
#include "../common/classes/File.h"
#include "../jrd/TempSpace.h"

namespace Jrd {

class Format;
class Record;

class RecordBuffer : public ScratchBird::PermanentStorage
{
public:
	RecordBuffer(MemoryPool&, const Format*);

	size_t getCount() const
	{
		return count;
	}

	Record* getTempRecord()
	{
		return record.get();
	}

	const Format* getFormat() const;

	void reset();
	offset_t store(const Record*);
	bool fetch(offset_t, Record*);

private:
	offset_t count = 0;
	ScratchBird::AutoPtr<Record> record;
	ScratchBird::AutoPtr<TempSpace> space;
};

} // namespace

#endif // JRD_RECORD_BUFFER_H
