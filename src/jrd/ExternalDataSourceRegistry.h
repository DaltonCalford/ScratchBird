/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		ExternalDataSourceRegistry.h
 *	DESCRIPTION:	Registry for 2PC-capable External Data Sources
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

#ifndef JRD_EXTERNAL_DATA_SOURCE_REGISTRY_H
#define JRD_EXTERNAL_DATA_SOURCE_REGISTRY_H

#include "../common/classes/fb_string.h"
#include "../common/classes/array.h"
#include "../common/classes/GenericMap.h"
#include "../common/classes/RefMutex.h"
#include "XACoordinator.h"
#include "extds/ExtDS.h"

namespace Jrd {

// External Data Source configuration
struct ExternalDataSourceConfig
{
	ScratchBird::string name;				// Unique data source name
	ScratchBird::string provider;			// Provider type (firebird, postgresql, mysql, etc.)
	ScratchBird::string connectionString;	// Connection parameters
	ScratchBird::string username;			// Authentication username
	ScratchBird::string password;			// Authentication password (encrypted)
	bool supports2PC;						// Whether data source supports 2PC
	bool poolConnections;					// Whether to pool connections
	ULONG maxConnections;					// Maximum connections in pool
	ULONG connectionTimeout;				// Connection timeout in seconds
	ULONG queryTimeout;						// Query timeout in seconds
	ScratchBird::string encryptionKey;		// Encryption key for credentials
	
	ExternalDataSourceConfig();
	ExternalDataSourceConfig(const ScratchBird::string& dsName, 
							 const ScratchBird::string& dsProvider,
							 const ScratchBird::string& connStr);
	
	void encryptCredentials();
	void decryptCredentials();
	bool isValid() const;
};

// Connection pool for external data sources
class ExternalDataSourcePool
{
public:
	ExternalDataSourcePool(const ExternalDataSourceConfig& config);
	~ExternalDataSourcePool();
	
	// Connection management
	EDS::Connection* getConnection(Jrd::thread_db* tdbb);
	void releaseConnection(EDS::Connection* connection);
	
	// Pool statistics
	ULONG getActiveConnections() const { return m_activeConnections; }
	ULONG getIdleConnections() const { return m_idleConnections.getCount(); }
	ULONG getTotalConnections() const { return m_activeConnections + getIdleConnections(); }
	
	// Configuration
	const ExternalDataSourceConfig& getConfig() const { return m_config; }
	
private:
	ExternalDataSourceConfig m_config;
	ScratchBird::Array<EDS::Connection*> m_idleConnections;
	ULONG m_activeConnections;
	ScratchBird::RefMutex m_mutex;
	
	EDS::Connection* createConnection(Jrd::thread_db* tdbb);
	void destroyConnection(EDS::Connection* connection);
	void validateConnection(EDS::Connection* connection);
};

// XA Resource Manager implementation for ScratchBird external data sources
class ScratchBirdXAResourceManager : public XAResourceManager
{
public:
	ScratchBirdXAResourceManager(const ExternalDataSourceConfig& config);
	virtual ~ScratchBirdXAResourceManager();
	
	// XAResourceManager interface
	virtual int xa_start(const XATransactionId& xid, SLONG flags) override;
	virtual int xa_end(const XATransactionId& xid, SLONG flags) override;
	virtual int xa_prepare(const XATransactionId& xid) override;
	virtual int xa_commit(const XATransactionId& xid, bool onePhase) override;
	virtual int xa_rollback(const XATransactionId& xid) override;
	virtual int xa_recover(XATransactionId* xids, SLONG count, SLONG flags) override;
	virtual int xa_forget(const XATransactionId& xid) override;
	
	virtual const char* getResourceManagerName() const override;
	virtual ScratchBird::string getConnectionString() const override;
	virtual bool isSameRM(const XAResourceManager* other) const override;
	
	// External data source specific methods
	EDS::Connection* getConnection(const XATransactionId& xid);
	void setConnectionPool(ExternalDataSourcePool* pool) { m_connectionPool = pool; }
	
private:
	ExternalDataSourceConfig m_config;
	ExternalDataSourcePool* m_connectionPool;
	
	typedef ScratchBird::GenericMap<ScratchBird::Pair<ScratchBird::Left<XATransactionId, EDS::Connection*>>> XAConnectionMap;
	XAConnectionMap m_xaConnections;
	ScratchBird::RefMutex m_mutex;
	
	EDS::Connection* findOrCreateConnection(const XATransactionId& xid);
	void removeConnection(const XATransactionId& xid);
};

// Registry for external data sources with 2PC support
class ExternalDataSourceRegistry
{
public:
	static ExternalDataSourceRegistry& instance();
	
	ExternalDataSourceRegistry();
	~ExternalDataSourceRegistry();
	
	// Data source management
	void registerDataSource(const ExternalDataSourceConfig& config);
	void unregisterDataSource(const ScratchBird::string& name);
	ExternalDataSourceConfig* findDataSource(const ScratchBird::string& name);
	
	// Connection pool management
	ExternalDataSourcePool* getConnectionPool(const ScratchBird::string& name);
	
