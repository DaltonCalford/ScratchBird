/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		ConfigCache.h
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

#ifndef COMMON_CONFIG_CASHE_H
#define COMMON_CONFIG_CASHE_H

#include "../common/classes/alloc.h"
#include "../common/classes/fb_string.h"
#include "../common/classes/rwlock.h"

class ConfigCache : public ScratchBird::PermanentStorage
{
public:
	ConfigCache(ScratchBird::MemoryPool& p, const ScratchBird::PathName& fName);
	virtual ~ConfigCache();

	void checkLoadConfig();
	bool addFile(const ScratchBird::PathName& fName);	// Returns true if file was added.
	ScratchBird::PathName getFileName();

protected:
	virtual void loadConfig() = 0;

private:
	class File : public ScratchBird::PermanentStorage
	{
	public:
		File(ScratchBird::MemoryPool& p, const ScratchBird::PathName& fName);
		~File();

		bool checkLoadConfig(bool set);
		bool add(const ScratchBird::PathName& fName);	// Returns true if file was added.
		void trim();

	public:
		ScratchBird::PathName fileName;

	private:
		volatile time_t fileTime;
		File* next;
		time_t getTime();
	};
	File* files;

public:
	ScratchBird::RWLock rwLock;
};

#endif // COMMON_CONFIG_CASHE_H
