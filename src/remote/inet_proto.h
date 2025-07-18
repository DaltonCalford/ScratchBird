/*
 *	PROGRAM:	JRD Remote Interface/Server
 *	MODULE:		inet_proto.h
 *	DESCRIPTION:	Prototpe header file for inet.cpp
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

#ifndef REMOTE_INET_PROTO_H
#define REMOTE_INET_PROTO_H

#include "../common/classes/fb_string.h"
#include "../common/classes/RefCounted.h"
#include "../common/config/config.h"

namespace ScratchBird
{
	class ClumpletReader;
}

rem_port*	INET_analyze(ClntAuthBlock*, const ScratchBird::PathName&, const TEXT*,
						 bool, ScratchBird::ClumpletReader&, ScratchBird::RefPtr<const ScratchBird::Config>*,
						 const ScratchBird::PathName*, ScratchBird::ICryptKeyCallback*, int af = AF_UNSPEC);
rem_port*	INET_connect(const TEXT*, struct packet*, USHORT, ScratchBird::ClumpletReader*,
						 ScratchBird::RefPtr<const ScratchBird::Config>*, int af = AF_UNSPEC);
rem_port*	INET_reconnect(SOCKET);
rem_port*	INET_server(SOCKET);
void		setStopMainThread(FPTR_INT func);

#endif // REMOTE_INET_PROTO_H
