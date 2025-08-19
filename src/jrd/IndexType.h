/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		IndexType.h
 *	DESCRIPTION:	Abstract base class for pluggable index types
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

#ifndef JRD_INDEX_TYPE_H
#define JRD_INDEX_TYPE_H

#include "../common/dsc.h"
#include "../jrd/RecordNumber.h"
#include "../common/classes/fb_string.h"

namespace Jrd {

class thread_db;
class Database;
class jrd_rel;
class jrd_tra;
class IndexRetrieval;
class Record;

struct index_desc;
struct temporary_key;

// Forward declarations for statistics
struct IndexStatistics 
{
	ULONG total_keys;          // Total number of keys in index
	ULONG total_nodes;         // Total number of nodes/pages
	ULONG avg_key_length;      // Average key length
	ULONG max_key_length;      // Maximum key length  
	float selectivity;         // Overall selectivity
	ULONG storage_bytes;       // Total storage used
	double avg_fanout;         // Average fanout (for tree-based indexes)
	ULONG overflow_pages;      // Number of overflow pages (for hash indexes)
	double load_factor;        // Load factor (for hash indexes)
};

// Index error codes specific to index types
enum index_error_t {
	idx_err_success = 0,
	idx_err_key_too_long,
	idx_err_duplicate_key,
	idx_err_key_not_found,
	idx_err_invalid_key_type,
	idx_err_index_corrupt,
	idx_err_out_of_memory,
	idx_err_invalid_operation
};

/**
 * Abstract base class for all index type implementations.
 * 
 * This class defines the interface that all index types must implement
 * to integrate with ScratchBird's index management system. Each index
 * type (B-Tree, Hash, GIN, etc.) will inherit from this class and
 * provide specific implementations for their storage and retrieval
 * algorithms.
 */
class IndexType
{
public:
	virtual ~IndexType() = default;

	/**
	 * Initialize the index with the given descriptor.
	 * Called during index creation or when opening an existing index.
	 * 
	 * @param tdbb		Thread database block
	 * @param database	Database instance  
	 * @param relation	Relation containing the index
	 * @param desc		Index descriptor with configuration
	 * @return			Success/error code
	 */
	virtual index_error_t initialize(thread_db* tdbb, Database* database, 
									 jrd_rel* relation, const index_desc* desc) = 0;

	/**
	 * Insert a key-value pair into the index.
	 * 
	 * @param tdbb		Thread database block
	 * @param key		Key descriptor to insert
	 * @param record	Record number associated with key
	 * @param transaction Transaction context
	 * @return			Success/error code
	 */
	virtual index_error_t insert(thread_db* tdbb, const dsc* key, 
								 RecordNumber record, jrd_tra* transaction) = 0;

	/**
	 * Look up keys in the index and populate retrieval structure.
	 * 
	 * @param tdbb		Thread database block
	 * @param key		Key to search for (can be partial)
	 * @param retrieval	Index retrieval structure to populate
	 * @return			True if key found, false otherwise
	 */
	virtual bool lookup(thread_db* tdbb, const dsc* key, IndexRetrieval* retrieval) = 0;

	/**
	 * Remove a key-value pair from the index.
	 * 
	 * @param tdbb		Thread database block
	 * @param key		Key descriptor to remove
	 * @param record	Record number associated with key
	 * @param transaction Transaction context
	 * @return			Success/error code
	 */
	virtual index_error_t remove(thread_db* tdbb, const dsc* key, 
								 RecordNumber record, jrd_tra* transaction) = 0;

	/**
	 * Calculate selectivity for a given key or key prefix.
	 * Used by the query optimizer for cost estimation.
	 * 
	 * @param tdbb		Thread database block
	 * @param key		Key to calculate selectivity for (can be partial)
	 * @return			Selectivity value (0.0 to 1.0)
	 */
	virtual double calculateSelectivity(thread_db* tdbb, const dsc* key) = 0;

	/**
	 * Get comprehensive statistics about the index.
	 * Used for monitoring and optimization.
	 * 
	 * @param tdbb		Thread database block
	 * @param stats		Statistics structure to populate
	 * @return			Success/error code
	 */
	virtual index_error_t getStatistics(thread_db* tdbb, IndexStatistics* stats) = 0;

