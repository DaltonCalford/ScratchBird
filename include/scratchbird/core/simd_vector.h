/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
// =================================================================================================
// ScratchBird Database Engine
// Copyright (C) 2025 ScratchBird Development Team
// =================================================================================================
//
// P3-6: SIMD Vector Operations
//
// High-performance vector operations using AVX2/AVX-512 SIMD instructions.
// Provides 4-8x faster performance for vector similarity queries.
//
// Supports:
// - Dot product
// - L2 (Euclidean) distance
// - Cosine similarity
// - L1 (Manhattan) distance
// - Inner product
//
// November 25, 2025

#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>
#include <cmath>

// SIMD headers (conditionally included based on platform)
#if defined(__x86_64__) || defined(_M_X64)
    #ifdef __AVX512F__
        #include <immintrin.h>
        #define SIMD_AVX512 1
    #elif defined(__AVX2__)
        #include <immintrin.h>
        #define SIMD_AVX2 1
    #elif defined(__SSE4_1__)
        #include <smmintrin.h>
        #define SIMD_SSE4 1
    #endif
#elif defined(__aarch64__)
    #include <arm_neon.h>
    #define SIMD_NEON 1
#endif

namespace scratchbird::core {

// SIMD capability flags
enum class SimdCapability : uint32_t {
    NONE = 0,
    SSE4 = 1 << 0,
    AVX2 = 1 << 1,
    AVX512 = 1 << 2,
    NEON = 1 << 3,
    FMA = 1 << 4,
};

// Detect available SIMD capabilities at runtime
SimdCapability detectSimdCapabilities();

// Get string description of SIMD capabilities
const char* simdCapabilityName(SimdCapability cap);

// ============================================================================
// Scalar Fallback Implementations
// ============================================================================

namespace scalar {

// Dot product (scalar fallback)
inline float dotProduct(const float* a, const float* b, size_t dim) {
    float sum = 0.0f;
    for (size_t i = 0; i < dim; ++i) {
        sum += a[i] * b[i];
    }
    return sum;
}

// L2 (Euclidean) distance squared
inline float l2DistanceSquared(const float* a, const float* b, size_t dim) {
    float sum = 0.0f;
    for (size_t i = 0; i < dim; ++i) {
        float diff = a[i] - b[i];
        sum += diff * diff;
    }
    return sum;
}

// L2 (Euclidean) distance
inline float l2Distance(const float* a, const float* b, size_t dim) {
    return std::sqrt(l2DistanceSquared(a, b, dim));
}

// L1 (Manhattan) distance
inline float l1Distance(const float* a, const float* b, size_t dim) {
    float sum = 0.0f;
    for (size_t i = 0; i < dim; ++i) {
        sum += std::fabs(a[i] - b[i]);
    }
    return sum;
}

// Vector magnitude (L2 norm)
inline float magnitude(const float* v, size_t dim) {
    float sum = 0.0f;
    for (size_t i = 0; i < dim; ++i) {
        sum += v[i] * v[i];
    }
    return std::sqrt(sum);
}

// Cosine similarity
inline float cosineSimilarity(const float* a, const float* b, size_t dim) {
    float dot = dotProduct(a, b, dim);
    float mag_a = magnitude(a, dim);
    float mag_b = magnitude(b, dim);
    if (mag_a == 0.0f || mag_b == 0.0f) return 0.0f;
    return dot / (mag_a * mag_b);
}

} // namespace scalar

// ============================================================================
// AVX2 Implementations
// ============================================================================

#ifdef SIMD_AVX2
namespace avx2 {

// Dot product using AVX2
inline float dotProduct(const float* a, const float* b, size_t dim) {
    __m256 sum = _mm256_setzero_ps();
    size_t i = 0;

    // Process 8 floats at a time
    for (; i + 8 <= dim; i += 8) {
        __m256 va = _mm256_loadu_ps(&a[i]);
        __m256 vb = _mm256_loadu_ps(&b[i]);
        #ifdef __FMA__
        sum = _mm256_fmadd_ps(va, vb, sum);
        #else
        sum = _mm256_add_ps(sum, _mm256_mul_ps(va, vb));
        #endif
    }

    // Horizontal sum
    __m128 hi = _mm256_extractf128_ps(sum, 1);
    __m128 lo = _mm256_castps256_ps128(sum);
    __m128 sum128 = _mm_add_ps(lo, hi);
    sum128 = _mm_hadd_ps(sum128, sum128);
    sum128 = _mm_hadd_ps(sum128, sum128);
    float result = _mm_cvtss_f32(sum128);

    // Handle remaining elements
    for (; i < dim; ++i) {
        result += a[i] * b[i];
    }

    return result;
}

// L2 distance squared using AVX2
inline float l2DistanceSquared(const float* a, const float* b, size_t dim) {
    __m256 sum = _mm256_setzero_ps();
    size_t i = 0;

    for (; i + 8 <= dim; i += 8) {
        __m256 va = _mm256_loadu_ps(&a[i]);
        __m256 vb = _mm256_loadu_ps(&b[i]);
        __m256 diff = _mm256_sub_ps(va, vb);
        #ifdef __FMA__
        sum = _mm256_fmadd_ps(diff, diff, sum);
        #else
        sum = _mm256_add_ps(sum, _mm256_mul_ps(diff, diff));
        #endif
    }

    // Horizontal sum
    __m128 hi = _mm256_extractf128_ps(sum, 1);
    __m128 lo = _mm256_castps256_ps128(sum);
    __m128 sum128 = _mm_add_ps(lo, hi);
    sum128 = _mm_hadd_ps(sum128, sum128);
    sum128 = _mm_hadd_ps(sum128, sum128);
    float result = _mm_cvtss_f32(sum128);

    // Handle remaining elements
    for (; i < dim; ++i) {
        float diff = a[i] - b[i];
        result += diff * diff;
    }

    return result;
}

inline float l2Distance(const float* a, const float* b, size_t dim) {
    return std::sqrt(l2DistanceSquared(a, b, dim));
}

// L1 distance using AVX2
inline float l1Distance(const float* a, const float* b, size_t dim) {
    __m256 sum = _mm256_setzero_ps();
    __m256 sign_mask = _mm256_set1_ps(-0.0f);
    size_t i = 0;

    for (; i + 8 <= dim; i += 8) {
        __m256 va = _mm256_loadu_ps(&a[i]);
        __m256 vb = _mm256_loadu_ps(&b[i]);
        __m256 diff = _mm256_sub_ps(va, vb);
        __m256 abs_diff = _mm256_andnot_ps(sign_mask, diff);
        sum = _mm256_add_ps(sum, abs_diff);
    }

    // Horizontal sum
    __m128 hi = _mm256_extractf128_ps(sum, 1);
    __m128 lo = _mm256_castps256_ps128(sum);
    __m128 sum128 = _mm_add_ps(lo, hi);
    sum128 = _mm_hadd_ps(sum128, sum128);
    sum128 = _mm_hadd_ps(sum128, sum128);
    float result = _mm_cvtss_f32(sum128);

    // Handle remaining elements
    for (; i < dim; ++i) {
        result += std::fabs(a[i] - b[i]);
    }

    return result;
}

// Magnitude using AVX2
inline float magnitude(const float* v, size_t dim) {
    __m256 sum = _mm256_setzero_ps();
    size_t i = 0;

    for (; i + 8 <= dim; i += 8) {
        __m256 vv = _mm256_loadu_ps(&v[i]);
        #ifdef __FMA__
        sum = _mm256_fmadd_ps(vv, vv, sum);
        #else
        sum = _mm256_add_ps(sum, _mm256_mul_ps(vv, vv));
        #endif
    }

    // Horizontal sum
    __m128 hi = _mm256_extractf128_ps(sum, 1);
    __m128 lo = _mm256_castps256_ps128(sum);
    __m128 sum128 = _mm_add_ps(lo, hi);
    sum128 = _mm_hadd_ps(sum128, sum128);
    sum128 = _mm_hadd_ps(sum128, sum128);
    float result = _mm_cvtss_f32(sum128);

    // Handle remaining elements
    for (; i < dim; ++i) {
        result += v[i] * v[i];
    }

    return std::sqrt(result);
}

inline float cosineSimilarity(const float* a, const float* b, size_t dim) {
    float dot = dotProduct(a, b, dim);
    float mag_a = magnitude(a, dim);
    float mag_b = magnitude(b, dim);
    if (mag_a == 0.0f || mag_b == 0.0f) return 0.0f;
    return dot / (mag_a * mag_b);
}

} // namespace avx2
#endif

// ============================================================================
// AVX-512 Implementations
// ============================================================================

#ifdef SIMD_AVX512
namespace avx512 {

// Dot product using AVX-512
inline float dotProduct(const float* a, const float* b, size_t dim) {
    __m512 sum = _mm512_setzero_ps();
    size_t i = 0;

    // Process 16 floats at a time
    for (; i + 16 <= dim; i += 16) {
        __m512 va = _mm512_loadu_ps(&a[i]);
        __m512 vb = _mm512_loadu_ps(&b[i]);
        sum = _mm512_fmadd_ps(va, vb, sum);
    }

    float result = _mm512_reduce_add_ps(sum);

    // Handle remaining elements
    for (; i < dim; ++i) {
        result += a[i] * b[i];
    }

    return result;
}

// L2 distance squared using AVX-512
inline float l2DistanceSquared(const float* a, const float* b, size_t dim) {
    __m512 sum = _mm512_setzero_ps();
    size_t i = 0;

    for (; i + 16 <= dim; i += 16) {
        __m512 va = _mm512_loadu_ps(&a[i]);
        __m512 vb = _mm512_loadu_ps(&b[i]);
        __m512 diff = _mm512_sub_ps(va, vb);
        sum = _mm512_fmadd_ps(diff, diff, sum);
    }

    float result = _mm512_reduce_add_ps(sum);

    // Handle remaining elements
    for (; i < dim; ++i) {
        float diff = a[i] - b[i];
        result += diff * diff;
    }

    return result;
}

inline float l2Distance(const float* a, const float* b, size_t dim) {
    return std::sqrt(l2DistanceSquared(a, b, dim));
}

// L1 distance using AVX-512
inline float l1Distance(const float* a, const float* b, size_t dim) {
    __m512 sum = _mm512_setzero_ps();
    size_t i = 0;

    for (; i + 16 <= dim; i += 16) {
        __m512 va = _mm512_loadu_ps(&a[i]);
        __m512 vb = _mm512_loadu_ps(&b[i]);
        __m512 diff = _mm512_sub_ps(va, vb);
        __m512 abs_diff = _mm512_abs_ps(diff);
        sum = _mm512_add_ps(sum, abs_diff);
    }

    float result = _mm512_reduce_add_ps(sum);

    // Handle remaining elements
    for (; i < dim; ++i) {
        result += std::fabs(a[i] - b[i]);
    }

    return result;
}

inline float magnitude(const float* v, size_t dim) {
    __m512 sum = _mm512_setzero_ps();
    size_t i = 0;

    for (; i + 16 <= dim; i += 16) {
        __m512 vv = _mm512_loadu_ps(&v[i]);
        sum = _mm512_fmadd_ps(vv, vv, sum);
    }

    float result = _mm512_reduce_add_ps(sum);

    // Handle remaining elements
    for (; i < dim; ++i) {
        result += v[i] * v[i];
    }

    return std::sqrt(result);
}

inline float cosineSimilarity(const float* a, const float* b, size_t dim) {
    float dot = dotProduct(a, b, dim);
    float mag_a = magnitude(a, dim);
    float mag_b = magnitude(b, dim);
    if (mag_a == 0.0f || mag_b == 0.0f) return 0.0f;
    return dot / (mag_a * mag_b);
}

} // namespace avx512
#endif

// ============================================================================
// Unified API with Runtime Dispatch
// ============================================================================

class SimdVectorOps {
public:
    // Get singleton instance
    static SimdVectorOps& getInstance();

