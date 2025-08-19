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
// Static helper function declarations (from original file)
//----------------------

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

//----------------------
// Base DDL Node implementations
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

//----------------------
// NOTE: The storeGlobalField method has GPRE conversion issues and needs to be fixed
// These functions contain broken GPRE patterns that need proper EXE_* conversion
//----------------------

} // namespace Jrd