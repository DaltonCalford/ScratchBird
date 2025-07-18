/*
 *	PROGRAM:	Interbase layered support library
 *	MODULE:		array.cpp
 *	DESCRIPTION:	Dynamic array support
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
 * 2001.09.18 Claudio Valderrama: get_name() was preventing the API calls
 *   isc_array_lookup_bounds, isc_lookup_desc & isc_array_set_desc
 *   from working properly with dialect 3 names. Therefore, incorrect names
 *   could be returned or a lookup for a blob field could fail. In addition,
 *   a possible buffer overrun due to unchecked bounds was closed. The fc
 *   get_name() as been renamed copy_exact_name().
 *
 * 2002-02-24 Sean Leyne - Code Cleanup of old Win 3.1 port (WINDOWS_ONLY)
 *
 *
 */

#include "firebird.h"
#include "firebird/Message.h"
#include <string.h>
#include <stdarg.h>
#include "ibase.h"
#include "../yvalve/array_proto.h"
#include "../yvalve/gds_proto.h"
#include "../yvalve/YObjects.h"
#include "../common/StatusArg.h"
#include "../jrd/constants.h"
#include "../common/utils_proto.h"

using namespace ScratchBird;

const int array_desc_column_major = 1;	// Set for FORTRAN

struct gen_t
{
	UCHAR* gen_sdl;
	UCHAR** gen_sdl_ptr;
	const UCHAR* gen_end;
	ISC_STATUS* gen_status;
	SSHORT gen_internal;
};


static void adjust_length(ISC_ARRAY_DESC*);
static void copy_exact_name (const char*, char*, SSHORT);
static ISC_STATUS error(ISC_STATUS* status, const Arg::StatusVector& v);
static ISC_STATUS gen_sdl(ISC_STATUS*, const ISC_ARRAY_DESC*, SSHORT*, UCHAR**, SSHORT*, bool);
static ISC_STATUS stuff_args(gen_t*, SSHORT, ...);
static ISC_STATUS stuff_literal(gen_t*, SLONG);
static ISC_STATUS stuff_string(gen_t*, UCHAR, const SCHAR*);

// stuff_sdl used in place of STUFF to avoid confusion with BLR STUFF
// macro defined in dsql.h

static inline ISC_STATUS stuff_sdl(gen_t* gen, int byte)
{
	return stuff_args(gen, 1, byte);
}

static inline ISC_STATUS stuff_sdl_word(gen_t* gen, int word)
{
	return stuff_args(gen, 2, word, word >> 8);
}

static inline ISC_STATUS stuff_sdl_long(gen_t* gen, int word)
{
	return stuff_args(gen, 4, word, word >> 8, word >> 16, word >> 24);
}


ISC_STATUS API_ROUTINE isc_array_gen_sdl(ISC_STATUS* status,
									 const ISC_ARRAY_DESC* desc,
									 SSHORT* sdl_buffer_length,
									 UCHAR* sdl_buffer, SSHORT* sdl_length)
{
/**************************************
  *
  *	i s c _ a r r a y _ g e n _ s d l
  *
  **************************************
  *
  * Functional description
  *
  **************************************/

 	return gen_sdl(status, desc, sdl_buffer_length, &sdl_buffer, sdl_length, false);
}


ISC_STATUS API_ROUTINE isc_array_get_slice(ISC_STATUS* status,
									   FB_API_HANDLE* db_handle,
									   FB_API_HANDLE* trans_handle,
									   ISC_QUAD* array_id,
									   const ISC_ARRAY_DESC* desc,
									   void* array,
									   SLONG* slice_length)
{
/**************************************
 *
 *	i s c _ a r r a y _ g e t _ s l i c e
 *
 **************************************
 *
 * Functional description
 *
 **************************************/
	UCHAR sdl_buffer[512];

	SSHORT sdl_length = sizeof(sdl_buffer);
	UCHAR* sdl = sdl_buffer;

	if (gen_sdl(status, desc, &sdl_length, &sdl, &sdl_length, true))
		return status[1];

	// SD: I do not complain but in ibase.h sdl is a plain char in functions'
	// declaration while it is UCHAR in the functions' implementations. Damned legacy!
	isc_get_slice(status, db_handle, trans_handle, array_id,
				  sdl_length, sdl,
				  0, NULL, *slice_length, array, slice_length);

	if (sdl != sdl_buffer)
		gds__free(sdl);

	return status[1];
}


