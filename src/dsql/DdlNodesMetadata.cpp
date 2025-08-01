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
// CreateDomainNode Class Implementation  
//----------------------

void CreateDomainNode::execute(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch,
	jrd_tra* transaction)
{
	Attachment* const attachment = transaction->tra_attachment;

	executeDdlTrigger(tdbb, dsqlScratch, transaction, DTW_BEFORE, DDL_TRIGGER_CREATE_DOMAIN, name);

	// Convert FOR loop #150: Check if domain already exists
	AutoCacheRequest request(tdbb, drq_l_domain_info, DYN_REQUESTS);
	EXE_start(tdbb, request, transaction);
	EXE_send(tdbb, request, 0, name.schema.length(), name.schema.c_str());
	EXE_send(tdbb, request, 1, name.object.length(), name.object.c_str());

	struct DomainCheckData {
		SSHORT systemFlag;
		SSHORT systemFlagNull;
	} domainData;

	if (EXE_receive(tdbb, request, 2, sizeof(domainData), &domainData))
	{
		EXE_unwind(tdbb, request);
		
		if (!domainData.systemFlagNull && domainData.systemFlag &&
			!(attachment->att_flags & ATT_system))
		{
			status_exception::raise(
				Arg::Gds(isc_dyn_cannot_mod_sysdomain) << name.toQuotedString());
		}

		status_exception::raise(
			Arg::Gds(isc_dyn_domain_exists) << name.toQuotedString());
	}
	EXE_unwind(tdbb, request);

	// Convert STORE operation #150: Store domain in RDB$FIELDS  
	AutoCacheRequest storeRequest(tdbb, drq_s_domain, DYN_REQUESTS);
	EXE_start(tdbb, storeRequest, transaction);

	struct RDB$FIELDS_DOMAIN_RECORD {
		char RDB$SCHEMA_NAME[MAX_SQL_IDENTIFIER_LEN];
		char RDB$FIELD_NAME[MAX_SQL_IDENTIFIER_LEN];
		SSHORT RDB$FIELD_TYPE;
		SSHORT RDB$FIELD_TYPE_NULL;
		SSHORT RDB$FIELD_SUB_TYPE;
		SSHORT RDB$FIELD_SUB_TYPE_NULL;
		SSHORT RDB$FIELD_LENGTH;
		SSHORT RDB$FIELD_LENGTH_NULL;
		SSHORT RDB$FIELD_SCALE;
		SSHORT RDB$FIELD_SCALE_NULL;
		SSHORT RDB$FIELD_PRECISION;
		SSHORT RDB$FIELD_PRECISION_NULL;
		SSHORT RDB$CHARACTER_SET_ID;
		SSHORT RDB$CHARACTER_SET_ID_NULL;
		SSHORT RDB$CHARACTER_LENGTH;
		SSHORT RDB$CHARACTER_LENGTH_NULL;
		SSHORT RDB$COLLATION_ID;
		SSHORT RDB$COLLATION_ID_NULL;
		SSHORT RDB$SEGMENT_LENGTH;
		SSHORT RDB$SEGMENT_LENGTH_NULL;
		SSHORT RDB$DIMENSIONS;
		SSHORT RDB$DIMENSIONS_NULL;
		SSHORT RDB$NULL_FLAG;
		SSHORT RDB$NULL_FLAG_NULL;
		bid RDB$DEFAULT_SOURCE;
		SSHORT RDB$DEFAULT_SOURCE_NULL;
		bid RDB$DEFAULT_VALUE;
		SSHORT RDB$DEFAULT_VALUE_NULL;
		bid RDB$VALIDATION_SOURCE;
		SSHORT RDB$VALIDATION_SOURCE_NULL;
		bid RDB$VALIDATION_BLR;
		SSHORT RDB$VALIDATION_BLR_NULL;
		bid RDB$DESCRIPTION;
		SSHORT RDB$DESCRIPTION_NULL;
		SSHORT RDB$SYSTEM_FLAG;
		SSHORT RDB$SYSTEM_FLAG_NULL;
		char RDB$OWNER_NAME[MAX_SQL_IDENTIFIER_LEN];
		SSHORT RDB$OWNER_NAME_NULL;
	} domainRecord;

	memset(&domainRecord, 0, sizeof(domainRecord));
	
	strcpy(domainRecord.RDB$SCHEMA_NAME, name.schema.c_str());
	strcpy(domainRecord.RDB$FIELD_NAME, name.object.c_str());

	domainRecord.RDB$OWNER_NAME_NULL = FALSE;
	strcpy(domainRecord.RDB$OWNER_NAME, attachment->getUserName().c_str());

	domainRecord.RDB$SYSTEM_FLAG_NULL = FALSE;
	domainRecord.RDB$SYSTEM_FLAG = 0;

	// Set field type information
	if (fieldType)
	{
		updateRdbFields(fieldType,
			domainRecord.RDB$FIELD_TYPE,
			domainRecord.RDB$FIELD_LENGTH,
			domainRecord.RDB$FIELD_SUB_TYPE_NULL, domainRecord.RDB$FIELD_SUB_TYPE,
			domainRecord.RDB$FIELD_SCALE_NULL, domainRecord.RDB$FIELD_SCALE,
			domainRecord.RDB$CHARACTER_SET_ID_NULL, domainRecord.RDB$CHARACTER_SET_ID,
			domainRecord.RDB$CHARACTER_LENGTH_NULL, domainRecord.RDB$CHARACTER_LENGTH,
			domainRecord.RDB$FIELD_PRECISION_NULL, domainRecord.RDB$FIELD_PRECISION,
			domainRecord.RDB$COLLATION_ID_NULL, domainRecord.RDB$COLLATION_ID,
			domainRecord.RDB$SEGMENT_LENGTH_NULL, domainRecord.RDB$SEGMENT_LENGTH);
	}

	// Set NOT NULL constraint
	if (notNullFlag)
	{
		domainRecord.RDB$NULL_FLAG_NULL = FALSE;
		domainRecord.RDB$NULL_FLAG = 1;
	}
	else
	{
		domainRecord.RDB$NULL_FLAG_NULL = TRUE;
	}

	// Set default value
	if (defaultValue && defaultValue->defaultSource.hasData())
	{
		domainRecord.RDB$DEFAULT_SOURCE_NULL = FALSE;
		attachment->storeMetaDataBlob(tdbb, transaction, &domainRecord.RDB$DEFAULT_SOURCE,
			defaultValue->defaultSource);

		if (defaultValue->defaultValue.hasData())
		{
			domainRecord.RDB$DEFAULT_VALUE_NULL = FALSE;
			attachment->storeBinaryBlob(tdbb, transaction, &domainRecord.RDB$DEFAULT_VALUE,
				defaultValue->defaultValue);
		}
		else
		{
			domainRecord.RDB$DEFAULT_VALUE_NULL = TRUE;
		}
	}
	else
	{
		domainRecord.RDB$DEFAULT_SOURCE_NULL = TRUE;
		domainRecord.RDB$DEFAULT_VALUE_NULL = TRUE;
	}

	// Set check constraint
	if (checkConstraint)
	{
		domainRecord.RDB$VALIDATION_SOURCE_NULL = FALSE;
		attachment->storeMetaDataBlob(tdbb, transaction, &domainRecord.RDB$VALIDATION_SOURCE,
			checkConstraint->source);

		if (checkConstraint->blr.hasData())
		{
			domainRecord.RDB$VALIDATION_BLR_NULL = FALSE;
			attachment->storeBinaryBlob(tdbb, transaction, &domainRecord.RDB$VALIDATION_BLR,
				checkConstraint->blr);
		}
		else
		{
			domainRecord.RDB$VALIDATION_BLR_NULL = TRUE;
		}
	}
	else
	{
		domainRecord.RDB$VALIDATION_SOURCE_NULL = TRUE;
		domainRecord.RDB$VALIDATION_BLR_NULL = TRUE;
	}

	domainRecord.RDB$DIMENSIONS_NULL = TRUE;
	domainRecord.RDB$DESCRIPTION_NULL = TRUE;

	EXE_send(tdbb, storeRequest, 0, sizeof(domainRecord), &domainRecord);
	EXE_unwind(tdbb, storeRequest);

	executeDdlTrigger(tdbb, dsqlScratch, transaction, DTW_AFTER, DDL_TRIGGER_CREATE_DOMAIN, name);
}

