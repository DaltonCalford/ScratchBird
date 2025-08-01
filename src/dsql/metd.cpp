/*
 *  PROGRAM:    Dynamic SQL runtime support
 *  MODULE:     metd.cpp
 *  DESCRIPTION:    Meta-data interface
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
 * 2001.11.28 Claudio Valderrama: load not only udfs but udf arguments;
 *   handle possible collisions with udf redefinitions (drop->declare).
 *   This closes SF Bug# 409769.
 * 2001.12.06 Claudio Valderrama: METD_get_charset_bpc() was added to
 *    get only the bytes per char of a field, given its charset id.
 *   This request is not cached.
 * 2001.02.23 Claudio Valderrama: Fix SF Bug #228135 with views spoiling
 *    NULLs in outer joins.
 * 2004.01.16 Vlad Horsun: make METD_get_col_default and
 *   METD_get_domain_default return actual length of default BLR
 * 2004.01.16 Vlad Horsun: added support for default parameters
 */

#include "scratchbird.h"
#include <string.h>
#include "../dsql/dsql.h"
#include "ibase.h"
#include "../jrd/align.h"
#include "../jrd/intl.h"
#include "../jrd/irq.h"
#include "../jrd/tra.h"
#include "../dsql/ExprNodes.h"
#include "../dsql/ddl_proto.h"
#include "../dsql/metd_proto.h"
#include "../dsql/make_proto.h"
#include "../dsql/errd_proto.h"
#include "../jrd/blb_proto.h"
#include "../jrd/cmp_proto.h"
#include "../jrd/exe_proto.h"
#include "../yvalve/gds_proto.h"
#include "../jrd/met_proto.h"
#include "../yvalve/why_proto.h"
#include "../common/utils_proto.h"
#include "../common/classes/init.h"

using namespace Jrd;
using namespace ScratchBird;

static void convert_dtype(TypeClause*, SSHORT);
static void free_relation(dsql_rel*);

namespace
{
	inline void validateTransaction(const jrd_tra* transaction)
	{
		if (!transaction || !transaction->checkHandle())
		{
			ERR_post(Arg::Gds(isc_bad_trans_handle));
		}
	}

	bool isSystemRelation(thread_db* tdbb, jrd_tra* transaction, const QualifiedName& relName)
	{
		bool rc = false;

		AutoCacheRequest handle(tdbb, irq_system_relation, IRQ_REQUESTS);
		
		// Converted FOR loop #1: Check for system relation
		EXE_start(tdbb, handle, transaction);
		
		const jrd_req* const request = handle;
		bool found = false;
		
		USHORT isc_req_11_3[56] = {
			blr_version5,
			blr_begin,
			blr_message, 0, 3, 0,
				blr_varying2, 3, 0, 0, 0,
				blr_varying2, 3, 0, 0, 0,
				blr_short, 0,
			blr_receive, 0,
			blr_begin,
				blr_for,
					blr_rse, 1,
						blr_relation, 13, 'R','D','B','$','R','E','L','A','T','I','O','N','S', 0,
						blr_and,
							blr_and,
								blr_eql,
									blr_field, 0, 16, 'R','D','B','$','S','C','H','E','M','A','_','N','A','M','E',
									blr_parameter, 0, 0, 0,
								blr_eql,
									blr_field, 0, 18, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
									blr_parameter, 0, 1, 0,
							blr_eql,
								blr_field, 0, 16, 'R','D','B','$','S','Y','S','T','E','M','_','F','L','A','G',
								blr_parameter, 0, 2, 0,
					blr_send, 1,
						blr_begin,
							blr_assignment,
								blr_literal, blr_short, 0, 1, 0,
								blr_parameter, 1, 0, 0,
						blr_end,
				blr_send, 1,
					blr_assignment,
						blr_literal, blr_short, 0, 0, 0,
						blr_parameter, 1, 0, 0,
			blr_end,
			blr_eoc
		};
		
		struct {
			char	isc_req_11_schema[256];
			char	isc_req_11_relation[256];
			SSHORT	isc_req_11_system_flag;
		} isc_req_11_in;
		
		struct {
			SSHORT	isc_req_11_found;
		} isc_req_11_out;
		
		strcpy(isc_req_11_in.isc_req_11_schema, relName.schema.c_str());
		strcpy(isc_req_11_in.isc_req_11_relation, relName.object.c_str());
		isc_req_11_in.isc_req_11_system_flag = 1;
		
		EXE_send(tdbb, request, 0, sizeof(isc_req_11_in), (UCHAR*)&isc_req_11_in);
		
		while (true)
		{
			EXE_receive(tdbb, request, 1, sizeof(isc_req_11_out), (UCHAR*)&isc_req_11_out);
			if (!isc_req_11_out.isc_req_11_found)
				break;
			rc = true;
		}

		return rc;
	}

	bool isSystemDomain(thread_db* tdbb, jrd_tra* transaction, const QualifiedName& fldName)
	{
		bool rc = false;

		AutoCacheRequest handle(tdbb, irq_system_domain, IRQ_REQUESTS);
		
		// Converted FOR loop #2: Check for system domain
		EXE_start(tdbb, handle, transaction);
		
		const jrd_req* const request = handle;
		bool found = false;
		
		USHORT isc_req_12_3[56] = {
			blr_version5,
			blr_begin,
			blr_message, 0, 3, 0,
				blr_varying2, 3, 0, 0, 0,
				blr_varying2, 3, 0, 0, 0,
				blr_short, 0,
			blr_receive, 0,
			blr_begin,
				blr_for,
					blr_rse, 1,
						blr_relation, 10, 'R','D','B','$','F','I','E','L','D','S', 0,
						blr_and,
							blr_and,
								blr_eql,
									blr_field, 0, 16, 'R','D','B','$','S','C','H','E','M','A','_','N','A','M','E',
									blr_parameter, 0, 0, 0,
								blr_eql,
									blr_field, 0, 15, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E',
									blr_parameter, 0, 1, 0,
							blr_eql,
								blr_field, 0, 16, 'R','D','B','$','S','Y','S','T','E','M','_','F','L','A','G',
								blr_parameter, 0, 2, 0,
					blr_send, 1,
						blr_begin,
							blr_assignment,
								blr_literal, blr_short, 0, 1, 0,
								blr_parameter, 1, 0, 0,
						blr_end,
				blr_send, 1,
					blr_assignment,
						blr_literal, blr_short, 0, 0, 0,
						blr_parameter, 1, 0, 0,
			blr_end,
			blr_eoc
		};
		
		struct {
			char	isc_req_12_schema[256];
			char	isc_req_12_field[256];
			SSHORT	isc_req_12_system_flag;
		} isc_req_12_in;
		
		struct {
			SSHORT	isc_req_12_found;
		} isc_req_12_out;
		
		strcpy(isc_req_12_in.isc_req_12_schema, fldName.schema.c_str());
		strcpy(isc_req_12_in.isc_req_12_field, fldName.object.c_str());
		isc_req_12_in.isc_req_12_system_flag = 1;
		
		EXE_send(tdbb, request, 0, sizeof(isc_req_12_in), (UCHAR*)&isc_req_12_in);
		
		while (true)
		{
			EXE_receive(tdbb, request, 1, sizeof(isc_req_12_out), (UCHAR*)&isc_req_12_out);
			if (!isc_req_12_out.isc_req_12_found)
				break;
			rc = true;
		}

		return rc;
	}
}


void METD_drop_charset(jrd_tra* transaction, const QualifiedName& metaName)
{
/**************************************
 *
 *  M E T D _ d r o p _ c h a r s e t
 *
 **************************************
 *
 * Functional description
 *  Drop a character set from our metadata, and the next caller who wants it will
 *  look up the new version.
 *  Dropping will be achieved by marking the character set
 *  as dropped.  Anyone with current access can continue
 *  accessing it.
 *
 **************************************/
	thread_db* tdbb = JRD_get_thread_data();
	dsql_dbb* dbb = transaction->getDsqlAttachment();
	dsql_intlsym* charSet;

	if (dbb->dbb_charsets.get(metaName, charSet))
	{
		MET_dsql_cache_use(tdbb, SYM_intlsym_charset, metaName);
		charSet->intlsym_flags |= INTLSYM_dropped;
		dbb->dbb_charsets.remove(metaName);
		dbb->dbb_charsets_by_id.remove(charSet->intlsym_charset_id);
	}
}


void METD_drop_collation(jrd_tra* transaction, const QualifiedName& name)
{
/**************************************
 *
 *  M E T D _ d r o p _ c o l l a t i o n
 *
 **************************************
 *
 * Functional description
 *  Drop a collation from our metadata, and
 *  the next caller who wants it will
 *  look up the new version.
 *
 *  Dropping will be achieved by marking the collation
 *  as dropped.  Anyone with current access can continue
 *  accessing it.
 *
 **************************************/
	thread_db* tdbb = JRD_get_thread_data();
	dsql_dbb* dbb = transaction->getDsqlAttachment();

	dsql_intlsym* collation;

	if (dbb->dbb_collations.get(name, collation))
	{
		MET_dsql_cache_use(tdbb, SYM_intlsym_collation, name);
		collation->intlsym_flags |= INTLSYM_dropped;
		dbb->dbb_collations.remove(name);
	}
}


void METD_drop_function(jrd_tra* transaction, const QualifiedName& name)
{
/**************************************
 *
 *  M E T D _ d r o p _ f u n c t i o n
 *
 **************************************
 *
 * Functional description
 *  Drop a user defined function from our metadata, and
 *  the next caller who wants it will
 *  look up the new version.
 *
 *  Dropping will be achieved by marking the function
 *  as dropped.  Anyone with current access can continue
 *  accessing it.
 *
 **************************************/
	thread_db* tdbb = JRD_get_thread_data();
	dsql_dbb* dbb = transaction->getDsqlAttachment();

	dsql_udf* function;

	if (dbb->dbb_functions.get(name, function))
	{
		MET_dsql_cache_use(tdbb, SYM_udf, name);
		function->udf_flags |= UDF_dropped;
		dbb->dbb_functions.remove(name);
	}

}


void METD_drop_procedure(jrd_tra* transaction, const QualifiedName& name)
{
/**************************************
 *
 *  M E T D _ d r o p _ p r o c e d u r e
 *
 **************************************
 *
 * Functional description
 *  Drop a procedure from our metadata, and
 *  the next caller who wants it will
 *  look up the new version.
 *
 *  Dropping will be achieved by marking the procedure
 *  as dropped.  Anyone with current access can continue
 *  accessing it.
 *
 **************************************/
	thread_db* tdbb = JRD_get_thread_data();
	dsql_dbb* dbb = transaction->getDsqlAttachment();

	dsql_prc* procedure;

	if (dbb->dbb_procedures.get(name, procedure))
	{
		MET_dsql_cache_use(tdbb, SYM_procedure, name);
		procedure->prc_flags |= PRC_dropped;
		dbb->dbb_procedures.remove(name);
	}
}


void METD_drop_relation(jrd_tra* transaction, const QualifiedName& name)
{
/**************************************
 *
 *  M E T D _ d r o p _ r e l a t i o n
 *
 **************************************
 *
 * Functional description
 *  Drop a relation from our metadata, and
 *  rely on the next guy who wants it to
 *  look up the new version.
 *
 *      Dropping will be achieved by marking the relation
 *      as dropped.  Anyone with current access can continue
 *      accessing it.
 *
 **************************************/
	thread_db* tdbb = JRD_get_thread_data();
	dsql_dbb* dbb = transaction->getDsqlAttachment();

	dsql_rel* relation;

	if (dbb->dbb_relations.get(name, relation))
	{
		MET_dsql_cache_use(tdbb, SYM_relation, name);
		relation->rel_flags |= REL_dropped;
		dbb->dbb_relations.remove(name);
	}
}


dsql_intlsym* METD_get_collation(jrd_tra* transaction, const QualifiedName& name, USHORT charset_id)
{
/**************************************
 *
 *  M E T D _ g e t _ c o l l a t i o n
 *
 **************************************
 *
 * Functional description
 *  Look up an international text type object.
 *  If it doesn't exist, return NULL.
 *
 **************************************/
	thread_db* tdbb = JRD_get_thread_data();

	validateTransaction(transaction);

	dsql_dbb* dbb = transaction->getDsqlAttachment();

	// Start by seeing if symbol is already defined

	dsql_intlsym* symbol;
	if (dbb->dbb_collations.get(name, symbol) && !(symbol->intlsym_flags & INTLSYM_dropped) &&
		symbol->intlsym_charset_id == charset_id)
	{
		if (MET_dsql_cache_use(tdbb, SYM_intlsym_collation, name))
			symbol->intlsym_flags |= INTLSYM_dropped;
		else
			return symbol;
	}

	// Now see if it is in the database

	symbol = NULL;

	AutoCacheRequest handle(tdbb, irq_collation, IRQ_REQUESTS);

	// Converted FOR loop #3: Get collation from database
	EXE_start(tdbb, handle, transaction);
	
	const jrd_req* const request = handle;
	bool found = false;
	
	USHORT isc_req_13_4[108] = {
		blr_version5,
		blr_begin,
		blr_message, 0, 4, 0,
			blr_varying2, 3, 0, 0, 0,
			blr_varying2, 3, 0, 0, 0,
			blr_short, 0,
			blr_short, 0,
		blr_receive, 0,
		blr_begin,
			blr_for,
				blr_rse, 1,
					blr_cross,
						blr_relation, 14, 'R','D','B','$','C','O','L','L','A','T','I','O','N','S', 0,
						blr_relation, 19, 'R','D','B','$','C','H','A','R','A','C','T','E','R','_','S','E','T','S', 1,
					blr_boolean,
						blr_and,
							blr_and,
								blr_and,
									blr_eql,
										blr_field, 0, 16, 'R','D','B','$','S','C','H','E','M','A','_','N','A','M','E',
										blr_parameter, 0, 0, 0,
									blr_eql,
										blr_field, 0, 18, 'R','D','B','$','C','O','L','L','A','T','I','O','N','_','N','A','M','E',
										blr_parameter, 0, 1, 0,
								blr_eql,
									blr_field, 0, 20, 'R','D','B','$','C','H','A','R','A','C','T','E','R','_','S','E','T','_','I','D',
									blr_parameter, 0, 2, 0,
							blr_eql,
								blr_field, 0, 20, 'R','D','B','$','C','H','A','R','A','C','T','E','R','_','S','E','T','_','I','D',
								blr_field, 1, 20, 'R','D','B','$','C','H','A','R','A','C','T','E','R','_','S','E','T','_','I','D',
				blr_send, 1,
					blr_begin,
						blr_assignment,
							blr_field, 0, 20, 'R','D','B','$','C','H','A','R','A','C','T','E','R','_','S','E','T','_','I','D',
							blr_parameter, 1, 0, 0,
						blr_assignment,
							blr_field, 0, 17, 'R','D','B','$','C','O','L','L','A','T','I','O','N','_','I','D',
							blr_parameter, 1, 1, 0,
						blr_assignment,
							blr_field, 1, 24, 'R','D','B','$','B','Y','T','E','S','_','P','E','R','_','C','H','A','R','A','C','T','E','R',
							blr_parameter, 1, 2, 0,
						blr_assignment,
							blr_literal, blr_short, 0, 1, 0,
							blr_parameter, 1, 3, 0,
					blr_end,
			blr_send, 1,
				blr_assignment,
					blr_literal, blr_short, 0, 0, 0,
					blr_parameter, 1, 3, 0,
		blr_end,
		blr_eoc
	};
	
	struct {
		char	isc_req_13_schema[256];
		char	isc_req_13_collation[256];
		SSHORT	isc_req_13_charset_id;
		SSHORT	isc_req_13_match_charset;
	} isc_req_13_in;
	
	struct {
		SSHORT	isc_req_13_charset_id_out;
		SSHORT	isc_req_13_collation_id;
		SSHORT	isc_req_13_bytes_per_char;
		SSHORT	isc_req_13_found;
	} isc_req_13_out;
	
	strcpy(isc_req_13_in.isc_req_13_schema, name.schema.c_str());
	strcpy(isc_req_13_in.isc_req_13_collation, name.object.c_str());
	isc_req_13_in.isc_req_13_charset_id = charset_id;
	isc_req_13_in.isc_req_13_match_charset = charset_id;
	
	EXE_send(tdbb, request, 0, sizeof(isc_req_13_in), (UCHAR*)&isc_req_13_in);
	
	while (true)
	{
		EXE_receive(tdbb, request, 1, sizeof(isc_req_13_out), (UCHAR*)&isc_req_13_out);
		if (!isc_req_13_out.isc_req_13_found)
			break;
			
		symbol = FB_NEW_POOL(dbb->dbb_pool) dsql_intlsym(dbb->dbb_pool);
		symbol->intlsym_name = name;
		symbol->intlsym_flags = 0;
		symbol->intlsym_charset_id = isc_req_13_out.isc_req_13_charset_id_out;
		symbol->intlsym_collate_id = isc_req_13_out.isc_req_13_collation_id;
		symbol->intlsym_ttype =
			INTL_CS_COLL_TO_TTYPE(symbol->intlsym_charset_id, symbol->intlsym_collate_id);
		symbol->intlsym_bytes_per_char = isc_req_13_out.isc_req_13_bytes_per_char;
	}

	if (!symbol)
		return NULL;

	dbb->dbb_collations.put(name, symbol);
	MET_dsql_cache_use(tdbb, SYM_intlsym_collation, name);

	return symbol;
}


