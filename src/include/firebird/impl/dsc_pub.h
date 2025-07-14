/*
 *	PROGRAM:	JRD access method
 *	MODULE:		dsc.h
 *	DESCRIPTION:	Definitions associated with descriptors
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
 * 2002.04.16  Paul Beach - HP10 Define changed from -4 to (-4) to make it
 *             compatible with the HP Compiler
 */

#ifndef FIREBIRD_IMPL_DSC_PUB_H
#define FIREBIRD_IMPL_DSC_PUB_H


/*
 * The following flags are used in an internal structure dsc (dsc.h) or in the external one paramdsc (ibase.h)
 */

/* values for dsc_flags
 * Note: DSC_null is only reliably set for local variables (blr_variable)
 */
#define DSC_null		1
#define DSC_no_subtype	2	/* dsc has no sub type specified */
#define DSC_nullable  	4	/* not stored. instead, is derived
							   from metadata primarily to flag
							   SQLDA (in DSQL)               */

#define dtype_unknown	0
#define dtype_text		1
#define dtype_cstring	2
#define dtype_varying	3

#define dtype_packed	6
#define dtype_byte		7
#define dtype_short		8
#define dtype_long		9
#define dtype_quad		10
#define dtype_real		11
#define dtype_double	12
#define dtype_d_float	13
#define dtype_sql_date	14
#define dtype_sql_time	15
#define dtype_timestamp	16
#define dtype_blob		17
#define dtype_array		18
#define dtype_int64		19
#define dtype_dbkey		20
#define dtype_boolean	21
#define dtype_dec64		22
#define dtype_dec128	23
#define dtype_int128	24
#define dtype_sql_time_tz	25
#define dtype_timestamp_tz	26
#define dtype_ex_time_tz	27
#define dtype_ex_timestamp_tz	28
#define dtype_uuid		29
#define dtype_json		30

/* ScratchBird unsigned integer extensions */
#define dtype_ushort	31	/* USMALLINT - 16-bit unsigned integer */
#define dtype_ulong		32	/* UINTEGER - 32-bit unsigned integer */
#define dtype_uint64	33	/* UBIGINT - 64-bit unsigned integer */
#define dtype_uint128	34	/* UINT128 - 128-bit unsigned integer */

/* ScratchBird enhanced VARCHAR support */
#define dtype_varying_large	35	/* Enhanced VARCHAR with 128KB support */

/* ScratchBird network address types */
#define dtype_inet		36	/* INET - IPv4/IPv6 addresses */
#define dtype_cidr		37	/* CIDR - Network blocks */
#define dtype_macaddr	38	/* MACADDR - MAC addresses */

/* ScratchBird enhanced text types */
#define dtype_citext	39	/* CITEXT - Case-insensitive text */

/* ScratchBird range types */
#define dtype_int4range		40	/* INT4RANGE - Integer ranges */
#define dtype_int8range		41	/* INT8RANGE - Big integer ranges */
#define dtype_numrange		42	/* NUMRANGE - Numeric ranges */
#define dtype_tsrange		43	/* TSRANGE - Timestamp ranges */
#define dtype_tstzrange		44	/* TSTZRANGE - Timestamp with timezone ranges */
#define dtype_daterange		45	/* DATERANGE - Date ranges */

/* ScratchBird advanced array types */
#define dtype_array_slice	46	/* Array slice operations */
#define dtype_array_md		47	/* Multi-dimensional arrays */

/* ScratchBird full-text search types */
#define dtype_tsvector		48	/* TSVECTOR - Text search vector */
#define dtype_tsquery		49	/* TSQUERY - Text search query */

#define DTYPE_TYPE_MAX		50

#define ISC_TIME_SECONDS_PRECISION		10000
#define ISC_TIME_SECONDS_PRECISION_SCALE	(-4)

#endif /* FIREBIRD_IMPL_DSC_PUB_H */
