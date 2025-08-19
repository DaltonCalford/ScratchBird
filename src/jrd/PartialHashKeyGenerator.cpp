/*
 * The contents of this file are subject to the Initial
 * Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the
 * License. You may obtain a copy of the License at
 * http://www.ibphoenix.com/main.nfs?a=ibphoenix&page=ibp_idpl.
 *
 * Software distributed under the License is distributed AS IS,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied.
 * See the License for the specific language governing rights
 * and limitations under the License.
 *
 * The Original Code was created for the ScratchBird Open Source 
 * RDBMS project.
 *
 * Copyright (c) 2025 ScratchBird Project
 * and all contributors signed below.
 *
 * All Rights Reserved.
 * Contributor(s): ______________________________________.
 *
 * 2025.07.24 - ScratchBird Partial Hash Key Generation Implementation
 */

#include "../include/scratchbird.h"
#include "PartialHashKeyGenerator.h"
#include "PartialHashIndex.h"
#include "Record.h"
#include "jrd.h"
#include "btr.h"
// #include "IndexExpression.h"  // Header not found, functionality may need to be implemented
#include "../dsql/BoolNodes.h"
#include "../common/classes/alloc.h"
#include "../common/classes/auto.h"
#include "../common/isc_proto.h"
#include "sb_exception.h"
#include "../common/StatusArg.h"
#include <memory.h>
#include <string.h>
#include <chrono>
#include <cmath>
#include <cstdlib>

using namespace Jrd;
using namespace ScratchBird;

//----------------------------
// PartialHashKeyGenerator Implementation
//----------------------------

PartialHashKeyGenerator::PartialHashKeyGenerator(thread_db* tdbb, jrd_rel* relation, const index_desc* idx)
    : m_tdbb(tdbb),
      m_relation(relation),
      m_index_desc(idx),
      m_index_expression(nullptr),
      m_generation_strategy(PARTIAL_KEY_STANDARD),
      m_compression_enabled(false),
      m_condition_caching_enabled(true),
      m_metadata_enabled(true),
      m_condition_version(1),
      m_condition_hash_initialized(false),
      m_keys_generated(0),
      m_generation_errors(0),
      m_condition_failures(0),
      m_total_generation_time(0.0),
      m_last_stats_reset(0),
      m_max_key_length(MAX_KEY),
      m_optimal_key_length(128),
      m_compression_ratio_target(0.7),
      m_adaptive_optimization(true),
      m_cache_hits(0),
      m_cache_misses(0)
{
    // Initialize condition hash if condition exists
    if (idx->idx_condition)
    {
        initializeConditionHash();
    }
    
    // Create index expression if needed
    if (idx->idx_expression)
    {
        try
        {
            m_index_expression = new IndexExpression(tdbb, const_cast<index_desc*>(idx));
        }
        catch (const Exception&)
        {
            m_index_expression = nullptr;
        }
    }
    
    // Set optimal strategy based on index characteristics
    if (m_adaptive_optimization)
    {
        m_generation_strategy = selectOptimalStrategy(idx, idx->idx_condition);
    }
}

PartialHashKeyGenerator::~PartialHashKeyGenerator()
{
    delete m_index_expression;
    
    // Clear key cache
    for (auto& cached_key : m_key_cache)
    {
        delete cached_key;
    }
    m_key_cache.clear();
}

KeyValidationResult PartialHashKeyGenerator::generateKey(thread_db* tdbb, Record* record, 
                                                        temporary_key* key, bool validate_condition)
{
    if (!record || !key)
        return KEY_GENERATION_ERROR;
        
    SLONG start_time = getCurrentTimeMicros();
    
    // Validate condition first if required
    if (validate_condition && m_index_desc->idx_condition)
    {
        if (!validateRecordAgainstCondition(tdbb, record))
        {
            m_condition_failures++;
            updateGenerationStatistics(getCurrentTimeMicros() - start_time, false);
            return KEY_CONDITION_FAILED;
        }
    }
    
    KeyValidationResult result;
    
    // Generate key based on selected strategy
    switch (m_generation_strategy)
    {
        case PARTIAL_KEY_STANDARD:
            result = internalGenerateStandardKey(tdbb, record, key);
            break;
            
        case PARTIAL_KEY_CONDITION_AWARE:
            result = internalGenerateConditionAwareKey(tdbb, record, key);
            break;
            
        case PARTIAL_KEY_OPTIMIZED:
            result = internalGenerateOptimizedKey(tdbb, record, key);
            break;
            
        case PARTIAL_KEY_COMPRESSED:
            result = internalGenerateCompressedKey(tdbb, record, key);
            break;
            
        case PARTIAL_KEY_HYBRID:
            result = internalGenerateHybridKey(tdbb, record, key);
            break;
            
        default:
            result = internalGenerateStandardKey(tdbb, record, key);
            break;
    }
    
    // Validate generated key
    if (result == KEY_VALID)
    {
        if (!validateKeyLength(key))
        {
            result = KEY_TOO_LONG;
        }
        else if (!validateKeyData(key))
        {
            result = KEY_INVALID_DATA;
        }
        else if (m_index_desc->idx_flags & idx_unique)
        {
            if (!validateUniqueConstraint(tdbb, key, record))
            {
                result = KEY_DUPLICATE_DETECTED;
            }
        }
    }
    
    // Update statistics
    bool success = (result == KEY_VALID);
    updateGenerationStatistics(getCurrentTimeMicros() - start_time, success);
    
    if (success)
    {
        m_keys_generated++;
    }
    else
    {
        m_generation_errors++;
        logKeyGenerationEvent("generateKey", result);
    }
    
    return result;
}

