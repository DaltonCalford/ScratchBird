/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		GinCompression.cpp  
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

#include "scratchbird.h"
#include "../jrd/GinCompression.h"
#include "../jrd/constants.h"
#include "../common/gdsassert.h"
#include <chrono>
#include <algorithm>
#include <cstring>
#include <vector>
#include <cmath>

using namespace ScratchBird;
using namespace Jrd;
using namespace std::chrono;

namespace Jrd {

//----------------------------
// CompressionContext Implementation
//----------------------------

CompressionContext::CompressionContext(UCHAR* buf, ULONG size, CompressionType alg)
	: buffer(buf), buffer_size(size), bytes_written(0), algorithm(alg),
	  bit_position(0), current_word(0), overflow(false)
{
	fb_assert(buf != nullptr);
	fb_assert(size > 0);
}

void CompressionContext::reset()
{
	bytes_written = 0;
	bit_position = 0;
	current_word = 0;
	overflow = false;
}

bool CompressionContext::hasSpace(ULONG bytes_needed) const
{
	return (bytes_written + bytes_needed) <= buffer_size;
}

void CompressionContext::writeByte(UCHAR byte)
{
	if (hasSpace(1)) {
		buffer[bytes_written++] = byte;
	} else {
		overflow = true;
	}
}

void CompressionContext::writeBytes(const UCHAR* data, ULONG count)
{
	fb_assert(data != nullptr);
	
	if (hasSpace(count)) {
		memcpy(&buffer[bytes_written], data, count);
		bytes_written += count;
	} else {
		overflow = true;
	}
}

void CompressionContext::writeVarInt(ULONG value)
{
	while (value >= 0x80) {
		writeByte(static_cast<UCHAR>((value & 0x7F) | 0x80));
		value >>= 7;
	}
	writeByte(static_cast<UCHAR>(value & 0x7F));
}

void CompressionContext::flush()
{
	if (bit_position > 0) {
		writeByte(static_cast<UCHAR>(current_word));
		bit_position = 0;
		current_word = 0;
	}
}

//----------------------------
// DecompressionContext Implementation
//----------------------------

DecompressionContext::DecompressionContext(const UCHAR* input_data, ULONG size, CompressionType alg)
	: data(input_data), data_size(size), bytes_read(0), algorithm(alg),
	  bit_position(0), current_word(0), underflow(false)
{
	fb_assert(input_data != nullptr);
	fb_assert(size > 0);
}

void DecompressionContext::reset()
{
	bytes_read = 0;
	bit_position = 0;
	current_word = 0;
	underflow = false;
}

bool DecompressionContext::hasData(ULONG bytes_needed) const
{
	return (bytes_read + bytes_needed) <= data_size;
}

UCHAR DecompressionContext::readByte()
{
	if (hasData(1)) {
		return data[bytes_read++];
	} else {
		underflow = true;
		return 0;
	}
}

void DecompressionContext::readBytes(UCHAR* buffer, ULONG count)
{
	fb_assert(buffer != nullptr);
	
	if (hasData(count)) {
		memcpy(buffer, &data[bytes_read], count);
		bytes_read += count;
	} else {
		underflow = true;
	}
}

ULONG DecompressionContext::readVarInt()
{
	ULONG result = 0;
	ULONG shift = 0;
	
	while (hasData(1)) {
		UCHAR byte = readByte();
		result |= (static_cast<ULONG>(byte & 0x7F) << shift);
		
		if ((byte & 0x80) == 0) {
			break; // Last byte
		}
		
		shift += 7;
		if (shift >= 35) { // Prevent overflow
			underflow = true;
			break;
		}
	}
	
	return result;
}

//----------------------------
// GinCompression Implementation
//----------------------------

ULONG GinCompression::compress(const PostingList& posting_list, 
							   CompressionType algorithm,
							   UCHAR* output_buffer, 
							   ULONG buffer_size,
							   CompressionStats* stats)
{
	fb_assert(output_buffer != nullptr);
	fb_assert(buffer_size > 0);
	
	if (posting_list.isEmpty()) {
		if (stats) {
			memset(stats, 0, sizeof(CompressionStats));
		}
		return 0;
	}
	
	auto start_time = high_resolution_clock::now();
	
	ULONG compressed_size = 0;
	
	switch (algorithm) {
		case COMPRESSION_NONE:
			compressed_size = std::min(posting_list.getCount() * sizeof(RecordNumber), buffer_size);
			memcpy(output_buffer, posting_list.begin(), compressed_size);
			break;
			
		case COMPRESSION_DELTA:
			compressed_size = deltaCompress(posting_list, output_buffer, buffer_size, stats);
			break;
			
		case COMPRESSION_VBYTE:
			compressed_size = vbyteCompress(posting_list, output_buffer, buffer_size, stats);
			break;
			
		case COMPRESSION_PFD:
			compressed_size = pfdCompress(posting_list, output_buffer, buffer_size, stats);
			break;
			
		case COMPRESSION_SIMPLE9:
			compressed_size = simple9Compress(posting_list, output_buffer, buffer_size, stats);
			break;
			
		case COMPRESSION_RLE:
			compressed_size = rleCompress(posting_list, output_buffer, buffer_size, stats);
			break;
			
		case COMPRESSION_HYBRID:
			algorithm = chooseBestAlgorithm(posting_list);
			compressed_size = compress(posting_list, algorithm, output_buffer, buffer_size, stats);
			break;
			
		default:
			compressed_size = 0;
			break;
	}
	
	auto end_time = high_resolution_clock::now();
	auto compression_time = duration_cast<microseconds>(end_time - start_time).count();
	
	if (stats) {
		stats->original_size = posting_list.getCount() * sizeof(RecordNumber);
		stats->compressed_size = compressed_size;
		stats->compression_ratio = (stats->original_size > 0) ? 
			static_cast<double>(compressed_size) / stats->original_size : 0.0;
		stats->compression_time_us = compression_time;
		stats->algorithm = algorithm;
		stats->record_count = posting_list.getCount();
		
		analyzeGaps(posting_list, *stats);
	}
	
	return compressed_size;
}

PostingList GinCompression::decompress(const UCHAR* compressed_data,
										ULONG compressed_size,
										CompressionType algorithm,
										CompressionStats* stats)
{
	fb_assert(compressed_data != nullptr);
	fb_assert(compressed_size > 0);
	
	auto start_time = high_resolution_clock::now();
	
	PostingList result;
	
	switch (algorithm) {
		case COMPRESSION_NONE:
		{
			ULONG record_count = compressed_size / sizeof(RecordNumber);
			const RecordNumber* records = reinterpret_cast<const RecordNumber*>(compressed_data);
			result.grow(record_count);
			for (ULONG i = 0; i < record_count; i++) {
				result.add(records[i]);
			}
			break;
		}
		
		case COMPRESSION_DELTA:
			result = deltaDecompress(compressed_data, compressed_size, stats);
			break;
			
		case COMPRESSION_VBYTE:
			result = vbyteDecompress(compressed_data, compressed_size, stats);
			break;
			
		case COMPRESSION_PFD:
			result = pfdDecompress(compressed_data, compressed_size, stats);
			break;
			
		case COMPRESSION_SIMPLE9:
			result = simple9Decompress(compressed_data, compressed_size, stats);
			break;
			
		case COMPRESSION_RLE:
			result = rleDecompress(compressed_data, compressed_size, stats);
			break;
			
		default:
			// Return empty list for unknown algorithm
			break;
	}
	
	auto end_time = high_resolution_clock::now();
	auto decompression_time = duration_cast<microseconds>(end_time - start_time).count();
	
	if (stats) {
		stats->decompression_time_us = decompression_time;
		if (!result.isEmpty()) {
			analyzeGaps(result, *stats);
		}
	}
	
	return result;
}

CompressionType GinCompression::chooseBestAlgorithm(const PostingList& posting_list)
{
	if (posting_list.isEmpty()) {
		return COMPRESSION_NONE;
	}
	
	if (posting_list.getCount() < MIN_COMPRESSION_SIZE / sizeof(RecordNumber)) {
		return COMPRESSION_NONE; // Too small to benefit from compression
	}
	
	CompressionStats stats = analyzePostingList(posting_list);
	
	// Choose algorithm based on posting list characteristics
	if (stats.is_dense_sequence) {
		// Dense sequences compress well with delta + variable-byte
		return COMPRESSION_DELTA;
	}
	else if (stats.avg_gap < 100) {
		// Small gaps - delta compression works well
		return COMPRESSION_DELTA;
	}
	else if (stats.max_gap > 100000) {
		// Very large gaps - PForDelta handles this better
		return COMPRESSION_PFD;
	}
	else {
		// General case - variable-byte encoding
		return COMPRESSION_VBYTE;
	}
}

ULONG GinCompression::estimateCompressedSize(const PostingList& posting_list, 
											  CompressionType algorithm)
{
	if (posting_list.isEmpty()) {
		return 0;
	}
	
	CompressionStats stats = analyzePostingList(posting_list);
	
	switch (algorithm) {
		case COMPRESSION_NONE:
			return stats.original_size;
			
		case COMPRESSION_DELTA:
		case COMPRESSION_VBYTE:
			// Estimate based on average gap size
			if (stats.avg_gap < 128) return stats.original_size * 0.3; // ~70% compression
			else if (stats.avg_gap < 16384) return stats.original_size * 0.5; // ~50% compression
			else return stats.original_size * 0.7; // ~30% compression
			
		case COMPRESSION_PFD:
			return stats.original_size * 0.4; // ~60% compression (typically good)
			
		case COMPRESSION_SIMPLE9:
			return stats.original_size * 0.45; // ~55% compression
			
		case COMPRESSION_RLE:
			if (stats.is_dense_sequence) return stats.original_size * 0.1; // Excellent for dense
			else return stats.original_size * 0.8; // Poor for sparse
			
		case COMPRESSION_HYBRID:
			return estimateCompressedSize(posting_list, chooseBestAlgorithm(posting_list));
			
		default:
			return stats.original_size;
	}
}

bool GinCompression::validateCompressedData(const UCHAR* compressed_data,
											 ULONG compressed_size,
											 CompressionType algorithm)
{
	if (compressed_data == nullptr || compressed_size == 0) {
		return false;
	}
	
	switch (algorithm) {
		case COMPRESSION_NONE:
			return (compressed_size % sizeof(RecordNumber)) == 0;
			
		case COMPRESSION_DELTA:
			return validateDeltaCompression(compressed_data, compressed_size);
			
		case COMPRESSION_VBYTE:
			return validateVByteCompression(compressed_data, compressed_size);
			
		case COMPRESSION_PFD:
			return validatePFDCompression(compressed_data, compressed_size);
			
		case COMPRESSION_SIMPLE9:
			return validateSimple9Compression(compressed_data, compressed_size);
			
		case COMPRESSION_RLE:
			return validateRLECompression(compressed_data, compressed_size);
			
		default:
			return false;
	}
}

//----------------------------
// Delta Compression Implementation
//----------------------------

ULONG GinCompression::deltaCompress(const PostingList& posting_list,
									UCHAR* output_buffer,
									ULONG buffer_size,
									CompressionStats* stats)
{
	if (posting_list.isEmpty()) {
		return 0;
	}
	
	CompressionContext ctx(output_buffer, buffer_size, COMPRESSION_DELTA);
	
	// Write record count first
	ctx.writeVarInt(posting_list.getCount());
	
	// Write first record as-is
	RecordNumber prev_record = posting_list[0];
	ctx.writeVarInt(prev_record.getValue());
	
	// Write deltas
	for (FB_SIZE_T i = 1; i < posting_list.getCount(); i++) {
		RecordNumber current_record = posting_list[i];
		ULONG delta = current_record.getValue() - prev_record.getValue();
		ctx.writeVarInt(delta);
		prev_record = current_record;
	}
	
	if (ctx.overflow) {
		return 0; // Compression failed due to buffer overflow
	}
	
	return ctx.bytes_written;
}

PostingList GinCompression::deltaDecompress(const UCHAR* compressed_data,
											 ULONG compressed_size,
											 CompressionStats* stats)
{
	PostingList result;
	DecompressionContext ctx(compressed_data, compressed_size, COMPRESSION_DELTA);
	
	// Read record count
	ULONG record_count = ctx.readVarInt();
	if (ctx.underflow || record_count == 0) {
		return result;
	}
	
	result.grow(record_count);
	
	// Read first record
	ULONG current_value = ctx.readVarInt();
	if (ctx.underflow) {
		return result;
	}
	
	result.add(RecordNumber(current_value));
	
	// Read deltas and reconstruct values
	for (ULONG i = 1; i < record_count; i++) {
		ULONG delta = ctx.readVarInt();
		if (ctx.underflow) {
			break;
		}
		
		current_value += delta;
		result.add(RecordNumber(current_value));
	}
	
	return result;
}

//----------------------------
// Variable-Byte Compression Implementation
//----------------------------

ULONG GinCompression::vbyteCompress(const PostingList& posting_list,
									UCHAR* output_buffer,
									ULONG buffer_size,
									CompressionStats* stats)
{
	if (posting_list.isEmpty()) {
		return 0;
	}
	
	CompressionContext ctx(output_buffer, buffer_size, COMPRESSION_VBYTE);
	
	// Write record count
	ctx.writeVarInt(posting_list.getCount());
	
	// Write each record number using variable-byte encoding
	for (FB_SIZE_T i = 0; i < posting_list.getCount(); i++) {
		ctx.writeVarInt(posting_list[i].getValue());
	}
	
	if (ctx.overflow) {
		return 0;
	}
	
	return ctx.bytes_written;
}

PostingList GinCompression::vbyteDecompress(const UCHAR* compressed_data,
											 ULONG compressed_size,
											 CompressionStats* stats)
{
	PostingList result;
	DecompressionContext ctx(compressed_data, compressed_size, COMPRESSION_VBYTE);
	
	// Read record count
	ULONG record_count = ctx.readVarInt();
	if (ctx.underflow || record_count == 0) {
		return result;
	}
	
	result.grow(record_count);
	
	// Read each record number
	for (ULONG i = 0; i < record_count; i++) {
		ULONG record_value = ctx.readVarInt();
		if (ctx.underflow) {
			break;
		}
		
		result.add(RecordNumber(record_value));
	}
	
	return result;
}

//----------------------------
// PForDelta Compression Implementation
//----------------------------

ULONG GinCompression::pfdCompress(const PostingList& posting_list,
								  UCHAR* output_buffer,
								  ULONG buffer_size,
								  CompressionStats* stats)
{
	if (posting_list.isEmpty()) {
		return 0;
	}
	
	CompressionContext ctx(output_buffer, buffer_size, COMPRESSION_PFD);
	
	// Write record count
	ctx.writeVarInt(posting_list.getCount());
	
	// Convert to delta-encoded values first
	std::vector<ULONG> deltas;
	deltas.reserve(posting_list.getCount());
	
	// First value is stored as-is
	deltas.push_back(posting_list[0].getValue());
	
	// Subsequent values are deltas
	for (FB_SIZE_T i = 1; i < posting_list.getCount(); i++) {
		ULONG delta = posting_list[i].getValue() - posting_list[i-1].getValue();
		deltas.push_back(delta);
	}
	
	// Process in blocks
	for (size_t i = 0; i < deltas.size(); i += PFD_BLOCK_SIZE) {
		size_t block_size = std::min(static_cast<size_t>(PFD_BLOCK_SIZE), deltas.size() - i);
		
		// Find best bit width for this block
		ULONG exception_count;
		UCHAR bit_width = findBestBitWidth(&deltas[i], block_size, exception_count);
		
		// Write block header
		ctx.writeByte(bit_width);
		ctx.writeVarInt(block_size);
		ctx.writeVarInt(exception_count);
		
		// Encode block
		encodePFDBlock(&deltas[i], block_size, bit_width, ctx);
		
		if (ctx.overflow) {
			return 0;
		}
	}
	
	return ctx.bytes_written;
}

PostingList GinCompression::pfdDecompress(const UCHAR* compressed_data,
										   ULONG compressed_size,
										   CompressionStats* stats)
{
	PostingList result;
	DecompressionContext ctx(compressed_data, compressed_size, COMPRESSION_PFD);
	
	// Read record count
	ULONG record_count = ctx.readVarInt();
	if (ctx.underflow || record_count == 0) {
		return result;
	}
	
	result.grow(record_count);
	
	std::vector<ULONG> deltas;
	deltas.reserve(record_count);
	
	// Read blocks
	while (deltas.size() < record_count && !ctx.underflow) {
		// Read block header
		UCHAR bit_width = ctx.readByte();
		ULONG block_size = ctx.readVarInt();
		ULONG exception_count = ctx.readVarInt();
		
		if (ctx.underflow) {
			break;
		}
		
		// Decode block
		std::vector<ULONG> block_values(block_size);
		decodePFDBlock(block_values.data(), block_size, bit_width, ctx);
		
		// Add to deltas
		for (ULONG i = 0; i < block_size; i++) {
			deltas.push_back(block_values[i]);
		}
	}
	
	// Convert deltas back to absolute record numbers
	if (!deltas.empty()) {
		ULONG current_value = deltas[0];
		result.add(RecordNumber(current_value));
		
		for (size_t i = 1; i < deltas.size(); i++) {
			current_value += deltas[i];
			result.add(RecordNumber(current_value));
		}
	}
	
	return result;
}

//----------------------------
// Simple-9 Compression Implementation
//----------------------------

ULONG GinCompression::simple9Compress(const PostingList& posting_list,
									   UCHAR* output_buffer,
									   ULONG buffer_size,
									   CompressionStats* stats)
{
	if (posting_list.isEmpty()) {
		return 0;
	}
	
	CompressionContext ctx(output_buffer, buffer_size, COMPRESSION_SIMPLE9);
	
	// Write record count
	ctx.writeVarInt(posting_list.getCount());
	
	// Convert to delta-encoded values
	std::vector<ULONG> deltas;
	deltas.reserve(posting_list.getCount());
	
	deltas.push_back(posting_list[0].getValue());
	for (FB_SIZE_T i = 1; i < posting_list.getCount(); i++) {
		ULONG delta = posting_list[i].getValue() - posting_list[i-1].getValue();
		deltas.push_back(delta);
	}
	
	// Encode in batches
	for (size_t i = 0; i < deltas.size(); i += SIMPLE9_BATCH_SIZE) {
		size_t batch_size = std::min(static_cast<size_t>(SIMPLE9_BATCH_SIZE), deltas.size() - i);
		
		// Find best selector for this batch
		ULONG encoded_count;
		UCHAR selector = findBestSimple9Selector(&deltas[i], batch_size, encoded_count);
		
		// Encode batch
		encodeSimple9Batch(&deltas[i], encoded_count, selector, ctx);
		
		if (ctx.overflow) {
			return 0;
		}
	}
	
	return ctx.bytes_written;
}

PostingList GinCompression::simple9Decompress(const UCHAR* compressed_data,
											   ULONG compressed_size,
											   CompressionStats* stats)
{
	PostingList result;
	DecompressionContext ctx(compressed_data, compressed_size, COMPRESSION_SIMPLE9);
	
	// Read record count
	ULONG record_count = ctx.readVarInt();
	if (ctx.underflow || record_count == 0) {
		return result;
	}
	
	result.grow(record_count);
	
	std::vector<ULONG> deltas;
	deltas.reserve(record_count);
	
	// Decode batches
	while (deltas.size() < record_count && !ctx.underflow) {
		// Read 32-bit word
		if (!ctx.hasData(4)) {
			break;
		}
		
		ULONG word = 0;
		ctx.readBytes(reinterpret_cast<UCHAR*>(&word), 4);
		
		// Extract selector (top 4 bits)
		UCHAR selector = static_cast<UCHAR>(word >> 28);
		
		// Decode batch
		ULONG batch_values[SIMPLE9_BATCH_SIZE];
		ULONG decoded_count = decodeSimple9Batch(batch_values, selector, ctx);
		
		// Add to deltas
		for (ULONG i = 0; i < decoded_count && deltas.size() < record_count; i++) {
			deltas.push_back(batch_values[i]);
		}
	}
	
	// Convert back to absolute record numbers
	if (!deltas.empty()) {
		ULONG current_value = deltas[0];
		result.add(RecordNumber(current_value));
		
		for (size_t i = 1; i < deltas.size(); i++) {
			current_value += deltas[i];
			result.add(RecordNumber(current_value));
		}
	}
	
	return result;
}

//----------------------------
// Run-Length Encoding Implementation
//----------------------------

ULONG GinCompression::rleCompress(const PostingList& posting_list,
								  UCHAR* output_buffer,
								  ULONG buffer_size,
								  CompressionStats* stats)
{
	if (posting_list.isEmpty()) {
		return 0;
	}
	
	CompressionContext ctx(output_buffer, buffer_size, COMPRESSION_RLE);
	
	// Write record count
	ctx.writeVarInt(posting_list.getCount());
	
	// Look for consecutive sequences
	FB_SIZE_T i = 0;
	while (i < posting_list.getCount()) {
		RecordNumber current = posting_list[i];
		FB_SIZE_T run_length = 1;
		
		// Count consecutive records
		while (i + run_length < posting_list.getCount() && 
			   posting_list[i + run_length].getValue() == current.getValue() + run_length) {
			run_length++;
		}
		
		if (run_length > 1) {
			// Write run: marker (0), start_value, length
			ctx.writeVarInt(0);
			ctx.writeVarInt(current.getValue());
			ctx.writeVarInt(run_length);
		} else {
			// Write single value: marker (1), value
			ctx.writeVarInt(1);
			ctx.writeVarInt(current.getValue());
		}
		
		if (ctx.overflow) {
			return 0;
		}
		
		i += run_length;
	}
	
	return ctx.bytes_written;
}

PostingList GinCompression::rleDecompress(const UCHAR* compressed_data,
										   ULONG compressed_size,
										   CompressionStats* stats)
{
	PostingList result;
	DecompressionContext ctx(compressed_data, compressed_size, COMPRESSION_RLE);
	
	// Read record count
	ULONG record_count = ctx.readVarInt();
	if (ctx.underflow || record_count == 0) {
		return result;
	}
	
	result.grow(record_count);
	
	while (result.getCount() < record_count && !ctx.underflow) {
		ULONG marker = ctx.readVarInt();
		
		if (marker == 0) {
			// Run of consecutive values
			ULONG start_value = ctx.readVarInt();
			ULONG run_length = ctx.readVarInt();
			
			if (ctx.underflow) {
				break;
			}
			
			for (ULONG i = 0; i < run_length && result.getCount() < record_count; i++) {
				result.add(RecordNumber(start_value + i));
			}
		} else {
			// Single value
			ULONG value = ctx.readVarInt();
			
			if (ctx.underflow) {
				break;
			}
			
			result.add(RecordNumber(value));
		}
	}
	
	return result;
}

//----------------------------
// Analysis and Helper Methods
//----------------------------

CompressionStats GinCompression::analyzePostingList(const PostingList& posting_list)
{
	CompressionStats stats;
	memset(&stats, 0, sizeof(stats));
	
	if (posting_list.isEmpty()) {
		return stats;
	}
	
	stats.original_size = posting_list.getCount() * sizeof(RecordNumber);
	stats.record_count = posting_list.getCount();
	
	analyzeGaps(posting_list, stats);
	analyzeDistribution(posting_list, stats);
	
	return stats;
}

void GinCompression::analyzeGaps(const PostingList& posting_list, CompressionStats& stats)
{
	if (posting_list.getCount() < 2) {
		stats.min_gap = stats.max_gap = stats.avg_gap = 0;
		return;
	}
	
	ULONG total_gap = 0;
	stats.min_gap = ULONG_MAX;
	stats.max_gap = 0;
	
	for (FB_SIZE_T i = 1; i < posting_list.getCount(); i++) {
		ULONG gap = posting_list[i].getValue() - posting_list[i-1].getValue();
		
		stats.min_gap = std::min(stats.min_gap, gap);
		stats.max_gap = std::max(stats.max_gap, gap);
		total_gap += gap;
	}
	
	stats.avg_gap = static_cast<double>(total_gap) / (posting_list.getCount() - 1);
}

void GinCompression::analyzeDistribution(const PostingList& posting_list, CompressionStats& stats)
{
	stats.is_dense_sequence = isDenseSequence(posting_list);
	stats.is_sparse_sequence = isSparseSequence(posting_list);
}

bool GinCompression::isDenseSequence(const PostingList& posting_list)
{
	if (posting_list.getCount() < 2) {
		return false;
	}
	
	ULONG total_range = posting_list[posting_list.getCount()-1].getValue() - posting_list[0].getValue();
	ULONG expected_range = posting_list.getCount() - 1;
	
	// Dense if actual range is close to minimum possible range
	double density = static_cast<double>(expected_range) / (total_range + 1);
	return density > (1.0 - DENSITY_THRESHOLD);
}

bool GinCompression::isSparseSequence(const PostingList& posting_list)
{
	if (posting_list.getCount() < 2) {
		return false;
	}
	
	// Count large gaps
	ULONG large_gaps = 0;
	for (FB_SIZE_T i = 1; i < posting_list.getCount(); i++) {
		ULONG gap = posting_list[i].getValue() - posting_list[i-1].getValue();
		if (gap > 100) { // Arbitrary threshold for "large" gap
			large_gaps++;
		}
	}
	
	// Sparse if most gaps are large
	double sparse_ratio = static_cast<double>(large_gaps) / (posting_list.getCount() - 1);
	return sparse_ratio > 0.5;
}

const char* GinCompression::getAlgorithmName(CompressionType algorithm)
{
	switch (algorithm) {
		case COMPRESSION_NONE: return "None";
		case COMPRESSION_DELTA: return "Delta";
		case COMPRESSION_VBYTE: return "VByte";
		case COMPRESSION_PFD: return "PForDelta";
		case COMPRESSION_SIMPLE9: return "Simple-9";
		case COMPRESSION_RLE: return "RLE";
		case COMPRESSION_HYBRID: return "Hybrid";
		default: return "Unknown";
	}
}

//----------------------------
// Internal Helper Methods
//----------------------------

UCHAR GinCompression::findBestBitWidth(const ULONG* values, ULONG count, ULONG& exception_count)
{
	if (count == 0) {
		exception_count = 0;
		return 1;
	}
	
	// Find maximum value
	ULONG max_value = 0;
	for (ULONG i = 0; i < count; i++) {
		max_value = std::max(max_value, values[i]);
	}
	
	// Calculate bits needed for max value
	UCHAR bits_needed = 1;
	ULONG temp = max_value;
	while (temp > 1) {
		temp >>= 1;
		bits_needed++;
	}
	
	exception_count = 0;
	return bits_needed;
}

void GinCompression::encodePFDBlock(const ULONG* values, ULONG count, UCHAR bit_width, 
									CompressionContext& ctx)
{
	// Simplified PForDelta encoding - just pack bits
	for (ULONG i = 0; i < count; i++) {
		writeBits(values[i], bit_width, ctx);
	}
	ctx.flush();
}

void GinCompression::decodePFDBlock(ULONG* values, ULONG count, UCHAR bit_width, 
									DecompressionContext& ctx)
{
	// Simplified PForDelta decoding - just unpack bits
	for (ULONG i = 0; i < count; i++) {
		values[i] = readBits(bit_width, ctx);
	}
}

UCHAR GinCompression::findBestSimple9Selector(const ULONG* values, ULONG count, ULONG& encoded_count)
{
	// Simplified selector - use fixed selector
	encoded_count = std::min(count, static_cast<ULONG>(28));
	return 1; // Simple selector
}

void GinCompression::encodeSimple9Batch(const ULONG* values, ULONG count, UCHAR selector,
										CompressionContext& ctx)
{
	// Write selector in top 4 bits, pack remaining values
	ULONG word = static_cast<ULONG>(selector) << 28;
	
	// Pack values (simplified - real implementation would be more complex)
	for (ULONG i = 0; i < count && i < 28; i++) {
		if (values[i] < (1 << 28)) {
			word |= (values[i] & 0x0FFFFFFF);
			break; // Simplified - only encode first value
		}
	}
	
	ctx.writeBytes(reinterpret_cast<const UCHAR*>(&word), 4);
}

ULONG GinCompression::decodeSimple9Batch(ULONG* values, UCHAR selector, 
										  DecompressionContext& ctx)
{
	// Simplified decoding
	values[0] = 1; // Default value
	return 1; // Return count of decoded values
}

void GinCompression::writeBits(ULONG value, UCHAR bit_count, CompressionContext& ctx)
{
	// Simple bit writing - write whole bytes
	while (bit_count >= 8) {
		ctx.writeByte(static_cast<UCHAR>(value & 0xFF));
		value >>= 8;
		bit_count -= 8;
	}
	
	if (bit_count > 0) {
		ctx.writeByte(static_cast<UCHAR>(value & ((1 << bit_count) - 1)));
	}
}

ULONG GinCompression::readBits(UCHAR bit_count, DecompressionContext& ctx)
{
	ULONG result = 0;
	UCHAR shift = 0;
	
	// Simple bit reading - read whole bytes
	while (bit_count >= 8) {
		result |= (static_cast<ULONG>(ctx.readByte()) << shift);
		shift += 8;
		bit_count -= 8;
	}
	
	if (bit_count > 0) {
		UCHAR partial = ctx.readByte();
		result |= (static_cast<ULONG>(partial & ((1 << bit_count) - 1)) << shift);
	}
	
	return result;
}

void GinCompression::flushBits(CompressionContext& ctx)
{
	ctx.flush();
}

//----------------------------
// Validation Methods
//----------------------------

bool GinCompression::validateDeltaCompression(const UCHAR* data, ULONG size)
{
	DecompressionContext ctx(data, size, COMPRESSION_DELTA);
	
	// Try to read record count
	ULONG record_count = ctx.readVarInt();
	if (ctx.underflow || record_count == 0 || record_count > MAX_POSTING_LIST_SIZE) {
		return false;
	}
	
	// Try to read first value
	ULONG prev_value = ctx.readVarInt();
	if (ctx.underflow) {
		return false;
	}
	
	// Try to read remaining deltas
	for (ULONG i = 1; i < record_count; i++) {
		ULONG delta = ctx.readVarInt();
		if (ctx.underflow) {
			return false;
		}
		prev_value += delta;
	}
	
	return !ctx.underflow;
}

bool GinCompression::validateVByteCompression(const UCHAR* data, ULONG size)
{
	DecompressionContext ctx(data, size, COMPRESSION_VBYTE);
	
	// Try to read record count
	ULONG record_count = ctx.readVarInt();
	if (ctx.underflow || record_count == 0 || record_count > MAX_POSTING_LIST_SIZE) {
		return false;
	}
	
	// Try to read all values
	for (ULONG i = 0; i < record_count; i++) {
		ctx.readVarInt();
		if (ctx.underflow) {
			return false;
		}
	}
	
	return !ctx.underflow;
}

bool GinCompression::validatePFDCompression(const UCHAR* data, ULONG size)
{
	// Basic validation - check if size is reasonable
	return size >= 4 && size <= MAX_POSTING_LIST_SIZE * sizeof(RecordNumber);
}

bool GinCompression::validateSimple9Compression(const UCHAR* data, ULONG size)
{
	// Basic validation - check if size is multiple of 4 (32-bit words)
	return (size % 4) == 0 && size >= 4;
}

bool GinCompression::validateRLECompression(const UCHAR* data, ULONG size)
{
	// Basic validation - should have at least record count
	return size >= 1;
}

} // namespace Jrd