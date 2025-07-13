/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		align.h
 *	DESCRIPTION:	Maximum alignments for corresponding datatype
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
 * 2002.10.28 Sean Leyne - Code cleanup, removed obsolete "MPEXL" port
 * 2002.10.28 Sean Leyne - Code cleanup, removed obsolete "DecOSF" port
 *
 */

#ifndef JRD_ALIGN_H
#define JRD_ALIGN_H

/*
Maximum alignments for corresponding data types are defined in dsc.h
*/

#include "../common/DecFloat.h"
#include "../common/Int128.h"
#include "firebird/impl/blr.h"

/*  The following macro must be defined as the highest-numericly-valued
 *  blr which describes a datatype: arrays are sized based on this value.
 *  if a new blr is defined to represent a datatype in blr.h, and the new
 *  value is greater than blr_blob_id, be sure to change the next define,
 *  and also add the required entries to all of the arrays below.
 */
const unsigned char DTYPE_BLR_MAX	= 59;  // Updated for full-text search types

/*
 the blr types are defined in blr.h

No need to worry about blr_blob or ?blr_blob_id

*/

#include "../common/dsc.h"
#include "../jrd/RecordNumber.h"

static const USHORT gds_cvt_blr_dtype[DTYPE_BLR_MAX + 1] =
{
	0, 0, 0, 0, 0, 0, 0,
	dtype_short,				/* blr_short == 7 */
	dtype_long,					/* blr_long == 8 */
	dtype_quad,					/* blr_quad == 9 */
	dtype_real,					/* blr_float == 10 */
	dtype_d_float,				/* blr_d_float == 11 */
	dtype_sql_date,				/* blr_sql_date == 12 */
	dtype_sql_time,				/* blr_sql_time == 13 */
	dtype_text,					/* blr_text == 14 */
	dtype_text,					/* blr_text2 == 15 */
	dtype_int64,				/* blr_int64 == 16 */
	0, 0, 0, 0, 0, 0,
	dtype_boolean,				// blr_bool == 23
	dtype_dec64,				/* blr_dec64 == 24 */
	dtype_dec128,				/* blr_dec128 == 25 */
	dtype_int128,				/* blr_int128 == 26 */
	dtype_double,				/* blr_double == 27 */
	dtype_sql_time_tz,			/* blr_sql_time_tz == 28 */
	dtype_timestamp_tz,			/* blr_timestamp_tz == 29 */
	dtype_ex_time_tz,			/* blr_ex_time_tz == 30 */
	dtype_ex_timestamp_tz,		/* blr_ex_timestamp_tz == 31 */
	dtype_uuid,				/* blr_uuid == 32 */
	dtype_json,				/* blr_json == 33 */
	0,
	dtype_timestamp,			/* blr_timestamp == 35 */
	0,
	dtype_varying,				/* blr_varying == 37 */
	dtype_varying,				/* blr_varying2 == 38 */
	dtype_ushort,				/* blr_ushort == 39 */
	dtype_cstring,				/* blr_cstring == 40 */
	dtype_cstring,				/* blr_cstring2 == 41 */
	dtype_ulong,				/* blr_ulong == 42 */
	dtype_uint64,				/* blr_uint64 == 43 */
	dtype_uint128,				/* blr_uint128 == 44 */
	dtype_varying_large,		/* blr_varying_large == 45 */
	dtype_inet,					/* blr_inet == 46 */
	dtype_cidr,					/* blr_cidr == 47 */
	dtype_macaddr,				/* blr_macaddr == 48 */
	dtype_citext,				/* blr_citext == 49 */
	dtype_int4range,			/* blr_int4range == 50 */
	dtype_int8range,			/* blr_int8range == 51 */
	dtype_numrange,				/* blr_numrange == 52 */
	dtype_tsrange,				/* blr_tsrange == 53 */
	dtype_tstzrange,			/* blr_tstzrange == 54 */
	dtype_daterange,			/* blr_daterange == 55 */
	dtype_array_slice,			/* blr_array_slice == 56 */
	dtype_array_md,				/* blr_array_md == 57 */
	dtype_tsvector,				/* blr_tsvector == 58 */
	dtype_tsquery				/* blr_tsquery == 59 */
};