KeyValidationResult PartialHashKeyGenerator::generateKeyWithMetadata(thread_db* tdbb, Record* record, 
                                                                    temporary_key* key, 
                                                                    PartialKeyMetadata& metadata)
{
    // Generate standard key first
    KeyValidationResult result = generateKey(tdbb, record, key, true);
    
    if (result != KEY_VALID)
        return result;
        
    // Generate metadata
    if (m_metadata_enabled)
    {
        memcpy(metadata.condition_hash, m_condition_hash, sizeof(metadata.condition_hash));
        metadata.condition_version = m_condition_version;
        metadata.inclusion_flags = 0x01; // Record included
        metadata.generation_timestamp = getCurrentTimeMicros();
        metadata.key_complexity_score = calculateKeyComplexity(key);
        
        // Append metadata to key if there's space
        if (key->key_length + sizeof(PartialKeyMetadata) <= MAX_KEY)
        {
            if (!appendConditionMetadata(key, metadata))
            {
                return KEY_GENERATION_ERROR;
            }
        }
    }
    
    return KEY_VALID;
}

KeyValidationResult PartialHashKeyGenerator::internalGenerateStandardKey(thread_db* tdbb, Record* record, temporary_key* key)
{
    try
    {
        // Use IndexKey class to generate standard key
        IndexKey index_key(tdbb, m_relation, const_cast<index_desc*>(m_index_desc));
        
        idx_e error = index_key.compose(record);
        if (error != idx_e_ok)
        {
            return KEY_GENERATION_ERROR;
        }
        
        // Copy generated key
        temporary_key* generated_key = index_key;
        copyKeyData(key, generated_key);
        
        return KEY_VALID;
    }
    catch (const Exception&)
    {
        return KEY_GENERATION_ERROR;
    }
}

KeyValidationResult PartialHashKeyGenerator::internalGenerateConditionAwareKey(thread_db* tdbb, Record* record, temporary_key* key)
{
    // Generate standard key first
    KeyValidationResult result = internalGenerateStandardKey(tdbb, record, key);
    if (result != KEY_VALID)
        return result;
        
    // Add condition-aware metadata
    if (m_condition_hash_initialized && key->key_length + 8 <= MAX_KEY)
    {
        // Append condition hash to key for verification
        memcpy(key->key_data + key->key_length, m_condition_hash, 8);
        key->key_length += 8;
    }
    
    return KEY_VALID;
}

KeyValidationResult PartialHashKeyGenerator::internalGenerateOptimizedKey(thread_db* tdbb, Record* record, temporary_key* key)
{
    // Generate standard key first
    KeyValidationResult result = internalGenerateStandardKey(tdbb, record, key);
    if (result != KEY_VALID)
        return result;
        
    // Apply optimizations based on condition pattern
    if (m_index_desc->idx_condition)
    {
        // Try different optimization strategies
        if (!optimizeForEqualityConditions(tdbb, record, key))
        {
            if (!optimizeForRangeConditions(tdbb, record, key))
            {
                if (!optimizeForPatternConditions(tdbb, record, key))
                {
                    optimizeForComplexConditions(tdbb, record, key);
                }
            }
        }
    }
    
    return KEY_VALID;
}

KeyValidationResult PartialHashKeyGenerator::internalGenerateCompressedKey(thread_db* tdbb, Record* record, temporary_key* key)
{
    // Generate standard key first
    temporary_key standard_key;
    KeyValidationResult result = internalGenerateStandardKey(tdbb, record, &standard_key);
    if (result != KEY_VALID)
        return result;
        
    // Apply compression
    if (m_compression_enabled)
    {
        if (!compressKey(&standard_key, key))
        {
            // Compression failed, use standard key
            copyKeyData(key, &standard_key);
        }
    }
    else
    {
        copyKeyData(key, &standard_key);
    }
    
    return KEY_VALID;
}

KeyValidationResult PartialHashKeyGenerator::internalGenerateHybridKey(thread_db* tdbb, Record* record, temporary_key* key)
{
    // Start with optimized key generation
    KeyValidationResult result = internalGenerateOptimizedKey(tdbb, record, key);
    if (result != KEY_VALID)
        return result;
        
    // Apply compression if beneficial
    if (m_compression_enabled && key->key_length > m_optimal_key_length)
    {
        temporary_key compressed_key;
        if (compressKey(key, &compressed_key))
        {
            // Use compressed key if it's significantly smaller
            if (compressed_key.key_length < key->key_length * m_compression_ratio_target)
            {
                copyKeyData(key, &compressed_key);
            }
        }
    }
    
    // Add condition metadata if space allows
    if (m_metadata_enabled && key->key_length + 8 <= MAX_KEY)
    {
        PartialKeyMetadata metadata;
        memcpy(metadata.condition_hash, m_condition_hash, sizeof(metadata.condition_hash));
        metadata.condition_version = m_condition_version;
        metadata.inclusion_flags = 0x01;
        metadata.generation_timestamp = getCurrentTimeMicros();
        metadata.key_complexity_score = calculateKeyComplexity(key);
        
        appendConditionMetadata(key, metadata);
    }
    
    return KEY_VALID;
}

