/*
 *	PROGRAM:		ScratchBird interface.
 *	MODULE:			MasterImplementation.h
 *	DESCRIPTION:	Main firebird interface, used to get other interfaces.
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
 *  Copyright (c) 2011 Alex Peshkov <peshkoff at mail.ru>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 *
 *
 */

#ifndef YVALVE_MASTER_IMPLEMENTATION_H
#define YVALVE_MASTER_IMPLEMENTATION_H

#include "firebird.h"
#include "firebird/Interface.h"
#include "../yvalve/YObjects.h"
#include "../yvalve/DistributedTransaction.h"
#include "../common/classes/ImplementHelper.h"

namespace ScratchBird
{
	class Mutex;
}

namespace Why
{
	class MasterImplementation :
		public ScratchBird::AutoIface<ScratchBird::IMasterImpl<MasterImplementation, ScratchBird::CheckStatusWrapper> >
	{
	public:
		static ScratchBird::Static<Dtc> dtc;

	public:
		// IMaster implementation
		ScratchBird::IStatus* getStatus();
		ScratchBird::IProvider* getDispatcher();
		ScratchBird::IPluginManager* getPluginManager();
		ScratchBird::ITimerControl* getTimerControl();
		ScratchBird::IAttachment* registerAttachment(ScratchBird::IProvider* provider,
			ScratchBird::IAttachment* attachment);
		ScratchBird::ITransaction* registerTransaction(ScratchBird::IAttachment* attachment,
			ScratchBird::ITransaction* transaction);
		Dtc* getDtc();
		ScratchBird::IMetadataBuilder* getMetadataBuilder(ScratchBird::CheckStatusWrapper* status, unsigned fieldCount);
		int serverMode(int mode);
		ScratchBird::IUtil* getUtilInterface();
		ScratchBird::IConfigManager* getConfigManager();
		FB_BOOLEAN getProcessExiting();
	};

	// Should be called if process must be terminated wihout any regular shutdown routines
	void abortShutdown();

	void shutdownTimers();

	ScratchBird::Mutex& pauseTimer();

	bool timerThreadStopped();
} // namespace Why

#endif // YVALVE_MASTER_IMPLEMENTATION_H
