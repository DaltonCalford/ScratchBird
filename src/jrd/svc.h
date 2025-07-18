/*
 *	PROGRAM:	JRD access method
 *	MODULE:		svc.h
 *	DESCRIPTION:	Service manager declarations
 *
 * The contents of this file are subject to the Interbase Public
 * License Version 1.0 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy
 * of the License at http://www.Inprise.com/IPL.html
 *
 * Software distributed under the License is distributed on an
 * "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, either express
 * or implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code was created by Inprise Corporation
 * and its predecessors. Portions created by Inprise Corporation are
 * Copyright (C) Inprise Corporation.
 *
 * All Rights Reserved.
 * Contributor(s): ______________________________________.
 */

#ifndef JRD_SVC_H
#define JRD_SVC_H

#include <stdio.h>

#include "fb_blk.h"
#include "firebird/impl/consts_pub.h"

#include "../jrd/svc_undoc.h"
#include "../common/ThreadStart.h"

#include "../common/classes/locks.h"
#include "../common/classes/semaphore.h"
#include "../common/classes/array.h"
#include "../common/classes/SafeArg.h"
#include "../common/UtilSvc.h"
#include "../jrd/EngineInterface.h"
#include "../common/classes/Switches.h"
#include "../common/classes/ClumpletReader.h"
#include "../common/classes/RefMutex.h"
#include "../burp/split/spit.h"
#include "../jrd/status.h"

namespace ScratchBird {
	namespace Arg {
		class StatusVector;
	}
}

namespace Jrd {

typedef int ServiceEntry(ScratchBird::UtilSvc*);

struct serv_entry
{
	USHORT				serv_action;		// isc_action_svc_....
	const TEXT*			serv_name;			// service name
	ServiceEntry*		serv_thd;			// thread to execute
};

const ULONG SERVICE_VERSION			= 2;

const int SVC_STDOUT_BUFFER_SIZE	= 1024;

// Flag of capabilities supported by the server
//const ULONG WAL_SUPPORT				= 0x1L;		// Write Ahead Log
const ULONG MULTI_CLIENT_SUPPORT		= 0x2L;		// SuperServer model (vs. multi-inet)
const ULONG REMOTE_HOP_SUPPORT			= 0x4L;		// Server can connect to other server
//const ULONG NO_SVR_STATS_SUPPORT		= 0x8L;		// Does not support statistics

//const ULONG NO_DB_STATS_SUPPORT		= 0x10L;	// Does not support statistics
// Really the 16 bit LIBS here?
//const ULONG LOCAL_ENGINE_SUPPORT		= 0x20L;	// The local 16 bit engine
//const ULONG NO_FORCED_WRITE_SUPPORT	= 0x40L;	// Can not configure sync writes
//const ULONG NO_SHUTDOWN_SUPPORT		= 0x80L;	// Can not shutdown/restart databases
const ULONG NO_SERVER_SHUTDOWN_SUPPORT	= 0x100L;	// Can not shutdown server
//const ULONG SERVER_CONFIG_SUPPORT		= 0x200L;	// Can configure server
const ULONG QUOTED_FILENAME_SUPPORT		= 0x400L;	// Can pass quoted filenames in

// Range definitions for service actions.  Any action outside of
// this range is not supported
const USHORT isc_action_min				= 1;
const USHORT isc_action_max				= isc_action_svc_last;

// Range definitions for service actions.  Any action outside of
// this range is not supported
//define isc_info_min                  50
//define isc_info_max                  67

// Bitmask values for the svc_flags variable
//const int SVC_shutdown	= 0x1;
//const int SVC_timeout		= 0x2;
//const int SVC_forked		= 0x4;
const int SVC_detached		= 0x8;
const int SVC_finished		= 0x10;
//const int SVC_thd_running	= 0x20;
const int SVC_evnt_fired	= 0x40;
const int SVC_cmd_line		= 0x80;
const int SVC_failed_start	= 0x100;

// forward decl.
class thread_db;
class TraceManager;

// Service manager
class Service : public ScratchBird::UtilSvc, public TypedHandle<type_svc>
{
public:		// utilities interface with service
	// output to svc_stdout verbose info
	void outputVerbose(const char* text) override;
	// outpur error text
	void outputError(const char* text) override;
	// output some data to service
	void outputData(const void* data, FB_SIZE_T len) override;
	// printf() to svc_stdout
    void printf(bool err, const SCHAR* format, ...) override;
	// returns true - it's service :)
	bool isService() override;
	// client thread started
	void started() override;
	// put various info items in info buffer
    void putLine(char tag, const char* val) override;
    void putSLong(char tag, SLONG val) override;
    void putSInt64(char tag, SINT64 val) override;
	void putChar(char tag, char val) override;
	// put raw bytes to svc_stdout
	void putBytes(const UCHAR*, FB_SIZE_T) override;
	// get raw bytes from svc_stdin
	ULONG getBytes(UCHAR*, ULONG) override;

private:
	// append status_vector to service's status
	void setServiceStatus(const ISC_STATUS* status_vector) override;
	// append error message to service's status
	void setServiceStatus(const USHORT facility, const USHORT errcode, const MsgFormat::SafeArg& args) override;

public:
	// no-op for services
	void hidePasswd(ArgvType&, int) override;
	// return service status
    StatusAccessor getStatusAccessor() override;
	// no-op for services
	void checkService() override;
	// add address path and utf8 flag (taken from spb) to dpb if present
	void fillDpb(ScratchBird::ClumpletWriter& dpb) override;
	// encoding for string parameters passed to utility
	bool utf8FileNames() override;
	// get database encryption key transfer callback routine
	ScratchBird::ICryptKeyCallback* getCryptCallback() override;
	int getParallelWorkers() override { return svc_parallel_workers; }

