/*
 *	PROGRAM:	SQL Trace plugin
 *	MODULE:		TracePluginImpl.h
 *	DESCRIPTION:	Plugin implementation
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

#ifndef TRACEPLUGINIMPL_H
#define TRACEPLUGINIMPL_H

#include "firebird.h"
#include "../../jrd/ntrace.h"
#include "TracePluginConfig.h"
#include "../../common/SimilarToRegex.h"
#include "../../common/classes/rwlock.h"
#include "../../common/classes/GenericMap.h"
#include "../../common/classes/locks.h"
#include "../../common/classes/ImplementHelper.h"

// Bring in off_t
#include <sys/types.h>

class TracePluginImpl final :
	public ScratchBird::RefCntIface<ScratchBird::ITracePluginImpl<TracePluginImpl, ScratchBird::CheckStatusWrapper> >
{
public:
	// Serialize exception to TLS buffer to return it to user
	static const char* marshal_exception(const ScratchBird::Exception& ex);

	// Data for tracked (active) connections
	struct ConnectionData
	{
		AttNumber id;
		ScratchBird::string* description;

		// Deallocate memory used by objects hanging off this structure
		void deallocate_references()
		{
			delete description;
			description = NULL;
		}

		static const AttNumber& generate(const void* /*sender*/, const ConnectionData& item)
		{
			return item.id;
		}
	};

	typedef ScratchBird::BePlusTree<ConnectionData, AttNumber, ConnectionData> ConnectionsTree;

	// Data for tracked (active) transactions
	struct TransactionData
	{
		TraNumber id;
		ScratchBird::string* description;

		// Deallocate memory used by objects hanging off this structure
		void deallocate_references()
		{
			delete description;
			description = NULL;
		}

		static const TraNumber& generate(const void* /*sender*/, const TransactionData& item)
		{
			return item.id;
		}
	};

	typedef ScratchBird::BePlusTree<TransactionData, TraNumber, TransactionData> TransactionsTree;

	// Data for tracked (active) statements
	struct StatementData
	{
		StmtNumber id;
		ScratchBird::string* description; // NULL in this field indicates that tracing of this statement is not desired

		static const StmtNumber& generate(const void* /*sender*/, const StatementData& item)
		{
			return item.id;
		}
	};

	typedef ScratchBird::BePlusTree<StatementData, StmtNumber, StatementData> StatementsTree;

	typedef void* ServiceId;
	struct ServiceData
	{
		ServiceId id;
		ScratchBird::string* description;
		bool enabled;

		// Deallocate memory used by objects hanging off this structure
		void deallocate_references()
		{
			delete description;
			description = NULL;
		}

		static const ServiceId& generate(const void* /*sender*/, const ServiceData& item)
		{
			return item.id;
		}
	};

	typedef ScratchBird::BePlusTree<ServiceData, ServiceId, ServiceData> ServicesTree;

	template <class C>
	struct RoutineHelper
	{
		C* const routine;
	};

	typedef ScratchBird::SortedArray<StmtNumber> RoutinesList;

	TracePluginImpl(ScratchBird::IPluginBase* factory, const TracePluginConfig& configuration, ScratchBird::ITraceInitInfo* initInfo);

