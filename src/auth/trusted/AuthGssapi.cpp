/*
 *	PROGRAM:		ScratchBird authentication
 *	MODULE:			AuthGssapi.cpp
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

#include "AuthGssapi.h"

#ifdef TRUSTED_AUTH_GSSAPI

#include "../common/classes/ClumpletReader.h"
#include "firebird/Interface.h"
#include "../common/classes/ImplementHelper.h"
#include "../common/isc_f_proto.h"
#include <pwd.h>
#include <grp.h>
#include <unistd.h>

using namespace ScratchBird;

namespace
{
	ScratchBird::SimpleFactory<Auth::LinuxGssapiServer> serverFactory;
	const char* plugName = "Linux_GSSAPI";

	// Helper function to display GSS-API errors
	void displayGssError(const char* msg, OM_uint32 maj_stat, OM_uint32 min_stat)
	{
		OM_uint32 msg_ctx = 0;
		OM_uint32 maj_stat_tmp, min_stat_tmp;
		gss_buffer_desc status_string;
		
		// Get major status message
		maj_stat_tmp = gss_display_status(&min_stat_tmp, maj_stat, GSS_C_GSS_CODE,
			GSS_C_NO_OID, &msg_ctx, &status_string);
		if (maj_stat_tmp == GSS_S_COMPLETE)
		{
			fprintf(stderr, "GSS-API %s: %.*s\n", msg, 
				(int)status_string.length, (char*)status_string.value);
			gss_release_buffer(&min_stat_tmp, &status_string);
		}
		
		// Get minor status message
		msg_ctx = 0;
		maj_stat_tmp = gss_display_status(&min_stat_tmp, min_stat, GSS_C_MECH_CODE,
			GSS_C_NO_OID, &msg_ctx, &status_string);
		if (maj_stat_tmp == GSS_S_COMPLETE)
		{
			fprintf(stderr, "GSS-API %s (minor): %.*s\n", msg,
				(int)status_string.length, (char*)status_string.value);
			gss_release_buffer(&min_stat_tmp, &status_string);
		}
	}
}

namespace Auth {

AuthGssapi::AuthGssapi()
	: context(GSS_C_NO_CONTEXT), server_creds(GSS_C_NO_CREDENTIAL),
	  client_name(GSS_C_NO_NAME), context_established(false),
	  has_credentials(false), client_principal(*getDefaultMemoryPool()),
	  is_admin(false), group_names(*getDefaultMemoryPool()),
	  session_key(*getDefaultMemoryPool())
{
	has_credentials = acquireCredentials();
}

AuthGssapi::~AuthGssapi()
{
	cleanupGssContext();
	
	if (server_creds != GSS_C_NO_CREDENTIAL)
	{
		OM_uint32 min_stat;
		gss_release_cred(&min_stat, &server_creds);
	}
}

bool AuthGssapi::acquireCredentials()
{
	OM_uint32 maj_stat, min_stat;
	
	// Acquire default credentials for server (accept credentials)
	maj_stat = gss_acquire_cred(&min_stat, GSS_C_NO_NAME, GSS_C_INDEFINITE,
		GSS_C_NO_OID_SET, GSS_C_ACCEPT, &server_creds, NULL, NULL);
		
	if (maj_stat != GSS_S_COMPLETE)
	{
		displayGssError("Failed to acquire server credentials", maj_stat, min_stat);
		return false;
	}
	
	return true;
}

void AuthGssapi::cleanupGssContext()
{
	OM_uint32 min_stat;
	
	if (context != GSS_C_NO_CONTEXT)
	{
		gss_delete_sec_context(&min_stat, &context, GSS_C_NO_BUFFER);
		context = GSS_C_NO_CONTEXT;
	}
	
	if (client_name != GSS_C_NO_NAME)
	{
		gss_release_name(&min_stat, &client_name);
		client_name = GSS_C_NO_NAME;
	}
	
	context_established = false;
}

bool AuthGssapi::accept(AuthGssapi::DataHolder& data)
{
	if (!has_credentials)
	{
		data.clear();
		return false;
	}

	OM_uint32 maj_stat, min_stat;
	gss_buffer_desc input_token, output_token;
	OM_uint32 ret_flags;
	
	// Set up input token from client data
	input_token.length = data.getCount();
	input_token.value = data.begin();
	
	// Initialize output token
	output_token.length = 0;
	output_token.value = NULL;
	
	// Accept the security context
	maj_stat = gss_accept_sec_context(&min_stat, &context, server_creds,
		&input_token, GSS_C_NO_CHANNEL_BINDINGS, &client_name, NULL,
		&output_token, &ret_flags, NULL, NULL);
		
	// Handle the result
	switch (maj_stat)
	{
		case GSS_S_COMPLETE:
			context_established = true;
			extractClientInfo();
			break;
			
		case GSS_S_CONTINUE_NEEDED:
			// More tokens needed - normal for multi-step authentication
			break;
			
		default:
			displayGssError("Failed to accept security context", maj_stat, min_stat);
			cleanupGssContext();
			data.clear();
			if (output_token.value)
				gss_release_buffer(&min_stat, &output_token);
			return false;
	}
	
	// Copy output token back to client
	if (output_token.length > 0)
	{
		memcpy(data.getBuffer(output_token.length), output_token.value, output_token.length);
		gss_release_buffer(&min_stat, &output_token);
	}
	else
	{
		data.clear();
	}
	
	return true;
}

bool AuthGssapi::extractClientInfo()
{
	if (client_name == GSS_C_NO_NAME)
		return false;
		
	OM_uint32 maj_stat, min_stat;
	gss_buffer_desc name_buffer;
	
	// Get the client principal name
	maj_stat = gss_display_name(&min_stat, client_name, &name_buffer, NULL);
	if (maj_stat != GSS_S_COMPLETE)
	{
		displayGssError("Failed to get client name", maj_stat, min_stat);
		return false;
	}
	
	// Convert to ScratchBird string and format conversion
	string temp_name((char*)name_buffer.value, name_buffer.length);
	client_principal = convertPrincipalFormat(temp_name);
	gss_release_buffer(&min_stat, &name_buffer);
	
	// Check for admin privileges
	is_admin = checkAdminPrivilege();
	
	// Extract session key for wire encryption
	gss_buffer_set_t data_set;
	maj_stat = gss_inquire_sec_context_by_oid(&min_stat, context,
		GSS_C_INQ_SSPI_SESSION_KEY, &data_set);
	if (maj_stat == GSS_S_COMPLETE && data_set && data_set->count > 0)
	{
		session_key.assign((unsigned char*)data_set->elements[0].value,
			data_set->elements[0].length);
		gss_release_buffer_set(&min_stat, &data_set);
	}
	
	return true;
}

ScratchBird::string AuthGssapi::convertPrincipalFormat(const ScratchBird::string& principal)
{
	// Convert from user@REALM.COM to REALM\user format for Windows compatibility
	size_t at_pos = principal.find('@');
	if (at_pos != string::npos)
	{
		string user = principal.substr(0, at_pos);
		string realm = principal.substr(at_pos + 1);
		
		// Convert realm to uppercase and format as DOMAIN\user
		realm.upper();
		return realm + "\\" + user;
	}
	
	// If no realm, return as-is
	return principal;
}

bool AuthGssapi::checkAdminPrivilege()
{
	// Extract the username from the principal for local privilege checking
	string username = client_principal;
	size_t backslash_pos = username.find('\\');
	if (backslash_pos != string::npos)
	{
		username = username.substr(backslash_pos + 1);
	}
	
	// Get user information
	struct passwd* pwd = getpwnam(username.c_str());
	if (!pwd)
		return false;
		
	// Check if user is in admin groups (wheel, sudo, admin)
	gid_t groups[32];
	int ngroups = 32;
	
	if (getgrouplist(username.c_str(), pwd->pw_gid, groups, &ngroups) == -1)
		return false;
		
	for (int i = 0; i < ngroups; i++)
	{
		struct group* grp = getgrgid(groups[i]);
		if (grp)
		{
			string group_name = grp->gr_name;
			group_names.add(group_name);
			
			// Check for administrative groups
			if (group_name == "wheel" || group_name == "sudo" || 
				group_name == "admin" || group_name == "root")
			{
				return true;
			}
		}
	}
	
	return false;
}

bool AuthGssapi::getLogin(string& login, bool& isAdministrator, GroupsList& grNames)
{
	if (!context_established || client_principal.isEmpty())
		return false;
		
	login = client_principal;
	isAdministrator = is_admin;
	grNames = group_names;
	
	// Clear the stored information after retrieval
	client_principal.erase();
	is_admin = false;
	group_names.clear();
	
	return true;
}

const AuthGssapi::Key* AuthGssapi::getKey() const
{
	if (session_key.hasData())
		return &session_key;
	return NULL;
}

// LinuxGssapiServer implementation

LinuxGssapiServer::LinuxGssapiServer(ScratchBird::IPluginConfig*)
	: gssapiData(getPool()), authentication_complete(false)
{
}

int LinuxGssapiServer::authenticate(ScratchBird::CheckStatusWrapper* status,
	IServerBlock* sBlock, IWriter* writerInterface)
{
	try
	{
		gssapiData.clear();
		unsigned int length;
		const unsigned char* bytes = sBlock->getData(&length);
		gssapiData.add(bytes, length);

		// If authentication is complete and no more data, return success
		if (authentication_complete && !length && !gssapi.isActive())
			return AUTH_SUCCESS;

		// Process the GSS-API authentication data
		if (!gssapi.accept(gssapiData))
			return AUTH_CONTINUE;

		// Check if authentication is now complete
		if (!gssapi.isActive())
		{
			bool is_admin = false;
			string login;
			AuthGssapi::GroupsList group_names;
			
			if (!gssapi.getLogin(login, is_admin, group_names))
				return AUTH_FAILED;

			// Convert system charset to UTF-8
			ISC_systemToUtf8(login);

			// Publish the authenticated user name
			writerInterface->add(status, login.c_str());
			if (status->getState() & IStatus::STATE_ERRORS)
				return AUTH_FAILED;

			// Add admin privileges if user is administrator
			if (is_admin)
			{
				writerInterface->add(status, "DOMAIN_ANY_RID_ADMINS");
				if (status->getState() & IStatus::STATE_ERRORS)
					return AUTH_FAILED;

				writerInterface->setType(status, "Predefined_Group");
				if (status->getState() & IStatus::STATE_ERRORS)
					return AUTH_FAILED;
			}

			// Add group memberships
			for (unsigned n = 0; n < group_names.getCount(); ++n)
			{
				string group_name = group_names[n];
				ISC_systemToUtf8(group_name);
				
				writerInterface->add(status, group_name.c_str());
				if (status->getState() & IStatus::STATE_ERRORS)
					return AUTH_FAILED;

				writerInterface->setType(status, "Group");
				if (status->getState() & IStatus::STATE_ERRORS)
					return AUTH_FAILED;
			}

			// Set wire encryption key if available
			const UCharBuffer* key = gssapi.getKey();
			if (key)
			{
				ICryptKey* cKey = sBlock->newKey(status);
				if (status->getState() & IStatus::STATE_ERRORS)
					return AUTH_FAILED;

				cKey->setSymmetric(status, "Symmetric", key->getCount(), key->begin());
				if (status->getState() & IStatus::STATE_ERRORS)
					return AUTH_FAILED;
			}

			authentication_complete = true;
			if (gssapiData.isEmpty())
				return AUTH_SUCCESS;
		}

		// Send response data back to client
		sBlock->putData(status, gssapiData.getCount(), gssapiData.begin());
	}
	catch (const ScratchBird::Exception& ex)
	{
		ex.stuffException(status);
		return AUTH_FAILED;
	}

	return authentication_complete ? AUTH_SUCCESS_WITH_DATA : AUTH_MORE_DATA;
}

// Registration function
void registerTrustedGssapiServer(ScratchBird::IPluginManager* iPlugin)
{
	iPlugin->registerPluginFactory(ScratchBird::IPluginManager::TYPE_AUTH_SERVER, 
		plugName, &serverFactory);
}

} // namespace Auth

#endif // TRUSTED_AUTH_GSSAPI