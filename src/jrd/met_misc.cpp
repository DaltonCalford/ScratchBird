/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		met_misc.cpp
 *	DESCRIPTION:	Miscellaneous metadata functions
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

using namespace Jrd;
using namespace ScratchBird;

namespace Jrd {

bool MET_check_schema_exists(thread_db* tdbb, const MetaName& name)
{
/**************************************
 *
 *      M E T _ c h e c k _ s c h e m a _ e x i s t s
 *
 **************************************
 *
 * Functional description
 *      Check if a schema exists in the database.
 *
 **************************************/
	SET_TDBB(tdbb);
	const auto attachment = tdbb->getAttachment();
	
	AutoCacheRequest request(tdbb, irq_l_schema, IRQ_REQUESTS);
	
	// EXE_start/EXE_send/EXE_receive pattern for:
	// FOR SCH IN RDB$SCHEMAS WITH SCH.RDB$SCHEMA_NAME EQ name.c_str()
	EXE_start(tdbb, request, attachment->getSysTransaction());
	EXE_send(tdbb, request, 0, strlen(name.c_str()), name.c_str());
	
	if (EXE_receive(tdbb, request))
	{
		EXE_unwind(tdbb, request);
		return true;
	}
	
	EXE_unwind(tdbb, request);
	return false;
}

int MET_get_linger(thread_db* tdbb)
{
/**************************************
 *
 *	M E T _ g e t _ l i n g e r
 *
 **************************************
 *
 * Functional description
 *      Return linger value for current database
 *
 **************************************/
	SET_TDBB(tdbb);
	Attachment* attachment = tdbb->getAttachment();
	int rc = 0;
	
	AutoCacheRequest request(tdbb, irq_linger, IRQ_REQUESTS);
	
	// EXE_start/EXE_send/EXE_receive pattern for:
	// FOR DAT IN RDB$DATABASE
	EXE_start(tdbb, request, attachment->getSysTransaction());
	
	if (EXE_receive(tdbb, request))
	{
		// Get RDB$LINGER field value
		const dsc* desc = EXE_get_field(tdbb, request, 0); // RDB$LINGER field
		if (desc && !(desc->dsc_flags & DSC_null))
		{
			rc = MOV_get_long(desc, 0);
		}
		EXE_unwind(tdbb, request);
	}
	else
	{
		EXE_unwind(tdbb, request);
	}
	
	return rc;
}

bool MET_get_repl_state(thread_db* tdbb, const QualifiedName& name)
{
/**************************************
 *
 *      M E T _ g e t _ r e p l _ s t a t e
 *
 **************************************
 *
 * Functional description
 *      Return replication state (enabled/disabled)
 *      for either database or given relation.
 *
 **************************************/
	SET_TDBB(tdbb);
	Attachment* const attachment = tdbb->getAttachment();
	bool state = false;
	
	if (name.object.hasData())
	{
		AutoCacheRequest request(tdbb, irq_repl_state, IRQ_REQUESTS);
		
		// EXE_start/EXE_send/EXE_receive pattern for:
		// FOR PTAB IN RDB$PUBLICATION_TABLES 
		// WITH PTAB.RDB$TABLE_SCHEMA_NAME EQ name.schema.c_str() AND
		//      PTAB.RDB$TABLE_NAME EQ name.object.c_str()
		EXE_start(tdbb, request, attachment->getSysTransaction());
		EXE_send(tdbb, request, 0, name.schema.length(), name.schema.c_str());
		EXE_send(tdbb, request, 1, name.object.length(), name.object.c_str());
		
		if (EXE_receive(tdbb, request))
		{
			state = true;
			EXE_unwind(tdbb, request);
		}
		else
		{
			EXE_unwind(tdbb, request);
		}
	}
	else
	{
		// Check database-level replication
		AutoCacheRequest request(tdbb, irq_repl_db_state, IRQ_REQUESTS);
		
		// EXE_start/EXE_send/EXE_receive pattern for:
		// FOR PUB IN RDB$PUBLICATIONS
		EXE_start(tdbb, request, attachment->getSysTransaction());
		
		if (EXE_receive(tdbb, request))
		{
			state = true;
			EXE_unwind(tdbb, request);
		}
		else
		{
			EXE_unwind(tdbb, request);
		}
	}
	
	return state;
}

ScratchBird::string MET_get_schema_hierarchy_path(thread_db* tdbb, const MetaName& schemaName)
{
/**************************************
 *
 *      M E T _ g e t _ s c h e m a _ h i e r a r c h y _ p a t h
 *
 **************************************
 *
 * Functional description
 *      Get the full hierarchical path for a schema.
 *
 **************************************/
	SET_TDBB(tdbb);
	const auto attachment = tdbb->getAttachment();
	
	AutoCacheRequest request(tdbb, irq_schema_path, IRQ_REQUESTS);
	
	// EXE_start/EXE_send/EXE_receive pattern for:
	// FOR SCH IN RDB$SCHEMAS WITH SCH.RDB$SCHEMA_NAME EQ schemaName.c_str()
	EXE_start(tdbb, request, attachment->getSysTransaction());
	EXE_send(tdbb, request, 0, strlen(schemaName.c_str()), schemaName.c_str());
	
	if (EXE_receive(tdbb, request))
	{
		const dsc* desc = EXE_get_field(tdbb, request, 1); // RDB$SCHEMA_PATH field
		if (desc && !(desc->dsc_flags & DSC_null))
		{
			ScratchBird::string result;
			MOV_get_string(desc, &result, 0);
			EXE_unwind(tdbb, request);
			return result;
		}
		EXE_unwind(tdbb, request);
	}
	else
	{
		EXE_unwind(tdbb, request);
	}
	
	return ScratchBird::string(schemaName.c_str());
}

ScratchBird::TriState MET_get_ss_definer(thread_db* tdbb, const MetaName& schemaName)
{
/**************************************
 *
 *	M E T _ g e t _ s s _ d e f i n e r
 *
 **************************************
 *
 * Functional description
 *      Return sql security value for current database
 *
 **************************************/
	SET_TDBB(tdbb);
	Attachment* attachment = tdbb->getAttachment();
	ScratchBird::TriState r;
	
	AutoCacheRequest request(tdbb, irq_dbb_ss_definer, IRQ_REQUESTS);
	
	// EXE_start/EXE_send/EXE_receive pattern for:
	// FOR SCH IN RDB$SCHEMAS CROSS DBB IN RDB$DATABASE
	// WITH SCH.RDB$SCHEMA_NAME EQ schemaName.c_str()
	EXE_start(tdbb, request, attachment->getSysTransaction());
	EXE_send(tdbb, request, 0, strlen(schemaName.c_str()), schemaName.c_str());
	
	if (EXE_receive(tdbb, request))
	{
		// Get SCH.RDB$SQL_SECURITY field
		const dsc* schemaSecDesc = EXE_get_field(tdbb, request, 0);
		if (schemaSecDesc && !(schemaSecDesc->dsc_flags & DSC_null))
		{
			r = (bool)MOV_get_long(schemaSecDesc, 0);
		}
		else
		{
			// Get DBB.RDB$SQL_SECURITY field
			const dsc* dbSecDesc = EXE_get_field(tdbb, request, 1);
			if (dbSecDesc && !(dbSecDesc->dsc_flags & DSC_null))
			{
				r = (bool)MOV_get_long(dbSecDesc, 0);
			}
		}
		EXE_unwind(tdbb, request);
	}
	else
	{
		EXE_unwind(tdbb, request);
	}
	
	return r;
}

void MET_prepare(thread_db* tdbb, jrd_tra* transaction, USHORT length, const UCHAR* msg)
{
/**************************************
 *
 *      M E T _ p r e p a r e
 *
 **************************************
 *
 * Functional description
 *      Post a transaction description to RDB$TRANSACTIONS.
 *
 **************************************/
	SET_TDBB(tdbb);
	Attachment* attachment = tdbb->getAttachment();
	
	AutoCacheRequest request(tdbb, irq_s_trans, IRQ_REQUESTS);
	
	// EXE_start/EXE_send pattern for:
	// STORE X IN RDB$TRANSACTIONS
	EXE_start(tdbb, request, attachment->getSysTransaction());
	
	// Set transaction ID
	EXE_send(tdbb, request, 0, sizeof(SLONG), &transaction->tra_number);
	
	// Set transaction state to LIMBO
	SSHORT state = tra_limbo;
	EXE_send(tdbb, request, 1, sizeof(SSHORT), &state);
	
	// Create and populate blob for transaction description
	blb* blob = blb::create(tdbb, attachment->getSysTransaction(), NULL);
	blob->BLB_put_segment(tdbb, msg, length);
	bid blob_id = blob->BLB_close(tdbb);
	
	// Send blob ID
	EXE_send(tdbb, request, 2, sizeof(bid), &blob_id);
	
	EXE_receive(tdbb, request);
	EXE_unwind(tdbb, request);
}

void MET_store_dependencies(thread_db* tdbb,
						   ScratchBird::Array<CompilerScratch::Dependency>& dependencies,
						   const jrd_rel* dep_rel,
						   const QualifiedName& object_name,
						   int dependency_type,
						   jrd_tra* transaction)
{
/**************************************
 *
 *      M E T _ s t o r e _ d e p e n d e n c i e s
 *
 **************************************
 *
 * Functional description
 *      Store records in RDB$DEPENDENCIES
 *      corresponding to the objects found during
 *      compilation of blr for a trigger, view, etc.
 *
 **************************************/
	SET_TDBB(tdbb);
	Attachment* attachment = tdbb->getAttachment();
	
	AutoCacheRequest request(tdbb, irq_store_deps, IRQ_REQUESTS);
	
	for (const auto& dep : dependencies)
	{
		// EXE_start/EXE_send pattern for:
		// STORE X IN RDB$DEPENDENCIES
		EXE_start(tdbb, request, transaction);
		
		// Dependent object info
		EXE_send(tdbb, request, 0, object_name.schema.length(), object_name.schema.c_str());
		EXE_send(tdbb, request, 1, object_name.object.length(), object_name.object.c_str());
		EXE_send(tdbb, request, 2, sizeof(SSHORT), &dependency_type);
		
		// Referenced object info
		EXE_send(tdbb, request, 3, dep.name.schema.length(), dep.name.schema.c_str());
		EXE_send(tdbb, request, 4, dep.name.object.length(), dep.name.object.c_str());
		EXE_send(tdbb, request, 5, sizeof(SSHORT), &dep.type);
		
		// Field name if applicable
		if (dep.fieldName.hasData())
		{
			EXE_send(tdbb, request, 6, dep.fieldName.length(), dep.fieldName.c_str());
		}
		else
		{
			EXE_send(tdbb, request, 6, 0, NULL); // NULL field name
		}
		
		EXE_receive(tdbb, request);
		EXE_unwind(tdbb, request);
	}
}

} // namespace Jrd