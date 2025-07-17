/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		ExternalDataSourceRegistry.cpp
 *	DESCRIPTION:	Registry for 2PC-capable External Data Sources Implementation
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
#include "ExternalDataSourceRegistry.h"
#include "XACoordinator.h"
#include "../jrd/jrd.h"
#include "../jrd/tra.h"
#include "../common/isc_proto.h"
#include "../common/classes/ClumpletWriter.h"

using namespace ScratchBird;
using namespace Jrd;

// Static member definitions
ExternalDataSourceRegistry* ExternalDataSourceRegistry::s_instance = nullptr;
GlobalPtr<Mutex> ExternalDataSourceRegistry::s_instanceMutex;

// ExternalDataSourceConfig implementation

ExternalDataSourceConfig::ExternalDataSourceConfig()
	: supports2PC(false), poolConnections(true), maxConnections(10),
	  connectionTimeout(30), queryTimeout(60)
{
}

ExternalDataSourceConfig::ExternalDataSourceConfig(const string& dsName,
													const string& dsProvider,
													const string& connStr)
	: name(dsName), provider(dsProvider), connectionString(connStr),
	  supports2PC(false), poolConnections(true), maxConnections(10),
	  connectionTimeout(30), queryTimeout(60)
{
}

void ExternalDataSourceConfig::encryptCredentials()
{
	// Simple XOR encryption for demonstration - should use proper encryption in production
	if (!password.isEmpty() && encryptionKey.isEmpty()) {
		encryptionKey = "ScratchBird2PC"; // Default key - should be configurable
	}
	
	if (!password.isEmpty() && !encryptionKey.isEmpty()) {
		string encrypted;
		for (size_t i = 0; i < password.length(); i++) {
			char c = password[i] ^ encryptionKey[i % encryptionKey.length()];
			encrypted += c;
		}
		password = encrypted;
	}
}

void ExternalDataSourceConfig::decryptCredentials()
{
	// Decrypt using same XOR method
	if (!password.isEmpty() && !encryptionKey.isEmpty()) {
		string decrypted;
		for (size_t i = 0; i < password.length(); i++) {
			char c = password[i] ^ encryptionKey[i % encryptionKey.length()];
			decrypted += c;
		}
		password = decrypted;
	}
}

bool ExternalDataSourceConfig::isValid() const
{
	return !name.isEmpty() && !provider.isEmpty() && !connectionString.isEmpty();
}

// ExternalDataSourcePool implementation

ExternalDataSourcePool::ExternalDataSourcePool(const ExternalDataSourceConfig& config)
	: m_config(config), m_activeConnections(0)
{
}

ExternalDataSourcePool::~ExternalDataSourcePool()
{
	// Clean up idle connections
	for (size_t i = 0; i < m_idleConnections.getCount(); i++) {
		destroyConnection(m_idleConnections[i]);
	}
	m_idleConnections.clear();
}

EDS::Connection* ExternalDataSourcePool::getConnection(thread_db* tdbb)
{
	RefMutexGuard guard(m_mutex, FB_FUNCTION);
	
	EDS::Connection* connection = nullptr;
	
	// Try to reuse idle connection
	if (m_idleConnections.getCount() > 0) {
		connection = m_idleConnections[m_idleConnections.getCount() - 1];
		m_idleConnections.remove(m_idleConnections.getCount() - 1);
		
		// Validate connection
		try {
			validateConnection(connection);
		} catch (...) {
			destroyConnection(connection);
			connection = nullptr;
		}
	}
	
	// Create new connection if needed
	if (!connection && (m_activeConnections < m_config.maxConnections)) {
		connection = createConnection(tdbb);
	}
	
	if (connection) {
		m_activeConnections++;
	}
	
	return connection;
}

void ExternalDataSourcePool::releaseConnection(EDS::Connection* connection)
{
	RefMutexGuard guard(m_mutex, FB_FUNCTION);
	
	if (connection) {
		m_activeConnections--;
		
		if (m_config.poolConnections) {
			m_idleConnections.add(connection);
		} else {
			destroyConnection(connection);
		}
	}
}

