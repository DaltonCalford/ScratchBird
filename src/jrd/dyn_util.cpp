/*
 *	PROGRAM:	JRD Data Definition Utility
 *	MODULE:		dyn_util.cpp
 *	DESCRIPTION:	Dynamic data definition - utility functions
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
 * 2002-02-24 Sean Leyne - Code Cleanup of old Win 3.1 port (WINDOWS_ONLY)
 *
 */

#include "scratchbird.h"
#include "dyn_consts.h"
#include <stdio.h>
#include <string.h>

#include "../jrd/jrd.h"
#include "../jrd/tra.h"
#include "../jrd/scl.h"
#include "../jrd/drq.h"
#include "../jrd/flags.h"
#include "../jrd/lls.h"
#include "../jrd/met.h"
#include "../jrd/btr.h"
#include "../jrd/intl.h"
#include "../jrd/dyn.h"
#include "../jrd/ods.h"
#include "../jrd/blb_proto.h"
#include "../jrd/cmp_proto.h"
#include "../jrd/dyn_ut_proto.h"
#include "../jrd/err_proto.h"
#include "../jrd/exe_proto.h"
#include "../yvalve/gds_proto.h"
#include "../jrd/inf_proto.h"
#include "../jrd/intl_proto.h"
#include "../common/isc_f_proto.h"
#include "../jrd/vio_proto.h"
#include "../common/utils_proto.h"

using MsgFormat::SafeArg;

using namespace ScratchBird;
using namespace Jrd;

// Replaced GPRE DATABASE DB = STATIC "ODS.RDB"; with modern approach
// Database access is handled through existing attachment mechanisms

// Constants for GPRE conversion
#ifndef MAX_SQL_IDENTIFIER_LEN
#define MAX_SQL_IDENTIFIER_LEN 68
#endif

static const UCHAR gen_id_blr1[] =
{
	blr_version5,
	blr_begin,
	blr_message, 0, 1, 0,
	blr_int64, 0,
	blr_begin,
	blr_send, 0,
	blr_begin,
	blr_assignment,
	blr_gen_id3,
	6, 'S', 'Y', 'S', 'T', 'E', 'M'	// SYSTEM_SCHEMA
};

static const UCHAR gen_id_blr2[] =
{
	1,
	blr_literal, blr_long, 0, 1, 0, 0, 0,
	blr_parameter, 0, 0, 0,
	blr_end, blr_end, blr_end, blr_eoc
};

