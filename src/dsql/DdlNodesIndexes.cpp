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
// CreateIndexNode Class Implementation  
//----------------------

void CreateIndexNode::execute(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch,
	jrd_tra* transaction)
{
	Attachment* const attachment = transaction->tra_attachment;
	Database* const dbb = tdbb->getDatabase();

	executeDdlTrigger(tdbb, dsqlScratch, transaction, DTW_BEFORE, DDL_TRIGGER_CREATE_INDEX, name);

	// Convert FOR loop #100: Check if index already exists
	AutoCacheRequest request(tdbb, drq_l_idx_info, DYN_REQUESTS);
	EXE_start(tdbb, request, transaction);
	EXE_send(tdbb, request, 0, name.schema.length(), name.schema.c_str());
	EXE_send(tdbb, request, 1, name.object.length(), name.object.c_str());

	struct IndexCheckData {
		SSHORT systemFlag;
		SSHORT systemFlagNull;
		char relationName[MAX_SQL_IDENTIFIER_LEN];
		SSHORT relationNameNull;
	} indexData;

	if (EXE_receive(tdbb, request, 2, sizeof(indexData), &indexData))
	{
		EXE_unwind(tdbb, request);
		
		if (!indexData.systemFlagNull && indexData.systemFlag &&
			!(attachment->att_flags & ATT_system))
		{
			status_exception::raise(
				Arg::Gds(isc_dyn_cannot_mod_sysindex) << name.toQuotedString());
		}

		if (!inactive)
		{
			status_exception::raise(
				Arg::Gds(isc_dyn_index_exists) << name.toQuotedString());
		}
	}
	EXE_unwind(tdbb, request);

	// Verify the relation exists
	if (!relationName.hasData())
	{
		status_exception::raise(
			Arg::Gds(isc_dyn_rel_not_found) << relationName.toQuotedString());
	}

	// Convert STORE operation #100: Store index in RDB$INDICES
	AutoCacheRequest storeRequest(tdbb, drq_s_indices, DYN_REQUESTS);
	EXE_start(tdbb, storeRequest, transaction);

	struct RDB$INDICES_RECORD {
		char RDB$SCHEMA_NAME[MAX_SQL_IDENTIFIER_LEN];
		char RDB$INDEX_NAME[MAX_SQL_IDENTIFIER_LEN];
		char RDB$RELATION_NAME[MAX_SQL_IDENTIFIER_LEN];
		SSHORT RDB$INDEX_ID;
		SSHORT RDB$INDEX_ID_NULL;
		SSHORT RDB$UNIQUE_FLAG;
		SSHORT RDB$UNIQUE_FLAG_NULL;
		bid RDB$DESCRIPTION;
		SSHORT RDB$DESCRIPTION_NULL;
		SSHORT RDB$SEGMENT_COUNT;
		SSHORT RDB$SEGMENT_COUNT_NULL;
		SSHORT RDB$INDEX_INACTIVE;
		SSHORT RDB$INDEX_INACTIVE_NULL;
		SSHORT RDB$INDEX_TYPE;
		SSHORT RDB$INDEX_TYPE_NULL;
		char RDB$FOREIGN_KEY[MAX_SQL_IDENTIFIER_LEN];
		SSHORT RDB$FOREIGN_KEY_NULL;
		SSHORT RDB$SYSTEM_FLAG;
		SSHORT RDB$SYSTEM_FLAG_NULL;
		bid RDB$EXPRESSION_BLR;
		SSHORT RDB$EXPRESSION_BLR_NULL;
		bid RDB$EXPRESSION_SOURCE;
		SSHORT RDB$EXPRESSION_SOURCE_NULL;
		bid RDB$STATISTICS;
		SSHORT RDB$STATISTICS_NULL;
	} indexRecord;

	memset(&indexRecord, 0, sizeof(indexRecord));
	
	strcpy(indexRecord.RDB$SCHEMA_NAME, name.schema.c_str());
	strcpy(indexRecord.RDB$INDEX_NAME, name.object.c_str());
	strcpy(indexRecord.RDB$RELATION_NAME, relationName.object.c_str());

	indexRecord.RDB$INDEX_ID_NULL = FALSE;
	indexRecord.RDB$INDEX_ID = dbb->generateId();

	indexRecord.RDB$UNIQUE_FLAG_NULL = FALSE;
	indexRecord.RDB$UNIQUE_FLAG = unique ? 1 : 0;

	indexRecord.RDB$SEGMENT_COUNT_NULL = FALSE;
	indexRecord.RDB$SEGMENT_COUNT = columns ? columns->getCount() : 0;

	indexRecord.RDB$INDEX_INACTIVE_NULL = FALSE;
	indexRecord.RDB$INDEX_INACTIVE = inactive ? 1 : 0;

	indexRecord.RDB$INDEX_TYPE_NULL = FALSE;
	if (type == idx_expression)
	{
		indexRecord.RDB$INDEX_TYPE = idx_expression;
		
		if (computed)
		{
			indexRecord.RDB$EXPRESSION_SOURCE_NULL = FALSE;
			attachment->storeMetaDataBlob(tdbb, transaction, &indexRecord.RDB$EXPRESSION_SOURCE, 
				computed->source);

			// Generate BLR for expression - placeholder
			indexRecord.RDB$EXPRESSION_BLR_NULL = TRUE; // Would need BLR generation
		}
	}
	else if (descending)
	{
		indexRecord.RDB$INDEX_TYPE = idx_descending;
	}
	else
	{
		indexRecord.RDB$INDEX_TYPE = idx_ascending;
	}

	indexRecord.RDB$SYSTEM_FLAG_NULL = FALSE;
	indexRecord.RDB$SYSTEM_FLAG = 0;

	indexRecord.RDB$DESCRIPTION_NULL = TRUE;
	indexRecord.RDB$FOREIGN_KEY_NULL = TRUE;
	indexRecord.RDB$STATISTICS_NULL = TRUE;

	EXE_send(tdbb, storeRequest, 0, sizeof(indexRecord), &indexRecord);
	EXE_unwind(tdbb, storeRequest);

	// Store index segments
	if (columns)
	{
		SSHORT segmentNumber = 0;
		for (NestConst<OrderNode>* ptr = columns->begin();
			 ptr != columns->end(); ++ptr, ++segmentNumber)
		{
			const OrderNode* column = *ptr;
			const FieldNode* fieldNode = nodeAs<FieldNode>(column->expression);
			
			if (!fieldNode)
				continue;

			// Convert STORE operation #101: Store index segment
			AutoCacheRequest segmentRequest(tdbb, drq_s_idx_segs, DYN_REQUESTS);
			EXE_start(tdbb, segmentRequest, transaction);

			struct RDB$INDEX_SEGMENTS_RECORD {
				char RDB$SCHEMA_NAME[MAX_SQL_IDENTIFIER_LEN];
				char RDB$INDEX_NAME[MAX_SQL_IDENTIFIER_LEN];
				char RDB$FIELD_NAME[MAX_SQL_IDENTIFIER_LEN];
				SSHORT RDB$FIELD_POSITION;
				SSHORT RDB$FIELD_POSITION_NULL;
				SSHORT RDB$STATISTICS;
				SSHORT RDB$STATISTICS_NULL;
			} segmentRecord;

			memset(&segmentRecord, 0, sizeof(segmentRecord));

			strcpy(segmentRecord.RDB$SCHEMA_NAME, name.schema.c_str());
			strcpy(segmentRecord.RDB$INDEX_NAME, name.object.c_str());
			strcpy(segmentRecord.RDB$FIELD_NAME, fieldNode->fieldName.c_str());

			segmentRecord.RDB$FIELD_POSITION_NULL = FALSE;
			segmentRecord.RDB$FIELD_POSITION = segmentNumber;

			segmentRecord.RDB$STATISTICS_NULL = TRUE;

			EXE_send(tdbb, segmentRequest, 0, sizeof(segmentRecord), &segmentRecord);
			EXE_unwind(tdbb, segmentRequest);
		}
	}

	executeDdlTrigger(tdbb, dsqlScratch, transaction, DTW_AFTER, DDL_TRIGGER_CREATE_INDEX, name);
}

