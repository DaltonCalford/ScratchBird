/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		met_relations.cpp
 *	DESCRIPTION:	Meta data relation/table management operations
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
 * and its predecessors. Portions created by Inprise Corporation
 * are Copyright (C) Inprise Corporation.
 *
 * All Rights Reserved.
 * Contributor(s): ______________________________________.
 * Adriano dos Santos Fernandes - moved from met.epp
 */

#include "scratchbird.h"
#include "../jrd/jrd.h"
#include "../jrd/req.h"
#include "../jrd/val.h"
#include "../jrd/exe.h"
#include "../dsql/sqlda_pub.h"
#include "../jrd/blb.h"
#include "../jrd/btr.h"
#include "../jrd/lck.h"
#include "../jrd/met.h"
#include "../jrd/met_proto.h"
#include "../jrd/movement_proto.h"
#include "../jrd/thread_proto.h"
#include "../jrd/tra.h"
#include "../common/dsc.h"
#include "../jrd/blb_proto.h"
#include "../jrd/err_proto.h"
#include "../jrd/ext_proto.h"
#include "../jrd/gds_proto.h"
#include "../jrd/idx.h"
#include "../jrd/lck_proto.h"
#include "../jrd/scl_proto.h"
#include "../jrd/tra_proto.h"
#include "../jrd/Relation.h"
#include "../jrd/Function.h"
#include "../jrd/Procedure.h"
#include "../jrd/Database.h"
#include "../jrd/Attachment.h"
#include "../jrd/constants.h"
#include "../dsql/node.h"
#include "../dsql/ExprNodes.h"
#include "../dsql/BoolNodes.h"
#include "../dsql/DdlNodes.h"
#include "../common/classes/auto.h"
#include "../common/classes/array.h"
#include "../common/classes/fb_string.h"
#include "../common/classes/MetaName.h"
#include "../jrd/TriggerCache.h"
#include "../jrd/recsrc/RecordSource.h"
#include "../common/StatusArg.h"

using namespace ScratchBird;

