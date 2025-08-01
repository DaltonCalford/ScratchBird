/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		BitmapIndexMaintenance.h
 *	DESCRIPTION:	Bitmap index maintenance during DML operations
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
 * 2025.07.23 - ScratchBird Bitmap Index DML Maintenance System
 */

#ifndef JRD_BITMAP_INDEX_MAINTENANCE_H
#define JRD_BITMAP_INDEX_MAINTENANCE_H

#include "BitmapIndex.h"
#include "BitmapIndexSystemCatalog.h"
#include "../jrd/constants.h"
#include "../common/classes/array.h"

namespace Jrd {

// Forward declarations
class thread_db;
class jrd_tra;
class jrd_rel;
struct index_desc;
class Record;
class RecordNumber;

//----------------------------
// DML Operation Types
//----------------------------
enum BitmapDMLOperation : UCHAR
{
	BITMAP_DML_INSERT = 0,		// INSERT operation
	BITMAP_DML_UPDATE = 1,		// UPDATE operation  
	BITMAP_DML_DELETE = 2,		// DELETE operation
	BITMAP_DML_MERGE = 3		// MERGE operation
};

//----------------------------
// Bitmap Index Change Tracking
//----------------------------
struct BitmapIndexChange
{
	index_desc* bitmap_index;		// Index being modified
	BitmapDMLOperation operation;	// Type of DML operation
	RecordNumber record_number;		// Record being modified
	dsc old_value;					// Original column value (for UPDATE/DELETE)
	dsc new_value;					// New column value (for INSERT/UPDATE)
	bool old_value_null;			// True if old value is NULL
	bool new_value_null;			// True if new value is NULL
	ULONG change_sequence;			// Sequence number for ordering changes
	
	BitmapIndexChange()
		: bitmap_index(nullptr), operation(BITMAP_DML_INSERT),
		  old_value_null(true), new_value_null(true), change_sequence(0)
	{
		memset(&old_value, 0, sizeof(dsc));
		memset(&new_value, 0, sizeof(dsc));
	}
};

//----------------------------
// Bitmap Index Maintenance Engine
//----------------------------
class BitmapIndexMaintenance
{
public:
	// Main DML maintenance interface
	static void maintainBitmapIndexesForInsert(thread_db* tdbb, jrd_tra* transaction,
		jrd_rel* relation, const Record* record, RecordNumber record_number);
	
	static void maintainBitmapIndexesForUpdate(thread_db* tdbb, jrd_tra* transaction,
		jrd_rel* relation, const Record* old_record, const Record* new_record,
		RecordNumber record_number);
	
	static void maintainBitmapIndexesForDelete(thread_db* tdbb, jrd_tra* transaction,
		jrd_rel* relation, const Record* record, RecordNumber record_number);
	
	// Batch operations for performance
	static void maintainBitmapIndexesForBulkInsert(thread_db* tdbb, jrd_tra* transaction,
		jrd_rel* relation, const ScratchBird::ObjectsArray<Record*>& records,
		const ScratchBird::ObjectsArray<RecordNumber>& record_numbers);
	
	static void maintainBitmapIndexesForBulkDelete(thread_db* tdbb, jrd_tra* transaction,
		jrd_rel* relation, const ScratchBird::ObjectsArray<RecordNumber>& record_numbers);

	// Transaction support
	static void prepareBitmapIndexTransaction(thread_db* tdbb, jrd_tra* transaction);
	static void commitBitmapIndexChanges(thread_db* tdbb, jrd_tra* transaction);
	static void rollbackBitmapIndexChanges(thread_db* tdbb, jrd_tra* transaction);
	
	// Index rebuild and repair
	static void rebuildBitmapIndex(thread_db* tdbb, jrd_tra* transaction,
		const index_desc* bitmap_index);
	static void repairBitmapIndex(thread_db* tdbb, jrd_tra* transaction,
		const index_desc* bitmap_index);
	
	// Maintenance validation and diagnostics
	static bool validateBitmapIndexConsistency(thread_db* tdbb, 
		const index_desc* bitmap_index, ScratchBird::string& error_message);
	static void analyzeBitmapIndexHealth(thread_db* tdbb, const index_desc* bitmap_index,
		BitmapIndexHealthReport& report);

private:
	// Core maintenance operations
	static void insertValueIntoBitmapIndex(thread_db* tdbb, jrd_tra* transaction,
		const index_desc* bitmap_index, const dsc* value, RecordNumber record_number);
	
	static void deleteValueFromBitmapIndex(thread_db* tdbb, jrd_tra* transaction,
		const index_desc* bitmap_index, const dsc* value, RecordNumber record_number);
	
