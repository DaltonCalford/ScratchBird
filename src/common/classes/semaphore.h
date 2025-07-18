/*
 *	PROGRAM:		Client/Server Common Code
 *	MODULE:			semaphore.h
 *	DESCRIPTION:	Semaphore lock
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

#ifndef CLASSES_SEMAPHORE_H
#define CLASSES_SEMAPHORE_H

#include "../common/gdsassert.h"
#include "sb_exception.h"

#ifdef _AIX
#undef HAVE_SEMAPHORE_H
#endif

#ifdef WIN_NT
// Note: Windows does not need signal safe version of the class

#include <windows.h>
#include <limits.h>

namespace ScratchBird
{

class MemoryPool;

class Semaphore
{
private:
	HANDLE hSemaphore;
	void init();

	// Forbid copying (there is no definition of these methods)
	Semaphore(const Semaphore&);
	Semaphore& operator=(const Semaphore&);

public:
	Semaphore() { init(); }
	explicit Semaphore(MemoryPool&) { init(); }

	~Semaphore();

#define CLASSES_SEMAPHORE_H_HAS_TRYENTER 1
	bool tryEnter(const int seconds = 0, int milliseconds = 0);

	void enter()
	{
		tryEnter(-1);
	}

	void release(SLONG count = 1);
};

} // namespace ScratchBird

#else // WINNT

#if defined(DARWIN)

// dispatch semaphore
#define COMMON_CLASSES_SEMAPHORE_DISPATCH
#include <dispatch/dispatch.h>

namespace ScratchBird
{

class MemoryPool;

class SignalSafeSemaphore
{
private:
	dispatch_semaphore_t semaphore;

	void init();

	// Forbid copying
	SignalSafeSemaphore(const SignalSafeSemaphore&);
	SignalSafeSemaphore& operator=(const SignalSafeSemaphore&);

public:
	SignalSafeSemaphore() { init(); }
	explicit SignalSafeSemaphore(MemoryPool&) { init(); }

	~SignalSafeSemaphore();

	void enter()
	{
		dispatch_semaphore_wait(semaphore, DISPATCH_TIME_FOREVER);
	}

	void release(SLONG count = 1)
	{
		fb_assert(count >= 0);
		while (count--)
		{
			dispatch_semaphore_signal(semaphore);
		}
	}
};

} // namespace ScratchBird


#elif defined(HAVE_SEMAPHORE_H)

#define COMMON_CLASSES_SEMAPHORE_POSIX_RT
#include <semaphore.h>
#include <errno.h>
#include <time.h>
#ifndef WORKING_SEM_INIT
#include <fcntl.h>
#endif // WORKING_SEM_INIT

namespace ScratchBird
{

class SignalSafeSemaphore
{
private:
#ifdef WORKING_SEM_INIT
	sem_t sem[1];
#else
	sem_t* sem;
#endif // WORKING_SEM_INIT

	void init();

	// Forbid copying
	SignalSafeSemaphore(const SignalSafeSemaphore&);
	SignalSafeSemaphore& operator=(const SignalSafeSemaphore&);

public:
	SignalSafeSemaphore() { init(); }
	explicit SignalSafeSemaphore(MemoryPool&) { init(); }

	~SignalSafeSemaphore();
	void enter();

	void release(SLONG count = 1)
	{
		for (int i = 0; i < count; i++)
		{
			if (sem_post(sem) == -1)
			{
				system_call_failed::raise("semaphore.h: release: sem_post()");
			}
		}
	}

#ifdef HAVE_SEM_TIMEDWAIT
	// In case when sem_timedwait() is implemented by host OS,
	// class SignalSafeSemaphore may have this function:
#define CLASSES_SEMAPHORE_H_HAS_TRYENTER 1
	bool tryEnter(const int seconds = 0, int milliseconds = 0);
#endif // HAVE_SEM_TIMEDWAIT

};

} // namespace ScratchBird

#else // posix OS choice - not found a way to have SignalSafeSemaphore

#define CLASSES_SEMAPHORE_H_NO_SS_SEM

#endif // posix OS choice

#ifdef CLASSES_SEMAPHORE_H_HAS_TRYENTER

// In case when sem_timedwait() is implemented by host OS,
// SignalSafeSemaphore and Semaphore are just the same
namespace ScratchBird
{
typedef SignalSafeSemaphore Semaphore;
}

#else // CLASSES_SEMAPHORE_H_HAS_TRYENTER

// Should implement Semaphore independent from SignalSafeSemaphore.
// In the worst case no SignalSafeSemaphore at all (and no SS for that platform).
#define COMMON_CLASSES_SEMAPHORE_COND_VAR
#include "sb_pthread.h"
#include <errno.h>

namespace ScratchBird
{

class Semaphore
{
private:
	pthread_mutex_t	mu;
	pthread_cond_t	cv;
	int value;

	void init();
	void mtxLock();
	void mtxUnlock();

	// Forbid copying
	Semaphore(const Semaphore&);
	Semaphore& operator=(const Semaphore&);

public:
	Semaphore() { init(); }
	explicit Semaphore(MemoryPool&) { init(); }

	~Semaphore();

	bool tryEnter(const int seconds = 0, int milliseconds = 0);
	void enter();
	void release(SLONG count = 1);
};

#ifdef CLASSES_SEMAPHORE_H_NO_SS_SEM

typedef Semaphore SignalSafeSemaphore;

#endif // CLASSES_SEMAPHORE_H_NO_SS_SEM

} // namespace ScratchBird

#endif // CLASSES_SEMAPHORE_H_HAS_TRYENTER

#endif // WIN_NT

#endif // CLASSES_SEMAPHORE_H
