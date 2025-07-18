/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		Aligned.h
 *	DESCRIPTION:	Aligned bytes buffer (typically on stack).
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
 *  Copyright (c) 2014 Alexander Peshkoff <peshkoff@mail.ru>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#ifndef CLASSES_ALIGNED_BUFFER_H
#define CLASSES_ALIGNED_BUFFER_H

namespace ScratchBird {

template <FB_SIZE_T N, FB_SIZE_T A = FB_ALIGNMENT>
class AlignedBuffer
{
private:
	UCHAR buffer[N + A - 1];

public:
	AlignedBuffer()
	{ }

	operator UCHAR*()
	{
		return (UCHAR*) FB_ALIGN(buffer, A);
	}

	FB_SIZE_T size() const
	{
		return N;
	}
};

} // namespace ScratchBird

#endif // CLASSES_ALIGNED_BUFFER_H
