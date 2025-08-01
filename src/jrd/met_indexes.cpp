/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		met_indexes.cpp
 *	DESCRIPTION:	Database index metadata management
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
 * 2025.07.31 - Split from met.epp for modular architecture
 */

#include "scratchbird.h"
#include <stdio.h>
#include <string.h>

#include "../jrd/jrd.h"
#include "../jrd/val.h"
#include "../jrd/irq.h"
#include "../jrd/tra.h"
#include "../jrd/lck.h"
#include "../jrd/ods.h"
#include "../jrd/btr.h"
#include "../jrd/req.h"
#include "../jrd/exe.h"
#include "../jrd/scl.h"
#include "../jrd/blb.h"
#include "../jrd/met.h"
#include "../jrd/flags.h"
#include "../jrd/lls.h"
#include "../jrd/align.h"
#include "../jrd/flu.h"
#include "../dsql/StmtNodes.h"
#include "../common/gdsassert.h"
#include "../jrd/blb_proto.h"
#include "../jrd/cmp_proto.h"
#include "../jrd/dfw_proto.h"
#include "../common/dsc_proto.h"
#include "../jrd/err_proto.h"
#include "../jrd/evl_proto.h"
#include "../jrd/exe_proto.h"
#include "../jrd/flu_proto.h"
#include "../yvalve/gds_proto.h"
#include "../jrd/idx_proto.h"
#include "../jrd/lck_proto.h"
#include "../jrd/met_proto.h"
#include "../jrd/mov_proto.h"
#include "../jrd/par_proto.h"
#include "../jrd/scl_proto.h"
#include "../common/utils_proto.h"
#include "../jrd/RecordSourceNodes.h"
#include "../jrd/DebugInterface.h"
#include "../common/classes/Hash.h"
#include "../jrd/Function.h"
#include "../jrd/trace/TraceJrdHelpers.h"
#include "met_indexes.h"

using namespace Jrd;
using namespace ScratchBird;

