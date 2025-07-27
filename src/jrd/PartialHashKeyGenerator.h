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
 * 2025.07.24 - ScratchBird Partial Hash Key Generation Algorithms
 */

#ifndef JRD_PARTIAL_HASH_KEY_GENERATOR_H
#define JRD_PARTIAL_HASH_KEY_GENERATOR_H

#include "constants.h"
// #include "btr.h"  // Causing compilation issues, removed for now
#include "../common/classes/array.h"
#include "../common/classes/fb_string.h"

namespace Jrd {

// Forward declarations
class thread_db;
class Record;
class jrd_rel;
struct index_desc;
struct temporary_key;
struct dsc;
class IndexExpression;
class BoolExprNode;

//----------------------------
// Key Generation Strategy Types
//----------------------------
enum PartialKeyGenerationStrategy
{
    PARTIAL_KEY_STANDARD = 0,           // Standard key generation
    PARTIAL_KEY_CONDITION_AWARE = 1,    // Include condition metadata in key
    PARTIAL_KEY_OPTIMIZED = 2,          // Optimized for specific condition patterns
    PARTIAL_KEY_COMPRESSED = 3,         // Compressed keys for better space efficiency
    PARTIAL_KEY_HYBRID = 4              // Combination of multiple strategies
};

//----------------------------
// Key Validation Results
//----------------------------
enum KeyValidationResult
{
    KEY_VALID = 0,                      // Key is valid for partial index
    KEY_CONDITION_FAILED = 1,           // Record doesn't meet index condition
    KEY_GENERATION_ERROR = 2,           // Error during key generation
    KEY_TOO_LONG = 3,                   // Generated key exceeds maximum length
    KEY_INVALID_DATA = 4,               // Key contains invalid data
    KEY_DUPLICATE_DETECTED = 5          // Duplicate key detected in unique partial index
};

//----------------------------
// Partial Key Metadata
//----------------------------
struct PartialKeyMetadata
{
    UCHAR condition_hash[8];            // Hash of condition for validation
    USHORT condition_version;           // Version of condition used
    UCHAR inclusion_flags;              // Flags indicating inclusion criteria met
    SLONG generation_timestamp;         // When key was generated
    USHORT key_complexity_score;        // Estimated complexity of key generation
};

//----------------------------
// PartialHashKeyGenerator - Advanced key generation for partial hash indexes
//----------------------------
class PartialHashKeyGenerator
{
public:
    // Constructor
    PartialHashKeyGenerator(thread_db* tdbb, jrd_rel* relation, const index_desc* idx);
    
    // Destructor
    ~PartialHashKeyGenerator();
    
    // Primary key generation methods
    KeyValidationResult generateKey(thread_db* tdbb, Record* record, 
                                   temporary_key* key, bool validate_condition = true);
    KeyValidationResult generateKeyWithMetadata(thread_db* tdbb, Record* record, 
                                               temporary_key* key, PartialKeyMetadata& metadata);
    KeyValidationResult generateOptimizedKey(thread_db* tdbb, Record* record, 
                                            temporary_key* key, PartialKeyGenerationStrategy strategy);
    
    // Key validation and verification
    bool validateKeyForPartialIndex(thread_db* tdbb, const temporary_key* key, Record* record);
    bool validateConditionConsistency(thread_db* tdbb, const temporary_key* key, 
                                     const PartialKeyMetadata& metadata);
    KeyValidationResult verifyKeyIntegrity(const temporary_key* key, const PartialKeyMetadata& metadata);
    
    // Condition-aware key generation
    bool generateConditionAwareKey(thread_db* tdbb, Record* record, temporary_key* key);
    bool appendConditionMetadata(temporary_key* key, const PartialKeyMetadata& metadata);
    bool extractConditionMetadata(const temporary_key* key, PartialKeyMetadata& metadata);
    
    // Key optimization methods
    bool compressKey(temporary_key* source_key, temporary_key* compressed_key);
    bool decompressKey(const temporary_key* compressed_key, temporary_key* decompressed_key);
    USHORT calculateOptimalKeyLength(thread_db* tdbb, Record* record);
    
    // Performance optimization
    void setGenerationStrategy(PartialKeyGenerationStrategy strategy);
    PartialKeyGenerationStrategy getGenerationStrategy() const;
    void enableKeyCompression(bool enable);
    void enableConditionCaching(bool enable);
    