dsql_intlsym* METD_get_charset(jrd_tra* transaction, const QualifiedName& name)
{
/**************************************
 *
 *  M E T D _ g e t _ c h a r s e t
 *
 **************************************
 *
 * Functional description
 *  Look up an international text type object.
 *  If it doesn't exist, return NULL.
 *
 **************************************/
	thread_db* tdbb = JRD_get_thread_data();

	validateTransaction(transaction);

	dsql_dbb* dbb = transaction->getDsqlAttachment();

	// Start by seeing if symbol is already defined

	dsql_intlsym* symbol;
	if (dbb->dbb_charsets.get(name, symbol) && !(symbol->intlsym_flags & INTLSYM_dropped))
	{
		if (MET_dsql_cache_use(tdbb, SYM_intlsym_charset, name))
			symbol->intlsym_flags |= INTLSYM_dropped;
		else
			return symbol;
	}

	// Now see if it is in the database

	symbol = NULL;

	AutoCacheRequest handle(tdbb, irq_charset, IRQ_REQUESTS);

	// Converted FOR loop #4: Get charset from database
	EXE_start(tdbb, handle, transaction);
	
	const jrd_req* const request = handle;
	bool found = false;
	
	USHORT isc_req_14_4[150] = {
		blr_version5,
		blr_begin,
		blr_message, 0, 2, 0,
			blr_varying2, 3, 0, 0, 0,
			blr_varying2, 3, 0, 0, 0,
		blr_receive, 0,
		blr_begin,
			blr_for,
				blr_rse, 1,
					blr_cross,
						blr_cross,
							blr_relation, 19, 'R','D','B','$','C','H','A','R','A','C','T','E','R','_','S','E','T','S', 0,
							blr_relation, 14, 'R','D','B','$','C','O','L','L','A','T','I','O','N','S', 1,
						blr_relation, 10, 'R','D','B','$','T','Y','P','E','S', 2,
					blr_boolean,
						blr_and,
							blr_and,
								blr_and,
									blr_and,
										blr_and,
											blr_eql,
												blr_field, 0, 16, 'R','D','B','$','S','C','H','E','M','A','_','N','A','M','E',
												blr_parameter, 0, 0, 0,
											blr_eql,
												blr_field, 2, 14, 'R','D','B','$','T','Y','P','E','_','N','A','M','E',
												blr_parameter, 0, 1, 0,
										blr_eql,
											blr_field, 1, 16, 'R','D','B','$','S','C','H','E','M','A','_','N','A','M','E',
											blr_field, 0, 30, 'R','D','B','$','D','E','F','A','U','L','T','_','C','O','L','L','A','T','E','_','S','C','H','E','M','A','_','N','A','M','E',
									blr_eql,
										blr_field, 1, 18, 'R','D','B','$','C','O','L','L','A','T','I','O','N','_','N','A','M','E',
										blr_field, 0, 26, 'R','D','B','$','D','E','F','A','U','L','T','_','C','O','L','L','A','T','E','_','N','A','M','E',
								blr_eql,
									blr_field, 2, 15, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E',
									blr_literal, blr_text, 24, 0, 'R','D','B','$','C','H','A','R','A','C','T','E','R','_','S','E','T','_','N','A','M','E',
							blr_eql,
								blr_field, 2, 9, 'R','D','B','$','T','Y','P','E',
								blr_field, 0, 20, 'R','D','B','$','C','H','A','R','A','C','T','E','R','_','S','E','T','_','I','D',
						blr_boolean,
							blr_and,
								blr_eql,
									blr_field, 1, 20, 'R','D','B','$','C','H','A','R','A','C','T','E','R','_','S','E','T','_','I','D',
									blr_field, 0, 20, 'R','D','B','$','C','H','A','R','A','C','T','E','R','_','S','E','T','_','I','D',
				blr_send, 1,
					blr_begin,
						blr_assignment,
							blr_field, 1, 20, 'R','D','B','$','C','H','A','R','A','C','T','E','R','_','S','E','T','_','I','D',
							blr_parameter, 1, 0, 0,
						blr_assignment,
							blr_field, 1, 17, 'R','D','B','$','C','O','L','L','A','T','I','O','N','_','I','D',
							blr_parameter, 1, 1, 0,
						blr_assignment,
							blr_field, 0, 24, 'R','D','B','$','B','Y','T','E','S','_','P','E','R','_','C','H','A','R','A','C','T','E','R',
							blr_parameter, 1, 2, 0,
						blr_assignment,
							blr_literal, blr_short, 0, 1, 0,
							blr_parameter, 1, 3, 0,
					blr_end,
			blr_send, 1,
				blr_assignment,
					blr_literal, blr_short, 0, 0, 0,
					blr_parameter, 1, 3, 0,
		blr_end,
		blr_eoc
	};
	
	struct {
		char	isc_req_14_schema[256];
		char	isc_req_14_type_name[256];
	} isc_req_14_in;
	
	struct {
		SSHORT	isc_req_14_charset_id;
		SSHORT	isc_req_14_collation_id;
		SSHORT	isc_req_14_bytes_per_char;
		SSHORT	isc_req_14_found;
	} isc_req_14_out;
	
	strcpy(isc_req_14_in.isc_req_14_schema, name.schema.c_str());
	strcpy(isc_req_14_in.isc_req_14_type_name, name.object.c_str());
	
	EXE_send(tdbb, request, 0, sizeof(isc_req_14_in), (UCHAR*)&isc_req_14_in);
	
	while (true)
	{
		EXE_receive(tdbb, request, 1, sizeof(isc_req_14_out), (UCHAR*)&isc_req_14_out);
		if (!isc_req_14_out.isc_req_14_found)
			break;
			
		symbol = FB_NEW_POOL(dbb->dbb_pool) dsql_intlsym(dbb->dbb_pool);
		symbol->intlsym_name = name;
		symbol->intlsym_flags = 0;
		symbol->intlsym_charset_id = isc_req_14_out.isc_req_14_charset_id;
		symbol->intlsym_collate_id = isc_req_14_out.isc_req_14_collation_id;
		symbol->intlsym_ttype =
			INTL_CS_COLL_TO_TTYPE(symbol->intlsym_charset_id, symbol->intlsym_collate_id);
		symbol->intlsym_bytes_per_char = isc_req_14_out.isc_req_14_bytes_per_char;
	}

	if (!symbol)
		return NULL;

	dbb->dbb_charsets.put(name, symbol);
	dbb->dbb_charsets_by_id.put(symbol->intlsym_charset_id, symbol);
	MET_dsql_cache_use(tdbb, SYM_intlsym_charset, name);

	return symbol;
}


USHORT METD_get_charset_bpc(jrd_tra* transaction, SSHORT charset_id)
{
/**************************************
 *
 *  M E T D _ g e t _ c h a r s e t _ b p c
 *
 **************************************
 *
 * Functional description
 *  Look up an international text type object.
 *  If it doesn't exist, return NULL.
 *  Go directly to system tables & return only the
 *  number of bytes per character. Lookup by
 *  charset' id, not by name.
 *
 **************************************/
	thread_db* tdbb = JRD_get_thread_data();

	dsql_dbb* dbb = transaction->getDsqlAttachment();

	if (charset_id == CS_dynamic)
		charset_id = tdbb->getCharSet();

	dsql_intlsym* symbol = NULL;
	if (!dbb->dbb_charsets_by_id.get(charset_id, symbol))
	{
		const auto cs_name = METD_get_charset_name(transaction, charset_id);
		symbol = METD_get_charset(transaction, cs_name);
	}

	fb_assert(symbol);

	return symbol ? symbol->intlsym_bytes_per_char : 0;
}


QualifiedName METD_get_charset_name(jrd_tra* transaction, SSHORT charset_id)
{
/**************************************
 *
 *  M E T D _ g e t _ c h a r s e t _ n a m e
 *
 **************************************
 *
 * Functional description
 *  Look up an international text type object.
 *  If it doesn't exist, return empty string.
 *  Go directly to system tables & return only the
 *  name.
 *
 **************************************/
	thread_db* tdbb = JRD_get_thread_data();

	validateTransaction(transaction);

	dsql_dbb* dbb = transaction->getDsqlAttachment();

	if (charset_id == CS_dynamic)
		charset_id = tdbb->getCharSet();

	dsql_intlsym* sym = NULL;
	if (dbb->dbb_charsets_by_id.get(charset_id, sym))
		return sym->intlsym_name;

	QualifiedName name;

	AutoCacheRequest handle(tdbb, irq_cs_name, IRQ_REQUESTS);

	// Converted FOR loop #5: Get charset name by ID
	EXE_start(tdbb, handle, transaction);
	
	const jrd_req* const request = handle;
	bool found = false;
	
	USHORT isc_req_15_1[48] = {
		blr_version5,
		blr_begin,
		blr_message, 0, 1, 0,
			blr_short, 0,
		blr_receive, 0,
		blr_begin,
			blr_for,
				blr_rse, 1,
					blr_relation, 19, 'R','D','B','$','C','H','A','R','A','C','T','E','R','_','S','E','T','S', 0,
					blr_boolean,
						blr_eql,
							blr_field, 0, 20, 'R','D','B','$','C','H','A','R','A','C','T','E','R','_','S','E','T','_','I','D',
							blr_parameter, 0, 0, 0,
				blr_send, 1,
					blr_begin,
						blr_assignment,
							blr_field, 0, 24, 'R','D','B','$','C','H','A','R','A','C','T','E','R','_','S','E','T','_','N','A','M','E',
							blr_parameter, 1, 0, 0,
						blr_assignment,
							blr_field, 0, 16, 'R','D','B','$','S','C','H','E','M','A','_','N','A','M','E',
							blr_parameter, 1, 1, 0,
						blr_assignment,
							blr_literal, blr_short, 0, 1, 0,
							blr_parameter, 1, 2, 0,
					blr_end,
			blr_send, 1,
				blr_assignment,
					blr_literal, blr_short, 0, 0, 0,
					blr_parameter, 1, 2, 0,
		blr_end,
		blr_eoc
	};
	
	struct {
		SSHORT	isc_req_15_charset_id;
	} isc_req_15_in;
	
	struct {
		char	isc_req_15_charset_name[256];
		char	isc_req_15_schema_name[256];
		SSHORT	isc_req_15_found;
	} isc_req_15_out;
	
	isc_req_15_in.isc_req_15_charset_id = charset_id;
	
	EXE_send(tdbb, request, 0, sizeof(isc_req_15_in), (UCHAR*)&isc_req_15_in);
	
	while (true)
	{
		EXE_receive(tdbb, request, 1, sizeof(isc_req_15_out), (UCHAR*)&isc_req_15_out);
		if (!isc_req_15_out.isc_req_15_found)
			break;
			
		name = QualifiedName(isc_req_15_out.isc_req_15_charset_name, isc_req_15_out.isc_req_15_schema_name);
	}

	// put new charset into hash table if needed
	METD_get_charset(transaction, name);

	return name;
}


// Find the default character set for a database
QualifiedName METD_get_database_charset(jrd_tra* transaction)
{
	thread_db* tdbb = JRD_get_thread_data();

	validateTransaction(transaction);

	dsql_dbb* dbb = transaction->getDsqlAttachment();
	if (dbb->dbb_no_charset)
		return {};

	if (dbb->dbb_dfl_charset.object.hasData())
		return dbb->dbb_dfl_charset;

	// Now see if it is in the database

	static const CachedRequestId requestHandleId;
	AutoCacheRequest requestHandle(tdbb, requestHandleId);

	// Converted FOR loop #6: Get database default charset
	EXE_start(tdbb, requestHandle, transaction);
	
	const jrd_req* const request = requestHandle;
	bool found = false;
	
	USHORT isc_req_16_0[42] = {
		blr_version5,
		blr_begin,
		blr_message, 0, 0, 0,
		blr_receive, 0,
		blr_begin,
			blr_for,
				blr_rse, 1,
					blr_relation, 12, 'R','D','B','$','D','A','T','A','B','A','S','E', 0,
					blr_first, 
						blr_literal, blr_long, 0, 1, 0, 0, 0,
					blr_boolean,
						blr_not,
							blr_missing,
								blr_field, 0, 24, 'R','D','B','$','C','H','A','R','A','C','T','E','R','_','S','E','T','_','N','A','M','E',
				blr_send, 1,
					blr_begin,
						blr_assignment,
							blr_field, 0, 24, 'R','D','B','$','C','H','A','R','A','C','T','E','R','_','S','E','T','_','N','A','M','E',
							blr_parameter, 1, 0, 0,
						blr_assignment,
							blr_field, 0, 31, 'R','D','B','$','C','H','A','R','A','C','T','E','R','_','S','E','T','_','S','C','H','E','M','A','_','N','A','M','E',
							blr_parameter, 1, 1, 0,
						blr_assignment,
							blr_literal, blr_short, 0, 1, 0,
							blr_parameter, 1, 2, 0,
					blr_end,
			blr_send, 1,
				blr_assignment,
					blr_literal, blr_short, 0, 0, 0,
					blr_parameter, 1, 2, 0,
		blr_end,
		blr_eoc
	};
	
	struct {
		char	isc_req_16_charset_name[256];
		char	isc_req_16_schema_name[256];
		SSHORT	isc_req_16_found;
	} isc_req_16_out;
	
	EXE_send(tdbb, request, 0, 0, NULL);
	
	while (true)
	{
		EXE_receive(tdbb, request, 1, sizeof(isc_req_16_out), (UCHAR*)&isc_req_16_out);
		if (!isc_req_16_out.isc_req_16_found)
			break;
			
		dbb->dbb_dfl_charset = QualifiedName(isc_req_16_out.isc_req_16_charset_name, isc_req_16_out.isc_req_16_schema_name);
	}

	if (dbb->dbb_dfl_charset.object.isEmpty())
	{
		fb_assert(false);
		dbb->dbb_no_charset = true;
	}

	return dbb->dbb_dfl_charset;
}


