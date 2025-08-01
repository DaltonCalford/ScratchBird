/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		IndexTypeRegistry.cpp
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

#include "scratchbird.h"
#include "../jrd/IndexTypeRegistry.h"
#include "../jrd/Database.h"
#include "../jrd/jrd.h"
#include "../jrd/tra.h"
#include "../jrd/constants.h"
#include "../common/classes/init.h"
#include "../common/gdsassert.h"

using namespace Jrd;
using namespace ScratchBird;

// Constants
const IndexTypeRegistry::IndexTypeName IndexTypeRegistry::DEFAULT_INDEX_TYPE = IDX_TYPE_NAME_BTREE;
const int IndexTypeRegistry::DEFAULT_INDEX_TYPE_ID = IDX_TYPE_BTREE;

namespace {
	// Global registry instance
	static GlobalPtr<IndexTypeRegistry> g_indexTypeRegistry;
}

//---------------------
// IndexTypeRegistry
//---------------------

IndexTypeRegistry::IndexTypeRegistry()
	: m_mutex("IndexTypeRegistry"),
	  m_initialized(false)
{
	// Constructor - initialization happens in initialize()
}

IndexTypeRegistry::~IndexTypeRegistry()
{
	try
	{
		shutdown();
	}
	catch (...)
	{
		// Destructor should not throw
	}
}

IndexTypeRegistry& IndexTypeRegistry::instance()
{
	return *g_indexTypeRegistry;
}

bool IndexTypeRegistry::initialize(thread_db* tdbb)
{
	MutexLockGuard guard(m_mutex, FB_FUNCTION);

	if (m_initialized)
		return true;

	try
	{
		// Initialize arrays
		m_entries.clear();
		m_factories.clear();
		m_typeNames.clear();

		// Register built-in index types
		registerBuiltinTypes();

		m_initialized = true;
		return true;
	}
	catch (const Exception& ex)
	{
		// Log error and return false
		gds__log("IndexTypeRegistry::initialize() failed: %s", ex.what());
		return false;
	}
	catch (...)
	{
		gds__log("IndexTypeRegistry::initialize() failed with unknown error");
		return false;
	}
}

void IndexTypeRegistry::shutdown()
{
	MutexLockGuard guard(m_mutex, FB_FUNCTION);

	if (!m_initialized)
		return;

	try
	{
		// Cleanup all registered factories
		for (size_t i = 0; i < m_entries.getCount(); i++)
		{
			RegistryEntry& entry = m_entries[i];
			if (entry.factory)
			{
				delete entry.factory;
				entry.factory = nullptr;
			}
		}

		// Clear all arrays
		m_entries.clear();
		m_factories.clear();
		m_typeNames.clear();

		m_initialized = false;
	}
	catch (...)
	{
		// Ignore errors during shutdown
	}
}

bool IndexTypeRegistry::registerIndexType(const IndexTypeName& typeName, int typeId,
										   IndexTypeFactory* factory)
{
	if (!factory || typeName.isEmpty() || !isValidTypeName(typeName))
		return false;

	MutexLockGuard guard(m_mutex, FB_FUNCTION);

	// Check for duplicate names or IDs
	if (findByName(typeName) || findById(typeId))
		return false;

	try
	{
		// Create new registry entry
		RegistryEntry entry(typeName, typeId, factory);
		m_entries.add(entry);

		// Also maintain separate arrays for quick access
		m_factories.add(factory);
		m_typeNames.add(typeName);

		return true;
	}
	catch (...)
	{
		return false;
	}
}

bool IndexTypeRegistry::unregisterIndexType(const IndexTypeName& typeName)
{
	MutexLockGuard guard(m_mutex, FB_FUNCTION);

	for (size_t i = 0; i < m_entries.getCount(); i++)
	{
		RegistryEntry& entry = m_entries[i];
		if (entry.typeName == typeName)
		{
			// Cleanup factory
			delete entry.factory;

			// Remove from all arrays
			m_entries.removeAt(i);
			
			// Also remove from other arrays (find by matching factory pointer)
			for (size_t j = 0; j < m_factories.getCount(); j++)
			{
				if (m_factories[j] == entry.factory)
				{
					m_factories.removeAt(j);
					m_typeNames.removeAt(j);
					break;
				}
			}

			return true;
		}
	}

	return false;
}

IndexType* IndexTypeRegistry::createIndex(thread_db* tdbb, const IndexTypeName& typeName,
										   Database* database, jrd_rel* relation,
										   const index_desc* desc)
{
	MutexLockGuard guard(m_mutex, FB_FUNCTION);

	RegistryEntry* entry = findByName(typeName);
	if (!entry || !entry->factory)
		return nullptr;

	try
	{
		return entry->factory->createIndex(tdbb, database, relation, desc);
	}
	catch (...)
	{
		return nullptr;
	}
}

