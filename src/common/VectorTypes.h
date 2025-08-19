/*
 *	PROGRAM:		ScratchBird Vector Types
 *	MODULE:			VectorTypes.h
 *	DESCRIPTION:	VECTOR datatype for AI/ML applications
 *
 *  The contents of this file are subject to the Initial
 *  Developer's Public License Version 1.0 (the "License");
 *  you may not use this file except in compliance with the
 *  License. You may obtain a copy of the License at
 *  http://www.ibphoenix.com/main.nfs?a=ibphoenix&page=ibp_idpl.
 *
 *  Software distributed under the License is distributed AS IS,
 *  WITHOUT WARRANTY OF ANY KIND, either express or implied.
 *  See the License for the specific language governing rights
 *  and limitations under the License.
 *
 *  The Original Code was created by ScratchBird Development Team
 *  for the ScratchBird Open Source RDBMS project.
 *
 *  Copyright (c) 2025 ScratchBird Development Team
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): _______________________________________.
 *
 */

#ifndef SB_VECTOR_TYPES_H
#define SB_VECTOR_TYPES_H

#include "firebird/Interface.h"
#include "sb_exception.h"
#include "classes/fb_string.h"
#include <vector>
#include <cmath>
#include <algorithm>

namespace ScratchBird {

// Vector distance/similarity metrics
enum VectorSimilarityType {
    VECTOR_L2_DISTANCE,        // Euclidean distance (default)
    VECTOR_COSINE_SIMILARITY,  // Cosine similarity
    VECTOR_DOT_PRODUCT,        // Dot product
    VECTOR_L1_DISTANCE,        // Manhattan distance
    VECTOR_JACCARD_SIMILARITY  // Jaccard similarity (for sparse vectors)
};

// Vector storage format
enum VectorStorageFormat {
    VECTOR_DENSE,              // Dense vector (all elements stored)
    VECTOR_SPARSE              // Sparse vector (non-zero elements only)
};

// Vector class for AI/ML operations
class Vector {
public:
    // Constructors
    Vector();
    Vector(USHORT dimensions);
    Vector(const std::vector<double>& values);
    Vector(const std::vector<float>& values);
    Vector(const double* values, USHORT count);
    Vector(const float* values, USHORT count);
    
    // Construction from string representation
    Vector(const char* vector_literal);
    Vector(const string& vector_literal);
    
    // Copy and assignment
    Vector(const Vector& other);
    Vector& operator=(const Vector& other);
    Vector(Vector&& other) noexcept;
    Vector& operator=(Vector&& other) noexcept;
    
    ~Vector();
    
    // Element access
    double& operator[](USHORT index);
    const double& operator[](USHORT index) const;
    double getValue(USHORT index) const;
    void setValue(USHORT index, double value);
    
    // Vector properties
    USHORT getDimensions() const { return dimensions_; }
    ULONG getStorageSize() const;
    VectorStorageFormat getStorageFormat() const { return format_; }
    bool isEmpty() const { return dimensions_ == 0; }
    bool isNormalized() const;
    
    // Vector operations
    Vector operator+(const Vector& other) const;
    Vector operator-(const Vector& other) const;
    Vector operator*(double scalar) const;
    Vector operator/(double scalar) const;
    Vector& operator+=(const Vector& other);
    Vector& operator-=(const Vector& other);
    Vector& operator*=(double scalar);
    Vector& operator/=(double scalar);
    
    // Comparison operators
    bool operator==(const Vector& other) const;
    bool operator!=(const Vector& other) const;
    
    // Similarity/distance functions (PostgreSQL-compatible)
    double cosineDistance(const Vector& other) const;  // Cosine distance
    double l2Distance(const Vector& other) const;     // L2 distance  
    double l1Distance(const Vector& other) const;     // L1 distance
    
    // Mathematical functions
    double magnitude() const;
    double norm() const { return magnitude(); }
    Vector normalize() const;
    double dotProduct(const Vector& other) const;
    double distance(const Vector& other, VectorSimilarityType type = VECTOR_L2_DISTANCE) const;
    double similarity(const Vector& other, VectorSimilarityType type = VECTOR_COSINE_SIMILARITY) const;
    
    // Advanced operations
    Vector crossProduct(const Vector& other) const;  // 3D vectors only
    double angle(const Vector& other) const;
    bool isOrthogonal(const Vector& other, double tolerance = 1e-10) const;
    bool isParallel(const Vector& other, double tolerance = 1e-10) const;
    
    // Statistics and analysis
    double mean() const;
    double variance() const;
    double standardDeviation() const;
    double min() const;
    double max() const;
    std::pair<double, double> minMax() const;
    
    // Vector transformations
    Vector abs() const;
    Vector round(int decimals = 0) const;
    Vector truncate(int decimals = 0) const;
    Vector clamp(double min_val, double max_val) const;
    
    // Sparse vector support
    void convertToSparse(double tolerance = 1e-10);
    void convertToDense();
    std::vector<std::pair<USHORT, double>> getSparseRepresentation() const;
    USHORT getNonZeroCount() const;
    double getDensity() const;
    
