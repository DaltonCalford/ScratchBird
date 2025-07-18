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
 *		Alex Peshkoff, 2010 - divided into class DataDump and the remaining part of DatabaseSnapshot
 */

#ifndef JRD_DATABASE_SNAPSHOT_H
#define JRD_DATABASE_SNAPSHOT_H

#include "../common/classes/array.h"
#include "../common/classes/init.h"
#include "../common/isc_s_proto.h"
#include "../common/classes/timestamp.h"
#include "../jrd/val.h"
#include "../jrd/recsrc/RecordSource.h"
#include "../jrd/TempSpace.h"

namespace Jrd {

// forward declarations
class jrd_rel;
class Record;
class RecordBuffer;
class RuntimeStatistics;

class SnapshotData
{
public:
	struct RelationData
	{
		int rel_id;
		RecordBuffer* data;
	};

	enum ValueType
	{
		VALUE_UNKNOWN,
		VALUE_GLOBAL_ID,
		VALUE_TABLE_ID_OBJECT_NAME,
		VALUE_INTEGER,
		VALUE_TIMESTAMP,
		VALUE_TIMESTAMP_TZ,
		VALUE_STRING,
		VALUE_BOOLEAN,
		VALUE_TABLE_ID_SCHEMA_NAME,
		VALUE_LAST_MARKER	// Should be last item
	};

	struct DumpField
	{
		DumpField(USHORT p_id, ValueType p_type, ULONG p_length, const void* p_data)
			: id(p_id), type(p_type), length(p_length), data(p_data)
		{}

		DumpField()
			: id(0), type(VALUE_UNKNOWN), length(0), data(NULL)
		{}

		USHORT id;
		ValueType type;
		ULONG length;
		const void* data;
	};

	class DumpRecord
	{
	public:
		class Writer
		{
		public:
			virtual void write(const DumpRecord& record) = 0;
		};

		explicit DumpRecord(MemoryPool& pool)
			: buffer(pool), offset(0), writer(NULL)
		{}

		DumpRecord(MemoryPool& pool, Writer& wr)
			: buffer(pool), offset(0), writer(&wr)
		{}

		void reset(int rel_id)
		{
			offset = 1;
			buffer.clear();
			buffer.add(rel_id);
		}

		void assign(ULONG length, const UCHAR* ptr)
		{
			offset = 0;
			buffer.assign(ptr, length);
		}

		ULONG getLength() const
		{
			return offset;
		}

		const UCHAR* getData() const
		{
			return buffer.begin();
		}

		void storeGlobalId(int field_id, SINT64 value)
		{
			storeField(field_id, VALUE_GLOBAL_ID, sizeof(SINT64), &value);
		}

		void storeTableIdObjectName(int field_id, SLONG value)
		{
			storeField(field_id, VALUE_TABLE_ID_OBJECT_NAME, sizeof(SLONG), &value);
		}

		void storeTableIdSchemaName(int field_id, SLONG value)
		{
			storeField(field_id, VALUE_TABLE_ID_SCHEMA_NAME, sizeof(SLONG), &value);
		}

		void storeInteger(int field_id, SINT64 value)
		{
			storeField(field_id, VALUE_INTEGER, sizeof(SINT64), &value);
		}

		void storeTimestamp(int field_id, const ScratchBird::TimeStamp& value)
		{
			if (!value.isEmpty())
				storeField(field_id, VALUE_TIMESTAMP, sizeof(ISC_TIMESTAMP), &value.value());
		}

		void storeTimestamp(int field_id, const ISC_TIMESTAMP& value)
		{
			storeField(field_id, VALUE_TIMESTAMP, sizeof(ISC_TIMESTAMP), &value);
		}

		void storeTimestampTz(int field_id, const ISC_TIMESTAMP_TZ& value)
		{
			storeField(field_id, VALUE_TIMESTAMP_TZ, sizeof(ISC_TIMESTAMP_TZ), &value);
		}

