#include "scratchbird/core/array.h"
#include <iostream>
#include "gtest/gtest.h"

using namespace scratchbird::core;


TEST(ArrayTest, Comprehensive) {

    std::cout << "Testing ARRAY Implementation...\n\n";

    // Test 1: Create 1D INT32 array
    std::cout << "Test 1: 1D INT32 array\n";
    {
        std::vector<int32_t> values = {1, 2, 3, 4, 5};
        auto arr = Array::fromInt32(values, {5});
        ASSERT_TRUE(arr.has_value());
        ASSERT_EQ(arr->getRank(), 1);
        ASSERT_EQ(arr->getTotalElements(), 5);
        ASSERT_TRUE(arr->at({0}).has_value());
        ASSERT_EQ(std::get<int32_t>(*arr->at({0})), 1);
        ASSERT_EQ(std::get<int32_t>(*arr->at({4})), 5);
        std::cout << "  Array: " << arr->toString() << " ✓\n";
    }
    std::cout << "  ✓ 1D INT32 array passed\n\n";

    // Test 2: Create 2D array
    std::cout << "Test 2: 2D array (matrix)\n";
    {
        std::vector<int32_t> values = {1, 2, 3, 4, 5, 6};
        auto arr = Array::fromInt32(values, {2, 3});  // 2x3 matrix
        ASSERT_TRUE(arr.has_value());
        ASSERT_EQ(arr->getRank(), 2);
        ASSERT_EQ(arr->getTotalElements(), 6);
        ASSERT_EQ(std::get<int32_t>(*arr->at({0, 0})), 1);
        ASSERT_EQ(std::get<int32_t>(*arr->at({0, 2})), 3);
        ASSERT_EQ(std::get<int32_t>(*arr->at({1, 0})), 4);
        ASSERT_EQ(std::get<int32_t>(*arr->at({1, 2})), 6);
        std::cout << "  Matrix: " << arr->toString() << " ✓\n";
    }
    std::cout << "  ✓ 2D array passed\n\n";

    // Test 3: FLOAT64 array
    std::cout << "Test 3: FLOAT64 array\n";
    {
        std::vector<double> values = {1.5, 2.5, 3.5};
        auto arr = Array::fromFloat64(values, {3});
        ASSERT_TRUE(arr.has_value());
        ASSERT_EQ(arr->getElementType(), ArrayElementType::FLOAT64);
        ASSERT_EQ(std::get<double>(*arr->at({0})), 1.5);
        std::cout << "  Array: " << arr->toString() << " ✓\n";
    }
    std::cout << "  ✓ FLOAT64 array passed\n\n";

    // Test 4: STRING array
    std::cout << "Test 4: STRING array\n";
    {
        std::vector<std::string> values = {"hello", "world", "test"};
        auto arr = Array::fromString(values, {3});
        ASSERT_TRUE(arr.has_value());
        ASSERT_EQ(arr->getElementType(), ArrayElementType::STRING);
        ASSERT_EQ(std::get<std::string>(*arr->at({0})), "hello");
        ASSERT_EQ(std::get<std::string>(*arr->at({2})), "test");
        std::cout << "  Array: " << arr->toString() << " ✓\n";
    }
    std::cout << "  ✓ STRING array passed\n\n";

    // Test 5: BOOL array
    std::cout << "Test 5: BOOL array\n";
    {
        std::vector<bool> values = {true, false, true, true};
        auto arr = Array::fromBool(values, {4});
        ASSERT_TRUE(arr.has_value());
        ASSERT_EQ(arr->getElementType(), ArrayElementType::BOOL);
        ASSERT_EQ(std::get<bool>(*arr->at({0})), true);
        ASSERT_EQ(std::get<bool>(*arr->at({1})), false);
        std::cout << "  Array: " << arr->toString() << " ✓\n";
    }
    std::cout << "  ✓ BOOL array passed\n\n";

    // Test 6: Binary encoding/decoding
    std::cout << "Test 6: Binary encoding/decoding\n";
    {
        std::vector<int32_t> values = {10, 20, 30, 40};
        auto arr1 = Array::fromInt32(values, {2, 2});
        auto binary = Array::encode(*arr1);

        std::cout << "  Binary size: " << binary.size() << " bytes\n";

        auto arr2 = Array::decode(binary);
        ASSERT_TRUE(arr2.has_value());
        ASSERT_EQ(arr2->getRank(), 2);
        ASSERT_EQ(arr2->getTotalElements(), 4);
        ASSERT_EQ(std::get<int32_t>(*arr2->at({0, 0})), 10);
        ASSERT_EQ(std::get<int32_t>(*arr2->at({1, 1})), 40);
        std::cout << "  Decoded: " << arr2->toString() << " ✓\n";
    }
    std::cout << "  ✓ Binary encoding/decoding passed\n\n";

    // Test 7: Reshape
    std::cout << "Test 7: Reshape\n";
    {
        std::vector<int32_t> values = {1, 2, 3, 4, 5, 6};
        auto arr = Array::fromInt32(values, {6});

        auto reshaped = arr->reshape({2, 3});
        ASSERT_TRUE(reshaped.has_value());
        ASSERT_EQ(reshaped->getRank(), 2);
        ASSERT_EQ(std::get<int32_t>(*reshaped->at({0, 0})), 1);
        ASSERT_EQ(std::get<int32_t>(*reshaped->at({1, 2})), 6);
        std::cout << "  Reshaped from {6} to {2,3}: " << reshaped->toString() << " ✓\n";

        // Invalid reshape
        auto invalid = arr->reshape({2, 2});  // Wrong total elements
        ASSERT_FALSE(invalid.has_value());
        std::cout << "  Invalid reshape rejected ✓\n";
    }
    std::cout << "  ✓ Reshape passed\n\n";

    // Test 8: Flatten
    std::cout << "Test 8: Flatten\n";
    {
        std::vector<int32_t> values = {1, 2, 3, 4};
        auto arr = Array::fromInt32(values, {2, 2});

        auto flat = arr->flatten();
        ASSERT_EQ(flat.getRank(), 1);
        ASSERT_EQ(flat.getDimensions()[0], 4);
        std::cout << "  Flattened: " << flat.toString() << " ✓\n";
    }
    std::cout << "  ✓ Flatten passed\n\n";

    // Test 9: Transpose (2D only)
    std::cout << "Test 9: Transpose\n";
    {
        std::vector<int32_t> values = {1, 2, 3, 4, 5, 6};
        auto arr = Array::fromInt32(values, {2, 3});

        auto trans = arr->transpose();
        ASSERT_TRUE(trans.has_value());
        ASSERT_EQ(trans->getRank(), 2);
        ASSERT_EQ(trans->getDimensions()[0], 3);
        ASSERT_EQ(trans->getDimensions()[1], 2);
        ASSERT_EQ(std::get<int32_t>(*trans->at({0, 0})), 1);
        ASSERT_EQ(std::get<int32_t>(*trans->at({2, 1})), 6);
        std::cout << "  Original: " << arr->toString() << "\n";
        std::cout << "  Transposed: " << trans->toString() << " ✓\n";
    }
    std::cout << "  ✓ Transpose passed\n\n";

    // Test 10: Slicing
    std::cout << "Test 10: Array slicing\n";
    {
        std::vector<int32_t> values = {1, 2, 3, 4, 5, 6};
        auto arr = Array::fromInt32(values, {2, 3});

        // Slice [0:1, 1:3] - first row, columns 1-2
        auto sliced = arr->slice({{0, 1}, {1, 3}});
        ASSERT_TRUE(sliced.has_value());
        ASSERT_EQ(sliced->getRank(), 2);
        ASSERT_EQ(sliced->getDimensions()[0], 1);
        ASSERT_EQ(sliced->getDimensions()[1], 2);
        ASSERT_EQ(std::get<int32_t>(*sliced->at({0, 0})), 2);
        ASSERT_EQ(std::get<int32_t>(*sliced->at({0, 1})), 3);
        std::cout << "  Sliced: " << sliced->toString() << " ✓\n";
    }
    std::cout << "  ✓ Slicing passed\n\n";

    // Test 11: 3D array
    std::cout << "Test 11: 3D array\n";
    {
        std::vector<int32_t> values;
        for (int i = 0; i < 24; ++i) {
            values.push_back(i + 1);
        }
        auto arr = Array::fromInt32(values, {2, 3, 4});  // 2x3x4 array
        ASSERT_TRUE(arr.has_value());
        ASSERT_EQ(arr->getRank(), 3);
        ASSERT_EQ(arr->getTotalElements(), 24);
        ASSERT_EQ(std::get<int32_t>(*arr->at({0, 0, 0})), 1);
        ASSERT_EQ(std::get<int32_t>(*arr->at({1, 2, 3})), 24);
        std::cout << "  3D array created with shape {2, 3, 4} ✓\n";
    }
    std::cout << "  ✓ 3D array passed\n\n";

    // Test 12: Out of bounds access
    std::cout << "Test 12: Bounds checking\n";
    {
        std::vector<int32_t> values = {1, 2, 3, 4};
        auto arr = Array::fromInt32(values, {2, 2});

        // Valid access
        ASSERT_TRUE(arr->at({0, 0}).has_value());
        ASSERT_TRUE(arr->at({1, 1}).has_value());

        // Out of bounds
        ASSERT_FALSE(arr->at({2, 0}).has_value());
        ASSERT_FALSE(arr->at({0, 2}).has_value());
        ASSERT_FALSE(arr->at({5, 5}).has_value());
        std::cout << "  Bounds checking works correctly ✓\n";
    }
    std::cout << "  ✓ Bounds checking passed\n\n";

    // Test 13: Type getters
    std::cout << "Test 13: Type-specific getters\n";
    {
        std::vector<float> values = {1.0f, 2.0f, 3.0f};
        auto arr = Array::fromFloat32(values, {3});

        auto vec = arr->getFloat32Vector();
        ASSERT_TRUE(vec.has_value());
        ASSERT_EQ(vec->size(), 3);
        ASSERT_EQ((*vec)[0], 1.0f);

        // Wrong type
        auto wrong = arr->getInt32Vector();
        ASSERT_FALSE(wrong.has_value());
        std::cout << "  Type-specific getters work ✓\n";
    }
    std::cout << "  ✓ Type-specific getters passed\n\n";

    std::cout << "========================================\n";
    std::cout << "ALL TESTS PASSED! ✓\n";
    std::cout << "ARRAY type is fully functional.\n";
    std::cout << "========================================\n";
}

