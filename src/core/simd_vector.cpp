// =================================================================================================
// ScratchBird Database Engine
// Copyright (C) 2025 ScratchBird Development Team
// =================================================================================================
//
// P3-6: SIMD Vector Operations Implementation
//
// November 25, 2025

#include "scratchbird/core/simd_vector.h"
#include <algorithm>

#if defined(__x86_64__) || defined(_M_X64)
#include <cpuid.h>
#endif

namespace scratchbird::core {

// ============================================================================
// SIMD Capability Detection
// ============================================================================

SimdCapability detectSimdCapabilities() {
    uint32_t result = 0;

#if defined(__x86_64__) || defined(_M_X64)
    unsigned int eax, ebx, ecx, edx;

    // Check for SSE4.1
    if (__get_cpuid(1, &eax, &ebx, &ecx, &edx)) {
        if (ecx & (1 << 19)) {  // SSE4.1
            result |= static_cast<uint32_t>(SimdCapability::SSE4);
        }
        if (ecx & (1 << 12)) {  // FMA
            result |= static_cast<uint32_t>(SimdCapability::FMA);
        }
    }

    // Check for AVX2 and AVX-512
    if (__get_cpuid_count(7, 0, &eax, &ebx, &ecx, &edx)) {
        if (ebx & (1 << 5)) {  // AVX2
            result |= static_cast<uint32_t>(SimdCapability::AVX2);
        }
        if (ebx & (1 << 16)) {  // AVX-512F
            result |= static_cast<uint32_t>(SimdCapability::AVX512);
        }
    }
#elif defined(__aarch64__)
    // ARM NEON is always available on AArch64
    result |= static_cast<uint32_t>(SimdCapability::NEON);
#endif

    return static_cast<SimdCapability>(result);
}

const char* simdCapabilityName(SimdCapability cap) {
    switch (cap) {
        case SimdCapability::NONE: return "None";
        case SimdCapability::SSE4: return "SSE4";
        case SimdCapability::AVX2: return "AVX2";
        case SimdCapability::AVX512: return "AVX-512";
        case SimdCapability::NEON: return "NEON";
        case SimdCapability::FMA: return "FMA";
        default: return "Unknown";
    }
}

// ============================================================================
// Function Pointer Initialization
// ============================================================================

// Default function pointers (scalar fallback)
float (*simd_dot_product)(const float*, const float*, size_t) = scalar::dotProduct;
float (*simd_l2_distance)(const float*, const float*, size_t) = scalar::l2Distance;
float (*simd_l2_distance_squared)(const float*, const float*, size_t) = scalar::l2DistanceSquared;
float (*simd_l1_distance)(const float*, const float*, size_t) = scalar::l1Distance;
float (*simd_cosine_similarity)(const float*, const float*, size_t) = scalar::cosineSimilarity;

// ============================================================================
// SimdVectorOps Implementation
// ============================================================================

SimdVectorOps::SimdVectorOps() {
    capability_ = detectSimdCapabilities();

    // Set function pointers based on detected capabilities
#ifdef SIMD_AVX512
    if (static_cast<uint32_t>(capability_) & static_cast<uint32_t>(SimdCapability::AVX512)) {
        simd_dot_product = avx512::dotProduct;
        simd_l2_distance = avx512::l2Distance;
        simd_l2_distance_squared = avx512::l2DistanceSquared;
        simd_l1_distance = avx512::l1Distance;
        simd_cosine_similarity = avx512::cosineSimilarity;
    } else
#endif
#ifdef SIMD_AVX2
    if (static_cast<uint32_t>(capability_) & static_cast<uint32_t>(SimdCapability::AVX2)) {
        simd_dot_product = avx2::dotProduct;
        simd_l2_distance = avx2::l2Distance;
        simd_l2_distance_squared = avx2::l2DistanceSquared;
        simd_l1_distance = avx2::l1Distance;
        simd_cosine_similarity = avx2::cosineSimilarity;
    } else
#endif
    {
        // Keep scalar defaults
    }
}

SimdVectorOps& SimdVectorOps::getInstance() {
    static SimdVectorOps instance;
    return instance;
}

float SimdVectorOps::dotProduct(const float* a, const float* b, size_t dim) const {
    return simd_dot_product(a, b, dim);
}

float SimdVectorOps::l2Distance(const float* a, const float* b, size_t dim) const {
    return simd_l2_distance(a, b, dim);
}

float SimdVectorOps::l2DistanceSquared(const float* a, const float* b, size_t dim) const {
    return simd_l2_distance_squared(a, b, dim);
}

float SimdVectorOps::l1Distance(const float* a, const float* b, size_t dim) const {
    return simd_l1_distance(a, b, dim);
}

float SimdVectorOps::cosineSimilarity(const float* a, const float* b, size_t dim) const {
    return simd_cosine_similarity(a, b, dim);
}

float SimdVectorOps::magnitude(const float* v, size_t dim) const {
#ifdef SIMD_AVX512
    if (static_cast<uint32_t>(capability_) & static_cast<uint32_t>(SimdCapability::AVX512)) {
        return avx512::magnitude(v, dim);
    }
#endif
#ifdef SIMD_AVX2
    if (static_cast<uint32_t>(capability_) & static_cast<uint32_t>(SimdCapability::AVX2)) {
        return avx2::magnitude(v, dim);
    }
#endif
    return scalar::magnitude(v, dim);
}

void SimdVectorOps::dotProductBatch(const float* query, const float* const* candidates,
                                    size_t num_candidates, size_t dim, float* results) const {
    for (size_t i = 0; i < num_candidates; ++i) {
        results[i] = dotProduct(query, candidates[i], dim);
    }
}

void SimdVectorOps::l2DistanceBatch(const float* query, const float* const* candidates,
                                    size_t num_candidates, size_t dim, float* results) const {
    for (size_t i = 0; i < num_candidates; ++i) {
        results[i] = l2Distance(query, candidates[i], dim);
    }
}

void SimdVectorOps::normalize(float* v, size_t dim) const {
    float mag = magnitude(v, dim);
    if (mag > 0.0f) {
        scale(v, 1.0f / mag, dim);
    }
}

void SimdVectorOps::add(const float* a, const float* b, float* result, size_t dim) const {
#ifdef SIMD_AVX2
    if (static_cast<uint32_t>(capability_) & static_cast<uint32_t>(SimdCapability::AVX2)) {
        size_t i = 0;
        for (; i + 8 <= dim; i += 8) {
            __m256 va = _mm256_loadu_ps(&a[i]);
            __m256 vb = _mm256_loadu_ps(&b[i]);
            __m256 vr = _mm256_add_ps(va, vb);
            _mm256_storeu_ps(&result[i], vr);
        }
        for (; i < dim; ++i) {
            result[i] = a[i] + b[i];
        }
        return;
    }
#endif
    // Scalar fallback
    for (size_t i = 0; i < dim; ++i) {
        result[i] = a[i] + b[i];
    }
}

void SimdVectorOps::scale(float* v, float scalar, size_t dim) const {
#ifdef SIMD_AVX2
    if (static_cast<uint32_t>(capability_) & static_cast<uint32_t>(SimdCapability::AVX2)) {
        __m256 vs = _mm256_set1_ps(scalar);
        size_t i = 0;
        for (; i + 8 <= dim; i += 8) {
            __m256 vv = _mm256_loadu_ps(&v[i]);
            __m256 vr = _mm256_mul_ps(vv, vs);
            _mm256_storeu_ps(&v[i], vr);
        }
        for (; i < dim; ++i) {
            v[i] *= scalar;
        }
        return;
    }
#endif
    // Scalar fallback
    for (size_t i = 0; i < dim; ++i) {
        v[i] *= scalar;
    }
}

} // namespace scratchbird::core
