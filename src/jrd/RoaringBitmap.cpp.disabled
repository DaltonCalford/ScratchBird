/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		RoaringBitmap.cpp
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

#include "scratchbird.h"
#include "RoaringBitmap.h"
#include "../common/gdsassert.h"
#include "../common/classes/alloc.h"
#include <algorithm>
#include <cstring>
#include <cmath>

using namespace ScratchBird;
using namespace Jrd;

//----------------------------
// RoaringContainer Base Implementation
//----------------------------

RoaringContainer::RoaringContainer(MemoryPool* pool, RoaringContainerType type)
    : m_pool(pool), m_type(type), m_storage_size(0)
{
    fb_assert(pool);
}

RoaringContainer* RoaringContainer::createOptimalContainer(MemoryPool* pool, 
                                                          const std::vector<USHORT>& values)
{
    ULONG cardinality = values.size();
    
    if (cardinality == 0) {
        return new RoaringArrayContainer(pool);
    }
    
    // Calculate run count for run container optimization
    ULONG run_count = 1;
    for (size_t i = 1; i < values.size(); i++) {
        if (values[i] != values[i-1] + 1) {
            run_count++;
        }
    }
    
    RoaringContainerType optimal_type = determineOptimalType(cardinality, run_count);
    
    switch (optimal_type) {
        case ROARING_ARRAY_CONTAINER:
            return new RoaringArrayContainer(pool, values);
        case ROARING_BITMAP_CONTAINER:
            return new RoaringBitmapContainer(pool, values);
        case ROARING_RUN_CONTAINER:
            return new RoaringRunContainer(pool, values);
        default:
            return new RoaringArrayContainer(pool, values);
    }
}

RoaringContainerType RoaringContainer::determineOptimalType(ULONG cardinality, ULONG run_count)
{
    // Use run container if we have very few runs (highly consecutive data)
    if (run_count > 0 && cardinality / run_count > 8) {
        return ROARING_RUN_CONTAINER;
    }
    
    // Use bitmap container for dense data
    if (cardinality >= ROARING_ARRAY_THRESHOLD) {
        return ROARING_BITMAP_CONTAINER;
    }
    
    // Use array container for sparse data
    return ROARING_ARRAY_CONTAINER;
}

//----------------------------
// RoaringArrayContainer Implementation
//----------------------------

RoaringArrayContainer::RoaringArrayContainer(MemoryPool* pool)
    : RoaringContainer(pool, ROARING_ARRAY_CONTAINER),
      m_values(nullptr), m_cardinality(0), m_capacity(0)
{
    ensureCapacity(16); // Start with small capacity
}

RoaringArrayContainer::RoaringArrayContainer(MemoryPool* pool, const std::vector<USHORT>& values)
    : RoaringContainer(pool, ROARING_ARRAY_CONTAINER),
      m_values(nullptr), m_cardinality(0), m_capacity(0)
{
    ensureCapacity(values.size());
    
    for (USHORT value : values) {
        add(value);
    }
}

RoaringArrayContainer::~RoaringArrayContainer()
{
    if (m_values && m_pool) {
        m_pool->deallocate(m_values);
    }
}

bool RoaringArrayContainer::contains(USHORT value) const
{
    return binarySearch(value) < m_cardinality && m_values[binarySearch(value)] == value;
}

bool RoaringArrayContainer::add(USHORT value)
{
    ULONG index = binarySearch(value);
    
    if (index < m_cardinality && m_values[index] == value) {
        return false; // Already exists
    }
    
    ensureCapacity(m_cardinality + 1);
    insertAt(index, value);
    return true;
}

bool RoaringArrayContainer::remove(USHORT value)
{
    ULONG index = binarySearch(value);
    
    if (index < m_cardinality && m_values[index] == value) {
        removeAt(index);
        return true;
    }
    
    return false;
}

ULONG RoaringArrayContainer::getCardinality() const
{
    return m_cardinality;
}

RoaringContainer* RoaringArrayContainer::optimize()
{
    // Convert to bitmap container if we exceed the threshold
    if (m_cardinality >= ROARING_ARRAY_THRESHOLD) {
        return convertToBitmapContainer();
    }
    
    // Convert to run container if highly consecutive
    if (m_cardinality > 100) {
        ULONG run_count = 1;
        for (ULONG i = 1; i < m_cardinality; i++) {
            if (m_values[i] != m_values[i-1] + 1) {
                run_count++;
            }
        }
        
        if (m_cardinality / run_count > 8) {
            return convertToRunContainer();
        }
    }
    
    // Optimize array storage
    trimToSize();
    return nullptr; // No conversion needed
}

