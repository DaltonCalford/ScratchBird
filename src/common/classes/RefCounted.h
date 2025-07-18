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
 *  The Original Code was created by Vlad Horsun
 *  for the ScratchBird Open Source RDBMS project.
 *
 *  Copyright (c) 2008 Vlad Horsun <hvlad@users.sf.net>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 *
 *  Dmitry Yemanov <dimitr@users.sf.net>
 */

#ifndef COMMON_REF_COUNTED_H
#define COMMON_REF_COUNTED_H

#include "sb_exception.h"
#include "../common/classes/sb_atomic.h"
#include "../common/gdsassert.h"

namespace ScratchBird
{
	class RefCounted
	{
	public:
		virtual int addRef() const
		{
			return ++m_refCnt;
		}

		virtual int release() const
		{
			fb_assert(m_refCnt.value() > 0);
			const int refCnt = --m_refCnt;
			if (!refCnt)
				delete this;
			return refCnt;
		}

		void assertNonZero()
		{
			fb_assert(m_refCnt.value() > 0);
		}

	protected:
		RefCounted() : m_refCnt(0) {}

		virtual ~RefCounted()
		{
			fb_assert(m_refCnt.value() == 0);
		}

	private:
		mutable AtomicCounter m_refCnt;
	};

	// reference counted object guard
	class Reference
	{
	public:
		explicit Reference(RefCounted& refCounted) :
			r(refCounted)
		{
			r.addRef();
		}

		~Reference()
		{
			try {
				r.release();
			}
			catch (const Exception&)
			{
				DtorException::devHalt();
			}
		}

	private:
		RefCounted& r;
	};

	enum NoIncrement {REF_NO_INCR};

	// controls reference counter of the object where points
	template <typename T>
	class RefPtr
	{
	public:
		RefPtr() : ptr(NULL)
		{ }

		explicit RefPtr(T* p) : ptr(p)
		{
			if (ptr)
			{
				ptr->addRef();
			}
		}

		// This special form of ctor is used to create refcounted ptr from interface,
		// returned by a function (which increments counter on return)
		RefPtr(NoIncrement x, T* p) : ptr(p)
		{ }

		RefPtr(const RefPtr& r) : ptr(r.ptr)
		{
			if (ptr)
			{
				ptr->addRef();
			}
		}

		RefPtr(RefPtr&& r) noexcept
			: ptr(r.ptr)
		{
			r.ptr = nullptr;
		}

		RefPtr(MemoryPool&, RefPtr&& r) noexcept
			: ptr(r.ptr)
		{
			r.ptr = nullptr;
		}

		~RefPtr()
		{
			if (ptr)
			{
				ptr->release();
			}
		}

		T* assignRefNoIncr(T* p)
		{
			assign(NULL);
			ptr = p;
			return ptr;
		}

		void moveFrom(RefPtr& r)
		{
			if (this != &r)
			{
				assign(nullptr);
				ptr = r.ptr;
				r.ptr = nullptr;
			}
		}

		T* clear()		// nullify pointer w/o calling release
		{
			T* rc = ptr;
			ptr = NULL;
			return rc;
		}

		T* operator=(T* p)
		{
			return assign(p);
		}

		T* operator=(const RefPtr& r)
		{
			return assign(r.ptr);
		}

		T* operator=(RefPtr&& r)
		{
			moveFrom(r);
			return ptr;
		}

		operator T*() const
		{
			return ptr;
		}

		T* operator->() const
		{
			return ptr;
		}

		bool hasData() const
		{
			return ptr ? true : false;
		}

		bool operator !() const
		{
			return !ptr;
		}

		bool operator ==(const RefPtr& r) const
		{
			return ptr == r.ptr;
		}

		bool operator !=(const RefPtr& r) const
		{
			return ptr != r.ptr;
		}

		T* getPtr()
		{
			return ptr;
		}

		const T* getPtr() const
		{
			return ptr;
		}

	protected:
		T* assign(T* const p)
		{
			if (ptr != p)
			{
				if (p)
				{
					p->addRef();
				}

				T* tmp = ptr;
				ptr = p;

				if (tmp)
				{
					tmp->release();
				}
			}

			return p;
		}

	private:
		T* ptr;
	};

	template <typename T>
	RefPtr<T> makeRef(T* o)
	{
		return RefPtr<T>(o);
	}

	template <typename T>
	RefPtr<T> makeNoIncRef(T* arg)
	{
		return RefPtr<T>(REF_NO_INCR, arg);
	}

	template <typename T>
	class AnyRef : public T, public RefCounted
	{
	public:
		using T::T;
	};
} // namespace

#endif // COMMON_REF_COUNTED_H
