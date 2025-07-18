/*
 *      PROGRAM:        JRD Access Method
 *      MODULE:         cvt.cpp
 *      DESCRIPTION:    Data mover and converter and comparator, etc.
 *
 * The contents of this file are subject to the Interbase Public
 * License Version 1.0 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy
 * of the License at http://www.Inprise.com/IPL.html
 *
 * Software distributed under the License is distributed on an
 * "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, either express
 * or implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code was created by Inprise Corporation
 * and its predecessors. Portions created by Inprise Corporation are
 * Copyright (C) Inprise Corporation.
 *
 * All Rights Reserved.
 * Contributor(s): ______________________________________.
 *
 * 2002.02.15 Sean Leyne - Code Cleanup, removed obsolete ports:
 *                          - DELTA and IMP
 *
 * 2001.6.16 Claudio Valderrama: Wiped out the leading space in
 * cast(float_expr as char(n)) in dialect 1, reported in SF.
 * 2001.11.19 Claudio Valderrama: integer_to_text() should use the
 * source descriptor "from" to call conversion_error.
 *
 * 2002.10.28 Sean Leyne - Completed removal of obsolete "DGUX" port
 *
 * 2008.07.09 Alex Peshkoff - moved part of CVT code to common part of engine
 *							  this avoids ugly export of CVT_move
 *
 */

#include "firebird.h"
#include <cmath>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "iberror.h"

#include "../jrd/constants.h"
#include "../common/intlobj_new.h"
#include "../common/gdsassert.h"
#include "../common/CharSet.h"
#include "../common/TimeZoneUtil.h"
#include "../common/classes/timestamp.h"
#include "../common/cvt.h"
#include "../jrd/intl.h"
#include "../jrd/constants.h"
#include "../common/classes/VaryStr.h"
#include "../common/classes/FpeControl.h"
#include "../common/dsc_proto.h"
#include "../common/utils_proto.h"
#include "../common/StatusArg.h"
#include "../common/status.h"
#include "../common/TimeZones.h"
#include "../common/UInt128.h"
#include "../common/InetAddr.h"


#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#ifdef HAVE_FLOAT_H
#include <float.h>
#else
#define DBL_MAX_10_EXP          308
#endif


using namespace ScratchBird;

/* normally the following two definitions are part of limits.h
   but due to a compiler bug on Apollo casting LONG_MIN to be a
   double, these have to be defined as double numbers...

   Also, since SunOS4.0 doesn't want to play with the rest of
   the ANSI world, these definitions have to be included explicitly.
   So, instead of some including <limits.h> and others using these
   definitions, just always use these definitions (huh?) */

#define LONG_MIN_real   -2147483648.	// min decimal value of an "SLONG"
#define LONG_MAX_real   2147483647.	// max decimal value of an "SLONG"
#define LONG_MIN_int    -2147483648	// min integer value of an "SLONG"
#define LONG_MAX_int    2147483647	// max integer value of an "SLONG"

// It turns out to be tricky to write the INT64 versions of those constant in
// a way that will do the right thing on all platforms.  Here we go.

#define LONG_MAX_int64 ((SINT64) 2147483647)	// max int64 value of an SLONG
#define LONG_MIN_int64 (-LONG_MAX_int64 - 1)	// min int64 value of an SLONG

#define QUAD_MIN_real   -9223372036854775808.	// min decimal value of quad
#define QUAD_MAX_real   9223372036854775807.	// max decimal value of quad

//#define QUAD_MIN_int    quad_min_int	// min integer value of quad
//#define QUAD_MAX_int    quad_max_int	// max integer value of quad

#ifdef FLT_MAX
#define FLOAT_MAX FLT_MAX // Approx. 3.4e38 max float (32 bit) value
#else
#define FLOAT_MAX 3.402823466E+38F // max float (32 bit) value
#endif

#define LETTER7_UPPER(c)      ((c) >= 'A' && (c) <= 'Z')
#define LETTER(c) (((c) >= 'a' && (c) <= 'z') || ((c) >= 'A' && (c) <= 'Z'))
#define DIGIT(c)        ((c) >= '0' && (c) <= '9')
#define ABSOLUT(x)      ((x) < 0 ? -(x) : (x))

/* The expressions for SHORT_LIMIT, LONG_LIMIT, INT64_LIMIT and
 * QUAD_LIMIT return the largest value that permit you to multiply by
 * 10 without getting an overflow.  The right operand of the << is two
 * less than the number of bits in the type: one bit is for the sign,
 * and the other is because we divide by 5, rather than 10.  */

const SSHORT SHORT_LIMIT = ((1 << 14) / 5);
const SLONG LONG_LIMIT = ((1L << 30) / 5);

// NOTE: The syntax for the below line may need modification to ensure
// the result of 1 << 62 is a quad

//#define QUAD_LIMIT      ((((SINT64) 1) << 62) / 5)
const SINT64 INT64_LIMIT = ((((SINT64) 1) << 62) / 5);

// Unsigned integer limits for adjustForScale  
const ULONG ULONG_LIMIT = ((((ULONG) 1) << 30) / 5);
const FB_UINT64 UINT64_LIMIT = ((((FB_UINT64) 1) << 62) / 5);

#define TODAY           "TODAY"
#define NOW             "NOW"
#define TOMORROW        "TOMORROW"
#define YESTERDAY       "YESTERDAY"

#define CVT_COPY_BUFF(from, to, len) \
{if (len) {memcpy(to, from, len); from += len; to += len;} }
// AP,2012: Looks like there is no need making len zero, but I keep old define for a reference.
// {if (len) {memcpy(to, from, len); from += len; to += len; len = 0;} }

static void datetime_to_text(const dsc*, dsc*, Callbacks*);
static void float_to_text(const dsc*, dsc*, Callbacks*);
static void decimal_float_to_text(const dsc*, dsc*, DecimalStatus, Callbacks*);
static void integer_to_text(const dsc*, dsc*, Callbacks*);
static void int128_to_text(const dsc*, dsc*, Callbacks* cb);
static void localError(const ScratchBird::Arg::StatusVector&);
static SSHORT cvt_get_short(const dsc* desc, SSHORT scale, DecimalStatus decSt, ErrorFunction err);
static void make_null_string(const dsc*, USHORT, const char**, vary*, USHORT, ScratchBird::DecimalStatus, ErrorFunction);

namespace {
	class RetPtr;
}
static SSHORT cvt_decompose(const char*, USHORT, RetPtr* return_value, ErrorFunction err);

class DummyException {};

//#ifndef WORDS_BIGENDIAN
//static const SQUAD quad_min_int = { 0, SLONG_MIN };
//static const SQUAD quad_max_int = { -1, SLONG_MAX };
//#else
//static const SQUAD quad_min_int = { SLONG_MIN, 0 };
//static const SQUAD quad_max_int = { SLONG_MAX, -1 };
//#endif


namespace {

class RetPtr
{
public:
	virtual ~RetPtr() { }

	enum lb10 {RETVAL_OVERFLOW, RETVAL_POSSIBLE_OVERFLOW, RETVAL_NO_OVERFLOW};

	virtual USHORT maxSize() = 0;
	virtual void truncate8() = 0;
	virtual void truncate16() = 0;
	virtual lb10 compareLimitBy10() = 0;
	virtual void nextDigit(unsigned digit, unsigned base) = 0;
	virtual bool isLowerLimit() = 0;
	virtual void neg() = 0;
};

template <class Traits>
class RetValue : public RetPtr
{
public:
	RetValue(typename Traits::ValueType* ptr)
		: return_value(ptr)
	{
		value = 0;
	}

	~RetValue()
	{
		*return_value = value;
	}

	USHORT maxSize() override
	{
		return sizeof(typename Traits::ValueType);
	}

	void truncate8() override
	{
		ULONG mask = 0xFFFFFFFF;
		value &= mask;
	}

	void truncate16() override
	{
		FB_UINT64 mask = 0xFFFFFFFFFFFFFFFF;
		value &= mask;
	}

	lb10 compareLimitBy10() override
	{
		if (static_cast<typename Traits::UnsignedType>(value) > Traits::UPPER_LIMIT_BY_10)
			return RETVAL_OVERFLOW;
		if (static_cast<typename Traits::UnsignedType>(value) == Traits::UPPER_LIMIT_BY_10)
			return RETVAL_POSSIBLE_OVERFLOW;
		return RETVAL_NO_OVERFLOW;
	}

	void nextDigit(unsigned digit, unsigned base) override
	{
		value *= base;
		value += digit;
	}

	bool isLowerLimit() override
	{
		return value == Traits::LOWER_LIMIT;
	}

	void neg() override
	{
		value = -value;
	}

protected:
	typename Traits::ValueType value;
	typename Traits::ValueType* return_value;
};

} // anonymous namespace

static const double eps_double = 1e-14;
static const double eps_float  = 1e-5;


static void validateTimeStamp(const ISC_TIMESTAMP timestamp, const EXPECT_DATETIME expectedType, const dsc* desc,
	Callbacks* cb)
{
	if (!NoThrowTimeStamp::isValidTimeStamp(timestamp))
	{
		switch (expectedType)
		{
			case expect_sql_date:
				cb->err(Arg::Gds(isc_date_range_exceeded));
				break;
			case expect_sql_time:
			case expect_sql_time_tz:
				cb->err(Arg::Gds(isc_time_range_exceeded));
				break;
			case expect_timestamp:
			case expect_timestamp_tz:
				cb->err(Arg::Gds(isc_datetime_range_exceeded));
				break;
			default: // this should never happen!
				CVT_conversion_error(desc, cb->err);
				break;
		}
	}
}

static void timeStampToUtc(ISC_TIMESTAMP_TZ& timestampTZ, USHORT sessionTimeZone, const EXPECT_DATETIME expectedType,
	Callbacks* cb)
{
	if (expectedType == expect_sql_time_tz || expectedType == expect_timestamp_tz || timestampTZ.time_zone != sessionTimeZone)
		TimeZoneUtil::localTimeStampToUtc(timestampTZ);

	if (timestampTZ.time_zone != sessionTimeZone)
	{
		if (expectedType == expect_sql_time)
		{
			ISC_TIME_TZ timeTz;
			timeTz.utc_time = timestampTZ.utc_timestamp.timestamp_time;
			timeTz.time_zone = timestampTZ.time_zone;
			timestampTZ.utc_timestamp.timestamp_time = TimeZoneUtil::timeTzToTime(timeTz, cb);
		}
		else if (expectedType == expect_timestamp)
			*(ISC_TIMESTAMP*) &timestampTZ = TimeZoneUtil::timeStampTzToTimeStamp(timestampTZ, sessionTimeZone);
	}
}


static void float_to_text(const dsc* from, dsc* to, Callbacks* cb)
{
/**************************************
 *
 *      f l o a t _ t o _ t e x t
 *
 **************************************
 *
 * Functional description
 *      Convert an arbitrary floating type number to text.
 *      To avoid messiness, convert first into a temp (fixed) then
 *      move to the destination.
 *      Never print more digits than the source type can actually
 *      provide: instead pad the output with blanks on the right if
 *      needed.
 *
 **************************************/
	double d;
	char temp[] = "-1.234567890123456E-300";

	const int to_len = DSC_string_length(to); // length of destination
	const int width = MIN(to_len, (int) sizeof(temp) - 1); // minimum width to print

	int precision;
	if (dtype_double == from->dsc_dtype)
	{
		precision = 16;			// minimum significant digits in a double
		d = *(double*) from->dsc_address;
	}
	else
	{
		fb_assert(dtype_real == from->dsc_dtype);
		precision = 8;			// minimum significant digits in a float
		d = (double) *(float*) from->dsc_address;
	}

	// If this is a double with non-zero scale, then it is an old-style
	// NUMERIC(15, -scale): print it in fixed format with -scale digits
	// to the right of the ".".

	// CVC: Here sprintf was given an extra space in the two formatting
	// masks used below, "%- #*.*f" and "%- #*.*g" but certainly with positive
	// quantities and CAST it yields an annoying leading space.
	// However, by getting rid of the space you get in dialect 1:
	// cast(17/13 as char(5))  => 1.308
	// cast(-17/13 as char(5)) => -1.31
	// Since this is inconsistent with dialect 3, see workaround at the tail
	// of this function.

	int chars_printed;			// number of characters printed
	if ((dtype_double == from->dsc_dtype) && (from->dsc_scale < 0))
	{
		chars_printed = fb_utils::snprintf(temp, sizeof(temp), "%- #*.*f", width, -from->dsc_scale, d);
		if (chars_printed <= 0 || chars_printed > width)
			chars_printed = -1;
	}
	else
		chars_printed = -1;

	// If it's not an old-style numeric, or the f-format was too long for the
	// destination, try g-format with the maximum precision which makes sense
	// for the input type: if it fits, we're done.

	if (chars_printed == -1)
	{
		char temp2[50];
		const char num_format[] = "%- #*.*g";
		chars_printed = fb_utils::snprintf(temp2, sizeof(temp2), num_format, width, precision, d);
		if (chars_printed <= 0 || static_cast<unsigned int>(chars_printed) >= sizeof(temp2))
			cb->err(Arg::Gds(isc_arith_except) << Arg::Gds(isc_numeric_out_of_range));

		// If the full-precision result is too wide for the destination,
		// reduce the precision and try again.

		if (chars_printed > width)
		{
			precision -= (chars_printed - width);

			// If we cannot print at least two digits, one on each side of the
			// ".", report an overflow exception.
			if (precision < 2)
				cb->err(Arg::Gds(isc_arith_except) << Arg::Gds(isc_numeric_out_of_range));

			chars_printed = fb_utils::snprintf(temp2, sizeof(temp2), num_format, width, precision, d);
			if (chars_printed <= 0 || static_cast<unsigned int>(chars_printed) >= sizeof(temp2))
				cb->err(Arg::Gds(isc_arith_except) << Arg::Gds(isc_numeric_out_of_range));

			// It's possible that reducing the precision caused sprintf to switch
			// from f-format to e-format, and that the output is still too long
			// for the destination.  If so, reduce the precision once more.
			// This is certain to give a short-enough result.

			if (chars_printed > width)
			{
				precision -= (chars_printed - width);
				if (precision < 2)
					cb->err(Arg::Gds(isc_arith_except) << Arg::Gds(isc_numeric_out_of_range));
				// Note: we use here temp2 with sizeof(temp) because temp2 is bigger than temp.
				// The check should be chars_printed > width because it's our last chance to
				// fit into "width" else we should throw error.
				chars_printed = fb_utils::snprintf(temp2, sizeof(temp), num_format, width, precision, d);
				if (chars_printed <= 0 || chars_printed > width)
					cb->err(Arg::Gds(isc_arith_except) << Arg::Gds(isc_numeric_out_of_range));
			}
		}

		memcpy(temp, temp2, sizeof(temp));
	}
	fb_assert(chars_printed <= width);

	// trim trailing spaces
	const char* p = strchr(temp + 1, ' ');
	if (p)
		chars_printed = p - temp;

	// Now move the result to the destination array.

	dsc intermediate;
	intermediate.dsc_dtype = dtype_text;
	intermediate.dsc_ttype() = ttype_ascii;
	// CVC: If you think this is dangerous, replace the "else" with a call to
	// MEMMOVE(temp, temp + 1, chars_printed) or something cleverer.
	// Paranoid assumption:
	// UCHAR is unsigned char as seen on common/common.h => same size.
	if (d < 0)
	{
		intermediate.dsc_address = reinterpret_cast<UCHAR*>(temp);
		intermediate.dsc_length = chars_printed;
	}
	else
	{
		fb_assert(chars_printed > 0);
		if (!temp[0])
			temp[1] = 0;
		intermediate.dsc_address = reinterpret_cast<UCHAR*>(temp) + 1;
		intermediate.dsc_length = chars_printed - 1;
	}

	CVT_move_common(&intermediate, to, 0, cb);
}


static void decimal_float_to_text(const dsc* from, dsc* to, DecimalStatus decSt, Callbacks* cb)
{
	char temp[50];

	try
	{
		if (from->dsc_dtype == dtype_dec64)
			((Decimal64*) from->dsc_address)->toString(decSt, sizeof(temp), temp);
		else if (from->dsc_dtype == dtype_dec128)
			((Decimal128*) from->dsc_address)->toString(decSt, sizeof(temp), temp);
		else
			fb_assert(false);
	}
	catch (const Exception& ex)
	{
		// reraise using function passed in callbacks
		Arg::StatusVector v(ex);
		cb->err(v);
	}

	dsc intermediate;
	intermediate.dsc_dtype = dtype_text;
	intermediate.dsc_ttype() = ttype_ascii;
	intermediate.dsc_address = reinterpret_cast<UCHAR*>(temp);
	intermediate.dsc_length = strlen(temp);

	CVT_move_common(&intermediate, to, 0, cb);
}


static void int128_to_text(const dsc* from, dsc* to, Callbacks* cb)
{
	char temp[50];

	try
	{
		if (from->dsc_dtype == dtype_int128)
			((Int128*) from->dsc_address)->toString(from->dsc_scale, sizeof(temp), temp);
		else
			fb_assert(false);
	}
	catch (const Exception& ex)
	{
		// reraise using function passed in callbacks
		Arg::StatusVector v(ex);
		cb->err(v);
	}

	dsc intermediate;
	intermediate.dsc_dtype = dtype_text;
	intermediate.dsc_ttype() = ttype_ascii;
	intermediate.dsc_address = reinterpret_cast<UCHAR*>(temp);
	intermediate.dsc_length = strlen(temp);

	CVT_move_common(&intermediate, to, 0, cb);
}