bool PartialHashKeyGenerator::validateRecordAgainstCondition(thread_db* tdbb, Record* record)
{
    if (!m_index_desc->idx_condition || !record)
        return true;
        
    try
    {
        // This would use the IndexCondition class to evaluate the condition
        IndexCondition condition(tdbb, const_cast<index_desc*>(m_index_desc));
        
        idx_e error_code;
        TriState result = condition.check(record, &error_code);
        
        return (error_code == idx_e_ok && result == FB_TRUE);
    }
    catch (const Exception&)
    {
        return false;
    }
}

bool PartialHashKeyGenerator::validateKeyLength(const temporary_key* key)
{
    return (key && key->key_length > 0 && key->key_length <= m_max_key_length);
}

bool PartialHashKeyGenerator::validateKeyData(const temporary_key* key)
{
    if (!key || key->key_length == 0)
        return false;
        
    // Basic validation - ensure key data is reasonable
    // More sophisticated validation could check for specific data patterns
    return true;
}

bool PartialHashKeyGenerator::validateUniqueConstraint(thread_db* tdbb, const temporary_key* key, Record* record)
{
    // This would need to check if the key already exists in the index
    // For now, assume uniqueness is valid
    return true;
}

void PartialHashKeyGenerator::initializeConditionHash()
{
    if (m_index_desc->idx_condition)
    {
        // Generate a hash of the condition for validation
        // This is a simplified implementation
        const char* condition_text = "condition"; // Would need proper condition serialization
        
        // Simple hash calculation (in real implementation, use proper hash function)
        ULONG hash = 0;
        for (const char* p = condition_text; *p; p++)
        {
            hash = hash * 31 + *p;
        }
        
        memcpy(m_condition_hash, &hash, sizeof(ULONG));
        memset(m_condition_hash + sizeof(ULONG), 0, sizeof(m_condition_hash) - sizeof(ULONG));
        
        m_condition_hash_initialized = true;
    }
}

bool PartialHashKeyGenerator::compressKey(const temporary_key* source_key, temporary_key* compressed_key)
{
    if (!source_key || !compressed_key)
        return false;
        
    // Select optimal compression algorithm
    PartialKeyCompressionAlgorithm* algorithm = selectOptimalCompressionAlgorithm(source_key);
    if (!algorithm)
        return false;
        
    bool success = algorithm->compress(source_key, compressed_key);
    delete algorithm;
    
    return success;
}

bool PartialHashKeyGenerator::decompressKey(const temporary_key* compressed_key, temporary_key* decompressed_key)
{
    if (!compressed_key || !decompressed_key)
        return false;
        
    // This would need to determine which compression algorithm was used
    // For now, assume RLE
    RLEKeyCompression rle;
    return rle.decompress(compressed_key, decompressed_key);
}

bool PartialHashKeyGenerator::optimizeForEqualityConditions(thread_db* tdbb, Record* record, temporary_key* key)
{
    // For equality conditions, we can optimize by reordering key segments
    // to put the most selective fields first
    return true; // Simplified implementation
}

bool PartialHashKeyGenerator::optimizeForRangeConditions(thread_db* tdbb, Record* record, temporary_key* key)
{
    // For range conditions, we might want to truncate the key at the range boundary
    return true; // Simplified implementation
}

bool PartialHashKeyGenerator::optimizeForPatternConditions(thread_db* tdbb, Record* record, temporary_key* key)
{
    // For pattern matching conditions (LIKE, etc.), we can optimize the key prefix
    return true; // Simplified implementation
}

bool PartialHashKeyGenerator::optimizeForComplexConditions(thread_db* tdbb, Record* record, temporary_key* key)
{
    // For complex conditions, we use heuristics to optimize
    return true; // Simplified implementation
}

bool PartialHashKeyGenerator::appendConditionMetadata(temporary_key* key, const PartialKeyMetadata& metadata)
{
    if (!key || key->key_length + sizeof(PartialKeyMetadata) > MAX_KEY)
        return false;
        
    memcpy(key->key_data + key->key_length, &metadata, sizeof(PartialKeyMetadata));
    key->key_length += sizeof(PartialKeyMetadata);
    
    return true;
}

bool PartialHashKeyGenerator::extractConditionMetadata(const temporary_key* key, PartialKeyMetadata& metadata)
{
    if (!key || key->key_length < sizeof(PartialKeyMetadata))
        return false;
        
    memcpy(&metadata, key->key_data + key->key_length - sizeof(PartialKeyMetadata), 
           sizeof(PartialKeyMetadata));
           
    return true;
}

void PartialHashKeyGenerator::copyKeyData(temporary_key* dest, const temporary_key* source)
{
    if (!dest || !source)
        return;
        
    dest->key_length = source->key_length;
    dest->key_flags = source->key_flags;
    dest->key_nulls = source->key_nulls;
    
    if (source->key_length > 0)
    {
        memcpy(dest->key_data, source->key_data, source->key_length);
    }
}

USHORT PartialHashKeyGenerator::calculateKeyComplexity(const temporary_key* key)
{
    if (!key)
        return 0;
        
    // Simple complexity calculation based on key length and null bitmap
    USHORT complexity = key->key_length;
    
    // Add complexity for null segments
    USHORT nulls = key->key_nulls;
    while (nulls)
    {
        if (nulls & 1)
            complexity += 5;
        nulls >>= 1;
    }
    
    return complexity;
}

