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

// Unsigned integer types
#define dtype_ushort		31
#define dtype_ulong		32
#define dtype_uint64		33
#define dtype_uint128		34

// Enhanced VARCHAR support
#define dtype_varying_large	35

// Network address types
#define dtype_inet		36
#define dtype_cidr		37
#define dtype_macaddr		38

// Enhanced text types
#define dtype_citext		39

// Range types
#define dtype_int4range		40
#define dtype_int8range		41
#define dtype_numrange		42
#define dtype_tsrange		43
#define dtype_tstzrange		44
#define dtype_daterange		45

// Advanced array types
#define dtype_array_slice	46
#define dtype_array_md		47

// Full-text search types
#define dtype_tsvector		48
#define dtype_tsquery		49

#define DTYPE_TYPE_MAX	50

#define ISC_TIME_SECONDS_PRECISION		10000
#define ISC_TIME_SECONDS_PRECISION_SCALE	(-4)

#endif /* FIREBIRD_IMPL_DSC_PUB_H */
