/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		par.cpp
 *	DESCRIPTION:	BLR Parser
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
 * 27-May-2001 Claudio Valderrama: par_plan() no longer uppercases
 *			an index's name before doing a lookup of such index.
 * 2001.07.28: Added parse code for blr_skip to support LIMIT.
 * 2002.09.28 Dmitry Yemanov: Reworked internal_info stuff, enhanced
 *                            exception handling in SPs/triggers,
 *                            implemented ROWS_AFFECTED system variable
 * 2002.10.21 Nickolay Samofatov: Added support for explicit pessimistic locks
 * 2002.10.28 Sean Leyne - Code cleanup, removed obsolete "MPEXL" port
 * 2002.10.29 Mike Nordell - Fixed breakage.
 * 2002.10.29 Nickolay Samofatov: Added support for savepoints
 * 2002.10.29 Sean Leyne - Removed obsolete "Netware" port
 * 2003.10.05 Dmitry Yemanov: Added support for explicit cursors in PSQL
 * 2004.01.16 Vlad Horsun: Added support for default parameters
 * Adriano dos Santos Fernandes
 */

#include "firebird.h"
#include <stdio.h>
#include <string.h>
#include "../jrd/jrd.h"
#include "../jrd/ini.h"
#include "../jrd/val.h"
#include "../jrd/align.h"
#include "../jrd/exe.h"
#include "../jrd/extds/ExtDS.h"
#include "../jrd/lls.h"
#include "../jrd/scl.h"
#include "../jrd/req.h"
#include "../jrd/blb.h"
#include "../jrd/intl.h"
#include "../jrd/met.h"
#include "../jrd/cmp_proto.h"
#include "../jrd/cvt_proto.h"
#include "../jrd/err_proto.h"
#include "../jrd/fun_proto.h"
#include "../yvalve/gds_proto.h"
#include "../jrd/met_proto.h"
#include "../jrd/par_proto.h"
#include "../common/MsgUtil.h"
#include "../common/utils_proto.h"
#include "../jrd/RecordSourceNodes.h"
#include "../jrd/SysFunction.h"
#include "../common/classes/BlrReader.h"
#include "../jrd/Function.h"
#include "../jrd/Attachment.h"
#include "../dsql/BoolNodes.h"
#include "../dsql/ExprNodes.h"
#include "../dsql/StmtNodes.h"


using namespace Jrd;
using namespace ScratchBird;


static NodeParseFunc blr_parsers[256] = {NULL};


static void par_error(BlrReader& blrReader, const Arg::StatusVector& v, bool isSyntaxError = true);
static PlanNode* par_plan(thread_db*, CompilerScratch*);
static void parseSubRoutines(thread_db* tdbb, CompilerScratch* csb);
static void setNodeLineColumn(CompilerScratch* csb, DmlNode* node, ULONG blrOffset);


namespace
{
	class BlrParseWrapper
	{
	public:
		BlrParseWrapper(MemoryPool& pool, jrd_rel* relation, CompilerScratch* view_csb,
						CompilerScratch** csb_ptr, const bool trigger, USHORT flags)
			: m_csbPtr(csb_ptr)
		{
			if (!(csb_ptr && (m_csb = *csb_ptr)))
			{
				m_csb = FB_NEW_POOL(pool) CompilerScratch(pool);
				m_csb->csb_g_flags |= flags;
			}

			// If there is a request ptr, this is a trigger.  Set up contexts 0 and 1 for
			// the target relation

			if (trigger)
			{
				StreamType stream = m_csb->nextStream();
				CompilerScratch::csb_repeat* t1 = CMP_csb_element(m_csb, 0);
				t1->csb_flags |= csb_used | csb_active | csb_trigger;
				t1->csb_relation = relation;
				t1->csb_stream = stream;

				stream = m_csb->nextStream();
				t1 = CMP_csb_element(m_csb, 1);
				t1->csb_flags |= csb_used | csb_active | csb_trigger;
				t1->csb_relation = relation;
				t1->csb_stream = stream;
			}
			else if (relation)
			{
				CompilerScratch::csb_repeat* t1 = CMP_csb_element(m_csb, 0);
				t1->csb_stream = m_csb->nextStream();
				t1->csb_relation = relation;
				t1->csb_flags = csb_used | csb_active;
			}

			if (view_csb)
			{
				CompilerScratch::rpt_itr ptr = view_csb->csb_rpt.begin();
				// AB: csb_n_stream replaced by view_csb->csb_rpt.getCount(), because there could
				// be more then just csb_n_stream-numbers that hold data.
				// Certainly csb_stream (see PAR_context where the context is retrieved)
				const CompilerScratch::rpt_const_itr end = view_csb->csb_rpt.end();
				for (StreamType stream = 0; ptr != end; ++ptr, ++stream)
				{
					CompilerScratch::csb_repeat* t2 = CMP_csb_element(m_csb, stream);
					t2->csb_relation = ptr->csb_relation;
					t2->csb_procedure = ptr->csb_procedure;
					t2->csb_table_value_fun = ptr->csb_table_value_fun;
					t2->csb_stream = ptr->csb_stream;
					t2->csb_flags = ptr->csb_flags & csb_used;
				}
				m_csb->csb_n_stream = view_csb->csb_n_stream;
			}
		}

		operator CompilerScratch*()
		{
			return m_csb;
		}

		CompilerScratch* operator->()
		{
			return m_csb;
		}

		~BlrParseWrapper()
		{
			if (m_csbPtr)
				*m_csbPtr = m_csb.release();
		}

	private:
		AutoPtr<CompilerScratch> m_csb;
		CompilerScratch** const m_csbPtr;
	};
}	// namespace


// Parse blr, returning a compiler scratch block with the results.
// Caller must do pool handling.
DmlNode* PAR_blr(thread_db* tdbb, const MetaName* schema, jrd_rel* relation, const UCHAR* blr, ULONG blr_length,
	CompilerScratch* view_csb, CompilerScratch** csb_ptr, Statement** statementPtr,
	const bool trigger, USHORT flags)
{
#ifdef CMP_DEBUG
	cmp_trace("BLR code given for JRD parsing:");
	// CVC: Couldn't find isc_trace_printer, so changed it to gds__trace_printer.
	fb_print_blr(blr, blr_length, gds__trace_printer, 0, 0);
#endif

	BlrParseWrapper csb(*tdbb->getDefaultPool(), relation, view_csb, csb_ptr, trigger, flags);

	if (schema)
		csb->csb_schema = *schema;

	csb->csb_blr_reader = BlrReader(blr, blr_length);

	PAR_getBlrVersionAndFlags(csb);

	csb->csb_node = PAR_parse_node(tdbb, csb);

	if (csb->csb_blr_reader.getByte() != (UCHAR) blr_eoc)
		PAR_syntax_error(csb, "end_of_command");

	parseSubRoutines(tdbb, csb);

	if (statementPtr)
		*statementPtr = Statement::makeStatement(tdbb, csb, false);

	return csb->csb_node;
}


