/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		RoaringBitmap.h
 *	DESCRIPTION:	Roaring Bitmap implementation for very large sparse bitmaps
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
 * 2025.07.23 - ScratchBird Roaring Bitmap Implementation
 */

#ifndef JRD_ROARING_BITMAP_H
#define JRD_ROARING_BITMAP_H

#include "../jrd/constants.h"
#include "../common/classes/array.h"
#include "../common/classes/fb_string.h"
#include "../common/gdsassert.h"
#include <vector>
#include <algorithm>
#include <memory>

namespace Jrd {

// Forward declarations
class MemoryPool;

//----------------------------
// Roaring Bitmap Constants
//----------------------------

// Roaring Bitmap uses 16-bit chunks (containers)
inline constexpr ULONG ROARING_CONTAINER_SIZE = 65536;     // 2^16 values per container
inline constexpr USHORT ROARING_MAX_CONTAINER_VALUE = 65535; // Maximum value in container
inline constexpr ULONG ROARING_BITMAP_THRESHOLD = 100000; // Switch to Roaring for bitmaps > 100K bits
inline constexpr USHORT ROARING_ARRAY_THRESHOLD = 4096;   // Array vs bitmap container threshold
inline constexpr USHORT ROARING_RUN_THRESHOLD = 2048;     // Run vs array container threshold

//----------------------------
// Container Types for Roaring Bitmap
//----------------------------

enum RoaringContainerType : UCHAR
{
    ROARING_ARRAY_CONTAINER = 0,    // Dense array of values
    ROARING_BITMAP_CONTAINER = 1,   // Traditional bitmap
    ROARING_RUN_CONTAINER = 2       // Run-length encoded container
};

//----------------------------
// Roaring Container Interface
//----------------------------

/**
 * Abstract base class for Roaring Bitmap containers
 */
class RoaringContainer
{
public:
    explicit RoaringContainer(MemoryPool* pool, RoaringContainerType type);
    virtual ~RoaringContainer() = default;

    // Core container operations
    virtual bool contains(USHORT value) const = 0;
    virtual bool add(USHORT value) = 0;
    virtual bool remove(USHORT value) = 0;
    virtual ULONG getCardinality() const = 0;
    
    // Container optimization
    virtual RoaringContainer* optimize() = 0;
    virtual RoaringContainer* clone() const = 0;
    
    // Serialization
    virtual ULONG serialize(UCHAR* buffer, ULONG buffer_size) const = 0;
    virtual bool deserialize(const UCHAR* buffer, ULONG buffer_size) = 0;
    
    // Container operations
    virtual RoaringContainer* andContainer(const RoaringContainer* other) const = 0;
    virtual RoaringContainer* orContainer(const RoaringContainer* other) const = 0;
    virtual RoaringContainer* xorContainer(const RoaringContainer* other) const = 0;
    virtual RoaringContainer* andNotContainer(const RoaringContainer* other) const = 0;
    
    // Iteration support
    virtual bool getNextValue(USHORT& value, USHORT& iterator_state) const = 0;
    virtual void resetIterator(USHORT& iterator_state) const = 0;
    
    // Properties
    RoaringContainerType getType() const { return m_type; }
    ULONG getStorageSize() const { return m_storage_size; }
    bool isEmpty() const { return getCardinality() == 0; }

protected:
    MemoryPool* m_pool;
    RoaringContainerType m_type;
    ULONG m_storage_size;
    
    // Helper methods for container conversion
    static RoaringContainer* createOptimalContainer(MemoryPool* pool, 
                                                   const std::vector<USHORT>& values);
    static RoaringContainerType determineOptimalType(ULONG cardinality, 
                                                     ULONG run_count = 0);
};

//----------------------------
// Array Container Implementation
//----------------------------

/**
 * Array container for sparse data (cardinality < 4096)
 */
class RoaringArrayContainer : public RoaringContainer
{
public:
    explicit RoaringArrayContainer(MemoryPool* pool);
    explicit RoaringArrayContainer(MemoryPool* pool, const std::vector<USHORT>& values);
    virtual ~RoaringArrayContainer();

    // RoaringContainer interface
    virtual bool contains(USHORT value) const override;
    virtual bool add(USHORT value) override;
    virtual bool remove(USHORT value) override;
    virtual ULONG getCardinality() const override;
    
    virtual RoaringContainer* optimize() override;
    virtual RoaringContainer* clone() const override;
    
    virtual ULONG serialize(UCHAR* buffer, ULONG buffer_size) const override;
    virtual bool deserialize(const UCHAR* buffer, ULONG buffer_size) override;
    