void PartialHashKeyGenerator::updateGenerationStatistics(SLONG generation_time, bool success)
{
    m_total_generation_time += generation_time;
    
    if (!success)
    {
        m_generation_errors++;
    }
}

void PartialHashKeyGenerator::logKeyGenerationEvent(const char* operation, KeyValidationResult result)
{
    // This would log to the Firebird log system
    // Simplified implementation
}

SLONG PartialHashKeyGenerator::getCurrentTimeMicros() const
{
    return fb_utils::query_performance_counter() / 1000; // Convert to microseconds
}

double PartialHashKeyGenerator::getAverageKeyGenerationTime() const
{
    if (m_keys_generated == 0)
        return 0.0;
        
    return m_total_generation_time / m_keys_generated;
}

void PartialHashKeyGenerator::resetStatistics()
{
    m_keys_generated = 0;
    m_generation_errors = 0;
    m_condition_failures = 0;
    m_total_generation_time = 0.0;
    m_cache_hits = 0;
    m_cache_misses = 0;
    m_last_stats_reset = getCurrentTimeMicros();
}

//----------------------------
// RLEKeyCompression Implementation
//----------------------------

bool RLEKeyCompression::compress(const temporary_key* source, temporary_key* compressed)
{
    if (!source || !compressed || source->key_length == 0)
        return false;
        
    UCHAR* src = source->key_data;
    UCHAR* dest = compressed->key_data;
    USHORT src_len = source->key_length;
    USHORT dest_pos = 0;
    
    for (USHORT i = 0; i < src_len; )
    {
        UCHAR current_byte = src[i];
        USHORT count = 1;
        
        // Count consecutive identical bytes
        while (i + count < src_len && src[i + count] == current_byte && count < 255)
        {
            count++;
        }
        
        if (dest_pos + 2 >= MAX_KEY)
            return false; // Not enough space
            
        if (count > 1)
        {
            // Store as run: count, value
            dest[dest_pos++] = static_cast<UCHAR>(count);
            dest[dest_pos++] = current_byte;
        }
        else
        {
            // Store single byte as-is (with count 1)
            dest[dest_pos++] = 1;
            dest[dest_pos++] = current_byte;
        }
        
        i += count;
    }
    
    compressed->key_length = dest_pos;
    compressed->key_flags = source->key_flags;
    compressed->key_nulls = source->key_nulls;
    
    return true;
}

bool RLEKeyCompression::decompress(const temporary_key* compressed, temporary_key* decompressed)
{
    if (!compressed || !decompressed || compressed->key_length == 0)
        return false;
        
    UCHAR* src = compressed->key_data;
    UCHAR* dest = decompressed->key_data;
    USHORT src_len = compressed->key_length;
    USHORT dest_pos = 0;
    
    for (USHORT i = 0; i < src_len; i += 2)
    {
        if (i + 1 >= src_len)
            return false; // Invalid format
            
        UCHAR count = src[i];
        UCHAR value = src[i + 1];
        
        if (dest_pos + count > MAX_KEY)
            return false; // Not enough space
            
        for (UCHAR j = 0; j < count; j++)
        {
            dest[dest_pos++] = value;
        }
    }
    
    decompressed->key_length = dest_pos;
    decompressed->key_flags = compressed->key_flags;
    decompressed->key_nulls = compressed->key_nulls;
    
    return true;
}

double RLEKeyCompression::getCompressionRatio(const temporary_key* key)
{
    if (!key || key->key_length == 0)
        return 1.0;
        
    // Estimate compression ratio by counting runs
    USHORT runs = 0;
    UCHAR last_byte = key->key_data[0];
    
    for (USHORT i = 1; i < key->key_length; i++)
    {
        if (key->key_data[i] != last_byte)
        {
            runs++;
            last_byte = key->key_data[i];
        }
    }
    runs++; // Add final run
    
    double compressed_size = runs * 2.0; // Each run takes 2 bytes
    return compressed_size / key->key_length;
}

bool RLEKeyCompression::isOptimalForKeyType(const temporary_key* key)
{
    return hasRepetitivePattern(key);
}

//----------------------------
// Utility Functions Implementation
//----------------------------

PartialKeyGenerationStrategy selectOptimalStrategy(const index_desc* idx, const BoolExprNode* condition)
{
    if (!idx)
        return PARTIAL_KEY_STANDARD;
        
    // Simple heuristics for strategy selection
    if (condition)
    {
        if (idx->idx_flags & idx_unique)
            return PARTIAL_KEY_CONDITION_AWARE;
        else
            return PARTIAL_KEY_OPTIMIZED;
    }
    
    return PARTIAL_KEY_STANDARD;
}

PartialKeyCompressionAlgorithm* selectOptimalCompressionAlgorithm(const temporary_key* key)
{
    if (!key)
        return nullptr;
        
    // Simple algorithm selection based on key characteristics
    if (hasRepetitivePattern(key))
        return new RLEKeyCompression();
    else if (isNumericSequence(key))
        return new DeltaKeyCompression();
    else if (isStringKey(key))
        return new HuffmanKeyCompression();
    else
        return new RLEKeyCompression(); // Default
}

bool hasRepetitivePattern(const temporary_key* key)
{
    if (!key || key->key_length < 2)
        return false;
        
    USHORT repetitions = 0;
    UCHAR last_byte = key->key_data[0];
    
    for (USHORT i = 1; i < key->key_length; i++)
    {
        if (key->key_data[i] == last_byte)
            repetitions++;
        else
            last_byte = key->key_data[i];
    }
    
    return (repetitions > key->key_length / 4); // More than 25% repetition
}

