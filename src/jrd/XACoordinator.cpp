/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		XACoordinator.cpp
 *	DESCRIPTION:	XA/2PC Transaction Coordinator Implementation
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

#include "firebird.h"
#include "XACoordinator.h"
#include "../jrd/jrd.h"
#include "../jrd/tra.h"
#include "../jrd/Database.h"
#include "../common/isc_proto.h"
#include "../common/ThreadStart.h"
#include "../common/os/guid.h"
#include <time.h>
#include <stdio.h>

using namespace ScratchBird;
using namespace Jrd;

// Static member definitions
XACoordinator* XACoordinator::s_instance = nullptr;
GlobalPtr<Mutex> XACoordinator::s_instanceMutex;

// XATransactionId implementation

XATransactionId::XATransactionId()
	: formatID(0), gtrid_length(0), bqual_length(0)
{
	memset(data, 0, sizeof(data));
}

XATransactionId::XATransactionId(SLONG format, const void* gtrid, SLONG gtrid_len,
								 const void* bqual, SLONG bqual_len)
	: formatID(format), gtrid_length(gtrid_len), bqual_length(bqual_len)
{
	if (gtrid_len > MAX_GTRID_SIZE || bqual_len > MAX_BQUAL_SIZE) {
		ERR_post(Arg::Gds(isc_random) << Arg::Str("XA XID too long"));
	}
	
	memset(data, 0, sizeof(data));
	if (gtrid && gtrid_len > 0) {
		memcpy(data, gtrid, gtrid_len);
	}
	if (bqual && bqual_len > 0) {
		memcpy(data + gtrid_len, bqual, bqual_len);
	}
}

bool XATransactionId::equals(const XATransactionId& other) const
{
	return formatID == other.formatID &&
		   gtrid_length == other.gtrid_length &&
		   bqual_length == other.bqual_length &&
		   memcmp(data, other.data, gtrid_length + bqual_length) == 0;
}

void XATransactionId::toString(string& result) const
{
	result.printf("XID[fmt=%ld,gtrid=%ld,bqual=%ld]", 
				  formatID, gtrid_length, bqual_length);
	
	if (gtrid_length > 0) {
		result += ":GTRID=";
		for (SLONG i = 0; i < gtrid_length; i++) {
			result.printf("%02X", data[i]);
		}
	}
	
	if (bqual_length > 0) {
		result += ":BQUAL=";
		for (SLONG i = 0; i < bqual_length; i++) {
			result.printf("%02X", data[gtrid_length + i]);
		}
	}
}

void XATransactionId::generateGlobalTxnId()
{
	// Generate unique global transaction ID using timestamp + random data
	formatID = 1; // ScratchBird format
	
	// GTRID: timestamp (8 bytes) + GUID (16 bytes) = 24 bytes
	gtrid_length = 24;
	
	ISC_TIMESTAMP timestamp;
	TimeStamp::getCurrentTimeStamp(timestamp);
	memcpy(data, &timestamp, sizeof(timestamp));
	
	// Generate GUID for uniqueness
	char guid[16];
	GenerateRandomBytes(guid, sizeof(guid));
	memcpy(data + sizeof(timestamp), guid, sizeof(guid));
	
	// BQUAL: process ID + thread ID + sequence
	bqual_length = 12;
	ULONG pid = getpid();
	ULONG tid = ScratchBird::getThreadId();
	static ULONG sequence = 0;
	sequence++;
	
	memcpy(data + gtrid_length, &pid, sizeof(pid));
	memcpy(data + gtrid_length + sizeof(pid), &tid, sizeof(tid));
	memcpy(data + gtrid_length + sizeof(pid) + sizeof(tid), &sequence, sizeof(sequence));
}

// XATransactionBranch implementation

XATransactionBranch::XATransactionBranch(XAResourceManager* rm, const XATransactionId& xid)
	: m_resourceManager(rm), m_xid(xid), m_state(XA_STATE_ACTIVE)
{
}

