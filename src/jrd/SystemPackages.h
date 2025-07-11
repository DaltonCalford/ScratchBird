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
 *  Copyright (c) 2018 Adriano dos Santos Fernandes <adrianosf@gmail.com>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#ifndef JRD_SYSTEM_PACKAGES_H
#define JRD_SYSTEM_PACKAGES_H

#include "firebird.h"
#include "../common/status.h"
#include "../common/classes/init.h"
#include "../common/classes/array.h"
#include "../common/classes/objects_array.h"
#include "../jrd/constants.h"
#include "../jrd/ini.h"
#include "../jrd/jrd.h"
#include "firebird/Interface.h"
#include <initializer_list>
#include <functional>

namespace Jrd
{
	struct SystemProcedureParameter
	{
		SystemProcedureParameter(
			const char* aName,
			USHORT aFieldId,
			bool aNullable,
			const char* aDefaultText = nullptr,
			std::initializer_list<UCHAR> aDefaultBlr = {}
		)
			: name(aName),
			  fieldId(aFieldId),
			  nullable(aNullable),
			  defaultText(aDefaultText),
			  defaultBlr(*getDefaultMemoryPool(), aDefaultBlr)
		{
		}

		SystemProcedureParameter(ScratchBird::MemoryPool& pool, const SystemProcedureParameter& other)
			: defaultBlr(pool)
		{
			*this = other;
		}

		const char* name;
		USHORT fieldId;
		bool nullable;
		const char* defaultText = nullptr;
		ScratchBird::Array<UCHAR> defaultBlr;
	};

	struct SystemProcedure
	{
		typedef std::function<ScratchBird::IExternalProcedure* (
				ScratchBird::ThrowStatusExceptionWrapper*,
				ScratchBird::IExternalContext*,
				ScratchBird::IRoutineMetadata*,
				ScratchBird::IMetadataBuilder*,
				ScratchBird::IMetadataBuilder*
			)> Factory;

		SystemProcedure(
			ScratchBird::MemoryPool& pool,
			const char* aName,
			Factory aFactory,
			prc_t aType,
			std::initializer_list<SystemProcedureParameter> aInputParameters,
			std::initializer_list<SystemProcedureParameter> aOutputParameters
		)
			: name(aName),
			  factory(aFactory),
			  type(aType),
			  inputParameters(pool, aInputParameters),
			  outputParameters(pool, aOutputParameters)
		{
		}

		SystemProcedure(ScratchBird::MemoryPool& pool, const SystemProcedure& other)
			: inputParameters(pool),
			  outputParameters(pool)
		{
			*this = other;
		}

		const char* name;
		Factory factory;
		prc_t type;
		ScratchBird::ObjectsArray<SystemProcedureParameter> inputParameters;
		ScratchBird::ObjectsArray<SystemProcedureParameter> outputParameters;
	};

	struct SystemFunctionParameter
	{
		SystemFunctionParameter(
			const char* aName,
			USHORT aFieldId,
			bool aNullable,
			const char* aDefaultText = nullptr,
			std::initializer_list<UCHAR> aDefaultBlr = {}
		)
			: name(aName),
			  fieldId(aFieldId),
			  nullable(aNullable),
			  defaultText(aDefaultText),
			  defaultBlr(*getDefaultMemoryPool(), aDefaultBlr)
		{
		}

		SystemFunctionParameter(ScratchBird::MemoryPool& pool, const SystemFunctionParameter& other)
			: defaultBlr(pool)
		{
			*this = other;
		}

		const char* name;
		USHORT fieldId;
		bool nullable;
		const char* defaultText = nullptr;
		ScratchBird::Array<UCHAR> defaultBlr;
	};

	struct SystemFunctionReturnType
	{
		USHORT fieldId;
		bool nullable;
	};

