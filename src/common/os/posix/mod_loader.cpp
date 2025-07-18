/*
 *	PROGRAM:		JRD Module Loader
 *	MODULE:			mod_loader.cpp
 *	DESCRIPTION:	POSIX-specific class for loadable modules.
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
 *  The Original Code was created by John Bellardo
 *  for the ScratchBird Open Source RDBMS project.
 *
 *  Copyright (c) 2002 John Bellardo <bellardo at cs.ucsd.edu>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 *
 */

#include "firebird.h"
#include "../common/os/mod_loader.h"
#include "../common/os/os_utils.h"
#include "../common/os/path_utils.h"
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <limits.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <link.h>
#include <dlfcn.h>

/// This is the POSIX (dlopen) implementation of the mod_loader abstraction.

class DlfcnModule : public ModuleLoader::Module
{
public:
	DlfcnModule(MemoryPool& pool, const ScratchBird::PathName& aFileName, void* m);
	~DlfcnModule();

	void* findSymbol(ISC_STATUS*, const ScratchBird::string&) override;

	bool getRealPath(const ScratchBird::string& anySymbol, ScratchBird::PathName& path) override;

private:
	void* module;
	ScratchBird::PathName realPath;
};

static void makeErrorStatus(ISC_STATUS* status, const char* text)
{
	if (status)
	{
		status[0] = isc_arg_gds;
		status[1] = isc_random;
		status[2] = isc_arg_string;
		status[3] = (ISC_STATUS) text;
		status[4] = isc_arg_end;
	}
}

bool ModuleLoader::isLoadableModule(const ScratchBird::PathName& module)
{
	struct STAT sb;

	if (-1 == os_utils::stat(module.c_str(), &sb))
		return false;

	if ( ! (sb.st_mode & S_IFREG) )		// Make sure it is a plain file
		return false;

	if ( -1 == access(module.c_str(), R_OK | X_OK))
		return false;

	return true;
}

bool ModuleLoader::doctorModuleExtension(ScratchBird::PathName& name, int& step)
{
	if (name.isEmpty())
		return false;

	switch (step++)
	{
	case 0: // Step 0: append missing extension
		{
			ScratchBird::PathName::size_type pos = name.rfind("." SHRLIB_EXT);
			if (pos != name.length() - 3)
			{
				pos = name.rfind("." SHRLIB_EXT ".");
				if (pos == ScratchBird::PathName::npos)
				{
					name += "." SHRLIB_EXT;
					return true;
				}
			}
			step++; // instead of break
		}
	case 1: // Step 1: insert missing prefix
		{
			ScratchBird::PathName::size_type pos = name.rfind('/');
			pos = (pos == ScratchBird::PathName::npos) ? 0 : pos + 1;
			if (name.find("lib", pos) != pos)
			{
				name.insert(pos, "lib");
				return true;
			}
		}
	}
	return false;
}

#ifdef DEV_BUILD
#define FB_RTLD_MODE RTLD_NOW	// make sure nothing left unresolved
#else
#define FB_RTLD_MODE RTLD_LAZY	// save time when loading library
#endif

ModuleLoader::Module* ModuleLoader::loadModule(ISC_STATUS* status, const ScratchBird::PathName& modPath)
{
	void* module = dlopen(modPath.nullStr(), FB_RTLD_MODE);
	if (module == NULL)
	{
		makeErrorStatus(status, dlerror());
		return 0;
	}

#ifdef DEBUG_THREAD_IN_UNLOADED_LIBRARY
	ScratchBird::string command;
	command.printf("echo +++ %s +++ >>/tmp/fbmaps;date >> /tmp/fbmaps;cat /proc/%d/maps >>/tmp/fbmaps",
		modPath.c_str(), getpid());
	system(command.c_str());
#endif

	ScratchBird::PathName linkPath = modPath;
	char b[PATH_MAX];
	const char* newPath = realpath(modPath.c_str(), b);
	if (newPath)
		linkPath = newPath;

	return FB_NEW_POOL(*getDefaultMemoryPool()) DlfcnModule(*getDefaultMemoryPool(), linkPath, module);
}

DlfcnModule::DlfcnModule(MemoryPool& pool, const ScratchBird::PathName& aFileName, void* m)
	: ModuleLoader::Module(pool, aFileName),
	  module(m),
	  realPath(pool)
{
	getRealPath("", realPath);
}

DlfcnModule::~DlfcnModule()
{
	if (module)
		dlclose(module);
}

void* DlfcnModule::findSymbol(ISC_STATUS* status, const ScratchBird::string& symName)
{
	void* result = dlsym(module, symName.c_str());

	if (!result)
	{
		ScratchBird::string newSym = '_' + symName;
		result = dlsym(module, newSym.c_str());
	}

	if (!result)
	{
		makeErrorStatus(status, dlerror());
		return NULL;
	}

#ifdef HAVE_DLADDR
	Dl_info info;
	if (!dladdr(result, &info))
	{
		makeErrorStatus(status, dlerror());
		return NULL;
	}

	const auto& libraryPath = realPath.isEmpty() ? fileName : realPath;

	char symbolPathBuffer[PATH_MAX];
	const char* symbolPath = symbolPathBuffer;

	if (!realpath(info.dli_fname, symbolPathBuffer))
		symbolPath = info.dli_fname;

	const char* errText = "Actual module name does not match requested";
	if (PathUtils::isRelative(libraryPath) || PathUtils::isRelative(symbolPath))
	{
		// check only name (not path) of the library
		ScratchBird::PathName dummyDir, nm1, nm2;
		PathUtils::splitLastComponent(dummyDir, nm1, libraryPath);
		PathUtils::splitLastComponent(dummyDir, nm2, symbolPath);
		if (nm1 != nm2)
		{
			makeErrorStatus(status, errText);
			return NULL;
		}
	}
	else if (libraryPath != symbolPath)
	{
		makeErrorStatus(status, errText);
		return NULL;
	}
#endif

	return result;
}

bool DlfcnModule::getRealPath(const ScratchBird::string& anySymbol, ScratchBird::PathName& path)
{
	if (realPath.hasData())
	{
		path = realPath;
		return true;
	}

	char buffer[PATH_MAX];

#ifdef HAVE_DLINFO
#ifdef HAVE_RTLD_DI_ORIGIN
	if (dlinfo(module, RTLD_DI_ORIGIN, buffer) == 0)
	{
		path = buffer;
		path += '/';
		path += fileName;

		if (realpath(path.c_str(), buffer))
		{
			path = buffer;
			return true;
		}
	}
#endif

#ifdef HAVE_RTLD_DI_LINKMAP
	struct link_map* lm;
	if (dlinfo(module, RTLD_DI_LINKMAP, &lm) == 0)
	{
		if (realpath(lm->l_name, buffer))
		{
			path = buffer;
			return true;
		}
	}
#endif

#endif

#ifdef HAVE_DLADDR
	if (anySymbol.hasData())
	{
		void* symbolPtr = dlsym(module, anySymbol.c_str());

		if (!symbolPtr)
			symbolPtr = dlsym(module, ('_' + anySymbol).c_str());

		if (symbolPtr)
		{
			Dl_info info;
			if (dladdr(symbolPtr, &info))
			{
				if (realpath(info.dli_fname, buffer))
				{
					path = buffer;
					return true;
				}
			}
		}
	}
#endif	// HAVE_DLADDR

	path.clear();
	return false;
}
