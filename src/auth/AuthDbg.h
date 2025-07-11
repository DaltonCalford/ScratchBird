/*
 *	PROGRAM:		ScratchBird authentication
 *	MODULE:			Auth.h
 *	DESCRIPTION:	Implementation of interfaces, passed to plugins
 *					Plugins loader
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

#ifndef FB_AUTHDBG_H
#define FB_AUTHDBG_H

#define AUTH_DEBUG

#ifdef AUTH_DEBUG

#include "firebird/Interface.h"
#include "../common/classes/ImplementHelper.h"
#include "../common/classes/ClumpletWriter.h"
#include "../common/classes/init.h"
#include "../common/classes/array.h"
#include "../common/classes/fb_string.h"

namespace Auth {

// The idea of debug plugin is to send some data from server to client,
// modify them on client and return result (which becomes login name) to the server

class DebugServer final :
	public ScratchBird::StdPlugin<ScratchBird::IServerImpl<DebugServer, ScratchBird::CheckStatusWrapper> >
{
public:
	explicit DebugServer(ScratchBird::IPluginConfig*);

    int authenticate(ScratchBird::CheckStatusWrapper* status, ScratchBird::IServerBlock* sBlock,
    				 ScratchBird::IWriter* writerInterface);
	void setDbCryptCallback(ScratchBird::CheckStatusWrapper*, ScratchBird::ICryptKeyCallback*);

private:
	ScratchBird::string str;
	ScratchBird::RefPtr<ScratchBird::IConfig> config;
};

class DebugClient final :
	public ScratchBird::StdPlugin<ScratchBird::IClientImpl<DebugClient, ScratchBird::CheckStatusWrapper> >
{
public:
	DebugClient(ScratchBird::IPluginConfig*);

    int authenticate(ScratchBird::CheckStatusWrapper* status, ScratchBird::IClientBlock* sBlock);

private:
	ScratchBird::string str;
};

} // namespace Auth

#endif // AUTH_DEBUG

#endif // FB_AUTHDBG_H