void iscArrayLookupBoundsImpl(Why::YAttachment* attachment,
	Why::YTransaction* transaction, const SCHAR* relationName, const SCHAR* fieldName, ISC_ARRAY_DESC* desc)
{
	LocalStatus status;
	CheckStatusWrapper statusWrapper(&status);

	MetaString globalField;
	iscArrayLookupDescImpl(attachment, transaction, relationName, fieldName, desc, &globalField);

	ISC_ARRAY_BOUND* tail = desc->array_desc_bounds;

	USHORT majorOdsVersion = 0;
	USHORT minorOdsVersion = 0;
	attachment->getOdsVersion(&majorOdsVersion, &minorOdsVersion);

	constexpr auto sqlSchemas = R"""(
		with search_path as (
		    select row_number() over () rn,
		           name
		      from system.rdb$sql.parse_unqualified_names(rdb$get_context('SYSTEM', 'SEARCH_PATH'))
		)
		select fd.rdb$lower_bound,
		       fd.rdb$upper_bound
		    from search_path sp
		    join system.rdb$field_dimensions fd
		      on fd.rdb$schema_name = sp.name
		    where fd.rdb$field_name = ?
		    order by sp.rn
		    rows 1
	)""";

	constexpr auto sqlNoSchemas = R"""(
		select fd.rdb$lower_bound,
		       fd.rdb$upper_bound
		    from rdb$field_dimensions fd
		    where fd.rdb$field_name = ?
		    order by fd.rdb$dimension
	)""";

	const auto sql = majorOdsVersion >= ODS_VERSION14 ? sqlSchemas : sqlNoSchemas;

	FB_MESSAGE(InputMessage, CheckStatusWrapper,
		(FB_VARCHAR(MAX_SQL_IDENTIFIER_LEN), fieldName)
	) inputMessage(&statusWrapper, MasterInterfacePtr());
	inputMessage.clear();

	FB_MESSAGE(OutputMessage, CheckStatusWrapper,
		(FB_INTEGER, lowerBound)
		(FB_INTEGER, upperBound)
	) outputMessage(&statusWrapper, MasterInterfacePtr());

	inputMessage->fieldNameNull = FB_FALSE;
	inputMessage->fieldName.set(globalField.c_str());

	auto resultSet = makeNoIncRef(attachment->openCursor(&statusWrapper, transaction, 0, sql,
		SQL_DIALECT_CURRENT, inputMessage.getMetadata(), inputMessage.getData(),
		outputMessage.getMetadata(), nullptr, 0));
	status.check();

	while (resultSet->fetchNext(&statusWrapper, outputMessage.getData()) == IStatus::RESULT_OK)
	{
		tail->array_bound_lower = outputMessage->lowerBoundNull ? 0 : outputMessage->lowerBound;
		tail->array_bound_upper = outputMessage->upperBoundNull ? 0 : outputMessage->upperBound;
		++tail;
	}

	status.check();
}