// Check if an object already exists. If yes, return false.
bool DYN_UTIL_check_unique_name_nothrow(thread_db* tdbb, jrd_tra* transaction,
	const QualifiedName& object_name, int object_type, USHORT* errorCode)
{
	SET_TDBB(tdbb);

	USHORT tempErrorCode;
	errorCode = errorCode ? errorCode : &tempErrorCode;
	*errorCode = 0;

	AutoCacheRequest requestHandle;

	switch (object_type)
	{
		case obj_relation:
		case obj_procedure:
		{
			static const CachedRequestId relationHandleId;
			requestHandle.reset(tdbb, relationHandleId);

			// Converted FOR loop #1: FOR(REQUEST_HANDLE requestHandle TRANSACTION_HANDLE transaction) EREL IN RDB$RELATIONS
			jrd_req* handle = requestHandle;
			EXE_start(tdbb, handle, transaction);
			
			struct {
				TEXT schema_name[MAX_SQL_IDENTIFIER_LEN];
				TEXT relation_name[MAX_SQL_IDENTIFIER_LEN];
			} rel_input;
			
			strcpy(rel_input.schema_name, object_name.schema.c_str());
			strcpy(rel_input.relation_name, object_name.object.c_str());
			
			EXE_send(tdbb, handle, 0, sizeof(rel_input), reinterpret_cast<UCHAR*>(&rel_input));

			struct {
				TEXT RDB$RELATION_NAME[MAX_SQL_IDENTIFIER_LEN];
			} rel_data;

			while (!EXE_receive(tdbb, handle, 1, sizeof(rel_data), reinterpret_cast<UCHAR*>(&rel_data)))
			{
				*errorCode = 132;	// isc_dyn_dup_table
			}

			if (!*errorCode)
			{
				static const CachedRequestId procedureHandleId;
				requestHandle.reset(tdbb, procedureHandleId);

				// Converted FOR loop #2: FOR (REQUEST_HANDLE requestHandle TRANSACTION_HANDLE transaction) EPRC IN RDB$PROCEDURES
				jrd_req* prc_handle = requestHandle;
				EXE_start(tdbb, prc_handle, transaction);
				
				struct {
					TEXT schema_name[MAX_SQL_IDENTIFIER_LEN];
					TEXT procedure_name[MAX_SQL_IDENTIFIER_LEN];
				} prc_input;
				
				strcpy(prc_input.schema_name, object_name.schema.c_str());
				strcpy(prc_input.procedure_name, object_name.object.c_str());
				
				EXE_send(tdbb, prc_handle, 0, sizeof(prc_input), reinterpret_cast<UCHAR*>(&prc_input));

				struct {
					TEXT RDB$PROCEDURE_NAME[MAX_SQL_IDENTIFIER_LEN];
					SSHORT package_name_null;
				} prc_data;

				while (!EXE_receive(tdbb, prc_handle, 1, sizeof(prc_data), reinterpret_cast<UCHAR*>(&prc_data)))
				{
					if (prc_data.package_name_null) // EPRC.RDB$PACKAGE_NAME MISSING
					{
						*errorCode = 135;	// isc_dyn_dup_procedure
					}
				}
			}
			break;
		}

		case obj_index:
		{
			static const CachedRequestId indexHandleId;
			requestHandle.reset(tdbb, indexHandleId);

			// Converted FOR loop #3: FOR(REQUEST_HANDLE requestHandle TRANSACTION_HANDLE transaction) EIDX IN RDB$INDICES
			jrd_req* handle = requestHandle;
			EXE_start(tdbb, handle, transaction);
			
			struct {
				TEXT schema_name[MAX_SQL_IDENTIFIER_LEN];
				TEXT index_name[MAX_SQL_IDENTIFIER_LEN];
			} idx_input;
			
			strcpy(idx_input.schema_name, object_name.schema.c_str());
			strcpy(idx_input.index_name, object_name.object.c_str());
			
			EXE_send(tdbb, handle, 0, sizeof(idx_input), reinterpret_cast<UCHAR*>(&idx_input));

			struct {
				TEXT RDB$INDEX_NAME[MAX_SQL_IDENTIFIER_LEN];
			} idx_data;

			while (!EXE_receive(tdbb, handle, 1, sizeof(idx_data), reinterpret_cast<UCHAR*>(&idx_data)))
			{
				*errorCode = 251;	// isc_dyn_dup_index
			}

			break;
		}

		case obj_exception:
		{
			static const CachedRequestId exceptionHandleId;
			requestHandle.reset(tdbb, exceptionHandleId);

			// Converted FOR loop #4: FOR(REQUEST_HANDLE requestHandle TRANSACTION_HANDLE transaction) EXCP IN RDB$EXCEPTIONS
			jrd_req* handle = requestHandle;
			EXE_start(tdbb, handle, transaction);
			
			struct {
				TEXT schema_name[MAX_SQL_IDENTIFIER_LEN];
				TEXT exception_name[MAX_SQL_IDENTIFIER_LEN];
			} excp_input;
			
			strcpy(excp_input.schema_name, object_name.schema.c_str());
			strcpy(excp_input.exception_name, object_name.object.c_str());
			
			EXE_send(tdbb, handle, 0, sizeof(excp_input), reinterpret_cast<UCHAR*>(&excp_input));

			struct {
				TEXT RDB$EXCEPTION_NAME[MAX_SQL_IDENTIFIER_LEN];
			} excp_data;

			while (!EXE_receive(tdbb, handle, 1, sizeof(excp_data), reinterpret_cast<UCHAR*>(&excp_data)))
			{
				*errorCode = 253;	// isc_dyn_dup_exception
			}

			break;
		}

		case obj_generator:
		{
			static const CachedRequestId generatorHandleId;
			requestHandle.reset(tdbb, generatorHandleId);

			// Converted FOR loop #5: FOR(REQUEST_HANDLE requestHandle TRANSACTION_HANDLE transaction) EGEN IN RDB$GENERATORS
			jrd_req* handle = requestHandle;
			EXE_start(tdbb, handle, transaction);
			
			struct {
				TEXT schema_name[MAX_SQL_IDENTIFIER_LEN];
				TEXT generator_name[MAX_SQL_IDENTIFIER_LEN];
			} gen_input;
			
			strcpy(gen_input.schema_name, object_name.schema.c_str());
			strcpy(gen_input.generator_name, object_name.object.c_str());
			
			EXE_send(tdbb, handle, 0, sizeof(gen_input), reinterpret_cast<UCHAR*>(&gen_input));

			struct {
				TEXT RDB$GENERATOR_NAME[MAX_SQL_IDENTIFIER_LEN];
			} gen_data;

			while (!EXE_receive(tdbb, handle, 1, sizeof(gen_data), reinterpret_cast<UCHAR*>(&gen_data)))
			{
				*errorCode = 254;	// isc_dyn_dup_generator
			}

			break;
		}

		case obj_udf:
		{
			static const CachedRequestId udfHandleId;
			requestHandle.reset(tdbb, udfHandleId);

			// Converted FOR loop #6: FOR(REQUEST_HANDLE requestHandle TRANSACTION_HANDLE transaction) EFUN IN RDB$FUNCTIONS
			jrd_req* handle = requestHandle;
			EXE_start(tdbb, handle, transaction);
			
			struct {
				TEXT schema_name[MAX_SQL_IDENTIFIER_LEN];
				TEXT function_name[MAX_SQL_IDENTIFIER_LEN];
			} fun_input;
			
			strcpy(fun_input.schema_name, object_name.schema.c_str());
			strcpy(fun_input.function_name, object_name.object.c_str());
			
			EXE_send(tdbb, handle, 0, sizeof(fun_input), reinterpret_cast<UCHAR*>(&fun_input));

			struct {
				TEXT RDB$FUNCTION_NAME[MAX_SQL_IDENTIFIER_LEN];
				SSHORT package_name_null;
			} fun_data;

			while (!EXE_receive(tdbb, handle, 1, sizeof(fun_data), reinterpret_cast<UCHAR*>(&fun_data)))
			{
				if (fun_data.package_name_null) // EFUN.RDB$PACKAGE_NAME MISSING
				{
					*errorCode = 268;	// isc_dyn_dup_function
				}
			}

			break;
		}

		case obj_trigger:
		{
			static const CachedRequestId triggerHandleId;
			requestHandle.reset(tdbb, triggerHandleId);

			// Converted FOR loop #7: FOR(REQUEST_HANDLE requestHandle TRANSACTION_HANDLE transaction) TRG IN RDB$TRIGGERS
			jrd_req* handle = requestHandle;
			EXE_start(tdbb, handle, transaction);
			
			struct {
				TEXT schema_name[MAX_SQL_IDENTIFIER_LEN];
				TEXT trigger_name[MAX_SQL_IDENTIFIER_LEN];
			} trg_input;
			
			strcpy(trg_input.schema_name, object_name.schema.c_str());
			strcpy(trg_input.trigger_name, object_name.object.c_str());
			
			EXE_send(tdbb, handle, 0, sizeof(trg_input), reinterpret_cast<UCHAR*>(&trg_input));

			struct {
				TEXT RDB$TRIGGER_NAME[MAX_SQL_IDENTIFIER_LEN];
			} trg_data;

			while (!EXE_receive(tdbb, handle, 1, sizeof(trg_data), reinterpret_cast<UCHAR*>(&trg_data)))
			{
				*errorCode = 310;	// isc_dyn_dup_trigger
			}

			break;
		}

		case obj_field:
		{
			static const CachedRequestId fieldHandleId;
			requestHandle.reset(tdbb, fieldHandleId);

			// Converted FOR loop #8: FOR(REQUEST_HANDLE requestHandle TRANSACTION_HANDLE transaction) FLD IN RDB$FIELDS
			jrd_req* handle = requestHandle;
			EXE_start(tdbb, handle, transaction);
			
			struct {
				TEXT schema_name[MAX_SQL_IDENTIFIER_LEN];
				TEXT field_name[MAX_SQL_IDENTIFIER_LEN];
			} fld_input;
			
			strcpy(fld_input.schema_name, object_name.schema.c_str());
			strcpy(fld_input.field_name, object_name.object.c_str());
			
			EXE_send(tdbb, handle, 0, sizeof(fld_input), reinterpret_cast<UCHAR*>(&fld_input));

			struct {
				TEXT RDB$FIELD_NAME[MAX_SQL_IDENTIFIER_LEN];
			} fld_data;

			while (!EXE_receive(tdbb, handle, 1, sizeof(fld_data), reinterpret_cast<UCHAR*>(&fld_data)))
			{
				*errorCode = 311;	// isc_dyn_dup_domain
			}

			break;
		}

		case obj_collation:
		{
			static const CachedRequestId collationHandleId;
			requestHandle.reset(tdbb, collationHandleId);

			// Converted FOR loop #9: FOR(REQUEST_HANDLE requestHandle TRANSACTION_HANDLE transaction) COLL IN RDB$COLLATIONS
			jrd_req* handle = requestHandle;
			EXE_start(tdbb, handle, transaction);
			
			struct {
				TEXT schema_name[MAX_SQL_IDENTIFIER_LEN];
				TEXT collation_name[MAX_SQL_IDENTIFIER_LEN];
			} coll_input;
			
			strcpy(coll_input.schema_name, object_name.schema.c_str());
			strcpy(coll_input.collation_name, object_name.object.c_str());
			
			EXE_send(tdbb, handle, 0, sizeof(coll_input), reinterpret_cast<UCHAR*>(&coll_input));

			struct {
				TEXT RDB$COLLATION_NAME[MAX_SQL_IDENTIFIER_LEN];
			} coll_data;

			while (!EXE_receive(tdbb, handle, 1, sizeof(coll_data), reinterpret_cast<UCHAR*>(&coll_data)))
			{
				*errorCode = 312;	// isc_dyn_dup_collation
			}

			break;
		}

		case obj_package_header:
		{
			static const CachedRequestId packageHandleId;
			requestHandle.reset(tdbb, packageHandleId);

			// Converted FOR loop #10: FOR(REQUEST_HANDLE requestHandle TRANSACTION_HANDLE transaction) PKG IN RDB$PACKAGES
			jrd_req* handle = requestHandle;
			EXE_start(tdbb, handle, transaction);
			
			struct {
				TEXT schema_name[MAX_SQL_IDENTIFIER_LEN];
				TEXT package_name[MAX_SQL_IDENTIFIER_LEN];
			} pkg_input;
			
			strcpy(pkg_input.schema_name, object_name.schema.c_str());
			strcpy(pkg_input.package_name, object_name.object.c_str());
			
			EXE_send(tdbb, handle, 0, sizeof(pkg_input), reinterpret_cast<UCHAR*>(&pkg_input));

			struct {
				TEXT RDB$PACKAGE_NAME[MAX_SQL_IDENTIFIER_LEN];
			} pkg_data;

			while (!EXE_receive(tdbb, handle, 1, sizeof(pkg_data), reinterpret_cast<UCHAR*>(&pkg_data)))
			{
				*errorCode = 313;	// isc_dyn_dup_package
			}

			break;
		}

		case obj_schema:
		{
			fb_assert(object_name.schema.isEmpty());

			static const CachedRequestId schemaHandleId;
			requestHandle.reset(tdbb, schemaHandleId);

			// Converted FOR loop #11: FOR(REQUEST_HANDLE requestHandle TRANSACTION_HANDLE transaction) SCH IN RDB$SCHEMAS
			jrd_req* handle = requestHandle;
			EXE_start(tdbb, handle, transaction);
			
			struct {
				TEXT schema_name[MAX_SQL_IDENTIFIER_LEN];
			} sch_input;
			
			strcpy(sch_input.schema_name, object_name.object.c_str());
			
			EXE_send(tdbb, handle, 0, sizeof(sch_input), reinterpret_cast<UCHAR*>(&sch_input));

			struct {
				TEXT RDB$SCHEMA_NAME[MAX_SQL_IDENTIFIER_LEN];
			} sch_data;

			while (!EXE_receive(tdbb, handle, 1, sizeof(sch_data), reinterpret_cast<UCHAR*>(&sch_data)))
			{
				*errorCode = 316;	// isc_dyn_dup_schema
			}

			break;
		}

		default:
			fb_assert(false);
	}

	return *errorCode == 0;
}

