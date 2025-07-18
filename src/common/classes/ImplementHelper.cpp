/*
 *	PROGRAM:		ScratchBird interface.
 *	MODULE:			ImplementHelper.cpp
 *	DESCRIPTION:	Tools to help create interfaces.
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
 *  Copyright (c) 2010 Alex Peshkov <peshkoff at mail.ru>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 *
 *
 */

#include "firebird.h"
#include "../common/classes/fb_tls.h"
#include "../common/classes/ImplementHelper.h"
#include "../common/status.h"

namespace
{
ScratchBird::IMaster* cached = NULL;

#ifdef NEVERDEF
typedef ScratchBird::ReferenceCounterDebugger* ReferenceCounterDebuggerPtr;
TLS_DECLARE(ReferenceCounterDebuggerPtr*, debugArray);
#endif // DEV_BUILD

ScratchBird::UnloadDetector myModule;
}

namespace ScratchBird
{

UnloadDetectorHelper* getUnloadDetector()
{
	return &myModule;
}

void CachedMasterInterface::set(IMaster* master)
{
	fb_assert(master);
	fb_assert(!cached);
	cached = master;
}

IMaster* CachedMasterInterface::getMasterInterface()
{
	if (!cached)
	{
		cached = fb_get_master_interface();
	}
	return cached;
}

unsigned int ConfigKeys::getKey(IScratchBirdConf* config, const char* keyName)
{
	FbLocalStatus status;
	unsigned int version = config->getVersion(&status) & 0xFFFF0000;
	for (const_iterator itr = this->begin(); itr != this->end(); ++itr)
	{
		if (((*itr) & 0xFFFF0000) == version)
			return *itr;
	}

	unsigned int secDbKey = config->getKey(keyName);
	if (secDbKey != INVALID_KEY)
		this->push(secDbKey);

	return secDbKey;
}


#ifdef NOT_USED_OR_REPLACED
class IDebug
{
public:
	virtual void push(ReferenceCounterDebugger* debugger)
	{
		ReferenceCounterDebuggerPtr* d = TLS_GET(debugArray);
		if (!d)
		{
			d = FB_NEW_POOL(*getDefaultMemoryPool()) ReferenceCounterDebuggerPtr[MaxDebugEvent];
			memset(d, 0, sizeof(ReferenceCounterDebuggerPtr) * MaxDebugEvent);
			TLS_SET(debugArray, d);
		}

		debugger->rcd_prev = d[debugger->rcd_code];
		d[debugger->rcd_code] = debugger;
	}

	virtual void pop(ReferenceCounterDebugger* debugger)
	{
		ReferenceCounterDebuggerPtr* d = TLS_GET(debugArray);
		fb_assert(d);
		fb_assert(debugger == d[debugger->rcd_code]);
		d[debugger->rcd_code] = d[debugger->rcd_code]->rcd_prev;
	}

	virtual ReferenceCounterDebugger* get(DebugEvent code)
	{
		ReferenceCounterDebuggerPtr* d = TLS_GET(debugArray);
		if (!d)
			return NULL;
		return d[code];
	}
};

ReferenceCounterDebugger::ReferenceCounterDebugger(DebugEvent code, const char* p)
	: rcd_point(p), rcd_prev(NULL), rcd_code(code)
{
	IDebug* d = MasterInterfacePtr()->getDebug();
	if (d)
		d->push(this);
}

ReferenceCounterDebugger::~ReferenceCounterDebugger()
{
	IDebug* d = MasterInterfacePtr()->getDebug();
	if (d)
		d->pop(this);
}

ReferenceCounterDebugger* ReferenceCounterDebugger::get(DebugEvent code)
{
	IDebug* d = MasterInterfacePtr()->getDebug();
	return d ? d->get(code) : NULL;
}

IDebug* getImpDebug()
{
	static IDebug impDebug;
	return &impDebug;
}
#endif //DEV_BUILD

}