// Find the default character set for a schema
QualifiedName METD_get_schema_charset(jrd_tra* transaction, const MetaName& schema)
{
	thread_db* tdbb = JRD_get_thread_data();

	validateTransaction(transaction);

	dsql_dbb* dbb = transaction->getDsqlAttachment();

	if (const auto charSet = dbb->dbb_schemas_dfl_charset.get(schema))
		return *charSet;

	// Now see if it is in the database

	static const CachedRequestId requestHandleId;
	AutoCacheRequest requestHandle(tdbb, requestHandleId);

	// Converted FOR loop #7: Get schema default charset
	EXE_start(tdbb, requestHandle, transaction);
	
	const jrd_req* const request = requestHandle;
	bool found = false;
	
	USHORT isc_req_17_1[85] = {
		blr_version5,
		blr_begin,
		blr_message, 0, 1, 0,
			blr_varying2, 3, 0, 0, 0,
		blr_receive, 0,
		blr_begin,
			blr_for,
				blr_rse, 1,
					blr_cross,
						blr_relation, 11, 'R','D','B','$','S','C','H','E','M','A','S', 0,
						blr_relation, 12, 'R','D','B','$','D','A','T','A','B','A','S','E', 1,
					blr_first, 
						blr_literal, blr_long, 0, 1, 0, 0, 0,
					blr_boolean,
						blr_eql,
							blr_field, 0, 16, 'R','D','B','$','S','C','H','E','M','A','_','N','A','M','E',
							blr_parameter, 0, 0, 0,
				blr_send, 1,
					blr_begin,
						blr_assignment,
							blr_field, 0, 24, 'R','D','B','$','C','H','A','R','A','C','T','E','R','_','S','E','T','_','N','A','M','E',
							blr_parameter, 1, 0, 0,
						blr_assignment,
							blr_field, 0, 31, 'R','D','B','$','C','H','A','R','A','C','T','E','R','_','S','E','T','_','S','C','H','E','M','A','_','N','A','M','E',
							blr_parameter, 1, 1, 0,
						blr_assignment,
							blr_field, 1, 24, 'R','D','B','$','C','H','A','R','A','C','T','E','R','_','S','E','T','_','N','A','M','E',
							blr_parameter, 1, 2, 0,
						blr_assignment,
							blr_field, 1, 31, 'R','D','B','$','C','H','A','R','A','C','T','E','R','_','S','E','T','_','S','C','H','E','M','A','_','N','A','M','E',
							blr_parameter, 1, 3, 0,
						blr_assignment,
							blr_literal, blr_short, 0, 1, 0,
							blr_parameter, 1, 4, 0,
					blr_end,
			blr_send, 1,
				blr_assignment,
					blr_literal, blr_short, 0, 0, 0,
					blr_parameter, 1, 4, 0,
		blr_end,
		blr_eoc
	};
	
	struct {
		char	isc_req_17_schema[256];
	} isc_req_17_in;
	
	struct {
		char	isc_req_17_sch_charset_name[256];
		char	isc_req_17_sch_schema_name[256];
		char	isc_req_17_db_charset_name[256];
		char	isc_req_17_db_schema_name[256];
		SSHORT	isc_req_17_found;
	} isc_req_17_out;
	
	strcpy(isc_req_17_in.isc_req_17_schema, schema.c_str());
	
	EXE_send(tdbb, request, 0, sizeof(isc_req_17_in), (UCHAR*)&isc_req_17_in);
	
	while (true)
	{
		EXE_receive(tdbb, request, 1, sizeof(isc_req_17_out), (UCHAR*)&isc_req_17_out);
		if (!isc_req_17_out.isc_req_17_found)
			break;
			
		if (isc_req_17_out.isc_req_17_sch_charset_name[0] != '\0')
		{
			QualifiedName charSet(isc_req_17_out.isc_req_17_sch_charset_name, isc_req_17_out.isc_req_17_sch_schema_name);
			dbb->dbb_schemas_dfl_charset.put(schema, charSet);
			return charSet;
		}
		else if (isc_req_17_out.isc_req_17_db_charset_name[0] != '\0')
		{
			QualifiedName charSet(isc_req_17_out.isc_req_17_db_charset_name, isc_req_17_out.isc_req_17_db_schema_name);
			return charSet;
		}
	}

	fb_assert(false);
	return {};
}


static void convert_dtype(TypeClause* field, SSHORT field_type)
{
/**************************************
 *
 *  c o n v e r t _ d t y p e
 *
 **************************************
 *
 * Functional description
 *  Convert from the blr_<type> stored in system metadata
 *  to the internal dtype_* descriptor.  Also set field
 *  length.
 *
 **************************************/

	// fill out the type descriptor
	switch (field_type)
	{
	case blr_text:
		field->dtype = dtype_text;
		break;
	case blr_varying:
		field->dtype = dtype_varying;
		field->length += sizeof(USHORT);
		break;
	case blr_blob:
		field->dtype = dtype_blob;
		field->length = type_lengths[field->dtype];
		break;
	default:
		field->dtype = gds_cvt_blr_dtype[field_type];
		field->length = type_lengths[field->dtype];

		fb_assert(field->dtype != dtype_unknown);
	}
}


#ifdef NOT_USED_OR_REPLACED
static void free_procedure(dsql_prc* procedure)
{
/**************************************
 *
 *  f r e e _ p r o c e d u r e
 *
 **************************************
 *
 * Functional description
 *  Free memory allocated for a procedure block and params
 *
 **************************************/
	dsql_fld* param;

	// release the input & output parameter blocks

	for (param = procedure->prc_inputs; param;)
	{
		dsql_fld* temp = param;
		param = param->fld_next;
		delete temp;
	}

	for (param = procedure->prc_outputs; param;)
	{
		dsql_fld* temp = param;
		param = param->fld_next;
		delete temp;
	}

	// release the procedure & symbol blocks

	delete procedure;
}
#endif	// NOT_USED_OR_REPLACED


static void free_relation(dsql_rel* relation)
{
/**************************************
 *
 *  f r e e _ r e l a t i o n
 *
 **************************************
 *
 * Functional description
 *  Free memory allocated for a relation block and fields
 *
 **************************************/

	// release the field blocks

	for (dsql_fld* field = relation->rel_fields; field;)
	{
		dsql_fld* temp = field;
		field = field->fld_next;
		delete temp;
	}

	// release the relation & symbol blocks

	delete relation;
}


bool METD_get_domain(jrd_tra* transaction, TypeClause* field, const QualifiedName& name)
{
/**************************************
 *
 *  M E T D _ g e t _ d o m a i n
 *
 **************************************
 *
 * Functional description
 *  Fetch domain information for field defined as 'name'
 *
 **************************************/
	thread_db* tdbb = JRD_get_thread_data();

	validateTransaction(transaction);

	bool found = false;

	AutoCacheRequest handle(tdbb, irq_domain, IRQ_REQUESTS);

	// Converted FOR loop #8: Get domain information from RDB$FIELDS
	EXE_start(tdbb, handle, transaction);
	
	const jrd_req* const request = handle;
	
	USHORT isc_req_18_2[78] = {
		blr_version5,
		blr_begin,
		blr_message, 0, 2, 0,
			blr_varying2, 3, 0, 0, 0,
			blr_varying2, 3, 0, 0, 0,
		blr_receive, 0,
		blr_begin,
			blr_for,
				blr_rse, 1,
					blr_relation, 10, 'R','D','B','$','F','I','E','L','D','S', 0,
					blr_boolean,
						blr_and,
							blr_eql,
								blr_field, 0, 16, 'R','D','B','$','S','C','H','E','M','A','_','N','A','M','E',
								blr_parameter, 0, 0, 0,
							blr_eql,
								blr_field, 0, 15, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E',
								blr_parameter, 0, 1, 0,
				blr_send, 1,
					blr_begin,
						blr_assignment,
							blr_field, 0, 17, 'R','D','B','$','F','I','E','L','D','_','L','E','N','G','T','H',
							blr_parameter, 1, 0, 0,
						blr_assignment,
							blr_field, 0, 16, 'R','D','B','$','F','I','E','L','D','_','S','C','A','L','E',
							blr_parameter, 1, 1, 0,
						blr_assignment,
							blr_field, 0, 15, 'R','D','B','$','F','I','E','L','D','_','T','Y','P','E',
							blr_parameter, 1, 2, 0,
						blr_assignment,
							blr_field, 0, 19, 'R','D','B','$','F','I','E','L','D','_','S','U','B','_','T','Y','P','E',
							blr_parameter, 1, 3, 0,
						blr_assignment,
							blr_field, 0, 15, 'R','D','B','$','D','I','M','E','N','S','I','O','N','S',
							blr_parameter, 1, 4, 0,
						blr_assignment,
							blr_field, 0, 20, 'R','D','B','$','C','H','A','R','A','C','T','E','R','_','S','E','T','_','I','D',
							blr_parameter, 1, 5, 0,
						blr_assignment,
							blr_field, 0, 17, 'R','D','B','$','C','O','L','L','A','T','I','O','N','_','I','D',
							blr_parameter, 1, 6, 0,
						blr_assignment,
							blr_field, 0, 19, 'R','D','B','$','C','H','A','R','A','C','T','E','R','_','L','E','N','G','T','H',
							blr_parameter, 1, 7, 0,
						blr_assignment,
							blr_field, 0, 16, 'R','D','B','$','C','O','M','P','U','T','E','D','_','B','L','R',
							blr_parameter, 1, 8, 0,
						blr_assignment,
							blr_field, 0, 13, 'R','D','B','$','N','U','L','L','_','F','L','A','G',
							blr_parameter, 1, 9, 0,
						blr_assignment,
							blr_field, 0, 16, 'R','D','B','$','S','Y','S','T','E','M','_','F','L','A','G',
							blr_parameter, 1, 10, 0,
						blr_assignment,
							blr_field, 0, 18, 'R','D','B','$','S','E','G','M','E','N','T','_','L','E','N','G','T','H',
							blr_parameter, 1, 11, 0,
						blr_assignment,
							blr_literal, blr_short, 0, 1, 0,
							blr_parameter, 1, 12, 0,
					blr_end,
			blr_send, 1,
				blr_assignment,
					blr_literal, blr_short, 0, 0, 0,
					blr_parameter, 1, 12, 0,
		blr_end,
		blr_eoc
	};
	
	struct {
		char	isc_req_18_schema[256];
		char	isc_req_18_field_name[256];
	} isc_req_18_in;
	
	struct {
		SSHORT	isc_req_18_field_length;
		SSHORT	isc_req_18_field_scale;
		SSHORT	isc_req_18_field_type;
		SSHORT	isc_req_18_field_sub_type;
		SSHORT	isc_req_18_dimensions;
		SSHORT	isc_req_18_charset_id;
		SSHORT	isc_req_18_collation_id;
		SSHORT	isc_req_18_char_length;
		SLONG	isc_req_18_computed_blr;
		SSHORT	isc_req_18_null_flag;
		SSHORT	isc_req_18_system_flag;
		SSHORT	isc_req_18_segment_length;
		SSHORT	isc_req_18_found;
	} isc_req_18_out;
	
	strcpy(isc_req_18_in.isc_req_18_schema, name.schema.c_str());
	strcpy(isc_req_18_in.isc_req_18_field_name, name.object.c_str());
	
	EXE_send(tdbb, request, 0, sizeof(isc_req_18_in), (UCHAR*)&isc_req_18_in);
	
	while (true)
	{
		EXE_receive(tdbb, request, 1, sizeof(isc_req_18_out), (UCHAR*)&isc_req_18_out);
		if (!isc_req_18_out.isc_req_18_found)
			break;
			
		found = true;
		field->length = isc_req_18_out.isc_req_18_field_length;
		field->scale = isc_req_18_out.isc_req_18_field_scale;
		field->subType = isc_req_18_out.isc_req_18_field_sub_type;
		field->dimensions = isc_req_18_out.isc_req_18_dimensions == 0 ? 0 : isc_req_18_out.isc_req_18_dimensions;

		field->charSetId = std::nullopt;
		if (isc_req_18_out.isc_req_18_charset_id != 0)
			field->charSetId = isc_req_18_out.isc_req_18_charset_id;
		field->collationId = 0;
		if (isc_req_18_out.isc_req_18_collation_id != 0)
			field->collationId = isc_req_18_out.isc_req_18_collation_id;
		field->charLength = 0;
		if (isc_req_18_out.isc_req_18_char_length != 0)
			field->charLength = isc_req_18_out.isc_req_18_char_length;

		if (isc_req_18_out.isc_req_18_computed_blr != 0)
			field->flags |= FLD_computed;

		if (isc_req_18_out.isc_req_18_null_flag == 0 || isc_req_18_out.isc_req_18_null_flag == -1)
			field->flags |= FLD_nullable;

		if (isc_req_18_out.isc_req_18_system_flag == 1)
			field->flags |= FLD_system;

		convert_dtype(field, isc_req_18_out.isc_req_18_field_type);

		if (isc_req_18_out.isc_req_18_field_type == blr_blob) {
			field->segLength = isc_req_18_out.isc_req_18_segment_length;
		}
	}

	return found;
}