namespace Jrd {

// Forward declarations
static ULONG get_rel_flags_from_FLAGS(USHORT flags);
static void scan_partners(thread_db* tdbb, jrd_rel* relation);
static void lookup_view_contexts(thread_db* tdbb, jrd_rel* view);
static ValueExprNode* parse_field_default_blr(thread_db* tdbb, const MetaName& schema, bid* blob_id);
static BoolExprNode* parse_field_validation_blr(thread_db* tdbb, bid* blob_id, const QualifiedName& name);


jrd_rel* MET_lookup_relation(thread_db* tdbb, const QualifiedName& name)
{
/**************************************
 *
 *      M E T _ l o o k u p _ r e l a t i o n
 *
 **************************************
 *
 * Functional description
 *      Lookup relation by name.  Name passed in is
 *      ASCIZ name.
 *
 **************************************/
	SET_TDBB(tdbb);
	Attachment* attachment = tdbb->getAttachment();

	// See if we already know the relation by name

	vec<jrd_rel*>* relations = attachment->att_relations;
	jrd_rel* check_relation = NULL;

	vec<jrd_rel*>::iterator ptr = relations->begin();
	for (const vec<jrd_rel*>::const_iterator end = relations->end(); ptr < end; ++ptr)
	{
		jrd_rel* const relation = *ptr;

		if (relation)
		{
			if (relation->rel_flags & REL_deleting)
				CheckoutLockGuard guard(tdbb, relation->rel_drop_mutex, FB_FUNCTION);

			if (!(relation->rel_flags & REL_deleted))
			{
				// dimitr: for non-system relations we should also check
				//		   REL_scanned and REL_being_scanned flags. Look
				//		   at MET_lookup_procedure for example.
				if (!(relation->rel_flags & REL_system) &&
					(!(relation->rel_flags & REL_scanned) || (relation->rel_flags & REL_being_scanned)))
				{
					continue;
				}

				if (relation->rel_name == name)
				{
					if (relation->rel_flags & REL_check_existence)
					{
						check_relation = relation;
						LCK_lock(tdbb, check_relation->rel_existence_lock, LCK_SR, LCK_WAIT);
						break;
					}

					return relation;
				}
			}
		}
	}

	// We need to look up the relation name in RDB$RELATIONS

	jrd_rel* relation = NULL;

	AutoCacheRequest request(tdbb, irq_l_relation, IRQ_REQUESTS);

	// Convert GPRE FOR to EXE_start/EXE_send/EXE_receive pattern
	EXE_start(tdbb, request, attachment->getSysTransaction());
	
	// Send schema and relation name parameters
	impure_value* impure = request.getImpure<impure_value>(0);
	impure->vlu_desc.dsc_address = (UCHAR*) name.schema.c_str();
	impure->vlu_desc.dsc_length = name.schema.length();
	impure->vlu_desc.dsc_dtype = dtype_text;
	
	impure = request.getImpure<impure_value>(1);
	impure->vlu_desc.dsc_address = (UCHAR*) name.object.c_str();
	impure->vlu_desc.dsc_length = name.object.length();
	impure->vlu_desc.dsc_dtype = dtype_text;

	EXE_send(tdbb, request, 0, 2 * sizeof(impure_value), (UCHAR*) request.getImpure<impure_value>(0));

	// Receive relation data
	while (EXE_receive(tdbb, request, 1, sizeof(struct irq_l_relation_struct), 
					   (UCHAR*) request.getImpure<struct irq_l_relation_struct>(2)))
	{
		struct irq_l_relation_struct* data = request.getImpure<struct irq_l_relation_struct>(2);
		
		if (!data->irq_l_relation_end_of_stream)
		{
			relation = MET_relation(tdbb, data->irq_l_relation_id);
			if (relation->rel_name.object.isEmpty()) {
				relation->rel_name = name;
			}

			relation->rel_flags |= get_rel_flags_from_FLAGS(data->irq_l_relation_flags);

			if (!data->irq_l_relation_type_null)
			{
				relation->rel_flags |= MET_get_rel_flags_from_TYPE(data->irq_l_relation_type);
			}
		}
	}

	if (check_relation)
	{
		check_relation->rel_flags &= ~REL_check_existence;
		if (check_relation != relation)
		{
			LCK_release(tdbb, check_relation->rel_existence_lock);
			if (!(check_relation->rel_flags & REL_check_partners))
			{
				check_relation->rel_flags |= REL_check_partners;
				LCK_release(tdbb, check_relation->rel_partners_lock);
				check_relation->rel_flags &= ~REL_check_partners;
			}
			LCK_release(tdbb, check_relation->rel_rescan_lock);
			check_relation->rel_flags |= REL_deleted;
		}
	}

	return relation;
}


jrd_rel* MET_lookup_relation_id(thread_db* tdbb, SLONG id, bool return_deleted)
{
/**************************************
 *
 *      M E T _ l o o k u p _ r e l a t i o n _ i d
 *
 **************************************
 *
 * Functional description
 *      Lookup relation by id. Make sure it really exists.
 *
 **************************************/
	SET_TDBB(tdbb);
	Attachment* const attachment = tdbb->getAttachment();

	// System relations are above suspicion

	if (id < (int) rel_MAX)
	{
		fb_assert(id < MAX_USHORT);
		return MET_relation(tdbb, (USHORT) id);
	}

	jrd_rel* check_relation = NULL;
	jrd_rel* relation;
	vec<jrd_rel*>* vector = attachment->att_relations;
	if (vector && (id < (SLONG) vector->count()) && (relation = (*vector)[id]))
	{
		if (relation->rel_flags & REL_deleting)
			CheckoutLockGuard guard(tdbb, relation->rel_drop_mutex, FB_FUNCTION);

		if (relation->rel_flags & REL_deleted)
			return return_deleted ? relation : NULL;

		if (relation->rel_flags & REL_check_existence)
		{
			check_relation = relation;
			LCK_lock(tdbb, check_relation->rel_existence_lock, LCK_SR, LCK_WAIT);
		}
		else
			return relation;
	}

	// We need to look up the relation id in RDB$RELATIONS

	relation = NULL;

	AutoCacheRequest request(tdbb, irq_l_rel_id, IRQ_REQUESTS);

	// Convert GPRE FOR to EXE_start/EXE_send/EXE_receive pattern
	EXE_start(tdbb, request, attachment->getSysTransaction());
	
	// Send relation id parameter
	impure_value* impure = request.getImpure<impure_value>(0);
	impure->vlu_desc.dsc_address = (UCHAR*) &id;
	impure->vlu_desc.dsc_length = sizeof(SLONG);
	impure->vlu_desc.dsc_dtype = dtype_long;

	EXE_send(tdbb, request, 0, sizeof(impure_value), (UCHAR*) impure);

	// Receive relation data
	while (EXE_receive(tdbb, request, 1, sizeof(struct irq_l_rel_id_struct), 
					   (UCHAR*) request.getImpure<struct irq_l_rel_id_struct>(1)))
	{
		struct irq_l_rel_id_struct* data = request.getImpure<struct irq_l_rel_id_struct>(1);
		
		if (!data->irq_l_rel_id_end_of_stream)
		{
			relation = MET_relation(tdbb, data->irq_l_rel_id_id);
			if (relation->rel_name.object.isEmpty())
				relation->rel_name = QualifiedName(data->irq_l_rel_id_name, data->irq_l_rel_id_schema_name);

			relation->rel_flags |= get_rel_flags_from_FLAGS(data->irq_l_rel_id_flags);

			if (!data->irq_l_rel_id_type_null)
			{
				relation->rel_flags |= MET_get_rel_flags_from_TYPE(data->irq_l_rel_id_type);
			}
		}
	}

	if (check_relation)
	{
		check_relation->rel_flags &= ~REL_check_existence;
		if (check_relation != relation)
		{
			LCK_release(tdbb, check_relation->rel_existence_lock);
			if (!(check_relation->rel_flags & REL_check_partners))
			{
				check_relation->rel_flags |= REL_check_partners;
				LCK_release(tdbb, check_relation->rel_partners_lock);
				check_relation->rel_flags &= ~REL_check_partners;
			}
			LCK_release(tdbb, check_relation->rel_rescan_lock);
			check_relation->rel_flags |= REL_deleted;
		}
	}

	return relation;
}


jrd_rel* MET_relation(thread_db* tdbb, USHORT id)
{
/**************************************
 *
 *      M E T _ r e l a t i o n
 *
 **************************************
 *
 * Functional description
 *      Find or create a relation block for a given relation id.
 *
 **************************************/
	SET_TDBB(tdbb);
	Database* dbb = tdbb->getDatabase();
	CHECK_DBB(dbb);

	Attachment* attachment = tdbb->getAttachment();
	vec<jrd_rel*>* vector = attachment->att_relations;
	MemoryPool* pool = attachment->att_pool;

	if (!vector)
		vector = attachment->att_relations = vec<jrd_rel*>::newVector(*pool, id + 10);
	else if (id >= vector->count())
		vector->resize(id + 10);

	jrd_rel* relation = (*vector)[id];
	if (relation)
		return relation;

	relation = FB_NEW_POOL(*pool) jrd_rel(*pool);
	(*vector)[id] = relation;
	relation->rel_id = id;

	{ // Scope block.
		Lock* lock = FB_NEW_RPT(*pool, 0)
			Lock(tdbb, sizeof(SLONG), LCK_rel_partners, relation, partners_ast_relation);
		relation->rel_partners_lock = lock;
		lock->setKey(relation->rel_id);
	}

	{ // Scope block.
		Lock* lock = FB_NEW_RPT(*pool, 0)
			Lock(tdbb, sizeof(SLONG), LCK_rel_rescan, relation, rescan_ast_relation);
		relation->rel_rescan_lock = lock;
		lock->setKey(relation->rel_id);
	}

	if (relation->rel_id < rel_MAX)
		return relation;

	{ // Scope block.
		Lock* lock = FB_NEW_RPT(*pool, 0)
			Lock(tdbb, sizeof(SLONG), LCK_rel_exist, relation, blocking_ast_relation);
		relation->rel_existence_lock = lock;
		lock->setKey(relation->rel_id);
	}

	relation->rel_flags |= (REL_check_existence | REL_check_partners);
	return relation;
}


void MET_post_existence(thread_db* tdbb, jrd_rel* relation)
{
/**************************************
 *
 *      M E T _ p o s t _ e x i s t e n c e
 *
 **************************************
 *
 * Functional description
 *      Post an interest in the existence of a relation.
 *
 **************************************/
	SET_TDBB(tdbb);

	relation->rel_use_count++;

	if (!MET_lookup_relation_id(tdbb, relation->rel_id, false))
	{
		relation->rel_use_count--;
		ERR_post(Arg::Gds(isc_relnotdef) << relation->rel_name.toQuotedString());
	}
}


void MET_release_existence(thread_db* tdbb, jrd_rel* relation)
{
/**************************************
 *
 *      M E T _ r e l e a s e _ e x i s t e n c e
 *
 **************************************
 *
 * Functional description
 *      Release interest in relation. If no remaining interest
 *      and we're blocking the drop of the relation then release
 *      existence lock and mark deleted.
 *
 **************************************/
	if (!relation->rel_use_count)
		return;

	--relation->rel_use_count;

	if (!relation->rel_use_count)
	{
		if (relation->rel_flags & REL_blocking)
			LCK_re_post(tdbb, relation->rel_existence_lock);

		// release trigger requests
		relation->releaseTriggers(tdbb, false);

		// close external file
		EXT_fini(relation, true);
	}
}


void MET_scan_partners(thread_db* tdbb, jrd_rel* relation)
{
/**************************************
 *
 *      M E T _ s c a n _ p a r t n e r s
 *
 **************************************
 *
 * Functional description
 *      Scan of foreign references on other relations' primary keys and
 *      scan of primary dependencies on relation's primary key.
 *
 **************************************/

	SET_TDBB(tdbb);

	if (relation->rel_flags & REL_check_partners)
		scan_partners(tdbb, relation);
}


void MET_scan_relation(thread_db* tdbb, jrd_rel* relation)
{
/**************************************
 *
 *      M E T _ s c a n _ r e l a t i o n
 *
 **************************************
 *
 * Functional description
 *      Scan a relation for view RecordSelExpr, computed by expressions, missing
 *      expressions, and validation expressions.
 *
 **************************************/
	SET_TDBB(tdbb);
	TrigVector* triggers[TRIGGER_MAX];
	Attachment* attachment = tdbb->getAttachment();
	Database* dbb = tdbb->getDatabase();
	Jrd::ContextPoolHolder context(tdbb, attachment->att_pool);
	bool dependencies = false;

	blb* blob = NULL;

	jrd_tra* depTrans = tdbb->getTransaction() ?
		tdbb->getTransaction() : attachment->getSysTransaction();

	// If anything errors, catch it to reset the scan flag.  This will
	// make sure that the error will be caught if the operation is tried again.

	try {

	if (relation->rel_flags & (REL_scanned | REL_deleted))
		return;

	relation->rel_flags |= REL_being_scanned;
	dependencies = (relation->rel_flags & REL_get_dependencies) ? true : false;
	relation->rel_flags &= ~REL_get_dependencies;

	for (USHORT itr = 0; itr < TRIGGER_MAX; ++itr)
		triggers[itr] = NULL;

	// Since this can be called recursively, find an inactive clone of the request

	AutoCacheRequest request(tdbb, irq_r_fields, IRQ_REQUESTS);
	CompilerScratch* csb = NULL;

	// Convert GPRE FOR to EXE_start/EXE_send/EXE_receive pattern
	EXE_start(tdbb, request, attachment->getSysTransaction());
	
	// Send relation id parameter
	impure_value* impure = request.getImpure<impure_value>(0);
	impure->vlu_desc.dsc_address = (UCHAR*) &relation->rel_id;
	impure->vlu_desc.dsc_length = sizeof(USHORT);
	impure->vlu_desc.dsc_dtype = dtype_short;

	EXE_send(tdbb, request, 0, sizeof(impure_value), (UCHAR*) impure);

	// Receive relation and schema data
	while (EXE_receive(tdbb, request, 1, sizeof(struct irq_r_fields_struct), 
					   (UCHAR*) request.getImpure<struct irq_r_fields_struct>(1)))
	{
		struct irq_r_fields_struct* REL = request.getImpure<struct irq_r_fields_struct>(1);
		
		if (!REL->irq_r_fields_end_of_stream)
		{
			// Pick up relation level stuff
			relation->rel_current_fmt = REL->irq_r_fields_format;
			vec<jrd_fld*>* vector = relation->rel_fields =
				vec<jrd_fld*>::newVector(*relation->rel_pool, relation->rel_fields, REL->irq_r_fields_field_id + 1);
			if (!REL->irq_r_fields_security_class_null)
				relation->rel_security_name = QualifiedName(REL->irq_r_fields_security_class, REL->irq_r_fields_schema_security_class);

			relation->rel_name = QualifiedName(REL->irq_r_fields_relation_name, REL->irq_r_fields_schema_name);
			relation->rel_owner_name = REL->irq_r_fields_owner_name;

			if (!REL->irq_r_fields_sql_security_null)
				relation->rel_ss_definer = (bool) REL->irq_r_fields_sql_security;
			else
				relation->rel_ss_definer = MET_get_ss_definer(tdbb, REL->irq_r_fields_schema_name);

			if (!REL->irq_r_fields_view_blr_null)
			{
				// parse the view blr, getting dependencies on relations, etc. at the same time

				DmlNode* rseNode;

				if (dependencies)
				{
					const QualifiedName depName(REL->irq_r_fields_relation_name, REL->irq_r_fields_schema_name);
					rseNode = MET_get_dependencies(tdbb, relation, NULL, 0, NULL, &REL->irq_r_fields_view_blr,
						NULL, &csb, depName, obj_view, 0, depTrans);
				}
				else
				{
					rseNode = MET_parse_blob(tdbb, &relation->rel_name.schema, relation, &REL->irq_r_fields_view_blr, &csb,
						NULL, false, false);
				}

				if (rseNode)
				{
					fb_assert(rseNode->getKind() == DmlNode::KIND_REC_SOURCE);
					relation->rel_view_rse = nodeAs<RseNode>(static_cast<RecordSourceNode*>(rseNode));
					fb_assert(relation->rel_view_rse);
				}
				else
					relation->rel_view_rse = NULL;

				// retrieve the view context names

				lookup_view_contexts(tdbb, relation);
			}

			relation->rel_flags |= REL_scanned;
			if (REL->irq_r_fields_external_file[0])
			{
				EXT_file(relation, REL->irq_r_fields_external_file); 
			}

			if (!REL->irq_r_fields_relation_type_null)
			{
				switch (REL->irq_r_fields_relation_type)
				{
					case rel_persistent:
						break;
					case rel_external:
						fb_assert(relation->rel_file);
						break;
					case rel_view:
						fb_assert(relation->rel_view_rse);
						fb_assert(relation->rel_flags & REL_jrd_view);
						relation->rel_flags |= REL_jrd_view;
						break;
					case rel_virtual:
						fb_assert(relation->rel_flags & REL_virtual);
						relation->rel_flags |= REL_virtual;
						break;
					case rel_global_temp_preserve:
						fb_assert(relation->rel_flags & REL_temp_conn);
						relation->rel_flags |= REL_temp_conn;
						break;
					case rel_global_temp_delete:
						fb_assert(relation->rel_flags & REL_temp_tran);
						relation->rel_flags |= REL_temp_tran;
						break;
					default:
						fb_assert(false);
				}
			}

			// Pick up field specific stuff - Runtime blob parsing
			// NOTE: This section involves complex blob parsing that may need additional GPRE conversion work

			blob = blb::open(tdbb, attachment->getSysTransaction(), &REL->irq_r_fields_runtime);
			HalfStaticArray<UCHAR, 256> temp;
			UCHAR* const buffer = temp.getBuffer(blob->getMaxSegment() + 1U);

			jrd_fld* field = NULL;
			ArrayField* array = 0;
			USHORT view_context = 0;
			USHORT field_id = 0;
			for (;;)
			{
				USHORT length = blob->BLB_get_segment(tdbb, buffer, blob->getMaxSegment());
				if (blob->blb_flags & BLB_eof)
				{
					break;
				}
				USHORT n;
				buffer[length] = 0;
				UCHAR* p = (UCHAR*) &n;
				const UCHAR* q = buffer + 1;
				while (q < buffer + 1 + sizeof(SSHORT))
				{
					*p++ = *q++;
				}
				p = buffer + 1;
				--length;
				switch ((rsr_t) buffer[0])
				{
				case RSR_field_id:
					if (field && field->fld_security_name.isEmpty() && !REL->irq_r_fields_default_class_null)
						field->fld_security_name = REL->irq_r_fields_default_class;
					field_id = n;
					field = (*vector)[field_id];

					if (field)
					{
						field->fld_computation = NULL;
						field->fld_missing_value = NULL;
						field->fld_default_value = NULL;
						field->fld_validation = NULL;
						field->fld_not_null = NULL;
						field->fld_generator_name.clear();
					}

					array = NULL;
					break;

				case RSR_field_name:
					if (field)
					{
						// The field exists.  If its name hasn't changed, then
						// there's no need to copy anything.

						if (field->fld_name == reinterpret_cast<char*>(p))
							break;

						field->fld_name = reinterpret_cast<char*>(p);
					}
					else
					{
						field = FB_NEW_POOL(*relation->rel_pool) jrd_fld(*relation->rel_pool);
						(*vector)[field_id] = field;
						field->fld_name = reinterpret_cast<char*>(p);
					}

					// CVC: Be paranoid and allow the possible trigger(s) to have a
					//   not null security class to work on, even if we only take it
					//   from the relation itself.
					if (field->fld_security_name.isEmpty() && !REL->irq_r_fields_default_class_null)
						field->fld_security_name = REL->irq_r_fields_default_class;

					break;

				case RSR_view_context:
					view_context = n;
					break;

				case RSR_base_field:
					if (dependencies)
					{
						csb->csb_g_flags |= csb_get_dependencies;
						field->fld_source = PAR_make_field(tdbb, csb, view_context, (TEXT*) p);
						const QualifiedName depName(REL->irq_r_fields_relation_name, REL->irq_r_fields_schema_name);
						MET_store_dependencies(tdbb, csb->csb_dependencies, 0, depName, obj_view, depTrans);
					}
					else
						field->fld_source = PAR_make_field(tdbb, csb, view_context, (TEXT*) p);

					{	// scope
						const ViewContexts& ctx = relation->rel_view_contexts;
						FB_SIZE_T pos;

						if (ctx.find(view_context, pos) &&
							(ctx[pos]->vcx_type == VCT_TABLE || ctx[pos]->vcx_type == VCT_VIEW))
						{
							field->fld_source_rel_field = QualifiedNameMetaNamePair(ctx[pos]->vcx_relation_name, (TEXT*) p);
						}
					}

					break;

				case RSR_computed_blr:
					{
						AutoSetRestoreFlag<USHORT> flag(&field->fld_flags, FLD_parse_computed, true);

						DmlNode* nod = dependencies ?
							MET_get_dependencies(tdbb, relation, p, length, csb, NULL, NULL, NULL,
								QualifiedName(field->fld_name, relation->rel_name.schema),
								obj_computed, csb_computed_field, depTrans) :
							PAR_blr(tdbb, &relation->rel_name.schema, relation, p, length, csb, NULL, NULL, false,
								csb_computed_field);

						field->fld_computation = static_cast<ValueExprNode*>(nod);
					}
					break;

				case RSR_missing_value:
					field->fld_missing_value = static_cast<ValueExprNode*>(
						PAR_blr(tdbb, &relation->rel_name.schema, relation, p, length, csb, NULL, NULL, false, 0));
					break;

				case RSR_default_value:
					field->fld_default_value = static_cast<ValueExprNode*>(
						PAR_blr(tdbb, &relation->rel_name.schema, relation, p, length, csb, NULL, NULL, false, 0));
					break;

				case RSR_validation_blr:
					// AB: 2005-04-25   bug SF#1168898
					// Ignore validation for VIEWs, because fields (domains) which are
					// defined with CHECK constraints and have sub-selects should at least
					// be parsed without view-context information. With view-context
					// information the context-numbers are wrong.
					// Because a VIEW can't have a validation section i ignored the whole call.
					if (!csb)
					{
						field->fld_validation = PAR_validation_blr(tdbb, &relation->rel_name.schema, relation, p, length,
							csb, NULL, csb_validation);
					}
					break;

				case RSR_field_not_null:
					field->fld_not_null = PAR_validation_blr(tdbb, &relation->rel_name.schema, relation, p, length,
						csb, NULL, csb_validation);
					break;

				case RSR_security_class:
					field->fld_security_name = (const TEXT*) p;
					break;

				case RSR_trigger_name:
					MET_load_trigger(tdbb, relation, QualifiedName((const TEXT*) p, REL->irq_r_fields_schema_name), triggers);
					break;

				case RSR_dimensions:
					field->fld_array = array = FB_NEW_RPT(*relation->rel_pool, n) ArrayField();
					array->arr_desc.iad_dimensions = n;
					break;

				case RSR_array_desc:
					if (array)
						memcpy(&array->arr_desc, p, length);
					break;

				case RSR_field_generator_name:
					field->fld_generator_name = QualifiedName((const TEXT*) p, REL->irq_r_fields_schema_name);
					if (!field->fld_identity_type.has_value())
						field->fld_identity_type = IDENT_TYPE_BY_DEFAULT;
					break;

				case RSR_field_uuid_generator:
					field->fld_uuid_generator = ScratchBird::string((const TEXT*) p, length);
					if (!field->fld_identity_type.has_value())
						field->fld_identity_type = IDENT_TYPE_BY_DEFAULT;
					break;

				case RSR_field_identity_type:
					field->fld_identity_type = static_cast<IdentityType>(n);
					break;

				default:    // Shut up compiler warning
					break;
				}
			}
			blob->BLB_close(tdbb);
			blob = 0;

			if (field && field->fld_security_name.isEmpty() && !REL->irq_r_fields_default_class_null)
				field->fld_security_name = REL->irq_r_fields_default_class;
		}
	}

	delete csb;

	// We have just loaded the triggers onto the local vector triggers.
	// It's now time to place them at their rightful place inside the relation block.
	relation->replaceTriggers(tdbb, triggers);

	LCK_lock(tdbb, relation->rel_rescan_lock, LCK_SR, LCK_WAIT);
	relation->rel_flags &= ~REL_being_scanned;

	relation->rel_current_format = NULL;

	}	// try
	catch (const Exception&)
	{
		relation->rel_flags &= ~(REL_being_scanned | REL_scanned);
		if (dependencies) {
			relation->rel_flags |= REL_get_dependencies;
		}
		if (blob)
			blob->BLB_close(tdbb);

		throw;
	}
}


void MET_revoke(thread_db* tdbb, jrd_tra* transaction, const QualifiedName& relation,
	const QualifiedName& revokee, const string& privilege)
{
/**************************************
 *
 *      M E T _ r e v o k e
 *
 **************************************
 *
 * Functional description
 *      Execute a recursive revoke.  This is called only when
 *      a revoked privilege had the grant option.
 *
 **************************************/
	SET_TDBB(tdbb);

	// See if the revokee still has the privilege.  If so, there's nothing to do

	USHORT count = 0;

	AutoCacheRequest request(tdbb, irq_revoke1, IRQ_REQUESTS);

	// Convert GPRE FOR to EXE_start/EXE_send/EXE_receive pattern
	EXE_start(tdbb, request, transaction);
	
	// Send relation and privilege parameters
	impure_value* impure = request.getImpure<impure_value>(0);
	impure->vlu_desc.dsc_address = (UCHAR*) relation.schema.c_str();
	impure->vlu_desc.dsc_length = relation.schema.length();
	impure->vlu_desc.dsc_dtype = dtype_text;
	
	impure = request.getImpure<impure_value>(1);
	impure->vlu_desc.dsc_address = (UCHAR*) relation.object.c_str();
	impure->vlu_desc.dsc_length = relation.object.length();
	impure->vlu_desc.dsc_dtype = dtype_text;

	impure = request.getImpure<impure_value>(2);
	impure->vlu_desc.dsc_address = (UCHAR*) privilege.c_str();
	impure->vlu_desc.dsc_length = privilege.length();
	impure->vlu_desc.dsc_dtype = dtype_text;

	impure = request.getImpure<impure_value>(3);
	impure->vlu_desc.dsc_address = (UCHAR*) revokee.schema.c_str();
	impure->vlu_desc.dsc_length = revokee.schema.length();
	impure->vlu_desc.dsc_dtype = dtype_text;

	impure = request.getImpure<impure_value>(4);
	impure->vlu_desc.dsc_address = (UCHAR*) revokee.object.c_str();
	impure->vlu_desc.dsc_length = revokee.object.length();
	impure->vlu_desc.dsc_dtype = dtype_text;

	EXE_send(tdbb, request, 0, 5 * sizeof(impure_value), (UCHAR*) request.getImpure<impure_value>(0));

	// Receive privilege count data
	while (EXE_receive(tdbb, request, 1, sizeof(struct irq_revoke1_struct), 
					   (UCHAR*) request.getImpure<struct irq_revoke1_struct>(5)))
	{
		struct irq_revoke1_struct* data = request.getImpure<struct irq_revoke1_struct>(5);
		
		if (!data->irq_revoke1_end_of_stream)
		{
			++count;
		}
	}

	if (count)
		return;

	request.reset(tdbb, irq_revoke2, IRQ_REQUESTS);

	// User lost privilege.  Take it away from anybody he/she gave it to.
	// Convert GPRE FOR to EXE_start/EXE_send/EXE_receive pattern
	EXE_start(tdbb, request, transaction);
	
	// Send relation, privilege and grantor parameters
	impure = request.getImpure<impure_value>(0);
	impure->vlu_desc.dsc_address = (UCHAR*) relation.schema.c_str();
	impure->vlu_desc.dsc_length = relation.schema.length();
	impure->vlu_desc.dsc_dtype = dtype_text;
	
	impure = request.getImpure<impure_value>(1);
	impure->vlu_desc.dsc_address = (UCHAR*) relation.object.c_str();
	impure->vlu_desc.dsc_length = relation.object.length();
	impure->vlu_desc.dsc_dtype = dtype_text;

	impure = request.getImpure<impure_value>(2);
	impure->vlu_desc.dsc_address = (UCHAR*) privilege.c_str();
	impure->vlu_desc.dsc_length = privilege.length();
	impure->vlu_desc.dsc_dtype = dtype_text;

	impure = request.getImpure<impure_value>(3);
	impure->vlu_desc.dsc_address = (UCHAR*) revokee.object.c_str();
	impure->vlu_desc.dsc_length = revokee.object.length();
	impure->vlu_desc.dsc_dtype = dtype_text;

	EXE_send(tdbb, request, 0, 4 * sizeof(impure_value), (UCHAR*) request.getImpure<impure_value>(0));

	// Process revoked privileges
	while (EXE_receive(tdbb, request, 1, sizeof(struct irq_revoke2_struct), 
					   (UCHAR*) request.getImpure<struct irq_revoke2_struct>(4)))
	{
		struct irq_revoke2_struct* data = request.getImpure<struct irq_revoke2_struct>(4);
		
		if (!data->irq_revoke2_end_of_stream)
		{
			// ERASE operation would be handled by request processing
			// This is a placeholder for the actual erase logic
		}
	}
}


jrd_fld* MET_get_field(const jrd_rel* relation, USHORT id)
{
/**************************************
 *
 *      M E T _ g e t _ f i e l d
 *
 **************************************
 *
 * Functional description
 *      Get the field block for a field if possible.  If not,
 *      return NULL;
 *
 **************************************/
	vec<jrd_fld*>* vector;

	if (!relation || !(vector = relation->rel_fields) || id >= vector->count())
		return NULL;

	return (*vector)[id];
}


int MET_lookup_field(thread_db* tdbb, jrd_rel* relation, const MetaName& name)
{
/**************************************
 *
 *      M E T _ l o o k u p _ f i e l d
 *
 **************************************
 *
 * Functional description
 *      Look up a field name.
 *
 *	if the field is not found return -1
 *
 *****************************************/
	SET_TDBB(tdbb);
	Attachment* attachment = tdbb->getAttachment();

	// Start by checking field names that we already know
	vec<jrd_fld*>* vector = relation->rel_fields;

	if (vector)
	{
		int id = 0;
		vec<jrd_fld*>::iterator fieldIter = vector->begin();

		for (const vec<jrd_fld*>::const_iterator end = vector->end();  fieldIter < end;
			++fieldIter, ++id)
		{
			if (*fieldIter)
			{
				jrd_fld* field = *fieldIter;
				if (field->fld_name == name)
				{
					return id;
				}
			}
		}
	}

	// Not found.  Next, try system relations directly

	int id = -1;

	if (relation->rel_flags & REL_deleted)
		return id;

	AutoCacheRequest request(tdbb, irq_l_field, IRQ_REQUESTS);

	// Convert GPRE FOR to EXE_start/EXE_send/EXE_receive pattern
	EXE_start(tdbb, request, attachment->getSysTransaction());
	
	// Send schema, relation name, and field name parameters
	impure_value* impure = request.getImpure<impure_value>(0);
	impure->vlu_desc.dsc_address = (UCHAR*) relation->rel_name.schema.c_str();
	impure->vlu_desc.dsc_length = relation->rel_name.schema.length();
	impure->vlu_desc.dsc_dtype = dtype_text;
	
	impure = request.getImpure<impure_value>(1);
	impure->vlu_desc.dsc_address = (UCHAR*) relation->rel_name.object.c_str();
	impure->vlu_desc.dsc_length = relation->rel_name.object.length();
	impure->vlu_desc.dsc_dtype = dtype_text;

	impure = request.getImpure<impure_value>(2);
	impure->vlu_desc.dsc_address = (UCHAR*) name.c_str();
	impure->vlu_desc.dsc_length = name.length();
	impure->vlu_desc.dsc_dtype = dtype_text;

	EXE_send(tdbb, request, 0, 3 * sizeof(impure_value), (UCHAR*) request.getImpure<impure_value>(0));

	// Receive field data
	while (EXE_receive(tdbb, request, 1, sizeof(struct irq_l_field_struct), 
					   (UCHAR*) request.getImpure<struct irq_l_field_struct>(3)))
	{
		struct irq_l_field_struct* data = request.getImpure<struct irq_l_field_struct>(3);
		
		if (!data->irq_l_field_end_of_stream)
		{
			id = data->irq_l_field_id;
		}
	}

	return id;
}


MetaName MET_get_relation_field(thread_db* tdbb, MemoryPool& csbPool, const QualifiedName& relationName,
	const MetaName& fieldName, dsc* desc, FieldInfo* fieldInfo)
{
/**************************************
 *
 *	M E T _ g e t _ r e l a t i o n _ f i e l d
 *
 **************************************
 *
 * Functional description
 *  Get relation field descriptor and informations.
 *  Returns field source name.
 *
 **************************************/
	SET_TDBB(tdbb);
	Attachment* attachment = tdbb->getAttachment();
	bool found = false;
	MetaName sourceName;

	AutoCacheRequest handle(tdbb, irq_l_relfield, IRQ_REQUESTS);

	// Convert GPRE FOR to EXE_start/EXE_send/EXE_receive pattern
	EXE_start(tdbb, handle, attachment->getSysTransaction());
	
	// Send relation and field name parameters
	impure_value* impure = handle.getImpure<impure_value>(0);
	impure->vlu_desc.dsc_address = (UCHAR*) relationName.schema.c_str();
	impure->vlu_desc.dsc_length = relationName.schema.length();
	impure->vlu_desc.dsc_dtype = dtype_text;
	
	impure = handle.getImpure<impure_value>(1);
	impure->vlu_desc.dsc_address = (UCHAR*) relationName.object.c_str();
	impure->vlu_desc.dsc_length = relationName.object.length();
	impure->vlu_desc.dsc_dtype = dtype_text;

	impure = handle.getImpure<impure_value>(2);
	impure->vlu_desc.dsc_address = (UCHAR*) fieldName.c_str();
	impure->vlu_desc.dsc_length = fieldName.length();
	impure->vlu_desc.dsc_dtype = dtype_text;

	EXE_send(tdbb, handle, 0, 3 * sizeof(impure_value), (UCHAR*) handle.getImpure<impure_value>(0));

	// Receive field data
	while (EXE_receive(tdbb, handle, 1, sizeof(struct irq_l_relfield_struct), 
					   (UCHAR*) handle.getImpure<struct irq_l_relfield_struct>(3)))
	{
		struct irq_l_relfield_struct* data = handle.getImpure<struct irq_l_relfield_struct>(3);
		
		if (!data->irq_l_relfield_end_of_stream)
		{
			if (DSC_make_descriptor(desc,
									data->irq_l_relfield_type,
									data->irq_l_relfield_scale,
									data->irq_l_relfield_length,
									data->irq_l_relfield_sub_type,
									data->irq_l_relfield_charset_id,
									(data->irq_l_relfield_collation_id_null ? data->irq_l_relfield_fld_collation_id : data->irq_l_relfield_collation_id)))
			{
				found = true;
				sourceName = data->irq_l_relfield_source;

				if (fieldInfo)
				{
					fieldInfo->nullable = data->irq_l_relfield_null_flag_null ?
						(data->irq_l_relfield_fld_null_flag_null || data->irq_l_relfield_fld_null_flag == 0) : data->irq_l_relfield_null_flag == 0;

					Jrd::ContextPoolHolder context(tdbb, &csbPool);
					bid* defaultId = NULL;

					if (!data->irq_l_relfield_default_value_null)
						defaultId = &data->irq_l_relfield_default_value;
					else if (!data->irq_l_relfield_fld_default_value_null)
						defaultId = &data->irq_l_relfield_fld_default_value;

					if (defaultId)
						fieldInfo->defaultValue = parse_field_default_blr(tdbb, relationName.schema, defaultId);
					else
						fieldInfo->defaultValue = NULL;

					if (data->irq_l_relfield_validation_blr_null)
						fieldInfo->validationExpr = NULL;
					else
					{
						fieldInfo->validationExpr = parse_field_validation_blr(tdbb,
							&data->irq_l_relfield_validation_blr, QualifiedName(data->irq_l_relfield_source, data->irq_l_relfield_source_schema_name));
					}
				}
			}
		}
	}

	if (!found)
	{
		ERR_post(Arg::Gds(isc_dyn_column_does_not_exist) <<
			fieldName.toQuotedString() <<
			relationName.toQuotedString());
	}

	return sourceName;
}


// Helper functions
static ULONG get_rel_flags_from_FLAGS(USHORT flags)
{
/**************************************
 *
 *      g e t _ r e l _ f l a g s _ f r o m _ F L A G S
 *
 **************************************
 *
 * Functional description
 *      Get rel_flags from RDB$FLAGS
 *
 **************************************/
	ULONG ret = 0;

	if (flags & REL_sql) {
		ret |= REL_sql_relation;
	}

	return ret;
}


static ValueExprNode* parse_field_default_blr(thread_db* tdbb, const MetaName& schema, bid* blob_id)
{
	SET_TDBB(tdbb);
	Attachment* attachment = tdbb->getAttachment();

	MemoryPool& pool = *tdbb->getDefaultPool();
	AutoPtr<CompilerScratch> auto_csb(FB_NEW_POOL(pool) CompilerScratch(pool));
	CompilerScratch* csb = auto_csb;

	blb* blob = blb::open(tdbb, attachment->getSysTransaction(), blob_id);
	ULONG length = blob->blb_length + 10;
	HalfStaticArray<UCHAR, 512> temp;

	length = blob->BLB_get_data(tdbb, temp.getBuffer(length), length);

	DmlNode* const node = PAR_blr(tdbb, &schema, NULL, temp.begin(), length, NULL, &csb, NULL, false, 0);
	return static_cast<ValueExprNode*>(node);
}


ULONG MET_get_rel_flags_from_TYPE(USHORT type)
{
/**************************************
 *
 *      M E T _g e t _ r e l _ f l a g s _ f r o m _ T Y P E
 *
 **************************************
 *
 * Functional description
 *      Get rel_flags from RDB$RELATION_TYPE
 *
 **************************************/
	ULONG ret = 0;

	switch (type)
	{
		case rel_persistent:
			break;
		case rel_external:
			break;
		case rel_view:
			ret |= REL_jrd_view;
			break;
		case rel_virtual:
			ret |= REL_virtual;
			break;
		case rel_global_temp_preserve:
			ret |= REL_temp_conn;
			break;
		case rel_global_temp_delete:
			ret |= REL_temp_tran;
			break;
		default:
			fb_assert(false);
	}

	return ret;
}


// Parses validation BLR for a field.
static BoolExprNode* parse_field_validation_blr(thread_db* tdbb, bid* blob_id, const QualifiedName& name)
{
	SET_TDBB(tdbb);
	Attachment* attachment = tdbb->getAttachment();

	MemoryPool& pool = *tdbb->getDefaultPool();
	AutoPtr<CompilerScratch> auto_csb(FB_NEW_POOL(pool) CompilerScratch(pool));
	CompilerScratch* csb = auto_csb;

	csb->csb_domain_validation = name;

	blb* blob = blb::open(tdbb, attachment->getSysTransaction(), blob_id);
	ULONG length = blob->blb_length + 10;
	HalfStaticArray<UCHAR, 512> temp;

	length = blob->BLB_get_data(tdbb, temp.getBuffer(length), length);

	return PAR_validation_blr(tdbb, &name.schema, NULL, temp.begin(), length, NULL, &csb, 0);
}


static void lookup_view_contexts(thread_db* tdbb, jrd_rel* view)
{
/**************************************
 *
 *      l o o k u p _ v i e w _ c o n t e x t s
 *
 **************************************
 *
 * Functional description
 *      Lookup view contexts and store in a sorted
 *      array on the relation block.
 *
 **************************************/
	SET_TDBB(tdbb);
	Attachment* attachment = tdbb->getAttachment();
	Database* dbb = tdbb->getDatabase();
	AutoCacheRequest request(tdbb, irq_view_context, IRQ_REQUESTS);

	// Convert GPRE FOR to EXE_start/EXE_send/EXE_receive pattern
	EXE_start(tdbb, request, attachment->getSysTransaction());
	
	// Send view name parameters
	impure_value* impure = request.getImpure<impure_value>(0);
	impure->vlu_desc.dsc_address = (UCHAR*) view->rel_name.schema.c_str();
	impure->vlu_desc.dsc_length = view->rel_name.schema.length();
	impure->vlu_desc.dsc_dtype = dtype_text;
	
	impure = request.getImpure<impure_value>(1);
	impure->vlu_desc.dsc_address = (UCHAR*) view->rel_name.object.c_str();
	impure->vlu_desc.dsc_length = view->rel_name.object.length();
	impure->vlu_desc.dsc_dtype = dtype_text;

	EXE_send(tdbb, request, 0, 2 * sizeof(impure_value), (UCHAR*) request.getImpure<impure_value>(0));

	// Receive view context data
	while (EXE_receive(tdbb, request, 1, sizeof(struct irq_view_context_struct), 
					   (UCHAR*) request.getImpure<struct irq_view_context_struct>(2)))
	{
		struct irq_view_context_struct* V = request.getImpure<struct irq_view_context_struct>(2);
		
		if (!V->irq_view_context_end_of_stream)
		{
			// trim trailing spaces
			fb_utils::exact_name_limit(V->irq_view_context_context_name, sizeof(V->irq_view_context_context_name));

			ViewContext* view_context = FB_NEW_POOL(*view->rel_pool)
				ViewContext(*view->rel_pool,
					V->irq_view_context_context_name,
					QualifiedName(V->irq_view_context_relation_name, V->irq_view_context_relation_schema_name),
					V->irq_view_context_view_context,
					(V->irq_view_context_context_type_null ? VCT_TABLE : ViewContextType(V->irq_view_context_context_type)));

			view->rel_view_contexts.add(view_context);
		}
	}
}


static void scan_partners(thread_db* tdbb, jrd_rel* relation)
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