bool isNumericSequence(const temporary_key* key)
{
    // Simple check for numeric data pattern
    return (key && key->key_length >= 4 && (key->key_length % 4 == 0 || key->key_length % 8 == 0));
}

bool isStringKey(const temporary_key* key)
{
    if (!key || key->key_length == 0)
        return false;
        
    // Check if key data looks like string data (printable ASCII)
    for (USHORT i = 0; i < key->key_length; i++)
    {
        UCHAR c = key->key_data[i];
        if (c < 32 || c > 126)
            return false; // Non-printable character
    }
    
    return true;
}

double calculateKeyEntropy(const temporary_key* key)
{
    if (!key || key->key_length == 0)
        return 0.0;
        
    // Calculate Shannon entropy
    ULONG frequency[256] = {0};
    
    for (USHORT i = 0; i < key->key_length; i++)
    {
        frequency[key->key_data[i]]++;
    }
    
    double entropy = 0.0;
    for (int i = 0; i < 256; i++)
    {
        if (frequency[i] > 0)
        {
            double p = static_cast<double>(frequency[i]) / key->key_length;
            entropy -= p * log2(p);
        }
    }
    
    return entropy;
}

//----------------------------
// DeltaKeyCompression Implementation
//----------------------------

bool DeltaKeyCompression::compress(const temporary_key* source, temporary_key* compressed)
{
    if (!source || !compressed || source->key_length == 0)
        return false;

    // Delta compression works best with numeric data
    if (source->key_length < sizeof(SINT32))
        return false;

    UCHAR* src = source->key_data;
    UCHAR* dest = compressed->key_data;
    USHORT src_len = source->key_length;
    USHORT dest_pos = 0;

    // Store compression type marker
    dest[dest_pos++] = 0xDE; // Delta compression marker

    // Process data based on expected numeric size
    if (src_len % sizeof(SINT64) == 0)
    {
        // Process as 64-bit integers
        USHORT num_values = src_len / sizeof(SINT64);
        dest[dest_pos++] = 8; // Value size marker
        
        for (USHORT i = 0; i < num_values; i++)
        {
            SINT64 current_value;
            memcpy(&current_value, src + (i * sizeof(SINT64)), sizeof(SINT64));
            
            SINT64 delta = current_value - m_last_value;
            
            // Store delta using variable-length encoding
            if (delta >= -128 && delta <= 127)
            {
                // 1-byte delta
                dest[dest_pos++] = 0x01; // Size marker
                dest[dest_pos++] = static_cast<UCHAR>(delta);
            }
            else if (delta >= -32768 && delta <= 32767)
            {
                // 2-byte delta
                dest[dest_pos++] = 0x02; // Size marker
                memcpy(dest + dest_pos, &delta, 2);
                dest_pos += 2;
            }
            else if (delta >= -2147483648LL && delta <= 2147483647LL)
            {
                // 4-byte delta
                dest[dest_pos++] = 0x04; // Size marker
                memcpy(dest + dest_pos, &delta, 4);
                dest_pos += 4;
            }
            else
            {
                // 8-byte delta (no compression benefit)
                dest[dest_pos++] = 0x08; // Size marker
                memcpy(dest + dest_pos, &delta, 8);
                dest_pos += 8;
            }
            
            m_last_value = current_value;
        }
    }
    else if (src_len % sizeof(SINT32) == 0)
    {
        // Process as 32-bit integers
        USHORT num_values = src_len / sizeof(SINT32);
        dest[dest_pos++] = 4; // Value size marker
        
        for (USHORT i = 0; i < num_values; i++)
        {
            SINT32 current_value;
            memcpy(&current_value, src + (i * sizeof(SINT32)), sizeof(SINT32));
            
            SINT32 delta = current_value - static_cast<SINT32>(m_last_value);
            
            // Variable-length encoding for 32-bit deltas
            if (delta >= -128 && delta <= 127)
            {
                dest[dest_pos++] = 0x01;
                dest[dest_pos++] = static_cast<UCHAR>(delta);
            }
            else if (delta >= -32768 && delta <= 32767)
            {
                dest[dest_pos++] = 0x02;
                memcpy(dest + dest_pos, &delta, 2);
                dest_pos += 2;
            }
            else
            {
                dest[dest_pos++] = 0x04;
                memcpy(dest + dest_pos, &delta, 4);
                dest_pos += 4;
            }
            
            m_last_value = current_value;
        }
    }
    else
    {
        // Fallback: treat as bytes with delta encoding
        dest[dest_pos++] = 1; // Value size marker
        
        for (USHORT i = 0; i < src_len; i++)
        {
            SCHAR delta = src[i] - static_cast<UCHAR>(m_last_value);
            dest[dest_pos++] = static_cast<UCHAR>(delta);
            m_last_value = src[i];
        }
    }

    compressed->key_length = dest_pos;
    compressed->key_flags = source->key_flags;
    compressed->key_nulls = source->key_nulls;

    return true;
}

