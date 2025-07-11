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
 *  The Original Code was created by Adriano dos Santos Fernandes
 *  for the ScratchBird Open Source RDBMS project.
 *
 *  Copyright (c) 2020 Adriano dos Santos Fernandes <adrianosf@gmail.com>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#ifndef JRD_BLOB_UTIL_H
#define JRD_BLOB_UTIL_H

#include "firebird.h"
#include "firebird/Message.h"
#include "../common/classes/fb_string.h"
#include "../common/classes/ImplementHelper.h"
#include "../common/status.h"
#include "../jrd/SystemPackages.h"

namespace Jrd {


class BlobUtilPackage : public SystemPackage
{
public:
	BlobUtilPackage(ScratchBird::MemoryPool& pool);

private:
	FB_MESSAGE(BinaryMessage, ScratchBird::ThrowStatusExceptionWrapper,
		(FB_INTL_VARCHAR(MAX_VARY_COLUMN_SIZE, 0), data)
	);

	FB_MESSAGE(BlobMessage, ScratchBird::ThrowStatusExceptionWrapper,
		(FB_BLOB, blob)
	);

	FB_MESSAGE(HandleMessage, ScratchBird::ThrowStatusExceptionWrapper,
		(FB_INTEGER, handle)
	);

	FB_MESSAGE(BooleanMessage, ScratchBird::ThrowStatusExceptionWrapper,
		(FB_BOOLEAN, boolean)
	);

	//----------

	static ScratchBird::IExternalResultSet* cancelBlobProcedure(ScratchBird::ThrowStatusExceptionWrapper* status,
		ScratchBird::IExternalContext* context, const BlobMessage::Type* in, void* out);

	//----------

	static ScratchBird::IExternalResultSet* closeHandleProcedure(ScratchBird::ThrowStatusExceptionWrapper* status,
		ScratchBird::IExternalContext* context, const HandleMessage::Type* in, void* out);

	//----------

	static void isWritableFunction(ScratchBird::ThrowStatusExceptionWrapper* status,
		ScratchBird::IExternalContext* context,
		const BlobMessage::Type* in, BooleanMessage::Type* out);

	//----------

	FB_MESSAGE(NewBlobInput, ScratchBird::ThrowStatusExceptionWrapper,
		(FB_BOOLEAN, segmented)
		(FB_BOOLEAN, tempStorage)
	);

	static void newBlobFunction(ScratchBird::ThrowStatusExceptionWrapper* status,
		ScratchBird::IExternalContext* context,
		const NewBlobInput::Type* in, BlobMessage::Type* out);

	//----------

	static void openBlobFunction(ScratchBird::ThrowStatusExceptionWrapper* status,
		ScratchBird::IExternalContext* context,
		const BlobMessage::Type* in, HandleMessage::Type* out);

	//----------

	FB_MESSAGE(SeekInput, ScratchBird::ThrowStatusExceptionWrapper,
		(FB_INTEGER, handle)
		(FB_INTEGER, mode)
		(FB_INTEGER, offset)
	);

	FB_MESSAGE(SeekOutput, ScratchBird::ThrowStatusExceptionWrapper,
		(FB_INTEGER, offset)
	);

	static void seekFunction(ScratchBird::ThrowStatusExceptionWrapper* status,
		ScratchBird::IExternalContext* context, const SeekInput::Type* in, SeekOutput::Type* out);

	//----------

	FB_MESSAGE(ReadDataInput, ScratchBird::ThrowStatusExceptionWrapper,
		(FB_INTEGER, handle)
		(FB_INTEGER, length)
	);

	static void readDataFunction(ScratchBird::ThrowStatusExceptionWrapper* status,
		ScratchBird::IExternalContext* context,
		const ReadDataInput::Type* in, BinaryMessage::Type* out);

	//----------

	static void makeBlobFunction(ScratchBird::ThrowStatusExceptionWrapper* status,
		ScratchBird::IExternalContext* context,
		const HandleMessage::Type* in, BlobMessage::Type* out);
};


}	// namespace

#endif	// JRD_BLOB_UTIL_H
