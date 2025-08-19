/*
 *	PROGRAM:	ScratchBird Enhanced Backup/Restore Utility
 *	MODULE:		GinIndexBackupSupport.h
 *	DESCRIPTION:	GIN index backup and restore support for sb_gbak
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
 * 2025.07.23 - ScratchBird GIN Index Backup/Restore Support
 */

#ifndef UTILITIES_GIN_INDEX_BACKUP_SUPPORT_H
#define UTILITIES_GIN_INDEX_BACKUP_SUPPORT_H

#include "../jrd/GinIndex.h"
#include "../jrd/GinCompression.h"
#include "../jrd/GinTokenizer.h"
#include "../common/classes/fb_string.h"
#include "../common/classes/array.h"
#include <vector>
#include <memory>
#include <map>

namespace SBBackup {

// Forward declarations
class BackupWriter;
class RestoreReader;

//----------------------------
// GIN Index Backup Metadata
//----------------------------
struct GinIndexBackupMetadata
{
	// Index identification
	ScratchBird::string index_name;
	ScratchBird::string relation_name;
	USHORT index_id;
	USHORT relation_id;
	
	// GIN-specific configuration
	USHORT tokenizer_type;				// Tokenizer algorithm used
	USHORT compression_type;			// Compression algorithm
	ULONG posting_list_threshold;		// Posting list compression threshold
	USHORT language_id;					// Language for tokenization
	ScratchBird::string custom_options;	// Custom tokenizer options
	
	// Index statistics
	ULONG total_terms;					// Total number of unique terms
	ULONG total_postings;				// Total posting list entries
	ULONG compressed_size;				// Compressed index size
	ULONG uncompressed_size;			// Uncompressed index size
	double compression_ratio;			// Achieved compression ratio
	
	// Backup metadata
	ULONG backup_version;				// Backup format version
	GDS_TIMESTAMP backup_timestamp;		// When backup was created
	ULONG checksum;						// Data integrity checksum
	
	GinIndexBackupMetadata()
		: index_id(0), relation_id(0), tokenizer_type(0), compression_type(0),
		  posting_list_threshold(0), language_id(0), total_terms(0),
		  total_postings(0), compressed_size(0), uncompressed_size(0),
		  compression_ratio(0.0), backup_version(1), backup_timestamp(0),
		  checksum(0)
	{
	}
};

//----------------------------
// GIN Term Entry for Backup
//----------------------------
struct GinTermBackupEntry
{
	ScratchBird::string term_text;		// The actual term text
	ULONG term_frequency;				// Number of documents containing term
	ULONG posting_list_size;			// Size of compressed posting list  
	ScratchBird::ObjectsArray<UCHAR> compressed_postings; // Compressed posting list data
	ULONG uncompressed_size;			// Original posting list size
	USHORT compression_algorithm;		// Algorithm used for this posting list
	
	GinTermBackupEntry()
		: term_frequency(0), posting_list_size(0), uncompressed_size(0),
		  compression_algorithm(0), compressed_postings(*ScratchBird::getDefaultMemoryPool())
	{
	}
};

//----------------------------
// GIN Index Backup Manager
//----------------------------
class GinIndexBackupManager
{
public:
	GinIndexBackupManager();
	~GinIndexBackupManager();
	
	// Main backup/restore operations
	bool backupGinIndex(const Jrd::index_desc* gin_index, BackupWriter* writer);
	bool restoreGinIndex(const GinIndexBackupMetadata& metadata, RestoreReader* reader);
	
	// Metadata operations
	bool writeGinIndexMetadata(const GinIndexBackupMetadata& metadata, BackupWriter* writer);
	bool readGinIndexMetadata(GinIndexBackupMetadata& metadata, RestoreReader* reader);
	
	// Term and posting list operations
	bool backupGinTerms(const Jrd::index_desc* gin_index, BackupWriter* writer);
	bool restoreGinTerms(const GinIndexBackupMetadata& metadata, RestoreReader* reader);
	
	// Validation and verification
	bool validateGinIndexBackup(const GinIndexBackupMetadata& metadata, RestoreReader* reader);
	bool verifyGinIndexIntegrity(const Jrd::index_desc* gin_index);
	