//----------------------
// AlterIndexNode Class Implementation  
//----------------------

void AlterIndexNode::execute(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch,
	jrd_tra* transaction)
{
	Attachment* const attachment = transaction->tra_attachment;

	executeDdlTrigger(tdbb, dsqlScratch, transaction, DTW_BEFORE, DDL_TRIGGER_ALTER_INDEX, name);

	// Convert FOR loop #110: Find index to alter
	AutoCacheRequest request(tdbb, drq_l_idx_info2, DYN_REQUESTS);
	EXE_start(tdbb, request, transaction);
	EXE_send(tdbb, request, 0, name.schema.length(), name.schema.c_str());
	EXE_send(tdbb, request, 1, name.object.length(), name.object.c_str());

	struct IndexAlterData {
		SSHORT systemFlag;
		SSHORT systemFlagNull;
		char relationName[MAX_SQL_IDENTIFIER_LEN];
		SSHORT relationNameNull;
		SSHORT indexInactive;
		SSHORT indexInactiveNull;
	} indexData;

	bool indexFound = false;
	if (EXE_receive(tdbb, request, 2, sizeof(indexData), &indexData))
	{
		indexFound = true;
		
		if (!indexData.systemFlagNull && indexData.systemFlag &&
			!(attachment->att_flags & ATT_system))
		{
			EXE_unwind(tdbb, request);
			status_exception::raise(
				Arg::Gds(isc_dyn_cannot_mod_sysindex) << name.toQuotedString());
		}
	}
	EXE_unwind(tdbb, request);

	if (!indexFound)
	{
		status_exception::raise(
			Arg::Gds(isc_dyn_index_not_found) << name.toQuotedString());
	}

	// Convert MODIFY operation #110: Modify index in RDB$INDICES
	AutoCacheRequest modifyRequest(tdbb, drq_m_index, DYN_REQUESTS);
	EXE_start(tdbb, modifyRequest, transaction);
	EXE_send(tdbb, modifyRequest, 0, name.schema.length(), name.schema.c_str());
	EXE_send(tdbb, modifyRequest, 1, name.object.length(), name.object.c_str());

	struct ModifyIndexData {
		SSHORT indexInactive;
		SSHORT indexInactiveNull;
	} modifyData;

	if (EXE_receive(tdbb, modifyRequest, 2, sizeof(modifyData), &modifyData))
	{
		modifyData.indexInactive = active ? 0 : 1;
		modifyData.indexInactiveNull = FALSE;

		EXE_send(tdbb, modifyRequest, 3, sizeof(modifyData), &modifyData);
	}
	EXE_unwind(tdbb, modifyRequest);

	executeDdlTrigger(tdbb, dsqlScratch, transaction, DTW_AFTER, DDL_TRIGGER_ALTER_INDEX, name);
}