// Check if an object already exists. If yes, throw error.
void DYN_UTIL_check_unique_name(thread_db* tdbb, jrd_tra* transaction, const QualifiedName& object_name,
	int object_type)
{
	USHORT errorCode;

	if (!DYN_UTIL_check_unique_name_nothrow(tdbb, transaction, object_name, object_type, &errorCode))
		status_exception::raise(Arg::PrivateDyn(errorCode) << object_name.toQuotedString());
}


SINT64 DYN_UTIL_gen_unique_id(thread_db* tdbb, SSHORT id, const char* generator_name)
{
/**************************************
 *
 *	D Y N _ U T I L _ g e n _ u n i q u e _ i d
 *
 **************************************
 *
 * Functional description
 *	Generate a unique id using a generator.
 *
 **************************************/
	SET_TDBB(tdbb);
	Jrd::Attachment* attachment = tdbb->getAttachment();

	AutoCacheRequest request(tdbb, id, DYN_REQUESTS);
	SINT64 value = 0;

	if (!request)
	{
		const FB_SIZE_T name_length = fb_strlen(generator_name);
		fb_assert(name_length < MAX_SQL_IDENTIFIER_SIZE);
		const FB_SIZE_T blr_size = static_cast<FB_SIZE_T>(
			sizeof(gen_id_blr1) + sizeof(gen_id_blr2)) + 1 + name_length;

		ScratchBird::UCharBuffer blr;
		UCHAR* p = blr.getBuffer(blr_size);

		memcpy(p, gen_id_blr1, sizeof(gen_id_blr1));
		p += sizeof(gen_id_blr1);
		*p++ = name_length;
		memcpy(p, generator_name, name_length);
		p += name_length;
		memcpy(p, gen_id_blr2, sizeof(gen_id_blr2));
		p += sizeof(gen_id_blr2);
		fb_assert(size_t(p - blr.begin()) == blr_size);

		request.compile(tdbb, blr.begin(), (ULONG) blr.getCount());
	}

	EXE_start(tdbb, request, attachment->getSysTransaction());
	EXE_receive(tdbb, request, 0, sizeof(value), (UCHAR*) &value);

	return value;
}