static void integer_to_text(const dsc* from, dsc* to, Callbacks* cb)
{
/**************************************
 *
 *      i n t e g e r _ t o _ t e x t
 *
 **************************************
 *
 * Functional description
 *      Convert your basic binary number to
 *      nice, formatted text.
 *
 **************************************/

	// For now, this routine does not handle quadwords unless this is
	// supported by the platform as a native datatype.

	if (from->dsc_dtype == dtype_quad)
	{
		fb_assert(false);
		cb->err(Arg::Gds(isc_badblk));	// internal error
	}

	SSHORT pad_count = 0, decimal = 0, neg = 0;

	// Save (or compute) scale of source.  Then convert source to ordinary
	// longword or int64.

	SCHAR scale = from->dsc_scale;

	if (scale > 0)
		pad_count = scale;
	else if (scale < 0)
		decimal = 1;

	SINT64 n;
	dsc intermediate;
	MOVE_CLEAR(&intermediate, sizeof(intermediate));
	intermediate.dsc_dtype = dtype_int64;
	intermediate.dsc_length = sizeof(n);
	intermediate.dsc_scale = scale;
	intermediate.dsc_address = (UCHAR*) &n;

	CVT_move_common(from, &intermediate, 0, cb);

	// Check for negation, then convert the number to a string of digits

	FB_UINT64 u;
	if (n >= 0)
		u = n;
	else
	{
		neg = 1;
		u = -n;
	}

    string temp;

	do {
		temp += (u % 10) + '0';
		u /= 10;
	} while (u);

	SSHORT l = (SSHORT) temp.length();

	// if scale < 0, we need at least abs(scale)+1 digits, so add
	// any leading zeroes required.
	while (l + scale <= 0)
	{
		temp += '0';
		l++;
	}
	// postassertion: l+scale > 0
	fb_assert(l + scale > 0);

	// Compute the total length of the field formatted.  Make sure it
	// fits.  Keep in mind that routine handles both string and varying
	// string fields.

	USHORT length = l + neg + decimal + pad_count;

	if ((to->dsc_dtype == dtype_text && length > to->dsc_length) ||
		(to->dsc_dtype == dtype_cstring && length >= to->dsc_length) ||
		(to->dsc_dtype == dtype_varying && length > (to->dsc_length - sizeof(USHORT))))
	{
	    CVT_conversion_error(from, cb->err);
	}

	const UCHAR* p = (UCHAR*) temp.c_str() + temp.length();
	UCHAR* q = (to->dsc_dtype == dtype_varying) ? to->dsc_address + sizeof(USHORT) : to->dsc_address;
	const UCHAR* start = q;

	// If negative, put in minus sign

	if (neg)
		*q++ = '-';

	// If a decimal point is required, do the formatting. Otherwise just copy number.

	if (scale >= 0)
	{
		do {
			*q++ = *--p;
		} while (--l);
	}
	else
	{
		l += scale;	// l > 0 (see postassertion: l + scale > 0 above)
		do {
			*q++ = *--p;
		} while (--l);
		*q++ = '.';
		do {
			*q++ = *--p;
		} while (++scale);
	}

	length = cb->validateLength(cb->getToCharset(to->getCharSet()), to->getCharSet(), length, start, TEXT_LEN(to));

	// If padding is required, do it now.

	if (pad_count)
	{
		do {
			*q++ = '0';
		} while (--pad_count);
	}

	// Finish up by padding (if fixed) or computing the actual length (varying string).

	if (to->dsc_dtype == dtype_text)
	{
		ULONG trailing = ULONG(to->dsc_length) - length;
		if (trailing > 0)
		{
			CHARSET_ID chid = cb->getChid(to); // : DSC_GET_CHARSET(to);

			const char pad = chid == ttype_binary ? '\0' : ' ';
			memset(q, pad, trailing);
		}
		return;
	}

	if (to->dsc_dtype == dtype_cstring)
	{
		*q = 0;
		return;
	}

	// dtype_varying
	*(USHORT*) (to->dsc_address) = static_cast<USHORT>(q - to->dsc_address - sizeof(SSHORT));
}


void CVT_string_to_datetime(const dsc* desc,
							   ISC_TIMESTAMP_TZ* date, bool* timezone_present,
							   const EXPECT_DATETIME expect_type, bool allow_special, Callbacks* cb)
{
/**************************************
 *
 *      s t r i n g _ t o _ d a t e t i m e
 *
 **************************************
 *
 * Functional description
 *      Convert an arbitrary string to a date and/or time.
 *
 *      String must be formed using ASCII characters only.
 *      Conversion routine can handle the following input formats
 *      "now"           current date & time
 *      "today"         Today's date       0:0:0.0 time
 *      "tomorrow"      Tomorrow's date    0:0:0.0 time
 *      "yesterday"     Yesterday's date   0:0:0.0 time
 *      YYYY-MM-DD [HH:[Min:[SS.[Thou]]]]]
 *      MM:DD[:YY [HH:[Min:[SS.[Thou]]]]]
 *      DD.MM[:YY [HH:[Min:[SS.[Thou]]]]]
 *      Where:
 *        DD = 1  .. 31    (Day of month)
 *        YY = 00 .. 99    2-digit years are converted to the nearest year
 *		           in a 50 year range.  Eg: if this is 1996
 *                              96 ==> 1996
 *                              97 ==> 1997
 *                              ...
 *                              00 ==> 2000
 *                              01 ==> 2001
 *                              ...
 *                              44 ==> 2044
 *                              45 ==> 2045
 *                              46 ==> 1946
 *                              47 ==> 1947
 *                              ...
 *                              95 ==> 1995
 *                         If the current year is 1997, then 46 is converted
 *                         to 2046 (etc).
 *           = 100.. 5200  (Year)
 *        MM = 1  .. 12    (Month of year)
 *           = "JANUARY"... (etc)
 *        HH = 0  .. 23    (Hour of day)
 *       Min = 0  .. 59    (Minute of hour)
 *        SS = 0  .. 59    (Second of minute - LEAP second not supported)
 *      Thou = 0  .. 9999  (Fraction of second)
 *      HH, Min, SS, Thou default to 0 if missing.
 *      YY defaults to current year if missing.
 *      Note: ANY punctuation can be used instead of : (eg: / - etc)
 *            Using . (period) in either of the first two separation
 *            points will cause the date to be parsed in European DMY
 *            format.
 *            Arbitrary whitespace (space or TAB) can occur between
 *            components.
 *
 **************************************/

	// Values inside of description
	// > 0 is number of digits
	//   0 means missing
	// ENGLISH_MONTH for the presence of an English month name
	// SPECIAL       for a special date verb
	const int ENGLISH_MONTH	= -1;
	const int SPECIAL		= -2; // CVC: I see it set, but never tested.

	unsigned int position_year = 0;
	unsigned int position_month = 1;
	unsigned int position_day = 2;
	bool have_english_month = false;
	char date_sep = '\0';
	VaryStr<TEMP_STR_LENGTH> buffer;			// arbitrarily large

	const char* p = NULL;
	const USHORT length = CVT_make_string(desc, ttype_ascii, &p, &buffer, sizeof(buffer), 0, cb->err);

	const char* const end = p + length;

	USHORT n, components[7];
	int description[7];
	memset(components, 0, sizeof(components));
	memset(description, 0, sizeof(description));

	if (timezone_present)
		*timezone_present = false;

	// Parse components
	// The 7 components are Year, Month, Day, Hours, Minutes, Seconds, Thou
	// The first 3 can be in any order

	const int start_component = (expect_type == expect_sql_time || expect_type == expect_sql_time_tz) ? 3 : 0;
	int i;

	for (i = start_component; i < 7; i++)
	{
		// Skip leading blanks.  If we run out of characters, we're done
		// with parse.

		while (p < end && (*p == ' ' || *p == '\t'))
			p++;
		if (p == end)
			break;

		// Handle digit or character strings

		TEXT c = UPPER7(*p);
		if (DIGIT(c))
		{
			USHORT precision = 0;
			n = 0;
			while (p < end && DIGIT(*p))
			{
				n = n * 10 + *p++ - '0';
				precision++;
			}
			description[i] = precision;
		}
		else if (LETTER7_UPPER(c) && !have_english_month && i - start_component < 2)
		{
			TEXT temp[sizeof(YESTERDAY) + 1];

			TEXT* t = temp;
			while ((p < end) && (t < &temp[sizeof(temp) - 1]))
			{
				c = UPPER7(*p);
				if (!LETTER7_UPPER(c))
					break;
				*t++ = c;
				p++;
			}
			*t = 0;

			// Insist on at least 3 characters for month names
			if (t - temp < 3)
			{
				CVT_conversion_error(desc, cb->err);
				return;
			}

			const TEXT* const* month_ptr = FB_LONG_MONTHS_UPPER;
			while (true)
			{
				// Month names are only allowed in first 2 positions
				if (*month_ptr && i < 2)
				{
					t = temp;
					const TEXT* m = *month_ptr++;
					while (*t && *t == *m)
					{
						++t;
						++m;
					}
					if (!*t)
						break;
				}
				else
				{
					// it's not a month name, so it's either a magic word or
					// a non-date string.  If there are more characters, it's bad

					if (!allow_special || i != start_component)
						CVT_conversion_error(desc, cb->err);

					description[i] = SPECIAL;

					while (++p < end)
					{
						if (*p != ' ' && *p != '\t' && *p != '\0')
							CVT_conversion_error(desc, cb->err);
					}

					// fetch the current datetime
					*date = TimeZoneUtil::getCurrentGmtTimeStamp();
					date->time_zone = cb->getSessionTimeZone();

					switch (expect_type)
					{
						case expect_sql_time_tz:
							date->utc_timestamp.timestamp_time =
								TimeZoneUtil::timeStampTzToTimeTz(*date).utc_time;
							break;

						case expect_sql_date:
						case expect_sql_time:
						case expect_timestamp:
							// Not really UTC.
							date->utc_timestamp = TimeZoneUtil::timeStampTzToTimeStamp(
								*date, cb->getSessionTimeZone());
							break;

						case expect_timestamp_tz:
							break;
					}

					if (strcmp(temp, NOW) == 0)
						return;

					if (expect_type == expect_sql_time || expect_type == expect_sql_time_tz)
					{
						CVT_conversion_error(desc, cb->err);
						return;
					}

					date->utc_timestamp.timestamp_time = 0;

					if (strcmp(temp, TODAY) == 0)
						return;

					if (strcmp(temp, TOMORROW) == 0)
					{
						++date->utc_timestamp.timestamp_date;
						return;
					}

					if (strcmp(temp, YESTERDAY) == 0)
					{
						--date->utc_timestamp.timestamp_date;
						return;
					}

					CVT_conversion_error(desc, cb->err);
					return;
				}
			}
			n = month_ptr - FB_LONG_MONTHS_UPPER;
			position_month = i;
			description[i] = ENGLISH_MONTH;
			have_english_month = true;
		}
		else
		{
			if (expect_type == expect_sql_date || i != 3)
				CVT_conversion_error(desc, cb->err);

			// May be a timezone after a date when expecting a timestamp.
			--i;
			break;
		}

		components[i] = n;

		bool hadSpace = false;

		// Grab whitespace following the number
		while (p < end && (*p == ' ' || *p == '\t'))
		{
			p++;
			hadSpace = true;
		}

		if (p == end)
			break;

		// Grab a separator character
		if (i <= 1)
		{
			if (date_sep == '\0' || (date_sep != ' ' && *p == date_sep) || (date_sep == ' ' && hadSpace))
			{
				if (date_sep != ' ' && (*p == '/' || *p == '-' || *p == '.'))
				{
					date_sep = *p++;
					continue;
				}
				else if (hadSpace)
				{
					date_sep = ' ';
					continue;
				}
			}
		}
		else if (i == 2)
			continue;
		else if (i >= 3 && i <= 5)
		{
			if (*p == ':')
			{
				p++;
				continue;
			}
			else if (*p == '.')
			{
				p++;
				i = 6 - 1;
				continue;
			}
			else
			{
				i = 7;
				break;
			}
		}
		else if (i == 6)
			break;

		CVT_conversion_error(desc, cb->err);
		return;
	}

	// User must provide at least 2 components
	if (i - start_component < 1)
	{
		CVT_conversion_error(desc, cb->err);
		return;
	}

	// Dates cannot have a Time portion
	if (expect_type == expect_sql_date && i > 2)
	{
		CVT_conversion_error(desc, cb->err);
		return;
	}

	USHORT sessionTimeZone = cb->getSessionTimeZone();
	USHORT zone = sessionTimeZone;

	if (expect_type != expect_sql_date)
	{
		while (p < end && (*p == ' ' || *p == '\t'))
			p++;

		if (p < end)
		{
			zone = TimeZoneUtil::parse(p, end - p);

			if (timezone_present)
				*timezone_present = true;
		}
	}
	else
	{
		// We won't allow random trash after the recognized string
		while (p < end)
		{
			if (*p != ' ' && *p != '\t' && *p != '\0')
			{
				CVT_conversion_error(desc, cb->err);
				return;
			}
			++p;
		}
	}

	tm times;
	memset(&times, 0, sizeof(times));

	if (expect_type != expect_sql_time && expect_type != expect_sql_time_tz)
	{
		// Figure out what format the user typed the date in

		if (description[0] >= 3)
		{
			// A 4 digit number to start implies YYYY-MM-DD
			position_year = 0;
			position_month = 1;
			position_day = 2;
		}
		else if (description[0] == ENGLISH_MONTH)
		{
			// An English month to start implies MM-DD-YYYY
			position_year = 2;
			position_month = 0;
			position_day = 1;
		}
		else if (description[1] == ENGLISH_MONTH)
		{
			// An English month in the middle implies DD-MM-YYYY
			position_year = 2;
			position_month = 1;
			position_day = 0;
		}
		else if (date_sep == '.')
		{
			// A period as a separator implies DD.MM.YYYY
			position_year = 2;
			position_month = 1;
			position_day = 0;
		}
		else
		{
			// Otherwise assume MM-DD-YYYY
			position_year = 2;
			position_month = 0;
			position_day = 1;
		}

		// Forbid years with more than 4 digits
		// Forbid months or days with more than 2 digits
		// Forbid months or days being missing
		if (description[position_year] > 4 ||
			description[position_month] > 2 || description[position_month] == 0 ||
			description[position_day] > 2 || description[position_day] <= 0)
		{
			CVT_conversion_error(desc, cb->err);
			return;
		}

		// Slide things into day, month, year form

		times.tm_year = components[position_year];
		times.tm_mon = components[position_month];
		times.tm_mday = components[position_day];

		// Fetch current date/time
		tm times2;
		ScratchBird::TimeStamp::getCurrentTimeStamp().decode(&times2);

		// Handle defaulting of year

		if (description[position_year] == 0) {
			times.tm_year = times2.tm_year + 1900;
		}
		else if (description[position_year] <= 2)
		{
			// Handle conversion of 2-digit years
			if (times.tm_year < (times2.tm_year - 50) % 100)
				times.tm_year += 2000;
			else
				times.tm_year += 1900;
		}

		times.tm_year -= 1900;
		times.tm_mon -= 1;
	}
	else
	{
		// Fetch current date
		tm times2;
		TimeStamp baseTimeStamp;
		baseTimeStamp.value().timestamp_date = TimeZoneUtil::TIME_TZ_BASE_DATE;
		baseTimeStamp.value().timestamp_time = 0;
		baseTimeStamp.decode(&times2);

		times.tm_year = times2.tm_year;
		times.tm_mon = times2.tm_mon;
		times.tm_mday = times2.tm_mday;
	}

	// Handle time values out of range - note possibility of 60 for
	// seconds value due to LEAP second (Not supported in V4.0).
	if (i > 2 &&
		(((times.tm_hour = components[3]) > 23) ||
			((times.tm_min = components[4]) > 59) ||
			((times.tm_sec = components[5]) > 59) ||
			description[3] > 2 || description[3] == 0 ||
			description[4] > 2 || description[4] == 0 ||
			description[5] > 2 ||
			description[6] > -ISC_TIME_SECONDS_PRECISION_SCALE))
	{
		CVT_conversion_error(desc, cb->err);
	}

	// convert day/month/year to Julian and validate result
	// This catches things like 29-Feb-1995 (not a leap year)

	ScratchBird::TimeStamp ts(times);
	validateTimeStamp(ts.value(), expect_type, desc, cb);

	if (expect_type != expect_sql_time && expect_type != expect_sql_time_tz)
	{
		tm times2;
		ts.decode(&times2);

		if (times.tm_year != times2.tm_year ||
			times.tm_mon != times2.tm_mon ||
			times.tm_mday != times2.tm_mday ||
			times.tm_hour != times2.tm_hour ||
			times.tm_min != times2.tm_min ||
			times.tm_sec != times2.tm_sec)
		{
			CVT_conversion_error(desc, cb->err);
		}
	}

	*(ISC_TIMESTAMP*) date = ts.value();

	// Convert fraction of seconds
	while (description[6]++ < -ISC_TIME_SECONDS_PRECISION_SCALE)
		components[6] *= 10;

	date->utc_timestamp.timestamp_time += components[6];
	date->time_zone = zone;

	timeStampToUtc(*date, sessionTimeZone, expect_type, cb);
}