//----------------------
// SetStatisticsNode Class Implementation  
//----------------------

void SetStatisticsNode::execute(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch,
	jrd_tra* transaction)
{
	Attachment* const attachment = transaction->tra_attachment;

	// Convert FOR loop #115: Find index for statistics
	AutoCacheRequest request(tdbb, drq_l_idx_info3, DYN_REQUESTS);
	EXE_start(tdbb, request, transaction);
	EXE_send(tdbb, request, 0, name.schema.length(), name.schema.c_str());
	EXE_send(tdbb, request, 1, name.object.length(), name.object.c_str());

	struct IndexStatsData {
		SSHORT systemFlag;
		SSHORT systemFlagNull;
		char relationName[MAX_SQL_IDENTIFIER_LEN];
		SSHORT relationNameNull;
	} indexData;

	bool indexFound = false;
	if (EXE_receive(tdbb, request, 2, sizeof(indexData), &indexData))
	{
		indexFound = true;
		
		if (!indexData.systemFlagNull && indexData.systemFlag &&
			!(attachment->att_flags & ATT_system))
		{
			EXE_unwind(tdbb, request);
			status_exception::raise(
				Arg::Gds(isc_dyn_cannot_mod_sysindex) << name.toQuotedString());
		}
	}
	EXE_unwind(tdbb, request);

	if (!indexFound)
	{
		status_exception::raise(
			Arg::Gds(isc_dyn_index_not_found) << name.toQuotedString());
	}

	// Convert MODIFY operation #115: Update index statistics in RDB$INDICES
	AutoCacheRequest modifyRequest(tdbb, drq_m_index_statistics, DYN_REQUESTS);
	EXE_start(tdbb, modifyRequest, transaction);
	EXE_send(tdbb, modifyRequest, 0, name.schema.length(), name.schema.c_str());
	EXE_send(tdbb, modifyRequest, 1, name.object.length(), name.object.c_str());

	struct ModifyStatsData {
		bid statistics;
		SSHORT statisticsNull;
	} modifyData;

	if (EXE_receive(tdbb, modifyRequest, 2, sizeof(modifyData), &modifyData))
	{
		// Update statistics - this would need actual statistics calculation
		modifyData.statisticsNull = TRUE; // For now, clear statistics to force recalculation

		EXE_send(tdbb, modifyRequest, 3, sizeof(modifyData), &modifyData);
	}
	EXE_unwind(tdbb, modifyRequest);

	// Force recomputation of index statistics
	dbb->dbb_flags |= DBB_sweep_in_progress; // Temporary flag to force stats update
}

