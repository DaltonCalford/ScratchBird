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
 * Adriano dos Santos Fernandes - refactored from pass1.cpp, ddl.cpp, dyn*.epp
 * ScratchBird Project - Split from monolithic DdlNodes.cpp for maintainability
 */

#include "scratchbird.h"
#include "dyn_consts.h"
#include "../dsql/DdlNodes.h"  
#include "../dsql/BoolNodes.h"
#include "../dsql/ExprNodes.h"
#include "../dsql/StmtNodes.h"
#include "scratchbird/impl/blr.h"
#include "../jrd/btr.h"
#include "../jrd/constants.h"
#include "../jrd/dyn.h"
#include "../jrd/flags.h"
#include "../jrd/intl.h"
#include "../jrd/jrd.h"
#include "../common/msg_encode.h"
#include "../jrd/obj.h"
#include "../jrd/ods.h"
#include "../jrd/tra.h"
#include "../jrd/constants.h"
#include "../common/os/path_utils.h"
#include "../jrd/CryptoManager.h"
#include "../jrd/IntlManager.h"
#include "../jrd/PreparedStatement.h"
#include "../jrd/ResultSet.h"
#include "../jrd/UserManagement.h"
#include "../jrd/blb_proto.h"
#include "../jrd/cmp_proto.h"
#include "../jrd/dfw_proto.h"
#include "../jrd/dpm_proto.h"
#include "../jrd/dyn_ut_proto.h"
#include "../jrd/exe_proto.h"
#include "../jrd/intl_proto.h"
#include "../common/isc_f_proto.h"
#include "../jrd/lck_proto.h"
#include "../jrd/met_proto.h"
#include "../jrd/scl_proto.h"
#include "../jrd/vio_proto.h"
#include "../dsql/ddl_proto.h"
#include "../dsql/errd_proto.h"
#include "../dsql/gen_proto.h"
#include "../dsql/make_proto.h"
#include "../dsql/metd_proto.h"
#include "../dsql/pass1_proto.h"
#include "../utilities/gsec/gsec.h"
#include "../common/dsc_proto.h"
#include "../common/StatusArg.h"
#include "../auth/SecureRemotePassword/Message.h"
#include "../jrd/Mapping.h"
#include "../jrd/extds/ExtDS.h"

