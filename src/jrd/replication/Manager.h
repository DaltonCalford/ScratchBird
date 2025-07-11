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
 *  Copyright (c) 2014 Dmitry Yemanov <dimitr@firebirdsql.org>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */


#ifndef JRD_REPLICATION_MANAGER_H
#define JRD_REPLICATION_MANAGER_H

#include "../common/classes/array.h"
#include "../common/classes/semaphore.h"
#include "../common/SimilarToRegex.h"
#include "../common/os/guid.h"
#include "../../jrd/QualifiedName.h"
#include "../common/isc_s_proto.h"
#include "../../jrd/intl_classes.h"

#include "Config.h"
#include "ChangeLog.h"

namespace Replication
{
	class TableMatcher
	{
		typedef ScratchBird::GenericMap<ScratchBird::Pair<ScratchBird::Left<Jrd::QualifiedName, bool> > > TablePermissionMap;

	public:
		TableMatcher(MemoryPool& pool,
					 const ScratchBird::string& includeSchemaFilter,
					 const ScratchBird::string& excludeSchemaFilter,
					 const ScratchBird::string& includeFilter,
					 const ScratchBird::string& excludeFilter);

		bool matchTable(const Jrd::QualifiedName& tableName);

	private:
		ScratchBird::AutoPtr<ScratchBird::SimilarToRegex> m_includeSchemaMatcher;
		ScratchBird::AutoPtr<ScratchBird::SimilarToRegex> m_excludeSchemaMatcher;
		ScratchBird::AutoPtr<ScratchBird::SimilarToRegex> m_includeMatcher;
		ScratchBird::AutoPtr<ScratchBird::SimilarToRegex> m_excludeMatcher;
		TablePermissionMap m_tables;
	};

	class Manager final : public ScratchBird::GlobalStorage
	{
		struct SyncReplica
		{
			SyncReplica(ScratchBird::MemoryPool& pool, ScratchBird::IAttachment* att, ScratchBird::IReplicator* repl)
				: status(pool), attachment(att), replicator(repl)
			{}

			ScratchBird::FbLocalStatus status;
			ScratchBird::IAttachment* attachment;
			ScratchBird::IReplicator* replicator;
		};

	public:
		Manager(const ScratchBird::string& dbId, const Replication::Config* config);
		~Manager();

		void shutdown();

		ScratchBird::UCharBuffer* getBuffer();
		void releaseBuffer(ScratchBird::UCharBuffer* buffer);

		void flush(ScratchBird::UCharBuffer* buffer, bool sync, bool prepare);

		void forceJournalSwitch()
		{
			if (m_changeLog)
				m_changeLog->forceSwitch();
		}

		const Replication::Config* getConfig() const
		{
			return m_config;
		}

	private:
		void bgWriter();

		static THREAD_ENTRY_DECLARE writer_thread(THREAD_ENTRY_PARAM arg)
		{
			Manager* const mgr = static_cast<Manager*>(arg);
			mgr->bgWriter();
			return 0;
		}

		ScratchBird::Semaphore m_startupSemaphore;
		ScratchBird::Semaphore m_cleanupSemaphore;
		ScratchBird::Semaphore m_workingSemaphore;

		const Replication::Config* const m_config;
		ScratchBird::Array<SyncReplica*> m_replicas;
		ScratchBird::Array<ScratchBird::UCharBuffer*> m_buffers;
		ScratchBird::Mutex m_buffersMutex;
		ScratchBird::Array<ScratchBird::UCharBuffer*> m_queue;
		ScratchBird::Mutex m_queueMutex;
		ULONG m_queueSize;
		FB_UINT64 m_sequence;

		volatile bool m_shutdown;
		volatile bool m_signalled;

		ScratchBird::AutoPtr<ChangeLog> m_changeLog;
		ScratchBird::RWLock m_lock;
	};
}

#endif // JRD_REPLICATION_MANAGER_H
