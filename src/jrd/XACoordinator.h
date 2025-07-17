/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		XACoordinator.h
 *	DESCRIPTION:	XA/2PC Transaction Coordinator for External Data Sources
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
 *  The Original Code was created by ScratchBird Development Team
 *  for the ScratchBird Open Source RDBMS project.
 *
 *  Copyright (c) 2025 ScratchBird Development Team
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#ifndef JRD_XA_COORDINATOR_H
#define JRD_XA_COORDINATOR_H

#include "../common/classes/fb_string.h"
#include "../common/classes/array.h"
#include "../common/classes/GenericMap.h"
#include "../common/classes/RefMutex.h"
#include "../common/ThreadStart.h"
#include "../jrd/tra.h"

namespace Jrd {

// XA Transaction Identifier (XID) - 128 bytes max
struct XATransactionId
{
	static const int MAX_GTRID_SIZE = 64;	// Global Transaction ID
	static const int MAX_BQUAL_SIZE = 64;	// Branch Qualifier
	
	SLONG formatID;							// Format identifier
	SLONG gtrid_length;						// Global transaction ID length  
	SLONG bqual_length;						// Branch qualifier length
	UCHAR data[MAX_GTRID_SIZE + MAX_BQUAL_SIZE];	// Combined GTRID and BQUAL
	
	XATransactionId();
	XATransactionId(SLONG format, const void* gtrid, SLONG gtrid_len, 
					const void* bqual, SLONG bqual_len);
	
	bool equals(const XATransactionId& other) const;
	void toString(ScratchBird::string& result) const;
	void generateGlobalTxnId();
	
	const UCHAR* getGTRID() const { return data; }
	const UCHAR* getBQUAL() const { return data + gtrid_length; }
};

// XA Resource Manager interface for external data sources
class XAResourceManager
{
public:
	virtual ~XAResourceManager() = default;
	
	// XA interface methods
	virtual int xa_start(const XATransactionId& xid, SLONG flags) = 0;
	virtual int xa_end(const XATransactionId& xid, SLONG flags) = 0;
	virtual int xa_prepare(const XATransactionId& xid) = 0;
	virtual int xa_commit(const XATransactionId& xid, bool onePhase) = 0;
	virtual int xa_rollback(const XATransactionId& xid) = 0;
	virtual int xa_recover(XATransactionId* xids, SLONG count, SLONG flags) = 0;
	virtual int xa_forget(const XATransactionId& xid) = 0;
	
	// Resource manager identification
	virtual const char* getResourceManagerName() const = 0;
	virtual ScratchBird::string getConnectionString() const = 0;
	virtual bool isSameRM(const XAResourceManager* other) const = 0;
};

// XA Transaction Branch - represents one resource manager's view of a transaction
class XATransactionBranch
{
public:
	enum State {
		XA_STATE_ACTIVE,		// Transaction is active
		XA_STATE_IDLE,			// Transaction is suspended
		XA_STATE_PREPARED,		// Transaction is prepared
		XA_STATE_ROLLBACK_ONLY,	// Transaction is marked for rollback
		XA_STATE_COMMITTED,		// Transaction is committed
		XA_STATE_ABORTED,		// Transaction is aborted
		XA_STATE_FORGOTTEN		// Transaction is forgotten
	};
	
	XATransactionBranch(XAResourceManager* rm, const XATransactionId& xid);
	~XATransactionBranch();
	
	// State management
	State getState() const { return m_state; }
	void setState(State state) { m_state = state; }
	
	// XA operations
	int start(SLONG flags);
	int end(SLONG flags);
	int prepare();
	int commit(bool onePhase);
	int rollback();
	int forget();
	
	// Accessors
	const XATransactionId& getXID() const { return m_xid; }
	XAResourceManager* getResourceManager() const { return m_resourceManager; }
	
private:
	XAResourceManager* m_resourceManager;
	XATransactionId m_xid;
	State m_state;
	ScratchBird::RefMutex m_mutex;
};

// Global Transaction - coordinates multiple resource managers
class XAGlobalTransaction
{
public:
	enum Phase {
		PHASE_ACTIVE,		// Transaction is active
		PHASE_PREPARING,	// Prepare phase in progress
		PHASE_PREPARED,		// All branches prepared
		PHASE_COMMITTING,	// Commit phase in progress
		PHASE_COMMITTED,	// Transaction committed
		PHASE_ABORTING,		// Abort in progress
		PHASE_ABORTED		// Transaction aborted
	};
	
	XAGlobalTransaction(const XATransactionId& globalXid, jrd_tra* localTra);
	~XAGlobalTransaction();
	
	// Branch management
	XATransactionBranch* addBranch(XAResourceManager* rm);
	void removeBranch(XATransactionBranch* branch);
	XATransactionBranch* findBranch(const XATransactionId& xid);
	
