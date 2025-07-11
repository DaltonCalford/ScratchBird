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
 *  Copyright (c) 2013 Dmitry Yemanov <dimitr@firebirdsql.org>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */


#ifndef JRD_REPLICATION_REPLICATOR_H
#define JRD_REPLICATION_REPLICATOR_H

#include "../common/classes/timestamp.h"
#include "../common/classes/MetaString.h"
#include "../common/classes/objects_array.h"
#include "../common/classes/QualifiedMetaString.h"
#include "../common/os/guid.h"
#include "../jrd/status.h"
#include "Protocol.h"
#include "Manager.h"
#include <utility>

namespace Replication
{
	class Replicator :
		public ScratchBird::StdPlugin<ScratchBird::IReplicatedSessionImpl<Replicator, ScratchBird::CheckStatusWrapper> >
	{
		typedef ScratchBird::ObjectsArray<ScratchBird::string> NameCache;
		typedef ScratchBird::HalfStaticArray<SavNumber, 16> SavepointStack;

		struct BatchBlock
		{
			Block header;
			ScratchBird::UCharBuffer* buffer;
			NameCache atoms;
			ULONG lastAtom;
			ULONG flushes;

			explicit BatchBlock(MemoryPool& pool)
				: buffer(NULL), atoms(pool),
				  lastAtom(MAX_ULONG), flushes(0)
			{
				memset(&header, 0, sizeof(Block));
			}

			ULONG getSize() const
			{
				return (ULONG) buffer->getCount();
			}

			void putTag(UCHAR tag)
			{
				putByte(tag);
			}

			void putByte(UCHAR value)
			{
				buffer->add(value);
			}

			void putInt16(SLONG value)
			{
				const auto ptr = (const UCHAR*) &value;
				buffer->add(ptr, sizeof(SSHORT));
			}

			void putInt32(SLONG value)
			{
				const auto ptr = (const UCHAR*) &value;
				buffer->add(ptr, sizeof(SLONG));
			}

			void putInt64(SINT64 value)
			{
				const auto ptr = (const UCHAR*) &value;
				buffer->add(ptr, sizeof(SINT64));
			}

			template <typename T>
			ULONG defineAtom(const T& name)
			{
				if (lastAtom < atoms.getCount() && atoms[lastAtom] == name)
					return lastAtom;

				FB_SIZE_T pos;
				if (!atoms.find(name, pos))
				{
					pos = atoms.getCount();
					atoms.add(name);
					putTag(opDefineAtom);
					putMetaName(name);
				}

				lastAtom = (ULONG) pos;
				return lastAtom;
			}

			std::pair<ULONG, ULONG> defineQualifiedAtom(const ScratchBird::QualifiedMetaString& name)
			{
				const auto schemaAtom = defineAtom(name.schema);
				const auto objectAtom = defineAtom(name.object);

				return std::make_pair(schemaAtom, objectAtom);
			}

			void putMetaName(const ScratchBird::MetaString& name)
			{
				const auto length = name.length();
				fb_assert(length <= MAX_UCHAR);
				putByte((UCHAR) length);
				buffer->add((const UCHAR*) name.c_str(), length);
			}

			void putString(const ScratchBird::string& str)
			{
				const auto length = str.length();
				putInt32(length);
				const auto ptr = (const UCHAR*) str.c_str();
				buffer->add(ptr, length);
			}

			void putBinary(ULONG length, const UCHAR* data)
			{
				buffer->add(data, length);
			}
		};

		class Transaction :
			public ScratchBird::AutoIface<ScratchBird::IReplicatedTransactionImpl<Transaction, ScratchBird::CheckStatusWrapper> >
		{
		public:
			explicit Transaction(Replicator* replicator, ScratchBird::ITransaction* trans)
				: m_replicator(replicator), m_transaction(trans), m_data(replicator->getPool())
			{}

			BatchBlock& getData()
			{
				return m_data;
			}

			ScratchBird::ITransaction* getInterface()
			{
				return m_transaction.getPtr();
			}

			// IDisposable methods

			void dispose() override
			{
				m_replicator->releaseTransaction(this);
				delete this;
			}

			// IReplicatedTransaction methods

			void prepare(ScratchBird::CheckStatusWrapper* status) override
			{
				m_replicator->prepareTransaction(status, this);
			}

			void commit(ScratchBird::CheckStatusWrapper* status) override
			{
				m_replicator->commitTransaction(status, this);
			}

			void rollback(ScratchBird::CheckStatusWrapper* status) override
			{
				m_replicator->rollbackTransaction(status, this);
			}

			void startSavepoint(ScratchBird::CheckStatusWrapper* status) override
			{
				m_replicator->startSavepoint(status, this);
			}

			void releaseSavepoint(ScratchBird::CheckStatusWrapper* status) override
			{
				m_replicator->releaseSavepoint(status, this);
			}

			void rollbackSavepoint(ScratchBird::CheckStatusWrapper* status) override
			{
				m_replicator->rollbackSavepoint(status, this);
			}

			void deprecatedInsertRecord(ScratchBird::CheckStatusWrapper* status, const char* name,
				ScratchBird::IReplicatedRecord* record) override
			{
				insertRecord2(status, nullptr, name, record);
			}

			void insertRecord2(ScratchBird::CheckStatusWrapper* status, const char* schemaName, const char* tableName,
				ScratchBird::IReplicatedRecord* record) override
			{
				m_replicator->insertRecord(status, this, schemaName, tableName, record);
			}

			void deprecatedUpdateRecord(ScratchBird::CheckStatusWrapper* status, const char* name,
				ScratchBird::IReplicatedRecord* orgRecord, ScratchBird::IReplicatedRecord* newRecord) override
			{
				updateRecord2(status, nullptr, name, orgRecord, newRecord);
			}

			void updateRecord2(ScratchBird::CheckStatusWrapper* status, const char* schemaName, const char* tableName,
				ScratchBird::IReplicatedRecord* orgRecord,
							  ScratchBird::IReplicatedRecord* newRecord) override
			{
				m_replicator->updateRecord(status, this, schemaName, tableName, orgRecord, newRecord);
			}

			void deprecatedDeleteRecord(ScratchBird::CheckStatusWrapper* status, const char* name,
				ScratchBird::IReplicatedRecord* record) override
			{
				deleteRecord2(status, nullptr, name, record);
			}

			void deleteRecord2(ScratchBird::CheckStatusWrapper* status, const char* schemaName, const char* tableName,
				ScratchBird::IReplicatedRecord* record) override
			{
				m_replicator->deleteRecord(status, this, schemaName, tableName, record);
			}

			void deprecatedExecuteSql(ScratchBird::CheckStatusWrapper* status, const char* sql) override
			{
				m_replicator->executeSql(status, this, sql);
			}

			void deprecatedExecuteSqlIntl(ScratchBird::CheckStatusWrapper* status, unsigned charset,
				const char* sql) override
			{
				executeSqlIntl2(status, charset, "", sql);
			}

			void executeSqlIntl2(ScratchBird::CheckStatusWrapper* status, unsigned charset, const char* schemaSearchPath,
				const char* sql) override
			{
				m_replicator->executeSqlIntl(status, this, charset, schemaSearchPath, sql);
			}

		private:
			Replicator* const m_replicator;
			ScratchBird::RefPtr<ScratchBird::ITransaction> m_transaction;
			BatchBlock m_data;
		};

		struct GeneratorValue
		{
			Jrd::QualifiedName name;
			SINT64 value;
		};

		typedef ScratchBird::Array<GeneratorValue> GeneratorCache;

		enum FlushReason
		{
			FLUSH_OVERFLOW,
			FLUSH_PREPARE,
			FLUSH_SYNC
		};

	public:
		Replicator(ScratchBird::MemoryPool& pool,
				   Manager* manager,
				   const ScratchBird::Guid& dbGuid,
				   const ScratchBird::MetaString& userName);

		// IReplicatedSession methods

		FB_BOOLEAN init(ScratchBird::CheckStatusWrapper* /*status*/, ScratchBird::IAttachment* att) override
		{
			m_attachment = att;
			return FB_TRUE;
		}

		ScratchBird::IReplicatedTransaction* startTransaction(ScratchBird::CheckStatusWrapper* status, ScratchBird::ITransaction* trans, SINT64 number) override;
		void cleanupTransaction(ScratchBird::CheckStatusWrapper* status, SINT64 number) override;

		void deprecatedSetSequence(ScratchBird::CheckStatusWrapper* status, const char* name, SINT64 value) override
		{
			setSequence2(status, nullptr, name, value);
		}

		void setSequence2(ScratchBird::CheckStatusWrapper* status, const char* schemaName, const char* genName,
			SINT64 value) override;

	private:
		Manager* const m_manager;
		const Config* const m_config;
		ScratchBird::Guid m_guid;
		const ScratchBird::MetaString m_user;
		ScratchBird::Array<Transaction*> m_transactions;
		GeneratorCache m_generators;
		ScratchBird::RefPtr<ScratchBird::IAttachment> m_attachment;

		void initialize();
		void flush(BatchBlock& txnData, FlushReason reason, ULONG flags = 0);
		// Blob id is passed by value because BlobWrapper requires reference to non-const ISC_QUAD
		void storeBlob(Transaction* transaction, ISC_QUAD blobId);
		void releaseTransaction(Transaction* transaction);

		void prepareTransaction(ScratchBird::CheckStatusWrapper* status, Transaction* transaction);
		void commitTransaction(ScratchBird::CheckStatusWrapper* status, Transaction* transaction);
		void rollbackTransaction(ScratchBird::CheckStatusWrapper* status, Transaction* transaction);

		void startSavepoint(ScratchBird::CheckStatusWrapper* status, Transaction* transaction);
		void releaseSavepoint(ScratchBird::CheckStatusWrapper* status, Transaction* transaction);
		void rollbackSavepoint(ScratchBird::CheckStatusWrapper* status, Transaction* transaction);

		void insertRecord(ScratchBird::CheckStatusWrapper* status, Transaction* transaction,
						  const char* schemaName, const char* tableName,
						  ScratchBird::IReplicatedRecord* record);
		void updateRecord(ScratchBird::CheckStatusWrapper* status, Transaction* transaction,
						  const char* schemaName, const char* tableName,
						  ScratchBird::IReplicatedRecord* orgRecord,
						  ScratchBird::IReplicatedRecord* newRecord);
		void deleteRecord(ScratchBird::CheckStatusWrapper* status, Transaction* transaction,
						  const char* schemaName, const char* tableName,
						  ScratchBird::IReplicatedRecord* record);

		void executeSql(ScratchBird::CheckStatusWrapper* status, Transaction* transaction,
						const char* sql)
		{
			executeSqlIntl(status, transaction, CS_UTF8, "", sql);
		}

		void executeSqlIntl(ScratchBird::CheckStatusWrapper* status, Transaction* transaction,
							unsigned charset,
							const char* schemaSearchPath,
							const char* sql);
};

} // namespace

#endif // JRD_REPLICATION_REPLICATOR_H