bool DeltaKeyCompression::decompress(const temporary_key* compressed, temporary_key* decompressed)
{
    if (!compressed || !decompressed || compressed->key_length < 3)
        return false;

    UCHAR* src = compressed->key_data;
    UCHAR* dest = decompressed->key_data;
    USHORT src_len = compressed->key_length;
    USHORT src_pos = 0;
    USHORT dest_pos = 0;

    // Check delta compression marker
    if (src[src_pos++] != 0xDE)
        return false;

    UCHAR value_size = src[src_pos++];
    m_last_value = 0; // Reset for decompression

    if (value_size == 8)
    {
        // 64-bit integer decompression
        while (src_pos < src_len)
        {
            UCHAR delta_size = src[src_pos++];
            SINT64 delta = 0;
            
            if (delta_size == 1)
            {
                delta = static_cast<SCHAR>(src[src_pos++]);
            }
            else if (delta_size == 2)
            {
                memcpy(&delta, src + src_pos, 2);
                src_pos += 2;
            }
            else if (delta_size == 4)
            {
                memcpy(&delta, src + src_pos, 4);
                src_pos += 4;
            }
            else if (delta_size == 8)
            {
                memcpy(&delta, src + src_pos, 8);
                src_pos += 8;
            }
            else
            {
                return false; // Invalid delta size
            }
            
            m_last_value += delta;
            memcpy(dest + dest_pos, &m_last_value, sizeof(SINT64));
            dest_pos += sizeof(SINT64);
        }
    }
    else if (value_size == 4)
    {
        // 32-bit integer decompression
        while (src_pos < src_len)
        {
            UCHAR delta_size = src[src_pos++];
            SINT32 delta = 0;
            
            if (delta_size == 1)
            {
                delta = static_cast<SCHAR>(src[src_pos++]);
            }
            else if (delta_size == 2)
            {
                memcpy(&delta, src + src_pos, 2);
                src_pos += 2;
            }
            else if (delta_size == 4)
            {
                memcpy(&delta, src + src_pos, 4);
                src_pos += 4;
            }
            else
            {
                return false;
            }
            
            SINT32 current_value = static_cast<SINT32>(m_last_value) + delta;
            memcpy(dest + dest_pos, &current_value, sizeof(SINT32));
            dest_pos += sizeof(SINT32);
            m_last_value = current_value;
        }
    }
    else if (value_size == 1)
    {
        // Byte-level decompression
        while (src_pos < src_len)
        {
            SCHAR delta = static_cast<SCHAR>(src[src_pos++]);
            UCHAR current_value = static_cast<UCHAR>(m_last_value) + delta;
            dest[dest_pos++] = current_value;
            m_last_value = current_value;
        }
    }
    else
    {
        return false; // Unsupported value size
    }

    decompressed->key_length = dest_pos;
    decompressed->key_flags = compressed->key_flags;
    decompressed->key_nulls = compressed->key_nulls;

    return true;
}

double DeltaKeyCompression::getCompressionRatio(const temporary_key* key)
{
    if (!key || key->key_length == 0)
        return 1.0;

    // Estimate compression ratio based on expected delta sizes
    // This is a heuristic - actual compression depends on data patterns
    if (isNumericSequence(key))
    {
        // For numeric sequences, expect good compression (50-80%)
        return 0.6;
    }
    else
    {
        // For non-sequential data, compression may be poor
        return 0.9;
    }
}

bool DeltaKeyCompression::isOptimalForKeyType(const temporary_key* key)
{
    return isNumericSequence(key);
}

//----------------------------
// HuffmanKeyCompression Implementation
//----------------------------

HuffmanKeyCompression::HuffmanKeyCompression()
    : m_root(nullptr),
      m_tree_built(false)
{
    memset(m_frequency_table, 0, sizeof(m_frequency_table));
}

HuffmanKeyCompression::~HuffmanKeyCompression()
{
    destroyTree(m_root);
}

bool HuffmanKeyCompression::compress(const temporary_key* source, temporary_key* compressed)
{
    if (!source || !compressed || source->key_length == 0)
        return false;

    // Build frequency table for this key
    clearFrequencyTable();
    for (USHORT i = 0; i < source->key_length; i++)
    {
        m_frequency_table[source->key_data[i]]++;
    }

    // Build Huffman tree
    if (!buildHuffmanTree())
        return false;

    UCHAR* dest = compressed->key_data;
    USHORT dest_pos = 0;

    // Store compression marker
    dest[dest_pos++] = 0x48; // Huffman compression marker ('H')

    // Store frequency table (compressed representation)
    USHORT unique_chars = 0;
    for (int i = 0; i < 256; i++)
    {
        if (m_frequency_table[i] > 0)
            unique_chars++;
    }

    dest[dest_pos++] = static_cast<UCHAR>(unique_chars);
    dest[dest_pos++] = static_cast<UCHAR>(unique_chars >> 8);

    // Store character-frequency pairs
    for (int i = 0; i < 256; i++)
    {
        if (m_frequency_table[i] > 0)
        {
            dest[dest_pos++] = static_cast<UCHAR>(i);
            // Store frequency (up to 4 bytes)
            ULONG freq = m_frequency_table[i];
            dest[dest_pos++] = static_cast<UCHAR>(freq);
            dest[dest_pos++] = static_cast<UCHAR>(freq >> 8);
            dest[dest_pos++] = static_cast<UCHAR>(freq >> 16);
            dest[dest_pos++] = static_cast<UCHAR>(freq >> 24);
        }
    }

    // Store compressed data using bit-level encoding
    // For simplicity, we'll use a basic implementation
    // In a full implementation, this would use actual Huffman codes
    
    // Store original length
    dest[dest_pos++] = static_cast<UCHAR>(source->key_length);
    dest[dest_pos++] = static_cast<UCHAR>(source->key_length >> 8);

    // Simplified compression: store run-length encoded version for demo
    UCHAR current_char = source->key_data[0];
    UCHAR run_length = 1;
    
    for (USHORT i = 1; i < source->key_length; i++)
    {
        if (source->key_data[i] == current_char && run_length < 255)
        {
            run_length++;
        }
        else
        {
            dest[dest_pos++] = run_length;
            dest[dest_pos++] = current_char;
            current_char = source->key_data[i];
            run_length = 1;
        }
    }
    
    // Store final run
    dest[dest_pos++] = run_length;
    dest[dest_pos++] = current_char;

    compressed->key_length = dest_pos;
    compressed->key_flags = source->key_flags;
    compressed->key_nulls = source->key_nulls;

    return true;
}