	TraceManager* getTraceManager()
	{
		return svc_trace_manager;
	}

	bool finished() override
	{
		return ((svc_flags & (SVC_finished | SVC_detached)) != 0)
			|| checkForShutdown();
	}

	// Get authentication block if present
	unsigned int getAuthBlock(const unsigned char** bytes) override;

public:		// external interface with service
	// Attach - service ctor
	Service(const TEXT* service_name, USHORT spb_length, const UCHAR* spb_data,
		ScratchBird::ICryptKeyCallback* crypt_callback);
	// Start service thread
	void start(USHORT spb_length, const UCHAR* spb_data);
	// Query service state (v. 1 & 2)
	void query(USHORT send_item_length, const UCHAR* send_items, USHORT recv_item_length,
			   const UCHAR* recv_items, USHORT buffer_length, UCHAR* info);
	ISC_STATUS query2(thread_db* tdbb, USHORT send_item_length, const UCHAR* send_items,
			   USHORT recv_item_length, const UCHAR* recv_items, USHORT buffer_length, UCHAR* info);
	// Cancel wait in query service
	void cancel(thread_db* tdbb);
	// Detach from service
	void detach();
	// get service version
	USHORT getVersion() const
	{
		return svc_spb_version;
	}

	// ScratchBird log reader
	static int readFbLog(ScratchBird::UtilSvc* uSvc);
	// Shuts all service threads (should be called after databases shutdown)
	static void shutdownServices();

	// Total number of service attachments
	static ULONG totalCount();

	const char* getServiceMgr() const;
	const char* getServiceName() const;

	const ScratchBird::string&	getUserName() const
	{
		return svc_username;
	}

	const ScratchBird::string&	getRoleName() const
	{
		return svc_sql_role;
	}

	// return true if user have admin privileges in security database used
	// for service user authentication
	bool getUserAdminFlag() const;