EDS::Connection* ExternalDataSourcePool::createConnection(thread_db* tdbb)
{
	// This is a simplified implementation - actual implementation would
	// use the EDS provider framework to create appropriate connections
	
	// For now, return nullptr to indicate connection creation is not implemented
	// Real implementation would:
	// 1. Get appropriate EDS provider based on m_config.provider
	// 2. Create ClumpletReader with connection parameters
	// 3. Call provider->createConnection()
	
	return nullptr;
}

void ExternalDataSourcePool::destroyConnection(EDS::Connection* connection)
{
	if (connection) {
		delete connection;
	}
}

void ExternalDataSourcePool::validateConnection(EDS::Connection* connection)
{
	// Simple validation - check if connection is still alive
	if (!connection->isConnected()) {
		throw Exception("Connection is not active");
	}
}

// ScratchBirdXAResourceManager implementation

ScratchBirdXAResourceManager::ScratchBirdXAResourceManager(const ExternalDataSourceConfig& config)
	: m_config(config), m_connectionPool(nullptr)
{
}

ScratchBirdXAResourceManager::~ScratchBirdXAResourceManager()
{
	// Clean up XA connections
	RefMutexGuard guard(m_mutex, FB_FUNCTION);
	
	XAConnectionMap::Accessor accessor(&m_xaConnections);
	for (bool found = accessor.getFirst(); found; found = accessor.getNext()) {
		EDS::Connection* conn = accessor.current()->second;
		if (m_connectionPool) {
			m_connectionPool->releaseConnection(conn);
		} else {
			delete conn;
		}
	}
	m_xaConnections.clear();
}

int ScratchBirdXAResourceManager::xa_start(const XATransactionId& xid, SLONG flags)
{
	RefMutexGuard guard(m_mutex, FB_FUNCTION);
	
	try {
		EDS::Connection* conn = findOrCreateConnection(xid);
		if (!conn) {
			return XA::XAER_RMERR;
		}
		
		// Start XA transaction on the connection
		// This would typically involve sending XA START command to remote database
		// For now, simulate success
		
		return XA::XA_OK;
	} catch (...) {
		return XA::XAER_RMERR;
	}
}

int ScratchBirdXAResourceManager::xa_end(const XATransactionId& xid, SLONG flags)
{
	RefMutexGuard guard(m_mutex, FB_FUNCTION);
	
	try {
		EDS::Connection* conn = nullptr;
		if (!m_xaConnections.get(xid, conn) || !conn) {
			return XA::XAER_NOTA;
		}
		
		// End XA transaction on the connection
		// This would typically involve sending XA END command to remote database
		
		return XA::XA_OK;
	} catch (...) {
		return XA::XAER_RMERR;
	}
}

int ScratchBirdXAResourceManager::xa_prepare(const XATransactionId& xid)
{
	RefMutexGuard guard(m_mutex, FB_FUNCTION);
	
	try {
		EDS::Connection* conn = nullptr;
		if (!m_xaConnections.get(xid, conn) || !conn) {
			return XA::XAER_NOTA;
		}
		
		// Prepare XA transaction on the connection
		// This would typically involve sending XA PREPARE command to remote database
		// For now, simulate success
		
		return XA::XA_OK;
	} catch (...) {
		return XA::XAER_RMERR;
	}
}

int ScratchBirdXAResourceManager::xa_commit(const XATransactionId& xid, bool onePhase)
{
	RefMutexGuard guard(m_mutex, FB_FUNCTION);
	
	try {
		EDS::Connection* conn = nullptr;
		if (!m_xaConnections.get(xid, conn) || !conn) {
			return XA::XAER_NOTA;
		}
		
		// Commit XA transaction on the connection
		// This would typically involve sending XA COMMIT command to remote database
		
		// Remove connection from XA map
		removeConnection(xid);
		
		return XA::XA_OK;
	} catch (...) {
		return XA::XAER_RMERR;
	}
}

