/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		met_shadows.cpp
 *	DESCRIPTION:	Shadow/backup file metadata management
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

#include "scratchbird.h"
#include <stdio.h>
#include <string.h>

#include "../jrd/jrd.h"
#include "../jrd/req.h"
#include "../jrd/exe.h"
#include "../jrd/met.h"
#include "../jrd/sdw.h"
#include "../jrd/flags.h"
#include "../jrd/os/pio.h"
#include "../jrd/tra.h"
#include "../jrd/align.h"
#include "../common/gdsassert.h"
#include "../jrd/exe_proto.h"
#include "../jrd/sdw_proto.h"
#include "../jrd/os/pio_proto.h"
#include "met_shadows.h"

using namespace Jrd;
using namespace ScratchBird;

void MET_activate_shadow(Jrd::thread_db* tdbb)
{
/**************************************
 *
 *      M E T _ a c t i v a t e _ s h a d o w
 *
 **************************************
 *
 * Functional description
 *      Activate the current database, which presumably
 *      was formerly a shadow, by deleting all records
 *      corresponding to the shadow that this database
 *      represents.
 *      Get rid of write ahead log for the activated shadow.
 *
 **************************************/
	SET_TDBB(tdbb);
	Attachment* attachment = tdbb->getAttachment();
	Database* dbb = tdbb->getDatabase();

	// Erase any secondary files of the primary database of the shadow being activated.
	{
		AutoCacheRequest request(tdbb, irq_activate_shadow1, IRQ_REQUESTS);

		// Execute: FOR X IN RDB$FILES WITH X.RDB$SHADOW_NUMBER NOT MISSING AND X.RDB$SHADOW_NUMBER EQ 0
		EXE_start(tdbb, request.getRequest(), attachment->getSysTransaction());
		
		// Set up message parameters for shadow number = 0
		USHORT shadow_num = 0;
		EXE_send(tdbb, request.getRequest(), 0, sizeof(shadow_num), &shadow_num);

		while (EXE_receive(tdbb, request.getRequest(), 1))
		{
			// Delete the record
			EXE_send(tdbb, request.getRequest(), 2, 0, nullptr);
		}
	}

	PageSpace* pageSpace = dbb->dbb_page_manager.findPageSpace(DB_PAGE_SPACE);
	const char* dbb_file_name = pageSpace->file->fil_string;

	// Go through files looking for any that expand to the current database name
	SCHAR expanded_name[MAXPATHLEN];
	{
		AutoCacheRequest request(tdbb, irq_activate_shadow2, IRQ_REQUESTS);

		// Execute: FOR X IN RDB$FILES WITH X.RDB$SHADOW_NUMBER NOT MISSING AND X.RDB$SHADOW_NUMBER NE 0
		EXE_start(tdbb, request.getRequest(), attachment->getSysTransaction());
		
		while (EXE_receive(tdbb, request.getRequest(), 1))
		{
			char file_name[MAXPATHLEN];
			USHORT current_shadow_num;
			
			// Receive file name and shadow number
			EXE_receive(tdbb, request.getRequest(), 1, sizeof(file_name), file_name);
			EXE_receive(tdbb, request.getRequest(), 1, sizeof(current_shadow_num), &current_shadow_num);

			PIO_expand(file_name, (USHORT)strlen(file_name), expanded_name, sizeof(expanded_name));

			if (!strcmp(expanded_name, dbb_file_name))
			{
				// Update all files with this shadow number to shadow number 0
				{
					AutoCacheRequest request2(tdbb, irq_activate_shadow3, IRQ_REQUESTS);
					
					EXE_start(tdbb, request2.getRequest(), attachment->getSysTransaction());
					EXE_send(tdbb, request2.getRequest(), 0, sizeof(current_shadow_num), &current_shadow_num);

					while (EXE_receive(tdbb, request2.getRequest(), 1))
					{
						// MODIFY Y: Y.RDB$SHADOW_NUMBER = 0
						USHORT zero_shadow = 0;
						EXE_send(tdbb, request2.getRequest(), 2, sizeof(zero_shadow), &zero_shadow);
					}
				}

				// Delete the current record
				EXE_send(tdbb, request.getRequest(), 2, 0, nullptr);
			}
		}
	}
}