void iscArrayLookupDescImpl(Why::YAttachment* attachment,
	Why::YTransaction* transaction, const SCHAR* relationName, const SCHAR* fieldName, ISC_ARRAY_DESC* desc,
	MetaString* globalField)
{
	LocalStatus status;
	CheckStatusWrapper statusWrapper(&status);

	copy_exact_name(fieldName, desc->array_desc_field_name, sizeof(desc->array_desc_field_name));
	copy_exact_name(relationName, desc->array_desc_relation_name, sizeof(desc->array_desc_relation_name));

	desc->array_desc_flags = 0;

	USHORT majorOdsVersion = 0;
	USHORT minorOdsVersion = 0;
	attachment->getOdsVersion(&majorOdsVersion, &minorOdsVersion);

	constexpr auto sqlSchemas = R"""(
		with search_path as (
		    select row_number() over () rn,
		           name
		      from system.rdb$sql.parse_unqualified_names(rdb$get_context('SYSTEM', 'SEARCH_PATH'))
		)
		select f.rdb$field_name,
		       f.rdb$field_type,
		       f.rdb$field_scale,
		       f.rdb$field_length,
		       f.rdb$dimensions
		    from search_path sp
		    join system.rdb$relation_fields rf
		      on rf.rdb$schema_name = sp.name
		    join system.rdb$fields f
		      on f.rdb$schema_name = rf.rdb$field_source_schema_name and
		         f.rdb$field_name = rf.rdb$field_source
		    where rf.rdb$relation_name = ? and
		          rf.rdb$field_name = ?
		    order by sp.rn
		    rows 1
	)""";

	constexpr auto sqlNoSchemas = R"""(
		select f.rdb$field_name,
		       f.rdb$field_type,
		       f.rdb$field_scale,
		       f.rdb$field_length,
		       f.rdb$dimensions
		    from rdb$relation_fields rf
		    join rdb$fields f
		      on f.rdb$field_name = rf.rdb$field_source
		    where rf.rdb$relation_name = ? and
		          rf.rdb$field_name = ?
	)""";

	const auto sql = majorOdsVersion >= ODS_VERSION14 ? sqlSchemas : sqlNoSchemas;

	FB_MESSAGE(InputMessage, CheckStatusWrapper,
		(FB_VARCHAR(MAX_SQL_IDENTIFIER_LEN), relationName)
		(FB_VARCHAR(MAX_SQL_IDENTIFIER_LEN), fieldName)
	) inputMessage(&statusWrapper, MasterInterfacePtr());
	inputMessage.clear();

	FB_MESSAGE(OutputMessage, CheckStatusWrapper,
		(FB_VARCHAR(MAX_SQL_IDENTIFIER_LEN), fieldName)
		(FB_INTEGER, fieldType)
		(FB_INTEGER, fieldScale)
		(FB_INTEGER, fieldLength)
		(FB_INTEGER, dimensions)
	) outputMessage(&statusWrapper, MasterInterfacePtr());

	inputMessage->relationNameNull = FB_FALSE;
	inputMessage->relationName.set((const char*) relationName);

	inputMessage->fieldNameNull = FB_FALSE;
	inputMessage->fieldName.set((const char*) fieldName);

	auto resultSet = makeNoIncRef(attachment->openCursor(&statusWrapper, transaction, 0, sql,
		SQL_DIALECT_CURRENT, inputMessage.getMetadata(), inputMessage.getData(),
		outputMessage.getMetadata(), nullptr, 0));
	status.check();

	if (resultSet->fetchNext(&statusWrapper, outputMessage.getData()) == IStatus::RESULT_OK)
	{
		if (globalField)
			globalField->assign(outputMessage->fieldName.str, outputMessage->fieldName.length);

		desc->array_desc_dtype = outputMessage->fieldTypeNull ? 0 : outputMessage->fieldType;
		desc->array_desc_scale = outputMessage->fieldScaleNull ? 0 : outputMessage->fieldScale;
		desc->array_desc_length = outputMessage->fieldLengthNull ? 0 : outputMessage->fieldLength;

		adjust_length(desc);
		desc->array_desc_dimensions = outputMessage->dimensionsNull ? 0 : outputMessage->dimensions;

		return;
	}

	status.check();

	(Arg::Gds(isc_fldnotdef) <<
		Arg::Str(desc->array_desc_field_name) <<
		Arg::Str(desc->array_desc_relation_name)).raise();
}