    virtual RoaringContainer* andContainer(const RoaringContainer* other) const override;
    virtual RoaringContainer* orContainer(const RoaringContainer* other) const override;
    virtual RoaringContainer* xorContainer(const RoaringContainer* other) const override;
    virtual RoaringContainer* andNotContainer(const RoaringContainer* other) const override;
    
    virtual bool getNextValue(USHORT& value, USHORT& iterator_state) const override;
    virtual void resetIterator(USHORT& iterator_state) const override;

    // Array-specific operations
    const USHORT* getArray() const { return m_values; }
    ULONG getCapacity() const { return m_capacity; }
    void trimToSize();

private:
    USHORT* m_values;           // Sorted array of values
    ULONG m_cardinality;        // Number of values in array
    ULONG m_capacity;           // Allocated capacity
    
    // Binary search helpers
    ULONG binarySearch(USHORT value) const;
    void insertAt(ULONG index, USHORT value);
    void removeAt(ULONG index);
    void ensureCapacity(ULONG required_capacity);
    
    // Container conversion helpers
    RoaringContainer* convertToBitmapContainer() const;
    RoaringContainer* convertToRunContainer() const;
};

//----------------------------
// Bitmap Container Implementation
//----------------------------

/**
 * Bitmap container for dense data (cardinality >= 4096)
 */
class RoaringBitmapContainer : public RoaringContainer
{
public:
    explicit RoaringBitmapContainer(MemoryPool* pool);
    explicit RoaringBitmapContainer(MemoryPool* pool, const std::vector<USHORT>& values);
    virtual ~RoaringBitmapContainer();

    // RoaringContainer interface
    virtual bool contains(USHORT value) const override;
    virtual bool add(USHORT value) override;
    virtual bool remove(USHORT value) override;
    virtual ULONG getCardinality() const override;
    
    virtual RoaringContainer* optimize() override;
    virtual RoaringContainer* clone() const override;
    
    virtual ULONG serialize(UCHAR* buffer, ULONG buffer_size) const override;
    virtual bool deserialize(const UCHAR* buffer, ULONG buffer_size) override;
    
    virtual RoaringContainer* andContainer(const RoaringContainer* other) const override;
    virtual RoaringContainer* orContainer(const RoaringContainer* other) const override;
    virtual RoaringContainer* xorContainer(const RoaringContainer* other) const override;
    virtual RoaringContainer* andNotContainer(const RoaringContainer* other) const override;
    
    virtual bool getNextValue(USHORT& value, USHORT& iterator_state) const override;
    virtual void resetIterator(USHORT& iterator_state) const override;

    // Bitmap-specific operations
    const ULONG* getBitmap() const { return m_bitmap; }
    void setBitRange(USHORT start, USHORT end);
    void clearBitRange(USHORT start, USHORT end);

private:
    ULONG* m_bitmap;            // Bitmap data (2048 32-bit words = 65536 bits)
    ULONG m_cardinality;        // Cached cardinality count
    bool m_cardinality_dirty;   // True if cardinality needs recalculation
    
    static constexpr ULONG BITMAP_WORDS = ROARING_CONTAINER_SIZE / 32; // 2048 words
    
    // Bitmap manipulation helpers
    void setBit(USHORT value);
    void clearBit(USHORT value);
    bool testBit(USHORT value) const;
    void updateCardinality();
    ULONG countSetBits() const;
    
    // Container conversion helpers
    RoaringContainer* convertToArrayContainer() const;
    RoaringContainer* convertToRunContainer() const;
    
    // Bitwise operations
    void bitwiseAnd(const RoaringBitmapContainer* other);
    void bitwiseOr(const RoaringBitmapContainer* other);
    void bitwiseXor(const RoaringBitmapContainer* other);
    void bitwiseAndNot(const RoaringBitmapContainer* other);
};

//----------------------------
// Run Container Implementation
//----------------------------

/**
 * Run container for consecutive ranges of values
 */
class RoaringRunContainer : public RoaringContainer
{
public:
    explicit RoaringRunContainer(MemoryPool* pool);
    explicit RoaringRunContainer(MemoryPool* pool, const std::vector<USHORT>& values);
    virtual ~RoaringRunContainer();

    // RoaringContainer interface
    virtual bool contains(USHORT value) const override;
    virtual bool add(USHORT value) override;
    virtual bool remove(USHORT value) override;
    virtual ULONG getCardinality() const override;
    
