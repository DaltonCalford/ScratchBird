/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		met_misc.h
 *	DESCRIPTION:	Miscellaneous metadata functions declarations
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
 * Contributor(s): _______________________________________.
 *
 * 2025.07.31 - Split from met.epp for modular architecture
 */

#ifndef JRD_MET_MISC_H
#define JRD_MET_MISC_H

#include "../jrd/MetaName.h"
#include "../jrd/QualifiedName.h"
#include "../common/classes/array.h"

namespace Jrd
{
	class thread_db;
	class jrd_tra;
	class jrd_rel;
	class CompilerScratch;
}

namespace Jrd {

// Schema existence check
bool MET_check_schema_exists(thread_db* tdbb, const MetaName& name);

// Database configuration functions
int MET_get_linger(thread_db* tdbb);
bool MET_get_repl_state(thread_db* tdbb, const QualifiedName& name);
ScratchBird::string MET_get_schema_hierarchy_path(thread_db* tdbb, const MetaName& schemaName);
ScratchBird::TriState MET_get_ss_definer(thread_db* tdbb, const MetaName& schemaName);

// Transaction and dependency management
void MET_prepare(thread_db* tdbb, jrd_tra* transaction, USHORT length, const UCHAR* msg);
void MET_store_dependencies(thread_db* tdbb,
						   ScratchBird::Array<CompilerScratch::Dependency>& dependencies,
						   const jrd_rel* dep_rel,
						   const QualifiedName& object_name,
						   int dependency_type,
						   jrd_tra* transaction);

} // namespace Jrd

#endif // JRD_MET_MISC_H