	static void updateValueInBitmapIndex(thread_db* tdbb, jrd_tra* transaction,
		const index_desc* bitmap_index, const dsc* old_value, const dsc* new_value,
		RecordNumber record_number);

	// Value processing and validation
	static void extractIndexedValue(const Record* record, USHORT field_id, dsc* value);
	static bool isValueChanged(const dsc* old_value, const dsc* new_value);
	static void validateIndexValue(const index_desc* bitmap_index, const dsc* value);
	
	// Bitmap operations
	static void addRecordToBitmap(thread_db* tdbb, jrd_tra* transaction,
		const index_desc* bitmap_index, const dsc* value, RecordNumber record_number);
	
	static void removeRecordFromBitmap(thread_db* tdbb, jrd_tra* transaction,
		const index_desc* bitmap_index, const dsc* value, RecordNumber record_number);
	
	// Statistics and metadata updates
	static void updateBitmapIndexStatistics(thread_db* tdbb, jrd_tra* transaction,
		const index_desc* bitmap_index);
	static void updateCardinalityStatistics(thread_db* tdbb, jrd_tra* transaction,
		const index_desc* bitmap_index, SLONG cardinality_change);
	
	// Performance optimization
	static bool shouldDeferMaintenance(thread_db* tdbb, const index_desc* bitmap_index);
	static void processDeferredMaintenance(thread_db* tdbb, jrd_tra* transaction,
		const index_desc* bitmap_index);
	
	// Error handling and recovery
	static void handleMaintenanceError(thread_db* tdbb, jrd_tra* transaction,
		const index_desc* bitmap_index, const Exception& error);
	static void logMaintenanceActivity(thread_db* tdbb, const index_desc* bitmap_index,
		BitmapDMLOperation operation, const char* details);

	// Utility methods
	static ScratchBird::ObjectsArray<index_desc*> getBitmapIndexesForRelation(
		thread_db* tdbb, jrd_rel* relation);
	static bool isBitmapIndexEnabled(thread_db* tdbb, const index_desc* bitmap_index);
	static ULONG getNextChangeSequence();
};

//----------------------------
// Bitmap Index Change Manager
//----------------------------
class BitmapIndexChangeManager
{
public:
	BitmapIndexChangeManager(thread_db* tdbb, jrd_tra* transaction);
	~BitmapIndexChangeManager();
	
	// Change tracking
	void recordChange(const BitmapIndexChange& change);
	void recordInsertChange(const index_desc* bitmap_index, const dsc* value, 
		RecordNumber record_number);
	void recordUpdateChange(const index_desc* bitmap_index, const dsc* old_value,
		const dsc* new_value, RecordNumber record_number);
	void recordDeleteChange(const index_desc* bitmap_index, const dsc* value,
		RecordNumber record_number);
	
	// Change processing
	void processAllChanges();
	void processChangesForIndex(const index_desc* bitmap_index);
	void rollbackAllChanges();
	
	// Transaction integration
	void prepareTransaction();
	void commitTransaction();
	void rollbackTransaction();
	
	// Statistics and reporting
	ULONG getChangeCount() const;
	ULONG getChangeCountForIndex(const index_desc* bitmap_index) const;
	void getChangeSummary(ScratchBird::string& summary);

private:
	thread_db* m_tdbb;
	jrd_tra* m_transaction;
	ScratchBird::ObjectsArray<BitmapIndexChange> m_changes;
	ScratchBird::ObjectsArray<BitmapIndexChange> m_processed_changes;
	bool m_changes_committed;
	ULONG m_next_sequence;
	
	// Change processing helpers
	void sortChangesBySequence();
	void groupChangesByIndex();
	void optimizeChanges();
	bool canOptimizeChanges(const BitmapIndexChange& change1, 
							const BitmapIndexChange& change2);
	
	// Validation and consistency
	void validateChange(const BitmapIndexChange& change);
	void validateChangeSequence();
};

//----------------------------
// Bitmap Index Health Monitor
//----------------------------
struct BitmapIndexHealthReport
{
	// Index identification
	ScratchBird::string index_name;
	ScratchBird::string relation_name;
	USHORT index_id;
	
	// Health metrics
	bool is_consistent;					// Overall consistency check
	bool metadata_valid;				// Metadata consistency
	bool bitmaps_valid;					// Bitmap data consistency
	bool statistics_accurate;			// Statistics accuracy
	