IndexType* IndexTypeRegistry::createIndexById(thread_db* tdbb, int typeId,
											   Database* database, jrd_rel* relation,
											   const index_desc* desc)
{
	MutexLockGuard guard(m_mutex, FB_FUNCTION);

	RegistryEntry* entry = findById(typeId);
	if (!entry || !entry->factory)
		return nullptr;

	try
	{
		return entry->factory->createIndex(tdbb, database, relation, desc);
	}
	catch (...)
	{
		return nullptr;
	}
}

bool IndexTypeRegistry::isSupported(const IndexTypeName& typeName) const
{
	MutexLockGuard guard(m_mutex, FB_FUNCTION);
	return findByName(typeName) != nullptr;
}

bool IndexTypeRegistry::isSupportedById(int typeId) const
{
	MutexLockGuard guard(m_mutex, FB_FUNCTION);
	return findById(typeId) != nullptr;
}

void IndexTypeRegistry::getSupportedTypes(NameArray& names) const
{
	MutexLockGuard guard(m_mutex, FB_FUNCTION);
	
	names.clear();
	for (size_t i = 0; i < m_entries.getCount(); i++)
	{
		names.add(m_entries[i].typeName);
	}
}

int IndexTypeRegistry::getTypeId(const IndexTypeName& typeName) const
{
	MutexLockGuard guard(m_mutex, FB_FUNCTION);

	RegistryEntry* entry = findByName(typeName);
	return entry ? entry->typeId : -1;
}

IndexTypeRegistry::IndexTypeName IndexTypeRegistry::getTypeName(int typeId) const
{
	MutexLockGuard guard(m_mutex, FB_FUNCTION);

	RegistryEntry* entry = findById(typeId);
	return entry ? entry->typeName : IndexTypeName();
}

IndexTypeFactory* IndexTypeRegistry::getFactory(const IndexTypeName& typeName) const
{
	MutexLockGuard guard(m_mutex, FB_FUNCTION);

	RegistryEntry* entry = findByName(typeName);
	return entry ? entry->factory : nullptr;
}

IndexTypeFactory* IndexTypeRegistry::getFactoryById(int typeId) const
{
	MutexLockGuard guard(m_mutex, FB_FUNCTION);

	RegistryEntry* entry = findById(typeId);
	return entry ? entry->factory : nullptr;
}

const IndexTypeRegistry::IndexTypeName& IndexTypeRegistry::getDefaultType() const
{
	return DEFAULT_INDEX_TYPE;
}

int IndexTypeRegistry::getDefaultTypeId() const
{
	return DEFAULT_INDEX_TYPE_ID;
}

bool IndexTypeRegistry::isValidTypeName(const IndexTypeName& typeName)
{
	if (typeName.isEmpty() || typeName.length() > 31)
		return false;

	// Type name must be a valid SQL identifier
	// Start with letter, contain only letters, digits, and underscores
	const char* str = typeName.c_str();
	
	if (!isalpha(*str) && *str != '_')
		return false;

	for (const char* p = str + 1; *p; p++)
	{
		if (!isalnum(*p) && *p != '_')
			return false;
	}

	return true;
}

IndexTypeRegistry::RegistryEntry* IndexTypeRegistry::findByName(const IndexTypeName& typeName) const
{
	for (size_t i = 0; i < m_entries.getCount(); i++)
	{
		if (m_entries[i].typeName == typeName)
			return const_cast<RegistryEntry*>(&m_entries[i]);
	}
	return nullptr;
}

IndexTypeRegistry::RegistryEntry* IndexTypeRegistry::findById(int typeId) const
{
	for (size_t i = 0; i < m_entries.getCount(); i++)
	{
		if (m_entries[i].typeId == typeId)
			return const_cast<RegistryEntry*>(&m_entries[i]);
	}
	return nullptr;
}

void IndexTypeRegistry::registerBuiltinTypes()
{
	// For now, we'll just have BTREE as the default type
	// Hash and other types will be registered when their implementations are complete
	
	// Note: We'll need to create a BTreeIndexFactory class later
	// For now, this is just the infrastructure setup
	
	// TODO: Register BTREE factory when BTreeIndex class is created
	// TODO: Register HASH factory when HashIndex class is created
	// TODO: Register GIN factory when GinIndex class is created
}

//---------------------
// IndexTypeRegistryInitializer
//---------------------

void IndexTypeRegistryInitializer::initialize()
{
	// This will be called during module initialization
	// The registry instance will be created automatically by GlobalPtr
}

void IndexTypeRegistryInitializer::shutdown()
{
	// Cleanup handled by GlobalPtr destructor
}

//---------------------
// Module initialization
//---------------------

namespace {
	class IndexTypeRegistryInit
	{
	public:
		IndexTypeRegistryInit()
		{
			IndexTypeRegistryInitializer::initialize();
		}

		~IndexTypeRegistryInit()
		{
			IndexTypeRegistryInitializer::shutdown();
		}
	};

	// This will trigger initialization when the module is loaded
	static IndexTypeRegistryInit g_init;
}