RoaringContainer* RoaringArrayContainer::clone() const
{
    std::vector<USHORT> values(m_values, m_values + m_cardinality);
    return new RoaringArrayContainer(m_pool, values);
}

ULONG RoaringArrayContainer::serialize(UCHAR* buffer, ULONG buffer_size) const
{
    ULONG required_size = sizeof(ULONG) + m_cardinality * sizeof(USHORT);
    
    if (buffer_size < required_size) {
        return required_size;
    }
    
    // Write cardinality
    memcpy(buffer, &m_cardinality, sizeof(ULONG));
    buffer += sizeof(ULONG);
    
    // Write values
    memcpy(buffer, m_values, m_cardinality * sizeof(USHORT));
    
    return required_size;
}

bool RoaringArrayContainer::deserialize(const UCHAR* buffer, ULONG buffer_size)
{
    if (buffer_size < sizeof(ULONG)) {
        return false;
    }
    
    // Read cardinality
    ULONG cardinality;
    memcpy(&cardinality, buffer, sizeof(ULONG));
    buffer += sizeof(ULONG);
    
    ULONG required_size = sizeof(ULONG) + cardinality * sizeof(USHORT);
    if (buffer_size < required_size) {
        return false;
    }
    
    // Prepare storage
    ensureCapacity(cardinality);
    m_cardinality = cardinality;
    
    // Read values
    memcpy(m_values, buffer, cardinality * sizeof(USHORT));
    
    m_storage_size = required_size;
    return true;
}

RoaringContainer* RoaringArrayContainer::andContainer(const RoaringContainer* other) const
{
    if (other->getType() == ROARING_ARRAY_CONTAINER) {
        const RoaringArrayContainer* other_array = static_cast<const RoaringArrayContainer*>(other);
        std::vector<USHORT> result;
        
        ULONG i = 0, j = 0;
        while (i < m_cardinality && j < other_array->m_cardinality) {
            if (m_values[i] == other_array->m_values[j]) {
                result.push_back(m_values[i]);
                i++;
                j++;
            } else if (m_values[i] < other_array->m_values[j]) {
                i++;
            } else {
                j++;
            }
        }
        
        return RoaringContainer::createOptimalContainer(m_pool, result);
    }
    
    // Delegate to other container type
    return other->andContainer(this);
}

RoaringContainer* RoaringArrayContainer::orContainer(const RoaringContainer* other) const
{
    if (other->getType() == ROARING_ARRAY_CONTAINER) {
        const RoaringArrayContainer* other_array = static_cast<const RoaringArrayContainer*>(other);
        std::vector<USHORT> result;
        result.reserve(m_cardinality + other_array->m_cardinality);
        
        ULONG i = 0, j = 0;
        while (i < m_cardinality && j < other_array->m_cardinality) {
            if (m_values[i] == other_array->m_values[j]) {
                result.push_back(m_values[i]);
                i++;
                j++;
            } else if (m_values[i] < other_array->m_values[j]) {
                result.push_back(m_values[i]);
                i++;
            } else {
                result.push_back(other_array->m_values[j]);
                j++;
            }
        }
        
        // Add remaining elements
        while (i < m_cardinality) {
            result.push_back(m_values[i++]);
        }
        while (j < other_array->m_cardinality) {
            result.push_back(other_array->m_values[j++]);
        }
        
        return RoaringContainer::createOptimalContainer(m_pool, result);
    }
    
    return other->orContainer(this);
}

RoaringContainer* RoaringArrayContainer::xorContainer(const RoaringContainer* other) const
{
    if (other->getType() == ROARING_ARRAY_CONTAINER) {
        const RoaringArrayContainer* other_array = static_cast<const RoaringArrayContainer*>(other);
        std::vector<USHORT> result;
        
        ULONG i = 0, j = 0;
        while (i < m_cardinality && j < other_array->m_cardinality) {
            if (m_values[i] == other_array->m_values[j]) {
                // Skip equal values (XOR eliminates them)
                i++;
                j++;
            } else if (m_values[i] < other_array->m_values[j]) {
                result.push_back(m_values[i]);
                i++;
            } else {
                result.push_back(other_array->m_values[j]);
                j++;
            }
        }
        
        // Add remaining elements
        while (i < m_cardinality) {
            result.push_back(m_values[i++]);
        }
        while (j < other_array->m_cardinality) {
            result.push_back(other_array->m_values[j++]);
        }
        
        return RoaringContainer::createOptimalContainer(m_pool, result);
    }
    
    return other->xorContainer(this);
}