XATransactionBranch::~XATransactionBranch()
{
}

int XATransactionBranch::start(SLONG flags)
{
	RefMutexGuard guard(m_mutex, FB_FUNCTION);
	
	if (m_state != XA_STATE_IDLE && m_state != XA_STATE_ACTIVE) {
		return XA::XAER_PROTO;
	}
	
	int result = m_resourceManager->xa_start(m_xid, flags);
	if (result == XA::XA_OK) {
		m_state = XA_STATE_ACTIVE;
	}
	
	return result;
}

int XATransactionBranch::end(SLONG flags)
{
	RefMutexGuard guard(m_mutex, FB_FUNCTION);
	
	if (m_state != XA_STATE_ACTIVE) {
		return XA::XAER_PROTO;
	}
	
	int result = m_resourceManager->xa_end(m_xid, flags);
	if (result == XA::XA_OK) {
		if (flags & XA::TMSUSPEND) {
			m_state = XA_STATE_IDLE;
		} else if (flags & XA::TMFAIL) {
			m_state = XA_STATE_ROLLBACK_ONLY;
		} else {
			m_state = XA_STATE_IDLE;
		}
	}
	
	return result;
}

int XATransactionBranch::prepare()
{
	RefMutexGuard guard(m_mutex, FB_FUNCTION);
	
	if (m_state != XA_STATE_IDLE) {
		return XA::XAER_PROTO;
	}
	
	int result = m_resourceManager->xa_prepare(m_xid);
	if (result == XA::XA_OK) {
		m_state = XA_STATE_PREPARED;
	} else if (result == XA::XA_RDONLY) {
		m_state = XA_STATE_COMMITTED; // Read-only, already committed
	}
	
	return result;
}

int XATransactionBranch::commit(bool onePhase)
{
	RefMutexGuard guard(m_mutex, FB_FUNCTION);
	
	if (!onePhase && m_state != XA_STATE_PREPARED) {
		return XA::XAER_PROTO;
	}
	
	int result = m_resourceManager->xa_commit(m_xid, onePhase);
	if (result == XA::XA_OK) {
		m_state = XA_STATE_COMMITTED;
	}
	
	return result;
}

int XATransactionBranch::rollback()
{
	RefMutexGuard guard(m_mutex, FB_FUNCTION);
	
	if (m_state != XA_STATE_PREPARED && m_state != XA_STATE_IDLE && 
		m_state != XA_STATE_ROLLBACK_ONLY) {
		return XA::XAER_PROTO;
	}
	
	int result = m_resourceManager->xa_rollback(m_xid);
	if (result == XA::XA_OK) {
		m_state = XA_STATE_ABORTED;
	}
	
	return result;
}

int XATransactionBranch::forget()
{
	RefMutexGuard guard(m_mutex, FB_FUNCTION);
	
	int result = m_resourceManager->xa_forget(m_xid);
	if (result == XA::XA_OK) {
		m_state = XA_STATE_FORGOTTEN;
	}
	
	return result;
}

// XAGlobalTransaction implementation

XAGlobalTransaction::XAGlobalTransaction(const XATransactionId& globalXid, jrd_tra* localTra)
	: m_globalXid(globalXid), m_localTransaction(localTra), 
	  m_phase(PHASE_ACTIVE), m_readOnly(false)
{
}

XAGlobalTransaction::~XAGlobalTransaction()
{
	// Clean up branches
	for (size_t i = 0; i < m_branches.getCount(); i++) {
		delete m_branches[i];
	}
	m_branches.clear();
}