template <typename V>
void adjustForScale(V& val, SSHORT scale, const V limit, ErrorFunction err)
{
	if (scale > 0)
	{
		int fraction = 0;
		do {
			if (scale == 1)
				fraction = int(val % 10);
			val /= 10;
		} while (--scale);

		if (fraction > 4)
			val++;
		// The following 2 lines are correct for platforms where
		// ((-85 / 10 == -8) && (-85 % 10 == -5)).  If we port to
		// a platform where ((-85 / 10 == -9) && (-85 % 10 == 5)),
		// we'll have to change this depending on the platform.
		else if (fraction < -4)
			val--;
	}
	else if (scale < 0)
	{
		do {
			if ((val > limit) || (val < -limit))
				err(Arg::Gds(isc_arith_except) << Arg::Gds(isc_numeric_out_of_range));
			val *= 10;
		} while (++scale);
	}
}


class SSHORTTraits
{
public:
	typedef SSHORT ValueType;
	typedef USHORT UnsignedType;
	static const USHORT UPPER_LIMIT_BY_10 = MAX_SSHORT / 10;
	static const SSHORT LOWER_LIMIT = MIN_SSHORT;
};

static SSHORT cvt_get_short(const dsc* desc, SSHORT scale, DecimalStatus decSt, ErrorFunction err)
{
/**************************************
 *
 *      C V T _ g e t _ s h o r t
 *
 **************************************
 *
 * Functional description
 *      Convert something arbitrary to a short (16 bit) integer of given
 *      scale.
 *
 **************************************/
	SSHORT value;

	if (desc->isText())
	{
		VaryStr<20> buffer;			// long enough to represent largest short in ASCII
		const char* p;
		USHORT length = CVT_make_string(desc, ttype_ascii, &p, &buffer, sizeof(buffer), decSt, err);

		{
			RetValue<SSHORTTraits> rv(&value);
			scale -= cvt_decompose(p, length, &rv, err);
		}
		adjustForScale(value, scale, SHORT_LIMIT, err);
	}
	else {
		ULONG lval = CVT_get_long(desc, scale, decSt, err);
		value = (SSHORT) lval;
		if (value != SLONG(lval))
			err(Arg::Gds(isc_arith_except) << Arg::Gds(isc_numeric_out_of_range));
	}

	return value;
}


class SLONGTraits
{
public:
	typedef SLONG ValueType;
	typedef ULONG UnsignedType;
	static const ULONG UPPER_LIMIT_BY_10 = MAX_SLONG / 10;
	static const SLONG LOWER_LIMIT = MIN_SLONG;
};

SLONG CVT_get_long(const dsc* desc, SSHORT scale, DecimalStatus decSt, ErrorFunction err)
{
/**************************************
 *
 *      C V T _ g e t _ l o n g
 *
 **************************************
 *
 * Functional description
 *      Convert something arbitrary to a long (32 bit) integer of given
 *      scale.
 *
 **************************************/
	SLONG value, high;

	double d, eps;
	Decimal128 d128;
	SINT64 val64;
	VaryStr<50> buffer;			// long enough to represent largest long in ASCII

	// adjust exact numeric values to same scaling

	if (DTYPE_IS_EXACT(desc->dsc_dtype))
		scale -= desc->dsc_scale;

	const char* p = reinterpret_cast<char*>(desc->dsc_address);

	switch (desc->dsc_dtype)
	{
	case dtype_short:
		value = *((SSHORT *) p);
		break;

	case dtype_long:
		value = *((SLONG *) p);
		break;

	case dtype_int64:
		val64 = *((SINT64 *) p);

		// adjust for scale first, *before* range-checking the value.
		adjustForScale(val64, scale, INT64_LIMIT, err);
		if ((val64 > LONG_MAX_int64) || (val64 < LONG_MIN_int64))
			err(Arg::Gds(isc_arith_except) << Arg::Gds(isc_numeric_out_of_range));
		return (SLONG) val64;

	case dtype_quad:
		value = ((SLONG *) p)[LOW_WORD];
		high = ((SLONG *) p)[HIGH_WORD];
		if ((value >= 0 && !high) || (value < 0 && high == -1))
			break;
		err(Arg::Gds(isc_arith_except) << Arg::Gds(isc_numeric_out_of_range));
		break;

	case dtype_dec64:
	case dtype_dec128:
		if (desc->dsc_dtype == dtype_dec64)
			d128 = *((Decimal64*) p);
		else
			d128 = *((Decimal128*) p);

		return d128.toInteger(decSt, scale);

	case dtype_int128:
		return ((Int128*) p)->toInteger(scale);

	case dtype_real:
	case dtype_double:
		if (desc->dsc_dtype == dtype_real)
		{
			d = *((float*) p);
			eps = eps_float;
		}
		else // if (desc->dsc_dtype == DEFAULT_DOUBLE)
		{
			d = *((double*) p);
			eps = eps_double;
		}

		if (scale > 0)
			d /= CVT_power_of_ten(scale);
		else if (scale < 0)
			d *= CVT_power_of_ten(-scale);

		if (d > 0)
			d += 0.5 + eps;
		else
			d -= 0.5 + eps;

		// make sure the cast will succeed - different machines
		// do different things if the value is larger than a long can hold
		// If rounding would yield a legitimate value, permit it

		if (d < (double) LONG_MIN_real)
		{
			if (d > (double) LONG_MIN_real - 1.)
				return SLONG_MIN;
			err(Arg::Gds(isc_arith_except) << Arg::Gds(isc_numeric_out_of_range));
		}
		if (d > (double) LONG_MAX_real)
		{
			if (d < (double) LONG_MAX_real + 1.)
				return LONG_MAX_int;
			err(Arg::Gds(isc_arith_except) << Arg::Gds(isc_numeric_out_of_range));
		}
		return (SLONG) d;

	case dtype_varying:
	case dtype_cstring:
	case dtype_text:
		{
			USHORT length =
				CVT_make_string(desc, ttype_ascii, &p, &buffer, sizeof(buffer), decSt, err);

			RetValue<SLONGTraits> rv(&value);
			scale -= cvt_decompose(p, length, &rv, err);
		}
		break;

	default:
		CVT_conversion_error(desc, err);
		break;
	}

	// Last, but not least, adjust for scale
	adjustForScale(value, scale, LONG_LIMIT, err);

	return value;
}


// Get the value of a boolean descriptor.
bool CVT_get_boolean(const dsc* desc, ErrorFunction err)
{
	switch (desc->dsc_dtype)
	{
		case dtype_boolean:
			return *desc->dsc_address != '\0';

		case dtype_varying:
		case dtype_cstring:
		case dtype_text:
		{
			VaryStr<TEMP_STR_LENGTH> buffer;	// arbitrarily large
			const char* p = NULL;
			int len = CVT_make_string(desc, ttype_ascii, &p, &buffer, sizeof(buffer), 0, err);

			// Remove heading and trailing spaces.

			while (len > 0 && fb_utils::isspace(*p))
			{
				++p;
				--len;
			}

			while (len > 0 && fb_utils::isspace(p[len - 1]))
				--len;

			if (len == 4 && fb_utils::strnicmp(p, "TRUE", len) == 0)
				return true;
			else if (len == 5 && fb_utils::strnicmp(p, "FALSE", len) == 0)
				return false;

			// fall into
		}

		default:
			CVT_conversion_error(desc, err);
			return false;	// silence warning
	}
}


// Get the value of a UUID descriptor.
void CVT_get_uuid(const dsc* desc, UCHAR* uuid_bytes, ErrorFunction err)
{
	switch (desc->dsc_dtype)
	{
		case dtype_uuid:
			memcpy(uuid_bytes, desc->dsc_address, 16);
			return;

		case dtype_varying:
		case dtype_cstring:
		case dtype_text:
		{
			VaryStr<40> buffer;
			const char* p = NULL;
			int len = CVT_make_string(desc, ttype_ascii, &p, &buffer, sizeof(buffer), 0, err);
			
			// Simple UUID string validation and conversion
			if (len != 36)
				CVT_conversion_error(desc, err);
			
			int pos = 0;
			for (int i = 0; i < len && pos < 16; i++)
			{
				if (p[i] != '-')  // Skip hyphens
				{
					int nibble1, nibble2;
					if (p[i] >= '0' && p[i] <= '9') nibble1 = p[i] - '0';
					else if (p[i] >= 'A' && p[i] <= 'F') nibble1 = p[i] - 'A' + 10;
					else if (p[i] >= 'a' && p[i] <= 'f') nibble1 = p[i] - 'a' + 10;
					else CVT_conversion_error(desc, err);
					
					i++;
					if (i >= len) CVT_conversion_error(desc, err);
					
					if (p[i] >= '0' && p[i] <= '9') nibble2 = p[i] - '0';
					else if (p[i] >= 'A' && p[i] <= 'F') nibble2 = p[i] - 'A' + 10;
					else if (p[i] >= 'a' && p[i] <= 'f') nibble2 = p[i] - 'a' + 10;
					else CVT_conversion_error(desc, err);
					
					uuid_bytes[pos++] = (nibble1 << 4) | nibble2;
				}
			}
			if (pos != 16) CVT_conversion_error(desc, err);
			return;
		}

		default:
			CVT_conversion_error(desc, err);
			return;
	}
}


// Get the value of a JSON descriptor as string.
USHORT CVT_get_json_string(const dsc* desc, const char** address, vary* temp, USHORT length, ErrorFunction err)
{
	switch (desc->dsc_dtype)
	{
		case dtype_json:
			// For now, treat JSON as text - full implementation would handle JSON-specific operations
			*address = reinterpret_cast<const char*>(desc->dsc_address);
			return desc->dsc_length;

		case dtype_varying:
		case dtype_cstring:
		case dtype_text:
			return CVT_make_string(desc, ttype_utf8, address, temp, length, 0, err);

		default:
			CVT_conversion_error(desc, err);
			return 0;
	}
}


double CVT_get_double(const dsc* desc, DecimalStatus decSt, ErrorFunction err, bool* getNumericOverflow)
{
/**************************************
 *
 *      C V T _ g e t _ d o u b l e
 *
 **************************************
 *
 * Functional description
 *      Convert something arbitrary to a double precision number
 *
 **************************************/
	double value;

	switch (desc->dsc_dtype)
	{
	case dtype_short:
		value = *((SSHORT *) desc->dsc_address);
		break;

	case dtype_long:
		value = *((SLONG *) desc->dsc_address);
		break;

	case dtype_quad:
		value = ((SLONG *) desc->dsc_address)[HIGH_WORD];
		value *= -((double) LONG_MIN_real);
		if (value < 0)
			value -= ((ULONG *) desc->dsc_address)[LOW_WORD];
		else
			value += ((ULONG *) desc->dsc_address)[LOW_WORD];
		break;

	case dtype_int64:
		value = (double) *((SINT64 *) desc->dsc_address);
		break;

	case dtype_real:
		return *((float*) desc->dsc_address);

	case DEFAULT_DOUBLE:
		// memcpy is done in case dsc_address is on a platform dependant
		// invalid alignment address for doubles
		memcpy(&value, desc->dsc_address, sizeof(double));
		return value;

	case dtype_dec64:
	case dtype_dec128:
		{
			Decimal128 d128;
			if (desc->dsc_dtype == dtype_dec64)
				d128 = *((Decimal64*) desc->dsc_address);
			else
				d128 = *((Decimal128*) desc->dsc_address);

			return d128.toDouble(decSt);
		}

	case dtype_int128:
		value = ((Int128*) desc->dsc_address)->toDouble();
		break;

	case dtype_varying:
	case dtype_cstring:
	case dtype_text:
		{
			VaryStr<TEMP_STR_LENGTH> buffer;	// must hold ascii of largest double
			const char* p;

			const USHORT length =
				CVT_make_string(desc, ttype_ascii, &p, &buffer, sizeof(buffer), decSt, err);
			value = 0.0;
			int scale = 0;
			SSHORT sign = 0;
			bool digit_seen = false, past_sign = false, fraction = false;
			const char* const end = p + length;

			// skip initial spaces
			while (p < end && *p == ' ')
				++p;

			for (; p < end; p++)
			{
				if (DIGIT(*p))
				{
					digit_seen = true;
					past_sign = true;
					if (fraction)
						scale++;
					value = value * 10. + (*p - '0');
				}
				else if (*p == '.')
				{
					past_sign = true;
					if (fraction)
						CVT_conversion_error(desc, err);
					else
						fraction = true;
				}
				else if (!past_sign && *p == '-')
				{
					sign = -1;
					past_sign = true;
				}
				else if (!past_sign && *p == '+')
				{
					sign = 1;
					past_sign = true;
				}
				else if (*p == 'e' || *p == 'E')
					break;
				else if (*p == ' ')
				{
					// skip spaces
					while (p < end && *p == ' ')
						++p;

					// throw if there is something after the spaces
					if (p < end)
						CVT_conversion_error(desc, err);
				}
				else
					CVT_conversion_error(desc, err);
			}

			// If we didn't see a digit then must be a funny string like "    ".
			if (!digit_seen)
				CVT_conversion_error(desc, err);

			if (sign == -1)
				value = -value;

			// If there's still something left, there must be an explicit exponent

			if (p < end)
			{
				digit_seen = false;
				sign = 0;
				SSHORT exp = 0;
				for (p++; p < end; p++)
				{
					if (DIGIT(*p))
					{
						digit_seen = true;
						exp = exp * 10 + *p - '0';

						// The following is a 'safe' test to prevent overflow of
						// exp here and of scale below. A more precise test occurs
						// later in this routine.

						if (exp >= SHORT_LIMIT)
						{
							if (getNumericOverflow)
							{
								*getNumericOverflow = true;
								return 0;
							}

							err(Arg::Gds(isc_arith_except) << Arg::Gds(isc_numeric_out_of_range));
						}
					}
					else if (*p == '-' && !digit_seen && !sign)
						sign = -1;
					else if (*p == '+' && !digit_seen && !sign)
						sign = 1;
					else if (*p == ' ')
					{
						// skip spaces
						while (p < end && *p == ' ')
							++p;

						// throw if there is something after the spaces
						if (p < end)
							CVT_conversion_error(desc, err);
					}
					else
						CVT_conversion_error(desc, err);
				}
				if (!digit_seen)
					CVT_conversion_error(desc, err);

				if (sign == -1)
					scale += exp;
				else
					scale -= exp;
			}

			// if the scale is greater than the power of 10 representable
			// in a double number, then something has gone wrong... let
			// the user know...

			if (ABSOLUT(scale) > DBL_MAX_10_EXP)
			{
				if (getNumericOverflow)
				{
					*getNumericOverflow = true;
					return 0;
				}

				err(Arg::Gds(isc_arith_except) << Arg::Gds(isc_numeric_out_of_range));
			}

			//  Repeated division is a good way to mung the least significant bits
			//  of your value, so we have replaced this iterative multiplication/division
			//  by a single multiplication or division, depending on sign(scale).
			//if (scale > 0)
		 	//	do value /= 10.; while (--scale);
		 	//else if (scale)
		 	//	do value *= 10.; while (++scale);
			if (scale > 0)
				value /= CVT_power_of_ten(scale);
			else if (scale < 0)
				value *= CVT_power_of_ten(-scale);

			if (std::isinf(value))
			{
				if (getNumericOverflow)
				{
					*getNumericOverflow = true;
					return 0;
				}

				err(Arg::Gds(isc_arith_except) << Arg::Gds(isc_numeric_out_of_range));
			}
		}
		return value;

	default:
		CVT_conversion_error(desc, err);
		break;
	}

	// Last, but not least, adjust for scale

	const int dscale = desc->dsc_scale;
	if (dscale == 0)
		return value;

	// if the scale is greater than the power of 10 representable
	// in a double number, then something has gone wrong... let
	// the user know...

	if (ABSOLUT(dscale) > DBL_MAX_10_EXP)
		err(Arg::Gds(isc_arith_except) << Arg::Gds(isc_numeric_out_of_range));

	if (dscale > 0)
		value *= CVT_power_of_ten(dscale);
	else if (dscale < 0)
		value /= CVT_power_of_ten(-dscale);

	return value;
}


