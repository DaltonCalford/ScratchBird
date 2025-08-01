/*
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
 *  The Original Code was created by Adriano dos Santos Fernandes
 *  for the Firebird Open Source RDBMS project.
 *
 *  Copyright (c) 2009 Adriano dos Santos Fernandes <adrianosf@uol.com.br>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#include "scratchbird.h"
#include "../dsql/PackageNodes.h"
#include "../jrd/dyn.h"
#include "../jrd/intl.h"
#include "../jrd/jrd.h"
#include "../jrd/tra.h"
#include "../jrd/dfw_proto.h"
#include "../jrd/dyn_ut_proto.h"
#include "../jrd/exe_proto.h"
#include "../jrd/met_proto.h"
#include "../jrd/vio_proto.h"
#include "../dsql/make_proto.h"
#include "../dsql/pass1_proto.h"
#include "../common/StatusArg.h"
#include "../common/classes/TriState.h"
#include "../jrd/Attachment.h"
#include "../jrd/scl_proto.h"


using namespace ScratchBird;

namespace Jrd {

using namespace ScratchBird;

// Replaced GPRE DATABASE DB = STATIC "ODS.RDB"; with modern approach
// Database access is handled through existing attachment mechanisms


//----------------------


namespace
{
	// Return function and procedure names (in the user charset) and optionally its details for a
	// given package.
	void collectPackagedItems(thread_db* tdbb, jrd_tra* transaction, const QualifiedName& packageName,
		SortedObjectsArray<Signature>& functions,
		SortedObjectsArray<Signature>& procedures, bool details)
	{
		AutoCacheRequest requestHandle(tdbb, drq_l_pkg_funcs, DYN_REQUESTS);
		AutoCacheRequest requestHandle2(tdbb, drq_l_pkg_func_args, DYN_REQUESTS);

		// Converted FOR loop #1: FOR (REQUEST_HANDLE requestHandle TRANSACTION_HANDLE transaction) FUN IN RDB$FUNCTIONS
		jrd_req* handle = requestHandle;
		EXE_start(tdbb, handle, transaction);
		
		struct {
			TEXT schema_name[MAX_SQL_IDENTIFIER_LEN];
			TEXT package_name[MAX_SQL_IDENTIFIER_LEN];
		} func_input;
		
		strcpy(func_input.schema_name, packageName.schema.c_str());
		strcpy(func_input.package_name, packageName.object.c_str());
		
		EXE_send(tdbb, handle, 0, sizeof(func_input), reinterpret_cast<UCHAR*>(&func_input));

		struct {
			TEXT RDB$FUNCTION_NAME[MAX_SQL_IDENTIFIER_LEN];
			ISC_QUAD RDB$FUNCTION_BLR;
			TEXT RDB$ENTRYPOINT[MAX_SQL_IDENTIFIER_LEN];
			SSHORT RDB$DETERMINISTIC_FLAG;
			SSHORT function_blr_null;
			SSHORT entrypoint_null;
			SSHORT deterministic_flag_null;
		} func_data;

		while (!EXE_receive(tdbb, handle, 1, sizeof(func_data), reinterpret_cast<UCHAR*>(&func_data)))
		{
			Signature function(func_data.RDB$FUNCTION_NAME);
			function.defined = !func_data.function_blr_null || !func_data.entrypoint_null;

			if (!func_data.deterministic_flag_null && func_data.RDB$DETERMINISTIC_FLAG != 0)
				function.flags |= Signature::FLAG_DETERMINISTIC;

			if (details)
			{
				// Converted FOR loop #2: FOR (REQUEST_HANDLE requestHandle2 TRANSACTION_HANDLE transaction) ARG IN RDB$FUNCTION_ARGUMENTS CROSS FLD IN RDB$FIELDS
				jrd_req* handle2 = requestHandle2;
				EXE_start(tdbb, handle2, transaction);
				
				struct {
					TEXT schema_name[MAX_SQL_IDENTIFIER_LEN];
					TEXT package_name[MAX_SQL_IDENTIFIER_LEN];
					TEXT function_name[MAX_SQL_IDENTIFIER_LEN];
				} arg_input;
				
				strcpy(arg_input.schema_name, packageName.schema.c_str());
				strcpy(arg_input.package_name, packageName.object.c_str());
				strcpy(arg_input.function_name, func_data.RDB$FUNCTION_NAME);
				
				EXE_send(tdbb, handle2, 0, sizeof(arg_input), reinterpret_cast<UCHAR*>(&arg_input));

				struct {
					SSHORT RDB$ARGUMENT_POSITION;
					TEXT RDB$ARGUMENT_NAME[MAX_SQL_IDENTIFIER_LEN];
					TEXT RDB$FIELD_SOURCE[MAX_SQL_IDENTIFIER_LEN];
					TEXT RDB$FIELD_SOURCE_SCHEMA_NAME[MAX_SQL_IDENTIFIER_LEN];
					SSHORT RDB$ARGUMENT_MECHANISM;
					TEXT RDB$FIELD_NAME[MAX_SQL_IDENTIFIER_LEN];
					TEXT RDB$RELATION_NAME[MAX_SQL_IDENTIFIER_LEN];
					TEXT RDB$RELATION_SCHEMA_NAME[MAX_SQL_IDENTIFIER_LEN];
					SSHORT RDB$COLLATION_ID;
					SSHORT RDB$NULL_FLAG;
					SSHORT RDB$FIELD_LENGTH;
					SSHORT RDB$FIELD_SCALE;
					SSHORT RDB$FIELD_TYPE;
					SSHORT RDB$FIELD_SUB_TYPE;
					SSHORT RDB$SEGMENT_LENGTH;
					SSHORT RDB$CHARACTER_LENGTH;
					SSHORT RDB$CHARACTER_SET_ID;
					SSHORT RDB$FIELD_PRECISION;
					SSHORT field_name_null;
					SSHORT relation_name_null;
					SSHORT collation_id_null;
					SSHORT null_flag_null;
					SSHORT field_length_null;
					SSHORT field_scale_null;
					SSHORT field_type_null;
					SSHORT field_sub_type_null;
					SSHORT segment_length_null;
					SSHORT field_null_flag_null;
					SSHORT character_length_null;
					SSHORT field_collation_id_null;
					SSHORT character_set_id_null;
					SSHORT field_precision_null;
				} arg_data;

				while (!EXE_receive(tdbb, handle2, 1, sizeof(arg_data), reinterpret_cast<UCHAR*>(&arg_data)))
				{
					SignatureParameter parameter(*getDefaultMemoryPool());

					parameter.number = arg_data.RDB$ARGUMENT_POSITION;
					parameter.name = arg_data.RDB$ARGUMENT_NAME;
					parameter.fieldSource = QualifiedName(arg_data.RDB$FIELD_SOURCE, arg_data.RDB$FIELD_SOURCE_SCHEMA_NAME);
					parameter.mechanism = arg_data.RDB$ARGUMENT_MECHANISM;

					if (!arg_data.field_name_null)
						parameter.fieldName = QualifiedName(arg_data.RDB$FIELD_NAME);
					if (!arg_data.relation_name_null)
						parameter.relationName = QualifiedName(arg_data.RDB$RELATION_NAME, arg_data.RDB$RELATION_SCHEMA_NAME);
					if (!arg_data.collation_id_null)
						parameter.collationId = arg_data.RDB$COLLATION_ID;
					if (!arg_data.null_flag_null)
						parameter.nullFlag = arg_data.RDB$NULL_FLAG;

					if (!arg_data.field_length_null)
						parameter.fieldLength = arg_data.RDB$FIELD_LENGTH;
					if (!arg_data.field_scale_null)
						parameter.fieldScale = arg_data.RDB$FIELD_SCALE;
					if (!arg_data.field_type_null)
						parameter.fieldType = arg_data.RDB$FIELD_TYPE;
					if (!arg_data.field_sub_type_null)
						parameter.fieldSubType = arg_data.RDB$FIELD_SUB_TYPE;
					if (!arg_data.segment_length_null)
						parameter.fieldSegmentLength = arg_data.RDB$SEGMENT_LENGTH;
					if (!arg_data.field_null_flag_null)
						parameter.fieldNullFlag = arg_data.RDB$NULL_FLAG;
					if (!arg_data.character_length_null)
						parameter.fieldCharLength = arg_data.RDB$CHARACTER_LENGTH;
					if (!arg_data.field_collation_id_null)
						parameter.fieldCollationId = arg_data.RDB$COLLATION_ID;
					if (!arg_data.character_set_id_null)
						parameter.fieldCharSetId = arg_data.RDB$CHARACTER_SET_ID;
					if (!arg_data.field_precision_null)
						parameter.fieldPrecision = arg_data.RDB$FIELD_PRECISION;

					function.parameters.add(parameter);
				}
			}

			functions.add(function);
		}

		requestHandle.reset(tdbb, drq_l_pkg_procs, DYN_REQUESTS);
		requestHandle2.reset(tdbb, drq_l_pkg_proc_args, DYN_REQUESTS);

		// Converted FOR loop #3: FOR (REQUEST_HANDLE requestHandle TRANSACTION_HANDLE transaction) PRC IN RDB$PROCEDURES
		handle = requestHandle;
		EXE_start(tdbb, handle, transaction);
		
		struct {
			TEXT schema_name[MAX_SQL_IDENTIFIER_LEN];
			TEXT package_name[MAX_SQL_IDENTIFIER_LEN];
		} proc_input;
		
		strcpy(proc_input.schema_name, packageName.schema.c_str());
		strcpy(proc_input.package_name, packageName.object.c_str());
		
		EXE_send(tdbb, handle, 0, sizeof(proc_input), reinterpret_cast<UCHAR*>(&proc_input));

		struct {
			TEXT RDB$PROCEDURE_NAME[MAX_SQL_IDENTIFIER_LEN];
			ISC_QUAD RDB$PROCEDURE_BLR;
			TEXT RDB$ENTRYPOINT[MAX_SQL_IDENTIFIER_LEN];
			SSHORT procedure_blr_null;
			SSHORT entrypoint_null;
		} proc_data;

		while (!EXE_receive(tdbb, handle, 1, sizeof(proc_data), reinterpret_cast<UCHAR*>(&proc_data)))
		{
			Signature procedure(proc_data.RDB$PROCEDURE_NAME);
			procedure.defined = !proc_data.procedure_blr_null || !proc_data.entrypoint_null;

			if (details)
			{
				// Converted FOR loop #4: FOR (REQUEST_HANDLE requestHandle2 TRANSACTION_HANDLE transaction) PRM IN RDB$PROCEDURE_PARAMETERS CROSS FLD IN RDB$FIELDS
				jrd_req* handle2 = requestHandle2;
				EXE_start(tdbb, handle2, transaction);
				
				struct {
					TEXT schema_name[MAX_SQL_IDENTIFIER_LEN];
					TEXT package_name[MAX_SQL_IDENTIFIER_LEN];
					TEXT procedure_name[MAX_SQL_IDENTIFIER_LEN];
				} prm_input;
				
				strcpy(prm_input.schema_name, packageName.schema.c_str());
				strcpy(prm_input.package_name, packageName.object.c_str());
				strcpy(prm_input.procedure_name, proc_data.RDB$PROCEDURE_NAME);
				
				EXE_send(tdbb, handle2, 0, sizeof(prm_input), reinterpret_cast<UCHAR*>(&prm_input));

				struct {
					SSHORT RDB$PARAMETER_TYPE;
					SSHORT RDB$PARAMETER_NUMBER;
					TEXT RDB$PARAMETER_NAME[MAX_SQL_IDENTIFIER_LEN];
					TEXT RDB$FIELD_SOURCE[MAX_SQL_IDENTIFIER_LEN];
					TEXT RDB$FIELD_SOURCE_SCHEMA_NAME[MAX_SQL_IDENTIFIER_LEN];
					SSHORT RDB$PARAMETER_MECHANISM;
					TEXT RDB$FIELD_NAME[MAX_SQL_IDENTIFIER_LEN];
					TEXT RDB$RELATION_NAME[MAX_SQL_IDENTIFIER_LEN];
					TEXT RDB$RELATION_SCHEMA_NAME[MAX_SQL_IDENTIFIER_LEN];
					SSHORT RDB$COLLATION_ID;
					SSHORT RDB$NULL_FLAG;
					SSHORT RDB$FIELD_LENGTH;
					SSHORT RDB$FIELD_SCALE;
					SSHORT RDB$FIELD_TYPE;
					SSHORT RDB$FIELD_SUB_TYPE;
					SSHORT RDB$SEGMENT_LENGTH;
					SSHORT RDB$CHARACTER_LENGTH;
					SSHORT RDB$CHARACTER_SET_ID;
					SSHORT RDB$FIELD_PRECISION;
					SSHORT field_name_null;
					SSHORT relation_name_null;
					SSHORT collation_id_null;
					SSHORT null_flag_null;
					SSHORT field_length_null;
					SSHORT field_scale_null;
					SSHORT field_type_null;
					SSHORT field_sub_type_null;
					SSHORT segment_length_null;
					SSHORT field_null_flag_null;
					SSHORT character_length_null;
					SSHORT field_collation_id_null;
					SSHORT character_set_id_null;
					SSHORT field_precision_null;
				} prm_data;

				while (!EXE_receive(tdbb, handle2, 1, sizeof(prm_data), reinterpret_cast<UCHAR*>(&prm_data)))
				{
					SignatureParameter parameter(*getDefaultMemoryPool());
					parameter.type = prm_data.RDB$PARAMETER_TYPE;
					parameter.number = prm_data.RDB$PARAMETER_NUMBER;
					parameter.name = prm_data.RDB$PARAMETER_NAME;
					parameter.fieldSource = QualifiedName(prm_data.RDB$FIELD_SOURCE, prm_data.RDB$FIELD_SOURCE_SCHEMA_NAME);
					parameter.mechanism = prm_data.RDB$PARAMETER_MECHANISM;

					if (!prm_data.field_name_null)
						parameter.fieldName = QualifiedName(prm_data.RDB$FIELD_NAME);
					if (!prm_data.relation_name_null)
						parameter.relationName = QualifiedName(prm_data.RDB$RELATION_NAME, prm_data.RDB$RELATION_SCHEMA_NAME);
					if (!prm_data.collation_id_null)
						parameter.collationId = prm_data.RDB$COLLATION_ID;
					if (!prm_data.null_flag_null)
						parameter.nullFlag = prm_data.RDB$NULL_FLAG;

					if (!prm_data.field_length_null)
						parameter.fieldLength = prm_data.RDB$FIELD_LENGTH;
					if (!prm_data.field_scale_null)
						parameter.fieldScale = prm_data.RDB$FIELD_SCALE;
					if (!prm_data.field_type_null)
						parameter.fieldType = prm_data.RDB$FIELD_TYPE;
					if (!prm_data.field_sub_type_null)
						parameter.fieldSubType = prm_data.RDB$FIELD_SUB_TYPE;
					if (!prm_data.segment_length_null)
						parameter.fieldSegmentLength = prm_data.RDB$SEGMENT_LENGTH;
					if (!prm_data.field_null_flag_null)
						parameter.fieldNullFlag = prm_data.RDB$NULL_FLAG;
					if (!prm_data.character_length_null)
						parameter.fieldCharLength = prm_data.RDB$CHARACTER_LENGTH;
					if (!prm_data.field_collation_id_null)
						parameter.fieldCollationId = prm_data.RDB$COLLATION_ID;
					if (!prm_data.character_set_id_null)
						parameter.fieldCharSetId = prm_data.RDB$CHARACTER_SET_ID;
					if (!prm_data.field_precision_null)
						parameter.fieldPrecision = prm_data.RDB$FIELD_PRECISION;

					procedure.parameters.add(parameter);
				}
			}

			procedures.add(procedure);
		}
	}
}	// namespace


//----------------------


string CreateAlterPackageNode::internalPrint(NodePrinter& printer) const
{
	DdlNode::internalPrint(printer);

	NODE_PRINT(printer, name);
	NODE_PRINT(printer, create);
	NODE_PRINT(printer, alter);
	NODE_PRINT(printer, source);
	//// FIXME-PRINT: NODE_PRINT(printer, items);
	NODE_PRINT(printer, functionNames);
	NODE_PRINT(printer, procedureNames);

	return "CreateAlterPackageNode";
}


DdlNode* CreateAlterPackageNode::dsqlPass(DsqlCompilerScratch* dsqlScratch)
{
	if (create)
		dsqlScratch->qualifyNewName(name);
	else
		dsqlScratch->qualifyExistingName(name, obj_package_header);

	protectSystemSchema(name.schema, obj_package_header);
	dsqlScratch->ddlSchema = name.schema;

	if (alter && !items)
		return DdlNode::dsqlPass(dsqlScratch);

	MemoryPool& pool = dsqlScratch->getPool();

	source.ltrim("\n\r\t ");

	// items
	for (unsigned i = 0; i < items->getCount(); ++i)
	{
		DdlNode* ddlNode;

		switch ((*items)[i].type)
		{
			case CreateAlterPackageNode::Item::FUNCTION:
			{
				CreateAlterFunctionNode* const fun = (*items)[i].function;
				ddlNode = fun;

				if (functionNames.exist(fun->name.object))
				{
					status_exception::raise(
						Arg::Gds(isc_no_meta_update) <<
						Arg::Gds(isc_dyn_duplicate_package_item) <<
							Arg::Str("FUNCTION") << fun->name.object.toQuotedString());
				}

				functionNames.add(fun->name.object);

				fun->alter = true;
				fun->name.schema = name.schema;
				fun->name.package = name.object;
				break;
			}

			case CreateAlterPackageNode::Item::PROCEDURE:
			{
				CreateAlterProcedureNode* const proc = (*items)[i].procedure;
				ddlNode = proc;

				if (procedureNames.exist(proc->name.object))
				{
					status_exception::raise(
						Arg::Gds(isc_no_meta_update) <<
						Arg::Gds(isc_dyn_duplicate_package_item) <<
							Arg::Str("PROCEDURE") << proc->name.object.toQuotedString());
				}

				procedureNames.add(proc->name.object);

				proc->alter = true;
				proc->name.schema = name.schema;
				proc->name.package = name.object;
				break;
			}

			default:
				fb_assert(false);
		}

		auto itemStatement = FB_NEW_POOL(pool) DsqlDdlStatement(pool, dsqlScratch->getAttachment(), ddlNode);

		auto itemScratch = (*items)[i].dsqlScratch =
			FB_NEW_POOL(pool) DsqlCompilerScratch(pool, dsqlScratch->getAttachment(),
				dsqlScratch->getTransaction(), itemStatement);

		itemScratch->ddlSchema = name.schema;
		itemScratch->clientDialect = dsqlScratch->clientDialect;
		itemScratch->flags |= DsqlCompilerScratch::FLAG_DDL;
		itemScratch->package = name;

		if (itemScratch->clientDialect > SQL_DIALECT_V5)
			itemStatement->setBlrVersion(5);
		else
			itemStatement->setBlrVersion(4);

		ddlNode->dsqlPass(itemScratch);
	}

	return DdlNode::dsqlPass(dsqlScratch);
}


void CreateAlterPackageNode::checkPermission(thread_db* tdbb, jrd_tra* transaction)
{
	if (alter)
	{
		if (SCL_check_package(tdbb, name, SCL_alter) || !create)
			return;
	}

	SCL_check_create_access(tdbb, obj_packages, name.schema);
}


void CreateAlterPackageNode::execute(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch,
	jrd_tra* transaction)
{
	fb_assert(create || alter);

	//Database* dbb = tdbb->getDatabase();

	//dbb->checkOdsForDsql(ODS_12_0);

	// run all statements under savepoint control
	AutoSavePoint savePoint(tdbb, transaction);

	const bool alterIndividualParameters = (!create && alter && !items);

	if (alter)
	{
		if (alterIndividualParameters)
		{
			if (!executeAlterIndividualParameters(tdbb, dsqlScratch, transaction))
				status_exception::raise(
					Arg::Gds(isc_no_meta_update) <<
					Arg::Gds(isc_dyn_package_not_found) << name.toQuotedString());
		}
		else if (!executeAlter(tdbb, dsqlScratch, transaction))
		{
			if (create)	// create or alter
				executeCreate(tdbb, dsqlScratch, transaction);
			else
			{
				status_exception::raise(
					Arg::Gds(isc_no_meta_update) <<
					Arg::Gds(isc_dyn_package_not_found) << name.toQuotedString());
			}
		}

		dsc schemaDesc, nameDesc;
		schemaDesc.makeText(name.schema.length(), ttype_metadata, (UCHAR*) const_cast<char*>(name.schema.c_str()));
		nameDesc.makeText(name.object.length(), ttype_metadata, (UCHAR*) const_cast<char*>(name.object.c_str()));
		DFW_post_work(transaction, dfw_modify_package_header, &nameDesc, &schemaDesc, 0);
	}
	else
		executeCreate(tdbb, dsqlScratch, transaction);

	savePoint.release();	// everything is ok
}


void CreateAlterPackageNode::executeCreate(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch,
	jrd_tra* transaction)
{
	Attachment* const attachment = transaction->getAttachment();
	const MetaString& ownerName = attachment->getEffectiveUserName();

	if (createIfNotExistsOnly && !DYN_UTIL_check_unique_name_nothrow(tdbb, transaction, name, obj_package_header))
		return;

	executeDdlTrigger(tdbb, dsqlScratch, transaction, DTW_BEFORE, DDL_TRIGGER_CREATE_PACKAGE, name, {});

	DYN_UTIL_check_unique_name(tdbb, transaction, name, obj_package_header);

	AutoCacheRequest requestHandle(tdbb, drq_s_pkg, DYN_REQUESTS);

	// Converted STORE operation: STORE (REQUEST_HANDLE requestHandle TRANSACTION_HANDLE transaction) PKG IN RDB$PACKAGES USING
	jrd_req* handle = requestHandle;
	EXE_start(tdbb, handle, transaction);
	
	struct {
		TEXT RDB$SCHEMA_NAME[MAX_SQL_IDENTIFIER_LEN];
		TEXT RDB$PACKAGE_NAME[MAX_SQL_IDENTIFIER_LEN];
		SSHORT RDB$SYSTEM_FLAG;
		TEXT RDB$OWNER_NAME[MAX_SQL_IDENTIFIER_LEN];
		ISC_QUAD RDB$PACKAGE_HEADER_SOURCE;
		SSHORT RDB$SQL_SECURITY;
		SSHORT schema_name_null;
		SSHORT package_name_null;
		SSHORT system_flag_null;
		SSHORT owner_name_null;
		SSHORT package_header_source_null;
		SSHORT sql_security_null;
	} pkg_data;
	
	pkg_data.schema_name_null = FALSE;
	strcpy(pkg_data.RDB$SCHEMA_NAME, name.schema.c_str());

	pkg_data.package_name_null = FALSE;
	strcpy(pkg_data.RDB$PACKAGE_NAME, name.object.c_str());

	pkg_data.system_flag_null = FALSE;
	pkg_data.RDB$SYSTEM_FLAG = 0;

	pkg_data.owner_name_null = FALSE;
	strcpy(pkg_data.RDB$OWNER_NAME, ownerName.c_str());

	pkg_data.package_header_source_null = FALSE;
	attachment->storeMetaDataBlob(tdbb, transaction, &pkg_data.RDB$PACKAGE_HEADER_SOURCE, source);

	if (ssDefiner.has_value())
	{
		pkg_data.sql_security_null = FALSE;
		pkg_data.RDB$SQL_SECURITY = ssDefiner.value() == SqlSecurity::SS_DEFINER ? FB_TRUE : FB_FALSE;
	}
	else
		pkg_data.sql_security_null = TRUE;
	
	EXE_send(tdbb, handle, 0, sizeof(pkg_data), reinterpret_cast<UCHAR*>(&pkg_data));

	storePrivileges(tdbb, transaction, name, obj_package_header, EXEC_PRIVILEGES);

	owner = ownerName;

	executeItems(tdbb, dsqlScratch, transaction);

	executeDdlTrigger(tdbb, dsqlScratch, transaction, DTW_AFTER, DDL_TRIGGER_CREATE_PACKAGE, name, {});
}


bool CreateAlterPackageNode::executeAlter(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch,
	jrd_tra* transaction)
{
	MemoryPool& pool = dsqlScratch->getPool();
	Attachment* attachment = transaction->getAttachment();
	AutoCacheRequest requestHandle(tdbb, drq_m_pkg, DYN_REQUESTS);
	bool modified = false;

	// Converted FOR loop #5: FOR (REQUEST_HANDLE requestHandle TRANSACTION_HANDLE transaction) PKG IN RDB$PACKAGES
	jrd_req* handle = requestHandle;
	EXE_start(tdbb, handle, transaction);
	
	struct {
		TEXT schema_name[MAX_SQL_IDENTIFIER_LEN];
		TEXT package_name[MAX_SQL_IDENTIFIER_LEN];
	} pkg_input;
	
	strcpy(pkg_input.schema_name, name.schema.c_str());
	strcpy(pkg_input.package_name, name.object.c_str());
	
	EXE_send(tdbb, handle, 0, sizeof(pkg_input), reinterpret_cast<UCHAR*>(&pkg_input));

	struct {
		TEXT RDB$OWNER_NAME[MAX_SQL_IDENTIFIER_LEN];
		SSHORT RDB$VALID_BODY_FLAG;
		SSHORT valid_body_flag_null;
	} pkg_data;

	while (!EXE_receive(tdbb, handle, 1, sizeof(pkg_data), reinterpret_cast<UCHAR*>(&pkg_data)))
	{
		modified = true;

		executeDdlTrigger(tdbb, dsqlScratch, transaction, DTW_BEFORE, DDL_TRIGGER_ALTER_PACKAGE, name, {});

		SortedObjectsArray<Signature> existingFuncs(pool);
		SortedObjectsArray<Signature> existingProcs(pool);
		collectPackagedItems(tdbb, transaction, name, existingFuncs, existingProcs, false);

		for (SortedObjectsArray<Signature>::iterator i = existingFuncs.begin();
			 i != existingFuncs.end(); ++i)
		{
			if (!functionNames.exist(i->name))
			{
				DropFunctionNode dropNode(pool, QualifiedName(i->name, name.schema, name.object));
				dropNode.dsqlPass(dsqlScratch);
				dropNode.executeDdl(tdbb, dsqlScratch, transaction, true);
			}
		}

		for (SortedObjectsArray<Signature>::iterator i = existingProcs.begin();
			 i != existingProcs.end(); ++i)
		{
			if (!procedureNames.exist(i->name))
			{
				DropProcedureNode dropNode(pool, QualifiedName(i->name, name.schema, name.object));
				dropNode.dsqlPass(dsqlScratch);
				dropNode.executeDdl(tdbb, dsqlScratch, transaction, true);
			}
		}

		// Converted MODIFY operation #1: MODIFY PKG
		AutoCacheRequest modify_request(tdbb, drq_modify_pkg_header, DYN_REQUESTS);
		jrd_req* modify_handle = modify_request;
		EXE_start(tdbb, modify_handle, transaction);
		
		struct {
			TEXT schema_name[MAX_SQL_IDENTIFIER_LEN];
			TEXT package_name[MAX_SQL_IDENTIFIER_LEN];
			ISC_QUAD RDB$PACKAGE_HEADER_SOURCE;
			SSHORT RDB$VALID_BODY_FLAG;
			SSHORT RDB$SQL_SECURITY;
			SSHORT package_header_source_null;
			SSHORT valid_body_flag_null;
			SSHORT sql_security_null;
		} modify_data;
		
		strcpy(modify_data.schema_name, name.schema.c_str());
		strcpy(modify_data.package_name, name.object.c_str());
		
		modify_data.package_header_source_null = FALSE;
		attachment->storeMetaDataBlob(tdbb, transaction, &modify_data.RDB$PACKAGE_HEADER_SOURCE, source);

		if (!pkg_data.valid_body_flag_null)
		{
			modify_data.valid_body_flag_null = FALSE;
			modify_data.RDB$VALID_BODY_FLAG = FALSE;
		}
		else
			modify_data.valid_body_flag_null = TRUE;

		if (ssDefiner.has_value())
		{
			modify_data.sql_security_null = FALSE;
			modify_data.RDB$SQL_SECURITY = ssDefiner.value() == SqlSecurity::SS_DEFINER ? FB_TRUE : FB_FALSE;
		}
		else
			modify_data.sql_security_null = TRUE;
		
		EXE_send(tdbb, modify_handle, 0, sizeof(modify_data), reinterpret_cast<UCHAR*>(&modify_data));

		owner = pkg_data.RDB$OWNER_NAME;

		dsc schemaDesc, nameDesc;
		schemaDesc.makeText(name.schema.length(), ttype_metadata, (UCHAR*) const_cast<char*>(name.schema.c_str()));
		nameDesc.makeText(name.object.length(), ttype_metadata, (UCHAR*) const_cast<char*>(name.object.c_str()));
		DFW_post_work(transaction, dfw_drop_package_body, &nameDesc, &schemaDesc, 0);
	}

	if (modified)
	{
		executeItems(tdbb, dsqlScratch, transaction);

		executeDdlTrigger(tdbb, dsqlScratch, transaction, DTW_AFTER, DDL_TRIGGER_ALTER_PACKAGE, name, {});
	}

	return modified;
}


bool CreateAlterPackageNode::executeAlterIndividualParameters(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction)
{
	AutoCacheRequest requestHandle(tdbb, drq_m_prm_pkg, DYN_REQUESTS);
	bool modified = false;

	// Converted FOR loop #6: FOR (REQUEST_HANDLE requestHandle TRANSACTION_HANDLE transaction) PKG IN RDB$PACKAGES
	jrd_req* handle = requestHandle;
	EXE_start(tdbb, handle, transaction);
	
	struct {
		TEXT schema_name[MAX_SQL_IDENTIFIER_LEN];
		TEXT package_name[MAX_SQL_IDENTIFIER_LEN];
	} pkg_input;
	
	strcpy(pkg_input.schema_name, name.schema.c_str());
	strcpy(pkg_input.package_name, name.object.c_str());
	
	EXE_send(tdbb, handle, 0, sizeof(pkg_input), reinterpret_cast<UCHAR*>(&pkg_input));

	struct {
		// Just a dummy struct for receive
		SSHORT dummy;
	} pkg_data;

	while (!EXE_receive(tdbb, handle, 1, sizeof(pkg_data), reinterpret_cast<UCHAR*>(&pkg_data)))
	{
		modified = true;

		executeDdlTrigger(tdbb, dsqlScratch, transaction, DTW_BEFORE, DDL_TRIGGER_ALTER_PACKAGE, name, {});

		// Converted MODIFY operation #2: MODIFY PKG
		AutoCacheRequest modify_request(tdbb, drq_modify_pkg_params, DYN_REQUESTS);
		jrd_req* modify_handle = modify_request;
		EXE_start(tdbb, modify_handle, transaction);
		
		struct {
			TEXT schema_name[MAX_SQL_IDENTIFIER_LEN];
			TEXT package_name[MAX_SQL_IDENTIFIER_LEN];
			SSHORT RDB$SQL_SECURITY;
			SSHORT sql_security_null;
		} modify_data;
		
		strcpy(modify_data.schema_name, name.schema.c_str());
		strcpy(modify_data.package_name, name.object.c_str());
		
		if (ssDefiner.has_value())
		{
			if (ssDefiner.value() != SqlSecurity::SS_DROP)
			{
				modify_data.sql_security_null = FALSE;
				modify_data.RDB$SQL_SECURITY = ssDefiner.value() == SqlSecurity::SS_DEFINER ? FB_TRUE : FB_FALSE;
			}
			else
				modify_data.sql_security_null = TRUE;
		}
		else
			modify_data.sql_security_null = TRUE;
		
		EXE_send(tdbb, modify_handle, 0, sizeof(modify_data), reinterpret_cast<UCHAR*>(&modify_data));
	}

	if (modified)
		executeDdlTrigger(tdbb, dsqlScratch, transaction, DTW_AFTER, DDL_TRIGGER_ALTER_PACKAGE, name, {});

	return modified;
}

void CreateAlterPackageNode::executeItems(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch,
	jrd_tra* transaction)
{
	for (unsigned i = 0; i < items->getCount(); ++i)
	{
		switch ((*items)[i].type)
		{
			case Item::FUNCTION:
				(*items)[i].function->packageOwner = owner;
				(*items)[i].function->executeDdl(tdbb, (*items)[i].dsqlScratch, transaction, true);
				break;

			case Item::PROCEDURE:
				(*items)[i].procedure->packageOwner = owner;
				(*items)[i].procedure->executeDdl(tdbb, (*items)[i].dsqlScratch, transaction, true);
				break;
		}
	}
}


//----------------------


string DropPackageNode::internalPrint(NodePrinter& printer) const
{
	DdlNode::internalPrint(printer);

	NODE_PRINT(printer, name);
	NODE_PRINT(printer, silent);

	return "DropPackageNode";
}


void DropPackageNode::checkPermission(thread_db* tdbb, jrd_tra* transaction)
{
	SCL_check_package(tdbb, name, SCL_drop);
}


void DropPackageNode::execute(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction)
{
	MemoryPool& pool = dsqlScratch->getPool();

	// run all statements under savepoint control
	AutoSavePoint savePoint(tdbb, transaction);

	bool found = false;
	AutoCacheRequest requestHandle(tdbb, drq_e_pkg, DYN_REQUESTS);

	// Converted FOR loop #7: FOR (REQUEST_HANDLE requestHandle TRANSACTION_HANDLE transaction) PKG IN RDB$PACKAGES
	jrd_req* handle = requestHandle;
	EXE_start(tdbb, handle, transaction);
	
	struct {
		TEXT schema_name[MAX_SQL_IDENTIFIER_LEN];
		TEXT package_name[MAX_SQL_IDENTIFIER_LEN];
	} pkg_input;
	
	strcpy(pkg_input.schema_name, name.schema.c_str());
	strcpy(pkg_input.package_name, name.object.c_str());
	
	EXE_send(tdbb, handle, 0, sizeof(pkg_input), reinterpret_cast<UCHAR*>(&pkg_input));

	struct {
		TEXT RDB$SECURITY_CLASS[MAX_SQL_IDENTIFIER_LEN];
		SSHORT security_class_null;
	} pkg_data;

	while (!EXE_receive(tdbb, handle, 1, sizeof(pkg_data), reinterpret_cast<UCHAR*>(&pkg_data)))
	{
		found = true;

		executeDdlTrigger(tdbb, dsqlScratch, transaction, DTW_BEFORE, DDL_TRIGGER_DROP_PACKAGE, name, {});

		// Converted ERASE operation #1: ERASE PKG;
		AutoCacheRequest erase_request(tdbb, drq_erase_pkg, DYN_REQUESTS);
		jrd_req* erase_handle = erase_request;
		EXE_start(tdbb, erase_handle, transaction);
		
		struct {
			TEXT schema_name[MAX_SQL_IDENTIFIER_LEN];
			TEXT package_name[MAX_SQL_IDENTIFIER_LEN];
		} erase_data;
		
		strcpy(erase_data.schema_name, name.schema.c_str());
		strcpy(erase_data.package_name, name.object.c_str());
		
		EXE_send(tdbb, erase_handle, 0, sizeof(erase_data), reinterpret_cast<UCHAR*>(&erase_data));

		if (!pkg_data.security_class_null)
			deleteSecurityClass(tdbb, transaction, pkg_data.RDB$SECURITY_CLASS);

		dsc schemaDesc, nameDesc;
		schemaDesc.makeText(name.schema.length(), ttype_metadata, (UCHAR*) const_cast<char*>(name.schema.c_str()));
		nameDesc.makeText(name.object.length(), ttype_metadata, (UCHAR*) const_cast<char*>(name.object.c_str()));
		DFW_post_work(transaction, dfw_drop_package_header, &nameDesc, &schemaDesc, 0);
	}

	if (!found && !silent)
	{
		status_exception::raise(
			Arg::Gds(isc_no_meta_update) <<
			Arg::Gds(isc_dyn_package_not_found) << name.toQuotedString());
	}

	SortedObjectsArray<Signature> existingFuncs(pool);
	SortedObjectsArray<Signature> existingProcs(pool);
	collectPackagedItems(tdbb, transaction, name, existingFuncs, existingProcs, false);

	for (SortedObjectsArray<Signature>::iterator i = existingFuncs.begin();
		 i != existingFuncs.end(); ++i)
	{
		DropFunctionNode dropNode(pool, QualifiedName(i->name, name.schema, name.object));
		dropNode.dsqlPass(dsqlScratch);
		dropNode.executeDdl(tdbb, dsqlScratch, transaction, true);
	}

	for (SortedObjectsArray<Signature>::iterator i = existingProcs.begin();
		 i != existingProcs.end(); ++i)
	{
		DropProcedureNode dropNode(pool, QualifiedName(i->name, name.schema, name.object));
		dropNode.dsqlPass(dsqlScratch);
		dropNode.executeDdl(tdbb, dsqlScratch, transaction, true);
	}

	requestHandle.reset(tdbb, drq_e_pkg_prv, DYN_REQUESTS);

	// Converted FOR loop #8: FOR (REQUEST_HANDLE requestHandle TRANSACTION_HANDLE transaction) PRIV IN RDB$USER_PRIVILEGES
	handle = requestHandle;
	EXE_start(tdbb, handle, transaction);
	
	struct {
		TEXT schema_name[MAX_SQL_IDENTIFIER_LEN];
		TEXT package_name[MAX_SQL_IDENTIFIER_LEN];
	} priv_input;
	
	strcpy(priv_input.schema_name, name.schema.c_str());
	strcpy(priv_input.package_name, name.object.c_str());
	
	EXE_send(tdbb, handle, 0, sizeof(priv_input), reinterpret_cast<UCHAR*>(&priv_input));

	struct {
		// Just a dummy struct for receive
		SSHORT dummy;
	} priv_data;

	while (!EXE_receive(tdbb, handle, 1, sizeof(priv_data), reinterpret_cast<UCHAR*>(&priv_data)))
	{
		// Converted ERASE operation #2: ERASE PRIV;
		AutoCacheRequest erase_priv_request(tdbb, drq_erase_pkg_priv, DYN_REQUESTS);
		jrd_req* erase_priv_handle = erase_priv_request;
		EXE_start(tdbb, erase_priv_handle, transaction);
		
		EXE_send(tdbb, erase_priv_handle, 0, sizeof(priv_input), reinterpret_cast<UCHAR*>(&priv_input));
	}

	if (found)
		executeDdlTrigger(tdbb, dsqlScratch, transaction, DTW_AFTER, DDL_TRIGGER_DROP_PACKAGE, name, {});

	savePoint.release();	// everything is ok
}


//----------------------


string CreatePackageBodyNode::internalPrint(NodePrinter& printer) const
{
	DdlNode::internalPrint(printer);

	NODE_PRINT(printer, name);
	NODE_PRINT(printer, source);
	//// FIXME-PRINT: NODE_PRINT(printer, declaredItems);
	//// FIXME-PRINT: NODE_PRINT(printer, items);

	return "CreatePackageBodyNode";
}


DdlNode* CreatePackageBodyNode::dsqlPass(DsqlCompilerScratch* dsqlScratch)
{
	dsqlScratch->qualifyExistingName(name, obj_package_header);
	protectSystemSchema(name.schema, obj_package_header);
	dsqlScratch->ddlSchema = name.schema;

	MemoryPool& pool = dsqlScratch->getPool();

	source.ltrim("\n\r\t ");

	// process declaredItems and items
	Array<CreateAlterPackageNode::Item>* arrays[] = {declaredItems, items};
	SortedArray<MetaName> functionNames[FB_NELEM(arrays)];
	SortedArray<MetaName> procedureNames[FB_NELEM(arrays)];

	for (unsigned i = 0; i < FB_NELEM(arrays); ++i)
	{
		if (!arrays[i])
			continue;

		for (unsigned j = 0; j < arrays[i]->getCount(); ++j)
		{
			DdlNode* ddlNode;

			switch ((*arrays[i])[j].type)
			{
				case CreateAlterPackageNode::Item::FUNCTION:
				{
					CreateAlterFunctionNode* const fun = (*arrays[i])[j].function;
					ddlNode = fun;

					if (functionNames[i].exist(fun->name.object))
					{
						status_exception::raise(
							Arg::Gds(isc_no_meta_update) <<
							Arg::Gds(isc_dyn_duplicate_package_item) <<
								Arg::Str("FUNCTION") << fun->name.object.toQuotedString());
					}

					functionNames[i].add(fun->name.object);

					fun->name.schema = name.schema;
					fun->name.package = name.object;
					fun->create = true;

					if (arrays[i] == items)
						fun->alter = true;

					break;
				}

				case CreateAlterPackageNode::Item::PROCEDURE:
				{
					CreateAlterProcedureNode* const proc = (*arrays[i])[j].procedure;
					ddlNode = proc;

					if (procedureNames[i].exist(proc->name.object))
					{
						status_exception::raise(
							Arg::Gds(isc_no_meta_update) <<
							Arg::Gds(isc_dyn_duplicate_package_item) <<
								Arg::Str("PROCEDURE") << proc->name.object.toQuotedString());
					}

					procedureNames[i].add(proc->name.object);

					proc->name.schema = name.schema;
					proc->name.package = name.object;
					proc->create = true;

					if (arrays[i] == items)
						proc->alter = true;

					break;
				}

				default:
					fb_assert(false);
			}

			auto itemStatement = FB_NEW_POOL(pool) DsqlDdlStatement(pool, dsqlScratch->getAttachment(), ddlNode);

			auto itemScratch = (*arrays[i])[j].dsqlScratch =
				FB_NEW_POOL(pool) DsqlCompilerScratch(pool, dsqlScratch->getAttachment(),
					dsqlScratch->getTransaction(), itemStatement);

			itemScratch->ddlSchema = name.schema;
			itemScratch->clientDialect = dsqlScratch->clientDialect;
			itemScratch->flags |= DsqlCompilerScratch::FLAG_DDL;
			itemScratch->package = name;

			if (itemScratch->clientDialect > SQL_DIALECT_V5)
				itemStatement->setBlrVersion(5);
			else
				itemStatement->setBlrVersion(4);

			ddlNode->dsqlPass(itemScratch);
		}
	}

	return DdlNode::dsqlPass(dsqlScratch);
}


void CreatePackageBodyNode::checkPermission(thread_db* tdbb, jrd_tra* transaction)
{
	SCL_check_create_access(tdbb, obj_packages, name.schema);
}


void CreatePackageBodyNode::execute(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction)
{
	MemoryPool& pool = dsqlScratch->getPool();
	Attachment* attachment = transaction->getAttachment();

	// run all statements under savepoint control
	AutoSavePoint savePoint(tdbb, transaction);

	AutoCacheRequest requestHandle(tdbb, drq_m_pkg_body, DYN_REQUESTS);
	bool modified = false;

	// Converted FOR loop #9: FOR (REQUEST_HANDLE requestHandle TRANSACTION_HANDLE transaction) PKG IN RDB$PACKAGES
	jrd_req* handle = requestHandle;
	EXE_start(tdbb, handle, transaction);
	
	struct {
		TEXT schema_name[MAX_SQL_IDENTIFIER_LEN];
		TEXT package_name[MAX_SQL_IDENTIFIER_LEN];
	} pkg_input;
	
	strcpy(pkg_input.schema_name, name.schema.c_str());
	strcpy(pkg_input.package_name, name.object.c_str());
	
	EXE_send(tdbb, handle, 0, sizeof(pkg_input), reinterpret_cast<UCHAR*>(&pkg_input));

	struct {
		TEXT RDB$OWNER_NAME[MAX_SQL_IDENTIFIER_LEN];
		SSHORT RDB$VALID_BODY_FLAG;
		SSHORT valid_body_flag_null;
	} pkg_data;

	while (!EXE_receive(tdbb, handle, 1, sizeof(pkg_data), reinterpret_cast<UCHAR*>(&pkg_data)))
	{
		if (!pkg_data.valid_body_flag_null && pkg_data.RDB$VALID_BODY_FLAG != 0)
		{
			if (createIfNotExistsOnly)
				return;

			status_exception::raise(
				Arg::Gds(isc_no_meta_update) <<
				Arg::Gds(isc_dyn_package_body_exists) << name.toQuotedString());
		}

		executeDdlTrigger(tdbb, dsqlScratch, transaction, DTW_BEFORE, DDL_TRIGGER_CREATE_PACKAGE_BODY, name, {});

		// Converted MODIFY operation #3: MODIFY PKG
		AutoCacheRequest modify_request(tdbb, drq_modify_pkg_body, DYN_REQUESTS);
		jrd_req* modify_handle = modify_request;
		EXE_start(tdbb, modify_handle, transaction);
		
		struct {
			TEXT schema_name[MAX_SQL_IDENTIFIER_LEN];
			TEXT package_name[MAX_SQL_IDENTIFIER_LEN];
			SSHORT RDB$VALID_BODY_FLAG;
			ISC_QUAD RDB$PACKAGE_BODY_SOURCE;
			SSHORT valid_body_flag_null;
			SSHORT package_body_source_null;
		} modify_data;
		
		strcpy(modify_data.schema_name, name.schema.c_str());
		strcpy(modify_data.package_name, name.object.c_str());
		
		modify_data.valid_body_flag_null = FALSE;
		modify_data.RDB$VALID_BODY_FLAG = TRUE;

		modify_data.package_body_source_null = FALSE;
		attachment->storeMetaDataBlob(tdbb, transaction, &modify_data.RDB$PACKAGE_BODY_SOURCE, source);
		
		EXE_send(tdbb, modify_handle, 0, sizeof(modify_data), reinterpret_cast<UCHAR*>(&modify_data));

		modified = true;

		owner = pkg_data.RDB$OWNER_NAME;
	}

	if (!modified)
	{
		status_exception::raise(
			Arg::Gds(isc_no_meta_update) <<
			Arg::Gds(isc_dyn_package_not_found) << name.toQuotedString());
	}

	SortedObjectsArray<Signature> headerFuncs(pool);
	SortedObjectsArray<Signature> headerProcs(pool);
	collectPackagedItems(tdbb, transaction, name, headerFuncs, headerProcs, false);

	SortedObjectsArray<Signature> existingFuncs(pool);
	SortedObjectsArray<Signature> existingProcs(pool);

	// process declaredItems and items
	Array<CreateAlterPackageNode::Item>* arrays[] = {declaredItems, items};

	for (unsigned i = 0; i < FB_NELEM(arrays); ++i)
	{
		if (!arrays[i])
			continue;

		if (arrays[i] == items)
		{
			existingFuncs.clear();
			existingProcs.clear();
		}

		collectPackagedItems(tdbb, transaction, name, existingFuncs, existingProcs, true);

		for (unsigned j = 0; j < arrays[i]->getCount(); ++j)
		{
			CreateAlterPackageNode::Item& elem = (*arrays[i])[j];

			switch (elem.type)
			{
				case CreateAlterPackageNode::Item::FUNCTION:
				{
					CreateAlterFunctionNode* func = elem.function;

					if (arrays[i] == items)
						func->privateScope = !headerFuncs.exist(Signature(func->name.object));
					else if (existingFuncs.exist(Signature(func->name.object)))
					{
						status_exception::raise(
							Arg::Gds(isc_no_meta_update) <<
							Arg::Gds(isc_dyn_duplicate_package_item) <<
								Arg::Str("FUNCTION") << func->name.toQuotedString());
					}

					func->packageOwner = owner;
					func->preserveDefaults =
						existingFuncs.exist(Signature(func->name.object)) && arrays[i] == items;
					func->executeDdl(tdbb, elem.dsqlScratch, transaction, true);
					break;
				}

				case CreateAlterPackageNode::Item::PROCEDURE:
				{
					CreateAlterProcedureNode* proc = elem.procedure;

					if (arrays[i] == items)
						proc->privateScope = !headerProcs.exist(Signature(proc->name.object));
					else if (existingProcs.exist(Signature(proc->name.object)))
					{
						status_exception::raise(
							Arg::Gds(isc_no_meta_update) <<
							Arg::Gds(isc_dyn_duplicate_package_item) <<
								Arg::Str("PROCEDURE") << proc->name.toQuotedString());
					}

					proc->packageOwner = owner;
					proc->preserveDefaults =
						existingProcs.exist(Signature(proc->name.object)) && arrays[i] == items;
					proc->executeDdl(tdbb, elem.dsqlScratch, transaction, true);
					break;
				}
			}
		}
	}

	SortedObjectsArray<Signature> newFuncs(pool);
	SortedObjectsArray<Signature> newProcs(pool);
	collectPackagedItems(tdbb, transaction, name, newFuncs, newProcs, true);

	for (SortedObjectsArray<Signature>::iterator i = existingFuncs.begin();
		 i != existingFuncs.end(); ++i)
	{
		FB_SIZE_T pos;
		bool found = newFuncs.find(Signature(pool, i->name), pos);

		if (!found || !newFuncs[pos].defined)
		{
			status_exception::raise(
				Arg::Gds(isc_dyn_funcnotdef_package) << i->name << name.toQuotedString());
		}
		else if (newFuncs[pos] != *i)
		{
			status_exception::raise(
				Arg::Gds(isc_dyn_funcsignat_package) << i->name << name.toQuotedString());
		}
	}

	for (SortedObjectsArray<Signature>::iterator i = existingProcs.begin();
		 i != existingProcs.end(); ++i)
	{
		FB_SIZE_T pos;
		bool found = newProcs.find(Signature(pool, i->name), pos);

		if (!found || !newProcs[pos].defined)
		{
			status_exception::raise(
				Arg::Gds(isc_dyn_procnotdef_package) << i->name << name.toQuotedString());
		}
		else if (newProcs[pos] != *i)
		{
			status_exception::raise(
				Arg::Gds(isc_dyn_procsignat_package) << i->name << name.toQuotedString());
		}
	}

	executeDdlTrigger(tdbb, dsqlScratch, transaction, DTW_AFTER, DDL_TRIGGER_CREATE_PACKAGE_BODY, name, {});

	savePoint.release();	// everything is ok
}


//----------------------


string DropPackageBodyNode::internalPrint(NodePrinter& printer) const
{
	DdlNode::internalPrint(printer);

	NODE_PRINT(printer, name);
	NODE_PRINT(printer, silent);

	return "DropPackageBodyNode";
}


void DropPackageBodyNode::checkPermission(thread_db* tdbb, jrd_tra* transaction)
{
	SCL_check_package(tdbb, name, SCL_drop);
}


void DropPackageBodyNode::execute(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch,
	jrd_tra* transaction)
{
	MemoryPool& pool = dsqlScratch->getPool();

	// run all statements under savepoint control
	AutoSavePoint savePoint(tdbb, transaction);

	bool found = false;
	AutoCacheRequest requestHandle(tdbb, drq_m_pkg_body2, DYN_REQUESTS);

	// Converted FOR loop #10: FOR (REQUEST_HANDLE requestHandle TRANSACTION_HANDLE transaction) PKG IN RDB$PACKAGES
	jrd_req* handle = requestHandle;
	EXE_start(tdbb, handle, transaction);
	
	struct {
		TEXT schema_name[MAX_SQL_IDENTIFIER_LEN];
		TEXT package_name[MAX_SQL_IDENTIFIER_LEN];
	} pkg_input;
	
	strcpy(pkg_input.schema_name, name.schema.c_str());
	strcpy(pkg_input.package_name, name.object.c_str());
	
	EXE_send(tdbb, handle, 0, sizeof(pkg_input), reinterpret_cast<UCHAR*>(&pkg_input));

	struct {
		// Just a dummy struct for receive
		SSHORT dummy;
	} pkg_data;

	while (!EXE_receive(tdbb, handle, 1, sizeof(pkg_data), reinterpret_cast<UCHAR*>(&pkg_data)))
	{
		found = true;

		executeDdlTrigger(tdbb, dsqlScratch, transaction, DTW_BEFORE, DDL_TRIGGER_DROP_PACKAGE_BODY, name, {});

		// Converted MODIFY operation #4: MODIFY PKG
		AutoCacheRequest modify_request(tdbb, drq_modify_drop_pkg_body, DYN_REQUESTS);
		jrd_req* modify_handle = modify_request;
		EXE_start(tdbb, modify_handle, transaction);
		
		struct {
			TEXT schema_name[MAX_SQL_IDENTIFIER_LEN];
			TEXT package_name[MAX_SQL_IDENTIFIER_LEN];
			SSHORT valid_body_flag_null;
			SSHORT package_body_source_null;
		} modify_data;
		
		strcpy(modify_data.schema_name, name.schema.c_str());
		strcpy(modify_data.package_name, name.object.c_str());
		
		modify_data.valid_body_flag_null = TRUE;
		modify_data.package_body_source_null = TRUE;

		EXE_send(tdbb, modify_handle, 0, sizeof(modify_data), reinterpret_cast<UCHAR*>(&modify_data));

		dsc schemaDesc, nameDesc;
		schemaDesc.makeText(name.schema.length(), ttype_metadata, (UCHAR*) const_cast<char*>(name.schema.c_str()));
		nameDesc.makeText(name.object.length(), ttype_metadata, (UCHAR*) const_cast<char*>(name.object.c_str()));
		DFW_post_work(transaction, dfw_drop_package_body, &nameDesc, &schemaDesc, 0);
	}

	if (!found)
	{
		if (silent)
		{
			savePoint.release();
			return;
		}

		status_exception::raise(
			Arg::Gds(isc_no_meta_update) <<
			Arg::Gds(isc_dyn_package_not_found) << name.toQuotedString());
	}

	requestHandle.reset(tdbb, drq_m_pkg_fun, DYN_REQUESTS);

	// Converted FOR loop #11: FOR (REQUEST_HANDLE requestHandle TRANSACTION_HANDLE transaction) FUN IN RDB$FUNCTIONS
	handle = requestHandle;
	EXE_start(tdbb, handle, transaction);
	
	struct {
		TEXT schema_name[MAX_SQL_IDENTIFIER_LEN];
		TEXT package_name[MAX_SQL_IDENTIFIER_LEN];
	} fun_input;
	
	strcpy(fun_input.schema_name, name.schema.c_str());
	strcpy(fun_input.package_name, name.object.c_str());
	
	EXE_send(tdbb, handle, 0, sizeof(fun_input), reinterpret_cast<UCHAR*>(&fun_input));

	struct {
		TEXT RDB$FUNCTION_NAME[MAX_SQL_IDENTIFIER_LEN];
		SSHORT RDB$PRIVATE_FLAG;
		SSHORT private_flag_null;
	} fun_data;

	while (!EXE_receive(tdbb, handle, 1, sizeof(fun_data), reinterpret_cast<UCHAR*>(&fun_data)))
	{
		if (!fun_data.private_flag_null && fun_data.RDB$PRIVATE_FLAG != 0)
		{
			DropFunctionNode dropNode(pool, QualifiedName(fun_data.RDB$FUNCTION_NAME, name.schema, name.object));
			dropNode.dsqlPass(dsqlScratch);
			dropNode.executeDdl(tdbb, dsqlScratch, transaction, true);
		}
		else
		{
			// Converted MODIFY operation #5: MODIFY FUN
			AutoCacheRequest modify_fun_request(tdbb, drq_modify_pkg_fun, DYN_REQUESTS);
			jrd_req* modify_fun_handle = modify_fun_request;
			EXE_start(tdbb, modify_fun_handle, transaction);
			
			struct {
				TEXT schema_name[MAX_SQL_IDENTIFIER_LEN];
				TEXT package_name[MAX_SQL_IDENTIFIER_LEN];
				TEXT function_name[MAX_SQL_IDENTIFIER_LEN];
				SSHORT function_type_null;
				SSHORT function_blr_null;
				SSHORT debug_info_null;
				SSHORT module_name_null;
				SSHORT engine_name_null;
				SSHORT entrypoint_null;
			} modify_fun_data;
			
			strcpy(modify_fun_data.schema_name, name.schema.c_str());
			strcpy(modify_fun_data.package_name, name.object.c_str());
			strcpy(modify_fun_data.function_name, fun_data.RDB$FUNCTION_NAME);
			
			modify_fun_data.function_type_null = TRUE;
			modify_fun_data.function_blr_null = TRUE;
			modify_fun_data.debug_info_null = TRUE;
			modify_fun_data.module_name_null = TRUE;
			modify_fun_data.engine_name_null = TRUE;
			modify_fun_data.entrypoint_null = TRUE;
			
			EXE_send(tdbb, modify_fun_handle, 0, sizeof(modify_fun_data), reinterpret_cast<UCHAR*>(&modify_fun_data));
		}
	}

	requestHandle.reset(tdbb, drq_m_pkg_prc, DYN_REQUESTS);

	// Converted FOR loop #12: FOR (REQUEST_HANDLE requestHandle TRANSACTION_HANDLE transaction) PRC IN RDB$PROCEDURES
	handle = requestHandle;
	EXE_start(tdbb, handle, transaction);
	
	struct {
		TEXT schema_name[MAX_SQL_IDENTIFIER_LEN];
		TEXT package_name[MAX_SQL_IDENTIFIER_LEN];
	} prc_input;
	
	strcpy(prc_input.schema_name, name.schema.c_str());
	strcpy(prc_input.package_name, name.object.c_str());
	
	EXE_send(tdbb, handle, 0, sizeof(prc_input), reinterpret_cast<UCHAR*>(&prc_input));

	struct {
		TEXT RDB$PROCEDURE_NAME[MAX_SQL_IDENTIFIER_LEN];
		SSHORT RDB$PRIVATE_FLAG;
		SSHORT private_flag_null;
	} prc_data;

	while (!EXE_receive(tdbb, handle, 1, sizeof(prc_data), reinterpret_cast<UCHAR*>(&prc_data)))
	{
		if (!prc_data.private_flag_null && prc_data.RDB$PRIVATE_FLAG != 0)
		{
			DropProcedureNode dropNode(pool, QualifiedName(prc_data.RDB$PROCEDURE_NAME, name.schema, name.object));
			dropNode.dsqlPass(dsqlScratch);
			dropNode.executeDdl(tdbb, dsqlScratch, transaction, true);
		}
		else
		{
			// Converted MODIFY operation #6: MODIFY PRC
			AutoCacheRequest modify_prc_request(tdbb, drq_modify_pkg_prc, DYN_REQUESTS);
			jrd_req* modify_prc_handle = modify_prc_request;
			EXE_start(tdbb, modify_prc_handle, transaction);
			
			struct {
				TEXT schema_name[MAX_SQL_IDENTIFIER_LEN];
				TEXT package_name[MAX_SQL_IDENTIFIER_LEN];
				TEXT procedure_name[MAX_SQL_IDENTIFIER_LEN];
				SSHORT procedure_type_null;
				SSHORT procedure_blr_null;
				SSHORT debug_info_null;
				SSHORT engine_name_null;
				SSHORT entrypoint_null;
			} modify_prc_data;
			
			strcpy(modify_prc_data.schema_name, name.schema.c_str());
			strcpy(modify_prc_data.package_name, name.object.c_str());
			strcpy(modify_prc_data.procedure_name, prc_data.RDB$PROCEDURE_NAME);
			
			modify_prc_data.procedure_type_null = TRUE;
			modify_prc_data.procedure_blr_null = TRUE;
			modify_prc_data.debug_info_null = TRUE;
			modify_prc_data.engine_name_null = TRUE;
			modify_prc_data.entrypoint_null = TRUE;
			
			EXE_send(tdbb, modify_prc_handle, 0, sizeof(modify_prc_data), reinterpret_cast<UCHAR*>(&modify_prc_data));
		}
	}

	executeDdlTrigger(tdbb, dsqlScratch, transaction, DTW_AFTER, DDL_TRIGGER_DROP_PACKAGE_BODY, name, {});

	savePoint.release();	// everything is ok
}


}	// namespace Jrd