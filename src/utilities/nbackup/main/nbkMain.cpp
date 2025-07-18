/*
 *	PROGRAM:		ScratchBird utilities
 *	MODULE:			nbkMain.cpp
 *	DESCRIPTION:	Proxy for real nbackup main function
 *
 *  The contents of this file are subject to the Initial
 *  Developer's Public License Version 1.0 (the "License");
 *  you may not use this file except in compliance with the
 *  License. You may obtain a copy of the License at
 *  http://www.ibphoenix.com/main.nfs?a=ibphoenix&page=ibp_idpl.
 *
 *  Software distributed under the License is distributed AS IS,
 *  WITHOUT WARRANTY OF ANY KIND, either express or implied.
 *  See the License for the specific language governing rights
 *  and limitations under the License.
 *
 *  The Original Code was created by Alex Peshkov
 *  for the ScratchBird Open Source RDBMS project.
 *
 *  Copyright (c) 2008 Alex Peshkov <peshkoff at mail dot ru>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 *
 *
 */

#include "firebird.h"
#include "../utilities/nbackup/nbk_proto.h"
#include "../common/classes/auto.h"

#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif


static void atexit_fb_shutdown()
{
	fb_shutdown(0, fb_shutrsn_app_stopped);
}

int CLIB_ROUTINE main(int argc, char* argv[])
{
/**************************************
 *
 *	m a i n
 *
 **************************************
 *
 * Functional description
 *	Invoke real nbackup main function
 *
 **************************************/
#ifdef HAVE_LOCALE_H
	// Pick up the system locale to allow SYSTEM<->UTF8 conversions
	setlocale(LC_CTYPE, "");
#endif
	atexit(&atexit_fb_shutdown);

	try
	{
		ScratchBird::AutoPtr<ScratchBird::UtilSvc> uSvc(ScratchBird::UtilSvc::createStandalone(argc, argv));
 		nbackup(uSvc);
	}
	catch (const ScratchBird::Exception&)
	{
		// should be already printed, no need to print once more here
		return FINI_ERROR;
	}

	return FINI_OK;
}
