/*
 *	PROGRAM:		MT debugging support.
 *	MODULE:			Reasons.h
 *	DESCRIPTION:	Tool to access latest history of taken locks.
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
 *  The Original Code was created by Alex Peshkov
 *  for the ScratchBird Open Source RDBMS project.
 *
 *  Copyright (c) 2012 Alex Peshkov <peshkoff at mail.ru>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 *
 *
 */

#ifndef FB_COMMON_CLASSES_REASONS
#define FB_COMMON_CLASSES_REASONS

#include <string.h>

#define FB_FUNCTION __func__

namespace ScratchBird
{
	class Reasons
	{
	public:
#ifndef DEV_BUILD
		void reason(const char*) { }
#else
		Reasons()
			: frIndex(0)
		{
			memset(from, 0, sizeof(from));
		}

		void reason(const char* fr)
		{
			from[frIndex % FB_NELEM(from)] = fr;
			frIndex++;
			frIndex %= FB_NELEM(from);
		}

private:
		const char* from[8];
		unsigned frIndex;
#endif
	};
} // namespace ScratchBird

#endif // FB_COMMON_CLASSES_REASONS
