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
 *  The Original Code was created by Alexander Peshkov
 *  for the ScratchBird Open Source RDBMS project.
 *
 *  Copyright (c) 2017 Alexander Peshkov <peshkoff@mail.ru>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ________________________________
 */

#ifndef DSQL_BATCH_H
#define DSQL_BATCH_H

#include "../jrd/TempSpace.h"
#include "../common/classes/alloc.h"
#include "../common/classes/RefCounted.h"
#include "../common/classes/vector.h"
#include "../common/classes/GenericMap.h"


namespace ScratchBird {

class ClumpletReader;

}


namespace Jrd {

class DsqlDmlRequest;
class dsql_msg;
class thread_db;
class JBatch;
class Attachment;

class DsqlBatch
{
public:
	DsqlBatch(DsqlDmlRequest* req, const dsql_msg* message, ScratchBird::IMessageMetadata* inMetadata,
		ScratchBird::ClumpletReader& pb);
	~DsqlBatch();

	static const ULONG RAM_BATCH = 128 * 1024;
	static const ULONG BUFFER_LIMIT = 16 * 1024 * 1024;
	static const ULONG HARD_BUFFER_LIMIT = 256 * 1024 * 1024;
	static const ULONG DETAILED_LIMIT = 64;
	static const ULONG SIZEOF_BLOB_HEAD = sizeof(ISC_QUAD) + 2 * sizeof(ULONG);
	static const unsigned BLOB_STREAM_ALIGN = 4;

	static DsqlBatch* open(thread_db* tdbb, DsqlDmlRequest* req, ScratchBird::IMessageMetadata* inMetadata,
		unsigned parLength, const UCHAR* par);

	Attachment* getAttachment() const;
	void setInterfacePtr(JBatch* interfacePtr) noexcept;

	void add(thread_db* tdbb, ULONG count, const void* inBuffer);
	void addBlob(thread_db* tdbb, ULONG length, const void* inBuffer, ISC_QUAD* blobId, unsigned parLength, const unsigned char* par);
	void appendBlobData(thread_db* tdbb, ULONG length, const void* inBuffer);
	void addBlobStream(thread_db* tdbb, unsigned length, const void* inBuffer);
	void registerBlob(thread_db* tdbb, const ISC_QUAD* existingBlob, ISC_QUAD* blobId);
	ScratchBird::IBatchCompletionState* execute(thread_db* tdbb);
	ScratchBird::IMessageMetadata* getMetadata(thread_db* tdbb);
	void cancel(thread_db* tdbb);
	void setDefaultBpb(thread_db* tdbb, unsigned parLength, const unsigned char* par);
	void info(thread_db* tdbb, unsigned int itemsLength, const unsigned char* items,
		unsigned int bufferLength, unsigned char* buffer);

	// Additional flags - start from the maximum one
	static const UCHAR FLAG_DEFAULT_SEGMENTED = 31;
	static const UCHAR FLAG_CURRENT_SEGMENTED = 30;

private:
	void genBlobId(ISC_QUAD* blobId);
	void blobPrepare();
	void blobSetSize();
	void blobCheckMode(bool stream, const char* fname);
	void blobCheckMeta();
	void registerBlob(const ISC_QUAD* engineBlob, const ISC_QUAD* batchBlob);
	void setDefBpb(unsigned parLength, const unsigned char* par);
	void putSegment(ULONG length, const void* inBuffer);

	void setFlag(UCHAR bit, bool value)
	{
		if (value)
			m_flags |= (1 << bit);
		else
			m_flags &= ~(1 << bit);
	}

	DsqlDmlRequest* const m_dsqlRequest;
	JBatch* m_batch = nullptr;
	ScratchBird::IMessageMetadata* const m_meta;

	class DataCache : public ScratchBird::PermanentStorage
	{
	public:
		DataCache(MemoryPool& p)
			: PermanentStorage(p),
			  m_cache(p)
		{ }

		void setBuf(ULONG size, ULONG cacheCapacity);

		void put(const void* data, ULONG dataSize);
		void put3(const void* data, ULONG dataSize, ULONG offset);
		void align(ULONG alignment);
		void done();
		ULONG get(UCHAR** buffer);
		ULONG reget(ULONG size, UCHAR** buffer, ULONG alignment);
		void remained(ULONG size, ULONG alignment = 0);
		ULONG getSize() const;
		ULONG getCapacity() const;
		void clear();
		void flush();

	private:
		typedef ScratchBird::Array<UCHAR> Cache;
		Cache m_cache;
		ScratchBird::AutoPtr<TempSpace> m_space;
		ULONG m_used = 0;
		ULONG m_got = 0;
		ULONG m_limit = 0;
		ULONG m_shift = 0;
		ULONG m_cacheCapacity = 0;
	};

	struct BlobMeta
	{
		unsigned nullOffset, offset;
	};

	class QuadComparator
	{
	public:
		static bool greaterThan(const ISC_QUAD& i1, const ISC_QUAD& i2)
		{
			return memcmp(&i1, &i2, sizeof(ISC_QUAD)) > 0;
		}
	};

	DataCache m_messages;
	DataCache m_blobs;
	ScratchBird::GenericMap<ScratchBird::Pair<ScratchBird::NonPooled<ISC_QUAD, ISC_QUAD>>, QuadComparator> m_blobMap;
	ScratchBird::HalfStaticArray<BlobMeta, 4> m_blobMeta;
	typedef ScratchBird::HalfStaticArray<UCHAR, 64> Bpb;
	Bpb m_defaultBpb;
	ISC_QUAD m_genId;
	ULONG m_messageSize = 0;
	ULONG m_alignedMessage = 0;
	ULONG m_alignment = 0;
	ULONG m_flags = 0;
	ULONG m_detailed = DETAILED_LIMIT;
	ULONG m_bufferSize = BUFFER_LIMIT;
	ULONG m_lastBlob = MAX_ULONG;
	bool m_setBlobSize = false;
	UCHAR m_blobPolicy = ScratchBird::IBatch::BLOB_NONE;
};

} // namespace

#endif // DSQL_BATCH_H
