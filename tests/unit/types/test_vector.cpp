/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include "scratchbird/core/vector.h"
#include <iostream>
#include "gtest/gtest.h"
#include <cmath>

using namespace scratchbird::core;

// Helper to compare floats with tolerance
bool floatEqual(double a, double b, double epsilon = 1e-6) {
    return std::abs(a - b) < epsilon;
}


TEST(VectorTest, Comprehensive) {

    std::cout << "Testing VECTOR Implementation...\n\n";

    // Test 1: Float32 vector creation
    std::cout << "Test 1: Float32 vector creation\n";
    {
        std::vector<float> data = {1.0f, 2.0f, 3.0f};
        auto vec = Vector::fromFloat32(data);

        ASSERT_EQ(vec.getType(), VectorType::FLOAT32);
        ASSERT_EQ(vec.getDimensions(), 3);
        ASSERT_EQ(vec.getFloat32(0).value(), 1.0f);
        ASSERT_EQ(vec.getFloat32(1).value(), 2.0f);
        ASSERT_EQ(vec.getFloat32(2).value(), 3.0f);
        std::cout << "  Float32 vector: " << vec.toString() << " ✓\n";
    }
    std::cout << "  ✓ Float32 vector creation passed\n\n";

    // Test 2: Float64 vector creation
    std::cout << "Test 2: Float64 vector creation\n";
    {
        std::vector<double> data = {1.5, 2.5, 3.5, 4.5};
        auto vec = Vector::fromFloat64(data);

        ASSERT_EQ(vec.getType(), VectorType::FLOAT64);
        ASSERT_EQ(vec.getDimensions(), 4);
        ASSERT_EQ(vec.getFloat64(0).value(), 1.5);
        ASSERT_EQ(vec.getFloat64(1).value(), 2.5);
        ASSERT_EQ(vec.getFloat64(2).value(), 3.5);
        ASSERT_EQ(vec.getFloat64(3).value(), 4.5);
        std::cout << "  Float64 vector: " << vec.toString() << " ✓\n";
    }
    std::cout << "  ✓ Float64 vector creation passed\n\n";

    // Test 3: Parse from string (Float32)
    std::cout << "Test 3: Parse from string\n";
    {
        auto vec = Vector::parse("[1.0, 2.0, 3.0]", VectorType::FLOAT32);
        ASSERT_TRUE(vec.has_value());
        ASSERT_EQ(vec->getDimensions(), 3);
        ASSERT_TRUE(floatEqual(vec->getAsFloat64(0).value(), 1.0));
        ASSERT_TRUE(floatEqual(vec->getAsFloat64(1).value(), 2.0));
        ASSERT_TRUE(floatEqual(vec->getAsFloat64(2).value(), 3.0));
        std::cout << "  Parsed: " << vec->toString() << " ✓\n";

        // With spaces
        auto vec2 = Vector::parse("[ 4.5 , 5.5 , 6.5 ]", VectorType::FLOAT64);
        ASSERT_TRUE(vec2.has_value());
        ASSERT_EQ(vec2->getDimensions(), 3);
        std::cout << "  Parsed with spaces: " << vec2->toString() << " ✓\n";

        // Empty vector
        auto vec3 = Vector::parse("[]", VectorType::FLOAT32);
        ASSERT_TRUE(vec3.has_value());
        ASSERT_TRUE(vec3->isEmpty());
        std::cout << "  Empty vector: " << vec3->toString() << " ✓\n";
    }
    std::cout << "  ✓ Parse from string passed\n\n";

    // Test 4: Binary encoding/decoding (Float32)
    std::cout << "Test 4: Binary encoding/decoding (Float32)\n";
    {
        auto vec1 = Vector::parse("[1.0, 2.0, 3.0]", VectorType::FLOAT32);
        auto binary = Vector::encode(*vec1);

        std::cout << "  Binary size: " << binary.size() << " bytes\n";
        ASSERT_EQ(binary.size(), 1 + 4 + 3*4);  // type + dims + 3 floats

        auto vec2 = Vector::decode(binary);
        ASSERT_TRUE(vec2.has_value());
        ASSERT_EQ(vec2->getDimensions(), 3);
        ASSERT_TRUE(floatEqual(vec2->getAsFloat64(0).value(), 1.0));
        ASSERT_TRUE(floatEqual(vec2->getAsFloat64(1).value(), 2.0));
        ASSERT_TRUE(floatEqual(vec2->getAsFloat64(2).value(), 3.0));
        std::cout << "  Encoded and decoded: " << vec2->toString() << " ✓\n";
    }
    std::cout << "  ✓ Binary encoding/decoding passed\n\n";

    // Test 5: Binary encoding/decoding (Float64)
    std::cout << "Test 5: Binary encoding/decoding (Float64)\n";
    {
        auto vec1 = Vector::parse("[1.5, 2.5, 3.5]", VectorType::FLOAT64);
        auto binary = Vector::encode(*vec1);

        std::cout << "  Binary size: " << binary.size() << " bytes\n";
        ASSERT_EQ(binary.size(), 1 + 4 + 3*8);  // type + dims + 3 doubles

        auto vec2 = Vector::decode(binary);
        ASSERT_TRUE(vec2.has_value());
        ASSERT_EQ(vec2->getDimensions(), 3);
        ASSERT_TRUE(floatEqual(vec2->getAsFloat64(0).value(), 1.5));
        ASSERT_TRUE(floatEqual(vec2->getAsFloat64(1).value(), 2.5));
        ASSERT_TRUE(floatEqual(vec2->getAsFloat64(2).value(), 3.5));
        std::cout << "  Encoded and decoded: " << vec2->toString() << " ✓\n";
    }
    std::cout << "  ✓ Binary encoding/decoding (Float64) passed\n\n";

    // Test 6: Magnitude calculation
    std::cout << "Test 6: Magnitude calculation\n";
    {
        auto vec = Vector::parse("[3.0, 4.0]", VectorType::FLOAT32);
        double mag = vec->magnitude();
        ASSERT_TRUE(floatEqual(mag, 5.0));  // sqrt(3^2 + 4^2) = 5
        std::cout << "  Magnitude of [3.0, 4.0]: " << mag << " ✓\n";

        auto vec2 = Vector::parse("[1.0, 1.0, 1.0]", VectorType::FLOAT64);
        double mag2 = vec2->magnitude();
        ASSERT_TRUE(floatEqual(mag2, std::sqrt(3.0)));
        std::cout << "  Magnitude of [1.0, 1.0, 1.0]: " << mag2 << " ✓\n";
    }
    std::cout << "  ✓ Magnitude calculation passed\n\n";

    // Test 7: Normalization
    std::cout << "Test 7: Normalization\n";
    {
        auto vec = Vector::parse("[3.0, 4.0]", VectorType::FLOAT32);
        auto normalized = vec->normalize();

        // Normalized magnitude should be 1.0
        ASSERT_TRUE(floatEqual(normalized.magnitude(), 1.0));

        // Components should be 3/5 and 4/5
        ASSERT_TRUE(floatEqual(normalized.getAsFloat64(0).value(), 0.6));
        ASSERT_TRUE(floatEqual(normalized.getAsFloat64(1).value(), 0.8));
        std::cout << "  Normalized [3.0, 4.0]: " << normalized.toString() << " ✓\n";
    }
    std::cout << "  ✓ Normalization passed\n\n";

    // Test 8: Dot product
    std::cout << "Test 8: Dot product\n";
    {
        auto vec1 = Vector::parse("[1.0, 2.0, 3.0]", VectorType::FLOAT32);
        auto vec2 = Vector::parse("[4.0, 5.0, 6.0]", VectorType::FLOAT32);

        auto dot = vec1->dotProduct(*vec2);
        ASSERT_TRUE(dot.has_value());
        ASSERT_TRUE(floatEqual(*dot, 32.0));  // 1*4 + 2*5 + 3*6 = 32
        std::cout << "  Dot product: " << *dot << " ✓\n";

        // Mismatched dimensions
        auto vec3 = Vector::parse("[1.0, 2.0]", VectorType::FLOAT32);
        auto dot2 = vec1->dotProduct(*vec3);
        ASSERT_FALSE(dot2.has_value());
        std::cout << "  Mismatched dimensions rejected ✓\n";
    }
    std::cout << "  ✓ Dot product passed\n\n";

    // Test 9: Euclidean distance
    std::cout << "Test 9: Euclidean distance\n";
    {
        auto vec1 = Vector::parse("[0.0, 0.0]", VectorType::FLOAT32);
        auto vec2 = Vector::parse("[3.0, 4.0]", VectorType::FLOAT32);

        auto dist = vec1->euclideanDistance(*vec2);
        ASSERT_TRUE(dist.has_value());
        ASSERT_TRUE(floatEqual(*dist, 5.0));  // sqrt(3^2 + 4^2) = 5
        std::cout << "  Distance between [0,0] and [3,4]: " << *dist << " ✓\n";

        auto vec3 = Vector::parse("[1.0, 1.0, 1.0]", VectorType::FLOAT64);
        auto vec4 = Vector::parse("[2.0, 2.0, 2.0]", VectorType::FLOAT64);
        auto dist2 = vec3->euclideanDistance(*vec4);
        ASSERT_TRUE(dist2.has_value());
        ASSERT_TRUE(floatEqual(*dist2, std::sqrt(3.0)));
        std::cout << "  Distance between [1,1,1] and [2,2,2]: " << *dist2 << " ✓\n";
    }
    std::cout << "  ✓ Euclidean distance passed\n\n";

    // Test 10: Cosine similarity
    std::cout << "Test 10: Cosine similarity\n";
    {
        // Identical vectors have similarity 1.0
        auto vec1 = Vector::parse("[1.0, 2.0, 3.0]", VectorType::FLOAT32);
        auto vec2 = Vector::parse("[1.0, 2.0, 3.0]", VectorType::FLOAT32);
        auto sim = vec1->cosineSimilarity(*vec2);
        ASSERT_TRUE(sim.has_value());
        ASSERT_TRUE(floatEqual(*sim, 1.0));
        std::cout << "  Cosine similarity (identical): " << *sim << " ✓\n";

        // Orthogonal vectors have similarity 0.0
        auto vec3 = Vector::parse("[1.0, 0.0]", VectorType::FLOAT32);
        auto vec4 = Vector::parse("[0.0, 1.0]", VectorType::FLOAT32);
        auto sim2 = vec3->cosineSimilarity(*vec4);
        ASSERT_TRUE(sim2.has_value());
        ASSERT_TRUE(floatEqual(*sim2, 0.0));
        std::cout << "  Cosine similarity (orthogonal): " << *sim2 << " ✓\n";

        // Opposite vectors have similarity -1.0
        auto vec5 = Vector::parse("[1.0, 2.0]", VectorType::FLOAT32);
        auto vec6 = Vector::parse("[-1.0, -2.0]", VectorType::FLOAT32);
        auto sim3 = vec5->cosineSimilarity(*vec6);
        ASSERT_TRUE(sim3.has_value());
        ASSERT_TRUE(floatEqual(*sim3, -1.0));
        std::cout << "  Cosine similarity (opposite): " << *sim3 << " ✓\n";
    }
    std::cout << "  ✓ Cosine similarity passed\n\n";

    // Test 11: Manhattan distance
    std::cout << "Test 11: Manhattan distance\n";
    {
        auto vec1 = Vector::parse("[0.0, 0.0]", VectorType::FLOAT32);
        auto vec2 = Vector::parse("[3.0, 4.0]", VectorType::FLOAT32);

        auto dist = vec1->manhattanDistance(*vec2);
        ASSERT_TRUE(dist.has_value());
        ASSERT_TRUE(floatEqual(*dist, 7.0));  // |3-0| + |4-0| = 7
        std::cout << "  Manhattan distance: " << *dist << " ✓\n";
    }
    std::cout << "  ✓ Manhattan distance passed\n\n";

    // Test 12: Vector addition
    std::cout << "Test 12: Vector addition\n";
    {
        auto vec1 = Vector::parse("[1.0, 2.0, 3.0]", VectorType::FLOAT32);
        auto vec2 = Vector::parse("[4.0, 5.0, 6.0]", VectorType::FLOAT32);

        auto sum = vec1->add(*vec2);
        ASSERT_TRUE(sum.has_value());
        ASSERT_EQ(sum->getDimensions(), 3);
        ASSERT_TRUE(floatEqual(sum->getAsFloat64(0).value(), 5.0));
        ASSERT_TRUE(floatEqual(sum->getAsFloat64(1).value(), 7.0));
        ASSERT_TRUE(floatEqual(sum->getAsFloat64(2).value(), 9.0));
        std::cout << "  Vector addition: " << sum->toString() << " ✓\n";
    }
    std::cout << "  ✓ Vector addition passed\n\n";

    // Test 13: Vector subtraction
    std::cout << "Test 13: Vector subtraction\n";
    {
        auto vec1 = Vector::parse("[5.0, 7.0, 9.0]", VectorType::FLOAT32);
        auto vec2 = Vector::parse("[1.0, 2.0, 3.0]", VectorType::FLOAT32);

        auto diff = vec1->subtract(*vec2);
        ASSERT_TRUE(diff.has_value());
        ASSERT_EQ(diff->getDimensions(), 3);
        ASSERT_TRUE(floatEqual(diff->getAsFloat64(0).value(), 4.0));
        ASSERT_TRUE(floatEqual(diff->getAsFloat64(1).value(), 5.0));
        ASSERT_TRUE(floatEqual(diff->getAsFloat64(2).value(), 6.0));
        std::cout << "  Vector subtraction: " << diff->toString() << " ✓\n";
    }
    std::cout << "  ✓ Vector subtraction passed\n\n";

    // Test 14: Scalar multiplication
    std::cout << "Test 14: Scalar multiplication\n";
    {
        auto vec = Vector::parse("[1.0, 2.0, 3.0]", VectorType::FLOAT32);
        auto scaled = vec->multiply(2.5);

        ASSERT_EQ(scaled.getDimensions(), 3);
        ASSERT_TRUE(floatEqual(scaled.getAsFloat64(0).value(), 2.5));
        ASSERT_TRUE(floatEqual(scaled.getAsFloat64(1).value(), 5.0));
        ASSERT_TRUE(floatEqual(scaled.getAsFloat64(2).value(), 7.5));
        std::cout << "  Scalar multiplication: " << scaled.toString() << " ✓\n";
    }
    std::cout << "  ✓ Scalar multiplication passed\n\n";

    // Test 15: Distance metrics enum
    std::cout << "Test 15: Distance metrics via enum\n";
    {
        auto vec1 = Vector::parse("[1.0, 2.0, 3.0]", VectorType::FLOAT32);
        auto vec2 = Vector::parse("[4.0, 5.0, 6.0]", VectorType::FLOAT32);

        auto euclidean = vec1->distance(*vec2, DistanceMetric::EUCLIDEAN);
        ASSERT_TRUE(euclidean.has_value());
        std::cout << "  Euclidean: " << *euclidean << " ✓\n";

        auto cosine = vec1->distance(*vec2, DistanceMetric::COSINE);
        ASSERT_TRUE(cosine.has_value());
        std::cout << "  Cosine: " << *cosine << " ✓\n";

        auto manhattan = vec1->distance(*vec2, DistanceMetric::MANHATTAN);
        ASSERT_TRUE(manhattan.has_value());
        std::cout << "  Manhattan: " << *manhattan << " ✓\n";

        auto dot = vec1->distance(*vec2, DistanceMetric::DOT_PRODUCT);
        ASSERT_TRUE(dot.has_value());
        std::cout << "  Dot product: " << *dot << " ✓\n";
    }
    std::cout << "  ✓ Distance metrics via enum passed\n\n";

    // Test 16: Validation
    std::cout << "Test 16: String validation\n";
    {
        ASSERT_TRUE(Vector::validate("[1.0, 2.0, 3.0]"));
        std::cout << "  Valid vector string accepted ✓\n";

        ASSERT_TRUE(Vector::validate("[]"));
        std::cout << "  Empty vector accepted ✓\n";

        ASSERT_FALSE(Vector::validate("[1.0, 2.0"));
        std::cout << "  Unclosed bracket rejected ✓\n";

        ASSERT_FALSE(Vector::validate("[1.0, abc]"));
        std::cout << "  Invalid number rejected ✓\n";

        ASSERT_FALSE(Vector::validate("not a vector"));
        std::cout << "  Non-vector string rejected ✓\n";
    }
    std::cout << "  ✓ Validation passed\n\n";

    // Test 17: Scientific notation
    std::cout << "Test 17: Scientific notation parsing\n";
    {
        auto vec = Vector::parse("[1.5e2, 2.5e-1, 3.0e0]", VectorType::FLOAT64);
        ASSERT_TRUE(vec.has_value());
        ASSERT_TRUE(floatEqual(vec->getAsFloat64(0).value(), 150.0));
        ASSERT_TRUE(floatEqual(vec->getAsFloat64(1).value(), 0.25));
        ASSERT_TRUE(floatEqual(vec->getAsFloat64(2).value(), 3.0));
        std::cout << "  Scientific notation: " << vec->toString() << " ✓\n";
    }
    std::cout << "  ✓ Scientific notation passed\n\n";

    // Test 18: Real-world example - embeddings similarity
    std::cout << "Test 18: Real-world example - text embeddings\n";
    {
        // Simulate word embeddings (simplified)
        auto embedding1 = Vector::parse("[0.5, 0.8, 0.3, 0.1]", VectorType::FLOAT32);
        auto embedding2 = Vector::parse("[0.6, 0.7, 0.4, 0.2]", VectorType::FLOAT32);
        auto embedding3 = Vector::parse("[-0.5, -0.8, -0.3, -0.1]", VectorType::FLOAT32);

        // Similar embeddings have high cosine similarity
        auto sim_similar = embedding1->cosineSimilarity(*embedding2);
        std::cout << "  Similarity (similar): " << *sim_similar << " ✓\n";
        ASSERT_GT(*sim_similar, 0.9);

        // Opposite embeddings have negative similarity
        auto sim_opposite = embedding1->cosineSimilarity(*embedding3);
        std::cout << "  Similarity (opposite): " << *sim_opposite << " ✓\n";
        ASSERT_LT(*sim_opposite, -0.9);
    }
    std::cout << "  ✓ Real-world example passed\n\n";

    // Test 19: Helper functions
    std::cout << "Test 19: Static helper functions\n";
    {
        auto vec1 = Vector::fromFloat32({1.0f, 2.0f, 3.0f});
        auto vec2 = Vector::fromFloat32({4.0f, 5.0f, 6.0f});

        auto euclidean = Vector::euclideanDistance(vec1, vec2);
        ASSERT_TRUE(euclidean.has_value());
        std::cout << "  Static euclideanDistance: " << *euclidean << " ✓\n";

        auto cosine = Vector::cosineSimilarity(vec1, vec2);
        ASSERT_TRUE(cosine.has_value());
        std::cout << "  Static cosineSimilarity: " << *cosine << " ✓\n";

        auto manhattan = Vector::manhattanDistance(vec1, vec2);
        ASSERT_TRUE(manhattan.has_value());
        std::cout << "  Static manhattanDistance: " << *manhattan << " ✓\n";

        auto dot = Vector::dotProduct(vec1, vec2);
        ASSERT_TRUE(dot.has_value());
        std::cout << "  Static dotProduct: " << *dot << " ✓\n";
    }
    std::cout << "  ✓ Static helper functions passed\n\n";

    // Test 20: Edge cases
    std::cout << "Test 20: Edge cases\n";
    {
        // Zero vector
        auto zero = Vector::parse("[0.0, 0.0, 0.0]", VectorType::FLOAT32);
        ASSERT_TRUE(floatEqual(zero->magnitude(), 0.0));
        std::cout << "  Zero vector magnitude: 0.0 ✓\n";

        // Normalization of zero vector (should return zero vector)
        auto normalized = zero->normalize();
        ASSERT_TRUE(floatEqual(normalized.magnitude(), 0.0));
        std::cout << "  Zero vector normalization: returns zero ✓\n";

        // Single element vector
        auto single = Vector::parse("[42.0]", VectorType::FLOAT64);
        ASSERT_EQ(single->getDimensions(), 1);
        ASSERT_TRUE(floatEqual(single->getAsFloat64(0).value(), 42.0));
        std::cout << "  Single element vector: " << single->toString() << " ✓\n";

        // Large vector
        std::vector<double> large_data;
        for (int i = 0; i < 100; ++i) {
            large_data.push_back(static_cast<double>(i));
        }
        auto large_vec = Vector::fromFloat64(large_data);
        ASSERT_EQ(large_vec.getDimensions(), 100);
        std::cout << "  Large vector (100 dimensions) created ✓\n";
    }
    std::cout << "  ✓ Edge cases passed\n\n";

    std::cout << "========================================\n";
    std::cout << "ALL TESTS PASSED! ✓\n";
    std::cout << "VECTOR type is fully functional.\n";
    std::cout << "========================================\n";
}

