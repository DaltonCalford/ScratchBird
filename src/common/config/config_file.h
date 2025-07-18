/*
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
 *  The Original Code was created by Dmitry Yemanov
 *  for the ScratchBird Open Source RDBMS project.
 *
 *  Copyright (c) 2002 Dmitry Yemanov <dimitr@users.sf.net>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#ifndef CONFIG_CONFIG_FILE_H
#define CONFIG_CONFIG_FILE_H

#include "../common/classes/alloc.h"
#include "../common/classes/fb_pair.h"
#include "../common/classes/objects_array.h"
#include "../common/classes/fb_string.h"
#include "../common/classes/auto.h"

/**
	Since the original (isc.cpp) code wasn't able to provide powerful and
	easy-to-use abilities to work with complex configurations, a decision
	has been made to create a completely new one.

	The below class implements generic file abstraction for new configuration
	manager. It allows "value-by-key" retrieval based on plain text files. Both
	keys and values are just strings that may be handled case-sensitively or
	case-insensitively, depending on host OS. The configuration file is loaded
	on demand, its current status can be checked with isLoaded() member function.
	All leading and trailing spaces are ignored. Comments (follow after a
	hash-mark) are ignored as well.

	Now this implementation is used by generic configuration manager
	(common/config/config.cpp) and server-side alias manager (common/db_alias.cpp).
**/

class ConfigCache;

class ConfigFile : public ScratchBird::AutoStorage, public ScratchBird::RefCounted
{
public:
	// flags for config file
	static const USHORT HAS_SUB_CONF	= 0x01;
	static const USHORT ERROR_WHEN_MISS	= 0x02;
	static const USHORT NATIVE_ORDER	= 0x04;
	static const USHORT NO_COMMENTS		= 0x08;
	static const USHORT CUSTOM_MACROS	= 0x10;
	static const USHORT REGEXP_SUPPORT	= 0x20;

	// enum to distinguish ctors
	enum UseText {USE_TEXT};

	// config_file strings are mostly case sensitive
	typedef ScratchBird::string String;
	// keys are case-insensitive
	typedef ScratchBird::NoCaseString KeyType;

	class Stream
	{
	public:
		virtual ~Stream();
		virtual bool getLine(String&, unsigned int&) = 0;
		virtual const char* getFileName() const = 0;
	};

	struct Parameter : public AutoStorage
	{
		Parameter(MemoryPool& p, const Parameter& par)
			: AutoStorage(p), name(getPool(), par.name), value(getPool(), par.value),
			  sub(par.sub), line(par.line), hasValue(par.hasValue)
		{ }
		Parameter()
			: AutoStorage(), name(getPool()), value(getPool()), line(0), hasValue(false)
		{ }

		SINT64 asInteger() const;
		bool asBoolean() const;

		KeyType name;
		String value;
		ScratchBird::RefPtr<ConfigFile> sub;
		unsigned int line;
		bool hasValue;

		static const KeyType* generate(const Parameter* item)
		{
			return &item->name;
		}
	};

    typedef ScratchBird::SortedObjectsArray<Parameter, ScratchBird::InlineStorage<Parameter*, 100>,
										 KeyType, Parameter> Parameters;
	typedef ScratchBird::ObjectsArray<ScratchBird::PathName> FilesArray;

	ConfigFile(const ScratchBird::PathName& file, USHORT fl = 0, ConfigCache* cache = NULL);
	ConfigFile(const char* file, USHORT fl = 0, ConfigCache* cache = NULL);
	ConfigFile(UseText, const char* configText, USHORT fl = 0);

	ConfigFile(MemoryPool& p, const ScratchBird::PathName& file, USHORT fl = 0, ConfigCache* cache = NULL);

private:
	ConfigFile(MemoryPool& p, ConfigFile::Stream* s, USHORT fl);

public:
	// key and value management
	const Parameter* findParameter(const KeyType& name) const;
	const Parameter* findParameter(const KeyType& name, const String& value) const;

	// all parameters access
	const Parameters& getParameters() const
	{
		return parameters;
	}

	// Substitute macro values in a string
	bool macroParse(String& value, const char* fileName) const;

private:
	enum LineType {LINE_BAD, LINE_REGULAR, LINE_START_SUB, LINE_END_SUB, LINE_INCLUDE};

    Parameters parameters;
	USHORT flags;
	unsigned includeLimit;
	ConfigCache* filesCache;
	static const unsigned INCLUDE_LIMIT = 64;

	// utilities
	bool getLine(Stream* stream, String&, unsigned int&);
	void parse(Stream* stream);
	LineType parseLine(const char* fileName, const String& input, Parameter& par);
	bool translate(const char* fileName, const String& from, String& to) const;
	[[noreturn]] void badLine(const char* fileName, const String& line);
	void include(const char* currentFileName, const ScratchBird::PathName& path);
	bool wildCards(const char* currentFileName, const ScratchBird::PathName& pathPrefix, FilesArray& components);
	bool substituteStandardDir(const String& from, String& to) const;
	void adjustMacroReplacePositions(const String& value, const String& macro, String::size_type& from, String::size_type& to) const;
	unsigned getDirSeparatorLength(const String& value, size_t subFrom) const;
};

#endif	// CONFIG_CONFIG_FILE_H