    virtual RoaringContainer* optimize() override;
    virtual RoaringContainer* clone() const override;
    
    virtual ULONG serialize(UCHAR* buffer, ULONG buffer_size) const override;
    virtual bool deserialize(const UCHAR* buffer, ULONG buffer_size) override;
    
    virtual RoaringContainer* andContainer(const RoaringContainer* other) const override;
    virtual RoaringContainer* orContainer(const RoaringContainer* other) const override;
    virtual RoaringContainer* xorContainer(const RoaringContainer* other) const override;
    virtual RoaringContainer* andNotContainer(const RoaringContainer* other) const override;
    
    virtual bool getNextValue(USHORT& value, USHORT& iterator_state) const override;
    virtual void resetIterator(USHORT& iterator_state) const override;

    // Run-specific operations
    struct Run
    {
        USHORT start;           // Start of run (inclusive)
        USHORT length;          // Length of run (0 means single value)
        
        Run(USHORT s = 0, USHORT l = 0) : start(s), length(l) {}
        USHORT getEnd() const { return start + length; }
        ULONG getCardinality() const { return length + 1; }
        bool contains(USHORT value) const { return value >= start && value <= getEnd(); }
    };
    
    const std::vector<Run>& getRuns() const { return m_runs; }
    ULONG getRunCount() const { return m_runs.size(); }

private:
    std::vector<Run> m_runs;    // Sorted array of runs
    ULONG m_cardinality;        // Cached cardinality
    
    // Run manipulation
    ULONG findRunIndex(USHORT value) const;
    void insertRun(const Run& run);
    void removeRun(ULONG index);
    void mergeAdjacentRuns();
    void splitRun(ULONG index, USHORT split_value);
    
    // Container conversion helpers
    RoaringContainer* convertToArrayContainer() const;
    RoaringContainer* convertToBitmapContainer() const;
    
    // Run operations
    std::vector<Run> intersectRuns(const std::vector<Run>& other_runs) const;
    std::vector<Run> unionRuns(const std::vector<Run>& other_runs) const;
    std::vector<Run> xorRuns(const std::vector<Run>& other_runs) const;
    std::vector<Run> andNotRuns(const std::vector<Run>& other_runs) const;
    
    void updateCardinality();
};

//----------------------------
// Main Roaring Bitmap Implementation
//----------------------------

/**
 * Roaring Bitmap main class for very large sparse bitmaps
 */
class RoaringBitmap
{
public:
    explicit RoaringBitmap(MemoryPool* pool);
    ~RoaringBitmap();

    // Basic bitmap operations
    bool contains(ULONG value) const;
    bool add(ULONG value);
    bool remove(ULONG value);
    void clear();
    
    // Bulk operations
    void addRange(ULONG start, ULONG end);
    void removeRange(ULONG start, ULONG end);
    bool addMany(const ULONG* values, ULONG count);
    
    // Bitmap operations
    RoaringBitmap* bitwiseAnd(const RoaringBitmap* other) const;
    RoaringBitmap* bitwiseOr(const RoaringBitmap* other) const;
    RoaringBitmap* bitwiseXor(const RoaringBitmap* other) const;
    RoaringBitmap* bitwiseAndNot(const RoaringBitmap* other) const;
    
    void bitwiseAndInPlace(const RoaringBitmap* other);
    void bitwiseOrInPlace(const RoaringBitmap* other);
    void bitwiseXorInPlace(const RoaringBitmap* other);
    void bitwiseAndNotInPlace(const RoaringBitmap* other);
    
    // Statistics and properties
    ULONG getCardinality() const;
    ULONG getStorageSize() const;
    double getCompressionRatio() const;
    bool isEmpty() const;
    
    // Optimization
    void optimize();
    void runOptimize();  // Optimize for run-length encoding
    
    // Serialization
    ULONG serialize(UCHAR* buffer, ULONG buffer_size) const;
    bool deserialize(const UCHAR* buffer, ULONG buffer_size);
    ULONG getSerializedSize() const;
    
    // Iteration support
    class Iterator
    {
    public:
        explicit Iterator(const RoaringBitmap* bitmap);
        bool hasNext() const;
        ULONG getNext();
        void reset();
        
    private:
        const RoaringBitmap* m_bitmap;
        USHORT m_container_index;
        USHORT m_container_iterator;
        bool m_initialized;
        
        void advanceToNextContainer();
    };
    
    Iterator getIterator() const;
    