// Finish parse of memory nodes, returning a compiler scratch block with the results.
// Caller must do pool handling.
void PAR_preparsed_node(thread_db* tdbb, jrd_rel* relation, DmlNode* node,
	CompilerScratch* view_csb, CompilerScratch** csb_ptr, Statement** statementPtr,
	const bool trigger, USHORT flags)
{
	fb_assert(csb_ptr && *csb_ptr);
	BlrParseWrapper csb(*tdbb->getDefaultPool(), relation, view_csb, csb_ptr, trigger, flags);

	csb->blrVersion = 5;	// blr_version5
	csb->csb_node = node;

	if (statementPtr)
		*statementPtr = Statement::makeStatement(tdbb, csb, false);
}


// PAR_blr equivalent for validation expressions.
// Validation expressions are boolean expressions, but may be prefixed with a blr_stmt_expr.
BoolExprNode* PAR_validation_blr(thread_db* tdbb, const MetaName* schema, jrd_rel* relation, const UCHAR* blr, ULONG blr_length,
	CompilerScratch* view_csb, CompilerScratch** csb_ptr, USHORT flags)
{
	SET_TDBB(tdbb);

#ifdef CMP_DEBUG
	cmp_trace("BLR code given for JRD parsing:");
	// CVC: Couldn't find isc_trace_printer, so changed it to gds__trace_printer.
	fb_print_blr(blr, blr_length, gds__trace_printer, 0, 0);
#endif

	BlrParseWrapper csb(*tdbb->getDefaultPool(), relation, view_csb, csb_ptr, false, flags);

	if (schema)
		csb->csb_schema = *schema;

	csb->csb_blr_reader = BlrReader(blr, blr_length);

	PAR_getBlrVersionAndFlags(csb);

	if (csb->csb_blr_reader.peekByte() == blr_stmt_expr)
	{
		// Parse it and ignore. This legacy and broken verb is transformed while parsing expressions.
		csb->csb_blr_reader.getByte();
		PAR_parse_stmt(tdbb, csb);
	}

	BoolExprNode* const expr = PAR_parse_boolean(tdbb, csb);

	if (csb->csb_blr_reader.getByte() != (UCHAR) blr_eoc)
		PAR_syntax_error(csb, "end_of_command");

	return expr;
}


// Parse a BLR datatype. Return the alignment requirements of the datatype.
USHORT PAR_datatype(BlrReader& blrReader, dsc* desc)
{
	desc->clear();

	const USHORT dtype = blrReader.getByte();
	USHORT textType;

	switch (dtype)
	{
		case blr_text:
			desc->makeText(blrReader.getWord(), ttype_dynamic);
			desc->dsc_flags |= DSC_no_subtype;
			break;

		case blr_cstring:
			desc->dsc_dtype = dtype_cstring;
			desc->dsc_flags |= DSC_no_subtype;
			desc->dsc_length = blrReader.getWord();
			desc->setTextType(ttype_dynamic);
			break;

		case blr_varying:
			desc->makeVarying(blrReader.getWord(), ttype_dynamic);
			desc->dsc_flags |= DSC_no_subtype;
			break;

		case blr_text2:
			textType = blrReader.getWord();
			desc->makeText(blrReader.getWord(), textType);
			break;

		case blr_cstring2:
			desc->dsc_dtype = dtype_cstring;
			desc->setTextType(blrReader.getWord());
			desc->dsc_length = blrReader.getWord();
			break;

		case blr_varying2:
			textType = blrReader.getWord();
			desc->makeVarying(blrReader.getWord(), textType);
			break;

		case blr_short:
			desc->dsc_dtype = dtype_short;
			desc->dsc_length = sizeof(SSHORT);
			desc->dsc_scale = (int) blrReader.getByte();
			break;

		case blr_long:
			desc->dsc_dtype = dtype_long;
			desc->dsc_length = sizeof(SLONG);
			desc->dsc_scale = (int) blrReader.getByte();
			break;

		case blr_int64:
			desc->dsc_dtype = dtype_int64;
			desc->dsc_length = sizeof(SINT64);
			desc->dsc_scale = (int) blrReader.getByte();
			break;

		case blr_quad:
			desc->dsc_dtype = dtype_quad;
			desc->dsc_length = sizeof(ISC_QUAD);
			desc->dsc_scale = (int) blrReader.getByte();
			break;

		case blr_float:
			desc->dsc_dtype = dtype_real;
			desc->dsc_length = sizeof(float);
			break;

		case blr_timestamp:
			desc->dsc_dtype = dtype_timestamp;
			desc->dsc_length = sizeof(ISC_QUAD);
			break;

		case blr_timestamp_tz:
			desc->dsc_dtype = dtype_timestamp_tz;
			desc->dsc_length = sizeof(ISC_TIMESTAMP_TZ);
			break;

		case blr_ex_timestamp_tz:
			desc->dsc_dtype = dtype_ex_timestamp_tz;
			desc->dsc_length = sizeof(ISC_TIMESTAMP_TZ_EX);
			break;

		case blr_sql_date:
			desc->dsc_dtype = dtype_sql_date;
			desc->dsc_length = type_lengths[dtype_sql_date];
			break;

		case blr_sql_time:
			desc->dsc_dtype = dtype_sql_time;
			desc->dsc_length = type_lengths[dtype_sql_time];
			break;

		case blr_sql_time_tz:
			desc->dsc_dtype = dtype_sql_time_tz;
			desc->dsc_length = type_lengths[dtype_sql_time_tz];
			break;

		case blr_ex_time_tz:
			desc->dsc_dtype = dtype_ex_time_tz;
			desc->dsc_length = type_lengths[dtype_ex_time_tz];
			break;

		case blr_double:
		case blr_d_float:
			desc->dsc_dtype = dtype_double;
			desc->dsc_length = sizeof(double);
			break;

		case blr_dec64:
			desc->dsc_dtype = dtype_dec64;
			desc->dsc_length = sizeof(Decimal64);
			break;

		case blr_dec128:
			desc->dsc_dtype = dtype_dec128;
			desc->dsc_length = sizeof(Decimal128);
			break;

		case blr_int128:
			desc->dsc_dtype = dtype_int128;
			desc->dsc_length = sizeof(Int128);
			desc->dsc_scale = (int) blrReader.getByte();
			break;

		case blr_blob2:
			desc->dsc_dtype = dtype_blob;
			desc->dsc_length = sizeof(ISC_QUAD);
			desc->dsc_sub_type = blrReader.getWord();
			textType = blrReader.getWord();
			desc->dsc_scale = textType & 0xFF;		// BLOB character set
			desc->dsc_flags = textType & 0xFF00;	// BLOB collation
			break;

		case blr_bool:
			desc->makeBoolean();
			break;

		default:
			par_error(blrReader, Arg::Gds(isc_datnotsup));
	}

	return type_alignments[desc->dsc_dtype];
}


