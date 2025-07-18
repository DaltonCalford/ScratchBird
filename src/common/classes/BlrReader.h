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
 *  The Original Code was created by Adriano dos Santos Fernandes
 *  for the ScratchBird Open Source RDBMS project.
 *
 *  Copyright (c) 2009 Adriano dos Santos Fernandes <adrianosf@uol.com.br>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#ifndef COMMON_CLASSES_BLR_READER_H
#define COMMON_CLASSES_BLR_READER_H

#include "iberror.h"
#include "../common/classes/fb_string.h"
#include "../common/StatusArg.h"
#include "../jrd/constants.h"

namespace ScratchBird {


class BlrReader
{
public:
	struct Flags
	{
		bool searchSystemSchema = false;
	};

public:
	BlrReader(const UCHAR* buffer, unsigned maxLen)
		: start(buffer),
		  end(buffer + maxLen),
		  pos(buffer)
	{
		// ASF: A big maxLen like MAX_ULONG could overflow the pointer size and
		// points to something before start. In this case, we set the end to the
		// max possible address.
		if (end < start)
			end = (UCHAR*) ~U_IPTR(0);
	}

	BlrReader()
		: start(NULL),
		  end(NULL),
		  pos(NULL)
	{
	}

public:
	unsigned getLength() const
	{
		return (unsigned) (end - start);
	}

	const UCHAR* getPos() const
	{
		fb_assert(pos);
		return pos;
	}

	void setPos(const UCHAR* newPos)
	{
		fb_assert(newPos >= start && newPos < end);
		pos = newPos;
	}

	void seekBackward(unsigned n)
	{
		setPos(getPos() - n);
	}

	void seekForward(unsigned n)
	{
		setPos(getPos() + n);
	}

	unsigned getOffset() const
	{
		return getPos() - start;
	}

	UCHAR peekByte() const
	{
		fb_assert(pos);

		if (pos >= end)
			(Arg::Gds(isc_invalid_blr) << Arg::Num(getOffset())).raise();

		return *pos;
	}

	UCHAR getByte()
	{
		UCHAR byte = peekByte();
		++pos;
		return byte;
	}

	USHORT getWord()
	{
		const UCHAR low = getByte();
		const UCHAR high = getByte();

		return high * 256 + low;
	}

	ULONG getLong()
	{
		const UCHAR b1 = getByte();
		const UCHAR b2 = getByte();
		const UCHAR b3 = getByte();
		const UCHAR b4 = getByte();

		return (b4 << 24) | (b3 << 16) | (b2 << 8) | b1;
	}

	UCHAR parseHeader(Flags* flags = nullptr)
	{
		const auto version = getByte();

		switch (version)
		{
			case blr_version4:
			case blr_version5:
			//case blr_version6:
				break;

			default:
				status_exception::raise(
					Arg::Gds(isc_metadata_corrupt) <<
					Arg::Gds(isc_wroblrver2) << Arg::Num(blr_version4) << Arg::Num(blr_version5/*6*/) <<
						Arg::Num(version));
		}

		auto code = getByte();

		if (code == blr_flags)
		{
			while ((code = getByte()) != blr_end)
			{
				if (flags)
				{
					switch (code)
					{
						case blr_flags_search_system_schema:
							flags->searchSystemSchema = true;
							break;
					}
				}

				const auto len = getWord();
				seekForward(len);
			}
		}
		else
			seekBackward(1);

		return version;
	}

	UCHAR checkByte(UCHAR expected)
	{
		UCHAR byte = getByte();

		if (byte != expected)
		{
			status_exception::raise(Arg::Gds(isc_syntaxerr) <<
				Arg::Num(expected) <<
				Arg::Num(getOffset() - 1) <<
				Arg::Num(byte));
		}

		return byte;
	}

	USHORT checkWord(USHORT expected)
	{
		USHORT word = getWord();

		if (word != expected)
		{
			status_exception::raise(Arg::Gds(isc_syntaxerr) <<
				Arg::Num(expected) <<
				Arg::Num(getOffset() - 2) <<
				Arg::Num(word));
		}

		return word;
	}

	void getString(string& s)
	{
		const unsigned len = getByte();

		if (pos + len >= end)
			(Arg::Gds(isc_invalid_blr) << Arg::Num(getOffset())).raise();

		s.assign(pos, len);

		seekForward(len);
	}

	template <typename STR>
	void getMetaName(STR& name)
	{
		string str;
		getString(str);

		// Check for overly long identifiers at BLR parse stage to prevent unwanted
		// surprises in deeper layers of the engine.
		if (str.length() > MAX_SQL_IDENTIFIER_LEN)
			(Arg::Gds(isc_identifier_too_long) << Arg::Str(str)).raise();

		name.assign(str.c_str());
	}

private:
	const UCHAR* start;
	const UCHAR* end;
	const UCHAR* pos;
};


}	// namespace

#endif	// COMMON_CLASSES_BLR_READER_H
