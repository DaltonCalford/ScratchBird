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
 *  Copyright (c) 2008 Adriano dos Santos Fernandes <adrianosf@uol.com.br>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#ifndef JRD_EXT_ENGINE_MANAGER_H
#define JRD_EXT_ENGINE_MANAGER_H

#include "firebird/Interface.h"

#include <memory>
#include <optional>

#include "../common/classes/array.h"
#include "../common/classes/fb_string.h"
#include "../common/classes/GenericMap.h"
#include "../jrd/MetaName.h"
#include "../jrd/QualifiedName.h"
#include "../common/classes/NestConst.h"
#include "../common/classes/auto.h"
#include "../common/classes/rwlock.h"
#include "../common/classes/ImplementHelper.h"
#include "../common/StatementMetadata.h"
#include "../common/classes/GetPlugins.h"

struct dsc;

namespace Jrd {

class thread_db;
class jrd_prc;
class Request;
class jrd_tra;
class Attachment;
class CompilerScratch;
class Database;
class Format;
class Trigger;
class Function;
class DeclareVariableNode;
class StmtNode;
class ValueExprNode;
struct impure_value;
struct record_param;


class ExtEngineManager final : public ScratchBird::PermanentStorage
{
private:
	class AttachmentImpl;
	template <typename T> class ContextManager;
	class TransactionImpl;

	class RoutineMetadata final :
		public ScratchBird::VersionedIface<ScratchBird::IRoutineMetadataImpl<RoutineMetadata, ScratchBird::CheckStatusWrapper> >,
		public ScratchBird::PermanentStorage
	{
	public:
		explicit RoutineMetadata(MemoryPool& pool)
			: PermanentStorage(pool),
			  name(pool),
			  entryPoint(pool),
			  body(pool),
			  triggerTable(pool),
			  triggerType(0)
		{
		}

		const char* getPackage(ScratchBird::CheckStatusWrapper* /*status*/) const
		{
			return name.package.nullStr();
		}

		const char* getName(ScratchBird::CheckStatusWrapper* /*status*/) const
		{
			return name.object.c_str();
		}

		const char* getEntryPoint(ScratchBird::CheckStatusWrapper* /*status*/) const
		{
			return entryPoint.c_str();
		}

		const char* getBody(ScratchBird::CheckStatusWrapper* /*status*/) const
		{
			return body.c_str();
		}

		ScratchBird::IMessageMetadata* getInputMetadata(ScratchBird::CheckStatusWrapper* /*status*/) const
		{
			return getMetadata(inputParameters);
		}

		ScratchBird::IMessageMetadata* getOutputMetadata(ScratchBird::CheckStatusWrapper* /*status*/) const
		{
			return getMetadata(outputParameters);
		}

		ScratchBird::IMessageMetadata* getTriggerMetadata(ScratchBird::CheckStatusWrapper* /*status*/) const
		{
			return getMetadata(triggerFields);
		}

		const char* getTriggerTable(ScratchBird::CheckStatusWrapper* /*status*/) const
		{
			return triggerTable.object.c_str();
		}

		unsigned getTriggerType(ScratchBird::CheckStatusWrapper* /*status*/) const
		{
			return triggerType;
		}

		const char* getSchema(ScratchBird::CheckStatusWrapper* /*status*/) const
		{
			return name.schema.c_str();
		}

	public:
		QualifiedName name;
		ScratchBird::string entryPoint;
		ScratchBird::string body;
		ScratchBird::RefPtr<ScratchBird::IMessageMetadata> inputParameters;
		ScratchBird::RefPtr<ScratchBird::IMessageMetadata> outputParameters;
		ScratchBird::RefPtr<ScratchBird::IMessageMetadata> triggerFields;
		QualifiedName triggerTable;
		unsigned triggerType;

	private:
		static ScratchBird::IMessageMetadata* getMetadata(const ScratchBird::IMessageMetadata* par)
		{
			ScratchBird::IMessageMetadata* rc = const_cast<ScratchBird::IMessageMetadata*>(par);
			rc->addRef();
			return rc;
		}
	};

