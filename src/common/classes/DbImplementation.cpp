/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		DbImplementation.cpp
 *	DESCRIPTION:	Database implementation
 *
 *  The contents of this file are subject to the Initial
 *  Developer's Public License Version 1.0 (the "License");
 *  you may not use this file except in compliance with the
 *  License. You may obtain a copy of the License at
 *  https://www.ibphoenix.com/about/firebird/idpl.
 *
 *  Software distributed under the License is distributed AS IS,
 *  WITHOUT WARRANTY OF ANY KIND, either express or implied.
 *  See the License for the specific language governing rights
 *  and limitations under the License.
 *
 *  The Original Code was created by Alexander Peshkoff
 *  for the ScratchBird Open Source RDBMS project.
 *
 *  Copyright (c) 2009 Alexander Peshkoff <peshkoff@mail.ru>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#include "firebird.h"
#include "../common/classes/DbImplementation.h"

#include "../jrd/ods.h"

namespace {

static const UCHAR CpuIntel = 0;
static const UCHAR CpuAmd = 1;
static const UCHAR CpuUltraSparc = 2;
static const UCHAR CpuPowerPc = 3;
static const UCHAR CpuPowerPc64 = 4;
static const UCHAR CpuMipsel = 5;
static const UCHAR CpuMips = 6;
static const UCHAR CpuArm = 7;
static const UCHAR CpuIa64 = 8;
static const UCHAR CpuS390 = 9;
static const UCHAR CpuS390x = 10;
static const UCHAR CpuSh = 11;
static const UCHAR CpuSheb = 12;
static const UCHAR CpuHppa = 13;
static const UCHAR CpuAlpha = 14;
static const UCHAR CpuArm64 = 15;
static const UCHAR CpuPowerPc64el = 16;
static const UCHAR CpuM68k = 17;
static const UCHAR CpuRiscV64 = 18;
static const UCHAR CpuMips64el = 19;
static const UCHAR CpuLoongArch = 20;

static const UCHAR OsWindows = 0;
static const UCHAR OsLinux = 1;
static const UCHAR OsDarwin = 2;
static const UCHAR OsSolaris = 3;
static const UCHAR OsHpux = 4;
static const UCHAR OsAix = 5;
static const UCHAR OsMms = 6;
static const UCHAR OsFreeBsd = 7;
static const UCHAR OsNetBsd = 8;

static const UCHAR CcMsvc = 0;
static const UCHAR CcGcc = 1;
static const UCHAR CcXlc = 2;
static const UCHAR CcAcc = 3;
static const UCHAR CcSunStudio = 4;
static const UCHAR CcIcc = 5;

static const UCHAR EndianLittle = 0;
static const UCHAR EndianBig = 1;
static const UCHAR EndianMask = 1;

const char* hardware[] = {
	"Intel/i386",
	"AMD/Intel/x64",
	"UltraSparc",
	"PowerPC",
	"PowerPC64",
	"MIPSEL",
	"MIPS",
	"ARM",
	"IA64",
	"s390",
	"s390x",
	"SH",
	"SHEB",
	"HPPA",
	"Alpha",
	"ARM64",
	"PowerPC64el",
	"M68k",
	"RiscV64",
	"MIPS64EL",
	"LOONGARCH"
};

const char* operatingSystem[] = {
	"Windows",
	"Linux",
	"Darwin",
	"Solaris",
	"HPUX",
	"AIX",
	"MVS",
	"FreeBSD",
	"NetBSD"
};

const char* compiler[] = {
	"MSVC",
	"gcc",
	"xlC",
	"aCC",
	"SunStudio",
	"icc"
};

// This table lists pre-fb3 implementation codes
const UCHAR backwardTable[FB_NELEM(hardware) * FB_NELEM(operatingSystem)] =
{
//				Intel	AMD		Sparc	PPC		PPC64	MIPSEL	MIPS	ARM		IA64	s390	s390x	SH		SHEB	HPPA	Alpha	ARM64	PPC64el	M68k	RiscV64 MIPS64EL LoongArch
/* Windows */	50,		68,		0,		0,		0,		0,		0,		0,		0,		0,		0,		0,		0,		0,		0,		0,		0,		0,		0,		0,		0,
/* Linux */		60,		66,		65,		69,		86,		71,		72,		75, 	76,		79, 	78,		80,		81,		82,		83,		84,		85,		87,		88,		90,		93,
/* Darwin */	70,		73,		0,		63,		77,		0,		0,		0,		0,		0,		0,		0,		0,		0,		0,		0,		0,		0,		0,		0,		0,
/* Solaris */	0,		0,		30,		0,		0,		0,		0,		0,		0,		0,		0,		0,		0,		0,		0,		0,		0,		0,		0,		0,		0,
/* HPUX */		0,		0,		0,		0,		0,		0,		0,		0,		0,		0,		0,		0,		0,		31,		0,		0,		0,		0,		0,		0,		0,
/* AIX */		0,		0,		0,		35,		0,		0,		0,		0,		0,		0,		0,		0,		0,		0,		0,		0,		0,		0,		0,		0,		0,
/* MVS */		0,		0,		0,		0,		0,		0,		0,		0,		0,		0,		0,		0,		0,		0,		0,		0,		0,		0,		0,		0,		0,
/* FreeBSD */	61,		67,		0,		92,		91,		0,		0,		0,		0,		0,		0,		0,		0,		0,		0,		0,		89,		0,		0,		0,		0,
/* NetBSD */	62,		0,		0,		0,		0,		0,		0,		0,		0,		0,		0,		0,		0,		0,		0,		0,		0,		0,		0,		0,		0
};


const UCHAR backEndianess[FB_NELEM(hardware)] =
{
//	Intel	AMD		Sparc	PPC		PPC64	MIPSEL	MIPS	ARM		IA64	s390	s390x	SH		SHEB	HPPA	Alpha	ARM64	PPC64el	M68k	RiscV64 MIPS64EL LoongArch
	0,		0,		1,		1,		1,		0,		1,		0,		0,		1,		1,		0,		1,		1,		0,		0,		0,		1,		0,		0,		0,
};

} // anonymous namespace

