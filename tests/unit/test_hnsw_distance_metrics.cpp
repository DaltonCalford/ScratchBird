#include <gtest/gtest.h>
#include "scratchbird/core/vector.h"
#include <cmath>

using namespace scratchbird::core;

/**
 * Test suite for HNSW distance metrics
 *
 * Verifies that all 4 distance metrics (Euclidean, Cosine, Manhattan, Dot Product)
 * work correctly for HNSW vector similarity search.
 */

class HNSWDistanceMetricsTest : public ::testing::Test
{
protected:
    // Helper to create test vectors (avoids default constructor issue)
    VectorValue createZero3D() const {
        return VectorValue(std::vector<float>{0.0f, 0.0f, 0.0f});
    }
    VectorValue createUnitX() const {
        return VectorValue(std::vector<float>{1.0f, 0.0f, 0.0f});
    }
    VectorValue createUnitY() const {
        return VectorValue(std::vector<float>{0.0f, 1.0f, 0.0f});
    }
    VectorValue createUnitZ() const {
        return VectorValue(std::vector<float>{0.0f, 0.0f, 1.0f});
    }
    VectorValue createDiagonal() const {
        return VectorValue(std::vector<float>{1.0f, 1.0f, 1.0f});
    }
    VectorValue createNegative() const {
        return VectorValue(std::vector<float>{-1.0f, -1.0f, -1.0f});
    }
    VectorValue createMixed() const {
        return VectorValue(std::vector<float>{1.0f, -1.0f, 2.0f});
    }

    // Helper to check approximate equality
    void expectNear(double actual, double expected, double epsilon = 1e-6)
    {
        EXPECT_NEAR(actual, expected, epsilon);
    }
};

// ========== EUCLIDEAN DISTANCE TESTS ==========

TEST_F(HNSWDistanceMetricsTest, EuclideanDistance_IdenticalVectors)
{
    VectorValue vec = createUnitX();
    auto result = vec.distance(vec, DistanceMetric::EUCLIDEAN);
    ASSERT_TRUE(result.has_value());
    expectNear(*result, 0.0);
}

TEST_F(HNSWDistanceMetricsTest, EuclideanDistance_OrthogonalVectors)
{
    // Distance between (1,0,0) and (0,1,0) is sqrt(2)
    auto result = createUnitX().distance(createUnitY(), DistanceMetric::EUCLIDEAN);
    ASSERT_TRUE(result.has_value());
    expectNear(*result, std::sqrt(2.0));
}

TEST_F(HNSWDistanceMetricsTest, EuclideanDistance_OppositeVectors)
{
    // Distance between (1,1,1) and (-1,-1,-1) is sqrt(12)
    auto result = createDiagonal().distance(createNegative(), DistanceMetric::EUCLIDEAN);
    ASSERT_TRUE(result.has_value());
    expectNear(*result, std::sqrt(12.0));
}

TEST_F(HNSWDistanceMetricsTest, EuclideanDistance_MixedVectors)
{
    VectorValue a(std::vector<float>{1.0f, 2.0f, 3.0f});
    VectorValue b(std::vector<float>{4.0f, 5.0f, 6.0f});
    // Distance = sqrt((1-4)^2 + (2-5)^2 + (3-6)^2) = sqrt(9+9+9) = sqrt(27)
    auto result = a.distance(b, DistanceMetric::EUCLIDEAN);
    ASSERT_TRUE(result.has_value());
    expectNear(*result, std::sqrt(27.0));
}

TEST_F(HNSWDistanceMetricsTest, EuclideanDistance_HighDimensional)
{
    // Test with 128-dimensional vectors (common for embeddings)
    std::vector<float> a_data(128, 1.0f);
    std::vector<float> b_data(128, 2.0f);
    VectorValue a(a_data);
    VectorValue b(b_data);

    // Distance = sqrt(128 * (1-2)^2) = sqrt(128) ≈ 11.31
    auto result = a.distance(b, DistanceMetric::EUCLIDEAN);
    ASSERT_TRUE(result.has_value());
    expectNear(*result, std::sqrt(128.0));
}

// ========== COSINE SIMILARITY TESTS ==========

TEST_F(HNSWDistanceMetricsTest, CosineSimilarity_IdenticalVectors)
{
    auto result = vec_diagonal.distance(vec_diagonal, DistanceMetric::COSINE);
    ASSERT_TRUE(result.has_value());
    expectNear(*result, 1.0);  // Cosine similarity of 1.0
}