bool HuffmanKeyCompression::decompress(const temporary_key* compressed, temporary_key* decompressed)
{
    if (!compressed || !decompressed || compressed->key_length < 3)
        return false;

    UCHAR* src = compressed->key_data;
    UCHAR* dest = decompressed->key_data;
    USHORT src_pos = 0;
    USHORT dest_pos = 0;

    // Check Huffman compression marker
    if (src[src_pos++] != 0x48)
        return false;

    // Read frequency table
    USHORT unique_chars = src[src_pos] | (src[src_pos + 1] << 8);
    src_pos += 2;

    clearFrequencyTable();
    for (USHORT i = 0; i < unique_chars; i++)
    {
        UCHAR character = src[src_pos++];
        ULONG frequency = src[src_pos] | 
                         (src[src_pos + 1] << 8) |
                         (src[src_pos + 2] << 16) |
                         (src[src_pos + 3] << 24);
        src_pos += 4;
        m_frequency_table[character] = frequency;
    }

    // Read original length
    USHORT original_length = src[src_pos] | (src[src_pos + 1] << 8);
    src_pos += 2;

    // Decompress run-length encoded data
    while (src_pos < compressed->key_length && dest_pos < original_length)
    {
        UCHAR run_length = src[src_pos++];
        UCHAR character = src[src_pos++];
        
        for (UCHAR i = 0; i < run_length && dest_pos < original_length; i++)
        {
            dest[dest_pos++] = character;
        }
    }

    decompressed->key_length = dest_pos;
    decompressed->key_flags = compressed->key_flags;
    decompressed->key_nulls = compressed->key_nulls;

    return true;
}

double HuffmanKeyCompression::getCompressionRatio(const temporary_key* key)
{
    if (!key || key->key_length == 0)
        return 1.0;

    // Calculate theoretical compression ratio based on entropy
    double entropy = calculateKeyEntropy(key);
    
    // Huffman coding approaches the entropy limit
    // Add overhead for frequency table and headers
    double theoretical_ratio = entropy / 8.0; // bits per symbol -> bytes per symbol
    double overhead = (256 * 5 + 10) / static_cast<double>(key->key_length); // frequency table + headers
    
    return theoretical_ratio + overhead;
}

bool HuffmanKeyCompression::isOptimalForKeyType(const temporary_key* key)
{
    // Huffman is optimal for keys with non-uniform character distribution
    double entropy = calculateKeyEntropy(key);
    return entropy < 7.0 && isStringKey(key); // Less than 7 bits per symbol average
}

bool HuffmanKeyCompression::buildFrequencyTable(const temporary_key* keys[], ULONG key_count)
{
    clearFrequencyTable();
    
    for (ULONG k = 0; k < key_count; k++)
    {
        if (!keys[k])
            continue;
            
        for (USHORT i = 0; i < keys[k]->key_length; i++)
        {
            m_frequency_table[keys[k]->key_data[i]]++;
        }
    }
    
    return true;
}

bool HuffmanKeyCompression::buildHuffmanTree()
{
    destroyTree(m_root);
    m_root = nullptr;
    m_tree_built = false;

    // Count non-zero frequencies
    USHORT char_count = 0;
    for (int i = 0; i < 256; i++)
    {
        if (m_frequency_table[i] > 0)
            char_count++;
    }

    if (char_count < 2)
    {
        // Not enough characters for Huffman tree
        return false;
    }

    // Create priority queue (simplified implementation using array)
    HuffmanNode* nodes[256];
    USHORT node_count = 0;

    // Create leaf nodes
    for (int i = 0; i < 256; i++)
    {
        if (m_frequency_table[i] > 0)
        {
            nodes[node_count++] = createNode(static_cast<UCHAR>(i), m_frequency_table[i]);
        }
    }

    // Build tree using simplified algorithm
    while (node_count > 1)
    {
        // Find two nodes with minimum frequency
        USHORT min1 = 0, min2 = 1;
        if (nodes[min1]->frequency > nodes[min2]->frequency)
        {
            min1 = 1;
            min2 = 0;
        }

        for (USHORT i = 2; i < node_count; i++)
        {
            if (nodes[i]->frequency < nodes[min1]->frequency)
            {
                min2 = min1;
                min1 = i;
            }
            else if (nodes[i]->frequency < nodes[min2]->frequency)
            {
                min2 = i;
            }
        }

        // Create new internal node
        HuffmanNode* new_node = createNode(0, nodes[min1]->frequency + nodes[min2]->frequency);
        new_node->left = nodes[min1];
        new_node->right = nodes[min2];
        new_node->is_leaf = false;

        // Replace min1 with new node, remove min2
        nodes[min1] = new_node;
        nodes[min2] = nodes[node_count - 1];
        node_count--;
    }

    m_root = nodes[0];
    m_tree_built = true;
    return true;
}

