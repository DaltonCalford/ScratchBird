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
// RelationNode Class Implementation  
//----------------------

void RelationNode::execute(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch,
	jrd_tra* transaction)
{
	Attachment* const attachment = transaction->tra_attachment;
	Database* const dbb = tdbb->getDatabase();

	fb_assert(name.hasData());

	if (clauses & CLAUSE_temp_scope)
	{
		if (tempScope.scope == tss_transaction)
		{
			// Find a relation which is already created in this transaction
			for (const auto& tr : attachment->att_temp_relations)
			{
				if (tr.second == transaction->tra_number && tr.first == name)
				{
					status_exception::raise(
						Arg::Gds(isc_dyn_duplicate_table) << name.toQuotedString());
				}
			}
		}
	}

	executeAlter(tdbb, dsqlScratch, transaction, true);
	executeAlter(tdbb, dsqlScratch, transaction, false);
}

//----------------------
// CreateRelationNode Class Implementation  
//----------------------

void CreateRelationNode::execute(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch,
	jrd_tra* transaction)
{
	Attachment* const attachment = transaction->tra_attachment;
	Database* const dbb = tdbb->getDatabase();

	executeDdlTrigger(tdbb, dsqlScratch, transaction, DTW_BEFORE,
		(clauses & CLAUSE_temp_scope) ? DDL_TRIGGER_CREATE_TABLE : DDL_TRIGGER_CREATE_TABLE, name);

	// Convert FOR loop #20: Check if relation already exists
	AutoCacheRequest request(tdbb, drq_l_rel_info, DYN_REQUESTS);
	EXE_start(tdbb, request, transaction);
	EXE_send(tdbb, request, 0, name.schema.length(), name.schema.c_str());
	EXE_send(tdbb, request, 1, name.object.length(), name.object.c_str());

	struct RelationData {
		SSHORT relationId;
		SSHORT relationIdNull;
		SSHORT systemFlag;
		SSHORT systemFlagNull;
		char ownerName[MAX_SQL_IDENTIFIER_LEN];
		SSHORT ownerNameNull;
		SSHORT relationType;
		SSHORT relationTypeNull;
	} relationData;

	if (EXE_receive(tdbb, request, 2, sizeof(relationData), &relationData))
	{
		EXE_unwind(tdbb, request);

		if (!relationData.systemFlagNull && relationData.systemFlag &&
			!(attachment->att_flags & ATT_system))
		{
			status_exception::raise(
				Arg::Gds(isc_dyn_cannot_mod_sysrel) << name.toQuotedString());
		}

		status_exception::raise(
			Arg::Gds(isc_dyn_rel_exists) << name.toQuotedString());
	}
	EXE_unwind(tdbb, request);

	RelationNode::execute(tdbb, dsqlScratch, transaction);

	// Convert STORE operation #30: Store relation in RDB$RELATIONS
	AutoCacheRequest storeRequest(tdbb, drq_s_relations, DYN_REQUESTS);
	EXE_start(tdbb, storeRequest, transaction);

	struct RDB$RELATIONS_RECORD {
		char RDB$SCHEMA_NAME[MAX_SQL_IDENTIFIER_LEN];
		char RDB$RELATION_NAME[MAX_SQL_IDENTIFIER_LEN];
		SSHORT RDB$RELATION_ID;
		SSHORT RDB$RELATION_ID_NULL;
		char RDB$OWNER_NAME[MAX_SQL_IDENTIFIER_LEN];
		SSHORT RDB$OWNER_NAME_NULL;
		SSHORT RDB$SYSTEM_FLAG;
		SSHORT RDB$SYSTEM_FLAG_NULL;
		SSHORT RDB$RELATION_TYPE;
		SSHORT RDB$RELATION_TYPE_NULL;
		bid RDB$DESCRIPTION;
		SSHORT RDB$DESCRIPTION_NULL;
		bid RDB$VIEW_BLR;
		SSHORT RDB$VIEW_BLR_NULL;
		bid RDB$VIEW_SOURCE;
		SSHORT RDB$VIEW_SOURCE_NULL;
		SSHORT RDB$EXTERNAL_FILE_NULL;
		bid RDB$EXTERNAL_FILE;
		SSHORT RDB$SECURITY_CLASS_NULL;
		char RDB$SECURITY_CLASS[MAX_SQL_IDENTIFIER_LEN];
		SSHORT RDB$FLAGS_NULL;
		SSHORT RDB$FLAGS;
		SSHORT RDB$SQL_SECURITY_NULL;
		SSHORT RDB$SQL_SECURITY;
	} relationRecord;

	memset(&relationRecord, 0, sizeof(relationRecord));
	
	strcpy(relationRecord.RDB$SCHEMA_NAME, name.schema.c_str());
	strcpy(relationRecord.RDB$RELATION_NAME, name.object.c_str());
	
	relationRecord.RDB$RELATION_ID_NULL = FALSE;
	relationRecord.RDB$RELATION_ID = dbb->generateId();

	relationRecord.RDB$OWNER_NAME_NULL = FALSE;
	strcpy(relationRecord.RDB$OWNER_NAME, attachment->getUserName().c_str());

	relationRecord.RDB$SYSTEM_FLAG_NULL = FALSE;
	relationRecord.RDB$SYSTEM_FLAG = 0;

	if (clauses & CLAUSE_temp_scope)
	{
		relationRecord.RDB$RELATION_TYPE_NULL = FALSE;
		relationRecord.RDB$RELATION_TYPE = (tempScope.scope == tss_transaction) ? 
			REL_temporary : REL_global_temporary_preserve;
	}
	else
	{
		relationRecord.RDB$RELATION_TYPE_NULL = TRUE;
	}

	relationRecord.RDB$DESCRIPTION_NULL = TRUE;
	relationRecord.RDB$VIEW_BLR_NULL = TRUE;
	relationRecord.RDB$VIEW_SOURCE_NULL = TRUE;
	relationRecord.RDB$EXTERNAL_FILE_NULL = TRUE;
	relationRecord.RDB$SECURITY_CLASS_NULL = TRUE;
	relationRecord.RDB$FLAGS_NULL = TRUE;
	relationRecord.RDB$SQL_SECURITY_NULL = TRUE;

	EXE_send(tdbb, storeRequest, 0, sizeof(relationRecord), &relationRecord);
	EXE_unwind(tdbb, storeRequest);

	if (clauses & CLAUSE_temp_scope)
	{
		attachment->att_temp_relations[name] = transaction->tra_number;
	}

	saveRelation(tdbb, dsqlScratch, name, false, true);

	executeDdlTrigger(tdbb, dsqlScratch, transaction, DTW_AFTER,
		(clauses & CLAUSE_temp_scope) ? DDL_TRIGGER_CREATE_TABLE : DDL_TRIGGER_CREATE_TABLE, name);
}

