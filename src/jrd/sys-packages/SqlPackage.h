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
 *  Copyright (c) 2023 Adriano dos Santos Fernandes <adrianosf@gmail.com>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#ifndef JRD_SYS_PACKAGES_SQL_PACKAGE_H
#define JRD_SYS_PACKAGES_SQL_PACKAGE_H

#include "firebird.h"
#include "firebird/Message.h"
#include "../common/classes/array.h"
#include "../common/classes/MetaString.h"
#include "../common/classes/objects_array.h"
#include "../jrd/SystemPackages.h"

namespace Jrd {


class SqlPackage final : public SystemPackage
{
public:
	SqlPackage(ScratchBird::MemoryPool& pool);

	SqlPackage(const SqlPackage&) = delete;
	SqlPackage& operator=(const SqlPackage&) = delete;

private:
	FB_MESSAGE(ExplainInput, ScratchBird::ThrowStatusExceptionWrapper,
		(FB_BLOB, sql)
	);

	FB_MESSAGE(ExplainOutput, ScratchBird::ThrowStatusExceptionWrapper,
		(FB_INTEGER, planLine)
		(FB_BIGINT, recordSourceId)
		(FB_BIGINT, parentRecordSourceId)
		(FB_INTEGER, level)
		(FB_SMALLINT, objectType)
		(FB_INTL_VARCHAR(METADATA_IDENTIFIER_CHAR_LEN * METADATA_BYTES_PER_CHAR, CS_METADATA), schemaName)
		(FB_INTL_VARCHAR(METADATA_IDENTIFIER_CHAR_LEN * METADATA_BYTES_PER_CHAR, CS_METADATA), packageName)
		(FB_INTL_VARCHAR(METADATA_IDENTIFIER_CHAR_LEN * METADATA_BYTES_PER_CHAR, CS_METADATA), objectName)
		(FB_INTL_VARCHAR(255 * METADATA_BYTES_PER_CHAR, CS_METADATA), alias)
		(FB_DOUBLE, cardinality)
		(FB_INTEGER, recordLength)
		(FB_INTEGER, keyLength)
		(FB_BLOB, accessPath)
	);

	class ExplainResultSet :
		public
			ScratchBird::DisposeIface<
				ScratchBird::IExternalResultSetImpl<
					ExplainResultSet,
					ScratchBird::ThrowStatusExceptionWrapper
				>
			>
	{
	public:
		ExplainResultSet(ScratchBird::ThrowStatusExceptionWrapper* status, ScratchBird::IExternalContext* context,
			const ExplainInput::Type* in, ExplainOutput::Type* out);

	public:
		void dispose() override
		{
			delete this;
		}

	public:
		FB_BOOLEAN fetch(ScratchBird::ThrowStatusExceptionWrapper* status) override;

	private:
		ExplainOutput::Type* out;
		ScratchBird::Array<ExplainOutput::Type> resultEntries{*getDefaultMemoryPool()};
		ScratchBird::Array<ExplainOutput::Type>::const_iterator resultIterator = nullptr;
	};

	static ScratchBird::IExternalResultSet* explainProcedure(ScratchBird::ThrowStatusExceptionWrapper* status,
		ScratchBird::IExternalContext* context, const ExplainInput::Type* in, ExplainOutput::Type* out);

	//----------

	FB_MESSAGE(ParseUnqualifiedNamesInput, ScratchBird::ThrowStatusExceptionWrapper,
		(FB_INTL_VARCHAR(MAX_VARY_COLUMN_SIZE / METADATA_BYTES_PER_CHAR * METADATA_BYTES_PER_CHAR, CS_METADATA), names)
	);

	FB_MESSAGE(ParseUnqualifiedNamesOutput, ScratchBird::ThrowStatusExceptionWrapper,
		(FB_INTL_VARCHAR(METADATA_IDENTIFIER_CHAR_LEN * METADATA_BYTES_PER_CHAR, CS_METADATA), name)
	);

	class ParseUnqualifiedNamesResultSet :
		public
			ScratchBird::DisposeIface<
				ScratchBird::IExternalResultSetImpl<
					ParseUnqualifiedNamesResultSet,
					ScratchBird::ThrowStatusExceptionWrapper
				>
			>
	{
	public:
		ParseUnqualifiedNamesResultSet(ScratchBird::ThrowStatusExceptionWrapper* status, ScratchBird::IExternalContext* context,
			const ParseUnqualifiedNamesInput::Type* in, ParseUnqualifiedNamesOutput::Type* out);

	public:
		void dispose() override
		{
			delete this;
		}

	public:
		FB_BOOLEAN fetch(ScratchBird::ThrowStatusExceptionWrapper* status) override;

	private:
		ParseUnqualifiedNamesOutput::Type* out;
		ScratchBird::ObjectsArray<ScratchBird::MetaString> resultEntries{*getDefaultMemoryPool()};
		ScratchBird::ObjectsArray<ScratchBird::MetaString>::const_iterator resultIterator;
	};

	static ScratchBird::IExternalResultSet* parseUnqualifiedNamesProcedure(ScratchBird::ThrowStatusExceptionWrapper* status,
		ScratchBird::IExternalContext* context,
		const ParseUnqualifiedNamesInput::Type* in, ParseUnqualifiedNamesOutput::Type* out);
};


}	// namespace

#endif	// JRD_SYS_PACKAGES_SQL_PACKAGE_H
