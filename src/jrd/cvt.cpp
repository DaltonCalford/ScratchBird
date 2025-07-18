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
 */

#include "firebird.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../jrd/jrd.h"
#include "../jrd/req.h"
#include "../jrd/val.h"
#include "iberror.h"
#include "../jrd/intl.h"
#include "../common/gdsassert.h"
#include "../common/TimeZoneUtil.h"
#include "../jrd/cvt_proto.h"
#include "../common/dsc_proto.h"
#include "../jrd/err_proto.h"
#include "../yvalve/gds_proto.h"
#include "../jrd/intl_proto.h"
#include "../common/classes/timestamp.h"
#include "../common/cvt.h"


#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#include "../jrd/intl_classes.h"

/* ERR_post is used to flag whether we were called from mov.cpp or
   anywhere else CVT is used from (by comparing with param err) */

/* normally the following two definitions are part of limits.h
   but due to a compiler bug on Apollo casting LONG_MIN to be a
   double, these have to be defined as double numbers...

   Also, since SunOS4.0 doesn't want to play with the rest of
   the ANSI world, these definitions have to be included explicitly.
   So, instead of some including <limits.h> and others using these
   definitions, just always use these definitions (huh?) */

/* It turns out to be tricky to write the INT64 versions of those constant in
   a way that will do the right thing on all platforms.  Here we go. */

#define LONG_MAX_int64 ((SINT64) 2147483647)	// max int64 value of an SLONG
#define LONG_MIN_int64 (-LONG_MAX_int64 - 1)	// min int64 value of an SLONG

#define DIGIT(c)        ((c) >= '0' && (c) <= '9')

// NOTE: The syntax for the below line may need modification to ensure
// the result of 1 << 62 is a quad

#define INT64_LIMIT     ((((SINT64) 1) << 62) / 5)
#define NUMERIC_LIMIT   (INT64_LIMIT)

using namespace Jrd;
using namespace ScratchBird;


double CVT_date_to_double(const dsc* desc)
{
/**************************************
 *
 *      C V T _ d a t e _ t o _ d o u b l e
 *
 **************************************
 *
 * Functional description
 *    Convert a date to double precision for
 *    date arithmetic routines.
 *
 **************************************/
	SLONG temp[2], *date;

	// If the input descriptor is not in date form, convert it.

	switch (desc->dsc_dtype)
	{
	case dtype_timestamp:
		date = (SLONG*) desc->dsc_address;
		break;
	case dtype_sql_time:
		// Temporarily convert the time to a timestamp for conversion
		date = temp;
		date[0] = 0;
		date[1] = *(SLONG*) desc->dsc_address;
		break;
	case dtype_sql_date:
		// Temporarily convert the date to a timestamp for conversion
		date = temp;
		date[0] = *(SLONG*) desc->dsc_address;
		date[1] = 0;
		break;
	default:
		{
			// Unknown type - most likely a string.  Try to convert it to a
			// timestamp -- or die trying (reporting an error).
			// Don't worry about users putting a TIME or DATE here - this
			// conversion is occurring in really flexible spots - we don't
			// want to overdo it.

			dsc temp_desc;
			MOVE_CLEAR(&temp_desc, sizeof(temp_desc));

			temp_desc.dsc_dtype = dtype_timestamp;
			temp_desc.dsc_length = sizeof(temp);
			date = temp;
			temp_desc.dsc_address = (UCHAR*) date;
			CVT_move(desc, &temp_desc, 0);
		}
	}

	/* Instead of returning the calculated double value in the return
	statement, am assigning the value to a local volatile double
	variable and returning that. This is to prevent a specific kind of
	precision error caused on Intel platforms (SCO and Linux) due
	to FPU register being 80 bits long and double being 64 bits long */
	volatile double retval;
	retval =
		date[0] +
		(double) date[1] / (24. * 60. * 60. * ISC_TIME_SECONDS_PRECISION);
	return retval;
}


void CVT_double_to_date(double real, SLONG fixed[2])
{
/**************************************
 *
 *      C V T _ d o u b l e _ t o _ d a t e
 *
 **************************************
 *
 * Functional description
 *      Convert a double precision representation of a date
 *      to a fixed point representation.   Double is used for
 *      date arithmetic.
 *
 **************************************/

	fixed[0] = (SLONG) real;
	fixed[1] = (SLONG) ((real - fixed[0]) * 24. * 60. * 60. * ISC_TIME_SECONDS_PRECISION);
}


static void error_swallow(const Arg::StatusVector& v)
{
	thread_db* tdbb = JRD_get_thread_data();
	v.copyTo(tdbb->tdbb_status_vector);
}


