/*
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
 */

#include "scratchbird.h"
#include "../common/gdsassert.h"
#include "../jrd/flags.h"
#include "../jrd/jrd.h"
#include "../jrd/val.h"
#include "../jrd/irq.h"
#include "../jrd/tra.h"
#include "../jrd/lck.h"
#include "../jrd/req.h"
#include "../jrd/exe.h"
#include "../jrd/blb.h"
#include "../jrd/met.h"
#include "../jrd/align.h"
#include "../dsql/ExprNodes.h"
#include "../dsql/StmtNodes.h"
#include "../jrd/blb_proto.h"
#include "../jrd/cmp_proto.h"
#include "../common/dsc_proto.h"
#include "../jrd/evl_proto.h"
#include "../jrd/exe_proto.h"
#include "../jrd/flu_proto.h"
#include "../jrd/fun_proto.h"
#include "../jrd/lck_proto.h"
#include "../jrd/met_proto.h"
#include "../jrd/mov_proto.h"
#include "../jrd/par_proto.h"
#include "../jrd/vio_proto.h"
#include "../common/utils_proto.h"
#include "../jrd/DebugInterface.h"
#include "../jrd/trace/TraceJrdHelpers.h"

#include "../jrd/Function.h"

using namespace ScratchBird;
using namespace Jrd;

// Replaced GPRE DATABASE DB = FILENAME "ODS.RDB"; with modern approach
// Database access is handled through existing attachment mechanisms

// Constants for GPRE conversion
#ifndef MAX_SQL_IDENTIFIER_LEN
#define MAX_SQL_IDENTIFIER_LEN 68
#endif

const char* const Function::EXCEPTION_MESSAGE = "The user defined function: \t%s\n\t   referencing"
	" entrypoint: \t%s\n\t                in module: \t%s\n\tcaused the fatal exception:";

Function* Function::lookup(thread_db* tdbb, USHORT id, bool return_deleted, bool noscan, USHORT flags)
{
	Jrd::Attachment* attachment = tdbb->getAttachment();
	Function* check_function = NULL;

	Function* function = (id < attachment->att_functions.getCount()) ? attachment->att_functions[id] : NULL;

	if (function && function->getId() == id &&
		!(function->flags & Routine::FLAG_CLEARED) &&
		!(function->flags & Routine::FLAG_BEING_SCANNED) &&
		((function->flags & Routine::FLAG_SCANNED) || noscan) &&
		!(function->flags & Routine::FLAG_BEING_ALTERED) &&
		(!(function->flags & Routine::FLAG_OBSOLETE) || return_deleted))
	{
		if (!(function->flags & Routine::FLAG_CHECK_EXISTENCE))
		{
			return function;
		}

		check_function = function;
		LCK_lock(tdbb, check_function->existenceLock, LCK_SR, LCK_WAIT);
	}

	// We need to look up the function in RDB$FUNCTIONS

	function = NULL;

	AutoCacheRequest request(tdbb, irq_l_fun_id, IRQ_REQUESTS);

	// Converted FOR loop #1: FOR(REQUEST_HANDLE request) X IN RDB$FUNCTIONS WITH X.RDB$FUNCTION_ID EQ id
	EXE_start(tdbb, request, attachment->getSysTransaction());
	EXE_send(tdbb, request, 0, sizeof(USHORT), reinterpret_cast<UCHAR*>(&id));

	struct {
		USHORT RDB$FUNCTION_ID;
	} struct_buffer_1;

	while (!EXE_receive(tdbb, request, 1, sizeof(struct_buffer_1), reinterpret_cast<UCHAR*>(&struct_buffer_1)))
	{
		function = loadMetadata(tdbb, struct_buffer_1.RDB$FUNCTION_ID, noscan, flags);
	}

	if (check_function)
	{
		check_function->flags &= ~Routine::FLAG_CHECK_EXISTENCE;
		if (check_function != function)
		{
			LCK_release(tdbb, check_function->existenceLock);
			check_function->flags |= Routine::FLAG_OBSOLETE;
		}
	}

	return function;
}