int ScratchBirdXAResourceManager::xa_rollback(const XATransactionId& xid)
{
	RefMutexGuard guard(m_mutex, FB_FUNCTION);
	
	try {
		EDS::Connection* conn = nullptr;
		if (!m_xaConnections.get(xid, conn) || !conn) {
			return XA::XAER_NOTA;
		}
		
		// Rollback XA transaction on the connection
		// This would typically involve sending XA ROLLBACK command to remote database
		
		// Remove connection from XA map
		removeConnection(xid);
		
		return XA::XA_OK;
	} catch (...) {
		return XA::XAER_RMERR;
	}
}

int ScratchBirdXAResourceManager::xa_recover(XATransactionId* xids, SLONG count, SLONG flags)
{
	// Query remote database for in-doubt transactions
	// This would typically involve sending XA RECOVER command to remote database
	// For now, return 0 (no in-doubt transactions)
	
	return 0;
}

int ScratchBirdXAResourceManager::xa_forget(const XATransactionId& xid)
{
	RefMutexGuard guard(m_mutex, FB_FUNCTION);
	
	try {
		// Tell remote database to forget about heuristically completed transaction
		// This would typically involve sending XA FORGET command to remote database
		
		// Remove from our tracking
		removeConnection(xid);
		
		return XA::XA_OK;
	} catch (...) {
		return XA::XAER_RMERR;
	}
}

const char* ScratchBirdXAResourceManager::getResourceManagerName() const
{
	return m_config.name.c_str();
}

string ScratchBirdXAResourceManager::getConnectionString() const
{
	return m_config.connectionString;
}

bool ScratchBirdXAResourceManager::isSameRM(const XAResourceManager* other) const
{
	const ScratchBirdXAResourceManager* otherRM = 
		dynamic_cast<const ScratchBirdXAResourceManager*>(other);
	
	if (!otherRM) {
		return false;
	}
	
	return m_config.connectionString == otherRM->m_config.connectionString;
}

EDS::Connection* ScratchBirdXAResourceManager::getConnection(const XATransactionId& xid)
{
	RefMutexGuard guard(m_mutex, FB_FUNCTION);
	
	EDS::Connection* conn = nullptr;
	m_xaConnections.get(xid, conn);
	return conn;
}

EDS::Connection* ScratchBirdXAResourceManager::findOrCreateConnection(const XATransactionId& xid)
{
	EDS::Connection* conn = nullptr;
	
	// Check if we already have a connection for this XID
	if (m_xaConnections.get(xid, conn)) {
		return conn;
	}
	
	// Get connection from pool or create new one
	if (m_connectionPool) {
		conn = m_connectionPool->getConnection(nullptr); // TODO: Pass proper thread_db
	}
	
	if (conn) {
		m_xaConnections.put(xid, conn);
	}
	
	return conn;
}

void ScratchBirdXAResourceManager::removeConnection(const XATransactionId& xid)
{
	EDS::Connection* conn = nullptr;
	if (m_xaConnections.get(xid, conn)) {
		m_xaConnections.remove(xid);
		
		if (m_connectionPool) {
			m_connectionPool->releaseConnection(conn);
		} else {
			delete conn;
		}
	}
}

// ExternalDataSourceRegistry implementation

ExternalDataSourceRegistry& ExternalDataSourceRegistry::instance()
{
	MutexLockGuard guard(s_instanceMutex, FB_FUNCTION);
	
	if (!s_instance) {
		s_instance = new ExternalDataSourceRegistry();
	}
	
	return *s_instance;
}

ExternalDataSourceRegistry::ExternalDataSourceRegistry()
	: m_configFileName("external_data_sources.conf")
{
	loadConfiguration();
}

ExternalDataSourceRegistry::~ExternalDataSourceRegistry()
{
	saveConfiguration();
	
	// Clean up data structures
	RefMutexGuard guard(m_mutex, FB_FUNCTION);
	
	// Clean up connection pools
	ConnectionPoolMap::Accessor poolAccessor(&m_connectionPools);
	for (bool found = poolAccessor.getFirst(); found; found = poolAccessor.getNext()) {
		delete poolAccessor.current()->second;
	}
	
	// Clean up XA resource managers
	XAResourceManagerMap::Accessor xaAccessor(&m_xaResourceManagers);
	for (bool found = xaAccessor.getFirst(); found; found = xaAccessor.getNext()) {
		delete xaAccessor.current()->second;
	}
	
	// Clean up data source configs
	DataSourceConfigMap::Accessor dsAccessor(&m_dataSources);
	for (bool found = dsAccessor.getFirst(); found; found = dsAccessor.getNext()) {
		delete dsAccessor.current()->second;
	}
}