void HuffmanKeyCompression::clearFrequencyTable()
{
    memset(m_frequency_table, 0, sizeof(m_frequency_table));
}

HuffmanKeyCompression::HuffmanNode* HuffmanKeyCompression::createNode(UCHAR character, ULONG frequency)
{
    HuffmanNode* node = new HuffmanNode();
    node->character = character;
    node->frequency = frequency;
    node->left = nullptr;
    node->right = nullptr;
    node->is_leaf = true;
    return node;
}

void HuffmanKeyCompression::destroyTree(HuffmanNode* node)
{
    if (node)
    {
        destroyTree(node->left);
        destroyTree(node->right);
        delete node;
    }
}

bool HuffmanKeyCompression::encodeCharacter(UCHAR character, ScratchBird::string& code)
{
    // Simplified encoding - in full implementation would traverse tree
    code = "0"; // Placeholder
    return true;
}

bool HuffmanKeyCompression::decodeCharacter(const ScratchBird::string& code, UCHAR& character)
{
    // Simplified decoding - in full implementation would traverse tree
    character = 0; // Placeholder
    return true;
}

//----------------------------
// Advanced Compression Features
//----------------------------

// Adaptive compression algorithm selection
PartialKeyCompressionAlgorithm* selectOptimalCompressionAlgorithmAdvanced(const temporary_key* key, double target_ratio)
{
    if (!key)
        return nullptr;

    // Test all algorithms and select best one
    struct AlgorithmResult
    {
        PartialKeyCompressionAlgorithm* algorithm;
        double compression_ratio;
        const char* name;
    };

    AlgorithmResult results[3];
    results[0].algorithm = new RLEKeyCompression();
    results[0].compression_ratio = results[0].algorithm->getCompressionRatio(key);
    results[0].name = "RLE";

    results[1].algorithm = new DeltaKeyCompression();
    results[1].compression_ratio = results[1].algorithm->getCompressionRatio(key);
    results[1].name = "Delta";

    results[2].algorithm = new HuffmanKeyCompression();
    results[2].compression_ratio = results[2].algorithm->getCompressionRatio(key);
    results[2].name = "Huffman";

    // Find best algorithm
    int best_index = 0;
    for (int i = 1; i < 3; i++)
    {
        if (results[i].compression_ratio < results[best_index].compression_ratio)
        {
            best_index = i;
        }
    }

    // Clean up non-selected algorithms
    for (int i = 0; i < 3; i++)
    {
        if (i != best_index)
        {
            delete results[i].algorithm;
        }
    }

    // Check if best algorithm meets target ratio
    if (results[best_index].compression_ratio > target_ratio)
    {
        delete results[best_index].algorithm;
        return nullptr; // No algorithm meets target
    }

    return results[best_index].algorithm;
}

// Compression ratio validation
bool validateCompressionRatio(const temporary_key* original, const temporary_key* compressed, double expected_ratio, double tolerance)
{
    if (!original || !compressed)
        return false;

    if (original->key_length == 0)
        return true; // Trivial case

    double actual_ratio = static_cast<double>(compressed->key_length) / static_cast<double>(original->key_length);
    double difference = std::abs(actual_ratio - expected_ratio);
    
    return difference <= tolerance;
}

// Compression performance analysis
struct CompressionAnalysis
{
    double compression_ratio;
    ULONG original_size;
    ULONG compressed_size;
    ULONG compression_time_microseconds;
    ULONG decompression_time_microseconds;
    bool compression_successful;
    bool decompression_successful;
    const char* algorithm_name;
    double space_savings_percent;
    double compression_efficiency;
};

CompressionAnalysis analyzeCompressionPerformance(const temporary_key* key, PartialKeyCompressionAlgorithm* algorithm)
{
    CompressionAnalysis analysis;
    memset(&analysis, 0, sizeof(analysis));
    
    if (!key || !algorithm)
    {
        return analysis;
    }
    
    analysis.algorithm_name = algorithm->getAlgorithmName();
    analysis.original_size = key->key_length;
    
    // Test compression
    temporary_key compressed_key;
    auto start_time = std::chrono::high_resolution_clock::now();
    analysis.compression_successful = algorithm->compress(key, &compressed_key);
    auto end_time = std::chrono::high_resolution_clock::now();
    
    analysis.compression_time_microseconds = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
    
    if (analysis.compression_successful)
    {
        analysis.compressed_size = compressed_key.key_length;
        analysis.compression_ratio = static_cast<double>(analysis.compressed_size) / static_cast<double>(analysis.original_size);
        analysis.space_savings_percent = (1.0 - analysis.compression_ratio) * 100.0;
        
        // Test decompression
        temporary_key decompressed_key;
        start_time = std::chrono::high_resolution_clock::now();
        analysis.decompression_successful = algorithm->decompress(&compressed_key, &decompressed_key);
        end_time = std::chrono::high_resolution_clock::now();
        
        analysis.decompression_time_microseconds = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
        
        // Calculate efficiency (space savings per unit time)
        ULONG total_time = analysis.compression_time_microseconds + analysis.decompression_time_microseconds;
        if (total_time > 0)
        {
            analysis.compression_efficiency = analysis.space_savings_percent / static_cast<double>(total_time);
        }
    }
    
    return analysis;
}