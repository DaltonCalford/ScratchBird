/*
 *	PROGRAM:	JRD International support
 *	MODULE:		intl_classes.h
 *	DESCRIPTION:	International text handling definitions
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
 */

#ifndef JRD_INTL_CLASSES_H
#define JRD_INTL_CLASSES_H

#include "firebird.h"

#include "../common/intlobj_new.h"
#include "../jrd/constants.h"
#include "../common/unicode_util.h"
#include "../common/CsConvert.h"
#include "../common/CharSet.h"
#include "../common/TextType.h"

namespace Jrd {

class PatternMatcher
{
public:
	PatternMatcher(MemoryPool& aPool, ScratchBird::TextType* aTextType)
		: pool(aPool),
		  textType(aTextType)
	{
	}

	virtual ~PatternMatcher()
	{
	}

	virtual void reset() = 0;
	virtual bool process(const UCHAR*, SLONG) = 0;
	virtual bool result() = 0;

protected:
	MemoryPool& pool;
	ScratchBird::TextType* textType;
};

class BaseSubstringSimilarMatcher : public PatternMatcher
{
public:
	BaseSubstringSimilarMatcher(MemoryPool& pool, ScratchBird::TextType* ttype)
		: PatternMatcher(pool, ttype)
	{
	}

	virtual void getResultInfo(unsigned* start, unsigned* length) = 0;
};

class NullStrConverter
{
public:
	NullStrConverter(MemoryPool& /*pool*/, const ScratchBird::TextType* /*obj*/, const UCHAR* /*str*/, SLONG /*len*/)
	{
	}
};

template <typename PrevConverter = NullStrConverter>
class UpcaseConverter : public PrevConverter
{
public:
	UpcaseConverter(MemoryPool& pool, ScratchBird::TextType* obj, const UCHAR*& str, SLONG& len)
		: PrevConverter(pool, obj, str, len)
	{
		const auto charSet = obj->getCharSet();
		const auto bufferSize = len / charSet->minBytesPerChar() * charSet->maxBytesPerChar();

		len = obj->str_to_upper(len, str, bufferSize, tempBuffer.getBuffer(bufferSize, false));
		str = tempBuffer.begin();
	}

private:
	ScratchBird::UCharBuffer tempBuffer;
};

template <typename PrevConverter = NullStrConverter>
class CanonicalConverter : public PrevConverter
{
public:
	CanonicalConverter(MemoryPool& pool, ScratchBird::TextType* obj, const UCHAR*& str, SLONG& len)
		: PrevConverter(pool, obj, str, len)
	{
		const SLONG out_len = len / obj->getCharSet()->minBytesPerChar() * obj->getCanonicalWidth();

		if (str)
		{
			len = obj->canonical(len, str, out_len, tempBuffer.getBuffer(out_len, false)) * obj->getCanonicalWidth();
			str = tempBuffer.begin();
		}
		else
			len = 0;
	}

private:
	ScratchBird::UCharBuffer tempBuffer;
};

} // namespace Jrd


#endif	// JRD_INTL_CLASSES_H