	const ScratchBird::string&	getNetworkProtocol() const	{ return svc_network_protocol; }
	const ScratchBird::string&	getRemoteAddress() const	{ return svc_remote_address; }
	const ScratchBird::string&	getRemoteProcess() const	{ return svc_remote_process; }
	int	getRemotePID() const { return svc_remote_pid; }
	const ScratchBird::PathName& getExpectedDb() const		{ return svc_expected_db; }

private:
	// Service must have private destructor, called from finish
	// when both (server and client) threads are finished
	~Service();
	// Find current service in global services list
	bool	locateInAllServices(FB_SIZE_T* posPtr = NULL);
	// Detach self from global services list
	void	removeFromAllServices();
	// The only service, implemented internally
	void	readFbLog();
	// Create argv, argc and svc_parsed_sw
	void	parseSwitches();
	// Check does this action need arg or not
	static bool actionNeedsArg(UCHAR action);
	// Put data into stdout buffer
	void	enqueue(const UCHAR* s, ULONG len);
	// true if there is no data in stdout buffer
	bool	empty(ULONG head) const;
	// true if no more space in stdout buffer
	bool	full() const;
	// start service thread
	void	start(const serv_entry* service_run);
	// Set the flag (either SVC_finished for the main service thread or SVC_detached for the client thread).
	// If both main thread and client thread are completed that is main thread is finished and
	// client is detached then free memory used by service.
	void	finish(USHORT flag);
	// Throws shutdown exception if global flag is set for it
	bool	checkForShutdown();
	// Check for the existence of errors in the service that has not started
	bool	checkForFailedStart();
	// Transfer data from svc_stdout into buffer
	void	get(UCHAR* buffer, USHORT length, USHORT flags, USHORT timeout, USHORT* return_length);
	// Sends stdin for a service
	// Returns number of bytes service wants more
	ULONG	put(const UCHAR* buffer, ULONG length);
	// Copies argument value to status vector
	void put_status_arg(ScratchBird::Arg::StatusVector& status, const MsgFormat::safe_cell& value);

	// Increment circular buffer pointer
	static ULONG		add_one(ULONG i);
	static ULONG		add_val(ULONG i, ULONG val);
	// Convert spb flags to utility switches
#ifndef DEV_BUILD
	static
#endif
	void				conv_switches(ScratchBird::ClumpletReader& spb, ScratchBird::string& switches);
	// Find spb switch in switch table
	static const TEXT*	find_switch(int in_spb_sw, const Switches::in_sw_tab_t* table, bool bitmask);
	// Loop through the appropriate switch table looking for the text for the given command switch
#ifndef DEV_BUILD
	static
#endif
	bool				process_switches(ScratchBird::ClumpletReader& spb, ScratchBird::string& switches);
	// Get bitmask from within spb buffer, find corresponding switches within specified table,
	// add them to the command line
	static bool get_action_svc_bitmask(const ScratchBird::ClumpletReader& spb,
									   const Switches::in_sw_tab_t* table,
									   ScratchBird::string& sw);
	// Get string from within spb buffer, add it to the command line
	static void get_action_svc_string(const ScratchBird::ClumpletReader& spb, ScratchBird::string& sw);
	// Get string from within spb buffer, insert it at given position into command line
	static void get_action_svc_string_pos(const ScratchBird::ClumpletReader& spb, ScratchBird::string& switches,
										  ScratchBird::string::size_type p = ScratchBird::string::npos);
	// Get integer from within spb buffer, add it to the command line
	static void get_action_svc_data(const ScratchBird::ClumpletReader& spb, ScratchBird::string& sw, bool bigint);
	// Get parameter from within spb buffer, find corresponding switch within specified table,
	// add it to the command line
	static bool get_action_svc_parameter(UCHAR tag, const Switches::in_sw_tab_t* table,
										 ScratchBird::string&);
	// Create 'SYSDBA needed' error in status vector
	static void need_admin_privs(ScratchBird::Arg::StatusVector& status, const char* message);
	// Does info buffer have enough space for SLONG?
	static bool ck_space_for_numeric(UCHAR*& info, const UCHAR* const end);
	// Make status vector permamnent, if one present in worker thread's space
	void makePermanentStatusVector() noexcept;
	// Read SPB on attach
	void getOptions(ScratchBird::ClumpletReader&);
	// Invoke appropriate service thread entry and finalize it correctly
	static THREAD_ENTRY_DECLARE run(THREAD_ENTRY_PARAM arg);

private:
	ScratchBird::FbLocalStatus svc_status;				// status vector for running service
	ScratchBird::Mutex svc_status_mutex;				// protects svc_status from access in different threads
	ScratchBird::string svc_parsed_sw;					// Here point elements of argv
	ULONG	svc_stdout_head;
	ULONG	svc_stdout_tail;
	UCHAR	svc_stdout[SVC_STDOUT_BUFFER_SIZE];		// output from service
	ScratchBird::Semaphore	svcStart;
	const serv_entry*	svc_service_run;			// running service's entry
	ScratchBird::Array<UCHAR> svc_resp_alloc;
	UCHAR*	svc_resp_buf;
	const UCHAR*	svc_resp_ptr;
	USHORT	svc_resp_buf_len;
	USHORT	svc_resp_len;
	USHORT	svc_flags;
	USHORT	svc_user_flag;
	USHORT	svc_spb_version;
	bool	svc_shutdown_server;
	bool	svc_shutdown_request;
	bool	svc_shutdown_in_progress;
	bool	svc_timeout;
	char	svc_arg_conv[MsgFormat::SAFEARG_MAX_ARG * 2];
	char*	svc_arg_ptr;