void DYN_UTIL_generate_constraint_name(thread_db* tdbb, QualifiedName& buffer)
{
/**************************************
 *
 *	D Y N _ U T I L _ g e n e r a t e _ c o n s t r a i n t _ n a m e
 *
 **************************************
 *
 * Functional description
 *	Generate a name unique to RDB$RELATION_CONSTRAINTS.
 *
 **************************************/
	SET_TDBB(tdbb);

	fb_assert(buffer.schema.hasData());

	Jrd::Attachment* attachment = tdbb->getAttachment();
	bool found = false;

	do
	{
		buffer.object.printf("INTEG_%" SQUADFORMAT,
				DYN_UTIL_gen_unique_id(tdbb, drq_g_nxt_con, "RDB$CONSTRAINT_NAME"));

		AutoCacheRequest request(tdbb, drq_f_nxt_con, DYN_REQUESTS);

		found = false;
		
		// Converted FOR loop #12: FOR(REQUEST_HANDLE request) FIRST 1 X IN RDB$RELATION_CONSTRAINTS
		jrd_req* handle = request;
		EXE_start(tdbb, handle, attachment->getSysTransaction());
		
		struct {
			TEXT schema_name[MAX_SQL_IDENTIFIER_LEN];
			TEXT constraint_name[MAX_SQL_IDENTIFIER_LEN];
		} con_input;
		
		strcpy(con_input.schema_name, buffer.schema.c_str());
		strcpy(con_input.constraint_name, buffer.object.c_str());
		
		EXE_send(tdbb, handle, 0, sizeof(con_input), reinterpret_cast<UCHAR*>(&con_input));

		struct {
			TEXT RDB$CONSTRAINT_NAME[MAX_SQL_IDENTIFIER_LEN];
		} con_data;

		while (!EXE_receive(tdbb, handle, 1, sizeof(con_data), reinterpret_cast<UCHAR*>(&con_data)))
		{
			found = true;
		}
	} while (found);
}