RoaringContainer* RoaringArrayContainer::andNotContainer(const RoaringContainer* other) const
{
    std::vector<USHORT> result;
    
    for (ULONG i = 0; i < m_cardinality; i++) {
        if (!other->contains(m_values[i])) {
            result.push_back(m_values[i]);
        }
    }
    
    return RoaringContainer::createOptimalContainer(m_pool, result);
}

bool RoaringArrayContainer::getNextValue(USHORT& value, USHORT& iterator_state) const
{
    if (iterator_state >= m_cardinality) {
        return false;
    }
    
    value = m_values[iterator_state];
    iterator_state++;
    return true;
}

void RoaringArrayContainer::resetIterator(USHORT& iterator_state) const
{
    iterator_state = 0;
}

ULONG RoaringArrayContainer::binarySearch(USHORT value) const
{
    ULONG left = 0, right = m_cardinality;
    
    while (left < right) {
        ULONG mid = left + (right - left) / 2;
        if (m_values[mid] < value) {
            left = mid + 1;
        } else {
            right = mid;
        }
    }
    
    return left;
}

void RoaringArrayContainer::insertAt(ULONG index, USHORT value)
{
    // Shift elements to the right
    for (ULONG i = m_cardinality; i > index; i--) {
        m_values[i] = m_values[i-1];
    }
    
    m_values[index] = value;
    m_cardinality++;
    m_storage_size = m_cardinality * sizeof(USHORT);
}

void RoaringArrayContainer::removeAt(ULONG index)
{
    // Shift elements to the left
    for (ULONG i = index; i < m_cardinality - 1; i++) {
        m_values[i] = m_values[i+1];
    }
    
    m_cardinality--;
    m_storage_size = m_cardinality * sizeof(USHORT);
}

void RoaringArrayContainer::ensureCapacity(ULONG required_capacity)
{
    if (required_capacity <= m_capacity) {
        return;
    }
    
    ULONG new_capacity = std::max(required_capacity, m_capacity * 2);
    USHORT* new_values = static_cast<USHORT*>(m_pool->allocate(new_capacity * sizeof(USHORT)));
    
    if (m_values) {
        memcpy(new_values, m_values, m_cardinality * sizeof(USHORT));
        m_pool->deallocate(m_values);
    }
    
    m_values = new_values;
    m_capacity = new_capacity;
}

void RoaringArrayContainer::trimToSize()
{
    if (m_capacity > m_cardinality && m_cardinality > 0) {
        USHORT* new_values = static_cast<USHORT*>(m_pool->allocate(m_cardinality * sizeof(USHORT)));
        memcpy(new_values, m_values, m_cardinality * sizeof(USHORT));
        m_pool->deallocate(m_values);
        
        m_values = new_values;
        m_capacity = m_cardinality;
    }
}

RoaringContainer* RoaringArrayContainer::convertToBitmapContainer() const
{
    std::vector<USHORT> values(m_values, m_values + m_cardinality);
    return new RoaringBitmapContainer(m_pool, values);
}

RoaringContainer* RoaringArrayContainer::convertToRunContainer() const
{
    std::vector<USHORT> values(m_values, m_values + m_cardinality);
    return new RoaringRunContainer(m_pool, values);
}

//----------------------------
// RoaringBitmapContainer Implementation
//----------------------------

RoaringBitmapContainer::RoaringBitmapContainer(MemoryPool* pool)
    : RoaringContainer(pool, ROARING_BITMAP_CONTAINER),
      m_bitmap(nullptr), m_cardinality(0), m_cardinality_dirty(false)
{
    m_bitmap = static_cast<ULONG*>(m_pool->allocate(BITMAP_WORDS * sizeof(ULONG)));
    memset(m_bitmap, 0, BITMAP_WORDS * sizeof(ULONG));
    m_storage_size = BITMAP_WORDS * sizeof(ULONG);
}

