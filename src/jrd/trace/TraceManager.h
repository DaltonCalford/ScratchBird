/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		TraceManager.h
 *	DESCRIPTION:	Trace API manager
 *
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
 *  The Original Code was created by Nickolay Samofatov
 *  for the ScratchBird Open Source RDBMS project.
 *
 *  Copyright (c) 2004 Nickolay Samofatov <nickolay@broadviewsoftware.com>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 *
 *  2008 Khorsun Vladyslav
 */

#ifndef JRD_TRACEMANAGER_H
#define JRD_TRACEMANAGER_H

#include <time.h>
#include "../../jrd/ntrace.h"
#include "../../common/classes/array.h"
#include "../../common/classes/fb_string.h"
#include "../../common/classes/init.h"
#include "../../common/classes/rwlock.h"
#include "../../common/classes/ImplementHelper.h"
#include "../../jrd/trace/TraceConfigStorage.h"
#include "../../jrd/trace/TraceSession.h"

namespace ScratchBird {

class ICryptKeyCallback;

}

namespace Jrd {

class Database;
class Attachment;
class jrd_tra;
class DsqlRequest;
class Service;

class TraceManager
{
public:
    /* Initializes plugins. */
	explicit TraceManager(Attachment* in_att);
	explicit TraceManager(Service* in_svc);
	TraceManager(const char* in_filename, ScratchBird::ICryptKeyCallback* callback, bool failedAttach);

	/* Finalize plugins. Called when database is closed by the engine */
	~TraceManager();

	static ConfigStorage* getStorage()
	{ return storageInstance->getStorage(); }

	static size_t pluginsCount()
	{ return factories->getCount(); }

	void event_attach(ScratchBird::ITraceDatabaseConnection* connection, bool create_db,
		ntrace_result_t att_result);

	void event_detach(ScratchBird::ITraceDatabaseConnection* connection, bool drop_db);

	/* Start/end transaction */
	void event_transaction_start(ScratchBird::ITraceDatabaseConnection* connection, ScratchBird::ITraceTransaction* transaction,
		unsigned tpb_length, const ntrace_byte_t* tpb, ntrace_result_t tra_result);

	void event_transaction_end(ScratchBird::ITraceDatabaseConnection* connection, ScratchBird::ITraceTransaction* transaction,
		bool commit, bool retain_context, ntrace_result_t tra_result);

	void event_set_context(ScratchBird::ITraceDatabaseConnection* connection,
		ScratchBird::ITraceTransaction* transaction, ScratchBird::ITraceContextVariable* variable);

	void event_proc_compile(ScratchBird::ITraceDatabaseConnection* connection,
		ScratchBird::ITraceProcedure* procedure, ntrace_counter_t time_millis, ntrace_result_t proc_result);

	void event_proc_execute(ScratchBird::ITraceDatabaseConnection* connection, ScratchBird::ITraceTransaction* transaction,
		ScratchBird::ITraceProcedure* procedure, bool started, ntrace_result_t proc_result);

	void event_func_compile(ScratchBird::ITraceDatabaseConnection* connection,
		ScratchBird::ITraceFunction* function, ntrace_counter_t time_millis, ntrace_result_t func_result);

	void event_func_execute(ScratchBird::ITraceDatabaseConnection* connection, ScratchBird::ITraceTransaction* transaction,
		ScratchBird::ITraceFunction* function, bool started, ntrace_result_t func_result);

	void event_trigger_compile(ScratchBird::ITraceDatabaseConnection* connection,
		ScratchBird::ITraceTrigger* trigger, ntrace_counter_t time_millis, ntrace_result_t trig_result);

	void event_trigger_execute(ScratchBird::ITraceDatabaseConnection* connection, ScratchBird::ITraceTransaction* transaction,
		ScratchBird::ITraceTrigger* trigger, bool started, ntrace_result_t trig_result);

	void event_blr_compile(ScratchBird::ITraceDatabaseConnection* connection,
		ScratchBird::ITraceTransaction* transaction, ScratchBird::ITraceBLRStatement* statement,
		ntrace_counter_t time_millis, ntrace_result_t req_result);

	void event_blr_execute(ScratchBird::ITraceDatabaseConnection* connection,
		ScratchBird::ITraceTransaction* transaction, ScratchBird::ITraceBLRStatement* statement,
		ntrace_result_t req_result);

	void event_dyn_execute(ScratchBird::ITraceDatabaseConnection* connection,
		ScratchBird::ITraceTransaction* transaction, ScratchBird::ITraceDYNRequest* request,
		ntrace_counter_t time_millis, ntrace_result_t req_result);

	void event_service_attach(ScratchBird::ITraceServiceConnection* service, ntrace_result_t att_result);

	void event_service_start(ScratchBird::ITraceServiceConnection* service,
		unsigned switches_length, const char* switches,
		ntrace_result_t start_result);

	void event_service_query(ScratchBird::ITraceServiceConnection* service,
		unsigned send_item_length, const ntrace_byte_t* send_items,
		unsigned recv_item_length, const ntrace_byte_t* recv_items,
		ntrace_result_t query_result);

	void event_service_detach(ScratchBird::ITraceServiceConnection* service, ntrace_result_t detach_result);

