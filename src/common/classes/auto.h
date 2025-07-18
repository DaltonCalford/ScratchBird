/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		auto.h
 *	DESCRIPTION:	STL::auto_ptr replacement
 *
 *		*** warning ***
 *  Unlike STL's auto_ptr ALWAYS deletes ptr in destructor -
 *  no ownership flag!
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
 *  Copyright (c) 2004 Alexander Peshkoff <peshkoff@mail.ru>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#ifndef CLASSES_AUTO_PTR_H
#define CLASSES_AUTO_PTR_H

#include <stdio.h>

namespace ScratchBird {


template <typename What>
class SimpleDelete
{
public:
	static void clear(What* ptr)
	{
		static_assert(sizeof(What) > 0, "can't delete pointer to incomplete type");
		delete ptr;
	}
};

template <>
inline void SimpleDelete<FILE>::clear(FILE* f)
{
	if (f) {
		fclose(f);
	}
}


template <typename What>
class ArrayDelete
{
public:
	static void clear(What* ptr)
	{
		static_assert(sizeof(What) > 0, "can't delete pointer to incomplete type");
		delete[] ptr;
	}
};


template <typename T>
class SimpleRelease
{
public:
	static void clear(T* ptr)
	{
		if (ptr)
		{
			ptr->release();
		}
	}
};


template <typename T>
class SimpleDispose
{
public:
	static void clear(T* ptr)
	{
		if (ptr)
		{
			ptr->dispose();
		}
	}
};


template <typename Where, template <typename W> class Clear = SimpleDelete >
class AutoPtr
{
private:
	Where* ptr;
public:
	AutoPtr(Where* v = nullptr) noexcept
		: ptr(v)
	{}

	AutoPtr(AutoPtr&& v) noexcept
		: ptr(v.ptr)
	{
		v.ptr = nullptr;
	}

	~AutoPtr()
	{
		Clear<Where>::clear(ptr);
	}

	AutoPtr& operator= (Where* v)
	{
		Clear<Where>::clear(ptr);
		ptr = v;
		return *this;
	}

	AutoPtr& operator=(AutoPtr&& r) noexcept
	{
		if (this != &r)
		{
			ptr = r.ptr;
			r.ptr = nullptr;
		}

		return *this;
	}

	const Where* get() const
	{
		return ptr;
	}

	operator const Where*() const
	{
		return ptr;
	}

	Where* get()
	{
		return ptr;
	}

	operator Where*()
	{
		return ptr;
	}

	bool operator !() const
	{
		return !ptr;
	}

	bool hasData() const
	{
		return ptr != NULL;
	}

	Where* operator->() const
	{
		return ptr;
	}

	Where* release()
	{
		Where* tmp = ptr;
		ptr = NULL;
		return tmp;
	}

	void reset(Where* v = NULL)
	{
		if (v != ptr)
		{
			Clear<Where>::clear(ptr);
			ptr = v;
		}
	}

private:
	AutoPtr(AutoPtr&);
	void operator=(AutoPtr&);
};

template <typename T>
using AutoDispose = AutoPtr<T, SimpleDispose>;

template <typename T>
using AutoRelease = AutoPtr<T, SimpleRelease>;


template <typename T>
class AutoSaveRestore
{
public:
	AutoSaveRestore(T* aValue)
		: value(aValue),
		  oldValue(*aValue)
	{ }

	~AutoSaveRestore()
	{
		*value = oldValue;
	}

private:
	// copying is prohibited
	AutoSaveRestore(const AutoSaveRestore&);
	AutoSaveRestore& operator =(const AutoSaveRestore&);

	T* value;
	T oldValue;
};

template <typename T>
class AutoSetRestore : public AutoSaveRestore<T>
{
public:
	AutoSetRestore(T* aValue, T newValue)
		: AutoSaveRestore<T>(aValue)
	{
		*aValue = newValue;
	}
};


template <typename T>
class AutoSetRestoreFlag
{
public:
	AutoSetRestoreFlag(T* aValue, T newBit, bool set)
		: value(aValue),
		  bit(newBit),
		  oldValue((*value) & bit)
	{
		if (set)
			*value |= bit;
		else
			*value &= ~bit;
	}

	~AutoSetRestoreFlag()
	{
		*value &= ~bit;
		*value |= oldValue;
	}

	void release(T cleanBit)
	{
		bit &= ~cleanBit;
		oldValue &= ~cleanBit;
	}

private:
	// copying is prohibited
	AutoSetRestoreFlag(const AutoSetRestoreFlag&);
	AutoSetRestoreFlag& operator =(const AutoSetRestoreFlag&);

	T* value;
	T bit;
	T oldValue;
};


template <typename T, typename T2>
class AutoSetRestore2
{
private:
	typedef T (T2::*Getter)();
	typedef void (T2::*Setter)(T);

public:
	AutoSetRestore2(T2* aPointer, Getter aGetter, Setter aSetter, T newValue)
		: pointer(aPointer),
		  setter(aSetter),
		  oldValue((aPointer->*aGetter)())
	{
		(aPointer->*aSetter)(newValue);
	}

	~AutoSetRestore2()
	{
		(pointer->*setter)(oldValue);
	}

private:
	// copying is prohibited
	AutoSetRestore2(const AutoSetRestore2&);
	AutoSetRestore2& operator =(const AutoSetRestore2&);

private:
	T2* pointer;
	Setter setter;
	T oldValue;
};

template <typename F>
class Cleanup
{
public:
	Cleanup(F&& clFunc)
		: clean(clFunc)
	{ }

	~Cleanup()
	{
		clean();
	}

private:
	F clean;
};

class CleanupFunction
{
	typedef void Func();

public:
	CleanupFunction(Func* clFunc)
		: clean(clFunc)
	{ }

	~CleanupFunction()
	{
		clean();
	}

private:
	Func* clean;
};

} //namespace ScratchBird

#endif // CLASSES_AUTO_PTR_H