XATransactionBranch* XAGlobalTransaction::addBranch(XAResourceManager* rm)
{
	RefMutexGuard guard(m_mutex, FB_FUNCTION);
	
	// Create branch XID by combining global XID with branch qualifier
	XATransactionId branchXid = m_globalXid;
	
	// Add branch sequence number to BQUAL to make it unique
	ULONG branchSeq = m_branches.getCount();
	if (branchXid.bqual_length + sizeof(branchSeq) <= XATransactionId::MAX_BQUAL_SIZE) {
		memcpy(branchXid.data + branchXid.gtrid_length + branchXid.bqual_length, 
			   &branchSeq, sizeof(branchSeq));
		branchXid.bqual_length += sizeof(branchSeq);
	}
	
	XATransactionBranch* branch = new XATransactionBranch(rm, branchXid);
	m_branches.add(branch);
	
	return branch;
}

void XAGlobalTransaction::removeBranch(XATransactionBranch* branch)
{
	RefMutexGuard guard(m_mutex, FB_FUNCTION);
	
	size_t pos;
	if (m_branches.find(branch, pos)) {
		m_branches.remove(pos);
		delete branch;
	}
}

XATransactionBranch* XAGlobalTransaction::findBranch(const XATransactionId& xid)
{
	RefMutexGuard guard(m_mutex, FB_FUNCTION);
	
	for (size_t i = 0; i < m_branches.getCount(); i++) {
		if (m_branches[i]->getXID().equals(xid)) {
			return m_branches[i];
		}
	}
	
	return nullptr;
}

bool XAGlobalTransaction::prepare()
{
	RefMutexGuard guard(m_mutex, FB_FUNCTION);
	
	if (m_phase != PHASE_ACTIVE) {
		return false;
	}
	
	setPhase(PHASE_PREPARING);
	
	bool success = preparePhase();
	if (success) {
		setPhase(PHASE_PREPARED);
		logTransaction(); // Log prepared state for recovery
	} else {
		setPhase(PHASE_ABORTING);
		rollbackPhase();
		setPhase(PHASE_ABORTED);
	}
	
	return success;
}

bool XAGlobalTransaction::commit()
{
	RefMutexGuard guard(m_mutex, FB_FUNCTION);
	
	if (m_phase != PHASE_PREPARED) {
		return false;
	}
	
	setPhase(PHASE_COMMITTING);
	
	bool success = commitPhase();
	if (success) {
		setPhase(PHASE_COMMITTED);
		clearLog(); // Remove from recovery log
	} else {
		// Heuristic completion - some branches committed, some failed
		// This requires manual intervention
		ERR_post(Arg::Gds(isc_random) << 
				 Arg::Str("XA heuristic completion detected - manual recovery required"));
	}
	
	return success;
}

bool XAGlobalTransaction::rollback()
{
	RefMutexGuard guard(m_mutex, FB_FUNCTION);
	
	if (m_phase == PHASE_COMMITTED || m_phase == PHASE_ABORTED) {
		return m_phase == PHASE_ABORTED;
	}
	
	setPhase(PHASE_ABORTING);
	
	bool success = rollbackPhase();
	setPhase(PHASE_ABORTED);
	clearLog(); // Remove from recovery log
	
	return success;
}

void XAGlobalTransaction::setPhase(Phase phase)
{
	m_phase = phase;
}

bool XAGlobalTransaction::preparePhase()
{
	int readOnlyCount = 0;
	int preparedCount = 0;
	
	// End all branches first
	for (size_t i = 0; i < m_branches.getCount(); i++) {
		XATransactionBranch* branch = m_branches[i];
		int result = branch->end(XA::TMSUCCESS);
		if (result != XA::XA_OK) {
			// Mark branch for rollback
			branch->end(XA::TMFAIL);
		}
	}
	
	// Prepare all branches
	for (size_t i = 0; i < m_branches.getCount(); i++) {
		XATransactionBranch* branch = m_branches[i];
		int result = branch->prepare();
		
		if (result == XA::XA_OK) {
			preparedCount++;
		} else if (result == XA::XA_RDONLY) {
			readOnlyCount++;
		} else {
			// Prepare failed - rollback all prepared branches
			for (size_t j = 0; j < i; j++) {
				if (m_branches[j]->getState() == XATransactionBranch::XA_STATE_PREPARED) {
					m_branches[j]->rollback();
				}
			}
			return false;
		}
	}
	
	m_readOnly = (readOnlyCount == m_branches.getCount());
	return true;
}