ISC_STATUS API_ROUTINE isc_array_put_slice(ISC_STATUS* status,
									   FB_API_HANDLE* db_handle,
									   FB_API_HANDLE* trans_handle,
									   ISC_QUAD* array_id,
									   const ISC_ARRAY_DESC* desc,
									   void* array,
									   SLONG* slice_length)
{
/**************************************
 *
 *	i s c _ a r r a y _ p u t _ s l i c e
 *
 **************************************
 *
 * Functional description
 *
 **************************************/
	UCHAR sdl_buffer[512];

	SSHORT sdl_length = sizeof(sdl_buffer);
	UCHAR* sdl = sdl_buffer;

	if (gen_sdl(status, desc, &sdl_length, &sdl, &sdl_length, true))
		return status[1];

	isc_put_slice(status, db_handle, trans_handle, array_id,
				  sdl_length, reinterpret_cast<const char*>(sdl),
				  0, NULL, *slice_length, array);

	if (sdl != sdl_buffer)
		gds__free(sdl);

	return status[1];
}


ISC_STATUS API_ROUTINE isc_array_set_desc(ISC_STATUS* status,
									  const SCHAR* relation_name,
									  const SCHAR* field_name,
									  const SSHORT* sql_dtype,
									  const SSHORT* sql_length,
									  const SSHORT* dimensions,
									  ISC_ARRAY_DESC* desc)
{
/**************************************
 *
 *	i s c _ a r r a y _ s e t _ d e s c
 *
 **************************************
 *
 * Functional description
 *
 **************************************/
    copy_exact_name(field_name, desc->array_desc_field_name, sizeof(desc->array_desc_field_name));
    copy_exact_name (relation_name, desc->array_desc_relation_name, sizeof(desc->array_desc_relation_name));

	desc->array_desc_flags = 0;
	desc->array_desc_dimensions = *dimensions;
	desc->array_desc_length = *sql_length;
	desc->array_desc_scale = 0;

	const SSHORT dtype = *sql_dtype & ~1;

	switch (dtype)
	{
	case SQL_VARYING:
		desc->array_desc_dtype = blr_varying;
		break;
	case SQL_TEXT:
		desc->array_desc_dtype = blr_text;
		break;
	case SQL_DOUBLE:
		desc->array_desc_dtype = blr_double;
		break;
	case SQL_FLOAT:
		desc->array_desc_dtype = blr_float;
		break;
	case SQL_D_FLOAT:
		desc->array_desc_dtype = blr_d_float;
		break;
	case SQL_TIMESTAMP:
		desc->array_desc_dtype = blr_timestamp;
		break;
	case SQL_TIMESTAMP_TZ:
		desc->array_desc_dtype = blr_timestamp_tz;
		break;
	case SQL_TIMESTAMP_TZ_EX:
		desc->array_desc_dtype = blr_ex_timestamp_tz;
		break;
	case SQL_TYPE_DATE:
		desc->array_desc_dtype = blr_sql_date;
		break;
	case SQL_TYPE_TIME:
		desc->array_desc_dtype = blr_sql_time;
		break;
	case SQL_TIME_TZ:
		desc->array_desc_dtype = blr_sql_time_tz;
		break;
	case SQL_TIME_TZ_EX:
		desc->array_desc_dtype = blr_ex_time_tz;
		break;
	case SQL_LONG:
		desc->array_desc_dtype = blr_long;
		break;
	case SQL_SHORT:
		desc->array_desc_dtype = blr_short;
		break;
	case SQL_INT64:
		desc->array_desc_dtype = blr_int64;
		break;
	case SQL_QUAD:
		desc->array_desc_dtype = blr_quad;
		break;
	case SQL_BOOLEAN:
		desc->array_desc_dtype = blr_bool;
		break;
	case SQL_DEC16:
		desc->array_desc_dtype = blr_dec64;
		break;
	case SQL_DEC34:
		desc->array_desc_dtype = blr_dec128;
		break;
	case SQL_INT128:
		desc->array_desc_dtype = blr_int128;
		break;
	default:
		return error(status, Arg::Gds(isc_sqlerr) << Arg::Num(-804) <<
							 Arg::Gds(isc_random) << Arg::Str("data type not understood"));
	}

	return error(status, Arg::Gds(FB_SUCCESS));
}