void MET_delete_shadow(Jrd::thread_db* tdbb, USHORT shadow_number)
{
/**************************************
 *
 *      M E T _ d e l e t e _ s h a d o w
 *
 **************************************
 *
 * Functional description
 *      When any of the shadows in RDB$FILES for a particular
 *      shadow are deleted, stop shadowing to that file and
 *      remove all other files from the same shadow.
 *
 **************************************/
	SET_TDBB(tdbb);
	Attachment* attachment = tdbb->getAttachment();
	Database* dbb = tdbb->getDatabase();

	{
		AutoCacheRequest request(tdbb, irq_delete_shadow, IRQ_REQUESTS);

		// Execute: FOR X IN RDB$FILES WITH X.RDB$SHADOW_NUMBER EQ shadow_number
		EXE_start(tdbb, request.getRequest(), attachment->getSysTransaction());
		EXE_send(tdbb, request.getRequest(), 0, sizeof(shadow_number), &shadow_number);

		while (EXE_receive(tdbb, request.getRequest(), 1))
		{
			// Delete the record
			EXE_send(tdbb, request.getRequest(), 2, 0, nullptr);
		}
	}

	for (Shadow* shadow = dbb->dbb_shadow; shadow; shadow = shadow->sdw_next)
	{
		if (shadow->sdw_number == shadow_number) {
			shadow->sdw_flags |= SDW_shutdown;
		}
	}

	// notify other processes to check for shadow deletion
	if (SDW_lck_update(tdbb, 0))
		SDW_notify(tdbb);
}


void MET_get_shadow_files(Jrd::thread_db* tdbb, bool delete_files)
{
/**************************************
 *
 *      M E T _ g e t _ s h a d o w _ f i l e s
 *
 **************************************
 *
 * Functional description
 *      Check the shadows found in the database against
 *      our in-memory list: if any new shadow files have
 *      been defined since the last time we looked, start
 *      shadowing to them; if any have been deleted, stop
 *      shadowing to them.
 *
 **************************************/
	SET_TDBB(tdbb);
	Attachment* attachment = tdbb->getAttachment();
	Database* dbb = tdbb->getDatabase();

	{
		AutoCacheRequest request(tdbb, irq_r_shadow_files, IRQ_REQUESTS);

		// Execute: FOR X IN RDB$FILES WITH X.RDB$SHADOW_NUMBER NOT MISSING AND X.RDB$SHADOW_NUMBER NE 0
		EXE_start(tdbb, request.getRequest(), attachment->getSysTransaction());

		while (EXE_receive(tdbb, request.getRequest(), 1))
		{
			char file_name[MAXPATHLEN];
			USHORT file_flags;
			USHORT shadow_num;

			// Receive file information
			EXE_receive(tdbb, request.getRequest(), 1, sizeof(file_name), file_name);
			EXE_receive(tdbb, request.getRequest(), 1, sizeof(file_flags), &file_flags);
			EXE_receive(tdbb, request.getRequest(), 1, sizeof(shadow_num), &shadow_num);

			if ((file_flags & FILE_shadow) && !(file_flags & FILE_inactive))
			{
				const USHORT current_file_flags = file_flags;
				SDW_start(tdbb, file_name, shadow_num, current_file_flags, delete_files);

				// If the shadow exists, mark the appropriate shadow
				// block as found for the purposes of this routine;
				// if the shadow was conditional and is no longer, note it
				for (Shadow* shadow = dbb->dbb_shadow; shadow; shadow = shadow->sdw_next)
				{
					if ((shadow->sdw_number == shadow_num) && !(shadow->sdw_flags & SDW_IGNORE))
					{
						shadow->sdw_flags |= SDW_found;
						if (!(current_file_flags & FILE_conditional)) {
							shadow->sdw_flags &= ~SDW_conditional;
						}
						break;
					}
				}
			}
		}
	}

	// if any current shadows were not defined in database, mark
	// them to be shutdown since they don't exist anymore
	for (Shadow* shadow = dbb->dbb_shadow; shadow; shadow = shadow->sdw_next)
	{
		if (!(shadow->sdw_flags & SDW_found))
			shadow->sdw_flags |= SDW_shutdown;
		else
			shadow->sdw_flags &= ~SDW_found;
	}

	SDW_check(tdbb);
}


void MET_update_shadow(Jrd::thread_db* tdbb, Jrd::Shadow* shadow, USHORT file_flags)
{
/**************************************
 *
 *      M E T _ u p d a t e _ s h a d o w
 *
 **************************************
 *
 * Functional description
 *      Update the stored file flags for the specified shadow.
 *
 **************************************/
	SET_TDBB(tdbb);
	Attachment* attachment = tdbb->getAttachment();

	{
		AutoCacheRequest request(tdbb, irq_update_shadow, IRQ_REQUESTS);

		// Execute: FOR FIL IN RDB$FILES WITH FIL.RDB$SHADOW_NUMBER EQ shadow->sdw_number
		EXE_start(tdbb, request.getRequest(), attachment->getSysTransaction());
		EXE_send(tdbb, request.getRequest(), 0, sizeof(shadow->sdw_number), &shadow->sdw_number);

		while (EXE_receive(tdbb, request.getRequest(), 1))
		{
			// MODIFY FIL: FIL.RDB$FILE_FLAGS = file_flags
			EXE_send(tdbb, request.getRequest(), 2, sizeof(file_flags), &file_flags);
		}
	}
}