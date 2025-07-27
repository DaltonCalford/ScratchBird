/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		GinCompression.h  
 *	DESCRIPTION:	Posting list compression algorithms for GIN indexes
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
 * 2025.07.22 - ScratchBird GIN Posting List Compression Implementation
 */

#ifndef JRD_GIN_COMPRESSION_H
#define JRD_GIN_COMPRESSION_H

#include "../jrd/jrd.h"
#include "../jrd/constants.h"
#include "../common/classes/array.h"
#include "../include/firebird/impl/dsc_pub.h"

namespace Jrd {

// Forward declarations
typedef ScratchBird::HalfStaticArray<RecordNumber, 32> PostingList;

//----------------------------
// Compression Algorithm Types
//----------------------------
enum CompressionType {
	COMPRESSION_NONE = 0,			// No compression (raw data)
	COMPRESSION_DELTA = 1,			// Delta encoding (gaps between record numbers)
	COMPRESSION_VBYTE = 2,			// Variable-byte encoding
	COMPRESSION_PFD = 3,			// PForDelta (Patched Frame of Reference)
	COMPRESSION_SIMPLE9 = 4,		// Simple-9 integer compression
	COMPRESSION_RLE = 5,			// Run-length encoding for dense sequences
	COMPRESSION_HYBRID = 6			// Hybrid approach (auto-select best)
};

//----------------------------
// Compression Statistics
//----------------------------
struct CompressionStats {
	ULONG original_size;			// Original size in bytes
	ULONG compressed_size;			// Compressed size in bytes
	double compression_ratio;		// Ratio (compressed/original)
	ULONG compression_time_us;		// Compression time in microseconds
	ULONG decompression_time_us;	// Decompression time in microseconds
	CompressionType algorithm;		// Algorithm used
	ULONG record_count;				// Number of records compressed
	ULONG min_gap;					// Minimum gap between records
	ULONG max_gap;					// Maximum gap between records
	double avg_gap;					// Average gap between records
	bool is_dense_sequence;			// True if sequence is dense (small gaps)
	bool is_sparse_sequence;		// True if sequence is sparse (large gaps)
};

//----------------------------
// Compression Context
//----------------------------
struct CompressionContext {
	UCHAR* buffer;					// Output buffer
	ULONG buffer_size;				// Buffer size
	ULONG bytes_written;			// Bytes written so far
	CompressionType algorithm;		// Algorithm being used
	ULONG bit_position;				// Current bit position (for bit-packing)
	ULONG current_word;				// Current word being built
	bool overflow;					// True if buffer overflow occurred
	
	CompressionContext(UCHAR* buf, ULONG size, CompressionType alg);
	void reset();
	bool hasSpace(ULONG bytes_needed) const;
	void writeByte(UCHAR byte);
	void writeBytes(const UCHAR* data, ULONG count);
	void writeVarInt(ULONG value);
	void flush();
};

//----------------------------
// Decompression Context
//----------------------------
struct DecompressionContext {
	const UCHAR* data;				// Input data
	ULONG data_size;				// Data size
	ULONG bytes_read;				// Bytes read so far
	CompressionType algorithm;		// Algorithm being used
	ULONG bit_position;				// Current bit position
	ULONG current_word;				// Current word being read
	bool underflow;					// True if data underflow occurred
	
	DecompressionContext(const UCHAR* input_data, ULONG size, CompressionType alg);
	void reset();
	bool hasData(ULONG bytes_needed) const;
	UCHAR readByte();
	void readBytes(UCHAR* buffer, ULONG count);
	ULONG readVarInt();
};

//----------------------------
// GIN Compression Engine
//----------------------------
class GinCompression
{
public:
	//----------------------------
	// Main Compression Interface
	//----------------------------
	
	// Compress posting list with specified algorithm
	static ULONG compress(const PostingList& posting_list, 
						  CompressionType algorithm,
						  UCHAR* output_buffer, 
						  ULONG buffer_size,
						  CompressionStats* stats = nullptr);
	
	// Decompress posting list
	static PostingList decompress(const UCHAR* compressed_data,
								  ULONG compressed_size,
								  CompressionType algorithm,
								  CompressionStats* stats = nullptr);
	
	// Auto-select best compression algorithm
	static CompressionType chooseBestAlgorithm(const PostingList& posting_list);
	
	// Estimate compressed size for given algorithm
	static ULONG estimateCompressedSize(const PostingList& posting_list, 
										 CompressionType algorithm);
	
