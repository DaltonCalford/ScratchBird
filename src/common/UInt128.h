/*
 *	PROGRAM:		Unsigned Integer 128 type.
 *	MODULE:			UInt128.h
 *	DESCRIPTION:	Big unsigned integer support.
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

#ifndef SB_UINT128
#define SB_UINT128

#include "firebird/Interface.h"
#include "sb_exception.h"

#include <string.h>

#include "classes/fb_string.h"

#ifdef FB_USE_ABSEIL_INT128

#include "absl/numeric/int128.h"

namespace ScratchBird {

class Decimal64;
class Decimal128;
struct DecimalStatus;

class UInt128
{
public:
#if SIZEOF_LONG < 8
	UInt128 set(unsigned int value, int scale)
	{
		return set(FB_UINT64(value), scale);
	}
#endif

	UInt128 set(ULONG value, int scale)
	{
		v = value;
		setScale(scale);
		return *this;
	}

	UInt128 set(FB_UINT64 value, int scale)
	{
		v = value;
		setScale(scale);
		return *this;
	}

	UInt128 set(double value)
	{
		v = absl::uint128(value);
		return *this;
	}

	// UInt128 set(DecimalStatus decSt, Decimal128 value);

	UInt128 set(UInt128 value)
	{
		v = value.v;
		return *this;
	}

	UInt128 operator=(FB_UINT64 value)
	{
		set(value, 0);
		return *this;
	}

#ifdef DEV_BUILD
	const char* show();
#endif

	unsigned int toUInteger(int scale) const
	{
		UInt128 tmp(*this);
		tmp.setScale(scale);
		unsigned int rc = static_cast<unsigned int>(tmp.v);
		if (tmp.v != rc)
			overflow();
		return rc;
	}

	FB_UINT64 toUInt64(int scale) const
	{
		UInt128 tmp(*this);
		tmp.setScale(scale);
		FB_UINT64 rc = static_cast<FB_UINT64>(tmp.v);
		if (tmp.v != rc)
			overflow();
		return rc;
	}

	void toString(int scale, unsigned length, char* to) const;
	void toString(int scale, string& to) const;

	double toDouble() const
	{
		return static_cast<double>(v);
	}

	UInt128 operator&=(FB_UINT64 mask)
	{
		v &= mask;
		return *this;
	}

	UInt128 operator&=(ULONG mask)
	{
		v &= mask;
		return *this;
	}

	UInt128 operator/(unsigned value) const
	{
		UInt128 rc;
		rc.v = v / value;
		return rc;
	}

	UInt128 operator+=(unsigned value)
	{
		v += value;
		return *this;
	}

	UInt128 operator-=(unsigned value)
	{
		v -= value;
		return *this;
	}

	UInt128 operator*=(unsigned value)
	{
		v *= value;
		return *this;
	}

	UInt128 operator<<(int value) const
	{
		UInt128 rc;
		rc.v = v << value;
		return rc;
	}

	UInt128 operator>>(int value) const
	{
		UInt128 rc;
		rc.v = v >> value;
		return rc;
	}

	int compare(UInt128 tgt) const
	{
		return v < tgt.v ? -1 : v > tgt.v ? 1 : 0;
	}

	bool operator>(UInt128 value) const
	{
		return v > value.v;
	}

	bool operator>=(UInt128 value) const
	{
		return v >= value.v;
	}

	bool operator==(UInt128 value) const
	{
		return v == value.v;
	}

	bool operator!=(UInt128 value) const
	{
		return v != value.v;
	}

	UInt128 operator&=(UInt128 value)
	{
		v &= value.v;
		return *this;
	}

	UInt128 operator|=(UInt128 value)
	{
		v |= value.v;
		return *this;
	}

	UInt128 operator^=(UInt128 value)
	{
		v ^= value.v;
		return *this;
	}

	UInt128 operator~() const
	{
		UInt128 rc;
		rc.v = ~v;
		return rc;
	}

	int sign() const
	{
		return v == 0 ? 0 : 1;  // Unsigned values are never negative
	}

	UInt128 add(UInt128 op2) const
	{
		UInt128 rc;
		rc.v = v + op2.v;

		// Check for unsigned overflow
		if (rc.v < v)  // Overflow occurred
			overflow();

		return rc;
	}

	UInt128 sub(UInt128 op2) const
	{
		UInt128 rc;
		if (v < op2.v)  // Would result in negative (underflow)
			overflow();
		
		rc.v = v - op2.v;
		return rc;
	}

	UInt128 mul(UInt128 op2) const
	{
		UInt128 rc;
		rc.v = v * op2.v;

		// Check for overflow (approximate check)
		if (op2.v != 0 && rc.v / op2.v != v)
			overflow();

		return rc;
	}

	UInt128 div(UInt128 op2, int scale) const;

	UInt128 mod(UInt128 op2) const
	{
		if (op2.v == 0)
			zerodivide();

		UInt128 rc;
		rc.v = v % op2.v;
		return rc;
	}

	void divMod(unsigned int divisor, unsigned int* remainder)
	{
		absl::uint128 d = divisor;
		*remainder = static_cast<unsigned int>(v % d);
		v /= d;
	}

	// returns internal data in per-32bit form
	void getTable32(unsigned* dwords) const
	{
		absl::uint128 vv = v;
		for (int i = 0; i < 4; ++i)
		{
			dwords[i] = static_cast<unsigned>(vv);
			vv >>= 32;
		}
	}

	void setScale(int scale);

	UCHAR* getBytes()
	{
		return (UCHAR*)(&v);
	}

	static const unsigned BIAS = 128;
	static const unsigned PMAX = 39;  // Maximum precision for decimal representation

	ULONG makeIndexKey(vary* buf, int scale);

	static ULONG getIndexKeyLength()
	{
		return 19;
	}

	// Maximum value for UInt128
	static UInt128 MAX_VALUE()
	{
		UInt128 result;
		result.v = absl::Uint128Max();
		return result;
	}

	UInt128 set(const char* value);

protected:
	absl::uint128 v;

	static void overflow();
	static void zerodivide();
};

class CUInt128 : public UInt128
{
public:
	enum minmax {MkMax, MkMin};

	CUInt128(FB_UINT64 value);
	CUInt128(minmax mm);
	CUInt128(const UInt128& value)
	{
		set(value);
	}
};

extern CUInt128 MAX_UInt128, MIN_UInt128;

class U128limit : public UInt128
{
public:
	U128limit()
	{
		v = 1;
		for (int i = 0; i < 127; ++i)  // Full 128 bits for unsigned
			v *= 2;
		v -= 1;  // Maximum unsigned value: 2^128 - 1
	}
};

} // namespace ScratchBird

#else // FB_USE_ABSEIL_INT128

#include "../../extern/ttmath/ttmath.h"

namespace ScratchBird {

class Decimal64;
class Decimal128;
struct DecimalStatus;

class UInt128
{
public:
	UInt128() {}
	UInt128(unsigned int value) { v = ttmath::UInt(value); }
	UInt128(const UInt128& other) { v = other.v; }
#if SIZEOF_LONG < 8
	UInt128 set(unsigned int value, int scale);
#endif
	UInt128 set(ULONG value, int scale);
	UInt128 set(FB_UINT64 value, int scale);
	UInt128 set(double value);
	// UInt128 set(DecimalStatus decSt, Decimal128 value);
	UInt128 set(UInt128 value)
	{
		v = value.v;
		return *this;
	}

	UInt128 operator=(FB_UINT64 value)
	{
		set(value, 0);
		return *this;
	}

#ifdef DEV_BUILD
	const char* show();
#endif

	unsigned int toUInteger(int scale) const;
	FB_UINT64 toUInt64(int scale) const;
	void toString(int scale, unsigned length, char* to) const;
	void toString(int scale, string& to) const;
	double toDouble() const;

	UInt128 operator&=(FB_UINT64 mask);
	UInt128 operator&=(ULONG mask);

	int compare(UInt128 tgt) const
	{
		return v < tgt.v ? -1 : v > tgt.v ? 1 : 0;
	}

	UInt128 operator/ (unsigned value) const
	{
		UInt128 rc (*this);
		rc.v = rc.v / ttmath::UInt(value);
		return rc;
	}

	UInt128 operator<< (int value) const
	{
		UInt128 rc (*this);
		// Simple left shift implementation
		for (int i = 0; i < value; ++i)
			rc.v = rc.v + rc.v;  // Multiply by 2
		return rc;
	}

	UInt128 operator>> (int value) const
	{
		UInt128 rc (*this);
		// Simple right shift implementation
		for (int i = 0; i < value; ++i)
			rc.v = rc.v / ttmath::UInt(2);
		return rc;
	}

	UInt128 operator&= (UInt128 value)
	{
		v.table[0] &= value.v.table[0];
		v.table[1] &= value.v.table[1];
		return *this;
	}

	UInt128 operator|= (UInt128 value)
	{
		v.table[0] |= value.v.table[0];
		v.table[1] |= value.v.table[1];
		return *this;
	}

	UInt128 operator^= (UInt128 value)
	{
		v.table[0] ^= value.v.table[0];
		v.table[1] ^= value.v.table[1];
		return *this;
	}

	UInt128 operator~ () const
	{
		UInt128 rc (*this);
		rc.v.table[0] = ~rc.v.table[0];
		rc.v.table[1] = ~rc.v.table[1];
		return rc;
	}

	UInt128 operator+= (unsigned value)
	{
		v = v + ttmath::UInt(value);
		return *this;
	}

	UInt128 operator-= (unsigned value)
	{
		ttmath::UInt val(value);
		if (v < val)  // Check for underflow
			overflow();
		v = v - val;
		return *this;
	}

	UInt128 operator*= (unsigned value)
	{
		ttmath::UInt old_v = v;
		v = v * ttmath::UInt(value);
		// Simple overflow check
		if (value != 0 && v < old_v)
			overflow();
		return *this;
	}

	bool operator> (UInt128 value) const
	{
		return v > value.v;
	}

	bool operator>= (UInt128 value) const
	{
		return v > value.v || v == value.v;
	}

	bool operator== (UInt128 value) const
	{
		return v == value.v;
	}

	bool operator!= (UInt128 value) const
	{
		return v != value.v;
	}

	UInt128 operator/ (UInt128 value) const
	{
		UInt128 rc(*this);
		rc.v = rc.v / value.v;
		return rc;
	}

	UInt128 operator* (UInt128 value) const
	{
		UInt128 rc(*this);
		rc.v = rc.v * value.v;
		return rc;
	}

	UInt128 operator- (UInt128 value) const
	{
		UInt128 rc(*this);
		rc.v = rc.v - value.v;
		return rc;
	}

	int sign() const
	{
		return (v.table[0] == 0 && v.table[1] == 0) ? 0 : 1;  // Unsigned values are never negative
	}

	UInt128 add(UInt128 op2) const
	{
		UInt128 rc(*this);
		ttmath::UInt old_v = rc.v;
		rc.v = rc.v + op2.v;
		if (rc.v < old_v)  // Overflow check
			overflow();
		return rc;
	}

	UInt128 sub(UInt128 op2) const
	{
		UInt128 rc(*this);
		if (rc.v < op2.v)  // Underflow check
			overflow();
		rc.v = rc.v - op2.v;
		return rc;
	}

	UInt128 mul(UInt128 op2) const;
	UInt128 div(UInt128 op2, int scale) const;

	UInt128 mod(UInt128 op2) const
	{
		if (op2.v.table[0] == 0 && op2.v.table[1] == 0)
			zerodivide();
		UInt128 dividend(*this);
		UInt128 quotient = dividend / op2;
		UInt128 rc = dividend - (quotient * op2);
		return rc;
	}

	void divMod(unsigned int divisor, unsigned int* remainder)
	{
		if (divisor == 0)
			zerodivide();
		ttmath::UInt div_val(divisor);
		UInt128 quotient = *this / UInt128(divisor);
		UInt128 rem = *this - (quotient * UInt128(divisor));
		v = quotient.v;
		*remainder = static_cast<unsigned int>(rem.v.table[0]);
	}

	void getTable32(unsigned* dwords) const;		// internal data in per-32bit form
	void setTable32(const unsigned* dwords);
	void setScale(int scale);

	UCHAR* getBytes()
	{
		return (UCHAR*)(v.table);
	}

	static const unsigned BIAS = 128;
	static const unsigned PMAX = 39;

	ULONG makeIndexKey(vary* buf, int scale);

	static ULONG getIndexKeyLength()
	{
		return 19;
	}

	// Maximum value for UInt128
	static UInt128 MAX_VALUE()
	{
		UInt128 result;
		result.v.table[0] = UINT64_MAX;
		result.v.table[1] = UINT64_MAX;
		return result;
	}

	UInt128 set(const char* value);

protected:
	ttmath::UInt v;

	static void overflow();
	static void zerodivide();
};

class CUInt128 : public UInt128
{
public:
	enum minmax {MkMax, MkMin};

	CUInt128(FB_UINT64 value);
	CUInt128(minmax mm);
	CUInt128(const UInt128& value)
	{
		set(value);
	}
};

extern CUInt128 MAX_UInt128, MIN_UInt128;

class U128limit : public UInt128
{
public:
	U128limit()
	{
		v.table[0] = UINT64_MAX;
		v.table[1] = UINT64_MAX;
	}
};

} // namespace ScratchBird

#endif // FB_USE_ABSEIL_INT128

#endif // SB_UINT128