	/**
	 * Validate the integrity of the index structure.
	 * Used during database validation and repair operations.
	 * 
	 * @param tdbb		Thread database block
	 * @return			Success/error code
	 */
	virtual index_error_t validate(thread_db* tdbb) = 0;

	/**
	 * Rebuild the index from scratch.
	 * Used during index maintenance and repair operations.
	 * 
	 * @param tdbb		Thread database block
	 * @param transaction Transaction context
	 * @return			Success/error code
	 */
	virtual index_error_t rebuild(thread_db* tdbb, jrd_tra* transaction) = 0;

	/**
	 * Get the name identifier for this index type.
	 * Used for catalog storage and user interface.
	 * 
	 * @return			String identifier (e.g., "BTREE", "HASH", "GIN")
	 */
	virtual const char* getTypeName() const = 0;

	/**
	 * Get version information for this index type implementation.
	 * Used for compatibility checking and upgrades.
	 * 
	 * @return			Version string
	 */
	virtual const char* getVersion() const = 0;

	/**
	 * Check if this index type supports the given data type.
	 * Used during index creation validation.
	 * 
	 * @param field_type	Field type to check (from btr.h)
	 * @return				True if supported, false otherwise
	 */
	virtual bool supportsDataType(int field_type) const = 0;

	/**
	 * Check if this index type supports the given index flags.
	 * Used during index creation validation.
	 * 
	 * @param flags		Index flags to check (unique, descending, etc.)
	 * @return			True if supported, false otherwise
	 */
	virtual bool supportsIndexFlags(USHORT flags) const = 0;

	/**
	 * Get the optimal page size for this index type given the key characteristics.
	 * Used during index creation for performance optimization.
	 * 
	 * @param avg_key_length	Average expected key length
	 * @param cardinality		Expected number of unique keys
	 * @return					Recommended page size in bytes
	 */
	virtual USHORT getOptimalPageSize(USHORT avg_key_length, ULONG cardinality) const = 0;

	/**
	 * Estimate storage requirements for index with given characteristics.
	 * Used for capacity planning and space allocation.
	 * 
	 * @param num_keys			Expected number of keys
	 * @param avg_key_length	Average key length
	 * @return					Estimated storage in bytes
	 */
	virtual ULONG estimateStorageSize(ULONG num_keys, USHORT avg_key_length) const = 0;

protected:
	// Helper methods for derived classes

	/**
	 * Validate a key descriptor for this index type.
	 * Common validation logic that can be used by all index types.
	 * 
	 * @param key		Key descriptor to validate
	 * @return			Success/error code
	 */
	virtual index_error_t validateKey(const dsc* key) const;

	/**
	 * Convert a key descriptor to internal key format.
	 * Helper for index types that need key preprocessing.
	 * 
	 * @param key		Input key descriptor
	 * @param temp_key	Output temporary key structure
	 * @return			Success/error code
	 */
	virtual index_error_t convertKey(const dsc* key, temporary_key* temp_key) const;
};

/**
 * Factory interface for creating index type instances.
 * Each index type implementation provides a factory that can create
 * new instances of that index type.
 */
class IndexTypeFactory
{
public:
	virtual ~IndexTypeFactory() = default;

	/**
	 * Create a new instance of this index type.
	 * 
	 * @param tdbb		Thread database block
	 * @param database	Database instance
	 * @param relation	Relation containing the index
	 * @param desc		Index descriptor with configuration
	 * @return			New index instance, or nullptr on error
	 */
	virtual IndexType* createIndex(thread_db* tdbb, Database* database,
								   jrd_rel* relation, const index_desc* desc) = 0;

	/**
	 * Get the type name for indexes created by this factory.
	 * 
	 * @return			String identifier for this index type
	 */
	virtual const char* getTypeName() const = 0;

	/**
	 * Get the numeric type identifier for this index type.
	 * Used in the ODS format for index type identification.
	 * 
	 * @return			Numeric type identifier
	 */
	virtual int getTypeId() const = 0;
};

} // namespace Jrd

#endif // JRD_INDEX_TYPE_H