// Parse a BLR descriptor. Return the alignment requirements of the datatype.
USHORT PAR_desc(thread_db* tdbb, CompilerScratch* csb, dsc* desc, ItemInfo* itemInfo)
{
	if (itemInfo)
	{
		itemInfo->nullable = true;
		itemInfo->explicitCollation = false;
		itemInfo->fullDomain = false;
	}

	desc->clear();

	bool explicitCollation = false;
	const USHORT dtype = csb->csb_blr_reader.getByte();

	switch (dtype)
	{
		case blr_not_nullable:
			PAR_desc(tdbb, csb, desc, itemInfo);
			if (itemInfo)
				itemInfo->nullable = false;
			break;

		case blr_domain_name:
		case blr_domain_name2:
		case blr_domain_name3:
		{
			const bool fullDomain = (csb->csb_blr_reader.getByte() == blr_domain_full);
			const auto name = FB_NEW_POOL(csb->csb_pool) QualifiedName(csb->csb_pool);

			if (dtype == blr_domain_name3)
				csb->csb_blr_reader.getMetaName(name->schema);

			csb->csb_blr_reader.getMetaName(name->object);
			csb->qualifyExistingName(tdbb, *name, obj_field);

			QualifiedNameMetaNamePair entry(*name, {});

			FieldInfo fieldInfo;
			bool exist = csb->csb_map_field_info.get(entry, fieldInfo);
			MET_get_domain(tdbb, csb->csb_pool, *name, desc, (exist ? NULL : &fieldInfo));

			if (!exist)
				csb->csb_map_field_info.put(entry, fieldInfo);

			if (itemInfo)
			{
				itemInfo->field = entry;

				if (fullDomain)
				{
					itemInfo->nullable = fieldInfo.nullable;
					itemInfo->fullDomain = true;
				}
				else
					itemInfo->nullable = true;
			}

			if (dtype == blr_domain_name2 ||
				(dtype == blr_domain_name3 && csb->csb_blr_reader.getByte() != 0))
			{
				explicitCollation = true;

				const USHORT ttype = csb->csb_blr_reader.getWord();

				switch (desc->dsc_dtype)
				{
					case dtype_cstring:
					case dtype_text:
					case dtype_varying:
					case dtype_varying_large:
						desc->setTextType(ttype);
						break;

					case dtype_blob:
						desc->dsc_scale = ttype & 0xFF;		// BLOB character set
						desc->dsc_flags = ttype & 0xFF00;	// BLOB collation
						break;

					default:
						PAR_error(csb, Arg::Gds(isc_collation_requires_text));
						break;
				}
			}

			if (csb->collectingDependencies())
			{
				CompilerScratch::Dependency dependency(obj_field);
				dependency.name = name;
				csb->addDependency(dependency);
			}

			break;
		}

		case blr_column_name:
		case blr_column_name2:
		case blr_column_name3:
		{
			const bool fullDomain = (csb->csb_blr_reader.getByte() == blr_domain_full);
			const auto relationName = FB_NEW_POOL(csb->csb_pool) QualifiedName(csb->csb_pool);

			if (dtype == blr_column_name3)
				csb->csb_blr_reader.getMetaName(relationName->schema);

			csb->csb_blr_reader.getMetaName(relationName->object);
			csb->qualifyExistingName(tdbb, *relationName, obj_relation);

			const auto fieldName = FB_NEW_POOL(csb->csb_pool) MetaName(csb->csb_pool);
			csb->csb_blr_reader.getMetaName(*fieldName);

			QualifiedNameMetaNamePair entry(*relationName, *fieldName);

			FieldInfo fieldInfo;
			bool exist = csb->csb_map_field_info.get(entry, fieldInfo);
			MET_get_relation_field(tdbb, csb->csb_pool, *relationName, *fieldName, desc,
				(exist ? NULL : &fieldInfo));

			if (!exist)
				csb->csb_map_field_info.put(entry, fieldInfo);

			if (itemInfo)
			{
				itemInfo->field = entry;

				if (fullDomain)
				{
					itemInfo->nullable = fieldInfo.nullable;
					itemInfo->fullDomain = true;
				}
				else
					itemInfo->nullable = true;
			}

			if (dtype == blr_column_name2 ||
				(dtype == blr_column_name3 && csb->csb_blr_reader.getByte() != 0))
			{
				explicitCollation = true;

				const USHORT ttype = csb->csb_blr_reader.getWord();

				switch (desc->dsc_dtype)
				{
					case dtype_cstring:
					case dtype_text:
					case dtype_varying:
					case dtype_varying_large:
						desc->setTextType(ttype);
						break;

					case dtype_blob:
						desc->dsc_scale = ttype & 0xFF;		// BLOB character set
						desc->dsc_flags = ttype & 0xFF00;	// BLOB collation
						break;

					default:
						PAR_error(csb, Arg::Gds(isc_collation_requires_text));
						break;
				}
			}

			if (csb->collectingDependencies())
			{
				CompilerScratch::Dependency dependency(obj_relation);
				dependency.relation = MET_lookup_relation(tdbb, *relationName);
				dependency.subName = fieldName;
				csb->addDependency(dependency);
			}

			break;
		}

		default:
			csb->csb_blr_reader.seekBackward(1);
			PAR_datatype(csb->csb_blr_reader, desc);
			break;
	}

	if (csb->collectingDependencies() && desc->getTextType() != CS_NONE)
	{
		CompilerScratch::Dependency dependency(obj_collation);
		dependency.number = INTL_TEXT_TYPE(*desc);
		csb->addDependency(dependency);
	}

	if (itemInfo)
	{
		if (dtype == blr_cstring2 || dtype == blr_text2 || dtype == blr_varying2 || dtype == blr_blob2 ||
			explicitCollation)
		{
			itemInfo->explicitCollation = true;
		}
	}

	return type_alignments[desc->dsc_dtype];
}


ValueExprNode* PAR_gen_field(thread_db* tdbb, StreamType stream, USHORT id, bool byId)
{
/**************************************
 *
 *	P A R _ g e n _ f i e l d
 *
 **************************************
 *
 * Functional description
 *	Generate a field block.
 *
 **************************************/
	SET_TDBB(tdbb);

	return FB_NEW_POOL(*tdbb->getDefaultPool()) FieldNode(*tdbb->getDefaultPool(), stream, id, byId);
}


