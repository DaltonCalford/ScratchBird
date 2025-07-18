/*
 *	PROGRAM:	JRD International support
 *	MODULE:		IntlUtil.h
 *	DESCRIPTION:	INTL Utility functions
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
 *  The Original Code was created by Adriano dos Santos Fernandes
 *  for the ScratchBird Open Source RDBMS project.
 *
 *  Copyright (c) 2006 Adriano dos Santos Fernandes <adrianosf@uol.com.br>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#ifndef COMMON_INTLUTIL_H
#define COMMON_INTLUTIL_H

#include "../common/classes/array.h"
#include "../common/classes/auto.h"
#include "../common/classes/GenericMap.h"
#include "../common/classes/fb_string.h"
#include "../common/classes/init.h"
#include "../common/intlobj_new.h"

namespace ScratchBird
{
class CharSet;

inline constexpr const char* ATTR_ICU_VERSION = "ICU-VERSION";
inline constexpr const char* ATTR_COLL_VERSION = "COLL-VERSION";

class IntlUtil
{
public:
	typedef Pair<Full<string, string> > SpecificAttribute;
	typedef GenericMap<SpecificAttribute> SpecificAttributesMap;

public:
	static CharSet* getUtf8CharSet()
	{
		return utf8CharSet->charSet;
	}

	static string generateSpecificAttributes(CharSet* cs, SpecificAttributesMap& map);
	static bool parseSpecificAttributes(CharSet* cs, ULONG len, const UCHAR* s,
										SpecificAttributesMap* map);

	static string convertAsciiToUtf16(const string& ascii);
	static string convertUtf16ToAscii(const string& utf16, bool* error);

	static ULONG cvtAsciiToUtf16(csconvert* obj, ULONG nSrc, const UCHAR* pSrc,
		ULONG nDest, UCHAR* pDest, USHORT* err_code, ULONG* err_position);
	static ULONG cvtUtf16ToAscii(csconvert* obj, ULONG nSrc, const UCHAR* pSrc,
		ULONG nDest, UCHAR* pDest, USHORT* err_code, ULONG* err_position);

	static ULONG cvtUtf8ToUtf16(csconvert* obj, ULONG nSrc, const UCHAR* pSrc,
		ULONG nDest, UCHAR* pDest, USHORT* err_code, ULONG* err_position);
	static ULONG cvtUtf16ToUtf8(csconvert* obj, ULONG nSrc, const UCHAR* pSrc,
		ULONG nDest, UCHAR* pDest, USHORT* err_code, ULONG* err_position);

	static INTL_BOOL asciiWellFormed(charset* cs, ULONG len, const UCHAR* str, ULONG* offendingPos);
	static INTL_BOOL utf8WellFormed(charset* cs, ULONG len, const UCHAR* str, ULONG* offendingPos);

	static ULONG utf8SubString(charset* cs, ULONG srcLen, const UCHAR* src, ULONG dstLen, UCHAR* dst,
		ULONG startPos, ULONG length);

	static void initAsciiCharset(charset* cs);
	static void initUtf8Charset(charset* cs);
	static void initConvert(csconvert* cvt, pfn_INTL_convert func);
	static void initNarrowCharset(charset* cs, const ASCII* name);
	static bool initUnicodeCollation(texttype* tt, charset* cs, const ASCII* name,
		USHORT attributes, const UCharBuffer& specificAttributes, const string& configInfo);

	static void finiCharset(charset* cs);

	static ULONG toLower(CharSet* cs, ULONG srcLen, const UCHAR* src, ULONG dstLen, UCHAR* dst,
		const ULONG* exceptions);
	static ULONG toUpper(CharSet* cs, ULONG srcLen, const UCHAR* src, ULONG dstLen, UCHAR* dst,
		const ULONG* exceptions);

	static bool readOneChar(CharSet* cs, const UCHAR** s, const UCHAR* end, ULONG* size);

	static bool setupIcuAttributes(charset* cs, const string& specificAttributes,
		const string& configInfo, string& newSpecificAttributes);

private:
	static string escapeAttribute(CharSet* cs, const string& s);
	static string unescapeAttribute(CharSet* cs, const string& s);

	static bool isAttributeEscape(CharSet* cs, const UCHAR* s, ULONG size);

	static bool readAttributeChar(CharSet* cs, const UCHAR** s, const UCHAR* end, ULONG* size,
		bool returnEscape);

private:
	class Utf8CharSet
	{
	public:
		explicit Utf8CharSet(MemoryPool& pool);

	public:
		charset obj;
		AutoPtr<CharSet> charSet;
	};

	static GlobalPtr<Utf8CharSet> utf8CharSet;
};

}	// namespace ScratchBird

#endif	// COMMON_INTLUTIL_H
