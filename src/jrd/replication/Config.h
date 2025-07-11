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


#ifndef JRD_REPLICATION_CONFIG_H
#define JRD_REPLICATION_CONFIG_H

#include <optional>

#include "../common/classes/array.h"
#include "../common/classes/objects_array.h"
#include "../common/classes/fb_string.h"
#include "../common/os/guid.h"

namespace Replication
{
	struct SyncReplica
	{
		explicit SyncReplica(MemoryPool& p)
			: database(p), username(p), password(p)
		{}

		SyncReplica(MemoryPool& p, const SyncReplica& other)
			: database(p, other.database), username(p, other.username), password(p, other.password)
		{}

		ScratchBird::string database;
		ScratchBird::string username;
		ScratchBird::string password;
	};

	struct Config : public ScratchBird::GlobalStorage
	{
		typedef ScratchBird::HalfStaticArray<Config*, 4> ReplicaList;

		Config();
		Config(const Config& other);

		static Config* get(const ScratchBird::PathName& dbName);
		static void enumerate(ReplicaList& replicas);
		static void splitConnectionString(const ScratchBird::string& input, ScratchBird::string& database,
										  ScratchBird::string& username, ScratchBird::string& password);

		ScratchBird::PathName dbName;
		ULONG bufferSize;
		ScratchBird::string includeSchemaFilter;
		ScratchBird::string excludeSchemaFilter;
		ScratchBird::string includeFilter;
		ScratchBird::string excludeFilter;
		ULONG segmentSize;
		ULONG segmentCount;
		ScratchBird::PathName journalDirectory;
		ScratchBird::PathName filePrefix;
		ULONG groupFlushDelay;
		ScratchBird::PathName archiveDirectory;
		ScratchBird::string archiveCommand;
		ULONG archiveTimeout;
		ScratchBird::ObjectsArray<SyncReplica> syncReplicas;
		ScratchBird::PathName sourceDirectory;
		std::optional<ScratchBird::Guid> sourceGuid;
		bool verboseLogging;
		ULONG applyIdleTimeout;
		ULONG applyErrorTimeout;
		ScratchBird::string schemaSearchPath;
		ScratchBird::string pluginName;
		bool logErrors;
		bool reportErrors;
		bool disableOnError;
		bool cascadeReplication;
	};
};

#endif // JRD_REPLICATION_CONFIG_H