void CVT_move_common(const dsc* from, dsc* to, DecimalStatus decSt, Callbacks* cb, bool trustedSource)
{
/**************************************
 *
 *      C V T _ m o v e _ c o m m o n
 *
 **************************************
 *
 * Functional description
 *      Move (and possible convert) something to something else.
 *
 **************************************/
	ULONG length = from->dsc_length;
	UCHAR* p = to->dsc_address;
	const UCHAR* q = from->dsc_address;

	// If the datatypes and lengths are identical, just move the
	// stuff byte by byte.  Although this may seem slower than
	// optimal, it would cost more to find the fast move than the
	// fast move would gain.

	// But do not do it for strings because their length has not been validated until this moment
	// (real source length must be validated against target maximum length
	// and this is the first common place where both are present).

	// ...unless these strings are coming from a trusted source (for example a cached record buffer)

	if (DSC_EQUIV(from, to, false) && (trustedSource || !DTYPE_IS_TEXT(from->dsc_dtype)))
	{
		if (length) {
			memcpy(p, q, length);
		}
		return;
	}

	// Special optimization case: RDB$DB_KEY is binary compatible with CHAR(8) OCTETS

	if ((from->dsc_dtype == dtype_text &&
		 to->dsc_dtype == dtype_dbkey &&
		 from->dsc_ttype() == ttype_binary &&
		 from->dsc_length == to->dsc_length) ||
		(to->dsc_dtype == dtype_text &&
		 from->dsc_dtype == dtype_dbkey &&
		 to->dsc_ttype() == ttype_binary &&
		 from->dsc_length == to->dsc_length))
	{
		memcpy(p, q, length);
		return;
	}

	// Do data type by data type conversions.  Not all are supported,
	// and some will drop out for additional handling.

	dsc d;
	switch (to->dsc_dtype)
	{
	case dtype_timestamp:
		switch (from->dsc_dtype)
		{
		case dtype_varying:
		case dtype_cstring:
		case dtype_text:
			{
				ISC_TIMESTAMP_TZ date;
				CVT_string_to_datetime(from, &date, NULL, expect_timestamp, true, cb);
				*((GDS_TIMESTAMP*) to->dsc_address) = *(GDS_TIMESTAMP*) &date;
			}
			return;

		case dtype_sql_date:
			((GDS_TIMESTAMP *) (to->dsc_address))->timestamp_date = *(GDS_DATE *) from->dsc_address;
			((GDS_TIMESTAMP *) (to->dsc_address))->timestamp_time = 0;
			return;

		case dtype_sql_time:
			((GDS_TIMESTAMP*) (to->dsc_address))->timestamp_date = 0;
			((GDS_TIMESTAMP*) (to->dsc_address))->timestamp_time = *(GDS_TIME*) from->dsc_address;
			((GDS_TIMESTAMP*) (to->dsc_address))->timestamp_date = cb->getLocalDate();
			return;

		case dtype_sql_time_tz:
		case dtype_ex_time_tz:
			*(ISC_TIMESTAMP*) to->dsc_address =
				TimeZoneUtil::timeTzToTimeStamp(*(ISC_TIME_TZ*) from->dsc_address, cb);
			return;

		case dtype_timestamp_tz:
		case dtype_ex_timestamp_tz:
			*(ISC_TIMESTAMP*) to->dsc_address =
				TimeZoneUtil::timeStampTzToTimeStamp(*(ISC_TIMESTAMP_TZ*) from->dsc_address, cb);
			return;

		default:
			CVT_conversion_error(from, cb->err);
			break;
		}
		break;

	case dtype_ex_timestamp_tz:
		d.makeTimestampTz((ISC_TIMESTAMP_TZ*) to->dsc_address);
		CVT_move_common(from, &d, decSt, cb);
		TimeZoneUtil::extractOffset(*(ISC_TIMESTAMP_TZ*) to->dsc_address,
			&((ISC_TIMESTAMP_TZ_EX*) to->dsc_address)->ext_offset);
		return;

	case dtype_ex_time_tz:
		d.makeTimeTz((ISC_TIME_TZ*) to->dsc_address);
		CVT_move_common(from, &d, decSt, cb);
		TimeZoneUtil::extractOffset(*(ISC_TIME_TZ*) to->dsc_address, &((ISC_TIME_TZ_EX*) to->dsc_address)->ext_offset);
		return;

	case dtype_timestamp_tz:
		switch (from->dsc_dtype)
		{
		case dtype_varying:
		case dtype_cstring:
		case dtype_text:
			{
				ISC_TIMESTAMP_TZ date;
				CVT_string_to_datetime(from, &date, NULL, expect_timestamp_tz, true, cb);
				*((ISC_TIMESTAMP_TZ*) to->dsc_address) = date;
			}
			return;

		case dtype_sql_time:
			*(ISC_TIMESTAMP_TZ*) to->dsc_address =
				TimeZoneUtil::timeToTimeStampTz(*(ISC_TIME*) from->dsc_address, cb);
			return;

		case dtype_ex_time_tz:
		case dtype_sql_time_tz:
			*((ISC_TIMESTAMP_TZ*) to->dsc_address) =
				TimeZoneUtil::timeTzToTimeStampTz(*(ISC_TIME_TZ*) from->dsc_address, cb);
			return;

		case dtype_ex_timestamp_tz:
			*((ISC_TIMESTAMP_TZ*) to->dsc_address) = *((ISC_TIMESTAMP_TZ*) from->dsc_address);
			return;

		case dtype_sql_date:
			*(ISC_TIMESTAMP_TZ*) to->dsc_address =
				TimeZoneUtil::dateToTimeStampTz(*(GDS_DATE*) from->dsc_address, cb);
			return;

		case dtype_timestamp:
			*(ISC_TIMESTAMP_TZ*) to->dsc_address =
				TimeZoneUtil::timeStampToTimeStampTz(*(ISC_TIMESTAMP*) from->dsc_address, cb);
			return;

		default:
			CVT_conversion_error(from, cb->err);
			break;
		}
		break;

	case dtype_sql_date:
		switch (from->dsc_dtype)
		{
		case dtype_varying:
		case dtype_cstring:
		case dtype_text:
			{
				ISC_TIMESTAMP_TZ date;
				CVT_string_to_datetime(from, &date, NULL, expect_sql_date, true, cb);
				*((GDS_DATE*) to->dsc_address) = date.utc_timestamp.timestamp_date;
			}
			return;

		case dtype_timestamp:
			*((GDS_DATE *) to->dsc_address) = ((GDS_TIMESTAMP *) from->dsc_address)->timestamp_date;
			return;

		case dtype_timestamp_tz:
		case dtype_ex_timestamp_tz:
			*(GDS_DATE*) to->dsc_address =
				TimeZoneUtil::timeStampTzToTimeStamp(*(ISC_TIMESTAMP_TZ*) from->dsc_address, cb).timestamp_date;
			return;

		default:
			CVT_conversion_error(from, cb->err);
			break;
		}
		break;

	case dtype_sql_time:
		switch (from->dsc_dtype)
		{
		case dtype_varying:
		case dtype_cstring:
		case dtype_text:
			{
				ISC_TIMESTAMP_TZ date;
				CVT_string_to_datetime(from, &date, NULL, expect_sql_time, true, cb);
				*((GDS_TIME *) to->dsc_address) = date.utc_timestamp.timestamp_time;
			}
			return;

		case dtype_sql_time_tz:
		case dtype_ex_time_tz:
			*(ISC_TIME*) to->dsc_address = TimeZoneUtil::timeTzToTime(*(ISC_TIME_TZ*) from->dsc_address, cb);
			return;

		case dtype_timestamp:
			*((GDS_TIME *) to->dsc_address) = ((GDS_TIMESTAMP *) from->dsc_address)->timestamp_time;
			return;

		case dtype_timestamp_tz:
		case dtype_ex_timestamp_tz:
			*(GDS_TIME*) to->dsc_address =
				TimeZoneUtil::timeStampTzToTimeStamp(*(ISC_TIMESTAMP_TZ*) from->dsc_address, cb).timestamp_time;
			return;

		default:
			CVT_conversion_error(from, cb->err);
			break;
		}
		break;

	case dtype_sql_time_tz:
		switch (from->dsc_dtype)
		{
		case dtype_varying:
		case dtype_cstring:
		case dtype_text:
			{
				ISC_TIMESTAMP_TZ date;
				CVT_string_to_datetime(from, &date, NULL, expect_sql_time_tz, true, cb);
				((ISC_TIME_TZ*) to->dsc_address)->utc_time = date.utc_timestamp.timestamp_time;
				((ISC_TIME_TZ*) to->dsc_address)->time_zone = date.time_zone;
			}
			return;

		case dtype_sql_time:
			*(ISC_TIME_TZ*) to->dsc_address = TimeZoneUtil::timeToTimeTz(*(ISC_TIME*) from->dsc_address, cb);
			return;

		case dtype_timestamp:
			*(ISC_TIME_TZ*) to->dsc_address =
				TimeZoneUtil::timeStampToTimeTz(*(ISC_TIMESTAMP*) from->dsc_address, cb);
			return;

		case dtype_timestamp_tz:
		case dtype_ex_timestamp_tz:
			*(ISC_TIME_TZ*) to->dsc_address =
				TimeZoneUtil::timeStampTzToTimeTz(*(ISC_TIMESTAMP_TZ*) from->dsc_address);
			return;

		case dtype_ex_time_tz:
			*(ISC_TIME_TZ*) to->dsc_address = *(ISC_TIME_TZ*) from->dsc_address;
			 return;

		default:
			CVT_conversion_error(from, cb->err);
			break;
		}
		break;

	case dtype_varying:
		MOVE_CLEAR(to->dsc_address, to->dsc_length);
		// fall through ...
	case dtype_text:
	case dtype_cstring:
		switch (from->dsc_dtype)
		{
		case dtype_dbkey:
			{
				USHORT len = to->getStringLength();
				UCHAR* ptr = to->dsc_address;

				if (to->dsc_dtype == dtype_varying)
					ptr += sizeof(USHORT);

				CharSet* charSet = cb->getToCharset(to->getCharSet());
				UCHAR maxBytesPerChar = charSet ? charSet->maxBytesPerChar() : 1;

				if (len / maxBytesPerChar < from->dsc_length)
				{
					cb->err(Arg::Gds(isc_arith_except) << Arg::Gds(isc_string_truncation) <<
						Arg::Gds(isc_trunc_limits) << Arg::Num(len / maxBytesPerChar) <<
						Arg::Num(from->dsc_length));
				}

				cb->validateData(charSet, from->dsc_length, from->dsc_address);

				memcpy(ptr, from->dsc_address, from->dsc_length);
				len -= from->dsc_length;
				ptr += from->dsc_length;

				switch (to->dsc_dtype)
				{
				case dtype_text:
					if (len > 0)
					{
						memset(ptr, 0, len);	// Always PAD with nulls, not spaces
					}
					break;

				case dtype_cstring:
					// Note: Following is only correct for narrow and
					// multibyte character sets which use a zero
					// byte to represent end-of-string
					*ptr = 0;
					break;

				case dtype_varying:
					((vary*) (to->dsc_address))->vary_length = from->dsc_length;
					break;
				}
			}
			return;

		case dtype_varying:
		case dtype_cstring:
		case dtype_text:
			{
				/* If we are within the engine, INTL_convert_string
				 * will convert the string between character sets
				 * (or die trying).
				 * This module, however, can be called from outside
				 * the engine (for instance, moving values around for
				 * DSQL).
				 * In that event, we'll move the values if we think
				 * they are compatible text types, otherwise fail.
				 * eg: Simple cases can be handled here (no
				 * character set conversion).
				 *
				 * a charset type binary is compatible with all other types.
				 * if a charset involved is ttype_dynamic, we must look up
				 *    the charset of the attachment (only if we are in the
				 *    engine). If we are outside the engine, the
				 *    assume that the engine has converted the values
				 *    previously in the request.
				 *
				 * Even within the engine, not calling INTL_convert_string
				 * unless really required is a good optimization.
				 */

				CHARSET_ID charset2;
				if (cb->transliterate(from, to, charset2))
					return;

				// At this point both `from` and `to` are guaranteed to have the same charset and this is stored in charset2
				// Because of this we can freely use `toCharset` against `from`.

				{ // scope
					USHORT strtype_unused;
					UCHAR *ptr;
					length = CVT_get_string_ptr_common(from, &strtype_unused, &ptr, NULL, 0, decSt, cb);
					q = ptr;
				} // end scope

				const USHORT to_size = TEXT_LEN(to);
				CharSet* toCharset = cb->getToCharset(charset2);

				ULONG toLength = length;

				if (!trustedSource)
				{
					// Most likely data already has been validated once or twice, but another validation won't hurt much.
					cb->validateData(toCharset, length, q);
					toLength = cb->validateLength(toCharset, charset2, length, q, to_size);
				}
				else
				{
					// Silently truncate. In the wild this should never happen
					if (length > to_size)
					{
						fb_assert(from->dsc_dtype == dtype_text);
						toLength = to_size;
					}
				}

				switch (to->dsc_dtype)
				{
					case dtype_text:
					{
						ULONG fill = ULONG(to->dsc_length) - toLength;
						CVT_COPY_BUFF(q, p, toLength);

						if (fill > 0)
						{
							fb_assert(!toCharset || toCharset->getSpaceLength() == 1);
							UCHAR fillChar = toCharset ?
								*toCharset->getSpace() :
								(charset2 == ttype_binary ? 0x00 : ASCII_SPACE);

							memset(p, fillChar, fill);
							p += fill;
						}
						break;
					}

					case dtype_cstring:
						CVT_COPY_BUFF(q, p, toLength);
						*p = 0;
						break;

					case dtype_varying:
						if (to->dsc_length > sizeof(USHORT))
						{
							// TMN: Here we should really have the following fb_assert
							// fb_assert(length <= MAX_USHORT);
							((vary*) p)->vary_length = (USHORT) toLength;
							p = reinterpret_cast<UCHAR*>(((vary*) p)->vary_string);
							CVT_COPY_BUFF(q, p, toLength);
						}
						else
							memset(to->dsc_address, 0, to->dsc_length);		// the best we can do
						break;
				}
			}
			return;

		case dtype_short:
		case dtype_long:
		case dtype_int64:
		case dtype_quad:
			integer_to_text(from, to, cb);
			return;

		case dtype_int128:
			int128_to_text(from, to, cb);
			return;

		case dtype_ushort:
		case dtype_ulong:
		case dtype_uint64:
		case dtype_uint128:
			integer_to_text(from, to, cb);  // Use existing integer_to_text for unsigned types
			return;

		case dtype_real:
		case dtype_double:
			float_to_text(from, to, cb);
			return;

		case dtype_dec64:
		case dtype_dec128:
			decimal_float_to_text(from, to, decSt, cb);
			return;

		case dtype_sql_date:
		case dtype_sql_time:
		case dtype_sql_time_tz:
		case dtype_ex_time_tz:
		case dtype_timestamp:
		case dtype_timestamp_tz:
		case dtype_ex_timestamp_tz:
			datetime_to_text(from, to, cb);
			return;

		case dtype_boolean:
			{
				char* text = const_cast<char*>(*(FB_BOOLEAN*) from->dsc_address ? "TRUE" : "FALSE");

				dsc intermediate;
				intermediate.dsc_dtype = dtype_text;
				intermediate.dsc_ttype() = ttype_ascii;
				intermediate.makeText(static_cast<USHORT>(strlen(text)), CS_ASCII,
					reinterpret_cast<UCHAR*>(text));

				CVT_move_common(&intermediate, to, decSt, cb);
				return;
			}

		case dtype_uuid:
			{
				// Convert UUID binary to string representation
				static char uuid_str[37]; // 36 + null terminator
				const UCHAR* uuid_bytes = (const UCHAR*) from->dsc_address;
				
				snprintf(uuid_str, sizeof(uuid_str),
					"%02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%02X%02X%02X",
					uuid_bytes[0], uuid_bytes[1], uuid_bytes[2], uuid_bytes[3],
					uuid_bytes[4], uuid_bytes[5],
					uuid_bytes[6], uuid_bytes[7],
					uuid_bytes[8], uuid_bytes[9],
					uuid_bytes[10], uuid_bytes[11], uuid_bytes[12], uuid_bytes[13], uuid_bytes[14], uuid_bytes[15]);

				dsc intermediate;
				intermediate.dsc_dtype = dtype_text;
				intermediate.dsc_ttype() = ttype_ascii;
				intermediate.makeText(36, CS_ASCII, reinterpret_cast<UCHAR*>(uuid_str));

				CVT_move_common(&intermediate, to, decSt, cb);
				return;
			}

		case dtype_json:
			{
				// Convert JSON to string (for now, just pass through)
				// Full implementation would need proper JSON serialization
				dsc intermediate;
				intermediate.dsc_dtype = dtype_text;
				intermediate.dsc_ttype() = ttype_utf8;
				intermediate.dsc_address = from->dsc_address;
				intermediate.dsc_length = from->dsc_length;

				CVT_move_common(&intermediate, to, decSt, cb);
				return;
			}

		case dtype_inet:
			CVT_inet_to_text(from, to, cb);
			return;

		case dtype_cidr:
			CVT_cidr_to_text(from, to, cb);
			return;

		case dtype_macaddr:
			CVT_macaddr_to_text(from, to, cb);
			return;

		default:
			fb_assert(false);		// Fall into ...

		case dtype_blob:
			CVT_conversion_error(from, cb->err);
			return;
		}
		break;

	case dtype_blob:
	case dtype_array:
		if (from->dsc_dtype == dtype_quad)
		{
			((SLONG*) p)[0] = ((SLONG*) q)[0];
			((SLONG*) p)[1] = ((SLONG*) q)[1];
			return;
		}

		if (to->dsc_dtype != from->dsc_dtype)
			cb->err(Arg::Gds(isc_wish_list) << Arg::Gds(isc_blobnotsup) << "move");

		// Note: DSC_EQUIV failed above as the blob sub_types were different,
		// or their character sets were different.  In V4 we aren't trying
		// to provide blob type integrity, so we just assign the blob id

		// Move blob_id byte-by-byte as that's the way it was done before
		CVT_COPY_BUFF(q, p, length);
		return;

	case dtype_short:
		*(SSHORT *) p = cvt_get_short(from, (SSHORT) to->dsc_scale, decSt, cb->err);
		return;

	case dtype_long:
		*(SLONG *) p = CVT_get_long(from, (SSHORT) to->dsc_scale, decSt, cb->err);
		return;

	case dtype_int64:
		*(SINT64 *) p = CVT_get_int64(from, (SSHORT) to->dsc_scale, decSt, cb->err);
		return;

	case dtype_quad:
		if (from->dsc_dtype == dtype_blob || from->dsc_dtype == dtype_array)
		{
			((SLONG *) p)[0] = ((SLONG *) q)[0];
			((SLONG *) p)[1] = ((SLONG *) q)[1];
			return;
		}
		*(SQUAD *) p = CVT_get_quad(from, (SSHORT) to->dsc_scale, decSt, cb->err);
		return;

	case dtype_real:
		{
			double d_value = CVT_get_double(from, decSt, cb->err);
			if (ABSOLUT(d_value) > FLOAT_MAX && ABSOLUT(d_value) != INFINITY)
				cb->err(Arg::Gds(isc_arith_except) << Arg::Gds(isc_numeric_out_of_range));
			*(float*) p = (float) d_value;
		}
		return;

	case dtype_dbkey:
		if (from->isText())
		{
			USHORT strtype_unused;
			UCHAR* ptr;
			USHORT len = CVT_get_string_ptr_common(from, &strtype_unused, &ptr, NULL, 0, decSt, cb);

			if (len == to->dsc_length)
			{
				memcpy(to->dsc_address, ptr, len);
				return;
			}
		}

		CVT_conversion_error(from, cb->err);
		break;

	case DEFAULT_DOUBLE:
#ifdef HPUX
		{
			const double d_value = CVT_get_double(from, decSt, cb->err);
			memcpy(p, &d_value, sizeof(double));
		}
#else
		*(double*) p = CVT_get_double(from, decSt, cb->err);
#endif
		return;

	case dtype_dec64:
		*((Decimal64*) p) = CVT_get_dec64(from, decSt, cb->err);
		return;

	case dtype_dec128:
		*((Decimal128*) p) = CVT_get_dec128(from, decSt, cb->err);
		return;

	case dtype_int128:
		*((Int128*) p) = CVT_get_int128(from, (SSHORT) to->dsc_scale, decSt, cb->err);
		return;

	case dtype_ushort:
		*((USHORT*) p) = static_cast<USHORT>(CVT_get_ulong(from, (SSHORT) to->dsc_scale, decSt, cb->err));
		return;

	case dtype_ulong:
		*((ULONG*) p) = static_cast<ULONG>(CVT_get_ulong(from, (SSHORT) to->dsc_scale, decSt, cb->err));
		return;

	case dtype_uint64:
		*((FB_UINT64*) p) = CVT_get_uint64(from, (SSHORT) to->dsc_scale, decSt, cb->err);
		return;

	case dtype_uint128:
		*((UInt128*) p) = CVT_get_uint128(from, (SSHORT) to->dsc_scale, decSt, cb->err);
		return;

	case dtype_boolean:
		switch (from->dsc_dtype)
		{
		case dtype_varying:
		case dtype_cstring:
		case dtype_text:
			*((FB_BOOLEAN*) to->dsc_address) = CVT_get_boolean(from, cb->err) ? '\1' : '\0';
			return;

		default:
			CVT_conversion_error(from, cb->err);
			break;
		}
		break;

	case dtype_uuid:
		switch (from->dsc_dtype)
		{
		case dtype_varying:
		case dtype_cstring:
		case dtype_text:
			{
				// Convert string to UUID
				VaryStr<40> buffer;  // UUID string is 36 chars + null terminator
				const char* str = NULL;
				int len = CVT_make_string(from, ttype_ascii, &str, &buffer, sizeof(buffer), 0, cb->err);
				
				// Simple validation - UUID string should be 36 characters
				if (len != 36)
					CVT_conversion_error(from, cb->err);
					
				// Convert hex string to binary (simplified conversion)
				UCHAR* uuid_bytes = (UCHAR*) to->dsc_address;
				int pos = 0;
				for (int i = 0; i < len && pos < 16; i++)
				{
					if (str[i] != '-')  // Skip hyphens
					{
						int nibble1, nibble2;
						if (str[i] >= '0' && str[i] <= '9') nibble1 = str[i] - '0';
						else if (str[i] >= 'A' && str[i] <= 'F') nibble1 = str[i] - 'A' + 10;
						else if (str[i] >= 'a' && str[i] <= 'f') nibble1 = str[i] - 'a' + 10;
						else CVT_conversion_error(from, cb->err);
						
						i++;
						if (i >= len) CVT_conversion_error(from, cb->err);
						
						if (str[i] >= '0' && str[i] <= '9') nibble2 = str[i] - '0';
						else if (str[i] >= 'A' && str[i] <= 'F') nibble2 = str[i] - 'A' + 10;
						else if (str[i] >= 'a' && str[i] <= 'f') nibble2 = str[i] - 'a' + 10;
						else CVT_conversion_error(from, cb->err);
						
						uuid_bytes[pos++] = (nibble1 << 4) | nibble2;
					}
				}
				if (pos != 16) CVT_conversion_error(from, cb->err);
			}
			return;

		default:
			CVT_conversion_error(from, cb->err);
			break;
		}
		break;

	case dtype_inet:
		switch (from->dsc_dtype)
		{
		case dtype_varying:
		case dtype_cstring:
		case dtype_text:
			{
				// Convert string to INET address
				VaryStr<50> buffer;  // INET string can be up to 45 chars + null
				const char* str = NULL;
				int len = CVT_make_string(from, ttype_ascii, &str, &buffer, sizeof(buffer), 0, cb->err);
				
				try {
					InetAddr addr(str);
					if (!addr.isValid())
						CVT_conversion_error(from, cb->err);
					
					// Store the address in the target buffer
					UCHAR* target = (UCHAR*) to->dsc_address;
					target[0] = addr.getFamily();  // 1 byte for family
					memcpy(target + 1, addr.getBytes(), 16);  // 16 bytes for address
				}
				catch (const Exception&) {
					CVT_conversion_error(from, cb->err);
				}
			}
			return;

		default:
			CVT_conversion_error(from, cb->err);
			break;
		}
		break;

	case dtype_cidr:
		switch (from->dsc_dtype)
		{
		case dtype_varying:
		case dtype_cstring:
		case dtype_text:
			{
				// Convert string to CIDR block
				VaryStr<50> buffer;  // CIDR string can be up to 45 chars + null
				const char* str = NULL;
				int len = CVT_make_string(from, ttype_ascii, &str, &buffer, sizeof(buffer), 0, cb->err);
				
				try {
					CidrBlock cidr(str);
					if (!cidr.isValid())
						CVT_conversion_error(from, cb->err);
					
					// Store the CIDR block in the target buffer
					UCHAR* target = (UCHAR*) to->dsc_address;
					target[0] = cidr.getFamily();  // 1 byte for family
					memcpy(target + 1, cidr.getAddress().getBytes(), 16);  // 16 bytes for address
					target[17] = cidr.getPrefixLength();  // 1 byte for prefix length
				}
				catch (const Exception&) {
					CVT_conversion_error(from, cb->err);
				}
			}
			return;

		default:
			CVT_conversion_error(from, cb->err);
			break;
		}
		break;

	case dtype_macaddr:
		switch (from->dsc_dtype)
		{
		case dtype_varying:
		case dtype_cstring:
		case dtype_text:
			{
				// Convert string to MAC address
				VaryStr<20> buffer;  // MAC string is 17 chars + null
				const char* str = NULL;
				int len = CVT_make_string(from, ttype_ascii, &str, &buffer, sizeof(buffer), 0, cb->err);
				
				try {
					MacAddr mac(str);
					if (!mac.isValid())
						CVT_conversion_error(from, cb->err);
					
					// Store the MAC address in the target buffer
					memcpy(to->dsc_address, mac.getBytes(), 6);
				}
				catch (const Exception&) {
					CVT_conversion_error(from, cb->err);
				}
			}
			return;

		default:
			CVT_conversion_error(from, cb->err);
			break;
		}
		break;

	case dtype_json:
		switch (from->dsc_dtype)
		{
		case dtype_varying:
		case dtype_cstring:
		case dtype_text:
			{
				// For now, store JSON as a BLOB-like structure
				// Full JSON implementation would require JSON parsing and validation
				memcpy(to->dsc_address, from->dsc_address, MIN(from->dsc_length, to->dsc_length));
			}
			return;

		default:
			CVT_conversion_error(from, cb->err);
			break;
		}
		break;
	}

	if (from->dsc_dtype == dtype_array || from->dsc_dtype == dtype_blob)
	{
		cb->err(Arg::Gds(isc_wish_list) << Arg::Gds(isc_blobnotsup) << "move");
	}

	fb_assert(false);
	cb->err(Arg::Gds(isc_badblk));	// internal error
}


