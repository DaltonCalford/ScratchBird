/*
 *	PROGRAM:		ScratchBird authentication
 *	MODULE:			Auth.cpp
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
 */

#include "firebird.h"
#include "../common/Auth.h"
#include "ibase.h"
#include "../common/classes/ImplementHelper.h"
#include "../common/utils_proto.h"
#include "../common/db_alias.h"

using namespace ScratchBird;

namespace Auth {

WriterImplementation::WriterImplementation()
	: current(*getDefaultMemoryPool(), ClumpletReader::WideUnTagged, MAX_DPB_SIZE),
	  result(*getDefaultMemoryPool(), ClumpletReader::WideUnTagged, MAX_DPB_SIZE),
	  plugin(*getDefaultMemoryPool()), type(*getDefaultMemoryPool()),
	  sequence(0)
{ }

void WriterImplementation::store(ClumpletWriter* to, unsigned char tag)
{
	putLevel();
	if (to)
	{
		to->deleteWithTag(tag);
		to->insertBytes(tag, result.getBuffer(), result.getBufferLength());
	}
}

void WriterImplementation::reset()
{
	result.clear();
	current.clear();
	sequence = 0;
}

void WriterImplementation::add(ScratchBird::CheckStatusWrapper* st, const char* name)
{
	try
	{
		putLevel();

		current.clear();
		current.insertString(AuthReader::AUTH_NAME, name, strlen(name));
		fb_assert(plugin.hasData());
		if (plugin.hasData())
		{
			current.insertString(AuthReader::AUTH_PLUGIN, plugin);
		}
		type = "USER";
	}
	catch (const ScratchBird::Exception& ex)
	{
		ex.stuffException(st);
	}
}

void WriterImplementation::setPlugin(const char* m)
{
	plugin = m;
}

void WriterImplementation::putLevel()
{
	current.rewind();
	if (current.isEof())
	{
		return;
	}
	current.insertString(AuthReader::AUTH_TYPE, type);

	result.insertBytes(sequence++, current.getBuffer(), current.getBufferLength());
}

void WriterImplementation::setType(ScratchBird::CheckStatusWrapper* st, const char* value)
{
	try
	{
		if (value)
			type = value;
	}
	catch (const ScratchBird::Exception& ex)
	{
		ex.stuffException(st);
	}
}

void WriterImplementation::setDb(ScratchBird::CheckStatusWrapper* st, const char* value)
{
	try
	{
		if (value)
		{
			PathName target;
			expandDatabaseName(value, target, NULL);
			current.insertString(AuthReader::AUTH_SECURE_DB, target);
		}
	}
	catch (const ScratchBird::Exception& ex)
	{
		ex.stuffException(st);
	}
}

} // namespace Auth