// Get the BLR version from the CSB stream and complain if it's unknown.
void PAR_getBlrVersionAndFlags(CompilerScratch* csb)
{
	BlrReader::Flags flags;
	const SSHORT version = csb->csb_blr_reader.parseHeader(&flags);

	switch (version)
	{
		case blr_version4:
			csb->blrVersion = 4;
			break;

		case blr_version5:
			csb->blrVersion = 5;
			break;

		//case blr_version6:
		//	csb->blrVersion = 6;
		//	break;

		default:
			PAR_error(csb,
				Arg::Gds(isc_metadata_corrupt) <<
				Arg::Gds(isc_wroblrver2) <<
					Arg::Num(blr_version4) << Arg::Num(blr_version5/*6*/) << Arg::Num(version));
	}

	if (flags.searchSystemSchema)
		csb->csb_g_flags |= csb_search_system_schema;
}


ValueExprNode* PAR_make_field(thread_db* tdbb, CompilerScratch* csb, USHORT context,
	const MetaName& base_field)
{
/**************************************
 *
 *	P A R _ m a k e _ f i e l d
 *
 **************************************
 *
 * Functional description
 *	Make up a field node in the permanent pool.  This is used
 *	by MET_scan_relation to handle view fields.
 *
 **************************************/
	SET_TDBB(tdbb);

	if (context >= csb->csb_rpt.getCount() || !(csb->csb_rpt[context].csb_flags & csb_used))
		return NULL;

	const StreamType stream = csb->csb_rpt[context].csb_stream;

	jrd_rel* const relation = csb->csb_rpt[stream].csb_relation;
	jrd_prc* const procedure = csb->csb_rpt[stream].csb_procedure;
	jrd_table_value_fun* const table_value_function = csb->csb_rpt[stream].csb_table_value_fun;

	const SSHORT id =
		relation ? MET_lookup_field(tdbb, relation, base_field) :
		procedure ? PAR_find_proc_field(procedure, base_field) :
		table_value_function ? table_value_function->getId(base_field) : -1;

	if (id < 0)
		return NULL;

	if (csb->collectingDependencies())
		PAR_dependency(tdbb, csb, stream, id, base_field);

	return PAR_gen_field(tdbb, stream, id);
}


// Make a list node out of a stack.
CompoundStmtNode* PAR_make_list(thread_db* tdbb, StmtNodeStack& stack)
{
	SET_TDBB(tdbb);

	// Count the number of nodes.
	USHORT count = stack.getCount();

	CompoundStmtNode* node = FB_NEW_POOL(*tdbb->getDefaultPool()) CompoundStmtNode(*tdbb->getDefaultPool());

	NestConst<StmtNode>* ptr = node->statements.getBuffer(count) + count;

	while (stack.hasData())
		*--ptr = stack.pop();

	return node;
}


ULONG PAR_marks(Jrd::CompilerScratch* csb)
{
	if (csb->csb_blr_reader.getByte() != blr_marks)
		PAR_syntax_error(csb, "blr_marks");

	switch (csb->csb_blr_reader.getByte())
	{
	case 1:
		return csb->csb_blr_reader.getByte();

	case 2:
		return csb->csb_blr_reader.getWord();

	case 4:
		return csb->csb_blr_reader.getLong();
	}
	PAR_syntax_error(csb, "valid length for blr_marks value (1, 2, or 4)");
	return 0;
}

CompilerScratch* PAR_parse(thread_db* tdbb, const UCHAR* blr, ULONG blr_length,
	bool internal_flag, ULONG dbginfo_length, const UCHAR* dbginfo)
{
/**************************************
 *
 *	P A R _ p a r s e
 *
 **************************************
 *
 * Functional description
 *	Parse blr, returning a compiler scratch block with the results.
 *
 **************************************/
	SET_TDBB(tdbb);

	MemoryPool& pool = *tdbb->getDefaultPool();
	AutoPtr<CompilerScratch> csb(FB_NEW_POOL(pool) CompilerScratch(pool));

	csb->csb_blr_reader = BlrReader(blr, blr_length);

	if (internal_flag)
	{
		csb->csb_g_flags |= csb_internal;
		csb->csb_schema = SYSTEM_SCHEMA;
	}

	PAR_getBlrVersionAndFlags(csb);

	if (dbginfo_length > 0)
		DBG_parse_debug_info(dbginfo_length, dbginfo, *csb->csb_dbg_info);

	DmlNode* node = PAR_parse_node(tdbb, csb);
	csb->csb_node = node;

	if (csb->csb_blr_reader.getByte() != (UCHAR) blr_eoc)
		PAR_syntax_error(csb, "end_of_command");

	parseSubRoutines(tdbb, csb);

	return csb.release();
}


SLONG PAR_symbol_to_gdscode(const ScratchBird::string& name)
{
/**************************************
 *
 *	P A R _ s y m b o l _ t o _ g d s c o d e
 *
 **************************************
 *
 * Functional description
 *	Symbolic ASCII names are used in blr for posting and handling
 *	exceptions.  They are also used to identify error codes
 *	within system triggers in a database.
 *
 *	Returns the gds error status code for the given symbolic
 *	name, or 0 if not found.
 *
 *	Symbolic names may be null or space terminated.
 *
 **************************************/

	return MsgUtil::getCodeByName(name.c_str());
}


// Registers a parse function (DmlNode creator) for a BLR code.
void PAR_register(UCHAR blr, NodeParseFunc parseFunc)
{
	fb_assert(!blr_parsers[blr] || blr_parsers[blr] == parseFunc);
	blr_parsers[blr] = parseFunc;
}


// We've got a blr error other than a syntax error. Handle it.
static void par_error(BlrReader& blrReader, const Arg::StatusVector& v, bool isSyntaxError)
{
	fb_assert(v.value()[0] == isc_arg_gds);

	// Don't bother to pass tdbb for error handling
	thread_db* tdbb = JRD_get_thread_data();

	if (isSyntaxError)
	{
		blrReader.seekBackward(1);
		Arg::Gds p(isc_invalid_blr);
		p << Arg::Num(blrReader.getOffset());
		p.append(v);
		p.copyTo(tdbb->tdbb_status_vector);
	}
	else
		v.copyTo(tdbb->tdbb_status_vector);

	// Give up whatever we were doing and return to the user.
	ERR_punt();
}

// We've got a blr error other than a syntax error. Handle it.
void PAR_error(CompilerScratch* csb, const Arg::StatusVector& v, bool isSyntaxError)
{
	par_error(csb->csb_blr_reader, v, isSyntaxError);
}