UCHAR CVT_get_numeric(const UCHAR* string, const USHORT length, SSHORT* scale, void* ptr)
{
/**************************************
 *
 *      C V T _ g e t _ n u m e r i c
 *
 **************************************
 *
 * Functional description
 *      Convert a numeric literal (string) to its binary value.
 *
 *	According to the literal passed (contains an exponent or not,
 *      what datatype fits) returns long, int64, int128, double or decfloat.
 *
 *      The return value from the function is set to dtype_decfloat, dtype_double,
 *      dtype_int128, dtype_int64 or dtype_long depending on the conversion performed.
 *      The binary value is stored at the address given by ptr.
 *
 **************************************/
	dsc desc;

	MOVE_CLEAR(&desc, sizeof(desc));
	desc.dsc_dtype = dtype_text;
	desc.dsc_ttype() = ttype_ascii;
	desc.dsc_length = length;
	desc.dsc_address = const_cast<UCHAR*>(string);
	// The above line allows the assignment, but "string" is treated as const
	// for all the purposes here.

	SINT64 value = 0;
	SSHORT local_scale = 0, sign = 0;
	bool digit_seen = false, fraction = false, over = false;

	const UCHAR* p = string;
	if (length > 2 && p[0] == '0' && p[1] == 'X')
	{
		*(Int128*) ptr = CVT_hex_to_int128(reinterpret_cast<const char*>(p + 2), length - 2);
		*scale = 0;
		return dtype_int128;
	}

	const UCHAR* const end = p + length;
	for (; p < end; p++)
	{
		if (DIGIT(*p))
		{
			digit_seen = true;

			// Before computing the next value, make sure there will be
			// no overflow. Trying to detect overflow after the fact is
			// tricky: the value doesn't always become negative after an
			// overflow!

			if (!over)
			{
				if (static_cast<FB_UINT64>(value) >= NUMERIC_LIMIT)
				{
					// possibility of an overflow
					if ((static_cast<FB_UINT64>(value) > NUMERIC_LIMIT) || (*p > '8' && sign == -1) || (*p > '7' && sign != -1))
						over = true;
				}

				// Force the subtraction to be performed before the addition,
				// thus preventing a possible signed arithmetic overflow.
				value = value * 10 + (*p - '0');
			}

			if (fraction)
				--local_scale;
		}
		else if (*p == '.')
		{
			if (fraction)
				CVT_conversion_error(&desc, ERR_post);
			else
				fraction = true;
		}
		else if (*p == '-' && !digit_seen && !sign && !fraction)
			sign = -1;
		else if (*p == '+' && !digit_seen && !sign && !fraction)
			sign = 1;
		else if (*p == 'e' || *p == 'E')
			break;
		else if (*p == '\0')
			break;
		else if (*p != ' ')
			CVT_conversion_error(&desc, ERR_post);
	}

	if (!digit_seen)
		CVT_conversion_error(&desc, ERR_post);

	if ((local_scale > MAX_SCHAR) || (local_scale < MIN_SCHAR))
		over = true;

	if ((!over) && (((p < end) && *p) ||		// there is an exponent
		((value < 0) && (sign != -1)))) // MAX_SINT64+1 wrapped around
	{
		// convert to double
		*(double*) ptr = CVT_get_double(&desc, 0, ERR_post, &over);
		if (!over)
			return dtype_double;
	}

	*scale = local_scale;

	if (over)
	{
		thread_db* tdbb = JRD_get_thread_data();

		tdbb->tdbb_status_vector->init();
		*scale = CVT_decompose(reinterpret_cast<const char*>(string), length, (Int128*) ptr, error_swallow);
		if (*scale >= MIN_SCHAR && *scale <= MAX_SCHAR &&
			(!(tdbb->tdbb_status_vector->getState() & IStatus::STATE_ERRORS)))
		{
			return dtype_int128;
		}
		tdbb->tdbb_status_vector->init();

		*(Decimal128*) ptr = CVT_get_dec128(&desc, tdbb->getAttachment()->att_dec_status, ERR_post);
		return dtype_dec128;
	}

	// The literal has already been converted to a 64-bit integer: return
	// a long if the value fits into a long, else return an int64.

	if ((value <= LONG_MAX_int64) && (value >= 0))
	{
		*(SLONG *) ptr = (SLONG) ((sign == -1) ? -value : value);
		return dtype_long;
	}

	if ((sign == -1) && (-value == LONG_MIN_int64))
	{
		*(SLONG *) ptr = SLONG_MIN;
		return dtype_long;
	}

	// Either MAX_SLONG < value <= MAX_SINT64, or
	// (value == MIN_SINT64) && (sign == -1)).
	// In the first case, the number can be negated, while in the second
	// negating the value will not change it on a 2s-complement system.

	*(SINT64 *) ptr = ((sign == -1) ? -value : value);
	return dtype_int64;
}