void ExternalDataSourceRegistry::registerDataSource(const ExternalDataSourceConfig& config)
{
	RefMutexGuard guard(m_mutex, FB_FUNCTION);
	
	if (!config.isValid()) {
		ERR_post(Arg::Gds(isc_random) << Arg::Str("Invalid data source configuration"));
	}
	
	// Create copy of config
	ExternalDataSourceConfig* configCopy = new ExternalDataSourceConfig(config);
	configCopy->encryptCredentials();
	
	// Store configuration
	m_dataSources.put(config.name, configCopy);
	
	// Create connection pool if pooling is enabled
	if (config.poolConnections) {
		createConnectionPool(config.name);
	}
	
	// Create XA resource manager if 2PC is supported
	if (config.supports2PC) {
		createXAResourceManager(config.name);
	}
	
	saveConfiguration();
}

void ExternalDataSourceRegistry::unregisterDataSource(const string& name)
{
	RefMutexGuard guard(m_mutex, FB_FUNCTION);
	
	// Clean up XA resource manager
	destroyXAResourceManager(name);
	
	// Clean up connection pool
	destroyConnectionPool(name);
	
	// Remove configuration
	ExternalDataSourceConfig* config = nullptr;
	if (m_dataSources.get(name, config)) {
		m_dataSources.remove(name);
		delete config;
	}
	
	saveConfiguration();
}

ExternalDataSourceConfig* ExternalDataSourceRegistry::findDataSource(const string& name)
{
	RefMutexGuard guard(m_mutex, FB_FUNCTION);
	
	ExternalDataSourceConfig* config = nullptr;
	m_dataSources.get(name, config);
	
	if (config) {
		config->decryptCredentials();
	}
	
	return config;
}

ExternalDataSourcePool* ExternalDataSourceRegistry::getConnectionPool(const string& name)
{
	RefMutexGuard guard(m_mutex, FB_FUNCTION);
	
	ExternalDataSourcePool* pool = nullptr;
	m_connectionPools.get(name, pool);
	return pool;
}

ScratchBirdXAResourceManager* ExternalDataSourceRegistry::getXAResourceManager(const string& name)
{
	RefMutexGuard guard(m_mutex, FB_FUNCTION);
	
	ScratchBirdXAResourceManager* rm = nullptr;
	m_xaResourceManagers.get(name, rm);
	return rm;
}

bool ExternalDataSourceRegistry::beginDistributedTransaction(jrd_tra* localTra,
															  const Array<string>& dataSourceNames)
{
	RefMutexGuard guard(m_mutex, FB_FUNCTION);
	
	// Create global transaction
	XACoordinator& coordinator = XACoordinator::instance();
	XAGlobalTransaction* globalTxn = coordinator.beginGlobalTransaction(localTra);
	
	// Add branches for each data source
	for (size_t i = 0; i < dataSourceNames.getCount(); i++) {
		ScratchBirdXAResourceManager* rm = getXAResourceManager(dataSourceNames[i]);
		if (rm) {
			XATransactionBranch* branch = globalTxn->addBranch(rm);
			if (!branch) {
				// Failed to add branch - rollback global transaction
				globalTxn->rollback();
				coordinator.endGlobalTransaction(globalTxn->getGlobalXID());
				return false;
			}
		} else {
			// Data source doesn't support 2PC
			globalTxn->rollback();
			coordinator.endGlobalTransaction(globalTxn->getGlobalXID());
			return false;
		}
	}
	
	// Store global transaction mapping
	m_globalTransactions.put(localTra, globalTxn);
	
	return true;
}

bool ExternalDataSourceRegistry::prepare2PC(jrd_tra* localTra)
{
	RefMutexGuard guard(m_mutex, FB_FUNCTION);
	
	XAGlobalTransaction* globalTxn = nullptr;
	if (!m_globalTransactions.get(localTra, globalTxn) || !globalTxn) {
		return false;
	}
	
	return globalTxn->prepare();
}