static void adjust_length(ISC_ARRAY_DESC*)
{
/**************************************
 *
 *	a d j u s t _ l e n g t h
 *
 **************************************
 *
 * Functional description
 *	Make architectural adjustment to fixed datatypes.
 *
 **************************************/
}


static void copy_exact_name (const char* from, char* to, SSHORT bsize)
{
/**************************************
 *
 *  c o p y _ e x a c t _ n a m e
 *
 **************************************
 *
 * Functional description
 *  Copy null terminated name ot stops at bsize - 1.
 *
 **************************************/
	const char* const from_end = from + bsize - 1;
	char* to2 = to - 1;
	while (*from && from < from_end)
	{
		if (*from != ' ') {
			to2 = to;
		}
		*to++ = *from++;
	}
	*++to2 = 0;
}


static ISC_STATUS error(ISC_STATUS* status, const Arg::StatusVector& v)
{
/**************************************
 *
 *	e r r o r
 *
 **************************************
 *
 * Functional description
 *	Stuff a status vector.
 *
 **************************************/
	return v.copyTo(status);
}


static ISC_STATUS gen_sdl(ISC_STATUS* status,
					  const ISC_ARRAY_DESC* desc,
					  SSHORT* sdl_buffer_length,
					  UCHAR** sdl_buffer,
                      SSHORT* sdl_length,
					  bool internal_flag)
{
/**************************************
 *
 *	g e n _ s d l
 *
 **************************************
 *
 * Functional description
 *
 **************************************/
	const SSHORT dimensions = desc->array_desc_dimensions;

	if (dimensions > 16)
		return error(status, Arg::Gds(isc_invalid_dimension) << Arg::Num(dimensions) << Arg::Num(16));

	gen_t gen_block;
	gen_t* gen = &gen_block;
	gen->gen_sdl = *sdl_buffer;
	gen->gen_sdl_ptr = sdl_buffer;
	gen->gen_end = *sdl_buffer + *sdl_buffer_length;
	gen->gen_status = status;
	gen->gen_internal = internal_flag ? 0 : -1;

	if (stuff_args(gen, 4, isc_sdl_version1, isc_sdl_struct, 1, desc->array_desc_dtype))
	{
		return status[1];
	}

	switch (desc->array_desc_dtype)
	{
	case blr_short:
	case blr_long:
	case blr_int64:
	case blr_quad:
	case blr_int128:
		if (stuff_sdl(gen, desc->array_desc_scale))
			return status[1];
		break;

	case blr_text:
	case blr_cstring:
	case blr_varying:
		if (stuff_sdl_word(gen, desc->array_desc_length))
			return status[1];
		break;
	default:
		break;
	}

	if (stuff_string(gen, isc_sdl_relation, desc->array_desc_relation_name))
		return status[1];

	if (stuff_string(gen, isc_sdl_field, desc->array_desc_field_name))
		return status[1];

	SSHORT from, to, increment;
	if (desc->array_desc_flags & array_desc_column_major)
	{
		from = dimensions - 1;
		to = -1;
		increment = -1;
	}
	else
	{
		from = 0;
		to = dimensions;
		increment = 1;
	}

    SSHORT n;
	for (n = from; n != to; n += increment)
	{
		const ISC_ARRAY_BOUND* tail = desc->array_desc_bounds + n;
		if (tail->array_bound_lower == 1)
		{
			if (stuff_args(gen, 2, isc_sdl_do1, n))
				return status[1];
		}
		else
		{
			if (stuff_args(gen, 2, isc_sdl_do2, n))
				return status[1];
			if (stuff_literal(gen, (SLONG) tail->array_bound_lower))
				return status[1];
		}
		if (stuff_literal(gen, (SLONG) tail->array_bound_upper))
			return status[1];
	}

	if (stuff_args(gen, 5, isc_sdl_element, 1, isc_sdl_scalar, 0, dimensions))
		return status[1];

	for (n = 0; n < dimensions; n++)
	{
		if (stuff_args(gen, 2, isc_sdl_variable, n))
			return status[1];
	}

	if (stuff_sdl(gen, isc_sdl_eoc))
		return status[1];
	*sdl_length = gen->gen_sdl - *gen->gen_sdl_ptr;

	return error(status, Arg::Gds(FB_SUCCESS));
}