void CVT_conversion_error(const dsc* desc, ErrorFunction err, const Exception* original)
{
/**************************************
 *
 *      c o n v e r s i o n _ e r r o r
 *
 **************************************
 *
 * Functional description
 *      A data conversion error occurred.  Complain.
 *
 **************************************/
	string message;

	if (desc->dsc_dtype >= DTYPE_TYPE_MAX)
	{
		fb_assert(false);
		err(Arg::Gds(isc_badblk));
	}

	if (desc->dsc_dtype == dtype_blob)
		message = "BLOB";
	else if (desc->dsc_dtype == dtype_array)
		message = "ARRAY";
	else if (desc->dsc_dtype == dtype_boolean)
		message = "BOOLEAN";
	else if (desc->dsc_dtype == dtype_dbkey)
		message = "DBKEY";
	else if (desc->dsc_dtype == dtype_uuid)
		message = "UUID";
	else if (desc->dsc_dtype == dtype_json)
		message = "JSON";
	else
	{
		// CVC: I don't have access here to JRD_get_thread_data())->tdbb_status_vector
		// to be able to clear it (I don't want errors appended, but replacing
		// the existing, misleading one). Since localError doesn't fill the thread's
		// status vector as ERR_post would do, our new error is the first and only one.
		// We discard errors from CVT_make_string because we are interested in reporting
		// isc_convert_error, not isc_arith_exception if CVT_make_string fails.
		// If someone finds a better way, these hacks can be deleted.
		// Initially I was trapping here status_exception and testing
		// e.value()[1] == isc_arith_except.

		try
	    {
			const char* p;
			VaryStr<TEMP_STR_LENGTH> s;
			const USHORT length =
				CVT_make_string(desc, ttype_ascii, &p, &s, sizeof(s), 0, localError);
			message.assign(p, length);

			// Convert to \xDD surely non-printable characters
			for (FB_SIZE_T n = 0; n < message.getCount(); ++n)
			{
				if (message[n] < ' ')
				{
					string hex;
					hex.printf("#x%02x", UCHAR(message[n]));
					message.replace(n, 1, hex);
					n += (hex.length() - 1);
				}
			}
		}
		/*
		catch (status_exception& e)
		{
			if (e.value()[1] == isc_arith_except)
			{
				// Apparently we can't do this line:
				JRD_get_thread_data())->tdbb_status_vector[0] = 0;

			    p = "<Too long string or can't be translated>";
			}
			else
				throw;
		}
		*/
		catch (DummyException&)
		{
			message = "<Too long string or can't be translated>";
		}
	}

	//// TODO: Need access to transliterate here to convert message to metadata charset.
	Arg::StatusVector vector;
	if (original)
		vector.assign(*original);
	vector << Arg::Gds(isc_convert_error) << message;
	err(vector);
}


static void datetime_to_text(const dsc* from, dsc* to, Callbacks* cb)
{
/**************************************
 *
 *       d a t e t i m e _ t o _ t e x t
 *
 **************************************
 *
 * Functional description
 *      Convert a timestamp, date or time value to text.
 *
 **************************************/
	bool version4 = true;

	fb_assert(DTYPE_IS_TEXT(to->dsc_dtype));

	// Convert a date or time value into a timestamp for manipulation

	bool tzLookup = true;
	tm times;
	memset(&times, 0, sizeof(struct tm));

	int	fractions = 0;
	USHORT timezone;

	switch (from->dsc_dtype)
	{
	case dtype_sql_time:
		ScratchBird::TimeStamp::decode_time(*(GDS_TIME*) from->dsc_address,
			&times.tm_hour, &times.tm_min, &times.tm_sec, &fractions);
		break;

	case dtype_sql_time_tz:
	case dtype_ex_time_tz:
		tzLookup = TimeZoneUtil::decodeTime(*(ISC_TIME_TZ*) from->dsc_address,
			true, TimeZoneUtil::NO_OFFSET, &times, &fractions);
		timezone = ((ISC_TIME_TZ*) from->dsc_address)->time_zone;
		break;

	case dtype_sql_date:
		ScratchBird::TimeStamp::decode_date(*(GDS_DATE *) from->dsc_address, &times);
		break;

	case dtype_timestamp:
		cb->isVersion4(version4); // Used in the conversion to text some lines below.
		ScratchBird::TimeStamp::decode_timestamp(*(GDS_TIMESTAMP*) from->dsc_address, &times, &fractions);
		break;

	case dtype_timestamp_tz:
	case dtype_ex_timestamp_tz:
		cb->isVersion4(version4); // Used in the conversion to text some lines below.
		tzLookup = TimeZoneUtil::decodeTimeStamp(*(ISC_TIMESTAMP_TZ*) from->dsc_address,
			true, TimeZoneUtil::NO_OFFSET, &times, &fractions);
		timezone = ((ISC_TIMESTAMP_TZ*) from->dsc_address)->time_zone;
		break;

	default:
		fb_assert(false);
		cb->err(Arg::Gds(isc_badblk));	// internal error
		break;
	}

	// Decode the timestamp into human readable terms

	string temp;
	// yyyy-mm-dd hh:mm:ss.tttt [{ +th:tm | zone-name }] OR dd-MMM-yyyy hh:mm:ss.tttt [{ +th:tm | zone-name }]
	temp.reserve(26 + TimeZoneUtil::MAX_LEN);

	// Make a textual date for data types that include it

	if (!from->isTime())
	{
		string dateStr;
		// yyyy-mm-dd OR dd-MMM-yyyy
		dateStr.reserve(11);
		if (from->dsc_dtype == dtype_sql_date || !version4)
		{
			dateStr.printf("%4.4d-%2.2d-%2.2d",
					times.tm_year + 1900, times.tm_mon + 1, times.tm_mday);
		}
		else
		{
			// Prior to BLR version 5 timestamps were converted to text in the dd-MMM-yyyy format
			dateStr.printf("%2.2d-%.3s-%4.4d",
					times.tm_mday,
					FB_LONG_MONTHS_UPPER[times.tm_mon], times.tm_year + 1900);
		}
		temp.append(dateStr);
	}

	// Put in a space to separate date & time components

	if (from->isTimeStamp() && !version4)
		temp.append(" ");

	// Add the time part for data types that include it

	if (from->dsc_dtype != dtype_sql_date)
	{
		string timeStr;
		// hh:mm:ss.tttt
		timeStr.reserve(13);
		if (from->isTime() || !version4)
		{
			timeStr.printf("%2.2d:%2.2d:%2.2d.%4.4d",
					times.tm_hour, times.tm_min, times.tm_sec, fractions);
		}
		else if (times.tm_hour || times.tm_min || times.tm_sec || fractions)
		{
			// Timestamp formating prior to BLR Version 5 is slightly different
			timeStr.printf(" %d:%.2d:%.2d.%.4d",
					times.tm_hour, times.tm_min, times.tm_sec, fractions);
		}
		temp.append(timeStr);
	}

	if (from->isDateTimeTz())
	{
		temp.append(" ");
		// [{ +th:tm | zone-name }] + nul-termination
		char tzStr[TimeZoneUtil::MAX_LEN + 1];
		TimeZoneUtil::format(tzStr, sizeof(tzStr), timezone, !tzLookup);
		temp.append(tzStr);
	}

	// Move the text version of the date/time value into the destination

	dsc desc;
	MOVE_CLEAR(&desc, sizeof(desc));
	desc.dsc_address = (UCHAR*) temp.c_str();
	desc.dsc_dtype = dtype_text;
	desc.dsc_ttype() = ttype_ascii;
	desc.dsc_length = static_cast<USHORT>(temp.length());

	if (from->isTimeStamp() && version4)
	{
		// Prior to BLR Version5, when a timestamp is converted to a string it
		// is silently truncated if the destination string is not large enough

		fb_assert(to->isText());

		const USHORT l = (to->dsc_dtype == dtype_cstring) ? 1 :
			(to->dsc_dtype == dtype_varying) ? sizeof(USHORT) : 0;
		desc.dsc_length = MIN(desc.dsc_length, (to->dsc_length - l));
	}

	CVT_move_common(&desc, to, 0, cb);
}


void make_null_string(const dsc*    desc,
					  USHORT        to_interp,
					  const char**  address,
					  vary*         temp,
					  USHORT        length,
					  DecimalStatus decSt,
					  ErrorFunction err)
{
/**************************************
 *
 *      C V T _ m a k e _ n u l l _ s t r i n g
 *
 **************************************
 *
 * Functional description
 *     Convert the data from the desc to a zero-terminated string.
 *     The pointer to this string is returned in address.
 *	   Data always placed to temp buffer.
 *
 **************************************/
	fb_assert(temp);

	USHORT len = CVT_make_string(desc, to_interp, address, temp, --length, decSt, err);

	if (*address != temp->vary_string)
	{
		length -= sizeof(USHORT);	// Take into an account VaryStr specifics
		if (len > length)
		{
			err(Arg::Gds(isc_arith_except) << Arg::Gds(isc_string_truncation) <<
				Arg::Gds(isc_imp_exc) <<
				Arg::Gds(isc_trunc_limits) << Arg::Num(length) << Arg::Num(len));
		}
		memcpy(temp->vary_string, *address, len);
		temp->vary_length = len;
	}

	fb_assert(temp->vary_length == len);
	temp->vary_string[len] = 0;

	for (USHORT n = 0; n < len; ++n)
	{
		if (!temp->vary_string[n])		// \0 in the middle of a string
			CVT_conversion_error(desc, err);
	}
}