void DYN_UTIL_generate_field_name(thread_db* tdbb, QualifiedName& buffer)
{
/**************************************
 *
 *	D Y N _ U T I L _ g e n e r a t e _ f i e l d _ n a m e
 *
 **************************************
 *
 * Functional description
 *	Generate a name unique to RDB$FIELDS.
 *
 **************************************/
	SET_TDBB(tdbb);

	fb_assert(buffer.schema.hasData());

	Jrd::Attachment* attachment = tdbb->getAttachment();
	bool found = false;

	do
	{
		buffer.object.printf("RDB$%" SQUADFORMAT,
				DYN_UTIL_gen_unique_id(tdbb, drq_g_nxt_fld, "RDB$FIELD_NAME"));

		AutoCacheRequest request(tdbb, drq_f_nxt_fld, DYN_REQUESTS);

		found = false;
		
		// Converted FOR loop #13: FOR(REQUEST_HANDLE request) FIRST 1 X IN RDB$FIELDS
		jrd_req* handle = request;
		EXE_start(tdbb, handle, attachment->getSysTransaction());
		
		struct {
			TEXT schema_name[MAX_SQL_IDENTIFIER_LEN];
			TEXT field_name[MAX_SQL_IDENTIFIER_LEN];
		} fld_input;
		
		strcpy(fld_input.schema_name, buffer.schema.c_str());
		strcpy(fld_input.field_name, buffer.object.c_str());
		
		EXE_send(tdbb, handle, 0, sizeof(fld_input), reinterpret_cast<UCHAR*>(&fld_input));

		struct {
			TEXT RDB$FIELD_NAME[MAX_SQL_IDENTIFIER_LEN];
		} fld_data;

		while (!EXE_receive(tdbb, handle, 1, sizeof(fld_data), reinterpret_cast<UCHAR*>(&fld_data)))
		{
			found = true;
		}
	} while (found);
}