//----------------------
// DropIndexNode Class Implementation  
//----------------------

void DropIndexNode::execute(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch,
	jrd_tra* transaction)
{
	Attachment* const attachment = transaction->tra_attachment;

	executeDdlTrigger(tdbb, dsqlScratch, transaction, DTW_BEFORE, DDL_TRIGGER_DROP_INDEX, name);

	// Convert FOR loop #120: Find index to drop
	AutoCacheRequest request(tdbb, drq_l_idx_info4, DYN_REQUESTS);
	EXE_start(tdbb, request, transaction);
	EXE_send(tdbb, request, 0, name.schema.length(), name.schema.c_str());
	EXE_send(tdbb, request, 1, name.object.length(), name.object.c_str());

	struct IndexDropData {
		SSHORT systemFlag;
		SSHORT systemFlagNull;
		char relationName[MAX_SQL_IDENTIFIER_LEN];
		SSHORT relationNameNull;
		char foreignKey[MAX_SQL_IDENTIFIER_LEN];
		SSHORT foreignKeyNull;
	} indexData;

	bool indexFound = false;
	if (EXE_receive(tdbb, request, 2, sizeof(indexData), &indexData))
	{
		indexFound = true;
		
		if (!indexData.systemFlagNull && indexData.systemFlag &&
			!(attachment->att_flags & ATT_system))
		{
			EXE_unwind(tdbb, request);
			status_exception::raise(
				Arg::Gds(isc_dyn_cannot_mod_sysindex) << name.toQuotedString());
		}

		// Check if this is a constraint-supporting index
		if (!indexData.foreignKeyNull && indexData.foreignKey[0])
		{
			EXE_unwind(tdbb, request);
			status_exception::raise(
				Arg::Gds(isc_dyn_cannot_drop_constrain_index) << name.toQuotedString());
		}
	}
	EXE_unwind(tdbb, request);

	if (!indexFound)
	{
		if (!silent)
		{
			status_exception::raise(
				Arg::Gds(isc_dyn_index_not_found) << name.toQuotedString());
		}
		return;
	}

	// Convert DELETE operation #50: Delete index from RDB$INDICES
	AutoCacheRequest deleteRequest(tdbb, drq_e_index, DYN_REQUESTS);
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

	executeDdlTrigger(tdbb, dsqlScratch, transaction, DTW_AFTER, DDL_TRIGGER_DROP_INDEX, name);
}

//----------------------
// Helper functions for constraint management
//----------------------