namespace Jrd {

using namespace ScratchBird;

//----------------------
// External function declarations from DdlNodesBase.cpp
//----------------------

extern void checkForeignKeyTempScope(thread_db* tdbb, jrd_tra* transaction,
	const QualifiedName&	childRelName, const QualifiedName& masterIndexName);
extern void checkSpTrigDependency(thread_db* tdbb, jrd_tra* transaction,
	const QualifiedName& relationName, const MetaName& fieldName);
extern void checkViewDependency(thread_db* tdbb, jrd_tra* transaction,
	const QualifiedName& relationName, const MetaName& fieldName);
extern void clearPermanentField(dsql_rel* relation, bool permanent);
extern void defineComputed(DsqlCompilerScratch* dsqlScratch, RelationSourceNode* relation,
	dsql_fld* field, ValueSourceClause* clause, string& source, BlrDebugWriter::BlrData& value);
extern void deleteKeyConstraint(thread_db* tdbb, jrd_tra* transaction,
	const QualifiedName& relationName, const MetaName& constraintName, const MetaName& indexName);
extern bool fieldExists(thread_db* tdbb, jrd_tra* transaction, const QualifiedName& relationName,
	const MetaName& fieldName);
extern bool isItSqlRole(thread_db* tdbb, jrd_tra* transaction, const MetaName& inputName,
	MetaName& outputName);
extern int getGrantorOption(thread_db* tdbb, jrd_tra* transaction, const MetaName& grantor,
	int grantorType, const MetaName& roleName);
extern QualifiedName getIndexRelationName(thread_db* tdbb, jrd_tra* transaction,
	const QualifiedName& indexName, bool& systemIndex, bool silent = false);
extern const char* getRelationScopeName(const rel_t type);
extern void makeRelationScopeName(string& to, const QualifiedName& name, const rel_t type);
extern void checkRelationType(const rel_t type, const QualifiedName& name);
extern void checkFkPairTypes(const rel_t masterType, const QualifiedName& masterName,
	const rel_t childType, const QualifiedName& childName);
extern void modifyLocalFieldPosition(thread_db* tdbb, jrd_tra* transaction,
	const QualifiedName& relationName, const MetaName& fieldName, USHORT newPosition);
extern rel_t relationType(SSHORT relationTypeNull, SSHORT relationType);
extern void saveField(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, const MetaName& fieldName);
extern void saveRelation(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch,
	const QualifiedName& relationName, bool view, bool creating);
extern void updateRdbFields(const TypeClause* type,
	SSHORT& fieldType,
	SSHORT& fieldLength,
	SSHORT& fieldSubTypeNull, SSHORT& fieldSubType,
	SSHORT& fieldScaleNull, SSHORT& fieldScale,
	SSHORT& characterSetIdNull, SSHORT& characterSetId,
	SSHORT& characterLengthNull, SSHORT& characterLength,
	SSHORT& fieldPrecisionNull, SSHORT& fieldPrecision,
	SSHORT& collationIdNull, SSHORT& collationId,
	SSHORT& segmentLengthNull, SSHORT& segmentLength);

//----------------------
// CreateAlterFunctionNode Class Implementation  
//----------------------

void CreateAlterFunctionNode::execute(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch,
	jrd_tra* transaction)
{
	Attachment* const attachment = transaction->tra_attachment;

	const int triggerType = create ? DDL_TRIGGER_CREATE_FUNCTION : DDL_TRIGGER_ALTER_FUNCTION;
	
	if (!runTriggers || name.package.isEmpty())
		executeDdlTrigger(tdbb, dsqlScratch, transaction, DTW_BEFORE, triggerType, name);

	if (create)
	{
		// Convert FOR loop #40: Check if function already exists
		AutoCacheRequest request(tdbb, drq_l_func_info, DYN_REQUESTS);
		EXE_start(tdbb, request, transaction);
		EXE_send(tdbb, request, 0, name.schema.length(), name.schema.c_str());
		EXE_send(tdbb, request, 1, name.object.length(), name.object.c_str());
		EXE_send(tdbb, request, 2, name.package.length(), name.package.c_str());

		struct FunctionCheckData {
			SSHORT systemFlag;
			SSHORT systemFlagNull;
			char ownerName[MAX_SQL_IDENTIFIER_LEN];
			SSHORT ownerNameNull;
		} funcData;

		if (EXE_receive(tdbb, request, 3, sizeof(funcData), &funcData))
		{
			EXE_unwind(tdbb, request);
			
			if (!funcData.systemFlagNull && funcData.systemFlag &&
				!(attachment->att_flags & ATT_system))
			{
				status_exception::raise(
					Arg::Gds(isc_dyn_cannot_mod_sysfunc) << name.toQuotedString());
			}

			status_exception::raise(
				Arg::Gds(isc_dyn_func_exists) << name.toQuotedString());
		}
		EXE_unwind(tdbb, request);

		// Convert STORE operation #40: Store function in RDB$FUNCTIONS
		AutoCacheRequest storeRequest(tdbb, drq_s_functions, DYN_REQUESTS);
		EXE_start(tdbb, storeRequest, transaction);

		struct RDB$FUNCTIONS_RECORD {
			char RDB$SCHEMA_NAME[MAX_SQL_IDENTIFIER_LEN];
			char RDB$FUNCTION_NAME[MAX_SQL_IDENTIFIER_LEN];
			char RDB$PACKAGE_NAME[MAX_SQL_IDENTIFIER_LEN];
			SSHORT RDB$PACKAGE_NAME_NULL;
			char RDB$OWNER_NAME[MAX_SQL_IDENTIFIER_LEN];
			SSHORT RDB$OWNER_NAME_NULL;
			SSHORT RDB$SYSTEM_FLAG;
			SSHORT RDB$SYSTEM_FLAG_NULL;
			bid RDB$FUNCTION_SOURCE;
			SSHORT RDB$FUNCTION_SOURCE_NULL;
			bid RDB$FUNCTION_BLR;
			SSHORT RDB$FUNCTION_BLR_NULL;
			SSHORT RDB$FUNCTION_ID;
			SSHORT RDB$FUNCTION_ID_NULL;
			SSHORT RDB$FUNCTION_TYPE;
			SSHORT RDB$FUNCTION_TYPE_NULL;
			SSHORT RDB$DETERMINISTIC_FLAG;
			SSHORT RDB$DETERMINISTIC_FLAG_NULL;
			SSHORT RDB$SQL_SECURITY;
			SSHORT RDB$SQL_SECURITY_NULL;
			bid RDB$DESCRIPTION;
			SSHORT RDB$DESCRIPTION_NULL;
			char RDB$ENGINE_NAME[MAX_SQL_IDENTIFIER_LEN];
			SSHORT RDB$ENGINE_NAME_NULL;
			char RDB$ENTRYPOINT[256];
			SSHORT RDB$ENTRYPOINT_NULL;
			char RDB$MODULE_NAME[256];
			SSHORT RDB$MODULE_NAME_NULL;
		} funcRecord;

		memset(&funcRecord, 0, sizeof(funcRecord));
		
		strcpy(funcRecord.RDB$SCHEMA_NAME, name.schema.c_str());
		strcpy(funcRecord.RDB$FUNCTION_NAME, name.object.c_str());
		
		if (name.package.hasData())
		{
			funcRecord.RDB$PACKAGE_NAME_NULL = FALSE;
			strcpy(funcRecord.RDB$PACKAGE_NAME, name.package.c_str());
		}
		else
		{
			funcRecord.RDB$PACKAGE_NAME_NULL = TRUE;
		}

		funcRecord.RDB$OWNER_NAME_NULL = FALSE;
		strcpy(funcRecord.RDB$OWNER_NAME, attachment->getUserName().c_str());

		funcRecord.RDB$SYSTEM_FLAG_NULL = FALSE;
		funcRecord.RDB$SYSTEM_FLAG = 0;

		funcRecord.RDB$FUNCTION_ID_NULL = FALSE;
		funcRecord.RDB$FUNCTION_ID = tdbb->getDatabase()->generateId();

		if (external)
		{
			funcRecord.RDB$FUNCTION_TYPE_NULL = FALSE;
			funcRecord.RDB$FUNCTION_TYPE = FUN_external;

			if (external->engine.hasData())
			{
				funcRecord.RDB$ENGINE_NAME_NULL = FALSE;
				strcpy(funcRecord.RDB$ENGINE_NAME, external->engine.c_str());
			}

			if (external->name.hasData())
			{
				funcRecord.RDB$ENTRYPOINT_NULL = FALSE;
				strcpy(funcRecord.RDB$ENTRYPOINT, external->name.c_str());
			}

			if (external->udfModule.hasData())
			{
				funcRecord.RDB$MODULE_NAME_NULL = FALSE;
				strcpy(funcRecord.RDB$MODULE_NAME, external->udfModule.c_str());
			}
		}
		else
		{
			funcRecord.RDB$FUNCTION_TYPE_NULL = FALSE;
			funcRecord.RDB$FUNCTION_TYPE = FUN_psql;

			if (source.hasData())
			{
				funcRecord.RDB$FUNCTION_SOURCE_NULL = FALSE;
				attachment->storeMetaDataBlob(tdbb, transaction, &funcRecord.RDB$FUNCTION_SOURCE, source);
			}

			// BLR would be generated from the function body
			funcRecord.RDB$FUNCTION_BLR_NULL = TRUE;
		}

		if (deterministic.isAssigned())
		{
			funcRecord.RDB$DETERMINISTIC_FLAG_NULL = FALSE;
			funcRecord.RDB$DETERMINISTIC_FLAG = deterministic.asBool() ? TRUE : FALSE;
		}
		else
		{
			funcRecord.RDB$DETERMINISTIC_FLAG_NULL = TRUE;
		}

		if (ssDefiner.has_value() && ssDefiner.value() != SqlSecurity::SS_DROP)
		{
			funcRecord.RDB$SQL_SECURITY_NULL = FALSE;
			funcRecord.RDB$SQL_SECURITY = (ssDefiner.value() == SqlSecurity::SS_DEFINER) ? FB_TRUE : FB_FALSE;
		}
		else
		{
			funcRecord.RDB$SQL_SECURITY_NULL = TRUE;
		}

		funcRecord.RDB$DESCRIPTION_NULL = TRUE;

		EXE_send(tdbb, storeRequest, 0, sizeof(funcRecord), &funcRecord);
		EXE_unwind(tdbb, storeRequest);

		// Store function parameters
		if (parameters)
		{
			SSHORT parameterNumber = 0;
			for (NestConst<ParameterClause>* ptr = parameters->begin();
				 ptr != parameters->end(); ++ptr, ++parameterNumber)
			{
				const ParameterClause* parameter = *ptr;

				// Convert STORE operation #41: Store function parameter
				AutoCacheRequest paramRequest(tdbb, drq_s_func_args, DYN_REQUESTS);
				EXE_start(tdbb, paramRequest, transaction);

				struct RDB$FUNCTION_ARGUMENTS_RECORD {
					char RDB$SCHEMA_NAME[MAX_SQL_IDENTIFIER_LEN];
					char RDB$FUNCTION_NAME[MAX_SQL_IDENTIFIER_LEN];
					char RDB$PACKAGE_NAME[MAX_SQL_IDENTIFIER_LEN];
					SSHORT RDB$PACKAGE_NAME_NULL;
					SSHORT RDB$ARGUMENT_POSITION;
					SSHORT RDB$ARGUMENT_POSITION_NULL;
					SSHORT RDB$MECHANISM;
					SSHORT RDB$MECHANISM_NULL;
					SSHORT RDB$FIELD_TYPE;
					SSHORT RDB$FIELD_TYPE_NULL;
					SSHORT RDB$FIELD_SCALE;
					SSHORT RDB$FIELD_SCALE_NULL;
					SSHORT RDB$FIELD_LENGTH;
					SSHORT RDB$FIELD_LENGTH_NULL;
					SSHORT RDB$FIELD_SUB_TYPE;
					SSHORT RDB$FIELD_SUB_TYPE_NULL;
					SSHORT RDB$CHARACTER_SET_ID;
					SSHORT RDB$CHARACTER_SET_ID_NULL;
					SSHORT RDB$FIELD_PRECISION;
					SSHORT RDB$FIELD_PRECISION_NULL;
					SSHORT RDB$CHARACTER_LENGTH;
					SSHORT RDB$CHARACTER_LENGTH_NULL;
					char RDB$ARGUMENT_NAME[MAX_SQL_IDENTIFIER_LEN];
					SSHORT RDB$ARGUMENT_NAME_NULL;
					bid RDB$DEFAULT_VALUE;
					SSHORT RDB$DEFAULT_VALUE_NULL;
					bid RDB$DEFAULT_SOURCE;
					SSHORT RDB$DEFAULT_SOURCE_NULL;
					SSHORT RDB$COLLATION_ID;
					SSHORT RDB$COLLATION_ID_NULL;
					SSHORT RDB$NULL_FLAG;
					SSHORT RDB$NULL_FLAG_NULL;
					SSHORT RDB$ARGUMENT_MECHANISM;
					SSHORT RDB$ARGUMENT_MECHANISM_NULL;
					char RDB$FIELD_NAME[MAX_SQL_IDENTIFIER_LEN];
					SSHORT RDB$FIELD_NAME_NULL;
					char RDB$RELATION_NAME[MAX_SQL_IDENTIFIER_LEN];
					SSHORT RDB$RELATION_NAME_NULL;
					SSHORT RDB$SYSTEM_FLAG;
					SSHORT RDB$SYSTEM_FLAG_NULL;
					bid RDB$DESCRIPTION;
					SSHORT RDB$DESCRIPTION_NULL;
				} paramRecord;

				memset(&paramRecord, 0, sizeof(paramRecord));

				strcpy(paramRecord.RDB$SCHEMA_NAME, name.schema.c_str());
				strcpy(paramRecord.RDB$FUNCTION_NAME, name.object.c_str());
				
				if (name.package.hasData())
				{
					paramRecord.RDB$PACKAGE_NAME_NULL = FALSE;
					strcpy(paramRecord.RDB$PACKAGE_NAME, name.package.c_str());
				}
				else
				{
					paramRecord.RDB$PACKAGE_NAME_NULL = TRUE;
				}

				paramRecord.RDB$ARGUMENT_POSITION_NULL = FALSE;
				paramRecord.RDB$ARGUMENT_POSITION = parameterNumber;

				if (parameter->name.hasData())
				{
					paramRecord.RDB$ARGUMENT_NAME_NULL = FALSE;
					strcpy(paramRecord.RDB$ARGUMENT_NAME, parameter->name.c_str());
				}
				else
				{
					paramRecord.RDB$ARGUMENT_NAME_NULL = TRUE;
				}

				paramRecord.RDB$SYSTEM_FLAG_NULL = FALSE;
				paramRecord.RDB$SYSTEM_FLAG = 0;

				if (parameter->type)
				{
					updateRdbFields(parameter->type->typeClause,
						paramRecord.RDB$FIELD_TYPE,
						paramRecord.RDB$FIELD_LENGTH,
						paramRecord.RDB$FIELD_SUB_TYPE_NULL, paramRecord.RDB$FIELD_SUB_TYPE,
						paramRecord.RDB$FIELD_SCALE_NULL, paramRecord.RDB$FIELD_SCALE,
						paramRecord.RDB$CHARACTER_SET_ID_NULL, paramRecord.RDB$CHARACTER_SET_ID,
						paramRecord.RDB$CHARACTER_LENGTH_NULL, paramRecord.RDB$CHARACTER_LENGTH,
						paramRecord.RDB$FIELD_PRECISION_NULL, paramRecord.RDB$FIELD_PRECISION,
						paramRecord.RDB$COLLATION_ID_NULL, paramRecord.RDB$COLLATION_ID,
						paramRecord.RDB$FIELD_SUB_TYPE_NULL, paramRecord.RDB$FIELD_SUB_TYPE);

					paramRecord.RDB$NULL_FLAG_NULL = FALSE;
					paramRecord.RDB$NULL_FLAG = parameter->type->notNull ? 1 : 0;
				}

				if (parameter->udfMechanism.has_value())
				{
					paramRecord.RDB$MECHANISM_NULL = FALSE;
					paramRecord.RDB$MECHANISM = parameter->udfMechanism.value();
				}
				else
				{
					paramRecord.RDB$MECHANISM_NULL = TRUE;
				}

				paramRecord.RDB$DEFAULT_VALUE_NULL = TRUE;
				paramRecord.RDB$DEFAULT_SOURCE_NULL = TRUE;
				paramRecord.RDB$DESCRIPTION_NULL = TRUE;

				EXE_send(tdbb, paramRequest, 0, sizeof(paramRecord), &paramRecord);
				EXE_unwind(tdbb, paramRequest);
			}
		}

		// Store return type as argument position -1 (or last position)
		if (returns)
		{
			// Convert STORE operation #42: Store function return type
			AutoCacheRequest returnRequest(tdbb, drq_s_func_return, DYN_REQUESTS);
			EXE_start(tdbb, returnRequest, transaction);

			struct RDB$FUNCTION_RETURN_RECORD {
				char RDB$SCHEMA_NAME[MAX_SQL_IDENTIFIER_LEN];
				char RDB$FUNCTION_NAME[MAX_SQL_IDENTIFIER_LEN];
				char RDB$PACKAGE_NAME[MAX_SQL_IDENTIFIER_LEN];
				SSHORT RDB$PACKAGE_NAME_NULL;
				SSHORT RDB$ARGUMENT_POSITION;
				SSHORT RDB$ARGUMENT_POSITION_NULL;
				SSHORT RDB$MECHANISM;
				SSHORT RDB$MECHANISM_NULL;
				SSHORT RDB$FIELD_TYPE;
				SSHORT RDB$FIELD_TYPE_NULL;
				SSHORT RDB$FIELD_SCALE;
				SSHORT RDB$FIELD_SCALE_NULL;
				SSHORT RDB$FIELD_LENGTH;
				SSHORT RDB$FIELD_LENGTH_NULL;
				SSHORT RDB$FIELD_SUB_TYPE;
				SSHORT RDB$FIELD_SUB_TYPE_NULL;
				SSHORT RDB$CHARACTER_SET_ID;
				SSHORT RDB$CHARACTER_SET_ID_NULL;
				SSHORT RDB$FIELD_PRECISION;
				SSHORT RDB$FIELD_PRECISION_NULL;
				SSHORT RDB$CHARACTER_LENGTH;
				SSHORT RDB$CHARACTER_LENGTH_NULL;
				SSHORT RDB$COLLATION_ID;
				SSHORT RDB$COLLATION_ID_NULL;
				SSHORT RDB$NULL_FLAG;
				SSHORT RDB$NULL_FLAG_NULL;
				SSHORT RDB$SYSTEM_FLAG;
				SSHORT RDB$SYSTEM_FLAG_NULL;
			} returnRecord;

			memset(&returnRecord, 0, sizeof(returnRecord));

			strcpy(returnRecord.RDB$SCHEMA_NAME, name.schema.c_str());
			strcpy(returnRecord.RDB$FUNCTION_NAME, name.object.c_str());
			
			if (name.package.hasData())
			{
				returnRecord.RDB$PACKAGE_NAME_NULL = FALSE;
				strcpy(returnRecord.RDB$PACKAGE_NAME, name.package.c_str());
			}
			else
			{
				returnRecord.RDB$PACKAGE_NAME_NULL = TRUE;
			}

			returnRecord.RDB$ARGUMENT_POSITION_NULL = FALSE;
			returnRecord.RDB$ARGUMENT_POSITION = -1; // Return type marker

			returnRecord.RDB$SYSTEM_FLAG_NULL = FALSE;
			returnRecord.RDB$SYSTEM_FLAG = 0;

			updateRdbFields(returns->typeClause,
				returnRecord.RDB$FIELD_TYPE,
				returnRecord.RDB$FIELD_LENGTH,
				returnRecord.RDB$FIELD_SUB_TYPE_NULL, returnRecord.RDB$FIELD_SUB_TYPE,
				returnRecord.RDB$FIELD_SCALE_NULL, returnRecord.RDB$FIELD_SCALE,
				returnRecord.RDB$CHARACTER_SET_ID_NULL, returnRecord.RDB$CHARACTER_SET_ID,
				returnRecord.RDB$CHARACTER_LENGTH_NULL, returnRecord.RDB$CHARACTER_LENGTH,
				returnRecord.RDB$FIELD_PRECISION_NULL, returnRecord.RDB$FIELD_PRECISION,
				returnRecord.RDB$COLLATION_ID_NULL, returnRecord.RDB$COLLATION_ID,
				returnRecord.RDB$FIELD_SUB_TYPE_NULL, returnRecord.RDB$FIELD_SUB_TYPE);

			returnRecord.RDB$NULL_FLAG_NULL = FALSE;
			returnRecord.RDB$NULL_FLAG = returns->notNull ? 1 : 0;

			returnRecord.RDB$MECHANISM_NULL = TRUE;

			EXE_send(tdbb, returnRequest, 0, sizeof(returnRecord), &returnRecord);
			EXE_unwind(tdbb, returnRequest);
		}
	}
	else
	{
		// ALTER FUNCTION case
		executeAlterIndividualParameters(tdbb, dsqlScratch, transaction, false, runTriggers);
	}

	if (!runTriggers || name.package.isEmpty())
		executeDdlTrigger(tdbb, dsqlScratch, transaction, DTW_AFTER, triggerType, name);
}

//----------------------
// DropFunctionNode Class Implementation  
//----------------------

void DropFunctionNode::execute(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch,
	jrd_tra* transaction)
{
	Attachment* const attachment = transaction->tra_attachment;

	if (name.package.isEmpty())
		executeDdlTrigger(tdbb, dsqlScratch, transaction, DTW_BEFORE, DDL_TRIGGER_DROP_FUNCTION, name);

	// Convert FOR loop #43: Find function to drop
	AutoCacheRequest request(tdbb, drq_l_func_info2, DYN_REQUESTS);
	EXE_start(tdbb, request, transaction);
	EXE_send(tdbb, request, 0, name.schema.length(), name.schema.c_str());
	EXE_send(tdbb, request, 1, name.object.length(), name.object.c_str());
	EXE_send(tdbb, request, 2, name.package.length(), name.package.c_str());

	struct FunctionDropData {
		SSHORT systemFlag;
		SSHORT systemFlagNull;
		char ownerName[MAX_SQL_IDENTIFIER_LEN];
		SSHORT ownerNameNull;
	} funcData;

	bool functionFound = false;
	if (EXE_receive(tdbb, request, 3, sizeof(funcData), &funcData))
	{
		functionFound = true;
		
		if (!funcData.systemFlagNull && funcData.systemFlag &&
			!(attachment->att_flags & ATT_system))
		{
			EXE_unwind(tdbb, request);
			status_exception::raise(
				Arg::Gds(isc_dyn_cannot_mod_sysfunc) << name.toQuotedString());
		}
	}
	EXE_unwind(tdbb, request);

	if (!functionFound)
	{
		if (!silent)
		{
			status_exception::raise(
				Arg::Gds(isc_dyn_func_not_found) << name.toQuotedString());
		}
		return;
	}

	// Convert DELETE operation #20: Delete function from RDB$FUNCTIONS
	AutoCacheRequest deleteRequest(tdbb, drq_e_function, DYN_REQUESTS);
	EXE_start(tdbb, deleteRequest, transaction);
	EXE_send(tdbb, deleteRequest, 0, name.schema.length(), name.schema.c_str());
	EXE_send(tdbb, deleteRequest, 1, name.object.length(), name.object.c_str());
	EXE_send(tdbb, deleteRequest, 2, name.package.length(), name.package.c_str());

	if (EXE_receive(tdbb, deleteRequest, 3, 0, NULL))
	{
		// Record found, delete it
		struct DeleteConfirm { char confirm; } deleteData;
		deleteData.confirm = 1;
		EXE_send(tdbb, deleteRequest, 4, sizeof(deleteData), &deleteData);
	}
	EXE_unwind(tdbb, deleteRequest);

	if (name.package.isEmpty())
		executeDdlTrigger(tdbb, dsqlScratch, transaction, DTW_AFTER, DDL_TRIGGER_DROP_FUNCTION, name);
}

//----------------------
// CreateAlterProcedureNode Class Implementation  
//----------------------

void CreateAlterProcedureNode::execute(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch,
	jrd_tra* transaction)
{
	Attachment* const attachment = transaction->tra_attachment;

	const int triggerType = create ? DDL_TRIGGER_CREATE_PROCEDURE : DDL_TRIGGER_ALTER_PROCEDURE;
	
	if (!runTriggers || name.package.isEmpty())
		executeDdlTrigger(tdbb, dsqlScratch, transaction, DTW_BEFORE, triggerType, name);

	if (create)
	{
		// Convert FOR loop #50: Check if procedure already exists
		AutoCacheRequest request(tdbb, drq_l_proc_info, DYN_REQUESTS);
		EXE_start(tdbb, request, transaction);
		EXE_send(tdbb, request, 0, name.schema.length(), name.schema.c_str());
		EXE_send(tdbb, request, 1, name.object.length(), name.object.c_str());
		EXE_send(tdbb, request, 2, name.package.length(), name.package.c_str());

		struct ProcedureCheckData {
			SSHORT systemFlag;
			SSHORT systemFlagNull;
			char ownerName[MAX_SQL_IDENTIFIER_LEN];
			SSHORT ownerNameNull;
		} procData;

		if (EXE_receive(tdbb, request, 3, sizeof(procData), &procData))
		{
			EXE_unwind(tdbb, request);
			
			if (!procData.systemFlagNull && procData.systemFlag &&
				!(attachment->att_flags & ATT_system))
			{
				status_exception::raise(
					Arg::Gds(isc_dyn_cannot_mod_sysproc) << name.toQuotedString());
			}

			status_exception::raise(
				Arg::Gds(isc_dyn_proc_exists) << name.toQuotedString());
		}
		EXE_unwind(tdbb, request);

		// Convert STORE operation #50: Store procedure in RDB$PROCEDURES
		AutoCacheRequest storeRequest(tdbb, drq_s_procedures, DYN_REQUESTS);
		EXE_start(tdbb, storeRequest, transaction);

		struct RDB$PROCEDURES_RECORD {
			char RDB$SCHEMA_NAME[MAX_SQL_IDENTIFIER_LEN];
			char RDB$PROCEDURE_NAME[MAX_SQL_IDENTIFIER_LEN];
			char RDB$PACKAGE_NAME[MAX_SQL_IDENTIFIER_LEN];
			SSHORT RDB$PACKAGE_NAME_NULL;
			SSHORT RDB$PROCEDURE_ID;
			SSHORT RDB$PROCEDURE_ID_NULL;
			SSHORT RDB$PROCEDURE_INPUTS;
			SSHORT RDB$PROCEDURE_INPUTS_NULL;
			SSHORT RDB$PROCEDURE_OUTPUTS;
			SSHORT RDB$PROCEDURE_OUTPUTS_NULL;
			bid RDB$PROCEDURE_SOURCE;
			SSHORT RDB$PROCEDURE_SOURCE_NULL;
			bid RDB$PROCEDURE_BLR;
			SSHORT RDB$PROCEDURE_BLR_NULL;
			char RDB$OWNER_NAME[MAX_SQL_IDENTIFIER_LEN];
			SSHORT RDB$OWNER_NAME_NULL;
			SSHORT RDB$SYSTEM_FLAG;
			SSHORT RDB$SYSTEM_FLAG_NULL;
			SSHORT RDB$PROCEDURE_TYPE;
			SSHORT RDB$PROCEDURE_TYPE_NULL;
			SSHORT RDB$SQL_SECURITY;
			SSHORT RDB$SQL_SECURITY_NULL;
			bid RDB$DESCRIPTION;
			SSHORT RDB$DESCRIPTION_NULL;
			char RDB$ENGINE_NAME[MAX_SQL_IDENTIFIER_LEN];
			SSHORT RDB$ENGINE_NAME_NULL;
			char RDB$ENTRYPOINT[256];
			SSHORT RDB$ENTRYPOINT_NULL;
		} procRecord;

		memset(&procRecord, 0, sizeof(procRecord));
		
		strcpy(procRecord.RDB$SCHEMA_NAME, name.schema.c_str());
		strcpy(procRecord.RDB$PROCEDURE_NAME, name.object.c_str());
		
		if (name.package.hasData())
		{
			procRecord.RDB$PACKAGE_NAME_NULL = FALSE;
			strcpy(procRecord.RDB$PACKAGE_NAME, name.package.c_str());
		}
		else
		{
			procRecord.RDB$PACKAGE_NAME_NULL = TRUE;
		}

		procRecord.RDB$PROCEDURE_ID_NULL = FALSE;
		procRecord.RDB$PROCEDURE_ID = tdbb->getDatabase()->generateId();

		procRecord.RDB$OWNER_NAME_NULL = FALSE;
		strcpy(procRecord.RDB$OWNER_NAME, attachment->getUserName().c_str());

		procRecord.RDB$SYSTEM_FLAG_NULL = FALSE;
		procRecord.RDB$SYSTEM_FLAG = 0;

		if (external)
		{
			procRecord.RDB$PROCEDURE_TYPE_NULL = FALSE;
			procRecord.RDB$PROCEDURE_TYPE = PROC_external;

			if (external->engine.hasData())
			{
				procRecord.RDB$ENGINE_NAME_NULL = FALSE;
				strcpy(procRecord.RDB$ENGINE_NAME, external->engine.c_str());
			}

			if (external->name.hasData())
			{
				procRecord.RDB$ENTRYPOINT_NULL = FALSE;
				strcpy(procRecord.RDB$ENTRYPOINT, external->name.c_str());
			}
		}
		else
		{
			procRecord.RDB$PROCEDURE_TYPE_NULL = FALSE;
			procRecord.RDB$PROCEDURE_TYPE = PROC_psql;

			if (source.hasData())
			{
				procRecord.RDB$PROCEDURE_SOURCE_NULL = FALSE;
				attachment->storeMetaDataBlob(tdbb, transaction, &procRecord.RDB$PROCEDURE_SOURCE, source);
			}

			// BLR would be generated from the procedure body
			procRecord.RDB$PROCEDURE_BLR_NULL = TRUE;
		}

		// Count input and output parameters
		SSHORT inputCount = 0;
		SSHORT outputCount = 0;
		
		if (inputParameters)
			inputCount = inputParameters->getCount();
		if (outputParameters)
			outputCount = outputParameters->getCount();

		procRecord.RDB$PROCEDURE_INPUTS_NULL = FALSE;
		procRecord.RDB$PROCEDURE_INPUTS = inputCount;
		
		procRecord.RDB$PROCEDURE_OUTPUTS_NULL = FALSE;
		procRecord.RDB$PROCEDURE_OUTPUTS = outputCount;

		if (ssDefiner.has_value() && ssDefiner.value() != SqlSecurity::SS_DROP)
		{
			procRecord.RDB$SQL_SECURITY_NULL = FALSE;
			procRecord.RDB$SQL_SECURITY = (ssDefiner.value() == SqlSecurity::SS_DEFINER) ? FB_TRUE : FB_FALSE;
		}
		else
		{
			procRecord.RDB$SQL_SECURITY_NULL = TRUE;
		}

		procRecord.RDB$DESCRIPTION_NULL = TRUE;

		EXE_send(tdbb, storeRequest, 0, sizeof(procRecord), &procRecord);
		EXE_unwind(tdbb, storeRequest);

		// Store input parameters
		if (inputParameters)
		{
			SSHORT parameterNumber = 0;
			for (NestConst<ParameterClause>* ptr = inputParameters->begin();
				 ptr != inputParameters->end(); ++ptr, ++parameterNumber)
			{
				const ParameterClause* parameter = *ptr;

				// Convert STORE operation #51: Store procedure input parameter
				AutoCacheRequest paramRequest(tdbb, drq_s_proc_prms, DYN_REQUESTS);
				EXE_start(tdbb, paramRequest, transaction);

				struct RDB$PROCEDURE_PARAMETERS_RECORD {
					char RDB$SCHEMA_NAME[MAX_SQL_IDENTIFIER_LEN];
					char RDB$PROCEDURE_NAME[MAX_SQL_IDENTIFIER_LEN];
					char RDB$PACKAGE_NAME[MAX_SQL_IDENTIFIER_LEN];
					SSHORT RDB$PACKAGE_NAME_NULL;
					SSHORT RDB$PARAMETER_NUMBER;
					SSHORT RDB$PARAMETER_NUMBER_NULL;
					SSHORT RDB$PARAMETER_TYPE;
					SSHORT RDB$PARAMETER_TYPE_NULL;
					char RDB$PARAMETER_NAME[MAX_SQL_IDENTIFIER_LEN];
					SSHORT RDB$PARAMETER_NAME_NULL;
					SSHORT RDB$FIELD_TYPE;
					SSHORT RDB$FIELD_TYPE_NULL;
					SSHORT RDB$FIELD_SCALE;
					SSHORT RDB$FIELD_SCALE_NULL;
					SSHORT RDB$FIELD_LENGTH;
					SSHORT RDB$FIELD_LENGTH_NULL;
					SSHORT RDB$FIELD_SUB_TYPE;
					SSHORT RDB$FIELD_SUB_TYPE_NULL;
					SSHORT RDB$CHARACTER_SET_ID;
					SSHORT RDB$CHARACTER_SET_ID_NULL;
					SSHORT RDB$FIELD_PRECISION;
					SSHORT RDB$FIELD_PRECISION_NULL;
					SSHORT RDB$CHARACTER_LENGTH;
					SSHORT RDB$CHARACTER_LENGTH_NULL;
					SSHORT RDB$COLLATION_ID;
					SSHORT RDB$COLLATION_ID_NULL;
					SSHORT RDB$NULL_FLAG;
					SSHORT RDB$NULL_FLAG_NULL;
					bid RDB$DEFAULT_VALUE;
					SSHORT RDB$DEFAULT_VALUE_NULL;
					bid RDB$DEFAULT_SOURCE;
					SSHORT RDB$DEFAULT_SOURCE_NULL;
					SSHORT RDB$SYSTEM_FLAG;
					SSHORT RDB$SYSTEM_FLAG_NULL;
					bid RDB$DESCRIPTION;
					SSHORT RDB$DESCRIPTION_NULL;
				} paramRecord;

				memset(&paramRecord, 0, sizeof(paramRecord));

				strcpy(paramRecord.RDB$SCHEMA_NAME, name.schema.c_str());
				strcpy(paramRecord.RDB$PROCEDURE_NAME, name.object.c_str());
				
				if (name.package.hasData())
				{
					paramRecord.RDB$PACKAGE_NAME_NULL = FALSE;
					strcpy(paramRecord.RDB$PACKAGE_NAME, name.package.c_str());
				}
				else
				{
					paramRecord.RDB$PACKAGE_NAME_NULL = TRUE;
				}

				paramRecord.RDB$PARAMETER_NUMBER_NULL = FALSE;
				paramRecord.RDB$PARAMETER_NUMBER = parameterNumber;

				paramRecord.RDB$PARAMETER_TYPE_NULL = FALSE;
				paramRecord.RDB$PARAMETER_TYPE = PARAM_input;

				if (parameter->name.hasData())
				{
					paramRecord.RDB$PARAMETER_NAME_NULL = FALSE;
					strcpy(paramRecord.RDB$PARAMETER_NAME, parameter->name.c_str());
				}
				else
				{
					paramRecord.RDB$PARAMETER_NAME_NULL = TRUE;
				}

				paramRecord.RDB$SYSTEM_FLAG_NULL = FALSE;
				paramRecord.RDB$SYSTEM_FLAG = 0;

				if (parameter->type)
				{
					updateRdbFields(parameter->type->typeClause,
						paramRecord.RDB$FIELD_TYPE,
						paramRecord.RDB$FIELD_LENGTH,
						paramRecord.RDB$FIELD_SUB_TYPE_NULL, paramRecord.RDB$FIELD_SUB_TYPE,
						paramRecord.RDB$FIELD_SCALE_NULL, paramRecord.RDB$FIELD_SCALE,
						paramRecord.RDB$CHARACTER_SET_ID_NULL, paramRecord.RDB$CHARACTER_SET_ID,
						paramRecord.RDB$CHARACTER_LENGTH_NULL, paramRecord.RDB$CHARACTER_LENGTH,
						paramRecord.RDB$FIELD_PRECISION_NULL, paramRecord.RDB$FIELD_PRECISION,
						paramRecord.RDB$COLLATION_ID_NULL, paramRecord.RDB$COLLATION_ID,
						paramRecord.RDB$FIELD_SUB_TYPE_NULL, paramRecord.RDB$FIELD_SUB_TYPE);

					paramRecord.RDB$NULL_FLAG_NULL = FALSE;
					paramRecord.RDB$NULL_FLAG = parameter->type->notNull ? 1 : 0;
				}

				paramRecord.RDB$DEFAULT_VALUE_NULL = TRUE;
				paramRecord.RDB$DEFAULT_SOURCE_NULL = TRUE;
				paramRecord.RDB$DESCRIPTION_NULL = TRUE;

				EXE_send(tdbb, paramRequest, 0, sizeof(paramRecord), &paramRecord);
				EXE_unwind(tdbb, paramRequest);
			}
		}

		// Store output parameters
		if (outputParameters)
		{
			SSHORT parameterNumber = 0;
			for (NestConst<ParameterClause>* ptr = outputParameters->begin();
				 ptr != outputParameters->end(); ++ptr, ++parameterNumber)
			{
				const ParameterClause* parameter = *ptr;

				// Convert STORE operation #52: Store procedure output parameter
				AutoCacheRequest paramRequest(tdbb, drq_s_proc_prms2, DYN_REQUESTS);
				EXE_start(tdbb, paramRequest, transaction);

				struct RDB$PROCEDURE_OUTPUT_PARAMETERS_RECORD {
					char RDB$SCHEMA_NAME[MAX_SQL_IDENTIFIER_LEN];
					char RDB$PROCEDURE_NAME[MAX_SQL_IDENTIFIER_LEN];
					char RDB$PACKAGE_NAME[MAX_SQL_IDENTIFIER_LEN];
					SSHORT RDB$PACKAGE_NAME_NULL;
					SSHORT RDB$PARAMETER_NUMBER;
					SSHORT RDB$PARAMETER_NUMBER_NULL;
					SSHORT RDB$PARAMETER_TYPE;
					SSHORT RDB$PARAMETER_TYPE_NULL;
					char RDB$PARAMETER_NAME[MAX_SQL_IDENTIFIER_LEN];
					SSHORT RDB$PARAMETER_NAME_NULL;
					SSHORT RDB$FIELD_TYPE;
					SSHORT RDB$FIELD_TYPE_NULL;
					SSHORT RDB$FIELD_SCALE;
					SSHORT RDB$FIELD_SCALE_NULL;
					SSHORT RDB$FIELD_LENGTH;
					SSHORT RDB$FIELD_LENGTH_NULL;
					SSHORT RDB$FIELD_SUB_TYPE;
					SSHORT RDB$FIELD_SUB_TYPE_NULL;
					SSHORT RDB$CHARACTER_SET_ID;
					SSHORT RDB$CHARACTER_SET_ID_NULL;
					SSHORT RDB$FIELD_PRECISION;
					SSHORT RDB$FIELD_PRECISION_NULL;
					SSHORT RDB$CHARACTER_LENGTH;
					SSHORT RDB$CHARACTER_LENGTH_NULL;
					SSHORT RDB$COLLATION_ID;
					SSHORT RDB$COLLATION_ID_NULL;
					SSHORT RDB$NULL_FLAG;
					SSHORT RDB$NULL_FLAG_NULL;
					SSHORT RDB$SYSTEM_FLAG;
					SSHORT RDB$SYSTEM_FLAG_NULL;
					bid RDB$DESCRIPTION;
					SSHORT RDB$DESCRIPTION_NULL;
				} paramRecord;

				memset(&paramRecord, 0, sizeof(paramRecord));

				strcpy(paramRecord.RDB$SCHEMA_NAME, name.schema.c_str());
				strcpy(paramRecord.RDB$PROCEDURE_NAME, name.object.c_str());
				
				if (name.package.hasData())
				{
					paramRecord.RDB$PACKAGE_NAME_NULL = FALSE;
					strcpy(paramRecord.RDB$PACKAGE_NAME, name.package.c_str());
				}
				else
				{
					paramRecord.RDB$PACKAGE_NAME_NULL = TRUE;
				}

				paramRecord.RDB$PARAMETER_NUMBER_NULL = FALSE;
				paramRecord.RDB$PARAMETER_NUMBER = parameterNumber;

				paramRecord.RDB$PARAMETER_TYPE_NULL = FALSE;
				paramRecord.RDB$PARAMETER_TYPE = PARAM_output;

				if (parameter->name.hasData())
				{
					paramRecord.RDB$PARAMETER_NAME_NULL = FALSE;
					strcpy(paramRecord.RDB$PARAMETER_NAME, parameter->name.c_str());
				}
				else
				{
					paramRecord.RDB$PARAMETER_NAME_NULL = TRUE;
				}

				paramRecord.RDB$SYSTEM_FLAG_NULL = FALSE;
				paramRecord.RDB$SYSTEM_FLAG = 0;

				if (parameter->type)
				{
					updateRdbFields(parameter->type->typeClause,
						paramRecord.RDB$FIELD_TYPE,
						paramRecord.RDB$FIELD_LENGTH,
						paramRecord.RDB$FIELD_SUB_TYPE_NULL, paramRecord.RDB$FIELD_SUB_TYPE,
						paramRecord.RDB$FIELD_SCALE_NULL, paramRecord.RDB$FIELD_SCALE,
						paramRecord.RDB$CHARACTER_SET_ID_NULL, paramRecord.RDB$CHARACTER_SET_ID,
						paramRecord.RDB$CHARACTER_LENGTH_NULL, paramRecord.RDB$CHARACTER_LENGTH,
						paramRecord.RDB$FIELD_PRECISION_NULL, paramRecord.RDB$FIELD_PRECISION,
						paramRecord.RDB$COLLATION_ID_NULL, paramRecord.RDB$COLLATION_ID,
						paramRecord.RDB$FIELD_SUB_TYPE_NULL, paramRecord.RDB$FIELD_SUB_TYPE);

					paramRecord.RDB$NULL_FLAG_NULL = FALSE;
					paramRecord.RDB$NULL_FLAG = parameter->type->notNull ? 1 : 0;
				}

				paramRecord.RDB$DESCRIPTION_NULL = TRUE;

				EXE_send(tdbb, paramRequest, 0, sizeof(paramRecord), &paramRecord);
				EXE_unwind(tdbb, paramRequest);
			}
		}
	}
	else
	{
		// ALTER PROCEDURE case - would need similar logic to functions
		// This is a placeholder for the alter procedure logic
	}

	if (!runTriggers || name.package.isEmpty())
		executeDdlTrigger(tdbb, dsqlScratch, transaction, DTW_AFTER, triggerType, name);
}

//----------------------
// DropProcedureNode Class Implementation  
//----------------------

void DropProcedureNode::execute(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch,
	jrd_tra* transaction)
{
	Attachment* const attachment = transaction->tra_attachment;

	if (name.package.isEmpty())
		executeDdlTrigger(tdbb, dsqlScratch, transaction, DTW_BEFORE, DDL_TRIGGER_DROP_PROCEDURE, name);

	// Convert FOR loop #53: Find procedure to drop
	AutoCacheRequest request(tdbb, drq_l_proc_info2, DYN_REQUESTS);
	EXE_start(tdbb, request, transaction);
	EXE_send(tdbb, request, 0, name.schema.length(), name.schema.c_str());
	EXE_send(tdbb, request, 1, name.object.length(), name.object.c_str());
	EXE_send(tdbb, request, 2, name.package.length(), name.package.c_str());

	struct ProcedureDropData {
		SSHORT systemFlag;
		SSHORT systemFlagNull;
		char ownerName[MAX_SQL_IDENTIFIER_LEN];
		SSHORT ownerNameNull;
	} procData;

	bool procedureFound = false;
	if (EXE_receive(tdbb, request, 3, sizeof(procData), &procData))
	{
		procedureFound = true;
		
		if (!procData.systemFlagNull && procData.systemFlag &&
			!(attachment->att_flags & ATT_system))
		{
			EXE_unwind(tdbb, request);
			status_exception::raise(
				Arg::Gds(isc_dyn_cannot_mod_sysproc) << name.toQuotedString());
		}
	}
	EXE_unwind(tdbb, request);

	if (!procedureFound)
	{
		if (!silent)
		{
			status_exception::raise(
				Arg::Gds(isc_dyn_proc_not_found) << name.toQuotedString());
		}
		return;
	}

	// Convert DELETE operation #25: Delete procedure from RDB$PROCEDURES
	AutoCacheRequest deleteRequest(tdbb, drq_e_procedure, DYN_REQUESTS);
	EXE_start(tdbb, deleteRequest, transaction);
	EXE_send(tdbb, deleteRequest, 0, name.schema.length(), name.schema.c_str());
	EXE_send(tdbb, deleteRequest, 1, name.object.length(), name.object.c_str());
	EXE_send(tdbb, deleteRequest, 2, name.package.length(), name.package.c_str());

	if (EXE_receive(tdbb, deleteRequest, 3, 0, NULL))
	{
		// Record found, delete it
		struct DeleteConfirm { char confirm; } deleteData;
		deleteData.confirm = 1;
		EXE_send(tdbb, deleteRequest, 4, sizeof(deleteData), &deleteData);
	}
	EXE_unwind(tdbb, deleteRequest);

	if (name.package.isEmpty())
		executeDdlTrigger(tdbb, dsqlScratch, transaction, DTW_AFTER, DDL_TRIGGER_DROP_PROCEDURE, name);
}

} // namespace Jrd