		template <class S>
		void storeString(int field_id, const S& value)
		{
			if (value.length())
				storeField(field_id, VALUE_STRING, value.length(), value.c_str());
		}

		void storeBoolean(int field_id, bool value)
		{
			const UCHAR boolean = value ? 1 : 0;
			storeField(field_id, VALUE_BOOLEAN, sizeof(UCHAR), &boolean);
		}

		int getRelationId()
		{
			fb_assert(!offset);
			return (ULONG) buffer[offset++];
		}

		bool getField(DumpField& field)
		{
			fb_assert(offset);

			if (offset < buffer.getCount())
			{
				field.id = (USHORT) buffer[offset++];
				field.type = (ValueType) buffer[offset++];
				fb_assert(field.type >= VALUE_GLOBAL_ID && field.type < VALUE_LAST_MARKER);
				memcpy(&field.length, &buffer[offset], sizeof(ULONG));
				offset += sizeof(ULONG);
				field.data = &buffer[offset];
				offset += field.length;
				return true;
			}

			return false;
		}

		void write() const
		{
			fb_assert(writer);
			writer->write(*this);
		}

	private:
		void storeField(int field_id, ValueType type, FB_SIZE_T length, const void* value)
		{
			const FB_SIZE_T delta = sizeof(UCHAR) + sizeof(UCHAR) + sizeof(ULONG) + length;
			buffer.resize(offset + delta);

			UCHAR* ptr = buffer.begin() + offset;
			fb_assert(field_id <= int(MAX_UCHAR));
			*ptr++ = (UCHAR) field_id;
			*ptr++ = (UCHAR) type;
			const ULONG adjusted_length = (ULONG) length;
			memcpy(ptr, &adjusted_length, sizeof(adjusted_length));
			ptr += sizeof(ULONG);
			memcpy(ptr, value, length);
			offset += (ULONG) delta;
		}

		ScratchBird::HalfStaticArray<UCHAR, 1024> buffer;
		ULONG offset;
		Writer* const writer;
	};

	explicit SnapshotData(MemoryPool& pool)
		: m_snapshot(pool), m_map(pool), m_counter(0)
	{}

	virtual ~SnapshotData()
	{
		clearSnapshot();
	}

	void putField(thread_db*, Record*, const DumpField&);

	RecordBuffer* allocBuffer(thread_db*, MemoryPool&, int);
	RecordBuffer* getData(const jrd_rel*) const;
	RecordBuffer* getData(int) const;
	void clearSnapshot();

private:
	ScratchBird::Array<RelationData> m_snapshot;
	ScratchBird::GenericMap<ScratchBird::Pair<ScratchBird::NonPooled<SINT64, SLONG> > > m_map;
	int m_counter;
};


struct MonitoringHeader : public ScratchBird::MemoryHeader
{
	ULONG used;
	ULONG allocated;
};


class MonitoringData final : public ScratchBird::PermanentStorage, public ScratchBird::IpcObject
{
	static const USHORT MONITOR_VERSION = 6;
	static const ULONG DEFAULT_SIZE = 1048576;

	typedef MonitoringHeader Header;

	struct Element
	{
		AttNumber attId;
		TEXT userName[USERNAME_LENGTH + 1];
		ULONG generation;
		ULONG length;

		inline ULONG getBlockLength() const
		{
			return (ULONG) FB_ALIGN(sizeof(Element) + length, FB_ALIGNMENT);
		}
	};

public:
	class Guard
	{
	public:
		explicit Guard(MonitoringData* ptr)
			: data(ptr)
		{
			data->acquire();
		}

		~Guard()
		{
			data->release();
		}

	private:
		Guard(const Guard&);
		Guard& operator=(const Guard&);

		MonitoringData* const data;
	};

	class Reader
	{
	public:
		Reader(MemoryPool& pool, TempSpace& temp)
			: source(temp), offset(0), buffer(pool)
		{}