//----------------------
// AlterDomainNode Class Implementation  
//----------------------

void AlterDomainNode::execute(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch,
	jrd_tra* transaction)
{
	Attachment* const attachment = transaction->tra_attachment;

	executeDdlTrigger(tdbb, dsqlScratch, transaction, DTW_BEFORE, DDL_TRIGGER_ALTER_DOMAIN, name);

	// Convert FOR loop #155: Find domain to alter
	AutoCacheRequest request(tdbb, drq_l_domain_info2, DYN_REQUESTS);
	EXE_start(tdbb, request, transaction);
	EXE_send(tdbb, request, 0, name.schema.length(), name.schema.c_str());
	EXE_send(tdbb, request, 1, name.object.length(), name.object.c_str());

	struct DomainAlterData {
		SSHORT systemFlag;
		SSHORT systemFlagNull;
		char ownerName[MAX_SQL_IDENTIFIER_LEN];
		SSHORT ownerNameNull;
		SSHORT nullFlag;
		SSHORT nullFlagNull;
	} domainData;

	bool domainFound = false;
	if (EXE_receive(tdbb, request, 2, sizeof(domainData), &domainData))
	{
		domainFound = true;
		
		if (!domainData.systemFlagNull && domainData.systemFlag &&
			!(attachment->att_flags & ATT_system))
		{
			EXE_unwind(tdbb, request);
			status_exception::raise(
				Arg::Gds(isc_dyn_cannot_mod_sysdomain) << name.toQuotedString());
		}
	}
	EXE_unwind(tdbb, request);

	if (!domainFound)
	{
		status_exception::raise(
			Arg::Gds(isc_dyn_domain_not_found) << name.toQuotedString());
	}

	// Convert MODIFY operation #155: Modify domain in RDB$FIELDS
	AutoCacheRequest modifyRequest(tdbb, drq_m_domain, DYN_REQUESTS);
	EXE_start(tdbb, modifyRequest, transaction);
	EXE_send(tdbb, modifyRequest, 0, name.schema.length(), name.schema.c_str());
	EXE_send(tdbb, modifyRequest, 1, name.object.length(), name.object.c_str());

	struct ModifyDomainData {
		SSHORT nullFlag;
		SSHORT nullFlagNull;
		bid defaultSource;
		SSHORT defaultSourceNull;
		bid defaultValue;
		SSHORT defaultValueNull;
		bid validationSource;
		SSHORT validationSourceNull;
		bid validationBlr;
		SSHORT validationBlrNull;
	} modifyData;

	if (EXE_receive(tdbb, modifyRequest, 2, sizeof(modifyData), &modifyData))
	{
		bool needsUpdate = false;

		// Handle SET/DROP DEFAULT
		if (action == AlterDomainAction::SET_DEFAULT)
		{
			if (defaultValue && defaultValue->defaultSource.hasData())
			{
				modifyData.defaultSourceNull = FALSE;
				attachment->storeMetaDataBlob(tdbb, transaction, &modifyData.defaultSource,
					defaultValue->defaultSource);

				if (defaultValue->defaultValue.hasData())
				{
					modifyData.defaultValueNull = FALSE;
					attachment->storeBinaryBlob(tdbb, transaction, &modifyData.defaultValue,
						defaultValue->defaultValue);
				}
				else
				{
					modifyData.defaultValueNull = TRUE;
				}
				needsUpdate = true;
			}
		}
		else if (action == AlterDomainAction::DROP_DEFAULT)
		{
			modifyData.defaultSourceNull = TRUE;
			modifyData.defaultValueNull = TRUE;
			needsUpdate = true;
		}

		// Handle SET/DROP NOT NULL
		else if (action == AlterDomainAction::SET_NOT_NULL)
		{
			modifyData.nullFlagNull = FALSE;
			modifyData.nullFlag = 1;
			needsUpdate = true;
		}
		else if (action == AlterDomainAction::DROP_NOT_NULL)
		{
			modifyData.nullFlagNull = TRUE;
			needsUpdate = true;
		}

		// Handle ADD/DROP CHECK
		else if (action == AlterDomainAction::ADD_CONSTRAINT)
		{
			if (checkConstraint)
			{
				modifyData.validationSourceNull = FALSE;
				attachment->storeMetaDataBlob(tdbb, transaction, &modifyData.validationSource,
					checkConstraint->source);

				if (checkConstraint->blr.hasData())
				{
					modifyData.validationBlrNull = FALSE;
					attachment->storeBinaryBlob(tdbb, transaction, &modifyData.validationBlr,
						checkConstraint->blr);
				}
				else
				{
					modifyData.validationBlrNull = TRUE;
				}
				needsUpdate = true;
			}
		}
		else if (action == AlterDomainAction::DROP_CONSTRAINT)
		{
			modifyData.validationSourceNull = TRUE;
			modifyData.validationBlrNull = TRUE;
			needsUpdate = true;
		}

		if (needsUpdate)
		{
			EXE_send(tdbb, modifyRequest, 3, sizeof(modifyData), &modifyData);
		}
	}
	EXE_unwind(tdbb, modifyRequest);

	executeDdlTrigger(tdbb, dsqlScratch, transaction, DTW_AFTER, DDL_TRIGGER_ALTER_DOMAIN, name);
}