static const USHORT type_alignments[DTYPE_TYPE_MAX] =
{
	0,
	0,							/* dtype_text */
	0,							/* dtype_cstring */
	sizeof(SSHORT),				/* dtype_varying */
	0,							/* unused */
	0,							/* unused */
	sizeof(SCHAR),				/* dtype_packed */
	sizeof(SCHAR),				/* dtype_byte */
	sizeof(SSHORT),				/* dtype_short */
	sizeof(SLONG),				/* dtype_long */
	sizeof(SLONG),				/* dtype_quad */
	sizeof(float),				/* dtype_real */
	FB_DOUBLE_ALIGN,			/* dtype_double */
	FB_DOUBLE_ALIGN,			/* dtype_d_float */
	sizeof(GDS_DATE),			/* dtype_sql_date */
	sizeof(GDS_TIME),			/* dtype_sql_time */
	sizeof(GDS_DATE),			/* dtype_timestamp */
	sizeof(SLONG),				/* dtype_blob */
	sizeof(SLONG),				/* dtype_array */
	sizeof(SINT64),				/* dtype_int64 */
	sizeof(ULONG),				/* dtype_dbkey */
	sizeof(UCHAR),				/* dtype_boolean */
	sizeof(ScratchBird::Decimal64),/* dtype_dec64 */
	sizeof(ScratchBird::Decimal64),/* dtype_dec128 */
	sizeof(SINT64),				/* dtype_int128 */
	sizeof(GDS_TIME),			/* dtype_sql_time_tz */
	sizeof(GDS_DATE),			/* dtype_timestamp_tz */
	sizeof(GDS_TIME),			/* dtype_ex_time_tz */
	sizeof(GDS_DATE),			/* dtype_ex_timestamp_tz */
	sizeof(SLONG),				/* dtype_uuid (16 bytes aligned to 4) */
	sizeof(SLONG),				/* dtype_json (variable length, aligned to 4) */

	// New unsigned integer types
	sizeof(USHORT),				/* dtype_ushort */
	sizeof(ULONG),				/* dtype_ulong */
	sizeof(FB_UINT64),			/* dtype_uint64 */
	sizeof(SINT64),				/* dtype_uint128 (aligned same as int128) */

	// Enhanced VARCHAR support
	sizeof(SSHORT),				/* dtype_varying_large */

	// Network address types
	sizeof(SLONG),				/* dtype_inet (IPv4/IPv6 address) */
	sizeof(SLONG),				/* dtype_cidr (network block) */
	sizeof(SLONG),				/* dtype_macaddr (MAC address) */

	// Enhanced text types
	sizeof(SSHORT),				/* dtype_citext (case-insensitive text) */

	// Range types
	sizeof(SLONG),				/* dtype_int4range */
	sizeof(SINT64),				/* dtype_int8range */
	sizeof(double),				/* dtype_numrange */
	sizeof(ISC_TIMESTAMP),		/* dtype_tsrange */
	sizeof(ISC_TIMESTAMP_TZ),	/* dtype_tstzrange */
	sizeof(ISC_DATE),			/* dtype_daterange */

	// Advanced array types
	sizeof(SLONG),				/* dtype_array_slice */
	sizeof(SLONG),				/* dtype_array_md */

	// Full-text search types
	sizeof(SLONG),				/* dtype_tsvector */
	sizeof(SLONG)				/* dtype_tsquery */
};

static const USHORT type_lengths[DTYPE_TYPE_MAX] =
{
	0,
	0,								/* dtype_text */
	0,								/* dtype_cstring */
	0,								/* dtype_varying */
	0,								/* unused */
	0,								/* unused */
	0,								/* dtype_packed */
	sizeof(SCHAR),					/* dtype_byte */
	sizeof(SSHORT),					/* dtype_short */
	sizeof(SLONG),					/* dtype_long */
	sizeof(ISC_QUAD),				/* dtype_quad */
	sizeof(float),					/* dtype_real */
	sizeof(double),					/* dtype_double */
	sizeof(double),					/* dtype_d_float */
	sizeof(GDS_DATE),				/* dtype_sql_date */
	sizeof(GDS_TIME),				/* dtype_sql_time */
	sizeof(GDS_TIMESTAMP),			/* dtype_timestamp */
	sizeof(ISC_QUAD),				/* dtype_blob */
	sizeof(ISC_QUAD),				/* dtype_array */
	sizeof(SINT64),					/* dtype_int64 */
	sizeof(RecordNumber::Packed),	/*dtype_dbkey */
	sizeof(UCHAR),					/* dtype_boolean */
	sizeof(ScratchBird::Decimal64),	/* dtype_dec64 */
	sizeof(ScratchBird::Decimal128),	/*dtype_dec128 */
	sizeof(ScratchBird::Int128),		/*	dtype_int128 */
	sizeof(ISC_TIME_TZ),			/* dtype_sql_time_tz */
	sizeof(ISC_TIMESTAMP_TZ),		/* dtype_timestamp_tz */
	sizeof(ISC_TIME_TZ_EX),			/* dtype_ex_time_tz */
	sizeof(ISC_TIMESTAMP_TZ_EX),	/* dtype_ex_timestamp_tz */
	16,								/* dtype_uuid (128-bit/16 bytes) */
	0,								/* dtype_json (variable length) */

	// New unsigned integer types
	sizeof(USHORT),					/* dtype_ushort */
	sizeof(ULONG),					/* dtype_ulong */
	sizeof(FB_UINT64),				/* dtype_uint64 */
	sizeof(ScratchBird::UInt128),	/* dtype_uint128 */

	// Enhanced VARCHAR support
	0,								/* dtype_varying_large (variable length) */

	// Network address types
	16,								/* dtype_inet (IPv4=4, IPv6=16 bytes max) */
	17,								/* dtype_cidr (IP + prefix length) */
	6,								/* dtype_macaddr (6 bytes) */

	// Enhanced text types
	0,								/* dtype_citext (variable length) */

	// Range types
	sizeof(SLONG) * 2 + 1,			/* dtype_int4range (2 bounds + flags) */
	sizeof(SINT64) * 2 + 1,			/* dtype_int8range (2 bounds + flags) */
	sizeof(double) * 2 + 1,			/* dtype_numrange (2 bounds + flags) */
	sizeof(ISC_TIMESTAMP) * 2 + 1,	/* dtype_tsrange (2 bounds + flags) */
	sizeof(ISC_TIMESTAMP_TZ) * 2 + 1, /* dtype_tstzrange (2 bounds + flags) */
	sizeof(ISC_DATE) * 2 + 1,		/* dtype_daterange (2 bounds + flags) */

	// Advanced array types
	0,								/* dtype_array_slice (variable length) */
	0,								/* dtype_array_md (variable length) */

	// Full-text search types
	0,								/* dtype_tsvector (variable length) */
	0								/* dtype_tsquery (variable length) */
};