// Look for named field in procedure output fields.
SSHORT PAR_find_proc_field(const jrd_prc* procedure, const MetaName& name)
{
	const Array<NestConst<Parameter> >& list = procedure->getOutputFields();

	Array<NestConst<Parameter> >::const_iterator ptr = list.begin();
	for (const Array<NestConst<Parameter> >::const_iterator end = list.end(); ptr < end; ++ptr)
	{
		const Parameter* param = *ptr;
		if (name == param->prm_name)
			return param->prm_number;
	}

	return -1;
}


// Parse a counted argument list, given the count.
ValueListNode* PAR_args(thread_db* tdbb, CompilerScratch* csb, USHORT count, USHORT allocCount)
{
	SET_TDBB(tdbb);

	fb_assert(allocCount >= count);

	MemoryPool& pool = *tdbb->getDefaultPool();
	ValueListNode* node = FB_NEW_POOL(pool) ValueListNode(pool, allocCount);
	NestConst<ValueExprNode>* ptr = node->items.begin();

	if (count)
	{
		do
		{
			if (csb->csb_blr_reader.peekByte() == blr_default_arg)
				csb->csb_blr_reader.getByte();
			else
				*ptr = PAR_parse_value(tdbb, csb);

			++ptr;
		} while (--count);
	}

	return node;
}


// Parse a counted argument list.
ValueListNode* PAR_args(thread_db* tdbb, CompilerScratch* csb)
{
	SET_TDBB(tdbb);
	const UCHAR count = csb->csb_blr_reader.getByte();
	return PAR_args(tdbb, csb, count, count);
}


// Introduce a new context into the system.
// This involves assigning a stream and possibly extending the compile scratch block.
StreamType par_context(CompilerScratch* csb, USHORT context)
{
	const auto tail = CMP_csb_element(csb, context);

	if (tail->csb_flags & csb_used)
	{
		if (csb->csb_g_flags & csb_reuse_context) {
			return tail->csb_stream;
		}

		PAR_error(csb, Arg::Gds(isc_ctxinuse));
	}

	const StreamType stream = csb->nextStream(false);
	if (stream >= MAX_STREAMS)
	{
		PAR_error(csb, Arg::Gds(isc_too_many_contexts));
	}

	tail->csb_flags |= csb_used;
	tail->csb_stream = stream;

	CMP_csb_element(csb, stream);

	return stream;
}

// Introduce a new context into the system - byte version.
StreamType PAR_context(CompilerScratch* csb, SSHORT* context_ptr)
{
	const USHORT context = csb->csb_blr_reader.getByte();

	if (context_ptr)
		*context_ptr = (SSHORT) context;

	return par_context(csb, context);
}

// Introduce a new context into the system - word version.
StreamType PAR_context2(CompilerScratch* csb, SSHORT* context_ptr)
{
	const USHORT context = csb->csb_blr_reader.getWord();

	if (context_ptr)
		*context_ptr = (SSHORT) context;

	return par_context(csb, context);
}


void PAR_dependency(thread_db* tdbb, CompilerScratch* csb, StreamType stream, SSHORT id,
	const MetaName& field_name)
{
/**************************************
 *
 *	P A R _ d e p e n d e n c y
 *
 **************************************
 *
 * Functional description
 *	Register a field, relation, procedure or exception reference
 *	as a dependency.
 *
 **************************************/
	SET_TDBB(tdbb);

	if (!csb->collectingDependencies())
		return;

	CompilerScratch::Dependency dependency(0);

	if (csb->csb_rpt[stream].csb_relation)
	{
		dependency.relation = csb->csb_rpt[stream].csb_relation;
		// How do I determine reliably this is a view?
		// At this time, rel_view_rse is still null.
		//if (is_view)
		//	dependency.objType = obj_view;
		//else
			dependency.objType = obj_relation;
	}
	else if (csb->csb_rpt[stream].csb_procedure)
	{
		if (csb->csb_rpt[stream].csb_procedure->isSubRoutine())
			return;

		dependency.procedure = csb->csb_rpt[stream].csb_procedure;
		dependency.objType = obj_procedure;
	}

	if (field_name.length() > 0)
		dependency.subName = FB_NEW_POOL(*tdbb->getDefaultPool()) MetaName(*tdbb->getDefaultPool(), field_name);
	else if (id >= 0)
		dependency.subNumber = id;

	csb->addDependency(dependency);
}