void DYN_UTIL_generate_field_position(thread_db* tdbb, const QualifiedName& relation_name,
	SLONG* field_pos)
{
/**************************************
 *
 *	D Y N _ U T I L _ g e n e r a t e _ f i e l d _ p o s i t i o n
 *
 **************************************
 *
 * Functional description
 *	Generate a field position if not specified
 *
 **************************************/
	SLONG field_position = -1;

	SET_TDBB(tdbb);
	Jrd::Attachment* attachment = tdbb->getAttachment();

	AutoCacheRequest request(tdbb, drq_l_fld_pos, DYN_REQUESTS);

	// Converted FOR loop #14: FOR(REQUEST_HANDLE request) X IN RDB$RELATION_FIELDS
	jrd_req* handle = request;
	EXE_start(tdbb, handle, attachment->getSysTransaction());
	
	struct {
		TEXT schema_name[MAX_SQL_IDENTIFIER_LEN];
		TEXT relation_name[MAX_SQL_IDENTIFIER_LEN];
	} rf_input;
	
	strcpy(rf_input.schema_name, relation_name.schema.c_str());
	strcpy(rf_input.relation_name, relation_name.object.c_str());
	
	EXE_send(tdbb, handle, 0, sizeof(rf_input), reinterpret_cast<UCHAR*>(&rf_input));

	struct {
		SLONG RDB$FIELD_POSITION;
		SSHORT field_position_null;
	} rf_data;

	while (!EXE_receive(tdbb, handle, 1, sizeof(rf_data), reinterpret_cast<UCHAR*>(&rf_data)))
	{
		if (rf_data.field_position_null) // X.RDB$FIELD_POSITION.NULL
			continue;

		field_position = MAX(rf_data.RDB$FIELD_POSITION, field_position);
	}

	*field_pos = field_position;
}


void DYN_UTIL_generate_index_name(thread_db* tdbb, jrd_tra* /*transaction*/,
								  QualifiedName& buffer, UCHAR verb)
{
/**************************************
 *
 *	D Y N _ U T I L _ g e n e r a t e _ i n d e x _ n a m e
 *
 **************************************
 *
 * Functional description
 *	Generate a name unique to RDB$INDICES.
 *
 **************************************/
	SET_TDBB(tdbb);

	fb_assert(buffer.schema.hasData());

	Jrd::Attachment* attachment = tdbb->getAttachment();
	bool found = false;

	do
	{
		const SCHAR* format;
		switch (verb)
		{
			case isc_dyn_def_primary_key:
				format = "RDB$PRIMARY%" SQUADFORMAT;
				break;
			case isc_dyn_def_foreign_key:
				format = "RDB$FOREIGN%" SQUADFORMAT;
				break;
			default:
				format = "RDB$%" SQUADFORMAT;
		}

		buffer.object.printf(format,
			DYN_UTIL_gen_unique_id(tdbb, drq_g_nxt_idx, "RDB$INDEX_NAME"));

		AutoCacheRequest request(tdbb, drq_f_nxt_idx, DYN_REQUESTS);

		found = false;
		
		// Converted FOR loop #15: FOR(REQUEST_HANDLE request) FIRST 1 X IN RDB$INDICES
		jrd_req* handle = request;
		EXE_start(tdbb, handle, attachment->getSysTransaction());
		
		struct {
			TEXT schema_name[MAX_SQL_IDENTIFIER_LEN];
			TEXT index_name[MAX_SQL_IDENTIFIER_LEN];
		} idx_input;
		
		strcpy(idx_input.schema_name, buffer.schema.c_str());
		strcpy(idx_input.index_name, buffer.object.c_str());
		
		EXE_send(tdbb, handle, 0, sizeof(idx_input), reinterpret_cast<UCHAR*>(&idx_input));

		struct {
			TEXT RDB$INDEX_NAME[MAX_SQL_IDENTIFIER_LEN];
		} idx_data;

		while (!EXE_receive(tdbb, handle, 1, sizeof(idx_data), reinterpret_cast<UCHAR*>(&idx_data)))
		{
			found = true;
		}
	} while (found);
}


// Generate a name unique to RDB$GENERATORS.
void DYN_UTIL_generate_generator_name(thread_db* tdbb, QualifiedName& buffer)
{
	SET_TDBB(tdbb);

	fb_assert(buffer.schema.hasData());

	Jrd::Attachment* attachment = tdbb->getAttachment();

	AutoCacheRequest request(tdbb, drq_f_nxt_gen, DYN_REQUESTS);
	bool found = false;

	do
	{
		buffer.object.printf("RDB$%" SQUADFORMAT,
			DYN_UTIL_gen_unique_id(tdbb, drq_g_nxt_gen, "RDB$GENERATOR_NAME"));

		found = false;

		// Converted FOR loop #16: FOR (REQUEST_HANDLE request) FIRST 1 X IN RDB$GENERATORS
		jrd_req* handle = request;
		EXE_start(tdbb, handle, attachment->getSysTransaction());
		
		struct {
			TEXT schema_name[MAX_SQL_IDENTIFIER_LEN];
			TEXT generator_name[MAX_SQL_IDENTIFIER_LEN];
		} gen_input;
		
		strcpy(gen_input.schema_name, buffer.schema.c_str());
		strcpy(gen_input.generator_name, buffer.object.c_str());
		
		EXE_send(tdbb, handle, 0, sizeof(gen_input), reinterpret_cast<UCHAR*>(&gen_input));

		struct {
			TEXT RDB$GENERATOR_NAME[MAX_SQL_IDENTIFIER_LEN];
		} gen_data;

		while (!EXE_receive(tdbb, handle, 1, sizeof(gen_data), reinterpret_cast<UCHAR*>(&gen_data)))
		{
			found = true;
		}
	} while (found);
}