dsql_udf* METD_get_function(jrd_tra* transaction, DsqlCompilerScratch* dsqlScratch, const QualifiedName& name)
{
/**************************************
 *
 *  M E T D _ g e t _ f u n c t i o n
 *
 **************************************
 *
 * Functional description
 *  Look up a user defined function.  If it doesn't exist,
 *  return NULL.
 *
 **************************************/
	thread_db* tdbb = JRD_get_thread_data();

	validateTransaction(transaction);

	dsql_dbb* dbb = transaction->getDsqlAttachment();

	// Start by seeing if symbol is already defined

	dsql_udf* userFunc = NULL;
	if (dbb->dbb_functions.get(name, userFunc))
	{
		if (MET_dsql_cache_use(tdbb, SYM_udf, name))
			userFunc->udf_flags |= UDF_dropped;
	}

	if (userFunc && (userFunc->udf_flags & UDF_dropped))
		userFunc = nullptr;

	if (userFunc)
		return userFunc;

	// Now see if it is in the database

	USHORT return_arg = 0;

	AutoCacheRequest handle1(tdbb, irq_function, IRQ_REQUESTS);

	// Converted FOR loop #9: Get function metadata from RDB$FUNCTIONS
	EXE_start(tdbb, handle1, transaction);
	
	const jrd_req* const request1 = handle1;
	
	USHORT isc_req_19_3[85] = {
		blr_version5,
		blr_begin,
		blr_message, 0, 3, 0,
			blr_varying2, 3, 0, 0, 0,
			blr_varying2, 3, 0, 0, 0,
			blr_varying2, 3, 0, 0, 0,
		blr_receive, 0,
		blr_begin,
			blr_for,
				blr_rse, 1,
					blr_relation, 13, 'R','D','B','$','F','U','N','C','T','I','O','N','S', 0,
					blr_boolean,
						blr_and,
							blr_and,
								blr_eql,
									blr_field, 0, 16, 'R','D','B','$','S','C','H','E','M','A','_','N','A','M','E',
									blr_parameter, 0, 0, 0,
								blr_eql,
									blr_field, 0, 18, 'R','D','B','$','F','U','N','C','T','I','O','N','_','N','A','M','E',
									blr_parameter, 0, 1, 0,
							blr_equiv,
								blr_field, 0, 17, 'R','D','B','$','P','A','C','K','A','G','E','_','N','A','M','E',
								blr_parameter, 0, 2, 0,
				blr_send, 1,
					blr_begin,
						blr_assignment,
							blr_field, 0, 16, 'R','D','B','$','P','R','I','V','A','T','E','_','F','L','A','G',
							blr_parameter, 1, 0, 0,
						blr_assignment,
							blr_field, 0, 19, 'R','D','B','$','R','E','T','U','R','N','_','A','R','G','U','M','E','N','T',
							blr_parameter, 1, 1, 0,
						blr_assignment,
							blr_literal, blr_short, 0, 1, 0,
							blr_parameter, 1, 2, 0,
					blr_end,
			blr_send, 1,
				blr_assignment,
					blr_literal, blr_short, 0, 0, 0,
					blr_parameter, 1, 2, 0,
		blr_end,
		blr_eoc
	};
	
	struct {
		char	isc_req_19_schema[256];
		char	isc_req_19_function[256];
		char	isc_req_19_package[256];
	} isc_req_19_in;
	
	struct {
		SSHORT	isc_req_19_private_flag;
		USHORT	isc_req_19_return_arg;
		SSHORT	isc_req_19_found;
	} isc_req_19_out;
	
	strcpy(isc_req_19_in.isc_req_19_schema, name.schema.c_str());
	strcpy(isc_req_19_in.isc_req_19_function, name.object.c_str());
	strcpy(isc_req_19_in.isc_req_19_package, name.package.hasData() ? name.package.c_str() : "");
	
	EXE_send(tdbb, request1, 0, sizeof(isc_req_19_in), (UCHAR*)&isc_req_19_in);
	
	while (true)
	{
		EXE_receive(tdbb, request1, 1, sizeof(isc_req_19_out), (UCHAR*)&isc_req_19_out);
		if (!isc_req_19_out.isc_req_19_found)
			break;
			
		userFunc = FB_NEW_POOL(dbb->dbb_pool) dsql_udf(dbb->dbb_pool);
		userFunc->udf_name = name;
		userFunc->udf_private = isc_req_19_out.isc_req_19_private_flag != 0;
		return_arg = isc_req_19_out.isc_req_19_return_arg;
	}

	if (!userFunc)
		return nullptr;

	SSHORT defaults = 0;

	AutoCacheRequest handle2(tdbb, irq_func_return, IRQ_REQUESTS);

	// Converted FOR loop #10: Get function arguments from RDB$FUNCTION_ARGUMENTS  
	EXE_start(tdbb, handle2, transaction);
	
	const jrd_req* const request2 = handle2;
	
	USHORT isc_req_20_3[98] = {
		blr_version5,
		blr_begin,
		blr_message, 0, 3, 0,
			blr_varying2, 3, 0, 0, 0,
			blr_varying2, 3, 0, 0, 0,
			blr_varying2, 3, 0, 0, 0,
		blr_receive, 0,
		blr_begin,
			blr_for,
				blr_rse, 1,
					blr_relation, 23, 'R','D','B','$','F','U','N','C','T','I','O','N','_','A','R','G','U','M','E','N','T','S', 0,
					blr_sort, 1,
						blr_field, 0, 19, 'R','D','B','$','A','R','G','U','M','E','N','T','_','P','O','S','I','T','I','O','N',
					blr_boolean,
						blr_and,
							blr_and,
								blr_eql,
									blr_field, 0, 16, 'R','D','B','$','S','C','H','E','M','A','_','N','A','M','E',
									blr_parameter, 0, 0, 0,
								blr_eql,
									blr_field, 0, 18, 'R','D','B','$','F','U','N','C','T','I','O','N','_','N','A','M','E',
									blr_parameter, 0, 1, 0,
							blr_equiv,
								blr_field, 0, 17, 'R','D','B','$','P','A','C','K','A','G','E','_','N','A','M','E',
								blr_parameter, 0, 2, 0,
				blr_send, 1,
					blr_begin,
						blr_assignment,
							blr_field, 0, 19, 'R','D','B','$','A','R','G','U','M','E','N','T','_','P','O','S','I','T','I','O','N',
							blr_parameter, 1, 0, 0,
						blr_assignment,
							blr_field, 0, 15, 'R','D','B','$','F','I','E','L','D','_','S','O','U','R','C','E',
							blr_parameter, 1, 1, 0,
						blr_assignment,
							blr_field, 0, 22, 'R','D','B','$','F','I','E','L','D','_','S','O','U','R','C','E','_','S','C','H','E','M','A','_','N','A','M','E',
							blr_parameter, 1, 2, 0,
						blr_assignment,
							blr_field, 0, 15, 'R','D','B','$','F','I','E','L','D','_','T','Y','P','E',
							blr_parameter, 1, 3, 0,
						blr_assignment,
							blr_field, 0, 16, 'R','D','B','$','F','I','E','L','D','_','S','C','A','L','E',
							blr_parameter, 1, 4, 0,
						blr_assignment,
							blr_field, 0, 17, 'R','D','B','$','F','I','E','L','D','_','L','E','N','G','T','H',
							blr_parameter, 1, 5, 0,
						blr_assignment,
							blr_field, 0, 19, 'R','D','B','$','F','I','E','L','D','_','S','U','B','_','T','Y','P','E',
							blr_parameter, 1, 6, 0,
						blr_assignment,
							blr_field, 0, 20, 'R','D','B','$','C','H','A','R','A','C','T','E','R','_','S','E','T','_','I','D',
							blr_parameter, 1, 7, 0,
						blr_assignment,
							blr_field, 0, 13, 'R','D','B','$','M','E','C','H','A','N','I','S','M',
							blr_parameter, 1, 8, 0,
						blr_assignment,
							blr_field, 0, 17, 'R','D','B','$','A','R','G','U','M','E','N','T','_','N','A','M','E',
							blr_parameter, 1, 9, 0,
						blr_assignment,
							blr_field, 0, 17, 'R','D','B','$','D','E','F','A','U','L','T','_','V','A','L','U','E',
							blr_parameter, 1, 10, 0,
						blr_assignment,
							blr_literal, blr_short, 0, 1, 0,
							blr_parameter, 1, 11, 0,
					blr_end,
			blr_send, 1,
				blr_assignment,
					blr_literal, blr_short, 0, 0, 0,
					blr_parameter, 1, 11, 0,
		blr_end,
		blr_eoc
	};
	
	struct {
		char	isc_req_20_schema[256];
		char	isc_req_20_function[256];
		char	isc_req_20_package[256];
	} isc_req_20_in;
	
	struct {
		USHORT	isc_req_20_arg_position;
		char	isc_req_20_field_source[256];
		char	isc_req_20_field_source_schema[256];
		SSHORT	isc_req_20_field_type;
		SSHORT	isc_req_20_field_scale;
		SSHORT	isc_req_20_field_length;
		SSHORT	isc_req_20_field_sub_type;
		SSHORT	isc_req_20_charset_id;
		SSHORT	isc_req_20_mechanism;
		char	isc_req_20_arg_name[256];
		SLONG	isc_req_20_default_value;
		SSHORT	isc_req_20_found;
	} isc_req_20_out;
	
	strcpy(isc_req_20_in.isc_req_20_schema, name.schema.c_str());
	strcpy(isc_req_20_in.isc_req_20_function, name.object.c_str());
	strcpy(isc_req_20_in.isc_req_20_package, name.package.hasData() ? name.package.c_str() : "");
	
	EXE_send(tdbb, request2, 0, sizeof(isc_req_20_in), (UCHAR*)&isc_req_20_in);
	
	while (true)
	{
		EXE_receive(tdbb, request2, 1, sizeof(isc_req_20_out), (UCHAR*)&isc_req_20_out);
		if (!isc_req_20_out.isc_req_20_found)
			break;
			
		if (isc_req_20_out.isc_req_20_field_source[0] != '\0')
		{
			// Handle field source case with nested query for RDB$FIELDS
			AutoCacheRequest handle3(tdbb, irq_func_ret_fld, IRQ_REQUESTS);
			
			// Converted FOR loop #11: Get field details from RDB$FIELDS for function arguments
			EXE_start(tdbb, handle3, transaction);
			
			const jrd_req* const request3 = handle3;
			
			USHORT isc_req_21_2[65] = {
				blr_version5,
				blr_begin,
				blr_message, 0, 2, 0,
					blr_varying2, 3, 0, 0, 0,
					blr_varying2, 3, 0, 0, 0,
				blr_receive, 0,
				blr_begin,
					blr_for,
						blr_rse, 1,
							blr_relation, 10, 'R','D','B','$','F','I','E','L','D','S', 0,
							blr_boolean,
								blr_and,
									blr_eql,
										blr_field, 0, 16, 'R','D','B','$','S','C','H','E','M','A','_','N','A','M','E',
										blr_parameter, 0, 0, 0,
									blr_eql,
										blr_field, 0, 15, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E',
										blr_parameter, 0, 1, 0,
						blr_send, 1,
							blr_begin,
								blr_assignment,
									blr_field, 0, 15, 'R','D','B','$','F','I','E','L','D','_','T','Y','P','E',
									blr_parameter, 1, 0, 0,
								blr_assignment,
									blr_field, 0, 16, 'R','D','B','$','F','I','E','L','D','_','S','C','A','L','E',
									blr_parameter, 1, 1, 0,
								blr_assignment,
									blr_field, 0, 17, 'R','D','B','$','F','I','E','L','D','_','L','E','N','G','T','H',
									blr_parameter, 1, 2, 0,
								blr_assignment,
									blr_field, 0, 19, 'R','D','B','$','F','I','E','L','D','_','S','U','B','_','T','Y','P','E',
									blr_parameter, 1, 3, 0,
								blr_assignment,
									blr_field, 0, 20, 'R','D','B','$','C','H','A','R','A','C','T','E','R','_','S','E','T','_','I','D',
									blr_parameter, 1, 4, 0,
								blr_assignment,
									blr_field, 0, 15, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E',
									blr_parameter, 1, 5, 0,
								blr_assignment,
									blr_field, 0, 17, 'R','D','B','$','D','E','F','A','U','L','T','_','V','A','L','U','E',
									blr_parameter, 1, 6, 0,
								blr_assignment,
									blr_literal, blr_short, 0, 1, 0,
									blr_parameter, 1, 7, 0,
							blr_end,
					blr_send, 1,
						blr_assignment,
							blr_literal, blr_short, 0, 0, 0,
							blr_parameter, 1, 7, 0,
				blr_end,
				blr_eoc
			};
			
			struct {
				char	isc_req_21_schema[256];
				char	isc_req_21_field_name[256];
			} isc_req_21_in;
			
			struct {
				SSHORT	isc_req_21_field_type;
				SSHORT	isc_req_21_field_scale;
				SSHORT	isc_req_21_field_length;
				SSHORT	isc_req_21_field_sub_type;
				SSHORT	isc_req_21_charset_id;
				char	isc_req_21_field_name_out[256];
				SLONG	isc_req_21_default_value;
				SSHORT	isc_req_21_found;
			} isc_req_21_out;
			
			strcpy(isc_req_21_in.isc_req_21_schema, isc_req_20_out.isc_req_20_field_source_schema);
			strcpy(isc_req_21_in.isc_req_21_field_name, isc_req_20_out.isc_req_20_field_source);
			
			EXE_send(tdbb, request3, 0, sizeof(isc_req_21_in), (UCHAR*)&isc_req_21_in);
			
			while (true)
			{
				EXE_receive(tdbb, request3, 1, sizeof(isc_req_21_out), (UCHAR*)&isc_req_21_out);
				if (!isc_req_21_out.isc_req_21_found)
					break;
					
				if (isc_req_20_out.isc_req_20_arg_position == return_arg)
				{
					// This is the return value
					userFunc->udf_dtype = (isc_req_21_out.isc_req_21_field_type != blr_blob) ?
						gds_cvt_blr_dtype[isc_req_21_out.isc_req_21_field_type] : dtype_blob;
					userFunc->udf_scale = isc_req_21_out.isc_req_21_field_scale;
					userFunc->udf_sub_type = isc_req_21_out.isc_req_21_field_sub_type;
					
					if (isc_req_21_out.isc_req_21_field_type == blr_blob)
						userFunc->udf_length = sizeof(ISC_QUAD);
					else
						userFunc->udf_length = isc_req_21_out.isc_req_21_field_length;

					if (isc_req_21_out.isc_req_21_charset_id != 0) {
						userFunc->udf_character_set_id = isc_req_21_out.isc_req_21_charset_id;
					}
					
					// Check if based on system domain/relation
					if (isSystemDomain(tdbb, transaction,
							QualifiedName(isc_req_20_out.isc_req_20_field_source, isc_req_20_out.isc_req_20_field_source_schema)))
					{
						userFunc->udf_flags |= UDF_sys_based;
					}
				}
				else
				{
					// This is an input argument
					DSC d;

					if (isc_req_20_out.isc_req_20_mechanism == FUN_scalar_array)
					{
						d.dsc_dtype = dtype_array;
						d.dsc_scale = 0;
						d.dsc_sub_type = 0;
						d.dsc_length = sizeof(ISC_QUAD);
						d.dsc_flags = DSC_nullable;
					}
					else
					{
						d.dsc_dtype = (isc_req_21_out.isc_req_21_field_type != blr_blob) ?
							gds_cvt_blr_dtype[isc_req_21_out.isc_req_21_field_type] : dtype_blob;
						// dimitr: adjust the UDF arguments for CSTRING
						if (d.dsc_dtype == dtype_cstring) {
							d.dsc_dtype = dtype_text;
						}
						d.dsc_scale = isc_req_21_out.isc_req_21_field_scale;
						d.dsc_sub_type = isc_req_21_out.isc_req_21_field_sub_type;
						d.dsc_length = isc_req_21_out.isc_req_21_field_length;
						if (d.dsc_dtype == dtype_varying) {
							d.dsc_length += sizeof(USHORT);
						}

						if (isc_req_21_out.isc_req_21_charset_id != 0)
						{
							if (d.dsc_dtype != dtype_blob) {
								d.dsc_ttype() = isc_req_21_out.isc_req_21_charset_id;
							} else {
								d.dsc_scale = isc_req_21_out.isc_req_21_charset_id;
							}
						}

						if (isc_req_20_out.isc_req_20_mechanism != FUN_value && isc_req_20_out.isc_req_20_mechanism != FUN_reference)
						{
							d.dsc_flags = DSC_nullable;
						}
					}

					d.dsc_address = NULL;

					if (isc_req_20_out.isc_req_20_default_value != 0 ||
						(fb_utils::implicit_domain(isc_req_21_out.isc_req_21_field_name_out) && isc_req_21_out.isc_req_21_default_value != 0))
					{
						defaults++;
					}

					auto& argument = userFunc->udf_arguments.add();
					argument.name = isc_req_20_out.isc_req_20_arg_name;
					argument.desc = d;
				}
			}
		}
		else
		{
			// Handle direct field type case (no field source)
			if (isc_req_20_out.isc_req_20_arg_position == return_arg)
			{
				userFunc->udf_dtype = (isc_req_20_out.isc_req_20_field_type != blr_blob) ?
					gds_cvt_blr_dtype[isc_req_20_out.isc_req_20_field_type] : dtype_blob;
				userFunc->udf_scale = isc_req_20_out.isc_req_20_field_scale;
				userFunc->udf_sub_type = isc_req_20_out.isc_req_20_field_sub_type;
				
				if (isc_req_20_out.isc_req_20_field_type == blr_blob)
					userFunc->udf_length = sizeof(ISC_QUAD);
				else
					userFunc->udf_length = isc_req_20_out.isc_req_20_field_length;

				if (isc_req_20_out.isc_req_20_charset_id != 0) {
					userFunc->udf_character_set_id = isc_req_20_out.isc_req_20_charset_id;
				}
			}
			else
			{
				DSC d;

				if (isc_req_20_out.isc_req_20_mechanism == FUN_scalar_array)
				{
					d.dsc_dtype = dtype_array;
					d.dsc_scale = 0;
					d.dsc_sub_type = 0;
					d.dsc_length = sizeof(ISC_QUAD);
					d.dsc_flags = DSC_nullable;
				}
				else
				{
					d.dsc_dtype = (isc_req_20_out.isc_req_20_field_type != blr_blob) ?
						gds_cvt_blr_dtype[isc_req_20_out.isc_req_20_field_type] : dtype_blob;
					// dimitr: adjust the UDF arguments for CSTRING
					if (d.dsc_dtype == dtype_cstring) {
						d.dsc_dtype = dtype_text;
					}
					d.dsc_scale = isc_req_20_out.isc_req_20_field_scale;
					d.dsc_sub_type = isc_req_20_out.isc_req_20_field_sub_type;
					d.dsc_length = isc_req_20_out.isc_req_20_field_length;
					if (d.dsc_dtype == dtype_varying) {
						d.dsc_length += sizeof(USHORT);
					}

					if (isc_req_20_out.isc_req_20_charset_id != 0)
					{
						if (d.dsc_dtype != dtype_blob) {
							d.dsc_ttype() = isc_req_20_out.isc_req_20_charset_id;
						} else {
							d.dsc_scale = isc_req_20_out.isc_req_20_charset_id;
						}
					}

					if (isc_req_20_out.isc_req_20_mechanism != FUN_value && isc_req_20_out.isc_req_20_mechanism != FUN_reference)
					{
						d.dsc_flags = DSC_nullable;
					}
				}

				d.dsc_address = NULL;

				if (isc_req_20_out.isc_req_20_default_value != 0)
				{
					defaults++;
				}

				auto& argument = userFunc->udf_arguments.add();
				argument.name = isc_req_20_out.isc_req_20_arg_name;
				argument.desc = d;
			}
		}
	}

	userFunc->udf_args.resize(userFunc->udf_arguments.getCount());
	for (unsigned i = 0; i < userFunc->udf_arguments.getCount(); ++i)
		userFunc->udf_args[i] = &userFunc->udf_arguments[i].desc;

	userFunc->udf_def_count = defaults;

	if (dbb->dbb_functions.get(name, userFunc))
	{
		delete userFunc;
		return userFunc;
	}

	dbb->dbb_functions.put(userFunc->udf_name, userFunc);
	MET_dsql_cache_use(tdbb, SYM_udf, userFunc->udf_name);

	return userFunc;
}