static PlanNode* par_plan(thread_db* tdbb, CompilerScratch* csb)
{
/**************************************
 *
 *	p a r _ p l a n
 *
 **************************************
 *
 * Functional description
 *	Parse an access plan expression.
 *	At this stage we are just generating the
 *	parse tree and checking contexts
 *	and indices.
 *
 **************************************/
	SET_TDBB(tdbb);

	auto blrOp = csb->csb_blr_reader.getByte();

	// a join type indicates a cross of two or more streams

	if (blrOp == blr_join || blrOp == blr_merge)
	{
		// CVC: bottleneck
		int count = (USHORT) csb->csb_blr_reader.getByte();
		PlanNode* plan = FB_NEW_POOL(csb->csb_pool) PlanNode(csb->csb_pool, PlanNode::TYPE_JOIN);

		while (count-- > 0)
			plan->subNodes.add(par_plan(tdbb, csb));

		return plan;
	}

	// we have hit a stream; parse the context number and access type

	jrd_rel* relation = nullptr;
	jrd_prc* procedure = nullptr;

	if (blrOp == blr_retrieve)
	{
		PlanNode* plan = FB_NEW_POOL(csb->csb_pool) PlanNode(csb->csb_pool, PlanNode::TYPE_RETRIEVE);

		// parse the relation name and context--the relation
		// itself is redundant except in the case of a view,
		// in which case the base relation (and alias) must be specified

		blrOp = csb->csb_blr_reader.getByte();

		// Don't make RecordSourceNode::parse() to parse the context, because
		// this would add a new context, while this is a reference to
		// an existing context

		switch (blrOp)
		{
			case blr_relation:
			case blr_rid:
			case blr_relation2:
			case blr_rid2:
			case blr_relation3:
			{
				const auto relationNode = RelationSourceNode::parse(tdbb, csb, blrOp, false);
				plan->recordSourceNode = relationNode;
				relation = relationNode->relation;
				break;
			}

			case blr_pid:
			case blr_pid2:
			case blr_procedure:
			case blr_procedure2:
			case blr_procedure3:
			case blr_procedure4:
			case blr_subproc:
			case blr_select_procedure:
			{
				const auto procedureNode = ProcedureSourceNode::parse(tdbb, csb, blrOp, false);
				plan->recordSourceNode = procedureNode;
				procedure = procedureNode->procedure;
				break;
			}

			case blr_local_table_id:
				// TODO

			default:
				PAR_syntax_error(csb, "TABLE or PROCEDURE");
		}

		// CVC: bottleneck
		const auto context = csb->csb_blr_reader.getByte();
		if (context >= csb->csb_rpt.getCount() || !(csb->csb_rpt[context].csb_flags & csb_used))
			PAR_error(csb, Arg::Gds(isc_ctxnotdef));
		const StreamType stream = csb->csb_rpt[context].csb_stream;

		plan->recordSourceNode->setStream(stream);
		/// plan->recordSourceNode->context = context; not needed ???

		if (procedure)
		{
			// Skip input parameters count, it's always zero for plans
			const auto count = csb->csb_blr_reader.getWord();
			fb_assert(!count);
		}

		// Access plan types (sequential is default)

		blrOp = csb->csb_blr_reader.getByte();

		const bool isGbak = tdbb->getAttachment()->isGbak();

		switch (blrOp)
		{
		case blr_navigational:
			{
				if (procedure)
					PAR_error(csb, Arg::Gds(isc_wrong_proc_plan));

				plan->accessType = FB_NEW_POOL(csb->csb_pool) PlanNode::AccessType(csb->csb_pool,
					PlanNode::AccessType::TYPE_NAVIGATIONAL);

				// pick up the index name and look up the appropriate ids

				QualifiedName name;
				csb->csb_blr_reader.getMetaName(name.object);
				name.schema = relation->rel_name.schema;

				SLONG relation_id;
				IndexStatus idx_status;
				const SLONG index_id = MET_lookup_index_name(tdbb, name, &relation_id, &idx_status);

				if (idx_status == MET_object_unknown || idx_status == MET_object_inactive)
				{
					if (isGbak)
					{
						PAR_warning(Arg::Warning(isc_indexname) << name.toQuotedString() <<
																   relation->rel_name.toQuotedString());
					}
					else
					{
						PAR_error(csb, Arg::Gds(isc_indexname) << name.toQuotedString() <<
															  	  relation->rel_name.toQuotedString());
					}
				}
				else if (idx_status == MET_object_deferred_active)
				{
					if (!isGbak)
					{
						PAR_error(csb, Arg::Gds(isc_indexname) << name.toQuotedString() <<
																  relation->rel_name.toQuotedString());
					}
				}

				// save both the relation id and the index id, since
				// the relation could be a base relation of a view;
				// save the index name also, for convenience

				PlanNode::AccessItem& item = plan->accessType->items.add();
				item.relationId = relation_id;
				item.indexId = index_id;
				item.indexName = name;

				if (csb->collectingDependencies())
				{
					CompilerScratch::Dependency dependency(obj_index);
					dependency.name = &item.indexName;
					csb->addDependency(dependency);
				}

				if (csb->csb_blr_reader.peekByte() != blr_indices)
					break;

				// dimitr:	FALL INTO, if the plan item is ORDER ... INDEX (...)
				[[fallthrough]];
			}

		case blr_indices:
			{
				if (procedure)
					PAR_error(csb, Arg::Gds(isc_wrong_proc_plan));

				if (plan->accessType)
					csb->csb_blr_reader.getByte(); // skip blr_indices
				else
				{
					plan->accessType = FB_NEW_POOL(csb->csb_pool) PlanNode::AccessType(csb->csb_pool,
						PlanNode::AccessType::TYPE_INDICES);
				}

				int count = (USHORT) csb->csb_blr_reader.getByte();

				// pick up the index names and look up the appropriate ids

				while (count-- > 0)
				{
					QualifiedName name;
					csb->csb_blr_reader.getMetaName(name.object);
					name.schema = relation->rel_name.schema;

					SLONG relation_id;
					IndexStatus idx_status;
					const SLONG index_id = MET_lookup_index_name(tdbb, name, &relation_id, &idx_status);

					if (idx_status == MET_object_unknown || idx_status == MET_object_inactive)
					{
						if (isGbak)
						{
							PAR_warning(Arg::Warning(isc_indexname) << name.toQuotedString() <<
																	   relation->rel_name.toQuotedString());
						}
						else
						{
							PAR_error(csb, Arg::Gds(isc_indexname) << name.toQuotedString() <<
																  	  relation->rel_name.toQuotedString());
						}
					}
					else if (idx_status == MET_object_deferred_active)
					{
						if (!isGbak)
						{
							PAR_error(csb, Arg::Gds(isc_indexname) << name.toQuotedString() <<
																	  relation->rel_name.toQuotedString());
						}
					}

					// save both the relation id and the index id, since
					// the relation could be a base relation of a view;
					// save the index name also, for convenience

					PlanNode::AccessItem& item = plan->accessType->items.add();
					item.relationId = relation_id;
					item.indexId = index_id;
					item.indexName = name;

					if (csb->collectingDependencies())
					{
						CompilerScratch::Dependency dependency(obj_index);
						dependency.name = &item.indexName;
						csb->addDependency(dependency);
		            }
				}
			}
			break;

		case blr_sequential:
			break;

		default:
			PAR_syntax_error(csb, "access type");
		}

		return plan;
	}

	PAR_syntax_error(csb, "plan item");
	return NULL;			// Added to remove compiler warning
}


// Parse a RecordSourceNode.
RecordSourceNode* PAR_parseRecordSource(thread_db* tdbb, CompilerScratch* csb)
{
	SET_TDBB(tdbb);

	const SSHORT blrOp = csb->csb_blr_reader.getByte();

	switch (blrOp)
	{
		case blr_pid:
		case blr_pid2:
		case blr_procedure:
		case blr_procedure2:
		case blr_procedure3:
		case blr_procedure4:
		case blr_subproc:
		case blr_select_procedure:
			return ProcedureSourceNode::parse(tdbb, csb, blrOp, true);

		case blr_rse:
		case blr_lateral_rse:
		case blr_rs_stream:
			return PAR_rse(tdbb, csb, blrOp);

		case blr_relation:
		case blr_rid:
		case blr_relation2:
		case blr_rid2:
		case blr_relation3:
			return RelationSourceNode::parse(tdbb, csb, blrOp, true);

		case blr_local_table_id:
			return LocalTableSourceNode::parse(tdbb, csb, blrOp, true);

		case blr_union:
		case blr_recurse:
			return UnionSourceNode::parse(tdbb, csb, blrOp);

		case blr_window:
			return WindowSourceNode::parse(tdbb, csb);

		case blr_aggregate:
			return AggregateSourceNode::parse(tdbb, csb);

		case blr_table_value_fun:
			return TableValueFunctionSourceNode::parse(tdbb, csb, blrOp);

		default:
			PAR_syntax_error(csb, "record source");
	}

	return NULL;	// silence warning
}