Function* Function::lookup(thread_db* tdbb, const QualifiedName& name, bool noscan)
{
	Jrd::Attachment* attachment = tdbb->getAttachment();

	Function* check_function = NULL;

	// See if we already know the function by name

	for (Function** iter = attachment->att_functions.begin(); iter < attachment->att_functions.end(); ++iter)
	{
		Function* const function = *iter;

		if (function && !(function->flags & Routine::FLAG_OBSOLETE) &&
			!(function->flags & Routine::FLAG_CLEARED) &&
			((function->flags & Routine::FLAG_SCANNED) || noscan) &&
			!(function->flags & Routine::FLAG_BEING_SCANNED) &&
			!(function->flags & Routine::FLAG_BEING_ALTERED))
		{
			if (function->getName() == name)
			{
				if (function->flags & Routine::FLAG_CHECK_EXISTENCE)
				{
					check_function = function;
					LCK_lock(tdbb, check_function->existenceLock, LCK_SR, LCK_WAIT);
					break;
				}

				return function;
			}
		}
	}

	// We need to look up the function in RDB$FUNCTIONS

	Function* function = NULL;

	AutoCacheRequest request(tdbb, irq_l_fun_name, IRQ_REQUESTS);

	// Converted FOR loop #2: FOR(REQUEST_HANDLE request) X IN RDB$FUNCTIONS WITH complex WHERE clause
	EXE_start(tdbb, request, attachment->getSysTransaction());
	
	struct {
		TEXT RDB$SCHEMA_NAME[MAX_SQL_IDENTIFIER_LEN];
		TEXT RDB$FUNCTION_NAME[MAX_SQL_IDENTIFIER_LEN]; 
		TEXT RDB$PACKAGE_NAME[MAX_SQL_IDENTIFIER_LEN];
		SSHORT schema_null;
		SSHORT function_null;
		SSHORT package_null;
	} input_params;
	
	strcpy(input_params.RDB$SCHEMA_NAME, name.schema.c_str());
	strcpy(input_params.RDB$FUNCTION_NAME, name.object.c_str());
	strcpy(input_params.RDB$PACKAGE_NAME, name.package.c_str());
	input_params.schema_null = 0;
	input_params.function_null = 0;
	input_params.package_null = name.package.hasData() ? 0 : -1;
	
	EXE_send(tdbb, request, 0, sizeof(input_params), reinterpret_cast<UCHAR*>(&input_params));

	struct {
		USHORT RDB$FUNCTION_ID;
	} output_buffer;

	while (!EXE_receive(tdbb, request, 1, sizeof(output_buffer), reinterpret_cast<UCHAR*>(&output_buffer)))
	{
		function = loadMetadata(tdbb, output_buffer.RDB$FUNCTION_ID, noscan, 0);
	}

	if (check_function)
	{
		check_function->flags &= ~Routine::FLAG_CHECK_EXISTENCE;
		if (check_function != function)
		{
			LCK_release(tdbb, check_function->existenceLock);
			check_function->flags |= Routine::FLAG_OBSOLETE;
		}
	}

	return function;
}