bool METD_get_type(jrd_tra* transaction, const MetaName& name, const char* field, SSHORT* value)
{
/**************************************
 *
 *  M E T D _ g e t _ t y p e
 *
 **************************************
 *
 * Functional description
 *  Look up a symbolic name in RDB$TYPES
 *
 **************************************/
	thread_db* tdbb = JRD_get_thread_data();

	validateTransaction(transaction);

	bool found = false;

	AutoCacheRequest handle(tdbb, irq_type, IRQ_REQUESTS);

	// Converted FOR loop #12: Get type information from RDB$TYPES
	EXE_start(tdbb, handle, transaction);
	
	const jrd_req* const request = handle;
	
	USHORT isc_req_22_2[58] = {
		blr_version5,
		blr_begin,
		blr_message, 0, 2, 0,
			blr_varying2, 3, 0, 0, 0,
			blr_varying2, 3, 0, 0, 0,
		blr_receive, 0,
		blr_begin,
			blr_for,
				blr_rse, 1,
					blr_relation, 10, 'R','D','B','$','T','Y','P','E','S', 0,
					blr_boolean,
						blr_and,
							blr_eql,
								blr_field, 0, 15, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E',
								blr_parameter, 0, 0, 0,
							blr_eql,
								blr_field, 0, 14, 'R','D','B','$','T','Y','P','E','_','N','A','M','E',
								blr_parameter, 0, 1, 0,
				blr_send, 1,
					blr_begin,
						blr_assignment,
							blr_field, 0, 9, 'R','D','B','$','T','Y','P','E',
							blr_parameter, 1, 0, 0,
						blr_assignment,
							blr_literal, blr_short, 0, 1, 0,
							blr_parameter, 1, 1, 0,
					blr_end,
			blr_send, 1,
				blr_assignment,
					blr_literal, blr_short, 0, 0, 0,
					blr_parameter, 1, 1, 0,
		blr_end,
		blr_eoc
	};
	
	struct {
		char	isc_req_22_field_name[256];
		char	isc_req_22_type_name[256];
	} isc_req_22_in;
	
	struct {
		SSHORT	isc_req_22_type;
		SSHORT	isc_req_22_found;
	} isc_req_22_out;
	
	strcpy(isc_req_22_in.isc_req_22_field_name, field);
	strcpy(isc_req_22_in.isc_req_22_type_name, name.c_str());
	
	EXE_send(tdbb, request, 0, sizeof(isc_req_22_in), (UCHAR*)&isc_req_22_in);
	
	while (true)
	{
		EXE_receive(tdbb, request, 1, sizeof(isc_req_22_out), (UCHAR*)&isc_req_22_out);
		if (!isc_req_22_out.isc_req_22_found)
			break;
			
		found = true;
		*value = isc_req_22_out.isc_req_22_type;
	}

	return found;
}


void METD_get_primary_key(jrd_tra* transaction, const QualifiedName& relationName,
	Array<NestConst<FieldNode> >& fields)
{
/**************************************
 *
 *  M E T D _ g e t _ p r i m a r y _ k e y
 *
 **************************************
 *
 * Functional description
 *  Lookup the fields for the primary key
 *  index on a relation, returning a list
 *  node of the fields.
 *
 **************************************/
	thread_db* tdbb = JRD_get_thread_data();
	MemoryPool& pool = *tdbb->getDefaultPool();

	validateTransaction(transaction);

	AutoCacheRequest handle(tdbb, irq_primary_key, IRQ_REQUESTS);

	// Converted FOR loop #13: Get primary key fields from RDB$INDICES/RDB$INDEX_SEGMENTS/RDB$RELATION_CONSTRAINTS
	EXE_start(tdbb, handle, transaction);
	
	const jrd_req* const request = handle;
	
	USHORT isc_req_23_1[128] = {
		blr_version5,
		blr_begin,
		blr_message, 0, 1, 0,
			blr_varying2, 3, 0, 0, 0,
		blr_receive, 0,
		blr_begin,
			blr_for,
				blr_rse, 1,
					blr_cross,
						blr_cross,
							blr_relation, 12, 'R','D','B','$','I','N','D','I','C','E','S', 0,
							blr_relation, 20, 'R','D','B','$','I','N','D','E','X','_','S','E','G','M','E','N','T','S', 1,
						blr_relation, 25, 'R','D','B','$','R','E','L','A','T','I','O','N','_','C','O','N','S','T','R','A','I','N','T','S', 2,
					blr_sort, 1,
						blr_field, 1, 18, 'R','D','B','$','F','I','E','L','D','_','P','O','S','I','T','I','O','N',
					blr_boolean,
						blr_and,
							blr_and,
								blr_and,
									blr_and,
										blr_and,
											blr_eql,
												blr_field, 0, 16, 'R','D','B','$','S','C','H','E','M','A','_','N','A','M','E',
												blr_field, 1, 16, 'R','D','B','$','S','C','H','E','M','A','_','N','A','M','E',
											blr_eql,
												blr_field, 0, 15, 'R','D','B','$','I','N','D','E','X','_','N','A','M','E',
												blr_field, 1, 15, 'R','D','B','$','I','N','D','E','X','_','N','A','M','E',
										blr_eql,
											blr_field, 2, 16, 'R','D','B','$','S','C','H','E','M','A','_','N','A','M','E',
											blr_field, 0, 16, 'R','D','B','$','S','C','H','E','M','A','_','N','A','M','E',
									blr_eql,
										blr_field, 2, 15, 'R','D','B','$','I','N','D','E','X','_','N','A','M','E',
										blr_field, 0, 15, 'R','D','B','$','I','N','D','E','X','_','N','A','M','E',
								blr_eql,
									blr_field, 2, 18, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
									blr_parameter, 0, 0, 0,
							blr_eql,
								blr_field, 2, 20, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','T','Y','P','E',
								blr_literal, blr_text, 11, 0, 'P','R','I','M','A','R','Y',' ','K','E','Y',
				blr_send, 1,
					blr_begin,
						blr_assignment,
							blr_field, 1, 15, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E',
							blr_parameter, 1, 0, 0,
						blr_assignment,
							blr_literal, blr_short, 0, 1, 0,
							blr_parameter, 1, 1, 0,
					blr_end,
			blr_send, 1,
				blr_assignment,
					blr_literal, blr_short, 0, 0, 0,
					blr_parameter, 1, 1, 0,
		blr_end,
		blr_eoc
	};
	
	struct {
		char	isc_req_23_relation[256];
	} isc_req_23_in;
	
	struct {
		char	isc_req_23_field_name[256];
		SSHORT	isc_req_23_found;
	} isc_req_23_out;
	
	strcpy(isc_req_23_in.isc_req_23_relation, relationName.object.c_str());
	
	EXE_send(tdbb, request, 0, sizeof(isc_req_23_in), (UCHAR*)&isc_req_23_in);
	
	while (true)
	{
		EXE_receive(tdbb, request, 1, sizeof(isc_req_23_out), (UCHAR*)&isc_req_23_out);
		if (!isc_req_23_out.isc_req_23_found)
			break;
			
		FieldNode* fieldNode = FB_NEW_POOL(pool) FieldNode(pool);
		fieldNode->dsqlName = isc_req_23_out.isc_req_23_field_name;
		fields.add(fieldNode);
	}
}