	class ExternalContextImpl :
		public ScratchBird::VersionedIface<ScratchBird::IExternalContextImpl<ExternalContextImpl, ScratchBird::CheckStatusWrapper> >
	{
	friend class AttachmentImpl;

	public:
		ExternalContextImpl(thread_db* tdbb, ScratchBird::IExternalEngine* aEngine);
		virtual ~ExternalContextImpl();

		void releaseTransaction();
		void setTransaction(thread_db* tdbb);

		ScratchBird::IMaster* getMaster();
		ScratchBird::IExternalEngine* getEngine(ScratchBird::CheckStatusWrapper* status);
		ScratchBird::IAttachment* getAttachment(ScratchBird::CheckStatusWrapper* status);
		ScratchBird::ITransaction* getTransaction(ScratchBird::CheckStatusWrapper* status);
		const char* getUserName();
		const char* getDatabaseName();
		const char* getClientCharSet();
		int obtainInfoCode();
		void* getInfo(int code);
		void* setInfo(int code, void* value);

	private:
		ScratchBird::IExternalEngine* engine;
		Attachment* internalAttachment;
		ScratchBird::ITransaction* internalTransaction;
		ScratchBird::IAttachment* externalAttachment;
		ScratchBird::ITransaction* externalTransaction;
		ScratchBird::GenericMap<ScratchBird::NonPooled<int, void*> > miscInfo;
		MetaName clientCharSet;
	};

	struct EngineAttachment
	{
		EngineAttachment(ScratchBird::IExternalEngine* aEngine, Jrd::Attachment* aAttachment)
			: engine(aEngine),
			  attachment(aAttachment)
		{
		}

		static bool greaterThan(const EngineAttachment& i1, const EngineAttachment& i2)
		{
			return (i1.engine > i2.engine) ||
				(i1.engine == i2.engine && i1.attachment > i2.attachment);
		}

		ScratchBird::IExternalEngine* engine;
		Jrd::Attachment* attachment;
	};

	struct EngineAttachmentInfo
	{
		EngineAttachmentInfo()
			: engine(NULL),
			  context(NULL),
			  adminCharSet(0)
		{
		}

		ScratchBird::IExternalEngine* engine;
		ScratchBird::AutoPtr<ExternalContextImpl> context;
		USHORT adminCharSet;
	};

public:
	class ExtRoutine
	{
	public:
		ExtRoutine(thread_db* tdbb, ExtEngineManager* aExtManager,
			ScratchBird::IExternalEngine* aEngine, RoutineMetadata* aMetadata);
		virtual ~ExtRoutine() = default;

	private:
		class PluginDeleter
		{
		public:
			void operator()(ScratchBird::IPluginBase* ptr);
		};

	protected:
		ExtEngineManager* extManager;
		std::unique_ptr<ScratchBird::IExternalEngine, PluginDeleter> engine;
		ScratchBird::AutoPtr<RoutineMetadata> metadata;
		Database* database;
	};

	class Function final : public ExtRoutine
	{
	private:
		struct Impl;	// hack to avoid circular inclusion of headers

	public:
		Function(thread_db* tdbb, MemoryPool& pool, CompilerScratch* csb, ExtEngineManager* aExtManager,
			ScratchBird::IExternalEngine* aEngine,
			RoutineMetadata* aMetadata,
			ScratchBird::IExternalFunction* aFunction,
			ScratchBird::RefPtr<ScratchBird::IMessageMetadata> extInputParameters,
			ScratchBird::RefPtr<ScratchBird::IMessageMetadata> extOutputParameters,
			const Jrd::Function* aUdf);
		~Function() override;

		void execute(thread_db* tdbb, Request* request, jrd_tra* transaction,
			unsigned inMsgLength, UCHAR* inMsg, unsigned outMsgLength, UCHAR* outMsg) const;

	private:
		void validateParameters(thread_db* tdbb, UCHAR* msg, bool input) const;

	private:
		ScratchBird::IExternalFunction* function;
		const Jrd::Function* udf;
		ScratchBird::AutoPtr<Format> extInputFormat;
		ScratchBird::AutoPtr<Format> extOutputFormat;
		ScratchBird::AutoPtr<Impl> impl;
		std::optional<ULONG> extInputImpureOffset;
		std::optional<ULONG> extOutputImpureOffset;
	};

	class ResultSet;

	class Procedure final : public ExtRoutine
	{
	friend class ResultSet;