RoaringBitmapContainer::RoaringBitmapContainer(MemoryPool* pool, const std::vector<USHORT>& values)
    : RoaringContainer(pool, ROARING_BITMAP_CONTAINER),
      m_bitmap(nullptr), m_cardinality(0), m_cardinality_dirty(false)
{
    m_bitmap = static_cast<ULONG*>(m_pool->allocate(BITMAP_WORDS * sizeof(ULONG)));
    memset(m_bitmap, 0, BITMAP_WORDS * sizeof(ULONG));
    m_storage_size = BITMAP_WORDS * sizeof(ULONG);
    
    for (USHORT value : values) {
        setBit(value);
    }
    
    updateCardinality();
}

RoaringBitmapContainer::~RoaringBitmapContainer()
{
    if (m_bitmap && m_pool) {
        m_pool->deallocate(m_bitmap);
    }
}

bool RoaringBitmapContainer::contains(USHORT value) const
{
    return testBit(value);
}

bool RoaringBitmapContainer::add(USHORT value)
{
    if (testBit(value)) {
        return false; // Already set
    }
    
    setBit(value);
    m_cardinality++;
    return true;
}

bool RoaringBitmapContainer::remove(USHORT value)
{
    if (!testBit(value)) {
        return false; // Not set
    }
    
    clearBit(value);
    m_cardinality--;
    return true;
}

ULONG RoaringBitmapContainer::getCardinality() const
{
    if (m_cardinality_dirty) {
        const_cast<RoaringBitmapContainer*>(this)->updateCardinality();
    }
    return m_cardinality;
}

RoaringContainer* RoaringBitmapContainer::optimize()
{
    updateCardinality();
    
    // Convert to array container if sparse enough
    if (m_cardinality < ROARING_ARRAY_THRESHOLD) {
        return convertToArrayContainer();
    }
    
    return nullptr; // No conversion needed
}

RoaringContainer* RoaringBitmapContainer::clone() const
{
    RoaringBitmapContainer* new_container = new RoaringBitmapContainer(m_pool);
    memcpy(new_container->m_bitmap, m_bitmap, BITMAP_WORDS * sizeof(ULONG));
    new_container->m_cardinality = m_cardinality;
    new_container->m_cardinality_dirty = m_cardinality_dirty;
    return new_container;
}

ULONG RoaringBitmapContainer::serialize(UCHAR* buffer, ULONG buffer_size) const
{
    ULONG required_size = BITMAP_WORDS * sizeof(ULONG);
    
    if (buffer_size < required_size) {
        return required_size;
    }
    
    memcpy(buffer, m_bitmap, required_size);
    return required_size;
}

bool RoaringBitmapContainer::deserialize(const UCHAR* buffer, ULONG buffer_size)
{
    ULONG required_size = BITMAP_WORDS * sizeof(ULONG);
    
    if (buffer_size < required_size) {
        return false;
    }
    
    memcpy(m_bitmap, buffer, required_size);
    m_cardinality_dirty = true;
    m_storage_size = required_size;
    return true;
}

void RoaringBitmapContainer::setBit(USHORT value)
{
    ULONG word_index = value / 32;
    ULONG bit_index = value % 32;
    m_bitmap[word_index] |= (1UL << bit_index);
}

void RoaringBitmapContainer::clearBit(USHORT value)
{
    ULONG word_index = value / 32;
    ULONG bit_index = value % 32;
    m_bitmap[word_index] &= ~(1UL << bit_index);
}

bool RoaringBitmapContainer::testBit(USHORT value) const
{
    ULONG word_index = value / 32;
    ULONG bit_index = value % 32;
    return (m_bitmap[word_index] & (1UL << bit_index)) != 0;
}

void RoaringBitmapContainer::updateCardinality()
{
    m_cardinality = countSetBits();
    m_cardinality_dirty = false;
}

ULONG RoaringBitmapContainer::countSetBits() const
{
    ULONG count = 0;
    for (ULONG i = 0; i < BITMAP_WORDS; i++) {
        ULONG word = m_bitmap[i];
        // Brian Kernighan's algorithm
        while (word) {
            word &= word - 1;
            count++;
        }
    }
    return count;
}

bool RoaringBitmapContainer::getNextValue(USHORT& value, USHORT& iterator_state) const
{
    for (USHORT i = iterator_state; i < ROARING_CONTAINER_SIZE; i++) {
        if (testBit(i)) {
            value = i;
            iterator_state = i + 1;
            return true;
        }
    }
    return false;
}

void RoaringBitmapContainer::resetIterator(USHORT& iterator_state) const
{
    iterator_state = 0;
}

