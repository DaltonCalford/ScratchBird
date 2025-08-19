/*
 *	PROGRAM:		ScratchBird Vector Types
 *	MODULE:			VectorTypes.cpp
 *	DESCRIPTION:	VECTOR datatype implementation
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

#include "scratchbird.h"
#include "VectorTypes.h"
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <regex>
#include <random>

namespace ScratchBird {

// Vector implementation

Vector::Vector() : dimensions_(0), values_(nullptr), format_(VECTOR_DENSE) {
}

Vector::Vector(USHORT dimensions) : dimensions_(0), values_(nullptr), format_(VECTOR_DENSE) {
    allocateStorage(dimensions);
    fill(0.0);
}

Vector::Vector(const std::vector<double>& values) : dimensions_(0), values_(nullptr), format_(VECTOR_DENSE) {
    fromStdVector(values);
}

Vector::Vector(const std::vector<float>& values) : dimensions_(0), values_(nullptr), format_(VECTOR_DENSE) {
    fromStdVector(values);
}

Vector::Vector(const double* values, USHORT count) : dimensions_(0), values_(nullptr), format_(VECTOR_DENSE) {
    if (values && count > 0) {
        allocateStorage(count);
        for (USHORT i = 0; i < count; ++i) {
            values_[i] = values[i];
        }
    }
}

Vector::Vector(const float* values, USHORT count) : dimensions_(0), values_(nullptr), format_(VECTOR_DENSE) {
    if (values && count > 0) {
        allocateStorage(count);
        for (USHORT i = 0; i < count; ++i) {
            values_[i] = static_cast<double>(values[i]);
        }
    }
}

Vector::Vector(const char* vector_literal) : dimensions_(0), values_(nullptr), format_(VECTOR_DENSE) {
    parseVector(vector_literal);
}

Vector::Vector(const string& vector_literal) : dimensions_(0), values_(nullptr), format_(VECTOR_DENSE) {
    parseVector(vector_literal.c_str());
}

Vector::Vector(const Vector& other) : dimensions_(0), values_(nullptr), format_(VECTOR_DENSE) {
    copyFrom(other);
}

Vector& Vector::operator=(const Vector& other) {
    if (this != &other) {
        deallocateStorage();
        copyFrom(other);
    }
    return *this;
}

Vector::Vector(Vector&& other) noexcept 
    : dimensions_(other.dimensions_), values_(other.values_), format_(other.format_),
      sparse_values_(std::move(other.sparse_values_)) {
    other.dimensions_ = 0;
    other.values_ = nullptr;
    other.format_ = VECTOR_DENSE;
}

Vector& Vector::operator=(Vector&& other) noexcept {
    if (this != &other) {
        deallocateStorage();
        dimensions_ = other.dimensions_;
        values_ = other.values_;
        format_ = other.format_;
        sparse_values_ = std::move(other.sparse_values_);
        
        other.dimensions_ = 0;
        other.values_ = nullptr;
        other.format_ = VECTOR_DENSE;
    }
    return *this;
}

Vector::~Vector() {
    deallocateStorage();
}

double& Vector::operator[](USHORT index) {
    validateIndex(index);
    if (format_ == VECTOR_SPARSE) {
        convertToDense();
    }
    return values_[index];
}

const double& Vector::operator[](USHORT index) const {
    validateIndex(index);
    if (format_ == VECTOR_SPARSE) {
        // For sparse vectors, find the value
        for (const auto& pair : sparse_values_) {
            if (pair.first == index) {
                return pair.second;
            }
        }
        static const double zero = 0.0;
        return zero;
    }
    return values_[index];
}

double Vector::getValue(USHORT index) const {
    return (*this)[index];
}

void Vector::setValue(USHORT index, double value) {
    validateIndex(index);
    
    if (format_ == VECTOR_SPARSE) {
        // Update sparse representation
        for (auto& pair : sparse_values_) {
            if (pair.first == index) {
                if (std::abs(value) < 1e-10) {
                    // Remove zero values
                    sparse_values_.erase(
                        std::remove_if(sparse_values_.begin(), sparse_values_.end(),
                                     [index](const std::pair<USHORT, double>& p) { return p.first == index; }),
                        sparse_values_.end());
                } else {
                    pair.second = value;
                }
                return;
            }
        }
        
        // Add new non-zero value
        if (std::abs(value) >= 1e-10) {
            sparse_values_.push_back({index, value});
            std::sort(sparse_values_.begin(), sparse_values_.end());
        }
    } else {
        values_[index] = value;
    }
}

ULONG Vector::getStorageSize() const {
    if (format_ == VECTOR_SPARSE) {
        return sizeof(USHORT) + sparse_values_.size() * (sizeof(USHORT) + sizeof(double));
    } else {
        return sizeof(USHORT) + dimensions_ * sizeof(double);
    }
}

bool Vector::isNormalized() const {
    double mag = magnitude();
    return std::abs(mag - 1.0) < 1e-10;
}

Vector Vector::operator+(const Vector& other) const {
    validateDimensions(other);
    
    Vector result(dimensions_);
    for (USHORT i = 0; i < dimensions_; ++i) {
        result[i] = getValue(i) + other.getValue(i);
    }
    return result;
}

Vector Vector::operator-(const Vector& other) const {
    validateDimensions(other);
    
    Vector result(dimensions_);
    for (USHORT i = 0; i < dimensions_; ++i) {
        result[i] = getValue(i) - other.getValue(i);
    }
    return result;
}

Vector Vector::operator*(double scalar) const {
    Vector result(dimensions_);
    for (USHORT i = 0; i < dimensions_; ++i) {
        result[i] = getValue(i) * scalar;
    }
    return result;
}

Vector Vector::operator/(double scalar) const {
    if (std::abs(scalar) < 1e-10) {
        throw std::invalid_argument("Division by zero");
    }
    return (*this) * (1.0 / scalar);
}

Vector& Vector::operator+=(const Vector& other) {
    validateDimensions(other);
    for (USHORT i = 0; i < dimensions_; ++i) {
        setValue(i, getValue(i) + other.getValue(i));
    }
    return *this;
}

Vector& Vector::operator-=(const Vector& other) {
    validateDimensions(other);
    for (USHORT i = 0; i < dimensions_; ++i) {
        setValue(i, getValue(i) - other.getValue(i));
    }
    return *this;
}

Vector& Vector::operator*=(double scalar) {
    for (USHORT i = 0; i < dimensions_; ++i) {
        setValue(i, getValue(i) * scalar);
    }
    return *this;
}

Vector& Vector::operator/=(double scalar) {
    if (std::abs(scalar) < 1e-10) {
        throw std::invalid_argument("Division by zero");
    }
    return (*this) *= (1.0 / scalar);
}

bool Vector::operator==(const Vector& other) const {
    if (dimensions_ != other.dimensions_) return false;
    
    for (USHORT i = 0; i < dimensions_; ++i) {
        if (std::abs(getValue(i) - other.getValue(i)) > 1e-10) {
            return false;
        }
    }
    return true;
}

bool Vector::operator!=(const Vector& other) const {
    return !(*this == other);
}

// PostgreSQL-compatible similarity functions
double Vector::cosineDistance(const Vector& other) const {
    return 1.0 - similarity(other, VECTOR_COSINE_SIMILARITY);
}

double Vector::l2Distance(const Vector& other) const {
    return distance(other, VECTOR_L2_DISTANCE);
}

double Vector::l1Distance(const Vector& other) const {
    return distance(other, VECTOR_L1_DISTANCE);
}

double Vector::magnitude() const {
    double sum = 0.0;
    for (USHORT i = 0; i < dimensions_; ++i) {
        double val = getValue(i);
        sum += val * val;
    }
    return std::sqrt(sum);
}

Vector Vector::normalize() const {
    double mag = magnitude();
    if (mag < 1e-10) {
        throw std::invalid_argument("Cannot normalize zero vector");
    }
    return (*this) / mag;
}

double Vector::dotProduct(const Vector& other) const {
    validateDimensions(other);
    
    double result = 0.0;
    for (USHORT i = 0; i < dimensions_; ++i) {
        result += getValue(i) * other.getValue(i);
    }
    return result;
}

double Vector::distance(const Vector& other, VectorSimilarityType type) const {
    return computeDistance(other, type);
}

double Vector::similarity(const Vector& other, VectorSimilarityType type) const {
    return computeSimilarity(other, type);
}

Vector Vector::crossProduct(const Vector& other) const {
    if (dimensions_ != 3 || other.dimensions_ != 3) {
        throw std::invalid_argument("Cross product only defined for 3D vectors");
    }
    
    Vector result(3);
    result[0] = getValue(1) * other.getValue(2) - getValue(2) * other.getValue(1);
    result[1] = getValue(2) * other.getValue(0) - getValue(0) * other.getValue(2);
    result[2] = getValue(0) * other.getValue(1) - getValue(1) * other.getValue(0);
    return result;
}

double Vector::angle(const Vector& other) const {
    double dot = dotProduct(other);
    double mags = magnitude() * other.magnitude();
    if (mags < 1e-10) {
        throw std::invalid_argument("Cannot compute angle with zero vector");
    }
    return std::acos(std::max(-1.0, std::min(1.0, dot / mags)));
}

bool Vector::isOrthogonal(const Vector& other, double tolerance) const {
    return std::abs(dotProduct(other)) < tolerance;
}

bool Vector::isParallel(const Vector& other, double tolerance) const {
    double cos_angle = dotProduct(other) / (magnitude() * other.magnitude());
    return std::abs(std::abs(cos_angle) - 1.0) < tolerance;
}

double Vector::mean() const {
    if (dimensions_ == 0) return 0.0;
    
    double sum = 0.0;
    for (USHORT i = 0; i < dimensions_; ++i) {
        sum += getValue(i);
    }
    return sum / dimensions_;
}

double Vector::variance() const {
    if (dimensions_ == 0) return 0.0;
    
    double m = mean();
    double sum = 0.0;
    for (USHORT i = 0; i < dimensions_; ++i) {
        double diff = getValue(i) - m;
        sum += diff * diff;
    }
    return sum / dimensions_;
}

double Vector::standardDeviation() const {
    return std::sqrt(variance());
}

double Vector::min() const {
    if (dimensions_ == 0) return 0.0;
    
    double result = getValue(0);
    for (USHORT i = 1; i < dimensions_; ++i) {
        result = std::min(result, getValue(i));
    }
    return result;
}

double Vector::max() const {
    if (dimensions_ == 0) return 0.0;
    
    double result = getValue(0);
    for (USHORT i = 1; i < dimensions_; ++i) {
        result = std::max(result, getValue(i));
    }
    return result;
}

std::pair<double, double> Vector::minMax() const {
    if (dimensions_ == 0) return {0.0, 0.0};
    
    double min_val = getValue(0);
    double max_val = getValue(0);
    for (USHORT i = 1; i < dimensions_; ++i) {
        double val = getValue(i);
        min_val = std::min(min_val, val);
        max_val = std::max(max_val, val);
    }
    return {min_val, max_val};
}

void Vector::toString(string& result, int precision) const {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(precision);
    oss << "[";
    
    for (USHORT i = 0; i < dimensions_; ++i) {
        if (i > 0) oss << ",";
        oss << getValue(i);
    }
    oss << "]";
    
    result = oss.str().c_str();
}

string Vector::toString(int precision) const {
    string result;
    toString(result, precision);
    return result;
}

void Vector::parseVector(const char* vector_literal) {
    if (!vector_literal) {
        invalid_vector();
        return;
    }
    
    // Parse format: [1.0,2.0,3.0] or (1.0,2.0,3.0)
    std::string input(vector_literal);
    
    // Remove whitespace
    input.erase(std::remove_if(input.begin(), input.end(), ::isspace), input.end());
    
    // Check for brackets or parentheses
    if (input.empty() || (input[0] != '[' && input[0] != '(')) {
        invalid_vector();
        return;
    }
    
    char close_char = (input[0] == '[') ? ']' : ')';
    if (input.back() != close_char) {
        invalid_vector();
        return;
    }
    
    // Extract content between brackets
    std::string content = input.substr(1, input.length() - 2);
    
    // Split by commas and parse values
    std::vector<double> values;
    std::istringstream ss(content);
    std::string token;
    
    while (std::getline(ss, token, ',')) {
        if (!token.empty()) {
            try {
                values.push_back(std::stod(token));
            } catch (const std::exception&) {
                invalid_vector();
                return;
            }
        }
    }
    
    if (values.empty()) {
        invalid_vector();
        return;
    }
    
    fromStdVector(values);
}

ULONG Vector::makeIndexKey(vary* buf) const {
    string str_repr = toCompactString();
    ULONG copy_length = std::min(static_cast<ULONG>(str_repr.length()),
                                static_cast<ULONG>(252 - sizeof(USHORT)));
    
    buf->vary_length = static_cast<USHORT>(copy_length);
    if (copy_length > 0) {
        memcpy(buf->vary_string, str_repr.c_str(), copy_length);
    }
    
    return sizeof(USHORT) + copy_length;
}

void Vector::resize(USHORT new_dimensions, double fill_value) {
    if (new_dimensions == dimensions_) return;
    
    Vector temp(new_dimensions);
    
    USHORT copy_count = std::min(dimensions_, new_dimensions);
    for (USHORT i = 0; i < copy_count; ++i) {
        temp[i] = getValue(i);
    }
    
    for (USHORT i = copy_count; i < new_dimensions; ++i) {
        temp[i] = fill_value;
    }
    
    *this = std::move(temp);
}

void Vector::clear() {
    deallocateStorage();
    dimensions_ = 0;
    format_ = VECTOR_DENSE;
}

void Vector::fill(double value) {
    for (USHORT i = 0; i < dimensions_; ++i) {
        setValue(i, value);
    }
}

std::vector<double> Vector::toStdVector() const {
    std::vector<double> result;
    result.reserve(dimensions_);
    for (USHORT i = 0; i < dimensions_; ++i) {
        result.push_back(getValue(i));
    }
    return result;
}

void Vector::fromStdVector(const std::vector<double>& vec) {
    allocateStorage(static_cast<USHORT>(vec.size()));
    for (size_t i = 0; i < vec.size(); ++i) {
        values_[i] = vec[i];
    }
}

void Vector::fromStdVector(const std::vector<float>& vec) {
    allocateStorage(static_cast<USHORT>(vec.size()));
    for (size_t i = 0; i < vec.size(); ++i) {
        values_[i] = static_cast<double>(vec[i]);
    }
}

// Private methods

void Vector::allocateStorage(USHORT dimensions) {
    if (dimensions == 0) {
        invalid_vector();
        return;
    }
    
    deallocateStorage();
    dimensions_ = dimensions;
    values_ = new double[dimensions];
    format_ = VECTOR_DENSE;
}

void Vector::deallocateStorage() {
    delete[] values_;
    values_ = nullptr;
    sparse_values_.clear();
}

void Vector::copyFrom(const Vector& other) {
    dimensions_ = other.dimensions_;
    format_ = other.format_;
    sparse_values_ = other.sparse_values_;
    
    if (other.values_) {
        values_ = new double[dimensions_];
        memcpy(values_, other.values_, dimensions_ * sizeof(double));
    } else {
        values_ = nullptr;
    }
}

void Vector::validateDimensions(const Vector& other) const {
    if (dimensions_ != other.dimensions_) {
        dimension_mismatch();
    }
}

void Vector::validateIndex(USHORT index) const {
    if (index >= dimensions_) {
        index_out_of_bounds();
    }
}

double Vector::computeDistance(const Vector& other, VectorSimilarityType type) const {
    validateDimensions(other);
    
    switch (type) {
        case VECTOR_L2_DISTANCE: {
            double sum = 0.0;
            for (USHORT i = 0; i < dimensions_; ++i) {
                double diff = getValue(i) - other.getValue(i);
                sum += diff * diff;
            }
            return std::sqrt(sum);
        }
        
        case VECTOR_L1_DISTANCE: {
            double sum = 0.0;
            for (USHORT i = 0; i < dimensions_; ++i) {
                sum += std::abs(getValue(i) - other.getValue(i));
            }
            return sum;
        }
        
        case VECTOR_COSINE_SIMILARITY:
            return 1.0 - computeSimilarity(other, VECTOR_COSINE_SIMILARITY);
            
        default:
            throw std::invalid_argument("Unsupported distance type");
    }
}

double Vector::computeSimilarity(const Vector& other, VectorSimilarityType type) const {
    validateDimensions(other);
    
    switch (type) {
        case VECTOR_COSINE_SIMILARITY: {
            double dot = dotProduct(other);
            double mag1 = magnitude();
            double mag2 = other.magnitude();
            
            if (mag1 < 1e-10 || mag2 < 1e-10) {
                return 0.0;
            }
            return dot / (mag1 * mag2);
        }
        
        case VECTOR_DOT_PRODUCT:
            return dotProduct(other);
            
        default:
            throw std::invalid_argument("Unsupported similarity type");
    }
}

void Vector::convertToDense() {
    if (format_ == VECTOR_DENSE) return;
    
    double* new_values = new double[dimensions_];
    std::fill(new_values, new_values + dimensions_, 0.0);
    
    for (const auto& pair : sparse_values_) {
        new_values[pair.first] = pair.second;
    }
    
    delete[] values_;
    values_ = new_values;
    format_ = VECTOR_DENSE;
    sparse_values_.clear();
}

void Vector::toCompactString(string& result) const {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(3);
    oss << "[";
    
    for (USHORT i = 0; i < std::min(dimensions_, static_cast<USHORT>(10)); ++i) {
        if (i > 0) oss << ",";
        oss << getValue(i);
    }
    
    if (dimensions_ > 10) {
        oss << "...";
    }
    oss << "]";
    
    result = oss.str().c_str();
}

string Vector::toCompactString() const {
    string result;
    toCompactString(result);
    return result;
}

void Vector::invalid_vector() {
    throw std::invalid_argument("Invalid VECTOR format");
}

void Vector::dimension_mismatch() {
    throw std::invalid_argument("Vector dimension mismatch");
}

void Vector::index_out_of_bounds() {
    throw std::out_of_range("Vector index out of bounds");
}

} // namespace ScratchBird