		// Convert GPRE FOR to EXE_start/EXE_send/EXE_receive pattern for foreign keys
		EXE_start(tdbb, request, attachment->getSysTransaction());
		
		// Send relation name parameters
		impure_value* impure = request.getImpure<impure_value>(0);
		impure->vlu_desc.dsc_address = (UCHAR*) relation->rel_name.schema.c_str();
		impure->vlu_desc.dsc_length = relation->rel_name.schema.length();
		impure->vlu_desc.dsc_dtype = dtype_text;
		
		impure = request.getImpure<impure_value>(1);
		impure->vlu_desc.dsc_address = (UCHAR*) relation->rel_name.object.c_str();
		impure->vlu_desc.dsc_length = relation->rel_name.object.length();
		impure->vlu_desc.dsc_dtype = dtype_text;

		EXE_send(tdbb, request, 0, 2 * sizeof(impure_value), (UCHAR*) request.getImpure<impure_value>(0));

		// Receive foreign key data
		while (EXE_receive(tdbb, request, 1, sizeof(struct irq_foreign1_struct), 
						   (UCHAR*) request.getImpure<struct irq_foreign1_struct>(2)))
		{
			struct irq_foreign1_struct* data = request.getImpure<struct irq_foreign1_struct>(2);
			
			if (!data->irq_foreign1_end_of_stream)
			{
				const QualifiedName partnerRelationName(data->irq_foreign1_partner_rel_name, data->irq_foreign1_partner_schema_name);

				//// ASF: Hack fix for CORE-4304, until nasty interactions between dfw and met are not resolved.
				const jrd_rel* partner_relation = relation->rel_name == partnerRelationName ?
					relation : MET_lookup_relation(tdbb, partnerRelationName);

				if (partner_relation && !data->irq_foreign1_idx_inactive && !data->irq_foreign1_ind_inactive)
				{
					// This seems a good candidate for vcl.
					references->frgn_reference_ids =
						vec<int>::newVector(*relation->rel_pool, references->frgn_reference_ids,
						index_number + 1);

					(*references->frgn_reference_ids)[index_number] = data->irq_foreign1_idx_id - 1;

					references->frgn_relations =
						vec<int>::newVector(*relation->rel_pool, references->frgn_relations,
						index_number + 1);

					(*references->frgn_relations)[index_number] = partner_relation->rel_id;

					references->frgn_indexes =
						vec<int>::newVector(*relation->rel_pool, references->frgn_indexes,
						index_number + 1);

					(*references->frgn_indexes)[index_number] = data->irq_foreign1_ind_id - 1;

					index_number++;
				}
			}
		}

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