    // Get detected SIMD capability
    SimdCapability capability() const { return capability_; }

    // Dot product
    float dotProduct(const float* a, const float* b, size_t dim) const;

    // L2 distance
    float l2Distance(const float* a, const float* b, size_t dim) const;
    float l2DistanceSquared(const float* a, const float* b, size_t dim) const;

    // L1 distance
    float l1Distance(const float* a, const float* b, size_t dim) const;

    // Cosine similarity
    float cosineSimilarity(const float* a, const float* b, size_t dim) const;

    // Magnitude
    float magnitude(const float* v, size_t dim) const;

    // Batch operations for k-NN
    void dotProductBatch(const float* query, const float* const* candidates,
                        size_t num_candidates, size_t dim, float* results) const;

    void l2DistanceBatch(const float* query, const float* const* candidates,
                        size_t num_candidates, size_t dim, float* results) const;

    // Vector normalization
    void normalize(float* v, size_t dim) const;

    // Vector addition
    void add(const float* a, const float* b, float* result, size_t dim) const;

    // Scalar multiplication
    void scale(float* v, float scalar, size_t dim) const;

private:
    SimdVectorOps();
    SimdCapability capability_;
};

// Convenience function pointers (set at initialization based on CPU capabilities)
extern float (*simd_dot_product)(const float*, const float*, size_t);
extern float (*simd_l2_distance)(const float*, const float*, size_t);
extern float (*simd_l2_distance_squared)(const float*, const float*, size_t);
extern float (*simd_l1_distance)(const float*, const float*, size_t);
extern float (*simd_cosine_similarity)(const float*, const float*, size_t);

} // namespace scratchbird::core
