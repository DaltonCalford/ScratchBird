/*
 *
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
 * 					Alex Peshkoff
 */

#include "scratchbird.h"
#include "../common/security.h"
#include "../common/StatusArg.h"
// #include "../utilities/gsec/gsec.h"		// gsec error codes - File not found, commented out
#include "../common/db_alias.h"
#include "../include/firebird/iberror.h"
#include "../include/scratchbird/IdlFbInterfaces.h"
// #include "../include/scratchbird/impl/msg/gsec.h"  // Temporarily disabled due to MSG_SYMBOL issues


using namespace ScratchBird;

namespace {

void raise()
{
	ScratchBird::Arg::Gds(isc_user_manager).raise();
}

} // anonymous namespace

namespace Auth {

Get::Get(const ScratchBird::Config* firebirdConf)
	: GetPlugins<ScratchBird::IManagement>(ScratchBird::IPluginManager::TYPE_AUTH_USER_MANAGEMENT, firebirdConf)
{
	if (!hasData())
	{
		raise();
	}
}

Get::Get(const ScratchBird::Config* firebirdConf, const char* plugName)
	: GetPlugins<ScratchBird::IManagement>(ScratchBird::IPluginManager::TYPE_AUTH_USER_MANAGEMENT, firebirdConf, plugName)
{
	if (!hasData())
	{
		raise();
	}
}

void UserData::clear(ScratchBird::CheckStatusWrapper*)
{
	op = 0;

	// interface fields
	user.clear();
	pass.clear();
	first.clear();
	last.clear();
	middle.clear();
	com.clear();
	attr.clear();
	adm.clear();
	act.clear();

	// internally used fields
	database.clear();
	dba.clear();
	dbaPassword.clear();
	role.clear();

	// never clear this permanent block!	authenticationBlock.clear();

	// internal support for deprecated fields
	group.clear();
	u.clear();
	g.clear();
}

// This function sets typical gsec return code based on requested operation if it was not set by plugin
int setGsecCode(int code, unsigned int operation)
{
	if (code >= 0)
	{
		return code;
	}

	switch(operation)
	{
	case ADD_OPER:
		return 19; // GsecMsg19 - temporarily using code numbers

	case MOD_OPER:
		return 20; // GsecMsg20;

	case DEL_OPER:
		return 23; // GsecMsg23;

	case OLD_DIS_OPER:
	case DIS_OPER:
		return 28; // GsecMsg28;

	case MAP_DROP_OPER:
	case MAP_SET_OPER:
		return 97; // GsecMsg97;
	}

	return 17; // GsecMsg17;
}

} // namespace Auth
