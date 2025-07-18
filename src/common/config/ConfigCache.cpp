/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		ConfigCache.cpp
 *	DESCRIPTION:	Base for class, representing cached config file.
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
 *  The Original Code was created by Alexander Peshkoff
 *  for the ScratchBird Open Source RDBMS project.
 *
 *  Copyright (c) 2010 Alexander Peshkoff <peshkoff@mail.ru>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 *
 */

#include "../common/config/ConfigCache.h"
#include "../common/config/config_file.h"
#include "../common/os/os_utils.h"

#include "iberror.h"

#include <sys/types.h>
#include <sys/stat.h>

#ifndef WIN_NT
#include <unistd.h>
#else
#include <errno.h>
#endif

using namespace ScratchBird;

ConfigCache::ConfigCache(MemoryPool& p, const PathName& fName)
	: PermanentStorage(p), files(FB_NEW_POOL(getPool()) ConfigCache::File(getPool(), fName))
{ }

ConfigCache::~ConfigCache()
{
	delete files;
}

void ConfigCache::checkLoadConfig()
{
	{	// scope
		ReadLockGuard guard(rwLock, "ConfigCache::checkLoadConfig");
		if (files->checkLoadConfig(false))
		{
			return;
		}
	}

	WriteLockGuard guard(rwLock, "ConfigCache::checkLoadConfig");
	// may be someone already reloaded?
	if (files->checkLoadConfig(true))
	{
		return;
	}

	files->trim();
	loadConfig();
}

bool ConfigCache::addFile(const ScratchBird::PathName& fName)
{
	return files->add(fName);
}

ScratchBird::PathName ConfigCache::getFileName()
{
	return files->fileName;
}

time_t ConfigCache::File::getTime()
{
	struct STAT st;

	if (os_utils::stat(fileName.c_str(), &st) != 0)
	{
		if (errno == ENOENT)
		{
			// config file is missing, but this is not our problem
			return 0;
		}
		system_call_failed::raise("stat");
	}

	return st.st_mtime;
}

ConfigCache::File::File(MemoryPool& p, const PathName& fName)
	: PermanentStorage(p), fileName(getPool(), fName), fileTime(0), next(NULL)
{ }

ConfigCache::File::~File()
{
	delete next;
}

bool ConfigCache::File::checkLoadConfig(bool set)
{
	time_t newTime = getTime();
	if (fileTime == newTime)
	{
		return next ? next->checkLoadConfig(set) : true;
	}

	if (set)
	{
		fileTime = newTime;
		if (next)
		{
			next->checkLoadConfig(set);
		}
	}
	return false;
}

bool ConfigCache::File::add(const PathName& fName)
{
	if (fName == fileName)
	{
		return false;
	}

	if (next)
	{
		return next->add(fName);
	}
	else
	{
		next = FB_NEW_POOL(getPool()) ConfigCache::File(getPool(), fName);
		next->checkLoadConfig(true); // To set current time and avoid unnecessary invalidation
	}

	return true;
}

void ConfigCache::File::trim()
{
	delete next;
	next = NULL;
}