//----------------------------
// RoaringBitmap Main Implementation
//----------------------------

RoaringBitmap::RoaringBitmap(MemoryPool* pool) : m_pool(pool)
{
    fb_assert(pool);
}

RoaringBitmap::~RoaringBitmap()
{
    clear();
}

bool RoaringBitmap::contains(ULONG value) const
{
    USHORT high_bits = getHighBits(value);
    USHORT low_bits = getLowBits(value);
    
    RoaringContainer* container = getContainer(high_bits);
    return container ? container->contains(low_bits) : false;
}

bool RoaringBitmap::add(ULONG value)
{
    USHORT high_bits = getHighBits(value);
    USHORT low_bits = getLowBits(value);
    
    RoaringContainer* container = getOrCreateContainer(high_bits);
    return container->add(low_bits);
}

bool RoaringBitmap::remove(ULONG value)
{
    USHORT high_bits = getHighBits(value);
    USHORT low_bits = getLowBits(value);
    
    ULONG index = findContainerIndex(high_bits);
    if (index >= m_containers.size() || m_containers[index].high != high_bits) {
        return false; // Container doesn't exist
    }
    
    bool removed = m_containers[index].container->remove(low_bits);
    
    // Remove empty containers
    if (removed && m_containers[index].container->isEmpty()) {
        delete m_containers[index].container;
        m_containers.erase(m_containers.begin() + index);
    }
    
    return removed;
}

void RoaringBitmap::clear()
{
    for (auto& pair : m_containers) {
        delete pair.container;
    }
    m_containers.clear();
}

ULONG RoaringBitmap::getCardinality() const
{
    ULONG total = 0;
    for (const auto& pair : m_containers) {
        total += pair.container->getCardinality();
    }
    return total;
}

ULONG RoaringBitmap::getStorageSize() const
{
    ULONG total = 0;
    for (const auto& pair : m_containers) {
        total += pair.container->getStorageSize();
    }
    return total + m_containers.size() * sizeof(HighLowPair);
}

double RoaringBitmap::getCompressionRatio() const
{
    ULONG cardinality = getCardinality();
    if (cardinality == 0) return 1.0;
    
    ULONG uncompressed_size = (getMaxValue() / 8) + 1; // Traditional bitmap size
    ULONG compressed_size = getStorageSize();
    
    return static_cast<double>(uncompressed_size) / static_cast<double>(compressed_size);
}

bool RoaringBitmap::isEmpty() const
{
    return m_containers.empty();
}

void RoaringBitmap::optimize()
{
    for (size_t i = 0; i < m_containers.size(); i++) {
        RoaringContainer* optimized = m_containers[i].container->optimize();
        if (optimized) {
            delete m_containers[i].container;
            m_containers[i].container = optimized;
        }
    }
}

ULONG RoaringBitmap::findContainerIndex(USHORT high_bits) const
{
    // Binary search for container
    ULONG left = 0, right = m_containers.size();
    
    while (left < right) {
        ULONG mid = left + (right - left) / 2;
        if (m_containers[mid].high < high_bits) {
            left = mid + 1;
        } else {
            right = mid;
        }
    }
    
    return left;
}

RoaringContainer* RoaringBitmap::getContainer(USHORT high_bits) const
{
    ULONG index = findContainerIndex(high_bits);
    if (index < m_containers.size() && m_containers[index].high == high_bits) {
        return m_containers[index].container;
    }
    return nullptr;
}

RoaringContainer* RoaringBitmap::getOrCreateContainer(USHORT high_bits)
{
    ULONG index = findContainerIndex(high_bits);
    
    if (index < m_containers.size() && m_containers[index].high == high_bits) {
        return m_containers[index].container;
    }
    
    // Create new array container
    RoaringContainer* new_container = new RoaringArrayContainer(m_pool);
    m_containers.insert(m_containers.begin() + index, HighLowPair(high_bits, new_container));
    
    return new_container;
}

ULONG RoaringBitmap::getMinValue() const
{
    if (m_containers.empty()) return 0;
    
    const auto& first_container = m_containers.front();
    USHORT iterator_state = 0;
    USHORT low_value;
    
    if (first_container.container->getNextValue(low_value, iterator_state)) {
        return combineHighLow(first_container.high, low_value);
    }
    
    return 0;
}