// Parse a record selection expression.
RseNode* PAR_rse(thread_db* tdbb, CompilerScratch* csb, SSHORT rse_op)
{
	SET_TDBB(tdbb);

	const ULONG blrOffset = csb->csb_blr_reader.getOffset() - 1;
	int count = (unsigned int) csb->csb_blr_reader.getByte();
	RseNode* rse = FB_NEW_POOL(*tdbb->getDefaultPool()) RseNode(*tdbb->getDefaultPool());

	setNodeLineColumn(csb, rse, blrOffset);

	if (rse_op == blr_lateral_rse)
		rse->flags |= RseNode::FLAG_LATERAL;

	while (--count >= 0)
		rse->rse_relations.add(PAR_parseRecordSource(tdbb, csb));

	while (true)
	{
		const UCHAR op = csb->csb_blr_reader.getByte();

		switch (op)
		{
		case blr_boolean:
			rse->rse_boolean = PAR_parse_boolean(tdbb, csb);
			break;

		case blr_first:
			if (rse_op == blr_rs_stream)
				PAR_syntax_error(csb, "RecordSelExpr stream clause");
			rse->rse_first = PAR_parse_value(tdbb, csb);
			rse->firstRows = true;
			break;

		case blr_skip:
			if (rse_op == blr_rs_stream)
				PAR_syntax_error(csb, "RecordSelExpr stream clause");
			rse->rse_skip = PAR_parse_value(tdbb, csb);
			break;

		case blr_sort:
			if (rse_op == blr_rs_stream)
				PAR_syntax_error(csb, "RecordSelExpr stream clause");
			csb->csb_blr_reader.seekBackward(1);
			rse->rse_sorted = PAR_sort(tdbb, csb, op, false);
			break;

		case blr_project:
			if (rse_op == blr_rs_stream)
				PAR_syntax_error(csb, "RecordSelExpr stream clause");
			csb->csb_blr_reader.seekBackward(1);
			rse->rse_projection = PAR_sort(tdbb, csb, op, false);
			break;

		case blr_join_type:
			{
				const auto jointype = csb->csb_blr_reader.getByte();
				if (jointype != blr_inner &&
					jointype != blr_left &&
					jointype != blr_right &&
					jointype != blr_full)
				{
					PAR_syntax_error(csb, "join type clause");
				}
				rse->rse_jointype = jointype;
				break;
			}

		case blr_plan:
			rse->rse_plan = par_plan(tdbb, csb);
			break;

		case blr_writelock:
			// PAR_parseRecordSource() called RelationSourceNode::parse() => MET_scan_relation().
			for (FB_SIZE_T iter = 0; iter < rse->rse_relations.getCount(); ++iter)
			{
				const RelationSourceNode* subNode = nodeAs<RelationSourceNode>(rse->rse_relations[iter]);
				if (!subNode)
					continue;
				const RelationSourceNode* relNode = static_cast<const RelationSourceNode*>(subNode);
				const jrd_rel* relation = relNode->relation;
				fb_assert(relation);
				if (relation->isVirtual())
					PAR_error(csb, Arg::Gds(isc_forupdate_virtualtbl) << relation->rel_name.toQuotedString(), false);
				if (relation->isSystem())
					PAR_error(csb, Arg::Gds(isc_forupdate_systbl) << relation->rel_name.toQuotedString(), false);
				if (relation->isTemporary())
					PAR_error(csb, Arg::Gds(isc_forupdate_temptbl) << relation->rel_name.toQuotedString(), false);
			}
			rse->flags |= RseNode::FLAG_WRITELOCK;
			break;

		case blr_skip_locked:
			rse->flags |= RseNode::FLAG_SKIP_LOCKED;
			break;

		case blr_optimize:
			rse->firstRows = (csb->csb_blr_reader.getByte() != 0);
			break;

		default:
			if (op == (UCHAR) blr_end)
			{
				// An outer join is only allowed when the stream count is 2
				// and a boolean expression has been supplied

				if (rse->isInnerJoin() ||
					(rse->rse_relations.getCount() == 2 && rse->rse_boolean))
				{
					// Convert right outer joins to left joins to avoid
					// RIGHT JOIN handling at lower engine levels
					if (rse->rse_jointype == blr_right)
					{
						// Swap sub-streams
						RecordSourceNode* temp = rse->rse_relations[0];
						rse->rse_relations[0] = rse->rse_relations[1];
						rse->rse_relations[1] = temp;

						rse->rse_jointype = blr_left;
					}

					return rse;
				}
			}

			PAR_syntax_error(csb, ((rse_op == blr_rs_stream) ?
						 "RecordSelExpr stream clause" :
						 "record selection expression clause"));
		}
	}
}


// Parse a record selection expression.
RseNode* PAR_rse(thread_db* tdbb, CompilerScratch* csb)
{
	SET_TDBB(tdbb);

	const ULONG blrOffset = csb->csb_blr_reader.getOffset();
	const SSHORT blrOp = csb->csb_blr_reader.getByte();
	RseNode* rseNode = nullptr;

	switch (blrOp)
	{
		case blr_rse:
		case blr_lateral_rse:
		case blr_rs_stream:
			rseNode = PAR_rse(tdbb, csb, blrOp);
			break;

		case blr_singular:
			rseNode = PAR_rse(tdbb, csb);
			rseNode->flags |= RseNode::FLAG_SINGULAR;
			break;

		case blr_scrollable:
			rseNode = PAR_rse(tdbb, csb);
			rseNode->flags |= RseNode::FLAG_SCROLLABLE;
			break;

		default:
			PAR_syntax_error(csb, "RecordSelExpr");
	}

	setNodeLineColumn(csb, rseNode, blrOffset);

	return rseNode;
}


// Parse a sort clause (sans header byte). This is used for blr_sort, blr_project and blr_group_by.
SortNode* PAR_sort(thread_db* tdbb, CompilerScratch* csb, UCHAR expectedBlr,
	bool nullForEmpty)
{
	SET_TDBB(tdbb);

	const UCHAR blrOp = csb->csb_blr_reader.getByte();

	if (blrOp != expectedBlr)
	{
		char s[20];
		snprintf(s, sizeof(s), "blr code %d", expectedBlr);
		PAR_syntax_error(csb, s);
	}

	USHORT count = csb->csb_blr_reader.getByte();

	if (count == 0 && nullForEmpty)
		return NULL;

	SortNode* sort = PAR_sort_internal(tdbb, csb, blrOp == blr_sort, count);

	if (blrOp != blr_sort)
		sort->unique = true;

	return sort;
}


