/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		met_triggers.h
 *	DESCRIPTION:	Database trigger metadata management declarations
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

#ifndef JRD_MET_TRIGGERS_H
#define JRD_MET_TRIGGERS_H

#include "../jrd/MetaName.h"
#include "../jrd/QualifiedName.h"
#include "../common/classes/array.h"

namespace Jrd
{
	class thread_db;
	class TrigVector;
}

namespace Jrd {

// Trigger constraint lookup
void MET_lookup_cnstrt_for_trigger(thread_db* tdbb,
								   MetaName& constraint_name,
								   QualifiedName& relation_name,
								   const QualifiedName& trigger_name);

// Trigger message handling
void MET_trigger_msg(thread_db* tdbb, ScratchBird::string& msg, const QualifiedName& name, USHORT number);

// Trigger resource management
void MET_release_trigger(thread_db* tdbb, TrigVector** vector_ptr, const QualifiedName& name);
void MET_release_triggers(thread_db* tdbb, TrigVector** vector_ptr, bool destroy);

} // namespace Jrd

#endif // JRD_MET_TRIGGERS_H