    // Utility methods
    ULONG getMinValue() const;
    ULONG getMaxValue() const;
    bool equals(const RoaringBitmap* other) const;
    ScratchBird::string toString() const;
    
    // Container management
    ULONG getContainerCount() const;
    void printStatistics() const;

private:
    MemoryPool* m_pool;
    
    // Container storage
    struct HighLowPair
    {
        USHORT high;                    // High 16 bits of 32-bit values
        RoaringContainer* container;    // Container for low 16 bits
        
        HighLowPair(USHORT h = 0, RoaringContainer* c = nullptr) 
            : high(h), container(c) {}
    };
    
    std::vector<HighLowPair> m_containers;  // Sorted by high value
    
    // Container management
    ULONG findContainerIndex(USHORT high_bits) const;
    RoaringContainer* getContainer(USHORT high_bits) const;
    RoaringContainer* getOrCreateContainer(USHORT high_bits);
    void removeContainer(ULONG index);
    void insertContainer(ULONG index, USHORT high_bits, RoaringContainer* container);
    
    // Optimization helpers
    void optimizeContainer(ULONG index);
    bool shouldOptimizeContainer(const RoaringContainer* container) const;
    
    // Utility functions
    static USHORT getHighBits(ULONG value) { return static_cast<USHORT>(value >> 16); }
    static USHORT getLowBits(ULONG value) { return static_cast<USHORT>(value & 0xFFFF); }
    static ULONG combineHighLow(USHORT high, USHORT low) { return (static_cast<ULONG>(high) << 16) | low; }
};

//----------------------------
// Roaring Bitmap Factory
//----------------------------

/**
 * Factory for creating and managing Roaring Bitmaps
 */
class RoaringBitmapFactory
{
public:
    // Bitmap creation
    static RoaringBitmap* createBitmap(MemoryPool* pool);
    static RoaringBitmap* createFromValues(MemoryPool* pool, const ULONG* values, ULONG count);
    static RoaringBitmap* createFromRanges(MemoryPool* pool, const std::vector<std::pair<ULONG, ULONG>>& ranges);
    
    // Container creation
    static RoaringContainer* createContainer(MemoryPool* pool, RoaringContainerType type);
    static RoaringContainer* createOptimalContainer(MemoryPool* pool, const std::vector<USHORT>& values);
    
    // Utility methods
    static bool shouldUseRoaringBitmap(ULONG max_value, ULONG cardinality, double sparsity);
    static RoaringContainerType recommendContainerType(ULONG cardinality, ULONG run_count = 0);
    static ULONG estimateStorageSize(ULONG max_value, ULONG cardinality);
};

//----------------------------
// Integration with Compressed Bitmap
//----------------------------

/**
 * Enhanced CompressedBitmap that uses Roaring format for large sparse bitmaps
 */
class EnhancedCompressedBitmap
{
public:
    explicit EnhancedCompressedBitmap(MemoryPool* pool);
    ~EnhancedCompressedBitmap();
    
    // Bitmap operations (same interface as CompressedBitmap)
    void setBit(ULONG bit_position);
    void clearBit(ULONG bit_position);
    bool testBit(ULONG bit_position) const;
    
    void bitwiseAnd(const EnhancedCompressedBitmap& other);
    void bitwiseOr(const EnhancedCompressedBitmap& other);
    void bitwiseXor(const EnhancedCompressedBitmap& other);
    void bitwiseNot();
    void bitwiseAndNot(const EnhancedCompressedBitmap& other);
    
    // Statistics
    ULONG getSetBitCount() const;
    ULONG getTotalBitCount() const;
    double getCompressionRatio() const;
    ULONG getStorageSize() const;
    bool isEmpty() const;
    
    // Automatic format selection
    void optimize();
    bool isUsingRoaringFormat() const { return m_using_roaring; }

private:
    MemoryPool* m_pool;
    bool m_using_roaring;
    
    // Storage (only one is active at a time)
    RoaringBitmap* m_roaring_bitmap;        // For large sparse bitmaps
    UCHAR* m_traditional_bitmap;            // For small/dense bitmaps
    ULONG m_traditional_size;
    ULONG m_bit_count;
    
    // Format selection
    void selectOptimalFormat();
    void convertToRoaring();
    void convertToTraditional();
    bool shouldUseRoaring() const;
    
    // Statistics tracking
    ULONG m_set_bit_count;
    ULONG m_max_bit_position;
    double m_sparsity_ratio;
    
    void updateStatistics();
    void calculateSparsity();
};

} // namespace Jrd

#endif // JRD_ROARING_BITMAP_H