	// Progress reporting
	void setProgressCallback(std::function<void(const std::string&, double)> callback);
	
	// Statistics and reporting
	void getBackupStatistics(std::map<std::string, std::string>& stats);
	void getRestoreStatistics(std::map<std::string, std::string>& stats);

private:
	// Internal backup operations
	bool extractGinIndexMetadata(const Jrd::index_desc* gin_index, 
								 GinIndexBackupMetadata& metadata);
	bool extractGinTermEntries(const Jrd::index_desc* gin_index,
							   ScratchBird::ObjectsArray<GinTermBackupEntry>& entries);
	bool compressPostingList(const ScratchBird::ObjectsArray<Jrd::RecordNumber>& postings,
							 USHORT compression_algorithm, GinTermBackupEntry& entry);
	
	// Internal restore operations  
	bool createGinIndexStructure(const GinIndexBackupMetadata& metadata);
	bool restoreGinTermEntry(const GinTermBackupEntry& entry,
							 const GinIndexBackupMetadata& metadata);
	bool decompressPostingList(const GinTermBackupEntry& entry,
							   ScratchBird::ObjectsArray<Jrd::RecordNumber>& postings);
	
	// Data integrity and validation
	ULONG calculateChecksumForMetadata(const GinIndexBackupMetadata& metadata);
	ULONG calculateChecksumForTerms(const ScratchBird::ObjectsArray<GinTermBackupEntry>& entries);
	bool validateChecksums(const GinIndexBackupMetadata& metadata, ULONG terms_checksum);
	
	// Progress tracking
	std::function<void(const std::string&, double)> m_progress_callback;
	void reportProgress(const std::string& operation, double percentage);
	
	// Statistics tracking
	struct BackupStats {
		ULONG terms_backed_up;
		ULONG postings_backed_up;
		ULONG bytes_written;
		ULONG compression_time_ms;
		double average_compression_ratio;
		
		BackupStats() : terms_backed_up(0), postings_backed_up(0), bytes_written(0),
						compression_time_ms(0), average_compression_ratio(0.0) {}
	} m_backup_stats;
	
	struct RestoreStats {
		ULONG terms_restored;
		ULONG postings_restored;
		ULONG bytes_read;
		ULONG decompression_time_ms;
		ULONG validation_errors;
		
		RestoreStats() : terms_restored(0), postings_restored(0), bytes_read(0),
						 decompression_time_ms(0), validation_errors(0) {}
	} m_restore_stats;
};

//----------------------------
// GIN Index Backup Writer
//----------------------------
class GinBackupWriter
{
public:
	GinBackupWriter(BackupWriter* writer);
	~GinBackupWriter();
	
	// Writing operations
	bool writeHeader(const GinIndexBackupMetadata& metadata);
	bool writeTermCount(ULONG term_count);
	bool writeTerm(const GinTermBackupEntry& term);
	bool writeFooter(ULONG checksum);
	
	// Data integrity
	bool writeChecksum(ULONG checksum);
	ULONG calculateRunningChecksum() const;

private:
	BackupWriter* m_writer;
	ULONG m_running_checksum;
	ULONG m_terms_written;
	
	// Internal writing helpers
	bool writeString(const ScratchBird::string& str);
	bool writeULong(ULONG value);
	bool writeUShort(USHORT value);
	bool writeByteArray(const ScratchBird::ObjectsArray<UCHAR>& data);
	
	void updateChecksum(const void* data, ULONG size);
};

//----------------------------
// GIN Index Backup Reader
//----------------------------
class GinBackupReader
{
public:
	GinBackupReader(RestoreReader* reader);
	~GinBackupReader();
	
	// Reading operations
	bool readHeader(GinIndexBackupMetadata& metadata);
	bool readTermCount(ULONG& term_count);
	bool readTerm(GinTermBackupEntry& term);
	bool readFooter(ULONG& checksum);
	
	// Data integrity
	bool readChecksum(ULONG& checksum);
	ULONG calculateRunningChecksum() const;
	bool validateChecksum(ULONG expected_checksum);

private:
	RestoreReader* m_reader;
	ULONG m_running_checksum;
	ULONG m_terms_read;
	