//----------------------
// AlterRelationNode Class Implementation  
//----------------------

void AlterRelationNode::execute(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch,
	jrd_tra* transaction)
{
	Attachment* const attachment = transaction->tra_attachment;

	executeDdlTrigger(tdbb, dsqlScratch, transaction, DTW_BEFORE, DDL_TRIGGER_ALTER_TABLE, name);

	// Convert FOR loop #25: Find relation to alter  
	AutoCacheRequest request(tdbb, drq_l_rel_info2, DYN_REQUESTS);
	EXE_start(tdbb, request, transaction);
	EXE_send(tdbb, request, 0, name.schema.length(), name.schema.c_str());
	EXE_send(tdbb, request, 1, name.object.length(), name.object.c_str());

	struct RelationAlterData {
		SSHORT relationId;
		SSHORT relationIdNull;
		SSHORT systemFlag;
		SSHORT systemFlagNull;
		char ownerName[MAX_SQL_IDENTIFIER_LEN];
		SSHORT ownerNameNull;
		SSHORT relationType;
		SSHORT relationTypeNull;
	} relationData;

	bool relationFound = false;
	if (EXE_receive(tdbb, request, 2, sizeof(relationData), &relationData))
	{
		relationFound = true;
		
		if (!relationData.systemFlagNull && relationData.systemFlag &&
			!(attachment->att_flags & ATT_system))
		{
			EXE_unwind(tdbb, request);
			status_exception::raise(
				Arg::Gds(isc_dyn_cannot_mod_sysrel) << name.toQuotedString());
		}
	}
	EXE_unwind(tdbb, request);

	if (!relationFound)
	{
		status_exception::raise(
			Arg::Gds(isc_dyn_rel_not_found) << name.toQuotedString());
	}

	RelationNode::execute(tdbb, dsqlScratch, transaction);

	executeDdlTrigger(tdbb, dsqlScratch, transaction, DTW_AFTER, DDL_TRIGGER_ALTER_TABLE, name);
}

