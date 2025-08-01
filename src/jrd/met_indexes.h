/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		met_indexes.h
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

#ifndef JRD_MET_INDEXES_H
#define JRD_MET_INDEXES_H

#include "firebird.h"

namespace Jrd {

// Forward declarations
class thread_db;
class jrd_rel;
class QualifiedName;
class MetaName;
struct index_desc;
struct bid;

// Index status enumeration
enum IndexStatus
{
	MET_object_active,
	MET_object_deferred_active,
	MET_object_inactive,
	MET_object_unknown
};

// Index management functions

// Look up index name from constraint name
void MET_lookup_index_for_cnstrt(thread_db* tdbb,
								QualifiedName& index_name,
								const QualifiedName& constraint_name);

// Look up constraint name from index name
void MET_lookup_cnstrt_for_index(thread_db* tdbb,
								MetaName& constraint_name,
								const QualifiedName& index_name);

// Look up index name from relation and index number
void MET_lookup_index(thread_db* tdbb,
					 QualifiedName& index_name,
					 const QualifiedName& relation_name,
					 USHORT number);

// Look up index ID from index name
SLONG MET_lookup_index_name(thread_db* tdbb,
						   const QualifiedName& index_name,
						   SLONG* relation_id,
						   IndexStatus* status);

// Look up index condition information
void MET_lookup_index_condition(thread_db* tdbb,
							   jrd_rel* relation,
							   index_desc* idx);

// Look up index expression information
void MET_lookup_index_expression(thread_db* tdbb,
								jrd_rel* relation,
								index_desc* idx);

// Look up index expression and condition blob IDs
bool MET_lookup_index_expr_cond_blr(thread_db* tdbb,
								   const QualifiedName& index_name,
								   bid& expr_blob_id,
								   bid& cond_blob_id);

// Find partner index participating in foreign key relationship
bool MET_lookup_partner(thread_db* tdbb,
					   jrd_rel* relation,
					   index_desc* idx,
					   const QualifiedName& index_name);

// Scan foreign key partners (internal helper function)
void scan_partners(thread_db* tdbb, jrd_rel* relation);

} // namespace Jrd

#endif // JRD_MET_INDEXES_H