USHORT CVT_make_string(const dsc*    desc,
					   USHORT        to_interp,
					   const char**  address,
					   vary*         temp,
					   USHORT        length,
					   DecimalStatus decSt,
					   ErrorFunction err)
{
/**************************************
 *
 *      C V T _ m a k e _ s t r i n g
 *
 **************************************
 *
 * Functional description
 *     Convert the data from the desc to a string in the specified interp.
 *     The pointer to this string is returned in address.
 *
 **************************************/
	fb_assert(desc != NULL);
	fb_assert(address != NULL);
	fb_assert(err != NULL);

	const USHORT from_interp = INTL_TTYPE(desc);

	const bool simple_return = desc->isText() &&
		(from_interp == to_interp || to_interp == ttype_none || to_interp == ttype_binary);

	fb_assert((temp != NULL && length > 0) || simple_return);

	if (simple_return)
	{
		*address = reinterpret_cast<char*>(desc->dsc_address);
		const USHORT from_len = desc->dsc_length;

		if (desc->dsc_dtype == dtype_text)
			return from_len;

		if (desc->dsc_dtype == dtype_cstring)
			return MIN((USHORT) strlen((char *) desc->dsc_address), from_len - 1);

		if (desc->dsc_dtype == dtype_varying)
		{
			vary* varying = (vary*) desc->dsc_address;
			*address = varying->vary_string;
			return MIN(varying->vary_length, (USHORT) (from_len - sizeof(USHORT)));
		}
	}

	// Not string data, then  -- convert value to varying string.

	dsc temp_desc;
	MOVE_CLEAR(&temp_desc, sizeof(temp_desc));
	temp_desc.dsc_length = length;
	temp_desc.dsc_address = (UCHAR *) temp;
	temp_desc.dsc_dtype = dtype_varying;
	temp_desc.setTextType(to_interp);
	CVT_move(desc, &temp_desc, decSt, err);
	*address = temp->vary_string;

	return temp->vary_length;
}


double CVT_power_of_ten(const int scale)
{
/*************************************
 *
 *      p o w e r _ o f _ t e n
 *
 *************************************
 *
 * Functional description
 *      return 10.0 raised to the scale power for 0 <= scale < 320.
 *
 *************************************/

	// Note that we could speed things up slightly by making the auxiliary
	// arrays global to this source module and replacing this function with
	// a macro, but the old code did up to 308 multiplies to our 1, and
	// that seems enough of a speed-up for now.

	static const double upper_part[] =
	{
		1.e000, 1.e032, 1.e064, 1.e096, 1.e128,
		1.e160, 1.e192, 1.e224, 1.e256, 1.e288
	};

	static const double lower_part[] =
	{
		1.e00, 1.e01, 1.e02, 1.e03, 1.e04, 1.e05,
		1.e06, 1.e07, 1.e08, 1.e09, 1.e10, 1.e11,
		1.e12, 1.e13, 1.e14, 1.e15, 1.e16, 1.e17,
		1.e18, 1.e19, 1.e20, 1.e21, 1.e22, 1.e23,
		1.e24, 1.e25, 1.e26, 1.e27, 1.e28, 1.e29,
		1.e30, 1.e31
	};

	// The sole caller of this function checks for scale <= 308 before calling,
	// but we just fb_assert the weakest precondition which lets the code work.
	// If the size of the exponent field, and thus the scaling, of doubles
	// gets bigger, increase the size of the upper_part array.

	fb_assert((scale >= 0) && (scale < 320));

	// Note that "scale >> 5" is another way of writing "scale / 32",
	// while "scale & 0x1f" is another way of writing "scale % 32".
	// We split the scale into the lower 5 bits and everything else,
	// then use the "everything else" to index into the upper_part array,
	// whose contents increase in steps of 1e32.

	return upper_part[scale >> 5] * lower_part[scale & 0x1f];
}


static void hex_to_value(const char*& string, const char* end, RetPtr* retValue);

static SSHORT cvt_decompose(const char*	string,
							USHORT		length,
							RetPtr*		return_value,
							ErrorFunction err)
{
/**************************************
 *
 *      d e c o m p o s e
 *
 **************************************
 *
 * Functional description
 *      Decompose a numeric string in mantissa and exponent,
 *      or if it is in hexadecimal notation.
 *
 **************************************/

	dsc errd;
	MOVE_CLEAR(&errd, sizeof(errd));
	errd.dsc_dtype = dtype_text;
	errd.dsc_ttype() = ttype_ascii;
	errd.dsc_length = length;
	errd.dsc_address = reinterpret_cast<UCHAR*>(const_cast<char*>(string));

	SSHORT scale = 0;
	int sign = 0;
	bool digit_seen = false, fraction = false;

	const char* p = string;
	const char* end = p + length;

	// skip initial spaces
	while (p < end && *p == ' ')
		++p;

	// Check if this is a numeric hex string. Must start with 0x or 0X, and be
	// no longer than 16 hex digits + 2 (the length of the 0x prefix) = 18.
	if (p + 2 < end && p[0] == '0' && UPPER(p[1]) == 'X')
	{
		p += 2; // skip over 0x part

		// skip non spaces
		const char* q = p;
		while (q < end && *q && *q != ' ')
			++q;

		const char* digits_end = q;

		// skip trailing spaces
		while (q < end && *q == ' ')
			q++;

		if (q != end || end - p == 0 || (end - p) * 4 > return_value->maxSize() * 8)
		{
			CVT_conversion_error(&errd, err);
			return 0;
		}

		q = p;
		hex_to_value(q, digits_end, return_value);

		if (q != digits_end)
		{
			CVT_conversion_error(&errd, err);
			return 0;
		}

		// 0xFFFFFFFF = -1; 0x0FFFFFFFF = 4294967295
		if (digits_end - p <= 8)
			return_value->truncate8();
		else if (digits_end - p <= 16)
			return_value->truncate16();

		return 0; // 0 scale for hex literals
	}

	for (; p < end; p++)
	{
		if (DIGIT(*p))
		{
			digit_seen = true;

			// Before computing the next value, make sure there will be
			// no overflow. Trying to detect overflow after the fact is
			// tricky: the value doesn't always become negative after an
			// overflow!

			switch(return_value->compareLimitBy10())
			{
			case RetPtr::RETVAL_OVERFLOW:
				if (fraction)
				{
					while (p < end)
					{
						if (*p != '0')
							break;
						++p;
					}
					if (p >= end)
						continue;
				}
				err(Arg::Gds(isc_arith_except) << Arg::Gds(isc_numeric_out_of_range));
				return 0;
			case RetPtr::RETVAL_POSSIBLE_OVERFLOW:
				if ((*p > '8' && sign == -1) || (*p > '7' && sign != -1))
				{
					err(Arg::Gds(isc_arith_except) << Arg::Gds(isc_numeric_out_of_range));
					return 0;
				}
				break;
			default:
				break;
			}

			return_value->nextDigit(*p - '0', 10);
			if (fraction)
				--scale;
		}
		else if (*p == '.')
		{
			if (fraction)
			{
				CVT_conversion_error(&errd, err);
				return 0;
			}
			fraction = true;
		}
		else if (*p == '-' && !digit_seen && !sign && !fraction)
			sign = -1;
		else if (*p == '+' && !digit_seen && !sign && !fraction)
			sign = 1;
		else if (*p == 'e' || *p == 'E')
			break;
		else if (*p == ' ')
		{
			// skip spaces
			while (p < end && *p == ' ')
				++p;

			// throw if there is something after the spaces
			if (p < end)
			{
				CVT_conversion_error(&errd, err);
				return 0;
			}
		}
		else
		{
			CVT_conversion_error(&errd, err);
			return 0;
		}
	}

	if (!digit_seen)
	{
		CVT_conversion_error(&errd, err);
		return 0;
	}

	if ((sign == -1) && !return_value->isLowerLimit())
		return_value->neg();

	// If there's still something left, there must be an explicit exponent
	if (p < end)
	{
		sign = 0;
		SSHORT exp = 0;
		digit_seen = false;
		for (p++; p < end; p++)
		{
			if (DIGIT(*p))
			{
				digit_seen = true;
				exp = exp * 10 + *p - '0';

				// The following is a 'safe' test to prevent overflow of
				// exp here and of scale below. A more precise test will
				// occur in the calling routine when the scale/exp is
				// applied to the value.

				if (exp >= SHORT_LIMIT)
				{
					err(Arg::Gds(isc_arith_except) << Arg::Gds(isc_numeric_out_of_range));
					return 0;
				}
			}
			else if (*p == '-' && !digit_seen && !sign)
				sign = -1;
			else if (*p == '+' && !digit_seen && !sign)
				sign = 1;
			else if (*p == ' ')
			{
				// skip spaces
				while (p < end && *p == ' ')
					++p;

				// throw if there is something after the spaces
				if (p < end)
				{
					CVT_conversion_error(&errd, err);
					return 0;
				}
			}
			else
			{
				CVT_conversion_error(&errd, err);
				return 0;
			}
		}
		if (sign == -1)
			scale -= exp;
		else
			scale += exp;

		if (!digit_seen)
		{
			CVT_conversion_error(&errd, err);
			return 0;
		}
	}

	return scale;
}


class I128Traits
{
public:
	typedef Int128 ValueType;
	typedef Int128 UnsignedType;			// To be fixed when adding int256
	static const CInt128 UPPER_LIMIT_BY_10;
	static const CInt128 LOWER_LIMIT;
};

const CInt128 I128Traits::UPPER_LIMIT_BY_10(CInt128(CInt128::MkMax) / 10);
const CInt128 I128Traits::LOWER_LIMIT(CInt128::MkMin);

class RetI128 : public RetValue<I128Traits>
{
public:
	RetI128(Int128* v)
		: RetValue<I128Traits>(v)
	{ }

	lb10 compareLimitBy10() override
	{
		lb10 rc = RetValue<I128Traits>::compareLimitBy10();
		if (rc != RETVAL_NO_OVERFLOW)
			return rc;

		if (value.sign() < 0)
			return RETVAL_OVERFLOW;
		return RETVAL_NO_OVERFLOW;
	}
};

SSHORT CVT_decompose(const char* str, USHORT len, Int128* val, ErrorFunction err)
{
/**************************************
 *
 *      d e c o m p o s e
 *
 **************************************
 *
 * Functional description
 *      Decompose a numeric string in mantissa and exponent,
 *      or if it is in hexadecimal notation.
 *
 **************************************/


	RetI128 value(val);
	return cvt_decompose(str, len, &value, err);
}


Int128 CVT_hex_to_int128(const char* str, USHORT len)
{
	Int128 val;
	RetValue<I128Traits> value(&val);
	hex_to_value(str, str + len, &value);

	return val;
}


USHORT CVT_get_string_ptr_common(const dsc* desc, USHORT* ttype, UCHAR** address,
								 vary* temp, USHORT length, DecimalStatus decSt, Callbacks* cb)
{
/**************************************
 *
 *      C V T _ g e t _ s t r i n g _ p t r
 *
 **************************************
 *
 * Functional description
 *      Get address and length of string, converting the value to
 *      string, if necessary.  The caller must provide a sufficiently
 *      large temporary.  The address of the resultant string is returned
 *      by reference.  Get_string returns the length of the string (in bytes).
 *
 *      The text-type of the string is returned in ttype.
 *
 *      Note: If the descriptor is known to be a string type, the fourth
 *      argument (temp buffer) may be NULL.
 *
 *      If input (desc) is already a string, the output pointers
 *      point to the data of the same text_type.  If (desc) is not
 *      already a string, output pointers point to ttype_ascii.
 *
 **************************************/
	fb_assert(desc != NULL);
	fb_assert(ttype != NULL);
	fb_assert(address != NULL);
	fb_assert(cb != NULL);
	fb_assert((temp != NULL && length > 0) || desc->isText() || desc->isDbKey());

	// If the value is already a string (fixed or varying), just return
	// the address and length.

	if (desc->isText())
	{
		*address = desc->dsc_address;
		*ttype = INTL_TTYPE(desc);
		if (desc->dsc_dtype == dtype_text)
			return desc->dsc_length;
		if (desc->dsc_dtype == dtype_cstring)
			return MIN((USHORT) strlen((char *) desc->dsc_address), desc->dsc_length - 1);
		if (desc->dsc_dtype == dtype_varying)
		{
			vary* varying = (vary*) desc->dsc_address;
			*address = reinterpret_cast<UCHAR*>(varying->vary_string);
			return MIN(varying->vary_length, (USHORT) (desc->dsc_length - sizeof(USHORT)));
		}
	}

	// Also trivial case - DB_KEY

	if (desc->dsc_dtype == dtype_dbkey)
	{
		*address = desc->dsc_address;
		*ttype = ttype_binary;
		return desc->dsc_length;
	}

	// No luck -- convert value to varying string.

	dsc temp_desc;
	MOVE_CLEAR(&temp_desc, sizeof(temp_desc));
	temp_desc.dsc_length = length;
	temp_desc.dsc_address = (UCHAR *) temp;
	temp_desc.dsc_dtype = dtype_varying;
	temp_desc.setTextType(ttype_ascii);
	CVT_move_common(desc, &temp_desc, decSt, cb);
	*address = reinterpret_cast<UCHAR*>(temp->vary_string);
	*ttype = INTL_TTYPE(&temp_desc);

	return temp->vary_length;
}


static void checkForIndeterminant(const Exception& e, const dsc* desc, ErrorFunction err)
{
	StaticStatusVector st;
	e.stuffException(st);
	if (fb_utils::containsErrorCode(st.begin(), isc_decfloat_invalid_operation))
		CVT_conversion_error(desc, err, &e);
}


static inline void SINT64_to_SQUAD(const SINT64 input, const SQUAD& value)
{
	((SLONG*) &value)[LOW_WORD] = (SLONG) (input & 0xffffffff);
	((SLONG*) &value)[HIGH_WORD] = (SLONG) (input >> 32);
}


Decimal64 CVT_get_dec64(const dsc* desc, DecimalStatus decSt, ErrorFunction err)
{
/**************************************
 *
 *      C V T _ g e t _ d e c 6 4
 *
 **************************************
 *
 * Functional description
 *      Convert something arbitrary to a DecFloat(16) / (64 bit).
 *
 **************************************/
	VaryStr<512> buffer;			// long enough to represent largest decimal float in ASCII
	Decimal64 d64;

	// adjust exact numeric values to same scaling
	int scale = 0;
	if (DTYPE_IS_EXACT(desc->dsc_dtype))
		scale = -desc->dsc_scale;

	const char* p = reinterpret_cast<char*>(desc->dsc_address);

	try
	{
		switch (desc->dsc_dtype)
		{
		case dtype_short:
			return d64.set(*(SSHORT*) p, decSt, scale);

		case dtype_long:
			return d64.set(*(SLONG*) p, decSt, scale);

		case dtype_quad:
			return d64.set(CVT_get_int64(desc, 0, decSt, err), decSt, scale);

		case dtype_int64:
			return d64.set(*(SINT64*) p, decSt, scale);

		case dtype_varying:
		case dtype_cstring:
		case dtype_text:
			make_null_string(desc, ttype_ascii, &p, &buffer, sizeof(buffer) - 1, decSt, err);
			try
			{
				return d64.set(buffer.vary_string, decSt);
			}
			catch (const Exception& e)
			{
				checkForIndeterminant(e, desc, err);
				throw;
			}

		case dtype_real:
			return d64.set(*((float*) p), decSt);

		case dtype_double:
			return d64.set(*((double*) p), decSt);

		case dtype_dec64:
			return *(Decimal64*) p;

		case dtype_dec128:
			return ((Decimal128*) p)->toDecimal64(decSt);

		case dtype_int128:
			return d64.set(*((Int128*) p), decSt, scale);

		default:
			CVT_conversion_error(desc, err);
			break;
		}
	}
	catch (const Exception& ex)
	{
		// reraise using passed error function
		Arg::StatusVector v(ex);
		err(v);
	}

	// compiler silencer
	return d64;
}


Decimal128 CVT_get_dec128(const dsc* desc, DecimalStatus decSt, ErrorFunction err)
{
/**************************************
 *
 *      C V T _ g e t _ d e c 1 2 8
 *
 **************************************
 *
 * Functional description
 *      Convert something arbitrary to a DecFloat(34) / (128 bit).
 *
 **************************************/
	VaryStr<1024> buffer;			// represents unreasonably long decfloat literal in ASCII
	Decimal128 d128;

	// adjust exact numeric values to same scaling
	int scale = 0;
	if (DTYPE_IS_EXACT(desc->dsc_dtype))
		scale = -desc->dsc_scale;

	const char* p = reinterpret_cast<char*>(desc->dsc_address);

	try
	{
		switch (desc->dsc_dtype)
		{
		case dtype_short:
			return d128.set(*(SSHORT*) p, decSt, scale);

		case dtype_long:
			return d128.set(*(SLONG*) p, decSt, scale);

		case dtype_quad:
			return d128.set(CVT_get_int64(desc, 0, decSt, err), decSt, scale);

		case dtype_int64:
			return d128.set(*(SINT64*) p, decSt, scale);

		case dtype_varying:
		case dtype_cstring:
		case dtype_text:
			make_null_string(desc, ttype_ascii, &p, &buffer, sizeof(buffer) - 1, decSt, err);
			try
			{
				return d128.set(buffer.vary_string, decSt);
			}
			catch (const Exception& e)
			{
				checkForIndeterminant(e, desc, err);
				throw;
			}

		case dtype_real:
			return d128.set(*((float*) p), decSt);

		case dtype_double:
			return d128.set(*((double*) p), decSt);

		case dtype_dec64:
			return (d128 = *((Decimal64*) p));			// cast to higher precision never cause rounding/traps

		case dtype_dec128:
			return *(Decimal128*) p;

		case dtype_int128:
			return d128.set(*((Int128*) p), decSt, scale);

		default:
			CVT_conversion_error(desc, err);
			break;
		}
	}
	catch (const Exception& ex)
	{
		// reraise using passed error function
		Arg::StatusVector v(ex);
		err(v);
	}

	// compiler silencer
	return d128;
}


