/*
 *	PROGRAM:	FPE handling
 *	MODULE:		FpeControl.h
 *	DESCRIPTION:	handle state of math coprocessor when thread
 *					enters / leaves engine
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
 *  Copyright (c) 2008 Alexander Peshkoff <peshkoff@mail.ru>,
 *					   Bill Oliver <Bill.Oliver@sas.com>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#ifndef CLASSES_FPE_CONTROL_H
#define CLASSES_FPE_CONTROL_H

#include <math.h>
#if defined(WIN_NT)
#include <float.h>
#else
#include <fenv.h>
#include <string.h>
#endif

#if defined(SOLARIS) && !defined(HAVE_FEGETENV)
// ok to remove this when Solaris 9 is no longer supported
#include <ieeefp.h>
#endif

namespace ScratchBird
{

// class to hold the state of the Floating Point Exception mask

// The firebird server *must* run with FP exceptions masked, as we may
// intentionally overflow and then check for infinity to raise an error.
// Most hosts run with FP exceptions masked by default, but we need
// to save the mask for cases where ScratchBird is used as an embedded
// database.
class FpeControl
{
public:
	// the constructor (1) saves the current floating point mask, and
	// (2) masks all floating point exceptions. Use is similar to
	// the ContextPoolHolder for memory allocation.

	// on modern systems, the default is to mask exceptions
	FpeControl() noexcept
	{
		getCurrentMask(savedMask);
		if (!areExceptionsMasked(savedMask))
		{
			maskAll();
		}
	}

	~FpeControl() noexcept
	{
		// change it back if necessary
		if (!areExceptionsMasked(savedMask))
		{
			restoreMask();
		}
	}

#if defined(WIN_NT)
	static void maskAll() noexcept
	{
		_clearfp(); // always call _clearfp() before setting control word

#if defined(AMD64) || defined(ARM64)
		_controlfp(_CW_DEFAULT, _MCW_EM);
#else
		Mask cw;
		__control87_2(_CW_DEFAULT, _MCW_EM, &cw, NULL);
#endif
	}

private:
	typedef unsigned int Mask;
	Mask savedMask;

	static bool areExceptionsMasked(const Mask& m) noexcept
	{
		return m == _CW_DEFAULT;
	}

	static void getCurrentMask(Mask& m) noexcept
	{
#if defined(AMD64) || defined(ARM64)
		m = _controlfp(0, 0);
#else
		__control87_2(0, 0, &m, NULL);
#endif
	}

	void restoreMask() noexcept
	{
		_clearfp(); // always call _clearfp() before setting control word

#if defined(AMD64) || defined(ARM64)
		_controlfp(savedMask, _MCW_EM); // restore saved
#else
		Mask cw;
		__control87_2(savedMask, _MCW_EM, &cw, NULL); // restore saved
#endif
	}

#elif defined(HAVE_FEGETENV)
	static void maskAll() noexcept
	{
		fesetenv(FE_DFL_ENV);
	}

private:
	// Can't dereference FE_DFL_ENV, therefore need to have something to compare with
	class DefaultEnvironment
	{
	public:
		DefaultEnvironment()
		{
			fenv_t saved;
			fegetenv(&saved);
			fesetenv(FE_DFL_ENV);
			fegetenv(&clean);
			fesetenv(&saved);
		}

		fenv_t clean;
	};

	fenv_t savedMask;

	static bool areExceptionsMasked(const fenv_t& m) noexcept
	{
		const static DefaultEnvironment defaultEnvironment;
		return memcmp(&defaultEnvironment.clean, &m, sizeof(fenv_t)) == 0;
	}

	static void getCurrentMask(fenv_t& m) noexcept
	{
		fegetenv(&m);
	}

	void restoreMask() noexcept
	{
		fesetenv(&savedMask);
	}
#elif defined(SOLARIS) && !defined(HAVE_FEGETENV)
	// ok to remove this when Solaris 9 is no longer supported
	// Solaris without fegetenv() implies Solaris 9 or older. In this case we
	// have to use the Solaris FPE routines.
	static void maskAll() noexcept
	{
		fpsetmask(~(FP_X_OFL | FP_X_INV | FP_X_UFL | FP_X_DZ | FP_X_IMP));
	}

private:
	// default environment is all traps disabled, but there is no
	// constand for this setting
	class DefaultEnvironment
	{
	public:
		DefaultEnvironment()
		{
			fp_except saved;
			saved = fpgetmask();
			fpsetmask(~(FP_X_OFL | FP_X_INV | FP_X_UFL | FP_X_DZ | FP_X_IMP));
			clean = fpgetmask();
			fpsetmask(saved);
		}

		fp_except clean;
	};

	fp_except savedMask;

	static bool areExceptionsMasked(const fp_except& m) noexcept
	{
		const static DefaultEnvironment defaultEnvironment;
		return memcmp(&defaultEnvironment.clean, &m, sizeof(fp_except)) == 0;
	}

	static void getCurrentMask(fp_except& m) noexcept
	{
		m = fpgetmask();
	}

	void restoreMask() noexcept
	{
		fpsetsticky(0); // clear exception sticky flags
		fpsetmask(savedMask);
	}

#elif defined(LINUX)
// Linux floating point exception handling
#include <fenv.h>

private:
	int savedMask;

public:
	void getCurrentMask(int& mask) noexcept
	{
		mask = fegetexcept();
	}

	bool areExceptionsMasked(int mask) const noexcept
	{
		return (mask & (FE_DIVBYZERO | FE_INVALID | FE_OVERFLOW)) == 0;
	}

	void maskAll() noexcept
	{
		savedMask = fegetexcept();
		fedisableexcept(FE_ALL_EXCEPT);
	}

	void restoreMask() noexcept
	{
		feclearexcept(FE_ALL_EXCEPT);
		feenableexcept(savedMask);
	}

#else
#error do not know how to mask floating point exceptions on this platform!
#endif

};

inline bool isNegativeInf(double x)
{
#ifdef WIN_NT
	return _fpclass(x) == _FPCLASS_NINF;
#else
	return x == -INFINITY;
#endif
}

}	// namespace ScratchBird

#endif // CLASSES_FPE_CONTROL_H