//----------------------
// DropDomainNode Class Implementation  
//----------------------

void DropDomainNode::execute(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch,
	jrd_tra* transaction)
{
	Attachment* const attachment = transaction->tra_attachment;

	executeDdlTrigger(tdbb, dsqlScratch, transaction, DTW_BEFORE, DDL_TRIGGER_DROP_DOMAIN, name);

	// Convert FOR loop #160: Find domain to drop
	AutoCacheRequest request(tdbb, drq_l_domain_info3, DYN_REQUESTS);
	EXE_start(tdbb, request, transaction);
	EXE_send(tdbb, request, 0, name.schema.length(), name.schema.c_str());
	EXE_send(tdbb, request, 1, name.object.length(), name.object.c_str());

	struct DomainDropData {
		SSHORT systemFlag;
		SSHORT systemFlagNull;
		char ownerName[MAX_SQL_IDENTIFIER_LEN];
		SSHORT ownerNameNull;
	} domainData;

	bool domainFound = false;
	if (EXE_receive(tdbb, request, 2, sizeof(domainData), &domainData))
	{
		domainFound = true;
		
		if (!domainData.systemFlagNull && domainData.systemFlag &&
			!(attachment->att_flags & ATT_system))
		{
			EXE_unwind(tdbb, request);
			status_exception::raise(
				Arg::Gds(isc_dyn_cannot_mod_sysdomain) << name.toQuotedString());
		}
	}
	EXE_unwind(tdbb, request);

	if (!domainFound)
	{
		if (!silent)
		{
			status_exception::raise(
				Arg::Gds(isc_dyn_domain_not_found) << name.toQuotedString());
		}
		return;
	}

	// Check if domain is in use before dropping
	// Convert FOR loop #161: Check domain usage
	AutoCacheRequest usageRequest(tdbb, drq_l_domain_usage, DYN_REQUESTS);
	EXE_start(tdbb, usageRequest, transaction);
	EXE_send(tdbb, usageRequest, 0, name.schema.length(), name.schema.c_str());
	EXE_send(tdbb, usageRequest, 1, name.object.length(), name.object.c_str());

	if (EXE_receive(tdbb, usageRequest, 2, 0, NULL))
	{
		EXE_unwind(tdbb, usageRequest);
		status_exception::raise(
			Arg::Gds(isc_dyn_domain_in_use) << name.toQuotedString());
	}
	EXE_unwind(tdbb, usageRequest);

	// Convert DELETE operation #60: Delete domain from RDB$FIELDS
	AutoCacheRequest deleteRequest(tdbb, drq_e_domain, DYN_REQUESTS);
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

	executeDdlTrigger(tdbb, dsqlScratch, transaction, DTW_AFTER, DDL_TRIGGER_DROP_DOMAIN, name);
}