Int128 CVT_get_int128(const dsc* desc, SSHORT scale, DecimalStatus decSt, ErrorFunction err)
{
/**************************************
 *
 *      C V T _ g e t _ d e c 1 2 8
 *
 **************************************
 *
 * Functional description
 *      Convert something arbitrary to 128 bit integer.
 *
 **************************************/
	VaryStr<1024> buffer;			// represents unreasonably long decfloat literal in ASCII
	Int128 int128;
	Decimal128 tmp;
	double d, eps;

	static const double I128_MIN_dbl = -1.7014118346046923e+38;
	static const double I128_MAX_dbl =  1.7014118346046921e+38;
	static const CDecimal128 I128_MIN_dcft("-1.701411834604692317316873037158841E+38", decSt);
	static const CDecimal128 I128_MAX_dcft( "1.701411834604692317316873037158841E+38", decSt);
	static const CDecimal128 DecFlt_05("0.5", decSt);

	// adjust exact numeric values to same scaling
	if (DTYPE_IS_EXACT(desc->dsc_dtype))
		scale -= desc->dsc_scale;

	const char* p = reinterpret_cast<char*>(desc->dsc_address);

	try
	{
		switch (desc->dsc_dtype)
		{
		case dtype_short:
			int128.set(SLONG(*(SSHORT*) p), scale);
			break;

		case dtype_long:
			int128.set(*(SLONG*) p, scale);
			break;

		case dtype_quad:
			int128.set(CVT_get_int64(desc, 0, decSt, err), scale);
			break;

		case dtype_int64:
			int128.set(*(SINT64*) p, scale);
			break;

		case dtype_varying:
		case dtype_cstring:
		case dtype_text:
			{
				USHORT length =
					CVT_make_string(desc, ttype_ascii, &p, &buffer, sizeof(buffer), decSt, err);
				scale -= CVT_decompose(p, length, &int128, err);
				int128.setScale(scale);
			}
			break;

		case dtype_blob:
		case dtype_sql_date:
		case dtype_sql_time:
		case dtype_timestamp:
		case dtype_array:
		case dtype_dbkey:
		case dtype_boolean:
			CVT_conversion_error(desc, err);
			break;

		case dtype_real:
		case dtype_double:
			if (desc->dsc_dtype == dtype_real)
			{
				d = *((float*) p);
				eps = eps_float;
			}
			else // if (desc->dsc_dtype == DEFAULT_DOUBLE)
			{
				d = *((double*) p);
				eps = eps_double;
			}

			if (scale > 0)
				d /= CVT_power_of_ten(scale);
			else if (scale < 0)
				d *= CVT_power_of_ten(-scale);

			if (d > 0)
				d += 0.5 + eps;
			else
				d -= 0.5 + eps;

			/* make sure the cast will succeed

			   Note that adding or subtracting 0.5, as we do in CVT_get_long,
			   will never allow the rounded value to fit into an Int128,
			   because when the double value is too large in magnitude
			   to fit, 0.5 is less than half of the least significant bit
			   of the significant (sometimes miscalled "mantissa") of the
			   double, and thus will have no effect on the sum. */

			if (d < I128_MIN_dbl || I128_MAX_dbl < d)
				err(Arg::Gds(isc_arith_except) << Arg::Gds(isc_numeric_out_of_range));

			int128.set(d);
			break;

		case dtype_dec64:
		case dtype_dec128:
			if (desc->dsc_dtype == dtype_dec64)
				tmp = *((Decimal64*) p);
			else
				tmp = *((Decimal128*) p);

			tmp.setScale(decSt, -scale);

			/* make sure the cast will succeed

			   Note that adding or subtracting 0.5, as we do in CVT_get_long,
			   will never allow the rounded value to fit into an Int128,
			   because when the double value is too large in magnitude
			   to fit, 0.5 is less than half of the least significant bit
			   of the significant (sometimes miscalled "mantissa") of the
			   double, and thus will have no effect on the sum. */

			if (tmp.compare(decSt, I128_MIN_dcft) < 0 || I128_MAX_dcft.compare(decSt, tmp) < 0)
				err(Arg::Gds(isc_arith_except) << Arg::Gds(isc_numeric_out_of_range));

			int128.set(decSt, tmp);
			break;

		case dtype_int128:
			int128 = *((Int128*) p);
			int128.setScale(scale);
			break;

		default:
			fb_assert(false);
			err(Arg::Gds(isc_badblk));	// internal error
			break;
		}
	}
	catch (const Exception& ex)
	{
		// reraise using passed error function
		Arg::StatusVector v(ex);
		err(v);
	}

	return int128;
}


UInt128 CVT_get_uint128(const dsc* desc, SSHORT scale, DecimalStatus decSt, ErrorFunction err)
{
/**************************************
 *
 *      C V T _ g e t _ u i n t 1 2 8
 *
 **************************************
 *
 * Functional description
 *      Convert something arbitrary to 128 bit unsigned integer.
 *
 **************************************/
	VaryStr<1024> buffer;			// represents unreasonably long decfloat literal in ASCII
	UInt128 uint128;
	Decimal128 tmp;
	double d, eps;

	static const double U128_MAX_dbl = 3.4028236692093846e+38;
	static const CDecimal128 U128_MAX_dcft("3.402823669209384634633746074317682E+38", decSt);
	static const CDecimal128 DecFlt_05("0.5", decSt);

	// adjust exact numeric values to same scaling
	if (DTYPE_IS_EXACT(desc->dsc_dtype))
		scale -= desc->dsc_scale;

	const char* p = reinterpret_cast<char*>(desc->dsc_address);

	try
	{
		switch (desc->dsc_dtype)
		{
		case dtype_short:
			{
				SSHORT val = *(SSHORT*) p;
				if (val < 0)
					err(Arg::Gds(isc_arith_except) << Arg::Gds(isc_numeric_out_of_range));
				uint128.set(static_cast<ULONG>(val), scale);
			}
			break;

		case dtype_long:
			{
				SLONG val = *(SLONG*) p;
				if (val < 0)
					err(Arg::Gds(isc_arith_except) << Arg::Gds(isc_numeric_out_of_range));
				uint128.set(static_cast<ULONG>(val), scale);
			}
			break;

		case dtype_quad:
			{
				SINT64 val = CVT_get_int64(desc, 0, decSt, err);
				if (val < 0)
					err(Arg::Gds(isc_arith_except) << Arg::Gds(isc_numeric_out_of_range));
				uint128.set(static_cast<FB_UINT64>(val), scale);
			}
			break;

		case dtype_int64:
			{
				SINT64 val = *(SINT64*) p;
				if (val < 0)
					err(Arg::Gds(isc_arith_except) << Arg::Gds(isc_numeric_out_of_range));
				uint128.set(static_cast<FB_UINT64>(val), scale);
			}
			break;

		case dtype_ushort:
			uint128.set(static_cast<ULONG>(*(USHORT*) p), scale);
			break;

		case dtype_ulong:
			uint128.set(*(ULONG*) p, scale);
			break;

		case dtype_uint64:
			uint128.set(*(FB_UINT64*) p, scale);
			break;

		case dtype_uint128:
			uint128 = *(UInt128*) p;
			uint128.setScale(scale);
			break;

		case dtype_varying:
		case dtype_cstring:
		case dtype_text:
			{
				const char* p = NULL;
				USHORT length = CVT_make_string(desc, ttype_ascii,
					&p, &buffer, sizeof(buffer), decSt, err);
				char temp_str[1025];  // Buffer for null terminator
				memcpy(temp_str, p, length);
				temp_str[length] = 0;
				uint128.set(temp_str);	// use set function
				uint128.setScale(scale);
			}
			break;

		case dtype_real:
			d = *(float*) p;
			if (d < 0)
				err(Arg::Gds(isc_arith_except) << Arg::Gds(isc_numeric_out_of_range));
			if (d > U128_MAX_dbl)
				err(Arg::Gds(isc_arith_except) << Arg::Gds(isc_numeric_out_of_range));
			uint128.set(d);
			uint128.setScale(scale);
			break;

		case dtype_double:
			d = *(double*) p;
			if (d < 0)
				err(Arg::Gds(isc_arith_except) << Arg::Gds(isc_numeric_out_of_range));
			if (d > U128_MAX_dbl)
				err(Arg::Gds(isc_arith_except) << Arg::Gds(isc_numeric_out_of_range));
			uint128.set(d);
			uint128.setScale(scale);
			break;

		// case dtype_dec64:
		//	{
		//		Decimal64 d64;
		//		d64 = *(Decimal64*) p;
		//		tmp = d64.toDecimal128(decSt);
		//		if (tmp < 0)
		//			err(Arg::Gds(isc_arith_except) << Arg::Gds(isc_numeric_out_of_range));
		//		if (tmp > U128_MAX_dcft)
		//			err(Arg::Gds(isc_arith_except) << Arg::Gds(isc_numeric_out_of_range));
		//		uint128.set(decSt, tmp);
		//		uint128.setScale(scale);
		//	}
		//	break;

		// case dtype_dec128:
		//	{
		//		tmp = *(Decimal128*) p;
		//		if (tmp < 0)
		//			err(Arg::Gds(isc_arith_except) << Arg::Gds(isc_numeric_out_of_range));
		//		if (tmp > U128_MAX_dcft)
		//			err(Arg::Gds(isc_arith_except) << Arg::Gds(isc_numeric_out_of_range));
		//		uint128.set(decSt, tmp);
		//		uint128.setScale(scale);
		//	}
		//	break;

		case dtype_int128:
			{
				Int128 i128 = *(Int128*) p;
				if (i128.sign() < 0)
					err(Arg::Gds(isc_arith_except) << Arg::Gds(isc_numeric_out_of_range));
				// Convert from signed to unsigned - this needs proper implementation
				uint128.set(static_cast<FB_UINT64>(0), 0);  // Placeholder
				uint128.setScale(scale);
			}
			break;

		default:
			CVT_conversion_error(desc, err);
			break;
		}
	}
	catch (const Exception& ex)
	{
		Arg::StatusVector v(ex);
		err(v);
	}

	return uint128;
}


SLONG CVT_get_ulong(const dsc* desc, SSHORT scale, DecimalStatus decSt, ErrorFunction err)
{
/**************************************
 *
 *      C V T _ g e t _ u l o n g
 *
 **************************************
 *
 * Functional description
 *      Convert something arbitrary to 32 bit unsigned integer.
 *
 **************************************/
	ULONG value = 0;

	const char* p = reinterpret_cast<char*>(desc->dsc_address);

	switch (desc->dsc_dtype)
	{
	case dtype_short:
		{
			SSHORT val = *(SSHORT*) p;
			if (val < 0)
				err(Arg::Gds(isc_arith_except) << Arg::Gds(isc_numeric_out_of_range));
			value = static_cast<ULONG>(val);
		}
		break;

	case dtype_long:
		{
			SLONG val = *(SLONG*) p;
			if (val < 0)
				err(Arg::Gds(isc_arith_except) << Arg::Gds(isc_numeric_out_of_range));
			value = static_cast<ULONG>(val);
		}
		break;

	case dtype_ushort:
		value = static_cast<ULONG>(*(USHORT*) p);
		break;

	case dtype_ulong:
		value = *(ULONG*) p;
		break;

	case dtype_uint64:
		{
			FB_UINT64 val = *(FB_UINT64*) p;
			if (val > ULONG_MAX)
				err(Arg::Gds(isc_arith_except) << Arg::Gds(isc_numeric_out_of_range));
			value = static_cast<ULONG>(val);
		}
		break;

	default:
		CVT_conversion_error(desc, err);
		break;
	}

	// Adjust for scale
	if (DTYPE_IS_EXACT(desc->dsc_dtype))
		scale -= desc->dsc_scale;

	adjustForScale(value, scale, ULONG_LIMIT, err);

	return static_cast<SLONG>(value);
}


FB_UINT64 CVT_get_uint64(const dsc* desc, SSHORT scale, DecimalStatus decSt, ErrorFunction err)
{
/**************************************
 *
 *      C V T _ g e t _ u i n t 6 4
 *
 **************************************
 *
 * Functional description
 *      Convert something arbitrary to 64 bit unsigned integer.
 *
 **************************************/
	FB_UINT64 value = 0;

	const char* p = reinterpret_cast<char*>(desc->dsc_address);

	switch (desc->dsc_dtype)
	{
	case dtype_short:
		{
			SSHORT val = *(SSHORT*) p;
			if (val < 0)
				err(Arg::Gds(isc_arith_except) << Arg::Gds(isc_numeric_out_of_range));
			value = static_cast<FB_UINT64>(val);
		}
		break;

	case dtype_long:
		{
			SLONG val = *(SLONG*) p;
			if (val < 0)
				err(Arg::Gds(isc_arith_except) << Arg::Gds(isc_numeric_out_of_range));
			value = static_cast<FB_UINT64>(val);
		}
		break;

	case dtype_int64:
		{
			SINT64 val = *(SINT64*) p;
			if (val < 0)
				err(Arg::Gds(isc_arith_except) << Arg::Gds(isc_numeric_out_of_range));
			value = static_cast<FB_UINT64>(val);
		}
		break;

	case dtype_ushort:
		value = static_cast<FB_UINT64>(*(USHORT*) p);
		break;

	case dtype_ulong:
		value = static_cast<FB_UINT64>(*(ULONG*) p);
		break;

	case dtype_uint64:
		value = *(FB_UINT64*) p;
		break;

	default:
		CVT_conversion_error(desc, err);
		break;
	}

	// Adjust for scale
	if (DTYPE_IS_EXACT(desc->dsc_dtype))
		scale -= desc->dsc_scale;

	adjustForScale(value, scale, UINT64_LIMIT, err);

	return value;
}


const UCHAR* CVT_get_bytes(const dsc* desc, unsigned& size)
{
/**************************************
 *
 *      C V T _ g e t _ b y t e s
 *
 **************************************
 *
 * Functional description
 *      Return raw data of descriptor.
 *
 **************************************/
	if (!desc)
	{
		size = 0;
		return nullptr;
	}

	switch (desc->dsc_dtype)
	{
		case dtype_varying:
			{
				vary* v = (vary*) desc->dsc_address;
				size = v->vary_length;
				return (const UCHAR*) v->vary_string;
			}

		case dtype_cstring:
			size = strlen((const char*) desc->dsc_address);
			return desc->dsc_address;

		default:
			size = desc->dsc_length;
			return desc->dsc_address;
	}

	return nullptr;	// compiler warning silencer
}


class SINT64Traits
{
public:
	typedef SINT64 ValueType;
	typedef FB_UINT64 UnsignedType;
	static const FB_UINT64 UPPER_LIMIT_BY_10 = MAX_SINT64 / 10;
	static const SINT64 LOWER_LIMIT = MIN_SINT64;
};

SQUAD CVT_get_quad(const dsc* desc, SSHORT scale, DecimalStatus decSt, ErrorFunction err)
{
/**************************************
 *
 *      C V T _ g e t _ q u a d
 *
 **************************************
 *
 * Functional description
 *      Convert something arbitrary to a quad (64 bit) integer of given
 *      scale.
 *
 **************************************/
	SQUAD value;
	VaryStr<50> buffer;			// long enough to represent largest quad in ASCII

	// adjust exact numeric values to same scaling

	if (DTYPE_IS_EXACT(desc->dsc_dtype))
		scale -= desc->dsc_scale;

	const char* p = reinterpret_cast<char*>(desc->dsc_address);

	switch (desc->dsc_dtype)
	{
	case dtype_short:
		{
			const SSHORT input = *(SSHORT*) p;
			((SLONG*) &value)[LOW_WORD] = input;
			((SLONG*) &value)[HIGH_WORD] = (input < 0) ? -1 : 0;
		}
		break;

	case dtype_long:
		{
			const SLONG input = *(SLONG*) p;
			((SLONG*) &value)[LOW_WORD] = input;
			((SLONG*) &value)[HIGH_WORD] = (input < 0) ? -1 : 0;
		}
		break;

	case dtype_quad:
		value = *((SQUAD*) p);
		break;

	case dtype_int64:
		SINT64_to_SQUAD(*(SINT64*) p, value);
		break;

	case dtype_varying:
	case dtype_cstring:
	case dtype_text:
		{
			USHORT length =
				CVT_make_string(desc, ttype_ascii, &p, &buffer, sizeof(buffer), decSt, err);

			SINT64 i64;
			{
				RetValue<SINT64Traits> rv(&i64);
				scale -= cvt_decompose(p, length, &rv, err);
			}
			SINT64_to_SQUAD(i64, value);
		}
		break;

	case dtype_blob:
	case dtype_sql_date:
	case dtype_sql_time:
	case dtype_timestamp:
	case dtype_array:
	case dtype_dbkey:
	case dtype_boolean:
		CVT_conversion_error(desc, err);
		break;

	case dtype_dec64:
	case dtype_dec128:
	case dtype_int128:
		SINT64_to_SQUAD(CVT_get_int64(desc, scale, decSt, err), value);
		break;

	default:
		fb_assert(false);
		err(Arg::Gds(isc_badblk));	// internal error
		break;
	}

	// Last, but not least, adjust for scale

	if (scale != 0)
	{
		fb_assert(false);
		err(Arg::Gds(isc_badblk));	// internal error
	}

	return value;
}


