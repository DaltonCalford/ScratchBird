/*
 *	PROGRAM:	Client/Server Common Code
 *	MODULE:		ClumpletReader.h
 *	DESCRIPTION:	Secure handling of clumplet buffers
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
 *  The Original Code was created by Nickolay Samofatov
 *  for the ScratchBird Open Source RDBMS project.
 *
 *  Copyright (c) 2004 Nickolay Samofatov <nickolay@broadviewsoftware.com>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 *
 *
 */

#ifndef CLUMPLETREADER_H
#define CLUMPLETREADER_H

#include "../common/classes/alloc.h"
#include "../common/classes/array.h"
#include "../common/classes/fb_string.h"

//#define DEBUG_CLUMPLETS
#if defined(DEV_BUILD) && !defined(DEBUG_CLUMPLETS)
#define DEBUG_CLUMPLETS
#endif

namespace ScratchBird {

// This class provides read access for clumplet structure
// Note: it doesn't make a copy of buffer it reads
class ClumpletReader : protected AutoStorage
{
public:
	enum Kind
	{
		EndOfList,
		Tagged,
		UnTagged,
		SpbAttach,
		SpbStart,
		Tpb,
		WideTagged,
		WideUnTagged,
		SpbSendItems,
		SpbReceiveItems,
		SpbResponse,
		InfoResponse,
		InfoItems
	};

	struct KindList
	{
		Kind kind;
		UCHAR tag;
	};

	struct SingleClumplet
	{
		UCHAR tag;
		FB_SIZE_T size;
		const UCHAR* data;
	};

	// Constructor prepares an object from plain PB
	ClumpletReader(Kind k, const UCHAR* buffer, FB_SIZE_T buffLen);
	ClumpletReader(MemoryPool& pool, Kind k, const UCHAR* buffer, FB_SIZE_T buffLen);

	// Different versions of clumplets may have different kinds
	ClumpletReader(const KindList* kl, const UCHAR* buffer, FB_SIZE_T buffLen, FPTR_VOID raise = NULL);
	ClumpletReader(MemoryPool& pool, const KindList* kl, const UCHAR* buffer, FB_SIZE_T buffLen, FPTR_VOID raise = NULL);
	virtual ~ClumpletReader() { }

	// Create a copy of reader
	ClumpletReader(MemoryPool& pool, const ClumpletReader& from);
	ClumpletReader(const ClumpletReader& from);

	// Navigation in clumplet buffer
	bool isEof() const { return cur_offset >= getBufferLength(); }
	void moveNext();
	void rewind();
	bool find(UCHAR tag);
	bool next(UCHAR tag);

    // Methods which work with currently selected clumplet
	UCHAR getClumpTag() const;
	FB_SIZE_T getClumpLength() const;

	SLONG getInt() const;
	bool getBoolean() const;
	SINT64 getBigInt() const;
	string& getString(string& str) const;
	PathName& getPath(PathName& str) const;
	void getData(UCharBuffer& data) const;
	const UCHAR* getBytes() const;
	double getDouble() const;
	ISC_TIMESTAMP getTimeStamp() const;
	ISC_TIME getTime() const { return getInt(); }
	ISC_DATE getDate() const { return getInt(); }

	template <typename STR>
	STR& getString(STR& str) const
	{
		const UCHAR* ptr = getBytes();
		const FB_SIZE_T length = getClumpLength();
		str.assign(reinterpret_cast<const char*>(ptr), length);
		return str;
	}

	// get the most generic representation of clumplet
	SingleClumplet getClumplet() const;

	// Return the tag for buffer (usually structure version)
	UCHAR getBufferTag() const;
	// true if buffer has tag
	bool isTagged() const noexcept;
	FB_SIZE_T getBufferLength() const
	{
		FB_SIZE_T rc = getBufferEnd() - getBuffer();
		if (rc == 1 && isTagged())
		{
			rc = 0;
		}
		return rc;
	}
	FB_SIZE_T getCurOffset() const noexcept { return cur_offset; }
	void setCurOffset(FB_SIZE_T newOffset) noexcept { cur_offset = newOffset; }

#ifdef DEBUG_CLUMPLETS
	// Sometimes it's really useful to have it in case of errors
	void dump() const;
#endif