TEST_F(HNSWDistanceMetricsTest, CosineSimilarity_OrthogonalVectors)
{
    // (1,0,0) and (0,1,0) are orthogonal, cosine similarity = 0
    auto result = vec_unit_x.distance(vec_unit_y, DistanceMetric::COSINE);
    ASSERT_TRUE(result.has_value());
    expectNear(*result, 0.0);
}

TEST_F(HNSWDistanceMetricsTest, CosineSimilarity_OppositeVectors)
{
    // (1,1,1) and (-1,-1,-1) point in opposite directions, cosine = -1
    auto result = vec_diagonal.distance(vec_negative, DistanceMetric::COSINE);
    ASSERT_TRUE(result.has_value());
    expectNear(*result, -1.0);
}

TEST_F(HNSWDistanceMetricsTest, CosineSimilarity_ParallelVectors)
{
    VectorValue a(std::vector<float>{1.0f, 2.0f, 3.0f});
    VectorValue b(std::vector<float>{2.0f, 4.0f, 6.0f});  // b = 2*a
    // Parallel vectors have cosine similarity = 1.0
    auto result = a.distance(b, DistanceMetric::COSINE);
    ASSERT_TRUE(result.has_value());
    expectNear(*result, 1.0);
}

TEST_F(HNSWDistanceMetricsTest, CosineSimilarity_ZeroVector)
{
    // Cosine with zero vector should return 0.0 (undefined, but handled)
    auto result = vec_unit_x.distance(vec_zero_3d, DistanceMetric::COSINE);
    ASSERT_TRUE(result.has_value());
    expectNear(*result, 0.0);
}

TEST_F(HNSWDistanceMetricsTest, CosineSimilarity_HighDimensional)
{
    // Test with 128-dimensional vectors
    std::vector<float> a_data(128);
    std::vector<float> b_data(128);
    for (size_t i = 0; i < 128; ++i) {
        a_data[i] = static_cast<float>(i);
        b_data[i] = static_cast<float>(i * 2);  // b = 2*a
    }
    VectorValue a(a_data);
    VectorValue b(b_data);

    // Parallel vectors, cosine = 1.0
    auto result = a.distance(b, DistanceMetric::COSINE);
    ASSERT_TRUE(result.has_value());
    expectNear(*result, 1.0);
}

// ========== MANHATTAN DISTANCE TESTS ==========

TEST_F(HNSWDistanceMetricsTest, ManhattanDistance_IdenticalVectors)
{
    auto result = vec_unit_x.distance(vec_unit_x, DistanceMetric::MANHATTAN);
    ASSERT_TRUE(result.has_value());
    expectNear(*result, 0.0);
}

TEST_F(HNSWDistanceMetricsTest, ManhattanDistance_OrthogonalVectors)
{
    // Manhattan distance between (1,0,0) and (0,1,0) is |1-0| + |0-1| + |0-0| = 2
    auto result = vec_unit_x.distance(vec_unit_y, DistanceMetric::MANHATTAN);
    ASSERT_TRUE(result.has_value());
    expectNear(*result, 2.0);
}

TEST_F(HNSWDistanceMetricsTest, ManhattanDistance_OppositeVectors)
{
    // Manhattan distance between (1,1,1) and (-1,-1,-1) is |1-(-1)| * 3 = 6
    auto result = vec_diagonal.distance(vec_negative, DistanceMetric::MANHATTAN);
    ASSERT_TRUE(result.has_value());
    expectNear(*result, 6.0);
}

TEST_F(HNSWDistanceMetricsTest, ManhattanDistance_MixedVectors)
{
    VectorValue a(std::vector<float>{1.0f, 2.0f, 3.0f});
    VectorValue b(std::vector<float>{4.0f, 5.0f, 6.0f});
    // Manhattan distance = |1-4| + |2-5| + |3-6| = 3 + 3 + 3 = 9
    auto result = a.distance(b, DistanceMetric::MANHATTAN);
    ASSERT_TRUE(result.has_value());
    expectNear(*result, 9.0);
}

TEST_F(HNSWDistanceMetricsTest, ManhattanDistance_NegativeComponents)
{
    VectorValue a(std::vector<float>{-1.0f, -2.0f, -3.0f});
    VectorValue b(std::vector<float>{1.0f, 2.0f, 3.0f});
    // Manhattan distance = |(-1)-1| + |(-2)-2| + |(-3)-3| = 2+4+6 = 12
    auto result = a.distance(b, DistanceMetric::MANHATTAN);
    ASSERT_TRUE(result.has_value());
    expectNear(*result, 12.0);
}