bool XAGlobalTransaction::commitPhase()
{
	bool allCommitted = true;
	
	for (size_t i = 0; i < m_branches.getCount(); i++) {
		XATransactionBranch* branch = m_branches[i];
		if (branch->getState() == XATransactionBranch::XA_STATE_PREPARED) {
			int result = branch->commit(false);
			if (result != XA::XA_OK) {
				allCommitted = false;
			}
		}
	}
	
	return allCommitted;
}

bool XAGlobalTransaction::rollbackPhase()
{
	bool allRolledBack = true;
	
	for (size_t i = 0; i < m_branches.getCount(); i++) {
		XATransactionBranch* branch = m_branches[i];
		int result = branch->rollback();
		if (result != XA::XA_OK) {
			allRolledBack = false;
		}
	}
	
	return allRolledBack;
}

void XAGlobalTransaction::logTransaction()
{
	XACoordinator::instance().logPreparedTransaction(m_globalXid, m_branches);
}

void XAGlobalTransaction::clearLog()
{
	XACoordinator::instance().removePreparedTransaction(m_globalXid);
}

// XACoordinator implementation

XACoordinator& XACoordinator::instance()
{
	MutexLockGuard guard(s_instanceMutex, FB_FUNCTION);
	
	if (!s_instance) {
		s_instance = new XACoordinator();
	}
	
	return *s_instance;
}

XACoordinator::XACoordinator()
	: m_recoveryThreadRunning(false), m_logFile(nullptr)
{
	openTransactionLog();
	startRecoveryThread();
}

XACoordinator::~XACoordinator()
{
	stopRecoveryThread();
	closeTransactionLog();
	
	// Clean up global transactions
	GlobalTransactionMap::Accessor accessor(&m_globalTransactions);
	for (bool found = accessor.getFirst(); found; found = accessor.getNext()) {
		delete accessor.current()->second;
	}
	
	// Clean up resource managers
	ResourceManagerMap::Accessor rmAccessor(&m_resourceManagers);
	for (bool found = rmAccessor.getFirst(); found; found = rmAccessor.getNext()) {
		// Note: We don't delete resource managers as they may be owned externally
	}
}

XAGlobalTransaction* XACoordinator::beginGlobalTransaction(jrd_tra* localTra)
{
	RefMutexGuard guard(m_mutex, FB_FUNCTION);
	
	XATransactionId globalXid;
	globalXid.generateGlobalTxnId();
	
	XAGlobalTransaction* globalTxn = new XAGlobalTransaction(globalXid, localTra);
	
	m_globalTransactions.put(globalXid, globalTxn);
	
	return globalTxn;
}

XAGlobalTransaction* XACoordinator::findGlobalTransaction(const XATransactionId& xid)
{
	RefMutexGuard guard(m_mutex, FB_FUNCTION);
	
	XAGlobalTransaction* globalTxn = nullptr;
	m_globalTransactions.get(xid, globalTxn);
	
	return globalTxn;
}

void XACoordinator::endGlobalTransaction(const XATransactionId& xid)
{
	RefMutexGuard guard(m_mutex, FB_FUNCTION);
	
	XAGlobalTransaction* globalTxn = nullptr;
	if (m_globalTransactions.get(xid, globalTxn)) {
		m_globalTransactions.remove(xid);
		delete globalTxn;
	}
}

void XACoordinator::registerResourceManager(XAResourceManager* rm)
{
	RefMutexGuard guard(m_mutex, FB_FUNCTION);
	
	string name = rm->getResourceManagerName();
	m_resourceManagers.put(name, rm);
}

