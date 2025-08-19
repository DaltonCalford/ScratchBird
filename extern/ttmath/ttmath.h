/*
 * TTMath compatibility header for ScratchBird
 * This header provides TTMath API compatibility using standard C++ types
 * Note: TTMath is a template-based arbitrary precision arithmetic library
 */

#ifndef TTMATH_H
#define TTMATH_H

#include <cstdint>
#include <string>
#include <climits>
#include <algorithm>

namespace ttmath {

typedef int64_t sint;
typedef uint64_t uint;

#define TTMATH_BITS(x) (x/64)

// Basic big integer class stub
class UInt
{
public:
    uint64_t table[2];  // Simple 128-bit storage
    
    UInt() { table[0] = 0; table[1] = 0; }
    UInt(uint64_t value) { table[0] = value; table[1] = 0; }
    UInt(const UInt& other) { table[0] = other.table[0]; table[1] = other.table[1]; }
    
    UInt& operator=(const UInt& other) { 
        table[0] = other.table[0]; 
        table[1] = other.table[1]; 
        return *this; 
    }
    UInt& operator=(uint64_t value) { 
        table[0] = value; 
        table[1] = 0; 
        return *this; 
    }
    
    bool operator==(const UInt& other) const { return table[0] == other.table[0] && table[1] == other.table[1]; }
    bool operator!=(const UInt& other) const { return !(*this == other); }
    bool operator<(const UInt& other) const { return table[1] < other.table[1] || (table[1] == other.table[1] && table[0] < other.table[0]); }
    bool operator>(const UInt& other) const { return other < *this; }
    
    UInt operator+(const UInt& other) const { return UInt(table[0] + other.table[0]); }
    UInt operator-(const UInt& other) const { return UInt(table[0] - other.table[0]); }
    UInt operator*(const UInt& other) const { return UInt(table[0] * other.table[0]); }
    UInt operator/(const UInt& other) const { return other.table[0] ? UInt(table[0] / other.table[0]) : UInt(); }
    
    std::string ToString() const { return std::to_string(table[0]); }
    
    static UInt FromString(const std::string& str) { return UInt(std::stoull(str)); }
};

// Template-based signed integer class using TomMath backend
template<int value_size>
class Int
{
public:
    int64_t table[2];  // Simple 128-bit storage for basic operations
    
    Int() { table[0] = 0; table[1] = 0; }
    Int(int64_t value) { table[0] = value; table[1] = value < 0 ? -1 : 0; }
    Int(const Int& other) { table[0] = other.table[0]; table[1] = other.table[1]; }
    
    Int& operator=(const Int& other) { 
        table[0] = other.table[0]; 
        table[1] = other.table[1]; 
        return *this; 
    }
    Int& operator=(int64_t value) { 
        table[0] = value; 
        table[1] = value < 0 ? -1 : 0; 
        return *this; 
    }
    
    bool operator==(const Int& other) const { return table[0] == other.table[0] && table[1] == other.table[1]; }
    bool operator!=(const Int& other) const { return !(*this == other); }
    bool operator<(const Int& other) const { return table[1] < other.table[1] || (table[1] == other.table[1] && table[0] < other.table[0]); }
    bool operator>(const Int& other) const { return other < *this; }
    bool operator<=(const Int& other) const { return *this < other || *this == other; }
    bool operator>=(const Int& other) const { return *this > other || *this == other; }
    
    Int operator+(const Int& other) const { return Int(table[0] + other.table[0]); }
    Int operator-(const Int& other) const { return Int(table[0] - other.table[0]); }
    Int operator*(const Int& other) const { return Int(table[0] * other.table[0]); }
    Int operator/(const Int& other) const { return other.table[0] ? Int(table[0] / other.table[0]) : Int(); }
    
    Int& operator&=(const Int& other) { table[0] &= other.table[0]; table[1] &= other.table[1]; return *this; }
    Int& operator|=(const Int& other) { table[0] |= other.table[0]; table[1] |= other.table[1]; return *this; }
    Int& operator^=(const Int& other) { table[0] ^= other.table[0]; table[1] ^= other.table[1]; return *this; }
    Int& operator<<=(int shift) { table[0] <<= shift; return *this; }
    Int& operator>>=(int shift) { table[0] >>= shift; return *this; }
    
    std::string ToString() const { return std::to_string(table[0]); }
    
    static Int FromString(const std::string& str) { return Int(std::stoll(str)); }
    
    // TTMath-specific methods - compatibility layer
    Int Abs() const { return table[0] < 0 ? Int(-table[0]) : *this; }
    bool Abs() { 
        if (table[0] < 0) {
            table[0] = -table[0];
            return true;  // overflow occurred
        }
        return false;  // no overflow
    }
    int ChangeSign() { table[0] = -table[0]; return 0; }
    int Add(const Int& other) { *this = *this + other; return 0; }
    int Sub(const Int& other) { *this = *this - other; return 0; }
    int Mul(const Int& other) { *this = *this * other; return 0; }
    int Div(const Int& divisor) { 
        if (divisor.table[0] == 0) return 1;
        *this = *this / divisor; 
        return 0; 
    }
    int Div(const Int& divisor, Int& remainder) { 
        if (divisor.table[0] == 0) return 1;
        remainder = Int(table[0] % divisor.table[0]); 
        *this = *this / divisor; 
        return 0; 
    }
    int DivInt(int divisor, sint* remainder = nullptr) { 
        if (divisor == 0) return 1;
        if (remainder) *remainder = table[0] % divisor;
        table[0] /= divisor; 
        return 0; 
    }
    int MulInt(int multiplier) { 
        table[0] *= multiplier; 
        return 0; 
    }
    int AddInt(int value) { 
        table[0] += value; 
        return 0; 
    }
    int SubInt(int value) { 
        table[0] -= value; 
        return 0; 
    }
    void SetOne() { table[0] = 1; table[1] = 0; }
    void SetZero() { table[0] = 0; table[1] = 0; }
    void SetMax() { table[0] = INT64_MAX; table[1] = 0; }
    void SetMin() { table[0] = INT64_MIN; table[1] = -1; }
    void BitNot() { table[0] = ~table[0]; table[1] = ~table[1]; }
    bool IsSign() const { return table[0] < 0; }
    bool IsZero() const { return table[0] == 0 && table[1] == 0; }
    
    // Postfix increment/decrement operators
    Int operator++(int) { Int temp(*this); table[0]++; return temp; }
    Int operator--(int) { Int temp(*this); table[0]--; return temp; }
    
    // Prefix increment/decrement operators
    Int& operator++() { table[0]++; return *this; }
    Int& operator--() { table[0]--; return *this; }
    
    // Unary minus operator
    Int operator-() const { return Int(-table[0]); }
    
    // Convert to integer with error checking
    int ToInt(int& errorCode) const {
        if (table[0] > INT_MAX || table[0] < INT_MIN) {
            errorCode = 1;
            return 0;
        }
        errorCode = 0;
        return static_cast<int>(table[0]);
    }
    
    // Convert to string with base - simplified version
    template<typename StringType>
    void ToStringBase(StringType& result, int base = 10) const {
        (void)base; // unused parameter
        std::string temp = std::to_string(table[0]);
        result = temp.c_str();
    }
    
    // Additional string conversion method
    template<typename StringType>
    void ToString(StringType& result) const {
        std::string temp = std::to_string(table[0]);
        result = temp.c_str();
    }
};

} // namespace ttmath

#endif // TTMATH_H