Function* Function::loadMetadata(thread_db* tdbb, USHORT id, bool noscan, USHORT flags)
{
	Jrd::Attachment* attachment = tdbb->getAttachment();
	jrd_tra* sysTransaction = attachment->getSysTransaction();
	Database* const dbb = tdbb->getDatabase();

	if (id >= attachment->att_functions.getCount())
		attachment->att_functions.grow(id + 1);

	Function* function = attachment->att_functions[id];

	if (function && !(function->flags & Routine::FLAG_OBSOLETE))
	{
		// Make sure Routine::FLAG_BEING_SCANNED and Routine::FLAG_SCANNED are not set at the same time
		fb_assert(!(function->flags & Routine::FLAG_BEING_SCANNED) ||
			!(function->flags & Routine::FLAG_SCANNED));

		if ((function->flags & Routine::FLAG_BEING_SCANNED) ||
			(function->flags & Routine::FLAG_SCANNED))
		{
			return function;
		}
	}

	if (!function)
		function = FB_NEW_POOL(*attachment->att_pool) Function(*attachment->att_pool);

	try
	{
	function->flags |= (Routine::FLAG_BEING_SCANNED | flags);
	function->flags &= ~(Routine::FLAG_OBSOLETE | Routine::FLAG_CLEARED);

	function->setId(id);
	attachment->att_functions[id] = function;

	if (!function->existenceLock)
	{
		Lock* const lock = FB_NEW_RPT(*attachment->att_pool, 0)
			Lock(tdbb, sizeof(SLONG), LCK_fun_exist, function, blockingAst);
		function->existenceLock = lock;
		lock->setKey(function->getId());
	}

	LCK_lock(tdbb, function->existenceLock, LCK_SR, LCK_WAIT);

	if (!noscan)
	{
		AutoCacheRequest request_fun(tdbb, irq_l_functions, IRQ_REQUESTS);

		// Converted FOR loop #3: FOR(REQUEST_HANDLE request_fun) X IN RDB$FUNCTIONS CROSS SCH IN RDB$SCHEMAS
		
		EXE_start(tdbb, request_fun, attachment->getSysTransaction());
		EXE_send(tdbb, handle_fun, 0, sizeof(USHORT), reinterpret_cast<UCHAR*>(&id));

		struct {
			USHORT RDB$FUNCTION_ID;
			TEXT RDB$FUNCTION_NAME[MAX_SQL_IDENTIFIER_LEN];
			TEXT RDB$SCHEMA_NAME[MAX_SQL_IDENTIFIER_LEN];
			TEXT RDB$PACKAGE_NAME[MAX_SQL_IDENTIFIER_LEN];
			TEXT RDB$OWNER_NAME[MAX_SQL_IDENTIFIER_LEN];
			TEXT RDB$SECURITY_CLASS[MAX_SQL_IDENTIFIER_LEN];
			TEXT RDB$ENGINE_NAME[MAX_SQL_IDENTIFIER_LEN];
			TEXT RDB$ENTRYPOINT[MAX_SQL_IDENTIFIER_LEN];
			TEXT RDB$MODULE_NAME[MAX_SQL_IDENTIFIER_LEN];
			ISC_QUAD RDB$FUNCTION_SOURCE;
			ISC_QUAD RDB$FUNCTION_BLR;
			ISC_QUAD RDB$DEBUG_INFO;
			SSHORT RDB$RETURN_ARGUMENT;
			SSHORT RDB$SQL_SECURITY;
			SSHORT RDB$DETERMINISTIC_FLAG;
			SSHORT RDB$VALID_BLR;
			SSHORT package_null;
			SSHORT security_class_null;
			SSHORT engine_name_null;
			SSHORT entrypoint_null;
			SSHORT module_name_null;
			SSHORT function_source_null;
			SSHORT function_blr_null;
			SSHORT debug_info_null;
			SSHORT sql_security_null;
			SSHORT deterministic_flag_null;
			SSHORT valid_blr_null;
		} function_data;

		while (!EXE_receive(tdbb, handle_fun, 1, sizeof(function_data), reinterpret_cast<UCHAR*>(&function_data)))
		{
			function->setName(QualifiedName(function_data.RDB$FUNCTION_NAME, function_data.RDB$SCHEMA_NAME,
				(function_data.package_null ? NULL : function_data.RDB$PACKAGE_NAME)));

			function->owner = function_data.RDB$OWNER_NAME;
			ScratchBird::TriState ssDefiner;

			if (!function_data.security_class_null)
				function->setSecurityName(QualifiedName(function_data.RDB$SECURITY_CLASS, function_data.RDB$SCHEMA_NAME));
			else if (!function_data.package_null)
			{
				AutoCacheRequest requestHandle(tdbb, irq_l_procedure_pkg_class, IRQ_REQUESTS);

				// Converted FOR loop #4: FOR (REQUEST_HANDLE requestHandle) PKG IN RDB$PACKAGES
				jrd_req* handle_pkg = requestHandle;
				EXE_start(tdbb, handle_pkg, attachment->getSysTransaction());
				
				struct {
					TEXT RDB$SCHEMA_NAME[MAX_SQL_IDENTIFIER_LEN];
					TEXT RDB$PACKAGE_NAME[MAX_SQL_IDENTIFIER_LEN];
				} pkg_input;
				
				strcpy(pkg_input.RDB$SCHEMA_NAME, function_data.RDB$SCHEMA_NAME);
				strcpy(pkg_input.RDB$PACKAGE_NAME, function_data.RDB$PACKAGE_NAME);
				
				EXE_send(tdbb, handle_pkg, 0, sizeof(pkg_input), reinterpret_cast<UCHAR*>(&pkg_input));

				struct {
					TEXT RDB$SECURITY_CLASS[MAX_SQL_IDENTIFIER_LEN];
					SSHORT RDB$SQL_SECURITY;
					SSHORT security_class_null;
					SSHORT sql_security_null;
				} pkg_data;

				while (!EXE_receive(tdbb, handle_pkg, 1, sizeof(pkg_data), reinterpret_cast<UCHAR*>(&pkg_data)))
				{
					if (!pkg_data.security_class_null)
						function->setSecurityName(QualifiedName(pkg_data.RDB$SECURITY_CLASS, function_data.RDB$SCHEMA_NAME));

					// SQL SECURITY of function must be the same if it's defined in package
					if (!pkg_data.sql_security_null)
						ssDefiner = (bool) pkg_data.RDB$SQL_SECURITY;
				}
			}

			if (!ssDefiner.isAssigned())
			{
				if (!function_data.sql_security_null)
					ssDefiner = (bool) function_data.RDB$SQL_SECURITY;
				else
					ssDefiner = MET_get_ss_definer(tdbb, function_data.RDB$SCHEMA_NAME);
			}

			if (ssDefiner.asBool())
				function->invoker = attachment->getUserId(function->owner);

			size_t count = 0;
			ULONG length = 0;

			function->fun_inputs = 0;
			function->setDefaultCount(0);

			function->getInputFields().clear();
			function->getOutputFields().clear();

			AutoCacheRequest request_arg(tdbb, irq_l_args, IRQ_REQUESTS);

			// Converted FOR loop #5: FOR(REQUEST_HANDLE request_arg) Y IN RDB$FUNCTION_ARGUMENTS
			jrd_req* handle_arg = request_arg;
			EXE_start(tdbb, handle_arg, attachment->getSysTransaction());
			
			struct {
				TEXT RDB$SCHEMA_NAME[MAX_SQL_IDENTIFIER_LEN];
				TEXT RDB$FUNCTION_NAME[MAX_SQL_IDENTIFIER_LEN];
				TEXT RDB$PACKAGE_NAME[MAX_SQL_IDENTIFIER_LEN];
				SSHORT package_null;
			} arg_input;
			
			strcpy(arg_input.RDB$SCHEMA_NAME, function->getName().schema.c_str());
			strcpy(arg_input.RDB$FUNCTION_NAME, function->getName().object.c_str());
			strcpy(arg_input.RDB$PACKAGE_NAME, function->getName().package.c_str());
			arg_input.package_null = function->getName().package.hasData() ? 0 : -1;
			
			EXE_send(tdbb, handle_arg, 0, sizeof(arg_input), reinterpret_cast<UCHAR*>(&arg_input));

			struct {
				SSHORT RDB$ARGUMENT_POSITION;
				SSHORT RDB$MECHANISM;
				TEXT RDB$ARGUMENT_NAME[MAX_SQL_IDENTIFIER_LEN];
				SSHORT RDB$NULL_FLAG;
				SSHORT RDB$ARGUMENT_MECHANISM;
				SSHORT RDB$COLLATION_ID;
				ISC_QUAD RDB$DEFAULT_VALUE;
				TEXT RDB$FIELD_SOURCE[MAX_SQL_IDENTIFIER_LEN];
				TEXT RDB$FIELD_SOURCE_SCHEMA_NAME[MAX_SQL_IDENTIFIER_LEN];
				SSHORT RDB$FIELD_TYPE;
				SSHORT RDB$FIELD_SCALE;
				SSHORT RDB$FIELD_LENGTH;
				SSHORT RDB$FIELD_SUB_TYPE;
				SSHORT RDB$CHARACTER_SET_ID;
				TEXT RDB$RELATION_NAME[MAX_SQL_IDENTIFIER_LEN];
				TEXT RDB$RELATION_SCHEMA_NAME[MAX_SQL_IDENTIFIER_LEN];
				TEXT RDB$FIELD_NAME[MAX_SQL_IDENTIFIER_LEN];
				SSHORT argument_name_null;
				SSHORT null_flag_null;
				SSHORT argument_mechanism_null;
				SSHORT collation_id_null;
				SSHORT default_value_null;
				SSHORT field_source_null;
				SSHORT relation_name_null;
				SSHORT field_name_null;
			} arg_data;

			while (!EXE_receive(tdbb, handle_arg, 1, sizeof(arg_data), reinterpret_cast<UCHAR*>(&arg_data)))
			{
				Parameter* parameter = FB_NEW_POOL(function->getPool()) Parameter(function->getPool());

				if (arg_data.RDB$ARGUMENT_POSITION != function_data.RDB$RETURN_ARGUMENT)
				{
					function->fun_inputs++;
					int newCount = arg_data.RDB$ARGUMENT_POSITION - function->getOutputFields().getCount();
					fb_assert(newCount >= 0);

					function->getInputFields().resize(newCount + 1);
					function->getInputFields()[newCount] = parameter;
				}
				else
				{
					fb_assert(function->getOutputFields().isEmpty());
					function->getOutputFields().add(parameter);
				}

				parameter->prm_fun_mechanism = (FUN_T) arg_data.RDB$MECHANISM;
				parameter->prm_number = arg_data.RDB$ARGUMENT_POSITION;
				parameter->prm_name = arg_data.argument_name_null ? "" : arg_data.RDB$ARGUMENT_NAME;
				parameter->prm_nullable = arg_data.null_flag_null || arg_data.RDB$NULL_FLAG == 0;
				parameter->prm_mechanism = arg_data.argument_mechanism_null ?
					prm_mech_normal : (prm_mech_t) arg_data.RDB$ARGUMENT_MECHANISM;

				const SSHORT collation_id_null = arg_data.collation_id_null;
				const SSHORT collation_id = arg_data.RDB$COLLATION_ID;

				SSHORT default_value_null = arg_data.default_value_null;
				bid default_value;
				default_value.bid_quad.bid_quad_high = arg_data.RDB$DEFAULT_VALUE.gds_quad_high;
				default_value.bid_quad.bid_quad_low = arg_data.RDB$DEFAULT_VALUE.gds_quad_low;

				if (!arg_data.field_source_null)
				{
					parameter->prm_field_source = QualifiedName(arg_data.RDB$FIELD_SOURCE, arg_data.RDB$FIELD_SOURCE_SCHEMA_NAME);

					AutoCacheRequest request_arg_fld(tdbb, irq_l_arg_fld, IRQ_REQUESTS);

					// Converted FOR loop #6: FOR(REQUEST_HANDLE request_arg_fld) F IN RDB$FIELDS
					jrd_req* handle_fld = request_arg_fld;
					EXE_start(tdbb, handle_fld, attachment->getSysTransaction());
					
					struct {
						TEXT RDB$SCHEMA_NAME[MAX_SQL_IDENTIFIER_LEN];  
						TEXT RDB$FIELD_NAME[MAX_SQL_IDENTIFIER_LEN];
					} fld_input;
					
					strcpy(fld_input.RDB$SCHEMA_NAME, arg_data.RDB$FIELD_SOURCE_SCHEMA_NAME);
					strcpy(fld_input.RDB$FIELD_NAME, arg_data.RDB$FIELD_SOURCE);
					
					EXE_send(tdbb, handle_fld, 0, sizeof(fld_input), reinterpret_cast<UCHAR*>(&fld_input));

					struct {
						SSHORT RDB$FIELD_TYPE;
						SSHORT RDB$FIELD_SCALE;
						SSHORT RDB$FIELD_LENGTH;
						SSHORT RDB$FIELD_SUB_TYPE;
						SSHORT RDB$CHARACTER_SET_ID;
						SSHORT RDB$COLLATION_ID;
						ISC_QUAD RDB$DEFAULT_VALUE;
						TEXT RDB$FIELD_NAME[MAX_SQL_IDENTIFIER_LEN];
						SSHORT default_value_null;
					} fld_data;

					while (!EXE_receive(tdbb, handle_fld, 1, sizeof(fld_data), reinterpret_cast<UCHAR*>(&fld_data)))
					{
						DSC_make_descriptor(&parameter->prm_desc, fld_data.RDB$FIELD_TYPE,
											fld_data.RDB$FIELD_SCALE, fld_data.RDB$FIELD_LENGTH,
											fld_data.RDB$FIELD_SUB_TYPE, fld_data.RDB$CHARACTER_SET_ID,
											(collation_id_null ? fld_data.RDB$COLLATION_ID : collation_id));

						if (default_value_null && fb_utils::implicit_domain(fld_data.RDB$FIELD_NAME))
						{
							default_value_null = fld_data.default_value_null;
							default_value.bid_quad.bid_quad_high = fld_data.RDB$DEFAULT_VALUE.gds_quad_high;
							default_value.bid_quad.bid_quad_low = fld_data.RDB$DEFAULT_VALUE.gds_quad_low;
						}
					}
				}
				else
				{
					DSC_make_descriptor(&parameter->prm_desc, arg_data.RDB$FIELD_TYPE,
										arg_data.RDB$FIELD_SCALE, arg_data.RDB$FIELD_LENGTH,
										arg_data.RDB$FIELD_SUB_TYPE, arg_data.RDB$CHARACTER_SET_ID,
										(collation_id_null ? 0 : collation_id));
				}

				if (parameter->prm_desc.isText() && parameter->prm_desc.getTextType() != CS_NONE)
				{
					if (!collation_id_null ||
						(!arg_data.field_source_null && fb_utils::implicit_domain(arg_data.RDB$FIELD_SOURCE)))
					{
						parameter->prm_text_type = parameter->prm_desc.getTextType();
					}
				}

				if (!arg_data.relation_name_null)
					parameter->prm_type_of_table = QualifiedName(arg_data.RDB$RELATION_NAME, arg_data.RDB$RELATION_SCHEMA_NAME);

				if (!arg_data.field_name_null)
					parameter->prm_type_of_column = arg_data.RDB$FIELD_NAME;

				if (arg_data.RDB$ARGUMENT_POSITION != function_data.RDB$RETURN_ARGUMENT && !default_value_null)
				{
					function->setDefaultCount(function->getDefaultCount() + 1);

					MemoryPool* const csb_pool = attachment->createPool();
					Jrd::ContextPoolHolder context(tdbb, csb_pool);

					try
					{
						parameter->prm_default_value = static_cast<ValueExprNode*>(MET_parse_blob(
							tdbb, &function->getName().schema, NULL, &default_value, NULL, NULL, false, false));
					}
					catch (const ScratchBird::Exception&)
					{
						attachment->deletePool(csb_pool);
						throw; // an explicit error message would be better
					}
				}

				if (parameter->prm_desc.dsc_dtype == dtype_cstring)
					parameter->prm_desc.dsc_length++;

				length += (parameter->prm_desc.dsc_dtype == dtype_blob) ?
					sizeof(udf_blob) : FB_ALIGN(parameter->prm_desc.dsc_length, FB_DOUBLE_ALIGN);

				count = MAX(count, size_t(arg_data.RDB$ARGUMENT_POSITION));
			}

			for (int i = (int) function->getInputFields().getCount() - 1; i >= 0; --i)
			{
				if (!function->getInputFields()[i])
					function->getInputFields().remove(i);
			}

			function->fun_return_arg = function_data.RDB$RETURN_ARGUMENT;
			function->fun_temp_length = length;

			// Prepare the exception message to be used in case this function ever
			// causes an exception.  This is done at this time to save us from preparing
			// (thus allocating) this message every time the function is called.
			function->fun_exception_message.printf(EXCEPTION_MESSAGE,
				function->getName().toQuotedString().c_str(), function_data.RDB$ENTRYPOINT, function_data.RDB$MODULE_NAME);

			if (!function_data.deterministic_flag_null)
				function->fun_deterministic = (function_data.RDB$DETERMINISTIC_FLAG != 0);

			function->setImplemented(true);
			function->setDefined(true);

			function->fun_entrypoint = NULL;
			function->fun_external = NULL;
			function->setStatement(NULL);

			if (!function_data.module_name_null && !function_data.entrypoint_null)
			{
				function->fun_entrypoint =
					Module::lookup(function_data.RDB$MODULE_NAME, function_data.RDB$ENTRYPOINT, dbb);

				// Could not find a function with given MODULE, ENTRYPOINT.
				// Try the list of internally implemented functions.
				if (!function->fun_entrypoint)
				{
					function->fun_entrypoint =
						BUILTIN_entrypoint(function_data.RDB$MODULE_NAME, function_data.RDB$ENTRYPOINT);
				}

				if (!function->fun_entrypoint)
					function->setDefined(false);
			}
			else if (!function_data.engine_name_null || !function_data.function_blr_null)
			{
				MemoryPool* const csb_pool = attachment->createPool();
				Jrd::ContextPoolHolder context(tdbb, csb_pool);

				try
				{
					ScratchBird::AutoPtr<CompilerScratch> csb(FB_NEW_POOL(*csb_pool) CompilerScratch(*csb_pool));

					if (!function_data.engine_name_null)
					{
						ScratchBird::HalfStaticArray<UCHAR, 512> body;

						if (!function_data.function_source_null)
						{
							bid function_source_bid;
							function_source_bid.bid_quad.bid_quad_high = function_data.RDB$FUNCTION_SOURCE.gds_quad_high;
							function_source_bid.bid_quad.bid_quad_low = function_data.RDB$FUNCTION_SOURCE.gds_quad_low;
							blb* const blob = blb::open(tdbb, sysTransaction, &function_source_bid);
							const ULONG len = blob->BLB_get_data(tdbb,
								body.getBuffer(blob->blb_length + 1), blob->blb_length + 1);
							body[MIN(blob->blb_length, len)] = 0;
						}
						else
							body.getBuffer(1)[0] = 0;

						dbb->dbb_extManager->makeFunction(tdbb, csb, function, function_data.RDB$ENGINE_NAME,
							(function_data.entrypoint_null ? "" : function_data.RDB$ENTRYPOINT), (char*) body.begin());

						if (!function->fun_external)
							function->setDefined(false);
					}
					else if (!function_data.function_blr_null)
					{
						const ScratchBird::string name = function->getName().toQuotedString();

						try
						{
							TraceFuncCompile trace(tdbb, name.c_str());

							bid function_blr_bid;
							function_blr_bid.bid_quad.bid_quad_high = function_data.RDB$FUNCTION_BLR.gds_quad_high;
							function_blr_bid.bid_quad.bid_quad_low = function_data.RDB$FUNCTION_BLR.gds_quad_low;
							bid debug_info_bid;
							if (!function_data.debug_info_null) {
								debug_info_bid.bid_quad.bid_quad_high = function_data.RDB$DEBUG_INFO.gds_quad_high;
								debug_info_bid.bid_quad.bid_quad_low = function_data.RDB$DEBUG_INFO.gds_quad_low;
							}
							function->parseBlr(tdbb, csb, &function_blr_bid,
								function_data.debug_info_null ? NULL : &debug_info_bid);

							trace.finish(function->getStatement(), ITracePlugin::RESULT_SUCCESS);
						}
						catch (const ScratchBird::Exception& ex)
						{
							ScratchBird::StaticStatusVector temp_status;
							ex.stuffException(temp_status);
							(ScratchBird::Arg::Gds(isc_bad_fun_BLR) << ScratchBird::Arg::Str(name)
								<< ScratchBird::Arg::StatusVector(temp_status.begin())).raise();
						}
					}
				}
				catch (const ScratchBird::Exception&)
				{
					attachment->deletePool(csb_pool);
					throw;
				}

				fb_assert(!function->isDefined() || function->getStatement()->function == function);
			}
			else
			{
				ScratchBird::RefPtr<ScratchBird::MsgMetadata> inputMetadata(ScratchBird::REF_NO_INCR, createMetadata(function->getInputFields(), false));
				function->setInputFormat(createFormat(function->getPool(), inputMetadata, false));

				ScratchBird::RefPtr<ScratchBird::MsgMetadata> outputMetadata(ScratchBird::REF_NO_INCR, createMetadata(function->getOutputFields(), false));
				function->setOutputFormat(createFormat(function->getPool(), outputMetadata, true));

				function->setImplemented(false);
			}

			function->flags |= Routine::FLAG_SCANNED;

			if (!dbb->readOnly() &&
				!function_data.function_blr_null &&
				!function_data.valid_blr_null && function_data.RDB$VALID_BLR == FALSE)
			{
				// If the BLR was marked as invalid but the function was compiled,
				// mark the BLR as valid.

				// Converted MODIFY operation: MODIFY X USING X.RDB$VALID_BLR = TRUE; X.RDB$VALID_BLR.NULL = FALSE; END_MODIFY
				AutoCacheRequest modify_request(tdbb, irq_modify_function_blr, IRQ_REQUESTS);
				jrd_req* modify_handle = modify_request;
				EXE_start(tdbb, modify_handle, attachment->getSysTransaction());
				
				struct {
					USHORT RDB$FUNCTION_ID;
					SSHORT RDB$VALID_BLR;
					SSHORT valid_blr_null;
				} modify_data;
				
				modify_data.RDB$FUNCTION_ID = id;
				modify_data.RDB$VALID_BLR = TRUE;
				modify_data.valid_blr_null = FALSE;
				
				EXE_send(tdbb, modify_handle, 0, sizeof(modify_data), reinterpret_cast<UCHAR*>(&modify_data));
			}
		}
	}

	// Make sure that it is really being scanned
	fb_assert(function->flags & Routine::FLAG_BEING_SCANNED);

	function->flags &= ~Routine::FLAG_BEING_SCANNED;

	}	// try
	catch (const ScratchBird::Exception&)
	{
		function->flags &= ~(Routine::FLAG_BEING_SCANNED | Routine::FLAG_SCANNED);

		if (function->existenceLock)
		{
			LCK_release(tdbb, function->existenceLock);
			delete function->existenceLock;
			function->existenceLock = NULL;
		}

		throw;
	}

	return function;
}