	// Internal reading helpers
	bool readString(ScratchBird::string& str);
	bool readULong(ULONG& value);
	bool readUShort(USHORT& value);
	bool readByteArray(ScratchBird::ObjectsArray<UCHAR>& data);
	
	void updateChecksum(const void* data, ULONG size);
};

//----------------------------
// GIN Index Backup Integration
//----------------------------
class GinIndexBackupIntegration
{
public:
	// Integration with sb_gbak
	static bool registerGinIndexBackupHandlers();
	static bool unregisterGinIndexBackupHandlers();
	
	// Callback handlers for sb_gbak
	static bool ginIndexBackupHandler(const Jrd::index_desc* index_desc, 
									  BackupWriter* writer, void* context);
	static bool ginIndexRestoreHandler(RestoreReader* reader, void* context);
	
	// GIN index detection and enumeration
	static bool isGinIndex(const Jrd::index_desc* index_desc);
	static bool enumerateGinIndexes(Jrd::thread_db* tdbb, 
									ScratchBird::ObjectsArray<Jrd::index_desc*>& gin_indexes);
	
	// Compatibility and version checking
	static bool checkGinIndexBackupCompatibility(ULONG backup_version);
	static ULONG getCurrentGinBackupVersion();
	
	// Error handling and logging
	static void logGinBackupError(const std::string& error_message);
	static void logGinBackupProgress(const std::string& operation, double percentage);

private:
	static std::unique_ptr<GinIndexBackupManager> s_backup_manager;
	static bool s_handlers_registered;
	
	// Internal helper methods
	static void initializeBackupManager();
	static void cleanupBackupManager();
};

//----------------------------
// GIN Backup Configuration
//----------------------------
struct GinBackupConfig
{
	// Compression settings
	bool enable_compression;			// Enable posting list compression
	USHORT compression_algorithm;		// Default compression algorithm
	ULONG compression_threshold;		// Minimum size for compression
	
	// Performance settings
	bool parallel_processing;			// Enable parallel term processing
	ULONG batch_size;					// Terms to process in batch
	ULONG max_memory_usage;				// Maximum memory for operations
	
	// Validation settings
	bool verify_integrity;				// Verify data integrity
	bool validate_checksums;			// Validate all checksums
	bool detailed_logging;				// Enable detailed operation logging
	
	// Compatibility settings
	ULONG backup_format_version;		// Backup format version to use
	bool backward_compatible;			// Maintain backward compatibility
	
	GinBackupConfig()
		: enable_compression(true), compression_algorithm(1), compression_threshold(1024),
		  parallel_processing(true), batch_size(1000), max_memory_usage(128 * 1024 * 1024),
		  verify_integrity(true), validate_checksums(true), detailed_logging(false),
		  backup_format_version(1), backward_compatible(true)
	{
	}
};

//----------------------------
// Utility Functions
//----------------------------
namespace GinBackupUtils {
	// String encoding/decoding for backup format
	std::string encodeStringForBackup(const ScratchBird::string& str);
	ScratchBird::string decodeStringFromBackup(const std::string& encoded_str);
	
	// Compression utilities
	bool compressData(const void* input, ULONG input_size, 
					  ScratchBird::ObjectsArray<UCHAR>& output, USHORT algorithm);
	bool decompressData(const ScratchBird::ObjectsArray<UCHAR>& input,
						void* output, ULONG output_size, USHORT algorithm);
	
	// Checksum utilities
	ULONG calculateCRC32(const void* data, ULONG size);
	ULONG updateCRC32(ULONG current_crc, const void* data, ULONG size);
	
	// Progress reporting utilities
	double calculateProgressPercentage(ULONG current, ULONG total);
	std::string formatProgressMessage(const std::string& operation, double percentage);
	
	// Memory management utilities
	bool allocateBackupBuffer(ULONG size, ScratchBird::ObjectsArray<UCHAR>& buffer);
	void deallocateBackupBuffer(ScratchBird::ObjectsArray<UCHAR>& buffer);
	
	// Error handling utilities
	std::string formatBackupError(const std::string& operation, const std::string& details);
	void logBackupOperation(const std::string& operation, bool success, 
						   const std::string& details = "");
}

} // namespace SBBackup

#endif // UTILITIES_GIN_INDEX_BACKUP_SUPPORT_H