	struct SystemFunction
	{
		typedef std::function<ScratchBird::IExternalFunction* (
				ScratchBird::ThrowStatusExceptionWrapper*,
				ScratchBird::IExternalContext*,
				ScratchBird::IRoutineMetadata*,
				ScratchBird::IMetadataBuilder*,
				ScratchBird::IMetadataBuilder*
			)> Factory;

		SystemFunction(
			ScratchBird::MemoryPool& pool,
			const char* aName,
			Factory aFactory,
			std::initializer_list<SystemFunctionParameter> aParameters,
			SystemFunctionReturnType aReturnType
		)
			: name(aName),
			  factory(aFactory),
			  parameters(pool, aParameters),
			  returnType(aReturnType)
		{
		}

		SystemFunction(ScratchBird::MemoryPool& pool, const SystemFunction& other)
			: parameters(pool)
		{
			*this = other;
		}

		const char* name;
		Factory factory;
		ScratchBird::ObjectsArray<SystemFunctionParameter> parameters;
		SystemFunctionReturnType returnType;
	};

	struct SystemPackage
	{
		SystemPackage(
			ScratchBird::MemoryPool& pool,
			const char* aName,
			USHORT aOdsVersion,
			std::initializer_list<SystemProcedure> aProcedures,
			std::initializer_list<SystemFunction> aFunctions
		)
			: name(aName),
			  odsVersion(aOdsVersion),
			  procedures(pool, aProcedures),
			  functions(pool, aFunctions)
		{
		}

		SystemPackage(ScratchBird::MemoryPool& pool, const SystemPackage& other)
			: procedures(pool),
			  functions(pool)
		{
			*this = other;
		}

		const char* name;
		USHORT odsVersion;
		ScratchBird::ObjectsArray<SystemProcedure> procedures;
		ScratchBird::ObjectsArray<SystemFunction> functions;

		static ScratchBird::ObjectsArray<SystemPackage>& get();

	private:
		SystemPackage(const SystemPackage&) = delete;
		SystemPackage& operator=(SystemPackage const&) = default;
	};

	class VoidMessage
	{
	public:
		typedef void Type;

	public:
		static void setup(ScratchBird::ThrowStatusExceptionWrapper*, ScratchBird::IMetadataBuilder*)
		{
		}
	};

	template <
		typename Input,
		typename Output,
		ScratchBird::IExternalResultSet* (*OpenFunction)(
			ScratchBird::ThrowStatusExceptionWrapper*,
			ScratchBird::IExternalContext*,
			const typename Input::Type*,
			typename Output::Type*
		)
	>
	struct SystemProcedureFactory
	{
		class SystemResultSet :
			public
				ScratchBird::DisposeIface<
					ScratchBird::IExternalResultSetImpl<
						SystemResultSet,
						ScratchBird::ThrowStatusExceptionWrapper
					>
				>
		{
		public:
			SystemResultSet(Attachment* aAttachment, ScratchBird::IExternalResultSet* aResultSet)
				: attachment(aAttachment),
				  resultSet(aResultSet)
			{
			}

		public:
			void dispose() override
			{
				delete this;
			}

		public:
			FB_BOOLEAN fetch(ScratchBird::ThrowStatusExceptionWrapper* status) override
			{
				// See comment in Attachment.h.
				// ScratchBird::AutoSetRestore<bool> autoInSystemPackage(&attachment->att_in_system_routine, true);

				return resultSet->fetch(status);
			}

		private:
			Attachment* attachment;
			ScratchBird::AutoDispose<ScratchBird::IExternalResultSet> resultSet;
		};