bool ExternalDataSourceRegistry::commit2PC(jrd_tra* localTra)
{
	RefMutexGuard guard(m_mutex, FB_FUNCTION);
	
	XAGlobalTransaction* globalTxn = nullptr;
	if (!m_globalTransactions.get(localTra, globalTxn) || !globalTxn) {
		return false;
	}
	
	bool result = globalTxn->commit();
	
	// Clean up
	m_globalTransactions.remove(localTra);
	XACoordinator::instance().endGlobalTransaction(globalTxn->getGlobalXID());
	
	return result;
}

bool ExternalDataSourceRegistry::rollback2PC(jrd_tra* localTra)
{
	RefMutexGuard guard(m_mutex, FB_FUNCTION);
	
	XAGlobalTransaction* globalTxn = nullptr;
	if (!m_globalTransactions.get(localTra, globalTxn) || !globalTxn) {
		return false;
	}
	
	bool result = globalTxn->rollback();
	
	// Clean up
	m_globalTransactions.remove(localTra);
	XACoordinator::instance().endGlobalTransaction(globalTxn->getGlobalXID());
	
	return result;
}

void ExternalDataSourceRegistry::createConnectionPool(const string& name)
{
	ExternalDataSourceConfig* config = nullptr;
	if (m_dataSources.get(name, config)) {
		ExternalDataSourcePool* pool = new ExternalDataSourcePool(*config);
		m_connectionPools.put(name, pool);
	}
}

void ExternalDataSourceRegistry::destroyConnectionPool(const string& name)
{
	ExternalDataSourcePool* pool = nullptr;
	if (m_connectionPools.get(name, pool)) {
		m_connectionPools.remove(name);
		delete pool;
	}
}

void ExternalDataSourceRegistry::createXAResourceManager(const string& name)
{
	ExternalDataSourceConfig* config = nullptr;
	if (m_dataSources.get(name, config)) {
		ScratchBirdXAResourceManager* rm = new ScratchBirdXAResourceManager(*config);
		
		// Set connection pool
		ExternalDataSourcePool* pool = nullptr;
		if (m_connectionPools.get(name, pool)) {
			rm->setConnectionPool(pool);
		}
		
		m_xaResourceManagers.put(name, rm);
		
		// Register with XA coordinator
		XACoordinator::instance().registerResourceManager(rm);
	}
}

void ExternalDataSourceRegistry::destroyXAResourceManager(const string& name)
{
	ScratchBirdXAResourceManager* rm = nullptr;
	if (m_xaResourceManagers.get(name, rm)) {
		// Unregister from XA coordinator
		XACoordinator::instance().unregisterResourceManager(rm);
		
		m_xaResourceManagers.remove(name);
		delete rm;
	}
}

void ExternalDataSourceRegistry::loadConfiguration()
{
	// TODO: Implement configuration file loading
	// For now, this is a placeholder
}

void ExternalDataSourceRegistry::saveConfiguration()
{
	// TODO: Implement configuration file saving
	// For now, this is a placeholder
}

void ExternalDataSourceRegistry::getStatistics(string& result)
{
	RefMutexGuard guard(m_mutex, FB_FUNCTION);
	
	result = "External Data Source Statistics:\n";
	result.printf("  Registered data sources: %d\n", m_dataSources.count());
	result.printf("  Active connection pools: %d\n", m_connectionPools.count());
	result.printf("  XA resource managers: %d\n", m_xaResourceManagers.count());
	
	// Add pool statistics
	ConnectionPoolMap::Accessor poolAccessor(&m_connectionPools);
	for (bool found = poolAccessor.getFirst(); found; found = poolAccessor.getNext()) {
		const string& name = poolAccessor.current()->first;
		ExternalDataSourcePool* pool = poolAccessor.current()->second;
		
		result.printf("  Pool '%s': %d active, %d idle, %d total\n",
					  name.c_str(),
					  pool->getActiveConnections(),
					  pool->getIdleConnections(),
					  pool->getTotalConnections());
	}
}