	// Performance metrics
	ULONG cardinality;					// Current cardinality
	double cardinality_ratio;			// Cardinality/total_records ratio
	double compression_ratio;			// Actual compression achieved
	USHORT cache_hit_rate;				// Cache performance
	
	// Maintenance metrics
	GDS_TIMESTAMP last_maintenance;		// Last maintenance timestamp
	ULONG maintenance_operations;		// Operations since last maintenance
	bool needs_maintenance;				// Maintenance recommendation
	
	// Error tracking
	ULONG error_count;					// Number of errors detected
	ScratchBird::string error_summary;	// Summary of errors found
	ScratchBird::string recommendations; // Recommended actions
	
	BitmapIndexHealthReport()
		: index_id(0), is_consistent(true), metadata_valid(true),
		  bitmaps_valid(true), statistics_accurate(true), cardinality(0),
		  cardinality_ratio(0.0), compression_ratio(0.0), cache_hit_rate(0),
		  last_maintenance(0), maintenance_operations(0), needs_maintenance(false),
		  error_count(0)
	{
	}
};

//----------------------------
// Bitmap Index Performance Monitor
//----------------------------
class BitmapIndexPerformanceMonitor
{
public:
	// Performance tracking
	static void recordMaintenanceOperation(const index_desc* bitmap_index,
		BitmapDMLOperation operation, ULONG duration_ms);
	static void recordQueryOperation(const index_desc* bitmap_index,
		ULONG records_processed, ULONG duration_ms);
	
	// Performance metrics
	static double getAverageMaintenanceTime(const index_desc* bitmap_index,
		BitmapDMLOperation operation);
	static double getAverageQueryTime(const index_desc* bitmap_index);
	static ULONG getOperationCount(const index_desc* bitmap_index,
		BitmapDMLOperation operation);
	
	// Performance analysis
	static void analyzeIndexPerformance(const index_desc* bitmap_index,
		ScratchBird::string& analysis);
	static bool isPerformanceDegraded(const index_desc* bitmap_index);
	static void getPerformanceRecommendations(const index_desc* bitmap_index,
		ScratchBird::string& recommendations);

private:
	// Performance data storage
	struct PerformanceMetrics
	{
		ULONG operation_count;
		ULONG total_duration_ms;
		ULONG min_duration_ms;
		ULONG max_duration_ms;
		double average_duration_ms;
		
		PerformanceMetrics()
			: operation_count(0), total_duration_ms(0),
			  min_duration_ms(ULONG_MAX), max_duration_ms(0),
			  average_duration_ms(0.0)
		{
		}
	};
	
	// Performance tracking data structures
	static ScratchBird::GenericMap<USHORT, PerformanceMetrics> s_maintenance_metrics;
	static ScratchBird::GenericMap<USHORT, PerformanceMetrics> s_query_metrics;
	
	// Performance calculation helpers
	static void updateMetrics(PerformanceMetrics& metrics, ULONG duration_ms);
	static double calculateTrendAnalysis(const PerformanceMetrics& metrics);
};

//----------------------------
// Bitmap Index Maintenance Configuration
//----------------------------
struct BitmapMaintenanceConfig
{
	// Maintenance behavior
	bool auto_maintenance_enabled;		// Enable automatic maintenance
	bool deferred_maintenance_enabled;	// Enable deferred maintenance
	ULONG maintenance_threshold;		// Operations before maintenance
	ULONG maintenance_interval_seconds; // Time-based maintenance interval
	
	// Performance settings
	bool batch_processing_enabled;		// Enable batch DML processing
	ULONG batch_size_limit;				// Maximum batch size
	ULONG parallel_workers;				// Number of parallel workers
	
	// Error handling
	bool stop_on_error;					// Stop processing on errors
	ULONG max_retry_attempts;			// Maximum retry attempts
	ULONG retry_delay_ms;				// Delay between retries
	
	// Logging and monitoring
	bool detailed_logging_enabled;		// Enable detailed logging
	bool performance_monitoring_enabled; // Enable performance tracking
	ScratchBird::string log_filename;	// Log file path
	
	BitmapMaintenanceConfig()
		: auto_maintenance_enabled(true), deferred_maintenance_enabled(false),
		  maintenance_threshold(1000), maintenance_interval_seconds(3600),
		  batch_processing_enabled(true), batch_size_limit(10000),
		  parallel_workers(2), stop_on_error(false), max_retry_attempts(3),
		  retry_delay_ms(100), detailed_logging_enabled(false),
		  performance_monitoring_enabled(true)
	{
	}
};

} // namespace Jrd

#endif // JRD_BITMAP_INDEX_MAINTENANCE_H