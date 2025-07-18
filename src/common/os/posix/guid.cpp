/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		guid.cpp
 *	DESCRIPTION:	Portable GUID (posix)
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

#include "firebird.h"
#include "../common/os/guid.h"

#include "sb_exception.h"
#include "../common/os/os_utils.h"
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>

namespace ScratchBird {


void GenerateRandomBytes(void* buffer, FB_SIZE_T size)
{
	// do not use /dev/random because it may return lesser data than we need.
	int fd = os_utils::open("/dev/urandom", O_RDONLY);
	if (fd < 0)
		ScratchBird::system_call_failed::raise("open");

	for (FB_SIZE_T offset = 0; offset < size; )
	{
		int rc = read(fd, static_cast<char*>(buffer) + offset, size - offset);
		if (rc < 0)
		{
			if (errno != EINTR)
				ScratchBird::system_call_failed::raise("read");
			continue;
		}
		if (rc == 0)
			ScratchBird::system_call_failed::raise("read", EIO);
		offset += static_cast<FB_SIZE_T>(rc);
	}
	if (close(fd) < 0)
	{
		if (errno != EINTR)
			ScratchBird::system_call_failed::raise("close");
		// In case when close() is interrupted by a signal,
		// the state of fd is unspecified - give up and return success.
	}
}

void GenerateGuid(UUID* guid)
{
	GenerateRandomBytes(guid, sizeof(UUID));

	guid->Data3 = (4 << 12) | (guid->Data3 & 0xFFF);	// version 4
	guid->Data4[0] = 0x80 | (guid->Data4[0] & 0x3F);	// variant
}


}	// namespace