int Function::blockingAst(void* ast_object)
{
	Function* const function = static_cast<Function*>(ast_object);

	try
	{
		Database* const dbb = function->existenceLock->lck_dbb;

		AsyncContextHolder tdbb(dbb, FB_FUNCTION, function->existenceLock);

		LCK_release(tdbb, function->existenceLock);
		function->flags |= Routine::FLAG_OBSOLETE;
	}
	catch (const ScratchBird::Exception&)
	{} // no-op

	return 0;
}

void Function::releaseLocks(thread_db* tdbb)
{
	if (existenceLock)
	{
		LCK_release(tdbb, existenceLock);
		flags |= Routine::FLAG_CHECK_EXISTENCE;
		useCount = 0;
	}
}

bool Function::checkCache(thread_db* tdbb) const
{
	return tdbb->getAttachment()->att_functions[getId()] == this;
}

void Function::clearCache(thread_db* tdbb)
{
	tdbb->getAttachment()->att_functions[getId()] = NULL;
}

bool Function::reload(thread_db* tdbb)
{
	fb_assert(this->flags & Routine::FLAG_RELOAD);

	Attachment* attachment = tdbb->getAttachment();
	AutoCacheRequest request(tdbb, irq_l_funct_blr, IRQ_REQUESTS);

	// Converted FOR loop #7: FOR(REQUEST_HANDLE request) X IN RDB$FUNCTIONS WITH X.RDB$FUNCTION_ID EQ this->getId()
	EXE_start(tdbb, request, attachment->getSysTransaction());
	USHORT function_id = this->getId();
	EXE_send(tdbb, request, 0, sizeof(USHORT), reinterpret_cast<UCHAR*>(&function_id));

	struct {
		ISC_QUAD RDB$FUNCTION_BLR;
		ISC_QUAD RDB$DEBUG_INFO;
		SSHORT function_blr_null;
		SSHORT debug_info_null;
	} reload_data;

	while (!EXE_receive(tdbb, request, 1, sizeof(reload_data), reinterpret_cast<UCHAR*>(&reload_data)))
	{
		if (reload_data.function_blr_null)
			continue;

		MemoryPool* const csb_pool = attachment->createPool();
		Jrd::ContextPoolHolder context(tdbb, csb_pool);

		try
		{
			ScratchBird::AutoPtr<CompilerScratch> csb(FB_NEW_POOL(*csb_pool) CompilerScratch(*csb_pool));

			try
			{
				bid function_blr_bid;
				function_blr_bid.bid_quad.bid_quad_high = reload_data.RDB$FUNCTION_BLR.gds_quad_high;
				function_blr_bid.bid_quad.bid_quad_low = reload_data.RDB$FUNCTION_BLR.gds_quad_low;
				bid debug_info_bid;
				if (!reload_data.debug_info_null) {
					debug_info_bid.bid_quad.bid_quad_high = reload_data.RDB$DEBUG_INFO.gds_quad_high;
					debug_info_bid.bid_quad.bid_quad_low = reload_data.RDB$DEBUG_INFO.gds_quad_low;
				}
				this->parseBlr(tdbb, csb, &function_blr_bid,
					reload_data.debug_info_null ? NULL : &debug_info_bid);

				// parseBlr() above could set FLAG_RELOAD again
				return !(this->flags & Routine::FLAG_RELOAD);
			}
			catch (const ScratchBird::Exception& ex)
			{
				ScratchBird::StaticStatusVector temp_status;
				ex.stuffException(temp_status);

				const ScratchBird::string name = this->getName().toQuotedString();
				(ScratchBird::Arg::Gds(isc_bad_fun_BLR) << ScratchBird::Arg::Str(name)
					<< ScratchBird::Arg::StatusVector(temp_status.begin())).raise();
			}
		}
		catch (const ScratchBird::Exception&)
		{
			attachment->deletePool(csb_pool);
			throw;
		}
	}

	return false;
}