//----------------------
// CreateAlterSequenceNode Class Implementation  
//----------------------

void CreateAlterSequenceNode::execute(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch,
	jrd_tra* transaction)
{
	Attachment* const attachment = transaction->tra_attachment;

	const int triggerType = create ? DDL_TRIGGER_CREATE_SEQUENCE : DDL_TRIGGER_ALTER_SEQUENCE;
	executeDdlTrigger(tdbb, dsqlScratch, transaction, DTW_BEFORE, triggerType, name);

	if (create)
	{
		// Convert FOR loop #165: Check if sequence already exists
		AutoCacheRequest request(tdbb, drq_l_seq_info, DYN_REQUESTS);
		EXE_start(tdbb, request, transaction);
		EXE_send(tdbb, request, 0, name.schema.length(), name.schema.c_str());
		EXE_send(tdbb, request, 1, name.object.length(), name.object.c_str());

		struct SequenceCheckData {
			SSHORT systemFlag;
			SSHORT systemFlagNull;
		} seqData;

		if (EXE_receive(tdbb, request, 2, sizeof(seqData), &seqData))
		{
			EXE_unwind(tdbb, request);
			
			if (!seqData.systemFlagNull && seqData.systemFlag &&
				!(attachment->att_flags & ATT_system))
			{
				status_exception::raise(
					Arg::Gds(isc_dyn_cannot_mod_sysseq) << name.toQuotedString());
			}

			status_exception::raise(
				Arg::Gds(isc_dyn_seq_exists) << name.toQuotedString());
		}
		EXE_unwind(tdbb, request);

		// Convert STORE operation #165: Store sequence in RDB$GENERATORS
		AutoCacheRequest storeRequest(tdbb, drq_s_generators, DYN_REQUESTS);
		EXE_start(tdbb, storeRequest, transaction);

		struct RDB$GENERATORS_RECORD {
			char RDB$SCHEMA_NAME[MAX_SQL_IDENTIFIER_LEN];
			char RDB$GENERATOR_NAME[MAX_SQL_IDENTIFIER_LEN];
			SSHORT RDB$GENERATOR_ID;
			SSHORT RDB$GENERATOR_ID_NULL;
			SINT64 RDB$GENERATOR_VALUE;
			SSHORT RDB$GENERATOR_VALUE_NULL;
			SSHORT RDB$SYSTEM_FLAG;
			SSHORT RDB$SYSTEM_FLAG_NULL;
			bid RDB$DESCRIPTION;
			SSHORT RDB$DESCRIPTION_NULL;
			char RDB$OWNER_NAME[MAX_SQL_IDENTIFIER_LEN];
			SSHORT RDB$OWNER_NAME_NULL;
			SINT64 RDB$INITIAL_VALUE;
			SSHORT RDB$INITIAL_VALUE_NULL;
			SINT64 RDB$INCREMENT;
			SSHORT RDB$INCREMENT_NULL;
		} seqRecord;

		memset(&seqRecord, 0, sizeof(seqRecord));
		
		strcpy(seqRecord.RDB$SCHEMA_NAME, name.schema.c_str());
		strcpy(seqRecord.RDB$GENERATOR_NAME, name.object.c_str());

		seqRecord.RDB$GENERATOR_ID_NULL = FALSE;
		seqRecord.RDB$GENERATOR_ID = tdbb->getDatabase()->generateId();

		seqRecord.RDB$OWNER_NAME_NULL = FALSE;
		strcpy(seqRecord.RDB$OWNER_NAME, attachment->getUserName().c_str());

		seqRecord.RDB$SYSTEM_FLAG_NULL = FALSE;
		seqRecord.RDB$SYSTEM_FLAG = 0;

		// Set initial value
		if (startWith.has_value())
		{
			seqRecord.RDB$INITIAL_VALUE_NULL = FALSE;
			seqRecord.RDB$INITIAL_VALUE = startWith.value();
			seqRecord.RDB$GENERATOR_VALUE_NULL = FALSE;
			seqRecord.RDB$GENERATOR_VALUE = startWith.value();
		}
		else
		{
			seqRecord.RDB$INITIAL_VALUE_NULL = FALSE;
			seqRecord.RDB$INITIAL_VALUE = 1;
			seqRecord.RDB$GENERATOR_VALUE_NULL = FALSE;
			seqRecord.RDB$GENERATOR_VALUE = 1;
		}

		// Set increment
		if (increment.has_value())
		{
			seqRecord.RDB$INCREMENT_NULL = FALSE;
			seqRecord.RDB$INCREMENT = increment.value();
		}
		else
		{
			seqRecord.RDB$INCREMENT_NULL = FALSE;
			seqRecord.RDB$INCREMENT = 1;
		}

		seqRecord.RDB$DESCRIPTION_NULL = TRUE;

		EXE_send(tdbb, storeRequest, 0, sizeof(seqRecord), &seqRecord);
		EXE_unwind(tdbb, storeRequest);
	}
	else
	{
		// ALTER SEQUENCE case
		// Convert FOR loop #166: Find sequence to alter
		AutoCacheRequest request(tdbb, drq_l_seq_info2, DYN_REQUESTS);
		EXE_start(tdbb, request, transaction);
		EXE_send(tdbb, request, 0, name.schema.length(), name.schema.c_str());
		EXE_send(tdbb, request, 1, name.object.length(), name.object.c_str());

		struct SequenceAlterData {
			SSHORT systemFlag;
			SSHORT systemFlagNull;
			char ownerName[MAX_SQL_IDENTIFIER_LEN];
			SSHORT ownerNameNull;
			SINT64 currentValue;
			SSHORT currentValueNull;
			SINT64 currentIncrement;
			SSHORT currentIncrementNull;
		} seqData;

		bool sequenceFound = false;
		if (EXE_receive(tdbb, request, 2, sizeof(seqData), &seqData))
		{
			sequenceFound = true;
			
			if (!seqData.systemFlagNull && seqData.systemFlag &&
				!(attachment->att_flags & ATT_system))
			{
				EXE_unwind(tdbb, request);
				status_exception::raise(
					Arg::Gds(isc_dyn_cannot_mod_sysseq) << name.toQuotedString());
			}
		}
		EXE_unwind(tdbb, request);

		if (!sequenceFound)
		{
			status_exception::raise(
				Arg::Gds(isc_dyn_seq_not_found) << name.toQuotedString());
		}

		// Convert MODIFY operation #166: Modify sequence in RDB$GENERATORS
		AutoCacheRequest modifyRequest(tdbb, drq_m_generator, DYN_REQUESTS);
		EXE_start(tdbb, modifyRequest, transaction);
		EXE_send(tdbb, modifyRequest, 0, name.schema.length(), name.schema.c_str());
		EXE_send(tdbb, modifyRequest, 1, name.object.length(), name.object.c_str());

		struct ModifySequenceData {
			SINT64 generatorValue;
			SSHORT generatorValueNull;
			SINT64 increment;
			SSHORT incrementNull;
		} modifyData;

		if (EXE_receive(tdbb, modifyRequest, 2, sizeof(modifyData), &modifyData))
		{
			bool needsUpdate = false;

			if (restartWith.has_value())
			{
				modifyData.generatorValueNull = FALSE;
				modifyData.generatorValue = restartWith.value();
				needsUpdate = true;
			}

			if (increment.has_value())
			{
				modifyData.incrementNull = FALSE;
				modifyData.increment = increment.value();
				needsUpdate = true;
			}

			if (needsUpdate)
			{
				EXE_send(tdbb, modifyRequest, 3, sizeof(modifyData), &modifyData);
			}
		}
		EXE_unwind(tdbb, modifyRequest);
	}

	executeDdlTrigger(tdbb, dsqlScratch, transaction, DTW_AFTER, triggerType, name);
}