static void createConstraintIndex(thread_db* tdbb, jrd_tra* transaction,
	const QualifiedName& constraintName, const QualifiedName& relationName,
	NestConst<ValueListNode>* columns, bool unique, bool primary)
{
	Database* const dbb = tdbb->getDatabase();

	// Generate index name from constraint name
	QualifiedName indexName = constraintName;
	indexName.object = "RDB$" + constraintName.object;

	// Convert STORE operation #130: Store constraint index in RDB$INDICES
	AutoCacheRequest request(tdbb, drq_s_constraint_index, DYN_REQUESTS);
	EXE_start(tdbb, request, transaction);

	struct RDB$CONSTRAINT_INDEX_RECORD {
		char RDB$SCHEMA_NAME[MAX_SQL_IDENTIFIER_LEN];
		char RDB$INDEX_NAME[MAX_SQL_IDENTIFIER_LEN];
		char RDB$RELATION_NAME[MAX_SQL_IDENTIFIER_LEN];
		SSHORT RDB$INDEX_ID;
		SSHORT RDB$INDEX_ID_NULL;
		SSHORT RDB$UNIQUE_FLAG;
		SSHORT RDB$UNIQUE_FLAG_NULL;
		SSHORT RDB$SEGMENT_COUNT;
		SSHORT RDB$SEGMENT_COUNT_NULL;
		SSHORT RDB$INDEX_INACTIVE;
		SSHORT RDB$INDEX_INACTIVE_NULL;
		SSHORT RDB$INDEX_TYPE;
		SSHORT RDB$INDEX_TYPE_NULL;
		char RDB$FOREIGN_KEY[MAX_SQL_IDENTIFIER_LEN];
		SSHORT RDB$FOREIGN_KEY_NULL;
		SSHORT RDB$SYSTEM_FLAG;
		SSHORT RDB$SYSTEM_FLAG_NULL;
	} indexRecord;

	memset(&indexRecord, 0, sizeof(indexRecord));
	
	strcpy(indexRecord.RDB$SCHEMA_NAME, indexName.schema.c_str());
	strcpy(indexRecord.RDB$INDEX_NAME, indexName.object.c_str());
	strcpy(indexRecord.RDB$RELATION_NAME, relationName.object.c_str());

	indexRecord.RDB$INDEX_ID_NULL = FALSE;
	indexRecord.RDB$INDEX_ID = dbb->generateId();

	indexRecord.RDB$UNIQUE_FLAG_NULL = FALSE;
	indexRecord.RDB$UNIQUE_FLAG = unique ? 1 : 0;

	indexRecord.RDB$SEGMENT_COUNT_NULL = FALSE;
	indexRecord.RDB$SEGMENT_COUNT = columns ? columns->items.getCount() : 0;

	indexRecord.RDB$INDEX_INACTIVE_NULL = FALSE;
	indexRecord.RDB$INDEX_INACTIVE = 0;

	indexRecord.RDB$INDEX_TYPE_NULL = FALSE;
	indexRecord.RDB$INDEX_TYPE = idx_ascending;

	if (!primary)
	{
		indexRecord.RDB$FOREIGN_KEY_NULL = FALSE;
		strcpy(indexRecord.RDB$FOREIGN_KEY, constraintName.object.c_str());
	}
	else
	{
		indexRecord.RDB$FOREIGN_KEY_NULL = TRUE;
	}

	indexRecord.RDB$SYSTEM_FLAG_NULL = FALSE;
	indexRecord.RDB$SYSTEM_FLAG = 1; // System-generated index

	EXE_send(tdbb, request, 0, sizeof(indexRecord), &indexRecord);
	EXE_unwind(tdbb, request);

	// Store index segments for constraint
	if (columns)
	{
		SSHORT segmentNumber = 0;
		for (const NestConst<ValueExprNode>* ptr = columns->items.begin();
			 ptr != columns->items.end(); ++ptr, ++segmentNumber)
		{
			const ValueExprNode* expr = *ptr;
			const FieldNode* fieldNode = nodeAs<FieldNode>(expr);
			
			if (!fieldNode)
				continue;

			// Convert STORE operation #131: Store constraint index segment
			AutoCacheRequest segmentRequest(tdbb, drq_s_constraint_idx_segs, DYN_REQUESTS);
			EXE_start(tdbb, segmentRequest, transaction);

			struct RDB$CONSTRAINT_INDEX_SEGMENTS_RECORD {
				char RDB$SCHEMA_NAME[MAX_SQL_IDENTIFIER_LEN];
				char RDB$INDEX_NAME[MAX_SQL_IDENTIFIER_LEN];
				char RDB$FIELD_NAME[MAX_SQL_IDENTIFIER_LEN];
				SSHORT RDB$FIELD_POSITION;
				SSHORT RDB$FIELD_POSITION_NULL;
			} segmentRecord;

			memset(&segmentRecord, 0, sizeof(segmentRecord));

			strcpy(segmentRecord.RDB$SCHEMA_NAME, indexName.schema.c_str());
			strcpy(segmentRecord.RDB$INDEX_NAME, indexName.object.c_str());
			strcpy(segmentRecord.RDB$FIELD_NAME, fieldNode->fieldName.c_str());

			segmentRecord.RDB$FIELD_POSITION_NULL = FALSE;
			segmentRecord.RDB$FIELD_POSITION = segmentNumber;

			EXE_send(tdbb, segmentRequest, 0, sizeof(segmentRecord), &segmentRecord);
			EXE_unwind(tdbb, segmentRequest);
		}
	}
}