SINT64 CVT_get_int64(const dsc* desc, SSHORT scale, DecimalStatus decSt, ErrorFunction err)
{
/**************************************
 *
 *      C V T _ g e t _ i n t 6 4
 *
 **************************************
 *
 * Functional description
 *      Convert something arbitrary to an SINT64 (64 bit integer)
 *      of given scale.
 *
 **************************************/
	SINT64 value;
	double d, eps;
	VaryStr<50> buffer;			// long enough to represent largest SINT64 in ASCII

	// adjust exact numeric values to same scaling

	if (DTYPE_IS_EXACT(desc->dsc_dtype))
		scale -= desc->dsc_scale;

	const char* p = reinterpret_cast<char*>(desc->dsc_address);

	switch (desc->dsc_dtype)
	{
	case dtype_short:
		value = *((SSHORT*) p);
		break;

	case dtype_long:
		value = *((SLONG*) p);
		break;

	case dtype_int64:
		value = *((SINT64*) p);
		break;

	case dtype_quad:
		value = (((SINT64) ((SLONG*) p)[HIGH_WORD]) << 32) + (((ULONG*) p)[LOW_WORD]);
		break;

	case dtype_dec64:
	case dtype_dec128:
		{
			Decimal128 d128;
			if (desc->dsc_dtype == dtype_dec64)
				d128 = *((Decimal64*) p);
			else
				d128 = *((Decimal128*) p);

			return d128.toInt64(decSt, scale);
		}

	case dtype_int128:
		return ((Int128*) p)->toInt64(scale);

	case dtype_real:
	case dtype_double:
		if (desc->dsc_dtype == dtype_real)
		{
			d = *((float*) p);
			eps = eps_float;
		}
		else // if (desc->dsc_dtype == DEFAULT_DOUBLE)
		{
			d = *((double*) p);
			eps = eps_double;
		}

		if (scale > 0)
			d /= CVT_power_of_ten(scale);
		else if (scale < 0)
			d *= CVT_power_of_ten(-scale);

		if (d > 0)
			d += 0.5 + eps;
		else
			d -= 0.5 + eps;

		/* make sure the cast will succeed - different machines
		   do different things if the value is larger than a quad
		   can hold.

		   Note that adding or subtracting 0.5, as we do in CVT_get_long,
		   will never allow the rounded value to fit into an int64,
		   because when the double value is too large in magnitude
		   to fit, 0.5 is less than half of the least significant bit
		   of the significant (sometimes miscalled "mantissa") of the
		   double, and thus will have no effect on the sum. */

		if (d < (double) QUAD_MIN_real || (double) QUAD_MAX_real < d)
			err(Arg::Gds(isc_arith_except) << Arg::Gds(isc_numeric_out_of_range));

		return (SINT64) d;

	case dtype_varying:
	case dtype_cstring:
	case dtype_text:
		{
			USHORT length =
				CVT_make_string(desc, ttype_ascii, &p, &buffer, sizeof(buffer), decSt, err);

			RetValue<SINT64Traits> rv(&value);
			scale -= cvt_decompose(p, length, &rv, err);
		}
		break;

	case dtype_blob:
	case dtype_sql_date:
	case dtype_sql_time:
	case dtype_sql_time_tz:
	case dtype_ex_time_tz:
	case dtype_timestamp:
	case dtype_timestamp_tz:
	case dtype_ex_timestamp_tz:
	case dtype_array:
	case dtype_dbkey:
	case dtype_boolean:
		CVT_conversion_error(desc, err);
		break;

	default:
		fb_assert(false);
		err(Arg::Gds(isc_badblk));	// internal error
		break;
	}

	// Last, but not least, adjust for scale
	adjustForScale(value, scale, INT64_LIMIT, err);

	return value;
}


static void hex_to_value(const char*& string, const char* end, RetPtr* retValue)
/*************************************
 *
 *      hex_to_value
 *
 *************************************
 *
 * Functional description
 *      Convert a hex string to a numeric value. This code only
 *      converts a hex string into a numeric value, and the
 *      size of biggest hex string depends upon RetPtr.
 *
 *************************************/
{
	// we already know this is a hex string, and there is no prefix.
	// So, string is something like DEADBEEF.

	UCHAR byte = 0;
	int nibble = ((end - string) & 1);
	char ch;

	while ((string < end) && ((DIGIT((ch = UPPER(*string)))) || ((ch >= 'A') && (ch <= 'F'))))
	{
		// Now convert the character to a nibble
		SSHORT c;

		if (ch >= 'A')
			c = (ch - 'A') + 10;
		else
			c = (ch - '0');

		if (nibble)
		{
			byte = (byte << 4) + (UCHAR) c;
			nibble = 0;
			retValue->nextDigit(byte, 256);
		}
		else
		{
			byte = c;
			nibble = 1;
		}

		++string;
	}

	fb_assert(string <= end);
}


static void localError(const ScratchBird::Arg::StatusVector&)
{
	throw DummyException();
}


namespace
{
	class CommonCallbacks : public Callbacks
	{
	public:
		explicit CommonCallbacks(ErrorFunction aErr)
			: Callbacks(aErr)
		{
		}

	public:
		virtual bool transliterate(const dsc* from, dsc* to, CHARSET_ID&);
		virtual CHARSET_ID getChid(const dsc* d);
		virtual CharSet* getToCharset(CHARSET_ID charset2);
		virtual void validateData(CharSet* toCharset, SLONG length, const UCHAR* q);
		virtual ULONG validateLength(CharSet* charSet, CHARSET_ID charSetId, ULONG length, const UCHAR* start,
			const USHORT size);
		virtual SLONG getLocalDate();
		virtual ISC_TIMESTAMP getCurrentGmtTimeStamp();
		virtual USHORT getSessionTimeZone();
		virtual void isVersion4(bool& v4);
	} commonCallbacks(status_exception::raise);

	bool CommonCallbacks::transliterate(const dsc*, dsc* to, CHARSET_ID& charset2)
	{
		charset2 = INTL_TTYPE(to);
		return false;
	}

	CharSet* CommonCallbacks::getToCharset(CHARSET_ID)
	{
		return NULL;
	}

	void CommonCallbacks::validateData(CharSet*, SLONG, const UCHAR*)
	{
	}

	ULONG CommonCallbacks::validateLength(CharSet* charSet, CHARSET_ID charSetId, ULONG length, const UCHAR* start,
		const USHORT size)
	{
		if (length > size)
		{
			fb_assert(!charSet || (!charSet->isMultiByte() && charSet->getSpaceLength() == 1));
			UCHAR fillChar = charSet ?
				*charSet->getSpace() :
				(charSetId == ttype_binary ? 0x00 : ASCII_SPACE);

			const UCHAR* p = start + size;

			// Scan the truncated string to ensure only spaces lost

			while (p < start + length)
			{
				if (*p++ != fillChar)
				{
					err(Arg::Gds(isc_arith_except) << Arg::Gds(isc_string_truncation) <<
						Arg::Gds(isc_trunc_limits) <<
							Arg::Num(size) << Arg::Num(length));
				}
			}
		}

		return MIN(length, size);
	}

	CHARSET_ID CommonCallbacks::getChid(const dsc* d)
	{
		return INTL_TTYPE(d);
	}

	SLONG CommonCallbacks::getLocalDate()
	{
		return TimeStamp::getCurrentTimeStamp().value().timestamp_date;
	}

	ISC_TIMESTAMP CommonCallbacks::getCurrentGmtTimeStamp()
	{
		return TimeZoneUtil::getCurrentGmtTimeStamp().utc_timestamp;
	}

	USHORT CommonCallbacks::getSessionTimeZone()
	{
		return TimeZoneUtil::getSystemTimeZone();
	}

	void CommonCallbacks::isVersion4(bool& /*v4*/)
	{
	}
}	// namespace

namespace ScratchBird {
	Callbacks* CVT_commonCallbacks  = &commonCallbacks;
}

USHORT CVT_get_string_ptr(const dsc* desc, USHORT* ttype, UCHAR** address,
						  vary* temp, USHORT length, DecimalStatus decSt, ErrorFunction err)
{
/**************************************
 *
 *      C V T _ g e t _ s t r i n g _ p t r
 *
 **************************************
 *
 * Functional description
 *      Get address and length of string, converting the value to
 *      string, if necessary.  The caller must provide a sufficiently
 *      large temporary.  The address of the resultant string is returned
 *      by reference.  Get_string returns the length of the string (in bytes).
 *
 *      The text-type of the string is returned in ttype.
 *
 *      Note: If the descriptor is known to be a string type, the fourth
 *      argument (temp buffer) may be NULL.
 *
 *      If input (desc) is already a string, the output pointers
 *      point to the data of the same text_type.  If (desc) is not
 *      already a string, output pointers point to ttype_ascii.
 *
 **************************************/
	fb_assert(err != NULL);

	CommonCallbacks callbacks(err);
	return CVT_get_string_ptr_common(desc, ttype, address, temp, length, decSt, &callbacks);
}


void CVT_move(const dsc* from, dsc* to, DecimalStatus decSt, ErrorFunction err, bool trustedSource)
{
/**************************************
 *
 *      C V T _ m o v e
 *
 **************************************
 *
 * Functional description
 *      Move (and possible convert) something to something else.
 *
 **************************************/
	CommonCallbacks callbacks(err);
	CVT_move_common(from, to, decSt, &callbacks, trustedSource);
}


//
// Network Type Conversion Functions
//

void CVT_inet_to_text(const dsc* from, dsc* to, ScratchBird::Callbacks* cb)
{
/**************************************
 *
 *      C V T _ i n e t _ t o _ t e x t
 *
 **************************************
 *
 * Functional description
 *      Convert INET address to text representation
 *
 **************************************/
	const UCHAR* inet_bytes = (const UCHAR*) from->dsc_address;
	InetFamily family = (InetFamily) inet_bytes[0];
	
	try {
		InetAddr addr;
		// Reconstruct InetAddr from stored format
		if (family == INET_IPV4 || family == INET_IPV6) {
			// Create temporary buffer with proper format
			UCHAR temp_buf[17];
			temp_buf[0] = family;
			memcpy(temp_buf + 1, inet_bytes + 1, 16);
			
			// Convert to string representation
			string result;
			addr.toString(result);
			
			dsc intermediate;
			intermediate.dsc_dtype = dtype_text;
			intermediate.dsc_ttype() = ttype_ascii;
			intermediate.makeText(static_cast<USHORT>(result.length()), CS_ASCII,
				reinterpret_cast<UCHAR*>(const_cast<char*>(result.c_str())));

			CVT_move_common(&intermediate, to, 0, cb);
		}
		else {
			CVT_conversion_error(from, cb->err);
		}
	}
	catch (const Exception&) {
		CVT_conversion_error(from, cb->err);
	}
}

void CVT_cidr_to_text(const dsc* from, dsc* to, ScratchBird::Callbacks* cb)
{
/**************************************
 *
 *      C V T _ c i d r _ t o _ t e x t
 *
 **************************************
 *
 * Functional description
 *      Convert CIDR block to text representation
 *
 **************************************/
	const UCHAR* cidr_bytes = (const UCHAR*) from->dsc_address;
	InetFamily family = (InetFamily) cidr_bytes[0];
	int prefix_length = cidr_bytes[17];
	
	try {
		// Reconstruct CidrBlock from stored format
		if (family == INET_IPV4 || family == INET_IPV6) {
			// Create InetAddr from stored address bytes
			UCHAR temp_addr_buf[17];
			temp_addr_buf[0] = family;
			memcpy(temp_addr_buf + 1, cidr_bytes + 1, 16);
			
			InetAddr addr;  // Reconstruct from bytes
			CidrBlock cidr(addr, prefix_length);
			
			// Convert to string representation
			string result;
			cidr.toString(result);
			
			dsc intermediate;
			intermediate.dsc_dtype = dtype_text;
			intermediate.dsc_ttype() = ttype_ascii;
			intermediate.makeText(static_cast<USHORT>(result.length()), CS_ASCII,
				reinterpret_cast<UCHAR*>(const_cast<char*>(result.c_str())));

			CVT_move_common(&intermediate, to, 0, cb);
		}
		else {
			CVT_conversion_error(from, cb->err);
		}
	}
	catch (const Exception&) {
		CVT_conversion_error(from, cb->err);
	}
}

void CVT_macaddr_to_text(const dsc* from, dsc* to, ScratchBird::Callbacks* cb)
{
/**************************************
 *
 *      C V T _ m a c a d d r _ t o _ t e x t
 *
 **************************************
 *
 * Functional description
 *      Convert MAC address to text representation
 *
 **************************************/
	const UCHAR* mac_bytes = (const UCHAR*) from->dsc_address;
	
	try {
		MacAddr mac(mac_bytes);
		
		// Convert to string representation
		string result;
		mac.toString(result);
		
		dsc intermediate;
		intermediate.dsc_dtype = dtype_text;
		intermediate.dsc_ttype() = ttype_ascii;
		intermediate.makeText(static_cast<USHORT>(result.length()), CS_ASCII,
			reinterpret_cast<UCHAR*>(const_cast<char*>(result.c_str())));

		CVT_move_common(&intermediate, to, 0, cb);
	}
	catch (const Exception&) {
		CVT_conversion_error(from, cb->err);
	}
}

ScratchBird::InetAddr CVT_get_inet(const dsc* desc, ScratchBird::Callbacks* cb)
{
/**************************************
 *
 *      C V T _ g e t _ i n e t
 *
 **************************************
 *
 * Functional description
 *      Get the value of an INET descriptor
 *
 **************************************/
	switch (desc->dsc_dtype)
	{
		case dtype_inet:
			{
				const UCHAR* inet_bytes = (const UCHAR*) desc->dsc_address;
				InetFamily family = (InetFamily) inet_bytes[0];
				// Reconstruct InetAddr from stored format
				InetAddr addr;
				// Implementation would reconstruct from binary format
				return addr;
			}

		case dtype_varying:
		case dtype_cstring:
		case dtype_text:
			{
				VaryStr<50> buffer;
				const char* str = NULL;
				int len = CVT_make_string(desc, ttype_ascii, &str, &buffer, sizeof(buffer), 0, cb->err);
				
				try {
					return InetAddr(str);
				}
				catch (const Exception&) {
					CVT_conversion_error(desc, cb->err);
				}
			}

		default:
			CVT_conversion_error(desc, cb->err);
			break;
	}
	
	return InetAddr();  // Should never reach here
}

ScratchBird::CidrBlock CVT_get_cidr(const dsc* desc, ScratchBird::Callbacks* cb)
{
/**************************************
 *
 *      C V T _ g e t _ c i d r
 *
 **************************************
 *
 * Functional description
 *      Get the value of a CIDR descriptor
 *
 **************************************/
	switch (desc->dsc_dtype)
	{
		case dtype_cidr:
			{
				const UCHAR* cidr_bytes = (const UCHAR*) desc->dsc_address;
				InetFamily family = (InetFamily) cidr_bytes[0];
				int prefix_length = cidr_bytes[17];
				// Reconstruct CidrBlock from stored format
				InetAddr addr;  // Would reconstruct from bytes
				return CidrBlock(addr, prefix_length);
			}

		case dtype_varying:
		case dtype_cstring:
		case dtype_text:
			{
				VaryStr<50> buffer;
				const char* str = NULL;
				int len = CVT_make_string(desc, ttype_ascii, &str, &buffer, sizeof(buffer), 0, cb->err);
				
				try {
					return CidrBlock(str);
				}
				catch (const Exception&) {
					CVT_conversion_error(desc, cb->err);
				}
			}

		default:
			CVT_conversion_error(desc, cb->err);
			break;
	}
	
	return CidrBlock();  // Should never reach here
}

ScratchBird::MacAddr CVT_get_macaddr(const dsc* desc, ScratchBird::Callbacks* cb)
{
/**************************************
 *
 *      C V T _ g e t _ m a c a d d r
 *
 **************************************
 *
 * Functional description
 *      Get the value of a MACADDR descriptor
 *
 **************************************/
	switch (desc->dsc_dtype)
	{
		case dtype_macaddr:
			{
				const UCHAR* mac_bytes = (const UCHAR*) desc->dsc_address;
				return MacAddr(mac_bytes);
			}

		case dtype_varying:
		case dtype_cstring:
		case dtype_text:
			{
				VaryStr<20> buffer;
				const char* str = NULL;
				int len = CVT_make_string(desc, ttype_ascii, &str, &buffer, sizeof(buffer), 0, cb->err);
				
				try {
					return MacAddr(str);
				}
				catch (const Exception&) {
					CVT_conversion_error(desc, cb->err);
				}
			}

		default:
			CVT_conversion_error(desc, cb->err);
			break;
	}
	
	return MacAddr();  // Should never reach here
}