ULONG RoaringBitmap::getMaxValue() const
{
    if (m_containers.empty()) return 0;
    
    // Find the last value in the last container
    const auto& last_container = m_containers.back();
    USHORT max_low = 0;
    USHORT iterator_state = 0;
    USHORT current_value;
    
    while (last_container.container->getNextValue(current_value, iterator_state)) {
        max_low = current_value;
    }
    
    return combineHighLow(last_container.high, max_low);
}

//----------------------------
// Iterator Implementation
//----------------------------

RoaringBitmap::Iterator::Iterator(const RoaringBitmap* bitmap)
    : m_bitmap(bitmap), m_container_index(0), m_container_iterator(0), m_initialized(false)
{
}

bool RoaringBitmap::Iterator::hasNext() const
{
    if (!m_initialized) {
        const_cast<Iterator*>(this)->reset();
    }
    
    if (m_container_index >= m_bitmap->m_containers.size()) {
        return false;
    }
    
    // Check if current container has more values
    USHORT temp_iterator = m_container_iterator;
    USHORT temp_value;
    return m_bitmap->m_containers[m_container_index].container->getNextValue(temp_value, temp_iterator);
}

ULONG RoaringBitmap::Iterator::getNext()
{
    if (!m_initialized) {
        reset();
    }
    
    while (m_container_index < m_bitmap->m_containers.size()) {
        USHORT low_value;
        if (m_bitmap->m_containers[m_container_index].container->getNextValue(low_value, m_container_iterator)) {
            return RoaringBitmap::combineHighLow(m_bitmap->m_containers[m_container_index].high, low_value);
        }
        
        // Move to next container
        advanceToNextContainer();
    }
    
    return 0; // No more values
}

void RoaringBitmap::Iterator::reset()
{
    m_container_index = 0;
    m_container_iterator = 0;
    m_initialized = true;
    
    if (!m_bitmap->m_containers.empty()) {
        m_bitmap->m_containers[0].container->resetIterator(m_container_iterator);
    }
}

void RoaringBitmap::Iterator::advanceToNextContainer()
{
    m_container_index++;
    m_container_iterator = 0;
    
    if (m_container_index < m_bitmap->m_containers.size()) {
        m_bitmap->m_containers[m_container_index].container->resetIterator(m_container_iterator);
    }
}

RoaringBitmap::Iterator RoaringBitmap::getIterator() const
{
    return Iterator(this);
}

//----------------------------
// Factory Implementation
//----------------------------

RoaringBitmap* RoaringBitmapFactory::createBitmap(MemoryPool* pool)
{
    return new RoaringBitmap(pool);
}

RoaringBitmap* RoaringBitmapFactory::createFromValues(MemoryPool* pool, const ULONG* values, ULONG count)
{
    RoaringBitmap* bitmap = new RoaringBitmap(pool);
    
    for (ULONG i = 0; i < count; i++) {
        bitmap->add(values[i]);
    }
    
    bitmap->optimize();
    return bitmap;
}

bool RoaringBitmapFactory::shouldUseRoaringBitmap(ULONG max_value, ULONG cardinality, double sparsity)
{
    // Use Roaring for large sparse bitmaps
    return max_value > ROARING_BITMAP_THRESHOLD && sparsity < 0.1;
}

//----------------------------
// Enhanced Compressed Bitmap Implementation
//----------------------------

EnhancedCompressedBitmap::EnhancedCompressedBitmap(MemoryPool* pool)
    : m_pool(pool), m_using_roaring(false), m_roaring_bitmap(nullptr),
      m_traditional_bitmap(nullptr), m_traditional_size(0), m_bit_count(0),
      m_set_bit_count(0), m_max_bit_position(0), m_sparsity_ratio(1.0)
{
}

EnhancedCompressedBitmap::~EnhancedCompressedBitmap()
{
    if (m_roaring_bitmap) {
        delete m_roaring_bitmap;
    }
    if (m_traditional_bitmap && m_pool) {
        m_pool->deallocate(m_traditional_bitmap);
    }
}