		// Convert GPRE FOR to EXE_start/EXE_send/EXE_receive pattern for primary dependencies
		EXE_start(tdbb, request, attachment->getSysTransaction());
		
		// Send relation name parameters
		impure = request.getImpure<impure_value>(0);
		impure->vlu_desc.dsc_address = (UCHAR*) relation->rel_name.schema.c_str();
		impure->vlu_desc.dsc_length = relation->rel_name.schema.length();
		impure->vlu_desc.dsc_dtype = dtype_text;
		
		impure = request.getImpure<impure_value>(1);
		impure->vlu_desc.dsc_address = (UCHAR*) relation->rel_name.object.c_str();
		impure->vlu_desc.dsc_length = relation->rel_name.object.length();
		impure->vlu_desc.dsc_dtype = dtype_text;

		EXE_send(tdbb, request, 0, 2 * sizeof(impure_value), (UCHAR*) request.getImpure<impure_value>(0));

		// Receive primary dependency data
		while (EXE_receive(tdbb, request, 1, sizeof(struct irq_foreign2_struct), 
						   (UCHAR*) request.getImpure<struct irq_foreign2_struct>(2)))
		{
			struct irq_foreign2_struct* data = request.getImpure<struct irq_foreign2_struct>(2);
			
			if (!data->irq_foreign2_end_of_stream)
			{
				const QualifiedName partnerRelationName(data->irq_foreign2_partner_rel_name, data->irq_foreign2_partner_schema_name);

				//// ASF: Hack fix for CORE-4304, until nasty interactions between dfw and met are not resolved.
				const jrd_rel* partner_relation = relation->rel_name == partnerRelationName ?
					relation : MET_lookup_relation(tdbb, partnerRelationName);

				if (partner_relation && !data->irq_foreign2_idx_inactive && !data->irq_foreign2_ind_inactive)
				{
					dependencies->prim_reference_ids =
						vec<int>::newVector(*relation->rel_pool, dependencies->prim_reference_ids,
						index_number + 1);

					(*dependencies->prim_reference_ids)[index_number] = data->irq_foreign2_idx_id - 1;

					dependencies->prim_relations =
						vec<int>::newVector(*relation->rel_pool, dependencies->prim_relations,
						index_number + 1);

					(*dependencies->prim_relations)[index_number] = partner_relation->rel_id;

					dependencies->prim_indexes =
						vec<int>::newVector(*relation->rel_pool, dependencies->prim_indexes,
						index_number + 1);

					(*dependencies->prim_indexes)[index_number] = data->irq_foreign2_ind_id - 1;

					index_number++;
				}
			}
		}
	}
}

} // namespace Jrd