private:
	~TracePluginImpl();

	// Used to not allow to destroy plugin factory and unload dll\so if
	// instance of TracePluginImpl class exists
	ScratchBird::RefPtr<ScratchBird::IPluginBase> factory;

	bool operational; // Set if plugin is fully initialized and is ready for logging
					  // Keep this member field first to ensure its correctness
					  // when destructor is called
	const int session_id;				// trace session ID, set by ScratchBird
	ScratchBird::string session_name;		// trace session name, set by ScratchBird
	ScratchBird::ITraceLogWriter* logWriter;
	TracePluginConfig config;	// Immutable, thus thread-safe
	ScratchBird::string record;

	// Data for currently active connections, transactions, statements
	ScratchBird::RWLock connectionsLock;
	ConnectionsTree connections;

	ScratchBird::RWLock transactionsLock;
	TransactionsTree transactions;

	ScratchBird::RWLock statementsLock;
	StatementsTree statements;

	ScratchBird::RWLock servicesLock;
	ServicesTree services;

	ScratchBird::RWLock routinesLock;
	RoutinesList routines;

	// Lock for log rotation
	ScratchBird::RWLock renameLock;

	ScratchBird::AutoPtr<ScratchBird::SimilarToRegex> include_matcher, exclude_matcher;

	// Filters for gds error codes
	typedef ScratchBird::SortedArray<ISC_STATUS> GdsCodesArray;
	GdsCodesArray include_codes;
	GdsCodesArray exclude_codes;

	void appendGlobalCounts(const ScratchBird::PerformanceInfo* info);
	void appendTableCounts(const ScratchBird::PerformanceInfo* info);
	void appendParams(ScratchBird::ITraceParams* params);
	void appendServiceQueryParams(size_t send_item_length, const ntrace_byte_t* send_items,
								  size_t recv_item_length, const ntrace_byte_t* recv_items);
	void formatStringArgument(ScratchBird::string& result, const UCHAR* str, size_t len);
	bool filterStatus(const ISC_STATUS* status, GdsCodesArray& arr);
	void str2Array(const ScratchBird::string& str, GdsCodesArray& arr);

	// register various objects
	void register_connection(ScratchBird::ITraceDatabaseConnection* connection);
	void register_transaction(ScratchBird::ITraceTransaction* transaction);
	void register_sql_statement(ScratchBird::ITraceSQLStatement* statement);
	void register_blr_statement(ScratchBird::ITraceBLRStatement* statement);
	void register_service(ScratchBird::ITraceServiceConnection* service);

	void register_procedure(ScratchBird::ITraceProcedure* procedure);
	void register_function(ScratchBird::ITraceFunction* function);
	void register_trigger(ScratchBird::ITraceTrigger* trigger);

	bool checkServiceFilter(ScratchBird::ITraceServiceConnection* service, bool started);

	bool checkRoutine(StmtNumber stmt_id);
	template <class C> ScratchBird::string getPlan(C* routine);

	// Write message to text log file
	void logRecord(const char* action);
	void logRecordConn(const char* action, ScratchBird::ITraceDatabaseConnection* connection);
	void logRecordTrans(const char* action, ScratchBird::ITraceDatabaseConnection* connection,
		ScratchBird::ITraceTransaction* transaction);
	void logRecordProc(const char* action, ScratchBird::ITraceDatabaseConnection* connection,
		ScratchBird::ITraceTransaction* transaction, ScratchBird::ITraceProcedure* procedure);
	void logRecordFunc(const char* action, ScratchBird::ITraceDatabaseConnection* connection,
		ScratchBird::ITraceTransaction* transaction, ScratchBird::ITraceFunction* function);
	void logRecordTrig(const char* action, ScratchBird::ITraceDatabaseConnection* connection,
		ScratchBird::ITraceTransaction* transaction, ScratchBird::ITraceTrigger* trigger);
	void logRecordStmt(const char* action, ScratchBird::ITraceDatabaseConnection* connection,
		ScratchBird::ITraceTransaction* transaction, ScratchBird::ITraceStatement* statement,
		bool isSQL);
	void logRecordServ(const char* action, ScratchBird::ITraceServiceConnection* service);
	void logRecordError(const char* action, ScratchBird::ITraceConnection* connection, ScratchBird::ITraceStatusVector* status);

	/* Methods which do logging of events to file */
	void log_init();
	void log_finalize();

	void log_event_attach(
		ScratchBird::ITraceDatabaseConnection* connection, FB_BOOLEAN create_db,
		unsigned att_result);
	void log_event_detach(
		ScratchBird::ITraceDatabaseConnection* connection, FB_BOOLEAN drop_db);

	void log_event_transaction_start(
		ScratchBird::ITraceDatabaseConnection* connection, ScratchBird::ITraceTransaction* transaction,
		size_t tpb_length, const ntrace_byte_t* tpb, unsigned tra_result);
	void log_event_transaction_end(
		ScratchBird::ITraceDatabaseConnection* connection, ScratchBird::ITraceTransaction* transaction,
		FB_BOOLEAN commit, FB_BOOLEAN retain_context, unsigned tra_result);

	void log_event_set_context(
		ScratchBird::ITraceDatabaseConnection* connection, ScratchBird::ITraceTransaction* transaction,
		ScratchBird::ITraceContextVariable* variable);

	void log_event_proc_compile(
		ScratchBird::ITraceDatabaseConnection* connection,
		ScratchBird::ITraceProcedure* procedure, ntrace_counter_t time_millis, unsigned proc_result);
	void log_event_proc_execute(
		ScratchBird::ITraceDatabaseConnection* connection, ScratchBird::ITraceTransaction* transaction,
		ScratchBird::ITraceProcedure* procedure, bool started, unsigned proc_result);

	void log_event_func_compile(
		ScratchBird::ITraceDatabaseConnection* connection,
		ScratchBird::ITraceFunction* function, ntrace_counter_t time_millis, unsigned func_result);
	void log_event_func_execute(
		ScratchBird::ITraceDatabaseConnection* connection, ScratchBird::ITraceTransaction* transaction,
		ScratchBird::ITraceFunction* function, bool started, unsigned func_result);

	void log_event_trigger_compile(
		ScratchBird::ITraceDatabaseConnection* connection,
		ScratchBird::ITraceTrigger* trigger, ntrace_counter_t time_millis, unsigned trig_result);
	void log_event_trigger_execute(
		ScratchBird::ITraceDatabaseConnection* connection, ScratchBird::ITraceTransaction* transaction,
		ScratchBird::ITraceTrigger* trigger, bool started, unsigned trig_result);

	void log_event_dsql_prepare(
		ScratchBird::ITraceDatabaseConnection* connection, ScratchBird::ITraceTransaction* transaction,
		ScratchBird::ITraceSQLStatement* statement, ntrace_counter_t time_millis, unsigned req_result);
	void log_event_dsql_free(
		ScratchBird::ITraceDatabaseConnection* connection, ScratchBird::ITraceSQLStatement* statement, unsigned short option);
	void log_event_dsql_execute(
		ScratchBird::ITraceDatabaseConnection* connection, ScratchBird::ITraceTransaction* transaction,
		ScratchBird::ITraceSQLStatement* statement, bool started, unsigned number, unsigned req_result);

	void log_event_blr_compile(
		ScratchBird::ITraceDatabaseConnection* connection, ScratchBird::ITraceTransaction* transaction,
		ScratchBird::ITraceBLRStatement* statement, ntrace_counter_t time_millis, unsigned req_result);
	void log_event_blr_execute(
		ScratchBird::ITraceDatabaseConnection* connection, ScratchBird::ITraceTransaction* transaction,
		ScratchBird::ITraceBLRStatement* statement, unsigned req_result);

	void log_event_dyn_execute(
		ScratchBird::ITraceDatabaseConnection* connection, ScratchBird::ITraceTransaction* transaction,
		ScratchBird::ITraceDYNRequest* request, ntrace_counter_t time_millis,
		unsigned req_result);

	void log_event_service_attach(ScratchBird::ITraceServiceConnection* service, unsigned att_result);
	void log_event_service_start(ScratchBird::ITraceServiceConnection* service, size_t switches_length, const char* switches,
								 unsigned start_result);
	void log_event_service_query(ScratchBird::ITraceServiceConnection* service, size_t send_item_length,
								 const ntrace_byte_t* send_items, size_t recv_item_length,
								 const ntrace_byte_t* recv_items, unsigned query_result);
	void log_event_service_detach(ScratchBird::ITraceServiceConnection* service, unsigned detach_result);

	void log_event_error(ScratchBird::ITraceConnection* connection, ScratchBird::ITraceStatusVector* status, const char* function);

	void log_event_sweep(ScratchBird::ITraceDatabaseConnection* connection, ScratchBird::ITraceSweepInfo* sweep,
		ntrace_process_state_t sweep_state);