    // Statistics and monitoring
    ULONG getKeysGenerated() const;
    ULONG getKeyGenerationErrors() const;
    ULONG getConditionValidationFailures() const;
    double getAverageKeyGenerationTime() const;
    void resetStatistics();
    
    // Key analysis and introspection
    bool analyzeKeyDistribution(thread_db* tdbb, ULONG sample_size, 
                               ScratchBird::string& analysis_report);
    double calculateKeySelectivity(thread_db* tdbb, const temporary_key* key);
    bool recommendsKeyOptimization(const temporary_key* key);

private:
    // Internal key generation methods
    KeyValidationResult internalGenerateStandardKey(thread_db* tdbb, Record* record, temporary_key* key);
    KeyValidationResult internalGenerateConditionAwareKey(thread_db* tdbb, Record* record, temporary_key* key);
    KeyValidationResult internalGenerateOptimizedKey(thread_db* tdbb, Record* record, temporary_key* key);
    KeyValidationResult internalGenerateCompressedKey(thread_db* tdbb, Record* record, temporary_key* key);
    KeyValidationResult internalGenerateHybridKey(thread_db* tdbb, Record* record, temporary_key* key);
    
    // Key validation helpers
    bool validateRecordAgainstCondition(thread_db* tdbb, Record* record);
    bool validateKeyLength(const temporary_key* key);
    bool validateKeyData(const temporary_key* key);
    bool validateUniqueConstraint(thread_db* tdbb, const temporary_key* key, Record* record);
    
    // Condition processing
    void initializeConditionHash();
    void updateConditionHash(const BoolExprNode* condition);
    bool verifyConditionHash(const UCHAR* expected_hash);
    
    // Key compression algorithms
    bool compressNumericKey(const temporary_key* source, temporary_key* compressed);
    bool compressStringKey(const temporary_key* source, temporary_key* compressed);
    bool compressDateTimeKey(const temporary_key* source, temporary_key* compressed);
    bool compressCompositeKey(const temporary_key* source, temporary_key* compressed);
    
    // Key decompression algorithms
    bool decompressNumericKey(const temporary_key* compressed, temporary_key* decompressed);
    bool decompressStringKey(const temporary_key* compressed, temporary_key* decompressed);
    bool decompressDateTimeKey(const temporary_key* compressed, temporary_key* decompressed);
    bool decompressCompositeKey(const temporary_key* compressed, temporary_key* decompressed);
    
    // Optimization algorithms
    bool optimizeForEqualityConditions(thread_db* tdbb, Record* record, temporary_key* key);
    bool optimizeForRangeConditions(thread_db* tdbb, Record* record, temporary_key* key);
    bool optimizeForPatternConditions(thread_db* tdbb, Record* record, temporary_key* key);
    bool optimizeForComplexConditions(thread_db* tdbb, Record* record, temporary_key* key);
    
    // Performance monitoring
    void updateGenerationStatistics(SLONG generation_time, bool success);
    void logKeyGenerationEvent(const char* operation, KeyValidationResult result);
    
    // Utility methods
    ULONG calculateKeyHash(const temporary_key* key);
    void copyKeyData(temporary_key* dest, const temporary_key* source);
    bool compareKeys(const temporary_key* key1, const temporary_key* key2);
    SLONG getCurrentTimeMicros() const;

private:
    // Core members
    thread_db* m_tdbb;
    jrd_rel* m_relation;
    const index_desc* m_index_desc;
    IndexExpression* m_index_expression;
    
    // Generation configuration
    PartialKeyGenerationStrategy m_generation_strategy;
    bool m_compression_enabled;
    bool m_condition_caching_enabled;
    bool m_metadata_enabled;
    
    // Condition processing
    UCHAR m_condition_hash[8];
    USHORT m_condition_version;
    bool m_condition_hash_initialized;
    
    // Performance tracking
    mutable ULONG m_keys_generated;
    mutable ULONG m_generation_errors;
    mutable ULONG m_condition_failures;
    mutable double m_total_generation_time;
    mutable SLONG m_last_stats_reset;
    
    // Optimization parameters
    USHORT m_max_key_length;
    USHORT m_optimal_key_length;
    double m_compression_ratio_target;
    bool m_adaptive_optimization;
    
