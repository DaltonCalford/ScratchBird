/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		met_relations.h
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

#ifndef JRD_MET_RELATIONS_H
#define JRD_MET_RELATIONS_H

#include "../common/classes/MetaName.h"
#include "../common/classes/QualifiedName.h"
#include "../common/dsc.h"

namespace Jrd {

// Forward declarations
class thread_db;
class jrd_rel;
class jrd_fld;
class jrd_tra;
class MemoryPool;
struct FieldInfo;

// Core relation management functions
jrd_rel* MET_lookup_relation(thread_db* tdbb, const QualifiedName& name);
jrd_rel* MET_lookup_relation_id(thread_db* tdbb, SLONG id, bool return_deleted);
jrd_rel* MET_relation(thread_db* tdbb, USHORT id);
void MET_post_existence(thread_db* tdbb, jrd_rel* relation);
void MET_release_existence(thread_db* tdbb, jrd_rel* relation);
void MET_scan_relation(thread_db* tdbb, jrd_rel* relation);
void MET_scan_partners(thread_db* tdbb, jrd_rel* relation);
void MET_revoke(thread_db* tdbb, jrd_tra* transaction, const QualifiedName& relation,
	const QualifiedName& revokee, const string& privilege);

// Field management functions
jrd_fld* MET_get_field(const jrd_rel* relation, USHORT id);
int MET_lookup_field(thread_db* tdbb, jrd_rel* relation, const MetaName& name);
MetaName MET_get_relation_field(thread_db* tdbb, MemoryPool& csbPool, const QualifiedName& relationName,
	const MetaName& fieldName, dsc* desc, FieldInfo* fieldInfo = NULL);

// Helper functions
ULONG MET_get_rel_flags_from_TYPE(USHORT type);

} // namespace Jrd

#endif // JRD_MET_RELATIONS_H