//----------------------
// DropRelationNode Class Implementation  
//----------------------

void DropRelationNode::execute(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch,
	jrd_tra* transaction)
{
	Attachment* const attachment = transaction->tra_attachment;

	executeDdlTrigger(tdbb, dsqlScratch, transaction, DTW_BEFORE, DDL_TRIGGER_DROP_TABLE, name);

	// Convert FOR loop #26: Find relation to drop
	AutoCacheRequest request(tdbb, drq_l_rel_info3, DYN_REQUESTS);
	EXE_start(tdbb, request, transaction);
	EXE_send(tdbb, request, 0, name.schema.length(), name.schema.c_str());
	EXE_send(tdbb, request, 1, name.object.length(), name.object.c_str());

	struct RelationDropData {
		SSHORT relationId;
		SSHORT relationIdNull;
		SSHORT systemFlag;
		SSHORT systemFlagNull;
		char ownerName[MAX_SQL_IDENTIFIER_LEN];
		SSHORT ownerNameNull;
		SSHORT relationType;
		SSHORT relationTypeNull;
	} relationData;

	bool relationFound = false;
	if (EXE_receive(tdbb, request, 2, sizeof(relationData), &relationData))
	{
		relationFound = true;
		
		if (!relationData.systemFlagNull && relationData.systemFlag &&
			!(attachment->att_flags & ATT_system))
		{
			EXE_unwind(tdbb, request);
			status_exception::raise(
				Arg::Gds(isc_dyn_cannot_mod_sysrel) << name.toQuotedString());
		}
	}
	EXE_unwind(tdbb, request);

	if (!relationFound)
	{
		if (!silent)
		{
			status_exception::raise(
				Arg::Gds(isc_dyn_rel_not_found) << name.toQuotedString());
		}
		return;
	}

	// Convert DELETE operation #15: Delete relation from RDB$RELATIONS
	AutoCacheRequest deleteRequest(tdbb, drq_e_relation, DYN_REQUESTS);
	EXE_start(tdbb, deleteRequest, transaction);
	EXE_send(tdbb, deleteRequest, 0, name.schema.length(), name.schema.c_str());
	EXE_send(tdbb, deleteRequest, 1, name.object.length(), name.object.c_str());

	if (EXE_receive(tdbb, deleteRequest, 2, 0, NULL))
	{
		// Record found, delete it
		struct DeleteConfirm { char confirm; } deleteData;
		deleteData.confirm = 1;
		EXE_send(tdbb, deleteRequest, 3, sizeof(deleteData), &deleteData);
	}
	EXE_unwind(tdbb, deleteRequest);

	// Remove from temporary relations if it was temporary
	auto it = attachment->att_temp_relations.find(name);
	if (it != attachment->att_temp_relations.end())
		attachment->att_temp_relations.erase(it);

	executeDdlTrigger(tdbb, dsqlScratch, transaction, DTW_AFTER, DDL_TRIGGER_DROP_TABLE, name);
}

//----------------------
// CreateAlterViewNode Class Implementation  
//----------------------