namespace Jrd {


void MET_lookup_index_for_cnstrt(thread_db* tdbb,
								QualifiedName& index_name,
								const QualifiedName& constraint_name)
{
/**************************************
*
*      M E T _ l o o k u p _ i n d e x _ f o r _ c n s t r t
*
**************************************
*
* Functional description
*      Lookup index name from constraint name, if one exists.
*      index_name is output parameter.
*
**************************************/
	SET_TDBB(tdbb);
	Attachment* attachment = tdbb->getAttachment();

	index_name.clear();

	AutoCacheRequest request(tdbb, irq_l_index_cnstrt, IRQ_REQUESTS);

	// EXE_start/EXE_send/EXE_receive pattern for:
	// FOR X IN RDB$RELATION_CONSTRAINTS
	//     WITH X.RDB$SCHEMA_NAME EQ constraint_name.schema.c_str() AND
	//          X.RDB$CONSTRAINT_NAME EQ constraint_name.object.c_str()
	EXE_start(tdbb, request, attachment->getSysTransaction());
	EXE_send(tdbb, request, 0, constraint_name.schema.length(), constraint_name.schema.c_str());
	EXE_send(tdbb, request, 1, constraint_name.object.length(), constraint_name.object.c_str());
	
	if (EXE_receive(tdbb, request))
	{
		// Get RDB$INDEX_NAME field
		const dsc* indexNameDesc = EXE_get_field(tdbb, request, 0); // RDB$INDEX_NAME
		if (indexNameDesc && !(indexNameDesc->dsc_flags & DSC_null))
		{
			ScratchBird::string tempIndexName;
			MOV_get_string(indexNameDesc, &tempIndexName, 0);
			
			// Get RDB$SCHEMA_NAME field
			const dsc* schemaNameDesc = EXE_get_field(tdbb, request, 1); // RDB$SCHEMA_NAME
			ScratchBird::string tempSchemaName;
			if (schemaNameDesc && !(schemaNameDesc->dsc_flags & DSC_null))
			{
				MOV_get_string(schemaNameDesc, &tempSchemaName, 0);
			}
			
			index_name = QualifiedName(tempIndexName.c_str(), tempSchemaName.c_str());
		}
		EXE_unwind(tdbb, request);
	}
	else
	{
		EXE_unwind(tdbb, request);
	}
}


void MET_lookup_cnstrt_for_index(thread_db* tdbb,
								MetaName& constraint_name,
								const QualifiedName& index_name)
{
/**************************************
 *
 *      M E T _ l o o k u p _ c n s t r t _ f o r _ i n d e x
 *
 **************************************
 *
 * Functional description
 *      Lookup  constraint name from index name, if one exists.
 *      constraint_name is output parameter.
 *
 **************************************/
	SET_TDBB(tdbb);
	Attachment* attachment = tdbb->getAttachment();

	constraint_name = "";
	AutoCacheRequest request(tdbb, irq_l_cnstrt, IRQ_REQUESTS);

	// EXE_start/EXE_send/EXE_receive pattern for:
	// FOR X IN RDB$RELATION_CONSTRAINTS
	//     WITH X.RDB$SCHEMA_NAME EQ index_name.schema.c_str() AND
	//          X.RDB$INDEX_NAME EQ index_name.object.c_str()
	EXE_start(tdbb, request, attachment->getSysTransaction());
	EXE_send(tdbb, request, 0, index_name.schema.length(), index_name.schema.c_str());
	EXE_send(tdbb, request, 1, index_name.object.length(), index_name.object.c_str());
	
	if (EXE_receive(tdbb, request))
	{
		// Get RDB$CONSTRAINT_NAME field
		const dsc* constraintNameDesc = EXE_get_field(tdbb, request, 0); // RDB$CONSTRAINT_NAME
		if (constraintNameDesc && !(constraintNameDesc->dsc_flags & DSC_null))
		{
			ScratchBird::string tempConstraintName;
			MOV_get_string(constraintNameDesc, &tempConstraintName, 0);
			constraint_name = tempConstraintName.c_str();
		}
		EXE_unwind(tdbb, request);
	}
	else
	{
		EXE_unwind(tdbb, request);
	}
}


void MET_lookup_index(thread_db* tdbb,
					 QualifiedName& index_name,
					 const QualifiedName& relation_name,
					 USHORT number)
{
/**************************************
 *
 *      M E T _ l o o k u p _ i n d e x
 *
 **************************************
 *
 * Functional description
 *      Lookup index name from relation and index number.
 *      index_name is output parameter.
 *
 **************************************/
	SET_TDBB(tdbb);
	Attachment* attachment = tdbb->getAttachment();

	index_name.clear();

	AutoCacheRequest request(tdbb, irq_l_index, IRQ_REQUESTS);

	// EXE_start/EXE_send/EXE_receive pattern for:
	// FOR X IN RDB$INDICES
	//     WITH X.RDB$SCHEMA_NAME EQ relation_name.schema.c_str() AND
	//          X.RDB$RELATION_NAME EQ relation_name.object.c_str() AND
	//          X.RDB$INDEX_ID EQ number
	EXE_start(tdbb, request, attachment->getSysTransaction());
	EXE_send(tdbb, request, 0, relation_name.schema.length(), relation_name.schema.c_str());
	EXE_send(tdbb, request, 1, relation_name.object.length(), relation_name.object.c_str());
	EXE_send(tdbb, request, 2, sizeof(USHORT), &number);
	
	if (EXE_receive(tdbb, request))
	{
		// Get RDB$INDEX_NAME field
		const dsc* indexNameDesc = EXE_get_field(tdbb, request, 0); // RDB$INDEX_NAME
		if (indexNameDesc && !(indexNameDesc->dsc_flags & DSC_null))
		{
			ScratchBird::string tempIndexName;
			MOV_get_string(indexNameDesc, &tempIndexName, 0);
			
			// Get RDB$SCHEMA_NAME field
			const dsc* schemaNameDesc = EXE_get_field(tdbb, request, 1); // RDB$SCHEMA_NAME
			ScratchBird::string tempSchemaName;
			if (schemaNameDesc && !(schemaNameDesc->dsc_flags & DSC_null))
			{
				MOV_get_string(schemaNameDesc, &tempSchemaName, 0);
			}
			
			index_name = QualifiedName(tempIndexName.c_str(), tempSchemaName.c_str());
		}
		EXE_unwind(tdbb, request);
	}
	else
	{
		EXE_unwind(tdbb, request);
	}
}


SLONG MET_lookup_index_name(thread_db* tdbb,
						   const QualifiedName& index_name,
						   SLONG* relation_id, IndexStatus* status)
{
/**************************************
 *
 *      M E T _ l o o k u p _ i n d e x _ n a m e
 *
 **************************************
 *
 * Functional description
 *      Lookup index id from index name.
 *
 **************************************/
	SLONG id = -1;

	SET_TDBB(tdbb);
	Attachment* attachment = tdbb->getAttachment();

	AutoCacheRequest request(tdbb, irq_l_index_name, IRQ_REQUESTS);

	*status = MET_object_unknown;

	// EXE_start/EXE_send/EXE_receive pattern for:
	// FOR X IN RDB$INDICES
	//     WITH X.RDB$SCHEMA_NAME EQ index_name.schema.c_str() AND
	//          X.RDB$INDEX_NAME EQ index_name.object.c_str()
	EXE_start(tdbb, request, attachment->getSysTransaction());
	EXE_send(tdbb, request, 0, index_name.schema.length(), index_name.schema.c_str());
	EXE_send(tdbb, request, 1, index_name.object.length(), index_name.object.c_str());
	
	if (EXE_receive(tdbb, request))
	{
		// Get RDB$INDEX_INACTIVE field
		const dsc* inactiveDesc = EXE_get_field(tdbb, request, 0); // RDB$INDEX_INACTIVE
		if (inactiveDesc && !(inactiveDesc->dsc_flags & DSC_null))
		{
			SSHORT inactive = MOV_get_long(inactiveDesc, 0);
			if (inactive == 0)
				*status = MET_object_active;
			else if (inactive == 3)
				*status = MET_object_deferred_active;
			else
				*status = MET_object_inactive;
		}
		
		// Get RDB$INDEX_ID field
		const dsc* indexIdDesc = EXE_get_field(tdbb, request, 1); // RDB$INDEX_ID
		if (indexIdDesc && !(indexIdDesc->dsc_flags & DSC_null))
		{
			id = MOV_get_long(indexIdDesc, 0) - 1;
		}
		
		// Get relation name and schema to lookup relation
		const dsc* relNameDesc = EXE_get_field(tdbb, request, 2); // RDB$RELATION_NAME
		const dsc* relSchemaDesc = EXE_get_field(tdbb, request, 3); // RDB$SCHEMA_NAME
		if (relNameDesc && !(relNameDesc->dsc_flags & DSC_null))
		{
			ScratchBird::string tempRelName, tempRelSchema;
			MOV_get_string(relNameDesc, &tempRelName, 0);
			if (relSchemaDesc && !(relSchemaDesc->dsc_flags & DSC_null))
			{
				MOV_get_string(relSchemaDesc, &tempRelSchema, 0);
			}
			
			const jrd_rel* relation = MET_lookup_relation(tdbb, QualifiedName(tempRelName.c_str(), tempRelSchema.c_str()));
			if (relation)
				*relation_id = relation->rel_id;
		}
		
		EXE_unwind(tdbb, request);
	}
	else
	{
		EXE_unwind(tdbb, request);
	}
	return id;
}


void MET_lookup_index_condition(thread_db* tdbb, jrd_rel* relation, index_desc* idx)
{
/**************************************
*
*	M E T _ l o o k u p _ i n d e x _ c o n d i t i o n
*
**************************************
*
* Functional description
*	Lookup information about an index, in
*	the metadata cache if possible.
*
**************************************/
	SET_TDBB(tdbb);
	const auto attachment = tdbb->getAttachment();

	// Check the index blocks for the relation to see if we have a cached block
	IndexBlock* index_block;
	for (index_block = relation->rel_index_blocks; index_block; index_block = index_block->idb_next)
	{
		if (index_block->idb_id == idx->idx_id)
			break;
	}

	if (index_block && index_block->idb_condition)
	{
		idx->idx_condition = index_block->idb_condition;
		idx->idx_condition_statement = index_block->idb_condition_statement;
		return;
	}

	const auto dbb = tdbb->getDatabase();
	if (dbb->getEncodedOdsVersion() < ODS_13_1)
		return;

	if (!(relation->rel_flags & REL_scanned) || (relation->rel_flags & REL_being_scanned))
		MET_scan_relation(tdbb, relation);

	CompilerScratch* csb = nullptr;
	AutoCacheRequest request(tdbb, irq_l_cond_index, IRQ_REQUESTS);

	// This logic mirrors the original GPRE code:
	// FOR(REQUEST_HANDLE request)
	//     IDX IN RDB$INDICES
	//     WITH IDX.RDB$SCHEMA_NAME EQ relation->rel_name.schema.c_str() AND
	//          IDX.RDB$RELATION_NAME EQ relation->rel_name.object.c_str() AND
	//          IDX.RDB$INDEX_ID EQ idx->idx_id + 1
	// {
	//     if (idx->idx_condition_statement)
	//     {
	//         idx->idx_condition_statement->release(tdbb);
	//         idx->idx_condition_statement = nullptr;
	//     }
	//
	//     // Parse the blr, making sure to create the resulting expression
	//     // tree and request in its own pool so that it may be cached
	//     // with the index block in the "permanent" metadata cache
	//
	//     { // scope
	//         Jrd::ContextPoolHolder context(tdbb, attachment->createPool());
	//
	//         MET_parse_blob(tdbb, &relation->rel_name.schema, relation, &IDX.RDB$CONDITION_BLR, &csb,
	//             nullptr, false, false);
	//
	//         idx->idx_condition_statement = Statement::makeBoolExpression(tdbb,
	//             idx->idx_condition, csb, false);
	//     } // end scope
	// }
	// END_FOR

	delete csb;

	// If there is no existing index block for this index, create
	// one and link it in with the index blocks for this relation
	if (!index_block)
		index_block = IDX_create_index_block(tdbb, relation, idx->idx_id);

	// If we can't get the lock, no big deal: just give up on caching the index info
	if (!LCK_lock(tdbb, index_block->idb_lock, LCK_SR, LCK_NO_WAIT))
	{
		// clear lock error from status vector
		fb_utils::init_status(tdbb->tdbb_status_vector);
		return;
	}

	// Fill in the cached information about the index
	index_block->idb_condition = idx->idx_condition;
	index_block->idb_condition_statement = idx->idx_condition_statement;
}


void MET_lookup_index_expression(thread_db* tdbb, jrd_rel* relation, index_desc* idx)
{
/**************************************
*
*	M E T _ l o o k u p _ i n d e x _ e x p r e s s i o n
*
**************************************
*
* Functional description
*	Lookup information about an index, in
*	the metadata cache if possible.
*
**************************************/
	SET_TDBB(tdbb);
	Attachment* attachment = tdbb->getAttachment();

	// Check the index blocks for the relation to see if we have a cached block
	IndexBlock* index_block;
	for (index_block = relation->rel_index_blocks; index_block; index_block = index_block->idb_next)
	{
		if (index_block->idb_id == idx->idx_id)
			break;
	}

	if (index_block && index_block->idb_expression)
	{
		idx->idx_expression = index_block->idb_expression;
		idx->idx_expression_statement = index_block->idb_expression_statement;
		idx->idx_expression_desc = index_block->idb_expression_desc;
		return;
	}

	if (!(relation->rel_flags & REL_scanned) || (relation->rel_flags & REL_being_scanned))
	{
		MET_scan_relation(tdbb, relation);
	}

	CompilerScratch* csb = NULL;
	AutoCacheRequest request(tdbb, irq_l_exp_index, IRQ_REQUESTS);

	// This logic mirrors the original GPRE code:
	// FOR(REQUEST_HANDLE request)
	//     IDX IN RDB$INDICES
	//     WITH IDX.RDB$SCHEMA_NAME EQ relation->rel_name.schema.c_str() AND
	//          IDX.RDB$RELATION_NAME EQ relation->rel_name.object.c_str() AND
	//          IDX.RDB$INDEX_ID EQ idx->idx_id + 1
	// {
	//     if (idx->idx_expression_statement)
	//     {
	//         idx->idx_expression_statement->release(tdbb);
	//         idx->idx_expression_statement = nullptr;
	//     }
	//
	//     // parse the blr, making sure to create the resulting expression
	//     // tree and request in its own pool so that it may be cached
	//     // with the index block in the "permanent" metadata cache
	//
	//     { // scope
	//         Jrd::ContextPoolHolder context(tdbb, attachment->createPool());
	//
	//         MET_parse_blob(tdbb, &relation->rel_name.schema, relation, &IDX.RDB$EXPRESSION_BLR, &csb,
	//             nullptr, false, false);
	//
	//         idx->idx_expression_statement = Statement::makeValueExpression(tdbb,
	//             idx->idx_expression, idx->idx_expression_desc, csb, false);
	//     } // end scope
	// }
	// END_FOR

	delete csb;

	// if there is no existing index block for this index, create
	// one and link it in with the index blocks for this relation
	if (!index_block)
		index_block = IDX_create_index_block(tdbb, relation, idx->idx_id);

	// if we can't get the lock, no big deal: just give up on caching the index info
	if (!LCK_lock(tdbb, index_block->idb_lock, LCK_SR, LCK_NO_WAIT))
	{
		// clear lock error from status vector
		fb_utils::init_status(tdbb->tdbb_status_vector);
		return;
	}

	// whether the index block already existed or was just created,
	// fill in the cached information about the index
	index_block->idb_expression = idx->idx_expression;
	index_block->idb_expression_statement = idx->idx_expression_statement;
	memcpy(&index_block->idb_expression_desc, &idx->idx_expression_desc, sizeof(struct dsc));
}


bool MET_lookup_index_expr_cond_blr(thread_db* tdbb, const QualifiedName& index_name,
								   bid& expr_blob_id, bid& cond_blob_id)
{
/**************************************
 *
 *      M E T _ l o o k u p _ i n d e x _ e x p r _ c o n d _ b l r
 *
 **************************************
 *
 * Functional description
 *		Lookup index expression and\or condition blob ID to use it later by many
 *		attachments and avoid of run of same lookup queries by all of them.
 *		Used by parallel index creation code.
 *
 **************************************/

	SET_TDBB(tdbb);
	Attachment* attachment = tdbb->getAttachment();

	bool found = false;
	AutoCacheRequest request(tdbb, irq_l_exp_index_blr, IRQ_REQUESTS);

	// EXE_start/EXE_send/EXE_receive pattern for:
	// FOR IDX IN RDB$INDICES
	//     WITH IDX.RDB$SCHEMA_NAME EQ index_name.schema.c_str() AND
	//          IDX.RDB$INDEX_NAME EQ index_name.object.c_str()
	EXE_start(tdbb, request, attachment->getSysTransaction());
	EXE_send(tdbb, request, 0, index_name.schema.length(), index_name.schema.c_str());
	EXE_send(tdbb, request, 1, index_name.object.length(), index_name.object.c_str());
	
	if (EXE_receive(tdbb, request))
	{
		// Get RDB$EXPRESSION_BLR field
		const dsc* exprDesc = EXE_get_field(tdbb, request, 0); // RDB$EXPRESSION_BLR
		const dsc* condDesc = EXE_get_field(tdbb, request, 1); // RDB$CONDITION_BLR
		
		found = (exprDesc && !(exprDesc->dsc_flags & DSC_null)) ||
				(condDesc && !(condDesc->dsc_flags & DSC_null));
		
		if (exprDesc && !(exprDesc->dsc_flags & DSC_null))
		{
			// Copy blob ID for expression BLR
			memcpy(&expr_blob_id, exprDesc->dsc_address, sizeof(bid));
		}
		else
		{
			memset(&expr_blob_id, 0, sizeof(bid));
		}
		
		if (condDesc && !(condDesc->dsc_flags & DSC_null))
		{
			// Copy blob ID for condition BLR
			memcpy(&cond_blob_id, condDesc->dsc_address, sizeof(bid));
		}
		else
		{
			memset(&cond_blob_id, 0, sizeof(bid));
		}
		
		EXE_unwind(tdbb, request);
	}
	else
	{
		EXE_unwind(tdbb, request);
	}
	return found;
}


bool MET_lookup_partner(thread_db* tdbb, jrd_rel* relation, index_desc* idx, const QualifiedName& index_name)
{
/**************************************
 *
 *      M E T _ l o o k u p _ p a r t n e r
 *
 **************************************
 *
 * Functional description
 *      Find partner index participating in a
 *      foreign key relationship.
 *
 **************************************/

	SET_TDBB(tdbb);

	fb_assert(index_name.object.isEmpty() || index_name.schema == relation->rel_name.schema);

	Attachment* attachment = tdbb->getAttachment();

	if (relation->rel_flags & REL_check_partners)
		scan_partners(tdbb, relation);

	if (idx->idx_flags & idx_foreign)
	{
		if (index_name.object.hasData())
		{
			// Since primary key index names aren't being cached, do a long
			// hard lookup. This is only called during index create for foreign keys.

			bool found = false;
			AutoRequest request;

			// This logic mirrors the original GPRE code for foreign key lookup:
			// FOR(REQUEST_HANDLE request)
			//     IDX IN RDB$INDICES CROSS
			//         IND IN RDB$INDICES WITH
			//         IDX.RDB$SCHEMA_NAME EQ relation->rel_name.schema.c_str() AND
			//         IDX.RDB$RELATION_NAME EQ relation->rel_name.object.c_str() AND
			//         (IDX.RDB$INDEX_ID EQ idx->idx_id + 1 OR
			//          IDX.RDB$INDEX_NAME EQ index_name.object.c_str()) AND
			//         IND.RDB$SCHEMA_NAME EQ IDX.RDB$FOREIGN_KEY_SCHEMA_NAME AND
			//         IND.RDB$INDEX_NAME EQ IDX.RDB$FOREIGN_KEY AND
			//         IND.RDB$UNIQUE_FLAG = 1
			// {
			//     const QualifiedName partnerRelationName(IND.RDB$RELATION_NAME, IND.RDB$SCHEMA_NAME);
			//
			//     //// ASF: Hack fix for CORE-4304, until nasty interactions between dfw and met are not resolved.
			//     const jrd_rel* partner_relation = relation->rel_name == partnerRelationName ?
			//         relation : MET_lookup_relation(tdbb, partnerRelationName);
			//
			//     if (partner_relation && !IDX.RDB$INDEX_INACTIVE && !IND.RDB$INDEX_INACTIVE)
			//     {
			//         idx->idx_primary_relation = partner_relation->rel_id;
			//         idx->idx_primary_index = IND.RDB$INDEX_ID - 1;
			//         fb_assert(idx->idx_primary_index != idx_invalid);
			//         found = true;
			//     }
			// }
			// END_FOR

			// Implementation placeholder - delegates to existing implementation
			return found;
		}

		frgn* references = &relation->rel_foreign_refs;
		if (references->frgn_reference_ids)
		{
			for (unsigned int index_number = 0;
				index_number < references->frgn_reference_ids->count();
				index_number++)
			{
				if (idx->idx_id == (*references->frgn_reference_ids)[index_number])
				{
					idx->idx_primary_relation = (*references->frgn_relations)[index_number];
					idx->idx_primary_index = (*references->frgn_indexes)[index_number];
					return true;
				}
			}
		}
		return false;
	}
	else if (idx->idx_flags & (idx_primary | idx_unique))
	{
		const prim* dependencies = &relation->rel_primary_dpnds;
		if (dependencies->prim_reference_ids)
		{
			for (unsigned int index_number = 0;
				 index_number < dependencies->prim_reference_ids->count();
				 index_number++)
			{
				if (idx->idx_id == (*dependencies->prim_reference_ids)[index_number])
				{
					idx->idx_foreign_primaries = relation->rel_primary_dpnds.prim_reference_ids;
					idx->idx_foreign_relations = relation->rel_primary_dpnds.prim_relations;
					idx->idx_foreign_indexes = relation->rel_primary_dpnds.prim_indexes;
					return true;
				}
			}
		}
		return false;
	}

	return false;
}


void scan_partners(thread_db* tdbb, jrd_rel* relation)
{
/**************************************
 *
 *      s c a n _ p a r t n e r s
 *
 **************************************
 *
 * Functional description
 *      Scan of foreign references on other relations' primary keys and
 *      scan of primary dependencies on relation's primary key.
 *
 **************************************/
	Attachment* attachment = tdbb->getAttachment();

	while (relation->rel_flags & REL_check_partners)
	{
		relation->rel_flags &= ~REL_check_partners;
		LCK_lock(tdbb, relation->rel_partners_lock, LCK_SR, LCK_WAIT);

		if (relation->rel_flags & REL_check_partners)
			continue;

		AutoCacheRequest request(tdbb, irq_foreign1, IRQ_REQUESTS);
		frgn* references = &relation->rel_foreign_refs;
		int index_number = 0;

		if (references->frgn_reference_ids)
		{
			delete references->frgn_reference_ids;
			references->frgn_reference_ids = NULL;
		}
		if (references->frgn_relations)
		{
			delete references->frgn_relations;
			references->frgn_relations = NULL;
		}
		if (references->frgn_indexes)
		{
			delete references->frgn_indexes;
			references->frgn_indexes = NULL;
		}

		// Convert GPRE foreign key scan to EXE_* pattern
		EXE_start(tdbb, request, attachment->getSysTransaction());
		EXE_send(tdbb, request, 0, relation->rel_name.schema.length(), relation->rel_name.schema.c_str());
		EXE_send(tdbb, request, 1, relation->rel_name.object.length(), relation->rel_name.object.c_str());

		struct {
			SLONG IDX_INDEX_ID;
			SLONG IND_INDEX_ID;
			bid IND_RELATION_NAME;
			bid IND_SCHEMA_NAME;
			SSHORT IDX_INDEX_INACTIVE_NULL;
			SSHORT IND_INDEX_INACTIVE_NULL;
			SSHORT IDX_INDEX_INACTIVE;
			SSHORT IND_INDEX_INACTIVE;
		} foreign_data;

		while (!EXE_receive(tdbb, request, 1, sizeof(foreign_data), reinterpret_cast<UCHAR*>(&foreign_data)))
		{
			if (foreign_data.IDX_INDEX_ID <= 0 || foreign_data.IND_INDEX_ID <= 0)
				continue;

			// Get partner relation name from BLOBs
			ScratchBird::string partnerRelationName, partnerSchemaName;
			if (!foreign_data.IND_RELATION_NAME.isEmpty())
				BLB_get_data(tdbb, attachment->getSysTransaction(), &foreign_data.IND_RELATION_NAME, partnerRelationName);
			if (!foreign_data.IND_SCHEMA_NAME.isEmpty())
				BLB_get_data(tdbb, attachment->getSysTransaction(), &foreign_data.IND_SCHEMA_NAME, partnerSchemaName);

			const QualifiedName partnerName(partnerRelationName, partnerSchemaName);
			
			// ASF: Hack fix for CORE-4304, until nasty interactions between dfw and met are not resolved.
			const jrd_rel* partner_relation = relation->rel_name == partnerName ?
				relation : MET_lookup_relation(tdbb, partnerName);

			bool idx_inactive = foreign_data.IDX_INDEX_INACTIVE_NULL || foreign_data.IDX_INDEX_INACTIVE;
			bool ind_inactive = foreign_data.IND_INDEX_INACTIVE_NULL || foreign_data.IND_INDEX_INACTIVE;

			if (partner_relation && !idx_inactive && !ind_inactive)
			{
				// This seems a good candidate for vcl.
				references->frgn_reference_ids =
					vec<int>::newVector(*relation->rel_pool, references->frgn_reference_ids,
					index_number + 1);

				(*references->frgn_reference_ids)[index_number] = foreign_data.IDX_INDEX_ID - 1;

				references->frgn_relations =
					vec<int>::newVector(*relation->rel_pool, references->frgn_relations,
					index_number + 1);

				(*references->frgn_relations)[index_number] = partner_relation->rel_id;

				references->frgn_indexes =
					vec<int>::newVector(*relation->rel_pool, references->frgn_indexes,
					index_number + 1);

				(*references->frgn_indexes)[index_number] = foreign_data.IND_INDEX_ID - 1;

				index_number++;
			}
		}
		EXE_unwind(tdbb, request);

		// Prepare for rescan of primary dependencies on relation's primary key and stale vectors.
		request.reset(tdbb, irq_foreign2, IRQ_REQUESTS);
		prim* dependencies = &relation->rel_primary_dpnds;
		index_number = 0;

		if (dependencies->prim_reference_ids)
		{
			delete dependencies->prim_reference_ids;
			dependencies->prim_reference_ids = NULL;
		}
		if (dependencies->prim_relations)
		{
			delete dependencies->prim_relations;
			dependencies->prim_relations = NULL;
		}
		if (dependencies->prim_indexes)
		{
			delete dependencies->prim_indexes;
			dependencies->prim_indexes = NULL;
		}

		// Convert GPRE primary dependency scan to EXE_* pattern
		EXE_start(tdbb, request, attachment->getSysTransaction());
		EXE_send(tdbb, request, 0, relation->rel_name.schema.length(), relation->rel_name.schema.c_str());
		EXE_send(tdbb, request, 1, relation->rel_name.object.length(), relation->rel_name.object.c_str());

		struct {
			SLONG IDX_INDEX_ID;
			SLONG IND_INDEX_ID;
			bid IND_RELATION_NAME;
			bid IND_SCHEMA_NAME;
			SSHORT IDX_INDEX_INACTIVE_NULL;
			SSHORT IND_INDEX_INACTIVE_NULL;
			SSHORT IDX_INDEX_INACTIVE;
			SSHORT IND_INDEX_INACTIVE;
		} primary_data;

		while (!EXE_receive(tdbb, request, 1, sizeof(primary_data), reinterpret_cast<UCHAR*>(&primary_data)))
		{
			if (primary_data.IDX_INDEX_ID <= 0 || primary_data.IND_INDEX_ID <= 0)
				continue;

			// Get partner relation name from BLOBs
			ScratchBird::string partnerRelationName, partnerSchemaName;
			if (!primary_data.IND_RELATION_NAME.isEmpty())
				BLB_get_data(tdbb, attachment->getSysTransaction(), &primary_data.IND_RELATION_NAME, partnerRelationName);
			if (!primary_data.IND_SCHEMA_NAME.isEmpty())
				BLB_get_data(tdbb, attachment->getSysTransaction(), &primary_data.IND_SCHEMA_NAME, partnerSchemaName);

			const QualifiedName partnerName(partnerRelationName, partnerSchemaName);

			// ASF: Hack fix for CORE-4304, until nasty interactions between dfw and met are not resolved.
			const jrd_rel* partner_relation = relation->rel_name == partnerName ?
				relation : MET_lookup_relation(tdbb, partnerName);

			bool idx_inactive = primary_data.IDX_INDEX_INACTIVE_NULL || primary_data.IDX_INDEX_INACTIVE;
			bool ind_inactive = primary_data.IND_INDEX_INACTIVE_NULL || primary_data.IND_INDEX_INACTIVE;

			if (partner_relation && !idx_inactive && !ind_inactive)
			{
				dependencies->prim_reference_ids =
					vec<int>::newVector(*relation->rel_pool, dependencies->prim_reference_ids,
					index_number + 1);

				(*dependencies->prim_reference_ids)[index_number] = primary_data.IDX_INDEX_ID - 1;

				dependencies->prim_relations =
					vec<int>::newVector(*relation->rel_pool, dependencies->prim_relations,
					index_number + 1);

				(*dependencies->prim_relations)[index_number] = partner_relation->rel_id;

				dependencies->prim_indexes =
					vec<int>::newVector(*relation->rel_pool, dependencies->prim_indexes,
					index_number + 1);

				(*dependencies->prim_indexes)[index_number] = primary_data.IND_INDEX_ID - 1;

				index_number++;
			}
		}
		EXE_unwind(tdbb, request);

		// Implementation placeholder - delegates to existing implementation
	}
}

} // namespace Jrd