	// 2PC operations
	bool prepare();
	bool commit();
	bool rollback();
	
	// State management
	Phase getPhase() const { return m_phase; }
	const XATransactionId& getGlobalXID() const { return m_globalXid; }
	jrd_tra* getLocalTransaction() const { return m_localTransaction; }
	
	// Recovery support
	void logTransaction();
	void clearLog();
	
private:
	XATransactionId m_globalXid;
	jrd_tra* m_localTransaction;
	ScratchBird::Array<XATransactionBranch*> m_branches;
	Phase m_phase;
	ScratchBird::RefMutex m_mutex;
	bool m_readOnly;
	
	void setPhase(Phase phase);
	bool preparePhase();
	bool commitPhase();
	bool rollbackPhase();
};

// XA Transaction Coordinator - manages global transactions
class XACoordinator
{
public:
	static XACoordinator& instance();
	
	XACoordinator();
	~XACoordinator();
	
	// Global transaction management
	XAGlobalTransaction* beginGlobalTransaction(jrd_tra* localTra);
	XAGlobalTransaction* findGlobalTransaction(const XATransactionId& xid);
	void endGlobalTransaction(const XATransactionId& xid);
	
	// Resource manager management
	void registerResourceManager(XAResourceManager* rm);
	void unregisterResourceManager(XAResourceManager* rm);
	XAResourceManager* findResourceManager(const ScratchBird::string& name);
	
	// Recovery operations
	void recoverInDoubtTransactions();
	void startRecoveryThread();
	void stopRecoveryThread();
	
	// Transaction log management
	void logPreparedTransaction(const XATransactionId& xid, 
								const ScratchBird::Array<XATransactionBranch*>& branches);
	void removePreparedTransaction(const XATransactionId& xid);
	
private:
	typedef ScratchBird::GenericMap<ScratchBird::Pair<ScratchBird::Left<XATransactionId, XAGlobalTransaction*>>> GlobalTransactionMap;
	typedef ScratchBird::GenericMap<ScratchBird::Pair<ScratchBird::Left<ScratchBird::string, XAResourceManager*>>> ResourceManagerMap;
	
	GlobalTransactionMap m_globalTransactions;
	ResourceManagerMap m_resourceManagers;
	ScratchBird::RefMutex m_mutex;
	
	// Recovery thread
	ScratchBird::Thread m_recoveryThread;
	bool m_recoveryThreadRunning;
	
	static void recoveryThreadRoutine(XACoordinator* coordinator);
	void performRecovery();
	
	// Transaction log file operations
	void openTransactionLog();
	void closeTransactionLog();
	FILE* m_logFile;
	ScratchBird::string m_logFileName;
	
	// Singleton instance
	static XACoordinator* s_instance;
	static ScratchBird::GlobalPtr<ScratchBird::Mutex> s_instanceMutex;
};

// XA Constants (based on X/Open XA specification)
namespace XA {
	// xa_start flags
	const SLONG TMNOFLAGS	= 0x00000000;	// No flags
	const SLONG TMJOIN		= 0x00200000;	// Join existing transaction
	const SLONG TMRESUME	= 0x08000000;	// Resume suspended transaction
	
	// xa_end flags  
	const SLONG TMSUCCESS	= 0x04000000;	// Normal completion
	const SLONG TMFAIL		= 0x20000000;	// Transaction failed
	const SLONG TMSUSPEND	= 0x02000000;	// Suspend transaction
	
	// xa_recover flags
	const SLONG TMSTARTRSCAN = 0x01000000;	// Start recovery scan
	const SLONG TMENDRSCAN	= 0x00800000;	// End recovery scan
	
	// Return codes
	const int XA_OK			= 0;		// Success
	const int XAER_ASYNC	= -2;		// Asynchronous operation
	const int XAER_RMERR	= -3;		// Resource manager error
	const int XAER_NOTA		= -4;		// XID not known
	const int XAER_INVAL	= -5;		// Invalid arguments
	const int XAER_PROTO	= -6;		// Protocol error
	const int XAER_RMFAIL	= -7;		// Resource manager failure
	const int XAER_DUPID	= -8;		// Duplicate XID
	const int XAER_OUTSIDE	= -9;		// Outside global transaction
	const int XA_RDONLY		= 3;		// Read-only transaction
	const int XA_RETRY		= 4;		// Retry later
	const int XA_HEURMIX	= 5;		// Heuristic mixed completion
	const int XA_HEURRB		= 6;		// Heuristic rollback
	const int XA_HEURCOM	= 7;		// Heuristic commit
	const int XA_HEURHAZ	= 8;		// Heuristic hazard
	const int XA_NOMIGRATE	= 9;		// No migration
}

} // namespace Jrd

#endif // JRD_XA_COORDINATOR_H