dsql_prc* METD_get_procedure(jrd_tra* transaction, DsqlCompilerScratch* dsqlScratch, const QualifiedName& name)
{
/**************************************
 *
 *  M E T D _ g e t _ p r o c e d u r e
 *
 **************************************
 *
 * Functional description
 *  Look up a procedure.  If it doesn't exist, return NULL.
 *  If it does, fetch field information as well.
 *  If it is marked dropped, try to read from system tables
 *
 **************************************/
	thread_db* tdbb = JRD_get_thread_data();

	validateTransaction(transaction);

	dsql_dbb* dbb = transaction->getDsqlAttachment();

	QualifiedName qualifiedName(name);

	// Start by seeing if symbol is already defined

	dsql_prc* procedure = NULL;
	if (dbb->dbb_procedures.get(qualifiedName, procedure))
	{
		if (MET_dsql_cache_use(tdbb, SYM_procedure, qualifiedName))
			procedure->prc_flags |= PRC_dropped;
	}

	if (procedure && (procedure->prc_flags & PRC_dropped))
		procedure = nullptr;

	if (procedure)
		return procedure;

	// now see if it is in the database

	AutoCacheRequest handle1(tdbb, irq_procedure, IRQ_REQUESTS);

	// Converted FOR loop #14: Get procedure metadata from RDB$PROCEDURES
	EXE_start(tdbb, handle1, transaction);
	
	const jrd_req* const request1 = handle1;
	
	USHORT isc_req_24_3[72] = {
		blr_version5,
		blr_begin,
		blr_message, 0, 3, 0,
			blr_varying2, 3, 0, 0, 0,
			blr_varying2, 3, 0, 0, 0,
			blr_varying2, 3, 0, 0, 0,
		blr_receive, 0,
		blr_begin,
			blr_for,
				blr_rse, 1,
					blr_relation, 15, 'R','D','B','$','P','R','O','C','E','D','U','R','E','S', 0,
					blr_boolean,
						blr_and,
							blr_and,
								blr_eql,
									blr_field, 0, 16, 'R','D','B','$','S','C','H','E','M','A','_','N','A','M','E',
									blr_parameter, 0, 0, 0,
								blr_eql,
									blr_field, 0, 19, 'R','D','B','$','P','R','O','C','E','D','U','R','E','_','N','A','M','E',
									blr_parameter, 0, 1, 0,
							blr_equiv,
								blr_field, 0, 17, 'R','D','B','$','P','A','C','K','A','G','E','_','N','A','M','E',
								blr_parameter, 0, 2, 0,
				blr_send, 1,
					blr_begin,
						blr_assignment,
							blr_field, 0, 16, 'R','D','B','$','P','R','O','C','E','D','U','R','E','_','I','D',
							blr_parameter, 1, 0, 0,
						blr_assignment,
							blr_field, 0, 15, 'R','D','B','$','O','W','N','E','R','_','N','A','M','E',
							blr_parameter, 1, 1, 0,
						blr_assignment,
							blr_field, 0, 16, 'R','D','B','$','P','R','I','V','A','T','E','_','F','L','A','G',
							blr_parameter, 1, 2, 0,
						blr_assignment,
							blr_literal, blr_short, 0, 1, 0,
							blr_parameter, 1, 3, 0,
					blr_end,
			blr_send, 1,
				blr_assignment,
					blr_literal, blr_short, 0, 0, 0,
					blr_parameter, 1, 3, 0,
		blr_end,
		blr_eoc
	};
	
	struct {
		char	isc_req_24_schema[256];
		char	isc_req_24_procedure[256];
		char	isc_req_24_package[256];
	} isc_req_24_in;
	
	struct {
		SSHORT	isc_req_24_procedure_id;
		char	isc_req_24_owner[256];
		SSHORT	isc_req_24_private_flag;
		SSHORT	isc_req_24_found;
	} isc_req_24_out;
	
	strcpy(isc_req_24_in.isc_req_24_schema, qualifiedName.schema.c_str());
	strcpy(isc_req_24_in.isc_req_24_procedure, qualifiedName.object.c_str());
	strcpy(isc_req_24_in.isc_req_24_package, qualifiedName.package.hasData() ? qualifiedName.package.c_str() : "");
	
	EXE_send(tdbb, request1, 0, sizeof(isc_req_24_in), (UCHAR*)&isc_req_24_in);
	
	while (true)
	{
		EXE_receive(tdbb, request1, 1, sizeof(isc_req_24_out), (UCHAR*)&isc_req_24_out);
		if (!isc_req_24_out.isc_req_24_found)
			break;
			
		procedure = FB_NEW_POOL(dbb->dbb_pool) dsql_prc(dbb->dbb_pool);
		procedure->prc_id = isc_req_24_out.isc_req_24_procedure_id;
		procedure->prc_name = qualifiedName;
		procedure->prc_owner = isc_req_24_out.isc_req_24_owner;
		procedure->prc_private = isc_req_24_out.isc_req_24_private_flag != 0;
	}

	if (!procedure)
		return nullptr;

	// Lookup parameter stuff

	for (int type = 0; type < 2; type++)
	{
		dsql_fld** const ptr = type ? &procedure->prc_outputs : &procedure->prc_inputs;

		SSHORT count = 0, defaults = 0;

		AutoCacheRequest handle2(tdbb, irq_parameters, IRQ_REQUESTS);

		// Converted FOR loop #15: Get procedure parameters from RDB$PROCEDURE_PARAMETERS/RDB$FIELDS
		EXE_start(tdbb, handle2, transaction);
		
		const jrd_req* const request2 = handle2;
		
		USHORT isc_req_25_4[160] = {
			blr_version5,
			blr_begin,
			blr_message, 0, 4, 0,
				blr_varying2, 3, 0, 0, 0,
				blr_varying2, 3, 0, 0, 0,
				blr_short, 0,
				blr_varying2, 3, 0, 0, 0,
			blr_receive, 0,
			blr_begin,
				blr_for,
					blr_rse, 1,
						blr_cross,
							blr_relation, 25, 'R','D','B','$','P','R','O','C','E','D','U','R','E','_','P','A','R','A','M','E','T','E','R','S', 0,
							blr_relation, 10, 'R','D','B','$','F','I','E','L','D','S', 1,
						blr_sort, 1,
							blr_descending,
								blr_field, 0, 20, 'R','D','B','$','P','A','R','A','M','E','T','E','R','_','N','U','M','B','E','R',
						blr_boolean,
							blr_and,
								blr_and,
									blr_and,
										blr_and,
											blr_and,
												blr_and,
													blr_eql,
														blr_field, 0, 16, 'R','D','B','$','S','C','H','E','M','A','_','N','A','M','E',
														blr_parameter, 0, 0, 0,
													blr_eql,
														blr_field, 0, 19, 'R','D','B','$','P','R','O','C','E','D','U','R','E','_','N','A','M','E',
														blr_parameter, 0, 1, 0,
												blr_eql,
													blr_field, 0, 18, 'R','D','B','$','P','A','R','A','M','E','T','E','R','_','T','Y','P','E',
													blr_parameter, 0, 2, 0,
											blr_equiv,
												blr_field, 0, 17, 'R','D','B','$','P','A','C','K','A','G','E','_','N','A','M','E',
												blr_parameter, 0, 3, 0,
										blr_eql,
											blr_field, 1, 16, 'R','D','B','$','S','C','H','E','M','A','_','N','A','M','E',
											blr_field, 0, 29, 'R','D','B','$','F','I','E','L','D','_','S','O','U','R','C','E','_','S','C','H','E','M','A','_','N','A','M','E',
									blr_eql,
										blr_field, 1, 15, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E',
										blr_field, 0, 15, 'R','D','B','$','F','I','E','L','D','_','S','O','U','R','C','E',
								blr_not,
									blr_missing,
										blr_field, 0, 20, 'R','D','B','$','P','A','R','A','M','E','T','E','R','_','N','U','M','B','E','R',
					blr_send, 1,
						blr_begin,
							blr_assignment,
								blr_field, 0, 17, 'R','D','B','$','C','O','L','L','A','T','I','O','N','_','I','D',
								blr_parameter, 1, 0, 0,
							blr_assignment,
								blr_field, 0, 17, 'R','D','B','$','D','E','F','A','U','L','T','_','V','A','L','U','E',
								blr_parameter, 1, 1, 0,
							blr_assignment,
								blr_field, 0, 13, 'R','D','B','$','N','U','L','L','_','F','L','A','G',
								blr_parameter, 1, 2, 0,
							blr_assignment,
								blr_field, 0, 24, 'R','D','B','$','P','A','R','A','M','E','T','E','R','_','M','E','C','H','A','N','I','S','M',
								blr_parameter, 1, 3, 0,
							blr_assignment,
								blr_field, 0, 19, 'R','D','B','$','P','A','R','A','M','E','T','E','R','_','N','A','M','E',
								blr_parameter, 1, 4, 0,
							blr_assignment,
								blr_field, 0, 29, 'R','D','B','$','F','I','E','L','D','_','S','O','U','R','C','E','_','S','C','H','E','M','A','_','N','A','M','E',
								blr_parameter, 1, 5, 0,
							blr_assignment,
								blr_field, 0, 15, 'R','D','B','$','F','I','E','L','D','_','S','O','U','R','C','E',
								blr_parameter, 1, 6, 0,
							blr_assignment,
								blr_field, 0, 20, 'R','D','B','$','P','A','R','A','M','E','T','E','R','_','N','U','M','B','E','R',
								blr_parameter, 1, 7, 0,
							blr_assignment,
								blr_field, 1, 17, 'R','D','B','$','F','I','E','L','D','_','L','E','N','G','T','H',
								blr_parameter, 1, 8, 0,
							blr_assignment,
								blr_field, 1, 16, 'R','D','B','$','F','I','E','L','D','_','S','C','A','L','E',
								blr_parameter, 1, 9, 0,
							blr_assignment,
								blr_field, 1, 19, 'R','D','B','$','F','I','E','L','D','_','S','U','B','_','T','Y','P','E',
								blr_parameter, 1, 10, 0,
							blr_assignment,
								blr_field, 1, 15, 'R','D','B','$','F','I','E','L','D','_','T','Y','P','E',
								blr_parameter, 1, 11, 0,
							blr_assignment,
								blr_field, 1, 20, 'R','D','B','$','C','H','A','R','A','C','T','E','R','_','S','E','T','_','I','D',
								blr_parameter, 1, 12, 0,
							blr_assignment,
								blr_field, 1, 17, 'R','D','B','$','C','O','L','L','A','T','I','O','N','_','I','D',
								blr_parameter, 1, 13, 0,
							blr_assignment,
								blr_field, 1, 18, 'R','D','B','$','S','E','G','M','E','N','T','_','L','E','N','G','T','H',
								blr_parameter, 1, 14, 0,
							blr_assignment,
								blr_field, 0, 15, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E',
								blr_parameter, 1, 15, 0,
							blr_assignment,
								blr_field, 0, 18, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
								blr_parameter, 1, 16, 0,
							blr_assignment,
								blr_field, 0, 25, 'R','D','B','$','R','E','L','A','T','I','O','N','_','S','C','H','E','M','A','_','N','A','M','E',
								blr_parameter, 1, 17, 0,
							blr_assignment,
								blr_field, 1, 15, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E',
								blr_parameter, 1, 18, 0,
							blr_assignment,
								blr_field, 1, 17, 'R','D','B','$','D','E','F','A','U','L','T','_','V','A','L','U','E',
								blr_parameter, 1, 19, 0,
							blr_assignment,
								blr_literal, blr_short, 0, 1, 0,
								blr_parameter, 1, 20, 0,
						blr_end,
				blr_send, 1,
					blr_assignment,
						blr_literal, blr_short, 0, 0, 0,
						blr_parameter, 1, 20, 0,
			blr_end,
			blr_eoc
		};
		
		struct {
			char	isc_req_25_schema[256];
			char	isc_req_25_procedure[256];
			SSHORT	isc_req_25_param_type;
			char	isc_req_25_package[256];
		} isc_req_25_in;
		
		struct {
			SSHORT	isc_req_25_pr_collation_id;
			SLONG	isc_req_25_pr_default_value;
			SSHORT	isc_req_25_pr_null_flag;
			SSHORT	isc_req_25_param_mechanism;
			char	isc_req_25_param_name[256];
			char	isc_req_25_field_source_schema[256];
			char	isc_req_25_field_source[256];
			USHORT	isc_req_25_param_number;
			SSHORT	isc_req_25_field_length;
			SSHORT	isc_req_25_field_scale;
			SSHORT	isc_req_25_field_sub_type;
			SSHORT	isc_req_25_field_type;
			SSHORT	isc_req_25_charset_id;
			SSHORT	isc_req_25_fld_collation_id;
			SSHORT	isc_req_25_segment_length;
			char	isc_req_25_field_name[256];
			char	isc_req_25_relation_name[256];
			char	isc_req_25_relation_schema[256];
			char	isc_req_25_fld_field_name[256];
			SLONG	isc_req_25_fld_default_value;
			SSHORT	isc_req_25_found;
		} isc_req_25_out;
		
		strcpy(isc_req_25_in.isc_req_25_schema, qualifiedName.schema.c_str());
		strcpy(isc_req_25_in.isc_req_25_procedure, qualifiedName.object.c_str());
		isc_req_25_in.isc_req_25_param_type = type;
		strcpy(isc_req_25_in.isc_req_25_package, qualifiedName.package.hasData() ? qualifiedName.package.c_str() : "");
		
		EXE_send(tdbb, request2, 0, sizeof(isc_req_25_in), (UCHAR*)&isc_req_25_in);
		
		while (true)
		{
			EXE_receive(tdbb, request2, 1, sizeof(isc_req_25_out), (UCHAR*)&isc_req_25_out);
			if (!isc_req_25_out.isc_req_25_found)
				break;
				
			const SSHORT pr_collation_id_null = (isc_req_25_out.isc_req_25_pr_collation_id == 0);
			const SSHORT pr_collation_id = isc_req_25_out.isc_req_25_pr_collation_id;

			const SSHORT pr_default_value_null = (isc_req_25_out.isc_req_25_pr_default_value == 0);

			const SSHORT pr_null_flag_null = (isc_req_25_out.isc_req_25_pr_null_flag == 0);
			const SSHORT pr_null_flag = isc_req_25_out.isc_req_25_pr_null_flag;

			const bool pr_type_of =
				(isc_req_25_out.isc_req_25_param_mechanism == prm_mech_type_of);

			count++;
			// allocate the field block

			fb_utils::exact_name(isc_req_25_out.isc_req_25_param_name);
			fb_utils::exact_name(isc_req_25_out.isc_req_25_field_source);

			dsql_fld* parameter = FB_NEW_POOL(dbb->dbb_pool) dsql_fld(dbb->dbb_pool);
			parameter->fld_next = *ptr;
			*ptr = parameter;

			// get parameter information

			parameter->fld_name = isc_req_25_out.isc_req_25_param_name;
			parameter->fieldSource = QualifiedName(isc_req_25_out.isc_req_25_field_source, isc_req_25_out.isc_req_25_field_source_schema);

			parameter->fld_id = isc_req_25_out.isc_req_25_param_number;
			parameter->length = isc_req_25_out.isc_req_25_field_length;
			parameter->scale = isc_req_25_out.isc_req_25_field_scale;
			parameter->subType = isc_req_25_out.isc_req_25_field_sub_type;
			parameter->fld_procedure = procedure;

			if (isc_req_25_out.isc_req_25_charset_id != 0)
				parameter->charSetId = isc_req_25_out.isc_req_25_charset_id;

			if (!pr_collation_id_null)
				parameter->collationId = pr_collation_id;
			else if (isc_req_25_out.isc_req_25_fld_collation_id != 0)
				parameter->collationId = isc_req_25_out.isc_req_25_fld_collation_id;

			convert_dtype(parameter, isc_req_25_out.isc_req_25_field_type);

			if (!pr_null_flag_null)
			{
				if (!pr_null_flag)
					parameter->flags |= FLD_nullable;
			}
			else if (isc_req_25_out.isc_req_25_pr_null_flag == 0 || pr_type_of)
				parameter->flags |= FLD_nullable;

			if (isc_req_25_out.isc_req_25_field_type == blr_blob)
				parameter->segLength = isc_req_25_out.isc_req_25_segment_length;

			if (isc_req_25_out.isc_req_25_field_name[0] != '\0')
				parameter->typeOfName = QualifiedName(isc_req_25_out.isc_req_25_field_name);

			if (isc_req_25_out.isc_req_25_relation_name[0] != '\0')
				parameter->typeOfTable = QualifiedName(isc_req_25_out.isc_req_25_relation_name, isc_req_25_out.isc_req_25_relation_schema);

			if (parameter->typeOfTable.object.hasData())
			{
				if (isSystemRelation(tdbb, transaction, parameter->typeOfTable))
					parameter->flags |= FLD_system;
			}
			else if (parameter->typeOfName.object.hasData())
			{
				if (isSystemDomain(tdbb, transaction, parameter->typeOfName))
					parameter->flags |= FLD_system;
			}
			else if (parameter->fieldSource.object.hasData())
			{
				if (isSystemDomain(tdbb, transaction, parameter->fieldSource))
					parameter->flags |= FLD_system;
			}

			if (type == 0 &&
				(!pr_default_value_null ||
					(fb_utils::implicit_domain(isc_req_25_out.isc_req_25_fld_field_name) && isc_req_25_out.isc_req_25_fld_default_value != 0)))
			{
				defaults++;
			}
		}

		if (type)
			procedure->prc_out_count = count;
		else
		{
			procedure->prc_in_count = count;
			procedure->prc_def_count = defaults;
		}
	}

	dbb->dbb_procedures.put(procedure->prc_name, procedure);

	MET_dsql_cache_use(tdbb, SYM_procedure, procedure->prc_name);

	return procedure;
}


