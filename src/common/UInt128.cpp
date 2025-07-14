/*
 *	PROGRAM:		Unsigned Integer 128 type.
 *	MODULE:			UInt128.cpp
 *	DESCRIPTION:	Big unsigned integer implementation.
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
 *
 */

#include "../include/firebird.h"
#include "UInt128.h"
#include "DecFloat.h"
#include "../common/status.h"
#include <algorithm>
#include <sstream>
#include <stdexcept>

namespace ScratchBird {

#ifdef FB_USE_ABSEIL_INT128

// Abseil-based implementation

UInt128 UInt128::set(const char* value)
{
	if (!value)
		overflow();
	
	// Parse string representation
	absl::uint128 result = 0;
	const char* p = value;
	
	// Skip leading whitespace
	while (*p && isspace(*p))
		p++;
	
	// Parse digits
	while (*p && isdigit(*p))
	{
		absl::uint128 digit = *p - '0';
		absl::uint128 old_result = result;
		result = result * 10 + digit;
		
		// Check for overflow
		if (result < old_result)
			overflow();
		
		p++;
	}
	
	// Skip trailing whitespace
	while (*p && isspace(*p))
		p++;
	
	// Check for invalid characters
	if (*p)
		overflow();
	
	v = result;
	return *this;
}

void UInt128::toString(int scale, unsigned length, char* to) const
{
	if (!to)
		return;
	
	if (length == 0)
	{
		*to = '\0';
		return;
	}
	
	// Convert to string using absl
	std::string temp_str = absl::StrCat(v);
	
	// Apply scaling if needed
	if (scale != 0)
	{
		// Add decimal point and handle scaling
		string scaled_str;
		if (scale > 0)
		{
			// Positive scale - add zeros at the end
			scaled_str = temp_str + string(scale, '0');
		}
		else
		{
			// Negative scale - add decimal point
			int abs_scale = -scale;
			if (temp_str.length() <= abs_scale)
			{
				// Need leading zeros
				scaled_str = "0." + string(abs_scale - temp_str.length(), '0') + temp_str;
			}
			else
			{
				// Insert decimal point
				int pos = temp_str.length() - abs_scale;
				scaled_str = temp_str.substr(0, pos) + "." + temp_str.substr(pos);
			}
		}
		temp_str = scaled_str;
	}
	
	// Copy to output buffer
	unsigned copy_len = std::min(length - 1, static_cast<unsigned>(temp_str.length()));
	memcpy(to, temp_str.c_str(), copy_len);
	to[copy_len] = '\0';
}

void UInt128::toString(int scale, string& to) const
{
	// Convert to string using absl
	std::string temp_str = absl::StrCat(v);
	
	// Apply scaling if needed
	if (scale != 0)
	{
		// Add decimal point and handle scaling
		if (scale > 0)
		{
			// Positive scale - add zeros at the end
			std::string result = temp_str + std::string(scale, '0');
			to = result.c_str();
		}
		else
		{
			// Negative scale - add decimal point
			int abs_scale = -scale;
			if (temp_str.length() <= abs_scale)
			{
				// Need leading zeros
				std::string result = "0." + std::string(abs_scale - temp_str.length(), '0') + temp_str;
				to = result.c_str();
			}
			else
			{
				// Insert decimal point
				int pos = temp_str.length() - abs_scale;
				std::string result = temp_str.substr(0, pos) + "." + temp_str.substr(pos);
				to = result.c_str();
			}
		}
	}
	else
	{
		to = temp_str.c_str();
	}
}

UInt128 UInt128::div(UInt128 op2, int scale) const
{
	if (op2.v == 0)
		zerodivide();
	
	UInt128 result;
	
	// Handle scaling
	absl::uint128 dividend = v;
	for (int i = 0; i < scale; ++i)
	{
		absl::uint128 old_dividend = dividend;
		dividend *= 10;
		if (dividend < old_dividend)  // Overflow
			overflow();
	}
	
	result.v = dividend / op2.v;
	return result;
}

void UInt128::setScale(int scale)
{
	if (scale > 0)
	{
		// Multiply by 10^scale
		for (int i = 0; i < scale; ++i)
		{
			absl::uint128 old_v = v;
			v *= 10;
			if (v < old_v)  // Overflow
				overflow();
		}
	}
	else if (scale < 0)
	{
		// Divide by 10^(-scale)
		int abs_scale = -scale;
		for (int i = 0; i < abs_scale; ++i)
		{
			v /= 10;
		}
	}
}

ULONG UInt128::makeIndexKey(vary* buf, int scale)
{
	if (!buf)
		return 0;
	
	// Apply scale first
	UInt128 temp(*this);
	temp.setScale(scale);
	
	// Convert to bytes (big-endian for proper sorting)
	UCHAR* p = reinterpret_cast<UCHAR*>(buf->vary_string);
	
	// Store sign (always positive for unsigned)
	*p++ = 1;  // Positive
	
	// Store 128-bit value in big-endian order
	absl::uint128 val = temp.v;
	for (int i = 15; i >= 0; --i)
	{
		p[i] = static_cast<UCHAR>(val & 0xFF);
		val >>= 8;
	}
	
	buf->vary_length = 17;  // 1 byte sign + 16 bytes value
	return 17;
}

UInt128 UInt128::set(DecimalStatus decSt, Decimal128 value)
{
	// This would need implementation with Decimal128 integration
	// For now, simple conversion
	*this = static_cast<FB_UINT64>(0);  // Placeholder
	return *this;
}

void UInt128::overflow()
{
	throw std::overflow_error("UInt128 overflow");
}

void UInt128::zerodivide()
{
	throw std::runtime_error("UInt128 division by zero");
}

#ifdef DEV_BUILD
const char* UInt128::show()
{
	static char buffer[64];
	std::string temp_str = absl::StrCat(v);
	strncpy(buffer, temp_str.c_str(), sizeof(buffer) - 1);
	buffer[sizeof(buffer) - 1] = '\0';
	return buffer;
}
#endif

#else // FB_USE_ABSEIL_INT128

// TTMath-based implementation

#if SIZEOF_LONG < 8
UInt128 UInt128::set(unsigned int value, int scale)
{
	v = value;
	setScale(scale);
	return *this;
}
#endif

UInt128 UInt128::set(ULONG value, int scale)
{
	v = value;
	setScale(scale);
	return *this;
}

UInt128 UInt128::set(FB_UINT64 value, int scale)
{
	v = value;
	setScale(scale);
	return *this;
}

UInt128 UInt128::set(double value)
{
	v = static_cast<uint64_t>(value);
	return *this;
}

UInt128 UInt128::set(const char* value)
{
	if (!value)
		overflow();
	
	v = ttmath::UInt::FromString(std::string(value));
	return *this;
}

unsigned int UInt128::toUInteger(int scale) const
{
	UInt128 temp(*this);
	temp.setScale(scale);
	
	if (temp.v.table[1] != 0 || temp.v.table[0] > UINT_MAX)
		overflow();
	
	return static_cast<unsigned int>(temp.v.table[0]);
}

FB_UINT64 UInt128::toUInt64(int scale) const
{
	UInt128 temp(*this);
	temp.setScale(scale);
	
	if (temp.v.table[1] != 0)
		overflow();
	
	return temp.v.table[0];
}

void UInt128::toString(int scale, unsigned length, char* to) const
{
	if (!to)
		return;
	
	if (length == 0)
	{
		*to = '\0';
		return;
	}
	
	string temp_str;
	toString(scale, temp_str);
	
	// Copy to output buffer
	unsigned copy_len = std::min(length - 1, static_cast<unsigned>(temp_str.length()));
	memcpy(to, temp_str.c_str(), copy_len);
	to[copy_len] = '\0';
}

void UInt128::toString(int scale, string& to) const
{
	std::string temp_str = v.ToString();
	
	// Apply scaling if needed
	if (scale != 0)
	{
		if (scale > 0)
		{
			// Positive scale - add zeros at the end
			std::string result = temp_str + std::string(scale, '0');
			to = result.c_str();
		}
		else
		{
			// Negative scale - add decimal point
			int abs_scale = -scale;
			if (temp_str.length() <= abs_scale)
			{
				// Need leading zeros
				std::string result = "0." + std::string(abs_scale - temp_str.length(), '0') + temp_str;
				to = result.c_str();
			}
			else
			{
				// Insert decimal point
				int pos = temp_str.length() - abs_scale;
				std::string result = temp_str.substr(0, pos) + "." + temp_str.substr(pos);
				to = result.c_str();
			}
		}
	}
	else
	{
		to = temp_str.c_str();
	}
}

double UInt128::toDouble() const
{
	// Convert 128-bit value to double (may lose precision)
	return static_cast<double>(v.table[0]) + static_cast<double>(v.table[1]) * 18446744073709551616.0;
}

UInt128 UInt128::operator&=(FB_UINT64 mask)
{
	v.table[0] &= mask;
	v.table[1] = 0;  // Clear upper 64 bits
	return *this;
}

UInt128 UInt128::operator&=(ULONG mask)
{
	v.table[0] &= mask;
	v.table[1] = 0;  // Clear upper 64 bits
	return *this;
}

UInt128 UInt128::mul(UInt128 op2) const
{
	UInt128 result(*this);
	result.v = result.v * op2.v;
	return result;
}

UInt128 UInt128::div(UInt128 op2, int scale) const
{
	if (op2.v.table[0] == 0 && op2.v.table[1] == 0)
		zerodivide();
	
	UInt128 result(*this);
	
	// Handle scaling - multiply by 10^scale
	for (int i = 0; i < scale; ++i)
	{
		ttmath::UInt old_v = result.v;
		result.v = result.v * ttmath::UInt(10);
		if (result.v < old_v)  // Overflow check
			overflow();
	}
	
	result.v = result.v / op2.v;
	return result;
}

void UInt128::getTable32(unsigned* dwords) const
{
	// Convert from 2x64-bit to 4x32-bit layout
	dwords[0] = static_cast<unsigned>(v.table[0] & 0xFFFFFFFF);
	dwords[1] = static_cast<unsigned>(v.table[0] >> 32);
	dwords[2] = static_cast<unsigned>(v.table[1] & 0xFFFFFFFF);
	dwords[3] = static_cast<unsigned>(v.table[1] >> 32);
}

void UInt128::setTable32(const unsigned* dwords)
{
	// Convert from 4x32-bit to 2x64-bit layout
	v.table[0] = static_cast<uint64_t>(dwords[0]) | (static_cast<uint64_t>(dwords[1]) << 32);
	v.table[1] = static_cast<uint64_t>(dwords[2]) | (static_cast<uint64_t>(dwords[3]) << 32);
}

void UInt128::setScale(int scale)
{
	if (scale > 0)
	{
		// Multiply by 10^scale
		for (int i = 0; i < scale; ++i)
		{
			ttmath::UInt old_v = v;
			v = v * ttmath::UInt(10);
			if (v < old_v)  // Overflow check
				overflow();
		}
	}
	else if (scale < 0)
	{
		// Divide by 10^(-scale)
		int abs_scale = -scale;
		for (int i = 0; i < abs_scale; ++i)
		{
			v = v / ttmath::UInt(10);
		}
	}
}

ULONG UInt128::makeIndexKey(vary* buf, int scale)
{
	if (!buf)
		return 0;
	
	// Apply scale first
	UInt128 temp(*this);
	temp.setScale(scale);
	
	// Convert to bytes (big-endian for proper sorting)
	UCHAR* p = reinterpret_cast<UCHAR*>(buf->vary_string);
	
	// Store sign (always positive for unsigned)
	*p++ = 1;  // Positive
	
	// Store 128-bit value in big-endian order
	unsigned dwords[4];
	temp.getTable32(dwords);
	
	// Convert from little-endian dwords to big-endian bytes
	for (int i = 3; i >= 0; --i)
	{
		for (int j = 3; j >= 0; --j)
		{
			*p++ = static_cast<UCHAR>((dwords[i] >> (j * 8)) & 0xFF);
		}
	}
	
	buf->vary_length = 17;  // 1 byte sign + 16 bytes value
	return 17;
}

// UInt128 UInt128::set(DecimalStatus decSt, Decimal128 value)
// {
// 	// This would need implementation with Decimal128 integration
// 	// For now, simple conversion
// 	v = ttmath::UInt(0);  // Placeholder
// 	return *this;
// }

void UInt128::overflow()
{
	throw std::overflow_error("UInt128 overflow");
}

void UInt128::zerodivide()
{
	throw std::runtime_error("UInt128 division by zero");
}

#ifdef DEV_BUILD
const char* UInt128::show()
{
	static char buffer[64];
	std::string temp_str = v.ToString();
	strncpy(buffer, temp_str.c_str(), sizeof(buffer) - 1);
	buffer[sizeof(buffer) - 1] = '\0';
	return buffer;
}
#endif

#endif // FB_USE_ABSEIL_INT128

// Common implementation for both backends

CUInt128::CUInt128(FB_UINT64 value)
{
	set(value, 0);
}

CUInt128::CUInt128(minmax mm)
{
	if (mm == MkMax)
		*this = MAX_VALUE();
	else
		set(0ULL, 0);
}

// Global constants
CUInt128 MAX_UInt128(CUInt128::MkMax);
CUInt128 MIN_UInt128(CUInt128::MkMin);

} // namespace ScratchBird