void XACoordinator::unregisterResourceManager(XAResourceManager* rm)
{
	RefMutexGuard guard(m_mutex, FB_FUNCTION);
	
	string name = rm->getResourceManagerName();
	m_resourceManagers.remove(name);
}

XAResourceManager* XACoordinator::findResourceManager(const string& name)
{
	RefMutexGuard guard(m_mutex, FB_FUNCTION);
	
	XAResourceManager* rm = nullptr;
	m_resourceManagers.get(name, rm);
	
	return rm;
}

void XACoordinator::startRecoveryThread()
{
	if (!m_recoveryThreadRunning) {
		m_recoveryThreadRunning = true;
		Thread::start(recoveryThreadRoutine, this, THREAD_medium, &m_recoveryThread);
	}
}

void XACoordinator::stopRecoveryThread()
{
	if (m_recoveryThreadRunning) {
		m_recoveryThreadRunning = false;
		Thread::waitForCompletion(m_recoveryThread);
	}
}

void XACoordinator::recoveryThreadRoutine(XACoordinator* coordinator)
{
	while (coordinator->m_recoveryThreadRunning) {
		try {
			coordinator->performRecovery();
		} catch (...) {
			// Log error and continue
		}
		
		// Sleep for 30 seconds before next recovery scan
		Thread::sleep(30000);
	}
}

void XACoordinator::performRecovery()
{
	RefMutexGuard guard(m_mutex, FB_FUNCTION);
	
	// Scan all resource managers for in-doubt transactions
	ResourceManagerMap::Accessor accessor(&m_resourceManagers);
	for (bool found = accessor.getFirst(); found; found = accessor.getNext()) {
		XAResourceManager* rm = accessor.current()->second;
		
		XATransactionId xids[100]; // Recover up to 100 XIDs at a time
		int count = rm->xa_recover(xids, 100, XA::TMSTARTRSCAN | XA::TMENDRSCAN);
		
		for (int i = 0; i < count; i++) {
			// Check if we have a record of this transaction
			XAGlobalTransaction* globalTxn = findGlobalTransaction(xids[i]);
			if (!globalTxn) {
				// Unknown transaction - ask RM to forget it
				rm->xa_forget(xids[i]);
			}
		}
	}
}

void XACoordinator::openTransactionLog()
{
	// Create transaction log file path
	m_logFileName = "xa_transaction.log"; // Should be configurable
	
	m_logFile = fopen(m_logFileName.c_str(), "a+");
	if (!m_logFile) {
		ERR_post(Arg::Gds(isc_io_error) << 
				 Arg::Str("open") << 
				 Arg::Str(m_logFileName.c_str()) << 
				 Arg::Unix(errno));
	}
}

void XACoordinator::closeTransactionLog()
{
	if (m_logFile) {
		fclose(m_logFile);
		m_logFile = nullptr;
	}
}

void XACoordinator::logPreparedTransaction(const XATransactionId& xid, 
										   const Array<XATransactionBranch*>& branches)
{
	if (!m_logFile) return;
	
	// Write transaction record to log
	string xidStr;
	xid.toString(xidStr);
	
	fprintf(m_logFile, "PREPARE %s %d\n", xidStr.c_str(), (int)branches.getCount());
	
	for (size_t i = 0; i < branches.getCount(); i++) {
		XATransactionBranch* branch = branches[i];
		string branchXidStr;
		branch->getXID().toString(branchXidStr);
		fprintf(m_logFile, "BRANCH %s %s\n", 
				branchXidStr.c_str(), 
				branch->getResourceManager()->getResourceManagerName());
	}
	
	fflush(m_logFile);
}

void XACoordinator::removePreparedTransaction(const XATransactionId& xid)
{
	if (!m_logFile) return;
	
	// Write completion record to log
	string xidStr;
	xid.toString(xidStr);
	
	fprintf(m_logFile, "COMPLETE %s\n", xidStr.c_str());
	fflush(m_logFile);
	
	// TODO: Implement log file compaction to remove completed transactions
}