dsql_rel* METD_get_relation(jrd_tra* transaction, DsqlCompilerScratch* dsqlScratch,
	const QualifiedName& name)
{
/**************************************
 *
 *  M E T D _ g e t _ r e l a t i o n
 *
 **************************************
 *
 * Functional description
 *  Look up a relation.  If it doesn't exist, return NULL.
 *  If it does, fetch field information as well.
 *
 **************************************/
	thread_db* tdbb = JRD_get_thread_data();

	validateTransaction(transaction);

	dsql_dbb* dbb = transaction->getDsqlAttachment();

	// See if the relation is the one currently being defined in this statement

	dsql_rel* temp = dsqlScratch->relation;
	if (temp != NULL && temp->rel_name == name)
		return temp;

	// Start by seeing if symbol is already defined

	if (dbb->dbb_relations.get(name, temp) && !(temp->rel_flags & REL_dropped))
	{
		if (MET_dsql_cache_use(tdbb, SYM_relation, name))
			temp->rel_flags |= REL_dropped;
		else
			return temp;
	}

	// If the relation id or any of the field ids have not yet been assigned,
	// and this is a type of statement which does not use ids, prepare a
	// temporary relation block to provide information without caching it

	bool permanent = true;

	AutoCacheRequest handle1(tdbb, irq_rel_ids, IRQ_REQUESTS);

	// Converted FOR loop #16: Check for relation ID assignment from RDB$RELATIONS/RDB$RELATION_FIELDS
	EXE_start(tdbb, handle1, transaction);
	
	const jrd_req* const request1 = handle1;
	
	USHORT isc_req_26_2[68] = {
		blr_version5,
		blr_begin,
		blr_message, 0, 2, 0,
			blr_varying2, 3, 0, 0, 0,
			blr_varying2, 3, 0, 0, 0,
		blr_receive, 0,
		blr_begin,
			blr_for,
				blr_rse, 1,
					blr_cross,
						blr_relation, 13, 'R','D','B','$','R','E','L','A','T','I','O','N','S', 0,
						blr_relation, 21, 'R','D','B','$','R','E','L','A','T','I','O','N','_','F','I','E','L','D','S', 1,
					blr_boolean,
						blr_and,
							blr_and,
								blr_and,
									blr_eql,
										blr_field, 0, 16, 'R','D','B','$','S','C','H','E','M','A','_','N','A','M','E',
										blr_parameter, 0, 0, 0,
									blr_eql,
										blr_field, 0, 18, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
										blr_parameter, 0, 1, 0,
								blr_eql,
									blr_field, 1, 16, 'R','D','B','$','S','C','H','E','M','A','_','N','A','M','E',
									blr_field, 0, 16, 'R','D','B','$','S','C','H','E','M','A','_','N','A','M','E',
							blr_eql,
								blr_field, 1, 18, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
								blr_field, 0, 18, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
					blr_first,
						blr_literal, blr_long, 0, 1, 0, 0, 0,
					blr_boolean,
						blr_or,
							blr_missing,
								blr_field, 0, 15, 'R','D','B','$','R','E','L','A','T','I','O','N','_','I','D',
							blr_missing,
								blr_field, 1, 13, 'R','D','B','$','F','I','E','L','D','_','I','D',
				blr_send, 1,
					blr_begin,
						blr_assignment,
							blr_literal, blr_short, 0, 1, 0,
							blr_parameter, 1, 0, 0,
					blr_end,
			blr_send, 1,
				blr_assignment,
					blr_literal, blr_short, 0, 0, 0,
					blr_parameter, 1, 0, 0,
		blr_end,
		blr_eoc
	};
	
	struct {
		char	isc_req_26_schema[256];
		char	isc_req_26_relation[256];
	} isc_req_26_in;
	
	struct {
		SSHORT	isc_req_26_found;
	} isc_req_26_out;
	
	strcpy(isc_req_26_in.isc_req_26_schema, name.schema.c_str());
	strcpy(isc_req_26_in.isc_req_26_relation, name.object.c_str());
	
	EXE_send(tdbb, request1, 0, sizeof(isc_req_26_in), (UCHAR*)&isc_req_26_in);
	
	while (true)
	{
		EXE_receive(tdbb, request1, 1, sizeof(isc_req_26_out), (UCHAR*)&isc_req_26_out);
		if (!isc_req_26_out.isc_req_26_found)
			break;
			
		permanent = false;
	}

	// Now see if it is in the database

	MemoryPool& pool = permanent ? dbb->dbb_pool : *tdbb->getDefaultPool();

	dsql_rel* relation = NULL;

	AutoCacheRequest handle2(tdbb, irq_relation, IRQ_REQUESTS);

	// Converted FOR loop #17: Get relation metadata from RDB$RELATIONS
	EXE_start(tdbb, handle2, transaction);
	
	const jrd_req* const request2 = handle2;
	
	USHORT isc_req_27_2[58] = {
		blr_version5,
		blr_begin,
		blr_message, 0, 2, 0,
			blr_varying2, 3, 0, 0, 0,
			blr_varying2, 3, 0, 0, 0,
		blr_receive, 0,
		blr_begin,
			blr_for,
				blr_rse, 1,
					blr_relation, 13, 'R','D','B','$','R','E','L','A','T','I','O','N','S', 0,
					blr_boolean,
						blr_and,
							blr_eql,
								blr_field, 0, 16, 'R','D','B','$','S','C','H','E','M','A','_','N','A','M','E',
								blr_parameter, 0, 0, 0,
							blr_eql,
								blr_field, 0, 18, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
								blr_parameter, 0, 1, 0,
				blr_send, 1,
					blr_begin,
						blr_assignment,
							blr_field, 0, 15, 'R','D','B','$','R','E','L','A','T','I','O','N','_','I','D',
							blr_parameter, 1, 0, 0,
						blr_assignment,
							blr_field, 0, 15, 'R','D','B','$','O','W','N','E','R','_','N','A','M','E',
							blr_parameter, 1, 1, 0,
						blr_assignment,
							blr_field, 0, 16, 'R','D','B','$','D','B','K','E','Y','_','L','E','N','G','T','H',
							blr_parameter, 1, 2, 0,
						blr_assignment,
							blr_field, 0, 12, 'R','D','B','$','V','I','E','W','_','B','L','R',
							blr_parameter, 1, 3, 0,
						blr_assignment,
							blr_field, 0, 17, 'R','D','B','$','E','X','T','E','R','N','A','L','_','F','I','L','E',
							blr_parameter, 1, 4, 0,
						blr_assignment,
							blr_literal, blr_short, 0, 1, 0,
							blr_parameter, 1, 5, 0,
					blr_end,
			blr_send, 1,
				blr_assignment,
					blr_literal, blr_short, 0, 0, 0,
					blr_parameter, 1, 5, 0,
		blr_end,
		blr_eoc
	};
	
	struct {
		char	isc_req_27_schema[256];
		char	isc_req_27_relation[256];
	} isc_req_27_in;
	
	struct {
		SSHORT	isc_req_27_relation_id;
		char	isc_req_27_owner[256];
		SSHORT	isc_req_27_dbkey_length;
		SLONG	isc_req_27_view_blr;
		SLONG	isc_req_27_external_file;
		SSHORT	isc_req_27_found;
	} isc_req_27_out;
	
	strcpy(isc_req_27_in.isc_req_27_schema, name.schema.c_str());
	strcpy(isc_req_27_in.isc_req_27_relation, name.object.c_str());
	
	EXE_send(tdbb, request2, 0, sizeof(isc_req_27_in), (UCHAR*)&isc_req_27_in);
	
	while (true)
	{
		EXE_receive(tdbb, request2, 1, sizeof(isc_req_27_out), (UCHAR*)&isc_req_27_out);
		if (!isc_req_27_out.isc_req_27_found)
			break;
			
		// Allocate from default or permanent pool as appropriate

		if (isc_req_27_out.isc_req_27_relation_id != 0)
		{
			relation = FB_NEW_POOL(pool) dsql_rel(pool);
			relation->rel_id = isc_req_27_out.isc_req_27_relation_id;
		}
		else if (!DDL_ids(dsqlScratch))
			relation = FB_NEW_POOL(pool) dsql_rel(pool);

		// fill out the relation information

		if (relation)
		{
			relation->rel_name = name;
			relation->rel_owner = isc_req_27_out.isc_req_27_owner;
			if (!(relation->rel_dbkey_length = isc_req_27_out.isc_req_27_dbkey_length))
				relation->rel_dbkey_length = 8;
			// CVC: let's see if this is a table or a view.
			if (isc_req_27_out.isc_req_27_view_blr != 0)
				relation->rel_flags |= REL_view;
			if (isc_req_27_out.isc_req_27_external_file != 0)
				relation->rel_flags |= REL_external;
		}
	}

	if (!relation)
		return NULL;

	// Lookup field stuff

	dsql_fld** ptr = &relation->rel_fields;

	AutoCacheRequest handle3(tdbb, irq_fields, IRQ_REQUESTS);

	// Converted FOR loop #18: Get relation fields from RDB$FIELDS/RDB$RELATION_FIELDS
	EXE_start(tdbb, handle3, transaction);
	
	const jrd_req* const request3 = handle3;
	
	USHORT isc_req_28_2[148] = {
		blr_version5,
		blr_begin,
		blr_message, 0, 2, 0,
			blr_varying2, 3, 0, 0, 0,
			blr_varying2, 3, 0, 0, 0,
		blr_receive, 0,
		blr_begin,
			blr_for,
				blr_rse, 1,
					blr_cross,
						blr_relation, 10, 'R','D','B','$','F','I','E','L','D','S', 0,
						blr_relation, 21, 'R','D','B','$','R','E','L','A','T','I','O','N','_','F','I','E','L','D','S', 1,
					blr_sort, 1,
						blr_field, 1, 18, 'R','D','B','$','F','I','E','L','D','_','P','O','S','I','T','I','O','N',
					blr_boolean,
						blr_and,
							blr_and,
								blr_and,
									blr_and,
										blr_eql,
											blr_field, 1, 16, 'R','D','B','$','S','C','H','E','M','A','_','N','A','M','E',
											blr_parameter, 0, 0, 0,
										blr_eql,
											blr_field, 1, 18, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
											blr_parameter, 0, 1, 0,
									blr_equiv,
										blr_field, 0, 16, 'R','D','B','$','S','C','H','E','M','A','_','N','A','M','E',
										blr_field, 1, 29, 'R','D','B','$','F','I','E','L','D','_','S','O','U','R','C','E','_','S','C','H','E','M','A','_','N','A','M','E',
								blr_eql,
									blr_field, 0, 15, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E',
									blr_field, 1, 15, 'R','D','B','$','F','I','E','L','D','_','S','O','U','R','C','E',
							blr_not,
								blr_missing,
									blr_field, 1, 18, 'R','D','B','$','F','I','E','L','D','_','P','O','S','I','T','I','O','N',
				blr_send, 1,
					blr_begin,
						blr_assignment,
							blr_field, 1, 13, 'R','D','B','$','F','I','E','L','D','_','I','D',
							blr_parameter, 1, 0, 0,
						blr_assignment,
							blr_field, 1, 15, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E',
							blr_parameter, 1, 1, 0,
						blr_assignment,
							blr_field, 1, 15, 'R','D','B','$','F','I','E','L','D','_','S','O','U','R','C','E',
							blr_parameter, 1, 2, 0,
						blr_assignment,
							blr_field, 1, 29, 'R','D','B','$','F','I','E','L','D','_','S','O','U','R','C','E','_','S','C','H','E','M','A','_','N','A','M','E',
							blr_parameter, 1, 3, 0,
						blr_assignment,
							blr_field, 0, 17, 'R','D','B','$','F','I','E','L','D','_','L','E','N','G','T','H',
							blr_parameter, 1, 4, 0,
						blr_assignment,
							blr_field, 0, 16, 'R','D','B','$','F','I','E','L','D','_','S','C','A','L','E',
							blr_parameter, 1, 5, 0,
						blr_assignment,
							blr_field, 0, 19, 'R','D','B','$','F','I','E','L','D','_','S','U','B','_','T','Y','P','E',
							blr_parameter, 1, 6, 0,
						blr_assignment,
							blr_field, 0, 16, 'R','D','B','$','C','O','M','P','U','T','E','D','_','B','L','R',
							blr_parameter, 1, 7, 0,
						blr_assignment,
							blr_field, 0, 15, 'R','D','B','$','F','I','E','L','D','_','T','Y','P','E',
							blr_parameter, 1, 8, 0,
						blr_assignment,
							blr_field, 0, 18, 'R','D','B','$','S','E','G','M','E','N','T','_','L','E','N','G','T','H',
							blr_parameter, 1, 9, 0,
						blr_assignment,
							blr_field, 0, 15, 'R','D','B','$','D','I','M','E','N','S','I','O','N','S',
							blr_parameter, 1, 10, 0,
						blr_assignment,
							blr_field, 0, 20, 'R','D','B','$','C','H','A','R','A','C','T','E','R','_','S','E','T','_','I','D',
							blr_parameter, 1, 11, 0,
						blr_assignment,
							blr_field, 1, 17, 'R','D','B','$','C','O','L','L','A','T','I','O','N','_','I','D',
							blr_parameter, 1, 12, 0,
						blr_assignment,
							blr_field, 0, 17, 'R','D','B','$','C','O','L','L','A','T','I','O','N','_','I','D',
							blr_parameter, 1, 13, 0,
						blr_assignment,
							blr_field, 1, 13, 'R','D','B','$','N','U','L','L','_','F','L','A','G',
							blr_parameter, 1, 14, 0,
						blr_assignment,
							blr_field, 0, 13, 'R','D','B','$','N','U','L','L','_','F','L','A','G',
							blr_parameter, 1, 15, 0,
						blr_assignment,
							blr_field, 1, 16, 'R','D','B','$','S','Y','S','T','E','M','_','F','L','A','G',
							blr_parameter, 1, 16, 0,
						blr_assignment,
							blr_field, 0, 16, 'R','D','B','$','S','Y','S','T','E','M','_','F','L','A','G',
							blr_parameter, 1, 17, 0,
						blr_assignment,
							blr_literal, blr_short, 0, 1, 0,
							blr_parameter, 1, 18, 0,
					blr_end,
			blr_send, 1,
				blr_assignment,
					blr_literal, blr_short, 0, 0, 0,
					blr_parameter, 1, 18, 0,
		blr_end,
		blr_eoc
	};
	
	struct {
		char	isc_req_28_schema[256];
		char	isc_req_28_relation[256];
	} isc_req_28_in;
	
	struct {
		SSHORT	isc_req_28_field_id;
		char	isc_req_28_field_name[256];
		char	isc_req_28_field_source[256];
		char	isc_req_28_field_source_schema[256];
		SSHORT	isc_req_28_field_length;
		SSHORT	isc_req_28_field_scale;
		SSHORT	isc_req_28_field_sub_type;
		SLONG	isc_req_28_computed_blr;
		SSHORT	isc_req_28_field_type;
		SSHORT	isc_req_28_segment_length;
		SSHORT	isc_req_28_dimensions;
		SSHORT	isc_req_28_charset_id;
		SSHORT	isc_req_28_rfr_collation_id;
		SSHORT	isc_req_28_fld_collation_id;
		SSHORT	isc_req_28_rfr_null_flag;
		SSHORT	isc_req_28_fld_null_flag;
		SSHORT	isc_req_28_rfr_system_flag;
		SSHORT	isc_req_28_fld_system_flag;
		SSHORT	isc_req_28_found;
	} isc_req_28_out;
	
	strcpy(isc_req_28_in.isc_req_28_schema, name.schema.c_str());
	strcpy(isc_req_28_in.isc_req_28_relation, name.object.c_str());
	
	EXE_send(tdbb, request3, 0, sizeof(isc_req_28_in), (UCHAR*)&isc_req_28_in);
	
	while (true)
	{
		EXE_receive(tdbb, request3, 1, sizeof(isc_req_28_out), (UCHAR*)&isc_req_28_out);
		if (!isc_req_28_out.isc_req_28_found)
			break;
			
		// Allocate the field block
		// Allocate from default or permanent pool as appropriate

		dsql_fld* field = NULL;

		if (isc_req_28_out.isc_req_28_field_id != 0)
		{
			field = FB_NEW_POOL(pool) dsql_fld(pool);
			field->fld_id = isc_req_28_out.isc_req_28_field_id;
		}
		else if (!DDL_ids(dsqlScratch))
			field = FB_NEW_POOL(pool) dsql_fld(pool);

		if (field)
		{
			*ptr = field;
			ptr = &field->fld_next;

			// get field information

			field->fld_name = isc_req_28_out.isc_req_28_field_name;
			field->fieldSource = QualifiedName(isc_req_28_out.isc_req_28_field_source, isc_req_28_out.isc_req_28_field_source_schema);
			field->length = isc_req_28_out.isc_req_28_field_length;
			field->scale = isc_req_28_out.isc_req_28_field_scale;
			field->subType = isc_req_28_out.isc_req_28_field_sub_type;
			field->fld_relation = relation;

			if (isc_req_28_out.isc_req_28_computed_blr != 0)
				field->flags |= FLD_computed;

			convert_dtype(field, isc_req_28_out.isc_req_28_field_type);

			if (isc_req_28_out.isc_req_28_field_type == blr_blob) {
				field->segLength = isc_req_28_out.isc_req_28_segment_length;
			}

			if (isc_req_28_out.isc_req_28_dimensions != 0 && isc_req_28_out.isc_req_28_dimensions != 0)
			{
				field->elementDtype = field->dtype;
				field->elementLength = field->length;
				field->dtype = dtype_array;
				field->length = sizeof(ISC_QUAD);
				field->dimensions = isc_req_28_out.isc_req_28_dimensions;
			}

			if (isc_req_28_out.isc_req_28_charset_id != 0)
				field->charSetId = isc_req_28_out.isc_req_28_charset_id;

			if (isc_req_28_out.isc_req_28_rfr_collation_id != 0)
				field->collationId = isc_req_28_out.isc_req_28_rfr_collation_id;
			else if (isc_req_28_out.isc_req_28_fld_collation_id != 0)
				field->collationId = isc_req_28_out.isc_req_28_fld_collation_id;

			if (!(isc_req_28_out.isc_req_28_rfr_null_flag || isc_req_28_out.isc_req_28_fld_null_flag) || (relation->rel_flags & REL_view))
			{
				field->flags |= FLD_nullable;
			}

			if (isc_req_28_out.isc_req_28_rfr_system_flag == 1 || isc_req_28_out.isc_req_28_fld_system_flag == 1)
				field->flags |= FLD_system;
		}
	}

	if (dbb->dbb_relations.get(name, temp) && !(temp->rel_flags & REL_dropped))
	{
		free_relation(relation);
		return temp;
	}

	// Add relation to the list

	if (permanent)
	{
		dbb->dbb_relations.put(relation->rel_name, relation);
		MET_dsql_cache_use(tdbb, SYM_relation, relation->rel_name);
	}
	else
		relation->rel_flags |= REL_new_relation;

	return relation;
}