GDS_DATE CVT_get_sql_date(const dsc* desc)
{
/**************************************
 *
 *      C V T _ g e t _ s q l _ d a t e
 *
 **************************************
 *
 * Functional description
 *      Convert something arbitrary to a SQL date value
 *
 **************************************/
	if (desc->dsc_dtype == dtype_sql_date)
		return *((GDS_DATE *) desc->dsc_address);

	DSC temp_desc;
	GDS_DATE value;
	memset(&temp_desc, 0, sizeof(temp_desc));
	temp_desc.dsc_dtype = dtype_sql_date;
	temp_desc.dsc_address = (UCHAR *) &value;
	CVT_move(desc, &temp_desc, 0);
	return value;
}


GDS_TIME CVT_get_sql_time(const dsc* desc)
{
/**************************************
 *
 *      C V T _ g e t _ s q l _ t i m e
 *
 **************************************
 *
 * Functional description
 *      Convert something arbitrary to a SQL time value
 *
 **************************************/
	if (desc->dsc_dtype == dtype_sql_time)
		return *((GDS_TIME *) desc->dsc_address);

	DSC temp_desc;
	GDS_TIME value;
	memset(&temp_desc, 0, sizeof(temp_desc));
	temp_desc.dsc_dtype = dtype_sql_time;
	temp_desc.dsc_address = (UCHAR *) &value;
	CVT_move(desc, &temp_desc, 0);
	return value;
}


ISC_TIME_TZ CVT_get_sql_time_tz(const dsc* desc)
{
/**************************************
 *
 *      C V T _ g e t _ s q l _ t i m e _ t z
 *
 **************************************
 *
 * Functional description
 *      Convert something arbitrary to a SQL time with time zone value
 *
 **************************************/
	if (desc->dsc_dtype == dtype_sql_time_tz)
		return *((ISC_TIME_TZ*) desc->dsc_address);

	DSC temp_desc;
	ISC_TIME_TZ value;
	memset(&temp_desc, 0, sizeof(temp_desc));
	temp_desc.dsc_dtype = dtype_sql_time_tz;
	temp_desc.dsc_address = (UCHAR*) &value;
	CVT_move(desc, &temp_desc, 0);
	return value;
}


GDS_TIMESTAMP CVT_get_timestamp(const dsc* desc)
{
/**************************************
 *
 *      C V T _ g e t _ t i m e s t a m p
 *
 **************************************
 *
 * Functional description
 *      Convert something arbitrary to a SQL timestamp
 *
 **************************************/
	if (desc->dsc_dtype == dtype_timestamp)
		return *((GDS_TIMESTAMP *) desc->dsc_address);

	DSC temp_desc;
	GDS_TIMESTAMP value;
	memset(&temp_desc, 0, sizeof(temp_desc));
	temp_desc.dsc_dtype = dtype_timestamp;
	temp_desc.dsc_address = (UCHAR *) &value;
	CVT_move(desc, &temp_desc, 0);
	return value;
}


ISC_TIMESTAMP_TZ CVT_get_timestamp_tz(const dsc* desc)
{
/**************************************
 *
 *      C V T _ g e t _ t i m e s t a m p _ t z
 *
 **************************************
 *
 * Functional description
 *      Convert something arbitrary to a SQL timestamp with time zone
 *
 **************************************/
	if (desc->dsc_dtype == dtype_timestamp_tz)
		return *((ISC_TIMESTAMP_TZ*) desc->dsc_address);

	DSC temp_desc;
	ISC_TIMESTAMP_TZ value;
	memset(&temp_desc, 0, sizeof(temp_desc));
	temp_desc.dsc_dtype = dtype_timestamp_tz;
	temp_desc.dsc_address = (UCHAR*) &value;
	CVT_move(desc, &temp_desc, 0);
	return value;
}


ScratchBird::GlobalPtr<EngineCallbacks> EngineCallbacks::instance;


bool EngineCallbacks::transliterate(const dsc* from, dsc* to, CHARSET_ID& charset2)
{
	CHARSET_ID charset1;
	if (INTL_TTYPE(from) == ttype_dynamic)
		charset1 = INTL_charset(NULL, INTL_TTYPE(from));
	else
		charset1 = INTL_TTYPE(from);

	if (INTL_TTYPE(to) == ttype_dynamic)
		charset2 = INTL_charset(NULL, INTL_TTYPE(to));
	else
		charset2 = INTL_TTYPE(to);

	// The charset[12] can be ttype_dynamic only if we are
	// outside the engine. Within the engine INTL_charset
	// would have set it to the ttype of the attachment

	if ((charset1 != charset2) &&
		(charset2 != ttype_none) &&
		(charset1 != ttype_binary) &&
		(charset2 != ttype_binary) &&
		(charset1 != ttype_dynamic) && (charset2 != ttype_dynamic))
	{
		INTL_convert_string(to, from, this);
		return true;
	}

	return false;
}