	// Validate compressed data integrity
	static bool validateCompressedData(const UCHAR* compressed_data,
									   ULONG compressed_size,
									   CompressionType algorithm);
	
	//----------------------------
	// Algorithm-Specific Methods
	//----------------------------
	
	// Delta compression (encode gaps between record numbers)
	static ULONG deltaCompress(const PostingList& posting_list,
							   UCHAR* output_buffer,
							   ULONG buffer_size,
							   CompressionStats* stats = nullptr);
	
	static PostingList deltaDecompress(const UCHAR* compressed_data,
									   ULONG compressed_size,
									   CompressionStats* stats = nullptr);
	
	// Variable-byte encoding
	static ULONG vbyteCompress(const PostingList& posting_list,
							   UCHAR* output_buffer,
							   ULONG buffer_size,
							   CompressionStats* stats = nullptr);
	
	static PostingList vbyteDecompress(const UCHAR* compressed_data,
									   ULONG compressed_size,
									   CompressionStats* stats = nullptr);
	
	// PForDelta compression
	static ULONG pfdCompress(const PostingList& posting_list,
							 UCHAR* output_buffer,
							 ULONG buffer_size,
							 CompressionStats* stats = nullptr);
	
	static PostingList pfdDecompress(const UCHAR* compressed_data,
									 ULONG compressed_size,
									 CompressionStats* stats = nullptr);
	
	// Simple-9 compression
	static ULONG simple9Compress(const PostingList& posting_list,
								 UCHAR* output_buffer,
								 ULONG buffer_size,
								 CompressionStats* stats = nullptr);
	
	static PostingList simple9Decompress(const UCHAR* compressed_data,
										 ULONG compressed_size,
										 CompressionStats* stats = nullptr);
	
	// Run-length encoding
	static ULONG rleCompress(const PostingList& posting_list,
							 UCHAR* output_buffer,
							 ULONG buffer_size,
							 CompressionStats* stats = nullptr);
	
	static PostingList rleDecompress(const UCHAR* compressed_data,
									 ULONG compressed_size,
									 CompressionStats* stats = nullptr);
	
	//----------------------------
	// Utility Methods
	//----------------------------
	
	// Analyze posting list characteristics
	static CompressionStats analyzePostingList(const PostingList& posting_list);
	
	// Calculate compression metrics
	static void calculateCompressionMetrics(ULONG original_size, 
											ULONG compressed_size,
											ULONG compression_time_us,
											ULONG decompression_time_us,
											CompressionStats& stats);
	
	// Get algorithm name
	static const char* getAlgorithmName(CompressionType algorithm);
	
	// Get recommended algorithm for given characteristics
	static CompressionType getRecommendedAlgorithm(const CompressionStats& characteristics);
	
	//----------------------------
	// Configuration Constants
	//----------------------------
	
	static const ULONG MAX_POSTING_LIST_SIZE = 1000000;		// Maximum records per list
	static const ULONG COMPRESSION_BUFFER_SIZE = 65536;		// Default compression buffer
	static const ULONG PFD_BLOCK_SIZE = 128;				// PForDelta block size
	static const ULONG SIMPLE9_BATCH_SIZE = 28;			// Simple-9 batch size
	static const double DENSITY_THRESHOLD = 0.1;			// Threshold for dense sequences
	static const ULONG MIN_COMPRESSION_SIZE = 16;			// Minimum size to attempt compression
	
private:
	//----------------------------
	// Internal Helper Methods
	//----------------------------
	
	// Variable-byte encoding helpers
	static void encodeVarInt(ULONG value, CompressionContext& ctx);
	static ULONG decodeVarInt(DecompressionContext& ctx);
	static ULONG getVarIntSize(ULONG value);
	
	// Bit manipulation helpers
	static void writeBits(ULONG value, UCHAR bit_count, CompressionContext& ctx);
	static ULONG readBits(UCHAR bit_count, DecompressionContext& ctx);
	static void flushBits(CompressionContext& ctx);
	
	// PForDelta helpers
	static UCHAR findBestBitWidth(const ULONG* values, ULONG count, ULONG& exception_count);
	static void encodePFDBlock(const ULONG* values, ULONG count, UCHAR bit_width, 
							   CompressionContext& ctx);
	static void decodePFDBlock(ULONG* values, ULONG count, UCHAR bit_width, 
							   DecompressionContext& ctx);
	