dsql_rel* METD_get_view_base(jrd_tra* transaction, DsqlCompilerScratch* dsqlScratch,
	const QualifiedName& viewName, MetaNamePairMap& fields)
{
/**************************************
 *
 *  M E T D _ g e t _ v i e w _ b a s e
 *
 **************************************
 *
 * Functional description
 *  Return the base table of a view or NULL if there
 *  is more than one.
 *  If there is only one base, put in fields a map of
 *  top view field name / bottom base field name.
 *  Ignores the field in the case of a base field name
 *  appearing more than one time in a level.
 *
 **************************************/
	thread_db* tdbb = JRD_get_thread_data();

	validateTransaction(transaction);

	auto nextViewName = viewName;
	dsql_rel* relation = nullptr;
	bool first = true;
	bool cont = true;
	MetaNamePairMap previousAux;

	fields.clear();

	while (cont)
	{
		AutoCacheRequest handle1(tdbb, irq_view_base, IRQ_REQUESTS);

		// Converted FOR loop #19: Get view base tables from RDB$VIEW_RELATIONS
		EXE_start(tdbb, handle1, transaction);
		
		const jrd_req* const request1 = handle1;
		
		USHORT isc_req_29_2[58] = {
			blr_version5,
			blr_begin,
			blr_message, 0, 2, 0,
				blr_varying2, 3, 0, 0, 0,
				blr_varying2, 3, 0, 0, 0,
			blr_receive, 0,
			blr_begin,
				blr_for,
					blr_rse, 1,
						blr_relation, 18, 'R','D','B','$','V','I','E','W','_','R','E','L','A','T','I','O','N','S', 0,
						blr_boolean,
							blr_and,
								blr_eql,
									blr_field, 0, 16, 'R','D','B','$','S','C','H','E','M','A','_','N','A','M','E',
									blr_parameter, 0, 0, 0,
								blr_eql,
									blr_field, 0, 14, 'R','D','B','$','V','I','E','W','_','N','A','M','E',
									blr_parameter, 0, 1, 0,
					blr_send, 1,
						blr_begin,
							blr_assignment,
								blr_field, 0, 17, 'R','D','B','$','V','I','E','W','_','C','O','N','T','E','X','T',
								blr_parameter, 1, 0, 0,
							blr_assignment,
								blr_field, 0, 16, 'R','D','B','$','C','O','N','T','E','X','T','_','T','Y','P','E',
								blr_parameter, 1, 1, 0,
							blr_assignment,
								blr_field, 0, 18, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
								blr_parameter, 1, 2, 0,
							blr_assignment,
								blr_field, 0, 25, 'R','D','B','$','R','E','L','A','T','I','O','N','_','S','C','H','E','M','A','_','N','A','M','E',
								blr_parameter, 1, 3, 0,
							blr_assignment,
								blr_literal, blr_short, 0, 1, 0,
								blr_parameter, 1, 4, 0,
						blr_end,
				blr_send, 1,
					blr_assignment,
						blr_literal, blr_short, 0, 0, 0,
						blr_parameter, 1, 4, 0,
			blr_end,
			blr_eoc
		};
		
		struct {
			char	isc_req_29_schema[256];
			char	isc_req_29_view_name[256];
		} isc_req_29_in;
		
		struct {
			SSHORT	isc_req_29_view_context;
			SSHORT	isc_req_29_context_type;
			char	isc_req_29_relation[256];
			char	isc_req_29_relation_schema[256];
			SSHORT	isc_req_29_found;
		} isc_req_29_out;
		
		strcpy(isc_req_29_in.isc_req_29_schema, nextViewName.schema.c_str());
		strcpy(isc_req_29_in.isc_req_29_view_name, nextViewName.object.c_str());
		
		EXE_send(tdbb, request1, 0, sizeof(isc_req_29_in), (UCHAR*)&isc_req_29_in);
		
		while (true)
		{
			EXE_receive(tdbb, request1, 1, sizeof(isc_req_29_out), (UCHAR*)&isc_req_29_out);
			if (!isc_req_29_out.isc_req_29_found)
				break;
				
			// return NULL if there is more than one context
			if (isc_req_29_out.isc_req_29_view_context != 1 || isc_req_29_out.isc_req_29_context_type == VCT_PROCEDURE)
			{
				relation = NULL;
				cont = false;
				break;
			}

			nextViewName = QualifiedName(isc_req_29_out.isc_req_29_relation, isc_req_29_out.isc_req_29_relation_schema);
			relation = METD_get_relation(transaction, dsqlScratch, nextViewName);

			Array<MetaName> ambiguities;
			MetaNamePairMap currentAux;

			if (!relation)
			{
				cont = false;
				break;
			}

			AutoCacheRequest handle2(tdbb, irq_view_base_flds, IRQ_REQUESTS);

			// Converted FOR loop #20: Get view relation fields from RDB$RELATION_FIELDS
			EXE_start(tdbb, handle2, transaction);
			
			const jrd_req* const request2 = handle2;
			
			USHORT isc_req_30_2[58] = {
				blr_version5,
				blr_begin,
				blr_message, 0, 2, 0,
					blr_varying2, 3, 0, 0, 0,
					blr_varying2, 3, 0, 0, 0,
				blr_receive, 0,
				blr_begin,
					blr_for,
						blr_rse, 1,
							blr_relation, 21, 'R','D','B','$','R','E','L','A','T','I','O','N','_','F','I','E','L','D','S', 0,
							blr_boolean,
								blr_and,
									blr_eql,
										blr_field, 0, 16, 'R','D','B','$','S','C','H','E','M','A','_','N','A','M','E',
										blr_parameter, 0, 0, 0,
									blr_eql,
										blr_field, 0, 18, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
										blr_parameter, 0, 1, 0,
						blr_send, 1,
							blr_begin,
								blr_assignment,
									blr_field, 0, 15, 'R','D','B','$','B','A','S','E','_','F','I','E','L','D',
									blr_parameter, 1, 0, 0,
								blr_assignment,
									blr_field, 0, 15, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E',
									blr_parameter, 1, 1, 0,
								blr_assignment,
									blr_literal, blr_short, 0, 1, 0,
									blr_parameter, 1, 2, 0,
							blr_end,
					blr_send, 1,
						blr_assignment,
							blr_literal, blr_short, 0, 0, 0,
							blr_parameter, 1, 2, 0,
				blr_end,
				blr_eoc
			};
			
			struct {
				char	isc_req_30_schema[256];
				char	isc_req_30_view_name[256];
			} isc_req_30_in;
			
			struct {
				char	isc_req_30_base_field[256];
				char	isc_req_30_field_name[256];
				SSHORT	isc_req_30_found;
			} isc_req_30_out;
			
			strcpy(isc_req_30_in.isc_req_30_schema, isc_req_29_out.isc_req_29_relation_schema);
			strcpy(isc_req_30_in.isc_req_30_view_name, isc_req_29_out.isc_req_29_relation);
			
			EXE_send(tdbb, request2, 0, sizeof(isc_req_30_in), (UCHAR*)&isc_req_30_in);
			
			while (true)
			{
				EXE_receive(tdbb, request2, 1, sizeof(isc_req_30_out), (UCHAR*)&isc_req_30_out);
				if (!isc_req_30_out.isc_req_30_found)
					break;
					
				if (isc_req_30_out.isc_req_30_base_field[0] == '\0' || isc_req_30_out.isc_req_30_field_name[0] == '\0')
					continue;

				const MetaName baseField(isc_req_30_out.isc_req_30_base_field);
				if (currentAux.exist(baseField))
					ambiguities.add(baseField);
				else
				{
					const MetaName fieldName(isc_req_30_out.isc_req_30_field_name);
					if (first)
					{
						fields.put(fieldName, baseField);
						currentAux.put(baseField, fieldName);
					}
					else
					{
						MetaName field;

						if (previousAux.get(fieldName, field))
						{
							fields.put(field, baseField);
							currentAux.put(baseField, field);
						}
					}
				}
			}

			for (const MetaName* i = ambiguities.begin(); i != ambiguities.end(); ++i)
			{
				MetaName field;

				if (currentAux.get(*i, field))
				{
					currentAux.remove(*i);
					fields.remove(field);
				}
			}

			previousAux.takeOwnership(currentAux);

			if (!(relation->rel_flags & REL_view))
			{
				cont = false;
				break;
			}

			first = false;
		}
	}

	if (!relation)
		fields.clear();

	return relation;
}


bool METD_get_view_relation(jrd_tra* transaction, DsqlCompilerScratch* dsqlScratch,
	const Jrd::QualifiedName& view_name, const Jrd::QualifiedName& relation_or_alias,
	dsql_rel*& relation, dsql_prc*& procedure)
{
/**************************************
 *
 *  M E T D _ g e t _ v i e w _ r e l a t i o n
 *
 **************************************
 *
 * Functional description
 *  Return TRUE if the passed view_name represents a
 *  view with the passed relation as a base table
 *  (the relation could be an alias).
 *
 **************************************/
	thread_db* tdbb = JRD_get_thread_data();

	validateTransaction(transaction);

	AutoCacheRequest handle(tdbb, irq_view, IRQ_REQUESTS);

	// Converted FOR loop #21: Get view relations from RDB$VIEW_RELATIONS
	EXE_start(tdbb, handle, transaction);
	
	const jrd_req* const request = handle;
	
	USHORT isc_req_31_2[68] = {
		blr_version5,
		blr_begin,
		blr_message, 0, 2, 0,
			blr_varying2, 3, 0, 0, 0,
			blr_varying2, 3, 0, 0, 0,
		blr_receive, 0,
		blr_begin,
			blr_for,
				blr_rse, 1,
					blr_relation, 18, 'R','D','B','$','V','I','E','W','_','R','E','L','A','T','I','O','N','S', 0,
					blr_boolean,
						blr_and,
							blr_eql,
								blr_field, 0, 16, 'R','D','B','$','S','C','H','E','M','A','_','N','A','M','E',
								blr_parameter, 0, 0, 0,
							blr_eql,
								blr_field, 0, 14, 'R','D','B','$','V','I','E','W','_','N','A','M','E',
								blr_parameter, 0, 1, 0,
				blr_send, 1,
					blr_begin,
						blr_assignment,
							blr_field, 0, 18, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
							blr_parameter, 1, 0, 0,
						blr_assignment,
							blr_field, 0, 25, 'R','D','B','$','R','E','L','A','T','I','O','N','_','S','C','H','E','M','A','_','N','A','M','E',
							blr_parameter, 1, 1, 0,
						blr_assignment,
							blr_field, 0, 16, 'R','D','B','$','C','O','N','T','E','X','T','_','N','A','M','E',
							blr_parameter, 1, 2, 0,
						blr_assignment,
							blr_field, 0, 17, 'R','D','B','$','P','A','C','K','A','G','E','_','N','A','M','E',
							blr_parameter, 1, 3, 0,
						blr_assignment,
							blr_literal, blr_short, 0, 1, 0,
							blr_parameter, 1, 4, 0,
					blr_end,
			blr_send, 1,
				blr_assignment,
					blr_literal, blr_short, 0, 0, 0,
					blr_parameter, 1, 4, 0,
		blr_end,
		blr_eoc
	};
	
	struct {
		char	isc_req_31_schema[256];
		char	isc_req_31_view_name[256];
	} isc_req_31_in;
	
	struct {
		char	isc_req_31_relation[256];
		char	isc_req_31_relation_schema[256];
		char	isc_req_31_context_name[256];
		char	isc_req_31_package_name[256];
		SSHORT	isc_req_31_found;
	} isc_req_31_out;
	
	strcpy(isc_req_31_in.isc_req_31_schema, view_name.schema.c_str());
	strcpy(isc_req_31_in.isc_req_31_view_name, view_name.object.c_str());
	
	EXE_send(tdbb, request, 0, sizeof(isc_req_31_in), (UCHAR*)&isc_req_31_in);
	
	while (true)
	{
		EXE_receive(tdbb, request, 1, sizeof(isc_req_31_out), (UCHAR*)&isc_req_31_out);
		if (!isc_req_31_out.isc_req_31_found)
			break;
			
		QualifiedName relationName(isc_req_31_out.isc_req_31_relation, isc_req_31_out.isc_req_31_relation_schema);

		ObjectsArray<QualifiedMetaString> contextName;
		try
		{
			QualifiedMetaString::parseSchemaObjectListNoSep(isc_req_31_out.isc_req_31_context_name, contextName);
		}
		catch (const status_exception&)
		{
			// Legacy (restored from backups) data stored in RDB$CONTEXT_NAME may cause exceptions.
			contextName.clear();
			contextName.push(QualifiedMetaString(isc_req_31_out.isc_req_31_context_name));
		}

		if (PASS1_compare_alias(relationName, relation_or_alias) ||
			(relation_or_alias.schema.isEmpty() &&
				contextName.getCount() == 1 &&
				PASS1_compare_alias(contextName[0], relation_or_alias)))
		{
			if ((relation = METD_get_relation(transaction, dsqlScratch, relationName)))
				return true;

			const QualifiedName procName(isc_req_31_out.isc_req_31_relation, isc_req_31_out.isc_req_31_relation_schema, isc_req_31_out.isc_req_31_package_name);

			if ( (procedure = METD_get_procedure(transaction, dsqlScratch, procName)) )
				return true;
		}

		if (METD_get_view_relation(transaction, dsqlScratch, relationName, relation_or_alias, relation, procedure))
			return true;
	}

	return false;
}