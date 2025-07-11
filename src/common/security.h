/*
 *	PROGRAM:	Security data base manager
 *	MODULE:		secur_proto.h
 *	DESCRIPTION:	Prototype header file for security.epp
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

#ifndef UTILITIES_SECUR_PROTO_H
#define UTILITIES_SECUR_PROTO_H

#include "firebird/Interface.h"
#include "../common/classes/ImplementHelper.h"
#include "../common/classes/GetPlugins.h"
#include "../common/classes/array.h"
#include "../common/classes/MetaString.h"
#include "../common/classes/objects_array.h"

namespace Auth {

class CharField :
	public ScratchBird::AutoIface<ScratchBird::ICharUserFieldImpl<CharField, ScratchBird::CheckStatusWrapper> >
{
public:
	CharField()
		: e(0), s(0), value(*getDefaultMemoryPool())
	{ }

	// ICharUserField implementation
	int entered()
	{
		return e;
	}

	int specified()
	{
		return s;
	}

	void setEntered(ScratchBird::CheckStatusWrapper*, int newValue)
	{
		e = newValue;
	}

	void setSpecified(int newValue)
	{
		s = newValue;
		if (s)
		{
			value = "";
		}
	}

	const char* get()
	{
		return value.c_str();
	}

	void set(ScratchBird::CheckStatusWrapper* status, const char* newValue)
	{
		try
		{
			value = newValue ? newValue : "";
		}
		catch (const ScratchBird::Exception& ex)
		{
			ex.stuffException(status);
		}
	}

	void set(const char* newValue, FB_SIZE_T len)
	{
		value.assign(newValue, len);
	}

	void clear() noexcept
	{
		e = s = 0;
		value.erase();		// should not call allocation function - no throw
	}

private:
	int e, s;
	ScratchBird::string value;
};

class IntField :
	public ScratchBird::AutoIface<ScratchBird::IIntUserFieldImpl<IntField, ScratchBird::CheckStatusWrapper> >
{
public:
	IntField()
		: e(0), s(0), value(0)
	{ }

	// IIntUserField implementation
	int entered()
	{
		return e;
	}

	int specified()
	{
		return s;
	}

	void setEntered(ScratchBird::CheckStatusWrapper*, int newValue)
	{
		e = newValue;
	}

	void setSpecified(int newValue)
	{
		s = newValue;
		if (s)
		{
			value = 0;
		}
	}

	int get()
	{
		return value;
	}

	void set(ScratchBird::CheckStatusWrapper*, int newValue)
	{
		value = newValue;
	}

	void clear() noexcept
	{
		e = s = 0;
		value = 0;
	}

private:
	int e, s;
	int value;
};

typedef ScratchBird::Array<UCHAR> AuthenticationBlock;

class UserData final :
	public ScratchBird::VersionedIface<ScratchBird::IUserImpl<UserData, ScratchBird::CheckStatusWrapper> >
{
public:
	UserData()
		: op(0), trustedAuth(0), silent(false), authenticationBlock(*getDefaultMemoryPool())
	{ }

	// IUser implementation
	unsigned int operation()
	{
		return op;
	}

	ScratchBird::ICharUserField* userName()
	{
		return &user;
	}

	ScratchBird::ICharUserField* password()
	{
		return &pass;
	}

	ScratchBird::ICharUserField* firstName()
	{
		return &first;
	}

	ScratchBird::ICharUserField* lastName()
	{
		return &last;
	}

	ScratchBird::ICharUserField* middleName()
	{
		return &middle;
	}

	ScratchBird::ICharUserField* comment()
	{
		return &com;
	}

	ScratchBird::ICharUserField* attributes()
	{
		return &attr;
	}

	ScratchBird::IIntUserField* admin()
	{
		return &adm;
	}

	ScratchBird::IIntUserField* active()
	{
		return &act;
	}

	void clear(ScratchBird::CheckStatusWrapper* status);


	unsigned int op;
	int trustedAuth;
	bool silent;
	bool createIfNotExistsOnly = false;
	CharField user, pass, first, last, middle, com, attr;
	IntField adm, act;
	CharField database, dba, dbaPassword, role;
	AuthenticationBlock authenticationBlock;

	ScratchBird::MetaString plugin;

	// deprecated
	CharField group;
	IntField u, g;
};

class Get : public ScratchBird::GetPlugins<ScratchBird::IManagement>
{
public:
	explicit Get(const ScratchBird::Config* firebirdConf);
	Get(const ScratchBird::Config* firebirdConf, const char* plugName);
};

int setGsecCode(int code, unsigned int operation);

} // namespace Auth

#endif // UTILITIES_SECUR_PROTO_H