	// XA Resource Manager management
	ScratchBirdXAResourceManager* getXAResourceManager(const ScratchBird::string& name);
	
	// 2PC transaction support
	bool beginDistributedTransaction(Jrd::jrd_tra* localTra, 
									 const ScratchBird::Array<ScratchBird::string>& dataSourceNames);
	bool prepare2PC(Jrd::jrd_tra* localTra);
	bool commit2PC(Jrd::jrd_tra* localTra);
	bool rollback2PC(Jrd::jrd_tra* localTra);
	
	// Configuration persistence
	void loadConfiguration();
	void saveConfiguration();
	
	// Statistics and monitoring
	void getStatistics(ScratchBird::string& result);
	
private:
	typedef ScratchBird::GenericMap<ScratchBird::Pair<ScratchBird::Left<ScratchBird::string, ExternalDataSourceConfig*>>> DataSourceConfigMap;
	typedef ScratchBird::GenericMap<ScratchBird::Pair<ScratchBird::Left<ScratchBird::string, ExternalDataSourcePool*>>> ConnectionPoolMap;
	typedef ScratchBird::GenericMap<ScratchBird::Pair<ScratchBird::Left<ScratchBird::string, ScratchBirdXAResourceManager*>>> XAResourceManagerMap;
	typedef ScratchBird::GenericMap<ScratchBird::Pair<ScratchBird::Left<Jrd::jrd_tra*, XAGlobalTransaction*>>> TransactionMap;
	
	DataSourceConfigMap m_dataSources;
	ConnectionPoolMap m_connectionPools;
	XAResourceManagerMap m_xaResourceManagers;
	TransactionMap m_globalTransactions;
	ScratchBird::RefMutex m_mutex;
	
	ScratchBird::string m_configFileName;
	
	void createConnectionPool(const ScratchBird::string& name);
	void destroyConnectionPool(const ScratchBird::string& name);
	void createXAResourceManager(const ScratchBird::string& name);
	void destroyXAResourceManager(const ScratchBird::string& name);
	
	// Configuration file operations
	void parseConfigurationFile(const ScratchBird::string& filename);
	void writeConfigurationFile(const ScratchBird::string& filename);
	
	// Singleton instance
	static ExternalDataSourceRegistry* s_instance;
	static ScratchBird::GlobalPtr<ScratchBird::Mutex> s_instanceMutex;
};

// DDL support for external data source management
class ExternalDataSourceDDL
{
public:
	// CREATE EXTERNAL DATA SOURCE name TYPE provider CONNECTION 'connection_string' 
	// [USER 'username' PASSWORD 'password'] [WITH 2PC] [POOL CONNECTIONS max_count]
	static void createExternalDataSource(Jrd::thread_db* tdbb,
										  const ScratchBird::string& name,
										  const ScratchBird::string& provider,
										  const ScratchBird::string& connectionString,
										  const ScratchBird::string& username = "",
										  const ScratchBird::string& password = "",
										  bool with2PC = false,
										  bool poolConnections = true,
										  ULONG maxConnections = 10);
	
	// ALTER EXTERNAL DATA SOURCE name [CONNECTION 'new_connection_string'] 
	// [USER 'new_username' PASSWORD 'new_password'] [WITH|WITHOUT 2PC]
	static void alterExternalDataSource(Jrd::thread_db* tdbb,
										 const ScratchBird::string& name,
										 const ScratchBird::string& newConnectionString = "",
										 const ScratchBird::string& newUsername = "",
										 const ScratchBird::string& newPassword = "",
										 bool enable2PC = false,
										 bool disable2PC = false);
	
	// DROP EXTERNAL DATA SOURCE name
	static void dropExternalDataSource(Jrd::thread_db* tdbb,
										const ScratchBird::string& name);
	
	// SHOW EXTERNAL DATA SOURCES
	static void showExternalDataSources(Jrd::thread_db* tdbb, ScratchBird::string& result);
};

// System tables for external data source metadata
namespace ExternalDataSourceMetadata {
	
	// RDB$EXTERNAL_DATA_SOURCES
	struct ExternalDataSourceRecord {
		ScratchBird::string name;
		ScratchBird::string provider;
		ScratchBird::string connectionString;
		ScratchBird::string username;
		ScratchBird::string encryptedPassword;
		bool supports2PC;
		bool poolConnections;
		ULONG maxConnections;
		ULONG connectionTimeout;
		ULONG queryTimeout;
		ScratchBird::string description;
	};
	
	void insertExternalDataSourceRecord(Jrd::thread_db* tdbb, const ExternalDataSourceRecord& record);
	void updateExternalDataSourceRecord(Jrd::thread_db* tdbb, const ExternalDataSourceRecord& record);
	void deleteExternalDataSourceRecord(Jrd::thread_db* tdbb, const ScratchBird::string& name);
	bool selectExternalDataSourceRecord(Jrd::thread_db* tdbb, const ScratchBird::string& name, ExternalDataSourceRecord& record);
	void selectAllExternalDataSourceRecords(Jrd::thread_db* tdbb, ScratchBird::Array<ExternalDataSourceRecord>& records);
}

} // namespace Jrd

#endif // JRD_EXTERNAL_DATA_SOURCE_REGISTRY_H