void DYN_UTIL_generate_trigger_name(thread_db* tdbb, jrd_tra* /*transaction*/, QualifiedName& buffer)
{
/**************************************
 *
 *	D Y N _ U T I L _ g e n e r a t e _ t r i g g e r _ n a m e
 *
 **************************************
 *
 * Functional description
 *	Generate a name unique to RDB$TRIGGERS.
 *
 **************************************/
	SET_TDBB(tdbb);

	fb_assert(buffer.schema.hasData());

	Jrd::Attachment* attachment = tdbb->getAttachment();
	bool found = false;

	do
	{
		buffer.object.printf("CHECK_%" SQUADFORMAT,
			DYN_UTIL_gen_unique_id(tdbb, drq_g_nxt_trg, "RDB$TRIGGER_NAME"));

		AutoCacheRequest request(tdbb, drq_f_nxt_trg, DYN_REQUESTS);

		found = false;
		
		// Converted FOR loop #17: FOR(REQUEST_HANDLE request) FIRST 1 X IN RDB$TRIGGERS
		jrd_req* handle = request;
		EXE_start(tdbb, handle, attachment->getSysTransaction());
		
		struct {
			TEXT schema_name[MAX_SQL_IDENTIFIER_LEN];
			TEXT trigger_name[MAX_SQL_IDENTIFIER_LEN];
		} trg_input;
		
		strcpy(trg_input.schema_name, buffer.schema.c_str());
		strcpy(trg_input.trigger_name, buffer.object.c_str());
		
		EXE_send(tdbb, handle, 0, sizeof(trg_input), reinterpret_cast<UCHAR*>(&trg_input));

		struct {
			TEXT RDB$TRIGGER_NAME[MAX_SQL_IDENTIFIER_LEN];
		} trg_data;

		while (!EXE_receive(tdbb, handle, 1, sizeof(trg_data), reinterpret_cast<UCHAR*>(&trg_data)))
		{
			found = true;
		}
	} while (found);
}