namespace ScratchBird {

DbImplementation::DbImplementation(const Ods::header_page* h)
	: di_cpu(h->hdr_db_impl.hdr_cpu), di_os(h->hdr_db_impl.hdr_os),
	  di_cc(h->hdr_db_impl.hdr_cc), di_flags(h->hdr_db_impl.hdr_compat)
{
}

#define GET_ARRAY_ELEMENT(array, elem) ((elem) < FB_NELEM(array) ? array[(elem)] : "** Unknown **")

const char* DbImplementation::cpu() const
{
	return GET_ARRAY_ELEMENT(hardware, di_cpu);
}

const char* DbImplementation::os() const
{
	return GET_ARRAY_ELEMENT(operatingSystem, di_os);
}

const char* DbImplementation::cc() const
{
	return GET_ARRAY_ELEMENT(compiler, di_cc);
}

string DbImplementation::implementation() const
{
	string rc("ScratchBird/");
	rc += os();
	rc += "/";
	rc += cpu();
	return rc;
}

const char* DbImplementation::endianess() const
{
	return (di_flags & EndianMask) == EndianBig ? "big" : "little";
}

const DbImplementation DbImplementation::current(
		FB_CPU, FB_OS, FB_CC,
#ifdef WORDS_BIGENDIAN
		EndianBig);
#else
		EndianLittle);
#endif

bool DbImplementation::compatible(const DbImplementation& v) const
{
	return di_flags == v.di_flags;
}

void DbImplementation::store(Ods::header_page* h) const
{
	h->hdr_db_impl.hdr_cpu = di_cpu;
	h->hdr_db_impl.hdr_os = di_os;
	h->hdr_db_impl.hdr_cc = di_cc;
	h->hdr_db_impl.hdr_compat = di_flags;
}

void DbImplementation::stuff(UCHAR** info) const
{
	UCHAR* p = *info;
	*p++ = di_cpu;
	*p++ = di_os;
	*p++ = di_cc;
	*p++ = di_flags;
	*info = p;
}

DbImplementation DbImplementation::pick(const UCHAR* info)
{
	//DbImplementation(UCHAR p_cpu, UCHAR p_os, UCHAR p_cc, UCHAR p_flags)
	return DbImplementation(info[0], info[1], info[2], info[3]);
}

DbImplementation DbImplementation::fromBackwardCompatibleByte(UCHAR bcImpl)
{
	for (UCHAR os = 0; os < FB_NELEM(operatingSystem); ++os)
	{
		for (UCHAR hw = 0; hw < FB_NELEM(hardware); ++hw)
		{
			const USHORT ind = USHORT(os) * FB_NELEM(hardware) + USHORT(hw);
			if (backwardTable[ind] == bcImpl)
			{
				return DbImplementation(hw, os, 0xFF, backEndianess[hw] ? EndianBig : EndianLittle);
			}
		}
	}

	return DbImplementation(0xFF, 0xFF, 0xFF, 0x80);
}

UCHAR DbImplementation::backwardCompatibleImplementation() const
{
	if (di_cpu >= FB_NELEM(hardware) || di_os >= FB_NELEM(operatingSystem))
	{
		return 0;
	}

	return backwardTable[USHORT(di_os) * FB_NELEM(hardware) + USHORT(di_cpu)];
}

} // namespace ScratchBird