		class SystemProcedureImpl :
			public
				ScratchBird::DisposeIface<
					ScratchBird::IExternalProcedureImpl<
						SystemProcedureImpl,
						ScratchBird::ThrowStatusExceptionWrapper
					>
				>
		{
		public:
			SystemProcedureImpl(ScratchBird::ThrowStatusExceptionWrapper* status,
				ScratchBird::IMetadataBuilder* inBuilder, ScratchBird::IMetadataBuilder* outBuilder)
			{
				const auto tdbb = JRD_get_thread_data();
				attachment = tdbb->getAttachment();

				Input::setup(status, inBuilder);
				Output::setup(status, outBuilder);
			}

		public:
			void dispose() override
			{
				delete this;
			}

		public:
			void getCharSet(ScratchBird::ThrowStatusExceptionWrapper* status, ScratchBird::IExternalContext* context,
				char* name, unsigned nameSize) override
			{
				strncpy(name, "UTF8", nameSize);
			}

			ScratchBird::IExternalResultSet* open(ScratchBird::ThrowStatusExceptionWrapper* status,
				ScratchBird::IExternalContext* context, void* inMsg, void* outMsg) override
			{
				// See comment in Attachment.h.
				// ScratchBird::AutoSetRestore<bool> autoInSystemPackage(&attachment->att_in_system_routine, true);

				const auto resultSet = OpenFunction(status, context,
					static_cast<typename Input::Type*>(inMsg),
					static_cast<typename Output::Type*>(outMsg));

				return resultSet ? FB_NEW SystemResultSet(attachment, resultSet) : nullptr;
			}

		private:
			Attachment* attachment;
		};

		SystemProcedureImpl* operator()(
			ScratchBird::ThrowStatusExceptionWrapper* status,
			ScratchBird::IExternalContext* /*context*/,
			ScratchBird::IRoutineMetadata* /*metadata*/,
			ScratchBird::IMetadataBuilder* inBuilder,
			ScratchBird::IMetadataBuilder* outBuilder)
		{
			return FB_NEW SystemProcedureImpl(status, inBuilder, outBuilder);
		}
	};

	template <
		typename Input,
		typename Output,
		void (*ExecFunction)(
			ScratchBird::ThrowStatusExceptionWrapper*,
			ScratchBird::IExternalContext*,
			const typename Input::Type*,
			typename Output::Type*
		)
	>
	struct SystemFunctionFactory
	{
		class SystemFunctionImpl :
			public
				ScratchBird::DisposeIface<
					ScratchBird::IExternalFunctionImpl<
						SystemFunctionImpl,
						ScratchBird::ThrowStatusExceptionWrapper
					>
				>
		{
		public:
			SystemFunctionImpl(ScratchBird::ThrowStatusExceptionWrapper* status,
				ScratchBird::IMetadataBuilder* inBuilder, ScratchBird::IMetadataBuilder* outBuilder)
			{
				const auto tdbb = JRD_get_thread_data();
				attachment = tdbb->getAttachment();

				Input::setup(status, inBuilder);
				Output::setup(status, outBuilder);
			}

		public:
			void getCharSet(ScratchBird::ThrowStatusExceptionWrapper* status, ScratchBird::IExternalContext* context,
				char* name, unsigned nameSize) override
			{
				strncpy(name, "UTF8", nameSize);
			}

			void execute(ScratchBird::ThrowStatusExceptionWrapper* status,
				ScratchBird::IExternalContext* context, void* inMsg, void* outMsg) override
			{
				// See comment in Attachment.h.
				// ScratchBird::AutoSetRestore<bool> autoInSystemPackage(&attachment->att_in_system_routine, true);

				ExecFunction(status, context,
					static_cast<typename Input::Type*>(inMsg),
					static_cast<typename Output::Type*>(outMsg));
			}

		private:
			Attachment* attachment;
		};

		SystemFunctionImpl* operator()(
			ScratchBird::ThrowStatusExceptionWrapper* status,
			ScratchBird::IExternalContext* /*context*/,
			ScratchBird::IRoutineMetadata* /*metadata*/,
			ScratchBird::IMetadataBuilder* inBuilder,
			ScratchBird::IMetadataBuilder* outBuilder)
		{
			return FB_NEW SystemFunctionImpl(status, inBuilder, outBuilder);
		}
	};
}	// namespace Jrd

#endif	// JRD_SYSTEM_PACKAGES_H