	// Simple-9 helpers
	static UCHAR findBestSimple9Selector(const ULONG* values, ULONG count, ULONG& encoded_count);
	static void encodeSimple9Batch(const ULONG* values, ULONG count, UCHAR selector,
								   CompressionContext& ctx);
	static ULONG decodeSimple9Batch(ULONG* values, UCHAR selector, 
									DecompressionContext& ctx);
	
	// Analysis helpers
	static void analyzeGaps(const PostingList& posting_list, CompressionStats& stats);
	static void analyzeDistribution(const PostingList& posting_list, CompressionStats& stats);
	static bool isDenseSequence(const PostingList& posting_list);
	static bool isSparseSequence(const PostingList& posting_list);
	
	// Validation helpers
	static bool validateDeltaCompression(const UCHAR* data, ULONG size);
	static bool validateVByteCompression(const UCHAR* data, ULONG size);
	static bool validatePFDCompression(const UCHAR* data, ULONG size);
	static bool validateSimple9Compression(const UCHAR* data, ULONG size);
	static bool validateRLECompression(const UCHAR* data, ULONG size);
	
	// Performance optimization
	static bool shouldUseCompression(const PostingList& posting_list, 
									CompressionType algorithm);
	static CompressionType selectHybridAlgorithm(const CompressionStats& stats);
};

//----------------------------
// Compression Algorithm Characteristics
//----------------------------
struct AlgorithmCharacteristics {
	CompressionType algorithm;
	double typical_compression_ratio;	// Typical compression achieved
	ULONG compression_speed;			// Relative compression speed (1-10)
	ULONG decompression_speed;			// Relative decompression speed (1-10)
	bool handles_dense_sequences;		// Good for dense record sequences
	bool handles_sparse_sequences;		// Good for sparse record sequences
	bool handles_large_gaps;			// Good for large gaps between records
	ULONG memory_overhead;				// Memory overhead during processing
	const char* description;			// Human-readable description
};

//----------------------------
// Compression Algorithm Registry
//----------------------------
class CompressionAlgorithmRegistry
{
public:
	// Get algorithm characteristics
	static const AlgorithmCharacteristics& getCharacteristics(CompressionType algorithm);
	
	// Get all available algorithms
	static std::vector<CompressionType> getAvailableAlgorithms();
	
	// Find best algorithm for given requirements
	static CompressionType findBestAlgorithm(const CompressionStats& posting_stats,
											  bool prefer_speed = false,
											  bool prefer_compression = true);
	
	// Get algorithm performance profile
	static void getPerformanceProfile(CompressionType algorithm,
									  ULONG record_count,
									  double& expected_ratio,
									  ULONG& expected_compress_time_us,
									  ULONG& expected_decompress_time_us);
	
private:
	static const AlgorithmCharacteristics algorithm_info[];
	static void initializeRegistry();
	static bool registry_initialized;
};

//----------------------------
// Compression Benchmark Framework
//----------------------------
class CompressionBenchmark
{
public:
	struct BenchmarkResult {
		CompressionType algorithm;
		ULONG record_count;
		ULONG original_size_bytes;
		ULONG compressed_size_bytes;
		double compression_ratio;
		ULONG compression_time_us;
		ULONG decompression_time_us;
		ULONG compression_throughput_mbps;
		ULONG decompression_throughput_mbps;
		bool compression_successful;
		bool decompression_successful;
		bool data_integrity_verified;
		const char* algorithm_name;
	};
	
	// Benchmark single algorithm
	static BenchmarkResult benchmarkAlgorithm(const PostingList& posting_list,
											   CompressionType algorithm,
											   ULONG iterations = 1);
	
	// Benchmark all algorithms
	static std::vector<BenchmarkResult> benchmarkAllAlgorithms(const PostingList& posting_list,
															   ULONG iterations = 1);
	
	// Generate synthetic posting lists for testing
	static PostingList generateDenseSequence(ULONG count, RecordNumber start_record = 1);
	static PostingList generateSparseSequence(ULONG count, ULONG avg_gap = 100);
	static PostingList generateRandomSequence(ULONG count, ULONG max_record = 1000000);
	static PostingList generateSkewedSequence(ULONG count, double skew_factor = 2.0);
	
	// Print benchmark results
	static void printBenchmarkResults(const std::vector<BenchmarkResult>& results);
	static void printCompressionStats(const CompressionStats& stats);
	
private:
	static void verifyDataIntegrity(const PostingList& original,
									const PostingList& decompressed,
									BenchmarkResult& result);
	static ULONG calculateThroughput(ULONG bytes, ULONG time_us);
};

} // namespace Jrd

#endif // JRD_GIN_COMPRESSION_H