    // String representation
    void toString(string& result, int precision = 6) const;
    string toString(int precision = 6) const;
    void toCompactString(string& result) const;
    string toCompactString() const;
    
    // Binary representation for storage
    ULONG toBinary(UCHAR* buffer, ULONG buffer_size) const;
    void fromBinary(const UCHAR* buffer, ULONG size);
    ULONG getBinarySize() const;
    
    // Index key generation
    ULONG makeIndexKey(vary* buf) const;
    static ULONG getIndexKeyLength() { return 252; }
    
    // Parsing and validation
    void parseVector(const char* vector_literal);
    static bool isValidVector(const char* vector_literal);
    static USHORT extractDimensions(const char* vector_literal);
    
    // Utility functions
    void resize(USHORT new_dimensions, double fill_value = 0.0);
    void clear();
    void fill(double value);
    void randomize(double min_val = -1.0, double max_val = 1.0);
    
    // Type conversion
    std::vector<double> toStdVector() const;
    std::vector<float> toFloatVector() const;
    void fromStdVector(const std::vector<double>& vec);
    void fromStdVector(const std::vector<float>& vec);

private:
    USHORT dimensions_;
    double* values_;               // Dense storage
    VectorStorageFormat format_;
    
    // Sparse storage (when format_ == VECTOR_SPARSE)
    std::vector<std::pair<USHORT, double>> sparse_values_;
    
    // Internal methods
    void allocateStorage(USHORT dimensions);
    void deallocateStorage();
    void copyFrom(const Vector& other);
    void validateDimensions(const Vector& other) const;
    void validateIndex(USHORT index) const;
    double computeDistance(const Vector& other, VectorSimilarityType type) const;
    double computeSimilarity(const Vector& other, VectorSimilarityType type) const;
    
    static void invalid_vector();
    static void dimension_mismatch();
    static void index_out_of_bounds();
};

// Vector utility functions
class VectorUtils {
public:
    // Vector creation
    static Vector zeros(USHORT dimensions);
    static Vector ones(USHORT dimensions);
    static Vector random(USHORT dimensions, double min_val = -1.0, double max_val = 1.0);
    static Vector gaussian(USHORT dimensions, double mean = 0.0, double stddev = 1.0);
    static Vector unit(USHORT dimensions, USHORT axis);  // Unit vector along axis
    
    // Vector operations
    static Vector add(const Vector& v1, const Vector& v2);
    static Vector subtract(const Vector& v1, const Vector& v2);
    static Vector multiply(const Vector& v, double scalar);
    static Vector divide(const Vector& v, double scalar);
    static double dot(const Vector& v1, const Vector& v2);
    static Vector cross(const Vector& v1, const Vector& v2);
    
    // Distance and similarity functions
    static double euclideanDistance(const Vector& v1, const Vector& v2);
    static double manhattanDistance(const Vector& v1, const Vector& v2);
    static double cosineDistance(const Vector& v1, const Vector& v2);
    static double cosineSimilarity(const Vector& v1, const Vector& v2);
    static double jaccardSimilarity(const Vector& v1, const Vector& v2);
    
    // Statistical functions
    static Vector mean(const std::vector<Vector>& vectors);
    static Vector median(const std::vector<Vector>& vectors);
    static Vector variance(const std::vector<Vector>& vectors);
    static Vector standardDeviation(const std::vector<Vector>& vectors);
    
    // Nearest neighbor functions
    static std::vector<ULONG> findNearestNeighbors(const Vector& query, 
                                                   const std::vector<Vector>& vectors,
                                                   USHORT k = 5,
                                                   VectorSimilarityType type = VECTOR_L2_DISTANCE);
    
    // Clustering and ML utilities
    static std::vector<Vector> kmeansCentroids(const std::vector<Vector>& vectors, USHORT k);
    static std::vector<USHORT> assignToClusters(const std::vector<Vector>& vectors,
                                                const std::vector<Vector>& centroids);
    
    // Dimensionality reduction
    static Vector projectToPCA(const Vector& v, const std::vector<Vector>& principal_components);
    
    // Validation and normalization
    static bool validateDimensions(const std::vector<Vector>& vectors);
    static std::vector<Vector> normalizeVectors(const std::vector<Vector>& vectors);
    
    // Serialization
    static string vectorsToJSON(const std::vector<Vector>& vectors);
    static std::vector<Vector> vectorsFromJSON(const string& json);
};

// Vector aggregation functions for SQL
class VectorAggregates {
public:
    static Vector avg(const std::vector<Vector>& vectors);
    static Vector sum(const std::vector<Vector>& vectors);
    static Vector min(const std::vector<Vector>& vectors);
    static Vector max(const std::vector<Vector>& vectors);
    static Vector variance(const std::vector<Vector>& vectors);
    static Vector stddev(const std::vector<Vector>& vectors);
};

// Type aliases for SQL compatibility
using vector_t = Vector;

} // namespace ScratchBird

#endif // SB_VECTOR_TYPES_H