/*
 *	PROGRAM:	Security data base manager
 *	MODULE:		security.cpp
 *	DESCRIPTION:	Security routines
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
#include "../common/classes/alloc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "ibase.h"
#include "../auth/SecurityDatabase/LegacyHash.h"
#include "../common/enc_proto.h"
#include "../yvalve/gds_proto.h"
#include "../common/isc_proto.h"
#include "../utilities/gsec/gsec.h"
#include "../common/utils_proto.h"
#include "../common/classes/init.h"
#include "../common/classes/UserBlob.h"
#include "../common/config/config_file.h"
#include "../auth/SecurityDatabase/LegacyManagement.h"
#include "../common/classes/ImplementHelper.h"
#include "../common/classes/ClumpletWriter.h"
#include "../common/StatusHolder.h"
#include "../common/security.h"
#include "../common/classes/ParsedList.h"
#include "firebird/Interface.h"

using namespace ScratchBird;

// Replaced GPRE DATABASE database = STATIC FILENAME "security.fdb"; with modern API
// Database access is handled through existing attachment mechanisms

static ScratchBird::GlobalPtr<ScratchBird::Mutex> execLineMutex;	// protects various database operations

static bool grantRevokeAdmin(ISC_STATUS* isc_status, FB_API_HANDLE database, FB_API_HANDLE trans,
							 ScratchBird::IUser* user)
{
	if (!user->admin()->entered())
	{
		return true;
	}

	ScratchBird::string userName(user->userName()->get());
	for (unsigned i = 0; i < userName.length(); ++i)
	{
		if (userName[i] == '"')
		{
			userName.insert(i++, 1, '"');
		}
	}

	ScratchBird::string sql;
	sql.printf((user->admin()->get() ? "GRANT %s TO \"%s\"" : "REVOKE %s FROM \"%s\""),
			ADMIN_ROLE, userName.c_str());
	isc_dsql_execute_immediate(isc_status, &database, &trans, sql.length(), sql.c_str(), SQL_DIALECT_V6, NULL);

	if (isc_status[1] && user->admin()->get() == 0)
	{
		// Converted FOR loop #1: FOR (TRANSACTION_HANDLE trans REQUEST_HANDLE request) R IN RDB$USER_PRIVILEGES
		isc_req_handle stmt_handle = 0;
		XSQLDA* out_sqlda = NULL;
		
		const char* query = "SELECT RDB$GRANTOR FROM RDB$USER_PRIVILEGES "
							"WHERE RDB$USER = ? AND RDB$RELATION_NAME = 'RDB$ADMIN' AND RDB$PRIVILEGE = 'M'";
		
		if (isc_dsql_allocate_statement(isc_status, &database, &stmt_handle) == 0)
		{
			if (isc_dsql_prepare(isc_status, &trans, &stmt_handle, 0, query, SQL_DIALECT_V6, NULL) == 0)
			{
				// Allocate input SQLDA for parameter
				XSQLDA* in_sqlda = (XSQLDA*) malloc(XSQLDA_LENGTH(1));
				in_sqlda->version = SQLDA_VERSION1;
				in_sqlda->sqln = 1;
				in_sqlda->sqld = 1;
				in_sqlda->sqlvar[0].sqltype = SQL_TEXT;
				in_sqlda->sqlvar[0].sqllen = strlen(user->userName()->get());
				in_sqlda->sqlvar[0].sqldata = (char*) user->userName()->get();
				in_sqlda->sqlvar[0].sqlind = NULL;
				
				// Allocate output SQLDA
				out_sqlda = (XSQLDA*) malloc(XSQLDA_LENGTH(1));
				out_sqlda->version = SQLDA_VERSION1;
				out_sqlda->sqln = 1;
				
				if (isc_dsql_describe(isc_status, &stmt_handle, SQL_DIALECT_V6, out_sqlda) == 0)
				{
					char grantor_name[32];
					out_sqlda->sqlvar[0].sqldata = grantor_name;
					
					if (isc_dsql_execute(isc_status, &trans, &stmt_handle, SQL_DIALECT_V6, in_sqlda) == 0)
					{
						while (isc_dsql_fetch(isc_status, &stmt_handle, SQL_DIALECT_V6, out_sqlda) == 0)
						{
							sql.printf("REVOKE " ADMIN_ROLE " FROM \"%s\" GRANTED BY \"%s\"",
								userName.c_str(), grantor_name);
						}
					}
				}
				
				free(in_sqlda);
				if (out_sqlda) free(out_sqlda);
			}
			isc_dsql_free_statement(isc_status, &stmt_handle, DSQL_drop);
		}

		isc_dsql_execute_immediate(isc_status, &database, &trans, sql.length(), sql.c_str(), SQL_DIALECT_V6, NULL);
	}

	return isc_status[1] == 0;
}


static ScratchBird::GlobalPtr<ScratchBird::ConfigKeys> keys;

namespace Auth {

SecurityDatabaseManagement::SecurityDatabaseManagement(ScratchBird::IPluginConfig* par)
	: database(0), transaction(0)
{
	ScratchBird::LocalStatus s;
	ScratchBird::CheckStatusWrapper statusWrapper(&s);
	config.assignRefNoIncr(par->getFirebirdConf(&statusWrapper));
	check(&s);
}

void SecurityDatabaseManagement::start(ScratchBird::CheckStatusWrapper* st, ScratchBird::ILogonInfo* logonInfo)
{
	try
	{
		st->init();

		unsigned int secDbKey = keys->getKey(config, "SecurityDatabase");
		const char* secDbName = config->asString(secDbKey);
		if (!(secDbName && secDbName[0]))
		{
			ScratchBird::Arg::Gds(isc_secdb_name).raise();
		}

		ScratchBird::ClumpletWriter dpb(ScratchBird::ClumpletReader::dpbList, MAX_DPB_SIZE);
		dpb.insertByte(isc_dpb_sec_attach, TRUE);
		dpb.insertString(isc_dpb_config, ScratchBird::ParsedList::getNonLoopbackProviders(secDbName));

		string schemaSearchPath;
		schemaSearchPath.printf("%s, %s", PLG_LEGACY_SEC_SCHEMA, SYSTEM_SCHEMA);
		dpb.insertString(isc_dpb_search_path, schemaSearchPath.c_str(), fb_strlen(schemaSearchPath.c_str()));

		unsigned int authBlockSize;
		const unsigned char* authBlock = logonInfo->authBlock(&authBlockSize);

		if (authBlockSize)
			dpb.insertBytes(isc_dpb_auth_block, authBlock, authBlockSize);
		else
		{
			const char* logon = logonInfo->name();
			if (logon && logon[0])
				dpb.insertString(isc_dpb_trusted_auth, logon, fb_strlen(logon));
		}

		const char* role = logonInfo->role();
		if (role && role[0])
			dpb.insertString(isc_dpb_sql_role_name, role, fb_strlen(role));

		ISC_STATUS_ARRAY status;
		if (isc_attach_database(status, 0, secDbName, &database,
								dpb.getBufferLength(), reinterpret_cast<const char*>(dpb.getBuffer())))
		{
			ScratchBird::status_exception::raise(status);
		}

		if (isc_start_transaction(status, &transaction, 1, &database, 0, NULL))
		{
			ScratchBird::status_exception::raise(status);
		}
	}
	catch (const ScratchBird::Exception& ex)
	{
		ex.stuffException(st);
	}
}

void SecurityDatabaseManagement::commit(ScratchBird::CheckStatusWrapper* st)
{
	try
	{
		st->init();

		ISC_STATUS_ARRAY status;
		if (transaction)
		{
			if (isc_commit_transaction(status, &transaction))
			{
				ScratchBird::status_exception::raise(status);
			}
		}
	}
	catch (const ScratchBird::Exception& ex)
	{
		ex.stuffException(st);
	}
}

void SecurityDatabaseManagement::rollback(ScratchBird::CheckStatusWrapper* st)
{
	try
	{
		st->init();

		ISC_STATUS_ARRAY status;
		if (transaction)
		{
			if (isc_rollback_transaction(status, &transaction))
			{
				ScratchBird::status_exception::raise(status);
			}
		}
	}
	catch (const ScratchBird::Exception& ex)
	{
		ex.stuffException(st);
	}
}

int SecurityDatabaseManagement::release()
{
	if (--refCounter == 0)
	{
		ISC_STATUS_ARRAY status;
		if (transaction)
			isc_rollback_transaction(status, &transaction);
		if (database)
			isc_detach_database(status, &database);

		delete this;
		return 0;
	}

	return 1;
}

#define STR_STORE(to, from) fb_utils::copy_terminate(to, from, sizeof(to))
#define STR_VSTORE(to, from) string2vary(&to, from, sizeof(to))
static void string2vary(void* to, const ScratchBird::string& from, size_t to_size)
{
	const size_t len = MIN(to_size - sizeof(USHORT), from.size());
	paramvary* v = reinterpret_cast<paramvary*>(to);
	v->vary_length = static_cast<ISC_USHORT>(len);
	memcpy(v->vary_string, from.c_str(), len);
}


int SecurityDatabaseManagement::execute(ScratchBird::CheckStatusWrapper* st, ScratchBird::IUser* user,
	ScratchBird::IListUsers* callback)
{
/*************************************
 *
 *	S E C U R I T Y _ e x e c _ l i n e
 *
 **************************************
 *
 * Functional description
 *	Process a command line for the security data base manager.
 *	This is used to add and delete users from the user information
 *	database (security2.fdb).   It also displays information
 *	about current users and allows modification of current
 *	users' parameters.
 *	Returns 0 on success, otherwise returns a Gsec message number
 *	and the status vector containing the error info.
 *	The syntax is:
 *
 *	Adding a new user:
 *
 *	    gsec -add <name> [ <parameter> ... ]    -- command line
 *	    add <name> [ <parameter> ... ]          -- interactive
 *
 *	Deleting a current user:
 *
 *	    gsec -delete <name>     -- command line
 *	    delete <name>           -- interactive
 *
 *	Displaying all current users:
 *
 *	    gsec -display           -- command line
 *	    display                 -- interactive
 *
 *	Displaying one user:
 *
 *	    gsec -display <name>    -- command line
 *	    display <name>          -- interactive
 *
 *	Modifying a user's parameters:
 *
 *	    gsec -modify <name> <parameter> [ <parameter> ... ] -- command line
 *	    modify <name> <parameter> [ <parameter> ... ]       -- interactive
 *
 *	Get help:
 *
 *	    gsec -help              -- command line
 *	    ?                       -- interactive
 *	    help                    -- interactive
 *
 *	Quit interactive session:
 *
 *	    quit                    -- interactive
 *
 *	where <parameter> can be one of:
 *
 *	    -uid <uid>
 *	    -gid <gid>
 *	    -fname <firstname>
 *	    -mname <middlename>
 *	    -lname <lastname>
 *
 **************************************/
	int ret = 0;

	try
	{
		ISC_STATUS_ARRAY isc_status;
		fb_utils::init_status(isc_status);
		st->init();

		ScratchBird::MutexLockGuard guard(execLineMutex, FB_FUNCTION);

		SCHAR encrypted1[MAX_LEGACY_PASSWORD_LENGTH + 2];
		ScratchBird::string encrypted2;
		bool found;

		// check for non-printable characters in user name
		for (const TEXT* p = user->userName()->get(); *p; p++)
		{
			if (!isprint(*p))
			{
				return GsecMsg75;  // Add special error message for this case ?
			}
		}

		isc_req_handle request = 0;
		isc_req_handle request2 = 0;

		switch (user->operation())
		{
		case ScratchBird::IUser::OP_USER_DROP_MAP:
		case ScratchBird::IUser::OP_USER_SET_MAP:
			{
				ScratchBird::string sql;
				sql.printf("ALTER ROLE " ADMIN_ROLE " %s AUTO ADMIN MAPPING",
					user->operation() == ScratchBird::IUser::OP_USER_SET_MAP ? "SET" : "DROP");
				isc_dsql_execute_immediate(isc_status, &database, &transaction, sql.length(), sql.c_str(), 1, NULL);
				if (isc_status[1] != 0)
				{
					ret = GsecMsg97;
				}
			}
			break;

		case ScratchBird::IUser::OP_USER_ADD:
			// this checks the "entered" flags for each parameter (except the name)
			// and makes all non-entered parameters null valued

			// Converted STORE operation: STORE (TRANSACTION_HANDLE transaction REQUEST_HANDLE request) U IN PLG$VIEW_USERS USING
			{
				isc_req_handle stmt_handle = 0;
				const char* insert_sql = "INSERT INTO PLG$VIEW_USERS (PLG$USER_NAME, PLG$UID, PLG$GID, PLG$GROUP_NAME, "
					"PLG$PASSWD, PLG$FIRST_NAME, PLG$MIDDLE_NAME, PLG$LAST_NAME) VALUES (?, ?, ?, ?, ?, ?, ?, ?)";
				
				if (isc_dsql_allocate_statement(isc_status, &database, &stmt_handle) == 0)
				{
					if (isc_dsql_prepare(isc_status, &transaction, &stmt_handle, 0, insert_sql, SQL_DIALECT_V6, NULL) == 0)
					{
						XSQLDA* in_sqlda = (XSQLDA*) malloc(XSQLDA_LENGTH(8));
						in_sqlda->version = SQLDA_VERSION1;
						in_sqlda->sqln = 8;
						in_sqlda->sqld = 8;
						
						// PLG$USER_NAME
						in_sqlda->sqlvar[0].sqltype = SQL_TEXT;
						in_sqlda->sqlvar[0].sqllen = strlen(user->userName()->get());
						in_sqlda->sqlvar[0].sqldata = (char*) user->userName()->get();
						in_sqlda->sqlvar[0].sqlind = NULL;
						
						// PLG$UID - set to NULL by default
						static short null_ind = -1;
						SLONG uid_val = 0;
						in_sqlda->sqlvar[1].sqltype = SQL_LONG + 1;
						in_sqlda->sqlvar[1].sqllen = sizeof(SLONG);
						in_sqlda->sqlvar[1].sqldata = (char*) &uid_val;
						in_sqlda->sqlvar[1].sqlind = &null_ind;
						
						// PLG$GID - set to NULL by default  
						SLONG gid_val = 0;
						in_sqlda->sqlvar[2].sqltype = SQL_LONG + 1;
						in_sqlda->sqlvar[2].sqllen = sizeof(SLONG);
						in_sqlda->sqlvar[2].sqldata = (char*) &gid_val;
						in_sqlda->sqlvar[2].sqlind = &null_ind;
						
						// PLG$GROUP_NAME - set to NULL by default
						char group_name[256] = "";
						in_sqlda->sqlvar[3].sqltype = SQL_TEXT + 1;
						in_sqlda->sqlvar[3].sqllen = sizeof(group_name);
						in_sqlda->sqlvar[3].sqldata = group_name;
						in_sqlda->sqlvar[3].sqlind = &null_ind;
						
						// Process attributes if entered
						if (user->attributes()->entered())
						{
							ConfigFile attr(ConfigFile::USE_TEXT, user->attributes()->get());
							const ConfigFile::Parameter* p;
							static short not_null = 0;

							if ((p = attr.findParameter("uid")) && p->value.hasData())
							{
								uid_val = p->asInteger();
								in_sqlda->sqlvar[1].sqlind = &not_null;
							}

							if ((p = attr.findParameter("gid")) && p->value.hasData())
							{
								gid_val = p->asInteger();
								in_sqlda->sqlvar[2].sqlind = &not_null;
							}

							if ((p = attr.findParameter("groupName")) && p->value.hasData())
							{
								STR_STORE(group_name, p->value.c_str());
								in_sqlda->sqlvar[3].sqlind = &not_null;
							}
						}
						
						// PLG$PASSWD
						paramvary passwd_vary;
						in_sqlda->sqlvar[4].sqltype = SQL_VARYING + 1;
						in_sqlda->sqlvar[4].sqllen = sizeof(paramvary);
						in_sqlda->sqlvar[4].sqldata = (char*) &passwd_vary;
						if (user->password()->entered())
						{
							ENC_crypt(encrypted1, sizeof encrypted1, user->password()->get(), LEGACY_PASSWORD_SALT);
							LegacyHash::hash(encrypted2, user->userName()->get(), &encrypted1[2]);
							STR_VSTORE(passwd_vary, encrypted2);
							static short not_null = 0;
							in_sqlda->sqlvar[4].sqlind = &not_null;
						}
						else
							in_sqlda->sqlvar[4].sqlind = &null_ind;
							
						// PLG$FIRST_NAME
						char first_name[256] = "";
						in_sqlda->sqlvar[5].sqltype = SQL_TEXT + 1;
						in_sqlda->sqlvar[5].sqllen = sizeof(first_name);
						in_sqlda->sqlvar[5].sqldata = first_name;
						if (user->firstName()->entered())
						{
							STR_STORE(first_name, user->firstName()->get());
							static short not_null = 0;
							in_sqlda->sqlvar[5].sqlind = &not_null;
						}
						else
							in_sqlda->sqlvar[5].sqlind = &null_ind;
							
						// PLG$MIDDLE_NAME
						char middle_name[256] = "";
						in_sqlda->sqlvar[6].sqltype = SQL_TEXT + 1;
						in_sqlda->sqlvar[6].sqllen = sizeof(middle_name);
						in_sqlda->sqlvar[6].sqldata = middle_name;
						if (user->middleName()->entered())
						{
							STR_STORE(middle_name, user->middleName()->get());
							static short not_null = 0;
							in_sqlda->sqlvar[6].sqlind = &not_null;
						}
						else
							in_sqlda->sqlvar[6].sqlind = &null_ind;
							
						// PLG$LAST_NAME
						char last_name[256] = "";
						in_sqlda->sqlvar[7].sqltype = SQL_TEXT + 1;
						in_sqlda->sqlvar[7].sqllen = sizeof(last_name);
						in_sqlda->sqlvar[7].sqldata = last_name;
						if (user->lastName()->entered())
						{
							STR_STORE(last_name, user->lastName()->get());
							static short not_null = 0;
							in_sqlda->sqlvar[7].sqlind = &not_null;
						}
						else
							in_sqlda->sqlvar[7].sqlind = &null_ind;
						
						if (isc_dsql_execute(isc_status, &transaction, &stmt_handle, SQL_DIALECT_V6, in_sqlda) != 0)
						{
							ret = GsecMsg19;	// gsec - add record error
						}
						
						free(in_sqlda);
					}
					else
						ret = GsecMsg19;
					
					isc_dsql_free_statement(isc_status, &stmt_handle, DSQL_drop);
				}
				else
					ret = GsecMsg19;
			}
			
			if (ret == 0 && !grantRevokeAdmin(isc_status, database, transaction, user))
			{
				ret = GsecMsg19;	// gsec - add record error
			}
			break;

		case ScratchBird::IUser::OP_USER_MODIFY:
			// this updates an existing record, replacing all fields that are
			// entered, and for those that were specified but not entered, it
			// changes the current value to the null value

			found = false;
			
			// Converted FOR loop #2: FOR (TRANSACTION_HANDLE transaction REQUEST_HANDLE request) U IN PLG$VIEW_USERS WITH U.PLG$USER_NAME EQ user->userName()->get()
			{
				isc_req_handle stmt_handle = 0;
				const char* select_sql = "SELECT PLG$USER_NAME FROM PLG$VIEW_USERS WHERE PLG$USER_NAME = ?";
				
				if (isc_dsql_allocate_statement(isc_status, &database, &stmt_handle) == 0)
				{
					if (isc_dsql_prepare(isc_status, &transaction, &stmt_handle, 0, select_sql, SQL_DIALECT_V6, NULL) == 0)
					{
						XSQLDA* in_sqlda = (XSQLDA*) malloc(XSQLDA_LENGTH(1));
						in_sqlda->version = SQLDA_VERSION1;
						in_sqlda->sqln = 1;
						in_sqlda->sqld = 1;
						in_sqlda->sqlvar[0].sqltype = SQL_TEXT;
						in_sqlda->sqlvar[0].sqllen = strlen(user->userName()->get());
						in_sqlda->sqlvar[0].sqldata = (char*) user->userName()->get();
						in_sqlda->sqlvar[0].sqlind = NULL;
						
						XSQLDA* out_sqlda = (XSQLDA*) malloc(XSQLDA_LENGTH(1));
						out_sqlda->version = SQLDA_VERSION1;
						out_sqlda->sqln = 1;
						
						if (isc_dsql_describe(isc_status, &stmt_handle, SQL_DIALECT_V6, out_sqlda) == 0)
						{
							char user_name_buf[256];
							out_sqlda->sqlvar[0].sqldata = user_name_buf;
							
							if (isc_dsql_execute(isc_status, &transaction, &stmt_handle, SQL_DIALECT_V6, in_sqlda) == 0)
							{
								while (isc_dsql_fetch(isc_status, &stmt_handle, SQL_DIALECT_V6, out_sqlda) == 0)
								{
									found = true;
									
									// Converted MODIFY operation: MODIFY U USING
									isc_req_handle update_handle = 0;
									const char* update_sql = "UPDATE PLG$VIEW_USERS SET PLG$UID = ?, PLG$GID = ?, PLG$GROUP_NAME = ?, "
														   "PLG$PASSWD = ?, PLG$FIRST_NAME = ?, PLG$MIDDLE_NAME = ?, PLG$LAST_NAME = ? "
														   "WHERE PLG$USER_NAME = ?";
									
									if (isc_dsql_allocate_statement(isc_status, &database, &update_handle) == 0)
									{
										if (isc_dsql_prepare(isc_status, &transaction, &update_handle, 0, update_sql, SQL_DIALECT_V6, NULL) == 0)
										{
											XSQLDA* upd_sqlda = (XSQLDA*) malloc(XSQLDA_LENGTH(8));
											upd_sqlda->version = SQLDA_VERSION1;
											upd_sqlda->sqln = 8;
											upd_sqlda->sqld = 8;
											
											static short null_ind = -1;
											static short not_null = 0;
											
											// PLG$UID
											SLONG uid_val = 0;
											upd_sqlda->sqlvar[0].sqltype = SQL_LONG + 1;
											upd_sqlda->sqlvar[0].sqllen = sizeof(SLONG);
											upd_sqlda->sqlvar[0].sqldata = (char*) &uid_val;
											upd_sqlda->sqlvar[0].sqlind = &null_ind;
											
											// PLG$GID  
											SLONG gid_val = 0;
											upd_sqlda->sqlvar[1].sqltype = SQL_LONG + 1;
											upd_sqlda->sqlvar[1].sqllen = sizeof(SLONG);
											upd_sqlda->sqlvar[1].sqldata = (char*) &gid_val;
											upd_sqlda->sqlvar[1].sqlind = &null_ind;
											
											// PLG$GROUP_NAME
											char group_name[256] = "";
											upd_sqlda->sqlvar[2].sqltype = SQL_TEXT + 1;
											upd_sqlda->sqlvar[2].sqllen = sizeof(group_name);
											upd_sqlda->sqlvar[2].sqldata = group_name;
											upd_sqlda->sqlvar[2].sqlind = &null_ind;
											
											// Process attributes
											if (user->attributes()->entered())
											{
												ConfigFile attr(ConfigFile::USE_TEXT, user->attributes()->get());
												const ConfigFile::Parameter* p;

												if ((p = attr.findParameter("uid")) && p->value.hasData())
												{
													uid_val = p->asInteger();
													upd_sqlda->sqlvar[0].sqlind = &not_null;
												}

												if ((p = attr.findParameter("gid")) && p->value.hasData())
												{
													gid_val = p->asInteger();
													upd_sqlda->sqlvar[1].sqlind = &not_null;
												}

												if ((p = attr.findParameter("groupName")) && p->value.hasData())
												{
													STR_STORE(group_name, p->value.c_str());
													upd_sqlda->sqlvar[2].sqlind = &not_null;
												}
											}
											
											// PLG$PASSWD
											paramvary passwd_vary;
											upd_sqlda->sqlvar[3].sqltype = SQL_VARYING + 1;
											upd_sqlda->sqlvar[3].sqllen = sizeof(paramvary);
											upd_sqlda->sqlvar[3].sqldata = (char*) &passwd_vary;
											if (user->password()->entered())
											{
												ENC_crypt(encrypted1, sizeof encrypted1, user->password()->get(), LEGACY_PASSWORD_SALT);
												LegacyHash::hash(encrypted2, user->userName()->get(), &encrypted1[2]);
												STR_VSTORE(passwd_vary, encrypted2);
												upd_sqlda->sqlvar[3].sqlind = &not_null;
											}
											else if (user->password()->specified())
												upd_sqlda->sqlvar[3].sqlind = &null_ind;
											else
												upd_sqlda->sqlvar[3].sqlind = &not_null; // Keep existing
												
											// PLG$FIRST_NAME
											char first_name[256] = "";
											upd_sqlda->sqlvar[4].sqltype = SQL_TEXT + 1;
											upd_sqlda->sqlvar[4].sqllen = sizeof(first_name);
											upd_sqlda->sqlvar[4].sqldata = first_name;
											if (user->firstName()->entered())
											{
												STR_STORE(first_name, user->firstName()->get());
												upd_sqlda->sqlvar[4].sqlind = &not_null;
											}
											else if (user->firstName()->specified())
												upd_sqlda->sqlvar[4].sqlind = &null_ind;
											else
												upd_sqlda->sqlvar[4].sqlind = &not_null; // Keep existing
												
											// PLG$MIDDLE_NAME
											char middle_name[256] = "";
											upd_sqlda->sqlvar[5].sqltype = SQL_TEXT + 1;
											upd_sqlda->sqlvar[5].sqllen = sizeof(middle_name);
											upd_sqlda->sqlvar[5].sqldata = middle_name;
											if (user->middleName()->entered())
											{
												STR_STORE(middle_name, user->middleName()->get());
												upd_sqlda->sqlvar[5].sqlind = &not_null;
											}
											else if (user->middleName()->specified())
												upd_sqlda->sqlvar[5].sqlind = &null_ind;
											else
												upd_sqlda->sqlvar[5].sqlind = &not_null; // Keep existing
												
											// PLG$LAST_NAME
											char last_name[256] = "";
											upd_sqlda->sqlvar[6].sqltype = SQL_TEXT + 1;
											upd_sqlda->sqlvar[6].sqllen = sizeof(last_name);
											upd_sqlda->sqlvar[6].sqldata = last_name;
											if (user->lastName()->entered())
											{
												STR_STORE(last_name, user->lastName()->get());
												upd_sqlda->sqlvar[6].sqlind = &not_null;
											}
											else if (user->lastName()->specified())
												upd_sqlda->sqlvar[6].sqlind = &null_ind;
											else
												upd_sqlda->sqlvar[6].sqlind = &not_null; // Keep existing
												
											// WHERE PLG$USER_NAME = ?
											upd_sqlda->sqlvar[7].sqltype = SQL_TEXT;
											upd_sqlda->sqlvar[7].sqllen = strlen(user->userName()->get());
											upd_sqlda->sqlvar[7].sqldata = (char*) user->userName()->get();
											upd_sqlda->sqlvar[7].sqlind = NULL;
											
											if (isc_dsql_execute(isc_status, &transaction, &update_handle, SQL_DIALECT_V6, upd_sqlda) != 0)
											{
												ret = GsecMsg20;
											}
											
											free(upd_sqlda);
										}
										else
											ret = GsecMsg20;
										
										isc_dsql_free_statement(isc_status, &update_handle, DSQL_drop);
									}
									else
										ret = GsecMsg20;
								}
							}
						}
						
						free(in_sqlda);
						free(out_sqlda);
					}
					else
						ret = GsecMsg21;
					
					isc_dsql_free_statement(isc_status, &stmt_handle, DSQL_drop);
				}
				else
					ret = GsecMsg21;
			}

			if (!ret && !found)
				ret = GsecMsg22;

			if (ret == 0 && !grantRevokeAdmin(isc_status, database, transaction, user))
			{
				ret = GsecMsg21;
			}
			break;

		case ScratchBird::IUser::OP_USER_DELETE:
			// looks up the specified user record and deletes it

			found = false;
			// Do not allow SYSDBA user to be deleted
			if (!fb_utils::stricmp(user->userName()->get(), DBA_USER_NAME))
				ret = GsecMsg23;
			else
			{
				// Converted FOR loop #3: FOR (TRANSACTION_HANDLE transaction REQUEST_HANDLE request) U IN PLG$VIEW_USERS WITH U.PLG$USER_NAME EQ user->userName()->get()
				isc_req_handle stmt_handle = 0;
				const char* select_sql = "SELECT PLG$USER_NAME FROM PLG$VIEW_USERS WHERE PLG$USER_NAME = ?";
				
				if (isc_dsql_allocate_statement(isc_status, &database, &stmt_handle) == 0)
				{
					if (isc_dsql_prepare(isc_status, &transaction, &stmt_handle, 0, select_sql, SQL_DIALECT_V6, NULL) == 0)
					{
						XSQLDA* in_sqlda = (XSQLDA*) malloc(XSQLDA_LENGTH(1));
						in_sqlda->version = SQLDA_VERSION1;
						in_sqlda->sqln = 1;
						in_sqlda->sqld = 1;
						in_sqlda->sqlvar[0].sqltype = SQL_TEXT;
						in_sqlda->sqlvar[0].sqllen = strlen(user->userName()->get());
						in_sqlda->sqlvar[0].sqldata = (char*) user->userName()->get();
						in_sqlda->sqlvar[0].sqlind = NULL;
						
						XSQLDA* out_sqlda = (XSQLDA*) malloc(XSQLDA_LENGTH(1));
						out_sqlda->version = SQLDA_VERSION1;
						out_sqlda->sqln = 1;
						
						if (isc_dsql_describe(isc_status, &stmt_handle, SQL_DIALECT_V6, out_sqlda) == 0)
						{
							char user_name_buf[256];
							out_sqlda->sqlvar[0].sqldata = user_name_buf;
							
							if (isc_dsql_execute(isc_status, &transaction, &stmt_handle, SQL_DIALECT_V6, in_sqlda) == 0)
							{
								while (isc_dsql_fetch(isc_status, &stmt_handle, SQL_DIALECT_V6, out_sqlda) == 0)
								{
									found = true;
									
									// Converted ERASE operation: ERASE U
									isc_req_handle delete_handle = 0;
									const char* delete_sql = "DELETE FROM PLG$VIEW_USERS WHERE PLG$USER_NAME = ?";
									
									if (isc_dsql_allocate_statement(isc_status, &database, &delete_handle) == 0)
									{
										if (isc_dsql_prepare(isc_status, &transaction, &delete_handle, 0, delete_sql, SQL_DIALECT_V6, NULL) == 0)
										{
											XSQLDA* del_sqlda = (XSQLDA*) malloc(XSQLDA_LENGTH(1));
											del_sqlda->version = SQLDA_VERSION1;
											del_sqlda->sqln = 1;
											del_sqlda->sqld = 1;
											del_sqlda->sqlvar[0].sqltype = SQL_TEXT;
											del_sqlda->sqlvar[0].sqllen = strlen(user->userName()->get());
											del_sqlda->sqlvar[0].sqldata = (char*) user->userName()->get();
											del_sqlda->sqlvar[0].sqlind = NULL;
											
											if (isc_dsql_execute(isc_status, &transaction, &delete_handle, SQL_DIALECT_V6, del_sqlda) != 0)
											{
												ret = GsecMsg23;	// gsec - delete record error
											}
											
											free(del_sqlda);
										}
										else
											ret = GsecMsg23;
										
										isc_dsql_free_statement(isc_status, &delete_handle, DSQL_drop);
									}
									else
										ret = GsecMsg23;
								}
							}
						}
						
						free(in_sqlda);
						free(out_sqlda);
					}
					else
						ret = GsecMsg24;
					
					isc_dsql_free_statement(isc_status, &stmt_handle, DSQL_drop);
				}
				else
					ret = GsecMsg24;
			}

			if (!ret && !found)
				ret = GsecMsg22;	// gsec - record not found for user:

			user->admin()->set(st, 0);
			check(st);
			user->admin()->setEntered(st, 1);
			check(st);
			if (ret == 0 && !grantRevokeAdmin(isc_status, database, transaction, user))
			{
				ret = GsecMsg24;
			}
			break;

		case ScratchBird::IUser::OP_USER_DISPLAY:
			// gets either the desired record, or all records, and displays them

			found = false;
			if (!user->userName()->entered())
			{
				ScratchBird::LocalStatus s2;
				ScratchBird::CheckStatusWrapper statusWrapper2(&s2);
				ScratchBird::CheckStatusWrapper* s = st;

				// Converted FOR loop #4: FOR (TRANSACTION_HANDLE transaction REQUEST_HANDLE request) U IN PLG$VIEW_USERS
				isc_req_handle stmt_handle = 0;
				const char* select_sql = "SELECT PLG$USER_NAME, PLG$UID, PLG$GID, PLG$GROUP_NAME, PLG$FIRST_NAME, PLG$MIDDLE_NAME, PLG$LAST_NAME FROM PLG$VIEW_USERS";
				
				if (isc_dsql_allocate_statement(isc_status, &database, &stmt_handle) == 0)
				{
					if (isc_dsql_prepare(isc_status, &transaction, &stmt_handle, 0, select_sql, SQL_DIALECT_V6, NULL) == 0)
					{
						XSQLDA* out_sqlda = (XSQLDA*) malloc(XSQLDA_LENGTH(7));
						out_sqlda->version = SQLDA_VERSION1;
						out_sqlda->sqln = 7;
						
						if (isc_dsql_describe(isc_status, &stmt_handle, SQL_DIALECT_V6, out_sqlda) == 0)
						{
							char user_name_buf[256];
							SLONG uid_buf, gid_buf;
							char group_name_buf[256], first_name_buf[256], middle_name_buf[256], last_name_buf[256];
							short uid_null, gid_null, group_name_null, first_name_null, middle_name_null, last_name_null;
							
							out_sqlda->sqlvar[0].sqldata = user_name_buf;
							out_sqlda->sqlvar[0].sqlind = NULL;
							out_sqlda->sqlvar[1].sqldata = (char*) &uid_buf;
							out_sqlda->sqlvar[1].sqlind = &uid_null;
							out_sqlda->sqlvar[2].sqldata = (char*) &gid_buf;
							out_sqlda->sqlvar[2].sqlind = &gid_null;
							out_sqlda->sqlvar[3].sqldata = group_name_buf;
							out_sqlda->sqlvar[3].sqlind = &group_name_null;
							out_sqlda->sqlvar[4].sqldata = first_name_buf;
							out_sqlda->sqlvar[4].sqlind = &first_name_null;
							out_sqlda->sqlvar[5].sqldata = middle_name_buf;
							out_sqlda->sqlvar[5].sqlind = &middle_name_null;
							out_sqlda->sqlvar[6].sqldata = last_name_buf;
							out_sqlda->sqlvar[6].sqlind = &last_name_null;
							
							if (isc_dsql_execute(isc_status, &transaction, &stmt_handle, SQL_DIALECT_V6, NULL) == 0)
							{
								while (isc_dsql_fetch(isc_status, &stmt_handle, SQL_DIALECT_V6, out_sqlda) == 0)
								{
									try
									{
										{
											ScratchBird::string attr, a1, a2, a3;

											if (!uid_null)
												a1.printf("Uid=%d\n", uid_buf);

											if (!gid_null)
												a2.printf("Gid=%d\n", gid_buf);

											if (!group_name_null)
												a3.printf("GroupName=%s\n", group_name_buf);

											attr = a1 + a2 + a3;
											user->attributes()->set(s, attr.c_str());
											check(s);
											user->attributes()->setEntered(s, attr.hasData() ? 1 : 0);
											check(s);
										}

										user->userName()->set(s, user_name_buf);
										check(s);
										user->userName()->setEntered(s, 1);
										check(s);
										user->password()->set(s, "");
										check(s);
										user->password()->setEntered(s, 0);
										check(s);
										user->firstName()->set(s, first_name_null ? "" : first_name_buf);
										check(s);
										user->firstName()->setEntered(s, first_name_null ? 0 : 1);
										check(s);
										user->middleName()->set(s, middle_name_null ? "" : middle_name_buf);
										check(s);
										user->middleName()->setEntered(s, middle_name_null ? 0 : 1);
										check(s);
										user->lastName()->set(s, last_name_null ? "" : last_name_buf);
										check(s);
										user->lastName()->setEntered(s, last_name_null ? 0 : 1);
										check(s);

										user->admin()->set(s, 0);
										check(s);
										user->admin()->setEntered(s, 1);
										check(s);

										// Converted FOR loop #5: FOR (TRANSACTION_HANDLE transaction REQUEST_HANDLE request2) P IN RDB$USER_PRIVILEGES
										isc_req_handle priv_handle = 0;
										const char* priv_sql = "SELECT RDB$USER FROM RDB$USER_PRIVILEGES WHERE RDB$USER = ? AND RDB$RELATION_NAME = 'RDB$ADMIN' AND RDB$PRIVILEGE = 'M'";
										
										if (isc_dsql_allocate_statement(isc_status, &database, &priv_handle) == 0)
										{
											if (isc_dsql_prepare(isc_status, &transaction, &priv_handle, 0, priv_sql, SQL_DIALECT_V6, NULL) == 0)
											{
												XSQLDA* priv_in_sqlda = (XSQLDA*) malloc(XSQLDA_LENGTH(1));
												priv_in_sqlda->version = SQLDA_VERSION1;
												priv_in_sqlda->sqln = 1;
												priv_in_sqlda->sqld = 1;
												priv_in_sqlda->sqlvar[0].sqltype = SQL_TEXT;
												priv_in_sqlda->sqlvar[0].sqllen = strlen(user_name_buf);
												priv_in_sqlda->sqlvar[0].sqldata = user_name_buf;
												priv_in_sqlda->sqlvar[0].sqlind = NULL;
												
												XSQLDA* priv_out_sqlda = (XSQLDA*) malloc(XSQLDA_LENGTH(1));
												priv_out_sqlda->version = SQLDA_VERSION1;
												priv_out_sqlda->sqln = 1;
												
												if (isc_dsql_describe(isc_status, &priv_handle, SQL_DIALECT_V6, priv_out_sqlda) == 0)
												{
													char priv_user_buf[256];
													priv_out_sqlda->sqlvar[0].sqldata = priv_user_buf;
													
													if (isc_dsql_execute(isc_status, &transaction, &priv_handle, SQL_DIALECT_V6, priv_in_sqlda) == 0)
													{
														while (isc_dsql_fetch(isc_status, &priv_handle, SQL_DIALECT_V6, priv_out_sqlda) == 0)
														{
															user->admin()->set(s, 1);
														}
													}
												}
												
												free(priv_in_sqlda);
												free(priv_out_sqlda);
											}
											isc_dsql_free_statement(isc_status, &priv_handle, DSQL_drop);
										}
										check(s);

										callback->list(s, user);
										check(s);

										found = true;
									}
									catch (const ScratchBird::Exception& ex)
									{
										ex.stuffException(s);
										s = &statusWrapper2;
									}
								}
							}
						}
						free(out_sqlda);
					}
					else
						ret = GsecMsg28;
					
					isc_dsql_free_statement(isc_status, &stmt_handle, DSQL_drop);
				}
				else
					ret = GsecMsg28;

				// real error raise - out of database operation
				check(st);
			}
			else
			{
				ScratchBird::string attr, a1, a2, a3;
				ScratchBird::LocalStatus s2;
				ScratchBird::CheckStatusWrapper statusWrapper2(&s2);
				ScratchBird::CheckStatusWrapper* s = st;

				// Converted FOR loop #6: FOR (TRANSACTION_HANDLE transaction REQUEST_HANDLE request) U IN PLG$VIEW_USERS WITH U.PLG$USER_NAME EQ user->userName()->get()
				isc_req_handle stmt_handle = 0;
				const char* select_sql = "SELECT PLG$USER_NAME, PLG$UID, PLG$GID, PLG$GROUP_NAME, PLG$FIRST_NAME, PLG$MIDDLE_NAME, PLG$LAST_NAME FROM PLG$VIEW_USERS WHERE PLG$USER_NAME = ?";
				
				if (isc_dsql_allocate_statement(isc_status, &database, &stmt_handle) == 0)
				{
					if (isc_dsql_prepare(isc_status, &transaction, &stmt_handle, 0, select_sql, SQL_DIALECT_V6, NULL) == 0)
					{
						XSQLDA* in_sqlda = (XSQLDA*) malloc(XSQLDA_LENGTH(1));
						in_sqlda->version = SQLDA_VERSION1;
						in_sqlda->sqln = 1;
						in_sqlda->sqld = 1;
						in_sqlda->sqlvar[0].sqltype = SQL_TEXT;
						in_sqlda->sqlvar[0].sqllen = strlen(user->userName()->get());
						in_sqlda->sqlvar[0].sqldata = (char*) user->userName()->get();
						in_sqlda->sqlvar[0].sqlind = NULL;
						
						XSQLDA* out_sqlda = (XSQLDA*) malloc(XSQLDA_LENGTH(7));
						out_sqlda->version = SQLDA_VERSION1;
						out_sqlda->sqln = 7;
						
						if (isc_dsql_describe(isc_status, &stmt_handle, SQL_DIALECT_V6, out_sqlda) == 0)
						{
							char user_name_buf[256];
							SLONG uid_buf, gid_buf;
							char group_name_buf[256], first_name_buf[256], middle_name_buf[256], last_name_buf[256];
							short uid_null, gid_null, group_name_null, first_name_null, middle_name_null, last_name_null;
							
							out_sqlda->sqlvar[0].sqldata = user_name_buf;
							out_sqlda->sqlvar[0].sqlind = NULL;
							out_sqlda->sqlvar[1].sqldata = (char*) &uid_buf;
							out_sqlda->sqlvar[1].sqlind = &uid_null;
							out_sqlda->sqlvar[2].sqldata = (char*) &gid_buf;
							out_sqlda->sqlvar[2].sqlind = &gid_null;
							out_sqlda->sqlvar[3].sqldata = group_name_buf;
							out_sqlda->sqlvar[3].sqlind = &group_name_null;
							out_sqlda->sqlvar[4].sqldata = first_name_buf;
							out_sqlda->sqlvar[4].sqlind = &first_name_null;
							out_sqlda->sqlvar[5].sqldata = middle_name_buf;
							out_sqlda->sqlvar[5].sqlind = &middle_name_null;
							out_sqlda->sqlvar[6].sqldata = last_name_buf;
							out_sqlda->sqlvar[6].sqlind = &last_name_null;
							
							if (isc_dsql_execute(isc_status, &transaction, &stmt_handle, SQL_DIALECT_V6, in_sqlda) == 0)
							{
								while (isc_dsql_fetch(isc_status, &stmt_handle, SQL_DIALECT_V6, out_sqlda) == 0)
								{
									try
									{
										if (!uid_null)
											a1.printf("Uid=%d\n", uid_buf);

										if (!gid_null)
											a2.printf("Gid=%d\n", gid_buf);

										if (!group_name_null)
											a3.printf("GroupName=%s\n", group_name_buf);

										attr = a1 + a2 + a3;
										user->attributes()->set(s, attr.c_str());
										check(s);
										user->attributes()->setEntered(s, attr.hasData() ? 1 : 0);
										check(s);

										user->userName()->set(s, user_name_buf);
										check(s);
										user->userName()->setEntered(s, 1);
										check(s);
										user->password()->set(s, "");
										check(s);
										user->password()->setEntered(s, 0);
										check(s);
										user->firstName()->set(s, first_name_null ? "" : first_name_buf);
										check(s);
										user->firstName()->setEntered(s, first_name_null ? 0 : 1);
										check(s);
										user->middleName()->set(s, middle_name_null ? "" : middle_name_buf);
										check(s);
										user->middleName()->setEntered(s, middle_name_null ? 0 : 1);
										check(s);
										user->lastName()->set(s, last_name_null ? "" : last_name_buf);
										check(s);
										user->lastName()->setEntered(s, last_name_null ? 0 : 1);
										check(s);

										user->admin()->set(s, 0);
										check(s);
										user->admin()->setEntered(s, 1);
										check(s);

										// Converted FOR loop #7: FOR (TRANSACTION_HANDLE transaction REQUEST_HANDLE request2) P IN RDB$USER_PRIVILEGES
										isc_req_handle priv_handle = 0;
										const char* priv_sql = "SELECT RDB$USER FROM RDB$USER_PRIVILEGES WHERE RDB$USER = ? AND RDB$RELATION_NAME = 'RDB$ADMIN' AND RDB$PRIVILEGE = 'M'";
										
										if (isc_dsql_allocate_statement(isc_status, &database, &priv_handle) == 0)
										{
											if (isc_dsql_prepare(isc_status, &transaction, &priv_handle, 0, priv_sql, SQL_DIALECT_V6, NULL) == 0)
											{
												XSQLDA* priv_in_sqlda = (XSQLDA*) malloc(XSQLDA_LENGTH(1));
												priv_in_sqlda->version = SQLDA_VERSION1;
												priv_in_sqlda->sqln = 1;
												priv_in_sqlda->sqld = 1;
												priv_in_sqlda->sqlvar[0].sqltype = SQL_TEXT;
												priv_in_sqlda->sqlvar[0].sqllen = strlen(user_name_buf);
												priv_in_sqlda->sqlvar[0].sqldata = user_name_buf;
												priv_in_sqlda->sqlvar[0].sqlind = NULL;
												
												XSQLDA* priv_out_sqlda = (XSQLDA*) malloc(XSQLDA_LENGTH(1));
												priv_out_sqlda->version = SQLDA_VERSION1;
												priv_out_sqlda->sqln = 1;
												
												if (isc_dsql_describe(isc_status, &priv_handle, SQL_DIALECT_V6, priv_out_sqlda) == 0)
												{
													char priv_user_buf[256];
													priv_out_sqlda->sqlvar[0].sqldata = priv_user_buf;
													
													if (isc_dsql_execute(isc_status, &transaction, &priv_handle, SQL_DIALECT_V6, priv_in_sqlda) == 0)
													{
														while (isc_dsql_fetch(isc_status, &priv_handle, SQL_DIALECT_V6, priv_out_sqlda) == 0)
														{
															user->admin()->set(s, 1);
														}
													}
												}
												
												free(priv_in_sqlda);
												free(priv_out_sqlda);
											}
											isc_dsql_free_statement(isc_status, &priv_handle, DSQL_drop);
										}
										check(s);

										callback->list(s, user);
										check(s);

										found = true;
									}
									catch (const ScratchBird::Exception& ex)
									{
										ex.stuffException(s);
										s = &statusWrapper2;
									}
								}
							}
						}
						
						free(in_sqlda);
						free(out_sqlda);
					}
					else
						ret = GsecMsg28;
					
					isc_dsql_free_statement(isc_status, &stmt_handle, DSQL_drop);
				}
				else
					ret = GsecMsg28;

				// real error raise - out of database operation
				check(st);
			}
			break;

		default:
			ret = GsecMsg16;		// gsec - error in switch specifications
			break;
		}

		if (request)
		{
			ISC_STATUS_ARRAY s;
			if (isc_release_request(s, &request) != FB_SUCCESS)
			{
				if (! ret)
				{
					ret = GsecMsg94;	// error releasing request in security database
				}
			}
		}

		if (request2)
		{
			ISC_STATUS_ARRAY s;
			if (isc_release_request(s, &request2) != FB_SUCCESS)
			{
				if (! ret)
				{
					ret = GsecMsg94;	// error releasing request in security database
				}
			}
		}

		fb_utils::setIStatus(st, isc_status);
	}
	catch (const ScratchBird::Exception& ex)
	{
		ex.stuffException(st);
	}

	return ret;
}

} // namespace Auth

// register plugin
static ScratchBird::SimpleFactory<Auth::SecurityDatabaseManagement> factory;

extern "C" FB_DLL_EXPORT void FB_PLUGIN_ENTRY_POINT(ScratchBird::IMaster* master)
{
	ScratchBird::CachedMasterInterface::set(master);
	ScratchBird::PluginManagerInterfacePtr()->registerPluginFactory(
		ScratchBird::IPluginManager::TYPE_AUTH_USER_MANAGEMENT, "Legacy_UserManager", &factory);
	ScratchBird::getUnloadDetector()->registerMe();
}