	void event_error(ScratchBird::ITraceConnection* connection, ScratchBird::ITraceStatusVector* status, const char* function);

	void event_sweep(ScratchBird::ITraceDatabaseConnection* connection, ScratchBird::ITraceSweepInfo* sweep,
		ntrace_process_state_t sweep_state);

	typedef ntrace_mask_t NotificationNeeds;

	inline bool needs(unsigned e)
	{
		if (!active || !init_factories)
			return false;

		if (changeNumber != getStorage()->getChangeNumber())
			update_sessions();

		return trace_needs & (FB_CONST64(1) << e);
	}

	// should be called after attachment user is authenticated
	void activate()
	{
		active = true;
	}

	// helps avoid early use
	bool isActive()
	{
		return active;
	}

	// external access to stored attachment
	Attachment* getAttachment()
	{
		return attachment;
	}

	/* DSQL-friendly routines to call Trace API hooks.
       Needed because DSQL cannot include JRD for the current engine */
	static bool need_dsql_prepare(Attachment* att);
	static bool need_dsql_free(Attachment* att);
	static bool need_dsql_execute(Attachment* att);

	static void event_dsql_prepare(Attachment* att, jrd_tra* transaction, ScratchBird::ITraceSQLStatement* statement,
		ntrace_counter_t time_millis, ntrace_result_t req_result);

	static void event_dsql_free(Attachment* att, ScratchBird::ITraceSQLStatement* statement,
		unsigned short option);

	static void event_dsql_execute(Attachment* att, jrd_tra* transaction, ScratchBird::ITraceSQLStatement* statement,
		bool started, ntrace_result_t req_result);

	static void event_dsql_restart(Attachment* att, jrd_tra* transaction, DsqlRequest* statement, const UCHAR* data,
		int number);

	static void shutdown();

private:
	Attachment*	attachment;
	Service* service;
	const char* filename;
	ScratchBird::ICryptKeyCallback* callback;
	NotificationNeeds trace_needs, new_needs;

	// This structure should be POD-like to be stored in Array
	struct FactoryInfo
	{
		FactoryInfo() : factory(NULL)
		{
			memset(name, 0, sizeof(name));
		}

		ScratchBird::ITraceFactory* factory;
		char name[MAXPATHLEN];
	};

	class Factories : public ScratchBird::Array<FactoryInfo>
	{
	public:
		explicit Factories(ScratchBird::MemoryPool& p)
			: ScratchBird::Array<FactoryInfo>(p)
		{ }

		~Factories()
		{
			ScratchBird::PluginManagerInterfacePtr pi;

			for (unsigned int i = 0; i < getCount(); ++i)
				pi->releasePlugin(getElement(i).factory);
		}
	};

	static Factories* factories;
	static ScratchBird::GlobalPtr<ScratchBird::RWLock> init_factories_lock;
	static volatile bool init_factories;

	struct SessionInfo
	{
		FactoryInfo* factory_info;
		ScratchBird::ITracePlugin* plugin;
		ULONG ses_id;

		static ULONG generate(const SessionInfo& item)
		{ return item.ses_id; }
	};
	class Sessions : public ScratchBird::SortedArray<SessionInfo, ScratchBird::EmptyStorage<SessionInfo>, ULONG, SessionInfo>
	{
	public:
		explicit Sessions(MemoryPool& p)
			: ScratchBird::SortedArray<SessionInfo, ScratchBird::EmptyStorage<SessionInfo>, ULONG, SessionInfo>(p)
		{ }

		~Sessions()
		{
			for (unsigned int i = 0; i < getCount(); ++i)
			{
				getElement(i).plugin->release();
			}
		}
	};
	Sessions trace_sessions;

	void init();
	void load_plugins();
	void update_sessions();
	void update_session(const ScratchBird::TraceSession& session);

	bool check_result(ScratchBird::ITracePlugin* plugin, const char* module, const char* function, bool result);

	/* DSQL statement lifecycle. To be moved to public and used directly when DSQL becomes a part of JRD */
	void event_dsql_prepare(ScratchBird::ITraceDatabaseConnection* connection, ScratchBird::ITraceTransaction* transaction,
		ScratchBird::ITraceSQLStatement* statement,
		ntrace_counter_t time_millis, ntrace_result_t req_result);

	void event_dsql_free(ScratchBird::ITraceDatabaseConnection* connection,
		ScratchBird::ITraceSQLStatement* statement, unsigned short option);

	void event_dsql_execute(ScratchBird::ITraceDatabaseConnection* connection, ScratchBird::ITraceTransaction* transaction,
		ScratchBird::ITraceSQLStatement* statement,
		bool started, ntrace_result_t req_result);

	void event_dsql_restart(ScratchBird::ITraceDatabaseConnection* connection, ScratchBird::ITraceTransaction* transaction, ScratchBird::ITraceSQLStatement* statement,
		unsigned number);

	static ScratchBird::GlobalPtr<StorageInstance, ScratchBird::InstanceControl::PRIORITY_DELETE_FIRST> storageInstance;

	ULONG changeNumber;
	bool active, failedAttach;
};

}

#endif