bool DYN_UTIL_find_field_source(thread_db* tdbb,
								jrd_tra* transaction,
								const QualifiedName& view_name,
								USHORT context,
								const TEXT* local_name,
								TEXT* output_field_schema_name,
								TEXT* output_field_name)
{
/**************************************
 *
 *	D Y N _ U T I L _ f i n d _ f i e l d _ s o u r c e
 *
 **************************************
 *
 * Functional description
 *	Find the original source for a view field.
 *
 **************************************/
	SET_TDBB(tdbb);

	AutoCacheRequest request(tdbb, drq_l_fld_src2, DYN_REQUESTS);
	bool found = false;

	// Converted FOR loop #18: FOR(REQUEST_HANDLE request TRANSACTION_HANDLE transaction) VRL IN RDB$VIEW_RELATIONS CROSS RFR IN RDB$RELATION_FIELDS
	jrd_req* handle = request;
	EXE_start(tdbb, handle, transaction);
	
	struct {
		TEXT schema_name[MAX_SQL_IDENTIFIER_LEN];
		TEXT view_name[MAX_SQL_IDENTIFIER_LEN];
		USHORT view_context;
		TEXT field_name[MAX_SQL_IDENTIFIER_LEN];
	} vrl_input;
	
	strcpy(vrl_input.schema_name, view_name.schema.c_str());
	strcpy(vrl_input.view_name, view_name.object.c_str());
	vrl_input.view_context = context;
	strcpy(vrl_input.field_name, local_name);
	
	EXE_send(tdbb, handle, 0, sizeof(vrl_input), reinterpret_cast<UCHAR*>(&vrl_input));

	struct {
		TEXT RDB$FIELD_SOURCE_SCHEMA_NAME[MAX_SQL_IDENTIFIER_LEN];
		TEXT RDB$FIELD_SOURCE[MAX_SQL_IDENTIFIER_LEN];
		SSHORT package_name_null;
	} vrl_data;

	while (!EXE_receive(tdbb, handle, 1, sizeof(vrl_data), reinterpret_cast<UCHAR*>(&vrl_data)))
	{
		if (vrl_data.package_name_null) // VRL.RDB$PACKAGE_NAME MISSING
		{
			found = true;

			fb_utils::exact_name_limit(vrl_data.RDB$FIELD_SOURCE_SCHEMA_NAME, sizeof(vrl_data.RDB$FIELD_SOURCE_SCHEMA_NAME));
			strcpy(output_field_schema_name, vrl_data.RDB$FIELD_SOURCE_SCHEMA_NAME);

			fb_utils::exact_name_limit(vrl_data.RDB$FIELD_SOURCE, sizeof(vrl_data.RDB$FIELD_SOURCE));
			strcpy(output_field_name, vrl_data.RDB$FIELD_SOURCE);
		}
	}

	if (!found)
	{
		request.reset(tdbb, drq_l_fld_src3, DYN_REQUESTS);

		// Converted FOR loop #19: FOR(REQUEST_HANDLE request TRANSACTION_HANDLE transaction) VRL IN RDB$VIEW_RELATIONS CROSS PPR IN RDB$PROCEDURE_PARAMETERS
		jrd_req* handle2 = request;
		EXE_start(tdbb, handle2, transaction);
		
		struct {
			TEXT schema_name[MAX_SQL_IDENTIFIER_LEN];
			TEXT view_name[MAX_SQL_IDENTIFIER_LEN];
			USHORT view_context;
			TEXT parameter_name[MAX_SQL_IDENTIFIER_LEN];
		} ppr_input;
		
		strcpy(ppr_input.schema_name, view_name.schema.c_str());
		strcpy(ppr_input.view_name, view_name.object.c_str());
		ppr_input.view_context = context;
		strcpy(ppr_input.parameter_name, local_name);
		
		EXE_send(tdbb, handle2, 0, sizeof(ppr_input), reinterpret_cast<UCHAR*>(&ppr_input));

		struct {
			TEXT RDB$FIELD_SOURCE_SCHEMA_NAME[MAX_SQL_IDENTIFIER_LEN];
			TEXT RDB$FIELD_SOURCE[MAX_SQL_IDENTIFIER_LEN];
			SSHORT package_name_null;
		} ppr_data;

		while (!EXE_receive(tdbb, handle2, 1, sizeof(ppr_data), reinterpret_cast<UCHAR*>(&ppr_data)))
		{
			if (ppr_data.package_name_null) // VRL.RDB$PACKAGE_NAME MISSING
			{
				found = true;

				fb_utils::exact_name_limit(ppr_data.RDB$FIELD_SOURCE_SCHEMA_NAME, sizeof(ppr_data.RDB$FIELD_SOURCE_SCHEMA_NAME));
				strcpy(output_field_schema_name, ppr_data.RDB$FIELD_SOURCE_SCHEMA_NAME);

				fb_utils::exact_name_limit(ppr_data.RDB$FIELD_SOURCE, sizeof(ppr_data.RDB$FIELD_SOURCE));
				strcpy(output_field_name, ppr_data.RDB$FIELD_SOURCE);
			}
		}
	}

	return found;
}


void DYN_UTIL_store_check_constraints(thread_db* tdbb, jrd_tra* transaction,
	const QualifiedName& constraint_name, const MetaName& trigger_name)
{
/**************************************
 *
 *	D Y N _ U T I L _ s t o r e _ c h e c k _ c o n s t r a i n t s
 *
 **************************************
 *
 * Functional description
 *	Fill in rdb$check_constraints the association between a check name and the
 *	system defined trigger that implements that check.
 *
 **************************************/
	SET_TDBB(tdbb);

	AutoCacheRequest request(tdbb, drq_s_chk_con, DYN_REQUESTS);

	// Converted STORE operation #1: STORE(REQUEST_HANDLE request TRANSACTION_HANDLE transaction) CHK IN RDB$CHECK_CONSTRAINTS
	jrd_req* store_handle = request;
	EXE_start(tdbb, store_handle, transaction);
	
	struct {
		TEXT RDB$SCHEMA_NAME[MAX_SQL_IDENTIFIER_LEN];
		TEXT RDB$CONSTRAINT_NAME[MAX_SQL_IDENTIFIER_LEN];
		TEXT RDB$TRIGGER_NAME[MAX_SQL_IDENTIFIER_LEN];
	} chk_data;
	
	strcpy(chk_data.RDB$SCHEMA_NAME, constraint_name.schema.c_str());
	strcpy(chk_data.RDB$CONSTRAINT_NAME, constraint_name.object.c_str());
	strcpy(chk_data.RDB$TRIGGER_NAME, trigger_name.c_str());
	
	EXE_send(tdbb, store_handle, 0, sizeof(chk_data), reinterpret_cast<UCHAR*>(&chk_data));
}