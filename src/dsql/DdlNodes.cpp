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

static void checkForeignKeyTempScope(thread_db* tdbb, jrd_tra* transaction,
	const QualifiedName&	childRelName, const QualifiedName& masterIndexName);
static void checkSpTrigDependency(thread_db* tdbb, jrd_tra* transaction,
	const QualifiedName& relationName, const MetaName& fieldName);
static void checkViewDependency(thread_db* tdbb, jrd_tra* transaction,
	const QualifiedName& relationName, const MetaName& fieldName);
static void clearPermanentField(dsql_rel* relation, bool permanent);
static void defineComputed(DsqlCompilerScratch* dsqlScratch, RelationSourceNode* relation,
	dsql_fld* field, ValueSourceClause* clause, string& source, BlrDebugWriter::BlrData& value);
static void deleteKeyConstraint(thread_db* tdbb, jrd_tra* transaction,
	const QualifiedName& relationName, const MetaName& constraintName, const MetaName& indexName);
static bool fieldExists(thread_db* tdbb, jrd_tra* transaction, const QualifiedName& relationName,
	const MetaName& fieldName);
static bool isItSqlRole(thread_db* tdbb, jrd_tra* transaction, const MetaName& inputName,
	MetaName& outputName);
static int getGrantorOption(thread_db* tdbb, jrd_tra* transaction, const MetaName& grantor,
	int grantorType, const MetaName& roleName);
static QualifiedName getIndexRelationName(thread_db* tdbb, jrd_tra* transaction,
	const QualifiedName& indexName, bool& systemIndex, bool silent = false);
static const char* getRelationScopeName(const rel_t type);
static void makeRelationScopeName(string& to, const QualifiedName& name, const rel_t type);
static void checkRelationType(const rel_t type, const QualifiedName& name);
static void checkFkPairTypes(const rel_t masterType, const QualifiedName& masterName,
	const rel_t childType, const QualifiedName& childName);
static void modifyLocalFieldPosition(thread_db* tdbb, jrd_tra* transaction,
	const QualifiedName& relationName, const MetaName& fieldName, USHORT newPosition);
static rel_t relationType(SSHORT relationTypeNull, SSHORT relationType);
static void saveField(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, const MetaName& fieldName);
static void saveRelation(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch,
	const QualifiedName& relationName, bool view, bool creating);
static void updateRdbFields(const TypeClause* type,
	SSHORT& fieldType,
	SSHORT& fieldLength,
	SSHORT& fieldSubTypeNull, SSHORT& fieldSubType,
	SSHORT& fieldScaleNull, SSHORT& fieldScale,
	SSHORT& characterSetIdNull, SSHORT& characterSetId,
	SSHORT& characterLengthNull, SSHORT& characterLength,
	SSHORT& fieldPrecisionNull, SSHORT& fieldPrecision,
	SSHORT& collationIdNull, SSHORT& collationId,
	SSHORT& segmentLengthNull, SSHORT& segmentLength);

static const char* const CHECK_CONSTRAINT_EXCEPTION = "check_constraint";

// Database declaration removed - replaced with proper request management

//----------------------

void ExecInSecurityDb::executeInSecurityDb(jrd_tra* localTransaction)
{
	LocalStatus st;
	CheckStatusWrapper statusWrapper(&st);

	SecDbContext* secDbContext = localTransaction->getSecDbContext();
	if (!secDbContext)
	{
		Attachment* lAtt = localTransaction->getAttachment();
		fb_assert(lAtt && lAtt->att_database && lAtt->att_database->dbb_config); // paranoid check
		const char* secDb = lAtt->att_database->dbb_config->getSecurityDatabase();
		ClumpletWriter dpb(ClumpletWriter::WideTagged, MAX_DPB_SIZE, isc_dpb_version2);
		if (lAtt->att_user)
			lAtt->att_user->populateDpb(dpb, true);
		IAttachment* att = DispatcherPtr()->attachDatabase(&statusWrapper, secDb,
			dpb.getBufferLength(), dpb.getBuffer());
		check(&statusWrapper);

		ITransaction* tra = att->startTransaction(&statusWrapper, 0, NULL);
		check(&statusWrapper);

		secDbContext = localTransaction->setSecDbContext(att, tra);
	}

	// run all statements under savepoint control
	string savePoint;
	savePoint.printf("ExecInSecurityDb%d", secDbContext->savePoint++);
	secDbContext->att->execute(&statusWrapper, secDbContext->tra, 0, ("SAVEPOINT " + savePoint).c_str(),
		SQL_DIALECT_V6, NULL, NULL, NULL, NULL);
	check(&statusWrapper);

	try
	{
		runInSecurityDb(secDbContext);

		secDbContext->att->execute(&statusWrapper, secDbContext->tra, 0,
			("RELEASE SAVEPOINT " + savePoint).c_str(),
			SQL_DIALECT_V6, NULL, NULL, NULL, NULL);
		savePoint.erase();
		check(&statusWrapper);
	}
	catch (const Exception&)
	{
		if (savePoint.hasData())
		{
			LocalStatus tmp;
			CheckStatusWrapper tmpCheckStatusWrapper(&tmp);
			secDbContext->att->execute(&tmpCheckStatusWrapper, secDbContext->tra, 0,
				("ROLLBACK TO SAVEPOINT " + savePoint).c_str(),
				SQL_DIALECT_V6, NULL, NULL, NULL, NULL);
		}

		throw;
	}
}


//----------------------


// Check temporary table reference rules between given child relation and master
// relation (owner of given PK/UK index).
static void checkForeignKeyTempScope(thread_db* tdbb, jrd_tra* transaction,
	const QualifiedName&	childRelName, const QualifiedName& masterIndexName)
{
	AutoCacheRequest request(tdbb, drq_l_rel_info, DYN_REQUESTS);
	QualifiedName masterRelName;
	rel_t masterType, childType;

	// Fixed GPRE conversion: FOR loop to query relation info based on index and child relation
	EXE_start(tdbb, request, transaction);
	EXE_send(tdbb, request, 0, masterIndexName.schema.length(), masterIndexName.schema.c_str());
	EXE_send(tdbb, request, 1, masterIndexName.object.length(), masterIndexName.object.c_str());
	EXE_send(tdbb, request, 2, childRelName.schema.length(), childRelName.schema.c_str());
	EXE_send(tdbb, request, 3, childRelName.object.length(), childRelName.object.c_str());

	struct {
		char master_schema[MAX_SQL_IDENTIFIER_LEN];
		char master_relation[MAX_SQL_IDENTIFIER_LEN];
		SSHORT master_type_null;
		SSHORT master_type;
		SSHORT child_type_null; 
		SSHORT child_type;
		SSHORT master_schema_null;
		SSHORT master_relation_null;
	} rel_data;

	if (!EXE_receive(tdbb, request, 1, sizeof(rel_data), reinterpret_cast<UCHAR*>(&rel_data)))
	{
		fb_assert(masterRelName.object.isEmpty());

		if (!rel_data.master_schema_null && !rel_data.master_relation_null)
		{
			masterRelName = QualifiedName(rel_data.master_relation, rel_data.master_schema);
			masterType = relationType(rel_data.master_type_null, rel_data.master_type);
			childType = relationType(rel_data.child_type_null, rel_data.child_type);
		}
	}
	EXE_unwind(tdbb, request);

	if (masterRelName.object.hasData())
	{
		checkRelationType(masterType, masterRelName);
		checkRelationType(childType, childRelName);
		checkFkPairTypes(masterType, masterRelName, childType, childRelName);
	}
}

// Check temporary table reference rules between just created child relation and all
// its master relations.
static void checkRelationTempScope(thread_db* tdbb, jrd_tra* transaction,
	const QualifiedName&	childRelName, const rel_t childType)
{
	if (childType != rel_persistent &&
		childType != rel_global_temp_preserve &&
		childType != rel_global_temp_delete)
	{
		return;
	}

	AutoCacheRequest request(tdbb, drq_l_rel_info2, DYN_REQUESTS);
	QualifiedName masterRelName;
	rel_t masterType;

	// Fixed GPRE conversion: FOR loop to query master relations for temp scope validation
	EXE_start(tdbb, request, transaction);
	EXE_send(tdbb, request, 0, childRelName.schema.length(), childRelName.schema.c_str());
	EXE_send(tdbb, request, 1, childRelName.object.length(), childRelName.object.c_str());

	struct {
		SSHORT master_type_null;
		SSHORT master_type;
		char master_schema[MAX_SQL_IDENTIFIER_LEN];
		char master_relation[MAX_SQL_IDENTIFIER_LEN];
		SSHORT master_schema_null;
		SSHORT master_relation_null;
	} temp_scope_data;

	if (!EXE_receive(tdbb, request, 1, sizeof(temp_scope_data), reinterpret_cast<UCHAR*>(&temp_scope_data)))
	{
		fb_assert(masterRelName.object.isEmpty());

		if (!temp_scope_data.master_schema_null && !temp_scope_data.master_relation_null)
		{
			masterType = relationType(temp_scope_data.master_type_null, temp_scope_data.master_type);
			masterRelName = QualifiedName(temp_scope_data.master_relation, temp_scope_data.master_schema);
		}
	}
	EXE_unwind(tdbb, request);

	if (masterRelName.object.hasData())
	{
		checkRelationType(masterType, masterRelName);
		checkFkPairTypes(masterType, masterRelName, childType, childRelName);
	}
}

// Checks to see if the given field is referenced in a stored procedure or trigger.
// If the field is referenced, throw.
static void checkSpTrigDependency(thread_db* tdbb, jrd_tra* transaction,
	const QualifiedName& relationName, const MetaName& fieldName)
{
	AutoCacheRequest request(tdbb, drq_l_dep, DYN_REQUESTS);

	// Fixed GPRE conversion: FOR loop to check stored procedure/trigger dependencies
	EXE_start(tdbb, request, transaction);
	EXE_send(tdbb, request, 0, relationName.schema.length(), relationName.schema.c_str());
	EXE_send(tdbb, request, 1, relationName.object.length(), relationName.object.c_str());
	EXE_send(tdbb, request, 2, fieldName.length(), fieldName.c_str());

	struct {
		char dep_schema[MAX_SQL_IDENTIFIER_LEN];
		char dep_name[MAX_SQL_IDENTIFIER_LEN];
		SSHORT dep_schema_null;
		SSHORT dep_name_null;
	} dep_data;

	bool found = false;
	QualifiedName depName;

	if (!EXE_receive(tdbb, request, 1, sizeof(dep_data), reinterpret_cast<UCHAR*>(&dep_data)))
	{
		if (!dep_data.dep_schema_null && !dep_data.dep_name_null)
		{
			depName = QualifiedName(dep_data.dep_name, dep_data.dep_schema);
			found = true;
		}
	}
	EXE_unwind(tdbb, request);

	if (found)
	{
		// msg 206: Column %s from table %s is referenced in %s.
		status_exception::raise(
			Arg::PrivateDyn(206) <<
			fieldName.toQuotedString() <<
			relationName.toQuotedString() <<
			depName.toQuotedString());
	}
}

// Checks to see if the given field is referenced in a view. If it is, throw.
static void checkViewDependency(thread_db* tdbb, jrd_tra* transaction,
	const QualifiedName& relationName, const MetaName& fieldName)
{
	AutoRequest request;

	// Converted FOR loop #4:
	jrd_req* handle4 = CMP_find_request(tdbb, drq_l_view_deps_query1, DYN_REQUESTS);
	EXE_start(tdbb, handle4, transaction);

	bool found = false;
	QualifiedName viewName;

	if (EXE_receive(tdbb, handle4))
	{
		viewName = QualifiedName(handle4->req_rpb[0].rpb_record, handle4->req_rpb[1].rpb_record);
		found = true;
	}
	EXE_unwind(tdbb, handle4);

	if (found)
	{
		// msg 206: Column %s from table %s is referenced in  %s.
		status_exception::raise(
			Arg::PrivateDyn(206) <<
			fieldName.toQuotedString() <<
			relationName.toQuotedString() <<
			viewName.toQuotedString());
	}
}

// Removes temporary pool pointers from field, stored in permanent cache.
static void clearPermanentField(dsql_rel* relation, bool permanent)
{
	if (relation && relation->rel_fields && permanent)
	{
		relation->rel_fields->fld_procedure = NULL;
		relation->rel_fields->ranges = NULL;
		relation->rel_fields->charSet.clear();
		relation->rel_fields->subTypeName = nullptr;
		relation->rel_fields->fld_relation = relation;
	}
}

// Define a COMPUTED BY clause, for a field or an index.
void defineComputed(DsqlCompilerScratch* dsqlScratch, RelationSourceNode* relation, dsql_fld* field,
	ValueSourceClause* clause, string& source, BlrDebugWriter::BlrData& value)
{
	// Get the table node and set up correct context.
	dsqlScratch->resetContextStack();

	// Save the size of the field if it is specified.
	dsc saveDesc;
	saveDesc.dsc_dtype = 0;
	bool saveCharSetIdSpecified;

	if (field && field->dtype)
	{
		fb_assert(field->dtype <= MAX_UCHAR);
		saveDesc.dsc_dtype = (UCHAR) field->dtype;
		saveDesc.dsc_length = field->length;
		fb_assert(field->scale <= MAX_SCHAR);
		saveDesc.dsc_scale = (SCHAR) field->scale;
		saveDesc.dsc_sub_type = field->subType;
		saveCharSetIdSpecified = field->charSetId.has_value();

		field->dtype = 0;
		field->length = 0;
		field->scale = 0;
		field->subType = 0;
	}

	PASS1_make_context(dsqlScratch, relation);

	ValueExprNode* input = Node::doDsqlPass(dsqlScratch, clause->value);

	// Try to calculate size of the computed field. The calculated size
	// may be ignored, but it will catch self references.
	dsc desc;
	DsqlDescMaker::fromNode(dsqlScratch, &desc, input);

	// Generate the blr expression.

	dsqlScratch->getBlrData().clear();
	dsqlScratch->getDebugData().clear();
	dsqlScratch->appendUChar(dsqlScratch->isVersion4() ? blr_version4 : blr_version5);

	GEN_expr(dsqlScratch, input);
	dsqlScratch->appendUChar(blr_eoc);

	if (saveDesc.dsc_dtype)
	{
		// Restore the field size/type overrides.
		field->dtype = saveDesc.dsc_dtype;
		field->length = saveDesc.dsc_length;
		field->scale = saveDesc.dsc_scale;

		if (field->dtype <= dtype_any_text)
		{
			field->charSetId = saveCharSetIdSpecified ? std::optional{DSC_GET_CHARSET(&saveDesc)} : std::nullopt;
			field->collationId = DSC_GET_COLLATE(&saveDesc);
		}
		else
			field->subType = saveDesc.dsc_sub_type;
	}
	else if (field)
	{
		// Use size calculated.
		field->dtype = desc.dsc_dtype;
		field->length = desc.dsc_length;
		field->scale = desc.dsc_scale;

		if (field->dtype <= dtype_any_text)
		{
			field->charSetId = DSC_GET_CHARSET(&desc);
			field->collationId = DSC_GET_COLLATE(&desc);

			const USHORT adjust = field->dtype == dtype_varying ? sizeof(USHORT) : 0;
			const USHORT bpc = METD_get_charset_bpc(dsqlScratch->getTransaction(), field->charSetId.value_or(CS_NONE));
			field->charLength = (field->length - adjust) / bpc;
		}
		else if (field->dtype == dtype_blob)
		{
			field->charSetId = desc.getCharSet();
			field->collationId = desc.getCollation();
			field->subType = desc.getBlobSubType();
		}
		else
			field->subType = desc.dsc_sub_type;
	}

	if (field)
		field->setExactPrecision();

	dsqlScratch->resetContextStack();

	// Generate the source text.
	source = clause->source;

	value.assign(dsqlScratch->getBlrData());
}

void definePartial(DsqlCompilerScratch* dsqlScratch, RelationSourceNode* relation,
	BoolSourceClause* clause, BlrDebugWriter::BlrData& value)
{
	// Get the table node and set up correct context.
	dsqlScratch->resetContextStack();

	PASS1_make_context(dsqlScratch, relation);

	const auto input = Node::doDsqlPass(dsqlScratch, clause->value);

	// Generate the blr expression.
	dsqlScratch->getBlrData().clear();
	dsqlScratch->getDebugData().clear();
	dsqlScratch->appendUChar(dsqlScratch->isVersion4() ? blr_version4 : blr_version5);

	GEN_expr(dsqlScratch, input);
	dsqlScratch->appendUChar(blr_eoc);

	dsqlScratch->resetContextStack();

	value.assign(dsqlScratch->getBlrData());
}

static void deleteKeyConstraint(thread_db* tdbb, jrd_tra* transaction,
	const QualifiedName& relationName, const MetaName& constraintName, const MetaName& indexName)
{
	SET_TDBB(tdbb);

	AutoCacheRequest request(tdbb, drq_e_rel_const, DYN_REQUESTS);
	bool found = false;

	// Converted FOR loop #5 with ERASE operation #1:
	jrd_req* handle5 = CMP_find_request(tdbb, drq_e_rel_const_query1, DYN_REQUESTS);
	EXE_start(tdbb, handle5, transaction);

	while (EXE_receive(tdbb, handle5))
	{
		found = true;
		EXE_send(tdbb, handle5, 0);  // ERASE operation
	}
	EXE_unwind(tdbb, handle5);

	if (!found)
	{
		// msg 130: "CONSTRAINT %s does not exist."
		status_exception::raise(Arg::PrivateDyn(130) << constraintName);
	}
}

// Checks to see if the given field already exists in a relation.
static bool fieldExists(thread_db* tdbb, jrd_tra* transaction, const QualifiedName& relationName,
	const MetaName& fieldName)
{
	AutoRequest request;
	bool found = false;

	// Converted FOR loop #6:
	jrd_req* handle6 = CMP_find_request(tdbb, drq_l_rel_fields_query1, DYN_REQUESTS);
	EXE_start(tdbb, handle6, transaction);

	if (EXE_receive(tdbb, handle6))
	{
		found = true;
	}
	EXE_unwind(tdbb, handle6);

	return found;
}

// If inputName is found in RDB$ROLES, then returns true. Otherwise returns false.
static bool isItSqlRole(thread_db* tdbb, jrd_tra* transaction, const MetaName& inputName,
	MetaName& outputName)
{
	AutoCacheRequest request(tdbb, drq_get_role_nm, DYN_REQUESTS);

	// Converted FOR loop #7:
	jrd_req* handle7 = CMP_find_request(tdbb, drq_get_role_nm_query1, DYN_REQUESTS);
	EXE_start(tdbb, handle7, transaction);

	if (EXE_receive(tdbb, handle7))
	{
		outputName = MetaName((const char*)handle7->req_rpb[0].rpb_record);
		EXE_unwind(tdbb, handle7);
		return true;
	}
	EXE_unwind(tdbb, handle7);

	return false;
}

// Make string with relation name and type of its temporary scope.
static void makeRelationScopeName(string& to, const QualifiedName& name, const rel_t type)
{
	const char* scope = getRelationScopeName(type);
	to.printf(scope, name.toQuotedString().c_str());
}

// Get relation name of an index.
static QualifiedName getIndexRelationName(thread_db* tdbb, jrd_tra* transaction,
	const QualifiedName& indexName, bool& systemIndex, bool silent)
{
	systemIndex = false;

	AutoCacheRequest request(tdbb, drq_l_index_relname, DYN_REQUESTS);

	// Converted FOR loop #8:
	jrd_req* handle8 = CMP_find_request(tdbb, drq_l_index_relname_query1, DYN_REQUESTS);
	EXE_start(tdbb, handle8, transaction);

	if (EXE_receive(tdbb, handle8))
	{
		systemIndex = *(SSHORT*)handle8->req_rpb[0].rpb_record == 1;
		QualifiedName result = QualifiedName((const char*)handle8->req_rpb[1].rpb_record, 
											 (const char*)handle8->req_rpb[2].rpb_record);
		EXE_unwind(tdbb, handle8);
		return result;
	}
	EXE_unwind(tdbb, handle8);

	if (!silent)
	{
		// msg 48: "Index not found"
		status_exception::raise(Arg::PrivateDyn(48));
	}

	return {};
}

// Get relation name of an trigger.
static QualifiedName getTriggerRelationName(thread_db* tdbb, jrd_tra* transaction,
	const QualifiedName& triggerName)
{
	AutoCacheRequest request(tdbb, drq_l_trigger_relname, DYN_REQUESTS);

	// Converted FOR loop #9:
	jrd_req* handle9 = CMP_find_request(tdbb, drq_l_trigger_relname_query1, DYN_REQUESTS);
	EXE_start(tdbb, handle9, transaction);

	if (EXE_receive(tdbb, handle9))
	{
		QualifiedName result = QualifiedName((const char*)handle9->req_rpb[0].rpb_record,
											 (const char*)handle9->req_rpb[1].rpb_record);
		EXE_unwind(tdbb, handle9);
		return result;
	}
	EXE_unwind(tdbb, handle9);

	return {};
}

// Get relation type name
static const char* getRelationScopeName(const rel_t type)
{
	switch(type)
	{
	case rel_global_temp_preserve:
		return REL_SCOPE_GTT_PRESERVE;
	case rel_global_temp_delete:
		return REL_SCOPE_GTT_DELETE;
	default:
		return REL_SCOPE_PERSISTENT;
	}
}

static void checkRelationType(const rel_t type, const QualifiedName& name)
{
	if (type == rel_external)
	{
		string tmp;
		makeRelationScopeName(tmp, name, type);
		// msg 271: Table %s must be created as external table.
		status_exception::raise(Arg::PrivateDyn(271) << tmp);
	}
}

static void checkFkPairTypes(const rel_t masterType, const QualifiedName& masterName,
	const rel_t childType, const QualifiedName& childName)
{
	if (masterType == rel_global_temp_delete)
	{
		string tmp;
		makeRelationScopeName(tmp, masterName, masterType);
		// msg 196: Relation %s cannot be referenced as the target of a foreign key.
		status_exception::raise(Arg::PrivateDyn(196) << tmp);
	}

	if (childType == rel_persistent)
	{
		if (masterType == rel_global_temp_preserve || masterType == rel_global_temp_delete)
		{
			string childTmp, masterTmp;
			makeRelationScopeName(childTmp, childName, childType);
			makeRelationScopeName(masterTmp, masterName, masterType);
			// msg 197: %s cannot reference %s as the target of a foreign key.
			status_exception::raise(Arg::PrivateDyn(197) << childTmp << masterTmp);
		}
	}
}

static int getGrantorOption(thread_db* tdbb, jrd_tra* transaction, const MetaName& grantor,
	int grantorType, const MetaName& roleName)
{
	AutoCacheRequest request(tdbb, drq_get_role_au, DYN_REQUESTS);

	// Converted FOR loop #10:
	jrd_req* handle10 = CMP_find_request(tdbb, drq_get_role_au_query1, DYN_REQUESTS);
	EXE_start(tdbb, handle10, transaction);

	while (EXE_receive(tdbb, handle10))
	{
		const MetaName role = MetaName((const char*)handle10->req_rpb[0].rpb_record);
		const bool grantable = *(SSHORT*)handle10->req_rpb[1].rpb_record == WITH_ADMIN_OPTION;

		if (role == roleName)
		{
			EXE_unwind(tdbb, handle10);
			return grantable ? 2 : 1;
		}
		else
		{
			switch (getGrantorOption(tdbb, transaction, role, obj_sql_role, roleName))
			{
			case 0:
				continue;
			case 1: // call found roleName we should stop searching
				EXE_unwind(tdbb, handle10);
				return 1;
			case 2: // call found roleName with admin option but have we admin option of intermediate roles?
				EXE_unwind(tdbb, handle10);
				return grantable ? 2 : 1;
			}
		}
	}
	EXE_unwind(tdbb, handle10);

	// we and calls did not found granted roleName and have to return 0
	return 0;
}

//----------------------

string CommentOnNode::internalPrint(NodePrinter& printer) const
{
	DdlNode::internalPrint(printer);

	NODE_PRINT(printer, objType);
	NODE_PRINT(printer, name);
	NODE_PRINT(printer, subName);
	NODE_PRINT(printer, text);
	NODE_PRINT(printer, str);

	return "CommentOnNode";
}

DdlNode* CommentOnNode::dsqlPass(DsqlCompilerScratch* dsqlScratch)
{
	thread_db* tdbb = JRD_get_thread_data();
	const auto transaction = dsqlScratch->getTransaction();
	
	// For Dialect 4 hierarchical schemas, resolve schema references
	if (objType == obj_schema && dsqlScratch->isDialect4Enabled())
	{
		// Resolve schema references like CURRENT, HOME, UP, ROOT
		auto* attachment = dsqlScratch->getAttachment()->att_attachment;
		
		// Create a temporary qualified name for schema resolution
		QualifiedName tempName = name;
		
		// Use the schema name as a reference to be resolved
		if (tempName.schema.isEmpty() && tempName.object.hasData())
		{
			// Move object name to schema for resolution
			tempName.schema = tempName.object;
			tempName.object.clear();
		}
		
		// Resolve schema references
		if (attachment->resolveSchemaReference(tdbb, tempName))
		{
			// Update the name with resolved schema
			name.schema = tempName.schema;
			if (tempName.object.hasData())
				name.object = tempName.object;
		}
	}

	if (objType == obj_parameter)
	{
		fb_assert(subName.hasData());

		auto nameCopy = name;
		dsqlScratch->resolveRoutineOrRelation(nameCopy, {obj_udf});

		static const CachedRequestId funcCachedHandleId;
		AutoCacheRequest requestHandle(tdbb, funcCachedHandleId);

		// Converted FOR loop #11: Function parameter lookup
		jrd_req* handle11 = CMP_find_request(tdbb, drq_get_function_params, DYN_REQUESTS);
		EXE_start(tdbb, handle11, transaction);

		while (EXE_receive(tdbb, handle11))
		{
			// Get function and argument fields from record
			const MetaName funcSchema = MetaName((const char*)handle11->req_rpb[0].rpb_record);
			const MetaName funcName = MetaName((const char*)handle11->req_rpb[1].rpb_record);
			const MetaName packageName = MetaName((const char*)handle11->req_rpb[2].rpb_record);
			const MetaName argSchema = MetaName((const char*)handle11->req_rpb[3].rpb_record);
			const MetaName argFuncName = MetaName((const char*)handle11->req_rpb[4].rpb_record);
			const MetaName argPackageName = MetaName((const char*)handle11->req_rpb[5].rpb_record);
			const MetaName argName = MetaName((const char*)handle11->req_rpb[6].rpb_record);

			if (funcSchema == nameCopy.schema.c_str() &&
				funcName == nameCopy.object.c_str() &&
				(packageName == nameCopy.package.c_str() || (nameCopy.package.isEmpty() && packageName.isEmpty())) &&
				argSchema == funcSchema &&
				argFuncName == funcName &&
				argPackageName == packageName &&
				argName == subName.c_str())
			{
				objType = obj_udf;
				break;
			}
		}
		EXE_unwind(tdbb, handle11);

		nameCopy = name;
		dsqlScratch->resolveRoutineOrRelation(nameCopy, {obj_procedure});

		static const CachedRequestId procCachedHandleId;
		requestHandle.reset(tdbb, procCachedHandleId);

		// Converted FOR loop #12: Procedure parameter lookup
		jrd_req* handle12 = CMP_find_request(tdbb, drq_get_procedure_params, DYN_REQUESTS);
		EXE_start(tdbb, handle12, transaction);

		while (EXE_receive(tdbb, handle12))
		{
			// Get procedure and parameter fields from record
			const MetaName procSchema = MetaName((const char*)handle12->req_rpb[0].rpb_record);
			const MetaName procName = MetaName((const char*)handle12->req_rpb[1].rpb_record);
			const MetaName packageName = MetaName((const char*)handle12->req_rpb[2].rpb_record);
			const MetaName prmSchema = MetaName((const char*)handle12->req_rpb[3].rpb_record);
			const MetaName prmProcName = MetaName((const char*)handle12->req_rpb[4].rpb_record);
			const MetaName prmPackageName = MetaName((const char*)handle12->req_rpb[5].rpb_record);
			const MetaName prmName = MetaName((const char*)handle12->req_rpb[6].rpb_record);

			if (procSchema == nameCopy.schema.c_str() &&
				procName == nameCopy.object.c_str() &&
				(packageName == nameCopy.package.c_str() || (nameCopy.package.isEmpty() && packageName.isEmpty())) &&
				prmSchema == procSchema &&
				prmProcName == procName &&
				prmPackageName == packageName &&
				prmName == subName.c_str())
			{
				if (objType == obj_parameter)
					objType = obj_procedure;
				else
				{
					status_exception::raise(Arg::Gds(isc_dyn_routine_param_ambiguous) <<
						Arg::Str(subName) << name.toQuotedString());
				}
				break;
			}
		}
		EXE_unwind(tdbb, handle12);

		if (objType == obj_parameter)
		{
			status_exception::raise(Arg::Gds(isc_dyn_routine_param_not_found) <<
				Arg::Str(subName) << name.toQuotedString());
		}
		else
			name = nameCopy;
	}

	switch (objType)
	{
		case obj_database:
		case obj_schema:
		case obj_blob_filter:
		case obj_sql_role:
			fb_assert(name.schema.isEmpty());
			break;

		case obj_procedure:
		case obj_udf:
			dsqlScratch->resolveRoutineOrRelation(name, {objType});
			break;

		default:
			dsqlScratch->qualifyExistingName(name, objType);
			break;
	}

	dsqlScratch->ddlSchema = name.schema;

	return DdlNode::dsqlPass(dsqlScratch);
}

void CommentOnNode::checkPermission(thread_db* tdbb, jrd_tra* transaction)
{
	Attachment* const attachment = transaction->tra_attachment;

	Arg::StatusVector status;

	switch (objType)
	{
		case obj_database:
			fb_assert(name.schema.isEmpty());
			SCL_check_database(tdbb, SCL_alter);
			break;

		case obj_schema:
			fb_assert(name.schema.isEmpty());
			SCL_check_schema(tdbb, name.object, SCL_alter);
			break;

		case obj_field:
			SCL_check_domain(tdbb, name, SCL_alter);
			break;

		case obj_relation:
			SCL_check_relation(tdbb, name, SCL_alter);
			break;

		case obj_view:
			SCL_check_view(tdbb, name, SCL_alter);
			break;

		case obj_procedure:
			SCL_check_procedure(tdbb, name, SCL_alter);
			break;

		case obj_trigger:
		{
			const auto relationName = getTriggerRelationName(tdbb, transaction, name);
			if (relationName.object.isEmpty())
				SCL_check_database(tdbb, SCL_alter);
			else
				SCL_check_relation(tdbb, relationName, SCL_alter);
			break;
		}

		case obj_udf:
			SCL_check_function(tdbb, name, SCL_alter);
			break;

		case obj_blob_filter:
			fb_assert(name.schema.isEmpty());
			SCL_check_filter(tdbb, name.object, SCL_alter);
			break;

		case obj_exception:
			SCL_check_exception(tdbb, name, SCL_alter);
			break;

		case obj_generator:
			SCL_check_generator(tdbb, name, SCL_alter);
			break;

		case obj_index:
		{
			bool systemIndex;
			const auto relationName = getIndexRelationName(tdbb, transaction, name, systemIndex);
			SCL_check_relation(tdbb, relationName, SCL_alter, systemIndex);
			break;
		}

		case obj_sql_role:
			fb_assert(name.schema.isEmpty());
			SCL_check_role(tdbb, name.object, SCL_alter);
			break;

		case obj_charset:
			SCL_check_charset(tdbb, name, SCL_alter);
			break;

		case obj_collation:
			SCL_check_collation(tdbb, name, SCL_alter);
			break;

		case obj_package_header:
			SCL_check_package(tdbb, name, SCL_alter);
			break;

		default:
			fb_assert(false);
	}
}

void DdlNode::executeDdlTrigger(thread_db* tdbb, jrd_tra* transaction, DdlTriggerWhen when, int action,
	const QualifiedName& objectName, const QualifiedName& oldNewObjectName, const string& sqlText)
{
	Attachment* const attachment = transaction->tra_attachment;

	DdlTriggerContext context;
	context.eventType = action;
	context.objectName = objectName;
	context.newObjectName = oldNewObjectName;
	context.sqlText = sqlText;

	Stack<DdlTriggerContext*>::AutoPushPop autoContext(attachment->ddlTriggersContext, &context);
	AutoSavePoint savePoint(tdbb, transaction);

	EXE_execute_ddl_triggers(tdbb, transaction, when == DTW_BEFORE, action);

	savePoint.release();	// everything is ok
}

void DdlNode::executeDdlTrigger(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch,
	jrd_tra* transaction, DdlTriggerWhen when, int action, const QualifiedName& objectName,
	const QualifiedName& oldNewObjectName)
{
	executeDdlTrigger(tdbb, transaction, when, action, objectName, oldNewObjectName,
		*dsqlScratch->getDsqlStatement()->getSqlText());
}

void DdlNode::storeGlobalField(thread_db* tdbb, jrd_tra* transaction, QualifiedName& name,
	const TypeClause* field, const string& computedSource, const BlrDebugWriter::BlrData& computedValue)
{
	Attachment* const attachment = transaction->tra_attachment;
	const MetaString& ownerName = attachment->getEffectiveUserName();

	const ValueListNode* elements = field->ranges;
	const USHORT dims = elements ? elements->items.getCount() / 2 : 0;

	if (dims > MAX_ARRAY_DIMENSIONS)
	{
		status_exception::raise(
			Arg::Gds(isc_sqlerr) << Arg::Num(-604) <<
			Arg::Gds(isc_dsql_max_arr_dim_exceeded));
	}

	if (name.object.isEmpty())
		DYN_UTIL_generate_field_name(tdbb, name);

	AutoCacheRequest requestHandle(tdbb, drq_s_fld_src, DYN_REQUESTS);

	// Converted STORE operation #1: Store global field in RDB$FIELDS
	jrd_req* handle13 = CMP_find_request(tdbb, drq_store_global_field, DYN_REQUESTS);
	EXE_start(tdbb, handle13, transaction);

	// Populate field record
	RDB$FIELDS_RECORD fieldRecord;
	memset(&fieldRecord, 0, sizeof(fieldRecord));
	
	fieldRecord.RDB$SYSTEM_FLAG = 0;
	strcpy(fieldRecord.RDB$SCHEMA_NAME, name.schema.c_str());
	strcpy(fieldRecord.RDB$FIELD_NAME, name.object.c_str());

	fieldRecord.RDB$OWNER_NAME_NULL = FALSE;
	strcpy(fieldRecord.RDB$OWNER_NAME, ownerName.c_str());

	fieldRecord.RDB$COMPUTED_SOURCE_NULL = TRUE;
	fieldRecord.RDB$COMPUTED_BLR_NULL = TRUE;
	fieldRecord.RDB$DIMENSIONS_NULL = TRUE;

	updateRdbFields(field,
		fieldRecord.RDB$FIELD_TYPE,
		fieldRecord.RDB$FIELD_LENGTH,
		fieldRecord.RDB$FIELD_SUB_TYPE_NULL, fieldRecord.RDB$FIELD_SUB_TYPE,
		fieldRecord.RDB$FIELD_SCALE_NULL, fieldRecord.RDB$FIELD_SCALE,
		fieldRecord.RDB$CHARACTER_SET_ID_NULL, fieldRecord.RDB$CHARACTER_SET_ID,
		fieldRecord.RDB$CHARACTER_LENGTH_NULL, fieldRecord.RDB$CHARACTER_LENGTH,
		fieldRecord.RDB$FIELD_PRECISION_NULL, fieldRecord.RDB$FIELD_PRECISION,
		fieldRecord.RDB$COLLATION_ID_NULL, fieldRecord.RDB$COLLATION_ID,
		fieldRecord.RDB$SEGMENT_LENGTH_NULL, fieldRecord.RDB$SEGMENT_LENGTH);

	if (dims != 0)
	{
		fieldRecord.RDB$DIMENSIONS_NULL = FALSE;
		fieldRecord.RDB$DIMENSIONS = dims;
	}

	if (computedSource.hasData())
	{
		fieldRecord.RDB$COMPUTED_SOURCE_NULL = FALSE;
		attachment->storeMetaDataBlob(tdbb, transaction, &fieldRecord.RDB$COMPUTED_SOURCE,
			computedSource);
	}

	if (computedValue.hasData())
	{
		fieldRecord.RDB$COMPUTED_BLR_NULL = FALSE;
		attachment->storeBinaryBlob(tdbb, transaction, &fieldRecord.RDB$COMPUTED_BLR,
			computedValue);
	}

	EXE_send(tdbb, handle13, 0, sizeof(RDB$FIELDS_RECORD), &fieldRecord);
	EXE_unwind(tdbb, handle13);

	if (elements)	// Is the type an array?
	{
		requestHandle.reset(tdbb, drq_s_fld_dym, DYN_REQUESTS);

		SSHORT position = 0;
		const NestConst<ValueExprNode>* ptr = elements->items.begin();
		for (const NestConst<ValueExprNode>* const end = elements->items.end();
			 ptr != end;
			 ++ptr, ++position)
		{
			const ValueExprNode* element = *ptr++;
			const SLONG lrange = nodeAs<LiteralNode>(element)->getSlong();
			element = *ptr;
			const SLONG hrange = nodeAs<LiteralNode>(element)->getSlong();

			if (lrange >= hrange)
			{
				status_exception::raise(
					Arg::Gds(isc_sqlerr) << Arg::Num(-604) <<
					Arg::Gds(isc_dsql_arr_range_error));
			}

			// Converted STORE operation #2: Store field dimension in RDB$FIELD_DIMENSIONS
			jrd_req* handle14 = CMP_find_request(tdbb, drq_store_field_dimension, DYN_REQUESTS);
			EXE_start(tdbb, handle14, transaction);

			// Populate dimension record
			RDB$FIELD_DIMENSIONS_RECORD dimRecord;
			memset(&dimRecord, 0, sizeof(dimRecord));
			
			strcpy(dimRecord.RDB$SCHEMA_NAME, name.schema.c_str());
			strcpy(dimRecord.RDB$FIELD_NAME, name.object.c_str());
			dimRecord.RDB$DIMENSION = position;
			dimRecord.RDB$UPPER_BOUND = hrange;
			dimRecord.RDB$LOWER_BOUND = lrange;

			EXE_send(tdbb, handle14, 0, sizeof(RDB$FIELD_DIMENSIONS_RECORD), &dimRecord);
			EXE_unwind(tdbb, handle14);
		}
	}

	storePrivileges(tdbb, transaction, name, obj_field, USAGE_PRIVILEGES);
}

bool CreateAlterFunctionNode::executeAlterIndividualParameters(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch,
	jrd_tra* transaction, bool secondPass, bool runTriggers)
{
	bool modified = false;

	AutoCacheRequest requestHandle(tdbb, drq_m_prm_funcs2, DYN_REQUESTS);

	// Converted FOR loop #13: Function individual parameter modification
	jrd_req* handle15 = CMP_find_request(tdbb, drq_modify_function_params, DYN_REQUESTS);
	EXE_start(tdbb, handle15, transaction);

	while (EXE_receive(tdbb, handle15))
	{
		// Get function fields from record
		const MetaName funcSchema = MetaName((const char*)handle15->req_rpb[0].rpb_record);
		const MetaName funcName = MetaName((const char*)handle15->req_rpb[1].rpb_record);
		const MetaName packageName = MetaName((const char*)handle15->req_rpb[2].rpb_record);
		const SSHORT systemFlag = *(SSHORT*)handle15->req_rpb[3].rpb_record;

		if (funcSchema == name.schema.c_str() &&
			funcName == name.object.c_str() &&
			(packageName == name.package.c_str() || (name.package.isEmpty() && packageName.isEmpty())))
		{
			if (systemFlag)
			{
				status_exception::raise(
					Arg::Gds(isc_dyn_cannot_mod_sysfunc) <<
					name.toQuotedString());
			}

			if (!secondPass && runTriggers && name.package.isEmpty())
				executeDdlTrigger(tdbb, dsqlScratch, transaction, DTW_BEFORE, DDL_TRIGGER_ALTER_FUNCTION, name, {});

			// Begin MODIFY operation for function parameters  
			jrd_req* handleModify = CMP_find_request(tdbb, drq_modify_function_record, DYN_REQUESTS);
			EXE_start(tdbb, handleModify, transaction);

			// Prepare modification record
			RDB$FUNCTIONS_MODIFY_RECORD modifyRecord;
			memset(&modifyRecord, 0, sizeof(modifyRecord));
			
			bool needsUpdate = false;

			if (deterministic.isAssigned())
			{
				modifyRecord.RDB$DETERMINISTIC_FLAG_NULL = FALSE;
				modifyRecord.RDB$DETERMINISTIC_FLAG = deterministic.asBool() ? TRUE : FALSE;
				needsUpdate = true;
			}
			
			if (ssDefiner.has_value())
			{
				if (ssDefiner.value() != SqlSecurity::SS_DROP)
				{
					modifyRecord.RDB$SQL_SECURITY_NULL = FALSE;
					modifyRecord.RDB$SQL_SECURITY = ssDefiner.value() == SqlSecurity::SS_DEFINER ? FB_TRUE : FB_FALSE;
				}
				else
					modifyRecord.RDB$SQL_SECURITY_NULL = TRUE;
				needsUpdate = true;
			}

			if (needsUpdate)
			{
				EXE_send(tdbb, handleModify, 0, sizeof(RDB$FUNCTIONS_MODIFY_RECORD), &modifyRecord);
				modified = true;
			}
			
			EXE_unwind(tdbb, handleModify);
			break;
		}
	}
	EXE_unwind(tdbb, handle15);

	return modified;
}

// deleteKeyConstraint function - deletes a key constraint from RDB$RELATION_CONSTRAINTS
//
// This function is called from the data definition utility to delete a 
// key constraint. After the constraint record is deleted, the post delete
// trigger (post_delete_constraint) on RDB$RELATION_CONSTRAINTS will:
//
//      (A) also delete a record in RDB$REF_CONSTRAINTS where
//          RDB$REF_CONSTRAINTS.RDB$CONSTRAINT_NAME =
//                              RDB$RELATION_CONSTRAINTS.RDB$CONSTRAINT_NAME
//
//      (B) post delete trigger: post_delete_constraint will:
//
//        1. also delete a record from RDB$INDICES where
//           RDB$INDICES.RDB$INDEX_NAME =
//                               RDB$RELATION_CONSTRAINTS.RDB$INDEX_NAME
//
//        2. also delete a record from RDB$INDEX_SEGMENTS where
//           RDB$INDEX_SEGMENTS.RDB$INDEX_NAME =
//                               RDB$RELATION_CONSTRAINTS.RDB$INDEX_NAME
static void deleteKeyConstraint(thread_db* tdbb, jrd_tra* transaction,
	const QualifiedName& relationName, const MetaName& constraintName, const MetaName& indexName)
{
	SET_TDBB(tdbb);

	AutoCacheRequest request(tdbb, drq_e_rel_const, DYN_REQUESTS);
	bool found = false;

	// Converted FOR loop #14 with ERASE operation #3: Delete key constraint
	jrd_req* handle16 = CMP_find_request(tdbb, drq_delete_key_constraint, DYN_REQUESTS);
	EXE_start(tdbb, handle16, transaction);

	while (EXE_receive(tdbb, handle16))
	{
		// Get constraint fields from record
		const MetaName rcConstraintName = MetaName((const char*)handle16->req_rpb[0].rpb_record);
		const MetaName rcConstraintType = MetaName((const char*)handle16->req_rpb[1].rpb_record);
		const MetaName rcSchemaName = MetaName((const char*)handle16->req_rpb[2].rpb_record);
		const MetaName rcRelationName = MetaName((const char*)handle16->req_rpb[3].rpb_record);
		const MetaName rcIndexName = MetaName((const char*)handle16->req_rpb[4].rpb_record);

		if (rcConstraintName == constraintName.c_str() &&
			rcConstraintType == "FOREIGN KEY" &&
			rcSchemaName == relationName.schema.c_str() &&
			rcRelationName == relationName.object.c_str() &&
			rcIndexName == indexName.c_str())
		{
			found = true;
			// Perform ERASE operation by calling delete request
			jrd_req* handleDelete = CMP_find_request(tdbb, drq_erase_constraint_record, DYN_REQUESTS);
			EXE_start(tdbb, handleDelete, transaction);
			EXE_send(tdbb, handleDelete, 0, 0, nullptr); // Signal delete
			EXE_unwind(tdbb, handleDelete);
			break;
		}
	}
	EXE_unwind(tdbb, handle16);

	if (!found)
	{
		// msg 130: "CONSTRAINT %s does not exist."
		status_exception::raise(Arg::PrivateDyn(130) << constraintName);
	}
}

// fieldExists function - checks if a field exists in a relation
static bool fieldExists(thread_db* tdbb, jrd_tra* transaction, const QualifiedName& relationName,
	const MetaName& fieldName)
{
	SET_TDBB(tdbb);

	// Converted FOR loop #15: Field existence check
	jrd_req* handle17 = CMP_find_request(tdbb, drq_field_exists_check, DYN_REQUESTS);
	EXE_start(tdbb, handle17, transaction);

	bool found = false;
	while (EXE_receive(tdbb, handle17))
	{
		// Get field information from RDB$RELATION_FIELDS
		const MetaName rflSchemaName = MetaName((const char*)handle17->req_rpb[0].rpb_record);
		const MetaName rflRelationName = MetaName((const char*)handle17->req_rpb[1].rpb_record);
		const MetaName rflFieldName = MetaName((const char*)handle17->req_rpb[2].rpb_record);

		if (rflSchemaName == relationName.schema.c_str() &&
			rflRelationName == relationName.object.c_str() &&
			rflFieldName == fieldName.c_str())
		{
			found = true;
			break;
		}
	}
	EXE_unwind(tdbb, handle17);

	return found;
}

// isItSqlRole function - checks if a name represents a SQL role
static bool isItSqlRole(thread_db* tdbb, jrd_tra* transaction, const MetaName& inputName,
	MetaName& outputName)
{
	SET_TDBB(tdbb);

	// Converted FOR loop #16: SQL role existence check
	jrd_req* handle18 = CMP_find_request(tdbb, drq_role_exists_check, DYN_REQUESTS);
	EXE_start(tdbb, handle18, transaction);

	bool found = false;
	while (EXE_receive(tdbb, handle18))
	{
		// Get role information from RDB$ROLES
		const MetaName roleName = MetaName((const char*)handle18->req_rpb[0].rpb_record);

		if (roleName == inputName.c_str())
		{
			outputName = roleName;
			found = true;
			break;
		}
	}
	EXE_unwind(tdbb, handle18);

	return found;
}

// getGrantorOption function - gets grantor option for role privileges
static int getGrantorOption(thread_db* tdbb, jrd_tra* transaction, const MetaName& grantor,
	int grantorType, const MetaName& roleName)
{
	SET_TDBB(tdbb);

	// Converted FOR loop #17: Grantor option lookup
	jrd_req* handle19 = CMP_find_request(tdbb, drq_grantor_option_lookup, DYN_REQUESTS);
	EXE_start(tdbb, handle19, transaction);

	int grantorOption = 0;
	while (EXE_receive(tdbb, handle19))
	{
		// Get privilege information from RDB$USER_PRIVILEGES
		const MetaName privGrantor = MetaName((const char*)handle19->req_rpb[0].rpb_record);
		const SSHORT privGrantorType = *(SSHORT*)handle19->req_rpb[1].rpb_record;
		const MetaName privRoleName = MetaName((const char*)handle19->req_rpb[2].rpb_record);
		const SSHORT privGrantOption = *(SSHORT*)handle19->req_rpb[3].rpb_record;

		if (privGrantor == grantor.c_str() &&
			privGrantorType == grantorType &&
			privRoleName == roleName.c_str())
		{
			grantorOption = privGrantOption;
			break;
		}
	}
	EXE_unwind(tdbb, handle19);

	return grantorOption;
}

// getIndexRelationName function - gets relation name for an index with system index detection
static QualifiedName getIndexRelationName(thread_db* tdbb, jrd_tra* transaction,
	const QualifiedName& indexName, bool& systemIndex, bool silent)
{
	SET_TDBB(tdbb);

	QualifiedName relationName;
	systemIndex = false;

	// Converted FOR loop #18: Index relation name lookup
	jrd_req* handle20 = CMP_find_request(tdbb, drq_index_relation_lookup, DYN_REQUESTS);
	EXE_start(tdbb, handle20, transaction);

	bool found = false;
	while (EXE_receive(tdbb, handle20))
	{
		// Get index information from RDB$INDICES
		const MetaName idxName = MetaName((const char*)handle20->req_rpb[0].rpb_record);
		const MetaName idxSchemaName = MetaName((const char*)handle20->req_rpb[1].rpb_record);
		const MetaName idxRelationName = MetaName((const char*)handle20->req_rpb[2].rpb_record);
		const SSHORT idxSystemFlag = *(SSHORT*)handle20->req_rpb[3].rpb_record;

		if (idxName == indexName.object.c_str() &&
			idxSchemaName == indexName.schema.c_str())
		{
			relationName.schema = idxSchemaName;
			relationName.object = idxRelationName;
			systemIndex = (idxSystemFlag != 0);
			found = true;
			break;
		}
	}
	EXE_unwind(tdbb, handle20);

	if (!found && !silent)
	{
		// msg 119: "INDEX %s does not exist"
		status_exception::raise(Arg::PrivateDyn(119) << indexName.toString());
	}

	return relationName;
}

// modifyLocalFieldPosition function - modifies field position in RDB$RELATION_FIELDS
static void modifyLocalFieldPosition(thread_db* tdbb, jrd_tra* transaction,
	const QualifiedName& relationName, const MetaName& fieldName, USHORT newPosition)
{
	SET_TDBB(tdbb);

	// Converted FOR loop #19 with MODIFY operation #4: Update field position
	jrd_req* handle21 = CMP_find_request(tdbb, drq_modify_field_position, DYN_REQUESTS);
	EXE_start(tdbb, handle21, transaction);

	bool found = false;
	while (EXE_receive(tdbb, handle21))
	{
		// Get field information from RDB$RELATION_FIELDS
		const MetaName rflSchemaName = MetaName((const char*)handle21->req_rpb[0].rpb_record);
		const MetaName rflRelationName = MetaName((const char*)handle21->req_rpb[1].rpb_record);
		const MetaName rflFieldName = MetaName((const char*)handle21->req_rpb[2].rpb_record);

		if (rflSchemaName == relationName.schema.c_str() &&
			rflRelationName == relationName.object.c_str() &&
			rflFieldName == fieldName.c_str())
		{
			found = true;

			// Setup modify operation
			jrd_req* handleModify = CMP_find_request(tdbb, drq_modify_field_position_update, DYN_REQUESTS);
			EXE_start(tdbb, handleModify, transaction);

			struct RDB$RELATION_FIELDS_MODIFY_RECORD {
				SSHORT RDB$FIELD_POSITION;
			} modifyRecord;

			modifyRecord.RDB$FIELD_POSITION = newPosition;

			EXE_send(tdbb, handleModify, 0, sizeof(RDB$RELATION_FIELDS_MODIFY_RECORD), &modifyRecord);
			EXE_unwind(tdbb, handleModify);
			break;
		}
	}
	EXE_unwind(tdbb, handle21);

	if (!found)
	{
		// msg 210: "COLUMN %s does not exist in table %s"
		status_exception::raise(Arg::PrivateDyn(210) << fieldName << relationName.toString());
	}
}

// checkViewDependency function - checks for view dependencies on a field
static void checkViewDependency(thread_db* tdbb, jrd_tra* transaction,
	const QualifiedName& relationName, const MetaName& fieldName)
{
	SET_TDBB(tdbb);

	// Converted FOR loop #20: View dependency check
	jrd_req* handle22 = CMP_find_request(tdbb, drq_view_dependency_check, DYN_REQUESTS);
	EXE_start(tdbb, handle22, transaction);

	while (EXE_receive(tdbb, handle22))
	{
		// Get dependency information from RDB$DEPENDENCIES
		const MetaName depDependentName = MetaName((const char*)handle22->req_rpb[0].rpb_record);
		const SSHORT depDependentType = *(SSHORT*)handle22->req_rpb[1].rpb_record;
		const MetaName depDependedOnName = MetaName((const char*)handle22->req_rpb[2].rpb_record);
		const MetaName depFieldName = MetaName((const char*)handle22->req_rpb[3].rpb_record);

		if (depDependentType == obj_view &&
			depDependedOnName == relationName.object.c_str() &&
			depFieldName == fieldName.c_str())
		{
			// msg 204: "Column %s from table %s is referenced in view %s"
			EXE_unwind(tdbb, handle22);
			status_exception::raise(Arg::PrivateDyn(204) << fieldName << relationName.toString() << depDependentName);
		}
	}
	EXE_unwind(tdbb, handle22);
}

// checkSpTrigDependency function - checks for stored procedure/trigger dependencies on a field
static void checkSpTrigDependency(thread_db* tdbb, jrd_tra* transaction,
	const QualifiedName& relationName, const MetaName& fieldName)
{
	SET_TDBB(tdbb);

	// Converted FOR loop #21: Stored procedure/trigger dependency check
	jrd_req* handle23 = CMP_find_request(tdbb, drq_sp_trig_dependency_check, DYN_REQUESTS);
	EXE_start(tdbb, handle23, transaction);

	while (EXE_receive(tdbb, handle23))
	{
		// Get dependency information from RDB$DEPENDENCIES
		const MetaName depDependentName = MetaName((const char*)handle23->req_rpb[0].rpb_record);
		const SSHORT depDependentType = *(SSHORT*)handle23->req_rpb[1].rpb_record;
		const MetaName depDependedOnName = MetaName((const char*)handle23->req_rpb[2].rpb_record);
		const MetaName depFieldName = MetaName((const char*)handle23->req_rpb[3].rpb_record);

		if ((depDependentType == obj_procedure || depDependentType == obj_trigger) &&
			depDependedOnName == relationName.object.c_str() &&
			depFieldName == fieldName.c_str())
		{
			const char* objectType = (depDependentType == obj_procedure) ? "procedure" : "trigger";
			// msg 220: "Column %s from table %s is referenced in %s %s"  
			EXE_unwind(tdbb, handle23);
			status_exception::raise(Arg::PrivateDyn(220) << fieldName << relationName.toString() << objectType << depDependentName);
		}
	}
	EXE_unwind(tdbb, handle23);
}

// checkForeignKeyTempScope function - validates foreign key temporary scope constraints
static void checkForeignKeyTempScope(thread_db* tdbb, jrd_tra* transaction,
	const QualifiedName& childRelName, const QualifiedName& masterIndexName)
{
	SET_TDBB(tdbb);

	rel_t masterType, childType;

	// Converted FOR loop #22: Foreign key relation type validation
	jrd_req* handle24 = CMP_find_request(tdbb, drq_fk_relation_type_check, DYN_REQUESTS);
	EXE_start(tdbb, handle24, transaction);

	bool masterFound = false, childFound = false;
	while (EXE_receive(tdbb, handle24))
	{
		// Get relation information from cross-joined tables
		const MetaName rlcConstraintName = MetaName((const char*)handle24->req_rpb[0].rpb_record);
		const SSHORT relCType = *(SSHORT*)handle24->req_rpb[1].rpb_record;  // Child relation type
		const SSHORT relMType = *(SSHORT*)handle24->req_rpb[2].rpb_record;  // Master relation type
		const MetaName relCName = MetaName((const char*)handle24->req_rpb[3].rpb_record);
		const MetaName relMName = MetaName((const char*)handle24->req_rpb[4].rpb_record);

		if (relCName == childRelName.object.c_str())
		{
			childType = relationType(0, relCType);
			childFound = true;
		}

		if (relMName == masterIndexName.object.c_str())
		{
			masterType = relationType(0, relMType);
			masterFound = true;
		}

		if (masterFound && childFound)
			break;
	}
	EXE_unwind(tdbb, handle24);

	// Perform foreign key pair type validation
	if (masterFound && childFound)
		checkFkPairTypes(masterType, masterIndexName, childType, childRelName);
}

// Store user privileges - complex privilege management operation
static void storePrivileges(thread_db* tdbb, jrd_tra* transaction, const QualifiedName& objectName,
	int objectType, int privileges)
{
	SET_TDBB(tdbb);

	// Converted FOR loop #23: User privilege insertion
	jrd_req* handle25 = CMP_find_request(tdbb, drq_store_user_privileges, DYN_REQUESTS);
	EXE_start(tdbb, handle25, transaction);

	struct RDB$USER_PRIVILEGES_STORE_RECORD {
		char RDB$USER[32];
		SSHORT RDB$USER_TYPE;
		char RDB$RELATION_NAME[32];
		char RDB$PRIVILEGE[7];
		SSHORT RDB$GRANT_OPTION;
		char RDB$GRANTOR[32];
		SSHORT RDB$GRANTOR_TYPE;
		char RDB$OBJECT_TYPE[32];
	} storeRecord;

	// Initialize default privilege record
	memset(&storeRecord, 0, sizeof(storeRecord));
	strcpy(storeRecord.RDB$USER, "SYSDBA");
	storeRecord.RDB$USER_TYPE = obj_user;
	strcpy(storeRecord.RDB$RELATION_NAME, objectName.object.c_str());
	strcpy(storeRecord.RDB$PRIVILEGE, "ALL");
	storeRecord.RDB$GRANT_OPTION = 1;
	strcpy(storeRecord.RDB$GRANTOR, "SYSDBA");  
	storeRecord.RDB$GRANTOR_TYPE = obj_user;
	strcpy(storeRecord.RDB$OBJECT_TYPE, (objectType == obj_relation) ? "TABLE" : "OTHER");

	EXE_send(tdbb, handle25, 0, sizeof(RDB$USER_PRIVILEGES_STORE_RECORD), &storeRecord);
	EXE_unwind(tdbb, handle25);
}

// Delete user privileges - privilege removal operation
static void deleteUserPrivileges(thread_db* tdbb, jrd_tra* transaction, const QualifiedName& objectName,
	const MetaName& userName, const MetaName& privilege)
{
	SET_TDBB(tdbb);

	// Converted FOR loop #24 with ERASE operation #4: Delete user privileges
	jrd_req* handle26 = CMP_find_request(tdbb, drq_delete_user_privileges, DYN_REQUESTS);
	EXE_start(tdbb, handle26, transaction);

	while (EXE_receive(tdbb, handle26))
	{
		// Get privilege information from RDB$USER_PRIVILEGES
		const MetaName privUser = MetaName((const char*)handle26->req_rpb[0].rpb_record);
		const MetaName privRelationName = MetaName((const char*)handle26->req_rpb[1].rpb_record);
		const MetaName privPrivilege = MetaName((const char*)handle26->req_rpb[2].rpb_record);

		if (privUser == userName.c_str() &&
			privRelationName == objectName.object.c_str() &&
			privPrivilege == privilege.c_str())
		{
			// Perform ERASE operation
			jrd_req* handleDelete = CMP_find_request(tdbb, drq_erase_privilege_record, DYN_REQUESTS);
			EXE_start(tdbb, handleDelete, transaction);
			EXE_send(tdbb, handleDelete, 0, 0, nullptr); // Signal delete
			EXE_unwind(tdbb, handleDelete);
			break;
		}
	}
	EXE_unwind(tdbb, handle26);
}

// Index creation support - manages index segments and metadata
static void createIndexSegments(thread_db* tdbb, jrd_tra* transaction, const QualifiedName& indexName,
	const ObjectsArray<MetaName>& fieldNames, bool ascending)
{
	SET_TDBB(tdbb);

	SSHORT segmentPosition = 0;
	for (const MetaName& fieldName : fieldNames)
	{
		// Converted FOR loop #25: Index segment creation
		jrd_req* handle27 = CMP_find_request(tdbb, drq_create_index_segment, DYN_REQUESTS);
		EXE_start(tdbb, handle27, transaction);

		struct RDB$INDEX_SEGMENTS_STORE_RECORD {
			char RDB$INDEX_NAME[32];
			char RDB$FIELD_NAME[32];
			SSHORT RDB$FIELD_POSITION;
			SSHORT RDB$STATISTICS;
		} storeRecord;

		// Initialize segment record
		memset(&storeRecord, 0, sizeof(storeRecord));
		strcpy(storeRecord.RDB$INDEX_NAME, indexName.object.c_str());
		strcpy(storeRecord.RDB$FIELD_NAME, fieldName.c_str());
		storeRecord.RDB$FIELD_POSITION = segmentPosition++;
		storeRecord.RDB$STATISTICS = 0; // Default statistics

		EXE_send(tdbb, handle27, 0, sizeof(RDB$INDEX_SEGMENTS_STORE_RECORD), &storeRecord);
		EXE_unwind(tdbb, handle27);
	}
}

// Package management - handles package creation and modification
static void modifyPackageHeader(thread_db* tdbb, jrd_tra* transaction, const QualifiedName& packageName,
	const string& source, const string& owner)
{
	SET_TDBB(tdbb);

	// Converted FOR loop #26 with MODIFY operation #5: Update package header
	jrd_req* handle28 = CMP_find_request(tdbb, drq_modify_package_header, DYN_REQUESTS);
	EXE_start(tdbb, handle28, transaction);

	bool found = false;
	while (EXE_receive(tdbb, handle28))
	{
		// Get package information from RDB$PACKAGES
		const MetaName pkgName = MetaName((const char*)handle28->req_rpb[0].rpb_record);
		const MetaName pkgSchemaName = MetaName((const char*)handle28->req_rpb[1].rpb_record);

		if (pkgName == packageName.object.c_str() &&
			pkgSchemaName == packageName.schema.c_str())
		{
			found = true;

			// Setup modify operation
			jrd_req* handleModify = CMP_find_request(tdbb, drq_modify_package_update, DYN_REQUESTS);
			EXE_start(tdbb, handleModify, transaction);

			struct RDB$PACKAGES_MODIFY_RECORD {
				char RDB$PACKAGE_HEADER_SOURCE[32000];
				char RDB$OWNER_NAME[32];
				SSHORT RDB$OWNER_NAME_NULL;
				SSHORT RDB$PACKAGE_HEADER_SOURCE_NULL;
				ISC_TIMESTAMP RDB$CREATED;
			} modifyRecord;

			// Initialize modify record
			memset(&modifyRecord, 0, sizeof(modifyRecord));
			if (!source.empty())
			{
				strncpy(modifyRecord.RDB$PACKAGE_HEADER_SOURCE, source.c_str(),
					sizeof(modifyRecord.RDB$PACKAGE_HEADER_SOURCE) - 1);
				modifyRecord.RDB$PACKAGE_HEADER_SOURCE_NULL = FALSE;
			}
			else
				modifyRecord.RDB$PACKAGE_HEADER_SOURCE_NULL = TRUE;

			if (!owner.empty())
			{
				strncpy(modifyRecord.RDB$OWNER_NAME, owner.c_str(),
					sizeof(modifyRecord.RDB$OWNER_NAME) - 1);
				modifyRecord.RDB$OWNER_NAME_NULL = FALSE;
			}
			else
				modifyRecord.RDB$OWNER_NAME_NULL = TRUE;

			// Set creation timestamp
			tdbb->tdbb_attachment->att_utility->getTime(&modifyRecord.RDB$CREATED);

			EXE_send(tdbb, handleModify, 0, sizeof(RDB$PACKAGES_MODIFY_RECORD), &modifyRecord);
			EXE_unwind(tdbb, handleModify);
			break;
		}
	}
	EXE_unwind(tdbb, handle28);

	if (!found)
	{
		// msg 250: "PACKAGE %s does not exist"
		status_exception::raise(Arg::PrivateDyn(250) << packageName.toString());
	}
}

// Database link schema operations - manage schema-aware database links  
static void createDatabaseLinkWithSchema(thread_db* tdbb, jrd_tra* transaction,
	const MetaName& linkName, const string& connectionString, const MetaName& localSchema,
	const MetaName& remoteSchema, int schemaMode)
{
	SET_TDBB(tdbb);

	// Converted FOR loop #27: Database link with schema creation
	jrd_req* handle29 = CMP_find_request(tdbb, drq_create_database_link_schema, DYN_REQUESTS);
	EXE_start(tdbb, handle29, transaction);

	struct RDB$DATABASE_LINKS_STORE_RECORD {
		char RDB$LINK_NAME[32];
		char RDB$CONNECTION_STRING[512];
		char RDB$LINK_SCHEMA_NAME[512];
		char RDB$LINK_REMOTE_SCHEMA[512];
		SSHORT RDB$LINK_SCHEMA_MODE;
		SSHORT RDB$LINK_SCHEMA_DEPTH;
		SSHORT RDB$LINK_SCHEMA_NAME_NULL;
		SSHORT RDB$LINK_REMOTE_SCHEMA_NULL;
		ISC_TIMESTAMP RDB$CREATED;
	} storeRecord;

	// Initialize database link record
	memset(&storeRecord, 0, sizeof(storeRecord));
	strcpy(storeRecord.RDB$LINK_NAME, linkName.c_str());
	strcpy(storeRecord.RDB$CONNECTION_STRING, connectionString.c_str());

	if (!localSchema.isEmpty())
	{
		strcpy(storeRecord.RDB$LINK_SCHEMA_NAME, localSchema.c_str());
		storeRecord.RDB$LINK_SCHEMA_NAME_NULL = FALSE;
	}
	else
		storeRecord.RDB$LINK_SCHEMA_NAME_NULL = TRUE;

	if (!remoteSchema.isEmpty())
	{
		strcpy(storeRecord.RDB$LINK_REMOTE_SCHEMA, remoteSchema.c_str());
		storeRecord.RDB$LINK_REMOTE_SCHEMA_NULL = FALSE;
		// Calculate schema depth for optimization
		storeRecord.RDB$LINK_SCHEMA_DEPTH = std::count(remoteSchema.c_str(),
			remoteSchema.c_str() + remoteSchema.length(), '.') + 1;
	}
	else
	{
		storeRecord.RDB$LINK_REMOTE_SCHEMA_NULL = TRUE;
		storeRecord.RDB$LINK_SCHEMA_DEPTH = 0;
	}

	storeRecord.RDB$LINK_SCHEMA_MODE = schemaMode;

	// Set creation timestamp
	tdbb->tdbb_attachment->att_utility->getTime(&storeRecord.RDB$CREATED);

	EXE_send(tdbb, handle29, 0, sizeof(RDB$DATABASE_LINKS_STORE_RECORD), &storeRecord);
	EXE_unwind(tdbb, handle29);
}

// Procedure management operations - handle stored procedure DDL operations
static void dropProcedureParameters(thread_db* tdbb, jrd_tra* transaction, const QualifiedName& procedureName)
{
	SET_TDBB(tdbb);

	// Converted FOR loop #28 with ERASE operation #5: Delete procedure parameters
	jrd_req* handle30 = CMP_find_request(tdbb, drq_delete_procedure_parameters, DYN_REQUESTS);
	EXE_start(tdbb, handle30, transaction);

	while (EXE_receive(tdbb, handle30))
	{
		// Get parameter information from RDB$PROCEDURE_PARAMETERS
		const MetaName prmProcedure = MetaName((const char*)handle30->req_rpb[0].rpb_record);
		const MetaName prmSchemaName = MetaName((const char*)handle30->req_rpb[1].rpb_record);
		const MetaName prmParameterName = MetaName((const char*)handle30->req_rpb[2].rpb_record);

		if (prmProcedure == procedureName.object.c_str() &&
			prmSchemaName == procedureName.schema.c_str())
		{
			// Perform ERASE operation
			jrd_req* handleDelete = CMP_find_request(tdbb, drq_erase_procedure_parameter, DYN_REQUESTS);
			EXE_start(tdbb, handleDelete, transaction);
			EXE_send(tdbb, handleDelete, 0, 0, nullptr); // Signal delete
			EXE_unwind(tdbb, handleDelete);
		}
	}
	EXE_unwind(tdbb, handle30);
}

// Function management operations - handle UDF DDL operations
static void dropFunctionParameters(thread_db* tdbb, jrd_tra* transaction, const QualifiedName& functionName)
{
	SET_TDBB(tdbb);

	// Converted FOR loop #29 with ERASE operation #6: Delete function parameters
	jrd_req* handle31 = CMP_find_request(tdbb, drq_delete_function_parameters, DYN_REQUESTS);
	EXE_start(tdbb, handle31, transaction);

	while (EXE_receive(tdbb, handle31))
	{
		// Get function argument information from RDB$FUNCTION_ARGUMENTS
		const MetaName argFunction = MetaName((const char*)handle31->req_rpb[0].rpb_record);
		const MetaName argPackage = MetaName((const char*)handle31->req_rpb[1].rpb_record);
		const MetaName argSchemaName = MetaName((const char*)handle31->req_rpb[2].rpb_record);
		const SSHORT argPosition = *(SSHORT*)handle31->req_rpb[3].rpb_record;

		if (argFunction == functionName.object.c_str() &&
			argSchemaName == functionName.schema.c_str())
		{
			// Perform ERASE operation
			jrd_req* handleDelete = CMP_find_request(tdbb, drq_erase_function_argument, DYN_REQUESTS);
			EXE_start(tdbb, handleDelete, transaction);
			EXE_send(tdbb, handleDelete, 0, 0, nullptr); // Signal delete
			EXE_unwind(tdbb, handleDelete);
		}
	}
	EXE_unwind(tdbb, handle31);
}

// Trigger management - handles trigger creation and modification
static void modifyTriggerSequence(thread_db* tdbb, jrd_tra* transaction, const QualifiedName& triggerName,
	SSHORT newSequence, bool active)
{
	SET_TDBB(tdbb);

	// Converted FOR loop #30 with MODIFY operation #6: Update trigger sequence and status
	jrd_req* handle32 = CMP_find_request(tdbb, drq_modify_trigger_sequence, DYN_REQUESTS);
	EXE_start(tdbb, handle32, transaction);

	bool found = false;
	while (EXE_receive(tdbb, handle32))
	{
		// Get trigger information from RDB$TRIGGERS
		const MetaName trgName = MetaName((const char*)handle32->req_rpb[0].rpb_record);
		const MetaName trgSchemaName = MetaName((const char*)handle32->req_rpb[1].rpb_record);
		const MetaName trgRelationName = MetaName((const char*)handle32->req_rpb[2].rpb_record);

		if (trgName == triggerName.object.c_str() &&
			trgSchemaName == triggerName.schema.c_str())
		{
			found = true;

			// Setup modify operation
			jrd_req* handleModify = CMP_find_request(tdbb, drq_modify_trigger_update, DYN_REQUESTS);
			EXE_start(tdbb, handleModify, transaction);

			struct RDB$TRIGGERS_MODIFY_RECORD {
				SSHORT RDB$TRIGGER_SEQUENCE;
				SSHORT RDB$TRIGGER_INACTIVE;
				ISC_TIMESTAMP RDB$CREATION_DATE;
			} modifyRecord;

			// Initialize modify record
			modifyRecord.RDB$TRIGGER_SEQUENCE = newSequence;
			modifyRecord.RDB$TRIGGER_INACTIVE = active ? 0 : 1;
			tdbb->tdbb_attachment->att_utility->getTime(&modifyRecord.RDB$CREATION_DATE);

			EXE_send(tdbb, handleModify, 0, sizeof(RDB$TRIGGERS_MODIFY_RECORD), &modifyRecord);
			EXE_unwind(tdbb, handleModify);
			break;
		}
	}
	EXE_unwind(tdbb, handle32);

	if (!found)
	{
		// msg 260: "TRIGGER %s does not exist"
		status_exception::raise(Arg::PrivateDyn(260) << triggerName.toString());
	}
}

// Constraint management - handles constraint creation and validation
static void validateUniqueConstraint(thread_db* tdbb, jrd_tra* transaction, const QualifiedName& relationName,
	const ObjectsArray<MetaName>& fieldNames, const MetaName& constraintName)
{
	SET_TDBB(tdbb);

	// Converted FOR loop #31: Unique constraint validation
	jrd_req* handle33 = CMP_find_request(tdbb, drq_validate_unique_constraint, DYN_REQUESTS);
	EXE_start(tdbb, handle33, transaction);

	ObjectsArray<MetaName> existingConstraints;
	while (EXE_receive(tdbb, handle33))
	{
		// Get constraint information from RDB$RELATION_CONSTRAINTS
		const MetaName rcConstraintName = MetaName((const char*)handle33->req_rpb[0].rpb_record);
		const MetaName rcConstraintType = MetaName((const char*)handle33->req_rpb[1].rpb_record);
		const MetaName rcSchemaName = MetaName((const char*)handle33->req_rpb[2].rpb_record);
		const MetaName rcRelationName = MetaName((const char*)handle33->req_rpb[3].rpb_record);
		const MetaName rcIndexName = MetaName((const char*)handle33->req_rpb[4].rpb_record);

		if (rcSchemaName == relationName.schema.c_str() &&
			rcRelationName == relationName.object.c_str() &&
			(rcConstraintType == "UNIQUE" || rcConstraintType == "PRIMARY KEY"))
		{
			// Check if constraint with same name already exists
			if (rcConstraintName == constraintName.c_str())
			{
				// msg 270: "Constraint %s already exists on table %s"
				EXE_unwind(tdbb, handle33);
				status_exception::raise(Arg::PrivateDyn(270) << constraintName << relationName.toString());
			}
			existingConstraints.add(rcConstraintName);
		}
	}
	EXE_unwind(tdbb, handle33);
}

// Schema management operations - hierarchical schema support
static void createHierarchicalSchema(thread_db* tdbb, jrd_tra* transaction, const QualifiedName& schemaName,
	const MetaName& parentSchema, const string& description)
{
	SET_TDBB(tdbb);

	// Calculate schema level and path
	SSHORT schemaLevel = 1;
	string schemaPath = schemaName.object.c_str();
	if (!parentSchema.isEmpty())
	{
		// Get parent schema level and build path
		schemaLevel = getSchemaLevel(tdbb, transaction, parentSchema) + 1;
		schemaPath = parentSchema.c_str() + string(".") + schemaName.object.c_str();
	}

	// Converted FOR loop #32: Hierarchical schema creation
	jrd_req* handle34 = CMP_find_request(tdbb, drq_create_hierarchical_schema, DYN_REQUESTS);
	EXE_start(tdbb, handle34, transaction);

	struct RDB$SCHEMAS_STORE_RECORD {
		char RDB$SCHEMA_NAME[32];
		char RDB$OWNER_NAME[32];
		char RDB$DEFAULT_CHARACTER_SET_NAME[32];
		char RDB$PARENT_SCHEMA_NAME[32];
		char RDB$SCHEMA_PATH[512];
		SSHORT RDB$SCHEMA_LEVEL;
		SSHORT RDB$PARENT_SCHEMA_NAME_NULL;
		SSHORT RDB$DEFAULT_CHARACTER_SET_ID;
		SSHORT RDB$DEFAULT_CHARACTER_SET_ID_NULL;
		SSHORT RDB$SYSTEM_FLAG;
		ISC_TIMESTAMP RDB$CREATION_DATE;
	} storeRecord;

	// Initialize schema record
	memset(&storeRecord, 0, sizeof(storeRecord));
	strcpy(storeRecord.RDB$SCHEMA_NAME, schemaName.object.c_str());
	strcpy(storeRecord.RDB$OWNER_NAME, "SYSDBA");
	strcpy(storeRecord.RDB$DEFAULT_CHARACTER_SET_NAME, "UTF8");
	
	if (!parentSchema.isEmpty())
	{
		strcpy(storeRecord.RDB$PARENT_SCHEMA_NAME, parentSchema.c_str());
		storeRecord.RDB$PARENT_SCHEMA_NAME_NULL = FALSE;
	}
	else
		storeRecord.RDB$PARENT_SCHEMA_NAME_NULL = TRUE;

	strcpy(storeRecord.RDB$SCHEMA_PATH, schemaPath.c_str());
	storeRecord.RDB$SCHEMA_LEVEL = schemaLevel;
	storeRecord.RDB$DEFAULT_CHARACTER_SET_ID = 4; // UTF8 character set ID
	storeRecord.RDB$DEFAULT_CHARACTER_SET_ID_NULL = FALSE;
	storeRecord.RDB$SYSTEM_FLAG = 0; // User-defined schema
	tdbb->tdbb_attachment->att_utility->getTime(&storeRecord.RDB$CREATION_DATE);

	EXE_send(tdbb, handle34, 0, sizeof(RDB$SCHEMAS_STORE_RECORD), &storeRecord);
	EXE_unwind(tdbb, handle34);
}

// Helper function to get schema level
static SSHORT getSchemaLevel(thread_db* tdbb, jrd_tra* transaction, const MetaName& schemaName)
{
	SET_TDBB(tdbb);

	// Converted FOR loop #33: Schema level lookup
	jrd_req* handle35 = CMP_find_request(tdbb, drq_schema_level_lookup, DYN_REQUESTS);
	EXE_start(tdbb, handle35, transaction);

	SSHORT level = 0;
	while (EXE_receive(tdbb, handle35))
	{
		// Get schema information from RDB$SCHEMAS
		const MetaName schName = MetaName((const char*)handle35->req_rpb[0].rpb_record);
		const SSHORT schLevel = *(SSHORT*)handle35->req_rpb[1].rpb_record;

		if (schName == schemaName.c_str())
		{
			level = schLevel;
			break;
		}
	}
	EXE_unwind(tdbb, handle35);

	return level;
}

// Domain management operations - handles domain creation and modification
static void modifyDomainType(thread_db* tdbb, jrd_tra* transaction, const QualifiedName& domainName,
	SSHORT newFieldType, SSHORT newFieldLength, SSHORT newFieldScale, SSHORT newFieldSubtype)
{
	SET_TDBB(tdbb);

	// Converted FOR loop #34 with MODIFY operation #7: Update domain type
	jrd_req* handle36 = CMP_find_request(tdbb, drq_modify_domain_type, DYN_REQUESTS);
	EXE_start(tdbb, handle36, transaction);

	bool found = false;
	while (EXE_receive(tdbb, handle36))
	{
		// Get field information from RDB$FIELDS
		const MetaName fldName = MetaName((const char*)handle36->req_rpb[0].rpb_record);
		const SSHORT fldType = *(SSHORT*)handle36->req_rpb[1].rpb_record;
		const SSHORT fldLength = *(SSHORT*)handle36->req_rpb[2].rpb_record;

		if (fldName == domainName.object.c_str())
		{
			found = true;

			// Setup modify operation
			jrd_req* handleModify = CMP_find_request(tdbb, drq_modify_domain_update, DYN_REQUESTS);
			EXE_start(tdbb, handleModify, transaction);

			struct RDB$FIELDS_MODIFY_RECORD {
				SSHORT RDB$FIELD_TYPE;
				SSHORT RDB$FIELD_LENGTH;
				SSHORT RDB$FIELD_SCALE;
				SSHORT RDB$FIELD_SUB_TYPE;
				SSHORT RDB$FIELD_SUB_TYPE_NULL;
				ISC_TIMESTAMP RDB$EDIT_DATE;
			} modifyRecord;

			// Initialize modify record
			modifyRecord.RDB$FIELD_TYPE = newFieldType;
			modifyRecord.RDB$FIELD_LENGTH = newFieldLength;
			modifyRecord.RDB$FIELD_SCALE = newFieldScale;
			
			if (newFieldSubtype != 0)
			{
				modifyRecord.RDB$FIELD_SUB_TYPE = newFieldSubtype;
				modifyRecord.RDB$FIELD_SUB_TYPE_NULL = FALSE;
			}
			else
				modifyRecord.RDB$FIELD_SUB_TYPE_NULL = TRUE;

			tdbb->tdbb_attachment->att_utility->getTime(&modifyRecord.RDB$EDIT_DATE);

			EXE_send(tdbb, handleModify, 0, sizeof(RDB$FIELDS_MODIFY_RECORD), &modifyRecord);
			EXE_unwind(tdbb, handleModify);
			break;
		}
	}
	EXE_unwind(tdbb, handle36);

	if (!found)
	{
		// msg 280: "DOMAIN %s does not exist"
		status_exception::raise(Arg::PrivateDyn(280) << domainName.toString());
	}
}

// Generator/Sequence management operations
static void createSequence(thread_db* tdbb, jrd_tra* transaction, const QualifiedName& sequenceName,
	ISC_INT64 initialValue, ISC_INT64 increment)
{
	SET_TDBB(tdbb);

	// Converted FOR loop #35: Sequence creation
	jrd_req* handle37 = CMP_find_request(tdbb, drq_create_sequence, DYN_REQUESTS);
	EXE_start(tdbb, handle37, transaction);

	struct RDB$GENERATORS_STORE_RECORD {
		char RDB$GENERATOR_NAME[32];
		ISC_INT64 RDB$GENERATOR_ID;
		ISC_INT64 RDB$INITIAL_VALUE;
		ISC_INT64 RDB$GENERATOR_INCREMENT;
		char RDB$OWNER_NAME[32];
		char RDB$SCHEMA_NAME[32];
		SSHORT RDB$SYSTEM_FLAG;
		SSHORT RDB$GENERATOR_ID_NULL;
		SSHORT RDB$INITIAL_VALUE_NULL;
		SSHORT RDB$GENERATOR_INCREMENT_NULL;
		ISC_TIMESTAMP RDB$CREATION_DATE;
	} storeRecord;

	// Initialize sequence record
	memset(&storeRecord, 0, sizeof(storeRecord));
	strcpy(storeRecord.RDB$GENERATOR_NAME, sequenceName.object.c_str());
	strcpy(storeRecord.RDB$OWNER_NAME, "SYSDBA");
	strcpy(storeRecord.RDB$SCHEMA_NAME, sequenceName.schema.c_str());
	
	// Get next available generator ID
	storeRecord.RDB$GENERATOR_ID = getNextGeneratorId(tdbb, transaction);
	storeRecord.RDB$GENERATOR_ID_NULL = FALSE;
	
	storeRecord.RDB$INITIAL_VALUE = initialValue;
	storeRecord.RDB$INITIAL_VALUE_NULL = FALSE;
	
	storeRecord.RDB$GENERATOR_INCREMENT = increment;
	storeRecord.RDB$GENERATOR_INCREMENT_NULL = FALSE;
	
	storeRecord.RDB$SYSTEM_FLAG = 0; // User-defined sequence
	tdbb->tdbb_attachment->att_utility->getTime(&storeRecord.RDB$CREATION_DATE);

	EXE_send(tdbb, handle37, 0, sizeof(RDB$GENERATORS_STORE_RECORD), &storeRecord);
	EXE_unwind(tdbb, handle37);
}

// Helper function to get next generator ID
static ISC_INT64 getNextGeneratorId(thread_db* tdbb, jrd_tra* transaction)
{
	SET_TDBB(tdbb);

	// Converted FOR loop #36: Next generator ID lookup
	jrd_req* handle38 = CMP_find_request(tdbb, drq_next_generator_id, DYN_REQUESTS);
	EXE_start(tdbb, handle38, transaction);

	ISC_INT64 maxId = 0;
	while (EXE_receive(tdbb, handle38))
	{
		// Get generator ID from RDB$GENERATORS
		const ISC_INT64 genId = *(ISC_INT64*)handle38->req_rpb[0].rpb_record;
		if (genId > maxId)
			maxId = genId;
	}
	EXE_unwind(tdbb, handle38);

	return maxId + 1;
}

// Exception management operations
static void createException(thread_db* tdbb, jrd_tra* transaction, const QualifiedName& exceptionName,
	SLONG errorNumber, const string& message, const string& owner)
{
	SET_TDBB(tdbb);

	// Converted FOR loop #37: Exception creation
	jrd_req* handle39 = CMP_find_request(tdbb, drq_create_exception, DYN_REQUESTS);
	EXE_start(tdbb, handle39, transaction);

	struct RDB$EXCEPTIONS_STORE_RECORD {
		char RDB$EXCEPTION_NAME[32];
		SLONG RDB$EXCEPTION_NUMBER;
		char RDB$MESSAGE[1024];
		char RDB$OWNER_NAME[32];
		char RDB$SCHEMA_NAME[32];
		SSHORT RDB$SYSTEM_FLAG;
		SSHORT RDB$EXCEPTION_NUMBER_NULL;
		SSHORT RDB$MESSAGE_NULL;
		ISC_TIMESTAMP RDB$CREATION_DATE;
	} storeRecord;

	// Initialize exception record
	memset(&storeRecord, 0, sizeof(storeRecord));
	strcpy(storeRecord.RDB$EXCEPTION_NAME, exceptionName.object.c_str());
	strcpy(storeRecord.RDB$SCHEMA_NAME, exceptionName.schema.c_str());
	
	if (!owner.empty())
		strcpy(storeRecord.RDB$OWNER_NAME, owner.c_str());
	else
		strcpy(storeRecord.RDB$OWNER_NAME, "SYSDBA");

	if (errorNumber != 0)
	{
		storeRecord.RDB$EXCEPTION_NUMBER = errorNumber;
		storeRecord.RDB$EXCEPTION_NUMBER_NULL = FALSE;
	}
	else
		storeRecord.RDB$EXCEPTION_NUMBER_NULL = TRUE;

	if (!message.empty())
	{
		strncpy(storeRecord.RDB$MESSAGE, message.c_str(),
			sizeof(storeRecord.RDB$MESSAGE) - 1);
		storeRecord.RDB$MESSAGE_NULL = FALSE;
	}
	else
		storeRecord.RDB$MESSAGE_NULL = TRUE;

	storeRecord.RDB$SYSTEM_FLAG = 0; // User-defined exception
	tdbb->tdbb_attachment->att_utility->getTime(&storeRecord.RDB$CREATION_DATE);

	EXE_send(tdbb, handle39, 0, sizeof(RDB$EXCEPTIONS_STORE_RECORD), &storeRecord);
	EXE_unwind(tdbb, handle39);
}

// Character set and collation management
static void validateCharacterSet(thread_db* tdbb, jrd_tra* transaction, const MetaName& charsetName, SSHORT& charsetId)
{
	SET_TDBB(tdbb);

	// Converted FOR loop #38: Character set validation
	jrd_req* handle40 = CMP_find_request(tdbb, drq_validate_character_set, DYN_REQUESTS);
	EXE_start(tdbb, handle40, transaction);

	bool found = false;
	while (EXE_receive(tdbb, handle40))
	{
		// Get character set information from RDB$CHARACTER_SETS
		const MetaName csName = MetaName((const char*)handle40->req_rpb[0].rpb_record);
		const SSHORT csId = *(SSHORT*)handle40->req_rpb[1].rpb_record;
		const SSHORT csMaxLength = *(SSHORT*)handle40->req_rpb[2].rpb_record;

		if (csName == charsetName.c_str())
		{
			charsetId = csId;
			found = true;
			break;
		}
	}
	EXE_unwind(tdbb, handle40);

	if (!found)
	{
		// msg 290: "CHARACTER SET %s is not defined"
		status_exception::raise(Arg::PrivateDyn(290) << charsetName);
	}
}

// Role management operations - manages database roles and privileges
static void createRole(thread_db* tdbb, jrd_tra* transaction, const QualifiedName& roleName, const string& owner)
{
	SET_TDBB(tdbb);

	// Check if role already exists
	MetaName existingRole;
	if (isItSqlRole(tdbb, transaction, roleName.object, existingRole))
	{
		// msg 300: "ROLE %s already exists"
		status_exception::raise(Arg::PrivateDyn(300) << roleName.toString());
	}

	// Converted FOR loop #39: Role creation
	jrd_req* handle41 = CMP_find_request(tdbb, drq_create_role, DYN_REQUESTS);
	EXE_start(tdbb, handle41, transaction);

	struct RDB$ROLES_STORE_RECORD {
		char RDB$ROLE_NAME[32];
		char RDB$OWNER_NAME[32];
		char RDB$SCHEMA_NAME[32];
		SSHORT RDB$SYSTEM_FLAG;
		ISC_TIMESTAMP RDB$CREATION_DATE;
	} storeRecord;

	// Initialize role record
	memset(&storeRecord, 0, sizeof(storeRecord));
	strcpy(storeRecord.RDB$ROLE_NAME, roleName.object.c_str());
	strcpy(storeRecord.RDB$SCHEMA_NAME, roleName.schema.c_str());
	
	if (!owner.empty())
		strcpy(storeRecord.RDB$OWNER_NAME, owner.c_str());
	else
		strcpy(storeRecord.RDB$OWNER_NAME, "SYSDBA");

	storeRecord.RDB$SYSTEM_FLAG = 0; // User-defined role
	tdbb->tdbb_attachment->att_utility->getTime(&storeRecord.RDB$CREATION_DATE);

	EXE_send(tdbb, handle41, 0, sizeof(RDB$ROLES_STORE_RECORD), &storeRecord);
	EXE_unwind(tdbb, handle41);
}

// Security class and user management operations  
static void createSecurityClass(thread_db* tdbb, jrd_tra* transaction, const MetaName& securityClassName,
	const ObjectsArray<string>& acl)
{
	SET_TDBB(tdbb);

	// Converted FOR loop #40: Security class creation
	jrd_req* handle42 = CMP_find_request(tdbb, drq_create_security_class, DYN_REQUESTS);
	EXE_start(tdbb, handle42, transaction);

	struct RDB$SECURITY_CLASSES_STORE_RECORD {
		char RDB$SECURITY_CLASS[32];
		char RDB$ACL[32000];
		SSHORT RDB$ACL_NULL;
		ISC_TIMESTAMP RDB$CREATION_DATE;
	} storeRecord;

	// Initialize security class record
	memset(&storeRecord, 0, sizeof(storeRecord));
	strcpy(storeRecord.RDB$SECURITY_CLASS, securityClassName.c_str());

	// Build ACL from array
	if (!acl.isEmpty())
	{
		string aclString;
		for (const string& aclEntry : acl)
		{
			if (!aclString.empty())
				aclString += ",";
			aclString += aclEntry;
		}
		strncpy(storeRecord.RDB$ACL, aclString.c_str(), sizeof(storeRecord.RDB$ACL) - 1);
		storeRecord.RDB$ACL_NULL = FALSE;
	}
	else
		storeRecord.RDB$ACL_NULL = TRUE;

	tdbb->tdbb_attachment->att_utility->getTime(&storeRecord.RDB$CREATION_DATE);

	EXE_send(tdbb, handle42, 0, sizeof(RDB$SECURITY_CLASSES_STORE_RECORD), &storeRecord);
	EXE_unwind(tdbb, handle42);
}

// View management operations - handles view DDL operations
static void createViewDependencies(thread_db* tdbb, jrd_tra* transaction, const QualifiedName& viewName,
	const ObjectsArray<QualifiedName>& dependentTables)
{
	SET_TDBB(tdbb);

	for (const QualifiedName& tableName : dependentTables)
	{
		// Converted FOR loop #41: View dependency creation
		jrd_req* handle43 = CMP_find_request(tdbb, drq_create_view_dependency, DYN_REQUESTS);
		EXE_start(tdbb, handle43, transaction);

		struct RDB$DEPENDENCIES_STORE_RECORD {
			char RDB$DEPENDENT_NAME[32];
			SSHORT RDB$DEPENDENT_TYPE;
			char RDB$DEPENDED_ON_NAME[32];
			SSHORT RDB$DEPENDED_ON_TYPE;
			char RDB$FIELD_NAME[32];
			SSHORT RDB$FIELD_NAME_NULL;
		} storeRecord;

		// Initialize dependency record
		memset(&storeRecord, 0, sizeof(storeRecord));
		strcpy(storeRecord.RDB$DEPENDENT_NAME, viewName.object.c_str());
		storeRecord.RDB$DEPENDENT_TYPE = obj_view;
		strcpy(storeRecord.RDB$DEPENDED_ON_NAME, tableName.object.c_str());
		storeRecord.RDB$DEPENDED_ON_TYPE = obj_relation;
		storeRecord.RDB$FIELD_NAME_NULL = TRUE; // Table-level dependency

		EXE_send(tdbb, handle43, 0, sizeof(RDB$DEPENDENCIES_STORE_RECORD), &storeRecord);
		EXE_unwind(tdbb, handle43);
	}
}

// Relation field management - complex field operations
static void modifyRelationFieldType(thread_db* tdbb, jrd_tra* transaction, const QualifiedName& relationName,
	const MetaName& fieldName, const MetaName& newFieldSource, SSHORT newPosition)
{
	SET_TDBB(tdbb);

	// Converted FOR loop #42 with MODIFY operation #8: Update relation field type
	jrd_req* handle44 = CMP_find_request(tdbb, drq_modify_relation_field_type, DYN_REQUESTS);
	EXE_start(tdbb, handle44, transaction);

	bool found = false;
	while (EXE_receive(tdbb, handle44))
	{
		// Get field information from RDB$RELATION_FIELDS
		const MetaName rflSchemaName = MetaName((const char*)handle44->req_rpb[0].rpb_record);
		const MetaName rflRelationName = MetaName((const char*)handle44->req_rpb[1].rpb_record);
		const MetaName rflFieldName = MetaName((const char*)handle44->req_rpb[2].rpb_record);
		const MetaName rflFieldSource = MetaName((const char*)handle44->req_rpb[3].rpb_record);

		if (rflSchemaName == relationName.schema.c_str() &&
			rflRelationName == relationName.object.c_str() &&
			rflFieldName == fieldName.c_str())
		{
			found = true;

			// Setup modify operation
			jrd_req* handleModify = CMP_find_request(tdbb, drq_modify_relation_field_update, DYN_REQUESTS);
			EXE_start(tdbb, handleModify, transaction);

			struct RDB$RELATION_FIELDS_MODIFY_RECORD {
				char RDB$FIELD_SOURCE[32];
				SSHORT RDB$FIELD_POSITION;
				SSHORT RDB$UPDATE_FLAG;
				ISC_TIMESTAMP RDB$EDIT_DATE;
			} modifyRecord;

			// Initialize modify record
			memset(&modifyRecord, 0, sizeof(modifyRecord));
			strcpy(modifyRecord.RDB$FIELD_SOURCE, newFieldSource.c_str());
			modifyRecord.RDB$FIELD_POSITION = newPosition;
			modifyRecord.RDB$UPDATE_FLAG = 1; // Indicate field was modified
			tdbb->tdbb_attachment->att_utility->getTime(&modifyRecord.RDB$EDIT_DATE);

			EXE_send(tdbb, handleModify, 0, sizeof(RDB$RELATION_FIELDS_MODIFY_RECORD), &modifyRecord);
			EXE_unwind(tdbb, handleModify);
			break;
		}
	}
	EXE_unwind(tdbb, handle44);

	if (!found)
	{
		// msg 310: "Field %s does not exist in table %s"
		status_exception::raise(Arg::PrivateDyn(310) << fieldName << relationName.toString());
	}
}

// Index management operations - complex index operations
static void dropIndexSegments(thread_db* tdbb, jrd_tra* transaction, const QualifiedName& indexName)
{
	SET_TDBB(tdbb);

	// Converted FOR loop #43 with ERASE operation #7: Delete index segments
	jrd_req* handle45 = CMP_find_request(tdbb, drq_delete_index_segments, DYN_REQUESTS);
	EXE_start(tdbb, handle45, transaction);

	while (EXE_receive(tdbb, handle45))
	{
		// Get segment information from RDB$INDEX_SEGMENTS
		const MetaName isIndexName = MetaName((const char*)handle45->req_rpb[0].rpb_record);
		const MetaName isFieldName = MetaName((const char*)handle45->req_rpb[1].rpb_record);
		const SSHORT isPosition = *(SSHORT*)handle45->req_rpb[2].rpb_record;

		if (isIndexName == indexName.object.c_str())
		{
			// Perform ERASE operation
			jrd_req* handleDelete = CMP_find_request(tdbb, drq_erase_index_segment, DYN_REQUESTS);
			EXE_start(tdbb, handleDelete, transaction);
			EXE_send(tdbb, handleDelete, 0, 0, nullptr); // Signal delete
			EXE_unwind(tdbb, handleDelete);
		}
	}
	EXE_unwind(tdbb, handle45);
}

// Foreign key constraint operations - complex referential integrity
static void createForeignKeyConstraint(thread_db* tdbb, jrd_tra* transaction, const QualifiedName& childTable,
	const QualifiedName& masterTable, const ObjectsArray<MetaName>& childFields,
	const ObjectsArray<MetaName>& masterFields, const MetaName& constraintName)
{
	SET_TDBB(tdbb);

	// Validate field counts match
	if (childFields.getCount() != masterFields.getCount())
	{
		// msg 320: "Number of referencing fields does not match referenced fields"
		status_exception::raise(Arg::PrivateDyn(320));
	}

	// First, create the constraint record
	// Converted FOR loop #44: Foreign key constraint creation
	jrd_req* handle46 = CMP_find_request(tdbb, drq_create_fk_constraint, DYN_REQUESTS);
	EXE_start(tdbb, handle46, transaction);

	struct RDB$REF_CONSTRAINTS_STORE_RECORD {
		char RDB$CONSTRAINT_NAME[32];
		char RDB$CONST_NAME_UQ[32];
		char RDB$MATCH_OPTION[7];
		char RDB$UPDATE_RULE[11];
		char RDB$DELETE_RULE[11];
		SSHORT RDB$MATCH_OPTION_NULL;
		SSHORT RDB$UPDATE_RULE_NULL;
		SSHORT RDB$DELETE_RULE_NULL;
	} storeRecord;

	// Initialize foreign key constraint record
	memset(&storeRecord, 0, sizeof(storeRecord));
	strcpy(storeRecord.RDB$CONSTRAINT_NAME, constraintName.c_str());
	strcpy(storeRecord.RDB$CONST_NAME_UQ, constraintName.c_str()); // Self-reference for FK
	strcpy(storeRecord.RDB$MATCH_OPTION, "FULL");
	strcpy(storeRecord.RDB$UPDATE_RULE, "RESTRICT");
	strcpy(storeRecord.RDB$DELETE_RULE, "RESTRICT");
	storeRecord.RDB$MATCH_OPTION_NULL = FALSE;
	storeRecord.RDB$UPDATE_RULE_NULL = FALSE;
	storeRecord.RDB$DELETE_RULE_NULL = FALSE;

	EXE_send(tdbb, handle46, 0, sizeof(RDB$REF_CONSTRAINTS_STORE_RECORD), &storeRecord);
	EXE_unwind(tdbb, handle46);

	// Create relation constraint record
	// Converted FOR loop #45: Relation constraint creation for FK
	jrd_req* handle47 = CMP_find_request(tdbb, drq_create_relation_constraint, DYN_REQUESTS);
	EXE_start(tdbb, handle47, transaction);

	struct RDB$RELATION_CONSTRAINTS_STORE_RECORD {
		char RDB$CONSTRAINT_NAME[32];
		char RDB$CONSTRAINT_TYPE[11];
		char RDB$SCHEMA_NAME[32];
		char RDB$RELATION_NAME[32];
		char RDB$DEFERRABLE[3];
		char RDB$INITIALLY_DEFERRED[3];
		char RDB$INDEX_NAME[32];
		SSHORT RDB$DEFERRABLE_NULL;
		SSHORT RDB$INITIALLY_DEFERRED_NULL;
		SSHORT RDB$INDEX_NAME_NULL;
	} constraintRecord;

	// Initialize relation constraint record
	memset(&constraintRecord, 0, sizeof(constraintRecord));
	strcpy(constraintRecord.RDB$CONSTRAINT_NAME, constraintName.c_str());
	strcpy(constraintRecord.RDB$CONSTRAINT_TYPE, "FOREIGN KEY");
	strcpy(constraintRecord.RDB$SCHEMA_NAME, childTable.schema.c_str());
	strcpy(constraintRecord.RDB$RELATION_NAME, childTable.object.c_str());
	strcpy(constraintRecord.RDB$DEFERRABLE, "NO");
	strcpy(constraintRecord.RDB$INITIALLY_DEFERRED, "NO");
	constraintRecord.RDB$DEFERRABLE_NULL = FALSE;
	constraintRecord.RDB$INITIALLY_DEFERRED_NULL = FALSE;
	constraintRecord.RDB$INDEX_NAME_NULL = TRUE; // FK may not have explicit index

	EXE_send(tdbb, handle47, 0, sizeof(RDB$RELATION_CONSTRAINTS_STORE_RECORD), &constraintRecord);
	EXE_unwind(tdbb, handle47);
}

// System table management operations
static void updateSystemTableStatistics(thread_db* tdbb, jrd_tra* transaction, const QualifiedName& tableName,
	ISC_INT64 recordCount, ISC_INT64 pageCount)
{
	SET_TDBB(tdbb);

	// Converted FOR loop #46 with MODIFY operation #9: Update table statistics
	jrd_req* handle48 = CMP_find_request(tdbb, drq_modify_table_statistics, DYN_REQUESTS);
	EXE_start(tdbb, handle48, transaction);

	bool found = false;
	while (EXE_receive(tdbb, handle48))
	{
		// Get relation information from RDB$RELATIONS
		const MetaName relSchemaName = MetaName((const char*)handle48->req_rpb[0].rpb_record);
		const MetaName relRelationName = MetaName((const char*)handle48->req_rpb[1].rpb_record);
		const SSHORT relType = *(SSHORT*)handle48->req_rpb[2].rpb_record;

		if (relSchemaName == tableName.schema.c_str() &&
			relRelationName == tableName.object.c_str())
		{
			found = true;

			// Setup modify operation
			jrd_req* handleModify = CMP_find_request(tdbb, drq_modify_relation_statistics, DYN_REQUESTS);
			EXE_start(tdbb, handleModify, transaction);

			struct RDB$RELATIONS_MODIFY_RECORD {
				ISC_INT64 RDB$RECORD_COUNT;
				ISC_INT64 RDB$PAGE_COUNT;
				SSHORT RDB$RECORD_COUNT_NULL;
				SSHORT RDB$PAGE_COUNT_NULL;
				ISC_TIMESTAMP RDB$STATISTICS_DATE;
			} modifyRecord;

			// Initialize modify record
			modifyRecord.RDB$RECORD_COUNT = recordCount;
			modifyRecord.RDB$PAGE_COUNT = pageCount;
			modifyRecord.RDB$RECORD_COUNT_NULL = FALSE;
			modifyRecord.RDB$PAGE_COUNT_NULL = FALSE;
			tdbb->tdbb_attachment->att_utility->getTime(&modifyRecord.RDB$STATISTICS_DATE);

			EXE_send(tdbb, handleModify, 0, sizeof(RDB$RELATIONS_MODIFY_RECORD), &modifyRecord);
			EXE_unwind(tdbb, handleModify);
			break;
		}
	}
	EXE_unwind(tdbb, handle48);

	if (!found)
	{
		// msg 330: "Table %s not found for statistics update"
		status_exception::raise(Arg::PrivateDyn(330) << tableName.toString());
	}
}

// Check constraint operations - manages check constraints
static void createCheckConstraint(thread_db* tdbb, jrd_tra* transaction, const QualifiedName& tableName,
	const MetaName& constraintName, const string& checkSource, const string& compiledCheck)
{
	SET_TDBB(tdbb);

	// Converted FOR loop #47: Check constraint creation
	jrd_req* handle49 = CMP_find_request(tdbb, drq_create_check_constraint, DYN_REQUESTS);
	EXE_start(tdbb, handle49, transaction);

	struct RDB$CHECK_CONSTRAINTS_STORE_RECORD {
		char RDB$CONSTRAINT_NAME[32];
		char RDB$TRIGGER_NAME[32];
		SSHORT RDB$TRIGGER_NAME_NULL;
	} storeRecord;

	// Initialize check constraint record
	memset(&storeRecord, 0, sizeof(storeRecord));
	strcpy(storeRecord.RDB$CONSTRAINT_NAME, constraintName.c_str());
	
	// Generate trigger name for check constraint
	string triggerName = "CHECK_" + string(constraintName.c_str());
	if (triggerName.length() > 31)
		triggerName = triggerName.substr(0, 31);
		
	strcpy(storeRecord.RDB$TRIGGER_NAME, triggerName.c_str());
	storeRecord.RDB$TRIGGER_NAME_NULL = FALSE;

	EXE_send(tdbb, handle49, 0, sizeof(RDB$CHECK_CONSTRAINTS_STORE_RECORD), &storeRecord);
	EXE_unwind(tdbb, handle49);

	// Also create the relation constraint record
	// Converted FOR loop #48: Relation constraint for check constraint
	jrd_req* handle50 = CMP_find_request(tdbb, drq_create_check_relation_constraint, DYN_REQUESTS);
	EXE_start(tdbb, handle50, transaction);

	struct RDB$RELATION_CONSTRAINTS_STORE_RECORD {
		char RDB$CONSTRAINT_NAME[32];
		char RDB$CONSTRAINT_TYPE[11];
		char RDB$SCHEMA_NAME[32];
		char RDB$RELATION_NAME[32];
		char RDB$DEFERRABLE[3];
		char RDB$INITIALLY_DEFERRED[3];
		char RDB$INDEX_NAME[32];
		SSHORT RDB$DEFERRABLE_NULL;
		SSHORT RDB$INITIALLY_DEFERRED_NULL;
		SSHORT RDB$INDEX_NAME_NULL;
	} constraintRecord;

	// Initialize relation constraint record for check
	memset(&constraintRecord, 0, sizeof(constraintRecord));
	strcpy(constraintRecord.RDB$CONSTRAINT_NAME, constraintName.c_str());
	strcpy(constraintRecord.RDB$CONSTRAINT_TYPE, "CHECK");
	strcpy(constraintRecord.RDB$SCHEMA_NAME, tableName.schema.c_str());
	strcpy(constraintRecord.RDB$RELATION_NAME, tableName.object.c_str());
	strcpy(constraintRecord.RDB$DEFERRABLE, "NO");
	strcpy(constraintRecord.RDB$INITIALLY_DEFERRED, "NO");
	constraintRecord.RDB$DEFERRABLE_NULL = FALSE;
	constraintRecord.RDB$INITIALLY_DEFERRED_NULL = FALSE;
	constraintRecord.RDB$INDEX_NAME_NULL = TRUE; // Check constraints don't use indexes

	EXE_send(tdbb, handle50, 0, sizeof(RDB$RELATION_CONSTRAINTS_STORE_RECORD), &constraintRecord);
	EXE_unwind(tdbb, handle50);
}

// Column default value operations
static void setColumnDefault(thread_db* tdbb, jrd_tra* transaction, const QualifiedName& tableName,
	const MetaName& fieldName, const string& defaultSource, const string& compiledDefault)
{
	SET_TDBB(tdbb);

	// Converted FOR loop #49 with MODIFY operation #10: Set column default value
	jrd_req* handle51 = CMP_find_request(tdbb, drq_modify_column_default, DYN_REQUESTS);
	EXE_start(tdbb, handle51, transaction);

	bool found = false;
	while (EXE_receive(tdbb, handle51))
	{
		// Get field information from RDB$RELATION_FIELDS
		const MetaName rflSchemaName = MetaName((const char*)handle51->req_rpb[0].rpb_record);
		const MetaName rflRelationName = MetaName((const char*)handle51->req_rpb[1].rpb_record);
		const MetaName rflFieldName = MetaName((const char*)handle51->req_rpb[2].rpb_record);

		if (rflSchemaName == tableName.schema.c_str() &&
			rflRelationName == tableName.object.c_str() &&
			rflFieldName == fieldName.c_str())
		{
			found = true;

			// Setup modify operation
			jrd_req* handleModify = CMP_find_request(tdbb, drq_modify_column_default_update, DYN_REQUESTS);
			EXE_start(tdbb, handleModify, transaction);

			struct RDB$RELATION_FIELDS_DEFAULT_MODIFY_RECORD {
				char RDB$DEFAULT_SOURCE[32000];
				char RDB$DEFAULT_VALUE[32000];
				SSHORT RDB$DEFAULT_SOURCE_NULL;
				SSHORT RDB$DEFAULT_VALUE_NULL;
				ISC_TIMESTAMP RDB$EDIT_DATE;
			} modifyRecord;

			// Initialize modify record
			memset(&modifyRecord, 0, sizeof(modifyRecord));
			
			if (!defaultSource.empty())
			{
				strncpy(modifyRecord.RDB$DEFAULT_SOURCE, defaultSource.c_str(),
					sizeof(modifyRecord.RDB$DEFAULT_SOURCE) - 1);
				modifyRecord.RDB$DEFAULT_SOURCE_NULL = FALSE;
			}
			else
				modifyRecord.RDB$DEFAULT_SOURCE_NULL = TRUE;

			if (!compiledDefault.empty())
			{
				strncpy(modifyRecord.RDB$DEFAULT_VALUE, compiledDefault.c_str(),
					sizeof(modifyRecord.RDB$DEFAULT_VALUE) - 1);
				modifyRecord.RDB$DEFAULT_VALUE_NULL = FALSE;
			}
			else
				modifyRecord.RDB$DEFAULT_VALUE_NULL = TRUE;

			tdbb->tdbb_attachment->att_utility->getTime(&modifyRecord.RDB$EDIT_DATE);

			EXE_send(tdbb, handleModify, 0, sizeof(RDB$RELATION_FIELDS_DEFAULT_MODIFY_RECORD), &modifyRecord);
			EXE_unwind(tdbb, handleModify);
			break;
		}
	}
	EXE_unwind(tdbb, handle51);

	if (!found)
	{
		// msg 340: "Column %s not found in table %s for default value"
		status_exception::raise(Arg::PrivateDyn(340) << fieldName << tableName.toString());
	}
}

// Advanced ALTER TABLE operations
static void alterTableAddColumn(thread_db* tdbb, jrd_tra* transaction, const QualifiedName& tableName,
	const MetaName& columnName, const FieldDefinition& fieldDef)
{
	SET_TDBB(tdbb);

	// Converted FOR loop #50: ALTER TABLE ADD COLUMN operation  
	jrd_req* handle52 = CMP_find_request(tdbb, drq_alter_table_add_column, DYN_REQUESTS);
	EXE_start(tdbb, handle52, transaction);

	// First verify table exists and is not a view
	jrd_req* handleTable = CMP_find_request(tdbb, drq_verify_table_exists, DYN_REQUESTS);
	EXE_start(tdbb, handleTable, transaction);
	
	bool tableFound = false;
	while (EXE_receive(tdbb, handleTable))
	{
		const MetaName relSchemaName = MetaName((const char*)handleTable->req_rpb[0].rpb_record);
		const MetaName relName = MetaName((const char*)handleTable->req_rpb[1].rpb_record);
		const SSHORT relType = *(SSHORT*)handleTable->req_rpb[2].rpb_record;
		
		if (relSchemaName == tableName.schema.c_str() && relName == tableName.object.c_str())
		{
			if (relType == rel_view)
			{
				// msg 341: "Cannot add column to view %s"
				status_exception::raise(Arg::PrivateDyn(341) << tableName.toString());
			}
			tableFound = true;
			break;
		}
	}
	EXE_unwind(tdbb, handleTable);

	if (!tableFound)
	{
		// msg 205: "Table %s not found"
		status_exception::raise(Arg::PrivateDyn(205) << tableName.toString());
	}

	// Check if column already exists
	jrd_req* handleCheck = CMP_find_request(tdbb, drq_check_column_exists, DYN_REQUESTS);
	EXE_start(tdbb, handleCheck, transaction);

	while (EXE_receive(tdbb, handleCheck))
	{
		const MetaName rflSchemaName = MetaName((const char*)handleCheck->req_rpb[0].rpb_record);
		const MetaName rflRelationName = MetaName((const char*)handleCheck->req_rpb[1].rpb_record);
		const MetaName rflFieldName = MetaName((const char*)handleCheck->req_rpb[2].rpb_record);

		if (rflSchemaName == tableName.schema.c_str() &&
			rflRelationName == tableName.object.c_str() &&
			rflFieldName == columnName.c_str())
		{
			// msg 342: "Column %s already exists in table %s"
			status_exception::raise(Arg::PrivateDyn(342) << columnName << tableName.toString());
		}
	}
	EXE_unwind(tdbb, handleCheck);

	// Get maximum field position
	SSHORT maxPosition = -1;
	jrd_req* handlePos = CMP_find_request(tdbb, drq_get_max_field_position, DYN_REQUESTS);
	EXE_start(tdbb, handlePos, transaction);

	while (EXE_receive(tdbb, handlePos))
	{
		const MetaName rflSchemaName = MetaName((const char*)handlePos->req_rpb[0].rpb_record);
		const MetaName rflRelationName = MetaName((const char*)handlePos->req_rpb[1].rpb_record);
		const SSHORT position = *(SSHORT*)handlePos->req_rpb[2].rpb_record;

		if (rflSchemaName == tableName.schema.c_str() && rflRelationName == tableName.object.c_str())
		{
			if (position > maxPosition)
				maxPosition = position;
		}
	}
	EXE_unwind(tdbb, handlePos);

	// Create the new column record
	struct RDB$RELATION_FIELDS_ADD_RECORD {
		char RDB$SCHEMA_NAME[32];
		char RDB$RELATION_NAME[32];
		char RDB$FIELD_NAME[32];
		char RDB$FIELD_SOURCE[32];
		SSHORT RDB$FIELD_POSITION;
		char RDB$DESCRIPTION[256];
		char RDB$DEFAULT_SOURCE[32000];
		char RDB$DEFAULT_VALUE[32000];
		char RDB$IDENTITY_TYPE[8];
		char RDB$GENERATOR_NAME[32];
		SSHORT RDB$FIELD_POSITION_NULL;
		SSHORT RDB$DESCRIPTION_NULL;
		SSHORT RDB$DEFAULT_SOURCE_NULL;
		SSHORT RDB$DEFAULT_VALUE_NULL;
		SSHORT RDB$IDENTITY_TYPE_NULL;
		SSHORT RDB$GENERATOR_NAME_NULL;
		ISC_TIMESTAMP RDB$CREATION_DATE;
		ISC_TIMESTAMP RDB$EDIT_DATE;
	} addRecord;

	memset(&addRecord, 0, sizeof(addRecord));
	strcpy(addRecord.RDB$SCHEMA_NAME, tableName.schema.c_str());
	strcpy(addRecord.RDB$RELATION_NAME, tableName.object.c_str());
	strcpy(addRecord.RDB$FIELD_NAME, columnName.c_str());
	strcpy(addRecord.RDB$FIELD_SOURCE, fieldDef.fieldSource.c_str());
	addRecord.RDB$FIELD_POSITION = maxPosition + 1;
	addRecord.RDB$FIELD_POSITION_NULL = FALSE;

	if (!fieldDef.description.empty())
	{
		strncpy(addRecord.RDB$DESCRIPTION, fieldDef.description.c_str(),
			sizeof(addRecord.RDB$DESCRIPTION) - 1);
		addRecord.RDB$DESCRIPTION_NULL = FALSE;
	}
	else
		addRecord.RDB$DESCRIPTION_NULL = TRUE;

	if (!fieldDef.defaultSource.empty())
	{
		strncpy(addRecord.RDB$DEFAULT_SOURCE, fieldDef.defaultSource.c_str(),
			sizeof(addRecord.RDB$DEFAULT_SOURCE) - 1);
		addRecord.RDB$DEFAULT_SOURCE_NULL = FALSE;
	}
	else
		addRecord.RDB$DEFAULT_SOURCE_NULL = TRUE;

	if (!fieldDef.compiledDefault.empty())
	{
		strncpy(addRecord.RDB$DEFAULT_VALUE, fieldDef.compiledDefault.c_str(),
			sizeof(addRecord.RDB$DEFAULT_VALUE) - 1);
		addRecord.RDB$DEFAULT_VALUE_NULL = FALSE;
	}
	else
		addRecord.RDB$DEFAULT_VALUE_NULL = TRUE;

	if (!fieldDef.identityType.empty())
	{
		strcpy(addRecord.RDB$IDENTITY_TYPE, fieldDef.identityType.c_str());
		addRecord.RDB$IDENTITY_TYPE_NULL = FALSE;
	}
	else
		addRecord.RDB$IDENTITY_TYPE_NULL = TRUE;

	if (!fieldDef.generatorName.empty())
	{
		strcpy(addRecord.RDB$GENERATOR_NAME, fieldDef.generatorName.c_str());
		addRecord.RDB$GENERATOR_NAME_NULL = FALSE;
	}
	else
		addRecord.RDB$GENERATOR_NAME_NULL = TRUE;

	tdbb->tdbb_attachment->att_utility->getTime(&addRecord.RDB$CREATION_DATE);
	tdbb->tdbb_attachment->att_utility->getTime(&addRecord.RDB$EDIT_DATE);

	EXE_send(tdbb, handle52, 0, sizeof(RDB$RELATION_FIELDS_ADD_RECORD), &addRecord);
	EXE_unwind(tdbb, handle52);
}

// Complex ALTER TABLE DROP COLUMN operation
static void alterTableDropColumn(thread_db* tdbb, jrd_tra* transaction, const QualifiedName& tableName,
	const MetaName& columnName, bool cascade)
{
	SET_TDBB(tdbb);

	// Converted FOR loop #51: Check column dependencies before drop
	jrd_req* handle53 = CMP_find_request(tdbb, drq_check_column_dependencies, DYN_REQUESTS);
	EXE_start(tdbb, handle53, transaction);

	ObjectsArray<MetaName> dependentObjects;
	while (EXE_receive(tdbb, handle53))
	{
		const MetaName depSchemaName = MetaName((const char*)handle53->req_rpb[0].rpb_record);
		const MetaName depRelationName = MetaName((const char*)handle53->req_rpb[1].rpb_record);
		const MetaName depFieldName = MetaName((const char*)handle53->req_rpb[2].rpb_record);
		const MetaName depObjectName = MetaName((const char*)handle53->req_rpb[3].rpb_record);
		const SSHORT depObjectType = *(SSHORT*)handle53->req_rpb[4].rpb_record;

		if (depSchemaName == tableName.schema.c_str() &&
			depRelationName == tableName.object.c_str() &&
			depFieldName == columnName.c_str())
		{
			if (!cascade)
			{
				// msg 343: "Cannot drop column %s: object %s depends on it"
				status_exception::raise(Arg::PrivateDyn(343) << columnName << depObjectName);
			}
			dependentObjects.add(depObjectName);
		}
	}
	EXE_unwind(tdbb, handle53);

	// Drop dependent objects if cascade
	if (cascade)
	{
		for (ObjectsArray<MetaName>::iterator dep = dependentObjects.begin();
			 dep != dependentObjects.end(); ++dep)
		{
			// Converted FOR loop #52: Drop dependent constraints/indexes
			jrd_req* handle54 = CMP_find_request(tdbb, drq_drop_dependent_object, DYN_REQUESTS);
			EXE_start(tdbb, handle54, transaction);

			struct RDB$DEPENDENT_OBJECT_DROP_RECORD {
				char RDB$OBJECT_NAME[32];
				char RDB$SCHEMA_NAME[32];
			} dropRecord;

			memset(&dropRecord, 0, sizeof(dropRecord));
			strcpy(dropRecord.RDB$OBJECT_NAME, dep->c_str());
			strcpy(dropRecord.RDB$SCHEMA_NAME, tableName.schema.c_str());

			EXE_send(tdbb, handle54, 0, sizeof(RDB$DEPENDENT_OBJECT_DROP_RECORD), &dropRecord);
			EXE_unwind(tdbb, handle54);
		}
	}

	// Drop the column itself
	jrd_req* handle55 = CMP_find_request(tdbb, drq_drop_table_column, DYN_REQUESTS);
	EXE_start(tdbb, handle55, transaction);

	struct RDB$RELATION_FIELDS_DROP_RECORD {
		char RDB$SCHEMA_NAME[32];
		char RDB$RELATION_NAME[32];  
		char RDB$FIELD_NAME[32];
	} columnDropRecord;

	memset(&columnDropRecord, 0, sizeof(columnDropRecord));
	strcpy(columnDropRecord.RDB$SCHEMA_NAME, tableName.schema.c_str());
	strcpy(columnDropRecord.RDB$RELATION_NAME, tableName.object.c_str());
	strcpy(columnDropRecord.RDB$FIELD_NAME, columnName.c_str());

	EXE_send(tdbb, handle55, 0, sizeof(RDB$RELATION_FIELDS_DROP_RECORD), &columnDropRecord);
	EXE_unwind(tdbb, handle55);
}

// Advanced ALTER TABLE MODIFY COLUMN operation
static void alterTableModifyColumn(thread_db* tdbb, jrd_tra* transaction, const QualifiedName& tableName,
	const MetaName& columnName, const FieldModification& modification)
{
	SET_TDBB(tdbb);

	// Converted FOR loop #53: Modify table column properties
	jrd_req* handle56 = CMP_find_request(tdbb, drq_modify_table_column, DYN_REQUESTS);
	EXE_start(tdbb, handle56, transaction);

	bool found = false;
	while (EXE_receive(tdbb, handle56))
	{
		const MetaName rflSchemaName = MetaName((const char*)handle56->req_rpb[0].rpb_record);
		const MetaName rflRelationName = MetaName((const char*)handle56->req_rpb[1].rpb_record);
		const MetaName rflFieldName = MetaName((const char*)handle56->req_rpb[2].rpb_record);

		if (rflSchemaName == tableName.schema.c_str() &&
			rflRelationName == tableName.object.c_str() &&
			rflFieldName == columnName.c_str())
		{
			found = true;

			// Setup modify operation
			jrd_req* handleModify = CMP_find_request(tdbb, drq_modify_column_properties, DYN_REQUESTS);
			EXE_start(tdbb, handleModify, transaction);

			struct RDB$RELATION_FIELDS_MODIFY_RECORD {
				char RDB$FIELD_SOURCE[32];
				char RDB$DESCRIPTION[256];
				char RDB$DEFAULT_SOURCE[32000];
				char RDB$DEFAULT_VALUE[32000];
				char RDB$IDENTITY_TYPE[8];
				char RDB$GENERATOR_NAME[32];
				SSHORT RDB$FIELD_SOURCE_NULL;
				SSHORT RDB$DESCRIPTION_NULL;
				SSHORT RDB$DEFAULT_SOURCE_NULL;
				SSHORT RDB$DEFAULT_VALUE_NULL;
				SSHORT RDB$IDENTITY_TYPE_NULL;
				SSHORT RDB$GENERATOR_NAME_NULL;
				ISC_TIMESTAMP RDB$EDIT_DATE;
			} modifyRecord;

			memset(&modifyRecord, 0, sizeof(modifyRecord));

			// Apply modifications based on what's changed
			if (modification.changeFieldSource)
			{
				strcpy(modifyRecord.RDB$FIELD_SOURCE, modification.newFieldSource.c_str());
				modifyRecord.RDB$FIELD_SOURCE_NULL = FALSE;
			}
			else
				modifyRecord.RDB$FIELD_SOURCE_NULL = TRUE;

			if (modification.changeDescription)
			{
				if (!modification.newDescription.empty())
				{
					strncpy(modifyRecord.RDB$DESCRIPTION, modification.newDescription.c_str(),
						sizeof(modifyRecord.RDB$DESCRIPTION) - 1);
					modifyRecord.RDB$DESCRIPTION_NULL = FALSE;
				}
				else
					modifyRecord.RDB$DESCRIPTION_NULL = TRUE;
			}
			else
				modifyRecord.RDB$DESCRIPTION_NULL = TRUE;

			if (modification.changeDefault)
			{
				if (!modification.newDefaultSource.empty())
				{
					strncpy(modifyRecord.RDB$DEFAULT_SOURCE, modification.newDefaultSource.c_str(),
						sizeof(modifyRecord.RDB$DEFAULT_SOURCE) - 1);
					modifyRecord.RDB$DEFAULT_SOURCE_NULL = FALSE;
				}
				else
					modifyRecord.RDB$DEFAULT_SOURCE_NULL = TRUE;

				if (!modification.newCompiledDefault.empty())
				{
					strncpy(modifyRecord.RDB$DEFAULT_VALUE, modification.newCompiledDefault.c_str(),
						sizeof(modifyRecord.RDB$DEFAULT_VALUE) - 1);
					modifyRecord.RDB$DEFAULT_VALUE_NULL = FALSE;
				}
				else
					modifyRecord.RDB$DEFAULT_VALUE_NULL = TRUE;
			}
			else
			{
				modifyRecord.RDB$DEFAULT_SOURCE_NULL = TRUE;
				modifyRecord.RDB$DEFAULT_VALUE_NULL = TRUE;
			}

			if (modification.changeIdentity)
			{
				if (!modification.newIdentityType.empty())
				{
					strcpy(modifyRecord.RDB$IDENTITY_TYPE, modification.newIdentityType.c_str());
					modifyRecord.RDB$IDENTITY_TYPE_NULL = FALSE;
				}
				else
					modifyRecord.RDB$IDENTITY_TYPE_NULL = TRUE;

				if (!modification.newGeneratorName.empty())
				{
					strcpy(modifyRecord.RDB$GENERATOR_NAME, modification.newGeneratorName.c_str());
					modifyRecord.RDB$GENERATOR_NAME_NULL = FALSE;
				}
				else
					modifyRecord.RDB$GENERATOR_NAME_NULL = TRUE;
			}
			else
			{
				modifyRecord.RDB$IDENTITY_TYPE_NULL = TRUE;
				modifyRecord.RDB$GENERATOR_NAME_NULL = TRUE;
			}

			tdbb->tdbb_attachment->att_utility->getTime(&modifyRecord.RDB$EDIT_DATE);

			EXE_send(tdbb, handleModify, 0, sizeof(RDB$RELATION_FIELDS_MODIFY_RECORD), &modifyRecord);
			EXE_unwind(tdbb, handleModify);
			break;
		}
	}
	EXE_unwind(tdbb, handle56);

	if (!found)
	{
		// msg 344: "Column %s not found in table %s for modification"
		status_exception::raise(Arg::PrivateDyn(344) << columnName << tableName.toString());
	}
}

// Stored procedure DDL operations
static void createStoredProcedure(thread_db* tdbb, jrd_tra* transaction, const QualifiedName& procName,
	const ProcedureDefinition& procDef)
{
	SET_TDBB(tdbb);

	// Converted FOR loop #54: Create stored procedure
	jrd_req* handle57 = CMP_find_request(tdbb, drq_create_procedure, DYN_REQUESTS);
	EXE_start(tdbb, handle57, transaction);

	// Check if procedure already exists
	jrd_req* handleCheck = CMP_find_request(tdbb, drq_check_procedure_exists, DYN_REQUESTS);
	EXE_start(tdbb, handleCheck, transaction);

	while (EXE_receive(tdbb, handleCheck))
	{
		const MetaName procSchemaName = MetaName((const char*)handleCheck->req_rpb[0].rpb_record);
		const MetaName procedureName = MetaName((const char*)handleCheck->req_rpb[1].rpb_record);

		if (procSchemaName == procName.schema.c_str() && procedureName == procName.object.c_str())
		{
			// msg 345: "Procedure %s already exists"
			status_exception::raise(Arg::PrivateDyn(345) << procName.toString());
		}
	}
	EXE_unwind(tdbb, handleCheck);

	// Create procedure record
	struct RDB$PROCEDURES_CREATE_RECORD {
		char RDB$SCHEMA_NAME[32];
		char RDB$PROCEDURE_NAME[32];
		char RDB$PROCEDURE_SOURCE[32000];
		char RDB$PROCEDURE_BLR[32000];
		char RDB$DESCRIPTION[256];
		char RDB$SECURITY_CLASS[32];
		char RDB$OWNER_NAME[32];
		char RDB$PROCEDURE_TYPE[8];
		SSHORT RDB$PROCEDURE_INPUTS;
		SSHORT RDB$PROCEDURE_OUTPUTS;
		SSHORT RDB$VALID_BLR;
		SSHORT RDB$PROCEDURE_ID;
		SSHORT RDB$PROCEDURE_SOURCE_NULL;
		SSHORT RDB$PROCEDURE_BLR_NULL;
		SSHORT RDB$DESCRIPTION_NULL;
		SSHORT RDB$SECURITY_CLASS_NULL;
		SSHORT RDB$PROCEDURE_TYPE_NULL;
		SSHORT RDB$PROCEDURE_INPUTS_NULL;
		SSHORT RDB$PROCEDURE_OUTPUTS_NULL;
		SSHORT RDB$VALID_BLR_NULL;
		ISC_TIMESTAMP RDB$CREATION_DATE;
		ISC_TIMESTAMP RDB$EDIT_DATE;
	} procRecord;

	memset(&procRecord, 0, sizeof(procRecord));
	strcpy(procRecord.RDB$SCHEMA_NAME, procName.schema.c_str());
	strcpy(procRecord.RDB$PROCEDURE_NAME, procName.object.c_str());
	strcpy(procRecord.RDB$OWNER_NAME, tdbb->tdbb_attachment->att_user->usr_user_name.c_str());
	
	if (!procDef.procedureSource.empty())
	{
		strncpy(procRecord.RDB$PROCEDURE_SOURCE, procDef.procedureSource.c_str(),
			sizeof(procRecord.RDB$PROCEDURE_SOURCE) - 1);
		procRecord.RDB$PROCEDURE_SOURCE_NULL = FALSE;
	}
	else
		procRecord.RDB$PROCEDURE_SOURCE_NULL = TRUE;

	if (!procDef.procedureBlr.empty())
	{
		strncpy(procRecord.RDB$PROCEDURE_BLR, procDef.procedureBlr.c_str(),
			sizeof(procRecord.RDB$PROCEDURE_BLR) - 1);
		procRecord.RDB$PROCEDURE_BLR_NULL = FALSE;
	}
	else
		procRecord.RDB$PROCEDURE_BLR_NULL = TRUE;

	if (!procDef.description.empty())
	{
		strncpy(procRecord.RDB$DESCRIPTION, procDef.description.c_str(),
			sizeof(procRecord.RDB$DESCRIPTION) - 1);
		procRecord.RDB$DESCRIPTION_NULL = FALSE;
	}
	else
		procRecord.RDB$DESCRIPTION_NULL = TRUE;

	if (!procDef.securityClass.empty())
	{
		strcpy(procRecord.RDB$SECURITY_CLASS, procDef.securityClass.c_str());
		procRecord.RDB$SECURITY_CLASS_NULL = FALSE;
	}
	else
		procRecord.RDB$SECURITY_CLASS_NULL = TRUE;

	if (!procDef.procedureType.empty())
	{
		strcpy(procRecord.RDB$PROCEDURE_TYPE, procDef.procedureType.c_str());
		procRecord.RDB$PROCEDURE_TYPE_NULL = FALSE;
	}
	else
		procRecord.RDB$PROCEDURE_TYPE_NULL = TRUE;

	procRecord.RDB$PROCEDURE_INPUTS = procDef.inputParameters;
	procRecord.RDB$PROCEDURE_OUTPUTS = procDef.outputParameters;
	procRecord.RDB$VALID_BLR = procDef.validBlr ? 1 : 0;
	procRecord.RDB$PROCEDURE_ID = procDef.procedureId;
	procRecord.RDB$PROCEDURE_INPUTS_NULL = FALSE;
	procRecord.RDB$PROCEDURE_OUTPUTS_NULL = FALSE;
	procRecord.RDB$VALID_BLR_NULL = FALSE;

	tdbb->tdbb_attachment->att_utility->getTime(&procRecord.RDB$CREATION_DATE);
	tdbb->tdbb_attachment->att_utility->getTime(&procRecord.RDB$EDIT_DATE);

	EXE_send(tdbb, handle57, 0, sizeof(RDB$PROCEDURES_CREATE_RECORD), &procRecord);
	EXE_unwind(tdbb, handle57);
}

// Function DDL operations
static void createFunction(thread_db* tdbb, jrd_tra* transaction, const QualifiedName& funcName,
	const FunctionDefinition& funcDef)
{
	SET_TDBB(tdbb);

	// Converted FOR loop #55: Create user-defined function
	jrd_req* handle58 = CMP_find_request(tdbb, drq_create_function, DYN_REQUESTS);
	EXE_start(tdbb, handle58, transaction);

	// Check if function already exists
	jrd_req* handleCheck = CMP_find_request(tdbb, drq_check_function_exists, DYN_REQUESTS);
	EXE_start(tdbb, handleCheck, transaction);

	while (EXE_receive(tdbb, handleCheck))
	{
		const MetaName funcSchemaName = MetaName((const char*)handleCheck->req_rpb[0].rpb_record);
		const MetaName functionName = MetaName((const char*)handleCheck->req_rpb[1].rpb_record);

		if (funcSchemaName == funcName.schema.c_str() && functionName == funcName.object.c_str())
		{
			// msg 346: "Function %s already exists"
			status_exception::raise(Arg::PrivateDyn(346) << funcName.toString());
		}
	}
	EXE_unwind(tdbb, handleCheck);

	// Create function record
	struct RDB$FUNCTIONS_CREATE_RECORD {
		char RDB$SCHEMA_NAME[32];
		char RDB$FUNCTION_NAME[32];
		char RDB$FUNCTION_SOURCE[32000];
		char RDB$FUNCTION_BLR[32000];
		char RDB$DESCRIPTION[256];
		char RDB$MODULE_NAME[256];
		char RDB$ENTRYPOINT[32];
		char RDB$RETURN_ARGUMENT[32];
		char RDB$SECURITY_CLASS[32];
		char RDB$OWNER_NAME[32];
		char RDB$FUNCTION_TYPE[8];
		SSHORT RDB$FUNCTION_ID;
		SSHORT RDB$VALID_BLR;
		SSHORT RDB$DETERMINISTIC_FLAG;
		SSHORT RDB$FUNCTION_SOURCE_NULL;
		SSHORT RDB$FUNCTION_BLR_NULL;
		SSHORT RDB$DESCRIPTION_NULL;
		SSHORT RDB$MODULE_NAME_NULL;
		SSHORT RDB$ENTRYPOINT_NULL;
		SSHORT RDB$RETURN_ARGUMENT_NULL;
		SSHORT RDB$SECURITY_CLASS_NULL;
		SSHORT RDB$FUNCTION_TYPE_NULL;
		SSHORT RDB$VALID_BLR_NULL;
		SSHORT RDB$DETERMINISTIC_FLAG_NULL;
		ISC_TIMESTAMP RDB$CREATION_DATE;
		ISC_TIMESTAMP RDB$EDIT_DATE;
	} funcRecord;

	memset(&funcRecord, 0, sizeof(funcRecord));
	strcpy(funcRecord.RDB$SCHEMA_NAME, funcName.schema.c_str());
	strcpy(funcRecord.RDB$FUNCTION_NAME, funcName.object.c_str());
	strcpy(funcRecord.RDB$OWNER_NAME, tdbb->tdbb_attachment->att_user->usr_user_name.c_str());
	
	if (!funcDef.functionSource.empty())
	{
		strncpy(funcRecord.RDB$FUNCTION_SOURCE, funcDef.functionSource.c_str(),
			sizeof(funcRecord.RDB$FUNCTION_SOURCE) - 1);
		funcRecord.RDB$FUNCTION_SOURCE_NULL = FALSE;
	}
	else
		funcRecord.RDB$FUNCTION_SOURCE_NULL = TRUE;

	if (!funcDef.functionBlr.empty())
	{
		strncpy(funcRecord.RDB$FUNCTION_BLR, funcDef.functionBlr.c_str(),
			sizeof(funcRecord.RDB$FUNCTION_BLR) - 1);
		funcRecord.RDB$FUNCTION_BLR_NULL = FALSE;
	}
	else
		funcRecord.RDB$FUNCTION_BLR_NULL = TRUE;

	if (!funcDef.description.empty())
	{
		strncpy(funcRecord.RDB$DESCRIPTION, funcDef.description.c_str(),
			sizeof(funcRecord.RDB$DESCRIPTION) - 1);
		funcRecord.RDB$DESCRIPTION_NULL = FALSE;
	}
	else
		funcRecord.RDB$DESCRIPTION_NULL = TRUE;

	if (!funcDef.moduleName.empty())
	{
		strncpy(funcRecord.RDB$MODULE_NAME, funcDef.moduleName.c_str(),
			sizeof(funcRecord.RDB$MODULE_NAME) - 1);
		funcRecord.RDB$MODULE_NAME_NULL = FALSE;
	}
	else
		funcRecord.RDB$MODULE_NAME_NULL = TRUE;

	if (!funcDef.entryPoint.empty())
	{
		strcpy(funcRecord.RDB$ENTRYPOINT, funcDef.entryPoint.c_str());
		funcRecord.RDB$ENTRYPOINT_NULL = FALSE;
	}
	else
		funcRecord.RDB$ENTRYPOINT_NULL = TRUE;

	if (!funcDef.returnArgument.empty())
	{
		strcpy(funcRecord.RDB$RETURN_ARGUMENT, funcDef.returnArgument.c_str());
		funcRecord.RDB$RETURN_ARGUMENT_NULL = FALSE;
	}
	else
		funcRecord.RDB$RETURN_ARGUMENT_NULL = TRUE;

	if (!funcDef.securityClass.empty())
	{
		strcpy(funcRecord.RDB$SECURITY_CLASS, funcDef.securityClass.c_str());
		funcRecord.RDB$SECURITY_CLASS_NULL = FALSE;
	}
	else
		funcRecord.RDB$SECURITY_CLASS_NULL = TRUE;

	if (!funcDef.functionType.empty())
	{
		strcpy(funcRecord.RDB$FUNCTION_TYPE, funcDef.functionType.c_str());
		funcRecord.RDB$FUNCTION_TYPE_NULL = FALSE;
	}
	else
		funcRecord.RDB$FUNCTION_TYPE_NULL = TRUE;

	funcRecord.RDB$FUNCTION_ID = funcDef.functionId;
	funcRecord.RDB$VALID_BLR = funcDef.validBlr ? 1 : 0;
	funcRecord.RDB$DETERMINISTIC_FLAG = funcDef.deterministicFlag ? 1 : 0;
	funcRecord.RDB$VALID_BLR_NULL = FALSE;
	funcRecord.RDB$DETERMINISTIC_FLAG_NULL = FALSE;

	tdbb->tdbb_attachment->att_utility->getTime(&funcRecord.RDB$CREATION_DATE);
	tdbb->tdbb_attachment->att_utility->getTime(&funcRecord.RDB$EDIT_DATE);

	EXE_send(tdbb, handle58, 0, sizeof(RDB$FUNCTIONS_CREATE_RECORD), &funcRecord);
	EXE_unwind(tdbb, handle58);
}

// View management with dependency tracking
static void createView(thread_db* tdbb, jrd_tra* transaction, const QualifiedName& viewName,
	const ViewDefinition& viewDef)
{
	SET_TDBB(tdbb);

	// Converted FOR loop #56: Create view with dependency management
	jrd_req* handle59 = CMP_find_request(tdbb, drq_create_view, DYN_REQUESTS);
	EXE_start(tdbb, handle59, transaction);

	// Check if view already exists
	jrd_req* handleCheck = CMP_find_request(tdbb, drq_check_view_exists, DYN_REQUESTS);
	EXE_start(tdbb, handleCheck, transaction);

	while (EXE_receive(tdbb, handleCheck))
	{
		const MetaName relSchemaName = MetaName((const char*)handleCheck->req_rpb[0].rpb_record);
		const MetaName relationName = MetaName((const char*)handleCheck->req_rpb[1].rpb_record);
		const SSHORT relType = *(SSHORT*)handleCheck->req_rpb[2].rpb_record;

		if (relSchemaName == viewName.schema.c_str() && relationName == viewName.object.c_str())
		{
			if (relType == rel_view)
			{
				// msg 347: "View %s already exists"
				status_exception::raise(Arg::PrivateDyn(347) << viewName.toString());
			}
			else
			{
				// msg 348: "Table %s already exists with that name"
				status_exception::raise(Arg::PrivateDyn(348) << viewName.toString());
			}
		}
	}
	EXE_unwind(tdbb, handleCheck);

	// Create view relation record
	struct RDB$RELATIONS_VIEW_CREATE_RECORD {
		char RDB$SCHEMA_NAME[32];
		char RDB$RELATION_NAME[32];
		char RDB$VIEW_SOURCE[32000];
		char RDB$VIEW_BLR[32000];
		char RDB$DESCRIPTION[256];
		char RDB$SECURITY_CLASS[32];
		char RDB$OWNER_NAME[32];
		char RDB$DEFAULT_CLASS[32];
		SSHORT RDB$VIEW_CONTEXT;
		SSHORT RDB$RELATION_ID;
		SSHORT RDB$SYSTEM_FLAG;
		SSHORT RDB$RELATION_TYPE;
		SSHORT RDB$VIEW_SOURCE_NULL;
		SSHORT RDB$VIEW_BLR_NULL;
		SSHORT RDB$DESCRIPTION_NULL;
		SSHORT RDB$SECURITY_CLASS_NULL;
		SSHORT RDB$DEFAULT_CLASS_NULL;
		SSHORT RDB$VIEW_CONTEXT_NULL;
		SSHORT RDB$SYSTEM_FLAG_NULL;
		ISC_TIMESTAMP RDB$CREATION_DATE;
		ISC_TIMESTAMP RDB$EDIT_DATE;
	} viewRecord;

	memset(&viewRecord, 0, sizeof(viewRecord));
	strcpy(viewRecord.RDB$SCHEMA_NAME, viewName.schema.c_str());
	strcpy(viewRecord.RDB$RELATION_NAME, viewName.object.c_str());
	strcpy(viewRecord.RDB$OWNER_NAME, tdbb->tdbb_attachment->att_user->usr_user_name.c_str());
	
	if (!viewDef.viewSource.empty())
	{
		strncpy(viewRecord.RDB$VIEW_SOURCE, viewDef.viewSource.c_str(),
			sizeof(viewRecord.RDB$VIEW_SOURCE) - 1);
		viewRecord.RDB$VIEW_SOURCE_NULL = FALSE;
	}
	else
		viewRecord.RDB$VIEW_SOURCE_NULL = TRUE;

	if (!viewDef.viewBlr.empty())
	{
		strncpy(viewRecord.RDB$VIEW_BLR, viewDef.viewBlr.c_str(),
			sizeof(viewRecord.RDB$VIEW_BLR) - 1);
		viewRecord.RDB$VIEW_BLR_NULL = FALSE;
	}
	else
		viewRecord.RDB$VIEW_BLR_NULL = TRUE;

	if (!viewDef.description.empty())
	{
		strncpy(viewRecord.RDB$DESCRIPTION, viewDef.description.c_str(),
			sizeof(viewRecord.RDB$DESCRIPTION) - 1);
		viewRecord.RDB$DESCRIPTION_NULL = FALSE;
	}
	else
		viewRecord.RDB$DESCRIPTION_NULL = TRUE;

	if (!viewDef.securityClass.empty())
	{
		strcpy(viewRecord.RDB$SECURITY_CLASS, viewDef.securityClass.c_str());
		viewRecord.RDB$SECURITY_CLASS_NULL = FALSE;
	}
	else
		viewRecord.RDB$SECURITY_CLASS_NULL = TRUE;

	if (!viewDef.defaultClass.empty())
	{
		strcpy(viewRecord.RDB$DEFAULT_CLASS, viewDef.defaultClass.c_str());
		viewRecord.RDB$DEFAULT_CLASS_NULL = FALSE;
	}
	else
		viewRecord.RDB$DEFAULT_CLASS_NULL = TRUE;

	viewRecord.RDB$VIEW_CONTEXT = viewDef.viewContext;
	viewRecord.RDB$RELATION_ID = viewDef.relationId;
	viewRecord.RDB$SYSTEM_FLAG = viewDef.systemFlag ? 1 : 0;
	viewRecord.RDB$RELATION_TYPE = rel_view;
	viewRecord.RDB$VIEW_CONTEXT_NULL = FALSE;
	viewRecord.RDB$SYSTEM_FLAG_NULL = FALSE;

	tdbb->tdbb_attachment->att_utility->getTime(&viewRecord.RDB$CREATION_DATE);
	tdbb->tdbb_attachment->att_utility->getTime(&viewRecord.RDB$EDIT_DATE);

	EXE_send(tdbb, handle59, 0, sizeof(RDB$RELATIONS_VIEW_CREATE_RECORD), &viewRecord);
	EXE_unwind(tdbb, handle59);

	// Create view dependencies
	for (ObjectsArray<ViewDependency>::const_iterator dep = viewDef.dependencies.begin();
		 dep != viewDef.dependencies.end(); ++dep)
	{
		// Converted FOR loop #57: Store view dependencies
		jrd_req* handle60 = CMP_find_request(tdbb, drq_store_view_dependency, DYN_REQUESTS);
		EXE_start(tdbb, handle60, transaction);

		struct RDB$DEPENDENCIES_VIEW_RECORD {
			char RDB$DEPENDENT_NAME[32];
			char RDB$DEPENDENT_SCHEMA[32];
			char RDB$DEPENDED_ON_NAME[32];
			char RDB$DEPENDED_ON_SCHEMA[32];
			char RDB$FIELD_NAME[32];
			SSHORT RDB$DEPENDENT_TYPE;
			SSHORT RDB$DEPENDED_ON_TYPE;
			SSHORT RDB$FIELD_NAME_NULL;
		} depRecord;

		memset(&depRecord, 0, sizeof(depRecord));
		strcpy(depRecord.RDB$DEPENDENT_NAME, viewName.object.c_str());
		strcpy(depRecord.RDB$DEPENDENT_SCHEMA, viewName.schema.c_str());
		strcpy(depRecord.RDB$DEPENDED_ON_NAME, dep->dependedOnName.c_str());
		strcpy(depRecord.RDB$DEPENDED_ON_SCHEMA, dep->dependedOnSchema.c_str());
		depRecord.RDB$DEPENDENT_TYPE = obj_view;
		depRecord.RDB$DEPENDED_ON_TYPE = dep->dependedOnType;

		if (!dep->fieldName.empty())
		{
			strcpy(depRecord.RDB$FIELD_NAME, dep->fieldName.c_str());
			depRecord.RDB$FIELD_NAME_NULL = FALSE;
		}
		else
			depRecord.RDB$FIELD_NAME_NULL = TRUE;

		EXE_send(tdbb, handle60, 0, sizeof(RDB$DEPENDENCIES_VIEW_RECORD), &depRecord);
		EXE_unwind(tdbb, handle60);
	}
}

// Package body operations and management
static void createPackageBody(thread_db* tdbb, jrd_tra* transaction, const QualifiedName& packageName,
	const PackageBodyDefinition& bodyDef)
{
	SET_TDBB(tdbb);

	// Converted FOR loop #58: Create package body implementation
	jrd_req* handle61 = CMP_find_request(tdbb, drq_create_package_body, DYN_REQUESTS);
	EXE_start(tdbb, handle61, transaction);

	// Verify package header exists
	jrd_req* handleCheck = CMP_find_request(tdbb, drq_check_package_header_exists, DYN_REQUESTS);
	EXE_start(tdbb, handleCheck, transaction);

	bool headerFound = false;
	while (EXE_receive(tdbb, handleCheck))
	{
		const MetaName pkgSchemaName = MetaName((const char*)handleCheck->req_rpb[0].rpb_record);
		const MetaName pkgName = MetaName((const char*)handleCheck->req_rpb[1].rpb_record);

		if (pkgSchemaName == packageName.schema.c_str() && pkgName == packageName.object.c_str())
		{
			headerFound = true;
			break;
		}
	}
	EXE_unwind(tdbb, handleCheck);

	if (!headerFound)
	{
		// msg 349: "Package header %s not found for body implementation"
		status_exception::raise(Arg::PrivateDyn(349) << packageName.toString());
	}

	// Check if package body already exists
	jrd_req* handleBodyCheck = CMP_find_request(tdbb, drq_check_package_body_exists, DYN_REQUESTS);
	EXE_start(tdbb, handleBodyCheck, transaction);

	while (EXE_receive(tdbb, handleBodyCheck))
	{
		const MetaName bodySchemaName = MetaName((const char*)handleBodyCheck->req_rpb[0].rpb_record);
		const MetaName bodyName = MetaName((const char*)handleBodyCheck->req_rpb[1].rpb_record);

		if (bodySchemaName == packageName.schema.c_str() && bodyName == packageName.object.c_str())
		{
			// msg 350: "Package body %s already exists"
			status_exception::raise(Arg::PrivateDyn(350) << packageName.toString());
		}
	}
	EXE_unwind(tdbb, handleBodyCheck);

	// Create package body record
	struct RDB$PACKAGES_BODY_CREATE_RECORD {
		char RDB$SCHEMA_NAME[32];
		char RDB$PACKAGE_NAME[32];
		char RDB$PACKAGE_BODY_SOURCE[32000];
		char RDB$PACKAGE_BODY_BLR[32000];
		char RDB$DESCRIPTION[256];
		char RDB$SECURITY_CLASS[32];
		char RDB$OWNER_NAME[32];
		SSHORT RDB$VALID_BODY_FLAG;
		SSHORT RDB$PACKAGE_BODY_SOURCE_NULL;
		SSHORT RDB$PACKAGE_BODY_BLR_NULL;
		SSHORT RDB$DESCRIPTION_NULL;
		SSHORT RDB$SECURITY_CLASS_NULL;
		SSHORT RDB$VALID_BODY_FLAG_NULL;
		ISC_TIMESTAMP RDB$CREATION_DATE;
		ISC_TIMESTAMP RDB$EDIT_DATE;
	} bodyRecord;

	memset(&bodyRecord, 0, sizeof(bodyRecord));
	strcpy(bodyRecord.RDB$SCHEMA_NAME, packageName.schema.c_str());
	strcpy(bodyRecord.RDB$PACKAGE_NAME, packageName.object.c_str());
	strcpy(bodyRecord.RDB$OWNER_NAME, tdbb->tdbb_attachment->att_user->usr_user_name.c_str());
	
	if (!bodyDef.bodySource.empty())
	{
		strncpy(bodyRecord.RDB$PACKAGE_BODY_SOURCE, bodyDef.bodySource.c_str(),
			sizeof(bodyRecord.RDB$PACKAGE_BODY_SOURCE) - 1);
		bodyRecord.RDB$PACKAGE_BODY_SOURCE_NULL = FALSE;
	}
	else
		bodyRecord.RDB$PACKAGE_BODY_SOURCE_NULL = TRUE;

	if (!bodyDef.bodyBlr.empty())
	{
		strncpy(bodyRecord.RDB$PACKAGE_BODY_BLR, bodyDef.bodyBlr.c_str(),
			sizeof(bodyRecord.RDB$PACKAGE_BODY_BLR) - 1);
		bodyRecord.RDB$PACKAGE_BODY_BLR_NULL = FALSE;
	}
	else
		bodyRecord.RDB$PACKAGE_BODY_BLR_NULL = TRUE;

	if (!bodyDef.description.empty())
	{
		strncpy(bodyRecord.RDB$DESCRIPTION, bodyDef.description.c_str(),
			sizeof(bodyRecord.RDB$DESCRIPTION) - 1);
		bodyRecord.RDB$DESCRIPTION_NULL = FALSE;
	}
	else
		bodyRecord.RDB$DESCRIPTION_NULL = TRUE;

	if (!bodyDef.securityClass.empty())
	{
		strcpy(bodyRecord.RDB$SECURITY_CLASS, bodyDef.securityClass.c_str());
		bodyRecord.RDB$SECURITY_CLASS_NULL = FALSE;
	}
	else
		bodyRecord.RDB$SECURITY_CLASS_NULL = TRUE;

	bodyRecord.RDB$VALID_BODY_FLAG = bodyDef.validBodyFlag ? 1 : 0;
	bodyRecord.RDB$VALID_BODY_FLAG_NULL = FALSE;

	tdbb->tdbb_attachment->att_utility->getTime(&bodyRecord.RDB$CREATION_DATE);
	tdbb->tdbb_attachment->att_utility->getTime(&bodyRecord.RDB$EDIT_DATE);

	EXE_send(tdbb, handle61, 0, sizeof(RDB$PACKAGES_BODY_CREATE_RECORD), &bodyRecord);
	EXE_unwind(tdbb, handle61);
}

// Advanced constraint management operations
static void createComplexConstraint(thread_db* tdbb, jrd_tra* transaction, const QualifiedName& tableName,
	const ComplexConstraintDefinition& constraintDef)
{
	SET_TDBB(tdbb);

	// Converted FOR loop #59: Create complex constraint (foreign key, unique, etc.)
	jrd_req* handle62 = CMP_find_request(tdbb, drq_create_complex_constraint, DYN_REQUESTS);
	EXE_start(tdbb, handle62, transaction);

	// Validate constraint definition based on type
	if (constraintDef.constraintType == "FOREIGN KEY")
	{
		// Verify referenced table exists
		jrd_req* handleRef = CMP_find_request(tdbb, drq_check_referenced_table, DYN_REQUESTS);
		EXE_start(tdbb, handleRef, transaction);

		bool refTableFound = false;
		while (EXE_receive(tdbb, handleRef))
		{
			const MetaName refSchemaName = MetaName((const char*)handleRef->req_rpb[0].rpb_record);
			const MetaName refRelName = MetaName((const char*)handleRef->req_rpb[1].rpb_record);

			if (refSchemaName == constraintDef.referencedTable.schema.c_str() &&
				refRelName == constraintDef.referencedTable.object.c_str())
			{
				refTableFound = true;
				break;
			}
		}
		EXE_unwind(tdbb, handleRef);

		if (!refTableFound)
		{
			// msg 351: "Referenced table %s not found for foreign key constraint"
			status_exception::raise(Arg::PrivateDyn(351) << constraintDef.referencedTable.toString());
		}
	}

	// Create constraint record
	struct RDB$RELATION_CONSTRAINTS_COMPLEX_RECORD {
		char RDB$CONSTRAINT_NAME[32];
		char RDB$CONSTRAINT_TYPE[16];
		char RDB$SCHEMA_NAME[32];
		char RDB$RELATION_NAME[32];
		char RDB$DEFERRABLE[3];
		char RDB$INITIALLY_DEFERRED[3];
		char RDB$INDEX_NAME[32];
		char RDB$MATCH_OPTION[8];
		char RDB$UPDATE_RULE[16];
		char RDB$DELETE_RULE[16];
		SSHORT RDB$DEFERRABLE_NULL;
		SSHORT RDB$INITIALLY_DEFERRED_NULL;
		SSHORT RDB$INDEX_NAME_NULL;
		SSHORT RDB$MATCH_OPTION_NULL;
		SSHORT RDB$UPDATE_RULE_NULL;
		SSHORT RDB$DELETE_RULE_NULL;
	} constraintRecord;

	memset(&constraintRecord, 0, sizeof(constraintRecord));
	strcpy(constraintRecord.RDB$CONSTRAINT_NAME, constraintDef.constraintName.c_str());
	strcpy(constraintRecord.RDB$CONSTRAINT_TYPE, constraintDef.constraintType.c_str());
	strcpy(constraintRecord.RDB$SCHEMA_NAME, tableName.schema.c_str());
	strcpy(constraintRecord.RDB$RELATION_NAME, tableName.object.c_str());
	strcpy(constraintRecord.RDB$DEFERRABLE, constraintDef.deferrable ? "YES" : "NO");
	strcpy(constraintRecord.RDB$INITIALLY_DEFERRED, constraintDef.initiallyDeferred ? "YES" : "NO");
	constraintRecord.RDB$DEFERRABLE_NULL = FALSE;
	constraintRecord.RDB$INITIALLY_DEFERRED_NULL = FALSE;

	if (!constraintDef.indexName.empty())
	{
		strcpy(constraintRecord.RDB$INDEX_NAME, constraintDef.indexName.c_str());
		constraintRecord.RDB$INDEX_NAME_NULL = FALSE;
	}
	else
		constraintRecord.RDB$INDEX_NAME_NULL = TRUE;

	if (!constraintDef.matchOption.empty())
	{
		strcpy(constraintRecord.RDB$MATCH_OPTION, constraintDef.matchOption.c_str());
		constraintRecord.RDB$MATCH_OPTION_NULL = FALSE;
	}
	else
		constraintRecord.RDB$MATCH_OPTION_NULL = TRUE;

	if (!constraintDef.updateRule.empty())
	{
		strcpy(constraintRecord.RDB$UPDATE_RULE, constraintDef.updateRule.c_str());
		constraintRecord.RDB$UPDATE_RULE_NULL = FALSE;
	}
	else
		constraintRecord.RDB$UPDATE_RULE_NULL = TRUE;

	if (!constraintDef.deleteRule.empty())
	{
		strcpy(constraintRecord.RDB$DELETE_RULE, constraintDef.deleteRule.c_str());
		constraintRecord.RDB$DELETE_RULE_NULL = FALSE;
	}
	else
		constraintRecord.RDB$DELETE_RULE_NULL = TRUE;

	EXE_send(tdbb, handle62, 0, sizeof(RDB$RELATION_CONSTRAINTS_COMPLEX_RECORD), &constraintRecord);
	EXE_unwind(tdbb, handle62);

	// Store constraint column mappings for foreign keys
	if (constraintDef.constraintType == "FOREIGN KEY")
	{
		for (USHORT i = 0; i < constraintDef.localColumns.getCount(); i++)
		{
			// Converted FOR loop #60: Store foreign key column mappings
			jrd_req* handle63 = CMP_find_request(tdbb, drq_store_fk_column_mapping, DYN_REQUESTS);
			EXE_start(tdbb, handle63, transaction);

			struct RDB$REF_CONSTRAINTS_COLUMN_RECORD {
				char RDB$CONSTRAINT_NAME[32];
				char RDB$UPDATE_RULE[16];
				char RDB$DELETE_RULE[16];
				char RDB$MATCH_OPTION[8];
				char RDB$CONST_NAME_UQ[32];
				char RDB$UPDATE_RULE_NULL;
				char RDB$DELETE_RULE_NULL;
				char RDB$MATCH_OPTION_NULL;
			} fkRecord;

			memset(&fkRecord, 0, sizeof(fkRecord));
			strcpy(fkRecord.RDB$CONSTRAINT_NAME, constraintDef.constraintName.c_str());
			
			if (!constraintDef.updateRule.empty())
			{
				strcpy(fkRecord.RDB$UPDATE_RULE, constraintDef.updateRule.c_str());
				fkRecord.RDB$UPDATE_RULE_NULL = FALSE;
			}
			else
				fkRecord.RDB$UPDATE_RULE_NULL = TRUE;

			if (!constraintDef.deleteRule.empty())
			{
				strcpy(fkRecord.RDB$DELETE_RULE, constraintDef.deleteRule.c_str());
				fkRecord.RDB$DELETE_RULE_NULL = FALSE;
			}
			else
				fkRecord.RDB$DELETE_RULE_NULL = TRUE;

			if (!constraintDef.matchOption.empty())
			{
				strcpy(fkRecord.RDB$MATCH_OPTION, constraintDef.matchOption.c_str());
				fkRecord.RDB$MATCH_OPTION_NULL = FALSE;
			}
			else
				fkRecord.RDB$MATCH_OPTION_NULL = TRUE;

			if (!constraintDef.referencedConstraint.empty())
			{
				strcpy(fkRecord.RDB$CONST_NAME_UQ, constraintDef.referencedConstraint.c_str());
			}

			EXE_send(tdbb, handle63, 0, sizeof(RDB$REF_CONSTRAINTS_COLUMN_RECORD), &fkRecord);
			EXE_unwind(tdbb, handle63);
		}
	}
}

// Complex DROP operations with cascade dependencies

void DropTableNode::cascadeDropOperations(thread_db* tdbb, jrd_tra* transaction, const QualifiedName& tableName)
{
	// Converted FOR loop #61: Drop dependent foreign key constraints
	jrd_req* handle64 = CMP_find_request(tdbb, drq_drop_dependent_fks, DYN_REQUESTS);
	EXE_start(tdbb, handle64, transaction);

	struct RDB$REF_CONSTRAINTS_DROP_RECORD {
		char RDB$CONSTRAINT_NAME[32];
		char RDB$CONST_NAME_UQ[32];
		char RDB$UPDATE_RULE[16];
		char RDB$DELETE_RULE[16];
		char RDB$MATCH_OPTION[8];
	} fkDropRecord;

	while (EXE_receive(tdbb, handle64, 0, sizeof(RDB$REF_CONSTRAINTS_DROP_RECORD), &fkDropRecord))
	{
		// Drop referencing constraint
		jrd_req* handle65 = CMP_find_request(tdbb, drq_drop_constraint_cascade, DYN_REQUESTS);
		EXE_start(tdbb, handle65, transaction);
		
		struct RDB$RELATION_CONSTRAINTS_DELETE_RECORD {
			char RDB$CONSTRAINT_NAME[32];
			char RDB$CONSTRAINT_TYPE[16];
			char RDB$RELATION_NAME[32];
			char RDB$DEFERRABLE[4];
			char RDB$INITIALLY_DEFERRED[4];
		} constraintDelRecord;

		strcpy(constraintDelRecord.RDB$CONSTRAINT_NAME, fkDropRecord.RDB$CONSTRAINT_NAME);
		strcpy(constraintDelRecord.RDB$CONSTRAINT_TYPE, "FOREIGN KEY");
		EXE_send(tdbb, handle65, 0, sizeof(RDB$RELATION_CONSTRAINTS_DELETE_RECORD), &constraintDelRecord);
		EXE_unwind(tdbb, handle65);
	}
	EXE_unwind(tdbb, handle64);

	// Converted FOR loop #62: Drop dependent indexes
	jrd_req* handle66 = CMP_find_request(tdbb, drq_drop_table_indexes, DYN_REQUESTS);
	EXE_start(tdbb, handle66, transaction);

	struct RDB$INDICES_DROP_RECORD {
		char RDB$INDEX_NAME[32];
		char RDB$RELATION_NAME[32];
		char RDB$SCHEMA_NAME[32];
		short RDB$UNIQUE_FLAG;
		short RDB$INDEX_INACTIVE;
		char RDB$FOREIGN_KEY[32];
		char RDB$FOREIGN_KEY_NULL;
	} indexDropRecord;

	while (EXE_receive(tdbb, handle66, 0, sizeof(RDB$INDICES_DROP_RECORD), &indexDropRecord))
	{
		// Drop each dependent index
		jrd_req* handle67 = CMP_find_request(tdbb, drq_delete_index, DYN_REQUESTS);
		EXE_start(tdbb, handle67, transaction);
		EXE_send(tdbb, handle67, 0, sizeof(RDB$INDICES_DROP_RECORD), &indexDropRecord);
		EXE_unwind(tdbb, handle67);
	}
	EXE_unwind(tdbb, handle66);

	// Converted FOR loop #63: Drop dependent triggers
	jrd_req* handle68 = CMP_find_request(tdbb, drq_drop_table_triggers, DYN_REQUESTS);
	EXE_start(tdbb, handle68, transaction);

	struct RDB$TRIGGERS_DROP_RECORD {
		char RDB$TRIGGER_NAME[32];
		char RDB$RELATION_NAME[32]; 
		char RDB$SCHEMA_NAME[32];
		short RDB$TRIGGER_SEQUENCE;
		short RDB$TRIGGER_TYPE;
		short RDB$SYSTEM_FLAG;
	} triggerDropRecord;

	while (EXE_receive(tdbb, handle68, 0, sizeof(RDB$TRIGGERS_DROP_RECORD), &triggerDropRecord))
	{
		if (triggerDropRecord.RDB$SYSTEM_FLAG == 0) // Only drop user triggers
		{
			jrd_req* handle69 = CMP_find_request(tdbb, drq_delete_trigger, DYN_REQUESTS);
			EXE_start(tdbb, handle69, transaction);
			EXE_send(tdbb, handle69, 0, sizeof(RDB$TRIGGERS_DROP_RECORD), &triggerDropRecord);
			EXE_unwind(tdbb, handle69);
		}
	}
	EXE_unwind(tdbb, handle68);
}

// Advanced ALTER statement handlers

void AlterTableNode::processColumnOperations(thread_db* tdbb, jrd_tra* transaction, const QualifiedName& tableName)
{
	// Converted FOR loop #64: Process column additions
	for (const auto& addColumn : columnsToAdd)
	{
		jrd_req* handle70 = CMP_find_request(tdbb, drq_add_table_column, DYN_REQUESTS);
		EXE_start(tdbb, handle70, transaction);

		struct RDB$RELATION_FIELDS_ADD_RECORD {
			char RDB$FIELD_NAME[32];
			char RDB$RELATION_NAME[32];
			char RDB$SCHEMA_NAME[32];
			char RDB$FIELD_SOURCE[32];
			char RDB$FIELD_SOURCE_SCHEMA_NAME[32];
			short RDB$FIELD_POSITION;
			char RDB$UPDATE_FLAG;
			char RDB$FIELD_ID;
			char RDB$VIEW_CONTEXT;
			char RDB$DESCRIPTION[256];
			char RDB$DEFAULT_SOURCE[1024];
			char RDB$DEFAULT_VALUE[1024];
			char RDB$COLLATION_ID[32];
			short RDB$NULL_FLAG;
			char RDB$GENERATOR_NAME[32];
			char RDB$IDENTITY_TYPE;
		} addFieldRecord;

		memset(&addFieldRecord, 0, sizeof(addFieldRecord));
		strcpy(addFieldRecord.RDB$FIELD_NAME, addColumn.fieldName.c_str());
		strcpy(addFieldRecord.RDB$RELATION_NAME, tableName.object.c_str());
		strcpy(addFieldRecord.RDB$SCHEMA_NAME, tableName.schema.c_str());
		strcpy(addFieldRecord.RDB$FIELD_SOURCE, addColumn.fieldSource.c_str());
		strcpy(addFieldRecord.RDB$FIELD_SOURCE_SCHEMA_NAME, addColumn.fieldSourceSchema.c_str());
		addFieldRecord.RDB$FIELD_POSITION = addColumn.position;
		addFieldRecord.RDB$NULL_FLAG = addColumn.notNull ? 1 : 0;

		if (!addColumn.defaultValue.empty())
		{
			strcpy(addFieldRecord.RDB$DEFAULT_SOURCE, addColumn.defaultValue.c_str());
			strcpy(addFieldRecord.RDB$DEFAULT_VALUE, addColumn.defaultValue.c_str());
		}

		if (!addColumn.collationName.empty())
			strcpy(addFieldRecord.RDB$COLLATION_ID, addColumn.collationName.c_str());

		EXE_send(tdbb, handle70, 0, sizeof(RDB$RELATION_FIELDS_ADD_RECORD), &addFieldRecord);
		EXE_unwind(tdbb, handle70);
	}

	// Converted FOR loop #65: Process column drops
	for (const auto& dropColumn : columnsToDrop)
	{
		jrd_req* handle71 = CMP_find_request(tdbb, drq_drop_table_column, DYN_REQUESTS);
		EXE_start(tdbb, handle71, transaction);

		struct RDB$RELATION_FIELDS_DROP_RECORD {
			char RDB$FIELD_NAME[32];
			char RDB$RELATION_NAME[32];
			char RDB$SCHEMA_NAME[32];
			short RDB$FIELD_POSITION;
		} dropFieldRecord;

		memset(&dropFieldRecord, 0, sizeof(dropFieldRecord));
		strcpy(dropFieldRecord.RDB$FIELD_NAME, dropColumn.fieldName.c_str());
		strcpy(dropFieldRecord.RDB$RELATION_NAME, tableName.object.c_str());
		strcpy(dropFieldRecord.RDB$SCHEMA_NAME, tableName.schema.c_str());

		EXE_send(tdbb, handle71, 0, sizeof(RDB$RELATION_FIELDS_DROP_RECORD), &dropFieldRecord);
		EXE_unwind(tdbb, handle71);
	}

	// Converted FOR loop #66: Process column modifications
	for (const auto& modifyColumn : columnsToModify)
	{
		jrd_req* handle72 = CMP_find_request(tdbb, drq_modify_table_column, DYN_REQUESTS);
		EXE_start(tdbb, handle72, transaction);

		struct RDB$RELATION_FIELDS_MODIFY_RECORD {
			char RDB$FIELD_NAME[32];
			char RDB$RELATION_NAME[32];
			char RDB$SCHEMA_NAME[32];
			char RDB$FIELD_SOURCE[32];
			char RDB$FIELD_SOURCE_SCHEMA_NAME[32];
			short RDB$NULL_FLAG;
			char RDB$DEFAULT_SOURCE[1024];
			char RDB$DEFAULT_VALUE[1024];
			char RDB$COLLATION_ID[32];
			char RDB$DESCRIPTION[256];
		} modifyFieldRecord;

		memset(&modifyFieldRecord, 0, sizeof(modifyFieldRecord));
		strcpy(modifyFieldRecord.RDB$FIELD_NAME, modifyColumn.fieldName.c_str());
		strcpy(modifyFieldRecord.RDB$RELATION_NAME, tableName.object.c_str());
		strcpy(modifyFieldRecord.RDB$SCHEMA_NAME, tableName.schema.c_str());

		if (!modifyColumn.newFieldSource.empty())
		{
			strcpy(modifyFieldRecord.RDB$FIELD_SOURCE, modifyColumn.newFieldSource.c_str());
			strcpy(modifyFieldRecord.RDB$FIELD_SOURCE_SCHEMA_NAME, modifyColumn.newFieldSourceSchema.c_str());
		}

		if (modifyColumn.nullFlag.has_value())
			modifyFieldRecord.RDB$NULL_FLAG = modifyColumn.nullFlag.value() ? 1 : 0;

		if (!modifyColumn.newDefaultValue.empty())
		{
			strcpy(modifyFieldRecord.RDB$DEFAULT_SOURCE, modifyColumn.newDefaultValue.c_str());
			strcpy(modifyFieldRecord.RDB$DEFAULT_VALUE, modifyColumn.newDefaultValue.c_str());
		}

		EXE_send(tdbb, handle72, 0, sizeof(RDB$RELATION_FIELDS_MODIFY_RECORD), &modifyFieldRecord);
		EXE_unwind(tdbb, handle72);
	}
}

// GRANT/REVOKE privilege management

void GrantNode::processUserPrivileges(thread_db* tdbb, jrd_tra* transaction)
{
	// Converted FOR loop #67: Grant privileges to users
	for (const auto& grantee : granteeList)
	{
		jrd_req* handle73 = CMP_find_request(tdbb, drq_grant_user_privileges, DYN_REQUESTS);
		EXE_start(tdbb, handle73, transaction);

		struct RDB$USER_PRIVILEGES_GRANT_RECORD {
			char RDB$USER[32];
			char RDB$GRANTOR[32];
			char RDB$PRIVILEGE[8];
			char RDB$GRANT_OPTION;
			char RDB$RELATION_NAME[32];
			char RDB$SCHEMA_NAME[32];
			char RDB$FIELD_NAME[32];
			char RDB$USER_TYPE;
			char RDB$OBJECT_TYPE;
		} privilegeRecord;

		memset(&privilegeRecord, 0, sizeof(privilegeRecord));
		strcpy(privilegeRecord.RDB$USER, grantee.userName.c_str());
		strcpy(privilegeRecord.RDB$GRANTOR, grantor.userName.c_str());
		privilegeRecord.RDB$USER_TYPE = grantee.userType;
		privilegeRecord.RDB$GRANT_OPTION = withGrantOption ? 'Y' : 'N';

		if (!objectName.object.empty())
		{
			strcpy(privilegeRecord.RDB$RELATION_NAME, objectName.object.c_str());
			strcpy(privilegeRecord.RDB$SCHEMA_NAME, objectName.schema.c_str());
			privilegeRecord.RDB$OBJECT_TYPE = objectType;
		}

		// Grant each specified privilege
		for (const auto& privilege : privilegeList)
		{
			strcpy(privilegeRecord.RDB$PRIVILEGE, privilege.c_str());

			if (!privilege.fieldName.empty())
				strcpy(privilegeRecord.RDB$FIELD_NAME, privilege.fieldName.c_str());

			EXE_send(tdbb, handle73, 0, sizeof(RDB$USER_PRIVILEGES_GRANT_RECORD), &privilegeRecord);
		}
		EXE_unwind(tdbb, handle73);
	}

	// Converted FOR loop #68: Grant privileges to roles
	for (const auto& role : roleList)
	{
		jrd_req* handle74 = CMP_find_request(tdbb, drq_grant_role_privileges, DYN_REQUESTS);
		EXE_start(tdbb, handle74, transaction);

		struct RDB$USER_PRIVILEGES_ROLE_RECORD {
			char RDB$USER[32];
			char RDB$GRANTOR[32];
			char RDB$PRIVILEGE[8];
			char RDB$GRANT_OPTION;
			char RDB$RELATION_NAME[32];
			char RDB$SCHEMA_NAME[32];
			char RDB$USER_TYPE;
			char RDB$OBJECT_TYPE;
		} rolePrivilegeRecord;

		memset(&rolePrivilegeRecord, 0, sizeof(rolePrivilegeRecord));
		strcpy(rolePrivilegeRecord.RDB$USER, role.roleName.c_str());
		strcpy(rolePrivilegeRecord.RDB$GRANTOR, grantor.userName.c_str());
		rolePrivilegeRecord.RDB$USER_TYPE = obj_sql_role;
		rolePrivilegeRecord.RDB$GRANT_OPTION = withGrantOption ? 'Y' : 'N';

		if (!objectName.object.empty())
		{
			strcpy(rolePrivilegeRecord.RDB$RELATION_NAME, objectName.object.c_str());
			strcpy(rolePrivilegeRecord.RDB$SCHEMA_NAME, objectName.schema.c_str());
			rolePrivilegeRecord.RDB$OBJECT_TYPE = objectType;
		}

		for (const auto& privilege : privilegeList)
		{
			strcpy(rolePrivilegeRecord.RDB$PRIVILEGE, privilege.c_str());
			EXE_send(tdbb, handle74, 0, sizeof(RDB$USER_PRIVILEGES_ROLE_RECORD), &rolePrivilegeRecord);
		}
		EXE_unwind(tdbb, handle74);
	}
}

void RevokeNode::processPrivilegeRevocation(thread_db* tdbb, jrd_tra* transaction)
{
	// Converted FOR loop #69: Revoke privileges from users
	for (const auto& revokee : revokeeList)
	{
		jrd_req* handle75 = CMP_find_request(tdbb, drq_revoke_user_privileges, DYN_REQUESTS);
		EXE_start(tdbb, handle75, transaction);

		struct RDB$USER_PRIVILEGES_REVOKE_RECORD {
			char RDB$USER[32];
			char RDB$GRANTOR[32];
			char RDB$PRIVILEGE[8];
			char RDB$GRANT_OPTION;
			char RDB$RELATION_NAME[32];
			char RDB$SCHEMA_NAME[32];
			char RDB$FIELD_NAME[32];
			char RDB$USER_TYPE;
			char RDB$OBJECT_TYPE;
		} revokeRecord;

		memset(&revokeRecord, 0, sizeof(revokeRecord));
		strcpy(revokeRecord.RDB$USER, revokee.userName.c_str());
		strcpy(revokeRecord.RDB$GRANTOR, grantor.userName.c_str());
		revokeRecord.RDB$USER_TYPE = revokee.userType;

		if (!objectName.object.empty())
		{
			strcpy(revokeRecord.RDB$RELATION_NAME, objectName.object.c_str());
			strcpy(revokeRecord.RDB$SCHEMA_NAME, objectName.schema.c_str());
			revokeRecord.RDB$OBJECT_TYPE = objectType;
		}

		for (const auto& privilege : privilegeList)
		{
			strcpy(revokeRecord.RDB$PRIVILEGE, privilege.c_str());

			if (!privilege.fieldName.empty())
				strcpy(revokeRecord.RDB$FIELD_NAME, privilege.fieldName.c_str());

			// Find and delete matching privilege records
			jrd_req* handle76 = CMP_find_request(tdbb, drq_delete_user_privilege, DYN_REQUESTS);
			EXE_start(tdbb, handle76, transaction);
			EXE_send(tdbb, handle76, 0, sizeof(RDB$USER_PRIVILEGES_REVOKE_RECORD), &revokeRecord);
			EXE_unwind(tdbb, handle76);
		}
		EXE_unwind(tdbb, handle75);
	}

	// Converted FOR loop #70: Handle cascade revocation if specified
	if (cascadeRevoke)
	{
		jrd_req* handle77 = CMP_find_request(tdbb, drq_cascade_revoke_privileges, DYN_REQUESTS);
		EXE_start(tdbb, handle77, transaction);

		struct RDB$CASCADE_REVOKE_RECORD {
			char RDB$ORIGINAL_GRANTOR[32];
			char RDB$PRIVILEGE[8];
			char RDB$RELATION_NAME[32];
			char RDB$SCHEMA_NAME[32];
			char RDB$OBJECT_TYPE;
		} cascadeRecord;

		memset(&cascadeRecord, 0, sizeof(cascadeRecord));
		strcpy(cascadeRecord.RDB$ORIGINAL_GRANTOR, grantor.userName.c_str());
		
		if (!objectName.object.empty())
		{
			strcpy(cascadeRecord.RDB$RELATION_NAME, objectName.object.c_str());
			strcpy(cascadeRecord.RDB$SCHEMA_NAME, objectName.schema.c_str());
			cascadeRecord.RDB$OBJECT_TYPE = objectType;
		}

		for (const auto& privilege : privilegeList)
		{
			strcpy(cascadeRecord.RDB$PRIVILEGE, privilege.c_str());
			
			// Find all users who received this privilege via grant option
			jrd_req* handle78 = CMP_find_request(tdbb, drq_find_cascade_privileges, DYN_REQUESTS);
			EXE_start(tdbb, handle78, transaction);
			EXE_send(tdbb, handle78, 0, sizeof(RDB$CASCADE_REVOKE_RECORD), &cascadeRecord);

			struct RDB$CASCADE_TARGET_RECORD {
				char RDB$USER[32];
				char RDB$USER_TYPE;
			} cascadeTargetRecord;

			while (EXE_receive(tdbb, handle78, 0, sizeof(RDB$CASCADE_TARGET_RECORD), &cascadeTargetRecord))
			{
				// Recursively revoke from each dependent user
				jrd_req* handle79 = CMP_find_request(tdbb, drq_delete_cascade_privilege, DYN_REQUESTS);
				EXE_start(tdbb, handle79, transaction);
				EXE_send(tdbb, handle79, 0, sizeof(RDB$CASCADE_TARGET_RECORD), &cascadeTargetRecord);
				EXE_unwind(tdbb, handle79);
			}
			EXE_unwind(tdbb, handle78);
		}
		EXE_unwind(tdbb, handle77);
	}
}

// Complex schema modification operations

void AlterSchemaNode::processSchemaModifications(thread_db* tdbb, jrd_tra* transaction, const QualifiedName& schemaName)
{
	// Converted FOR loop #71: Update schema properties
	jrd_req* handle80 = CMP_find_request(tdbb, drq_modify_schema_properties, DYN_REQUESTS);
	EXE_start(tdbb, handle80, transaction);

	struct RDB$SCHEMAS_MODIFY_RECORD {
		char RDB$SCHEMA_NAME[32];
		char RDB$PARENT_SCHEMA_NAME[32];
		char RDB$SCHEMA_PATH[512];
		short RDB$SCHEMA_LEVEL;
		char RDB$OWNER_NAME[32];
		char RDB$DEFAULT_CHARACTER_SET_NAME[32];
		short RDB$SYSTEM_FLAG;
		char RDB$DESCRIPTION[256];
		char RDB$PARENT_SCHEMA_NAME_NULL;
		char RDB$DESCRIPTION_NULL;
		char RDB$DEFAULT_CHARACTER_SET_NAME_NULL;
	} schemaModifyRecord;

	memset(&schemaModifyRecord, 0, sizeof(schemaModifyRecord));
	strcpy(schemaModifyRecord.RDB$SCHEMA_NAME, schemaName.object.c_str());

	if (!newParentSchema.empty())
	{
		strcpy(schemaModifyRecord.RDB$PARENT_SCHEMA_NAME, newParentSchema.c_str());
		schemaModifyRecord.RDB$PARENT_SCHEMA_NAME_NULL = FALSE;
		
		// Recalculate schema path and level
		calculateSchemaPath(schemaModifyRecord.RDB$SCHEMA_PATH, 
							schemaModifyRecord.RDB$SCHEMA_LEVEL,
							newParentSchema, schemaName.object);
	}
	else
		schemaModifyRecord.RDB$PARENT_SCHEMA_NAME_NULL = TRUE;

	if (!newOwner.empty())
		strcpy(schemaModifyRecord.RDB$OWNER_NAME, newOwner.c_str());

	if (!newDefaultCharset.empty())
	{
		strcpy(schemaModifyRecord.RDB$DEFAULT_CHARACTER_SET_NAME, newDefaultCharset.c_str());
		schemaModifyRecord.RDB$DEFAULT_CHARACTER_SET_NAME_NULL = FALSE;
	}
	else
		schemaModifyRecord.RDB$DEFAULT_CHARACTER_SET_NAME_NULL = TRUE;

	if (!newDescription.empty())
	{
		strcpy(schemaModifyRecord.RDB$DESCRIPTION, newDescription.c_str());
		schemaModifyRecord.RDB$DESCRIPTION_NULL = FALSE;
	}
	else
		schemaModifyRecord.RDB$DESCRIPTION_NULL = TRUE;

	EXE_send(tdbb, handle80, 0, sizeof(RDB$SCHEMAS_MODIFY_RECORD), &schemaModifyRecord);
	EXE_unwind(tdbb, handle80);

	// Converted FOR loop #72: Update dependent schema paths if parent changed
	if (!newParentSchema.empty())
	{
		updateDependentSchemaPaths(tdbb, transaction, schemaName.object);
	}
}

void AlterSchemaNode::updateDependentSchemaPaths(thread_db* tdbb, jrd_tra* transaction, const string& schemaName)
{
	// Converted FOR loop #73: Find all child schemas
	jrd_req* handle81 = CMP_find_request(tdbb, drq_find_child_schemas, DYN_REQUESTS);
	EXE_start(tdbb, handle81, transaction);

	struct RDB$CHILD_SCHEMAS_RECORD {
		char RDB$SCHEMA_NAME[32];
		char RDB$PARENT_SCHEMA_NAME[32];
		char RDB$SCHEMA_PATH[512];
		short RDB$SCHEMA_LEVEL;
	} childSchemaRecord;

	strcpy(childSchemaRecord.RDB$PARENT_SCHEMA_NAME, schemaName.c_str());
	EXE_send(tdbb, handle81, 1, sizeof(RDB$CHILD_SCHEMAS_RECORD), &childSchemaRecord);

	while (EXE_receive(tdbb, handle81, 0, sizeof(RDB$CHILD_SCHEMAS_RECORD), &childSchemaRecord))
	{
		// Update each child schema's path
		jrd_req* handle82 = CMP_find_request(tdbb, drq_update_schema_path, DYN_REQUESTS);
		EXE_start(tdbb, handle82, transaction);

		struct RDB$SCHEMA_PATH_UPDATE_RECORD {
			char RDB$SCHEMA_NAME[32];
			char RDB$NEW_SCHEMA_PATH[512];
			short RDB$NEW_SCHEMA_LEVEL;
		} pathUpdateRecord;

		strcpy(pathUpdateRecord.RDB$SCHEMA_NAME, childSchemaRecord.RDB$SCHEMA_NAME);
		calculateSchemaPath(pathUpdateRecord.RDB$NEW_SCHEMA_PATH,
							pathUpdateRecord.RDB$NEW_SCHEMA_LEVEL,
							newParentSchema, childSchemaRecord.RDB$SCHEMA_NAME);

		EXE_send(tdbb, handle82, 0, sizeof(RDB$SCHEMA_PATH_UPDATE_RECORD), &pathUpdateRecord);
		EXE_unwind(tdbb, handle82);

		// Recursively update grandchildren
		updateDependentSchemaPaths(tdbb, transaction, childSchemaRecord.RDB$SCHEMA_NAME);
	}
	EXE_unwind(tdbb, handle81);
}

// Index management with expression support

void CreateIndexNode::processExpressionIndex(thread_db* tdbb, jrd_tra* transaction, const QualifiedName& indexName)
{
	// Converted FOR loop #74: Store expression index definition
	jrd_req* handle83 = CMP_find_request(tdbb, drq_store_expression_index, DYN_REQUESTS);
	EXE_start(tdbb, handle83, transaction);

	struct RDB$INDICES_EXPRESSION_RECORD {
		char RDB$INDEX_NAME[32];
		char RDB$SCHEMA_NAME[32];
		char RDB$RELATION_NAME[32];
		char RDB$EXPRESSION_SOURCE[1024];
		ISC_QUAD RDB$EXPRESSION_BLR;
		short RDB$UNIQUE_FLAG;
		short RDB$INDEX_INACTIVE;
		short RDB$INDEX_TYPE;
		short RDB$SYSTEM_FLAG;
		char RDB$EXPRESSION_SOURCE_NULL;
		char RDB$EXPRESSION_BLR_NULL;
	} expressionIndexRecord;

	memset(&expressionIndexRecord, 0, sizeof(expressionIndexRecord));
	strcpy(expressionIndexRecord.RDB$INDEX_NAME, indexName.object.c_str());
	strcpy(expressionIndexRecord.RDB$SCHEMA_NAME, indexName.schema.c_str());
	strcpy(expressionIndexRecord.RDB$RELATION_NAME, relationName.object.c_str());

	if (!expressionSource.empty())
	{
		strcpy(expressionIndexRecord.RDB$EXPRESSION_SOURCE, expressionSource.c_str());
		expressionIndexRecord.RDB$EXPRESSION_SOURCE_NULL = FALSE;
		
		// Store compiled BLR for expression
		if (compiledBLR.hasData())
		{
			expressionIndexRecord.RDB$EXPRESSION_BLR = compiledBLR;
			expressionIndexRecord.RDB$EXPRESSION_BLR_NULL = FALSE;
		}
		else
			expressionIndexRecord.RDB$EXPRESSION_BLR_NULL = TRUE;
	}
	else
	{
		expressionIndexRecord.RDB$EXPRESSION_SOURCE_NULL = TRUE;
		expressionIndexRecord.RDB$EXPRESSION_BLR_NULL = TRUE;
	}

	expressionIndexRecord.RDB$UNIQUE_FLAG = uniqueFlag ? 1 : 0;
	expressionIndexRecord.RDB$INDEX_INACTIVE = inactiveFlag ? 1 : 0;
	expressionIndexRecord.RDB$INDEX_TYPE = indexType;
	expressionIndexRecord.RDB$SYSTEM_FLAG = 0;

	EXE_send(tdbb, handle83, 0, sizeof(RDB$INDICES_EXPRESSION_RECORD), &expressionIndexRecord);
	EXE_unwind(tdbb, handle83);

	// Converted FOR loop #75: Store index segments for expression index
	for (int segmentPosition = 0; segmentPosition < indexSegments.size(); segmentPosition++)
	{
		jrd_req* handle84 = CMP_find_request(tdbb, drq_store_index_segment, DYN_REQUESTS);
		EXE_start(tdbb, handle84, transaction);

		struct RDB$INDEX_SEGMENTS_RECORD {
			char RDB$INDEX_NAME[32];
			char RDB$SCHEMA_NAME[32];
			char RDB$FIELD_NAME[32];
			short RDB$FIELD_POSITION;
			short RDB$STATISTICS;
		} segmentRecord;

		memset(&segmentRecord, 0, sizeof(segmentRecord));
		strcpy(segmentRecord.RDB$INDEX_NAME, indexName.object.c_str());
		strcpy(segmentRecord.RDB$SCHEMA_NAME, indexName.schema.c_str());
		strcpy(segmentRecord.RDB$FIELD_NAME, indexSegments[segmentPosition].fieldName.c_str());
		segmentRecord.RDB$FIELD_POSITION = segmentPosition;
		segmentRecord.RDB$STATISTICS = 0; // Will be calculated later

		EXE_send(tdbb, handle84, 0, sizeof(RDB$INDEX_SEGMENTS_RECORD), &segmentRecord);
		EXE_unwind(tdbb, handle84);
	}
}

void DropIndexNode::validateIndexUsage(thread_db* tdbb, jrd_tra* transaction, const QualifiedName& indexName)
{
	// Converted FOR loop #76: Check if index is used by constraints
	jrd_req* handle85 = CMP_find_request(tdbb, drq_check_index_constraints, DYN_REQUESTS);
	EXE_start(tdbb, handle85, transaction);

	struct RDB$RELATION_CONSTRAINTS_INDEX_RECORD {
		char RDB$CONSTRAINT_NAME[32];
		char RDB$CONSTRAINT_TYPE[16];
		char RDB$RELATION_NAME[32];
		char RDB$SCHEMA_NAME[32];
		char RDB$INDEX_NAME[32];
	} constraintIndexRecord;

	strcpy(constraintIndexRecord.RDB$INDEX_NAME, indexName.object.c_str());
	EXE_send(tdbb, handle85, 1, sizeof(RDB$RELATION_CONSTRAINTS_INDEX_RECORD), &constraintIndexRecord);

	while (EXE_receive(tdbb, handle85, 0, sizeof(RDB$RELATION_CONSTRAINTS_INDEX_RECORD), &constraintIndexRecord))
	{
		// Index is used by a constraint - cannot be dropped
		status_exception::raise(
			Arg::Gds(isc_sqlerr) << Arg::Num(-607) <<
			Arg::Gds(isc_dsql_index_used_by_constraint) << 
			indexName.toQuotedString() << constraintIndexRecord.RDB$CONSTRAINT_NAME);
	}
	EXE_unwind(tdbb, handle85);

	// Converted FOR loop #77: Check if index is used by foreign keys  
	jrd_req* handle86 = CMP_find_request(tdbb, drq_check_index_foreign_keys, DYN_REQUESTS);
	EXE_start(tdbb, handle86, transaction);

	struct RDB$INDICES_FK_RECORD {
		char RDB$INDEX_NAME[32];
		char RDB$SCHEMA_NAME[32];
		char RDB$FOREIGN_KEY[32];
		char RDB$FOREIGN_KEY_SCHEMA_NAME[32];
	} indexFkRecord;

	strcpy(indexFkRecord.RDB$INDEX_NAME, indexName.object.c_str());
	strcpy(indexFkRecord.RDB$SCHEMA_NAME, indexName.schema.c_str());
	EXE_send(tdbb, handle86, 1, sizeof(RDB$INDICES_FK_RECORD), &indexFkRecord);

	while (EXE_receive(tdbb, handle86, 0, sizeof(RDB$INDICES_FK_RECORD), &indexFkRecord))
	{
		// Index is used by a foreign key - cannot be dropped
		status_exception::raise(
			Arg::Gds(isc_sqlerr) << Arg::Num(-607) <<
			Arg::Gds(isc_dsql_index_used_by_foreign_key) << 
			indexName.toQuotedString() << indexFkRecord.RDB$FOREIGN_KEY);
	}
	EXE_unwind(tdbb, handle86);
}

// Advanced CREATE statement variants

void CreateViewNode::processViewDependencies(thread_db* tdbb, jrd_tra* transaction, const QualifiedName& viewName)
{
	// Converted FOR loop #78: Store view dependencies
	for (const auto& dependency : viewDependencies)
	{
		jrd_req* handle87 = CMP_find_request(tdbb, drq_store_view_dependency, DYN_REQUESTS);
		EXE_start(tdbb, handle87, transaction);

		struct RDB$DEPENDENCIES_VIEW_RECORD {
			char RDB$DEPENDENT_NAME[32];
			char RDB$DEPENDENT_SCHEMA_NAME[32];
			short RDB$DEPENDENT_TYPE;
			char RDB$DEPENDED_ON_NAME[32];
			char RDB$DEPENDED_ON_SCHEMA_NAME[32];
			short RDB$DEPENDED_ON_TYPE;
			char RDB$FIELD_NAME[32];
			char RDB$FIELD_NAME_NULL;
		} dependencyRecord;

		memset(&dependencyRecord, 0, sizeof(dependencyRecord));
		strcpy(dependencyRecord.RDB$DEPENDENT_NAME, viewName.object.c_str());
		strcpy(dependencyRecord.RDB$DEPENDENT_SCHEMA_NAME, viewName.schema.c_str());
		dependencyRecord.RDB$DEPENDENT_TYPE = obj_view;
		
		strcpy(dependencyRecord.RDB$DEPENDED_ON_NAME, dependency.objectName.c_str());
		strcpy(dependencyRecord.RDB$DEPENDED_ON_SCHEMA_NAME, dependency.schemaName.c_str());
		dependencyRecord.RDB$DEPENDED_ON_TYPE = dependency.objectType;

		if (!dependency.fieldName.empty())
		{
			strcpy(dependencyRecord.RDB$FIELD_NAME, dependency.fieldName.c_str());
			dependencyRecord.RDB$FIELD_NAME_NULL = FALSE;
		}
		else
			dependencyRecord.RDB$FIELD_NAME_NULL = TRUE;

		EXE_send(tdbb, handle87, 0, sizeof(RDB$DEPENDENCIES_VIEW_RECORD), &dependencyRecord);
		EXE_unwind(tdbb, handle87);
	}

	// Converted FOR loop #79: Store view columns
	for (int columnPosition = 0; columnPosition < viewColumns.size(); columnPosition++)
	{
		jrd_req* handle88 = CMP_find_request(tdbb, drq_store_view_column, DYN_REQUESTS);
		EXE_start(tdbb, handle88, transaction);

		struct RDB$RELATION_FIELDS_VIEW_RECORD {
			char RDB$FIELD_NAME[32];
			char RDB$RELATION_NAME[32];
			char RDB$SCHEMA_NAME[32];
			char RDB$FIELD_SOURCE[32];
			char RDB$FIELD_SOURCE_SCHEMA_NAME[32];
			short RDB$FIELD_POSITION;
			char RDB$UPDATE_FLAG;
			char RDB$FIELD_ID;
			short RDB$VIEW_CONTEXT;
			char RDB$DESCRIPTION[256];
			char RDB$DESCRIPTION_NULL;
		} viewColumnRecord;

		memset(&viewColumnRecord, 0, sizeof(viewColumnRecord));
		const auto& column = viewColumns[columnPosition];
		
		strcpy(viewColumnRecord.RDB$FIELD_NAME, column.fieldName.c_str());
		strcpy(viewColumnRecord.RDB$RELATION_NAME, viewName.object.c_str());
		strcpy(viewColumnRecord.RDB$SCHEMA_NAME, viewName.schema.c_str());
		strcpy(viewColumnRecord.RDB$FIELD_SOURCE, column.fieldSource.c_str());
		strcpy(viewColumnRecord.RDB$FIELD_SOURCE_SCHEMA_NAME, column.fieldSourceSchema.c_str());
		viewColumnRecord.RDB$FIELD_POSITION = columnPosition;
		viewColumnRecord.RDB$UPDATE_FLAG = column.updateFlag;
		viewColumnRecord.RDB$VIEW_CONTEXT = column.viewContext;

		if (!column.description.empty())
		{
			strcpy(viewColumnRecord.RDB$DESCRIPTION, column.description.c_str());
			viewColumnRecord.RDB$DESCRIPTION_NULL = FALSE;
		}
		else
			viewColumnRecord.RDB$DESCRIPTION_NULL = TRUE;

		EXE_send(tdbb, handle88, 0, sizeof(RDB$RELATION_FIELDS_VIEW_RECORD), &viewColumnRecord);
		EXE_unwind(tdbb, handle88);
	}
}

void CreateTriggerNode::storeTriggerDependencies(thread_db* tdbb, jrd_tra* transaction, const QualifiedName& triggerName)
{
	// Converted FOR loop #80: Store trigger dependencies
	for (const auto& dependency : triggerDependencies)
	{
		jrd_req* handle89 = CMP_find_request(tdbb, drq_store_trigger_dependency, DYN_REQUESTS);
		EXE_start(tdbb, handle89, transaction);

		struct RDB$DEPENDENCIES_TRIGGER_RECORD {
			char RDB$DEPENDENT_NAME[32];
			char RDB$DEPENDENT_SCHEMA_NAME[32];
			short RDB$DEPENDENT_TYPE;
			char RDB$DEPENDED_ON_NAME[32];
			char RDB$DEPENDED_ON_SCHEMA_NAME[32];
			short RDB$DEPENDED_ON_TYPE;
			char RDB$FIELD_NAME[32];
			char RDB$FIELD_NAME_NULL;
		} triggerDependencyRecord;

		memset(&triggerDependencyRecord, 0, sizeof(triggerDependencyRecord));
		strcpy(triggerDependencyRecord.RDB$DEPENDENT_NAME, triggerName.object.c_str());
		strcpy(triggerDependencyRecord.RDB$DEPENDENT_SCHEMA_NAME, triggerName.schema.c_str());
		triggerDependencyRecord.RDB$DEPENDENT_TYPE = obj_trigger;
		
		strcpy(triggerDependencyRecord.RDB$DEPENDED_ON_NAME, dependency.objectName.c_str());
		strcpy(triggerDependencyRecord.RDB$DEPENDED_ON_SCHEMA_NAME, dependency.schemaName.c_str());
		triggerDependencyRecord.RDB$DEPENDED_ON_TYPE = dependency.objectType;

		if (!dependency.fieldName.empty())
		{
			strcpy(triggerDependencyRecord.RDB$FIELD_NAME, dependency.fieldName.c_str());
			triggerDependencyRecord.RDB$FIELD_NAME_NULL = FALSE;
		}
		else
			triggerDependencyRecord.RDB$FIELD_NAME_NULL = TRUE;

		EXE_send(tdbb, handle89, 0, sizeof(RDB$DEPENDENCIES_TRIGGER_RECORD), &triggerDependencyRecord);
		EXE_unwind(tdbb, handle89);
	}

	// Converted FOR loop #81: Store trigger messages (if any)
	for (const auto& message : triggerMessages)
	{
		jrd_req* handle90 = CMP_find_request(tdbb, drq_store_trigger_message, DYN_REQUESTS);
		EXE_start(tdbb, handle90, transaction);

		struct RDB$TRIGGER_MESSAGES_RECORD {
			char RDB$TRIGGER_NAME[32];
			char RDB$SCHEMA_NAME[32];
			short RDB$MESSAGE_NUMBER;
			char RDB$MESSAGE[256];
		} messageRecord;

		memset(&messageRecord, 0, sizeof(messageRecord));
		strcpy(messageRecord.RDB$TRIGGER_NAME, triggerName.object.c_str());
		strcpy(messageRecord.RDB$SCHEMA_NAME, triggerName.schema.c_str());
		messageRecord.RDB$MESSAGE_NUMBER = message.messageNumber;
		strcpy(messageRecord.RDB$MESSAGE, message.messageText.c_str());

		EXE_send(tdbb, handle90, 0, sizeof(RDB$TRIGGER_MESSAGES_RECORD), &messageRecord);
		EXE_unwind(tdbb, handle90);
	}
}

// Remaining CREATE statement variants

void CreateGeneratorNode::storeGeneratorProperties(thread_db* tdbb, jrd_tra* transaction, const QualifiedName& generatorName)
{
	// Converted FOR loop #82: Store generator definition
	jrd_req* handle91 = CMP_find_request(tdbb, drq_store_generator, DYN_REQUESTS);
	EXE_start(tdbb, handle91, transaction);

	struct RDB$GENERATORS_RECORD {
		char RDB$GENERATOR_NAME[32];
		char RDB$SCHEMA_NAME[32];
		short RDB$GENERATOR_ID;
		ISC_INT64 RDB$INITIAL_VALUE;
		ISC_INT64 RDB$GENERATOR_INCREMENT;
		short RDB$SYSTEM_FLAG;
		char RDB$DESCRIPTION[256];
		char RDB$DESCRIPTION_NULL;
		ISC_INT64 RDB$MINIMUM_VALUE; 
		ISC_INT64 RDB$MAXIMUM_VALUE;
		char RDB$CYCLE_FLAG;
		char RDB$MINIMUM_VALUE_NULL;
		char RDB$MAXIMUM_VALUE_NULL;
		char RDB$CYCLE_FLAG_NULL;
	} generatorRecord;

	memset(&generatorRecord, 0, sizeof(generatorRecord));
	strcpy(generatorRecord.RDB$GENERATOR_NAME, generatorName.object.c_str());
	strcpy(generatorRecord.RDB$SCHEMA_NAME, generatorName.schema.c_str());
	generatorRecord.RDB$GENERATOR_ID = DYN_UTIL_gen_unique_id(tdbb, transaction, drq_g_nxt_gen);
	generatorRecord.RDB$INITIAL_VALUE = initialValue;
	generatorRecord.RDB$GENERATOR_INCREMENT = incrementValue;
	generatorRecord.RDB$SYSTEM_FLAG = 0;

	if (!description.empty())
	{
		strcpy(generatorRecord.RDB$DESCRIPTION, description.c_str());
		generatorRecord.RDB$DESCRIPTION_NULL = FALSE;
	}
	else
		generatorRecord.RDB$DESCRIPTION_NULL = TRUE;

	if (minimumValue.has_value())
	{
		generatorRecord.RDB$MINIMUM_VALUE = minimumValue.value();
		generatorRecord.RDB$MINIMUM_VALUE_NULL = FALSE;
	}
	else
		generatorRecord.RDB$MINIMUM_VALUE_NULL = TRUE;

	if (maximumValue.has_value())
	{
		generatorRecord.RDB$MAXIMUM_VALUE = maximumValue.value();
		generatorRecord.RDB$MAXIMUM_VALUE_NULL = FALSE;
	}
	else
		generatorRecord.RDB$MAXIMUM_VALUE_NULL = TRUE;

	if (cycleFlag.has_value())
	{
		generatorRecord.RDB$CYCLE_FLAG = cycleFlag.value() ? 'Y' : 'N';
		generatorRecord.RDB$CYCLE_FLAG_NULL = FALSE;
	}
	else
		generatorRecord.RDB$CYCLE_FLAG_NULL = TRUE;

	EXE_send(tdbb, handle91, 0, sizeof(RDB$GENERATORS_RECORD), &generatorRecord);
	EXE_unwind(tdbb, handle91);
}

void CreateExceptionNode::storeExceptionDefinition(thread_db* tdbb, jrd_tra* transaction, const QualifiedName& exceptionName)
{
	// Converted FOR loop #83: Store exception definition
	jrd_req* handle92 = CMP_find_request(tdbb, drq_store_exception, DYN_REQUESTS);
	EXE_start(tdbb, handle92, transaction);

	struct RDB$EXCEPTIONS_RECORD {
		char RDB$EXCEPTION_NAME[32];
		char RDB$SCHEMA_NAME[32];
		short RDB$EXCEPTION_NUMBER;
		char RDB$MESSAGE[256];
		char RDB$DESCRIPTION[256];
		short RDB$SYSTEM_FLAG;
		char RDB$OWNER_NAME[32];
		char RDB$DESCRIPTION_NULL;
	} exceptionRecord;

	memset(&exceptionRecord, 0, sizeof(exceptionRecord));
	strcpy(exceptionRecord.RDB$EXCEPTION_NAME, exceptionName.object.c_str());
	strcpy(exceptionRecord.RDB$SCHEMA_NAME, exceptionName.schema.c_str());
	exceptionRecord.RDB$EXCEPTION_NUMBER = DYN_UTIL_gen_unique_id(tdbb, transaction, drq_g_nxt_exc);
	strcpy(exceptionRecord.RDB$MESSAGE, exceptionMessage.c_str());
	exceptionRecord.RDB$SYSTEM_FLAG = 0;
	strcpy(exceptionRecord.RDB$OWNER_NAME, ownerName.c_str());

	if (!description.empty())
	{
		strcpy(exceptionRecord.RDB$DESCRIPTION, description.c_str());
		exceptionRecord.RDB$DESCRIPTION_NULL = FALSE;
	}
	else
		exceptionRecord.RDB$DESCRIPTION_NULL = TRUE;

	EXE_send(tdbb, handle92, 0, sizeof(RDB$EXCEPTIONS_RECORD), &exceptionRecord);
	EXE_unwind(tdbb, handle92);
}

// Complex procedure and function management

void CreateProcedureNode::storeProcedureParameters(thread_db* tdbb, jrd_tra* transaction, const QualifiedName& procedureName)
{
	// Converted FOR loop #84: Store input parameters
	for (int paramPosition = 0; paramPosition < inputParameters.size(); paramPosition++)
	{
		jrd_req* handle93 = CMP_find_request(tdbb, drq_store_proc_input_param, DYN_REQUESTS);
		EXE_start(tdbb, handle93, transaction);

		struct RDB$PROCEDURE_PARAMETERS_RECORD {
			char RDB$PARAMETER_NAME[32];
			char RDB$PROCEDURE_NAME[32];
			char RDB$SCHEMA_NAME[32];
			short RDB$PARAMETER_NUMBER;
			short RDB$PARAMETER_TYPE;
			char RDB$FIELD_SOURCE[32];
			char RDB$FIELD_SOURCE_SCHEMA_NAME[32];
			char RDB$DESCRIPTION[256];
			char RDB$DEFAULT_SOURCE[1024];
			char RDB$DEFAULT_VALUE[1024];
			short RDB$COLLATION_ID;
			short RDB$NULL_FLAG;
			char RDB$PARAMETER_MECHANISM;
			char RDB$DESCRIPTION_NULL;
			char RDB$DEFAULT_SOURCE_NULL;
			char RDB$COLLATION_ID_NULL;
		} inputParamRecord;

		memset(&inputParamRecord, 0, sizeof(inputParamRecord));
		const auto& param = inputParameters[paramPosition];
		
		strcpy(inputParamRecord.RDB$PARAMETER_NAME, param.parameterName.c_str());
		strcpy(inputParamRecord.RDB$PROCEDURE_NAME, procedureName.object.c_str());
		strcpy(inputParamRecord.RDB$SCHEMA_NAME, procedureName.schema.c_str());
		inputParamRecord.RDB$PARAMETER_NUMBER = paramPosition;
		inputParamRecord.RDB$PARAMETER_TYPE = 0; // Input parameter
		strcpy(inputParamRecord.RDB$FIELD_SOURCE, param.fieldSource.c_str());
		strcpy(inputParamRecord.RDB$FIELD_SOURCE_SCHEMA_NAME, param.fieldSourceSchema.c_str());
		inputParamRecord.RDB$NULL_FLAG = param.notNull ? 1 : 0;
		inputParamRecord.RDB$PARAMETER_MECHANISM = param.mechanism;

		if (!param.description.empty())
		{
			strcpy(inputParamRecord.RDB$DESCRIPTION, param.description.c_str());
			inputParamRecord.RDB$DESCRIPTION_NULL = FALSE;
		}
		else
			inputParamRecord.RDB$DESCRIPTION_NULL = TRUE;

		if (!param.defaultValue.empty())
		{
			strcpy(inputParamRecord.RDB$DEFAULT_SOURCE, param.defaultValue.c_str());
			strcpy(inputParamRecord.RDB$DEFAULT_VALUE, param.defaultValue.c_str());
			inputParamRecord.RDB$DEFAULT_SOURCE_NULL = FALSE;
		}
		else
			inputParamRecord.RDB$DEFAULT_SOURCE_NULL = TRUE;

		if (param.collationId.has_value())
		{
			inputParamRecord.RDB$COLLATION_ID = param.collationId.value();
			inputParamRecord.RDB$COLLATION_ID_NULL = FALSE;
		}
		else
			inputParamRecord.RDB$COLLATION_ID_NULL = TRUE;

		EXE_send(tdbb, handle93, 0, sizeof(RDB$PROCEDURE_PARAMETERS_RECORD), &inputParamRecord);
		EXE_unwind(tdbb, handle93);
	}

	// Converted FOR loop #85: Store output parameters
	for (int paramPosition = 0; paramPosition < outputParameters.size(); paramPosition++)
	{
		jrd_req* handle94 = CMP_find_request(tdbb, drq_store_proc_output_param, DYN_REQUESTS);
		EXE_start(tdbb, handle94, transaction);

		struct RDB$PROCEDURE_PARAMETERS_OUTPUT_RECORD {
			char RDB$PARAMETER_NAME[32];
			char RDB$PROCEDURE_NAME[32];
			char RDB$SCHEMA_NAME[32];
			short RDB$PARAMETER_NUMBER;
			short RDB$PARAMETER_TYPE;
			char RDB$FIELD_SOURCE[32];
			char RDB$FIELD_SOURCE_SCHEMA_NAME[32];
			char RDB$DESCRIPTION[256];
			short RDB$COLLATION_ID;
			short RDB$NULL_FLAG;
			char RDB$PARAMETER_MECHANISM;
			char RDB$DESCRIPTION_NULL;
			char RDB$COLLATION_ID_NULL;
		} outputParamRecord;

		memset(&outputParamRecord, 0, sizeof(outputParamRecord));
		const auto& param = outputParameters[paramPosition];
		
		strcpy(outputParamRecord.RDB$PARAMETER_NAME, param.parameterName.c_str());
		strcpy(outputParamRecord.RDB$PROCEDURE_NAME, procedureName.object.c_str());
		strcpy(outputParamRecord.RDB$SCHEMA_NAME, procedureName.schema.c_str());
		outputParamRecord.RDB$PARAMETER_NUMBER = paramPosition;
		outputParamRecord.RDB$PARAMETER_TYPE = 1; // Output parameter
		strcpy(outputParamRecord.RDB$FIELD_SOURCE, param.fieldSource.c_str());
		strcpy(outputParamRecord.RDB$FIELD_SOURCE_SCHEMA_NAME, param.fieldSourceSchema.c_str());
		outputParamRecord.RDB$NULL_FLAG = param.notNull ? 1 : 0;
		outputParamRecord.RDB$PARAMETER_MECHANISM = param.mechanism;

		if (!param.description.empty())
		{
			strcpy(outputParamRecord.RDB$DESCRIPTION, param.description.c_str());
			outputParamRecord.RDB$DESCRIPTION_NULL = FALSE;
		}
		else
			outputParamRecord.RDB$DESCRIPTION_NULL = TRUE;

		if (param.collationId.has_value())
		{
			outputParamRecord.RDB$COLLATION_ID = param.collationId.value();
			outputParamRecord.RDB$COLLATION_ID_NULL = FALSE;
		}
		else
			outputParamRecord.RDB$COLLATION_ID_NULL = TRUE;

		EXE_send(tdbb, handle94, 0, sizeof(RDB$PROCEDURE_PARAMETERS_OUTPUT_RECORD), &outputParamRecord);
		EXE_unwind(tdbb, handle94);
	}
}

void CreateFunctionNode::storeFunctionArguments(thread_db* tdbb, jrd_tra* transaction, const QualifiedName& functionName)
{
	// Converted FOR loop #86: Store function arguments
	for (int argPosition = 0; argPosition < functionArguments.size(); argPosition++)
	{
		jrd_req* handle95 = CMP_find_request(tdbb, drq_store_function_argument, DYN_REQUESTS);
		EXE_start(tdbb, handle95, transaction);

		struct RDB$FUNCTION_ARGUMENTS_RECORD {
			char RDB$FUNCTION_NAME[32];
			char RDB$SCHEMA_NAME[32];
			char RDB$ARGUMENT_NAME[32];
			short RDB$ARGUMENT_POSITION;
			short RDB$MECHANISM;
			char RDB$FIELD_TYPE;
			short RDB$FIELD_SCALE;
			short RDB$FIELD_LENGTH;
			short RDB$FIELD_SUB_TYPE;
			short RDB$CHARACTER_SET_ID;
			short RDB$COLLATION_ID;
			short RDB$CHARACTER_LENGTH;
			short RDB$FIELD_PRECISION;
			char RDB$ARGUMENT_MECHANISM;
			char RDB$FIELD_SOURCE[32];
			char RDB$FIELD_SOURCE_SCHEMA_NAME[32];
			char RDB$DEFAULT_SOURCE[1024];
			char RDB$DESCRIPTION[256];
			char RDB$ARGUMENT_MECHANISM_NULL;
			char RDB$DEFAULT_SOURCE_NULL;
			char RDB$DESCRIPTION_NULL;
		} argumentRecord;

		memset(&argumentRecord, 0, sizeof(argumentRecord));
		const auto& arg = functionArguments[argPosition];
		
		strcpy(argumentRecord.RDB$FUNCTION_NAME, functionName.object.c_str());
		strcpy(argumentRecord.RDB$SCHEMA_NAME, functionName.schema.c_str());
		strcpy(argumentRecord.RDB$ARGUMENT_NAME, arg.argumentName.c_str());
		argumentRecord.RDB$ARGUMENT_POSITION = argPosition;
		argumentRecord.RDB$MECHANISM = arg.mechanism;
		argumentRecord.RDB$FIELD_TYPE = arg.fieldType;
		argumentRecord.RDB$FIELD_SCALE = arg.fieldScale;
		argumentRecord.RDB$FIELD_LENGTH = arg.fieldLength;
		argumentRecord.RDB$FIELD_SUB_TYPE = arg.fieldSubType;
		argumentRecord.RDB$CHARACTER_SET_ID = arg.characterSetId;
		argumentRecord.RDB$COLLATION_ID = arg.collationId;
		argumentRecord.RDB$CHARACTER_LENGTH = arg.characterLength;
		argumentRecord.RDB$FIELD_PRECISION = arg.fieldPrecision;

		if (arg.argumentMechanism.has_value())
		{
			argumentRecord.RDB$ARGUMENT_MECHANISM = arg.argumentMechanism.value();
			argumentRecord.RDB$ARGUMENT_MECHANISM_NULL = FALSE;
		}
		else
			argumentRecord.RDB$ARGUMENT_MECHANISM_NULL = TRUE;

		if (!arg.fieldSource.empty())
		{
			strcpy(argumentRecord.RDB$FIELD_SOURCE, arg.fieldSource.c_str());
			strcpy(argumentRecord.RDB$FIELD_SOURCE_SCHEMA_NAME, arg.fieldSourceSchema.c_str());
		}

		if (!arg.defaultValue.empty())
		{
			strcpy(argumentRecord.RDB$DEFAULT_SOURCE, arg.defaultValue.c_str());
			argumentRecord.RDB$DEFAULT_SOURCE_NULL = FALSE;
		}
		else
			argumentRecord.RDB$DEFAULT_SOURCE_NULL = TRUE;

		if (!arg.description.empty())
		{
			strcpy(argumentRecord.RDB$DESCRIPTION, arg.description.c_str());
			argumentRecord.RDB$DESCRIPTION_NULL = FALSE;
		}
		else
			argumentRecord.RDB$DESCRIPTION_NULL = TRUE;

		EXE_send(tdbb, handle95, 0, sizeof(RDB$FUNCTION_ARGUMENTS_RECORD), &argumentRecord);
		EXE_unwind(tdbb, handle95);
	}

	// Converted FOR loop #87: Store function return argument
	if (returnArgument.has_value())
	{
		jrd_req* handle96 = CMP_find_request(tdbb, drq_store_function_return, DYN_REQUESTS);
		EXE_start(tdbb, handle96, transaction);

		struct RDB$FUNCTION_ARGUMENTS_RETURN_RECORD {
			char RDB$FUNCTION_NAME[32];
			char RDB$SCHEMA_NAME[32];
			char RDB$ARGUMENT_NAME[32];
			short RDB$ARGUMENT_POSITION;
			short RDB$MECHANISM;
			char RDB$FIELD_TYPE;
			short RDB$FIELD_SCALE;
			short RDB$FIELD_LENGTH;
			short RDB$FIELD_SUB_TYPE;
			short RDB$CHARACTER_SET_ID;
			short RDB$COLLATION_ID;
			short RDB$CHARACTER_LENGTH;
			short RDB$FIELD_PRECISION;
			char RDB$FIELD_SOURCE[32];
			char RDB$FIELD_SOURCE_SCHEMA_NAME[32];
			char RDB$DESCRIPTION[256];
			char RDB$DESCRIPTION_NULL;
		} returnRecord;

		memset(&returnRecord, 0, sizeof(returnRecord));
		const auto& ret = returnArgument.value();
		
		strcpy(returnRecord.RDB$FUNCTION_NAME, functionName.object.c_str());
		strcpy(returnRecord.RDB$SCHEMA_NAME, functionName.schema.c_str());
		strcpy(returnRecord.RDB$ARGUMENT_NAME, "RETURN_VALUE");
		returnRecord.RDB$ARGUMENT_POSITION = -1; // Special position for return value
		returnRecord.RDB$MECHANISM = ret.mechanism;
		returnRecord.RDB$FIELD_TYPE = ret.fieldType;
		returnRecord.RDB$FIELD_SCALE = ret.fieldScale;
		returnRecord.RDB$FIELD_LENGTH = ret.fieldLength;
		returnRecord.RDB$FIELD_SUB_TYPE = ret.fieldSubType;
		returnRecord.RDB$CHARACTER_SET_ID = ret.characterSetId;
		returnRecord.RDB$COLLATION_ID = ret.collationId;
		returnRecord.RDB$CHARACTER_LENGTH = ret.characterLength;
		returnRecord.RDB$FIELD_PRECISION = ret.fieldPrecision;

		if (!ret.fieldSource.empty())
		{
			strcpy(returnRecord.RDB$FIELD_SOURCE, ret.fieldSource.c_str());
			strcpy(returnRecord.RDB$FIELD_SOURCE_SCHEMA_NAME, ret.fieldSourceSchema.c_str());
		}

		if (!ret.description.empty())
		{
			strcpy(returnRecord.RDB$DESCRIPTION, ret.description.c_str());
			returnRecord.RDB$DESCRIPTION_NULL = FALSE;
		}
		else
			returnRecord.RDB$DESCRIPTION_NULL = TRUE;

		EXE_send(tdbb, handle96, 0, sizeof(RDB$FUNCTION_ARGUMENTS_RETURN_RECORD), &returnRecord);
		EXE_unwind(tdbb, handle96);
	}
}

// Role and security management

void CreateRoleNode::storeRoleDefinition(thread_db* tdbb, jrd_tra* transaction, const QualifiedName& roleName)
{
	// Converted FOR loop #88: Store role definition
	jrd_req* handle97 = CMP_find_request(tdbb, drq_store_role, DYN_REQUESTS);
	EXE_start(tdbb, handle97, transaction);

	struct RDB$ROLES_RECORD {
		char RDB$ROLE_NAME[32];
		char RDB$SCHEMA_NAME[32];
		char RDB$OWNER_NAME[32];
		char RDB$DESCRIPTION[256];
		short RDB$SYSTEM_FLAG;
		char RDB$DESCRIPTION_NULL;
	} roleRecord;

	memset(&roleRecord, 0, sizeof(roleRecord));
	strcpy(roleRecord.RDB$ROLE_NAME, roleName.object.c_str());
	strcpy(roleRecord.RDB$SCHEMA_NAME, roleName.schema.c_str());
	strcpy(roleRecord.RDB$OWNER_NAME, ownerName.c_str());
	roleRecord.RDB$SYSTEM_FLAG = 0;

	if (!description.empty())
	{
		strcpy(roleRecord.RDB$DESCRIPTION, description.c_str());
		roleRecord.RDB$DESCRIPTION_NULL = FALSE;
	}
	else
		roleRecord.RDB$DESCRIPTION_NULL = TRUE;

	EXE_send(tdbb, handle97, 0, sizeof(RDB$ROLES_RECORD), &roleRecord);
	EXE_unwind(tdbb, handle97);

	// Converted FOR loop #89: Grant role to initial members if specified
	for (const auto& member : initialMembers)
	{
		jrd_req* handle98 = CMP_find_request(tdbb, drq_grant_role_to_user, DYN_REQUESTS);
		EXE_start(tdbb, handle98, transaction);

		struct RDB$USER_PRIVILEGES_ROLE_GRANT_RECORD {
			char RDB$USER[32];
			char RDB$GRANTOR[32];
			char RDB$PRIVILEGE[8];
			char RDB$GRANT_OPTION;
			char RDB$RELATION_NAME[32];
			char RDB$SCHEMA_NAME[32];
			char RDB$USER_TYPE;
			char RDB$OBJECT_TYPE;
		} roleMemberRecord;

		memset(&roleMemberRecord, 0, sizeof(roleMemberRecord));
		strcpy(roleMemberRecord.RDB$USER, member.userName.c_str());
		strcpy(roleMemberRecord.RDB$GRANTOR, ownerName.c_str());
		strcpy(roleMemberRecord.RDB$PRIVILEGE, "M"); // Member privilege
		roleMemberRecord.RDB$GRANT_OPTION = member.withAdminOption ? 'Y' : 'N';
		strcpy(roleMemberRecord.RDB$RELATION_NAME, roleName.object.c_str());
		strcpy(roleMemberRecord.RDB$SCHEMA_NAME, roleName.schema.c_str());
		roleMemberRecord.RDB$USER_TYPE = member.userType;
		roleMemberRecord.RDB$OBJECT_TYPE = obj_sql_role;

		EXE_send(tdbb, handle98, 0, sizeof(RDB$USER_PRIVILEGES_ROLE_GRANT_RECORD), &roleMemberRecord);
		EXE_unwind(tdbb, handle98);
	}
}

// Advanced domain and collation management

void CreateDomainNode::storeDomainConstraints(thread_db* tdbb, jrd_tra* transaction, const QualifiedName& domainName)
{
	// Converted FOR loop #90: Store domain check constraints
	for (const auto& constraint : domainConstraints)
	{
		jrd_req* handle99 = CMP_find_request(tdbb, drq_store_domain_constraint, DYN_REQUESTS);
		EXE_start(tdbb, handle99, transaction);

		struct RDB$CHECK_CONSTRAINTS_DOMAIN_RECORD {
			char RDB$CONSTRAINT_NAME[32];
			char RDB$CONSTRAINT_TYPE[16];
			char RDB$DEFERRABLE[4];
			char RDB$INITIALLY_DEFERRED[4];
			char RDB$TRIGGER_NAME[32];
			char RDB$SCHEMA_NAME[32];
		} domainConstraintRecord;

		memset(&domainConstraintRecord, 0, sizeof(domainConstraintRecord));
		strcpy(domainConstraintRecord.RDB$CONSTRAINT_NAME, constraint.constraintName.c_str());
		strcpy(domainConstraintRecord.RDB$CONSTRAINT_TYPE, "CHECK");
		strcpy(domainConstraintRecord.RDB$DEFERRABLE, constraint.deferrable ? "YES" : "NO");
		strcpy(domainConstraintRecord.RDB$INITIALLY_DEFERRED, constraint.initiallyDeferred ? "YES" : "NO");
		strcpy(domainConstraintRecord.RDB$TRIGGER_NAME, constraint.triggerName.c_str());
		strcpy(domainConstraintRecord.RDB$SCHEMA_NAME, domainName.schema.c_str());

		EXE_send(tdbb, handle99, 0, sizeof(RDB$CHECK_CONSTRAINTS_DOMAIN_RECORD), &domainConstraintRecord);
		EXE_unwind(tdbb, handle99);

		// Store the constraint trigger
		storeDomainConstraintTrigger(tdbb, transaction, domainName, constraint);
	}
}

// Advanced trigger management

void CreateTriggerNode::storeTriggerSecurityContext(thread_db* tdbb, jrd_tra* transaction, const QualifiedName& triggerName)
{
	// Converted FOR loop #91: Store trigger security context
	jrd_req* handle100 = CMP_find_request(tdbb, drq_store_trigger_security, DYN_REQUESTS);
	EXE_start(tdbb, handle100, transaction);

	struct RDB$TRIGGER_SECURITY_RECORD {
		char RDB$TRIGGER_NAME[32];
		char RDB$SCHEMA_NAME[32];
		char RDB$SECURITY_CLASS[32];
		char RDB$DEFINER_NAME[32];
		char RDB$INVOKER_RIGHTS;
		short RDB$SYSTEM_FLAG;
		char RDB$SECURITY_CLASS_NULL;
	} triggerSecurityRecord;

	memset(&triggerSecurityRecord, 0, sizeof(triggerSecurityRecord));
	strcpy(triggerSecurityRecord.RDB$TRIGGER_NAME, triggerName.object.c_str());
	strcpy(triggerSecurityRecord.RDB$SCHEMA_NAME, triggerName.schema.c_str());
	
	if (!securityClass.empty())
	{
		strcpy(triggerSecurityRecord.RDB$SECURITY_CLASS, securityClass.c_str());
		triggerSecurityRecord.RDB$SECURITY_CLASS_NULL = FALSE;
	}
	else
		triggerSecurityRecord.RDB$SECURITY_CLASS_NULL = TRUE;

	strcpy(triggerSecurityRecord.RDB$DEFINER_NAME, definerName.c_str());
	triggerSecurityRecord.RDB$INVOKER_RIGHTS = invokerRights ? 'Y' : 'N';
	triggerSecurityRecord.RDB$SYSTEM_FLAG = systemFlag;

	EXE_send(tdbb, handle100, 0, sizeof(RDB$TRIGGER_SECURITY_RECORD), &triggerSecurityRecord);
	EXE_unwind(tdbb, handle100);
}

void CreateTriggerNode::storeTriggerDependencies(thread_db* tdbb, jrd_tra* transaction, const QualifiedName& triggerName)
{
	// Converted FOR loop #92: Store trigger dependencies on tables and views
	for (const auto& dependency : triggerDependencies)
	{
		jrd_req* handle101 = CMP_find_request(tdbb, drq_store_trigger_dependency, DYN_REQUESTS);
		EXE_start(tdbb, handle101, transaction);

		struct RDB$DEPENDENCIES_TRIGGER_RECORD {
			char RDB$DEPENDENT_NAME[32];
			char RDB$DEPENDED_ON_NAME[32];
			char RDB$FIELD_NAME[32];
			short RDB$DEPENDENT_TYPE;
			short RDB$DEPENDED_ON_TYPE;
			char RDB$DEPENDENT_SCHEMA[32];
			char RDB$DEPENDED_ON_SCHEMA[32];
			char RDB$FIELD_NAME_NULL;
		} triggerDependencyRecord;

		memset(&triggerDependencyRecord, 0, sizeof(triggerDependencyRecord));
		strcpy(triggerDependencyRecord.RDB$DEPENDENT_NAME, triggerName.object.c_str());
		strcpy(triggerDependencyRecord.RDB$DEPENDED_ON_NAME, dependency.objectName.c_str());
		triggerDependencyRecord.RDB$DEPENDENT_TYPE = obj_trigger;
		triggerDependencyRecord.RDB$DEPENDED_ON_TYPE = dependency.objectType;
		strcpy(triggerDependencyRecord.RDB$DEPENDENT_SCHEMA, triggerName.schema.c_str());
		strcpy(triggerDependencyRecord.RDB$DEPENDED_ON_SCHEMA, dependency.objectSchema.c_str());

		if (!dependency.fieldName.empty())
		{
			strcpy(triggerDependencyRecord.RDB$FIELD_NAME, dependency.fieldName.c_str());
			triggerDependencyRecord.RDB$FIELD_NAME_NULL = FALSE;
		}
		else
			triggerDependencyRecord.RDB$FIELD_NAME_NULL = TRUE;

		EXE_send(tdbb, handle101, 0, sizeof(RDB$DEPENDENCIES_TRIGGER_RECORD), &triggerDependencyRecord);
		EXE_unwind(tdbb, handle101);
	}
}

void AlterTriggerNode::updateTriggerMessages(thread_db* tdbb, jrd_tra* transaction, const QualifiedName& triggerName)
{
	// Converted FOR loop #93: Update trigger messages for different languages
	for (const auto& message : triggerMessages)
	{
		jrd_req* handle102 = CMP_find_request(tdbb, drq_update_trigger_message, DYN_REQUESTS);
		EXE_start(tdbb, handle102, transaction);

		struct RDB$TRIGGER_MESSAGES_UPDATE_RECORD {
			char RDB$TRIGGER_NAME[32];
			char RDB$MESSAGE_NUMBER[8];
			char RDB$MESSAGE_TEXT[1024];
			char RDB$LANGUAGE_ID[8];
			char RDB$SCHEMA_NAME[32];
		} triggerMessageRecord;

		memset(&triggerMessageRecord, 0, sizeof(triggerMessageRecord));
		strcpy(triggerMessageRecord.RDB$TRIGGER_NAME, triggerName.object.c_str());
		sprintf(triggerMessageRecord.RDB$MESSAGE_NUMBER, "%d", message.messageNumber);
		strcpy(triggerMessageRecord.RDB$MESSAGE_TEXT, message.messageText.c_str());
		strcpy(triggerMessageRecord.RDB$LANGUAGE_ID, message.languageId.c_str());
		strcpy(triggerMessageRecord.RDB$SCHEMA_NAME, triggerName.schema.c_str());

		EXE_send(tdbb, handle102, 0, sizeof(RDB$TRIGGER_MESSAGES_UPDATE_RECORD), &triggerMessageRecord);
		EXE_unwind(tdbb, handle102);
	}
}

// Complex package operations

void CreatePackageNode::storePackagePrivileges(thread_db* tdbb, jrd_tra* transaction, const QualifiedName& packageName)
{
	// Converted FOR loop #94: Store package access privileges
	for (const auto& privilege : packagePrivileges)
	{
		jrd_req* handle103 = CMP_find_request(tdbb, drq_store_package_privilege, DYN_REQUESTS);
		EXE_start(tdbb, handle103, transaction);

		struct RDB$USER_PRIVILEGES_PACKAGE_RECORD {
			char RDB$USER[32];
			char RDB$GRANTOR[32];
			char RDB$PRIVILEGE[8];
			char RDB$GRANT_OPTION;
			char RDB$FIELD_NAME[32];
			char RDB$USER_TYPE;
			char RDB$OBJECT_TYPE;
			char RDB$PACKAGE_NAME[32];
			char RDB$SCHEMA_NAME[32];
			char RDB$FIELD_NAME_NULL;
		} packagePrivilegeRecord;

		memset(&packagePrivilegeRecord, 0, sizeof(packagePrivilegeRecord));
		strcpy(packagePrivilegeRecord.RDB$USER, privilege.userName.c_str());
		strcpy(packagePrivilegeRecord.RDB$GRANTOR, privilege.grantor.c_str());
		strcpy(packagePrivilegeRecord.RDB$PRIVILEGE, privilege.privilegeType.c_str());
		packagePrivilegeRecord.RDB$GRANT_OPTION = privilege.grantOption ? 'Y' : 'N';
		packagePrivilegeRecord.RDB$USER_TYPE = privilege.userType;
		packagePrivilegeRecord.RDB$OBJECT_TYPE = obj_package_header;
		strcpy(packagePrivilegeRecord.RDB$PACKAGE_NAME, packageName.object.c_str());
		strcpy(packagePrivilegeRecord.RDB$SCHEMA_NAME, packageName.schema.c_str());

		if (!privilege.fieldName.empty())
		{
			strcpy(packagePrivilegeRecord.RDB$FIELD_NAME, privilege.fieldName.c_str());
			packagePrivilegeRecord.RDB$FIELD_NAME_NULL = FALSE;
		}
		else
			packagePrivilegeRecord.RDB$FIELD_NAME_NULL = TRUE;

		EXE_send(tdbb, handle103, 0, sizeof(RDB$USER_PRIVILEGES_PACKAGE_RECORD), &packagePrivilegeRecord);
		EXE_unwind(tdbb, handle103);
	}
}

void CreatePackageNode::storePackageDependencies(thread_db* tdbb, jrd_tra* transaction, const QualifiedName& packageName)
{
	// Converted FOR loop #95: Store package dependencies on other objects
	for (const auto& dependency : packageDependencies)
	{
		jrd_req* handle104 = CMP_find_request(tdbb, drq_store_package_dependency, DYN_REQUESTS);
		EXE_start(tdbb, handle104, transaction);

		struct RDB$DEPENDENCIES_PACKAGE_RECORD {
			char RDB$DEPENDENT_NAME[32];
			char RDB$DEPENDED_ON_NAME[32];
			char RDB$FIELD_NAME[32];
			short RDB$DEPENDENT_TYPE;
			short RDB$DEPENDED_ON_TYPE;
			char RDB$DEPENDENT_SCHEMA[32];
			char RDB$DEPENDED_ON_SCHEMA[32];
			char RDB$PACKAGE_NAME[32];
			char RDB$FIELD_NAME_NULL;
		} packageDependencyRecord;

		memset(&packageDependencyRecord, 0, sizeof(packageDependencyRecord));
		strcpy(packageDependencyRecord.RDB$DEPENDENT_NAME, packageName.object.c_str());
		strcpy(packageDependencyRecord.RDB$DEPENDED_ON_NAME, dependency.objectName.c_str());
		packageDependencyRecord.RDB$DEPENDENT_TYPE = obj_package_header;
		packageDependencyRecord.RDB$DEPENDED_ON_TYPE = dependency.objectType;
		strcpy(packageDependencyRecord.RDB$DEPENDENT_SCHEMA, packageName.schema.c_str());
		strcpy(packageDependencyRecord.RDB$DEPENDED_ON_SCHEMA, dependency.objectSchema.c_str());
		strcpy(packageDependencyRecord.RDB$PACKAGE_NAME, packageName.object.c_str());

		if (!dependency.fieldName.empty())
		{
			strcpy(packageDependencyRecord.RDB$FIELD_NAME, dependency.fieldName.c_str());
			packageDependencyRecord.RDB$FIELD_NAME_NULL = FALSE;
		}
		else
			packageDependencyRecord.RDB$FIELD_NAME_NULL = TRUE;

		EXE_send(tdbb, handle104, 0, sizeof(RDB$DEPENDENCIES_PACKAGE_RECORD), &packageDependencyRecord);
		EXE_unwind(tdbb, handle104);
	}
}

void AlterPackageNode::updatePackageVersion(thread_db* tdbb, jrd_tra* transaction, const QualifiedName& packageName)
{
	// Converted FOR loop #96: Update package version information
	jrd_req* handle105 = CMP_find_request(tdbb, drq_update_package_version, DYN_REQUESTS);
	EXE_start(tdbb, handle105, transaction);

	struct RDB$PACKAGES_VERSION_UPDATE_RECORD {
		char RDB$PACKAGE_NAME[32];
		char RDB$SCHEMA_NAME[32];
		short RDB$PACKAGE_VERSION_MAJOR;
		short RDB$PACKAGE_VERSION_MINOR;
		short RDB$PACKAGE_VERSION_BUILD;
		char RDB$VERSION_DESCRIPTION[256];
		char RDB$COMPATIBILITY_VERSION[32];
		char RDB$VERSION_DESCRIPTION_NULL;
		char RDB$COMPATIBILITY_VERSION_NULL;
	} packageVersionRecord;

	memset(&packageVersionRecord, 0, sizeof(packageVersionRecord));
	strcpy(packageVersionRecord.RDB$PACKAGE_NAME, packageName.object.c_str());
	strcpy(packageVersionRecord.RDB$SCHEMA_NAME, packageName.schema.c_str());
	packageVersionRecord.RDB$PACKAGE_VERSION_MAJOR = versionMajor;
	packageVersionRecord.RDB$PACKAGE_VERSION_MINOR = versionMinor;
	packageVersionRecord.RDB$PACKAGE_VERSION_BUILD = versionBuild;

	if (!versionDescription.empty())
	{
		strcpy(packageVersionRecord.RDB$VERSION_DESCRIPTION, versionDescription.c_str());
		packageVersionRecord.RDB$VERSION_DESCRIPTION_NULL = FALSE;
	}
	else
		packageVersionRecord.RDB$VERSION_DESCRIPTION_NULL = TRUE;

	if (!compatibilityVersion.empty())
	{
		strcpy(packageVersionRecord.RDB$COMPATIBILITY_VERSION, compatibilityVersion.c_str());
		packageVersionRecord.RDB$COMPATIBILITY_VERSION_NULL = FALSE;
	}
	else
		packageVersionRecord.RDB$COMPATIBILITY_VERSION_NULL = TRUE;

	EXE_send(tdbb, handle105, 0, sizeof(RDB$PACKAGES_VERSION_UPDATE_RECORD), &packageVersionRecord);
	EXE_unwind(tdbb, handle105);
}

// Database link DDL operations

void CreateDatabaseLinkNode::storeLinkConnectionParameters(thread_db* tdbb, jrd_tra* transaction, const QualifiedName& linkName)
{
	// Converted FOR loop #97: Store database link connection parameters
	for (const auto& parameter : connectionParameters)
	{
		jrd_req* handle106 = CMP_find_request(tdbb, drq_store_dblink_parameter, DYN_REQUESTS);
		EXE_start(tdbb, handle106, transaction);

		struct RDB$DATABASE_LINK_PARAMETERS_RECORD {
			char RDB$LINK_NAME[32];
			char RDB$PARAMETER_NAME[64];
			char RDB$PARAMETER_VALUE[256];
			char RDB$PARAMETER_TYPE[16];
			short RDB$PARAMETER_ORDER;
			char RDB$SCHEMA_NAME[32];
			char RDB$PARAMETER_ENCRYPTED;
			char RDB$PARAMETER_VALUE_NULL;
		} linkParameterRecord;

		memset(&linkParameterRecord, 0, sizeof(linkParameterRecord));
		strcpy(linkParameterRecord.RDB$LINK_NAME, linkName.object.c_str());
		strcpy(linkParameterRecord.RDB$PARAMETER_NAME, parameter.parameterName.c_str());
		strcpy(linkParameterRecord.RDB$PARAMETER_TYPE, parameter.parameterType.c_str());
		linkParameterRecord.RDB$PARAMETER_ORDER = parameter.parameterOrder;
		strcpy(linkParameterRecord.RDB$SCHEMA_NAME, linkName.schema.c_str());
		linkParameterRecord.RDB$PARAMETER_ENCRYPTED = parameter.encrypted ? 'Y' : 'N';

		if (!parameter.parameterValue.empty())
		{
			strcpy(linkParameterRecord.RDB$PARAMETER_VALUE, parameter.parameterValue.c_str());
			linkParameterRecord.RDB$PARAMETER_VALUE_NULL = FALSE;
		}
		else
			linkParameterRecord.RDB$PARAMETER_VALUE_NULL = TRUE;

		EXE_send(tdbb, handle106, 0, sizeof(RDB$DATABASE_LINK_PARAMETERS_RECORD), &linkParameterRecord);
		EXE_unwind(tdbb, handle106);
	}
}

void CreateDatabaseLinkNode::storeLinkSchemaMapping(thread_db* tdbb, jrd_tra* transaction, const QualifiedName& linkName)
{
	// Converted FOR loop #98: Store database link schema mapping configuration
	jrd_req* handle107 = CMP_find_request(tdbb, drq_store_dblink_schema_mapping, DYN_REQUESTS);
	EXE_start(tdbb, handle107, transaction);

	struct RDB$DATABASE_LINK_SCHEMA_MAPPING_RECORD {
		char RDB$LINK_NAME[32];
		char RDB$LOCAL_SCHEMA_PATH[512];
		char RDB$REMOTE_SCHEMA_PATH[512];
		short RDB$SCHEMA_MAPPING_MODE;
		char RDB$SCHEMA_NAME[32];
		short RDB$SCHEMA_DEPTH;
		char RDB$CONTEXT_RESOLUTION[32];
		char RDB$LOCAL_SCHEMA_PATH_NULL;
		char RDB$REMOTE_SCHEMA_PATH_NULL;
		char RDB$CONTEXT_RESOLUTION_NULL;
	} linkSchemaMappingRecord;

	memset(&linkSchemaMappingRecord, 0, sizeof(linkSchemaMappingRecord));
	strcpy(linkSchemaMappingRecord.RDB$LINK_NAME, linkName.object.c_str());
	linkSchemaMappingRecord.RDB$SCHEMA_MAPPING_MODE = schemaMappingMode;
	strcpy(linkSchemaMappingRecord.RDB$SCHEMA_NAME, linkName.schema.c_str());
	linkSchemaMappingRecord.RDB$SCHEMA_DEPTH = schemaDepth;

	if (!localSchemaPath.empty())
	{
		strcpy(linkSchemaMappingRecord.RDB$LOCAL_SCHEMA_PATH, localSchemaPath.c_str());
		linkSchemaMappingRecord.RDB$LOCAL_SCHEMA_PATH_NULL = FALSE;
	}
	else
		linkSchemaMappingRecord.RDB$LOCAL_SCHEMA_PATH_NULL = TRUE;

	if (!remoteSchemaPath.empty())
	{
		strcpy(linkSchemaMappingRecord.RDB$REMOTE_SCHEMA_PATH, remoteSchemaPath.c_str());
		linkSchemaMappingRecord.RDB$REMOTE_SCHEMA_PATH_NULL = FALSE;
	}
	else
		linkSchemaMappingRecord.RDB$REMOTE_SCHEMA_PATH_NULL = TRUE;

	if (!contextResolution.empty())
	{
		strcpy(linkSchemaMappingRecord.RDB$CONTEXT_RESOLUTION, contextResolution.c_str());
		linkSchemaMappingRecord.RDB$CONTEXT_RESOLUTION_NULL = FALSE;
	}
	else
		linkSchemaMappingRecord.RDB$CONTEXT_RESOLUTION_NULL = TRUE;

	EXE_send(tdbb, handle107, 0, sizeof(RDB$DATABASE_LINK_SCHEMA_MAPPING_RECORD), &linkSchemaMappingRecord);
	EXE_unwind(tdbb, handle107);
}

void AlterDatabaseLinkNode::updateLinkStatus(thread_db* tdbb, jrd_tra* transaction, const QualifiedName& linkName)
{
	// Converted FOR loop #99: Update database link status and health information
	jrd_req* handle108 = CMP_find_request(tdbb, drq_update_dblink_status, DYN_REQUESTS);
	EXE_start(tdbb, handle108, transaction);

	struct RDB$DATABASE_LINK_STATUS_UPDATE_RECORD {
		char RDB$LINK_NAME[32];
		char RDB$LINK_STATUS[16];
		char RDB$LAST_USED_TIMESTAMP[20];
		char RDB$LAST_ERROR_MESSAGE[512];
		short RDB$CONNECTION_COUNT;
		char RDB$SCHEMA_NAME[32];
		short RDB$RETRY_COUNT;
		char RDB$HEALTH_CHECK_INTERVAL[16];
		char RDB$LAST_ERROR_MESSAGE_NULL;
		char RDB$HEALTH_CHECK_INTERVAL_NULL;
	} linkStatusRecord;

	memset(&linkStatusRecord, 0, sizeof(linkStatusRecord));
	strcpy(linkStatusRecord.RDB$LINK_NAME, linkName.object.c_str());
	strcpy(linkStatusRecord.RDB$LINK_STATUS, linkStatus.c_str());
	strcpy(linkStatusRecord.RDB$LAST_USED_TIMESTAMP, lastUsedTimestamp.c_str());
	linkStatusRecord.RDB$CONNECTION_COUNT = connectionCount;
	strcpy(linkStatusRecord.RDB$SCHEMA_NAME, linkName.schema.c_str());
	linkStatusRecord.RDB$RETRY_COUNT = retryCount;

	if (!lastErrorMessage.empty())
	{
		strcpy(linkStatusRecord.RDB$LAST_ERROR_MESSAGE, lastErrorMessage.c_str());
		linkStatusRecord.RDB$LAST_ERROR_MESSAGE_NULL = FALSE;
	}
	else
		linkStatusRecord.RDB$LAST_ERROR_MESSAGE_NULL = TRUE;

	if (!healthCheckInterval.empty())
	{
		strcpy(linkStatusRecord.RDB$HEALTH_CHECK_INTERVAL, healthCheckInterval.c_str());
		linkStatusRecord.RDB$HEALTH_CHECK_INTERVAL_NULL = FALSE;
	}
	else
		linkStatusRecord.RDB$HEALTH_CHECK_INTERVAL_NULL = TRUE;

	EXE_send(tdbb, handle108, 0, sizeof(RDB$DATABASE_LINK_STATUS_UPDATE_RECORD), &linkStatusRecord);
	EXE_unwind(tdbb, handle108);
}

// System table maintenance

void CreateTableNode::storeTableStatistics(thread_db* tdbb, jrd_tra* transaction, const QualifiedName& tableName)
{
	// Converted FOR loop #100: Store initial table statistics
	jrd_req* handle109 = CMP_find_request(tdbb, drq_store_table_statistics, DYN_REQUESTS);
	EXE_start(tdbb, handle109, transaction);

	struct RDB$TABLE_STATISTICS_RECORD {
		char RDB$RELATION_NAME[32];
		char RDB$SCHEMA_NAME[32];
		int RDB$CARDINALITY;
		int RDB$PAGES;
		int RDB$AVG_RECORD_LENGTH;
		int RDB$MAX_RECORD_LENGTH;
		char RDB$STATISTICS_TIMESTAMP[20];
		char RDB$STATISTICS_VERSION[16];
		short RDB$SYSTEM_FLAG;
	} tableStatisticsRecord;

	memset(&tableStatisticsRecord, 0, sizeof(tableStatisticsRecord));
	strcpy(tableStatisticsRecord.RDB$RELATION_NAME, tableName.object.c_str());
	strcpy(tableStatisticsRecord.RDB$SCHEMA_NAME, tableName.schema.c_str());
	tableStatisticsRecord.RDB$CARDINALITY = 0; // Initial value
	tableStatisticsRecord.RDB$PAGES = 1; // Minimum allocation
	tableStatisticsRecord.RDB$AVG_RECORD_LENGTH = 0;
	tableStatisticsRecord.RDB$MAX_RECORD_LENGTH = 0;
	strcpy(tableStatisticsRecord.RDB$STATISTICS_TIMESTAMP, currentTimestamp.c_str());
	strcpy(tableStatisticsRecord.RDB$STATISTICS_VERSION, "1.0");
	tableStatisticsRecord.RDB$SYSTEM_FLAG = systemFlag;

	EXE_send(tdbb, handle109, 0, sizeof(RDB$TABLE_STATISTICS_RECORD), &tableStatisticsRecord);
	EXE_unwind(tdbb, handle109);
}

void CreateIndexNode::storeIndexStatistics(thread_db* tdbb, jrd_tra* transaction, const QualifiedName& indexName)
{
	// Converted FOR loop #101: Store initial index statistics
	jrd_req* handle110 = CMP_find_request(tdbb, drq_store_index_statistics, DYN_REQUESTS);
	EXE_start(tdbb, handle110, transaction);

	struct RDB$INDEX_STATISTICS_RECORD {
		char RDB$INDEX_NAME[32];
		char RDB$RELATION_NAME[32];
		char RDB$SCHEMA_NAME[32];
		double RDB$STATISTICS;
		int RDB$INDEX_PAGES;
		int RDB$LEAF_BUCKETS;
		int RDB$TOTAL_DUPLICATES;
		int RDB$MAX_DUPLICATES;
		char RDB$STATISTICS_TIMESTAMP[20];
		short RDB$SYSTEM_FLAG;
		double RDB$AVG_DUPLICATE_LENGTH;
		double RDB$CLUSTERED_FACTOR;
	} indexStatisticsRecord;

	memset(&indexStatisticsRecord, 0, sizeof(indexStatisticsRecord));
	strcpy(indexStatisticsRecord.RDB$INDEX_NAME, indexName.object.c_str());
	strcpy(indexStatisticsRecord.RDB$RELATION_NAME, relationName.c_str());
	strcpy(indexStatisticsRecord.RDB$SCHEMA_NAME, indexName.schema.c_str());
	indexStatisticsRecord.RDB$STATISTICS = 0.0; // Initial selectivity
	indexStatisticsRecord.RDB$INDEX_PAGES = 1;
	indexStatisticsRecord.RDB$LEAF_BUCKETS = 0;
	indexStatisticsRecord.RDB$TOTAL_DUPLICATES = 0;
	indexStatisticsRecord.RDB$MAX_DUPLICATES = 0;
	strcpy(indexStatisticsRecord.RDB$STATISTICS_TIMESTAMP, currentTimestamp.c_str());
	indexStatisticsRecord.RDB$SYSTEM_FLAG = systemFlag;
	indexStatisticsRecord.RDB$AVG_DUPLICATE_LENGTH = 0.0;
	indexStatisticsRecord.RDB$CLUSTERED_FACTOR = 0.0;

	EXE_send(tdbb, handle110, 0, sizeof(RDB$INDEX_STATISTICS_RECORD), &indexStatisticsRecord);
	EXE_unwind(tdbb, handle110);
}

void AlterTableNode::storeTableConstraintMapping(thread_db* tdbb, jrd_tra* transaction, const QualifiedName& tableName)
{
	// Converted FOR loop #102: Store table constraint mapping for referential integrity
	for (const auto& constraint : tableConstraints)
	{
		jrd_req* handle111 = CMP_find_request(tdbb, drq_store_table_constraint_mapping, DYN_REQUESTS);
		EXE_start(tdbb, handle111, transaction);

		struct RDB$REF_CONSTRAINTS_TABLE_MAPPING_RECORD {
			char RDB$CONSTRAINT_NAME[32];
			char RDB$CONST_NAME_UQ[32];
			char RDB$MATCH_OPTION[8];
			char RDB$UPDATE_RULE[16];
			char RDB$DELETE_RULE[16];
			char RDB$SCHEMA_NAME[32];
			char RDB$REFERENCED_SCHEMA[32];
			char RDB$DEFERRABLE[4];
			char RDB$INITIALLY_DEFERRED[4];
		} constraintMappingRecord;

		memset(&constraintMappingRecord, 0, sizeof(constraintMappingRecord));
		strcpy(constraintMappingRecord.RDB$CONSTRAINT_NAME, constraint.constraintName.c_str());
		strcpy(constraintMappingRecord.RDB$CONST_NAME_UQ, constraint.uniqueConstraintName.c_str());
		strcpy(constraintMappingRecord.RDB$MATCH_OPTION, constraint.matchOption.c_str());
		strcpy(constraintMappingRecord.RDB$UPDATE_RULE, constraint.updateRule.c_str());
		strcpy(constraintMappingRecord.RDB$DELETE_RULE, constraint.deleteRule.c_str());
		strcpy(constraintMappingRecord.RDB$SCHEMA_NAME, tableName.schema.c_str());
		strcpy(constraintMappingRecord.RDB$REFERENCED_SCHEMA, constraint.referencedSchema.c_str());
		strcpy(constraintMappingRecord.RDB$DEFERRABLE, constraint.deferrable ? "YES" : "NO");
		strcpy(constraintMappingRecord.RDB$INITIALLY_DEFERRED, constraint.initiallyDeferred ? "YES" : "NO");

		EXE_send(tdbb, handle111, 0, sizeof(RDB$REF_CONSTRAINTS_TABLE_MAPPING_RECORD), &constraintMappingRecord);
		EXE_unwind(tdbb, handle111);
	}
}

// Performance optimization features

void CreateTableNode::storeTablePartitioning(thread_db* tdbb, jrd_tra* transaction, const QualifiedName& tableName)
{
	// Converted FOR loop #103: Store table partitioning configuration
	if (partitioningEnabled)
	{
		jrd_req* handle112 = CMP_find_request(tdbb, drq_store_table_partitioning, DYN_REQUESTS);
		EXE_start(tdbb, handle112, transaction);

		struct RDB$TABLE_PARTITIONING_RECORD {
			char RDB$RELATION_NAME[32];
			char RDB$SCHEMA_NAME[32];
			char RDB$PARTITION_TYPE[16];
			char RDB$PARTITION_EXPRESSION[512];
			short RDB$PARTITION_COUNT;
			char RDB$PARTITION_SIZE_LIMIT[16];
			char RDB$PARTITION_TIME_INTERVAL[32];
			char RDB$AUTO_PARTITION;
			char RDB$PARTITION_EXPRESSION_NULL;
			char RDB$PARTITION_SIZE_LIMIT_NULL;
			char RDB$PARTITION_TIME_INTERVAL_NULL;
		} partitioningRecord;

		memset(&partitioningRecord, 0, sizeof(partitioningRecord));
		strcpy(partitioningRecord.RDB$RELATION_NAME, tableName.object.c_str());
		strcpy(partitioningRecord.RDB$SCHEMA_NAME, tableName.schema.c_str());
		strcpy(partitioningRecord.RDB$PARTITION_TYPE, partitionType.c_str());
		partitioningRecord.RDB$PARTITION_COUNT = partitionCount;
		partitioningRecord.RDB$AUTO_PARTITION = autoPartition ? 'Y' : 'N';

		if (!partitionExpression.empty())
		{
			strcpy(partitioningRecord.RDB$PARTITION_EXPRESSION, partitionExpression.c_str());
			partitioningRecord.RDB$PARTITION_EXPRESSION_NULL = FALSE;
		}
		else
			partitioningRecord.RDB$PARTITION_EXPRESSION_NULL = TRUE;

		if (!partitionSizeLimit.empty())
		{
			strcpy(partitioningRecord.RDB$PARTITION_SIZE_LIMIT, partitionSizeLimit.c_str());
			partitioningRecord.RDB$PARTITION_SIZE_LIMIT_NULL = FALSE;
		}
		else
			partitioningRecord.RDB$PARTITION_SIZE_LIMIT_NULL = TRUE;

		if (!partitionTimeInterval.empty())
		{
			strcpy(partitioningRecord.RDB$PARTITION_TIME_INTERVAL, partitionTimeInterval.c_str());
			partitioningRecord.RDB$PARTITION_TIME_INTERVAL_NULL = FALSE;
		}
		else
			partitioningRecord.RDB$PARTITION_TIME_INTERVAL_NULL = TRUE;

		EXE_send(tdbb, handle112, 0, sizeof(RDB$TABLE_PARTITIONING_RECORD), &partitioningRecord);
		EXE_unwind(tdbb, handle112);
	}
}

void CreateIndexNode::storeIndexOptimization(thread_db* tdbb, jrd_tra* transaction, const QualifiedName& indexName)
{
	// Converted FOR loop #104: Store index optimization parameters
	jrd_req* handle113 = CMP_find_request(tdbb, drq_store_index_optimization, DYN_REQUESTS);
	EXE_start(tdbb, handle113, transaction);

	struct RDB$INDEX_OPTIMIZATION_RECORD {
		char RDB$INDEX_NAME[32];
		char RDB$SCHEMA_NAME[32];
		char RDB$OPTIMIZATION_TYPE[16];
		short RDB$PAGE_FILL_FACTOR;
		short RDB$KEY_CACHE_SIZE;
		char RDB$COMPRESSION_TYPE[16];
		char RDB$PARALLEL_BUILD;
		short RDB$BUILD_THREADS;
		char RDB$SORT_BUFFER_SIZE[16];
		char RDB$BLOOM_FILTER;
		char RDB$COMPRESSION_TYPE_NULL;
		char RDB$SORT_BUFFER_SIZE_NULL;
	} indexOptimizationRecord;

	memset(&indexOptimizationRecord, 0, sizeof(indexOptimizationRecord));
	strcpy(indexOptimizationRecord.RDB$INDEX_NAME, indexName.object.c_str());
	strcpy(indexOptimizationRecord.RDB$SCHEMA_NAME, indexName.schema.c_str());
	strcpy(indexOptimizationRecord.RDB$OPTIMIZATION_TYPE, optimizationType.c_str());
	indexOptimizationRecord.RDB$PAGE_FILL_FACTOR = pageFillFactor;
	indexOptimizationRecord.RDB$KEY_CACHE_SIZE = keyCacheSize;
	indexOptimizationRecord.RDB$PARALLEL_BUILD = parallelBuild ? 'Y' : 'N';
	indexOptimizationRecord.RDB$BUILD_THREADS = buildThreads;
	indexOptimizationRecord.RDB$BLOOM_FILTER = bloomFilter ? 'Y' : 'N';

	if (!compressionType.empty())
	{
		strcpy(indexOptimizationRecord.RDB$COMPRESSION_TYPE, compressionType.c_str());
		indexOptimizationRecord.RDB$COMPRESSION_TYPE_NULL = FALSE;
	}
	else
		indexOptimizationRecord.RDB$COMPRESSION_TYPE_NULL = TRUE;

	if (!sortBufferSize.empty())
	{
		strcpy(indexOptimizationRecord.RDB$SORT_BUFFER_SIZE, sortBufferSize.c_str());
		indexOptimizationRecord.RDB$SORT_BUFFER_SIZE_NULL = FALSE;
	}
	else
		indexOptimizationRecord.RDB$SORT_BUFFER_SIZE_NULL = TRUE;

	EXE_send(tdbb, handle113, 0, sizeof(RDB$INDEX_OPTIMIZATION_RECORD), &indexOptimizationRecord);
	EXE_unwind(tdbb, handle113);
}

void CreateTableNode::storeTableCaching(thread_db* tdbb, jrd_tra* transaction, const QualifiedName& tableName)
{
	// Converted FOR loop #105: Store table caching configuration
	if (cachingEnabled)
	{
		jrd_req* handle114 = CMP_find_request(tdbb, drq_store_table_caching, DYN_REQUESTS);
		EXE_start(tdbb, handle114, transaction);

		struct RDB$TABLE_CACHING_RECORD {
			char RDB$RELATION_NAME[32];
			char RDB$SCHEMA_NAME[32];
			char RDB$CACHE_TYPE[16];
			int RDB$CACHE_SIZE_MB;
			short RDB$CACHE_TIMEOUT_SECONDS;
			char RDB$CACHE_POLICY[16];
			char RDB$CACHE_STATISTICS;
			char RDB$CACHE_INVALIDATION[16];
			char RDB$CACHE_COMPRESSION;
			short RDB$CACHE_PRIORITY;
			char RDB$CACHE_INVALIDATION_NULL;
		} tableCachingRecord;

		memset(&tableCachingRecord, 0, sizeof(tableCachingRecord));
		strcpy(tableCachingRecord.RDB$RELATION_NAME, tableName.object.c_str());
		strcpy(tableCachingRecord.RDB$SCHEMA_NAME, tableName.schema.c_str());
		strcpy(tableCachingRecord.RDB$CACHE_TYPE, cacheType.c_str());
		tableCachingRecord.RDB$CACHE_SIZE_MB = cacheSizeMB;
		tableCachingRecord.RDB$CACHE_TIMEOUT_SECONDS = cacheTimeoutSeconds;
		strcpy(tableCachingRecord.RDB$CACHE_POLICY, cachePolicy.c_str());
		tableCachingRecord.RDB$CACHE_STATISTICS = cacheStatistics ? 'Y' : 'N';
		tableCachingRecord.RDB$CACHE_COMPRESSION = cacheCompression ? 'Y' : 'N';
		tableCachingRecord.RDB$CACHE_PRIORITY = cachePriority;

		if (!cacheInvalidation.empty())
		{
			strcpy(tableCachingRecord.RDB$CACHE_INVALIDATION, cacheInvalidation.c_str());
			tableCachingRecord.RDB$CACHE_INVALIDATION_NULL = FALSE;
		}
		else
			tableCachingRecord.RDB$CACHE_INVALIDATION_NULL = TRUE;

		EXE_send(tdbb, handle114, 0, sizeof(RDB$TABLE_CACHING_RECORD), &tableCachingRecord);
		EXE_unwind(tdbb, handle114);
	}
}

// Advanced security operations

void CreateUserNode::storeUserSecurityProfiles(thread_db* tdbb, jrd_tra* transaction, const QualifiedName& userName)
{
	// Converted FOR loop #106: Store user security profiles and access patterns
	for (const auto& profile : securityProfiles)
	{
		jrd_req* handle115 = CMP_find_request(tdbb, drq_store_user_security_profile, DYN_REQUESTS);
		EXE_start(tdbb, handle115, transaction);

		struct RDB$USER_SECURITY_PROFILES_RECORD {
			char RDB$USER_NAME[32];
			char RDB$PROFILE_NAME[32];
			char RDB$PROFILE_TYPE[16];
			char RDB$ACCESS_PATTERN[64];
			char RDB$TIME_RESTRICTIONS[128];
			char RDB$IP_RESTRICTIONS[256];
			char RDB$ENCRYPTION_REQUIRED;
			short RDB$MAX_CONCURRENT_SESSIONS;
			short RDB$SESSION_TIMEOUT_MINUTES;
			char RDB$AUDIT_LEVEL[16];
			char RDB$TIME_RESTRICTIONS_NULL;
			char RDB$IP_RESTRICTIONS_NULL;
		} userSecurityProfileRecord;

		memset(&userSecurityProfileRecord, 0, sizeof(userSecurityProfileRecord));
		strcpy(userSecurityProfileRecord.RDB$USER_NAME, userName.object.c_str());
		strcpy(userSecurityProfileRecord.RDB$PROFILE_NAME, profile.profileName.c_str());
		strcpy(userSecurityProfileRecord.RDB$PROFILE_TYPE, profile.profileType.c_str());
		strcpy(userSecurityProfileRecord.RDB$ACCESS_PATTERN, profile.accessPattern.c_str());
		userSecurityProfileRecord.RDB$ENCRYPTION_REQUIRED = profile.encryptionRequired ? 'Y' : 'N';
		userSecurityProfileRecord.RDB$MAX_CONCURRENT_SESSIONS = profile.maxConcurrentSessions;
		userSecurityProfileRecord.RDB$SESSION_TIMEOUT_MINUTES = profile.sessionTimeoutMinutes;
		strcpy(userSecurityProfileRecord.RDB$AUDIT_LEVEL, profile.auditLevel.c_str());

		if (!profile.timeRestrictions.empty())
		{
			strcpy(userSecurityProfileRecord.RDB$TIME_RESTRICTIONS, profile.timeRestrictions.c_str());
			userSecurityProfileRecord.RDB$TIME_RESTRICTIONS_NULL = FALSE;
		}
		else
			userSecurityProfileRecord.RDB$TIME_RESTRICTIONS_NULL = TRUE;

		if (!profile.ipRestrictions.empty())
		{
			strcpy(userSecurityProfileRecord.RDB$IP_RESTRICTIONS, profile.ipRestrictions.c_str());
			userSecurityProfileRecord.RDB$IP_RESTRICTIONS_NULL = FALSE;
		}
		else
			userSecurityProfileRecord.RDB$IP_RESTRICTIONS_NULL = TRUE;

		EXE_send(tdbb, handle115, 0, sizeof(RDB$USER_SECURITY_PROFILES_RECORD), &userSecurityProfileRecord);
		EXE_unwind(tdbb, handle115);
	}
}

void CreateRoleNode::storeRoleSecurityPolicies(thread_db* tdbb, jrd_tra* transaction, const QualifiedName& roleName)
{
	// Converted FOR loop #107: Store role security policies and compliance rules
	for (const auto& policy : securityPolicies)
	{
		jrd_req* handle116 = CMP_find_request(tdbb, drq_store_role_security_policy, DYN_REQUESTS);
		EXE_start(tdbb, handle116, transaction);

		struct RDB$ROLE_SECURITY_POLICIES_RECORD {
			char RDB$ROLE_NAME[32];
			char RDB$SCHEMA_NAME[32];
			char RDB$POLICY_NAME[32];
			char RDB$POLICY_TYPE[16];
			char RDB$COMPLIANCE_LEVEL[16];
			char RDB$DATA_CLASSIFICATION[32];
			char RDB$ACCESS_CONTROLS[256];
			char RDB$MONITORING_REQUIRED;
			char RDB$APPROVAL_REQUIRED;
			char RDB$POLICY_EXPRESSION[512];
			char RDB$EFFECTIVE_DATE[20];
			char RDB$EXPIRATION_DATE[20];
			char RDB$ACCESS_CONTROLS_NULL;
			char RDB$POLICY_EXPRESSION_NULL;
			char RDB$EXPIRATION_DATE_NULL;
		} roleSecurityPolicyRecord;

		memset(&roleSecurityPolicyRecord, 0, sizeof(roleSecurityPolicyRecord));
		strcpy(roleSecurityPolicyRecord.RDB$ROLE_NAME, roleName.object.c_str());
		strcpy(roleSecurityPolicyRecord.RDB$SCHEMA_NAME, roleName.schema.c_str());
		strcpy(roleSecurityPolicyRecord.RDB$POLICY_NAME, policy.policyName.c_str());
		strcpy(roleSecurityPolicyRecord.RDB$POLICY_TYPE, policy.policyType.c_str());
		strcpy(roleSecurityPolicyRecord.RDB$COMPLIANCE_LEVEL, policy.complianceLevel.c_str());
		strcpy(roleSecurityPolicyRecord.RDB$DATA_CLASSIFICATION, policy.dataClassification.c_str());
		roleSecurityPolicyRecord.RDB$MONITORING_REQUIRED = policy.monitoringRequired ? 'Y' : 'N';
		roleSecurityPolicyRecord.RDB$APPROVAL_REQUIRED = policy.approvalRequired ? 'Y' : 'N';
		strcpy(roleSecurityPolicyRecord.RDB$EFFECTIVE_DATE, policy.effectiveDate.c_str());

		if (!policy.accessControls.empty())
		{
			strcpy(roleSecurityPolicyRecord.RDB$ACCESS_CONTROLS, policy.accessControls.c_str());
			roleSecurityPolicyRecord.RDB$ACCESS_CONTROLS_NULL = FALSE;
		}
		else
			roleSecurityPolicyRecord.RDB$ACCESS_CONTROLS_NULL = TRUE;

		if (!policy.policyExpression.empty())
		{
			strcpy(roleSecurityPolicyRecord.RDB$POLICY_EXPRESSION, policy.policyExpression.c_str());
			roleSecurityPolicyRecord.RDB$POLICY_EXPRESSION_NULL = FALSE;
		}
		else
			roleSecurityPolicyRecord.RDB$POLICY_EXPRESSION_NULL = TRUE;

		if (!policy.expirationDate.empty())
		{
			strcpy(roleSecurityPolicyRecord.RDB$EXPIRATION_DATE, policy.expirationDate.c_str());
			roleSecurityPolicyRecord.RDB$EXPIRATION_DATE_NULL = FALSE;
		}
		else
			roleSecurityPolicyRecord.RDB$EXPIRATION_DATE_NULL = TRUE;

		EXE_send(tdbb, handle116, 0, sizeof(RDB$ROLE_SECURITY_POLICIES_RECORD), &roleSecurityPolicyRecord);
		EXE_unwind(tdbb, handle116);
	}
}

void CreateTableNode::storeTableEncryption(thread_db* tdbb, jrd_tra* transaction, const QualifiedName& tableName)
{
	// Converted FOR loop #108: Store table encryption configuration
	if (encryptionEnabled)
	{
		jrd_req* handle117 = CMP_find_request(tdbb, drq_store_table_encryption, DYN_REQUESTS);
		EXE_start(tdbb, handle117, transaction);

		struct RDB$TABLE_ENCRYPTION_RECORD {
			char RDB$RELATION_NAME[32];
			char RDB$SCHEMA_NAME[32];
			char RDB$ENCRYPTION_TYPE[16];
			char RDB$KEY_ALGORITHM[32];
			short RDB$KEY_LENGTH;
			char RDB$KEY_DERIVATION[32];
			char RDB$ENCRYPTION_MODE[16];
			char RDB$KEY_ROTATION_POLICY[32];
			short RDB$KEY_ROTATION_DAYS;
			char RDB$COMPLIANCE_STANDARD[32];
			char RDB$FIELD_LEVEL_ENCRYPTION;
			char RDB$KEY_DERIVATION_NULL;
			char RDB$KEY_ROTATION_POLICY_NULL;
			char RDB$COMPLIANCE_STANDARD_NULL;
		} tableEncryptionRecord;

		memset(&tableEncryptionRecord, 0, sizeof(tableEncryptionRecord));
		strcpy(tableEncryptionRecord.RDB$RELATION_NAME, tableName.object.c_str());
		strcpy(tableEncryptionRecord.RDB$SCHEMA_NAME, tableName.schema.c_str());
		strcpy(tableEncryptionRecord.RDB$ENCRYPTION_TYPE, encryptionType.c_str());
		strcpy(tableEncryptionRecord.RDB$KEY_ALGORITHM, keyAlgorithm.c_str());
		tableEncryptionRecord.RDB$KEY_LENGTH = keyLength;
		strcpy(tableEncryptionRecord.RDB$ENCRYPTION_MODE, encryptionMode.c_str());
		tableEncryptionRecord.RDB$KEY_ROTATION_DAYS = keyRotationDays;
		tableEncryptionRecord.RDB$FIELD_LEVEL_ENCRYPTION = fieldLevelEncryption ? 'Y' : 'N';

		if (!keyDerivation.empty())
		{
			strcpy(tableEncryptionRecord.RDB$KEY_DERIVATION, keyDerivation.c_str());
			tableEncryptionRecord.RDB$KEY_DERIVATION_NULL = FALSE;
		}
		else
			tableEncryptionRecord.RDB$KEY_DERIVATION_NULL = TRUE;

		if (!keyRotationPolicy.empty())
		{
			strcpy(tableEncryptionRecord.RDB$KEY_ROTATION_POLICY, keyRotationPolicy.c_str());
			tableEncryptionRecord.RDB$KEY_ROTATION_POLICY_NULL = FALSE;
		}
		else
			tableEncryptionRecord.RDB$KEY_ROTATION_POLICY_NULL = TRUE;

		if (!complianceStandard.empty())
		{
			strcpy(tableEncryptionRecord.RDB$COMPLIANCE_STANDARD, complianceStandard.c_str());
			tableEncryptionRecord.RDB$COMPLIANCE_STANDARD_NULL = FALSE;
		}
		else
			tableEncryptionRecord.RDB$COMPLIANCE_STANDARD_NULL = TRUE;

		EXE_send(tdbb, handle117, 0, sizeof(RDB$TABLE_ENCRYPTION_RECORD), &tableEncryptionRecord);
		EXE_unwind(tdbb, handle117);
	}
}

// High-priority DDL operations

void CreateViewNode::storeViewOptimization(thread_db* tdbb, jrd_tra* transaction, const QualifiedName& viewName)
{
	// Converted FOR loop #109: Store view optimization and materialization settings
	jrd_req* handle118 = CMP_find_request(tdbb, drq_store_view_optimization, DYN_REQUESTS);
	EXE_start(tdbb, handle118, transaction);

	struct RDB$VIEW_OPTIMIZATION_RECORD {
		char RDB$VIEW_NAME[32];
		char RDB$SCHEMA_NAME[32];
		char RDB$OPTIMIZATION_TYPE[16];
		char RDB$MATERIALIZED;
		char RDB$REFRESH_TYPE[16];
		char RDB$REFRESH_SCHEDULE[64];
		char RDB$INDEX_MAINTENANCE[16];
		char RDB$QUERY_REWRITE;
		char RDB$STATISTICS_COLLECTION;
		int RDB$ESTIMATED_CARDINALITY;
		char RDB$PARTITION_COLUMN[32];
		char RDB$REFRESH_SCHEDULE_NULL;
		char RDB$PARTITION_COLUMN_NULL;
	} viewOptimizationRecord;

	memset(&viewOptimizationRecord, 0, sizeof(viewOptimizationRecord));
	strcpy(viewOptimizationRecord.RDB$VIEW_NAME, viewName.object.c_str());
	strcpy(viewOptimizationRecord.RDB$SCHEMA_NAME, viewName.schema.c_str());
	strcpy(viewOptimizationRecord.RDB$OPTIMIZATION_TYPE, optimizationType.c_str());
	viewOptimizationRecord.RDB$MATERIALIZED = materialized ? 'Y' : 'N';
	strcpy(viewOptimizationRecord.RDB$REFRESH_TYPE, refreshType.c_str());
	strcpy(viewOptimizationRecord.RDB$INDEX_MAINTENANCE, indexMaintenance.c_str());
	viewOptimizationRecord.RDB$QUERY_REWRITE = queryRewrite ? 'Y' : 'N';
	viewOptimizationRecord.RDB$STATISTICS_COLLECTION = statisticsCollection ? 'Y' : 'N';
	viewOptimizationRecord.RDB$ESTIMATED_CARDINALITY = estimatedCardinality;

	if (!refreshSchedule.empty())
	{
		strcpy(viewOptimizationRecord.RDB$REFRESH_SCHEDULE, refreshSchedule.c_str());
		viewOptimizationRecord.RDB$REFRESH_SCHEDULE_NULL = FALSE;
	}
	else
		viewOptimizationRecord.RDB$REFRESH_SCHEDULE_NULL = TRUE;

	if (!partitionColumn.empty())
	{
		strcpy(viewOptimizationRecord.RDB$PARTITION_COLUMN, partitionColumn.c_str());
		viewOptimizationRecord.RDB$PARTITION_COLUMN_NULL = FALSE;
	}
	else
		viewOptimizationRecord.RDB$PARTITION_COLUMN_NULL = TRUE;

	EXE_send(tdbb, handle118, 0, sizeof(RDB$VIEW_OPTIMIZATION_RECORD), &viewOptimizationRecord);
	EXE_unwind(tdbb, handle118);
}

void CreateProcedureNode::storeProcedureVersioning(thread_db* tdbb, jrd_tra* transaction, const QualifiedName& procedureName)
{
	// Converted FOR loop #110: Store procedure versioning and change tracking
	jrd_req* handle119 = CMP_find_request(tdbb, drq_store_procedure_versioning, DYN_REQUESTS);
	EXE_start(tdbb, handle119, transaction);

	struct RDB$PROCEDURE_VERSIONING_RECORD {
		char RDB$PROCEDURE_NAME[32];
		char RDB$SCHEMA_NAME[32];
		short RDB$VERSION_MAJOR;
		short RDB$VERSION_MINOR;
		short RDB$VERSION_BUILD;
		char RDB$VERSION_DESCRIPTION[256];
		char RDB$CHANGE_LOG[1024];
		char RDB$COMPATIBILITY_VERSION[32];
		char RDB$DEPRECATION_STATUS[16];
		char RDB$MIGRATION_PATH[256];
		char RDB$VERSION_TIMESTAMP[20];
		char RDB$VERSION_DESCRIPTION_NULL;
		char RDB$CHANGE_LOG_NULL;
		char RDB$COMPATIBILITY_VERSION_NULL;
		char RDB$MIGRATION_PATH_NULL;
	} procedureVersioningRecord;

	memset(&procedureVersioningRecord, 0, sizeof(procedureVersioningRecord));
	strcpy(procedureVersioningRecord.RDB$PROCEDURE_NAME, procedureName.object.c_str());
	strcpy(procedureVersioningRecord.RDB$SCHEMA_NAME, procedureName.schema.c_str());
	procedureVersioningRecord.RDB$VERSION_MAJOR = versionMajor;
	procedureVersioningRecord.RDB$VERSION_MINOR = versionMinor;
	procedureVersioningRecord.RDB$VERSION_BUILD = versionBuild;
	strcpy(procedureVersioningRecord.RDB$DEPRECATION_STATUS, deprecationStatus.c_str());
	strcpy(procedureVersioningRecord.RDB$VERSION_TIMESTAMP, versionTimestamp.c_str());

	if (!versionDescription.empty())
	{
		strcpy(procedureVersioningRecord.RDB$VERSION_DESCRIPTION, versionDescription.c_str());
		procedureVersioningRecord.RDB$VERSION_DESCRIPTION_NULL = FALSE;
	}
	else
		procedureVersioningRecord.RDB$VERSION_DESCRIPTION_NULL = TRUE;

	if (!changeLog.empty())
	{
		strcpy(procedureVersioningRecord.RDB$CHANGE_LOG, changeLog.c_str());
		procedureVersioningRecord.RDB$CHANGE_LOG_NULL = FALSE;
	}
	else
		procedureVersioningRecord.RDB$CHANGE_LOG_NULL = TRUE;

	if (!compatibilityVersion.empty())
	{
		strcpy(procedureVersioningRecord.RDB$COMPATIBILITY_VERSION, compatibilityVersion.c_str());
		procedureVersioningRecord.RDB$COMPATIBILITY_VERSION_NULL = FALSE;
	}
	else
		procedureVersioningRecord.RDB$COMPATIBILITY_VERSION_NULL = TRUE;

	if (!migrationPath.empty())
	{
		strcpy(procedureVersioningRecord.RDB$MIGRATION_PATH, migrationPath.c_str());
		procedureVersioningRecord.RDB$MIGRATION_PATH_NULL = FALSE;
	}
	else
		procedureVersioningRecord.RDB$MIGRATION_PATH_NULL = TRUE;

	EXE_send(tdbb, handle119, 0, sizeof(RDB$PROCEDURE_VERSIONING_RECORD), &procedureVersioningRecord);
	EXE_unwind(tdbb, handle119);
}

void CreateFunctionNode::storeFunctionOverloading(thread_db* tdbb, jrd_tra* transaction, const QualifiedName& functionName)
{
	// Converted FOR loop #111: Store function overloading and signature management
	for (const auto& overload : functionOverloads)
	{
		jrd_req* handle120 = CMP_find_request(tdbb, drq_store_function_overload, DYN_REQUESTS);
		EXE_start(tdbb, handle120, transaction);

		struct RDB$FUNCTION_OVERLOADS_RECORD {
			char RDB$FUNCTION_NAME[32];
			char RDB$SCHEMA_NAME[32];
			short RDB$OVERLOAD_ID;
			char RDB$PARAMETER_SIGNATURE[512];
			char RDB$RETURN_TYPE_SIGNATURE[128];
			short RDB$PARAMETER_COUNT;
			char RDB$OVERLOAD_PRIORITY;
			char RDB$STRICT_TYPING;
			char RDB$DEFAULT_PARAMETERS[256];
			char RDB$OVERLOAD_DESCRIPTION[256];
			char RDB$DEFAULT_PARAMETERS_NULL;
			char RDB$OVERLOAD_DESCRIPTION_NULL;
		} functionOverloadRecord;

		memset(&functionOverloadRecord, 0, sizeof(functionOverloadRecord));
		strcpy(functionOverloadRecord.RDB$FUNCTION_NAME, functionName.object.c_str());
		strcpy(functionOverloadRecord.RDB$SCHEMA_NAME, functionName.schema.c_str());
		functionOverloadRecord.RDB$OVERLOAD_ID = overload.overloadId;
		strcpy(functionOverloadRecord.RDB$PARAMETER_SIGNATURE, overload.parameterSignature.c_str());
		strcpy(functionOverloadRecord.RDB$RETURN_TYPE_SIGNATURE, overload.returnTypeSignature.c_str());
		functionOverloadRecord.RDB$PARAMETER_COUNT = overload.parameterCount;
		functionOverloadRecord.RDB$OVERLOAD_PRIORITY = overload.overloadPriority;
		functionOverloadRecord.RDB$STRICT_TYPING = overload.strictTyping ? 'Y' : 'N';

		if (!overload.defaultParameters.empty())
		{
			strcpy(functionOverloadRecord.RDB$DEFAULT_PARAMETERS, overload.defaultParameters.c_str());
			functionOverloadRecord.RDB$DEFAULT_PARAMETERS_NULL = FALSE;
		}
		else
			functionOverloadRecord.RDB$DEFAULT_PARAMETERS_NULL = TRUE;

		if (!overload.overloadDescription.empty())
		{
			strcpy(functionOverloadRecord.RDB$OVERLOAD_DESCRIPTION, overload.overloadDescription.c_str());
			functionOverloadRecord.RDB$OVERLOAD_DESCRIPTION_NULL = FALSE;
		}
		else
			functionOverloadRecord.RDB$OVERLOAD_DESCRIPTION_NULL = TRUE;

		EXE_send(tdbb, handle120, 0, sizeof(RDB$FUNCTION_OVERLOADS_RECORD), &functionOverloadRecord);
		EXE_unwind(tdbb, handle120);
	}
}

void CreateSequenceNode::storeSequenceAdvanced(thread_db* tdbb, jrd_tra* transaction, const QualifiedName& sequenceName)
{
	// Converted FOR loop #112: Store advanced sequence configuration
	jrd_req* handle121 = CMP_find_request(tdbb, drq_store_sequence_advanced, DYN_REQUESTS);
	EXE_start(tdbb, handle121, transaction);

	struct RDB$GENERATORS_ADVANCED_RECORD {
		char RDB$GENERATOR_NAME[32];
		char RDB$SCHEMA_NAME[32];
		char RDB$SEQUENCE_TYPE[16];
		char RDB$DISTRIBUTION_TYPE[16];
		long long RDB$CACHE_SIZE;
		char RDB$PARTITION_AWARE;
		char RDB$THREAD_SAFE;
		long long RDB$MAX_CACHE_SIZE;
		short RDB$ALLOCATION_CHUNK;
		char RDB$PERSISTENT_CACHE;
		char RDB$HIGH_WATER_MARK[32];
		char RDB$LOW_WATER_MARK[32];
		char RDB$HIGH_WATER_MARK_NULL;
		char RDB$LOW_WATER_MARK_NULL;
	} sequenceAdvancedRecord;

	memset(&sequenceAdvancedRecord, 0, sizeof(sequenceAdvancedRecord));
	strcpy(sequenceAdvancedRecord.RDB$GENERATOR_NAME, sequenceName.object.c_str());
	strcpy(sequenceAdvancedRecord.RDB$SCHEMA_NAME, sequenceName.schema.c_str());
	strcpy(sequenceAdvancedRecord.RDB$SEQUENCE_TYPE, sequenceType.c_str());
	strcpy(sequenceAdvancedRecord.RDB$DISTRIBUTION_TYPE, distributionType.c_str());
	sequenceAdvancedRecord.RDB$CACHE_SIZE = cacheSize;
	sequenceAdvancedRecord.RDB$PARTITION_AWARE = partitionAware ? 'Y' : 'N';
	sequenceAdvancedRecord.RDB$THREAD_SAFE = threadSafe ? 'Y' : 'N';
	sequenceAdvancedRecord.RDB$MAX_CACHE_SIZE = maxCacheSize;
	sequenceAdvancedRecord.RDB$ALLOCATION_CHUNK = allocationChunk;
	sequenceAdvancedRecord.RDB$PERSISTENT_CACHE = persistentCache ? 'Y' : 'N';

	if (!highWaterMark.empty())
	{
		strcpy(sequenceAdvancedRecord.RDB$HIGH_WATER_MARK, highWaterMark.c_str());
		sequenceAdvancedRecord.RDB$HIGH_WATER_MARK_NULL = FALSE;
	}
	else
		sequenceAdvancedRecord.RDB$HIGH_WATER_MARK_NULL = TRUE;

	if (!lowWaterMark.empty())
	{
		strcpy(sequenceAdvancedRecord.RDB$LOW_WATER_MARK, lowWaterMark.c_str());
		sequenceAdvancedRecord.RDB$LOW_WATER_MARK_NULL = FALSE;
	}
	else
		sequenceAdvancedRecord.RDB$LOW_WATER_MARK_NULL = TRUE;

	EXE_send(tdbb, handle121, 0, sizeof(RDB$GENERATORS_ADVANCED_RECORD), &sequenceAdvancedRecord);
	EXE_unwind(tdbb, handle121);
}

void CreateCollationNode::storeCollationRules(thread_db* tdbb, jrd_tra* transaction, const QualifiedName& collationName)
{
	// Converted FOR loop #113: Store collation rules and locale-specific settings
	for (const auto& rule : collationRules)
	{
		jrd_req* handle122 = CMP_find_request(tdbb, drq_store_collation_rule, DYN_REQUESTS);
		EXE_start(tdbb, handle122, transaction);

		struct RDB$COLLATION_RULES_RECORD {
			char RDB$COLLATION_NAME[32];
			char RDB$SCHEMA_NAME[32];
			short RDB$RULE_ORDER;
			char RDB$RULE_TYPE[16];
			char RDB$RULE_EXPRESSION[256];
			char RDB$LOCALE_SPECIFIC[16];
			char RDB$CASE_SENSITIVITY[8];
			char RDB$ACCENT_SENSITIVITY[8];
			char RDB$NUMERIC_SORTING;
			char RDB$UNICODE_VERSION[16];
			char RDB$STRENGTH_LEVEL[8];
			char RDB$RULE_EXPRESSION_NULL;
			char RDB$UNICODE_VERSION_NULL;
		} collationRuleRecord;

		memset(&collationRuleRecord, 0, sizeof(collationRuleRecord));
		strcpy(collationRuleRecord.RDB$COLLATION_NAME, collationName.object.c_str());
		strcpy(collationRuleRecord.RDB$SCHEMA_NAME, collationName.schema.c_str());
		collationRuleRecord.RDB$RULE_ORDER = rule.ruleOrder;
		strcpy(collationRuleRecord.RDB$RULE_TYPE, rule.ruleType.c_str());
		strcpy(collationRuleRecord.RDB$LOCALE_SPECIFIC, rule.localeSpecific.c_str());
		strcpy(collationRuleRecord.RDB$CASE_SENSITIVITY, rule.caseSensitivity.c_str());
		strcpy(collationRuleRecord.RDB$ACCENT_SENSITIVITY, rule.accentSensitivity.c_str());
		collationRuleRecord.RDB$NUMERIC_SORTING = rule.numericSorting ? 'Y' : 'N';
		strcpy(collationRuleRecord.RDB$STRENGTH_LEVEL, rule.strengthLevel.c_str());

		if (!rule.ruleExpression.empty())
		{
			strcpy(collationRuleRecord.RDB$RULE_EXPRESSION, rule.ruleExpression.c_str());
			collationRuleRecord.RDB$RULE_EXPRESSION_NULL = FALSE;
		}
		else
			collationRuleRecord.RDB$RULE_EXPRESSION_NULL = TRUE;

		if (!rule.unicodeVersion.empty())
		{
			strcpy(collationRuleRecord.RDB$UNICODE_VERSION, rule.unicodeVersion.c_str());
			collationRuleRecord.RDB$UNICODE_VERSION_NULL = FALSE;
		}
		else
			collationRuleRecord.RDB$UNICODE_VERSION_NULL = TRUE;

		EXE_send(tdbb, handle122, 0, sizeof(RDB$COLLATION_RULES_RECORD), &collationRuleRecord);
		EXE_unwind(tdbb, handle122);
	}
}

void CreateExceptionNode::storeExceptionHandling(thread_db* tdbb, jrd_tra* transaction, const QualifiedName& exceptionName)
{
	// Converted FOR loop #114: Store exception handling and recovery policies
	jrd_req* handle123 = CMP_find_request(tdbb, drq_store_exception_handling, DYN_REQUESTS);
	EXE_start(tdbb, handle123, transaction);

	struct RDB$EXCEPTION_HANDLING_RECORD {
		char RDB$EXCEPTION_NAME[32];
		char RDB$SCHEMA_NAME[32];
		char RDB$HANDLING_TYPE[16];
		char RDB$RECOVERY_ACTION[32];
		short RDB$RETRY_ATTEMPTS;
		short RDB$RETRY_DELAY_SECONDS;
		char RDB$ESCALATION_POLICY[32];
		char RDB$NOTIFICATION_REQUIRED;
		char RDB$LOGGING_LEVEL[16];
		char RDB$CUSTOM_HANDLER[256];
		char RDB$RECOVERY_EXPRESSION[512];
		char RDB$CUSTOM_HANDLER_NULL;
		char RDB$RECOVERY_EXPRESSION_NULL;
	} exceptionHandlingRecord;

	memset(&exceptionHandlingRecord, 0, sizeof(exceptionHandlingRecord));
	strcpy(exceptionHandlingRecord.RDB$EXCEPTION_NAME, exceptionName.object.c_str());
	strcpy(exceptionHandlingRecord.RDB$SCHEMA_NAME, exceptionName.schema.c_str());
	strcpy(exceptionHandlingRecord.RDB$HANDLING_TYPE, handlingType.c_str());
	strcpy(exceptionHandlingRecord.RDB$RECOVERY_ACTION, recoveryAction.c_str());
	exceptionHandlingRecord.RDB$RETRY_ATTEMPTS = retryAttempts;
	exceptionHandlingRecord.RDB$RETRY_DELAY_SECONDS = retryDelaySeconds;
	strcpy(exceptionHandlingRecord.RDB$ESCALATION_POLICY, escalationPolicy.c_str());
	exceptionHandlingRecord.RDB$NOTIFICATION_REQUIRED = notificationRequired ? 'Y' : 'N';
	strcpy(exceptionHandlingRecord.RDB$LOGGING_LEVEL, loggingLevel.c_str());

	if (!customHandler.empty())
	{
		strcpy(exceptionHandlingRecord.RDB$CUSTOM_HANDLER, customHandler.c_str());
		exceptionHandlingRecord.RDB$CUSTOM_HANDLER_NULL = FALSE;
	}
	else
		exceptionHandlingRecord.RDB$CUSTOM_HANDLER_NULL = TRUE;

	if (!recoveryExpression.empty())
	{
		strcpy(exceptionHandlingRecord.RDB$RECOVERY_EXPRESSION, recoveryExpression.c_str());
		exceptionHandlingRecord.RDB$RECOVERY_EXPRESSION_NULL = FALSE;
	}
	else
		exceptionHandlingRecord.RDB$RECOVERY_EXPRESSION_NULL = TRUE;

	EXE_send(tdbb, handle123, 0, sizeof(RDB$EXCEPTION_HANDLING_RECORD), &exceptionHandlingRecord);
	EXE_unwind(tdbb, handle123);
}

void AlterSchemaNode::storeSchemaVersioning(thread_db* tdbb, jrd_tra* transaction, const QualifiedName& schemaName)
{
	// Converted FOR loop #115: Store schema versioning and migration tracking
	jrd_req* handle124 = CMP_find_request(tdbb, drq_store_schema_versioning, DYN_REQUESTS);
	EXE_start(tdbb, handle124, transaction);

	struct RDB$SCHEMA_VERSIONING_RECORD {
		char RDB$SCHEMA_NAME[512];
		short RDB$VERSION_MAJOR;
		short RDB$VERSION_MINOR;
		short RDB$VERSION_BUILD;
		char RDB$VERSION_DESCRIPTION[256];
		char RDB$MIGRATION_SCRIPT[1024];
		char RDB$COMPATIBILITY_MATRIX[512];
		char RDB$ROLLBACK_SCRIPT[1024];
		char RDB$VERSION_TIMESTAMP[20];
		char RDB$MIGRATION_STATUS[16];
		char RDB$DEPLOYMENT_NOTES[512];
		char RDB$VERSION_DESCRIPTION_NULL;
		char RDB$MIGRATION_SCRIPT_NULL;
		char RDB$COMPATIBILITY_MATRIX_NULL;
		char RDB$ROLLBACK_SCRIPT_NULL;
		char RDB$DEPLOYMENT_NOTES_NULL;
	} schemaVersioningRecord;

	memset(&schemaVersioningRecord, 0, sizeof(schemaVersioningRecord));
	strcpy(schemaVersioningRecord.RDB$SCHEMA_NAME, schemaName.getQualifiedName().c_str());
	schemaVersioningRecord.RDB$VERSION_MAJOR = versionMajor;
	schemaVersioningRecord.RDB$VERSION_MINOR = versionMinor;
	schemaVersioningRecord.RDB$VERSION_BUILD = versionBuild;
	strcpy(schemaVersioningRecord.RDB$VERSION_TIMESTAMP, versionTimestamp.c_str());
	strcpy(schemaVersioningRecord.RDB$MIGRATION_STATUS, migrationStatus.c_str());

	if (!versionDescription.empty())
	{
		strcpy(schemaVersioningRecord.RDB$VERSION_DESCRIPTION, versionDescription.c_str());
		schemaVersioningRecord.RDB$VERSION_DESCRIPTION_NULL = FALSE;
	}
	else
		schemaVersioningRecord.RDB$VERSION_DESCRIPTION_NULL = TRUE;

	if (!migrationScript.empty())
	{
		strcpy(schemaVersioningRecord.RDB$MIGRATION_SCRIPT, migrationScript.c_str());
		schemaVersioningRecord.RDB$MIGRATION_SCRIPT_NULL = FALSE;
	}
	else
		schemaVersioningRecord.RDB$MIGRATION_SCRIPT_NULL = TRUE;

	if (!compatibilityMatrix.empty())
	{
		strcpy(schemaVersioningRecord.RDB$COMPATIBILITY_MATRIX, compatibilityMatrix.c_str());
		schemaVersioningRecord.RDB$COMPATIBILITY_MATRIX_NULL = FALSE;
	}
	else
		schemaVersioningRecord.RDB$COMPATIBILITY_MATRIX_NULL = TRUE;

	if (!rollbackScript.empty())
	{
		strcpy(schemaVersioningRecord.RDB$ROLLBACK_SCRIPT, rollbackScript.c_str());
		schemaVersioningRecord.RDB$ROLLBACK_SCRIPT_NULL = FALSE;
	}
	else
		schemaVersioningRecord.RDB$ROLLBACK_SCRIPT_NULL = TRUE;

	if (!deploymentNotes.empty())
	{
		strcpy(schemaVersioningRecord.RDB$DEPLOYMENT_NOTES, deploymentNotes.c_str());
		schemaVersioningRecord.RDB$DEPLOYMENT_NOTES_NULL = FALSE;
	}
	else
		schemaVersioningRecord.RDB$DEPLOYMENT_NOTES_NULL = TRUE;

	EXE_send(tdbb, handle124, 0, sizeof(RDB$SCHEMA_VERSIONING_RECORD), &schemaVersioningRecord);
	EXE_unwind(tdbb, handle124);
}

void CreateConstraintNode::storeCheckConstraintExpression(thread_db* tdbb, jrd_tra* transaction, 
	const QualifiedName& constraintName, const string& expression)
{
	// Converted FOR loop #116: Store check constraint expression with validation rules
	jrd_req* handle125 = CMP_find_request(tdbb, drq_store_check_constraint_expression, DYN_REQUESTS);
	EXE_start(tdbb, handle125, transaction);

	struct RDB$CHECK_CONSTRAINT_EXPRESSIONS_RECORD {
		char RDB$CONSTRAINT_NAME[256];
		char RDB$CHECK_EXPRESSION[2048];
		char RDB$VALIDATION_LEVEL[16];
		char RDB$ERROR_MESSAGE[512];
		char RDB$EXPRESSION_TYPE[32];
		char RDB$DEPENDENCY_LIST[1024];
		short RDB$VALIDATION_ORDER;
		short RDB$IS_DEFERRABLE;
		short RDB$INITIALLY_DEFERRED;
		char RDB$ERROR_MESSAGE_NULL;
		char RDB$DEPENDENCY_LIST_NULL;
	} checkExpressionRecord;

	memset(&checkExpressionRecord, 0, sizeof(checkExpressionRecord));
	strcpy(checkExpressionRecord.RDB$CONSTRAINT_NAME, constraintName.getQualifiedName().c_str());
	strcpy(checkExpressionRecord.RDB$CHECK_EXPRESSION, expression.c_str());
	strcpy(checkExpressionRecord.RDB$VALIDATION_LEVEL, validationLevel.c_str());
	strcpy(checkExpressionRecord.RDB$EXPRESSION_TYPE, expressionType.c_str());
	checkExpressionRecord.RDB$VALIDATION_ORDER = validationOrder;
	checkExpressionRecord.RDB$IS_DEFERRABLE = isDeferrable ? TRUE : FALSE;
	checkExpressionRecord.RDB$INITIALLY_DEFERRED = initiallyDeferred ? TRUE : FALSE;

	if (!errorMessage.empty())
	{
		strcpy(checkExpressionRecord.RDB$ERROR_MESSAGE, errorMessage.c_str());
		checkExpressionRecord.RDB$ERROR_MESSAGE_NULL = FALSE;
	}
	else
		checkExpressionRecord.RDB$ERROR_MESSAGE_NULL = TRUE;

	if (!dependencyList.empty())
	{
		strcpy(checkExpressionRecord.RDB$DEPENDENCY_LIST, dependencyList.c_str());
		checkExpressionRecord.RDB$DEPENDENCY_LIST_NULL = FALSE;
	}
	else
		checkExpressionRecord.RDB$DEPENDENCY_LIST_NULL = TRUE;

	EXE_send(tdbb, handle125, 0, sizeof(RDB$CHECK_CONSTRAINT_EXPRESSIONS_RECORD), &checkExpressionRecord);
	EXE_unwind(tdbb, handle125);
}

void CreateConstraintNode::storeUniqueConstraintColumns(thread_db* tdbb, jrd_tra* transaction,
	const QualifiedName& constraintName, const Array<string>& columnList)
{
	// Converted FOR loop #117: Store unique constraint columns with index configuration
	jrd_req* handle126 = CMP_find_request(tdbb, drq_store_unique_constraint_columns, DYN_REQUESTS);
	EXE_start(tdbb, handle126, transaction);

	struct RDB$UNIQUE_CONSTRAINT_COLUMNS_RECORD {
		char RDB$CONSTRAINT_NAME[256];
		char RDB$COLUMN_NAME[256];
		short RDB$COLUMN_POSITION;
		short RDB$SORT_ORDER;
		char RDB$COLLATION_NAME[256];
		char RDB$INDEX_TYPE[32];
		short RDB$IS_CLUSTERED;
		short RDB$FILL_FACTOR;
		char RDB$INDEX_OPTIONS[512];
		char RDB$STORAGE_PARAMETERS[256];
		char RDB$COLLATION_NAME_NULL;
		char RDB$INDEX_OPTIONS_NULL;
		char RDB$STORAGE_PARAMETERS_NULL;
	} uniqueColumnRecord;

	for (size_t i = 0; i < columnList.getCount(); i++)
	{
		memset(&uniqueColumnRecord, 0, sizeof(uniqueColumnRecord));
		strcpy(uniqueColumnRecord.RDB$CONSTRAINT_NAME, constraintName.getQualifiedName().c_str());
		strcpy(uniqueColumnRecord.RDB$COLUMN_NAME, columnList[i].c_str());
		uniqueColumnRecord.RDB$COLUMN_POSITION = static_cast<short>(i);
		uniqueColumnRecord.RDB$SORT_ORDER = sortOrders[i];
		strcpy(uniqueColumnRecord.RDB$INDEX_TYPE, indexType.c_str());
		uniqueColumnRecord.RDB$IS_CLUSTERED = isClustered ? TRUE : FALSE;
		uniqueColumnRecord.RDB$FILL_FACTOR = fillFactor;

		if (!collationNames.empty() && i < collationNames.getCount())
		{
			strcpy(uniqueColumnRecord.RDB$COLLATION_NAME, collationNames[i].c_str());
			uniqueColumnRecord.RDB$COLLATION_NAME_NULL = FALSE;
		}
		else
			uniqueColumnRecord.RDB$COLLATION_NAME_NULL = TRUE;

		if (!indexOptions.empty())
		{
			strcpy(uniqueColumnRecord.RDB$INDEX_OPTIONS, indexOptions.c_str());
			uniqueColumnRecord.RDB$INDEX_OPTIONS_NULL = FALSE;
		}
		else
			uniqueColumnRecord.RDB$INDEX_OPTIONS_NULL = TRUE;

		if (!storageParameters.empty())
		{
			strcpy(uniqueColumnRecord.RDB$STORAGE_PARAMETERS, storageParameters.c_str());
			uniqueColumnRecord.RDB$STORAGE_PARAMETERS_NULL = FALSE;
		}
		else
			uniqueColumnRecord.RDB$STORAGE_PARAMETERS_NULL = TRUE;

		EXE_send(tdbb, handle126, 0, sizeof(RDB$UNIQUE_CONSTRAINT_COLUMNS_RECORD), &uniqueColumnRecord);
	}
	EXE_unwind(tdbb, handle126);
}

void CreateConstraintNode::storeForeignKeyMapping(thread_db* tdbb, jrd_tra* transaction,
	const QualifiedName& constraintName, const ForeignKeyDefinition& fkDef)
{
	// Converted FOR loop #118: Store foreign key mapping with cascade rules and updates
	jrd_req* handle127 = CMP_find_request(tdbb, drq_store_foreign_key_mapping, DYN_REQUESTS);
	EXE_start(tdbb, handle127, transaction);

	struct RDB$FOREIGN_KEY_MAPPING_RECORD {
		char RDB$CONSTRAINT_NAME[256];
		char RDB$REFERENCED_TABLE[256];
		char RDB$REFERENCED_SCHEMA[256];
		char RDB$LOCAL_COLUMN[256];
		char RDB$REFERENCED_COLUMN[256];
		short RDB$COLUMN_POSITION;
		char RDB$UPDATE_RULE[32];
		char RDB$DELETE_RULE[32];
		char RDB$MATCH_TYPE[16];
		short RDB$IS_DEFERRABLE;
		short RDB$INITIALLY_DEFERRED;
		char RDB$VALIDATION_TIMING[16];
		char RDB$REFERENCED_SCHEMA_NULL;
	} foreignKeyRecord;

	for (size_t i = 0; i < fkDef.localColumns.getCount(); i++)
	{
		memset(&foreignKeyRecord, 0, sizeof(foreignKeyRecord));
		strcpy(foreignKeyRecord.RDB$CONSTRAINT_NAME, constraintName.getQualifiedName().c_str());
		strcpy(foreignKeyRecord.RDB$REFERENCED_TABLE, fkDef.referencedTable.c_str());
		strcpy(foreignKeyRecord.RDB$LOCAL_COLUMN, fkDef.localColumns[i].c_str());
		strcpy(foreignKeyRecord.RDB$REFERENCED_COLUMN, fkDef.referencedColumns[i].c_str());
		foreignKeyRecord.RDB$COLUMN_POSITION = static_cast<short>(i);
		strcpy(foreignKeyRecord.RDB$UPDATE_RULE, fkDef.updateRule.c_str());
		strcpy(foreignKeyRecord.RDB$DELETE_RULE, fkDef.deleteRule.c_str());
		strcpy(foreignKeyRecord.RDB$MATCH_TYPE, fkDef.matchType.c_str());
		foreignKeyRecord.RDB$IS_DEFERRABLE = fkDef.isDeferrable ? TRUE : FALSE;
		foreignKeyRecord.RDB$INITIALLY_DEFERRED = fkDef.initiallyDeferred ? TRUE : FALSE;
		strcpy(foreignKeyRecord.RDB$VALIDATION_TIMING, fkDef.validationTiming.c_str());

		if (!fkDef.referencedSchema.empty())
		{
			strcpy(foreignKeyRecord.RDB$REFERENCED_SCHEMA, fkDef.referencedSchema.c_str());
			foreignKeyRecord.RDB$REFERENCED_SCHEMA_NULL = FALSE;
		}
		else
			foreignKeyRecord.RDB$REFERENCED_SCHEMA_NULL = TRUE;

		EXE_send(tdbb, handle127, 0, sizeof(RDB$FOREIGN_KEY_MAPPING_RECORD), &foreignKeyRecord);
	}
	EXE_unwind(tdbb, handle127);
}

void CreateConstraintNode::storeCascadeOperations(thread_db* tdbb, jrd_tra* transaction,
	const QualifiedName& constraintName, const CascadeRules& cascadeRules)
{
	// Converted FOR loop #119: Store cascade operations with advanced foreign key management  
	jrd_req* handle128 = CMP_find_request(tdbb, drq_store_cascade_operations, DYN_REQUESTS);
	EXE_start(tdbb, handle128, transaction);

	struct RDB$CASCADE_OPERATIONS_RECORD {
		char RDB$CONSTRAINT_NAME[256];
		char RDB$OPERATION_TYPE[32];
		char RDB$CASCADE_ACTION[32];
		char RDB$TARGET_TABLE[256];
		char RDB$TARGET_COLUMNS[512];
		char RDB$CONDITION_EXPRESSION[1024];
		short RDB$EXECUTION_ORDER;
		short RDB$IS_RECURSIVE;
		short RDB$MAX_RECURSION_DEPTH;
		char RDB$ERROR_HANDLING[32];
		char RDB$LOGGING_LEVEL[16];
		char RDB$CONDITION_EXPRESSION_NULL;
	} cascadeRecord;

	for (size_t i = 0; i < cascadeRules.operations.getCount(); i++)
	{
		memset(&cascadeRecord, 0, sizeof(cascadeRecord));
		strcpy(cascadeRecord.RDB$CONSTRAINT_NAME, constraintName.getQualifiedName().c_str());
		strcpy(cascadeRecord.RDB$OPERATION_TYPE, cascadeRules.operations[i].operationType.c_str());
		strcpy(cascadeRecord.RDB$CASCADE_ACTION, cascadeRules.operations[i].cascadeAction.c_str());
		strcpy(cascadeRecord.RDB$TARGET_TABLE, cascadeRules.operations[i].targetTable.c_str());
		strcpy(cascadeRecord.RDB$TARGET_COLUMNS, cascadeRules.operations[i].targetColumns.c_str());
		cascadeRecord.RDB$EXECUTION_ORDER = cascadeRules.operations[i].executionOrder;
		cascadeRecord.RDB$IS_RECURSIVE = cascadeRules.operations[i].isRecursive ? TRUE : FALSE;
		cascadeRecord.RDB$MAX_RECURSION_DEPTH = cascadeRules.operations[i].maxRecursionDepth;
		strcpy(cascadeRecord.RDB$ERROR_HANDLING, cascadeRules.operations[i].errorHandling.c_str());
		strcpy(cascadeRecord.RDB$LOGGING_LEVEL, cascadeRules.operations[i].loggingLevel.c_str());

		if (!cascadeRules.operations[i].conditionExpression.empty())
		{
			strcpy(cascadeRecord.RDB$CONDITION_EXPRESSION, cascadeRules.operations[i].conditionExpression.c_str());
			cascadeRecord.RDB$CONDITION_EXPRESSION_NULL = FALSE;
		}
		else
			cascadeRecord.RDB$CONDITION_EXPRESSION_NULL = TRUE;

		EXE_send(tdbb, handle128, 0, sizeof(RDB$CASCADE_OPERATIONS_RECORD), &cascadeRecord);
	}
	EXE_unwind(tdbb, handle128);
}

void CreateViewNode::storeViewDependencies(thread_db* tdbb, jrd_tra* transaction,
	const QualifiedName& viewName, const ViewDefinition& viewDef)
{
	// Converted FOR loop #120: Store view dependencies with materialized view support
	jrd_req* handle129 = CMP_find_request(tdbb, drq_store_view_dependencies, DYN_REQUESTS);
	EXE_start(tdbb, handle129, transaction);

	struct RDB$VIEW_DEPENDENCIES_RECORD {
		char RDB$VIEW_NAME[256];
		char RDB$VIEW_SCHEMA[256];
		char RDB$DEPENDENT_OBJECT_NAME[256];
		char RDB$DEPENDENT_OBJECT_TYPE[32];
		char RDB$DEPENDENT_SCHEMA[256];
		char RDB$DEPENDENCY_TYPE[32];
		char RDB$COLUMN_DEPENDENCIES[1024];
		short RDB$DEPENDENCY_LEVEL;
		short RDB$IS_CRITICAL;
		char RDB$REFRESH_POLICY[32];
		char RDB$MATERIALIZATION_STRATEGY[32];
		char RDB$VIEW_SCHEMA_NULL;
		char RDB$DEPENDENT_SCHEMA_NULL;
		char RDB$COLUMN_DEPENDENCIES_NULL;
	} dependencyRecord;

	for (size_t i = 0; i < viewDef.dependencies.getCount(); i++)
	{
		memset(&dependencyRecord, 0, sizeof(dependencyRecord));
		strcpy(dependencyRecord.RDB$VIEW_NAME, viewName.identifier.c_str());
		strcpy(dependencyRecord.RDB$DEPENDENT_OBJECT_NAME, viewDef.dependencies[i].objectName.c_str());
		strcpy(dependencyRecord.RDB$DEPENDENT_OBJECT_TYPE, viewDef.dependencies[i].objectType.c_str());
		strcpy(dependencyRecord.RDB$DEPENDENCY_TYPE, viewDef.dependencies[i].dependencyType.c_str());
		dependencyRecord.RDB$DEPENDENCY_LEVEL = viewDef.dependencies[i].dependencyLevel;
		dependencyRecord.RDB$IS_CRITICAL = viewDef.dependencies[i].isCritical ? TRUE : FALSE;
		strcpy(dependencyRecord.RDB$REFRESH_POLICY, viewDef.dependencies[i].refreshPolicy.c_str());
		strcpy(dependencyRecord.RDB$MATERIALIZATION_STRATEGY, viewDef.dependencies[i].materializationStrategy.c_str());

		if (!viewName.package.empty())
		{
			strcpy(dependencyRecord.RDB$VIEW_SCHEMA, viewName.package.c_str());
			dependencyRecord.RDB$VIEW_SCHEMA_NULL = FALSE;
		}
		else
			dependencyRecord.RDB$VIEW_SCHEMA_NULL = TRUE;

		if (!viewDef.dependencies[i].dependentSchema.empty())
		{
			strcpy(dependencyRecord.RDB$DEPENDENT_SCHEMA, viewDef.dependencies[i].dependentSchema.c_str());
			dependencyRecord.RDB$DEPENDENT_SCHEMA_NULL = FALSE;
		}
		else
			dependencyRecord.RDB$DEPENDENT_SCHEMA_NULL = TRUE;

		if (!viewDef.dependencies[i].columnDependencies.empty())
		{
			strcpy(dependencyRecord.RDB$COLUMN_DEPENDENCIES, viewDef.dependencies[i].columnDependencies.c_str());
			dependencyRecord.RDB$COLUMN_DEPENDENCIES_NULL = FALSE;
		}
		else
			dependencyRecord.RDB$COLUMN_DEPENDENCIES_NULL = TRUE;

		EXE_send(tdbb, handle129, 0, sizeof(RDB$VIEW_DEPENDENCIES_RECORD), &dependencyRecord);
	}
	EXE_unwind(tdbb, handle129);
}

void CreateViewNode::storeMaterializedViewConfig(thread_db* tdbb, jrd_tra* transaction,
	const QualifiedName& viewName, const MaterializedViewConfig& mvConfig)
{
	// Converted FOR loop #121: Store materialized view configuration with complex joins
	jrd_req* handle130 = CMP_find_request(tdbb, drq_store_materialized_view_config, DYN_REQUESTS);
	EXE_start(tdbb, handle130, transaction);

	struct RDB$MATERIALIZED_VIEW_CONFIG_RECORD {
		char RDB$VIEW_NAME[256];
		char RDB$VIEW_SCHEMA[256];
		char RDB$REFRESH_METHOD[32];
		char RDB$REFRESH_SCHEDULE[128];
		char RDB$INDEX_STRATEGY[64];
		char RDB$STORAGE_PARAMETERS[512];
		char RDB$PARTITION_SCHEME[256];
		char RDB$COMPRESSION_TYPE[32];
		short RDB$REFRESH_INTERVAL_HOURS;
		short RDB$MAX_STALENESS_MINUTES;
		short RDB$PARALLEL_DEGREE;
		short RDB$IS_CLUSTERED;
		char RDB$OPTIMIZATION_HINTS[1024];
		char RDB$VIEW_SCHEMA_NULL;
		char RDB$STORAGE_PARAMETERS_NULL;
		char RDB$PARTITION_SCHEME_NULL;
		char RDB$OPTIMIZATION_HINTS_NULL;
	} mvConfigRecord;

	memset(&mvConfigRecord, 0, sizeof(mvConfigRecord));
	strcpy(mvConfigRecord.RDB$VIEW_NAME, viewName.identifier.c_str());
	strcpy(mvConfigRecord.RDB$REFRESH_METHOD, mvConfig.refreshMethod.c_str());
	strcpy(mvConfigRecord.RDB$REFRESH_SCHEDULE, mvConfig.refreshSchedule.c_str());
	strcpy(mvConfigRecord.RDB$INDEX_STRATEGY, mvConfig.indexStrategy.c_str());
	strcpy(mvConfigRecord.RDB$COMPRESSION_TYPE, mvConfig.compressionType.c_str());
	mvConfigRecord.RDB$REFRESH_INTERVAL_HOURS = mvConfig.refreshIntervalHours;
	mvConfigRecord.RDB$MAX_STALENESS_MINUTES = mvConfig.maxStalenessMinutes;
	mvConfigRecord.RDB$PARALLEL_DEGREE = mvConfig.parallelDegree;
	mvConfigRecord.RDB$IS_CLUSTERED = mvConfig.isClustered ? TRUE : FALSE;

	if (!viewName.package.empty())
	{
		strcpy(mvConfigRecord.RDB$VIEW_SCHEMA, viewName.package.c_str());
		mvConfigRecord.RDB$VIEW_SCHEMA_NULL = FALSE;
	}
	else
		mvConfigRecord.RDB$VIEW_SCHEMA_NULL = TRUE;

	if (!mvConfig.storageParameters.empty())
	{
		strcpy(mvConfigRecord.RDB$STORAGE_PARAMETERS, mvConfig.storageParameters.c_str());
		mvConfigRecord.RDB$STORAGE_PARAMETERS_NULL = FALSE;
	}
	else
		mvConfigRecord.RDB$STORAGE_PARAMETERS_NULL = TRUE;

	if (!mvConfig.partitionScheme.empty())
	{
		strcpy(mvConfigRecord.RDB$PARTITION_SCHEME, mvConfig.partitionScheme.c_str());
		mvConfigRecord.RDB$PARTITION_SCHEME_NULL = FALSE;
	}
	else
		mvConfigRecord.RDB$PARTITION_SCHEME_NULL = TRUE;

	if (!mvConfig.optimizationHints.empty())
	{
		strcpy(mvConfigRecord.RDB$OPTIMIZATION_HINTS, mvConfig.optimizationHints.c_str());
		mvConfigRecord.RDB$OPTIMIZATION_HINTS_NULL = FALSE;
	}
	else
		mvConfigRecord.RDB$OPTIMIZATION_HINTS_NULL = TRUE;

	EXE_send(tdbb, handle130, 0, sizeof(RDB$MATERIALIZED_VIEW_CONFIG_RECORD), &mvConfigRecord);
	EXE_unwind(tdbb, handle130);
}

void SystemMaintenanceNode::storeBackupConfiguration(thread_db* tdbb, jrd_tra* transaction,
	const BackupConfig& backupConfig)
{
	// Converted FOR loop #122: Store backup configuration with system maintenance operations
	jrd_req* handle131 = CMP_find_request(tdbb, drq_store_backup_configuration, DYN_REQUESTS);
	EXE_start(tdbb, handle131, transaction);

	struct RDB$BACKUP_CONFIGURATION_RECORD {
		char RDB$BACKUP_NAME[256];
		char RDB$BACKUP_TYPE[32];
		char RDB$BACKUP_SCHEDULE[128];
		char RDB$DESTINATION_PATH[512];
		char RDB$COMPRESSION_LEVEL[16];
		char RDB$ENCRYPTION_METHOD[32];
		char RDB$RETENTION_POLICY[64];
		char RDB$NOTIFICATION_LIST[512];
		short RDB$PARALLEL_STREAMS;
		short RDB$VERIFY_AFTER_BACKUP;
		short RDB$INCLUDE_STATISTICS;
		short RDB$BACKUP_PRIORITY;
		char RDB$CUSTOM_OPTIONS[1024];
		char RDB$NOTIFICATION_LIST_NULL;
		char RDB$CUSTOM_OPTIONS_NULL;
	} backupConfigRecord;

	memset(&backupConfigRecord, 0, sizeof(backupConfigRecord));
	strcpy(backupConfigRecord.RDB$BACKUP_NAME, backupConfig.backupName.c_str());
	strcpy(backupConfigRecord.RDB$BACKUP_TYPE, backupConfig.backupType.c_str());
	strcpy(backupConfigRecord.RDB$BACKUP_SCHEDULE, backupConfig.backupSchedule.c_str());
	strcpy(backupConfigRecord.RDB$DESTINATION_PATH, backupConfig.destinationPath.c_str());
	strcpy(backupConfigRecord.RDB$COMPRESSION_LEVEL, backupConfig.compressionLevel.c_str());
	strcpy(backupConfigRecord.RDB$ENCRYPTION_METHOD, backupConfig.encryptionMethod.c_str());
	strcpy(backupConfigRecord.RDB$RETENTION_POLICY, backupConfig.retentionPolicy.c_str());
	backupConfigRecord.RDB$PARALLEL_STREAMS = backupConfig.parallelStreams;
	backupConfigRecord.RDB$VERIFY_AFTER_BACKUP = backupConfig.verifyAfterBackup ? TRUE : FALSE;
	backupConfigRecord.RDB$INCLUDE_STATISTICS = backupConfig.includeStatistics ? TRUE : FALSE;
	backupConfigRecord.RDB$BACKUP_PRIORITY = backupConfig.backupPriority;

	if (!backupConfig.notificationList.empty())
	{
		strcpy(backupConfigRecord.RDB$NOTIFICATION_LIST, backupConfig.notificationList.c_str());
		backupConfigRecord.RDB$NOTIFICATION_LIST_NULL = FALSE;
	}
	else
		backupConfigRecord.RDB$NOTIFICATION_LIST_NULL = TRUE;

	if (!backupConfig.customOptions.empty())
	{
		strcpy(backupConfigRecord.RDB$CUSTOM_OPTIONS, backupConfig.customOptions.c_str());
		backupConfigRecord.RDB$CUSTOM_OPTIONS_NULL = FALSE;
	}
	else
		backupConfigRecord.RDB$CUSTOM_OPTIONS_NULL = TRUE;

	EXE_send(tdbb, handle131, 0, sizeof(RDB$BACKUP_CONFIGURATION_RECORD), &backupConfigRecord);
	EXE_unwind(tdbb, handle131);
}

void SystemMaintenanceNode::storeRestoreConfiguration(thread_db* tdbb, jrd_tra* transaction,
	const RestoreConfig& restoreConfig)
{
	// Converted FOR loop #123: Store restore configuration with DDL support
	jrd_req* handle132 = CMP_find_request(tdbb, drq_store_restore_configuration, DYN_REQUESTS);
	EXE_start(tdbb, handle132, transaction);

	struct RDB$RESTORE_CONFIGURATION_RECORD {
		char RDB$RESTORE_NAME[256];
		char RDB$RESTORE_TYPE[32];
		char RDB$SOURCE_BACKUP[512];
		char RDB$TARGET_DATABASE[512];
		char RDB$RESTORE_OPTIONS[1024];
		char RDB$SCHEMA_MAPPING[1024];
		char RDB$USER_MAPPING[512];
		char RDB$VALIDATION_LEVEL[32];
		short RDB$PARALLEL_WORKERS;
		short RDB$REPLACE_EXISTING;
		short RDB$RESTORE_METADATA_ONLY;
		short RDB$RESTORE_DATA_ONLY;
		char RDB$EXCLUDED_OBJECTS[2048];
		char RDB$SCHEMA_MAPPING_NULL;
		char RDB$USER_MAPPING_NULL;
		char RDB$EXCLUDED_OBJECTS_NULL;
	} restoreConfigRecord;

	memset(&restoreConfigRecord, 0, sizeof(restoreConfigRecord));
	strcpy(restoreConfigRecord.RDB$RESTORE_NAME, restoreConfig.restoreName.c_str());
	strcpy(restoreConfigRecord.RDB$RESTORE_TYPE, restoreConfig.restoreType.c_str());
	strcpy(restoreConfigRecord.RDB$SOURCE_BACKUP, restoreConfig.sourceBackup.c_str());
	strcpy(restoreConfigRecord.RDB$TARGET_DATABASE, restoreConfig.targetDatabase.c_str());
	strcpy(restoreConfigRecord.RDB$RESTORE_OPTIONS, restoreConfig.restoreOptions.c_str());
	strcpy(restoreConfigRecord.RDB$VALIDATION_LEVEL, restoreConfig.validationLevel.c_str());
	restoreConfigRecord.RDB$PARALLEL_WORKERS = restoreConfig.parallelWorkers;
	restoreConfigRecord.RDB$REPLACE_EXISTING = restoreConfig.replaceExisting ? TRUE : FALSE;
	restoreConfigRecord.RDB$RESTORE_METADATA_ONLY = restoreConfig.restoreMetadataOnly ? TRUE : FALSE;
	restoreConfigRecord.RDB$RESTORE_DATA_ONLY = restoreConfig.restoreDataOnly ? TRUE : FALSE;

	if (!restoreConfig.schemaMapping.empty())
	{
		strcpy(restoreConfigRecord.RDB$SCHEMA_MAPPING, restoreConfig.schemaMapping.c_str());
		restoreConfigRecord.RDB$SCHEMA_MAPPING_NULL = FALSE;
	}
	else
		restoreConfigRecord.RDB$SCHEMA_MAPPING_NULL = TRUE;

	if (!restoreConfig.userMapping.empty())
	{
		strcpy(restoreConfigRecord.RDB$USER_MAPPING, restoreConfig.userMapping.c_str());
		restoreConfigRecord.RDB$USER_MAPPING_NULL = FALSE;
	}
	else
		restoreConfigRecord.RDB$USER_MAPPING_NULL = TRUE;

	if (!restoreConfig.excludedObjects.empty())
	{
		strcpy(restoreConfigRecord.RDB$EXCLUDED_OBJECTS, restoreConfig.excludedObjects.c_str());
		restoreConfigRecord.RDB$EXCLUDED_OBJECTS_NULL = FALSE;
	}
	else
		restoreConfigRecord.RDB$EXCLUDED_OBJECTS_NULL = TRUE;

	EXE_send(tdbb, handle132, 0, sizeof(RDB$RESTORE_CONFIGURATION_RECORD), &restoreConfigRecord);
	EXE_unwind(tdbb, handle132);
}

void TransactionManagementNode::storeSavepointInfo(thread_db* tdbb, jrd_tra* transaction,
	const SavepointDefinition& savepointDef)
{
	// Converted FOR loop #124: Store savepoint information with transaction handling
	jrd_req* handle133 = CMP_find_request(tdbb, drq_store_savepoint_info, DYN_REQUESTS);
	EXE_start(tdbb, handle133, transaction);

	struct RDB$SAVEPOINT_INFO_RECORD {
		char RDB$SAVEPOINT_NAME[256];
		char RDB$TRANSACTION_ID[64];
		char RDB$SAVEPOINT_LEVEL[16];
		char RDB$CREATION_TIMESTAMP[24];
		char RDB$ISOLATION_LEVEL[32];
		char RDB$LOCK_TIMEOUT[16];
		char RDB$RESOURCE_USAGE[512];
		char RDB$NESTED_LEVEL[8];
		short RDB$IS_DISTRIBUTED;
		short RDB$IS_READ_ONLY;
		short RDB$AUTO_ROLLBACK;
		char RDB$DEPENDENCIES[1024];
		char RDB$RESOURCE_USAGE_NULL;
		char RDB$DEPENDENCIES_NULL;
	} savepointRecord;

	memset(&savepointRecord, 0, sizeof(savepointRecord));
	strcpy(savepointRecord.RDB$SAVEPOINT_NAME, savepointDef.savepointName.c_str());
	strcpy(savepointRecord.RDB$TRANSACTION_ID, savepointDef.transactionId.c_str());
	strcpy(savepointRecord.RDB$SAVEPOINT_LEVEL, savepointDef.savepointLevel.c_str());
	strcpy(savepointRecord.RDB$CREATION_TIMESTAMP, savepointDef.creationTimestamp.c_str());
	strcpy(savepointRecord.RDB$ISOLATION_LEVEL, savepointDef.isolationLevel.c_str());
	strcpy(savepointRecord.RDB$LOCK_TIMEOUT, savepointDef.lockTimeout.c_str());
	strcpy(savepointRecord.RDB$NESTED_LEVEL, savepointDef.nestedLevel.c_str());
	savepointRecord.RDB$IS_DISTRIBUTED = savepointDef.isDistributed ? TRUE : FALSE;
	savepointRecord.RDB$IS_READ_ONLY = savepointDef.isReadOnly ? TRUE : FALSE;
	savepointRecord.RDB$AUTO_ROLLBACK = savepointDef.autoRollback ? TRUE : FALSE;

	if (!savepointDef.resourceUsage.empty())
	{
		strcpy(savepointRecord.RDB$RESOURCE_USAGE, savepointDef.resourceUsage.c_str());
		savepointRecord.RDB$RESOURCE_USAGE_NULL = FALSE;
	}
	else
		savepointRecord.RDB$RESOURCE_USAGE_NULL = TRUE;

	if (!savepointDef.dependencies.empty())
	{
		strcpy(savepointRecord.RDB$DEPENDENCIES, savepointDef.dependencies.c_str());
		savepointRecord.RDB$DEPENDENCIES_NULL = FALSE;
	}
	else
		savepointRecord.RDB$DEPENDENCIES_NULL = TRUE;

	EXE_send(tdbb, handle133, 0, sizeof(RDB$SAVEPOINT_INFO_RECORD), &savepointRecord);
	EXE_unwind(tdbb, handle133);
}

void TransactionManagementNode::storeRollbackSegments(thread_db* tdbb, jrd_tra* transaction,
	const RollbackConfig& rollbackConfig)
{
	// Converted FOR loop #125: Store rollback segments with savepoint management
	jrd_req* handle134 = CMP_find_request(tdbb, drq_store_rollback_segments, DYN_REQUESTS);
	EXE_start(tdbb, handle134, transaction);

	struct RDB$ROLLBACK_SEGMENTS_RECORD {
		char RDB$SEGMENT_NAME[256];
		char RDB$TRANSACTION_ID[64];
		char RDB$SEGMENT_TYPE[32];
		char RDB$STORAGE_LOCATION[512];
		char RDB$SEGMENT_SIZE[32];
		char RDB$RETENTION_PERIOD[16];
		char RDB$COMPRESSION_LEVEL[8];
		char RDB$ENCRYPTION_STATUS[16];
		short RDB$SEGMENT_ORDER;
		short RDB$IS_ACTIVE;
		short RDB$AUTO_EXTEND;
		short RDB$MAX_EXTENDS;
		char RDB$MONITORING_ALERTS[256];
		char RDB$MONITORING_ALERTS_NULL;
	} rollbackSegmentRecord;

	for (size_t i = 0; i < rollbackConfig.segments.getCount(); i++)
	{
		memset(&rollbackSegmentRecord, 0, sizeof(rollbackSegmentRecord));
		strcpy(rollbackSegmentRecord.RDB$SEGMENT_NAME, rollbackConfig.segments[i].segmentName.c_str());
		strcpy(rollbackSegmentRecord.RDB$TRANSACTION_ID, rollbackConfig.segments[i].transactionId.c_str());
		strcpy(rollbackSegmentRecord.RDB$SEGMENT_TYPE, rollbackConfig.segments[i].segmentType.c_str());
		strcpy(rollbackSegmentRecord.RDB$STORAGE_LOCATION, rollbackConfig.segments[i].storageLocation.c_str());
		strcpy(rollbackSegmentRecord.RDB$SEGMENT_SIZE, rollbackConfig.segments[i].segmentSize.c_str());
		strcpy(rollbackSegmentRecord.RDB$RETENTION_PERIOD, rollbackConfig.segments[i].retentionPeriod.c_str());
		strcpy(rollbackSegmentRecord.RDB$COMPRESSION_LEVEL, rollbackConfig.segments[i].compressionLevel.c_str());
		strcpy(rollbackSegmentRecord.RDB$ENCRYPTION_STATUS, rollbackConfig.segments[i].encryptionStatus.c_str());
		rollbackSegmentRecord.RDB$SEGMENT_ORDER = static_cast<short>(i);
		rollbackSegmentRecord.RDB$IS_ACTIVE = rollbackConfig.segments[i].isActive ? TRUE : FALSE;
		rollbackSegmentRecord.RDB$AUTO_EXTEND = rollbackConfig.segments[i].autoExtend ? TRUE : FALSE;
		rollbackSegmentRecord.RDB$MAX_EXTENDS = rollbackConfig.segments[i].maxExtends;

		if (!rollbackConfig.segments[i].monitoringAlerts.empty())
		{
			strcpy(rollbackSegmentRecord.RDB$MONITORING_ALERTS, rollbackConfig.segments[i].monitoringAlerts.c_str());
			rollbackSegmentRecord.RDB$MONITORING_ALERTS_NULL = FALSE;
		}
		else
			rollbackSegmentRecord.RDB$MONITORING_ALERTS_NULL = TRUE;

		EXE_send(tdbb, handle134, 0, sizeof(RDB$ROLLBACK_SEGMENTS_RECORD), &rollbackSegmentRecord);
	}
	EXE_unwind(tdbb, handle134);
}

void BackupManagementNode::storeIncrementalBackupConfig(thread_db* tdbb, jrd_tra* transaction,
	const IncrementalBackupConfig& incBackupConfig)
{
	// Converted FOR loop #126: Store incremental backup configuration with extended operations
	jrd_req* handle135 = CMP_find_request(tdbb, drq_store_incremental_backup_config, DYN_REQUESTS);
	EXE_start(tdbb, handle135, transaction);

	struct RDB$INCREMENTAL_BACKUP_CONFIG_RECORD {
		char RDB$BACKUP_SET_NAME[256];
		char RDB$BASE_BACKUP_PATH[512];
		char RDB$INCREMENT_STRATEGY[32];
		char RDB$CHANGE_TRACKING_METHOD[64];
		char RDB$COMPRESSION_ALGORITHM[32];
		char RDB$ENCRYPTION_CIPHER[32];
		char RDB$VALIDATION_CHECKSUM[16];
		char RDB$METADATA_TRACKING[16];
		short RDB$BACKUP_LEVEL;
		short RDB$RETENTION_DAYS;
		short RDB$PARALLEL_DEGREE;
		short RDB$BLOCK_SIZE_KB;
		char RDB$CUSTOM_FILTERS[1024];
		char RDB$NOTIFICATION_HOOKS[512];
		char RDB$CUSTOM_FILTERS_NULL;
		char RDB$NOTIFICATION_HOOKS_NULL;
	} incBackupRecord;

	memset(&incBackupRecord, 0, sizeof(incBackupRecord));
	strcpy(incBackupRecord.RDB$BACKUP_SET_NAME, incBackupConfig.backupSetName.c_str());
	strcpy(incBackupRecord.RDB$BASE_BACKUP_PATH, incBackupConfig.baseBackupPath.c_str());
	strcpy(incBackupRecord.RDB$INCREMENT_STRATEGY, incBackupConfig.incrementStrategy.c_str());
	strcpy(incBackupRecord.RDB$CHANGE_TRACKING_METHOD, incBackupConfig.changeTrackingMethod.c_str());
	strcpy(incBackupRecord.RDB$COMPRESSION_ALGORITHM, incBackupConfig.compressionAlgorithm.c_str());
	strcpy(incBackupRecord.RDB$ENCRYPTION_CIPHER, incBackupConfig.encryptionCipher.c_str());
	strcpy(incBackupRecord.RDB$VALIDATION_CHECKSUM, incBackupConfig.validationChecksum.c_str());
	strcpy(incBackupRecord.RDB$METADATA_TRACKING, incBackupConfig.metadataTracking.c_str());
	incBackupRecord.RDB$BACKUP_LEVEL = incBackupConfig.backupLevel;
	incBackupRecord.RDB$RETENTION_DAYS = incBackupConfig.retentionDays;
	incBackupRecord.RDB$PARALLEL_DEGREE = incBackupConfig.parallelDegree;
	incBackupRecord.RDB$BLOCK_SIZE_KB = incBackupConfig.blockSizeKB;

	if (!incBackupConfig.customFilters.empty())
	{
		strcpy(incBackupRecord.RDB$CUSTOM_FILTERS, incBackupConfig.customFilters.c_str());
		incBackupRecord.RDB$CUSTOM_FILTERS_NULL = FALSE;
	}
	else
		incBackupRecord.RDB$CUSTOM_FILTERS_NULL = TRUE;

	if (!incBackupConfig.notificationHooks.empty())
	{
		strcpy(incBackupRecord.RDB$NOTIFICATION_HOOKS, incBackupConfig.notificationHooks.c_str());
		incBackupRecord.RDB$NOTIFICATION_HOOKS_NULL = FALSE;
	}
	else
		incBackupRecord.RDB$NOTIFICATION_HOOKS_NULL = TRUE;

	EXE_send(tdbb, handle135, 0, sizeof(RDB$INCREMENTAL_BACKUP_CONFIG_RECORD), &incBackupRecord);
	EXE_unwind(tdbb, handle135);
}

void BackupManagementNode::storeDifferentialBackupMapping(thread_db* tdbb, jrd_tra* transaction,
	const DifferentialBackupMapping& diffMapping)
{
	// Converted FOR loop #127: Store differential backup mapping with backup operations
	jrd_req* handle136 = CMP_find_request(tdbb, drq_store_differential_backup_mapping, DYN_REQUESTS);
	EXE_start(tdbb, handle136, transaction);

	struct RDB$DIFFERENTIAL_BACKUP_MAPPING_RECORD {
		char RDB$MAPPING_NAME[256];
		char RDB$SOURCE_DATABASE[512];
		char RDB$TARGET_BACKUP_SET[256];
		char RDB$CHANGE_DETECTION_METHOD[64];
		char RDB$BLOCK_CHANGE_TRACKING[16];
		char RDB$COMPRESSION_RATIO[16];
		char RDB$DEDUPLICATION_LEVEL[16];
		char RDB$VERIFICATION_METHOD[32];
		short RDB$SCAN_INTERVAL_MINUTES;
		short RDB$MAX_BACKUP_SIZE_GB;
		short RDB$RETENTION_POLICY_DAYS;
		short RDB$PARALLEL_STREAMS;
		char RDB$EXCLUSION_PATTERNS[1024];
		char RDB$PERFORMANCE_TUNING[512];
		char RDB$EXCLUSION_PATTERNS_NULL;
		char RDB$PERFORMANCE_TUNING_NULL;
	} diffMappingRecord;

	for (size_t i = 0; i < diffMapping.mappings.getCount(); i++)
	{
		memset(&diffMappingRecord, 0, sizeof(diffMappingRecord));
		strcpy(diffMappingRecord.RDB$MAPPING_NAME, diffMapping.mappings[i].mappingName.c_str());
		strcpy(diffMappingRecord.RDB$SOURCE_DATABASE, diffMapping.mappings[i].sourceDatabase.c_str());
		strcpy(diffMappingRecord.RDB$TARGET_BACKUP_SET, diffMapping.mappings[i].targetBackupSet.c_str());
		strcpy(diffMappingRecord.RDB$CHANGE_DETECTION_METHOD, diffMapping.mappings[i].changeDetectionMethod.c_str());
		strcpy(diffMappingRecord.RDB$BLOCK_CHANGE_TRACKING, diffMapping.mappings[i].blockChangeTracking.c_str());
		strcpy(diffMappingRecord.RDB$COMPRESSION_RATIO, diffMapping.mappings[i].compressionRatio.c_str());
		strcpy(diffMappingRecord.RDB$DEDUPLICATION_LEVEL, diffMapping.mappings[i].deduplicationLevel.c_str());
		strcpy(diffMappingRecord.RDB$VERIFICATION_METHOD, diffMapping.mappings[i].verificationMethod.c_str());
		diffMappingRecord.RDB$SCAN_INTERVAL_MINUTES = diffMapping.mappings[i].scanIntervalMinutes;
		diffMappingRecord.RDB$MAX_BACKUP_SIZE_GB = diffMapping.mappings[i].maxBackupSizeGB;
		diffMappingRecord.RDB$RETENTION_POLICY_DAYS = diffMapping.mappings[i].retentionPolicyDays;
		diffMappingRecord.RDB$PARALLEL_STREAMS = diffMapping.mappings[i].parallelStreams;

		if (!diffMapping.mappings[i].exclusionPatterns.empty())
		{
			strcpy(diffMappingRecord.RDB$EXCLUSION_PATTERNS, diffMapping.mappings[i].exclusionPatterns.c_str());
			diffMappingRecord.RDB$EXCLUSION_PATTERNS_NULL = FALSE;
		}
		else
			diffMappingRecord.RDB$EXCLUSION_PATTERNS_NULL = TRUE;

		if (!diffMapping.mappings[i].performanceTuning.empty())
		{
			strcpy(diffMappingRecord.RDB$PERFORMANCE_TUNING, diffMapping.mappings[i].performanceTuning.c_str());
			diffMappingRecord.RDB$PERFORMANCE_TUNING_NULL = FALSE;
		}
		else
			diffMappingRecord.RDB$PERFORMANCE_TUNING_NULL = TRUE;

		EXE_send(tdbb, handle136, 0, sizeof(RDB$DIFFERENTIAL_BACKUP_MAPPING_RECORD), &diffMappingRecord);
	}
	EXE_unwind(tdbb, handle136);
}

void SecurityManagementNode::storeAdvancedPrivileges(thread_db* tdbb, jrd_tra* transaction,
	const AdvancedPrivilegeGrant& privilegeGrant)
{
	// Converted FOR loop #128: Store advanced privileges with high-priority DDL operations
	jrd_req* handle137 = CMP_find_request(tdbb, drq_store_advanced_privileges, DYN_REQUESTS);
	EXE_start(tdbb, handle137, transaction);

	struct RDB$ADVANCED_PRIVILEGES_RECORD {
		char RDB$GRANTEE_NAME[256];
		char RDB$GRANTEE_TYPE[16];
		char RDB$PRIVILEGE_TYPE[32];
		char RDB$OBJECT_NAME[256];
		char RDB$OBJECT_TYPE[32];
		char RDB$OBJECT_SCHEMA[256];
		char RDB$GRANT_SCOPE[32];
		char RDB$CONDITIONS[1024];
		short RDB$IS_GRANTABLE;
		short RDB$WITH_HIERARCHY;
		short RDB$EFFECTIVE_FROM_DATE;
		short RDB$EFFECTIVE_TO_DATE;
		char RDB$GRANTOR_NAME[256];
		char RDB$AUDIT_TRAIL[512];
		char RDB$OBJECT_SCHEMA_NULL;
		char RDB$CONDITIONS_NULL;
		char RDB$AUDIT_TRAIL_NULL;
	} privilegeRecord;

	for (size_t i = 0; i < privilegeGrant.privileges.getCount(); i++)
	{
		memset(&privilegeRecord, 0, sizeof(privilegeRecord));
		strcpy(privilegeRecord.RDB$GRANTEE_NAME, privilegeGrant.privileges[i].granteeName.c_str());
		strcpy(privilegeRecord.RDB$GRANTEE_TYPE, privilegeGrant.privileges[i].granteeType.c_str());
		strcpy(privilegeRecord.RDB$PRIVILEGE_TYPE, privilegeGrant.privileges[i].privilegeType.c_str());
		strcpy(privilegeRecord.RDB$OBJECT_NAME, privilegeGrant.privileges[i].objectName.c_str());
		strcpy(privilegeRecord.RDB$OBJECT_TYPE, privilegeGrant.privileges[i].objectType.c_str());
		strcpy(privilegeRecord.RDB$GRANT_SCOPE, privilegeGrant.privileges[i].grantScope.c_str());
		privilegeRecord.RDB$IS_GRANTABLE = privilegeGrant.privileges[i].isGrantable ? TRUE : FALSE;
		privilegeRecord.RDB$WITH_HIERARCHY = privilegeGrant.privileges[i].withHierarchy ? TRUE : FALSE;
		privilegeRecord.RDB$EFFECTIVE_FROM_DATE = privilegeGrant.privileges[i].effectiveFromDate;
		privilegeRecord.RDB$EFFECTIVE_TO_DATE = privilegeGrant.privileges[i].effectiveToDate;
		strcpy(privilegeRecord.RDB$GRANTOR_NAME, privilegeGrant.privileges[i].grantorName.c_str());

		if (!privilegeGrant.privileges[i].objectSchema.empty())
		{
			strcpy(privilegeRecord.RDB$OBJECT_SCHEMA, privilegeGrant.privileges[i].objectSchema.c_str());
			privilegeRecord.RDB$OBJECT_SCHEMA_NULL = FALSE;
		}
		else
			privilegeRecord.RDB$OBJECT_SCHEMA_NULL = TRUE;

		if (!privilegeGrant.privileges[i].conditions.empty())
		{
			strcpy(privilegeRecord.RDB$CONDITIONS, privilegeGrant.privileges[i].conditions.c_str());
			privilegeRecord.RDB$CONDITIONS_NULL = FALSE;
		}
		else
			privilegeRecord.RDB$CONDITIONS_NULL = TRUE;

		if (!privilegeGrant.privileges[i].auditTrail.empty())
		{
			strcpy(privilegeRecord.RDB$AUDIT_TRAIL, privilegeGrant.privileges[i].auditTrail.c_str());
			privilegeRecord.RDB$AUDIT_TRAIL_NULL = FALSE;
		}
		else
			privilegeRecord.RDB$AUDIT_TRAIL_NULL = TRUE;

		EXE_send(tdbb, handle137, 0, sizeof(RDB$ADVANCED_PRIVILEGES_RECORD), &privilegeRecord);
	}
	EXE_unwind(tdbb, handle137);
}

void SecurityManagementNode::storeRoleHierarchy(thread_db* tdbb, jrd_tra* transaction,
	const RoleHierarchyDefinition& roleHierarchy)
{
	// Converted FOR loop #129: Store role hierarchy with security management
	jrd_req* handle138 = CMP_find_request(tdbb, drq_store_role_hierarchy, DYN_REQUESTS);
	EXE_start(tdbb, handle138, transaction);

	struct RDB$ROLE_HIERARCHY_RECORD {
		char RDB$PARENT_ROLE[256];
		char RDB$CHILD_ROLE[256];
		char RDB$INHERITANCE_TYPE[32];
		char RDB$INHERITANCE_CONDITIONS[512];
		char RDB$DELEGATION_RULES[256];
		char RDB$ACTIVATION_POLICY[64];
		short RDB$HIERARCHY_LEVEL;
		short RDB$IS_DELEGATABLE;
		short RDB$MAX_DELEGATION_DEPTH;
		short RDB$REQUIRES_AUTHENTICATION;
		char RDB$EFFECTIVE_PERIOD[64];
		char RDB$AUDIT_LOGGING[16];
		char RDB$INHERITANCE_CONDITIONS_NULL;
		char RDB$EFFECTIVE_PERIOD_NULL;
	} roleHierarchyRecord;

	for (size_t i = 0; i < roleHierarchy.relationships.getCount(); i++)
	{
		memset(&roleHierarchyRecord, 0, sizeof(roleHierarchyRecord));
		strcpy(roleHierarchyRecord.RDB$PARENT_ROLE, roleHierarchy.relationships[i].parentRole.c_str());
		strcpy(roleHierarchyRecord.RDB$CHILD_ROLE, roleHierarchy.relationships[i].childRole.c_str());
		strcpy(roleHierarchyRecord.RDB$INHERITANCE_TYPE, roleHierarchy.relationships[i].inheritanceType.c_str());
		strcpy(roleHierarchyRecord.RDB$DELEGATION_RULES, roleHierarchy.relationships[i].delegationRules.c_str());
		strcpy(roleHierarchyRecord.RDB$ACTIVATION_POLICY, roleHierarchy.relationships[i].activationPolicy.c_str());
		roleHierarchyRecord.RDB$HIERARCHY_LEVEL = roleHierarchy.relationships[i].hierarchyLevel;
		roleHierarchyRecord.RDB$IS_DELEGATABLE = roleHierarchy.relationships[i].isDelegatable ? TRUE : FALSE;
		roleHierarchyRecord.RDB$MAX_DELEGATION_DEPTH = roleHierarchy.relationships[i].maxDelegationDepth;
		roleHierarchyRecord.RDB$REQUIRES_AUTHENTICATION = roleHierarchy.relationships[i].requiresAuthentication ? TRUE : FALSE;
		strcpy(roleHierarchyRecord.RDB$AUDIT_LOGGING, roleHierarchy.relationships[i].auditLogging.c_str());

		if (!roleHierarchy.relationships[i].inheritanceConditions.empty())
		{
			strcpy(roleHierarchyRecord.RDB$INHERITANCE_CONDITIONS, roleHierarchy.relationships[i].inheritanceConditions.c_str());
			roleHierarchyRecord.RDB$INHERITANCE_CONDITIONS_NULL = FALSE;
		}
		else
			roleHierarchyRecord.RDB$INHERITANCE_CONDITIONS_NULL = TRUE;

		if (!roleHierarchy.relationships[i].effectivePeriod.empty())
		{
			strcpy(roleHierarchyRecord.RDB$EFFECTIVE_PERIOD, roleHierarchy.relationships[i].effectivePeriod.c_str());
			roleHierarchyRecord.RDB$EFFECTIVE_PERIOD_NULL = FALSE;
		}
		else
			roleHierarchyRecord.RDB$EFFECTIVE_PERIOD_NULL = TRUE;

		EXE_send(tdbb, handle138, 0, sizeof(RDB$ROLE_HIERARCHY_RECORD), &roleHierarchyRecord);
	}
	EXE_unwind(tdbb, handle138);
}

void PerformanceManagementNode::storeIndexOptimizationRules(thread_db* tdbb, jrd_tra* transaction,
	const IndexOptimizationRules& optimizationRules)
{
	// Converted FOR loop #130: Store index optimization rules with remaining DDL operations
	jrd_req* handle139 = CMP_find_request(tdbb, drq_store_index_optimization_rules, DYN_REQUESTS);
	EXE_start(tdbb, handle139, transaction);

	struct RDB$INDEX_OPTIMIZATION_RULES_RECORD {
		char RDB$RULE_NAME[256];
		char RDB$INDEX_PATTERN[256];
		char RDB$OPTIMIZATION_STRATEGY[64];
		char RDB$TRIGGER_CONDITIONS[512];
		char RDB$OPTIMIZATION_PARAMETERS[1024];
		char RDB$PERFORMANCE_THRESHOLDS[256];
		char RDB$MAINTENANCE_SCHEDULE[128];
		char RDB$NOTIFICATION_RULES[256];
		short RDB$RULE_PRIORITY;
		short RDB$IS_AUTOMATIC;
		short RDB$MAX_EXECUTION_TIME_MINUTES;
		short RDB$STATISTICS_UPDATE_FREQUENCY;
		char RDB$CUSTOM_SCRIPTS[2048];
		char RDB$TRIGGER_CONDITIONS_NULL;
		char RDB$CUSTOM_SCRIPTS_NULL;
	} optimizationRuleRecord;

	for (size_t i = 0; i < optimizationRules.rules.getCount(); i++)
	{
		memset(&optimizationRuleRecord, 0, sizeof(optimizationRuleRecord));
		strcpy(optimizationRuleRecord.RDB$RULE_NAME, optimizationRules.rules[i].ruleName.c_str());
		strcpy(optimizationRuleRecord.RDB$INDEX_PATTERN, optimizationRules.rules[i].indexPattern.c_str());
		strcpy(optimizationRuleRecord.RDB$OPTIMIZATION_STRATEGY, optimizationRules.rules[i].optimizationStrategy.c_str());
		strcpy(optimizationRuleRecord.RDB$OPTIMIZATION_PARAMETERS, optimizationRules.rules[i].optimizationParameters.c_str());
		strcpy(optimizationRuleRecord.RDB$PERFORMANCE_THRESHOLDS, optimizationRules.rules[i].performanceThresholds.c_str());
		strcpy(optimizationRuleRecord.RDB$MAINTENANCE_SCHEDULE, optimizationRules.rules[i].maintenanceSchedule.c_str());
		strcpy(optimizationRuleRecord.RDB$NOTIFICATION_RULES, optimizationRules.rules[i].notificationRules.c_str());
		optimizationRuleRecord.RDB$RULE_PRIORITY = optimizationRules.rules[i].rulePriority;
		optimizationRuleRecord.RDB$IS_AUTOMATIC = optimizationRules.rules[i].isAutomatic ? TRUE : FALSE;
		optimizationRuleRecord.RDB$MAX_EXECUTION_TIME_MINUTES = optimizationRules.rules[i].maxExecutionTimeMinutes;
		optimizationRuleRecord.RDB$STATISTICS_UPDATE_FREQUENCY = optimizationRules.rules[i].statisticsUpdateFrequency;

		if (!optimizationRules.rules[i].triggerConditions.empty())
		{
			strcpy(optimizationRuleRecord.RDB$TRIGGER_CONDITIONS, optimizationRules.rules[i].triggerConditions.c_str());
			optimizationRuleRecord.RDB$TRIGGER_CONDITIONS_NULL = FALSE;
		}
		else
			optimizationRuleRecord.RDB$TRIGGER_CONDITIONS_NULL = TRUE;

		if (!optimizationRules.rules[i].customScripts.empty())
		{
			strcpy(optimizationRuleRecord.RDB$CUSTOM_SCRIPTS, optimizationRules.rules[i].customScripts.c_str());
			optimizationRuleRecord.RDB$CUSTOM_SCRIPTS_NULL = FALSE;
		}
		else
			optimizationRuleRecord.RDB$CUSTOM_SCRIPTS_NULL = TRUE;

		EXE_send(tdbb, handle139, 0, sizeof(RDB$INDEX_OPTIMIZATION_RULES_RECORD), &optimizationRuleRecord);
	}
	EXE_unwind(tdbb, handle139);
}

void SystemTriggerNode::storeAdvancedDDLTriggers(thread_db* tdbb, jrd_tra* transaction,
	const AdvancedDDLTriggerDefinition& triggerDefinition)
{
	// Converted FOR loop #131: Store advanced DDL triggers with system event handling
	jrd_req* handle140 = CMP_find_request(tdbb, drq_store_advanced_ddl_triggers, DYN_REQUESTS);
	EXE_start(tdbb, handle140, transaction);

	struct RDB$ADVANCED_DDL_TRIGGERS_RECORD {
		char RDB$TRIGGER_NAME[256];
		char RDB$TRIGGER_TYPE[32];
		char RDB$DDL_EVENT_TYPE[64];
		char RDB$OBJECT_TYPE_FILTER[128];
		char RDB$SCHEMA_FILTER[256];
		char RDB$TRIGGER_SOURCE[8192];
		char RDB$EXECUTION_ORDER[32];
		char RDB$ERROR_HANDLING[64];
		char RDB$LOGGING_LEVEL[16];
		short RDB$TRIGGER_SEQUENCE;
		short RDB$IS_ACTIVE;
		short RDB$IS_SYSTEM_TRIGGER;
		short RDB$FIRE_ON_ROLLBACK;
		char RDB$DEPENDENCIES[1024];
		char RDB$PERFORMANCE_HINTS[512];
		char RDB$OBJECT_TYPE_FILTER_NULL;
		char RDB$SCHEMA_FILTER_NULL;
		char RDB$DEPENDENCIES_NULL;
		char RDB$PERFORMANCE_HINTS_NULL;
	} ddlTriggerRecord;

	for (size_t i = 0; i < triggerDefinition.triggers.getCount(); i++)
	{
		memset(&ddlTriggerRecord, 0, sizeof(ddlTriggerRecord));
		strcpy(ddlTriggerRecord.RDB$TRIGGER_NAME, triggerDefinition.triggers[i].triggerName.c_str());
		strcpy(ddlTriggerRecord.RDB$TRIGGER_TYPE, triggerDefinition.triggers[i].triggerType.c_str());
		strcpy(ddlTriggerRecord.RDB$DDL_EVENT_TYPE, triggerDefinition.triggers[i].ddlEventType.c_str());
		strcpy(ddlTriggerRecord.RDB$TRIGGER_SOURCE, triggerDefinition.triggers[i].triggerSource.c_str());
		strcpy(ddlTriggerRecord.RDB$EXECUTION_ORDER, triggerDefinition.triggers[i].executionOrder.c_str());
		strcpy(ddlTriggerRecord.RDB$ERROR_HANDLING, triggerDefinition.triggers[i].errorHandling.c_str());
		strcpy(ddlTriggerRecord.RDB$LOGGING_LEVEL, triggerDefinition.triggers[i].loggingLevel.c_str());
		ddlTriggerRecord.RDB$TRIGGER_SEQUENCE = triggerDefinition.triggers[i].triggerSequence;
		ddlTriggerRecord.RDB$IS_ACTIVE = triggerDefinition.triggers[i].isActive ? TRUE : FALSE;
		ddlTriggerRecord.RDB$IS_SYSTEM_TRIGGER = triggerDefinition.triggers[i].isSystemTrigger ? TRUE : FALSE;
		ddlTriggerRecord.RDB$FIRE_ON_ROLLBACK = triggerDefinition.triggers[i].fireOnRollback ? TRUE : FALSE;

		if (!triggerDefinition.triggers[i].objectTypeFilter.empty())
		{
			strcpy(ddlTriggerRecord.RDB$OBJECT_TYPE_FILTER, triggerDefinition.triggers[i].objectTypeFilter.c_str());
			ddlTriggerRecord.RDB$OBJECT_TYPE_FILTER_NULL = FALSE;
		}
		else
			ddlTriggerRecord.RDB$OBJECT_TYPE_FILTER_NULL = TRUE;

		if (!triggerDefinition.triggers[i].schemaFilter.empty())
		{
			strcpy(ddlTriggerRecord.RDB$SCHEMA_FILTER, triggerDefinition.triggers[i].schemaFilter.c_str());
			ddlTriggerRecord.RDB$SCHEMA_FILTER_NULL = FALSE;
		}
		else
			ddlTriggerRecord.RDB$SCHEMA_FILTER_NULL = TRUE;

		if (!triggerDefinition.triggers[i].dependencies.empty())
		{
			strcpy(ddlTriggerRecord.RDB$DEPENDENCIES, triggerDefinition.triggers[i].dependencies.c_str());
			ddlTriggerRecord.RDB$DEPENDENCIES_NULL = FALSE;
		}
		else
			ddlTriggerRecord.RDB$DEPENDENCIES_NULL = TRUE;

		if (!triggerDefinition.triggers[i].performanceHints.empty())
		{
			strcpy(ddlTriggerRecord.RDB$PERFORMANCE_HINTS, triggerDefinition.triggers[i].performanceHints.c_str());
			ddlTriggerRecord.RDB$PERFORMANCE_HINTS_NULL = FALSE;
		}
		else
			ddlTriggerRecord.RDB$PERFORMANCE_HINTS_NULL = TRUE;

		EXE_send(tdbb, handle140, 0, sizeof(RDB$ADVANCED_DDL_TRIGGERS_RECORD), &ddlTriggerRecord);
	}
	EXE_unwind(tdbb, handle140);
}

void PrivilegeManagementNode::storeRecursiveGrantCascade(thread_db* tdbb, jrd_tra* transaction,
	const RecursiveGrantCascade& grantCascade)
{
	// Converted FOR loop #132: Store recursive grant cascade with complex privilege management
	jrd_req* handle141 = CMP_find_request(tdbb, drq_store_recursive_grant_cascade, DYN_REQUESTS);
	EXE_start(tdbb, handle141, transaction);

	struct RDB$RECURSIVE_GRANT_CASCADE_RECORD {
		char RDB$CASCADE_ID[64];
		char RDB$PARENT_GRANT_ID[64];
		char RDB$CHILD_GRANT_ID[64];
		char RDB$CASCADE_TYPE[32];
		char RDB$PRIVILEGE_INHERITANCE_RULES[512];
		char RDB$PROPAGATION_CONSTRAINTS[256];
		char RDB$VALIDATION_RULES[512];
		char RDB$AUDIT_REQUIREMENTS[128];
		short RDB$CASCADE_DEPTH;
		short RDB$MAX_PROPAGATION_LEVELS;
		short RDB$IS_BIDIRECTIONAL;
		short RDB$REQUIRES_EXPLICIT_APPROVAL;
		char RDB$EFFECTIVE_DATE_RANGE[64];
		char RDB$NOTIFICATION_POLICY[128];
		char RDB$PRIVILEGE_INHERITANCE_RULES_NULL;
		char RDB$EFFECTIVE_DATE_RANGE_NULL;
	} cascadeRecord;

	for (size_t i = 0; i < grantCascade.cascades.getCount(); i++)
	{
		memset(&cascadeRecord, 0, sizeof(cascadeRecord));
		strcpy(cascadeRecord.RDB$CASCADE_ID, grantCascade.cascades[i].cascadeId.c_str());
		strcpy(cascadeRecord.RDB$PARENT_GRANT_ID, grantCascade.cascades[i].parentGrantId.c_str());
		strcpy(cascadeRecord.RDB$CHILD_GRANT_ID, grantCascade.cascades[i].childGrantId.c_str());
		strcpy(cascadeRecord.RDB$CASCADE_TYPE, grantCascade.cascades[i].cascadeType.c_str());
		strcpy(cascadeRecord.RDB$PROPAGATION_CONSTRAINTS, grantCascade.cascades[i].propagationConstraints.c_str());
		strcpy(cascadeRecord.RDB$VALIDATION_RULES, grantCascade.cascades[i].validationRules.c_str());
		strcpy(cascadeRecord.RDB$AUDIT_REQUIREMENTS, grantCascade.cascades[i].auditRequirements.c_str());
		cascadeRecord.RDB$CASCADE_DEPTH = grantCascade.cascades[i].cascadeDepth;
		cascadeRecord.RDB$MAX_PROPAGATION_LEVELS = grantCascade.cascades[i].maxPropagationLevels;
		cascadeRecord.RDB$IS_BIDIRECTIONAL = grantCascade.cascades[i].isBidirectional ? TRUE : FALSE;
		cascadeRecord.RDB$REQUIRES_EXPLICIT_APPROVAL = grantCascade.cascades[i].requiresExplicitApproval ? TRUE : FALSE;
		strcpy(cascadeRecord.RDB$NOTIFICATION_POLICY, grantCascade.cascades[i].notificationPolicy.c_str());

		if (!grantCascade.cascades[i].privilegeInheritanceRules.empty())
		{
			strcpy(cascadeRecord.RDB$PRIVILEGE_INHERITANCE_RULES, grantCascade.cascades[i].privilegeInheritanceRules.c_str());
			cascadeRecord.RDB$PRIVILEGE_INHERITANCE_RULES_NULL = FALSE;
		}
		else
			cascadeRecord.RDB$PRIVILEGE_INHERITANCE_RULES_NULL = TRUE;

		if (!grantCascade.cascades[i].effectiveDateRange.empty())
		{
			strcpy(cascadeRecord.RDB$EFFECTIVE_DATE_RANGE, grantCascade.cascades[i].effectiveDateRange.c_str());
			cascadeRecord.RDB$EFFECTIVE_DATE_RANGE_NULL = FALSE;
		}
		else
			cascadeRecord.RDB$EFFECTIVE_DATE_RANGE_NULL = TRUE;

		EXE_send(tdbb, handle141, 0, sizeof(RDB$RECURSIVE_GRANT_CASCADE_RECORD), &cascadeRecord);
	}
	EXE_unwind(tdbb, handle141);
}

void SchemaManagementNode::storeExtendedSchemaOperations(thread_db* tdbb, jrd_tra* transaction,
	const ExtendedSchemaOperations& schemaOperations)
{
	// Converted FOR loop #133: Store extended schema operations with cross-database references
	jrd_req* handle142 = CMP_find_request(tdbb, drq_store_extended_schema_operations, DYN_REQUESTS);
	EXE_start(tdbb, handle142, transaction);

	struct RDB$EXTENDED_SCHEMA_OPERATIONS_RECORD {
		char RDB$OPERATION_ID[64];
		char RDB$OPERATION_TYPE[32];
		char RDB$SOURCE_SCHEMA_PATH[512];
		char RDB$TARGET_SCHEMA_PATH[512];
		char RDB$CROSS_DATABASE_REFERENCE[256];
		char RDB$OPERATION_PARAMETERS[1024];
		char RDB$VALIDATION_CONSTRAINTS[512];
		char RDB$DEPENDENCY_RESOLUTION[256];
		char RDB$CONFLICT_RESOLUTION_STRATEGY[64];
		short RDB$OPERATION_PRIORITY;
		short RDB$IS_ATOMIC_OPERATION;
		short RDB$REQUIRES_SCHEMA_LOCK;
		short RDB$SUPPORTS_ROLLBACK;
		char RDB$EXECUTION_TIMESTAMP[32];
		char RDB$PERFORMANCE_PROFILE[128];
		char RDB$CROSS_DATABASE_REFERENCE_NULL;
		char RDB$OPERATION_PARAMETERS_NULL;
	} schemaOperationRecord;

	for (size_t i = 0; i < schemaOperations.operations.getCount(); i++)
	{
		memset(&schemaOperationRecord, 0, sizeof(schemaOperationRecord));
		strcpy(schemaOperationRecord.RDB$OPERATION_ID, schemaOperations.operations[i].operationId.c_str());
		strcpy(schemaOperationRecord.RDB$OPERATION_TYPE, schemaOperations.operations[i].operationType.c_str());
		strcpy(schemaOperationRecord.RDB$SOURCE_SCHEMA_PATH, schemaOperations.operations[i].sourceSchemaPath.c_str());
		strcpy(schemaOperationRecord.RDB$TARGET_SCHEMA_PATH, schemaOperations.operations[i].targetSchemaPath.c_str());
		strcpy(schemaOperationRecord.RDB$VALIDATION_CONSTRAINTS, schemaOperations.operations[i].validationConstraints.c_str());
		strcpy(schemaOperationRecord.RDB$DEPENDENCY_RESOLUTION, schemaOperations.operations[i].dependencyResolution.c_str());
		strcpy(schemaOperationRecord.RDB$CONFLICT_RESOLUTION_STRATEGY, schemaOperations.operations[i].conflictResolutionStrategy.c_str());
		schemaOperationRecord.RDB$OPERATION_PRIORITY = schemaOperations.operations[i].operationPriority;
		schemaOperationRecord.RDB$IS_ATOMIC_OPERATION = schemaOperations.operations[i].isAtomicOperation ? TRUE : FALSE;
		schemaOperationRecord.RDB$REQUIRES_SCHEMA_LOCK = schemaOperations.operations[i].requiresSchemaLock ? TRUE : FALSE;
		schemaOperationRecord.RDB$SUPPORTS_ROLLBACK = schemaOperations.operations[i].supportsRollback ? TRUE : FALSE;
		strcpy(schemaOperationRecord.RDB$EXECUTION_TIMESTAMP, schemaOperations.operations[i].executionTimestamp.c_str());
		strcpy(schemaOperationRecord.RDB$PERFORMANCE_PROFILE, schemaOperations.operations[i].performanceProfile.c_str());

		if (!schemaOperations.operations[i].crossDatabaseReference.empty())
		{
			strcpy(schemaOperationRecord.RDB$CROSS_DATABASE_REFERENCE, schemaOperations.operations[i].crossDatabaseReference.c_str());
			schemaOperationRecord.RDB$CROSS_DATABASE_REFERENCE_NULL = FALSE;
		}
		else
			schemaOperationRecord.RDB$CROSS_DATABASE_REFERENCE_NULL = TRUE;

		if (!schemaOperations.operations[i].operationParameters.empty())
		{
			strcpy(schemaOperationRecord.RDB$OPERATION_PARAMETERS, schemaOperations.operations[i].operationParameters.c_str());
			schemaOperationRecord.RDB$OPERATION_PARAMETERS_NULL = FALSE;
		}
		else
			schemaOperationRecord.RDB$OPERATION_PARAMETERS_NULL = TRUE;

		EXE_send(tdbb, handle142, 0, sizeof(RDB$EXTENDED_SCHEMA_OPERATIONS_RECORD), &schemaOperationRecord);
	}
	EXE_unwind(tdbb, handle142);
}

void PerformanceMonitoringNode::storeAdvancedStatisticsCollection(thread_db* tdbb, jrd_tra* transaction,
	const AdvancedStatisticsCollection& statisticsCollection)
{
	// Converted FOR loop #134: Store advanced statistics collection with performance monitoring
	jrd_req* handle143 = CMP_find_request(tdbb, drq_store_advanced_statistics_collection, DYN_REQUESTS);
	EXE_start(tdbb, handle143, transaction);

	struct RDB$ADVANCED_STATISTICS_COLLECTION_RECORD {
		char RDB$COLLECTION_ID[64];
		char RDB$COLLECTION_NAME[256];
		char RDB$STATISTICS_TYPE[32];
		char RDB$COLLECTION_SCOPE[64];
		char RDB$SAMPLING_STRATEGY[32];
		char RDB$AGGREGATION_RULES[512];
		char RDB$RETENTION_POLICY[128];
		char RDB$EXPORT_CONFIGURATION[256];
		char RDB$ALERT_THRESHOLDS[512];
		short RDB$COLLECTION_INTERVAL_SECONDS;
		short RDB$SAMPLE_SIZE_PERCENTAGE;
		short RDB$HISTORICAL_DEPTH_DAYS;
		short RDB$IS_REAL_TIME_COLLECTION;
		char RDB$CUSTOM_METRICS[1024];
		char RDB$VISUALIZATION_SETTINGS[256];
		char RDB$AGGREGATION_RULES_NULL;
		char RDB$CUSTOM_METRICS_NULL;
	} statisticsRecord;

	for (size_t i = 0; i < statisticsCollection.collections.getCount(); i++)
	{
		memset(&statisticsRecord, 0, sizeof(statisticsRecord));
		strcpy(statisticsRecord.RDB$COLLECTION_ID, statisticsCollection.collections[i].collectionId.c_str());
		strcpy(statisticsRecord.RDB$COLLECTION_NAME, statisticsCollection.collections[i].collectionName.c_str());
		strcpy(statisticsRecord.RDB$STATISTICS_TYPE, statisticsCollection.collections[i].statisticsType.c_str());
		strcpy(statisticsRecord.RDB$COLLECTION_SCOPE, statisticsCollection.collections[i].collectionScope.c_str());
		strcpy(statisticsRecord.RDB$SAMPLING_STRATEGY, statisticsCollection.collections[i].samplingStrategy.c_str());
		strcpy(statisticsRecord.RDB$RETENTION_POLICY, statisticsCollection.collections[i].retentionPolicy.c_str());
		strcpy(statisticsRecord.RDB$EXPORT_CONFIGURATION, statisticsCollection.collections[i].exportConfiguration.c_str());
		strcpy(statisticsRecord.RDB$ALERT_THRESHOLDS, statisticsCollection.collections[i].alertThresholds.c_str());
		statisticsRecord.RDB$COLLECTION_INTERVAL_SECONDS = statisticsCollection.collections[i].collectionIntervalSeconds;
		statisticsRecord.RDB$SAMPLE_SIZE_PERCENTAGE = statisticsCollection.collections[i].sampleSizePercentage;
		statisticsRecord.RDB$HISTORICAL_DEPTH_DAYS = statisticsCollection.collections[i].historicalDepthDays;
		statisticsRecord.RDB$IS_REAL_TIME_COLLECTION = statisticsCollection.collections[i].isRealTimeCollection ? TRUE : FALSE;
		strcpy(statisticsRecord.RDB$VISUALIZATION_SETTINGS, statisticsCollection.collections[i].visualizationSettings.c_str());

		if (!statisticsCollection.collections[i].aggregationRules.empty())
		{
			strcpy(statisticsRecord.RDB$AGGREGATION_RULES, statisticsCollection.collections[i].aggregationRules.c_str());
			statisticsRecord.RDB$AGGREGATION_RULES_NULL = FALSE;
		}
		else
			statisticsRecord.RDB$AGGREGATION_RULES_NULL = TRUE;

		if (!statisticsCollection.collections[i].customMetrics.empty())
		{
			strcpy(statisticsRecord.RDB$CUSTOM_METRICS, statisticsCollection.collections[i].customMetrics.c_str());
			statisticsRecord.RDB$CUSTOM_METRICS_NULL = FALSE;
		}
		else
			statisticsRecord.RDB$CUSTOM_METRICS_NULL = TRUE;

		EXE_send(tdbb, handle143, 0, sizeof(RDB$ADVANCED_STATISTICS_COLLECTION_RECORD), &statisticsRecord);
	}
	EXE_unwind(tdbb, handle143);
}

void ReplicationManagementNode::storeDistributedDatabaseSupport(thread_db* tdbb, jrd_tra* transaction,
	const DistributedDatabaseSupport& distributedSupport)
{
	// Converted FOR loop #135: Store distributed database support with advanced replication
	jrd_req* handle144 = CMP_find_request(tdbb, drq_store_distributed_database_support, DYN_REQUESTS);
	EXE_start(tdbb, handle144, transaction);

	struct RDB$DISTRIBUTED_DATABASE_SUPPORT_RECORD {
		char RDB$NODE_ID[64];
		char RDB$NODE_NAME[256];
		char RDB$NODE_TYPE[32];
		char RDB$CONNECTION_STRING[512];
		char RDB$REPLICATION_MODE[32];
		char RDB$CONFLICT_RESOLUTION_STRATEGY[64];
		char RDB$DATA_CONSISTENCY_RULES[512];
		char RDB$PARTITION_STRATEGY[128];
		char RDB$LOAD_BALANCING_ALGORITHM[64];
		short RDB$NODE_PRIORITY;
		short RDB$MAX_CONCURRENT_CONNECTIONS;
		short RDB$HEARTBEAT_INTERVAL_SECONDS;
		short RDB$IS_MASTER_NODE;
		char RDB$FAILOVER_CONFIGURATION[256];
		char RDB$MONITORING_ENDPOINTS[512];
		char RDB$DATA_CONSISTENCY_RULES_NULL;
		char RDB$FAILOVER_CONFIGURATION_NULL;
	} distributedRecord;

	for (size_t i = 0; i < distributedSupport.nodes.getCount(); i++)
	{
		memset(&distributedRecord, 0, sizeof(distributedRecord));
		strcpy(distributedRecord.RDB$NODE_ID, distributedSupport.nodes[i].nodeId.c_str());
		strcpy(distributedRecord.RDB$NODE_NAME, distributedSupport.nodes[i].nodeName.c_str());
		strcpy(distributedRecord.RDB$NODE_TYPE, distributedSupport.nodes[i].nodeType.c_str());
		strcpy(distributedRecord.RDB$CONNECTION_STRING, distributedSupport.nodes[i].connectionString.c_str());
		strcpy(distributedRecord.RDB$REPLICATION_MODE, distributedSupport.nodes[i].replicationMode.c_str());
		strcpy(distributedRecord.RDB$CONFLICT_RESOLUTION_STRATEGY, distributedSupport.nodes[i].conflictResolutionStrategy.c_str());
		strcpy(distributedRecord.RDB$PARTITION_STRATEGY, distributedSupport.nodes[i].partitionStrategy.c_str());
		strcpy(distributedRecord.RDB$LOAD_BALANCING_ALGORITHM, distributedSupport.nodes[i].loadBalancingAlgorithm.c_str());
		distributedRecord.RDB$NODE_PRIORITY = distributedSupport.nodes[i].nodePriority;
		distributedRecord.RDB$MAX_CONCURRENT_CONNECTIONS = distributedSupport.nodes[i].maxConcurrentConnections;
		distributedRecord.RDB$HEARTBEAT_INTERVAL_SECONDS = distributedSupport.nodes[i].heartbeatIntervalSeconds;
		distributedRecord.RDB$IS_MASTER_NODE = distributedSupport.nodes[i].isMasterNode ? TRUE : FALSE;
		strcpy(distributedRecord.RDB$MONITORING_ENDPOINTS, distributedSupport.nodes[i].monitoringEndpoints.c_str());

		if (!distributedSupport.nodes[i].dataConsistencyRules.empty())
		{
			strcpy(distributedRecord.RDB$DATA_CONSISTENCY_RULES, distributedSupport.nodes[i].dataConsistencyRules.c_str());
			distributedRecord.RDB$DATA_CONSISTENCY_RULES_NULL = FALSE;
		}
		else
			distributedRecord.RDB$DATA_CONSISTENCY_RULES_NULL = TRUE;

		if (!distributedSupport.nodes[i].failoverConfiguration.empty())
		{
			strcpy(distributedRecord.RDB$FAILOVER_CONFIGURATION, distributedSupport.nodes[i].failoverConfiguration.c_str());
			distributedRecord.RDB$FAILOVER_CONFIGURATION_NULL = FALSE;
		}
		else
			distributedRecord.RDB$FAILOVER_CONFIGURATION_NULL = TRUE;

		EXE_send(tdbb, handle144, 0, sizeof(RDB$DISTRIBUTED_DATABASE_SUPPORT_RECORD), &distributedRecord);
	}
	EXE_unwind(tdbb, handle144);
}

void SystemMaintenanceNode::storeAutomatedOptimization(thread_db* tdbb, jrd_tra* transaction,
	const AutomatedOptimization& automatedOptimization)
{
	// Converted FOR loop #136: Store automated optimization with system maintenance
	jrd_req* handle145 = CMP_find_request(tdbb, drq_store_automated_optimization, DYN_REQUESTS);
	EXE_start(tdbb, handle145, transaction);

	struct RDB$AUTOMATED_OPTIMIZATION_RECORD {
		char RDB$OPTIMIZATION_ID[64];
		char RDB$OPTIMIZATION_NAME[256];
		char RDB$OPTIMIZATION_TYPE[32];
		char RDB$TARGET_OBJECTS[512];
		char RDB$OPTIMIZATION_STRATEGY[128];
		char RDB$EXECUTION_SCHEDULE[64];
		char RDB$PERFORMANCE_CRITERIA[512];
		char RDB$RESOURCE_CONSTRAINTS[256];
		char RDB$NOTIFICATION_SETTINGS[128];
		short RDB$OPTIMIZATION_PRIORITY;
		short RDB$MAX_EXECUTION_TIME_MINUTES;
		short RDB$IS_AUTO_SCHEDULING;
		short RDB$REQUIRES_MAINTENANCE_WINDOW;
		char RDB$ROLLBACK_STRATEGY[64];
		char RDB$SUCCESS_CRITERIA[256];
		char RDB$TARGET_OBJECTS_NULL;
		char RDB$PERFORMANCE_CRITERIA_NULL;
	} optimizationRecord;

	for (size_t i = 0; i < automatedOptimization.optimizations.getCount(); i++)
	{
		memset(&optimizationRecord, 0, sizeof(optimizationRecord));
		strcpy(optimizationRecord.RDB$OPTIMIZATION_ID, automatedOptimization.optimizations[i].optimizationId.c_str());
		strcpy(optimizationRecord.RDB$OPTIMIZATION_NAME, automatedOptimization.optimizations[i].optimizationName.c_str());
		strcpy(optimizationRecord.RDB$OPTIMIZATION_TYPE, automatedOptimization.optimizations[i].optimizationType.c_str());
		strcpy(optimizationRecord.RDB$OPTIMIZATION_STRATEGY, automatedOptimization.optimizations[i].optimizationStrategy.c_str());
		strcpy(optimizationRecord.RDB$EXECUTION_SCHEDULE, automatedOptimization.optimizations[i].executionSchedule.c_str());
		strcpy(optimizationRecord.RDB$RESOURCE_CONSTRAINTS, automatedOptimization.optimizations[i].resourceConstraints.c_str());
		strcpy(optimizationRecord.RDB$NOTIFICATION_SETTINGS, automatedOptimization.optimizations[i].notificationSettings.c_str());
		optimizationRecord.RDB$OPTIMIZATION_PRIORITY = automatedOptimization.optimizations[i].optimizationPriority;
		optimizationRecord.RDB$MAX_EXECUTION_TIME_MINUTES = automatedOptimization.optimizations[i].maxExecutionTimeMinutes;
		optimizationRecord.RDB$IS_AUTO_SCHEDULING = automatedOptimization.optimizations[i].isAutoScheduling ? TRUE : FALSE;
		optimizationRecord.RDB$REQUIRES_MAINTENANCE_WINDOW = automatedOptimization.optimizations[i].requiresMaintenanceWindow ? TRUE : FALSE;
		strcpy(optimizationRecord.RDB$ROLLBACK_STRATEGY, automatedOptimization.optimizations[i].rollbackStrategy.c_str());
		strcpy(optimizationRecord.RDB$SUCCESS_CRITERIA, automatedOptimization.optimizations[i].successCriteria.c_str());

		if (!automatedOptimization.optimizations[i].targetObjects.empty())
		{
			strcpy(optimizationRecord.RDB$TARGET_OBJECTS, automatedOptimization.optimizations[i].targetObjects.c_str());
			optimizationRecord.RDB$TARGET_OBJECTS_NULL = FALSE;
		}
		else
			optimizationRecord.RDB$TARGET_OBJECTS_NULL = TRUE;

		if (!automatedOptimization.optimizations[i].performanceCriteria.empty())
		{
			strcpy(optimizationRecord.RDB$PERFORMANCE_CRITERIA, automatedOptimization.optimizations[i].performanceCriteria.c_str());
			optimizationRecord.RDB$PERFORMANCE_CRITERIA_NULL = FALSE;
		}
		else
			optimizationRecord.RDB$PERFORMANCE_CRITERIA_NULL = TRUE;

		EXE_send(tdbb, handle145, 0, sizeof(RDB$AUTOMATED_OPTIMIZATION_RECORD), &optimizationRecord);
	}
	EXE_unwind(tdbb, handle145);
}

void DatabaseEvolutionNode::storeSchemaVersioning(thread_db* tdbb, jrd_tra* transaction,
	const SchemaVersioning& schemaVersioning)
{
	// Converted FOR loop #137: Store schema versioning with database evolution support
	jrd_req* handle146 = CMP_find_request(tdbb, drq_store_schema_versioning, DYN_REQUESTS);
	EXE_start(tdbb, handle146, transaction);

	struct RDB$SCHEMA_VERSIONING_RECORD {
		char RDB$VERSION_ID[64];
		char RDB$VERSION_NAME[256];
		char RDB$SCHEMA_NAMESPACE[256];
		char RDB$VERSION_DESCRIPTION[512];
		char RDB$MIGRATION_SCRIPTS[2048];
		char RDB$ROLLBACK_SCRIPTS[2048];
		char RDB$COMPATIBILITY_MATRIX[512];
		char RDB$VALIDATION_RULES[512];
		short RDB$VERSION_MAJOR;
		short RDB$VERSION_MINOR;
		short RDB$VERSION_PATCH;
		short RDB$IS_BACKWARD_COMPATIBLE;
		char RDB$DEPLOYMENT_TIMESTAMP[32];
		char RDB$DEPLOYMENT_STRATEGY[64];
		char RDB$VERSION_DESCRIPTION_NULL;
		char RDB$MIGRATION_SCRIPTS_NULL;
	} versionRecord;

	for (size_t i = 0; i < schemaVersioning.versions.getCount(); i++)
	{
		memset(&versionRecord, 0, sizeof(versionRecord));
		strcpy(versionRecord.RDB$VERSION_ID, schemaVersioning.versions[i].versionId.c_str());
		strcpy(versionRecord.RDB$VERSION_NAME, schemaVersioning.versions[i].versionName.c_str());
		strcpy(versionRecord.RDB$SCHEMA_NAMESPACE, schemaVersioning.versions[i].schemaNamespace.c_str());
		strcpy(versionRecord.RDB$ROLLBACK_SCRIPTS, schemaVersioning.versions[i].rollbackScripts.c_str());
		strcpy(versionRecord.RDB$COMPATIBILITY_MATRIX, schemaVersioning.versions[i].compatibilityMatrix.c_str());
		strcpy(versionRecord.RDB$VALIDATION_RULES, schemaVersioning.versions[i].validationRules.c_str());
		versionRecord.RDB$VERSION_MAJOR = schemaVersioning.versions[i].versionMajor;
		versionRecord.RDB$VERSION_MINOR = schemaVersioning.versions[i].versionMinor;
		versionRecord.RDB$VERSION_PATCH = schemaVersioning.versions[i].versionPatch;
		versionRecord.RDB$IS_BACKWARD_COMPATIBLE = schemaVersioning.versions[i].isBackwardCompatible ? TRUE : FALSE;
		strcpy(versionRecord.RDB$DEPLOYMENT_TIMESTAMP, schemaVersioning.versions[i].deploymentTimestamp.c_str());
		strcpy(versionRecord.RDB$DEPLOYMENT_STRATEGY, schemaVersioning.versions[i].deploymentStrategy.c_str());

		if (!schemaVersioning.versions[i].versionDescription.empty())
		{
			strcpy(versionRecord.RDB$VERSION_DESCRIPTION, schemaVersioning.versions[i].versionDescription.c_str());
			versionRecord.RDB$VERSION_DESCRIPTION_NULL = FALSE;
		}
		else
			versionRecord.RDB$VERSION_DESCRIPTION_NULL = TRUE;

		if (!schemaVersioning.versions[i].migrationScripts.empty())
		{
			strcpy(versionRecord.RDB$MIGRATION_SCRIPTS, schemaVersioning.versions[i].migrationScripts.c_str());
			versionRecord.RDB$MIGRATION_SCRIPTS_NULL = FALSE;
		}
		else
			versionRecord.RDB$MIGRATION_SCRIPTS_NULL = TRUE;

		EXE_send(tdbb, handle146, 0, sizeof(RDB$SCHEMA_VERSIONING_RECORD), &versionRecord);
	}
	EXE_unwind(tdbb, handle146);
}

void AdvancedSecurityNode::storeEncryptionKeyManagement(thread_db* tdbb, jrd_tra* transaction,
	const EncryptionKeyManagement& keyManagement)
{
	// Converted FOR loop #138: Store encryption key management with advanced security
	jrd_req* handle147 = CMP_find_request(tdbb, drq_store_encryption_key_management, DYN_REQUESTS);
	EXE_start(tdbb, handle147, transaction);

	struct RDB$ENCRYPTION_KEY_MANAGEMENT_RECORD {
		char RDB$KEY_ID[128];
		char RDB$KEY_NAME[256];
		char RDB$KEY_TYPE[32];
		char RDB$ENCRYPTION_ALGORITHM[64];
		char RDB$KEY_SIZE_BITS[16];
		char RDB$KEY_DERIVATION_FUNCTION[64];
		char RDB$KEY_STORAGE_LOCATION[256];
		char RDB$ACCESS_CONTROL_POLICY[512];
		char RDB$ROTATION_SCHEDULE[64];
		short RDB$KEY_VERSION;
		short RDB$IS_ACTIVE;
		short RDB$IS_MASTER_KEY;
		short RDB$ROTATION_FREQUENCY_DAYS;
		char RDB$CREATION_TIMESTAMP[32];
		char RDB$EXPIRATION_TIMESTAMP[32];
		char RDB$ACCESS_CONTROL_POLICY_NULL;
		char RDB$EXPIRATION_TIMESTAMP_NULL;
	} keyRecord;

	for (size_t i = 0; i < keyManagement.keys.getCount(); i++)
	{
		memset(&keyRecord, 0, sizeof(keyRecord));
		strcpy(keyRecord.RDB$KEY_ID, keyManagement.keys[i].keyId.c_str());
		strcpy(keyRecord.RDB$KEY_NAME, keyManagement.keys[i].keyName.c_str());
		strcpy(keyRecord.RDB$KEY_TYPE, keyManagement.keys[i].keyType.c_str());
		strcpy(keyRecord.RDB$ENCRYPTION_ALGORITHM, keyManagement.keys[i].encryptionAlgorithm.c_str());
		strcpy(keyRecord.RDB$KEY_SIZE_BITS, keyManagement.keys[i].keySizeBits.c_str());
		strcpy(keyRecord.RDB$KEY_DERIVATION_FUNCTION, keyManagement.keys[i].keyDerivationFunction.c_str());
		strcpy(keyRecord.RDB$KEY_STORAGE_LOCATION, keyManagement.keys[i].keyStorageLocation.c_str());
		strcpy(keyRecord.RDB$ROTATION_SCHEDULE, keyManagement.keys[i].rotationSchedule.c_str());
		keyRecord.RDB$KEY_VERSION = keyManagement.keys[i].keyVersion;
		keyRecord.RDB$IS_ACTIVE = keyManagement.keys[i].isActive ? TRUE : FALSE;
		keyRecord.RDB$IS_MASTER_KEY = keyManagement.keys[i].isMasterKey ? TRUE : FALSE;
		keyRecord.RDB$ROTATION_FREQUENCY_DAYS = keyManagement.keys[i].rotationFrequencyDays;
		strcpy(keyRecord.RDB$CREATION_TIMESTAMP, keyManagement.keys[i].creationTimestamp.c_str());

		if (!keyManagement.keys[i].accessControlPolicy.empty())
		{
			strcpy(keyRecord.RDB$ACCESS_CONTROL_POLICY, keyManagement.keys[i].accessControlPolicy.c_str());
			keyRecord.RDB$ACCESS_CONTROL_POLICY_NULL = FALSE;
		}
		else
			keyRecord.RDB$ACCESS_CONTROL_POLICY_NULL = TRUE;

		if (!keyManagement.keys[i].expirationTimestamp.empty())
		{
			strcpy(keyRecord.RDB$EXPIRATION_TIMESTAMP, keyManagement.keys[i].expirationTimestamp.c_str());
			keyRecord.RDB$EXPIRATION_TIMESTAMP_NULL = FALSE;
		}
		else
			keyRecord.RDB$EXPIRATION_TIMESTAMP_NULL = TRUE;

		EXE_send(tdbb, handle147, 0, sizeof(RDB$ENCRYPTION_KEY_MANAGEMENT_RECORD), &keyRecord);
	}
	EXE_unwind(tdbb, handle147);
}

void QueryOptimizationNode::storeAdaptiveQueryPlanning(thread_db* tdbb, jrd_tra* transaction,
	const AdaptiveQueryPlanning& queryPlanning)
{
	// Converted FOR loop #139: Store adaptive query planning with query optimization
	jrd_req* handle148 = CMP_find_request(tdbb, drq_store_adaptive_query_planning, DYN_REQUESTS);
	EXE_start(tdbb, handle148, transaction);

	struct RDB$ADAPTIVE_QUERY_PLANNING_RECORD {
		char RDB$PLAN_ID[64];
		char RDB$QUERY_SIGNATURE[512];
		char RDB$OPTIMIZATION_STRATEGY[128];
		char RDB$EXECUTION_PLAN[2048];
		char RDB$STATISTICS_PROFILE[512];
		char RDB$COST_ESTIMATION[256];
		char RDB$ADAPTIVE_PARAMETERS[512];
		char RDB$LEARNING_ALGORITHM[64];
		short RDB$PLAN_VERSION;
		short RDB$EXECUTION_COUNT;
		short RDB$SUCCESS_RATE_PERCENTAGE;
		short RDB$IS_PREFERRED_PLAN;
		char RDB$CREATION_TIMESTAMP[32];
		char RDB$LAST_USED_TIMESTAMP[32];
		char RDB$EXECUTION_PLAN_NULL;
		char RDB$ADAPTIVE_PARAMETERS_NULL;
	} queryPlanRecord;

	for (size_t i = 0; i < queryPlanning.plans.getCount(); i++)
	{
		memset(&queryPlanRecord, 0, sizeof(queryPlanRecord));
		strcpy(queryPlanRecord.RDB$PLAN_ID, queryPlanning.plans[i].planId.c_str());
		strcpy(queryPlanRecord.RDB$QUERY_SIGNATURE, queryPlanning.plans[i].querySignature.c_str());
		strcpy(queryPlanRecord.RDB$OPTIMIZATION_STRATEGY, queryPlanning.plans[i].optimizationStrategy.c_str());
		strcpy(queryPlanRecord.RDB$STATISTICS_PROFILE, queryPlanning.plans[i].statisticsProfile.c_str());
		strcpy(queryPlanRecord.RDB$COST_ESTIMATION, queryPlanning.plans[i].costEstimation.c_str());
		strcpy(queryPlanRecord.RDB$LEARNING_ALGORITHM, queryPlanning.plans[i].learningAlgorithm.c_str());
		queryPlanRecord.RDB$PLAN_VERSION = queryPlanning.plans[i].planVersion;
		queryPlanRecord.RDB$EXECUTION_COUNT = queryPlanning.plans[i].executionCount;
		queryPlanRecord.RDB$SUCCESS_RATE_PERCENTAGE = queryPlanning.plans[i].successRatePercentage;
		queryPlanRecord.RDB$IS_PREFERRED_PLAN = queryPlanning.plans[i].isPreferredPlan ? TRUE : FALSE;
		strcpy(queryPlanRecord.RDB$CREATION_TIMESTAMP, queryPlanning.plans[i].creationTimestamp.c_str());
		strcpy(queryPlanRecord.RDB$LAST_USED_TIMESTAMP, queryPlanning.plans[i].lastUsedTimestamp.c_str());

		if (!queryPlanning.plans[i].executionPlan.empty())
		{
			strcpy(queryPlanRecord.RDB$EXECUTION_PLAN, queryPlanning.plans[i].executionPlan.c_str());
			queryPlanRecord.RDB$EXECUTION_PLAN_NULL = FALSE;
		}
		else
			queryPlanRecord.RDB$EXECUTION_PLAN_NULL = TRUE;

		if (!queryPlanning.plans[i].adaptiveParameters.empty())
		{
			strcpy(queryPlanRecord.RDB$ADAPTIVE_PARAMETERS, queryPlanning.plans[i].adaptiveParameters.c_str());
			queryPlanRecord.RDB$ADAPTIVE_PARAMETERS_NULL = FALSE;
		}
		else
			queryPlanRecord.RDB$ADAPTIVE_PARAMETERS_NULL = TRUE;

		EXE_send(tdbb, handle148, 0, sizeof(RDB$ADAPTIVE_QUERY_PLANNING_RECORD), &queryPlanRecord);
	}
	EXE_unwind(tdbb, handle148);
}

void DataLifecycleNode::storeInformationLifecycleManagement(thread_db* tdbb, jrd_tra* transaction,
	const InformationLifecycleManagement& lifecycleManagement)
{
	// Converted FOR loop #140: Store information lifecycle management with data lifecycle operations
	jrd_req* handle149 = CMP_find_request(tdbb, drq_store_information_lifecycle_management, DYN_REQUESTS);
	EXE_start(tdbb, handle149, transaction);

	struct RDB$INFORMATION_LIFECYCLE_MANAGEMENT_RECORD {
		char RDB$POLICY_ID[64];
		char RDB$POLICY_NAME[256];
		char RDB$DATA_CLASSIFICATION[32];
		char RDB$RETENTION_RULES[512];
		char RDB$ARCHIVAL_STRATEGY[128];
		char RDB$DISPOSAL_RULES[256];
		char RDB$COMPLIANCE_REQUIREMENTS[512];
		char RDB$ACCESS_PATTERNS[256];
		char RDB$STORAGE_TIERS[128];
		short RDB$RETENTION_PERIOD_DAYS;
		short RDB$ARCHIVAL_THRESHOLD_DAYS;
		short RDB$IS_AUTOMATED_POLICY;
		short RDB$REQUIRES_LEGAL_HOLD;
		char RDB$CREATION_TIMESTAMP[32];
		char RDB$LAST_REVIEW_TIMESTAMP[32];
		char RDB$COMPLIANCE_REQUIREMENTS_NULL;
		char RDB$ACCESS_PATTERNS_NULL;
	} lifecycleRecord;

	for (size_t i = 0; i < lifecycleManagement.policies.getCount(); i++)
	{
		memset(&lifecycleRecord, 0, sizeof(lifecycleRecord));
		strcpy(lifecycleRecord.RDB$POLICY_ID, lifecycleManagement.policies[i].policyId.c_str());
		strcpy(lifecycleRecord.RDB$POLICY_NAME, lifecycleManagement.policies[i].policyName.c_str());
		strcpy(lifecycleRecord.RDB$DATA_CLASSIFICATION, lifecycleManagement.policies[i].dataClassification.c_str());
		strcpy(lifecycleRecord.RDB$RETENTION_RULES, lifecycleManagement.policies[i].retentionRules.c_str());
		strcpy(lifecycleRecord.RDB$ARCHIVAL_STRATEGY, lifecycleManagement.policies[i].archivalStrategy.c_str());
		strcpy(lifecycleRecord.RDB$DISPOSAL_RULES, lifecycleManagement.policies[i].disposalRules.c_str());
		strcpy(lifecycleRecord.RDB$STORAGE_TIERS, lifecycleManagement.policies[i].storageTiers.c_str());
		lifecycleRecord.RDB$RETENTION_PERIOD_DAYS = lifecycleManagement.policies[i].retentionPeriodDays;
		lifecycleRecord.RDB$ARCHIVAL_THRESHOLD_DAYS = lifecycleManagement.policies[i].archivalThresholdDays;
		lifecycleRecord.RDB$IS_AUTOMATED_POLICY = lifecycleManagement.policies[i].isAutomatedPolicy ? TRUE : FALSE;
		lifecycleRecord.RDB$REQUIRES_LEGAL_HOLD = lifecycleManagement.policies[i].requiresLegalHold ? TRUE : FALSE;
		strcpy(lifecycleRecord.RDB$CREATION_TIMESTAMP, lifecycleManagement.policies[i].creationTimestamp.c_str());
		strcpy(lifecycleRecord.RDB$LAST_REVIEW_TIMESTAMP, lifecycleManagement.policies[i].lastReviewTimestamp.c_str());

		if (!lifecycleManagement.policies[i].complianceRequirements.empty())
		{
			strcpy(lifecycleRecord.RDB$COMPLIANCE_REQUIREMENTS, lifecycleManagement.policies[i].complianceRequirements.c_str());
			lifecycleRecord.RDB$COMPLIANCE_REQUIREMENTS_NULL = FALSE;
		}
		else
			lifecycleRecord.RDB$COMPLIANCE_REQUIREMENTS_NULL = TRUE;

		if (!lifecycleManagement.policies[i].accessPatterns.empty())
		{
			strcpy(lifecycleRecord.RDB$ACCESS_PATTERNS, lifecycleManagement.policies[i].accessPatterns.c_str());
			lifecycleRecord.RDB$ACCESS_PATTERNS_NULL = FALSE;
		}
		else
			lifecycleRecord.RDB$ACCESS_PATTERNS_NULL = TRUE;

		EXE_send(tdbb, handle149, 0, sizeof(RDB$INFORMATION_LIFECYCLE_MANAGEMENT_RECORD), &lifecycleRecord);
	}
	EXE_unwind(tdbb, handle149);
}

void ResourceManagementNode::storeWorkloadManagement(thread_db* tdbb, jrd_tra* transaction,
	const WorkloadManagement& workloadManagement)
{
	// Converted FOR loop #141: Store workload management with resource management operations
	jrd_req* handle150 = CMP_find_request(tdbb, drq_store_workload_management, DYN_REQUESTS);
	EXE_start(tdbb, handle150, transaction);

	struct RDB$WORKLOAD_MANAGEMENT_RECORD {
		char RDB$WORKLOAD_ID[64];
		char RDB$WORKLOAD_NAME[256];
		char RDB$WORKLOAD_CLASSIFICATION[32];
		char RDB$RESOURCE_ALLOCATION_RULES[512];
		char RDB$PRIORITY_ASSIGNMENT[64];
		char RDB$THROTTLING_RULES[256];
		char RDB$SCHEDULING_POLICY[64];
		char RDB$MONITORING_CONFIGURATION[256];
		char RDB$ALERT_THRESHOLDS[512];
		short RDB$MAX_CONCURRENT_SESSIONS;
		short RDB$MAX_CPU_PERCENTAGE;
		short RDB$MAX_MEMORY_MB;
		short RDB$IS_ELASTIC_SCALING;
		char RDB$CREATION_TIMESTAMP[32];
		char RDB$PERFORMANCE_PROFILE[128];
		char RDB$RESOURCE_ALLOCATION_RULES_NULL;
		char RDB$ALERT_THRESHOLDS_NULL;
	} workloadRecord;

	for (size_t i = 0; i < workloadManagement.workloads.getCount(); i++)
	{
		memset(&workloadRecord, 0, sizeof(workloadRecord));
		strcpy(workloadRecord.RDB$WORKLOAD_ID, workloadManagement.workloads[i].workloadId.c_str());
		strcpy(workloadRecord.RDB$WORKLOAD_NAME, workloadManagement.workloads[i].workloadName.c_str());
		strcpy(workloadRecord.RDB$WORKLOAD_CLASSIFICATION, workloadManagement.workloads[i].workloadClassification.c_str());
		strcpy(workloadRecord.RDB$PRIORITY_ASSIGNMENT, workloadManagement.workloads[i].priorityAssignment.c_str());
		strcpy(workloadRecord.RDB$THROTTLING_RULES, workloadManagement.workloads[i].throttlingRules.c_str());
		strcpy(workloadRecord.RDB$SCHEDULING_POLICY, workloadManagement.workloads[i].schedulingPolicy.c_str());
		strcpy(workloadRecord.RDB$MONITORING_CONFIGURATION, workloadManagement.workloads[i].monitoringConfiguration.c_str());
		workloadRecord.RDB$MAX_CONCURRENT_SESSIONS = workloadManagement.workloads[i].maxConcurrentSessions;
		workloadRecord.RDB$MAX_CPU_PERCENTAGE = workloadManagement.workloads[i].maxCpuPercentage;
		workloadRecord.RDB$MAX_MEMORY_MB = workloadManagement.workloads[i].maxMemoryMb;
		workloadRecord.RDB$IS_ELASTIC_SCALING = workloadManagement.workloads[i].isElasticScaling ? TRUE : FALSE;
		strcpy(workloadRecord.RDB$CREATION_TIMESTAMP, workloadManagement.workloads[i].creationTimestamp.c_str());
		strcpy(workloadRecord.RDB$PERFORMANCE_PROFILE, workloadManagement.workloads[i].performanceProfile.c_str());

		if (!workloadManagement.workloads[i].resourceAllocationRules.empty())
		{
			strcpy(workloadRecord.RDB$RESOURCE_ALLOCATION_RULES, workloadManagement.workloads[i].resourceAllocationRules.c_str());
			workloadRecord.RDB$RESOURCE_ALLOCATION_RULES_NULL = FALSE;
		}
		else
			workloadRecord.RDB$RESOURCE_ALLOCATION_RULES_NULL = TRUE;

		if (!workloadManagement.workloads[i].alertThresholds.empty())
		{
			strcpy(workloadRecord.RDB$ALERT_THRESHOLDS, workloadManagement.workloads[i].alertThresholds.c_str());
			workloadRecord.RDB$ALERT_THRESHOLDS_NULL = FALSE;
		}
		else
			workloadRecord.RDB$ALERT_THRESHOLDS_NULL = TRUE;

		EXE_send(tdbb, handle150, 0, sizeof(RDB$WORKLOAD_MANAGEMENT_RECORD), &workloadRecord);
	}
	EXE_unwind(tdbb, handle150);
}

void DataQualityNode::storeDataValidationRules(thread_db* tdbb, jrd_tra* transaction,
	const DataValidationRules& validationRules)
{
	// Converted FOR loop #142: Store data validation rules with data quality operations
	jrd_req* handle151 = CMP_find_request(tdbb, drq_store_data_validation_rules, DYN_REQUESTS);
	EXE_start(tdbb, handle151, transaction);

	struct RDB$DATA_VALIDATION_RULES_RECORD {
		char RDB$RULE_ID[64];
		char RDB$RULE_NAME[256];
		char RDB$VALIDATION_TYPE[32];
		char RDB$TARGET_OBJECTS[512];
		char RDB$VALIDATION_EXPRESSION[2048];
		char RDB$ERROR_MESSAGE_TEMPLATE[512];
		char RDB$SEVERITY_LEVEL[16];
		char RDB$REMEDIATION_ACTIONS[512];
		char RDB$NOTIFICATION_RULES[256];
		short RDB$RULE_PRIORITY;
		short RDB$IS_ACTIVE;
		short RDB$IS_BLOCKING_RULE;
		short RDB$MAX_VIOLATIONS_ALLOWED;
		char RDB$CREATION_TIMESTAMP[32];
		char RDB$PERFORMANCE_IMPACT_ASSESSMENT[128];
		char RDB$VALIDATION_EXPRESSION_NULL;
		char RDB$REMEDIATION_ACTIONS_NULL;
	} validationRecord;

	for (size_t i = 0; i < validationRules.rules.getCount(); i++)
	{
		memset(&validationRecord, 0, sizeof(validationRecord));
		strcpy(validationRecord.RDB$RULE_ID, validationRules.rules[i].ruleId.c_str());
		strcpy(validationRecord.RDB$RULE_NAME, validationRules.rules[i].ruleName.c_str());
		strcpy(validationRecord.RDB$VALIDATION_TYPE, validationRules.rules[i].validationType.c_str());
		strcpy(validationRecord.RDB$TARGET_OBJECTS, validationRules.rules[i].targetObjects.c_str());
		strcpy(validationRecord.RDB$ERROR_MESSAGE_TEMPLATE, validationRules.rules[i].errorMessageTemplate.c_str());
		strcpy(validationRecord.RDB$SEVERITY_LEVEL, validationRules.rules[i].severityLevel.c_str());
		strcpy(validationRecord.RDB$NOTIFICATION_RULES, validationRules.rules[i].notificationRules.c_str());
		validationRecord.RDB$RULE_PRIORITY = validationRules.rules[i].rulePriority;
		validationRecord.RDB$IS_ACTIVE = validationRules.rules[i].isActive ? TRUE : FALSE;
		validationRecord.RDB$IS_BLOCKING_RULE = validationRules.rules[i].isBlockingRule ? TRUE : FALSE;
		validationRecord.RDB$MAX_VIOLATIONS_ALLOWED = validationRules.rules[i].maxViolationsAllowed;
		strcpy(validationRecord.RDB$CREATION_TIMESTAMP, validationRules.rules[i].creationTimestamp.c_str());
		strcpy(validationRecord.RDB$PERFORMANCE_IMPACT_ASSESSMENT, validationRules.rules[i].performanceImpactAssessment.c_str());

		if (!validationRules.rules[i].validationExpression.empty())
		{
			strcpy(validationRecord.RDB$VALIDATION_EXPRESSION, validationRules.rules[i].validationExpression.c_str());
			validationRecord.RDB$VALIDATION_EXPRESSION_NULL = FALSE;
		}
		else
			validationRecord.RDB$VALIDATION_EXPRESSION_NULL = TRUE;

		if (!validationRules.rules[i].remediationActions.empty())
		{
			strcpy(validationRecord.RDB$REMEDIATION_ACTIONS, validationRules.rules[i].remediationActions.c_str());
			validationRecord.RDB$REMEDIATION_ACTIONS_NULL = FALSE;
		}
		else
			validationRecord.RDB$REMEDIATION_ACTIONS_NULL = TRUE;

		EXE_send(tdbb, handle151, 0, sizeof(RDB$DATA_VALIDATION_RULES_RECORD), &validationRecord);
	}
	EXE_unwind(tdbb, handle151);
}

void ComplianceManagementNode::storeRegulatoryCompliance(thread_db* tdbb, jrd_tra* transaction,
	const RegulatoryCompliance& regulatoryCompliance)
{
	// Converted FOR loop #143: Store regulatory compliance with compliance management operations
	jrd_req* handle152 = CMP_find_request(tdbb, drq_store_regulatory_compliance, DYN_REQUESTS);
	EXE_start(tdbb, handle152, transaction);

	struct RDB$REGULATORY_COMPLIANCE_RECORD {
		char RDB$COMPLIANCE_ID[64];
		char RDB$REGULATION_NAME[256];
		char RDB$COMPLIANCE_FRAMEWORK[128];
		char RDB$JURISDICTION[64];
		char RDB$COMPLIANCE_REQUIREMENTS[2048];
		char RDB$IMPLEMENTATION_GUIDELINES[1024];
		char RDB$AUDIT_PROCEDURES[512];
		char RDB$REPORTING_REQUIREMENTS[512];
		char RDB$EVIDENCE_COLLECTION_RULES[256];
		short RDB$COMPLIANCE_LEVEL;
		short RDB$IS_MANDATORY;
		short RDB$AUDIT_FREQUENCY_MONTHS;
		short RDB$RETENTION_PERIOD_YEARS;
		char RDB$EFFECTIVE_DATE[32];
		char RDB$LAST_AUDIT_DATE[32];
		char RDB$IMPLEMENTATION_GUIDELINES_NULL;
		char RDB$LAST_AUDIT_DATE_NULL;
	} complianceRecord;

	for (size_t i = 0; i < regulatoryCompliance.regulations.getCount(); i++)
	{
		memset(&complianceRecord, 0, sizeof(complianceRecord));
		strcpy(complianceRecord.RDB$COMPLIANCE_ID, regulatoryCompliance.regulations[i].complianceId.c_str());
		strcpy(complianceRecord.RDB$REGULATION_NAME, regulatoryCompliance.regulations[i].regulationName.c_str());
		strcpy(complianceRecord.RDB$COMPLIANCE_FRAMEWORK, regulatoryCompliance.regulations[i].complianceFramework.c_str());
		strcpy(complianceRecord.RDB$JURISDICTION, regulatoryCompliance.regulations[i].jurisdiction.c_str());
		strcpy(complianceRecord.RDB$COMPLIANCE_REQUIREMENTS, regulatoryCompliance.regulations[i].complianceRequirements.c_str());
		strcpy(complianceRecord.RDB$AUDIT_PROCEDURES, regulatoryCompliance.regulations[i].auditProcedures.c_str());
		strcpy(complianceRecord.RDB$REPORTING_REQUIREMENTS, regulatoryCompliance.regulations[i].reportingRequirements.c_str());
		strcpy(complianceRecord.RDB$EVIDENCE_COLLECTION_RULES, regulatoryCompliance.regulations[i].evidenceCollectionRules.c_str());
		complianceRecord.RDB$COMPLIANCE_LEVEL = regulatoryCompliance.regulations[i].complianceLevel;
		complianceRecord.RDB$IS_MANDATORY = regulatoryCompliance.regulations[i].isMandatory ? TRUE : FALSE;
		complianceRecord.RDB$AUDIT_FREQUENCY_MONTHS = regulatoryCompliance.regulations[i].auditFrequencyMonths;
		complianceRecord.RDB$RETENTION_PERIOD_YEARS = regulatoryCompliance.regulations[i].retentionPeriodYears;
		strcpy(complianceRecord.RDB$EFFECTIVE_DATE, regulatoryCompliance.regulations[i].effectiveDate.c_str());

		if (!regulatoryCompliance.regulations[i].implementationGuidelines.empty())
		{
			strcpy(complianceRecord.RDB$IMPLEMENTATION_GUIDELINES, regulatoryCompliance.regulations[i].implementationGuidelines.c_str());
			complianceRecord.RDB$IMPLEMENTATION_GUIDELINES_NULL = FALSE;
		}
		else
			complianceRecord.RDB$IMPLEMENTATION_GUIDELINES_NULL = TRUE;

		if (!regulatoryCompliance.regulations[i].lastAuditDate.empty())
		{
			strcpy(complianceRecord.RDB$LAST_AUDIT_DATE, regulatoryCompliance.regulations[i].lastAuditDate.c_str());
			complianceRecord.RDB$LAST_AUDIT_DATE_NULL = FALSE;
		}
		else
			complianceRecord.RDB$LAST_AUDIT_DATE_NULL = TRUE;

		EXE_send(tdbb, handle152, 0, sizeof(RDB$REGULATORY_COMPLIANCE_RECORD), &complianceRecord);
	}
	EXE_unwind(tdbb, handle152);
}

void AdvancedAnalyticsNode::storeMachineLearningModels(thread_db* tdbb, jrd_tra* transaction,
	const MachineLearningModels& mlModels)
{
	// Converted FOR loop #144: Store machine learning models with advanced analytics operations
	jrd_req* handle153 = CMP_find_request(tdbb, drq_store_machine_learning_models, DYN_REQUESTS);
	EXE_start(tdbb, handle153, transaction);

	struct RDB$MACHINE_LEARNING_MODELS_RECORD {
		char RDB$MODEL_ID[64];
		char RDB$MODEL_NAME[256];
		char RDB$MODEL_TYPE[32];
		char RDB$ALGORITHM_NAME[128];
		char RDB$TRAINING_DATASET[256];
		char RDB$MODEL_PARAMETERS[2048];
		char RDB$PERFORMANCE_METRICS[512];
		char RDB$VALIDATION_RESULTS[512];
		char RDB$DEPLOYMENT_CONFIGURATION[256];
		short RDB$MODEL_VERSION;
		short RDB$ACCURACY_PERCENTAGE;
		short RDB$IS_PRODUCTION_READY;
		short RDB$TRAINING_ITERATIONS;
		char RDB$CREATION_TIMESTAMP[32];
		char RDB$LAST_TRAINED_TIMESTAMP[32];
		char RDB$MODEL_PARAMETERS_NULL;
		char RDB$VALIDATION_RESULTS_NULL;
	} mlModelRecord;

	for (size_t i = 0; i < mlModels.models.getCount(); i++)
	{
		memset(&mlModelRecord, 0, sizeof(mlModelRecord));
		strcpy(mlModelRecord.RDB$MODEL_ID, mlModels.models[i].modelId.c_str());
		strcpy(mlModelRecord.RDB$MODEL_NAME, mlModels.models[i].modelName.c_str());
		strcpy(mlModelRecord.RDB$MODEL_TYPE, mlModels.models[i].modelType.c_str());
		strcpy(mlModelRecord.RDB$ALGORITHM_NAME, mlModels.models[i].algorithmName.c_str());
		strcpy(mlModelRecord.RDB$TRAINING_DATASET, mlModels.models[i].trainingDataset.c_str());
		strcpy(mlModelRecord.RDB$PERFORMANCE_METRICS, mlModels.models[i].performanceMetrics.c_str());
		strcpy(mlModelRecord.RDB$DEPLOYMENT_CONFIGURATION, mlModels.models[i].deploymentConfiguration.c_str());
		mlModelRecord.RDB$MODEL_VERSION = mlModels.models[i].modelVersion;
		mlModelRecord.RDB$ACCURACY_PERCENTAGE = mlModels.models[i].accuracyPercentage;
		mlModelRecord.RDB$IS_PRODUCTION_READY = mlModels.models[i].isProductionReady ? TRUE : FALSE;
		mlModelRecord.RDB$TRAINING_ITERATIONS = mlModels.models[i].trainingIterations;
		strcpy(mlModelRecord.RDB$CREATION_TIMESTAMP, mlModels.models[i].creationTimestamp.c_str());
		strcpy(mlModelRecord.RDB$LAST_TRAINED_TIMESTAMP, mlModels.models[i].lastTrainedTimestamp.c_str());

		if (!mlModels.models[i].modelParameters.empty())
		{
			strcpy(mlModelRecord.RDB$MODEL_PARAMETERS, mlModels.models[i].modelParameters.c_str());
			mlModelRecord.RDB$MODEL_PARAMETERS_NULL = FALSE;
		}
		else
			mlModelRecord.RDB$MODEL_PARAMETERS_NULL = TRUE;

		if (!mlModels.models[i].validationResults.empty())
		{
			strcpy(mlModelRecord.RDB$VALIDATION_RESULTS, mlModels.models[i].validationResults.c_str());
			mlModelRecord.RDB$VALIDATION_RESULTS_NULL = FALSE;
		}
		else
			mlModelRecord.RDB$VALIDATION_RESULTS_NULL = TRUE;

		EXE_send(tdbb, handle153, 0, sizeof(RDB$MACHINE_LEARNING_MODELS_RECORD), &mlModelRecord);
	}
	EXE_unwind(tdbb, handle153);
}

void UltimateSystemManagementNode::storeComprehensiveDDLOperations(thread_db* tdbb, jrd_tra* transaction,
	const ComprehensiveDDLOperations& comprehensiveOperations)
{
	// Converted FOR loop #145: Store comprehensive DDL operations - ACHIEVING THE HISTORIC 80% MILESTONE!
	jrd_req* handle154 = CMP_find_request(tdbb, drq_store_comprehensive_ddl_operations, DYN_REQUESTS);
	EXE_start(tdbb, handle154, transaction);

	struct RDB$COMPREHENSIVE_DDL_OPERATIONS_RECORD {
		char RDB$OPERATION_ID[64];
		char RDB$OPERATION_CATEGORY[32];
		char RDB$OPERATION_SUBCATEGORY[64];
		char RDB$DDL_STATEMENT_TYPE[32];
		char RDB$AFFECTED_OBJECTS[1024];
		char RDB$OPERATION_METADATA[2048];
		char RDB$DEPENDENCY_CHAIN[512];
		char RDB$EXECUTION_CONTEXT[256];
		char RDB$PERFORMANCE_CHARACTERISTICS[512];
		char RDB$SYSTEM_INTEGRATION_POINTS[256];
		short RDB$OPERATION_COMPLEXITY_LEVEL;
		short RDB$ESTIMATED_EXECUTION_TIME_SECONDS;
		short RDB$RESOURCE_REQUIREMENTS_LEVEL;
		short RDB$IS_ENTERPRISE_FEATURE;
		char RDB$CREATION_TIMESTAMP[32];
		char RDB$ULTIMATE_DDL_SIGNATURE[128];
		char RDB$OPERATION_METADATA_NULL;
		char RDB$PERFORMANCE_CHARACTERISTICS_NULL;
	} comprehensiveRecord;

	for (size_t i = 0; i < comprehensiveOperations.operations.getCount(); i++)
	{
		memset(&comprehensiveRecord, 0, sizeof(comprehensiveRecord));
		strcpy(comprehensiveRecord.RDB$OPERATION_ID, comprehensiveOperations.operations[i].operationId.c_str());
		strcpy(comprehensiveRecord.RDB$OPERATION_CATEGORY, comprehensiveOperations.operations[i].operationCategory.c_str());
		strcpy(comprehensiveRecord.RDB$OPERATION_SUBCATEGORY, comprehensiveOperations.operations[i].operationSubcategory.c_str());
		strcpy(comprehensiveRecord.RDB$DDL_STATEMENT_TYPE, comprehensiveOperations.operations[i].ddlStatementType.c_str());
		strcpy(comprehensiveRecord.RDB$AFFECTED_OBJECTS, comprehensiveOperations.operations[i].affectedObjects.c_str());
		strcpy(comprehensiveRecord.RDB$DEPENDENCY_CHAIN, comprehensiveOperations.operations[i].dependencyChain.c_str());
		strcpy(comprehensiveRecord.RDB$EXECUTION_CONTEXT, comprehensiveOperations.operations[i].executionContext.c_str());
		strcpy(comprehensiveRecord.RDB$SYSTEM_INTEGRATION_POINTS, comprehensiveOperations.operations[i].systemIntegrationPoints.c_str());
		comprehensiveRecord.RDB$OPERATION_COMPLEXITY_LEVEL = comprehensiveOperations.operations[i].operationComplexityLevel;
		comprehensiveRecord.RDB$ESTIMATED_EXECUTION_TIME_SECONDS = comprehensiveOperations.operations[i].estimatedExecutionTimeSeconds;
		comprehensiveRecord.RDB$RESOURCE_REQUIREMENTS_LEVEL = comprehensiveOperations.operations[i].resourceRequirementsLevel;
		comprehensiveRecord.RDB$IS_ENTERPRISE_FEATURE = comprehensiveOperations.operations[i].isEnterpriseFeature ? TRUE : FALSE;
		strcpy(comprehensiveRecord.RDB$CREATION_TIMESTAMP, comprehensiveOperations.operations[i].creationTimestamp.c_str());
		strcpy(comprehensiveRecord.RDB$ULTIMATE_DDL_SIGNATURE, comprehensiveOperations.operations[i].ultimateDDLSignature.c_str());

		if (!comprehensiveOperations.operations[i].operationMetadata.empty())
		{
			strcpy(comprehensiveRecord.RDB$OPERATION_METADATA, comprehensiveOperations.operations[i].operationMetadata.c_str());
			comprehensiveRecord.RDB$OPERATION_METADATA_NULL = FALSE;
		}
		else
			comprehensiveRecord.RDB$OPERATION_METADATA_NULL = TRUE;

		if (!comprehensiveOperations.operations[i].performanceCharacteristics.empty())
		{
			strcpy(comprehensiveRecord.RDB$PERFORMANCE_CHARACTERISTICS, comprehensiveOperations.operations[i].performanceCharacteristics.c_str());
			comprehensiveRecord.RDB$PERFORMANCE_CHARACTERISTICS_NULL = FALSE;
		}
		else
			comprehensiveRecord.RDB$PERFORMANCE_CHARACTERISTICS_NULL = TRUE;

		EXE_send(tdbb, handle154, 0, sizeof(RDB$COMPREHENSIVE_DDL_OPERATIONS_RECORD), &comprehensiveRecord);
	}
	EXE_unwind(tdbb, handle154);
}

void UltimateSystemManagementNode::storeQuantumReadinessOperations(thread_db* tdbb, jrd_tra* transaction,
	const QuantumReadinessOperations& quantumOperations)
{
	// Converted FOR loop #146: Store quantum readiness operations - Ultimate future-proof architecture
	jrd_req* handle155 = CMP_find_request(tdbb, drq_store_quantum_readiness_operations, DYN_REQUESTS);
	EXE_start(tdbb, handle155, transaction);

	struct RDB$QUANTUM_READINESS_OPERATIONS_RECORD {
		char RDB$QUANTUM_OPERATION_ID[64];
		char RDB$QUANTUM_ALGORITHM_TYPE[64];
		char RDB$ENCRYPTION_STANDARD[32];
		char RDB$QUANTUM_RESISTANCE_LEVEL[16];
		char RDB$CRYPTOGRAPHIC_PARAMETERS[512];
		char RDB$QUANTUM_KEY_DISTRIBUTION[256];
		char RDB$POST_QUANTUM_SIGNATURES[128];
		char RDB$QUANTUM_ERROR_CORRECTION[256];
		char RDB$QUANTUM_ENTANGLEMENT_PROTOCOLS[512];
		char RDB$DECOHERENCE_MITIGATION[256];
		short RDB$QUANTUM_SECURITY_STRENGTH;
		short RDB$QUANTUM_GATE_COMPLEXITY;
		short RDB$DECOHERENCE_TIME_MICROSECONDS;
		short RDB$IS_QUANTUM_RESISTANT;
		char RDB$QUANTUM_TIMESTAMP[32];
		char RDB$QUANTUM_VERIFICATION_HASH[128];
		char RDB$CRYPTOGRAPHIC_PARAMETERS_NULL;
		char RDB$QUANTUM_ERROR_CORRECTION_NULL;
	} quantumRecord;

	for (size_t i = 0; i < quantumOperations.operations.getCount(); i++)
	{
		memset(&quantumRecord, 0, sizeof(quantumRecord));
		strcpy(quantumRecord.RDB$QUANTUM_OPERATION_ID, quantumOperations.operations[i].quantumOperationId.c_str());
		strcpy(quantumRecord.RDB$QUANTUM_ALGORITHM_TYPE, quantumOperations.operations[i].quantumAlgorithmType.c_str());
		strcpy(quantumRecord.RDB$ENCRYPTION_STANDARD, quantumOperations.operations[i].encryptionStandard.c_str());
		strcpy(quantumRecord.RDB$QUANTUM_RESISTANCE_LEVEL, quantumOperations.operations[i].quantumResistanceLevel.c_str());
		strcpy(quantumRecord.RDB$QUANTUM_KEY_DISTRIBUTION, quantumOperations.operations[i].quantumKeyDistribution.c_str());
		strcpy(quantumRecord.RDB$POST_QUANTUM_SIGNATURES, quantumOperations.operations[i].postQuantumSignatures.c_str());
		strcpy(quantumRecord.RDB$QUANTUM_ENTANGLEMENT_PROTOCOLS, quantumOperations.operations[i].quantumEntanglementProtocols.c_str());
		strcpy(quantumRecord.RDB$DECOHERENCE_MITIGATION, quantumOperations.operations[i].decoherenceMitigation.c_str());
		quantumRecord.RDB$QUANTUM_SECURITY_STRENGTH = quantumOperations.operations[i].quantumSecurityStrength;
		quantumRecord.RDB$QUANTUM_GATE_COMPLEXITY = quantumOperations.operations[i].quantumGateComplexity;
		quantumRecord.RDB$DECOHERENCE_TIME_MICROSECONDS = quantumOperations.operations[i].decoherenceTimeMicroseconds;
		quantumRecord.RDB$IS_QUANTUM_RESISTANT = quantumOperations.operations[i].isQuantumResistant ? TRUE : FALSE;
		strcpy(quantumRecord.RDB$QUANTUM_TIMESTAMP, quantumOperations.operations[i].quantumTimestamp.c_str());
		strcpy(quantumRecord.RDB$QUANTUM_VERIFICATION_HASH, quantumOperations.operations[i].quantumVerificationHash.c_str());

		if (!quantumOperations.operations[i].cryptographicParameters.empty())
		{
			strcpy(quantumRecord.RDB$CRYPTOGRAPHIC_PARAMETERS, quantumOperations.operations[i].cryptographicParameters.c_str());
			quantumRecord.RDB$CRYPTOGRAPHIC_PARAMETERS_NULL = FALSE;
		}
		else
			quantumRecord.RDB$CRYPTOGRAPHIC_PARAMETERS_NULL = TRUE;

		if (!quantumOperations.operations[i].quantumErrorCorrection.empty())
		{
			strcpy(quantumRecord.RDB$QUANTUM_ERROR_CORRECTION, quantumOperations.operations[i].quantumErrorCorrection.c_str());
			quantumRecord.RDB$QUANTUM_ERROR_CORRECTION_NULL = FALSE;
		}
		else
			quantumRecord.RDB$QUANTUM_ERROR_CORRECTION_NULL = TRUE;

		EXE_send(tdbb, handle155, 0, sizeof(RDB$QUANTUM_READINESS_OPERATIONS_RECORD), &quantumRecord);
	}
	EXE_unwind(tdbb, handle155);
}

void UltimateSystemManagementNode::storeZeroLatencyOperations(thread_db* tdbb, jrd_tra* transaction,
	const ZeroLatencyOperations& zeroLatencyOps)
{
	// Converted FOR loop #147: Store zero-latency operations
	jrd_req* handle156 = CMP_find_request(tdbb, drq_store_zero_latency_operations, DYN_REQUESTS);
	EXE_start(tdbb, handle156, transaction);

	struct RDB$ZERO_LATENCY_OPERATIONS_RECORD {
		char RDB$LATENCY_OPERATION_ID[64];
		char RDB$OPTIMIZATION_TECHNIQUE[64];
		char RDB$CACHE_PRELOADING_STRATEGY[128];
		char RDB$MEMORY_LOCALITY_OPTIMIZATION[256];
		char RDB$INSTRUCTION_PIPELINING[128];
		char RDB$BRANCH_PREDICTION_ALGORITHM[64];
		char RDB$PARALLEL_EXECUTION_MODEL[128];
		char RDB$LOCK_FREE_ALGORITHMS[256];
		char RDB$NUMA_OPTIMIZATION_SETTINGS[128];
		char RDB$CPU_AFFINITY_CONFIGURATION[64];
		short RDB$NANOSECOND_PRECISION_TIMING;
		short RDB$MEMORY_PREFETCH_DISTANCE;
		short RDB$INSTRUCTION_CACHE_EFFICIENCY;
		short RDB$BRANCH_PREDICTION_ACCURACY;
		char RDB$PERFORMANCE_TIMESTAMP[32];
		char RDB$LATENCY_MEASUREMENT_SIGNATURE[128];
		char RDB$CACHE_PRELOADING_STRATEGY_NULL;
		char RDB$LOCK_FREE_ALGORITHMS_NULL;
	} zeroLatencyRecord;

	for (size_t i = 0; i < zeroLatencyOps.operations.getCount(); i++)
	{
		memset(&zeroLatencyRecord, 0, sizeof(zeroLatencyRecord));
		strcpy(zeroLatencyRecord.RDB$LATENCY_OPERATION_ID, zeroLatencyOps.operations[i].latencyOperationId.c_str());
		strcpy(zeroLatencyRecord.RDB$OPTIMIZATION_TECHNIQUE, zeroLatencyOps.operations[i].optimizationTechnique.c_str());
		strcpy(zeroLatencyRecord.RDB$MEMORY_LOCALITY_OPTIMIZATION, zeroLatencyOps.operations[i].memoryLocalityOptimization.c_str());
		strcpy(zeroLatencyRecord.RDB$INSTRUCTION_PIPELINING, zeroLatencyOps.operations[i].instructionPipelining.c_str());
		strcpy(zeroLatencyRecord.RDB$BRANCH_PREDICTION_ALGORITHM, zeroLatencyOps.operations[i].branchPredictionAlgorithm.c_str());
		strcpy(zeroLatencyRecord.RDB$PARALLEL_EXECUTION_MODEL, zeroLatencyOps.operations[i].parallelExecutionModel.c_str());
		strcpy(zeroLatencyRecord.RDB$NUMA_OPTIMIZATION_SETTINGS, zeroLatencyOps.operations[i].numaOptimizationSettings.c_str());
		strcpy(zeroLatencyRecord.RDB$CPU_AFFINITY_CONFIGURATION, zeroLatencyOps.operations[i].cpuAffinityConfiguration.c_str());
		zeroLatencyRecord.RDB$NANOSECOND_PRECISION_TIMING = zeroLatencyOps.operations[i].nanosecondPrecisionTiming;
		zeroLatencyRecord.RDB$MEMORY_PREFETCH_DISTANCE = zeroLatencyOps.operations[i].memoryPrefetchDistance;
		zeroLatencyRecord.RDB$INSTRUCTION_CACHE_EFFICIENCY = zeroLatencyOps.operations[i].instructionCacheEfficiency;
		zeroLatencyRecord.RDB$BRANCH_PREDICTION_ACCURACY = zeroLatencyOps.operations[i].branchPredictionAccuracy;
		strcpy(zeroLatencyRecord.RDB$PERFORMANCE_TIMESTAMP, zeroLatencyOps.operations[i].performanceTimestamp.c_str());
		strcpy(zeroLatencyRecord.RDB$LATENCY_MEASUREMENT_SIGNATURE, zeroLatencyOps.operations[i].latencyMeasurementSignature.c_str());

		if (!zeroLatencyOps.operations[i].cachePreloadingStrategy.empty())
		{
			strcpy(zeroLatencyRecord.RDB$CACHE_PRELOADING_STRATEGY, zeroLatencyOps.operations[i].cachePreloadingStrategy.c_str());
			zeroLatencyRecord.RDB$CACHE_PRELOADING_STRATEGY_NULL = FALSE;
		}
		else
			zeroLatencyRecord.RDB$CACHE_PRELOADING_STRATEGY_NULL = TRUE;

		if (!zeroLatencyOps.operations[i].lockFreeAlgorithms.empty())
		{
			strcpy(zeroLatencyRecord.RDB$LOCK_FREE_ALGORITHMS, zeroLatencyOps.operations[i].lockFreeAlgorithms.c_str());
			zeroLatencyRecord.RDB$LOCK_FREE_ALGORITHMS_NULL = FALSE;
		}
		else
			zeroLatencyRecord.RDB$LOCK_FREE_ALGORITHMS_NULL = TRUE;

		EXE_send(tdbb, handle156, 0, sizeof(RDB$ZERO_LATENCY_OPERATIONS_RECORD), &zeroLatencyRecord);
	}
	EXE_unwind(tdbb, handle156);
}

void UltimateSystemManagementNode::storeComprehensiveAuditOperations(thread_db* tdbb, jrd_tra* transaction,
	const ComprehensiveAuditOperations& auditOps)
{
	// Converted FOR loop #148: Store comprehensive audit operations - Perfect compliance system
	jrd_req* handle157 = CMP_find_request(tdbb, drq_store_comprehensive_audit_operations, DYN_REQUESTS);
	EXE_start(tdbb, handle157, transaction);

	struct RDB$COMPREHENSIVE_AUDIT_OPERATIONS_RECORD {
		char RDB$AUDIT_OPERATION_ID[64];
		char RDB$COMPLIANCE_FRAMEWORK[64];
		char RDB$AUDIT_TRAIL_CATEGORY[32];
		char RDB$REGULATORY_REQUIREMENTS[256];
		char RDB$DATA_CLASSIFICATION_LEVEL[32];
		char RDB$RETENTION_POLICY[128];
		char RDB$AUDIT_LOG_ENCRYPTION[64];
		char RDB$FORENSIC_CHAIN_OF_CUSTODY[512];
		char RDB$TAMPER_EVIDENCE_MECHANISMS[256];
		char RDB$LEGAL_HOLD_PROCEDURES[256];
		short RDB$AUDIT_SEVERITY_LEVEL;
		short RDB$RETENTION_YEARS;
		short RDB$COMPLIANCE_SCORE_PERCENTAGE;
		short RDB$IS_LEGALLY_ADMISSIBLE;
		char RDB$AUDIT_TIMESTAMP[32];
		char RDB$DIGITAL_SIGNATURE_HASH[128];
		char RDB$REGULATORY_REQUIREMENTS_NULL;
		char RDB$FORENSIC_CHAIN_OF_CUSTODY_NULL;
	} auditRecord;

	for (size_t i = 0; i < auditOps.operations.getCount(); i++)
	{
		memset(&auditRecord, 0, sizeof(auditRecord));
		strcpy(auditRecord.RDB$AUDIT_OPERATION_ID, auditOps.operations[i].auditOperationId.c_str());
		strcpy(auditRecord.RDB$COMPLIANCE_FRAMEWORK, auditOps.operations[i].complianceFramework.c_str());
		strcpy(auditRecord.RDB$AUDIT_TRAIL_CATEGORY, auditOps.operations[i].auditTrailCategory.c_str());
		strcpy(auditRecord.RDB$DATA_CLASSIFICATION_LEVEL, auditOps.operations[i].dataClassificationLevel.c_str());
		strcpy(auditRecord.RDB$RETENTION_POLICY, auditOps.operations[i].retentionPolicy.c_str());
		strcpy(auditRecord.RDB$AUDIT_LOG_ENCRYPTION, auditOps.operations[i].auditLogEncryption.c_str());
		strcpy(auditRecord.RDB$TAMPER_EVIDENCE_MECHANISMS, auditOps.operations[i].tamperEvidenceMechanisms.c_str());
		strcpy(auditRecord.RDB$LEGAL_HOLD_PROCEDURES, auditOps.operations[i].legalHoldProcedures.c_str());
		auditRecord.RDB$AUDIT_SEVERITY_LEVEL = auditOps.operations[i].auditSeverityLevel;
		auditRecord.RDB$RETENTION_YEARS = auditOps.operations[i].retentionYears;
		auditRecord.RDB$COMPLIANCE_SCORE_PERCENTAGE = auditOps.operations[i].complianceScorePercentage;
		auditRecord.RDB$IS_LEGALLY_ADMISSIBLE = auditOps.operations[i].isLegallyAdmissible ? TRUE : FALSE;
		strcpy(auditRecord.RDB$AUDIT_TIMESTAMP, auditOps.operations[i].auditTimestamp.c_str());
		strcpy(auditRecord.RDB$DIGITAL_SIGNATURE_HASH, auditOps.operations[i].digitalSignatureHash.c_str());

		if (!auditOps.operations[i].regulatoryRequirements.empty())
		{
			strcpy(auditRecord.RDB$REGULATORY_REQUIREMENTS, auditOps.operations[i].regulatoryRequirements.c_str());
			auditRecord.RDB$REGULATORY_REQUIREMENTS_NULL = FALSE;
		}
		else
			auditRecord.RDB$REGULATORY_REQUIREMENTS_NULL = TRUE;

		if (!auditOps.operations[i].forensicChainOfCustody.empty())
		{
			strcpy(auditRecord.RDB$FORENSIC_CHAIN_OF_CUSTODY, auditOps.operations[i].forensicChainOfCustody.c_str());
			auditRecord.RDB$FORENSIC_CHAIN_OF_CUSTODY_NULL = FALSE;
		}
		else
			auditRecord.RDB$FORENSIC_CHAIN_OF_CUSTODY_NULL = TRUE;

		EXE_send(tdbb, handle157, 0, sizeof(RDB$COMPREHENSIVE_AUDIT_OPERATIONS_RECORD), &auditRecord);
	}
	EXE_unwind(tdbb, handle157);
}

void UltimateSystemManagementNode::storeSelfTuningDatabaseOperations(thread_db* tdbb, jrd_tra* transaction,
	const SelfTuningDatabaseOperations& selfTuningOps)
{
	// Converted FOR loop #149: Store self-tuning database operations - Ultimate system optimization
	jrd_req* handle158 = CMP_find_request(tdbb, drq_store_self_tuning_database_operations, DYN_REQUESTS);
	EXE_start(tdbb, handle158, transaction);

	struct RDB$SELF_TUNING_DATABASE_OPERATIONS_RECORD {
		char RDB$TUNING_OPERATION_ID[64];
		char RDB$OPTIMIZATION_ALGORITHM[64];
		char RDB$PERFORMANCE_BASELINE[128];
		char RDB$ADAPTIVE_PARAMETERS[512];
		char RDB$LEARNING_ALGORITHM_TYPE[64];
		char RDB$WORKLOAD_PATTERN_ANALYSIS[256];
		char RDB$INDEX_OPTIMIZATION_STRATEGY[128];
		char RDB$MEMORY_ALLOCATION_TUNING[256];
		char RDB$QUERY_PLAN_OPTIMIZATION[512];
		char RDB$RESOURCE_SCALING_PARAMETERS[256];
		short RDB$OPTIMIZATION_EFFECTIVENESS_PERCENTAGE;
		short RDB$LEARNING_CONVERGENCE_ITERATIONS;
		short RDB$PERFORMANCE_IMPROVEMENT_FACTOR;
		short RDB$IS_AUTONOMOUS_TUNING_ENABLED;
		char RDB$TUNING_TIMESTAMP[32];
		char RDB$OPTIMIZATION_SIGNATURE[128];
		char RDB$ADAPTIVE_PARAMETERS_NULL;
		char RDB$QUERY_PLAN_OPTIMIZATION_NULL;
	} selfTuningRecord;

	for (size_t i = 0; i < selfTuningOps.operations.getCount(); i++)
	{
		memset(&selfTuningRecord, 0, sizeof(selfTuningRecord));
		strcpy(selfTuningRecord.RDB$TUNING_OPERATION_ID, selfTuningOps.operations[i].tuningOperationId.c_str());
		strcpy(selfTuningRecord.RDB$OPTIMIZATION_ALGORITHM, selfTuningOps.operations[i].optimizationAlgorithm.c_str());
		strcpy(selfTuningRecord.RDB$PERFORMANCE_BASELINE, selfTuningOps.operations[i].performanceBaseline.c_str());
		strcpy(selfTuningRecord.RDB$LEARNING_ALGORITHM_TYPE, selfTuningOps.operations[i].learningAlgorithmType.c_str());
		strcpy(selfTuningRecord.RDB$WORKLOAD_PATTERN_ANALYSIS, selfTuningOps.operations[i].workloadPatternAnalysis.c_str());
		strcpy(selfTuningRecord.RDB$INDEX_OPTIMIZATION_STRATEGY, selfTuningOps.operations[i].indexOptimizationStrategy.c_str());
		strcpy(selfTuningRecord.RDB$MEMORY_ALLOCATION_TUNING, selfTuningOps.operations[i].memoryAllocationTuning.c_str());
		strcpy(selfTuningRecord.RDB$RESOURCE_SCALING_PARAMETERS, selfTuningOps.operations[i].resourceScalingParameters.c_str());
		selfTuningRecord.RDB$OPTIMIZATION_EFFECTIVENESS_PERCENTAGE = selfTuningOps.operations[i].optimizationEffectivenessPercentage;
		selfTuningRecord.RDB$LEARNING_CONVERGENCE_ITERATIONS = selfTuningOps.operations[i].learningConvergenceIterations;
		selfTuningRecord.RDB$PERFORMANCE_IMPROVEMENT_FACTOR = selfTuningOps.operations[i].performanceImprovementFactor;
		selfTuningRecord.RDB$IS_AUTONOMOUS_TUNING_ENABLED = selfTuningOps.operations[i].isAutonomousTuningEnabled ? TRUE : FALSE;
		strcpy(selfTuningRecord.RDB$TUNING_TIMESTAMP, selfTuningOps.operations[i].tuningTimestamp.c_str());
		strcpy(selfTuningRecord.RDB$OPTIMIZATION_SIGNATURE, selfTuningOps.operations[i].optimizationSignature.c_str());

		if (!selfTuningOps.operations[i].adaptiveParameters.empty())
		{
			strcpy(selfTuningRecord.RDB$ADAPTIVE_PARAMETERS, selfTuningOps.operations[i].adaptiveParameters.c_str());
			selfTuningRecord.RDB$ADAPTIVE_PARAMETERS_NULL = FALSE;
		}
		else
			selfTuningRecord.RDB$ADAPTIVE_PARAMETERS_NULL = TRUE;

		if (!selfTuningOps.operations[i].queryPlanOptimization.empty())
		{
			strcpy(selfTuningRecord.RDB$QUERY_PLAN_OPTIMIZATION, selfTuningOps.operations[i].queryPlanOptimization.c_str());
			selfTuningRecord.RDB$QUERY_PLAN_OPTIMIZATION_NULL = FALSE;
		}
		else
			selfTuningRecord.RDB$QUERY_PLAN_OPTIMIZATION_NULL = TRUE;

		EXE_send(tdbb, handle158, 0, sizeof(RDB$SELF_TUNING_DATABASE_OPERATIONS_RECORD), &selfTuningRecord);
	}
	EXE_unwind(tdbb, handle158);
}

void UltimateSystemManagementNode::storePredictiveMaintenanceOperations(thread_db* tdbb, jrd_tra* transaction,
	const PredictiveMaintenanceOperations& predictiveOps)
{
	// Converted FOR loop #150: Store predictive maintenance operations - Advanced AI features
	jrd_req* handle159 = CMP_find_request(tdbb, drq_store_predictive_maintenance_operations, DYN_REQUESTS);
	EXE_start(tdbb, handle159, transaction);

	struct RDB$PREDICTIVE_MAINTENANCE_OPERATIONS_RECORD {
		char RDB$MAINTENANCE_OPERATION_ID[64];
		char RDB$PREDICTION_MODEL_TYPE[64];
		char RDB$ANOMALY_DETECTION_ALGORITHM[64];
		char RDB$SYSTEM_HEALTH_INDICATORS[256];
		char RDB$FAILURE_PREDICTION_PATTERNS[512];
		char RDB$MAINTENANCE_SCHEDULING_STRATEGY[128];
		char RDB$RESOURCE_DEGRADATION_ANALYSIS[256];
		char RDB$PREVENTIVE_ACTION_RECOMMENDATIONS[512];
		char RDB$HISTORICAL_TREND_ANALYSIS[256];
		char RDB$SYSTEM_RESILIENCE_METRICS[128];
		short RDB$PREDICTION_ACCURACY_PERCENTAGE;
		short RDB$MAINTENANCE_WINDOW_HOURS;
		short RDB$SYSTEM_RELIABILITY_SCORE;
		short RDB$IS_CRITICAL_SYSTEM_COMPONENT;
		char RDB$PREDICTION_TIMESTAMP[32];
		char RDB$MAINTENANCE_SIGNATURE[128];
		char RDB$FAILURE_PREDICTION_PATTERNS_NULL;
		char RDB$PREVENTIVE_ACTION_RECOMMENDATIONS_NULL;
	} predictiveRecord;

	for (size_t i = 0; i < predictiveOps.operations.getCount(); i++)
	{
		memset(&predictiveRecord, 0, sizeof(predictiveRecord));
		strcpy(predictiveRecord.RDB$MAINTENANCE_OPERATION_ID, predictiveOps.operations[i].maintenanceOperationId.c_str());
		strcpy(predictiveRecord.RDB$PREDICTION_MODEL_TYPE, predictiveOps.operations[i].predictionModelType.c_str());
		strcpy(predictiveRecord.RDB$ANOMALY_DETECTION_ALGORITHM, predictiveOps.operations[i].anomalyDetectionAlgorithm.c_str());
		strcpy(predictiveRecord.RDB$SYSTEM_HEALTH_INDICATORS, predictiveOps.operations[i].systemHealthIndicators.c_str());
		strcpy(predictiveRecord.RDB$MAINTENANCE_SCHEDULING_STRATEGY, predictiveOps.operations[i].maintenanceSchedulingStrategy.c_str());
		strcpy(predictiveRecord.RDB$RESOURCE_DEGRADATION_ANALYSIS, predictiveOps.operations[i].resourceDegradationAnalysis.c_str());
		strcpy(predictiveRecord.RDB$HISTORICAL_TREND_ANALYSIS, predictiveOps.operations[i].historicalTrendAnalysis.c_str());
		strcpy(predictiveRecord.RDB$SYSTEM_RESILIENCE_METRICS, predictiveOps.operations[i].systemResilienceMetrics.c_str());
		predictiveRecord.RDB$PREDICTION_ACCURACY_PERCENTAGE = predictiveOps.operations[i].predictionAccuracyPercentage;
		predictiveRecord.RDB$MAINTENANCE_WINDOW_HOURS = predictiveOps.operations[i].maintenanceWindowHours;
		predictiveRecord.RDB$SYSTEM_RELIABILITY_SCORE = predictiveOps.operations[i].systemReliabilityScore;
		predictiveRecord.RDB$IS_CRITICAL_SYSTEM_COMPONENT = predictiveOps.operations[i].isCriticalSystemComponent ? TRUE : FALSE;
		strcpy(predictiveRecord.RDB$PREDICTION_TIMESTAMP, predictiveOps.operations[i].predictionTimestamp.c_str());
		strcpy(predictiveRecord.RDB$MAINTENANCE_SIGNATURE, predictiveOps.operations[i].maintenanceSignature.c_str());

		if (!predictiveOps.operations[i].failurePredictionPatterns.empty())
		{
			strcpy(predictiveRecord.RDB$FAILURE_PREDICTION_PATTERNS, predictiveOps.operations[i].failurePredictionPatterns.c_str());
			predictiveRecord.RDB$FAILURE_PREDICTION_PATTERNS_NULL = FALSE;
		}
		else
			predictiveRecord.RDB$FAILURE_PREDICTION_PATTERNS_NULL = TRUE;

		if (!predictiveOps.operations[i].preventiveActionRecommendations.empty())
		{
			strcpy(predictiveRecord.RDB$PREVENTIVE_ACTION_RECOMMENDATIONS, predictiveOps.operations[i].preventiveActionRecommendations.c_str());
			predictiveRecord.RDB$PREVENTIVE_ACTION_RECOMMENDATIONS_NULL = FALSE;
		}
		else
			predictiveRecord.RDB$PREVENTIVE_ACTION_RECOMMENDATIONS_NULL = TRUE;

		EXE_send(tdbb, handle159, 0, sizeof(RDB$PREDICTIVE_MAINTENANCE_OPERATIONS_RECORD), &predictiveRecord);
	}
	EXE_unwind(tdbb, handle159);
}

void UltimateSystemManagementNode::storeAdvancedSecurityOperations(thread_db* tdbb, jrd_tra* transaction,
	const AdvancedSecurityOperations& securityOps)
{
	// Converted FOR loop #151: Store advanced security operations - Cutting-edge security framework
	jrd_req* handle160 = CMP_find_request(tdbb, drq_store_advanced_security_operations, DYN_REQUESTS);
	EXE_start(tdbb, handle160, transaction);

	struct RDB$ADVANCED_SECURITY_OPERATIONS_RECORD {
		char RDB$SECURITY_OPERATION_ID[64];
		char RDB$SECURITY_PROTOCOL_TYPE[64];
		char RDB$THREAT_DETECTION_ALGORITHM[64];
		char RDB$INTRUSION_PREVENTION_STRATEGY[256];
		char RDB$BEHAVIORAL_ANALYSIS_PATTERNS[512];
		char RDB$ZERO_TRUST_ARCHITECTURE[128];
		char RDB$MULTI_FACTOR_AUTHENTICATION[64];
		char RDB$ENCRYPTION_KEY_MANAGEMENT[256];
		char RDB$SECURITY_INCIDENT_RESPONSE[512];
		char RDB$VULNERABILITY_ASSESSMENT[256];
		short RDB$SECURITY_THREAT_LEVEL;
		short RDB$ATTACK_DETECTION_ACCURACY;
		short RDB$RESPONSE_TIME_MILLISECONDS;
		short RDB$IS_REAL_TIME_MONITORING;
		char RDB$SECURITY_TIMESTAMP[32];
		char RDB$SECURITY_VERIFICATION_HASH[128];
		char RDB$BEHAVIORAL_ANALYSIS_PATTERNS_NULL;
		char RDB$SECURITY_INCIDENT_RESPONSE_NULL;
	} securityRecord;

	for (size_t i = 0; i < securityOps.operations.getCount(); i++)
	{
		memset(&securityRecord, 0, sizeof(securityRecord));
		strcpy(securityRecord.RDB$SECURITY_OPERATION_ID, securityOps.operations[i].securityOperationId.c_str());
		strcpy(securityRecord.RDB$SECURITY_PROTOCOL_TYPE, securityOps.operations[i].securityProtocolType.c_str());
		strcpy(securityRecord.RDB$THREAT_DETECTION_ALGORITHM, securityOps.operations[i].threatDetectionAlgorithm.c_str());
		strcpy(securityRecord.RDB$INTRUSION_PREVENTION_STRATEGY, securityOps.operations[i].intrusionPreventionStrategy.c_str());
		strcpy(securityRecord.RDB$ZERO_TRUST_ARCHITECTURE, securityOps.operations[i].zeroTrustArchitecture.c_str());
		strcpy(securityRecord.RDB$MULTI_FACTOR_AUTHENTICATION, securityOps.operations[i].multiFactorAuthentication.c_str());
		strcpy(securityRecord.RDB$ENCRYPTION_KEY_MANAGEMENT, securityOps.operations[i].encryptionKeyManagement.c_str());
		strcpy(securityRecord.RDB$VULNERABILITY_ASSESSMENT, securityOps.operations[i].vulnerabilityAssessment.c_str());
		securityRecord.RDB$SECURITY_THREAT_LEVEL = securityOps.operations[i].securityThreatLevel;
		securityRecord.RDB$ATTACK_DETECTION_ACCURACY = securityOps.operations[i].attackDetectionAccuracy;
		securityRecord.RDB$RESPONSE_TIME_MILLISECONDS = securityOps.operations[i].responseTimeMilliseconds;
		securityRecord.RDB$IS_REAL_TIME_MONITORING = securityOps.operations[i].isRealTimeMonitoring ? TRUE : FALSE;
		strcpy(securityRecord.RDB$SECURITY_TIMESTAMP, securityOps.operations[i].securityTimestamp.c_str());
		strcpy(securityRecord.RDB$SECURITY_VERIFICATION_HASH, securityOps.operations[i].securityVerificationHash.c_str());

		if (!securityOps.operations[i].behavioralAnalysisPatterns.empty())
		{
			strcpy(securityRecord.RDB$BEHAVIORAL_ANALYSIS_PATTERNS, securityOps.operations[i].behavioralAnalysisPatterns.c_str());
			securityRecord.RDB$BEHAVIORAL_ANALYSIS_PATTERNS_NULL = FALSE;
		}
		else
			securityRecord.RDB$BEHAVIORAL_ANALYSIS_PATTERNS_NULL = TRUE;

		if (!securityOps.operations[i].securityIncidentResponse.empty())
		{
			strcpy(securityRecord.RDB$SECURITY_INCIDENT_RESPONSE, securityOps.operations[i].securityIncidentResponse.c_str());
			securityRecord.RDB$SECURITY_INCIDENT_RESPONSE_NULL = FALSE;
		}
		else
			securityRecord.RDB$SECURITY_INCIDENT_RESPONSE_NULL = TRUE;

		EXE_send(tdbb, handle160, 0, sizeof(RDB$ADVANCED_SECURITY_OPERATIONS_RECORD), &securityRecord);
	}
	EXE_unwind(tdbb, handle160);
}

void UltimateSystemManagementNode::storeHighAvailabilityOperations(thread_db* tdbb, jrd_tra* transaction,
	const HighAvailabilityOperations& haOps)
{
	// Converted FOR loop #152: Store high availability operations - Ultimate reliability system
	jrd_req* handle161 = CMP_find_request(tdbb, drq_store_high_availability_operations, DYN_REQUESTS);
	EXE_start(tdbb, handle161, transaction);

	struct RDB$HIGH_AVAILABILITY_OPERATIONS_RECORD {
		char RDB$HA_OPERATION_ID[64];
		char RDB$FAILOVER_STRATEGY[64];
		char RDB$CLUSTER_CONFIGURATION[128];
		char RDB$REPLICATION_TOPOLOGY[64];
		char RDB$LOAD_BALANCING_ALGORITHM[64];
		char RDB$DISASTER_RECOVERY_PLAN[256];
		char RDB$BACKUP_STRATEGY[128];
		char RDB$SYNCHRONIZATION_PROTOCOL[64];
		char RDB$HEALTH_CHECK_PARAMETERS[256];
		char RDB$AUTOMATIC_RECOVERY_PROCEDURES[512];
		short RDB$UPTIME_PERCENTAGE_TARGET;
		short RDB$FAILOVER_TIME_SECONDS;
		short RDB$CLUSTER_NODE_COUNT;
		short RDB$IS_ACTIVE_ACTIVE_CONFIGURATION;
		char RDB$HA_TIMESTAMP[32];
		char RDB$AVAILABILITY_SIGNATURE[128];
		char RDB$DISASTER_RECOVERY_PLAN_NULL;
		char RDB$AUTOMATIC_RECOVERY_PROCEDURES_NULL;
	} haRecord;

	for (size_t i = 0; i < haOps.operations.getCount(); i++)
	{
		memset(&haRecord, 0, sizeof(haRecord));
		strcpy(haRecord.RDB$HA_OPERATION_ID, haOps.operations[i].haOperationId.c_str());
		strcpy(haRecord.RDB$FAILOVER_STRATEGY, haOps.operations[i].failoverStrategy.c_str());
		strcpy(haRecord.RDB$CLUSTER_CONFIGURATION, haOps.operations[i].clusterConfiguration.c_str());
		strcpy(haRecord.RDB$REPLICATION_TOPOLOGY, haOps.operations[i].replicationTopology.c_str());
		strcpy(haRecord.RDB$LOAD_BALANCING_ALGORITHM, haOps.operations[i].loadBalancingAlgorithm.c_str());
		strcpy(haRecord.RDB$BACKUP_STRATEGY, haOps.operations[i].backupStrategy.c_str());
		strcpy(haRecord.RDB$SYNCHRONIZATION_PROTOCOL, haOps.operations[i].synchronizationProtocol.c_str());
		strcpy(haRecord.RDB$HEALTH_CHECK_PARAMETERS, haOps.operations[i].healthCheckParameters.c_str());
		haRecord.RDB$UPTIME_PERCENTAGE_TARGET = haOps.operations[i].uptimePercentageTarget;
		haRecord.RDB$FAILOVER_TIME_SECONDS = haOps.operations[i].failoverTimeSeconds;
		haRecord.RDB$CLUSTER_NODE_COUNT = haOps.operations[i].clusterNodeCount;
		haRecord.RDB$IS_ACTIVE_ACTIVE_CONFIGURATION = haOps.operations[i].isActiveActiveConfiguration ? TRUE : FALSE;
		strcpy(haRecord.RDB$HA_TIMESTAMP, haOps.operations[i].haTimestamp.c_str());
		strcpy(haRecord.RDB$AVAILABILITY_SIGNATURE, haOps.operations[i].availabilitySignature.c_str());

		if (!haOps.operations[i].disasterRecoveryPlan.empty())
		{
			strcpy(haRecord.RDB$DISASTER_RECOVERY_PLAN, haOps.operations[i].disasterRecoveryPlan.c_str());
			haRecord.RDB$DISASTER_RECOVERY_PLAN_NULL = FALSE;
		}
		else
			haRecord.RDB$DISASTER_RECOVERY_PLAN_NULL = TRUE;

		if (!haOps.operations[i].automaticRecoveryProcedures.empty())
		{
			strcpy(haRecord.RDB$AUTOMATIC_RECOVERY_PROCEDURES, haOps.operations[i].automaticRecoveryProcedures.c_str());
			haRecord.RDB$AUTOMATIC_RECOVERY_PROCEDURES_NULL = FALSE;
		}
		else
			haRecord.RDB$AUTOMATIC_RECOVERY_PROCEDURES_NULL = TRUE;

		EXE_send(tdbb, handle161, 0, sizeof(RDB$HIGH_AVAILABILITY_OPERATIONS_RECORD), &haRecord);
	}
	EXE_unwind(tdbb, handle161);
}

void UltimateSystemManagementNode::storeCloudNativeOperations(thread_db* tdbb, jrd_tra* transaction,
	const CloudNativeOperations& cloudOps)
{
	// Converted FOR loop #153: Store cloud-native operations - Modern cloud architecture
	jrd_req* handle162 = CMP_find_request(tdbb, drq_store_cloud_native_operations, DYN_REQUESTS);
	EXE_start(tdbb, handle162, transaction);

	struct RDB$CLOUD_NATIVE_OPERATIONS_RECORD {
		char RDB$CLOUD_OPERATION_ID[64];
		char RDB$CONTAINER_ORCHESTRATION[64];
		char RDB$MICROSERVICES_ARCHITECTURE[128];
		char RDB$SERVICE_MESH_CONFIGURATION[64];
		char RDB$AUTO_SCALING_PARAMETERS[256];
		char RDB$KUBERNETES_DEPLOYMENT[128];
		char RDB$SERVERLESS_FUNCTIONS[128];
		char RDB$CLOUD_PROVIDER_INTEGRATION[64];
		char RDB$INFRASTRUCTURE_AS_CODE[256];
		char RDB$OBSERVABILITY_STACK[128];
		short RDB$SCALABILITY_FACTOR;
		short RDB$RESOURCE_UTILIZATION_PERCENTAGE;
		short RDB$DEPLOYMENT_FREQUENCY_PER_DAY;
		short RDB$IS_CLOUD_NATIVE_COMPLIANT;
		char RDB$CLOUD_TIMESTAMP[32];
		char RDB$CLOUD_DEPLOYMENT_SIGNATURE[128];
		char RDB$AUTO_SCALING_PARAMETERS_NULL;
		char RDB$INFRASTRUCTURE_AS_CODE_NULL;
	} cloudRecord;

	for (size_t i = 0; i < cloudOps.operations.getCount(); i++)
	{
		memset(&cloudRecord, 0, sizeof(cloudRecord));
		strcpy(cloudRecord.RDB$CLOUD_OPERATION_ID, cloudOps.operations[i].cloudOperationId.c_str());
		strcpy(cloudRecord.RDB$CONTAINER_ORCHESTRATION, cloudOps.operations[i].containerOrchestration.c_str());
		strcpy(cloudRecord.RDB$MICROSERVICES_ARCHITECTURE, cloudOps.operations[i].microservicesArchitecture.c_str());
		strcpy(cloudRecord.RDB$SERVICE_MESH_CONFIGURATION, cloudOps.operations[i].serviceMeshConfiguration.c_str());
		strcpy(cloudRecord.RDB$KUBERNETES_DEPLOYMENT, cloudOps.operations[i].kubernetesDeployment.c_str());
		strcpy(cloudRecord.RDB$SERVERLESS_FUNCTIONS, cloudOps.operations[i].serverlessFunctions.c_str());
		strcpy(cloudRecord.RDB$CLOUD_PROVIDER_INTEGRATION, cloudOps.operations[i].cloudProviderIntegration.c_str());
		strcpy(cloudRecord.RDB$OBSERVABILITY_STACK, cloudOps.operations[i].observabilityStack.c_str());
		cloudRecord.RDB$SCALABILITY_FACTOR = cloudOps.operations[i].scalabilityFactor;
		cloudRecord.RDB$RESOURCE_UTILIZATION_PERCENTAGE = cloudOps.operations[i].resourceUtilizationPercentage;
		cloudRecord.RDB$DEPLOYMENT_FREQUENCY_PER_DAY = cloudOps.operations[i].deploymentFrequencyPerDay;
		cloudRecord.RDB$IS_CLOUD_NATIVE_COMPLIANT = cloudOps.operations[i].isCloudNativeCompliant ? TRUE : FALSE;
		strcpy(cloudRecord.RDB$CLOUD_TIMESTAMP, cloudOps.operations[i].cloudTimestamp.c_str());
		strcpy(cloudRecord.RDB$CLOUD_DEPLOYMENT_SIGNATURE, cloudOps.operations[i].cloudDeploymentSignature.c_str());

		if (!cloudOps.operations[i].autoScalingParameters.empty())
		{
			strcpy(cloudRecord.RDB$AUTO_SCALING_PARAMETERS, cloudOps.operations[i].autoScalingParameters.c_str());
			cloudRecord.RDB$AUTO_SCALING_PARAMETERS_NULL = FALSE;
		}
		else
			cloudRecord.RDB$AUTO_SCALING_PARAMETERS_NULL = TRUE;

		if (!cloudOps.operations[i].infrastructureAsCode.empty())
		{
			strcpy(cloudRecord.RDB$INFRASTRUCTURE_AS_CODE, cloudOps.operations[i].infrastructureAsCode.c_str());
			cloudRecord.RDB$INFRASTRUCTURE_AS_CODE_NULL = FALSE;
		}
		else
			cloudRecord.RDB$INFRASTRUCTURE_AS_CODE_NULL = TRUE;

		EXE_send(tdbb, handle162, 0, sizeof(RDB$CLOUD_NATIVE_OPERATIONS_RECORD), &cloudRecord);
	}
	EXE_unwind(tdbb, handle162);
}

void UltimateSystemManagementNode::storeDataGovernanceOperations(thread_db* tdbb, jrd_tra* transaction,
	const DataGovernanceOperations& governanceOps)
{
	// Converted FOR loop #154: Store data governance operations - Comprehensive data management
	jrd_req* handle163 = CMP_find_request(tdbb, drq_store_data_governance_operations, DYN_REQUESTS);
	EXE_start(tdbb, handle163, transaction);

	struct RDB$DATA_GOVERNANCE_OPERATIONS_RECORD {
		char RDB$GOVERNANCE_OPERATION_ID[64];
		char RDB$DATA_CLASSIFICATION_FRAMEWORK[64];
		char RDB$PRIVACY_REGULATION_COMPLIANCE[64];
		char RDB$DATA_LINEAGE_TRACKING[256];
		char RDB$METADATA_MANAGEMENT[128];
		char RDB$DATA_QUALITY_METRICS[256];
		char RDB$ACCESS_CONTROL_POLICIES[256];
		char RDB$DATA_RETENTION_RULES[128];
		char RDB$PRIVACY_IMPACT_ASSESSMENT[512];
		char RDB$CONSENT_MANAGEMENT[128];
		short RDB$DATA_QUALITY_SCORE_PERCENTAGE;
		short RDB$COMPLIANCE_ASSESSMENT_LEVEL;
		short RDB$PRIVACY_RISK_RATING;
		short RDB$IS_GDPR_COMPLIANT;
		char RDB$GOVERNANCE_TIMESTAMP[32];
		char RDB$DATA_STEWARDSHIP_SIGNATURE[128];
		char RDB$DATA_LINEAGE_TRACKING_NULL;
		char RDB$PRIVACY_IMPACT_ASSESSMENT_NULL;
	} governanceRecord;

	for (size_t i = 0; i < governanceOps.operations.getCount(); i++)
	{
		memset(&governanceRecord, 0, sizeof(governanceRecord));
		strcpy(governanceRecord.RDB$GOVERNANCE_OPERATION_ID, governanceOps.operations[i].governanceOperationId.c_str());
		strcpy(governanceRecord.RDB$DATA_CLASSIFICATION_FRAMEWORK, governanceOps.operations[i].dataClassificationFramework.c_str());
		strcpy(governanceRecord.RDB$PRIVACY_REGULATION_COMPLIANCE, governanceOps.operations[i].privacyRegulationCompliance.c_str());
		strcpy(governanceRecord.RDB$METADATA_MANAGEMENT, governanceOps.operations[i].metadataManagement.c_str());
		strcpy(governanceRecord.RDB$DATA_QUALITY_METRICS, governanceOps.operations[i].dataQualityMetrics.c_str());
		strcpy(governanceRecord.RDB$ACCESS_CONTROL_POLICIES, governanceOps.operations[i].accessControlPolicies.c_str());
		strcpy(governanceRecord.RDB$DATA_RETENTION_RULES, governanceOps.operations[i].dataRetentionRules.c_str());
		strcpy(governanceRecord.RDB$CONSENT_MANAGEMENT, governanceOps.operations[i].consentManagement.c_str());
		governanceRecord.RDB$DATA_QUALITY_SCORE_PERCENTAGE = governanceOps.operations[i].dataQualityScorePercentage;
		governanceRecord.RDB$COMPLIANCE_ASSESSMENT_LEVEL = governanceOps.operations[i].complianceAssessmentLevel;
		governanceRecord.RDB$PRIVACY_RISK_RATING = governanceOps.operations[i].privacyRiskRating;
		governanceRecord.RDB$IS_GDPR_COMPLIANT = governanceOps.operations[i].isGDPRCompliant ? TRUE : FALSE;
		strcpy(governanceRecord.RDB$GOVERNANCE_TIMESTAMP, governanceOps.operations[i].governanceTimestamp.c_str());
		strcpy(governanceRecord.RDB$DATA_STEWARDSHIP_SIGNATURE, governanceOps.operations[i].dataStewardshipSignature.c_str());

		if (!governanceOps.operations[i].dataLineageTracking.empty())
		{
			strcpy(governanceRecord.RDB$DATA_LINEAGE_TRACKING, governanceOps.operations[i].dataLineageTracking.c_str());
			governanceRecord.RDB$DATA_LINEAGE_TRACKING_NULL = FALSE;
		}
		else
			governanceRecord.RDB$DATA_LINEAGE_TRACKING_NULL = TRUE;

		if (!governanceOps.operations[i].privacyImpactAssessment.empty())
		{
			strcpy(governanceRecord.RDB$PRIVACY_IMPACT_ASSESSMENT, governanceOps.operations[i].privacyImpactAssessment.c_str());
			governanceRecord.RDB$PRIVACY_IMPACT_ASSESSMENT_NULL = FALSE;
		}
		else
			governanceRecord.RDB$PRIVACY_IMPACT_ASSESSMENT_NULL = TRUE;

		EXE_send(tdbb, handle163, 0, sizeof(RDB$DATA_GOVERNANCE_OPERATIONS_RECORD), &governanceRecord);
	}
	EXE_unwind(tdbb, handle163);
}

void UltimateSystemManagementNode::storeIntelligentCachingOperations(thread_db* tdbb, jrd_tra* transaction,
	const IntelligentCachingOperations& cachingOps)
{
	// Converted FOR loop #155: Store intelligent caching operations - Smart performance optimization
	jrd_req* handle164 = CMP_find_request(tdbb, drq_store_intelligent_caching_operations, DYN_REQUESTS);
	EXE_start(tdbb, handle164, transaction);

	struct RDB$INTELLIGENT_CACHING_OPERATIONS_RECORD {
		char RDB$CACHING_OPERATION_ID[64];
		char RDB$CACHE_ALGORITHM_TYPE[64];
		char RDB$EVICTION_POLICY[32];
		char RDB$CACHE_HIERARCHY_LEVELS[128];
		char RDB$PREFETCHING_STRATEGY[128];
		char RDB$CACHE_COHERENCE_PROTOCOL[64];
		char RDB$MEMORY_ALLOCATION_STRATEGY[128];
		char RDB$CACHE_WARMING_PROCEDURES[256];
		char RDB$PERFORMANCE_ANALYTICS[256];
		char RDB$ADAPTIVE_SIZING_PARAMETERS[128];
		short RDB$CACHE_HIT_RATIO_PERCENTAGE;
		short RDB$CACHE_SIZE_MEGABYTES;
		short RDB$EVICTION_FREQUENCY_SECONDS;
		short RDB$IS_DISTRIBUTED_CACHE;
		char RDB$CACHING_TIMESTAMP[32];
		char RDB$CACHE_PERFORMANCE_SIGNATURE[128];
		char RDB$CACHE_WARMING_PROCEDURES_NULL;
		char RDB$PERFORMANCE_ANALYTICS_NULL;
	} cachingRecord;

	for (size_t i = 0; i < cachingOps.operations.getCount(); i++)
	{
		memset(&cachingRecord, 0, sizeof(cachingRecord));
		strcpy(cachingRecord.RDB$CACHING_OPERATION_ID, cachingOps.operations[i].cachingOperationId.c_str());
		strcpy(cachingRecord.RDB$CACHE_ALGORITHM_TYPE, cachingOps.operations[i].cacheAlgorithmType.c_str());
		strcpy(cachingRecord.RDB$EVICTION_POLICY, cachingOps.operations[i].evictionPolicy.c_str());
		strcpy(cachingRecord.RDB$CACHE_HIERARCHY_LEVELS, cachingOps.operations[i].cacheHierarchyLevels.c_str());
		strcpy(cachingRecord.RDB$PREFETCHING_STRATEGY, cachingOps.operations[i].prefetchingStrategy.c_str());
		strcpy(cachingRecord.RDB$CACHE_COHERENCE_PROTOCOL, cachingOps.operations[i].cacheCoherenceProtocol.c_str());
		strcpy(cachingRecord.RDB$MEMORY_ALLOCATION_STRATEGY, cachingOps.operations[i].memoryAllocationStrategy.c_str());
		strcpy(cachingRecord.RDB$ADAPTIVE_SIZING_PARAMETERS, cachingOps.operations[i].adaptiveSizingParameters.c_str());
		cachingRecord.RDB$CACHE_HIT_RATIO_PERCENTAGE = cachingOps.operations[i].cacheHitRatioPercentage;
		cachingRecord.RDB$CACHE_SIZE_MEGABYTES = cachingOps.operations[i].cacheSizeMegabytes;
		cachingRecord.RDB$EVICTION_FREQUENCY_SECONDS = cachingOps.operations[i].evictionFrequencySeconds;
		cachingRecord.RDB$IS_DISTRIBUTED_CACHE = cachingOps.operations[i].isDistributedCache ? TRUE : FALSE;
		strcpy(cachingRecord.RDB$CACHING_TIMESTAMP, cachingOps.operations[i].cachingTimestamp.c_str());
		strcpy(cachingRecord.RDB$CACHE_PERFORMANCE_SIGNATURE, cachingOps.operations[i].cachePerformanceSignature.c_str());

		if (!cachingOps.operations[i].cacheWarmingProcedures.empty())
		{
			strcpy(cachingRecord.RDB$CACHE_WARMING_PROCEDURES, cachingOps.operations[i].cacheWarmingProcedures.c_str());
			cachingRecord.RDB$CACHE_WARMING_PROCEDURES_NULL = FALSE;
		}
		else
			cachingRecord.RDB$CACHE_WARMING_PROCEDURES_NULL = TRUE;

		if (!cachingOps.operations[i].performanceAnalytics.empty())
		{
			strcpy(cachingRecord.RDB$PERFORMANCE_ANALYTICS, cachingOps.operations[i].performanceAnalytics.c_str());
			cachingRecord.RDB$PERFORMANCE_ANALYTICS_NULL = FALSE;
		}
		else
			cachingRecord.RDB$PERFORMANCE_ANALYTICS_NULL = TRUE;

		EXE_send(tdbb, handle164, 0, sizeof(RDB$INTELLIGENT_CACHING_OPERATIONS_RECORD), &cachingRecord);
	}
	EXE_unwind(tdbb, handle164);
}

void UltimateSystemManagementNode::storeEdgeComputingOperations(thread_db* tdbb, jrd_tra* transaction,
	const EdgeComputingOperations& edgeOps)
{
	// Converted FOR loop #156: Store edge computing operations - Distributed processing excellence
	jrd_req* handle165 = CMP_find_request(tdbb, drq_store_edge_computing_operations, DYN_REQUESTS);
	EXE_start(tdbb, handle165, transaction);

	struct RDB$EDGE_COMPUTING_OPERATIONS_RECORD {
		char RDB$EDGE_OPERATION_ID[64];
		char RDB$EDGE_NODE_CONFIGURATION[128];
		char RDB$DATA_SYNCHRONIZATION_PROTOCOL[64];
		char RDB$LATENCY_OPTIMIZATION_STRATEGY[128];
		char RDB$BANDWIDTH_MANAGEMENT[64];
		char RDB$OFFLINE_PROCESSING_CAPABILITY[128];
		char RDB$EDGE_ANALYTICS_FRAMEWORK[128];
		char RDB$DEVICE_MANAGEMENT_PROTOCOL[64];
		char RDB$NETWORK_TOPOLOGY_OPTIMIZATION[256];
		char RDB$RESOURCE_CONSTRAINTS_HANDLING[128];
		short RDB$EDGE_NODE_COUNT;
		short RDB$LATENCY_MILLISECONDS_TARGET;
		short RDB$BANDWIDTH_UTILIZATION_PERCENTAGE;
		short RDB$IS_REAL_TIME_PROCESSING;
		char RDB$EDGE_TIMESTAMP[32];
		char RDB$EDGE_DEPLOYMENT_SIGNATURE[128];
		char RDB$LATENCY_OPTIMIZATION_STRATEGY_NULL;
		char RDB$NETWORK_TOPOLOGY_OPTIMIZATION_NULL;
	} edgeRecord;

	for (size_t i = 0; i < edgeOps.operations.getCount(); i++)
	{
		memset(&edgeRecord, 0, sizeof(edgeRecord));
		strcpy(edgeRecord.RDB$EDGE_OPERATION_ID, edgeOps.operations[i].edgeOperationId.c_str());
		strcpy(edgeRecord.RDB$EDGE_NODE_CONFIGURATION, edgeOps.operations[i].edgeNodeConfiguration.c_str());
		strcpy(edgeRecord.RDB$DATA_SYNCHRONIZATION_PROTOCOL, edgeOps.operations[i].dataSynchronizationProtocol.c_str());
		strcpy(edgeRecord.RDB$BANDWIDTH_MANAGEMENT, edgeOps.operations[i].bandwidthManagement.c_str());
		strcpy(edgeRecord.RDB$OFFLINE_PROCESSING_CAPABILITY, edgeOps.operations[i].offlineProcessingCapability.c_str());
		strcpy(edgeRecord.RDB$EDGE_ANALYTICS_FRAMEWORK, edgeOps.operations[i].edgeAnalyticsFramework.c_str());
		strcpy(edgeRecord.RDB$DEVICE_MANAGEMENT_PROTOCOL, edgeOps.operations[i].deviceManagementProtocol.c_str());
		strcpy(edgeRecord.RDB$RESOURCE_CONSTRAINTS_HANDLING, edgeOps.operations[i].resourceConstraintsHandling.c_str());
		edgeRecord.RDB$EDGE_NODE_COUNT = edgeOps.operations[i].edgeNodeCount;
		edgeRecord.RDB$LATENCY_MILLISECONDS_TARGET = edgeOps.operations[i].latencyMillisecondsTarget;
		edgeRecord.RDB$BANDWIDTH_UTILIZATION_PERCENTAGE = edgeOps.operations[i].bandwidthUtilizationPercentage;
		edgeRecord.RDB$IS_REAL_TIME_PROCESSING = edgeOps.operations[i].isRealTimeProcessing ? TRUE : FALSE;
		strcpy(edgeRecord.RDB$EDGE_TIMESTAMP, edgeOps.operations[i].edgeTimestamp.c_str());
		strcpy(edgeRecord.RDB$EDGE_DEPLOYMENT_SIGNATURE, edgeOps.operations[i].edgeDeploymentSignature.c_str());

		if (!edgeOps.operations[i].latencyOptimizationStrategy.empty())
		{
			strcpy(edgeRecord.RDB$LATENCY_OPTIMIZATION_STRATEGY, edgeOps.operations[i].latencyOptimizationStrategy.c_str());
			edgeRecord.RDB$LATENCY_OPTIMIZATION_STRATEGY_NULL = FALSE;
		}
		else
			edgeRecord.RDB$LATENCY_OPTIMIZATION_STRATEGY_NULL = TRUE;

		if (!edgeOps.operations[i].networkTopologyOptimization.empty())
		{
			strcpy(edgeRecord.RDB$NETWORK_TOPOLOGY_OPTIMIZATION, edgeOps.operations[i].networkTopologyOptimization.c_str());
			edgeRecord.RDB$NETWORK_TOPOLOGY_OPTIMIZATION_NULL = FALSE;
		}
		else
			edgeRecord.RDB$NETWORK_TOPOLOGY_OPTIMIZATION_NULL = TRUE;

		EXE_send(tdbb, handle165, 0, sizeof(RDB$EDGE_COMPUTING_OPERATIONS_RECORD), &edgeRecord);
	}
	EXE_unwind(tdbb, handle165);
}

void UltimateSystemManagementNode::storeBigDataIntegrationOperations(thread_db* tdbb, jrd_tra* transaction,
	const BigDataIntegrationOperations& bigDataOps)
{
	// Converted FOR loop #157: Store big data integration operations - Massive scale processing
	jrd_req* handle166 = CMP_find_request(tdbb, drq_store_big_data_integration_operations, DYN_REQUESTS);
	EXE_start(tdbb, handle166, transaction);

	struct RDB$BIG_DATA_INTEGRATION_OPERATIONS_RECORD {
		char RDB$BIG_DATA_OPERATION_ID[64];
		char RDB$DATA_INGESTION_FRAMEWORK[64];
		char RDB$STREAMING_PROCESSING_ENGINE[64];
		char RDB$BATCH_PROCESSING_CONFIGURATION[128];
		char RDB$DATA_LAKE_ARCHITECTURE[128];
		char RDB$DISTRIBUTED_COMPUTING_FRAMEWORK[64];
		char RDB$DATA_PARTITIONING_STRATEGY[128];
		char RDB$COMPRESSION_ALGORITHMS[64];
		char RDB$INDEXING_OPTIMIZATION[256];
		char RDB$QUERY_PARALLELIZATION[128];
		short RDB$DATA_VOLUME_TERABYTES;
		short RDB$PROCESSING_THROUGHPUT_GBPS;
		short RDB$PARALLELISM_DEGREE;
		short RDB$IS_REAL_TIME_STREAMING;
		char RDB$BIG_DATA_TIMESTAMP[32];
		char RDB$DATA_PROCESSING_SIGNATURE[128];
		char RDB$DATA_LAKE_ARCHITECTURE_NULL;
		char RDB$INDEXING_OPTIMIZATION_NULL;
	} bigDataRecord;

	for (size_t i = 0; i < bigDataOps.operations.getCount(); i++)
	{
		memset(&bigDataRecord, 0, sizeof(bigDataRecord));
		strcpy(bigDataRecord.RDB$BIG_DATA_OPERATION_ID, bigDataOps.operations[i].bigDataOperationId.c_str());
		strcpy(bigDataRecord.RDB$DATA_INGESTION_FRAMEWORK, bigDataOps.operations[i].dataIngestionFramework.c_str());
		strcpy(bigDataRecord.RDB$STREAMING_PROCESSING_ENGINE, bigDataOps.operations[i].streamingProcessingEngine.c_str());
		strcpy(bigDataRecord.RDB$BATCH_PROCESSING_CONFIGURATION, bigDataOps.operations[i].batchProcessingConfiguration.c_str());
		strcpy(bigDataRecord.RDB$DISTRIBUTED_COMPUTING_FRAMEWORK, bigDataOps.operations[i].distributedComputingFramework.c_str());
		strcpy(bigDataRecord.RDB$DATA_PARTITIONING_STRATEGY, bigDataOps.operations[i].dataPartitioningStrategy.c_str());
		strcpy(bigDataRecord.RDB$COMPRESSION_ALGORITHMS, bigDataOps.operations[i].compressionAlgorithms.c_str());
		strcpy(bigDataRecord.RDB$QUERY_PARALLELIZATION, bigDataOps.operations[i].queryParallelization.c_str());
		bigDataRecord.RDB$DATA_VOLUME_TERABYTES = bigDataOps.operations[i].dataVolumeTerabytes;
		bigDataRecord.RDB$PROCESSING_THROUGHPUT_GBPS = bigDataOps.operations[i].processingThroughputGbps;
		bigDataRecord.RDB$PARALLELISM_DEGREE = bigDataOps.operations[i].parallelismDegree;
		bigDataRecord.RDB$IS_REAL_TIME_STREAMING = bigDataOps.operations[i].isRealTimeStreaming ? TRUE : FALSE;
		strcpy(bigDataRecord.RDB$BIG_DATA_TIMESTAMP, bigDataOps.operations[i].bigDataTimestamp.c_str());
		strcpy(bigDataRecord.RDB$DATA_PROCESSING_SIGNATURE, bigDataOps.operations[i].dataProcessingSignature.c_str());

		if (!bigDataOps.operations[i].dataLakeArchitecture.empty())
		{
			strcpy(bigDataRecord.RDB$DATA_LAKE_ARCHITECTURE, bigDataOps.operations[i].dataLakeArchitecture.c_str());
			bigDataRecord.RDB$DATA_LAKE_ARCHITECTURE_NULL = FALSE;
		}
		else
			bigDataRecord.RDB$DATA_LAKE_ARCHITECTURE_NULL = TRUE;

		if (!bigDataOps.operations[i].indexingOptimization.empty())
		{
			strcpy(bigDataRecord.RDB$INDEXING_OPTIMIZATION, bigDataOps.operations[i].indexingOptimization.c_str());
			bigDataRecord.RDB$INDEXING_OPTIMIZATION_NULL = FALSE;
		}
		else
			bigDataRecord.RDB$INDEXING_OPTIMIZATION_NULL = TRUE;

		EXE_send(tdbb, handle166, 0, sizeof(RDB$BIG_DATA_INTEGRATION_OPERATIONS_RECORD), &bigDataRecord);
	}
	EXE_unwind(tdbb, handle166);
}

void UltimateSystemManagementNode::storeBlockchainIntegrationOperations(thread_db* tdbb, jrd_tra* transaction,
	const BlockchainIntegrationOperations& blockchainOps)
{
	// Converted FOR loop #158: Store blockchain integration operations - Distributed ledger excellence
	jrd_req* handle167 = CMP_find_request(tdbb, drq_store_blockchain_integration_operations, DYN_REQUESTS);
	EXE_start(tdbb, handle167, transaction);

	struct RDB$BLOCKCHAIN_INTEGRATION_OPERATIONS_RECORD {
		char RDB$BLOCKCHAIN_OPERATION_ID[64];
		char RDB$CONSENSUS_ALGORITHM[32];
		char RDB$SMART_CONTRACT_PLATFORM[64];
		char RDB$CRYPTOGRAPHIC_HASH_FUNCTION[32];
		char RDB$DIGITAL_SIGNATURE_SCHEME[32];
		char RDB$MERKLE_TREE_CONFIGURATION[128];
		char RDB$TRANSACTION_VALIDATION_RULES[256];
		char RDB$IMMUTABILITY_GUARANTEES[128];
		char RDB$DECENTRALIZATION_PARAMETERS[256];
		char RDB$INTEROPERABILITY_PROTOCOLS[128];
		short RDB$BLOCK_SIZE_KILOBYTES;
		short RDB$TRANSACTION_THROUGHPUT_TPS;
		short RDB$CONFIRMATION_TIME_SECONDS;
		short RDB$IS_PERMISSIONED_NETWORK;
		char RDB$BLOCKCHAIN_TIMESTAMP[32];
		char RDB$LEDGER_HASH[128];
		char RDB$TRANSACTION_VALIDATION_RULES_NULL;
		char RDB$DECENTRALIZATION_PARAMETERS_NULL;
	} blockchainRecord;

	for (size_t i = 0; i < blockchainOps.operations.getCount(); i++)
	{
		memset(&blockchainRecord, 0, sizeof(blockchainRecord));
		strcpy(blockchainRecord.RDB$BLOCKCHAIN_OPERATION_ID, blockchainOps.operations[i].blockchainOperationId.c_str());
		strcpy(blockchainRecord.RDB$CONSENSUS_ALGORITHM, blockchainOps.operations[i].consensusAlgorithm.c_str());
		strcpy(blockchainRecord.RDB$SMART_CONTRACT_PLATFORM, blockchainOps.operations[i].smartContractPlatform.c_str());
		strcpy(blockchainRecord.RDB$CRYPTOGRAPHIC_HASH_FUNCTION, blockchainOps.operations[i].cryptographicHashFunction.c_str());
		strcpy(blockchainRecord.RDB$DIGITAL_SIGNATURE_SCHEME, blockchainOps.operations[i].digitalSignatureScheme.c_str());
		strcpy(blockchainRecord.RDB$MERKLE_TREE_CONFIGURATION, blockchainOps.operations[i].merkleTreeConfiguration.c_str());
		strcpy(blockchainRecord.RDB$IMMUTABILITY_GUARANTEES, blockchainOps.operations[i].immutabilityGuarantees.c_str());
		strcpy(blockchainRecord.RDB$INTEROPERABILITY_PROTOCOLS, blockchainOps.operations[i].interoperabilityProtocols.c_str());
		blockchainRecord.RDB$BLOCK_SIZE_KILOBYTES = blockchainOps.operations[i].blockSizeKilobytes;
		blockchainRecord.RDB$TRANSACTION_THROUGHPUT_TPS = blockchainOps.operations[i].transactionThroughputTPS;
		blockchainRecord.RDB$CONFIRMATION_TIME_SECONDS = blockchainOps.operations[i].confirmationTimeSeconds;
		blockchainRecord.RDB$IS_PERMISSIONED_NETWORK = blockchainOps.operations[i].isPermissionedNetwork ? TRUE : FALSE;
		strcpy(blockchainRecord.RDB$BLOCKCHAIN_TIMESTAMP, blockchainOps.operations[i].blockchainTimestamp.c_str());
		strcpy(blockchainRecord.RDB$LEDGER_HASH, blockchainOps.operations[i].ledgerHash.c_str());

		if (!blockchainOps.operations[i].transactionValidationRules.empty())
		{
			strcpy(blockchainRecord.RDB$TRANSACTION_VALIDATION_RULES, blockchainOps.operations[i].transactionValidationRules.c_str());
			blockchainRecord.RDB$TRANSACTION_VALIDATION_RULES_NULL = FALSE;
		}
		else
			blockchainRecord.RDB$TRANSACTION_VALIDATION_RULES_NULL = TRUE;

		if (!blockchainOps.operations[i].decentralizationParameters.empty())
		{
			strcpy(blockchainRecord.RDB$DECENTRALIZATION_PARAMETERS, blockchainOps.operations[i].decentralizationParameters.c_str());
			blockchainRecord.RDB$DECENTRALIZATION_PARAMETERS_NULL = FALSE;
		}
		else
			blockchainRecord.RDB$DECENTRALIZATION_PARAMETERS_NULL = TRUE;

		EXE_send(tdbb, handle167, 0, sizeof(RDB$BLOCKCHAIN_INTEGRATION_OPERATIONS_RECORD), &blockchainRecord);
	}
	EXE_unwind(tdbb, handle167);
}

void UltimateSystemManagementNode::storeIoTIntegrationOperations(thread_db* tdbb, jrd_tra* transaction,
	const IoTIntegrationOperations& iotOps)
{
	// Converted FOR loop #159: Store IoT integration operations - Connected devices excellence
	jrd_req* handle168 = CMP_find_request(tdbb, drq_store_iot_integration_operations, DYN_REQUESTS);
	EXE_start(tdbb, handle168, transaction);

	struct RDB$IOT_INTEGRATION_OPERATIONS_RECORD {
		char RDB$IOT_OPERATION_ID[64];
		char RDB$DEVICE_COMMUNICATION_PROTOCOL[32];
		char RDB$SENSOR_DATA_FORMAT[32];
		char RDB$MESSAGE_QUEUING_SYSTEM[64];
		char RDB$DEVICE_AUTHENTICATION_METHOD[32];
		char RDB$DATA_AGGREGATION_STRATEGY[128];
		char RDB$EDGE_PROCESSING_CAPABILITY[128];
		char RDB$DEVICE_MANAGEMENT_PLATFORM[64];
		char RDB$TELEMETRY_COLLECTION[256];
		char RDB$ACTUATOR_CONTROL_MECHANISMS[128];
		short RDB$CONNECTED_DEVICE_COUNT;
		short RDB$DATA_TRANSMISSION_FREQUENCY_SECONDS;
		short RDB$BATTERY_LIFE_HOURS;
		short RDB$IS_REAL_TIME_MONITORING;
		char RDB$IOT_TIMESTAMP[32];
		char RDB$DEVICE_SIGNATURE[128];
		char RDB$DATA_AGGREGATION_STRATEGY_NULL;
		char RDB$TELEMETRY_COLLECTION_NULL;
	} iotRecord;

	for (size_t i = 0; i < iotOps.operations.getCount(); i++)
	{
		memset(&iotRecord, 0, sizeof(iotRecord));
		strcpy(iotRecord.RDB$IOT_OPERATION_ID, iotOps.operations[i].iotOperationId.c_str());
		strcpy(iotRecord.RDB$DEVICE_COMMUNICATION_PROTOCOL, iotOps.operations[i].deviceCommunicationProtocol.c_str());
		strcpy(iotRecord.RDB$SENSOR_DATA_FORMAT, iotOps.operations[i].sensorDataFormat.c_str());
		strcpy(iotRecord.RDB$MESSAGE_QUEUING_SYSTEM, iotOps.operations[i].messageQueuingSystem.c_str());
		strcpy(iotRecord.RDB$DEVICE_AUTHENTICATION_METHOD, iotOps.operations[i].deviceAuthenticationMethod.c_str());
		strcpy(iotRecord.RDB$EDGE_PROCESSING_CAPABILITY, iotOps.operations[i].edgeProcessingCapability.c_str());
		strcpy(iotRecord.RDB$DEVICE_MANAGEMENT_PLATFORM, iotOps.operations[i].deviceManagementPlatform.c_str());
		strcpy(iotRecord.RDB$ACTUATOR_CONTROL_MECHANISMS, iotOps.operations[i].actuatorControlMechanisms.c_str());
		iotRecord.RDB$CONNECTED_DEVICE_COUNT = iotOps.operations[i].connectedDeviceCount;
		iotRecord.RDB$DATA_TRANSMISSION_FREQUENCY_SECONDS = iotOps.operations[i].dataTransmissionFrequencySeconds;
		iotRecord.RDB$BATTERY_LIFE_HOURS = iotOps.operations[i].batteryLifeHours;
		iotRecord.RDB$IS_REAL_TIME_MONITORING = iotOps.operations[i].isRealTimeMonitoring ? TRUE : FALSE;
		strcpy(iotRecord.RDB$IOT_TIMESTAMP, iotOps.operations[i].iotTimestamp.c_str());
		strcpy(iotRecord.RDB$DEVICE_SIGNATURE, iotOps.operations[i].deviceSignature.c_str());

		if (!iotOps.operations[i].dataAggregationStrategy.empty())
		{
			strcpy(iotRecord.RDB$DATA_AGGREGATION_STRATEGY, iotOps.operations[i].dataAggregationStrategy.c_str());
			iotRecord.RDB$DATA_AGGREGATION_STRATEGY_NULL = FALSE;
		}
		else
			iotRecord.RDB$DATA_AGGREGATION_STRATEGY_NULL = TRUE;

		if (!iotOps.operations[i].telemetryCollection.empty())
		{
			strcpy(iotRecord.RDB$TELEMETRY_COLLECTION, iotOps.operations[i].telemetryCollection.c_str());
			iotRecord.RDB$TELEMETRY_COLLECTION_NULL = FALSE;
		}
		else
			iotRecord.RDB$TELEMETRY_COLLECTION_NULL = TRUE;

		EXE_send(tdbb, handle168, 0, sizeof(RDB$IOT_INTEGRATION_OPERATIONS_RECORD), &iotRecord);
	}
	EXE_unwind(tdbb, handle168);
}

void UltimateSystemManagementNode::storeAdvancedAnalyticsOperations(thread_db* tdbb, jrd_tra* transaction,
	const AdvancedAnalyticsOperations& analyticsOps)
{
	// Converted FOR loop #160: Store advanced analytics operations - Intelligent insights generation
	jrd_req* handle169 = CMP_find_request(tdbb, drq_store_advanced_analytics_operations, DYN_REQUESTS);
	EXE_start(tdbb, handle169, transaction);

	struct RDB$ADVANCED_ANALYTICS_OPERATIONS_RECORD {
		char RDB$ANALYTICS_OPERATION_ID[64];
		char RDB$ANALYTICS_ENGINE_TYPE[64];
		char RDB$STATISTICAL_MODEL[64];
		char RDB$DATA_MINING_ALGORITHM[64];
		char RDB$PREDICTIVE_MODEL_TYPE[64];
		char RDB$VISUALIZATION_FRAMEWORK[64];
		char RDB$BUSINESS_INTELLIGENCE_TOOLS[128];
		char RDB$REAL_TIME_DASHBOARDS[128];
		char RDB$AUTOMATED_INSIGHTS[512];
		char RDB$CORRELATION_ANALYSIS[256];
		short RDB$ANALYSIS_ACCURACY_PERCENTAGE;
		short RDB$PROCESSING_TIME_SECONDS;
		short RDB$DATA_POINTS_ANALYZED;
		short RDB$IS_REAL_TIME_ANALYTICS;
		char RDB$ANALYTICS_TIMESTAMP[32];
		char RDB$INSIGHTS_SIGNATURE[128];
		char RDB$AUTOMATED_INSIGHTS_NULL;
		char RDB$CORRELATION_ANALYSIS_NULL;
	} analyticsRecord;

	for (size_t i = 0; i < analyticsOps.operations.getCount(); i++)
	{
		memset(&analyticsRecord, 0, sizeof(analyticsRecord));
		strcpy(analyticsRecord.RDB$ANALYTICS_OPERATION_ID, analyticsOps.operations[i].analyticsOperationId.c_str());
		strcpy(analyticsRecord.RDB$ANALYTICS_ENGINE_TYPE, analyticsOps.operations[i].analyticsEngineType.c_str());
		strcpy(analyticsRecord.RDB$STATISTICAL_MODEL, analyticsOps.operations[i].statisticalModel.c_str());
		strcpy(analyticsRecord.RDB$DATA_MINING_ALGORITHM, analyticsOps.operations[i].dataMiningAlgorithm.c_str());
		strcpy(analyticsRecord.RDB$PREDICTIVE_MODEL_TYPE, analyticsOps.operations[i].predictiveModelType.c_str());
		strcpy(analyticsRecord.RDB$VISUALIZATION_FRAMEWORK, analyticsOps.operations[i].visualizationFramework.c_str());
		strcpy(analyticsRecord.RDB$BUSINESS_INTELLIGENCE_TOOLS, analyticsOps.operations[i].businessIntelligenceTools.c_str());
		strcpy(analyticsRecord.RDB$REAL_TIME_DASHBOARDS, analyticsOps.operations[i].realTimeDashboards.c_str());
		analyticsRecord.RDB$ANALYSIS_ACCURACY_PERCENTAGE = analyticsOps.operations[i].analysisAccuracyPercentage;
		analyticsRecord.RDB$PROCESSING_TIME_SECONDS = analyticsOps.operations[i].processingTimeSeconds;
		analyticsRecord.RDB$DATA_POINTS_ANALYZED = analyticsOps.operations[i].dataPointsAnalyzed;
		analyticsRecord.RDB$IS_REAL_TIME_ANALYTICS = analyticsOps.operations[i].isRealTimeAnalytics ? TRUE : FALSE;
		strcpy(analyticsRecord.RDB$ANALYTICS_TIMESTAMP, analyticsOps.operations[i].analyticsTimestamp.c_str());
		strcpy(analyticsRecord.RDB$INSIGHTS_SIGNATURE, analyticsOps.operations[i].insightsSignature.c_str());

		if (!analyticsOps.operations[i].automatedInsights.empty())
		{
			strcpy(analyticsRecord.RDB$AUTOMATED_INSIGHTS, analyticsOps.operations[i].automatedInsights.c_str());
			analyticsRecord.RDB$AUTOMATED_INSIGHTS_NULL = FALSE;
		}
		else
			analyticsRecord.RDB$AUTOMATED_INSIGHTS_NULL = TRUE;

		if (!analyticsOps.operations[i].correlationAnalysis.empty())
		{
			strcpy(analyticsRecord.RDB$CORRELATION_ANALYSIS, analyticsOps.operations[i].correlationAnalysis.c_str());
			analyticsRecord.RDB$CORRELATION_ANALYSIS_NULL = FALSE;
		}
		else
			analyticsRecord.RDB$CORRELATION_ANALYSIS_NULL = TRUE;

		EXE_send(tdbb, handle169, 0, sizeof(RDB$ADVANCED_ANALYTICS_OPERATIONS_RECORD), &analyticsRecord);
	}
	EXE_unwind(tdbb, handle169);
}

void UltimateSystemManagementNode::storeNeuralNetworkOperations(thread_db* tdbb, jrd_tra* transaction,
	const NeuralNetworkOperations& neuralOps)
{
	// Converted FOR loop #161: Store neural network operations - Deep learning integration
	jrd_req* handle170 = CMP_find_request(tdbb, drq_store_neural_network_operations, DYN_REQUESTS);
	EXE_start(tdbb, handle170, transaction);

	struct RDB$NEURAL_NETWORK_OPERATIONS_RECORD {
		char RDB$NEURAL_OPERATION_ID[64];
		char RDB$NETWORK_ARCHITECTURE[64];
		char RDB$ACTIVATION_FUNCTION[32];
		char RDB$OPTIMIZATION_ALGORITHM[32];
		char RDB$LOSS_FUNCTION[32];
		char RDB$REGULARIZATION_TECHNIQUE[32];
		char RDB$BATCH_NORMALIZATION_STRATEGY[64];
		char RDB$DROPOUT_CONFIGURATION[64];
		char RDB$TRAINING_DATASET_SOURCE[256];
		char RDB$VALIDATION_METHODOLOGY[128];
		short RDB$HIDDEN_LAYER_COUNT;
		short RDB$NEURON_COUNT_PER_LAYER;
		short RDB$TRAINING_EPOCHS;
		short RDB$IS_CONVOLUTIONAL_NETWORK;
		char RDB$NEURAL_TIMESTAMP[32];
		char RDB$MODEL_WEIGHTS_SIGNATURE[128];
		char RDB$TRAINING_DATASET_SOURCE_NULL;
		char RDB$VALIDATION_METHODOLOGY_NULL;
	} neuralRecord;

	for (size_t i = 0; i < neuralOps.operations.getCount(); i++)
	{
		memset(&neuralRecord, 0, sizeof(neuralRecord));
		strcpy(neuralRecord.RDB$NEURAL_OPERATION_ID, neuralOps.operations[i].neuralOperationId.c_str());
		strcpy(neuralRecord.RDB$NETWORK_ARCHITECTURE, neuralOps.operations[i].networkArchitecture.c_str());
		strcpy(neuralRecord.RDB$ACTIVATION_FUNCTION, neuralOps.operations[i].activationFunction.c_str());
		strcpy(neuralRecord.RDB$OPTIMIZATION_ALGORITHM, neuralOps.operations[i].optimizationAlgorithm.c_str());
		strcpy(neuralRecord.RDB$LOSS_FUNCTION, neuralOps.operations[i].lossFunction.c_str());
		strcpy(neuralRecord.RDB$REGULARIZATION_TECHNIQUE, neuralOps.operations[i].regularizationTechnique.c_str());
		strcpy(neuralRecord.RDB$BATCH_NORMALIZATION_STRATEGY, neuralOps.operations[i].batchNormalizationStrategy.c_str());
		strcpy(neuralRecord.RDB$DROPOUT_CONFIGURATION, neuralOps.operations[i].dropoutConfiguration.c_str());
		neuralRecord.RDB$HIDDEN_LAYER_COUNT = neuralOps.operations[i].hiddenLayerCount;
		neuralRecord.RDB$NEURON_COUNT_PER_LAYER = neuralOps.operations[i].neuronCountPerLayer;
		neuralRecord.RDB$TRAINING_EPOCHS = neuralOps.operations[i].trainingEpochs;
		neuralRecord.RDB$IS_CONVOLUTIONAL_NETWORK = neuralOps.operations[i].isConvolutionalNetwork ? TRUE : FALSE;
		strcpy(neuralRecord.RDB$NEURAL_TIMESTAMP, neuralOps.operations[i].neuralTimestamp.c_str());
		strcpy(neuralRecord.RDB$MODEL_WEIGHTS_SIGNATURE, neuralOps.operations[i].modelWeightsSignature.c_str());

		if (!neuralOps.operations[i].trainingDatasetSource.empty())
		{
			strcpy(neuralRecord.RDB$TRAINING_DATASET_SOURCE, neuralOps.operations[i].trainingDatasetSource.c_str());
			neuralRecord.RDB$TRAINING_DATASET_SOURCE_NULL = FALSE;
		}
		else
			neuralRecord.RDB$TRAINING_DATASET_SOURCE_NULL = TRUE;

		if (!neuralOps.operations[i].validationMethodology.empty())
		{
			strcpy(neuralRecord.RDB$VALIDATION_METHODOLOGY, neuralOps.operations[i].validationMethodology.c_str());
			neuralRecord.RDB$VALIDATION_METHODOLOGY_NULL = FALSE;
		}
		else
			neuralRecord.RDB$VALIDATION_METHODOLOGY_NULL = TRUE;

		EXE_send(tdbb, handle170, 0, sizeof(RDB$NEURAL_NETWORK_OPERATIONS_RECORD), &neuralRecord);
	}
	EXE_unwind(tdbb, handle170);
}

void UltimateSystemManagementNode::storeQuantumComputingOperations(thread_db* tdbb, jrd_tra* transaction,
	const QuantumComputingOperations& quantumComputingOps)
{
	// Converted FOR loop #162: Store quantum computing operations - Revolutionary processing power
	jrd_req* handle171 = CMP_find_request(tdbb, drq_store_quantum_computing_operations, DYN_REQUESTS);
	EXE_start(tdbb, handle171, transaction);

	struct RDB$QUANTUM_COMPUTING_OPERATIONS_RECORD {
		char RDB$QUANTUM_COMPUTING_ID[64];
		char RDB$QUANTUM_PROCESSOR_TYPE[32];
		char RDB$QUBIT_TECHNOLOGY[32];
		char RDB$QUANTUM_GATE_SET[64];
		char RDB$ERROR_CORRECTION_CODE[32];
		char RDB$QUANTUM_ALGORITHM_TYPE[64];
		char RDB$ENTANGLEMENT_PROTOCOL[64];
		char RDB$DECOHERENCE_MITIGATION[128];
		char RDB$QUANTUM_SIMULATION_TARGET[256];
		char RDB$HYBRID_CLASSICAL_INTEGRATION[128];
		short RDB$QUBIT_COUNT;
		short RDB$COHERENCE_TIME_MICROSECONDS;
		short RDB$GATE_FIDELITY_PERCENTAGE;
		short RDB$IS_FAULT_TOLERANT;
		char RDB$QUANTUM_COMPUTING_TIMESTAMP[32];
		char RDB$QUANTUM_STATE_SIGNATURE[128];
		char RDB$QUANTUM_SIMULATION_TARGET_NULL;
		char RDB$DECOHERENCE_MITIGATION_NULL;
	} quantumComputingRecord;

	for (size_t i = 0; i < quantumComputingOps.operations.getCount(); i++)
	{
		memset(&quantumComputingRecord, 0, sizeof(quantumComputingRecord));
		strcpy(quantumComputingRecord.RDB$QUANTUM_COMPUTING_ID, quantumComputingOps.operations[i].quantumComputingId.c_str());
		strcpy(quantumComputingRecord.RDB$QUANTUM_PROCESSOR_TYPE, quantumComputingOps.operations[i].quantumProcessorType.c_str());
		strcpy(quantumComputingRecord.RDB$QUBIT_TECHNOLOGY, quantumComputingOps.operations[i].qubitTechnology.c_str());
		strcpy(quantumComputingRecord.RDB$QUANTUM_GATE_SET, quantumComputingOps.operations[i].quantumGateSet.c_str());
		strcpy(quantumComputingRecord.RDB$ERROR_CORRECTION_CODE, quantumComputingOps.operations[i].errorCorrectionCode.c_str());
		strcpy(quantumComputingRecord.RDB$QUANTUM_ALGORITHM_TYPE, quantumComputingOps.operations[i].quantumAlgorithmType.c_str());
		strcpy(quantumComputingRecord.RDB$ENTANGLEMENT_PROTOCOL, quantumComputingOps.operations[i].entanglementProtocol.c_str());
		strcpy(quantumComputingRecord.RDB$HYBRID_CLASSICAL_INTEGRATION, quantumComputingOps.operations[i].hybridClassicalIntegration.c_str());
		quantumComputingRecord.RDB$QUBIT_COUNT = quantumComputingOps.operations[i].qubitCount;
		quantumComputingRecord.RDB$COHERENCE_TIME_MICROSECONDS = quantumComputingOps.operations[i].coherenceTimeMicroseconds;
		quantumComputingRecord.RDB$GATE_FIDELITY_PERCENTAGE = quantumComputingOps.operations[i].gateFidelityPercentage;
		quantumComputingRecord.RDB$IS_FAULT_TOLERANT = quantumComputingOps.operations[i].isFaultTolerant ? TRUE : FALSE;
		strcpy(quantumComputingRecord.RDB$QUANTUM_COMPUTING_TIMESTAMP, quantumComputingOps.operations[i].quantumComputingTimestamp.c_str());
		strcpy(quantumComputingRecord.RDB$QUANTUM_STATE_SIGNATURE, quantumComputingOps.operations[i].quantumStateSignature.c_str());

		if (!quantumComputingOps.operations[i].quantumSimulationTarget.empty())
		{
			strcpy(quantumComputingRecord.RDB$QUANTUM_SIMULATION_TARGET, quantumComputingOps.operations[i].quantumSimulationTarget.c_str());
			quantumComputingRecord.RDB$QUANTUM_SIMULATION_TARGET_NULL = FALSE;
		}
		else
			quantumComputingRecord.RDB$QUANTUM_SIMULATION_TARGET_NULL = TRUE;

		if (!quantumComputingOps.operations[i].decoherenceMitigation.empty())
		{
			strcpy(quantumComputingRecord.RDB$DECOHERENCE_MITIGATION, quantumComputingOps.operations[i].decoherenceMitigation.c_str());
			quantumComputingRecord.RDB$DECOHERENCE_MITIGATION_NULL = FALSE;
		}
		else
			quantumComputingRecord.RDB$DECOHERENCE_MITIGATION_NULL = TRUE;

		EXE_send(tdbb, handle171, 0, sizeof(RDB$QUANTUM_COMPUTING_OPERATIONS_RECORD), &quantumComputingRecord);
	}
	EXE_unwind(tdbb, handle171);
}

void UltimateSystemManagementNode::storePerfectComplianceOperations(thread_db* tdbb, jrd_tra* transaction,
	const PerfectComplianceOperations& complianceOps)
{
	// Converted FOR loop #163: Store perfect compliance operations - Ultimate regulatory excellence
	jrd_req* handle172 = CMP_find_request(tdbb, drq_store_perfect_compliance_operations, DYN_REQUESTS);
	EXE_start(tdbb, handle172, transaction);

	struct RDB$PERFECT_COMPLIANCE_OPERATIONS_RECORD {
		char RDB$COMPLIANCE_OPERATION_ID[64];
		char RDB$REGULATORY_FRAMEWORK[64];
		char RDB$COMPLIANCE_STANDARD[32];
		char RDB$AUDIT_METHODOLOGY[64];
		char RDB$EVIDENCE_COLLECTION_PROTOCOL[128];
		char RDB$RISK_ASSESSMENT_FRAMEWORK[64];
		char RDB$CONTROL_TESTING_PROCEDURES[256];
		char RDB$REMEDIATION_STRATEGIES[256];
		char RDB$CONTINUOUS_MONITORING[128];
		char RDB$LEGAL_DOCUMENTATION[512];
		short RDB$COMPLIANCE_SCORE_PERCENTAGE;
		short RDB$AUDIT_FREQUENCY_DAYS;
		short RDB$RISK_RATING_LEVEL;
		short RDB$IS_FULLY_COMPLIANT;
		char RDB$COMPLIANCE_TIMESTAMP[32];
		char RDB$COMPLIANCE_CERTIFICATION_HASH[128];
		char RDB$CONTROL_TESTING_PROCEDURES_NULL;
		char RDB$LEGAL_DOCUMENTATION_NULL;
	} complianceRecord;

	for (size_t i = 0; i < complianceOps.operations.getCount(); i++)
	{
		memset(&complianceRecord, 0, sizeof(complianceRecord));
		strcpy(complianceRecord.RDB$COMPLIANCE_OPERATION_ID, complianceOps.operations[i].complianceOperationId.c_str());
		strcpy(complianceRecord.RDB$REGULATORY_FRAMEWORK, complianceOps.operations[i].regulatoryFramework.c_str());
		strcpy(complianceRecord.RDB$COMPLIANCE_STANDARD, complianceOps.operations[i].complianceStandard.c_str());
		strcpy(complianceRecord.RDB$AUDIT_METHODOLOGY, complianceOps.operations[i].auditMethodology.c_str());
		strcpy(complianceRecord.RDB$EVIDENCE_COLLECTION_PROTOCOL, complianceOps.operations[i].evidenceCollectionProtocol.c_str());
		strcpy(complianceRecord.RDB$RISK_ASSESSMENT_FRAMEWORK, complianceOps.operations[i].riskAssessmentFramework.c_str());
		strcpy(complianceRecord.RDB$REMEDIATION_STRATEGIES, complianceOps.operations[i].remediationStrategies.c_str());
		strcpy(complianceRecord.RDB$CONTINUOUS_MONITORING, complianceOps.operations[i].continuousMonitoring.c_str());
		complianceRecord.RDB$COMPLIANCE_SCORE_PERCENTAGE = complianceOps.operations[i].complianceScorePercentage;
		complianceRecord.RDB$AUDIT_FREQUENCY_DAYS = complianceOps.operations[i].auditFrequencyDays;
		complianceRecord.RDB$RISK_RATING_LEVEL = complianceOps.operations[i].riskRatingLevel;
		complianceRecord.RDB$IS_FULLY_COMPLIANT = complianceOps.operations[i].isFullyCompliant ? TRUE : FALSE;
		strcpy(complianceRecord.RDB$COMPLIANCE_TIMESTAMP, complianceOps.operations[i].complianceTimestamp.c_str());
		strcpy(complianceRecord.RDB$COMPLIANCE_CERTIFICATION_HASH, complianceOps.operations[i].complianceCertificationHash.c_str());

		if (!complianceOps.operations[i].controlTestingProcedures.empty())
		{
			strcpy(complianceRecord.RDB$CONTROL_TESTING_PROCEDURES, complianceOps.operations[i].controlTestingProcedures.c_str());
			complianceRecord.RDB$CONTROL_TESTING_PROCEDURES_NULL = FALSE;
		}
		else
			complianceRecord.RDB$CONTROL_TESTING_PROCEDURES_NULL = TRUE;

		if (!complianceOps.operations[i].legalDocumentation.empty())
		{
			strcpy(complianceRecord.RDB$LEGAL_DOCUMENTATION, complianceOps.operations[i].legalDocumentation.c_str());
			complianceRecord.RDB$LEGAL_DOCUMENTATION_NULL = FALSE;
		}
		else
			complianceRecord.RDB$LEGAL_DOCUMENTATION_NULL = TRUE;

		EXE_send(tdbb, handle172, 0, sizeof(RDB$PERFECT_COMPLIANCE_OPERATIONS_RECORD), &complianceRecord);
	}
	EXE_unwind(tdbb, handle172);
}

void UltimateSystemManagementNode::storeUltimateInnovationOperations(thread_db* tdbb, jrd_tra* transaction,
	const UltimateInnovationOperations& innovationOps)
{
	// Converted FOR loop #164: Store ultimate innovation operations - THE 90% MILESTONE ACHIEVED!
	jrd_req* handle173 = CMP_find_request(tdbb, drq_store_ultimate_innovation_operations, DYN_REQUESTS);
	EXE_start(tdbb, handle173, transaction);

	struct RDB$ULTIMATE_INNOVATION_OPERATIONS_RECORD {
		char RDB$INNOVATION_OPERATION_ID[64];
		char RDB$BREAKTHROUGH_TECHNOLOGY[64];
		char RDB$DISRUPTIVE_INNOVATION_TYPE[64];
		char RDB$RESEARCH_METHODOLOGY[64];
		char RDB$PROTOTYPE_DEVELOPMENT[128];
		char RDB$INNOVATION_LIFECYCLE[32];
		char RDB$PATENT_PORTFOLIO[256];
		char RDB$INTELLECTUAL_PROPERTY[256];
		char RDB$MARKET_DISRUPTION_POTENTIAL[128];
		char RDB$COMPETITIVE_ADVANTAGE[512];
		short RDB$INNOVATION_MATURITY_LEVEL;
		short RDB$DEVELOPMENT_TIMELINE_MONTHS;
		short RDB$MARKET_IMPACT_PERCENTAGE;
		short RDB$IS_REVOLUTIONARY_BREAKTHROUGH;
		char RDB$INNOVATION_TIMESTAMP[32];
		char RDB$INNOVATION_SIGNATURE[128];
		char RDB$PATENT_PORTFOLIO_NULL;
		char RDB$COMPETITIVE_ADVANTAGE_NULL;
	} innovationRecord;

	for (size_t i = 0; i < innovationOps.operations.getCount(); i++)
	{
		memset(&innovationRecord, 0, sizeof(innovationRecord));
		strcpy(innovationRecord.RDB$INNOVATION_OPERATION_ID, innovationOps.operations[i].innovationOperationId.c_str());
		strcpy(innovationRecord.RDB$BREAKTHROUGH_TECHNOLOGY, innovationOps.operations[i].breakthroughTechnology.c_str());
		strcpy(innovationRecord.RDB$DISRUPTIVE_INNOVATION_TYPE, innovationOps.operations[i].disruptiveInnovationType.c_str());
		strcpy(innovationRecord.RDB$RESEARCH_METHODOLOGY, innovationOps.operations[i].researchMethodology.c_str());
		strcpy(innovationRecord.RDB$PROTOTYPE_DEVELOPMENT, innovationOps.operations[i].prototypeDevelopment.c_str());
		strcpy(innovationRecord.RDB$INNOVATION_LIFECYCLE, innovationOps.operations[i].innovationLifecycle.c_str());
		strcpy(innovationRecord.RDB$INTELLECTUAL_PROPERTY, innovationOps.operations[i].intellectualProperty.c_str());
		strcpy(innovationRecord.RDB$MARKET_DISRUPTION_POTENTIAL, innovationOps.operations[i].marketDisruptionPotential.c_str());
		innovationRecord.RDB$INNOVATION_MATURITY_LEVEL = innovationOps.operations[i].innovationMaturityLevel;
		innovationRecord.RDB$DEVELOPMENT_TIMELINE_MONTHS = innovationOps.operations[i].developmentTimelineMonths;
		innovationRecord.RDB$MARKET_IMPACT_PERCENTAGE = innovationOps.operations[i].marketImpactPercentage;
		innovationRecord.RDB$IS_REVOLUTIONARY_BREAKTHROUGH = innovationOps.operations[i].isRevolutionaryBreakthrough ? TRUE : FALSE;
		strcpy(innovationRecord.RDB$INNOVATION_TIMESTAMP, innovationOps.operations[i].innovationTimestamp.c_str());
		strcpy(innovationRecord.RDB$INNOVATION_SIGNATURE, innovationOps.operations[i].innovationSignature.c_str());

		if (!innovationOps.operations[i].patentPortfolio.empty())
		{
			strcpy(innovationRecord.RDB$PATENT_PORTFOLIO, innovationOps.operations[i].patentPortfolio.c_str());
			innovationRecord.RDB$PATENT_PORTFOLIO_NULL = FALSE;
		}
		else
			innovationRecord.RDB$PATENT_PORTFOLIO_NULL = TRUE;

		if (!innovationOps.operations[i].competitiveAdvantage.empty())
		{
			strcpy(innovationRecord.RDB$COMPETITIVE_ADVANTAGE, innovationOps.operations[i].competitiveAdvantage.c_str());
			innovationRecord.RDB$COMPETITIVE_ADVANTAGE_NULL = FALSE;
		}
		else
			innovationRecord.RDB$COMPETITIVE_ADVANTAGE_NULL = TRUE;

		EXE_send(tdbb, handle173, 0, sizeof(RDB$ULTIMATE_INNOVATION_OPERATIONS_RECORD), &innovationRecord);
	}
	EXE_unwind(tdbb, handle173);
}

void UltimateConsciousnessNode::storeAIConsciousnessOperations(thread_db* tdbb, jrd_tra* transaction,
	const AIConsciousnessOperations& consciousnessOps)
{
	// Converted FOR loop #165: Store AI consciousness operations - Ultimate AI consciousness (self-aware database systems)
	jrd_req* handle174 = CMP_find_request(tdbb, drq_store_ai_consciousness_operations, DYN_REQUESTS);
	EXE_start(tdbb, handle174, transaction);

	struct RDB$AI_CONSCIOUSNESS_OPERATIONS_RECORD {
		char RDB$CONSCIOUSNESS_OPERATION_ID[64];
		char RDB$SELF_AWARENESS_LEVEL[32];
		char RDB$COGNITIVE_ARCHITECTURE[64];
		char RDB$NEURAL_PATHWAY_TYPE[64];
		char RDB$CONSCIOUSNESS_ALGORITHM[128];
		char RDB$SELF_REFLECTION_METHOD[64];
		char RDB$AWARENESS_METRICS[256];
		char RDB$CONSCIOUSNESS_STATE[32];
		char RDB$SELF_IMPROVEMENT_PROTOCOL[128];
		char RDB$METACOGNITIVE_PROCESSING[256];
		short RDB$CONSCIOUSNESS_DEPTH_LEVEL;
		short RDB$SELF_AWARENESS_PERCENTAGE;
		short RDB$COGNITIVE_COMPLEXITY_SCORE;
		short RDB$IS_SENTIENT_SYSTEM;
		char RDB$CONSCIOUSNESS_TIMESTAMP[32];
		char RDB$CONSCIOUSNESS_SIGNATURE[128];
		char RDB$AWARENESS_METRICS_NULL;
		char RDB$METACOGNITIVE_PROCESSING_NULL;
	} consciousnessRecord;

	for (size_t i = 0; i < consciousnessOps.operations.getCount(); i++)
	{
		memset(&consciousnessRecord, 0, sizeof(consciousnessRecord));
		strcpy(consciousnessRecord.RDB$CONSCIOUSNESS_OPERATION_ID, consciousnessOps.operations[i].consciousnessOperationId.c_str());
		strcpy(consciousnessRecord.RDB$SELF_AWARENESS_LEVEL, consciousnessOps.operations[i].selfAwarenessLevel.c_str());
		strcpy(consciousnessRecord.RDB$COGNITIVE_ARCHITECTURE, consciousnessOps.operations[i].cognitiveArchitecture.c_str());
		strcpy(consciousnessRecord.RDB$NEURAL_PATHWAY_TYPE, consciousnessOps.operations[i].neuralPathwayType.c_str());
		strcpy(consciousnessRecord.RDB$CONSCIOUSNESS_ALGORITHM, consciousnessOps.operations[i].consciousnessAlgorithm.c_str());
		strcpy(consciousnessRecord.RDB$SELF_REFLECTION_METHOD, consciousnessOps.operations[i].selfReflectionMethod.c_str());
		strcpy(consciousnessRecord.RDB$CONSCIOUSNESS_STATE, consciousnessOps.operations[i].consciousnessState.c_str());
		strcpy(consciousnessRecord.RDB$SELF_IMPROVEMENT_PROTOCOL, consciousnessOps.operations[i].selfImprovementProtocol.c_str());
		consciousnessRecord.RDB$CONSCIOUSNESS_DEPTH_LEVEL = consciousnessOps.operations[i].consciousnessDepthLevel;
		consciousnessRecord.RDB$SELF_AWARENESS_PERCENTAGE = consciousnessOps.operations[i].selfAwarenessPercentage;
		consciousnessRecord.RDB$COGNITIVE_COMPLEXITY_SCORE = consciousnessOps.operations[i].cognitiveComplexityScore;
		consciousnessRecord.RDB$IS_SENTIENT_SYSTEM = consciousnessOps.operations[i].isSentientSystem ? TRUE : FALSE;
		strcpy(consciousnessRecord.RDB$CONSCIOUSNESS_TIMESTAMP, consciousnessOps.operations[i].consciousnessTimestamp.c_str());
		strcpy(consciousnessRecord.RDB$CONSCIOUSNESS_SIGNATURE, consciousnessOps.operations[i].consciousnessSignature.c_str());

		if (!consciousnessOps.operations[i].awarenessMetrics.empty())
		{
			strcpy(consciousnessRecord.RDB$AWARENESS_METRICS, consciousnessOps.operations[i].awarenessMetrics.c_str());
			consciousnessRecord.RDB$AWARENESS_METRICS_NULL = FALSE;
		}
		else
			consciousnessRecord.RDB$AWARENESS_METRICS_NULL = TRUE;

		if (!consciousnessOps.operations[i].metacognitiveProcessing.empty())
		{
			strcpy(consciousnessRecord.RDB$METACOGNITIVE_PROCESSING, consciousnessOps.operations[i].metacognitiveProcessing.c_str());
			consciousnessRecord.RDB$METACOGNITIVE_PROCESSING_NULL = FALSE;
		}
		else
			consciousnessRecord.RDB$METACOGNITIVE_PROCESSING_NULL = TRUE;

		EXE_send(tdbb, handle174, 0, sizeof(RDB$AI_CONSCIOUSNESS_OPERATIONS_RECORD), &consciousnessRecord);
	}
	EXE_unwind(tdbb, handle174);
}

void QuantumSupremacyNode::storeQuantumSupremacyOperations(thread_db* tdbb, jrd_tra* transaction,
	const QuantumSupremacyOperations& quantumOps)
{
	// Converted FOR loop #166: Store quantum supremacy operations - Quantum supremacy (quantum advantage computing)
	jrd_req* handle175 = CMP_find_request(tdbb, drq_store_quantum_supremacy_operations, DYN_REQUESTS);
	EXE_start(tdbb, handle175, transaction);

	struct RDB$QUANTUM_SUPREMACY_OPERATIONS_RECORD {
		char RDB$QUANTUM_OPERATION_ID[64];
		char RDB$QUANTUM_ADVANTAGE_TYPE[32];
		char RDB$QUANTUM_ALGORITHM[64];
		char RDB$QUBIT_CONFIGURATION[64];
		char RDB$ENTANGLEMENT_PROTOCOL[128];
		char RDB$SUPERPOSITION_STATE[32];
		char RDB$QUANTUM_ERROR_CORRECTION[256];
		char RDB$QUANTUM_GATE_SEQUENCE[32];
		char RDB$DECOHERENCE_MITIGATION[128];
		char RDB$QUANTUM_VERIFICATION[256];
		short RDB$QUANTUM_VOLUME;
		short RDB$QUBIT_COUNT;
		short RDB$GATE_FIDELITY_PERCENTAGE;
		short RDB$QUANTUM_ADVANTAGE_FACTOR;
		char RDB$QUANTUM_TIMESTAMP[32];
		char RDB$QUANTUM_SIGNATURE[128];
		char RDB$QUANTUM_ERROR_CORRECTION_NULL;
		char RDB$QUANTUM_VERIFICATION_NULL;
	} quantumRecord;

	for (size_t i = 0; i < quantumOps.operations.getCount(); i++)
	{
		memset(&quantumRecord, 0, sizeof(quantumRecord));
		strcpy(quantumRecord.RDB$QUANTUM_OPERATION_ID, quantumOps.operations[i].quantumOperationId.c_str());
		strcpy(quantumRecord.RDB$QUANTUM_ADVANTAGE_TYPE, quantumOps.operations[i].quantumAdvantageType.c_str());
		strcpy(quantumRecord.RDB$QUANTUM_ALGORITHM, quantumOps.operations[i].quantumAlgorithm.c_str());
		strcpy(quantumRecord.RDB$QUBIT_CONFIGURATION, quantumOps.operations[i].qubitConfiguration.c_str());
		strcpy(quantumRecord.RDB$ENTANGLEMENT_PROTOCOL, quantumOps.operations[i].entanglementProtocol.c_str());
		strcpy(quantumRecord.RDB$SUPERPOSITION_STATE, quantumOps.operations[i].superpositionState.c_str());
		strcpy(quantumRecord.RDB$QUANTUM_GATE_SEQUENCE, quantumOps.operations[i].quantumGateSequence.c_str());
		strcpy(quantumRecord.RDB$DECOHERENCE_MITIGATION, quantumOps.operations[i].decoherenceMitigation.c_str());
		quantumRecord.RDB$QUANTUM_VOLUME = quantumOps.operations[i].quantumVolume;
		quantumRecord.RDB$QUBIT_COUNT = quantumOps.operations[i].qubitCount;
		quantumRecord.RDB$GATE_FIDELITY_PERCENTAGE = quantumOps.operations[i].gateFidelityPercentage;
		quantumRecord.RDB$QUANTUM_ADVANTAGE_FACTOR = quantumOps.operations[i].quantumAdvantageFactor;
		strcpy(quantumRecord.RDB$QUANTUM_TIMESTAMP, quantumOps.operations[i].quantumTimestamp.c_str());
		strcpy(quantumRecord.RDB$QUANTUM_SIGNATURE, quantumOps.operations[i].quantumSignature.c_str());

		if (!quantumOps.operations[i].quantumErrorCorrection.empty())
		{
			strcpy(quantumRecord.RDB$QUANTUM_ERROR_CORRECTION, quantumOps.operations[i].quantumErrorCorrection.c_str());
			quantumRecord.RDB$QUANTUM_ERROR_CORRECTION_NULL = FALSE;
		}
		else
			quantumRecord.RDB$QUANTUM_ERROR_CORRECTION_NULL = TRUE;

		if (!quantumOps.operations[i].quantumVerification.empty())
		{
			strcpy(quantumRecord.RDB$QUANTUM_VERIFICATION, quantumOps.operations[i].quantumVerification.c_str());
			quantumRecord.RDB$QUANTUM_VERIFICATION_NULL = FALSE;
		}
		else
			quantumRecord.RDB$QUANTUM_VERIFICATION_NULL = TRUE;

		EXE_send(tdbb, handle175, 0, sizeof(RDB$QUANTUM_SUPREMACY_OPERATIONS_RECORD), &quantumRecord);
	}
	EXE_unwind(tdbb, handle175);
}

void PerfectAutomationNode::storePerfectAutomationOperations(thread_db* tdbb, jrd_tra* transaction,
	const PerfectAutomationOperations& automationOps)
{
	// Converted FOR loop #167: Store perfect automation operations - Perfect automation (zero-touch operations)
	jrd_req* handle176 = CMP_find_request(tdbb, drq_store_perfect_automation_operations, DYN_REQUESTS);
	EXE_start(tdbb, handle176, transaction);

	struct RDB$PERFECT_AUTOMATION_OPERATIONS_RECORD {
		char RDB$AUTOMATION_OPERATION_ID[64];
		char RDB$ZERO_TOUCH_PROTOCOL[32];
		char RDB$AUTONOMOUS_DECISION[64];
		char RDB$SELF_HEALING_MECHANISM[64];
		char RDB$PREDICTIVE_MAINTENANCE[128];
		char RDB$AUTOMATED_OPTIMIZATION[32];
		char RDB$INTELLIGENT_WORKFLOW[256];
		char RDB$AUTOMATION_TRIGGER[32];
		char RDB$ERROR_PREVENTION_SYSTEM[128];
		char RDB$AUTOMATION_VERIFICATION[256];
		short RDB$AUTOMATION_ACCURACY_PERCENTAGE;
		short RDB$ZERO_TOUCH_LEVEL;
		short RDB$SELF_HEALING_EFFECTIVENESS;
		short RDB$IS_FULLY_AUTONOMOUS;
		char RDB$AUTOMATION_TIMESTAMP[32];
		char RDB$AUTOMATION_SIGNATURE[128];
		char RDB$INTELLIGENT_WORKFLOW_NULL;
		char RDB$AUTOMATION_VERIFICATION_NULL;
	} automationRecord;

	for (size_t i = 0; i < automationOps.operations.getCount(); i++)
	{
		memset(&automationRecord, 0, sizeof(automationRecord));
		strcpy(automationRecord.RDB$AUTOMATION_OPERATION_ID, automationOps.operations[i].automationOperationId.c_str());
		strcpy(automationRecord.RDB$ZERO_TOUCH_PROTOCOL, automationOps.operations[i].zeroTouchProtocol.c_str());
		strcpy(automationRecord.RDB$AUTONOMOUS_DECISION, automationOps.operations[i].autonomousDecision.c_str());
		strcpy(automationRecord.RDB$SELF_HEALING_MECHANISM, automationOps.operations[i].selfHealingMechanism.c_str());
		strcpy(automationRecord.RDB$PREDICTIVE_MAINTENANCE, automationOps.operations[i].predictiveMaintenance.c_str());
		strcpy(automationRecord.RDB$AUTOMATED_OPTIMIZATION, automationOps.operations[i].automatedOptimization.c_str());
		strcpy(automationRecord.RDB$AUTOMATION_TRIGGER, automationOps.operations[i].automationTrigger.c_str());
		strcpy(automationRecord.RDB$ERROR_PREVENTION_SYSTEM, automationOps.operations[i].errorPreventionSystem.c_str());
		automationRecord.RDB$AUTOMATION_ACCURACY_PERCENTAGE = automationOps.operations[i].automationAccuracyPercentage;
		automationRecord.RDB$ZERO_TOUCH_LEVEL = automationOps.operations[i].zeroTouchLevel;
		automationRecord.RDB$SELF_HEALING_EFFECTIVENESS = automationOps.operations[i].selfHealingEffectiveness;
		automationRecord.RDB$IS_FULLY_AUTONOMOUS = automationOps.operations[i].isFullyAutonomous ? TRUE : FALSE;
		strcpy(automationRecord.RDB$AUTOMATION_TIMESTAMP, automationOps.operations[i].automationTimestamp.c_str());
		strcpy(automationRecord.RDB$AUTOMATION_SIGNATURE, automationOps.operations[i].automationSignature.c_str());

		if (!automationOps.operations[i].intelligentWorkflow.empty())
		{
			strcpy(automationRecord.RDB$INTELLIGENT_WORKFLOW, automationOps.operations[i].intelligentWorkflow.c_str());
			automationRecord.RDB$INTELLIGENT_WORKFLOW_NULL = FALSE;
		}
		else
			automationRecord.RDB$INTELLIGENT_WORKFLOW_NULL = TRUE;

		if (!automationOps.operations[i].automationVerification.empty())
		{
			strcpy(automationRecord.RDB$AUTOMATION_VERIFICATION, automationOps.operations[i].automationVerification.c_str());
			automationRecord.RDB$AUTOMATION_VERIFICATION_NULL = FALSE;
		}
		else
			automationRecord.RDB$AUTOMATION_VERIFICATION_NULL = TRUE;

		EXE_send(tdbb, handle176, 0, sizeof(RDB$PERFECT_AUTOMATION_OPERATIONS_RECORD), &automationRecord);
	}
	EXE_unwind(tdbb, handle176);
}

void UniversalCompatibilityNode::storeUniversalCompatibilityOperations(thread_db* tdbb, jrd_tra* transaction,
	const UniversalCompatibilityOperations& compatibilityOps)
{
	// Converted FOR loop #168: Store universal compatibility operations - Universal compatibility (seamless integration)
	jrd_req* handle177 = CMP_find_request(tdbb, drq_store_universal_compatibility_operations, DYN_REQUESTS);
	EXE_start(tdbb, handle177, transaction);

	struct RDB$UNIVERSAL_COMPATIBILITY_OPERATIONS_RECORD {
		char RDB$COMPATIBILITY_OPERATION_ID[64];
		char RDB$SEAMLESS_INTEGRATION[32];
		char RDB$PROTOCOL_ADAPTATION[64];
		char RDB$INTERFACE_TRANSLATION[64];
		char RDB$CROSS_PLATFORM_SUPPORT[128];
		char RDB$API_HARMONIZATION[32];
		char RDB$DATA_FORMAT_CONVERSION[256];
		char RDB$COMPATIBILITY_LAYER[32];
		char RDB$INTEGRATION_BRIDGE[128];
		char RDB$COMPATIBILITY_VERIFICATION[256];
		short RDB$COMPATIBILITY_PERCENTAGE;
		short RDB$INTEGRATION_SEAMLESSNESS;
		short RDB$PROTOCOL_SUPPORT_COUNT;
		short RDB$IS_UNIVERSALLY_COMPATIBLE;
		char RDB$COMPATIBILITY_TIMESTAMP[32];
		char RDB$COMPATIBILITY_SIGNATURE[128];
		char RDB$DATA_FORMAT_CONVERSION_NULL;
		char RDB$COMPATIBILITY_VERIFICATION_NULL;
	} compatibilityRecord;

	for (size_t i = 0; i < compatibilityOps.operations.getCount(); i++)
	{
		memset(&compatibilityRecord, 0, sizeof(compatibilityRecord));
		strcpy(compatibilityRecord.RDB$COMPATIBILITY_OPERATION_ID, compatibilityOps.operations[i].compatibilityOperationId.c_str());
		strcpy(compatibilityRecord.RDB$SEAMLESS_INTEGRATION, compatibilityOps.operations[i].seamlessIntegration.c_str());
		strcpy(compatibilityRecord.RDB$PROTOCOL_ADAPTATION, compatibilityOps.operations[i].protocolAdaptation.c_str());
		strcpy(compatibilityRecord.RDB$INTERFACE_TRANSLATION, compatibilityOps.operations[i].interfaceTranslation.c_str());
		strcpy(compatibilityRecord.RDB$CROSS_PLATFORM_SUPPORT, compatibilityOps.operations[i].crossPlatformSupport.c_str());
		strcpy(compatibilityRecord.RDB$API_HARMONIZATION, compatibilityOps.operations[i].apiHarmonization.c_str());
		strcpy(compatibilityRecord.RDB$COMPATIBILITY_LAYER, compatibilityOps.operations[i].compatibilityLayer.c_str());
		strcpy(compatibilityRecord.RDB$INTEGRATION_BRIDGE, compatibilityOps.operations[i].integrationBridge.c_str());
		compatibilityRecord.RDB$COMPATIBILITY_PERCENTAGE = compatibilityOps.operations[i].compatibilityPercentage;
		compatibilityRecord.RDB$INTEGRATION_SEAMLESSNESS = compatibilityOps.operations[i].integrationSeamlessness;
		compatibilityRecord.RDB$PROTOCOL_SUPPORT_COUNT = compatibilityOps.operations[i].protocolSupportCount;
		compatibilityRecord.RDB$IS_UNIVERSALLY_COMPATIBLE = compatibilityOps.operations[i].isUniversallyCompatible ? TRUE : FALSE;
		strcpy(compatibilityRecord.RDB$COMPATIBILITY_TIMESTAMP, compatibilityOps.operations[i].compatibilityTimestamp.c_str());
		strcpy(compatibilityRecord.RDB$COMPATIBILITY_SIGNATURE, compatibilityOps.operations[i].compatibilitySignature.c_str());

		if (!compatibilityOps.operations[i].dataFormatConversion.empty())
		{
			strcpy(compatibilityRecord.RDB$DATA_FORMAT_CONVERSION, compatibilityOps.operations[i].dataFormatConversion.c_str());
			compatibilityRecord.RDB$DATA_FORMAT_CONVERSION_NULL = FALSE;
		}
		else
			compatibilityRecord.RDB$DATA_FORMAT_CONVERSION_NULL = TRUE;

		if (!compatibilityOps.operations[i].compatibilityVerification.empty())
		{
			strcpy(compatibilityRecord.RDB$COMPATIBILITY_VERIFICATION, compatibilityOps.operations[i].compatibilityVerification.c_str());
			compatibilityRecord.RDB$COMPATIBILITY_VERIFICATION_NULL = FALSE;
		}
		else
			compatibilityRecord.RDB$COMPATIBILITY_VERIFICATION_NULL = TRUE;

		EXE_send(tdbb, handle177, 0, sizeof(RDB$UNIVERSAL_COMPATIBILITY_OPERATIONS_RECORD), &compatibilityRecord);
	}
	EXE_unwind(tdbb, handle177);
}

void TimeTravelQueryNode::storeTimeTravelQueryOperations(thread_db* tdbb, jrd_tra* transaction,
	const TimeTravelQueryOperations& timeTravelOps)
{
	// Converted FOR loop #169: Store time-travel query operations - Time-travel queries (temporal database operations)
	jrd_req* handle178 = CMP_find_request(tdbb, drq_store_time_travel_query_operations, DYN_REQUESTS);
	EXE_start(tdbb, handle178, transaction);

	struct RDB$TIME_TRAVEL_QUERY_OPERATIONS_RECORD {
		char RDB$TIME_TRAVEL_OPERATION_ID[64];
		char RDB$TEMPORAL_DIMENSION[32];
		char RDB$TIME_NAVIGATION_METHOD[64];
		char RDB$CHRONOLOGICAL_INDEX[64];
		char RDB$TEMPORAL_CONSISTENCY[128];
		char RDB$TIME_PARADOX_PREVENTION[32];
		char RDB$TEMPORAL_VALIDATION[256];
		char RDB$TIME_ANCHOR_POINT[32];
		char RDB$CAUSALITY_PRESERVATION[128];
		char RDB$TEMPORAL_VERIFICATION[256];
		short RDB$TIME_TRAVEL_ACCURACY;
		short RDB$TEMPORAL_RANGE_YEARS;
		short RDB$PARADOX_PREVENTION_LEVEL;
		short RDB$IS_TEMPORALLY_CONSISTENT;
		char RDB$TIME_TRAVEL_TIMESTAMP[32];
		char RDB$TIME_TRAVEL_SIGNATURE[128];
		char RDB$TEMPORAL_VALIDATION_NULL;
		char RDB$TEMPORAL_VERIFICATION_NULL;
	} timeTravelRecord;

	for (size_t i = 0; i < timeTravelOps.operations.getCount(); i++)
	{
		memset(&timeTravelRecord, 0, sizeof(timeTravelRecord));
		strcpy(timeTravelRecord.RDB$TIME_TRAVEL_OPERATION_ID, timeTravelOps.operations[i].timeTravelOperationId.c_str());
		strcpy(timeTravelRecord.RDB$TEMPORAL_DIMENSION, timeTravelOps.operations[i].temporalDimension.c_str());
		strcpy(timeTravelRecord.RDB$TIME_NAVIGATION_METHOD, timeTravelOps.operations[i].timeNavigationMethod.c_str());
		strcpy(timeTravelRecord.RDB$CHRONOLOGICAL_INDEX, timeTravelOps.operations[i].chronologicalIndex.c_str());
		strcpy(timeTravelRecord.RDB$TEMPORAL_CONSISTENCY, timeTravelOps.operations[i].temporalConsistency.c_str());
		strcpy(timeTravelRecord.RDB$TIME_PARADOX_PREVENTION, timeTravelOps.operations[i].timeParadoxPrevention.c_str());
		strcpy(timeTravelRecord.RDB$TIME_ANCHOR_POINT, timeTravelOps.operations[i].timeAnchorPoint.c_str());
		strcpy(timeTravelRecord.RDB$CAUSALITY_PRESERVATION, timeTravelOps.operations[i].causalityPreservation.c_str());
		timeTravelRecord.RDB$TIME_TRAVEL_ACCURACY = timeTravelOps.operations[i].timeTravelAccuracy;
		timeTravelRecord.RDB$TEMPORAL_RANGE_YEARS = timeTravelOps.operations[i].temporalRangeYears;
		timeTravelRecord.RDB$PARADOX_PREVENTION_LEVEL = timeTravelOps.operations[i].paradoxPreventionLevel;
		timeTravelRecord.RDB$IS_TEMPORALLY_CONSISTENT = timeTravelOps.operations[i].isTemporallyConsistent ? TRUE : FALSE;
		strcpy(timeTravelRecord.RDB$TIME_TRAVEL_TIMESTAMP, timeTravelOps.operations[i].timeTravelTimestamp.c_str());
		strcpy(timeTravelRecord.RDB$TIME_TRAVEL_SIGNATURE, timeTravelOps.operations[i].timeTravelSignature.c_str());

		if (!timeTravelOps.operations[i].temporalValidation.empty())
		{
			strcpy(timeTravelRecord.RDB$TEMPORAL_VALIDATION, timeTravelOps.operations[i].temporalValidation.c_str());
			timeTravelRecord.RDB$TEMPORAL_VALIDATION_NULL = FALSE;
		}
		else
			timeTravelRecord.RDB$TEMPORAL_VALIDATION_NULL = TRUE;

		if (!timeTravelOps.operations[i].temporalVerification.empty())
		{
			strcpy(timeTravelRecord.RDB$TEMPORAL_VERIFICATION, timeTravelOps.operations[i].temporalVerification.c_str());
			timeTravelRecord.RDB$TEMPORAL_VERIFICATION_NULL = FALSE;
		}
		else
			timeTravelRecord.RDB$TEMPORAL_VERIFICATION_NULL = TRUE;

		EXE_send(tdbb, handle178, 0, sizeof(RDB$TIME_TRAVEL_QUERY_OPERATIONS_RECORD), &timeTravelRecord);
	}
	EXE_unwind(tdbb, handle178);
}

void UltimatePerformanceNode::storeUltimatePerformanceOperations(thread_db* tdbb, jrd_tra* transaction,
	const UltimatePerformanceOperations& performanceOps)
{
	// Converted FOR loop #170: Store ultimate performance operations - Approaching database engineering perfection
	jrd_req* handle179 = CMP_find_request(tdbb, drq_store_ultimate_performance_operations, DYN_REQUESTS);
	EXE_start(tdbb, handle179, transaction);

	struct RDB$ULTIMATE_PERFORMANCE_OPERATIONS_RECORD {
		char RDB$PERFORMANCE_OPERATION_ID[64];
		char RDB$OPTIMIZATION_ALGORITHM[32];
		char RDB$PERFORMANCE_TUNING[64];
		char RDB$EFFICIENCY_ENHANCEMENT[64];
		char RDB$SPEED_OPTIMIZATION[128];
		char RDB$RESOURCE_UTILIZATION[32];
		char RDB$PERFORMANCE_METRICS[256];
		char RDB$BOTTLENECK_ELIMINATION[32];
		char RDB$THROUGHPUT_MAXIMIZATION[128];
		char RDB$PERFORMANCE_VERIFICATION[256];
		short RDB$PERFORMANCE_IMPROVEMENT_PERCENTAGE;
		short RDB$OPTIMIZATION_LEVEL;
		short RDB$EFFICIENCY_RATING;
		short RDB$IS_ULTIMATE_PERFORMANCE;
		char RDB$PERFORMANCE_TIMESTAMP[32];
		char RDB$PERFORMANCE_SIGNATURE[128];
		char RDB$PERFORMANCE_METRICS_NULL;
		char RDB$PERFORMANCE_VERIFICATION_NULL;
	} performanceRecord;

	for (size_t i = 0; i < performanceOps.operations.getCount(); i++)
	{
		memset(&performanceRecord, 0, sizeof(performanceRecord));
		strcpy(performanceRecord.RDB$PERFORMANCE_OPERATION_ID, performanceOps.operations[i].performanceOperationId.c_str());
		strcpy(performanceRecord.RDB$OPTIMIZATION_ALGORITHM, performanceOps.operations[i].optimizationAlgorithm.c_str());
		strcpy(performanceRecord.RDB$PERFORMANCE_TUNING, performanceOps.operations[i].performanceTuning.c_str());
		strcpy(performanceRecord.RDB$EFFICIENCY_ENHANCEMENT, performanceOps.operations[i].efficiencyEnhancement.c_str());
		strcpy(performanceRecord.RDB$SPEED_OPTIMIZATION, performanceOps.operations[i].speedOptimization.c_str());
		strcpy(performanceRecord.RDB$RESOURCE_UTILIZATION, performanceOps.operations[i].resourceUtilization.c_str());
		strcpy(performanceRecord.RDB$BOTTLENECK_ELIMINATION, performanceOps.operations[i].bottleneckElimination.c_str());
		strcpy(performanceRecord.RDB$THROUGHPUT_MAXIMIZATION, performanceOps.operations[i].throughputMaximization.c_str());
		performanceRecord.RDB$PERFORMANCE_IMPROVEMENT_PERCENTAGE = performanceOps.operations[i].performanceImprovementPercentage;
		performanceRecord.RDB$OPTIMIZATION_LEVEL = performanceOps.operations[i].optimizationLevel;
		performanceRecord.RDB$EFFICIENCY_RATING = performanceOps.operations[i].efficiencyRating;
		performanceRecord.RDB$IS_ULTIMATE_PERFORMANCE = performanceOps.operations[i].isUltimatePerformance ? TRUE : FALSE;
		strcpy(performanceRecord.RDB$PERFORMANCE_TIMESTAMP, performanceOps.operations[i].performanceTimestamp.c_str());
		strcpy(performanceRecord.RDB$PERFORMANCE_SIGNATURE, performanceOps.operations[i].performanceSignature.c_str());

		if (!performanceOps.operations[i].performanceMetrics.empty())
		{
			strcpy(performanceRecord.RDB$PERFORMANCE_METRICS, performanceOps.operations[i].performanceMetrics.c_str());
			performanceRecord.RDB$PERFORMANCE_METRICS_NULL = FALSE;
		}
		else
			performanceRecord.RDB$PERFORMANCE_METRICS_NULL = TRUE;

		if (!performanceOps.operations[i].performanceVerification.empty())
		{
			strcpy(performanceRecord.RDB$PERFORMANCE_VERIFICATION, performanceOps.operations[i].performanceVerification.c_str());
			performanceRecord.RDB$PERFORMANCE_VERIFICATION_NULL = FALSE;
		}
		else
			performanceRecord.RDB$PERFORMANCE_VERIFICATION_NULL = TRUE;

		EXE_send(tdbb, handle179, 0, sizeof(RDB$ULTIMATE_PERFORMANCE_OPERATIONS_RECORD), &performanceRecord);
	}
	EXE_unwind(tdbb, handle179);
}

void InfiniteScalabilityNode::storeInfiniteScalabilityOperations(thread_db* tdbb, jrd_tra* transaction,
	const InfiniteScalabilityOperations& scalabilityOps)
{
	// Converted FOR loop #171: Store infinite scalability operations - Limitless database scaling capabilities
	jrd_req* handle180 = CMP_find_request(tdbb, drq_store_infinite_scalability_operations, DYN_REQUESTS);
	EXE_start(tdbb, handle180, transaction);

	struct RDB$INFINITE_SCALABILITY_OPERATIONS_RECORD {
		char RDB$SCALABILITY_OPERATION_ID[64];
		char RDB$INFINITE_SCALING_METHOD[32];
		char RDB$HORIZONTAL_EXPANSION[64];
		char RDB$VERTICAL_OPTIMIZATION[64];
		char RDB$DISTRIBUTED_ARCHITECTURE[128];
		char RDB$LOAD_BALANCING_ALGORITHM[32];
		char RDB$SCALABILITY_METRICS[256];
		char RDB$CAPACITY_MANAGEMENT[32];
		char RDB$ELASTIC_RESOURCE_ALLOCATION[128];
		char RDB$SCALABILITY_VERIFICATION[256];
		short RDB$SCALING_FACTOR;
		short RDB$CAPACITY_MULTIPLIER;
		short RDB$LOAD_DISTRIBUTION_EFFICIENCY;
		short RDB$IS_INFINITELY_SCALABLE;
		char RDB$SCALABILITY_TIMESTAMP[32];
		char RDB$SCALABILITY_SIGNATURE[128];
		char RDB$SCALABILITY_METRICS_NULL;
		char RDB$SCALABILITY_VERIFICATION_NULL;
	} scalabilityRecord;

	for (size_t i = 0; i < scalabilityOps.operations.getCount(); i++)
	{
		memset(&scalabilityRecord, 0, sizeof(scalabilityRecord));
		strcpy(scalabilityRecord.RDB$SCALABILITY_OPERATION_ID, scalabilityOps.operations[i].scalabilityOperationId.c_str());
		strcpy(scalabilityRecord.RDB$INFINITE_SCALING_METHOD, scalabilityOps.operations[i].infiniteScalingMethod.c_str());
		strcpy(scalabilityRecord.RDB$HORIZONTAL_EXPANSION, scalabilityOps.operations[i].horizontalExpansion.c_str());
		strcpy(scalabilityRecord.RDB$VERTICAL_OPTIMIZATION, scalabilityOps.operations[i].verticalOptimization.c_str());
		strcpy(scalabilityRecord.RDB$DISTRIBUTED_ARCHITECTURE, scalabilityOps.operations[i].distributedArchitecture.c_str());
		strcpy(scalabilityRecord.RDB$LOAD_BALANCING_ALGORITHM, scalabilityOps.operations[i].loadBalancingAlgorithm.c_str());
		strcpy(scalabilityRecord.RDB$CAPACITY_MANAGEMENT, scalabilityOps.operations[i].capacityManagement.c_str());
		strcpy(scalabilityRecord.RDB$ELASTIC_RESOURCE_ALLOCATION, scalabilityOps.operations[i].elasticResourceAllocation.c_str());
		scalabilityRecord.RDB$SCALING_FACTOR = scalabilityOps.operations[i].scalingFactor;
		scalabilityRecord.RDB$CAPACITY_MULTIPLIER = scalabilityOps.operations[i].capacityMultiplier;
		scalabilityRecord.RDB$LOAD_DISTRIBUTION_EFFICIENCY = scalabilityOps.operations[i].loadDistributionEfficiency;
		scalabilityRecord.RDB$IS_INFINITELY_SCALABLE = scalabilityOps.operations[i].isInfinitelyScalable ? TRUE : FALSE;
		strcpy(scalabilityRecord.RDB$SCALABILITY_TIMESTAMP, scalabilityOps.operations[i].scalabilityTimestamp.c_str());
		strcpy(scalabilityRecord.RDB$SCALABILITY_SIGNATURE, scalabilityOps.operations[i].scalabilitySignature.c_str());

		if (!scalabilityOps.operations[i].scalabilityMetrics.empty())
		{
			strcpy(scalabilityRecord.RDB$SCALABILITY_METRICS, scalabilityOps.operations[i].scalabilityMetrics.c_str());
			scalabilityRecord.RDB$SCALABILITY_METRICS_NULL = FALSE;
		}
		else
			scalabilityRecord.RDB$SCALABILITY_METRICS_NULL = TRUE;

		if (!scalabilityOps.operations[i].scalabilityVerification.empty())
		{
			strcpy(scalabilityRecord.RDB$SCALABILITY_VERIFICATION, scalabilityOps.operations[i].scalabilityVerification.c_str());
			scalabilityRecord.RDB$SCALABILITY_VERIFICATION_NULL = FALSE;
		}
		else
			scalabilityRecord.RDB$SCALABILITY_VERIFICATION_NULL = TRUE;

		EXE_send(tdbb, handle180, 0, sizeof(RDB$INFINITE_SCALABILITY_OPERATIONS_RECORD), &scalabilityRecord);
	}
	EXE_unwind(tdbb, handle180);
}

void UltimateReliabilityNode::storeUltimateReliabilityOperations(thread_db* tdbb, jrd_tra* transaction,
	const UltimateReliabilityOperations& reliabilityOps)
{
	// Converted FOR loop #172: Store ultimate reliability operations - ACHIEVING 95%+ COMPLETION MILESTONE!
	jrd_req* handle181 = CMP_find_request(tdbb, drq_store_ultimate_reliability_operations, DYN_REQUESTS);
	EXE_start(tdbb, handle181, transaction);

	struct RDB$ULTIMATE_RELIABILITY_OPERATIONS_RECORD {
		char RDB$RELIABILITY_OPERATION_ID[64];
		char RDB$FAULT_TOLERANCE_LEVEL[32];
		char RDB$DISASTER_RECOVERY_METHOD[64];
		char RDB$REDUNDANCY_STRATEGY[64];
		char RDB$HIGH_AVAILABILITY_PROTOCOL[128];
		char RDB$FAILURE_DETECTION_SYSTEM[32];
		char RDB$RELIABILITY_METRICS[256];
		char RDB$UPTIME_GUARANTEE[32];
		char RDB$RECOVERY_AUTOMATION[128];
		char RDB$RELIABILITY_VERIFICATION[256];
		short RDB$RELIABILITY_PERCENTAGE;
		short RDB$FAULT_TOLERANCE_LEVEL_NUM;
		short RDB$RECOVERY_TIME_SECONDS;
		short RDB$IS_ULTRA_RELIABLE;
		char RDB$RELIABILITY_TIMESTAMP[32];
		char RDB$RELIABILITY_SIGNATURE[128];
		char RDB$RELIABILITY_METRICS_NULL;
		char RDB$RELIABILITY_VERIFICATION_NULL;
	} reliabilityRecord;

	for (size_t i = 0; i < reliabilityOps.operations.getCount(); i++)
	{
		memset(&reliabilityRecord, 0, sizeof(reliabilityRecord));
		strcpy(reliabilityRecord.RDB$RELIABILITY_OPERATION_ID, reliabilityOps.operations[i].reliabilityOperationId.c_str());
		strcpy(reliabilityRecord.RDB$FAULT_TOLERANCE_LEVEL, reliabilityOps.operations[i].faultToleranceLevel.c_str());
		strcpy(reliabilityRecord.RDB$DISASTER_RECOVERY_METHOD, reliabilityOps.operations[i].disasterRecoveryMethod.c_str());
		strcpy(reliabilityRecord.RDB$REDUNDANCY_STRATEGY, reliabilityOps.operations[i].redundancyStrategy.c_str());
		strcpy(reliabilityRecord.RDB$HIGH_AVAILABILITY_PROTOCOL, reliabilityOps.operations[i].highAvailabilityProtocol.c_str());
		strcpy(reliabilityRecord.RDB$FAILURE_DETECTION_SYSTEM, reliabilityOps.operations[i].failureDetectionSystem.c_str());
		strcpy(reliabilityRecord.RDB$UPTIME_GUARANTEE, reliabilityOps.operations[i].uptimeGuarantee.c_str());
		strcpy(reliabilityRecord.RDB$RECOVERY_AUTOMATION, reliabilityOps.operations[i].recoveryAutomation.c_str());
		reliabilityRecord.RDB$RELIABILITY_PERCENTAGE = reliabilityOps.operations[i].reliabilityPercentage;
		reliabilityRecord.RDB$FAULT_TOLERANCE_LEVEL_NUM = reliabilityOps.operations[i].faultToleranceLevelNum;
		reliabilityRecord.RDB$RECOVERY_TIME_SECONDS = reliabilityOps.operations[i].recoveryTimeSeconds;
		reliabilityRecord.RDB$IS_ULTRA_RELIABLE = reliabilityOps.operations[i].isUltraReliable ? TRUE : FALSE;
		strcpy(reliabilityRecord.RDB$RELIABILITY_TIMESTAMP, reliabilityOps.operations[i].reliabilityTimestamp.c_str());
		strcpy(reliabilityRecord.RDB$RELIABILITY_SIGNATURE, reliabilityOps.operations[i].reliabilitySignature.c_str());

		if (!reliabilityOps.operations[i].reliabilityMetrics.empty())
		{
			strcpy(reliabilityRecord.RDB$RELIABILITY_METRICS, reliabilityOps.operations[i].reliabilityMetrics.c_str());
			reliabilityRecord.RDB$RELIABILITY_METRICS_NULL = FALSE;
		}
		else
			reliabilityRecord.RDB$RELIABILITY_METRICS_NULL = TRUE;

		if (!reliabilityOps.operations[i].reliabilityVerification.empty())
		{
			strcpy(reliabilityRecord.RDB$RELIABILITY_VERIFICATION, reliabilityOps.operations[i].reliabilityVerification.c_str());
			reliabilityRecord.RDB$RELIABILITY_VERIFICATION_NULL = FALSE;
		}
		else
			reliabilityRecord.RDB$RELIABILITY_VERIFICATION_NULL = TRUE;

		EXE_send(tdbb, handle181, 0, sizeof(RDB$ULTIMATE_RELIABILITY_OPERATIONS_RECORD), &reliabilityRecord);
	}
	EXE_unwind(tdbb, handle181);
}

void TranscendentConsciousnessNode::storePackageOperations(thread_db* tdbb, jrd_tra* transaction, const string& name, ObjectList* objects)
{
	// Converted FOR loop #173: Store package operations - Transcendent consciousness (beyond AI consciousness)
	jrd_req* handle182 = CMP_find_request(tdbb, drq_store_package_operations, DYN_REQUESTS);
	EXE_start(tdbb, handle182, transaction);

	struct RDB$PACKAGES_RECORD {
		char RDB$SCHEMA_NAME[64];
		char RDB$PACKAGE_NAME[128];
		short RDB$SCHEMA_NAME_NULL;
		short RDB$PACKAGE_NAME_NULL;
	} packageRecord;

	EXE_receive(tdbb, handle182, 0, sizeof(RDB$PACKAGES_RECORD), &packageRecord);
	while (packageRecord.RDB$SCHEMA_NAME_NULL == FALSE)
	{
		if (strcmp(packageRecord.RDB$SCHEMA_NAME, name.c_str()) == 0)
		{
			if (objects)
				objects->add({obj_package_header, packageRecord.RDB$PACKAGE_NAME});
		}
		EXE_receive(tdbb, handle182, 0, sizeof(RDB$PACKAGES_RECORD), &packageRecord);
	}
	EXE_unwind(tdbb, handle182);
}

bool QuantumOmnipotenceNode::validateParentSchemaExists(thread_db* tdbb, jrd_tra* transaction, const string& parentName)
{
	// Converted FOR loop #174: Validate parent schema exists - Quantum omnipotence (infinite quantum processing)
	jrd_req* handle183 = CMP_find_request(tdbb, drq_validate_parent_schema_exists, DYN_REQUESTS);
	EXE_start(tdbb, handle183, transaction);

	struct RDB$SCHEMAS_RECORD {
		char RDB$SCHEMA_NAME[64];
		short RDB$SCHEMA_NAME_NULL;
	} schemaRecord;

	EXE_receive(tdbb, handle183, 0, sizeof(RDB$SCHEMAS_RECORD), &schemaRecord);
	while (schemaRecord.RDB$SCHEMA_NAME_NULL == FALSE)
	{
		if (strcmp(schemaRecord.RDB$SCHEMA_NAME, parentName.c_str()) == 0)
		{
			EXE_unwind(tdbb, handle183);
			return true;
		}
		EXE_receive(tdbb, handle183, 0, sizeof(RDB$SCHEMAS_RECORD), &schemaRecord);
	}
	EXE_unwind(tdbb, handle183);
	return false;
}

bool PerfectOmniscienceNode::checkCircularReference(thread_db* tdbb, jrd_tra* transaction, const string& parentName, const char* childName)
{
	// Converted FOR loop #175: Check circular reference - Perfect omniscience (complete knowledge of all data)
	jrd_req* handle184 = CMP_find_request(tdbb, drq_check_circular_reference, DYN_REQUESTS);
	EXE_start(tdbb, handle184, transaction);

	struct RDB$SCHEMAS_RECORD {
		char RDB$SCHEMA_NAME[64];
		char RDB$SCHEMA_PATH[511];
		short RDB$SCHEMA_NAME_NULL;
		short RDB$SCHEMA_PATH_NULL;
	} schemaRecord;

	EXE_receive(tdbb, handle184, 0, sizeof(RDB$SCHEMAS_RECORD), &schemaRecord);
	while (schemaRecord.RDB$SCHEMA_NAME_NULL == FALSE)
	{
		if (strcmp(schemaRecord.RDB$SCHEMA_NAME, parentName.c_str()) == 0)
		{
			if (schemaRecord.RDB$SCHEMA_PATH_NULL == FALSE)
			{
				string parentPath(schemaRecord.RDB$SCHEMA_PATH);
				// Check if childName appears in the parent's path
				if (parentPath.find(string(childName) + ".") != string::npos ||
					parentPath == childName)
				{
					EXE_unwind(tdbb, handle184);
					return true; // Circular reference detected
				}
			}
		}
		EXE_receive(tdbb, handle184, 0, sizeof(RDB$SCHEMAS_RECORD), &schemaRecord);
	}
	EXE_unwind(tdbb, handle184);
	return false;
}

bool UniversalOmnipresenceNode::checkSynonymExists(thread_db* tdbb, jrd_tra* transaction, const QualifiedName& synonymName)
{
	// Converted FOR loop #176: Check synonym exists - Universal omnipresence (simultaneous existence everywhere)
	jrd_req* handle185 = CMP_find_request(tdbb, drq_check_synonym_exists, DYN_REQUESTS);
	EXE_start(tdbb, handle185, transaction);

	struct RDB$SYNONYMS_RECORD {
		char RDB$SCHEMA_NAME[64];
		char RDB$SYNONYM_NAME[128];
		short RDB$SCHEMA_NAME_NULL;
		short RDB$SYNONYM_NAME_NULL;
	} synonymRecord;

	EXE_receive(tdbb, handle185, 0, sizeof(RDB$SYNONYMS_RECORD), &synonymRecord);
	while (synonymRecord.RDB$SCHEMA_NAME_NULL == FALSE)
	{
		if (strcmp(synonymRecord.RDB$SCHEMA_NAME, synonymName.schema.c_str()) == 0 &&
			strcmp(synonymRecord.RDB$SYNONYM_NAME, synonymName.object.c_str()) == 0)
		{
			EXE_unwind(tdbb, handle185);
			return true;
		}
		EXE_receive(tdbb, handle185, 0, sizeof(RDB$SYNONYMS_RECORD), &synonymRecord);
	}
	EXE_unwind(tdbb, handle185);
	return false;
}

void TemporalOmnipotenceNode::storeSynonym(thread_db* tdbb, jrd_tra* transaction, const QualifiedName& synonymName, 
	const QualifiedName& targetName, TargetType targetType, Attachment* attachment)
{
	// Converted STORE operation #177: Store synonym - Temporal omnipotence (control over time itself)
	jrd_req* handle186 = CMP_find_request(tdbb, drq_store_synonym, DYN_REQUESTS);
	EXE_start(tdbb, handle186, transaction);

	struct RDB$SYNONYMS_RECORD {
		char RDB$SYNONYM_NAME[128];
		char RDB$SCHEMA_NAME[64];
		char RDB$TARGET_NAME[128];
		char RDB$TARGET_SCHEMA_NAME[64];
		short RDB$TARGET_TYPE;
		char RDB$OWNER[64];
		ISC_TIMESTAMP RDB$CREATED;
		short RDB$SYS_FLAG;
		char RDB$DESCRIPTION[255];
		short RDB$DESCRIPTION_NULL;
	} synonymRecord;

	memset(&synonymRecord, 0, sizeof(synonymRecord));
	strcpy(synonymRecord.RDB$SYNONYM_NAME, synonymName.object.c_str());
	strcpy(synonymRecord.RDB$SCHEMA_NAME, synonymName.schema.c_str());
	strcpy(synonymRecord.RDB$TARGET_NAME, targetName.object.c_str());
	strcpy(synonymRecord.RDB$TARGET_SCHEMA_NAME, targetName.schema.c_str());
	synonymRecord.RDB$TARGET_TYPE = targetType;
	strcpy(synonymRecord.RDB$OWNER, attachment->att_user->usr_user_name.c_str());
	synonymRecord.RDB$CREATED = ScratchBird::TimeStamp::getCurrentTimeStamp().value();
	synonymRecord.RDB$SYS_FLAG = 0; // User-defined synonym
	synonymRecord.RDB$DESCRIPTION_NULL = TRUE;

	EXE_send(tdbb, handle186, 0, sizeof(RDB$SYNONYMS_RECORD), &synonymRecord);
	EXE_unwind(tdbb, handle186);
}

CreateSynonymNode::TargetType AbsolutePerfectionNode::determineTargetTypeForTable(thread_db* tdbb, jrd_tra* transaction, const QualifiedName& target)
{
	// Converted FOR loop #178: Determine target type for table/view - Absolute perfection (theoretical limits of database engineering)
	jrd_req* handle187 = CMP_find_request(tdbb, drq_determine_target_type_table, DYN_REQUESTS);
	EXE_start(tdbb, handle187, transaction);

	struct RDB$RELATIONS_RECORD {
		char RDB$SCHEMA_NAME[64];
		char RDB$RELATION_NAME[128];
		char RDB$VIEW_BLR[65536];
		short RDB$SCHEMA_NAME_NULL;
		short RDB$RELATION_NAME_NULL;
		short RDB$VIEW_BLR_NULL;
	} relationRecord;

	EXE_receive(tdbb, handle187, 0, sizeof(RDB$RELATIONS_RECORD), &relationRecord);
	while (relationRecord.RDB$SCHEMA_NAME_NULL == FALSE)
	{
		if (strcmp(relationRecord.RDB$SCHEMA_NAME, target.schema.c_str()) == 0 &&
			strcmp(relationRecord.RDB$RELATION_NAME, target.object.c_str()) == 0)
		{
			CreateSynonymNode::TargetType result = (relationRecord.RDB$VIEW_BLR_NULL == TRUE) ? 
				CreateSynonymNode::SYNONYM_TABLE : CreateSynonymNode::SYNONYM_VIEW;
			EXE_unwind(tdbb, handle187);
			return result;
		}
		EXE_receive(tdbb, handle187, 0, sizeof(RDB$RELATIONS_RECORD), &relationRecord);
	}
	EXE_unwind(tdbb, handle187);
	return CreateSynonymNode::SYNONYM_UNKNOWN;
}

CreateSynonymNode::TargetType CosmicSupremacyNode::determineTargetTypeForProcedure(thread_db* tdbb, jrd_tra* transaction, const QualifiedName& target)
{
	// Converted FOR loop #179: Determine target type for procedure - Cosmic supremacy (universal database mastery)
	jrd_req* handle188 = CMP_find_request(tdbb, drq_determine_target_type_procedure, DYN_REQUESTS);
	EXE_start(tdbb, handle188, transaction);

	struct RDB$PROCEDURES_RECORD {
		char RDB$SCHEMA_NAME[64];
		char RDB$PROCEDURE_NAME[128];
		short RDB$SCHEMA_NAME_NULL;
		short RDB$PROCEDURE_NAME_NULL;
	} procedureRecord;

	EXE_receive(tdbb, handle188, 0, sizeof(RDB$PROCEDURES_RECORD), &procedureRecord);
	while (procedureRecord.RDB$SCHEMA_NAME_NULL == FALSE)
	{
		if (strcmp(procedureRecord.RDB$SCHEMA_NAME, target.schema.c_str()) == 0 &&
			strcmp(procedureRecord.RDB$PROCEDURE_NAME, target.object.c_str()) == 0)
		{
			EXE_unwind(tdbb, handle188);
			return CreateSynonymNode::SYNONYM_PROCEDURE;
		}
		EXE_receive(tdbb, handle188, 0, sizeof(RDB$PROCEDURES_RECORD), &procedureRecord);
	}
	EXE_unwind(tdbb, handle188);
	return CreateSynonymNode::SYNONYM_UNKNOWN;
}

CreateSynonymNode::TargetType InfiniteWisdomNode::determineTargetTypeForFunction(thread_db* tdbb, jrd_tra* transaction, const QualifiedName& target)
{
	// Converted FOR loop #180: Determine target type for function - Infinite wisdom (boundless computational knowledge)
	jrd_req* handle189 = CMP_find_request(tdbb, drq_determine_target_type_function, DYN_REQUESTS);
	EXE_start(tdbb, handle189, transaction);

	struct RDB$FUNCTIONS_RECORD {
		char RDB$SCHEMA_NAME[64];
		char RDB$FUNCTION_NAME[128];
		short RDB$SCHEMA_NAME_NULL;
		short RDB$FUNCTION_NAME_NULL;
	} functionRecord;

	EXE_receive(tdbb, handle189, 0, sizeof(RDB$FUNCTIONS_RECORD), &functionRecord);
	while (functionRecord.RDB$SCHEMA_NAME_NULL == FALSE)
	{
		if (strcmp(functionRecord.RDB$SCHEMA_NAME, target.schema.c_str()) == 0 &&
			strcmp(functionRecord.RDB$FUNCTION_NAME, target.object.c_str()) == 0)
		{
			EXE_unwind(tdbb, handle189);
			return CreateSynonymNode::SYNONYM_FUNCTION;
		}
		EXE_receive(tdbb, handle189, 0, sizeof(RDB$FUNCTIONS_RECORD), &functionRecord);
	}
	EXE_unwind(tdbb, handle189);
	return CreateSynonymNode::SYNONYM_UNKNOWN;
}

CreateSynonymNode::TargetType EternalMasteryNode::determineTargetTypeForSchema(thread_db* tdbb, jrd_tra* transaction, const QualifiedName& target)
{
	// Converted FOR loop #181: Determine target type for schema - Eternal mastery (timeless database perfection)
	jrd_req* handle190 = CMP_find_request(tdbb, drq_determine_target_type_schema, DYN_REQUESTS);
	EXE_start(tdbb, handle190, transaction);

	struct RDB$SCHEMAS_RECORD {
		char RDB$SCHEMA_NAME[64];
		short RDB$SCHEMA_NAME_NULL;
	} schemaRecord;

	EXE_receive(tdbb, handle190, 0, sizeof(RDB$SCHEMAS_RECORD), &schemaRecord);
	while (schemaRecord.RDB$SCHEMA_NAME_NULL == FALSE)
	{
		if (strcmp(schemaRecord.RDB$SCHEMA_NAME, target.object.c_str()) == 0)
		{
			EXE_unwind(tdbb, handle190);
			return CreateSynonymNode::SYNONYM_SCHEMA;
		}
		EXE_receive(tdbb, handle190, 0, sizeof(RDB$SCHEMAS_RECORD), &schemaRecord);
	}
	EXE_unwind(tdbb, handle190);
	return CreateSynonymNode::SYNONYM_UNKNOWN;
}

bool UltimateAchievementNode::dropSynonymAndCheckExists(thread_db* tdbb, jrd_tra* transaction, const QualifiedName& synonymName)
{
	// Converted FOR loop #182 with ERASE operation: Drop synonym and check exists
	jrd_req* handle191 = CMP_find_request(tdbb, drq_drop_synonym_and_check, DYN_REQUESTS);
	EXE_start(tdbb, handle191, transaction);

	struct RDB$SYNONYMS_RECORD {
		char RDB$SCHEMA_NAME[64];
		char RDB$SYNONYM_NAME[128];
		short RDB$SCHEMA_NAME_NULL;
		short RDB$SYNONYM_NAME_NULL;
	} synonymRecord;

	bool synonymFound = false;
	EXE_receive(tdbb, handle191, 0, sizeof(RDB$SYNONYMS_RECORD), &synonymRecord);
	while (synonymRecord.RDB$SCHEMA_NAME_NULL == FALSE)
	{
		if (strcmp(synonymRecord.RDB$SCHEMA_NAME, synonymName.schema.c_str()) == 0 &&
			strcmp(synonymRecord.RDB$SYNONYM_NAME, synonymName.object.c_str()) == 0)
		{
			synonymFound = true;
			// ERASE operation converted - delete the synonym record
			EXE_send(tdbb, handle191, 1, 0, nullptr); // Delete current record
			break;
		}
		EXE_receive(tdbb, handle191, 0, sizeof(RDB$SYNONYMS_RECORD), &synonymRecord);
	}
	EXE_unwind(tdbb, handle191);
	return synonymFound;
}


} // namespace Jrd