//----------------------
// DropSequenceNode Class Implementation  
//----------------------

void DropSequenceNode::execute(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch,
	jrd_tra* transaction)
{
	Attachment* const attachment = transaction->tra_attachment;

	executeDdlTrigger(tdbb, dsqlScratch, transaction, DTW_BEFORE, DDL_TRIGGER_DROP_SEQUENCE, name);

	// Convert FOR loop #170: Find sequence to drop
	AutoCacheRequest request(tdbb, drq_l_seq_info3, DYN_REQUESTS);
	EXE_start(tdbb, request, transaction);
	EXE_send(tdbb, request, 0, name.schema.length(), name.schema.c_str());
	EXE_send(tdbb, request, 1, name.object.length(), name.object.c_str());

	struct SequenceDropData {
		SSHORT systemFlag;
		SSHORT systemFlagNull;
		char ownerName[MAX_SQL_IDENTIFIER_LEN];
		SSHORT ownerNameNull;
	} seqData;

	bool sequenceFound = false;
	if (EXE_receive(tdbb, request, 2, sizeof(seqData), &seqData))
	{
		sequenceFound = true;
		
		if (!seqData.systemFlagNull && seqData.systemFlag &&
			!(attachment->att_flags & ATT_system))
		{
			EXE_unwind(tdbb, request);
			status_exception::raise(
				Arg::Gds(isc_dyn_cannot_mod_sysseq) << name.toQuotedString());
		}
	}
	EXE_unwind(tdbb, request);

	if (!sequenceFound)
	{
		if (!silent)
		{
			status_exception::raise(
				Arg::Gds(isc_dyn_seq_not_found) << name.toQuotedString());
		}
		return;
	}

	// Convert DELETE operation #65: Delete sequence from RDB$GENERATORS
	AutoCacheRequest deleteRequest(tdbb, drq_e_generator, DYN_REQUESTS);
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

	executeDdlTrigger(tdbb, dsqlScratch, transaction, DTW_AFTER, DDL_TRIGGER_DROP_SEQUENCE, name);
}

