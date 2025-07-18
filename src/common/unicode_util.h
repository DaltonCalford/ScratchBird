/*
 *	PROGRAM:	JRD International support
 *	MODULE:		unicode_util.h
 *	DESCRIPTION:	Unicode functions
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
 *  Copyright (c) 2004 Adriano dos Santos Fernandes <adrianosf@uol.com.br>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#ifndef COMMON_UNICODE_UTIL_H
#define COMMON_UNICODE_UTIL_H

#include "intlobj_new.h"
#include "../common/IntlUtil.h"
#include "../common/os/mod_loader.h"
#include "../common/classes/array.h"
#include "../common/classes/fb_string.h"
#include "../common/classes/GenericMap.h"
#include "../common/classes/objects_array.h"
#include <unicode/ucnv.h>
#include <unicode/ucal.h>

struct UCollator;
struct USet;

namespace ScratchBird {

class UnicodeUtil
{
private:
	struct ICU;

public:
	// encapsulate ICU conversion library
	struct ConversionICU
	{
		UConverter* (U_EXPORT2* ucnv_open) (const char* converterName, UErrorCode* err);
		void (U_EXPORT2* ucnv_close) (UConverter *converter);
		int32_t (U_EXPORT2* ucnv_fromUChars) (UConverter *cnv,
											  char *dest, int32_t destCapacity,
											  const UChar *src, int32_t srcLength,
											  UErrorCode *pErrorCode);

		UChar32 (U_EXPORT2* u_tolower) (UChar32 c);
		UChar32 (U_EXPORT2* u_toupper) (UChar32 c);
		int32_t (U_EXPORT2* u_strCompare) (const UChar* s1, int32_t length1,
										   const UChar* s2, int32_t length2, UBool codePointOrder);
		int32_t (U_EXPORT2* u_countChar32) (const UChar* s, int32_t length);

		UChar32 (U_EXPORT2* utf8_nextCharSafeBody) (const uint8_t* s, int32_t* pi, int32_t length, UChar32 c, UBool strict);

		void (U_EXPORT2* UCNV_TO_U_CALLBACK_STOP) (
                const void *context,
                UConverterToUnicodeArgs *toUArgs,
                const char* codeUnits,
                int32_t length,
                UConverterCallbackReason reason,
                UErrorCode * err);

		void (U_EXPORT2* ucnv_setToUCallBack) (
				UConverter * converter,
                UConverterToUCallback newAction,
                const void* newContext,
                UConverterToUCallback *oldAction,
                const void** oldContext,
                UErrorCode * err);
		void (U_EXPORT2* ucnv_setFromUCallBack) (
				UConverter * converter,
                UConverterFromUCallback newAction,
                const void *newContext,
                UConverterFromUCallback *oldAction,
                const void **oldContext,
                UErrorCode * err);

		void (U_EXPORT2* ucnv_fromUnicode) (
				UConverter * converter,
                char **target,
                const char *targetLimit,
                const UChar ** source,
                const UChar * sourceLimit,
                int32_t* offsets,
                UBool flush,
                UErrorCode * err);
		void (U_EXPORT2* ucnv_toUnicode) (
				UConverter *converter,
                UChar **target,
                const UChar *targetLimit,
                const char **source,
                const char *sourceLimit,
                int32_t *offsets,
                UBool flush,
                UErrorCode *err);

		void (U_EXPORT2* ucnv_getInvalidChars) (
				const UConverter *converter,
                char *errBytes,
                int8_t *len,
                UErrorCode *err);
		int8_t (U_EXPORT2* ucnv_getMaxCharSize) (const UConverter *converter);
		int8_t (U_EXPORT2* ucnv_getMinCharSize) (const UConverter *converter);

		int32_t (U_EXPORT2* ustrcmp) (const UChar* s1, const UChar* s2);

		const char* (U_EXPORT2* ucalGetTZDataVersion) (UErrorCode* status);
		int32_t (U_EXPORT2* ucalGetDefaultTimeZone) (UChar* result, int32_t resultCapacity, UErrorCode* ec);
		UCalendar* (U_EXPORT2* ucalOpen) (const UChar* zoneID, int32_t len, const char* locale, UCalendarType type,
			UErrorCode* err);
		void (U_EXPORT2* ucalClose) (UCalendar* cal);
		void (U_EXPORT2* ucalSetAttribute) (UCalendar* cal, UCalendarAttribute attr, int32_t newValue);
		void (U_EXPORT2* ucalSetMillis) (UCalendar* cal, UDate dateTime, UErrorCode* err);
		int32_t (U_EXPORT2* ucalGet) (UCalendar* cal, UCalendarDateFields field, UErrorCode* err);
		void (U_EXPORT2* ucalSetDateTime) (UCalendar* cal, int32_t year, int32_t month, int32_t date, int32_t hour,
			int32_t minute, int32_t second, UErrorCode* err);

		UDate (U_EXPORT2* ucalGetNow) ();
		UBool (U_EXPORT2* ucalGetTimeZoneTransitionDate) (const UCalendar* cal, UTimeZoneTransitionType type,
			UDate* transition, UErrorCode* status);

		int vMajor, vMinor;
	};

	static ScratchBird::string getDefaultIcuVersion();

	class ICUModules;
	// routines semantically equivalent with intlobj_new.h

	static USHORT utf16KeyLength(USHORT len);	// BOCU-1
	static USHORT utf16ToKey(USHORT srcLen, const USHORT* src, USHORT dstLen, UCHAR* dst);	// BOCU-1
	static ULONG utf16LowerCase(ULONG srcLen, const USHORT* src, ULONG dstLen, USHORT* dst,
								const ULONG* exceptions);
	static ULONG utf16UpperCase(ULONG srcLen, const USHORT* src, ULONG dstLen, USHORT* dst,
								const ULONG* exceptions);
	static ULONG utf16ToUtf8(ULONG srcLen, const USHORT* src, ULONG dstLen, UCHAR* dst,
							 USHORT* err_code, ULONG* err_position);
	static ULONG utf8ToUtf16(ULONG srcLen, const UCHAR* src, ULONG dstLen, USHORT* dst,
							 USHORT* err_code, ULONG* err_position);
	static ULONG utf16ToUtf32(ULONG srcLen, const USHORT* src, ULONG dstLen, ULONG* dst,
							  USHORT* err_code, ULONG* err_position);
	static ULONG utf32ToUtf16(ULONG srcLen, const ULONG* src, ULONG dstLen, USHORT* dst,
							  USHORT* err_code, ULONG* err_position);
	static SSHORT utf16Compare(ULONG len1, const USHORT* str1, ULONG len2, const USHORT* str2,
							   INTL_BOOL* error_flag);

	static ULONG utf16Length(ULONG len, const USHORT* str);
	static ULONG utf16Substring(ULONG srcLen, const USHORT* src, ULONG dstLen, USHORT* dst,
								ULONG startPos, ULONG length);
	static INTL_BOOL utf8WellFormed(ULONG len, const UCHAR* str, ULONG* offending_position);
	static INTL_BOOL utf16WellFormed(ULONG len, const USHORT* str, ULONG* offending_position);
	static INTL_BOOL utf32WellFormed(ULONG len, const ULONG* str, ULONG* offending_position);

	static void utf8Normalize(ScratchBird::UCharBuffer& data);

	static ConversionICU& getConversionICU();
	static ICU* loadICU(const ScratchBird::string& icuVersion, const ScratchBird::string& configInfo);
	static void getICUVersion(ICU* icu, int& majorVersion, int& minorVersion);
	static ICU* getCollVersion(const ScratchBird::string& icuVersion,
		const ScratchBird::string& configInfo, ScratchBird::string& collVersion);

	class Utf16Collation
	{
	public:
		static Utf16Collation* create(texttype* tt, USHORT attributes,
									  ScratchBird::IntlUtil::SpecificAttributesMap& specificAttributes,
									  const ScratchBird::string& configInfo);

		Utf16Collation()
			: contractionsPrefix(*getDefaultMemoryPool())
		{
		}

		~Utf16Collation();

		USHORT keyLength(USHORT len) const;
		USHORT stringToKey(USHORT srcLen, const USHORT* src, USHORT dstLen, UCHAR* dst,
						   USHORT key_type) const;
		SSHORT compare(ULONG len1, const USHORT* str1, ULONG len2, const USHORT* str2,
					   INTL_BOOL* error_flag) const;
		ULONG canonical(ULONG srcLen, const USHORT* src, ULONG dstLen, ULONG* dst, const ULONG* exceptions);

	private:
		template <typename T>
		class ArrayComparator
		{
		public:
			static bool greaterThan(const ScratchBird::Array<T>& i1, const ScratchBird::Array<T>& i2)
			{
				FB_SIZE_T minCount = MIN(i1.getCount(), i2.getCount());
				int cmp = memcmp(i1.begin(), i2.begin(), minCount * sizeof(T));

				if (cmp != 0)
					return cmp > 0;

				return i1.getCount() > i2.getCount();
			}

			static bool greaterThan(const ScratchBird::Array<T>* i1, const ScratchBird::Array<T>* i2)
			{
				return greaterThan(*i1, *i2);
			}
		};

		typedef ScratchBird::SortedObjectsArray<
					ScratchBird::Array<UCHAR>,
					ScratchBird::InlineStorage<ScratchBird::Array<UCHAR>*, 3>,
					ScratchBird::Array<UCHAR>,
					ScratchBird::DefaultKeyValue<const ScratchBird::Array<UCHAR>*>,
					ArrayComparator<UCHAR>
				> SortKeyArray;

		typedef ScratchBird::GenericMap<
					ScratchBird::Pair<
						ScratchBird::Full<
							ScratchBird::Array<USHORT>,	// UTF-16 string
							SortKeyArray				// sort keys
						>
					>,
					ArrayComparator<USHORT>
				> ContractionsPrefixMap;

		static ICU* loadICU(const ScratchBird::string& icuVersion, const ScratchBird::string& collVersion,
			const ScratchBird::string& locale, const ScratchBird::string& configInfo);

		void normalize(ULONG* strLen, const USHORT** str, bool forNumericSort,
			ScratchBird::HalfStaticArray<USHORT, BUFFER_SMALL / 2>& buffer) const;

		ICU* icu;
		texttype* tt;
		USHORT attributes;
		UCollator* compareCollator;
		UCollator* partialCollator;
		UCollator* sortCollator;
		ContractionsPrefixMap contractionsPrefix;
		unsigned maxContractionsPrefixLength;	// number of characters
		bool numericSort;
	};

	friend class Utf16Collation;
};

}	// namespace ScratchBird

#endif	// COMMON_UNICODE_UTIL_H
