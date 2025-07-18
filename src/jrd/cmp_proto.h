/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		cmp_proto.h
 *	DESCRIPTION:	Prototype header file for cmp.cpp
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
 */

#ifndef JRD_CMP_PROTO_H
#define JRD_CMP_PROTO_H

#include "../jrd/req.h"
// req.h includes exe.h => Jrd::CompilerScratch and Jrd::CompilerScratch::csb_repeat.
#include "../jrd/scl.h"

namespace Jrd
{
	class RelationSourceNode;
}

StreamType* CMP_alloc_map(Jrd::thread_db*, Jrd::CompilerScratch*, StreamType stream);
Jrd::ValueExprNode* CMP_clone_node_opt(Jrd::thread_db*, Jrd::CompilerScratch*, Jrd::ValueExprNode*);
Jrd::BoolExprNode* CMP_clone_node_opt(Jrd::thread_db*, Jrd::CompilerScratch*, Jrd::BoolExprNode*);
Jrd::ValueExprNode* CMP_clone_node(Jrd::thread_db*, Jrd::CompilerScratch*, Jrd::ValueExprNode*);
Jrd::Statement* CMP_compile(Jrd::thread_db* tdbb, const UCHAR* blr, ULONG blrLength, bool internalFlag,
	ULONG dbginfoLength, const UCHAR* dbginfo);
Jrd::Request* CMP_compile_request(Jrd::thread_db*, const UCHAR* blr, ULONG blrLength, bool internalFlag);
Jrd::CompilerScratch::csb_repeat* CMP_csb_element(Jrd::CompilerScratch*, StreamType element);
const Jrd::Format* CMP_format(Jrd::thread_db*, Jrd::CompilerScratch*, StreamType);
Jrd::IndexLock* CMP_get_index_lock(Jrd::thread_db*, Jrd::jrd_rel*, USHORT);
Jrd::Request* CMP_make_request(Jrd::thread_db*, Jrd::CompilerScratch*, bool);
Jrd::ItemInfo* CMP_pass2_validation(Jrd::thread_db*, Jrd::CompilerScratch*, const Jrd::Item&);

bool CMP_procedure_arguments(
	Jrd::thread_db* tdbb,
	Jrd::CompilerScratch* csb,
	Jrd::Routine* routine,
	bool isInput,
	USHORT argCount,
	ScratchBird::ObjectsArray<Jrd::MetaName>* argNames,
	NestConst<Jrd::ValueListNode>& sources,
	NestConst<Jrd::ValueListNode>& targets,
	NestConst<Jrd::MessageNode>& message,
	ScratchBird::Arg::StatusVector& mismatchStatus);

void CMP_post_access(Jrd::thread_db*, Jrd::CompilerScratch*, const Jrd::MetaName&, SLONG ssRelationId,
					 Jrd::SecurityClass::flags_t, ObjectType obj_type,
					 const Jrd::QualifiedName&,
					 const Jrd::MetaName& = {});

void CMP_post_procedure_access(Jrd::thread_db*, Jrd::CompilerScratch*, Jrd::jrd_prc*);
void CMP_post_resource(Jrd::ResourceList*, void*, Jrd::Resource::rsc_s, USHORT);
Jrd::RecordSource* CMP_post_rse(Jrd::thread_db*, Jrd::CompilerScratch*, Jrd::RseNode*);
void CMP_release(Jrd::thread_db*, Jrd::Request*);

#endif // JRD_CMP_PROTO_H