void CreateAlterViewNode::execute(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch,
	jrd_tra* transaction)
{
	Attachment* const attachment = transaction->tra_attachment;

	const int triggerType = create ? DDL_TRIGGER_CREATE_VIEW : DDL_TRIGGER_ALTER_VIEW;
	executeDdlTrigger(tdbb, dsqlScratch, transaction, DTW_BEFORE, triggerType, name);

	if (create)
	{
		// Convert FOR loop #27: Check if view already exists
		AutoCacheRequest request(tdbb, drq_l_view_info, DYN_REQUESTS);
		EXE_start(tdbb, request, transaction);
		EXE_send(tdbb, request, 0, name.schema.length(), name.schema.c_str());
		EXE_send(tdbb, request, 1, name.object.length(), name.object.c_str());

		struct ViewCheckData {
			SSHORT systemFlag;
			SSHORT systemFlagNull;
		} viewData;

		if (EXE_receive(tdbb, request, 2, sizeof(viewData), &viewData))
		{
			EXE_unwind(tdbb, request);
			
			if (!viewData.systemFlagNull && viewData.systemFlag &&
				!(attachment->att_flags & ATT_system))
			{
				status_exception::raise(
					Arg::Gds(isc_dyn_cannot_mod_sysrel) << name.toQuotedString());
			}

			status_exception::raise(
				Arg::Gds(isc_dyn_view_exists) << name.toQuotedString());
		}
		EXE_unwind(tdbb, request);
	}

	RelationNode::execute(tdbb, dsqlScratch, transaction);

	// Convert STORE/MODIFY operation #31: Store or modify view in RDB$RELATIONS
	if (create)
	{
		AutoCacheRequest storeRequest(tdbb, drq_s_view_relation, DYN_REQUESTS);
		EXE_start(tdbb, storeRequest, transaction);

		struct RDB$VIEW_RELATIONS_RECORD {
			char RDB$SCHEMA_NAME[MAX_SQL_IDENTIFIER_LEN];
			char RDB$RELATION_NAME[MAX_SQL_IDENTIFIER_LEN];
			SSHORT RDB$RELATION_ID;
			SSHORT RDB$RELATION_ID_NULL;
			char RDB$OWNER_NAME[MAX_SQL_IDENTIFIER_LEN];
			SSHORT RDB$OWNER_NAME_NULL;
			SSHORT RDB$SYSTEM_FLAG;
			SSHORT RDB$SYSTEM_FLAG_NULL;
			SSHORT RDB$RELATION_TYPE;
			SSHORT RDB$RELATION_TYPE_NULL;
			bid RDB$VIEW_BLR;
			SSHORT RDB$VIEW_BLR_NULL;
			bid RDB$VIEW_SOURCE;
			SSHORT RDB$VIEW_SOURCE_NULL;
		} viewRecord;

		memset(&viewRecord, 0, sizeof(viewRecord));
		
		strcpy(viewRecord.RDB$SCHEMA_NAME, name.schema.c_str());
		strcpy(viewRecord.RDB$RELATION_NAME, name.object.c_str());
		
		viewRecord.RDB$RELATION_ID_NULL = FALSE;
		viewRecord.RDB$RELATION_ID = tdbb->getDatabase()->generateId();

		viewRecord.RDB$OWNER_NAME_NULL = FALSE;
		strcpy(viewRecord.RDB$OWNER_NAME, attachment->getUserName().c_str());

		viewRecord.RDB$SYSTEM_FLAG_NULL = FALSE;
		viewRecord.RDB$SYSTEM_FLAG = 0;

		viewRecord.RDB$RELATION_TYPE_NULL = FALSE;
		viewRecord.RDB$RELATION_TYPE = REL_view;

		if (querySpec)
		{
			viewRecord.RDB$VIEW_BLR_NULL = FALSE;
			// Store BLR here - would need to compile querySpec to BLR
			// This is a complex operation that needs BLR generation
		}

		if (withCheckOption)
		{
			// Handle WITH CHECK OPTION
		}

		EXE_send(tdbb, storeRequest, 0, sizeof(viewRecord), &viewRecord);
		EXE_unwind(tdbb, storeRequest);

		saveRelation(tdbb, dsqlScratch, name, true, true);
	}
	else
	{
		// Convert MODIFY operation for existing view
		AutoCacheRequest modifyRequest(tdbb, drq_m_view_relation, DYN_REQUESTS);
		EXE_start(tdbb, modifyRequest, transaction);
		EXE_send(tdbb, modifyRequest, 0, name.schema.length(), name.schema.c_str());
		EXE_send(tdbb, modifyRequest, 1, name.object.length(), name.object.c_str());

		struct ModifyViewData {
			bid RDB$VIEW_BLR;
			SSHORT RDB$VIEW_BLR_NULL;
			bid RDB$VIEW_SOURCE;
			SSHORT RDB$VIEW_SOURCE_NULL;
		} modifyData;

		if (EXE_receive(tdbb, modifyRequest, 2, sizeof(modifyData), &modifyData))
		{
			if (querySpec)
			{
				modifyData.RDB$VIEW_BLR_NULL = FALSE;
				// Update BLR here
			}

			EXE_send(tdbb, modifyRequest, 3, sizeof(modifyData), &modifyData);
		}
		EXE_unwind(tdbb, modifyRequest);

		saveRelation(tdbb, dsqlScratch, name, true, false);
	}

	executeDdlTrigger(tdbb, dsqlScratch, transaction, DTW_AFTER, triggerType, name);
}

} // namespace Jrd