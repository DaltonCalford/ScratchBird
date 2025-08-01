/*
 *	PROGRAM:	JRD Access Method  
 *	MODULE:		met_hierarchical_schemas.h
 *	DESCRIPTION:	Hierarchical Schema Support Functions
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
 * ScratchBird specific hierarchical schema functionality
 */

#ifndef JRD_MET_HIERARCHICAL_SCHEMAS_H
#define JRD_MET_HIERARCHICAL_SCHEMAS_H

#include "../jrd/jrd.h"
#include "../jrd/tra.h"
#include "../common/classes/array.h"
#include "../dsql/Nodes.h"

namespace Jrd {

// Forward declarations
class thread_db;
class QualifiedName;
class MetaName;

// Hierarchical schema existence check with caching
bool MET_check_hierarchical_schema_exists(Jrd::thread_db* tdbb, const Jrd::MetaName& schemaName, const ScratchBird::string& fullPath);

// Administrative functions for hierarchical schema management
ScratchBird::ObjectsArray<Jrd::QualifiedName>* MET_get_schema_children(Jrd::thread_db* tdbb, const Jrd::MetaName& parentSchema);

// Get full hierarchical path for a schema name
ScratchBird::string MET_get_schema_hierarchy_path(Jrd::thread_db* tdbb, const Jrd::MetaName& schemaName);

// Get schema nesting level
SSHORT MET_get_schema_level(Jrd::thread_db* tdbb, const Jrd::MetaName& schemaName);

// Validate integrity of schema hierarchy
bool MET_validate_schema_hierarchy_integrity(Jrd::thread_db* tdbb);

// Cache invalidation for schema changes
void MET_invalidate_schema_cache(Jrd::thread_db* tdbb);

} // namespace Jrd

#endif // JRD_MET_HIERARCHICAL_SCHEMAS_H