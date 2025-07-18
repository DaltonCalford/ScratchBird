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
 *  The Original Code was created by Adriano dos Santos Fernandes
 *  for the ScratchBird Open Source RDBMS project.
 *
 *  Copyright (c) 2018 Adriano dos Santos Fernandes <adrianosf@gmail.com>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#ifndef JRD_TIME_ZONE_H
#define JRD_TIME_ZONE_H

#include "firebird.h"
#include "firebird/Message.h"
#include "../common/classes/fb_string.h"
#include "../jrd/Monitoring.h"
#include "../jrd/SystemPackages.h"
#include "../jrd/recsrc/RecordSource.h"

namespace Jrd {

class thread_db;
class jrd_tra;
class RecordBuffer;


class TimeZoneSnapshot : public SnapshotData
{
public:
	TimeZoneSnapshot(thread_db* tdbb, MemoryPool& pool);
};

class TimeZonesTableScan final : public VirtualTableScan
{
public:
	TimeZonesTableScan(CompilerScratch* csb, const ScratchBird::string& alias, StreamType stream, jrd_rel* relation);

protected:
	const Format* getFormat(thread_db* tdbb, jrd_rel* relation) const override;
	bool retrieveRecord(thread_db* tdbb, jrd_rel* relation, FB_UINT64 position, Record* record) const override;
};


class TimeZonePackage : public SystemPackage
{
public:
	TimeZonePackage(ScratchBird::MemoryPool& pool);

private:
	FB_MESSAGE(TransitionsInput, ScratchBird::ThrowStatusExceptionWrapper,
		(FB_INTL_VARCHAR(MAX_SQL_IDENTIFIER_LEN, CS_METADATA), timeZoneName)
		(FB_TIMESTAMP_TZ, fromTimestamp)
		(FB_TIMESTAMP_TZ, toTimestamp)
	);

	FB_MESSAGE(TransitionsOutput, ScratchBird::ThrowStatusExceptionWrapper,
		(FB_TIMESTAMP_TZ, startTimestamp)
		(FB_TIMESTAMP_TZ, endTimestamp)
		(FB_SMALLINT, zoneOffset)
		(FB_SMALLINT, dstOffset)
		(FB_SMALLINT, effectiveOffset)
	);

	class TransitionsResultSet :
		public
			ScratchBird::DisposeIface<
				ScratchBird::IExternalResultSetImpl<
					TransitionsResultSet,
					ScratchBird::ThrowStatusExceptionWrapper
				>
			>
	{
	public:
		TransitionsResultSet(ScratchBird::ThrowStatusExceptionWrapper* status, ScratchBird::IExternalContext* context,
			const TransitionsInput::Type* in, TransitionsOutput::Type* out);

	public:
		void dispose() override
		{
			delete this;
		}

	public:
		FB_BOOLEAN fetch(ScratchBird::ThrowStatusExceptionWrapper* status) override;

	private:
		TransitionsOutput::Type* out;
		ScratchBird::AutoPtr<ScratchBird::TimeZoneRuleIterator> iterator;
	};

	static ScratchBird::IExternalResultSet* transitionsProcedure(ScratchBird::ThrowStatusExceptionWrapper* status,
		ScratchBird::IExternalContext* context,
		const TransitionsInput::Type* in, TransitionsOutput::Type* out)
	{
		return FB_NEW TransitionsResultSet(status, context, in, out);
	}

	//----------

	FB_MESSAGE(DatabaseVersionOutput, ScratchBird::ThrowStatusExceptionWrapper,
		(FB_INTL_VARCHAR(10, CS_ASCII), version)
	);

	static void databaseVersionFunction(ScratchBird::ThrowStatusExceptionWrapper* status,
		ScratchBird::IExternalContext* context, const void* in, DatabaseVersionOutput::Type* out);
};


}	// namespace

#endif	// JRD_TIME_ZONE_H