// This table is only used by gpre's cme.cpp.
// float, double are numbers from IEEE floating-point standard (IEEE 754)
static const USHORT type_significant_bits[DTYPE_TYPE_MAX] =
{
	0,
	0,							/* dtype_text */
	0,							/* dtype_cstring */
	0,							/* dtype_varying */
	0,							/* unused */
	0,							/* unused */
	0,							/* dtype_packed */
	sizeof(SCHAR) * 8,			/* dtype_byte */
	sizeof(SSHORT) * 8,			/* dtype_short */
	sizeof(SLONG) * 8,			/* dtype_long */
	sizeof(ISC_QUAD) * 8,		/* dtype_quad */
	23,							/* dtype_real,  23 sign. bits = 7 sign. digits */
	52,							/* dtype_double,  52 sign. bits = 15 sign. digits */
	52,							/* dtype_d_float,  52 sign. bits = 15 sign. digits */
	sizeof(GDS_DATE) * 8,		/* dtype_sql_date */
	sizeof(GDS_TIME) * 8,		/* dtype_sql_time */
	sizeof(GDS_TIMESTAMP) * 8,	/* dtype_timestamp */
	sizeof(ISC_QUAD) * 8,		/* dtype_blob */
	sizeof(ISC_QUAD) * 8,		/* dtype_array */
	sizeof(SINT64) * 8,			/* dtype_int64 */
	0,							// dtype_dbkey
	0,							// dtype_boolean
	0,							// dtype_dec64
	0,							// dtype_dec128
	0,							// dtype_int128
	0,							// dtype_sql_time_tz
	0,							// dtype_timestamp_tz
	0,							// dtype_ex_time_tz
	0,							// dtype_ex_timestamp_tz
	128,						// dtype_uuid (128 bits)
	0,							// dtype_json (variable)

	// New unsigned integer types
	sizeof(USHORT) * 8,			// dtype_ushort (16 bits)
	sizeof(ULONG) * 8,			// dtype_ulong (32 bits)
	sizeof(FB_UINT64) * 8,		// dtype_uint64 (64 bits)
	128,						// dtype_uint128 (128 bits)

	// Enhanced VARCHAR support
	0,							// dtype_varying_large (variable length)

	// Network address types
	128,						// dtype_inet (128 bits for IPv6)
	128,						// dtype_cidr (128 bits + prefix)
	48,							// dtype_macaddr (48 bits)

	// Enhanced text types
	0,							// dtype_citext (variable length)

	// Range types
	sizeof(SLONG) * 8 * 2,		// dtype_int4range (2 x 32-bit bounds)
	sizeof(SINT64) * 8 * 2,		// dtype_int8range (2 x 64-bit bounds)
	64 * 2,						// dtype_numrange (2 x double precision)
	sizeof(ISC_TIMESTAMP) * 8 * 2, // dtype_tsrange (2 timestamp bounds)
	sizeof(ISC_TIMESTAMP_TZ) * 8 * 2, // dtype_tstzrange (2 timestamp_tz bounds)
	sizeof(ISC_DATE) * 8 * 2,	// dtype_daterange (2 date bounds)

	// Advanced array types
	0,							// dtype_array_slice (variable length)
	0,							// dtype_array_md (variable length)

	// Full-text search types
	0,							// dtype_tsvector (variable length)
	0							// dtype_tsquery (variable length)
};

#endif /* JRD_ALIGN_H */