//----------------------
// CreateAlterExceptionNode Class Implementation  
//----------------------

void CreateAlterExceptionNode::execute(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch,
	jrd_tra* transaction)
{
	Attachment* const attachment = transaction->tra_attachment;

	const int triggerType = create ? DDL_TRIGGER_CREATE_EXCEPTION : DDL_TRIGGER_ALTER_EXCEPTION;
	executeDdlTrigger(tdbb, dsqlScratch, transaction, DTW_BEFORE, triggerType, name);

	if (create)
	{
		// Convert FOR loop #175: Check if exception already exists
		AutoCacheRequest request(tdbb, drq_l_exc_info, DYN_REQUESTS);
		EXE_start(tdbb, request, transaction);
		EXE_send(tdbb, request, 0, name.schema.length(), name.schema.c_str());
		EXE_send(tdbb, request, 1, name.object.length(), name.object.c_str());

		struct ExceptionCheckData {
			SSHORT systemFlag;
			SSHORT systemFlagNull;
		} excData;

		if (EXE_receive(tdbb, request, 2, sizeof(excData), &excData))
		{
			EXE_unwind(tdbb, request);
			
			if (!excData.systemFlagNull && excData.systemFlag &&
				!(attachment->att_flags & ATT_system))
			{
				status_exception::raise(
					Arg::Gds(isc_dyn_cannot_mod_sysexc) << name.toQuotedString());
			}

			status_exception::raise(
				Arg::Gds(isc_dyn_exc_exists) << name.toQuotedString());
		}
		EXE_unwind(tdbb, request);

		// Convert STORE operation #175: Store exception in RDB$EXCEPTIONS
		AutoCacheRequest storeRequest(tdbb, drq_s_exceptions, DYN_REQUESTS);
		EXE_start(tdbb, storeRequest, transaction);

		struct RDB$EXCEPTIONS_RECORD {
			char RDB$SCHEMA_NAME[MAX_SQL_IDENTIFIER_LEN];
			char RDB$EXCEPTION_NAME[MAX_SQL_IDENTIFIER_LEN];
			SSHORT RDB$EXCEPTION_NUMBER;
			SSHORT RDB$EXCEPTION_NUMBER_NULL;
			bid RDB$MESSAGE;
			SSHORT RDB$MESSAGE_NULL;
			bid RDB$DESCRIPTION;
			SSHORT RDB$DESCRIPTION_NULL;
			SSHORT RDB$SYSTEM_FLAG;
			SSHORT RDB$SYSTEM_FLAG_NULL;
			char RDB$OWNER_NAME[MAX_SQL_IDENTIFIER_LEN];
			SSHORT RDB$OWNER_NAME_NULL;
		} excRecord;

		memset(&excRecord, 0, sizeof(excRecord));
		
		strcpy(excRecord.RDB$SCHEMA_NAME, name.schema.c_str());
		strcpy(excRecord.RDB$EXCEPTION_NAME, name.object.c_str());

		excRecord.RDB$EXCEPTION_NUMBER_NULL = FALSE;
		excRecord.RDB$EXCEPTION_NUMBER = tdbb->getDatabase()->generateId();

		excRecord.RDB$OWNER_NAME_NULL = FALSE;
		strcpy(excRecord.RDB$OWNER_NAME, attachment->getUserName().c_str());

		excRecord.RDB$SYSTEM_FLAG_NULL = FALSE;
		excRecord.RDB$SYSTEM_FLAG = 0;

		if (message.hasData())
		{
			excRecord.RDB$MESSAGE_NULL = FALSE;
			attachment->storeMetaDataBlob(tdbb, transaction, &excRecord.RDB$MESSAGE, message);
		}
		else
		{
			excRecord.RDB$MESSAGE_NULL = TRUE;
		}

		excRecord.RDB$DESCRIPTION_NULL = TRUE;

		EXE_send(tdbb, storeRequest, 0, sizeof(excRecord), &excRecord);
		EXE_unwind(tdbb, storeRequest);
	}
	else
	{
		// ALTER EXCEPTION case
		// Convert FOR loop #176: Find exception to alter
		AutoCacheRequest request(tdbb, drq_l_exc_info2, DYN_REQUESTS);
		EXE_start(tdbb, request, transaction);
		EXE_send(tdbb, request, 0, name.schema.length(), name.schema.c_str());
		EXE_send(tdbb, request, 1, name.object.length(), name.object.c_str());

		struct ExceptionAlterData {
			SSHORT systemFlag;
			SSHORT systemFlagNull;
			char ownerName[MAX_SQL_IDENTIFIER_LEN];
			SSHORT ownerNameNull;
		} excData;

		bool exceptionFound = false;
		if (EXE_receive(tdbb, request, 2, sizeof(excData), &excData))
		{
			exceptionFound = true;
			
			if (!excData.systemFlagNull && excData.systemFlag &&
				!(attachment->att_flags & ATT_system))
			{
				EXE_unwind(tdbb, request);
				status_exception::raise(
					Arg::Gds(isc_dyn_cannot_mod_sysexc) << name.toQuotedString());
			}
		}
		EXE_unwind(tdbb, request);

		if (!exceptionFound)
		{
			status_exception::raise(
				Arg::Gds(isc_dyn_exc_not_found) << name.toQuotedString());
		}

		// Convert MODIFY operation #176: Modify exception in RDB$EXCEPTIONS
		AutoCacheRequest modifyRequest(tdbb, drq_m_exception, DYN_REQUESTS);
		EXE_start(tdbb, modifyRequest, transaction);
		EXE_send(tdbb, modifyRequest, 0, name.schema.length(), name.schema.c_str());
		EXE_send(tdbb, modifyRequest, 1, name.object.length(), name.object.c_str());

		struct ModifyExceptionData {
			bid message;
			SSHORT messageNull;
		} modifyData;

		if (EXE_receive(tdbb, modifyRequest, 2, sizeof(modifyData), &modifyData))
		{
			if (message.hasData())
			{
				modifyData.messageNull = FALSE;
				attachment->storeMetaDataBlob(tdbb, transaction, &modifyData.message, message);
			}
			else
			{
				modifyData.messageNull = TRUE;
			}

			EXE_send(tdbb, modifyRequest, 3, sizeof(modifyData), &modifyData);
		}
		EXE_unwind(tdbb, modifyRequest);
	}

	executeDdlTrigger(tdbb, dsqlScratch, transaction, DTW_AFTER, triggerType, name);
}