	// it is not exact comparison as clumplets may have same tags
	// but in different order
	bool simpleCompare(const ClumpletReader &other) const
	{
		const FB_SIZE_T len = getBufferLength();
		return (len == other.getBufferLength()) && (memcmp(getBuffer(), other.getBuffer(), len) == 0);
	}

	// Methods are virtual so writer can override 'em
	virtual const UCHAR* getBuffer() const;
	virtual const UCHAR* getBufferEnd() const;

protected:
	enum ClumpletType {TraditionalDpb,	// one byte length, n bytes value
					   SingleTpb,		// no data after
					   StringSpb,		// two bytes length, n bytes data
					   IntSpb,			// four bytes data
					   BigIntSpb,		// eight bytes data
					   ByteSpb,			// one byte data
					   Wide				// four bytes length, n bytes data
					  };
	ClumpletType getClumpletType(UCHAR tag) const;
	FB_SIZE_T getClumpletSize(bool wTag, bool wLength, bool wData) const;
	void adjustSpbState();

	FB_SIZE_T cur_offset;
	Kind kind;
	UCHAR spbState;		// Reflects state of spb parser/writer

	// These functions are called when error condition is detected by this class.
	// They may throw exceptions. If they don't reader tries to do something
	// sensible, certainly not overwrite memory or read past the end of buffer

	// This appears to be a programming error in buffer access pattern
	virtual void usage_mistake(const char* what) const;

	// This is called when passed buffer appears invalid
	virtual void invalid_structure(const char* what, const int data = 0) const;

private:
	// Assignment not implemented.
	ClumpletReader& operator=(const ClumpletReader& from);

	const UCHAR* static_buffer;
	const UCHAR* static_buffer_end;

	static SINT64 fromVaxInteger(const UCHAR* ptr, FB_SIZE_T length);
	void create(const KindList* kl, FB_SIZE_T buffLen, FPTR_VOID raise);

public:
	// Some frequently used kind lists
	static const KindList dpbList[];
	static const KindList spbList[];
};

class AuthReader : public ClumpletReader
{
public:
	static constexpr unsigned char AUTH_NAME = 1;		// name described by it's type
	static constexpr unsigned char AUTH_PLUGIN = 2;		// plugin which added a record
	static constexpr unsigned char AUTH_TYPE = 3;		// it can be user/group/role/etc. - what plugin sets
	static constexpr unsigned char AUTH_SECURE_DB = 4;	// sec. db in which context record was added
														// missing when plugin is server-wide
	static constexpr unsigned char AUTH_ORIG_PLUG = 5;	// original plugin that added a mapped record
														// (human information reasons only)
	typedef Array<UCHAR> AuthBlock;

	struct Info
	{
		NoCaseString type, name, plugin, secDb, origPlug;
		unsigned found, current;

		Info() noexcept
			: found(0), current(0)
		{ }

		Info(MemoryPool& pool)
			: type(pool), name(pool), plugin(pool), secDb(pool), origPlug(pool), found(0), current(0)
		{ }
	};

	AuthReader(MemoryPool& pool, const AuthBlock& authBlock);
	explicit AuthReader(const AuthBlock& authBlock);
	explicit AuthReader(const ClumpletReader& rdr)
		: ClumpletReader(rdr)
	{ }

	bool getInfo(Info& info);
};

//#define AUTH_BLOCK_DEBUG
#ifdef AUTH_BLOCK_DEBUG
void dumpAuthBlock(const char* text, ClumpletReader* pb, unsigned char param);
#else
static inline void dumpAuthBlock(const char*, ClumpletReader*, unsigned char) noexcept { }
#endif

} // namespace ScratchBird

#endif // CLUMPLETREADER_H