void EnhancedCompressedBitmap::setBit(ULONG bit_position)
{
    if (shouldUseRoaring() && !m_using_roaring) {
        convertToRoaring();
    }
    
    if (m_using_roaring) {
        if (!m_roaring_bitmap->contains(bit_position)) {
            m_roaring_bitmap->add(bit_position);
            m_set_bit_count++;
        }
    } else {
        // Traditional bitmap implementation
        ULONG byte_index = bit_position / 8;
        UCHAR bit_mask = 1 << (bit_position % 8);
        
        if (byte_index >= m_traditional_size) {
            // Expand traditional bitmap
            ULONG new_size = byte_index + 1024; // Grow in chunks
            UCHAR* new_bitmap = static_cast<UCHAR*>(m_pool->allocate(new_size));
            memset(new_bitmap, 0, new_size);
            
            if (m_traditional_bitmap) {
                memcpy(new_bitmap, m_traditional_bitmap, m_traditional_size);
                m_pool->deallocate(m_traditional_bitmap);
            }
            
            m_traditional_bitmap = new_bitmap;
            m_traditional_size = new_size;
        }
        
        if (!(m_traditional_bitmap[byte_index] & bit_mask)) {
            m_traditional_bitmap[byte_index] |= bit_mask;
            m_set_bit_count++;
        }
    }
    
    if (bit_position >= m_bit_count) {
        m_bit_count = bit_position + 1;
    }
    
    m_max_bit_position = std::max(m_max_bit_position, bit_position);
    updateStatistics();
}

bool EnhancedCompressedBitmap::testBit(ULONG bit_position) const
{
    if (bit_position >= m_bit_count) {
        return false;
    }
    
    if (m_using_roaring) {
        return m_roaring_bitmap->contains(bit_position);
    } else {
        ULONG byte_index = bit_position / 8;
        if (byte_index >= m_traditional_size) {
            return false;
        }
        
        UCHAR bit_mask = 1 << (bit_position % 8);
        return (m_traditional_bitmap[byte_index] & bit_mask) != 0;
    }
}

void EnhancedCompressedBitmap::optimize()
{
    selectOptimalFormat();
    
    if (m_using_roaring && m_roaring_bitmap) {
        m_roaring_bitmap->optimize();
    }
}

bool EnhancedCompressedBitmap::shouldUseRoaring() const
{
    return RoaringBitmapFactory::shouldUseRoaringBitmap(m_max_bit_position, m_set_bit_count, m_sparsity_ratio);
}

void EnhancedCompressedBitmap::selectOptimalFormat()
{
    calculateSparsity();
    
    bool should_use_roaring = shouldUseRoaring();
    
    if (should_use_roaring && !m_using_roaring) {
        convertToRoaring();
    } else if (!should_use_roaring && m_using_roaring) {
        convertToTraditional();
    }
}

void EnhancedCompressedBitmap::convertToRoaring()
{
    if (m_using_roaring) return;
    
    m_roaring_bitmap = RoaringBitmapFactory::createBitmap(m_pool);
    
    if (m_traditional_bitmap) {
        // Convert traditional bitmap to roaring
        for (ULONG i = 0; i < m_bit_count; i++) {
            if (testBit(i)) {
                m_roaring_bitmap->add(i);
            }
        }
        
        m_pool->deallocate(m_traditional_bitmap);
        m_traditional_bitmap = nullptr;
        m_traditional_size = 0;
    }
    
    m_using_roaring = true;
}

void EnhancedCompressedBitmap::updateStatistics()
{
    calculateSparsity();
}

void EnhancedCompressedBitmap::calculateSparsity()
{
    if (m_bit_count > 0) {
        m_sparsity_ratio = 1.0 - (static_cast<double>(m_set_bit_count) / static_cast<double>(m_bit_count));
    } else {
        m_sparsity_ratio = 1.0;
    }
}

ULONG EnhancedCompressedBitmap::getSetBitCount() const
{
    return m_set_bit_count;
}

ULONG EnhancedCompressedBitmap::getTotalBitCount() const
{
    return m_bit_count;
}

ULONG EnhancedCompressedBitmap::getStorageSize() const
{
    if (m_using_roaring && m_roaring_bitmap) {
        return m_roaring_bitmap->getStorageSize();
    } else {
        return m_traditional_size;
    }
}

double EnhancedCompressedBitmap::getCompressionRatio() const
{
    if (m_using_roaring && m_roaring_bitmap) {
        return m_roaring_bitmap->getCompressionRatio();
    } else {
        if (m_bit_count == 0) return 1.0;
        ULONG uncompressed_size = (m_bit_count + 7) / 8;
        return static_cast<double>(uncompressed_size) / static_cast<double>(m_traditional_size);
    }
}

bool EnhancedCompressedBitmap::isEmpty() const
{
    return m_set_bit_count == 0;
}