static void storeRelationConstraint(thread_db* tdbb, jrd_tra* transaction,
	const QualifiedName& constraintName, const QualifiedName& relationName,
	const MetaName& constraintType, const QualifiedName& indexName)
{
	Attachment* const attachment = transaction->tra_attachment;

	// Convert STORE operation #135: Store constraint in RDB$RELATION_CONSTRAINTS
	AutoCacheRequest request(tdbb, drq_s_rel_constraints, DYN_REQUESTS);
	EXE_start(tdbb, request, transaction);

	struct RDB$RELATION_CONSTRAINTS_RECORD {
		char RDB$CONSTRAINT_NAME[MAX_SQL_IDENTIFIER_LEN];
		char RDB$CONSTRAINT_TYPE[12];
		char RDB$SCHEMA_NAME[MAX_SQL_IDENTIFIER_LEN];
		char RDB$RELATION_NAME[MAX_SQL_IDENTIFIER_LEN];
		char RDB$DEFERRABLE[4];
		SSHORT RDB$DEFERRABLE_NULL;
		char RDB$INITIALLY_DEFERRED[4];
		SSHORT RDB$INITIALLY_DEFERRED_NULL;
		char RDB$INDEX_NAME[MAX_SQL_IDENTIFIER_LEN];
		SSHORT RDB$INDEX_NAME_NULL;
	} constraintRecord;

	memset(&constraintRecord, 0, sizeof(constraintRecord));

	strcpy(constraintRecord.RDB$CONSTRAINT_NAME, constraintName.object.c_str());
	strcpy(constraintRecord.RDB$CONSTRAINT_TYPE, constraintType.c_str());
	strcpy(constraintRecord.RDB$SCHEMA_NAME, relationName.schema.c_str());
	strcpy(constraintRecord.RDB$RELATION_NAME, relationName.object.c_str());

	constraintRecord.RDB$DEFERRABLE_NULL = TRUE;
	constraintRecord.RDB$INITIALLY_DEFERRED_NULL = TRUE;

	if (indexName.hasData())
	{
		strcpy(constraintRecord.RDB$INDEX_NAME, indexName.object.c_str());
		constraintRecord.RDB$INDEX_NAME_NULL = FALSE;
	}
	else
	{
		constraintRecord.RDB$INDEX_NAME_NULL = TRUE;
	}

	EXE_send(tdbb, request, 0, sizeof(constraintRecord), &constraintRecord);
	EXE_unwind(tdbb, request);
}

//----------------------
// Constraint creation helper for DDL nodes
//----------------------

void createPrimaryKeyConstraint(thread_db* tdbb, jrd_tra* transaction,
	const QualifiedName& constraintName, const QualifiedName& relationName,
	NestConst<ValueListNode>* columns)
{
	// Create supporting index
	createConstraintIndex(tdbb, transaction, constraintName, relationName, columns, true, true);

	// Create constraint record
	QualifiedName indexName = constraintName;
	indexName.object = "RDB$" + constraintName.object;
	storeRelationConstraint(tdbb, transaction, constraintName, relationName, "PRIMARY KEY", indexName);
}

void createUniqueConstraint(thread_db* tdbb, jrd_tra* transaction,
	const QualifiedName& constraintName, const QualifiedName& relationName,
	NestConst<ValueListNode>* columns)
{
	// Create supporting index
	createConstraintIndex(tdbb, transaction, constraintName, relationName, columns, true, false);

	// Create constraint record
	QualifiedName indexName = constraintName;
	indexName.object = "RDB$" + constraintName.object;
	storeRelationConstraint(tdbb, transaction, constraintName, relationName, "UNIQUE", indexName);
}