//----------------------
// DropExceptionNode Class Implementation  
//----------------------

void DropExceptionNode::execute(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch,
	jrd_tra* transaction)
{
	Attachment* const attachment = transaction->tra_attachment;

	executeDdlTrigger(tdbb, dsqlScratch, transaction, DTW_BEFORE, DDL_TRIGGER_DROP_EXCEPTION, name);

	// Convert FOR loop #180: Find exception to drop
	AutoCacheRequest request(tdbb, drq_l_exc_info3, DYN_REQUESTS);
	EXE_start(tdbb, request, transaction);
	EXE_send(tdbb, request, 0, name.schema.length(), name.schema.c_str());
	EXE_send(tdbb, request, 1, name.object.length(), name.object.c_str());

	struct ExceptionDropData {
		SSHORT systemFlag;
		SSHORT systemFlagNull;
		char ownerName[MAX_SQL_IDENTIFIER_LEN];
		SSHORT ownerNameNull;
	} excData;

	bool exceptionFound = false;
	if (EXE_receive(tdbb, request, 2, sizeof(excData), &excData))
	{
		exceptionFound = true;
		
		if (!excData.systemFlagNull && excData.systemFlag &&
			!(attachment->att_flags & ATT_system))
		{
			EXE_unwind(tdbb, request);
			status_exception::raise(
				Arg::Gds(isc_dyn_cannot_mod_sysexc) << name.toQuotedString());
		}
	}
	EXE_unwind(tdbb, request);

	if (!exceptionFound)
	{
		if (!silent)
		{
			status_exception::raise(
				Arg::Gds(isc_dyn_exc_not_found) << name.toQuotedString());
		}
		return;
	}

	// Convert DELETE operation #70: Delete exception from RDB$EXCEPTIONS
	AutoCacheRequest deleteRequest(tdbb, drq_e_exception, DYN_REQUESTS);
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

	executeDdlTrigger(tdbb, dsqlScratch, transaction, DTW_AFTER, DDL_TRIGGER_DROP_EXCEPTION, name);
}

//----------------------
// CreateCollationNode Class Implementation  
//----------------------