	ScratchBird::string	svc_username;
	ScratchBird::string	svc_sql_role;
	ScratchBird::AuthReader::AuthBlock	svc_auth_block;
	ScratchBird::PathName	svc_expected_db;
	bool                svc_trusted_role;
	bool				svc_utf8;
	ScratchBird::string	svc_switches;	// Full set of switches
	ScratchBird::string	svc_perm_sw;	// Switches, taken from services table and/or passed using spb_command_line
	ScratchBird::UCharBuffer	svc_address_path;
	ScratchBird::string	svc_command_line;
	int					svc_parallel_workers;

	ScratchBird::string	svc_network_protocol;
	ScratchBird::string	svc_remote_address;
	ScratchBird::string	svc_remote_process;
	SLONG				svc_remote_pid;

	TraceManager*		svc_trace_manager;
	ScratchBird::ICryptKeyCallback* svc_crypt_callback;

public:
	ScratchBird::Semaphore	svc_detach_sem;

	class SvcMutex : public ScratchBird::RefMutex
	{
	public:
		explicit SvcMutex(Service* svc)
			: link(svc)
		{ }

		Service* link;
	};

	ScratchBird::RefPtr<SvcMutex> svc_existence;

private:
	ScratchBird::Semaphore svc_sem_empty, svc_sem_full;
	bool svc_output_overflow;

	void unblockQueryGet(bool over = false);

public:
	class Validate
	{
	public:
		explicit Validate(Service* svc);
		ScratchBird::MutexEnsureUnlock sharedGuard;
	};

private:
	class SafeMutexLock : private Validate
	{
	public:
		SafeMutexLock(Service* svc, const char* f);
		bool lock();

	protected:
		ScratchBird::RefPtr<SvcMutex> existenceMutex;
		const char* from;
	};

	friend class SafeMutexLock;

	//Service existence guard
	class ExistenceGuard : private SafeMutexLock
	{
	public:
		explicit ExistenceGuard(Service* svc, const char* from);
		~ExistenceGuard();
	};

	//Service unlock guard
	class UnlockGuard : private SafeMutexLock
	{
	public:
		explicit UnlockGuard(Service* svc, const char* from);
		bool enter();
		~UnlockGuard();
	private:
		bool locked, doLock;
	};

	// Data pipe from client to service
	ScratchBird::Semaphore svc_stdin_semaphore;
	ScratchBird::Mutex svc_stdin_mutex;
	// Size of data, requested by service (set in getBytes, reset in put)
	ULONG svc_stdin_size_requested;
	// Buffer passed by service
	UCHAR* svc_stdin_buffer;
	// Size of data, preloaded by user (set in put, reset in getBytes)
	ULONG svc_stdin_size_preload;
	// Buffer for datam preloaded by user
	ScratchBird::AutoPtr<UCHAR> svc_stdin_preload;
	// Size of data, requested from user to preload (set in getBytes)
	ULONG svc_stdin_preload_requested;
	// Size of data, placed into svc_stdin_buffer (set in put)
	ULONG svc_stdin_user_size;
	static const ULONG PRELOAD_BUFFER_SIZE = SVC_IO_BUFFER_SIZE;
	// Handle of a thread to wait for when closing
	Thread::Handle svc_thread;

#ifdef DEV_BUILD
	bool svc_debug;
#endif
};

} //namespace Jrd

#endif // JRD_SVC_H