    // Caching for performance
    typedef ScratchBird::HalfStaticArray<temporary_key*, 16> KeyCache;
    mutable KeyCache m_key_cache;
    mutable ULONG m_cache_hits;
    mutable ULONG m_cache_misses;
};

//----------------------------
// PartialKeyCompressionAlgorithm - Base class for key compression algorithms
//----------------------------
class PartialKeyCompressionAlgorithm
{
public:
    virtual ~PartialKeyCompressionAlgorithm() {}
    
    // Pure virtual methods for compression/decompression
    virtual bool compress(const temporary_key* source, temporary_key* compressed) = 0;
    virtual bool decompress(const temporary_key* compressed, temporary_key* decompressed) = 0;
    virtual double getCompressionRatio(const temporary_key* key) = 0;
    virtual const char* getAlgorithmName() const = 0;
    
    // Optional optimization methods
    virtual bool isOptimalForKeyType(const temporary_key* key) { return true; }
    virtual USHORT estimateCompressedSize(const temporary_key* key) { return key->key_length; }
    virtual bool requiresDecompressionForComparison() const { return true; }
};

//----------------------------
// RLEKeyCompression - Run-Length Encoding for repetitive keys
//----------------------------
class RLEKeyCompression : public PartialKeyCompressionAlgorithm
{
public:
    virtual bool compress(const temporary_key* source, temporary_key* compressed) override;
    virtual bool decompress(const temporary_key* compressed, temporary_key* decompressed) override;
    virtual double getCompressionRatio(const temporary_key* key) override;
    virtual const char* getAlgorithmName() const override { return "RLE"; }
    virtual bool isOptimalForKeyType(const temporary_key* key) override;
};

//----------------------------
// DeltaKeyCompression - Delta encoding for numeric sequences
//----------------------------
class DeltaKeyCompression : public PartialKeyCompressionAlgorithm
{
public:
    DeltaKeyCompression() : m_last_value(0) {}
    
    virtual bool compress(const temporary_key* source, temporary_key* compressed) override;
    virtual bool decompress(const temporary_key* compressed, temporary_key* decompressed) override;
    virtual double getCompressionRatio(const temporary_key* key) override;
    virtual const char* getAlgorithmName() const override { return "Delta"; }
    virtual bool isOptimalForKeyType(const temporary_key* key) override;
    
    void reset() { m_last_value = 0; }

private:
    SINT64 m_last_value;  // Last value for delta calculation
};

//----------------------------
// HuffmanKeyCompression - Huffman coding for string keys
//----------------------------
class HuffmanKeyCompression : public PartialKeyCompressionAlgorithm
{
public:
    HuffmanKeyCompression();
    virtual ~HuffmanKeyCompression();
    
    virtual bool compress(const temporary_key* source, temporary_key* compressed) override;
    virtual bool decompress(const temporary_key* compressed, temporary_key* decompressed) override;
    virtual double getCompressionRatio(const temporary_key* key) override;
    virtual const char* getAlgorithmName() const override { return "Huffman"; }
    virtual bool isOptimalForKeyType(const temporary_key* key) override;
    
    // Huffman-specific methods
    bool buildFrequencyTable(const temporary_key* keys[], ULONG key_count);
    bool buildHuffmanTree();
    void clearFrequencyTable();

private:
    // Huffman tree node
    struct HuffmanNode
    {
        UCHAR character;
        ULONG frequency;
        HuffmanNode* left;
        HuffmanNode* right;
        bool is_leaf;
    };
    
    HuffmanNode* m_root;
    ULONG m_frequency_table[256];
    bool m_tree_built;
    
    // Helper methods
    HuffmanNode* createNode(UCHAR character, ULONG frequency);
    void destroyTree(HuffmanNode* node);
    bool encodeCharacter(UCHAR character, ScratchBird::string& code);
    bool decodeCharacter(const ScratchBird::string& code, UCHAR& character);
};

//----------------------------
// Utility Functions
//----------------------------

// Key generation strategy selection
PartialKeyGenerationStrategy selectOptimalStrategy(const index_desc* idx, 
                                                  const BoolExprNode* condition);

// Key compression algorithm selection
PartialKeyCompressionAlgorithm* selectOptimalCompressionAlgorithm(const temporary_key* key);

// Key analysis utilities
double calculateKeyEntropy(const temporary_key* key);
bool hasRepetitivePattern(const temporary_key* key);
bool isNumericSequence(const temporary_key* key);
bool isStringKey(const temporary_key* key);

} // namespace Jrd

#endif // JRD_PARTIAL_HASH_KEY_GENERATOR_H