TEST_F(HNSWDistanceMetricsTest, ManhattanDistance_HighDimensional)
{
    // Test with 128-dimensional vectors
    std::vector<float> a_data(128, 1.0f);
    std::vector<float> b_data(128, 2.0f);
    VectorValue a(a_data);
    VectorValue b(b_data);

    // Manhattan distance = 128 * |1-2| = 128
    auto result = a.distance(b, DistanceMetric::MANHATTAN);
    ASSERT_TRUE(result.has_value());
    expectNear(*result, 128.0);
}

// ========== DOT PRODUCT TESTS ==========

TEST_F(HNSWDistanceMetricsTest, DotProduct_IdenticalVectors)
{
    // Dot product of (1,1,1) with itself = 1*1 + 1*1 + 1*1 = 3
    auto result = vec_diagonal.distance(vec_diagonal, DistanceMetric::DOT_PRODUCT);
    ASSERT_TRUE(result.has_value());
    expectNear(*result, 3.0);
}

TEST_F(HNSWDistanceMetricsTest, DotProduct_OrthogonalVectors)
{
    // Dot product of orthogonal vectors = 0
    auto result = vec_unit_x.distance(vec_unit_y, DistanceMetric::DOT_PRODUCT);
    ASSERT_TRUE(result.has_value());
    expectNear(*result, 0.0);
}

TEST_F(HNSWDistanceMetricsTest, DotProduct_OppositeVectors)
{
    // Dot product of (1,1,1) and (-1,-1,-1) = -3
    auto result = vec_diagonal.distance(vec_negative, DistanceMetric::DOT_PRODUCT);
    ASSERT_TRUE(result.has_value());
    expectNear(*result, -3.0);
}

TEST_F(HNSWDistanceMetricsTest, DotProduct_MixedVectors)
{
    VectorValue a(std::vector<float>{1.0f, 2.0f, 3.0f});
    VectorValue b(std::vector<float>{4.0f, 5.0f, 6.0f});
    // Dot product = 1*4 + 2*5 + 3*6 = 4 + 10 + 18 = 32
    auto result = a.distance(b, DistanceMetric::DOT_PRODUCT);
    ASSERT_TRUE(result.has_value());
    expectNear(*result, 32.0);
}

TEST_F(HNSWDistanceMetricsTest, DotProduct_ZeroVector)
{
    // Dot product with zero vector = 0
    auto result = vec_unit_x.distance(vec_zero_3d, DistanceMetric::DOT_PRODUCT);
    ASSERT_TRUE(result.has_value());
    expectNear(*result, 0.0);
}

TEST_F(HNSWDistanceMetricsTest, DotProduct_HighDimensional)
{
    // Test with 128-dimensional vectors
    std::vector<float> a_data(128, 1.0f);
    std::vector<float> b_data(128, 2.0f);
    VectorValue a(a_data);
    VectorValue b(b_data);

    // Dot product = 128 * (1 * 2) = 256
    auto result = a.distance(b, DistanceMetric::DOT_PRODUCT);
    ASSERT_TRUE(result.has_value());
    expectNear(*result, 256.0);
}

// ========== DIMENSION MISMATCH TESTS ==========

TEST_F(HNSWDistanceMetricsTest, DimensionMismatch_AllMetrics)
{
    VectorValue vec_2d(std::vector<float>{1.0f, 2.0f});
    VectorValue vec_3d(std::vector<float>{1.0f, 2.0f, 3.0f});

    // All metrics should return nullopt for mismatched dimensions
    EXPECT_FALSE(vec_2d.distance(vec_3d, DistanceMetric::EUCLIDEAN).has_value());
    EXPECT_FALSE(vec_2d.distance(vec_3d, DistanceMetric::COSINE).has_value());
    EXPECT_FALSE(vec_2d.distance(vec_3d, DistanceMetric::MANHATTAN).has_value());
    EXPECT_FALSE(vec_2d.distance(vec_3d, DistanceMetric::DOT_PRODUCT).has_value());
}

// ========== FLOAT64 TYPE TESTS ==========