public:
	// TracePlugin implementation
	const char* trace_get_error();

	// Create/close attachment
	FB_BOOLEAN trace_attach(ScratchBird::ITraceDatabaseConnection* connection, FB_BOOLEAN create_db, unsigned att_result);
	FB_BOOLEAN trace_detach(ScratchBird::ITraceDatabaseConnection* connection, FB_BOOLEAN drop_db);

	// Start/end transaction
	FB_BOOLEAN trace_transaction_start(ScratchBird::ITraceDatabaseConnection* connection, ScratchBird::ITraceTransaction* transaction,
			unsigned tpb_length, const ntrace_byte_t* tpb, unsigned tra_result);
	FB_BOOLEAN trace_transaction_end(ScratchBird::ITraceDatabaseConnection* connection, ScratchBird::ITraceTransaction* transaction,
			FB_BOOLEAN commit, FB_BOOLEAN retain_context, unsigned tra_result);

	// Stored procedures, functions and triggers compilation and execution
	FB_BOOLEAN trace_proc_compile(ScratchBird::ITraceDatabaseConnection* connection,
			ScratchBird::ITraceProcedure* procedure, ISC_INT64 time_millis, unsigned proc_result);
	FB_BOOLEAN trace_proc_execute(ScratchBird::ITraceDatabaseConnection* connection, ScratchBird::ITraceTransaction* transaction,
			ScratchBird::ITraceProcedure* procedure, FB_BOOLEAN started, unsigned proc_result);
	FB_BOOLEAN trace_func_compile(ScratchBird::ITraceDatabaseConnection* connection,
			ScratchBird::ITraceFunction* function, ISC_INT64 time_millis, unsigned func_result);
	FB_BOOLEAN trace_func_execute(ScratchBird::ITraceDatabaseConnection* connection, ScratchBird::ITraceTransaction* transaction,
			ScratchBird::ITraceFunction* function, FB_BOOLEAN started, unsigned func_result);
	FB_BOOLEAN trace_trigger_compile(ScratchBird::ITraceDatabaseConnection* connection,
			ScratchBird::ITraceTrigger* trigger, ISC_INT64 time_millis, unsigned trig_result);
	FB_BOOLEAN trace_trigger_execute(ScratchBird::ITraceDatabaseConnection* connection, ScratchBird::ITraceTransaction* transaction,
			ScratchBird::ITraceTrigger* trigger, FB_BOOLEAN started, unsigned trig_result);

	// Assignment to context variables
	FB_BOOLEAN trace_set_context(ScratchBird::ITraceDatabaseConnection* connection, ScratchBird::ITraceTransaction* transaction,
			ScratchBird::ITraceContextVariable* variable);

	// DSQL statement lifecycle
	FB_BOOLEAN trace_dsql_prepare(ScratchBird::ITraceDatabaseConnection* connection, ScratchBird::ITraceTransaction* transaction,
			ScratchBird::ITraceSQLStatement* statement, ISC_INT64 time_millis, unsigned req_result);
	FB_BOOLEAN trace_dsql_free(ScratchBird::ITraceDatabaseConnection* connection, ScratchBird::ITraceSQLStatement* statement,
			unsigned option);
	FB_BOOLEAN trace_dsql_execute(ScratchBird::ITraceDatabaseConnection* connection, ScratchBird::ITraceTransaction* transaction,
			ScratchBird::ITraceSQLStatement* statement, FB_BOOLEAN started, unsigned req_result);
	FB_BOOLEAN trace_dsql_restart(ScratchBird::ITraceDatabaseConnection* connection, ScratchBird::ITraceTransaction* transaction,
			ScratchBird::ITraceSQLStatement* statement, unsigned number);

	// BLR requests
	FB_BOOLEAN trace_blr_compile(ScratchBird::ITraceDatabaseConnection* connection, ScratchBird::ITraceTransaction* transaction,
			ScratchBird::ITraceBLRStatement* statement, ISC_INT64 time_millis, unsigned req_result);
	FB_BOOLEAN trace_blr_execute(ScratchBird::ITraceDatabaseConnection* connection, ScratchBird::ITraceTransaction* transaction,
			ScratchBird::ITraceBLRStatement* statement, unsigned req_result);

	// DYN requests
	FB_BOOLEAN trace_dyn_execute(ScratchBird::ITraceDatabaseConnection* connection, ScratchBird::ITraceTransaction* transaction,
			ScratchBird::ITraceDYNRequest* request, ISC_INT64 time_millis, unsigned req_result);

	// Using the services
	FB_BOOLEAN trace_service_attach(ScratchBird::ITraceServiceConnection* service, unsigned att_result);
	FB_BOOLEAN trace_service_start(ScratchBird::ITraceServiceConnection* service, unsigned switches_length, const char* switches,
			unsigned start_result);
	FB_BOOLEAN trace_service_query(ScratchBird::ITraceServiceConnection* service, unsigned send_item_length,
			const ntrace_byte_t* send_items, unsigned recv_item_length,
			const ntrace_byte_t* recv_items, unsigned query_result);
	FB_BOOLEAN trace_service_detach(ScratchBird::ITraceServiceConnection* service, unsigned detach_result);

	// Errors happened
	virtual FB_BOOLEAN trace_event_error(ScratchBird::ITraceConnection* connection, ScratchBird::ITraceStatusVector* status,
			const char* function);

	// Sweep activity
	virtual FB_BOOLEAN trace_event_sweep(ScratchBird::ITraceDatabaseConnection* connection,
			ScratchBird::ITraceSweepInfo* sweep, ntrace_process_state_t sweep_state);
};


#endif // TRACEPLUGINIMPL_H