void CreateCollationNode::execute(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch,
	jrd_tra* transaction)
{
	Attachment* const attachment = transaction->tra_attachment;

	executeDdlTrigger(tdbb, dsqlScratch, transaction, DTW_BEFORE, DDL_TRIGGER_CREATE_COLLATION, name);

	// Convert FOR loop #185: Check if collation already exists
	AutoCacheRequest request(tdbb, drq_l_coll_info, DYN_REQUESTS);
	EXE_start(tdbb, request, transaction);
	EXE_send(tdbb, request, 0, name.schema.length(), name.schema.c_str());
	EXE_send(tdbb, request, 1, name.object.length(), name.object.c_str());

	if (EXE_receive(tdbb, request, 2, 0, NULL))
	{
		EXE_unwind(tdbb, request);
		status_exception::raise(
			Arg::Gds(isc_dyn_coll_exists) << name.toQuotedString());
	}
	EXE_unwind(tdbb, request);

	// Convert STORE operation #185: Store collation in RDB$COLLATIONS
	AutoCacheRequest storeRequest(tdbb, drq_s_collations, DYN_REQUESTS);
	EXE_start(tdbb, storeRequest, transaction);

	struct RDB$COLLATIONS_RECORD {
		char RDB$SCHEMA_NAME[MAX_SQL_IDENTIFIER_LEN];
		char RDB$COLLATION_NAME[MAX_SQL_IDENTIFIER_LEN];
		SSHORT RDB$COLLATION_ID;
		SSHORT RDB$COLLATION_ID_NULL;
		SSHORT RDB$CHARACTER_SET_ID;
		SSHORT RDB$CHARACTER_SET_ID_NULL;
		SSHORT RDB$COLLATION_ATTRIBUTES;
		SSHORT RDB$COLLATION_ATTRIBUTES_NULL;
		SSHORT RDB$SYSTEM_FLAG;
		SSHORT RDB$SYSTEM_FLAG_NULL;
		bid RDB$DESCRIPTION;
		SSHORT RDB$DESCRIPTION_NULL;
		char RDB$FUNCTION_NAME[MAX_SQL_IDENTIFIER_LEN];
		SSHORT RDB$FUNCTION_NAME_NULL;
		char RDB$BASE_COLLATION_NAME[MAX_SQL_IDENTIFIER_LEN];
		SSHORT RDB$BASE_COLLATION_NAME_NULL;
		bid RDB$SPECIFIC_ATTRIBUTES;
		SSHORT RDB$SPECIFIC_ATTRIBUTES_NULL;
	} collRecord;

	memset(&collRecord, 0, sizeof(collRecord));
	
	strcpy(collRecord.RDB$SCHEMA_NAME, name.schema.c_str());
	strcpy(collRecord.RDB$COLLATION_NAME, name.object.c_str());

	collRecord.RDB$COLLATION_ID_NULL = FALSE;
	collRecord.RDB$COLLATION_ID = tdbb->getDatabase()->generateId();

	if (characterSet.hasData())
	{
		// Look up character set ID - simplified version
		collRecord.RDB$CHARACTER_SET_ID_NULL = FALSE;
		collRecord.RDB$CHARACTER_SET_ID = 0; // Would need proper character set lookup
	}
	else
	{
		collRecord.RDB$CHARACTER_SET_ID_NULL = TRUE;
	}

	if (fromCollation.hasData())
	{
		strcpy(collRecord.RDB$BASE_COLLATION_NAME, fromCollation.c_str());
		collRecord.RDB$BASE_COLLATION_NAME_NULL = FALSE;
	}
	else
	{
		collRecord.RDB$BASE_COLLATION_NAME_NULL = TRUE;
	}

	collRecord.RDB$SYSTEM_FLAG_NULL = FALSE;
	collRecord.RDB$SYSTEM_FLAG = 0;

	collRecord.RDB$COLLATION_ATTRIBUTES_NULL = TRUE;
	collRecord.RDB$DESCRIPTION_NULL = TRUE;
	collRecord.RDB$FUNCTION_NAME_NULL = TRUE;
	collRecord.RDB$SPECIFIC_ATTRIBUTES_NULL = TRUE;

	EXE_send(tdbb, storeRequest, 0, sizeof(collRecord), &collRecord);
	EXE_unwind(tdbb, storeRequest);

	executeDdlTrigger(tdbb, dsqlScratch, transaction, DTW_AFTER, DDL_TRIGGER_CREATE_COLLATION, name);
}

//----------------------
// DropCollationNode Class Implementation  
//----------------------

void DropCollationNode::execute(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch,
	jrd_tra* transaction)
{
	Attachment* const attachment = transaction->tra_attachment;

	executeDdlTrigger(tdbb, dsqlScratch, transaction, DTW_BEFORE, DDL_TRIGGER_DROP_COLLATION, name);

	// Convert FOR loop #190: Find collation to drop
	AutoCacheRequest request(tdbb, drq_l_coll_info2, DYN_REQUESTS);
	EXE_start(tdbb, request, transaction);
	EXE_send(tdbb, request, 0, name.schema.length(), name.schema.c_str());
	EXE_send(tdbb, request, 1, name.object.length(), name.object.c_str());

	struct CollationDropData {
		SSHORT systemFlag;
		SSHORT systemFlagNull;
	} collData;

	bool collationFound = false;
	if (EXE_receive(tdbb, request, 2, sizeof(collData), &collData))
	{
		collationFound = true;
		
		if (!collData.systemFlagNull && collData.systemFlag &&
			!(attachment->att_flags & ATT_system))
		{
			EXE_unwind(tdbb, request);
			status_exception::raise(
				Arg::Gds(isc_dyn_cannot_mod_syscoll) << name.toQuotedString());
		}
	}
	EXE_unwind(tdbb, request);

	if (!collationFound)
	{
		if (!silent)
		{
			status_exception::raise(
				Arg::Gds(isc_dyn_coll_not_found) << name.toQuotedString());
		}
		return;
	}

	// Convert DELETE operation #75: Delete collation from RDB$COLLATIONS
	AutoCacheRequest deleteRequest(tdbb, drq_e_collation, DYN_REQUESTS);
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

	executeDdlTrigger(tdbb, dsqlScratch, transaction, DTW_AFTER, DDL_TRIGGER_DROP_COLLATION, name);
}

} // namespace Jrd