CharSet* EngineCallbacks::getToCharset(CHARSET_ID charSetId)
{
	thread_db* tdbb = JRD_get_thread_data();
	return INTL_charset_lookup(tdbb, charSetId);
}


void EngineCallbacks::validateData(CharSet* toCharSet, SLONG length, const UCHAR* q)
{
	if (toCharSet && !toCharSet->wellFormed(length, q))
		err(Arg::Gds(isc_malformed_string));
}


ULONG EngineCallbacks::validateLength(CharSet* charSet, CHARSET_ID charSetId, ULONG length, const UCHAR* start,
	const USHORT size)
{
	fb_assert(charSet);

	if (charSet && (charSet->isMultiByte() || length > size))
	{
		const ULONG srcCharLength = charSet->length(length, start, true);
		const ULONG destCharLength = (ULONG) size / charSet->maxBytesPerChar();

		if (srcCharLength > destCharLength)
		{
			const ULONG spaceByteLength = charSet->getSpaceLength();
			const ULONG trimmedByteLength = charSet->removeTrailingSpaces(length, start);
			const ULONG trimmedCharLength = srcCharLength - (length - trimmedByteLength) / spaceByteLength;

			if (trimmedCharLength <= destCharLength)
				return trimmedByteLength + (destCharLength - trimmedCharLength) * spaceByteLength;
			else
			{
				err(Arg::Gds(isc_arith_except) << Arg::Gds(isc_string_truncation) <<
					Arg::Gds(isc_trunc_limits) << Arg::Num(destCharLength) << Arg::Num(srcCharLength));
			}
		}
	}

	return length;
}


ULONG TruncateCallbacks::validateLength(CharSet* charSet, CHARSET_ID charSetId, ULONG length, const UCHAR* start,
	const USHORT size)
{
	fb_assert(charSet);

	if (charSet && (charSet->isMultiByte() || length > size))
	{
		const ULONG srcCharLength = charSet->length(length, start, true);
		const ULONG destCharLength = (ULONG) size / charSet->maxBytesPerChar();

		if (srcCharLength > destCharLength)
		{
			const ULONG spaceByteLength = charSet->getSpaceLength();
			const ULONG trimmedByteLength = charSet->removeTrailingSpaces(length, start);
			const ULONG trimmedCharLength = srcCharLength - (length - trimmedByteLength) / spaceByteLength;

			if (trimmedCharLength <= destCharLength)
				return trimmedByteLength + (destCharLength - trimmedCharLength) * spaceByteLength;
			else if (charSet->isMultiByte())
			{
				HalfStaticArray<UCHAR, BUFFER_SMALL, USHORT> buffer(size);
				length = charSet->substring(
					length, start, buffer.getCapacity(), buffer.begin(),
					0, destCharLength);
			}
			else
				length = size;

			ERR_post_warning(Arg::Warning(isc_truncate_warn) << Arg::Warning(truncateReason));
		}
	}

	return length;
}


CHARSET_ID EngineCallbacks::getChid(const dsc* to)
{
	if (INTL_TTYPE(to) == ttype_dynamic)
		return INTL_charset(NULL, INTL_TTYPE(to));

	return INTL_TTYPE(to);
}


SLONG EngineCallbacks::getLocalDate()
{
	thread_db* tdbb = JRD_get_thread_data();

	if (tdbb && (tdbb->getType() == ThreadData::tddDBB) && tdbb->getRequest())
		return tdbb->getRequest()->getLocalTimeStamp().timestamp_date;

	return TimeZoneUtil::timeStampTzToTimeStamp(
		TimeZoneUtil::getCurrentSystemTimeStamp(), getSessionTimeZone()).timestamp_date;
}


ISC_TIMESTAMP EngineCallbacks::getCurrentGmtTimeStamp()
{
	thread_db* tdbb = JRD_get_thread_data();

	if (tdbb && (tdbb->getType() == ThreadData::tddDBB) && tdbb->getRequest())
		return tdbb->getRequest()->getGmtTimeStamp();

	return TimeZoneUtil::timeStampTzToTimeStamp(TimeZoneUtil::getCurrentSystemTimeStamp(), TimeZoneUtil::GMT_ZONE);
}


USHORT EngineCallbacks::getSessionTimeZone()
{
	thread_db* tdbb = JRD_get_thread_data();

	if (tdbb && (tdbb->getType() == ThreadData::tddDBB) && tdbb->getAttachment())
		return tdbb->getAttachment()->att_current_timezone;

	return TimeZoneUtil::GMT_ZONE;
}


void EngineCallbacks::isVersion4(bool& v4)
{
	thread_db* tdbb = JRD_get_thread_data();

	if (tdbb && (tdbb->getType() == ThreadData::tddDBB) && tdbb->getRequest())
	{
		v4 = (tdbb->getRequest()->getStatement()->blrVersion == 4);
	}
}
