/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		met_triggers.cpp
 *	DESCRIPTION:	Database trigger metadata management
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
#include "met_triggers.h"

using namespace Jrd;
using namespace ScratchBird;

namespace Jrd {

void MET_lookup_cnstrt_for_trigger(thread_db* tdbb,
								   MetaName& constraint_name,
								   QualifiedName& relation_name,
								   const QualifiedName& trigger_name)
{
/**************************************
 *
 *      M E T _ l o o k u p _ c n s t r t _ f o r _ t r i g g e r
 *
 **************************************
 *
 * Functional description
 *      Lookup constraint name from trigger name, if one exists.
 *      constraint_name and relation_name are output parameters.
 *
 **************************************/
	SET_TDBB(tdbb);
	Attachment* attachment = tdbb->getAttachment();
	
	constraint_name = "";
	relation_name.clear();
	
	AutoCacheRequest request(tdbb, irq_l_check, IRQ_REQUESTS);
	
	// EXE_start/EXE_send/EXE_receive pattern for:
	// FOR RC IN RDB$RELATION_CONSTRAINTS
	//     WITH RC.RDB$SCHEMA_NAME EQ trigger_name.schema.c_str() AND
	//          RC.RDB$TRIGGER_NAME EQ trigger_name.object.c_str()
	EXE_start(tdbb, request, attachment->getSysTransaction());
	EXE_send(tdbb, request, 0, trigger_name.schema.length(), trigger_name.schema.c_str());
	EXE_send(tdbb, request, 1, trigger_name.object.length(), trigger_name.object.c_str());
	
	if (EXE_receive(tdbb, request))
	{
		// Get constraint name
		const dsc* cnstrDesc = EXE_get_field(tdbb, request, 0); // RDB$CONSTRAINT_NAME
		if (cnstrDesc && !(cnstrDesc->dsc_flags & DSC_null))
		{
			ScratchBird::string tempName;
			MOV_get_string(cnstrDesc, &tempName, 0);
			constraint_name = tempName.c_str();
		}
		
		// Get relation name
		const dsc* relNameDesc = EXE_get_field(tdbb, request, 1); // RDB$RELATION_NAME
		const dsc* relSchemaDesc = EXE_get_field(tdbb, request, 2); // RDB$SCHEMA_NAME
		if (relNameDesc && !(relNameDesc->dsc_flags & DSC_null))
		{
			ScratchBird::string tempRelName, tempRelSchema;
			MOV_get_string(relNameDesc, &tempRelName, 0);
			if (relSchemaDesc && !(relSchemaDesc->dsc_flags & DSC_null))
			{
				MOV_get_string(relSchemaDesc, &tempRelSchema, 0);
			}
			relation_name = QualifiedName(tempRelName.c_str(), tempRelSchema.c_str());
		}
		
		EXE_unwind(tdbb, request);
	}
	else
	{
		EXE_unwind(tdbb, request);
	}
}

void MET_trigger_msg(thread_db* tdbb, ScratchBird::string& msg, const QualifiedName& name, USHORT number)
{
/**************************************
 *
 *      M E T _ t r i g g e r _ m s g
 *
 **************************************
 *
 * Functional description
 *      Look up trigger message using trigger and abort code.
 *
 **************************************/
	SET_TDBB(tdbb);
	Attachment* attachment = tdbb->getAttachment();
	
	AutoCacheRequest request(tdbb, irq_s_msgs, IRQ_REQUESTS);
	
	// EXE_start/EXE_send/EXE_receive pattern for:
	// FOR MSG IN RDB$TRIGGER_MESSAGES
	//     WITH MSG.RDB$SCHEMA_NAME EQ name.schema.c_str() AND
	//          MSG.RDB$TRIGGER_NAME EQ name.object.c_str() AND
	//          MSG.RDB$MESSAGE_NUMBER EQ number
	EXE_start(tdbb, request, attachment->getSysTransaction());
	EXE_send(tdbb, request, 0, name.schema.length(), name.schema.c_str());
	EXE_send(tdbb, request, 1, name.object.length(), name.object.c_str());
	EXE_send(tdbb, request, 2, sizeof(USHORT), &number);
	
	if (EXE_receive(tdbb, request))
	{
		// Get message text
		const dsc* msgDesc = EXE_get_field(tdbb, request, 0); // RDB$MESSAGE
		if (msgDesc && !(msgDesc->dsc_flags & DSC_null))
		{
			MOV_get_string(msgDesc, &msg, 0);
		}
		EXE_unwind(tdbb, request);
	}
	else
	{
		EXE_unwind(tdbb, request);
	}
}

void MET_release_trigger(thread_db* tdbb, TrigVector** vector_ptr, const QualifiedName& name)
{
/***********************************************
 *
 *      M E T _ r e l e a s e _ t r i g g e r
 *
 ***********************************************
 *
 * Functional description
 *      Release a specified trigger.
 *      If trigger are still active let someone
 *      else do the work.
 *
 **************************************/
	if (!*vector_ptr)
		return;
	
	TrigVector& vector = **vector_ptr;
	SET_TDBB(tdbb);
	
	for (FB_SIZE_T i = 0; i < vector.getCount(); ++i)
	{
		if (vector[i].name == name)
		{
			Statement* stmt = vector[i].statement;
			if (stmt)
			{
				stmt->release(tdbb);
				vector[i].statement = nullptr;
			}
			
			// Remove the trigger from the vector
			vector.remove(i);
			break;
		}
	}
	
	// If vector is now empty, delete it
	if (vector.getCount() == 0)
	{
		delete *vector_ptr;
		*vector_ptr = nullptr;
	}
}

void MET_release_triggers(thread_db* tdbb, TrigVector** vector_ptr, bool destroy)
{
/***********************************************
 *
 *      M E T _ r e l e a s e _ t r i g g e r s
 *
 ***********************************************
 *
 * Functional description
 *      Release a possibly null vector of triggers.
 *      If triggers are still active let someone
 *      else do the work.
 *
 **************************************/
	TrigVector* vector = *vector_ptr;
	if (!vector)
		return;
	
	if (!destroy)
	{
		vector->decompile(tdbb);
		return;
	}
	
	*vector_ptr = nullptr;
	
	SET_TDBB(tdbb);
	
	// Release all statements in the vector
	for (FB_SIZE_T i = 0; i < vector->getCount(); ++i)
	{
		Statement* stmt = (*vector)[i].statement;
		if (stmt)
		{
			stmt->release(tdbb);
		}
	}
	
	// Delete the vector
	delete vector;
}

} // namespace Jrd