TEST_F(HNSWDistanceMetricsTest, Float64_AllMetrics)
{
    VectorValue a(std::vector<double>{1.0, 2.0, 3.0});
    VectorValue b(std::vector<double>{4.0, 5.0, 6.0});

    // Euclidean
    auto euclidean = a.distance(b, DistanceMetric::EUCLIDEAN);
    ASSERT_TRUE(euclidean.has_value());
    expectNear(*euclidean, std::sqrt(27.0));

    // Cosine
    auto cosine = a.distance(b, DistanceMetric::COSINE);
    ASSERT_TRUE(cosine.has_value());
    // dot = 32, ||a|| = sqrt(14), ||b|| = sqrt(77)
    double expected_cosine = 32.0 / (std::sqrt(14.0) * std::sqrt(77.0));
    expectNear(*cosine, expected_cosine);

    // Manhattan
    auto manhattan = a.distance(b, DistanceMetric::MANHATTAN);
    ASSERT_TRUE(manhattan.has_value());
    expectNear(*manhattan, 9.0);

    // Dot product
    auto dot = a.distance(b, DistanceMetric::DOT_PRODUCT);
    ASSERT_TRUE(dot.has_value());
    expectNear(*dot, 32.0);
}

// ========== REAL-WORLD EMBEDDING TESTS ==========

TEST_F(HNSWDistanceMetricsTest, RealWorld_TextEmbeddings)
{
    // Simulate OpenAI text embeddings (typically 1536 dimensions, normalized)
    std::vector<float> doc1(1536);
    std::vector<float> doc2(1536);
    std::vector<float> doc3(1536);

    // Doc1 and Doc2 are similar (small perturbation)
    for (size_t i = 0; i < 1536; ++i) {
        doc1[i] = static_cast<float>(std::sin(i * 0.01));
        doc2[i] = static_cast<float>(std::sin(i * 0.01) + 0.1);
        doc3[i] = static_cast<float>(std::cos(i * 0.01));  // Different pattern
    }

    VectorValue v1(doc1);
    VectorValue v2(doc2);
    VectorValue v3(doc3);

    // Cosine similarity: v1 should be closer to v2 than to v3
    auto sim_1_2 = v1.distance(v2, DistanceMetric::COSINE);
    auto sim_1_3 = v1.distance(v3, DistanceMetric::COSINE);

    ASSERT_TRUE(sim_1_2.has_value());
    ASSERT_TRUE(sim_1_3.has_value());

    // Higher cosine similarity means more similar
    EXPECT_GT(*sim_1_2, *sim_1_3);
}

TEST_F(HNSWDistanceMetricsTest, RealWorld_ImageEmbeddings)
{
    // Simulate CLIP image embeddings (typically 512 dimensions)
    std::vector<float> image1(512);
    std::vector<float> image2(512);

    // Similar images have high cosine similarity
    for (size_t i = 0; i < 512; ++i) {
        image1[i] = static_cast<float>(i) / 512.0f;
        image2[i] = static_cast<float>(i) / 512.0f * 1.1f;  // 10% variation
    }

    VectorValue v1(image1);
    VectorValue v2(image2);

    auto cosine = v1.distance(v2, DistanceMetric::COSINE);
    ASSERT_TRUE(cosine.has_value());

    // Should be very similar (cosine > 0.99)
    EXPECT_GT(*cosine, 0.99);
}

// ========== PERFORMANCE SANITY TESTS ==========

TEST_F(HNSWDistanceMetricsTest, Performance_1000Vectors)
{
    // Create 1000 random 128-dimensional vectors
    std::vector<VectorValue> vectors;
    for (int i = 0; i < 1000; ++i) {
        std::vector<float> data(128);
        for (size_t j = 0; j < 128; ++j) {
            data[j] = static_cast<float>(std::sin(i * j * 0.001));
        }
        vectors.emplace_back(data);
    }

    VectorValue query(std::vector<float>(128, 1.0f));

    // Compute distance to all 1000 vectors (should complete quickly)
    for (const auto& vec : vectors) {
        auto euclidean = query.distance(vec, DistanceMetric::EUCLIDEAN);
        auto cosine = query.distance(vec, DistanceMetric::COSINE);
        auto manhattan = query.distance(vec, DistanceMetric::MANHATTAN);
        auto dot = query.distance(vec, DistanceMetric::DOT_PRODUCT);

        ASSERT_TRUE(euclidean.has_value());
        ASSERT_TRUE(cosine.has_value());
        ASSERT_TRUE(manhattan.has_value());
        ASSERT_TRUE(dot.has_value());
    }

    // If we got here without timeout, performance is acceptable
    SUCCEED();
}