	public:
		Procedure(thread_db* tdbb, ExtEngineManager* aExtManager,
			ScratchBird::IExternalEngine* aEngine,
			RoutineMetadata* aMetadata,
			ScratchBird::IExternalProcedure* aProcedure,
			const jrd_prc* aPrc);
		~Procedure() override;

		ResultSet* open(thread_db* tdbb, UCHAR* inMsg, UCHAR* outMsg) const;

	private:
		ScratchBird::IExternalProcedure* procedure;
		const jrd_prc* prc;
	};

	class ResultSet
	{
	public:
		ResultSet(thread_db* tdbb, UCHAR* inMsg, UCHAR* outMsg, const Procedure* aProcedure);
		~ResultSet();

		bool fetch(thread_db* tdbb);

	private:
		const Procedure* procedure;
		Attachment* attachment;
		bool firstFetch;
		EngineAttachmentInfo* attInfo;
		ScratchBird::IExternalResultSet* resultSet;
		USHORT charSet;
	};

	class Trigger final : public ExtRoutine
	{
	public:
		Trigger(thread_db* tdbb, MemoryPool& pool, CompilerScratch* csb, ExtEngineManager* aExtManager,
			ScratchBird::IExternalEngine* aEngine, RoutineMetadata* aMetadata,
			ScratchBird::IExternalTrigger* aTrigger, const Jrd::Trigger* aTrg);
		~Trigger() override;

		void execute(thread_db* tdbb, Request* request, unsigned action,
			record_param* oldRpb, record_param* newRpb) const;

	private:
		void setupComputedFields(thread_db* tdbb, MemoryPool& pool, CompilerScratch* csb);
		void setValues(thread_db* tdbb, Request* request, ScratchBird::Array<UCHAR>& msgBuffer, record_param* rpb) const;

	public:
		ScratchBird::Array<NestConst<StmtNode>> computedStatements;

	private:
		ScratchBird::AutoPtr<Format> format;
		ScratchBird::IExternalTrigger* trigger;
		const Jrd::Trigger* trg;
		ScratchBird::Array<USHORT> fieldsPos;
		ScratchBird::Array<const DeclareVariableNode*> varDecls;
		USHORT computedCount;
	};

public:
	explicit ExtEngineManager(MemoryPool& p);
	~ExtEngineManager();

public:
	static void initialize();

public:
	void closeAttachment(thread_db* tdbb, Attachment* attachment);

	void makeFunction(thread_db* tdbb, CompilerScratch* csb, Jrd::Function* udf,
		const MetaName& engine, const ScratchBird::string& entryPoint,
		const ScratchBird::string& body);
	void makeProcedure(thread_db* tdbb, CompilerScratch* csb, jrd_prc* prc,
		const MetaName& engine, const ScratchBird::string& entryPoint,
		const ScratchBird::string& body);
	void makeTrigger(thread_db* tdbb, CompilerScratch* csb, Jrd::Trigger* trg,
		const MetaName& engine, const ScratchBird::string& entryPoint,
		const ScratchBird::string& body, unsigned type);

private:
	ScratchBird::IExternalEngine* getEngine(thread_db* tdbb,
		const MetaName& name);
	EngineAttachmentInfo* getEngineAttachment(thread_db* tdbb,
		const MetaName& name);
	EngineAttachmentInfo* getEngineAttachment(thread_db* tdbb,
		ScratchBird::IExternalEngine* engine, bool closing = false);
	void setupAdminCharSet(thread_db* tdbb, ScratchBird::IExternalEngine* engine,
		EngineAttachmentInfo* attInfo);

private:
	typedef ScratchBird::GenericMap<ScratchBird::Pair<
		ScratchBird::Left<MetaName, ScratchBird::IExternalEngine*> > > EnginesMap;
	typedef ScratchBird::GenericMap<ScratchBird::Pair<ScratchBird::NonPooled<
		EngineAttachment, EngineAttachmentInfo*> >, EngineAttachment> EnginesAttachmentsMap;

	ScratchBird::RWLock enginesLock;
	EnginesMap engines;
	EnginesAttachmentsMap enginesAttachments;
};


}	// namespace Jrd

#endif	// JRD_EXT_ENGINE_MANAGER_H