// Parse the internals of a sort clause. This is used for blr_sort, blr_project, blr_group_by
// and blr_partition_by.
SortNode* PAR_sort_internal(thread_db* tdbb, CompilerScratch* csb, bool allClauses, USHORT count)
{
	SET_TDBB(tdbb);

	SortNode* sort = FB_NEW_POOL(*tdbb->getDefaultPool()) SortNode(
		*tdbb->getDefaultPool());

	NestConst<ValueExprNode>* ptr = sort->expressions.getBuffer(count);
	SortDirection* ptr2 = sort->direction.getBuffer(count);
	NullsPlacement* ptr3 = sort->nullOrder.getBuffer(count);

	while (count-- > 0)
	{
		if (allClauses)
		{
			UCHAR code = csb->csb_blr_reader.getByte();

			switch (code)
			{
				case blr_nullsfirst:
					*ptr3++ = NULLS_FIRST;
					code = csb->csb_blr_reader.getByte();
					break;

				case blr_nullslast:
					*ptr3++ = NULLS_LAST;
					code = csb->csb_blr_reader.getByte();
					break;

				default:
					*ptr3++ = NULLS_DEFAULT;
			}

			*ptr2++ = (code == blr_descending) ? ORDER_DESC : ORDER_ASC;
		}
		else
		{
			*ptr2++ = ORDER_ANY;
			*ptr3++ = NULLS_DEFAULT;
		}

		*ptr++ = PAR_parse_value(tdbb, csb);
	}

	return sort;
}


// Parse a boolean node.
BoolExprNode* PAR_parse_boolean(thread_db* tdbb, CompilerScratch* csb)
{
	DmlNode* node = PAR_parse_node(tdbb, csb);

	if (node->getKind() != DmlNode::KIND_BOOLEAN)
		PAR_syntax_error(csb, "boolean");

	return static_cast<BoolExprNode*>(node);
}

// Parse a value node.
ValueExprNode* PAR_parse_value(thread_db* tdbb, CompilerScratch* csb)
{
	DmlNode* node = PAR_parse_node(tdbb, csb);

	if (node->getKind() != DmlNode::KIND_VALUE)
		PAR_syntax_error(csb, "value");

	return static_cast<ValueExprNode*>(node);
}

// Parse a statement node.
StmtNode* PAR_parse_stmt(thread_db* tdbb, CompilerScratch* csb)
{
	DmlNode* node = PAR_parse_node(tdbb, csb);

	if (node->getKind() != DmlNode::KIND_STATEMENT)
		PAR_syntax_error(csb, "statement");

	return static_cast<StmtNode*>(node);
}

// Parse a BLR node.
DmlNode* PAR_parse_node(thread_db* tdbb, CompilerScratch* csb)
{
	SET_TDBB(tdbb);

	const ULONG blrOffset = csb->csb_blr_reader.getOffset();
	const SSHORT blrOperator = csb->csb_blr_reader.getByte();

	if (blrOperator < 0 || static_cast<FB_SIZE_T>(blrOperator) >= FB_NELEM(blr_parsers))
	{
        // NS: This error string is correct, please do not mangle it again and again.
		// The whole error message is "BLR syntax error: expected %s at offset %d, encountered %d"
		PAR_syntax_error(csb, "valid BLR code");
    }

	// Dispatch on operator type.

	switch (blrOperator)
	{
		case blr_rse:
		case blr_lateral_rse:
		case blr_rs_stream:
		case blr_singular:
		case blr_scrollable:
			csb->csb_blr_reader.seekBackward(1);
			return PAR_rse(tdbb, csb);

		case blr_pid:
		case blr_pid2:
		case blr_procedure:
		case blr_procedure2:
		case blr_procedure3:
		case blr_procedure4:
		case blr_subproc:
		case blr_select_procedure:
		case blr_relation:
		case blr_rid:
		case blr_relation2:
		case blr_rid2:
		case blr_relation3:
		case blr_local_table_id:
		case blr_union:
		case blr_recurse:
		case blr_window:
		case blr_aggregate:
		case blr_table_value_fun:
			csb->csb_blr_reader.seekBackward(1);
			return PAR_parseRecordSource(tdbb, csb);
	}

	if (!blr_parsers[blrOperator])
		PAR_syntax_error(csb, "valid BLR code");

	DmlNode* node = blr_parsers[blrOperator](tdbb, *tdbb->getDefaultPool(), csb, blrOperator);
	setNodeLineColumn(csb, node, blrOffset);

	return node;
}


void PAR_syntax_error(CompilerScratch* csb, const TEXT* string)
{
/**************************************
 *
 *	P A R _ s y n t a x _ e r r o r
 *
 **************************************
 *
 * Functional description
 *	Post a syntax error message.
 *
 **************************************/

	csb->csb_blr_reader.seekBackward(1);

	// BLR syntax error: expected @1 at offset @2, encountered @3
	PAR_error(csb, Arg::Gds(isc_syntaxerr) << Arg::Str(string) <<
										  Arg::Num(csb->csb_blr_reader.getOffset()) <<
										  Arg::Num(csb->csb_blr_reader.peekByte()));
}


void PAR_warning(const Arg::StatusVector& v)
{
/**************************************
 *
 *	P A R _ w a r n i n g
 *
 **************************************
 *
 * Functional description
 *      This is for GBAK so that we can pass warning messages
 *      back to the client.  DO NOT USE this function until we
 *      fully implement warning at the engine level.
 *
 *	We will use the status vector like a warning vector.  What
 *	we are going to do is leave the [1] position of the vector
 *	as 0 so that this will not be treated as an error, and we
 *	will place our warning message in the consecutive positions.
 *	It will be up to the caller to check these positions for
 *	the message.
 *
 **************************************/
	fb_assert(v.value()[0] == isc_arg_warning);
	ERR_post_warning(v);
}


// Parse subroutines.
static void parseSubRoutines(thread_db* tdbb, CompilerScratch* csb)
{
	for (auto& pair : csb->subFunctions)
	{
		const auto node = pair.second;
		Jrd::ContextPoolHolder context(tdbb, &node->subCsb->csb_pool);
		PAR_blr(tdbb, &csb->csb_schema, nullptr, node->blrStart, node->blrLength, nullptr,
			&node->subCsb, nullptr, false, 0);
	}

	for (auto& pair : csb->subProcedures)
	{
		const auto node = pair.second;
		Jrd::ContextPoolHolder context(tdbb, &node->subCsb->csb_pool);
		PAR_blr(tdbb, &csb->csb_schema, nullptr, node->blrStart, node->blrLength, nullptr,
			&node->subCsb, nullptr, false, 0);
	}
}


// Set node line/column for a given blr offset.
static void setNodeLineColumn(CompilerScratch* csb, DmlNode* node, ULONG blrOffset)
{
	FB_SIZE_T pos;

	if (node && csb->csb_dbg_info->blrToSrc.find(blrOffset, pos))
	{
		MapBlrToSrcItem& i = csb->csb_dbg_info->blrToSrc[pos];
		node->line = i.mbs_src_line;
		node->column = i.mbs_src_col;

		if (node->getKind() == DmlNode::KIND_STATEMENT)
			static_cast<StmtNode*>(node)->hasLineColumn = true;
	}
}