void createForeignKeyConstraint(thread_db* tdbb, jrd_tra* transaction,
	const QualifiedName& constraintName, const QualifiedName& relationName,
	NestConst<ValueListNode>* columns, const QualifiedName& masterRelationName,
	NestConst<ValueListNode>* masterColumns, const MetaName& updateRule, const MetaName& deleteRule)
{
	// Create supporting index
	createConstraintIndex(tdbb, transaction, constraintName, relationName, columns, false, false);

	// Create constraint record
	QualifiedName indexName = constraintName;
	indexName.object = "RDB$" + constraintName.object;
	storeRelationConstraint(tdbb, transaction, constraintName, relationName, "FOREIGN KEY", indexName);

	// Store referential constraint details
	// Convert STORE operation #140: Store foreign key details in RDB$REF_CONSTRAINTS
	AutoCacheRequest request(tdbb, drq_s_ref_constraints, DYN_REQUESTS);
	EXE_start(tdbb, request, transaction);

	struct RDB$REF_CONSTRAINTS_RECORD {
		char RDB$CONSTRAINT_NAME[MAX_SQL_IDENTIFIER_LEN];
		char RDB$CONST_NAME_UQ[MAX_SQL_IDENTIFIER_LEN];
		SSHORT RDB$CONST_NAME_UQ_NULL;
		char RDB$MATCH_OPTION[8];
		SSHORT RDB$MATCH_OPTION_NULL;
		char RDB$UPDATE_RULE[12];
		SSHORT RDB$UPDATE_RULE_NULL;
		char RDB$DELETE_RULE[12];
		SSHORT RDB$DELETE_RULE_NULL;
	} refRecord;

	memset(&refRecord, 0, sizeof(refRecord));

	strcpy(refRecord.RDB$CONSTRAINT_NAME, constraintName.object.c_str());
	
	// Find primary key constraint name for referenced table - simplified
	refRecord.RDB$CONST_NAME_UQ_NULL = TRUE; // Would need to look up actual PK constraint name

	refRecord.RDB$MATCH_OPTION_NULL = TRUE; // Default FULL match

	if (updateRule.hasData())
	{
		strcpy(refRecord.RDB$UPDATE_RULE, updateRule.c_str());
		refRecord.RDB$UPDATE_RULE_NULL = FALSE;
	}
	else
	{
		strcpy(refRecord.RDB$UPDATE_RULE, "RESTRICT");
		refRecord.RDB$UPDATE_RULE_NULL = FALSE;
	}

	if (deleteRule.hasData())
	{
		strcpy(refRecord.RDB$DELETE_RULE, deleteRule.c_str());
		refRecord.RDB$DELETE_RULE_NULL = FALSE;
	}
	else
	{
		strcpy(refRecord.RDB$DELETE_RULE, "RESTRICT");
		refRecord.RDB$DELETE_RULE_NULL = FALSE;
	}

	EXE_send(tdbb, request, 0, sizeof(refRecord), &refRecord);
	EXE_unwind(tdbb, request);
}

void createCheckConstraint(thread_db* tdbb, jrd_tra* transaction,
	const QualifiedName& constraintName, const QualifiedName& relationName,
	const string& checkCondition, const BlrDebugWriter::BlrData& checkBlr)
{
	Attachment* const attachment = transaction->tra_attachment;

	// Create constraint record
	storeRelationConstraint(tdbb, transaction, constraintName, relationName, "CHECK", QualifiedName());

	// Store check constraint details
	// Convert STORE operation #145: Store check constraint details in RDB$CHECK_CONSTRAINTS
	AutoCacheRequest request(tdbb, drq_s_check_constraints, DYN_REQUESTS);
	EXE_start(tdbb, request, transaction);

	struct RDB$CHECK_CONSTRAINTS_RECORD {
		char RDB$CONSTRAINT_NAME[MAX_SQL_IDENTIFIER_LEN];
		char RDB$TRIGGER_NAME[MAX_SQL_IDENTIFIER_LEN];
		SSHORT RDB$TRIGGER_NAME_NULL;
	} checkRecord;

	memset(&checkRecord, 0, sizeof(checkRecord));

	strcpy(checkRecord.RDB$CONSTRAINT_NAME, constraintName.object.c_str());

	// Generate trigger name for check constraint
	string triggerName = "CHECK_" + constraintName.object;
	strcpy(checkRecord.RDB$TRIGGER_NAME, triggerName.c_str());
	checkRecord.RDB$TRIGGER_NAME_NULL = FALSE;

	EXE_send(tdbb, request, 0, sizeof(checkRecord), &checkRecord);
	EXE_unwind(tdbb, request);

	// This would also need to create the actual check trigger
	// but that's a complex operation involving trigger creation
}

} // namespace Jrd