static ISC_STATUS stuff_args(gen_t* gen, SSHORT count, ...)
{
/**************************************
 *
 *	s t u f f
 *
 **************************************
 *
 * Functional description
 *	Stuff a SDL byte.
 *
 **************************************/
	UCHAR c;
	va_list ptr;

	if (gen->gen_sdl + count >= gen->gen_end)
	{
		if (gen->gen_internal < 0) {
			return error(gen->gen_status, Arg::Gds(isc_misc_interpreted) << Arg::Str("SDL buffer overflow"));
		}

		// The sdl buffer is too small.  Allocate a larger one.

		const SSHORT new_len = gen->gen_end - *gen->gen_sdl_ptr + 512 + count;
		UCHAR* const new_sdl = (UCHAR*) gds__alloc(new_len);
		if (!new_sdl)
		{
			return error(gen->gen_status, Arg::Gds(isc_misc_interpreted) << Arg::Str("SDL buffer overflow") <<
										  Arg::Gds(isc_virmemexh));
		}

		const SSHORT current_len = gen->gen_sdl - *gen->gen_sdl_ptr;
		memcpy(new_sdl, *gen->gen_sdl_ptr, current_len);
		if (gen->gen_internal++)
			gds__free(*gen->gen_sdl_ptr);
		gen->gen_sdl = new_sdl + current_len;
		*gen->gen_sdl_ptr = new_sdl;
		gen->gen_end = new_sdl + new_len;
	}

	va_start(ptr, count);

	for (; count; --count)
	{
		c = va_arg(ptr, int);
		*(gen->gen_sdl)++ = c;
	}

	va_end(ptr);
	return 0;
}


static ISC_STATUS stuff_literal(gen_t* gen, SLONG literal)
{
/**************************************
 *
 *	s t u f f _ l i t e r a l
 *
 **************************************
 *
 * Functional description
 *	Stuff an SDL literal.
 *
 **************************************/
	ISC_STATUS*	status = gen->gen_status;

	if (literal >= -128 && literal <= 127)
		return stuff_args(gen, 2, isc_sdl_tiny_integer, literal);

	if (literal >= -32768 && literal <= 32767)
		return stuff_args(gen, 3, isc_sdl_short_integer, literal, literal >> 8);

	if (stuff_sdl(gen, isc_sdl_long_integer))
		return status[1];
	if (stuff_sdl_long(gen, literal))
		return status[1];

	return FB_SUCCESS;
}


static ISC_STATUS stuff_string(gen_t* gen, UCHAR sdl, const SCHAR* string)
{
/**************************************
 *
 *	s t u f f _ s t r i n g
 *
 **************************************
 *
 * Functional description
 *	Stuff a "thing" then a counted string.
 *
 **************************************/
	ISC_STATUS* status = gen->gen_status;

	if (stuff_sdl(gen, sdl))
		return status[1];
	if (stuff_sdl(gen, static_cast<int>(strlen(string))))
		return status[1];

	while (*string)
	{
		if (stuff_sdl(gen, *string++))
			return status[1];
	}

	return FB_SUCCESS;
}