		bool getRecord(SnapshotData::DumpRecord& record)
		{
			if (offset < source.getSize())
			{
				ULONG length;
				source.read(offset, &length, sizeof(ULONG));
				offset += sizeof(ULONG);

				UCHAR* const ptr = buffer.getBuffer(length);
				source.read(offset, ptr, length);
				offset += length;

				record.assign(length, ptr);
				return true;
			}

			return false;
		}

	private:
		TempSpace& source;
		offset_t offset;
		ScratchBird::UCharBuffer buffer;
	};

	typedef ScratchBird::HalfStaticArray<AttNumber, 64> SessionList;

	explicit MonitoringData(Database*);
	~MonitoringData();

	bool initialize(ScratchBird::SharedMemoryBase*, bool) override;
	void mutexBug(int osErrorCode, const char* text) override;

	USHORT getType() const override { return ScratchBird::SharedMemoryBase::SRAM_DATABASE_SNAPSHOT; }
	USHORT getVersion() const override { return MONITOR_VERSION; }
	const char* getName() const override { return "MonitoringData"; }

	void initSharedFile();

	void acquire();
	void release();

	void enumerate(const char*, ULONG, SessionList&);
	void read(const char*, TempSpace&);
	ULONG setup(AttNumber, const char*, ULONG);
	void write(ULONG, ULONG, const void*);

	void cleanup(AttNumber);

private:
	// copying is prohibited
	MonitoringData(const MonitoringData&);
	MonitoringData& operator =(const MonitoringData&);

	void ensureSpace(ULONG);

	const ScratchBird::string& m_dbId;
	ScratchBird::AutoPtr<ScratchBird::SharedMemory<MonitoringHeader> > m_sharedMemory;
	ScratchBird::Mutex m_localMutex;
};


class MonitoringTableScan: public VirtualTableScan
{
public:
	MonitoringTableScan(CompilerScratch* csb, const ScratchBird::string& alias,
						StreamType stream, jrd_rel* relation)
		: VirtualTableScan(csb, alias, stream, relation)
	{}

protected:
	const Format* getFormat(thread_db* tdbb, jrd_rel* relation) const override;
	bool retrieveRecord(thread_db* tdbb, jrd_rel* relation, FB_UINT64 position,
		Record* record) const override;
};


class MonitoringSnapshot : public SnapshotData
{
public:
	static MonitoringSnapshot* create(thread_db* tdbb);

protected:
	MonitoringSnapshot(thread_db* tdbb, MemoryPool& pool);
};


class Monitoring
{
public:
	static int blockingAst(void* ast_object);

	static ULONG checkGeneration(const Database* dbb, const Attachment* attachment)
	{
		const auto generation = dbb->getMonitorGeneration();

		if (generation != attachment->att_monitor_generation)
			return generation;

		return 0;
	}

	static void checkState(thread_db* tdbb);
	static SnapshotData* getSnapshot(thread_db* tdbb);

	static void dumpAttachment(thread_db* tdbb, Attachment* attachment, ULONG generation);

	static void publishAttachment(thread_db* tdbb);
	static void cleanupAttachment(thread_db* tdbb);

	static void putDatabase(thread_db* tdbb, SnapshotData::DumpRecord&);
private:
	static SINT64 getGlobalId(int);

	static void putAttachment(SnapshotData::DumpRecord&, const Attachment*);
	static void putTransaction(SnapshotData::DumpRecord&, const jrd_tra*);
	static void putStatement(SnapshotData::DumpRecord&, const Statement*, const ScratchBird::string&);
	static void putRequest(SnapshotData::DumpRecord&, const Request*, const ScratchBird::string&);
	static void putCall(SnapshotData::DumpRecord&, const Request*);
	static void putStatistics(SnapshotData::DumpRecord&, const RuntimeStatistics&, int, int);
	static void putContextVars(SnapshotData::DumpRecord&, const ScratchBird::StringMap&, SINT64, bool);
	static void putMemoryUsage(SnapshotData::DumpRecord&, const ScratchBird::MemoryStats&, int, int);
};

} // namespace

#endif // JRD_DATABASE_SNAPSHOT_H
