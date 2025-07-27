/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		IndexTypeRegistry.h
 *	DESCRIPTION:	Registry for pluggable index type implementations
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
 *
 * 2025.07.22 - ScratchBird Hash Index Implementation - Index Type Registry
 */

#ifndef JRD_INDEX_TYPE_REGISTRY_H
#define JRD_INDEX_TYPE_REGISTRY_H

#include "../common/classes/fb_string.h"
#include "../common/classes/objects_array.h"
#include "../common/classes/init.h"
#include "../common/classes/auto.h"
#include "../common/ThreadStart.h"
#include "../jrd/IndexType.h"

namespace Jrd {

class thread_db;
class Database;
class jrd_rel;

struct index_desc;

/**
 * Global registry for index type implementations.
 * 
 * This singleton class manages the registration and creation of different
 * index types in ScratchBird. Index types are registered at system startup
 * and can be queried and instantiated throughout the database lifetime.
 * 
 * The registry is thread-safe and handles concurrent access from multiple
 * database connections.
 */
class IndexTypeRegistry
{
public:
	// Type alias for cleaner code
	typedef ScratchBird::string IndexTypeName;
	typedef ScratchBird::ObjectsArray<IndexTypeFactory*> FactoryArray;
	typedef ScratchBird::ObjectsArray<IndexTypeName> NameArray;

private:
	// Singleton pattern - private constructor
	IndexTypeRegistry();

	// Registry data protected by mutex
	mutable ScratchBird::Mutex m_mutex;
	FactoryArray m_factories;
	NameArray m_typeNames;
	bool m_initialized;

	// Registry entry structure
	struct RegistryEntry
	{
		IndexTypeName typeName;
		int typeId;
		IndexTypeFactory* factory;
		
		RegistryEntry(const IndexTypeName& name, int id, IndexTypeFactory* f)
			: typeName(name), typeId(id), factory(f) {}
	};

	ScratchBird::ObjectsArray<RegistryEntry> m_entries;

public:
	// Singleton access
	static IndexTypeRegistry& instance();
	
	// Destructor
	~IndexTypeRegistry();

	/**
	 * Initialize the registry with built-in index types.
	 * Called during database system startup.
	 * 
	 * @param tdbb		Thread database block
	 * @return			True on success, false on error
	 */
	bool initialize(thread_db* tdbb);

	/**
	 * Shutdown the registry and cleanup resources.
	 * Called during database system shutdown.
	 */
	void shutdown();

	/**
	 * Register a new index type factory.
	 * 
	 * @param typeName	Name of the index type (e.g., "HASH")
	 * @param typeId	Numeric identifier for the index type
	 * @param factory	Factory instance for creating indexes
	 * @return			True on success, false if type already exists
	 */
	bool registerIndexType(const IndexTypeName& typeName, int typeId, 
						   IndexTypeFactory* factory);

	/**
	 * Unregister an index type (used mainly for testing/cleanup).
	 * 
	 * @param typeName	Name of the index type to unregister
	 * @return			True on success, false if type not found
	 */
	bool unregisterIndexType(const IndexTypeName& typeName);

	/**
	 * Create an instance of the specified index type.
	 * 
	 * @param tdbb		Thread database block
	 * @param typeName	Name of the index type to create
	 * @param database	Database instance
	 * @param relation	Relation containing the index
	 * @param desc		Index descriptor with configuration
	 * @return			New index instance, or nullptr on error
	 */
	IndexType* createIndex(thread_db* tdbb, const IndexTypeName& typeName,
						   Database* database, jrd_rel* relation, 
						   const index_desc* desc);

	/**
	 * Create an index by numeric type ID (used by ODS loading).
	 * 
	 * @param tdbb		Thread database block
	 * @param typeId	Numeric type identifier
	 * @param database	Database instance
	 * @param relation	Relation containing the index
	 * @param desc		Index descriptor with configuration
	 * @return			New index instance, or nullptr on error
	 */
	IndexType* createIndexById(thread_db* tdbb, int typeId,
							   Database* database, jrd_rel* relation,
							   const index_desc* desc);

	/**
	 * Check if an index type is registered and supported.
	 * 
	 * @param typeName	Name of the index type to check
	 * @return			True if supported, false otherwise
	 */
	bool isSupported(const IndexTypeName& typeName) const;

	/**
	 * Check if a numeric type ID is registered and supported.
	 * 
	 * @param typeId	Numeric type identifier to check
	 * @return			True if supported, false otherwise
	 */
	bool isSupportedById(int typeId) const;

	/**
	 * Get a list of all supported index type names.
	 * Used for DDL validation and user interfaces.
	 * 
	 * @param names		Array to populate with type names
	 */
	void getSupportedTypes(NameArray& names) const;

	/**
	 * Get the numeric type ID for a given type name.
	 * 
	 * @param typeName	Name of the index type
	 * @return			Numeric type ID, or -1 if not found
	 */
	int getTypeId(const IndexTypeName& typeName) const;

	/**
	 * Get the type name for a given numeric type ID.
	 * 
	 * @param typeId	Numeric type identifier  
	 * @return			Type name, or empty string if not found
	 */
	IndexTypeName getTypeName(int typeId) const;

	/**
	 * Get the factory for a given index type.
	 * Used for introspection and advanced operations.
	 * 
	 * @param typeName	Name of the index type
	 * @return			Factory instance, or nullptr if not found
	 */
	IndexTypeFactory* getFactory(const IndexTypeName& typeName) const;

	/**
	 * Get the factory for a given numeric type ID.
	 * 
	 * @param typeId	Numeric type identifier
	 * @return			Factory instance, or nullptr if not found
	 */
	IndexTypeFactory* getFactoryById(int typeId) const;

	/**
	 * Get the default index type name.
	 * Used when no explicit type is specified in CREATE INDEX.
	 * 
	 * @return			Default index type name (currently "BTREE")
	 */
	const IndexTypeName& getDefaultType() const;

	/**
	 * Get the default index type ID.
	 * 
	 * @return			Default index type ID
	 */
	int getDefaultTypeId() const;

	/**
	 * Validate that an index type name is a valid identifier.
	 * 
	 * @param typeName	Type name to validate
	 * @return			True if valid, false otherwise
	 */
	static bool isValidTypeName(const IndexTypeName& typeName);

private:
	// Internal helper methods

	/**
	 * Find registry entry by type name.
	 * Must be called with mutex held.
	 * 
	 * @param typeName	Type name to find
	 * @return			Registry entry, or nullptr if not found
	 */
	RegistryEntry* findByName(const IndexTypeName& typeName) const;

	/**
	 * Find registry entry by type ID.
	 * Must be called with mutex held.
	 * 
	 * @param typeId	Type ID to find
	 * @return			Registry entry, or nullptr if not found
	 */
	RegistryEntry* findById(int typeId) const;

	/**
	 * Register built-in index types.
	 * Called during initialize().
	 */
	void registerBuiltinTypes();

	// Constants
	static const IndexTypeName DEFAULT_INDEX_TYPE;
	static const int DEFAULT_INDEX_TYPE_ID;
};

// Global initialization helper
class IndexTypeRegistryInitializer
{
public:
	static void initialize();
	static void shutdown();
};

// Module initialization - called at library load time
static ScratchBird::GlobalPtr<IndexTypeRegistryInitializer> g_indexTypeRegistryInit;

} // namespace Jrd

#endif // JRD_INDEX_TYPE_REGISTRY_H