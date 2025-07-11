/*
 *	PROGRAM:		ScratchBird authentication
 *	MODULE:			AuthGssapi.h
 *	DESCRIPTION:	Linux GSS-API trusted authentication
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
 *  The Original Code was created by DCal Ford
 *  for the ScratchBird Open Source RDBMS project.
 *
 *  Copyright (c) 2024 DCal Ford
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 *
 */

#ifndef AUTH_GSSAPI_H
#define AUTH_GSSAPI_H

#include <firebird.h>

#ifdef TRUSTED_AUTH_GSSAPI

#include "../common/classes/fb_string.h"
#include "../common/classes/array.h"
#include "../common/classes/ImplementHelper.h"
#include <ibase.h>
#include "firebird/Interface.h"
#include "../common/classes/objects_array.h"

#include <gssapi/gssapi.h>
#include <gssapi/gssapi_krb5.h>
#include <string.h>
#include <stdio.h>

namespace Auth {

class AuthGssapi
{
public:
	typedef ScratchBird::ObjectsArray<ScratchBird::string> GroupsList;
	typedef ScratchBird::UCharBuffer Key;

private:
	enum {BUFSIZE = 4096};

	// GSS-API context and credentials
	gss_ctx_id_t context;
	gss_cred_id_t server_creds;
	gss_name_t client_name;
	bool context_established;
	bool has_credentials;
	
	// Client information
	ScratchBird::string client_principal;
	bool is_admin;
	GroupsList group_names;
	Key session_key;

	// Internal helper methods
	bool acquireCredentials();
	bool extractClientInfo();
	bool checkAdminPrivilege();
	void cleanupGssContext();
	ScratchBird::string gssNameToString(gss_name_t name);
	ScratchBird::string convertPrincipalFormat(const ScratchBird::string& principal);

public:
	typedef ScratchBird::Array<unsigned char> DataHolder;

	AuthGssapi();
	~AuthGssapi();

	// Returns true when context is established and ready
	bool isActive() const
	{
		return context_established;
	}

	// Accept security context from client (used by server)
	bool accept(DataHolder& data);

	// Returns client login information after successful authentication
	bool getLogin(ScratchBird::string& login, bool& isAdministrator, GroupsList& grNames);

	// Returns session key for wire encryption
	const Key* getKey() const;
};

class LinuxGssapiServer :
	public ScratchBird::StdPlugin<ScratchBird::IServerImpl<LinuxGssapiServer, ScratchBird::CheckStatusWrapper> >
{
public:
	// IServer implementation
	int authenticate(ScratchBird::CheckStatusWrapper* status, ScratchBird::IServerBlock* sBlock,
		ScratchBird::IWriter* writerInterface);
	void setDbCryptCallback(ScratchBird::CheckStatusWrapper* status, ScratchBird::ICryptKeyCallback* callback) {}; // do nothing

	LinuxGssapiServer(ScratchBird::IPluginConfig*);

private:
	AuthGssapi::DataHolder gssapiData;
	AuthGssapi gssapi;
	bool authentication_complete;
};

// Registration functions
void registerTrustedGssapiServer(ScratchBird::IPluginManager* iPlugin);

} // namespace Auth

#endif // TRUSTED_AUTH_GSSAPI
#endif // AUTH_GSSAPI_H