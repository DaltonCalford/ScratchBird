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
 *  The Original Code was created by Alex Peshkov
 *  for the ScratchBird Open Source RDBMS project.
 *
 *  Copyright (c) 2011 Alex Peshkov <peshkoff@mail.ru>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#ifndef JRD_ENGINE_INTERFACE_H
#define JRD_ENGINE_INTERFACE_H

#include "firebird/Interface.h"
#include "../common/classes/ImplementHelper.h"
#include "../common/StatementMetadata.h"
#include "../common/classes/RefCounted.h"

namespace Jrd {

// Engine objects used by interface objects
class blb;
class jrd_tra;
class DsqlCursor;
class DsqlBatch;
class DsqlRequest;
class Statement;
class StableAttachmentPart;
class Attachment;
class Service;
class UserId;
class Applier;

// forward declarations
class JStatement;
class JAttachment;
class JProvider;

class JBlob final :
	public ScratchBird::RefCntIface<ScratchBird::IBlobImpl<JBlob, ScratchBird::CheckStatusWrapper> >
{
public:
	// IBlob implementation
	int release() override;
	void getInfo(ScratchBird::CheckStatusWrapper* status,
		unsigned int itemsLength, const unsigned char* items,
		unsigned int bufferLength, unsigned char* buffer) override;
	int getSegment(ScratchBird::CheckStatusWrapper* status, unsigned int length, void* buffer,
		unsigned int* segmentLength) override;
	void putSegment(ScratchBird::CheckStatusWrapper* status, unsigned int length, const void* buffer) override;
	void cancel(ScratchBird::CheckStatusWrapper* status) override;
	void close(ScratchBird::CheckStatusWrapper* status) override;
	int seek(ScratchBird::CheckStatusWrapper* status, int mode, int offset) override;			// returns position
	void deprecatedCancel(ScratchBird::CheckStatusWrapper* status) override;
	void deprecatedClose(ScratchBird::CheckStatusWrapper* status) override;

public:
	JBlob(blb* handle, StableAttachmentPart* sa);

	StableAttachmentPart* getAttachment()
	{
		return sAtt;
	}

	blb* getHandle() noexcept
	{
		return blob;
	}

	void clearHandle()
	{
		blob = NULL;
	}

private:
	blb* blob;
	ScratchBird::RefPtr<StableAttachmentPart> sAtt;

	void freeEngineData(ScratchBird::CheckStatusWrapper* status);
	void internalClose(ScratchBird::CheckStatusWrapper* status);
};

class JTransaction final :
	public ScratchBird::RefCntIface<ScratchBird::ITransactionImpl<JTransaction, ScratchBird::CheckStatusWrapper> >
{
public:
	// ITransaction implementation
	int release() override;
	void getInfo(ScratchBird::CheckStatusWrapper* status,
		unsigned int itemsLength, const unsigned char* items,
		unsigned int bufferLength, unsigned char* buffer) override;
	void prepare(ScratchBird::CheckStatusWrapper* status,
		unsigned int msg_length = 0, const unsigned char* message = 0) override;
	void commit(ScratchBird::CheckStatusWrapper* status) override;
	void commitRetaining(ScratchBird::CheckStatusWrapper* status) override;
	void rollback(ScratchBird::CheckStatusWrapper* status) override;
	void rollbackRetaining(ScratchBird::CheckStatusWrapper* status) override;
	void disconnect(ScratchBird::CheckStatusWrapper* status) override;
	ScratchBird::ITransaction* join(ScratchBird::CheckStatusWrapper* status, ScratchBird::ITransaction* transaction) override;
	JTransaction* validate(ScratchBird::CheckStatusWrapper* status, ScratchBird::IAttachment* testAtt) override;
	JTransaction* enterDtc(ScratchBird::CheckStatusWrapper* status) override;
	void deprecatedCommit(ScratchBird::CheckStatusWrapper* status) override;
	void deprecatedRollback(ScratchBird::CheckStatusWrapper* status) override;
	void deprecatedDisconnect(ScratchBird::CheckStatusWrapper* status) override;

public:
	JTransaction(jrd_tra* handle, StableAttachmentPart* sa);

	jrd_tra* getHandle() noexcept
	{
		return transaction;
	}

	void setHandle(jrd_tra* handle)
	{
		transaction = handle;
	}

	StableAttachmentPart* getAttachment()
	{
		return sAtt;
	}

	void clear()
	{
		transaction = NULL;
		release();
	}

private:
	jrd_tra* transaction;
	ScratchBird::RefPtr<StableAttachmentPart> sAtt;

	JTransaction(JTransaction* from);

	void freeEngineData(ScratchBird::CheckStatusWrapper* status);
	void internalCommit(ScratchBird::CheckStatusWrapper* status);
	void internalRollback(ScratchBird::CheckStatusWrapper* status);
	void internalDisconnect(ScratchBird::CheckStatusWrapper* status);
};

class JResultSet final :
	public ScratchBird::RefCntIface<ScratchBird::IResultSetImpl<JResultSet, ScratchBird::CheckStatusWrapper> >
{
public:
	// IResultSet implementation
	int release() override;
	int fetchNext(ScratchBird::CheckStatusWrapper* status, void* message) override;
	int fetchPrior(ScratchBird::CheckStatusWrapper* status, void* message) override;
	int fetchFirst(ScratchBird::CheckStatusWrapper* status, void* message) override;
	int fetchLast(ScratchBird::CheckStatusWrapper* status, void* message) override;
	int fetchAbsolute(ScratchBird::CheckStatusWrapper* status, int position, void* message) override;
	int fetchRelative(ScratchBird::CheckStatusWrapper* status, int offset, void* message) override;
	FB_BOOLEAN isEof(ScratchBird::CheckStatusWrapper* status) override;
	FB_BOOLEAN isBof(ScratchBird::CheckStatusWrapper* status) override;
	ScratchBird::IMessageMetadata* getMetadata(ScratchBird::CheckStatusWrapper* status) override;
	void close(ScratchBird::CheckStatusWrapper* status) override;
	void deprecatedClose(ScratchBird::CheckStatusWrapper* status) override;
	void setDelayedOutputFormat(ScratchBird::CheckStatusWrapper* status, ScratchBird::IMessageMetadata* format) override;
	void getInfo(ScratchBird::CheckStatusWrapper* status,
		unsigned int itemsLength, const unsigned char* items,
		unsigned int bufferLength, unsigned char* buffer) override;

public:
	JResultSet(DsqlCursor* handle, JStatement* aStatement);

	StableAttachmentPart* getAttachment();

	DsqlCursor* getHandle() noexcept
	{
		return cursor;
	}

	void resetHandle()
	{
		cursor = NULL;
	}

private:
	DsqlCursor* cursor;
	ScratchBird::RefPtr<JStatement> statement;
	int state;

	void freeEngineData(ScratchBird::CheckStatusWrapper* status);
};

class JBatch final :
	public ScratchBird::RefCntIface<ScratchBird::IBatchImpl<JBatch, ScratchBird::CheckStatusWrapper> >
{
public:
	// IBatch implementation
	int release() override;
	void add(ScratchBird::CheckStatusWrapper* status, unsigned count, const void* inBuffer) override;
	void addBlob(ScratchBird::CheckStatusWrapper* status, unsigned length, const void* inBuffer, ISC_QUAD* blobId,
		unsigned parLength, const unsigned char* par) override;
	void appendBlobData(ScratchBird::CheckStatusWrapper* status, unsigned length, const void* inBuffer) override;
	void addBlobStream(ScratchBird::CheckStatusWrapper* status, unsigned length, const void* inBuffer) override;
	void registerBlob(ScratchBird::CheckStatusWrapper* status, const ISC_QUAD* existingBlob, ISC_QUAD* blobId) override;
	ScratchBird::IBatchCompletionState* execute(ScratchBird::CheckStatusWrapper* status,
		ScratchBird::ITransaction* transaction) override;
	void cancel(ScratchBird::CheckStatusWrapper* status) override;
	unsigned getBlobAlignment(ScratchBird::CheckStatusWrapper* status) override;
	ScratchBird::IMessageMetadata* getMetadata(ScratchBird::CheckStatusWrapper* status) override;
	void setDefaultBpb(ScratchBird::CheckStatusWrapper* status, unsigned parLength, const unsigned char* par) override;
	void close(ScratchBird::CheckStatusWrapper* status) override;
	void deprecatedClose(ScratchBird::CheckStatusWrapper* status) override;
	void getInfo(ScratchBird::CheckStatusWrapper* status, unsigned int itemsLength, const unsigned char* items,
		unsigned int bufferLength, unsigned char* buffer) override;

public:
	JBatch(DsqlBatch* handle, JStatement* aStatement, ScratchBird::IMessageMetadata* aMetadata);

	StableAttachmentPart* getAttachment();

	DsqlBatch* getHandle() noexcept
	{
		return batch;
	}

	void resetHandle()
	{
		batch = NULL;
	}

private:
	DsqlBatch* batch;
	ScratchBird::RefPtr<JStatement> statement;
	ScratchBird::RefPtr<ScratchBird::IMessageMetadata> m_meta;

	void freeEngineData(ScratchBird::CheckStatusWrapper* status);
};

class JReplicator final :
	public ScratchBird::RefCntIface<ScratchBird::IReplicatorImpl<JReplicator, ScratchBird::CheckStatusWrapper> >
{
public:
	// IReplicator implementation
	int release() override;
	void process(ScratchBird::CheckStatusWrapper* status, unsigned length, const unsigned char* data) override;
	void close(ScratchBird::CheckStatusWrapper* status) override;
	void deprecatedClose(ScratchBird::CheckStatusWrapper* status) override;

public:
	JReplicator(Applier* appl, StableAttachmentPart* sa);

	StableAttachmentPart* getAttachment()
	{
		return sAtt;
	}

	Applier* getHandle() noexcept
	{
		return applier;
	}

	void resetHandle()
	{
		applier = NULL;
	}

private:
	Applier* applier;
	ScratchBird::RefPtr<StableAttachmentPart> sAtt;

	void freeEngineData(ScratchBird::CheckStatusWrapper* status);
};

class JStatement final :
	public ScratchBird::RefCntIface<ScratchBird::IStatementImpl<JStatement, ScratchBird::CheckStatusWrapper> >
{
public:
	// IStatement implementation
	int release() override;
	void getInfo(ScratchBird::CheckStatusWrapper* status,
		unsigned int itemsLength, const unsigned char* items,
		unsigned int bufferLength, unsigned char* buffer) override;
	void free(ScratchBird::CheckStatusWrapper* status) override;
	void deprecatedFree(ScratchBird::CheckStatusWrapper* status) override;
	ISC_UINT64 getAffectedRecords(ScratchBird::CheckStatusWrapper* userStatus) override;
	ScratchBird::IMessageMetadata* getOutputMetadata(ScratchBird::CheckStatusWrapper* userStatus) override;
	ScratchBird::IMessageMetadata* getInputMetadata(ScratchBird::CheckStatusWrapper* userStatus) override;
	unsigned getType(ScratchBird::CheckStatusWrapper* status) override;
    const char* getPlan(ScratchBird::CheckStatusWrapper* status, FB_BOOLEAN detailed) override;
	ScratchBird::ITransaction* execute(ScratchBird::CheckStatusWrapper* status,
		ScratchBird::ITransaction* transaction, ScratchBird::IMessageMetadata* inMetadata, void* inBuffer,
		ScratchBird::IMessageMetadata* outMetadata, void* outBuffer) override;
	JResultSet* openCursor(ScratchBird::CheckStatusWrapper* status,
		ScratchBird::ITransaction* transaction, ScratchBird::IMessageMetadata* inMetadata, void* inBuffer,
		ScratchBird::IMessageMetadata* outMetadata, unsigned int flags) override;
	void setCursorName(ScratchBird::CheckStatusWrapper* status, const char* name) override;
	unsigned getFlags(ScratchBird::CheckStatusWrapper* status) override;

	unsigned int getTimeout(ScratchBird::CheckStatusWrapper* status) override;
	void setTimeout(ScratchBird::CheckStatusWrapper* status, unsigned int timeOut) override;
	JBatch* createBatch(ScratchBird::CheckStatusWrapper* status, ScratchBird::IMessageMetadata* inMetadata,
		unsigned parLength, const unsigned char* par) override;

	unsigned getMaxInlineBlobSize(ScratchBird::CheckStatusWrapper* status) override;
	void setMaxInlineBlobSize(ScratchBird::CheckStatusWrapper* status, unsigned size) override;

public:
	JStatement(DsqlRequest* handle, StableAttachmentPart* sa, ScratchBird::Array<UCHAR>& meta);

	StableAttachmentPart* getAttachment()
	{
		return sAtt;
	}

	DsqlRequest* getHandle() noexcept
	{
		return statement;
	}

private:
	DsqlRequest* statement;
	ScratchBird::RefPtr<StableAttachmentPart> sAtt;
	ScratchBird::StatementMetadata metadata;

	void freeEngineData(ScratchBird::CheckStatusWrapper* status);
};

class JRequest final :
	public ScratchBird::RefCntIface<ScratchBird::IRequestImpl<JRequest, ScratchBird::CheckStatusWrapper> >
{
public:
	// IRequest implementation
	int release() override;
	void receive(ScratchBird::CheckStatusWrapper* status, int level, unsigned int msg_type,
		unsigned int length, void* message) override;
	void send(ScratchBird::CheckStatusWrapper* status, int level, unsigned int msg_type,
		unsigned int length, const void* message) override;
	void getInfo(ScratchBird::CheckStatusWrapper* status, int level,
		unsigned int itemsLength, const unsigned char* items,
		unsigned int bufferLength, unsigned char* buffer) override;
	void start(ScratchBird::CheckStatusWrapper* status, ScratchBird::ITransaction* tra, int level) override;
	void startAndSend(ScratchBird::CheckStatusWrapper* status, ScratchBird::ITransaction* tra, int level,
		unsigned int msg_type, unsigned int length, const void* message) override;
	void unwind(ScratchBird::CheckStatusWrapper* status, int level) override;
	void free(ScratchBird::CheckStatusWrapper* status) override;
	void deprecatedFree(ScratchBird::CheckStatusWrapper* status) override;

public:
	JRequest(Statement* handle, StableAttachmentPart* sa);

	StableAttachmentPart* getAttachment()
	{
		return sAtt;
	}

	Statement* getHandle() noexcept
	{
		return rq;
	}

private:
	Statement* rq;
	ScratchBird::RefPtr<StableAttachmentPart> sAtt;

	void freeEngineData(ScratchBird::CheckStatusWrapper* status);
};

class JEvents final : public ScratchBird::RefCntIface<ScratchBird::IEventsImpl<JEvents, ScratchBird::CheckStatusWrapper> >
{
public:
	// IEvents implementation
	int release() override;
	void cancel(ScratchBird::CheckStatusWrapper* status) override;
	void deprecatedCancel(ScratchBird::CheckStatusWrapper* status) override;

public:
	JEvents(int aId, StableAttachmentPart* sa, ScratchBird::IEventCallback* aCallback);

	JEvents* getHandle() noexcept
	{
		return this;
	}

	StableAttachmentPart* getAttachment()
	{
		return sAtt;
	}

private:
	int id;
	ScratchBird::RefPtr<StableAttachmentPart> sAtt;
	ScratchBird::RefPtr<ScratchBird::IEventCallback> callback;

	void freeEngineData(ScratchBird::CheckStatusWrapper* status);
};

class JAttachment final :
	public ScratchBird::RefCntIface<ScratchBird::IAttachmentImpl<JAttachment, ScratchBird::CheckStatusWrapper> >
{
public:
	// IAttachment implementation
	int release() override;
	void addRef() override;

	void getInfo(ScratchBird::CheckStatusWrapper* status,
		unsigned int itemsLength, const unsigned char* items,
		unsigned int bufferLength, unsigned char* buffer) override;
	JTransaction* startTransaction(ScratchBird::CheckStatusWrapper* status,
		unsigned int tpbLength, const unsigned char* tpb) override;
	JTransaction* reconnectTransaction(ScratchBird::CheckStatusWrapper* status,
		unsigned int length, const unsigned char* id) override;
	JRequest* compileRequest(ScratchBird::CheckStatusWrapper* status,
		unsigned int blr_length, const unsigned char* blr) override;
	void transactRequest(ScratchBird::CheckStatusWrapper* status, ScratchBird::ITransaction* transaction,
		unsigned int blr_length, const unsigned char* blr,
		unsigned int in_msg_length, const unsigned char* in_msg,
		unsigned int out_msg_length, unsigned char* out_msg) override;
	JBlob* createBlob(ScratchBird::CheckStatusWrapper* status, ScratchBird::ITransaction* transaction,
		ISC_QUAD* id, unsigned int bpbLength = 0, const unsigned char* bpb = 0) override;
	JBlob* openBlob(ScratchBird::CheckStatusWrapper* status, ScratchBird::ITransaction* transaction,
		ISC_QUAD* id, unsigned int bpbLength = 0, const unsigned char* bpb = 0) override;
	int getSlice(ScratchBird::CheckStatusWrapper* status, ScratchBird::ITransaction* transaction, ISC_QUAD* id,
		unsigned int sdl_length, const unsigned char* sdl,
		unsigned int param_length, const unsigned char* param,
		int sliceLength, unsigned char* slice) override;
	void putSlice(ScratchBird::CheckStatusWrapper* status, ScratchBird::ITransaction* transaction, ISC_QUAD* id,
		unsigned int sdl_length, const unsigned char* sdl,
		unsigned int param_length, const unsigned char* param,
		int sliceLength, unsigned char* slice) override;
	void executeDyn(ScratchBird::CheckStatusWrapper* status, ScratchBird::ITransaction* transaction,
		unsigned int length, const unsigned char* dyn) override;
	JStatement* prepare(ScratchBird::CheckStatusWrapper* status, ScratchBird::ITransaction* tra,
		unsigned int stmtLength, const char* sqlStmt, unsigned int dialect, unsigned int flags) override;
	ScratchBird::ITransaction* execute(ScratchBird::CheckStatusWrapper* status,
		ScratchBird::ITransaction* transaction, unsigned int stmtLength, const char* sqlStmt,
		unsigned int dialect, ScratchBird::IMessageMetadata* inMetadata, void* inBuffer,
		ScratchBird::IMessageMetadata* outMetadata, void* outBuffer) override;
	ScratchBird::IResultSet* openCursor(ScratchBird::CheckStatusWrapper* status,
		ScratchBird::ITransaction* transaction, unsigned int stmtLength, const char* sqlStmt,
		unsigned int dialect, ScratchBird::IMessageMetadata* inMetadata, void* inBuffer,
		ScratchBird::IMessageMetadata* outMetadata, const char* cursorName, unsigned int cursorFlags) override;
	JEvents* queEvents(ScratchBird::CheckStatusWrapper* status, ScratchBird::IEventCallback* callback,
		unsigned int length, const unsigned char* events) override;
	void cancelOperation(ScratchBird::CheckStatusWrapper* status, int option) override;
	void ping(ScratchBird::CheckStatusWrapper* status) override;
	void detach(ScratchBird::CheckStatusWrapper* status) override;
	void dropDatabase(ScratchBird::CheckStatusWrapper* status) override;
	void deprecatedDetach(ScratchBird::CheckStatusWrapper* status) override;
	void deprecatedDropDatabase(ScratchBird::CheckStatusWrapper* status) override;

	unsigned int getIdleTimeout(ScratchBird::CheckStatusWrapper* status) override;
	void setIdleTimeout(ScratchBird::CheckStatusWrapper* status, unsigned int timeOut) override;
	unsigned int getStatementTimeout(ScratchBird::CheckStatusWrapper* status) override;
	void setStatementTimeout(ScratchBird::CheckStatusWrapper* status, unsigned int timeOut) override;
	ScratchBird::IBatch* createBatch(ScratchBird::CheckStatusWrapper* status, ScratchBird::ITransaction* transaction,
		unsigned stmtLength, const char* sqlStmt, unsigned dialect,
		ScratchBird::IMessageMetadata* inMetadata, unsigned parLength, const unsigned char* par) override;
	ScratchBird::IReplicator* createReplicator(ScratchBird::CheckStatusWrapper* status) override;

	unsigned getMaxBlobCacheSize(ScratchBird::CheckStatusWrapper* status) override;
	void setMaxBlobCacheSize(ScratchBird::CheckStatusWrapper* status, unsigned size) override;
	unsigned getMaxInlineBlobSize(ScratchBird::CheckStatusWrapper* status) override;
	void setMaxInlineBlobSize(ScratchBird::CheckStatusWrapper* status, unsigned size) override;
public:
	explicit JAttachment(StableAttachmentPart* js);

	StableAttachmentPart* getStable() noexcept
	{
		return att;
	}

	Jrd::Attachment* getHandle() noexcept;
	const Jrd::Attachment* getHandle() const noexcept;

	StableAttachmentPart* getAttachment() noexcept
	{
		return att;
	}

	JTransaction* getTransactionInterface(ScratchBird::CheckStatusWrapper* status, ScratchBird::ITransaction* tra);
	jrd_tra* getEngineTransaction(ScratchBird::CheckStatusWrapper* status, ScratchBird::ITransaction* tra);

private:
	friend class StableAttachmentPart;

	StableAttachmentPart* att;

	void freeEngineData(ScratchBird::CheckStatusWrapper* status, bool forceFree);

	void detachEngine()
	{
		att = NULL;
	}

	void internalDetach(ScratchBird::CheckStatusWrapper* status);
	void internalDropDatabase(ScratchBird::CheckStatusWrapper* status);
};

class JService final :
	public ScratchBird::RefCntIface<ScratchBird::IServiceImpl<JService, ScratchBird::CheckStatusWrapper> >
{
public:
	// IService implementation
	int release() override;
	void detach(ScratchBird::CheckStatusWrapper* status) override;
	void deprecatedDetach(ScratchBird::CheckStatusWrapper* status) override;
	void query(ScratchBird::CheckStatusWrapper* status,
		unsigned int sendLength, const unsigned char* sendItems,
		unsigned int receiveLength, const unsigned char* receiveItems,
		unsigned int bufferLength, unsigned char* buffer) override;
	void start(ScratchBird::CheckStatusWrapper* status,
		unsigned int spbLength, const unsigned char* spb) override;
	void cancel(ScratchBird::CheckStatusWrapper* status) override;

public:
	explicit JService(Jrd::Service* handle);
	Jrd::Service* svc;

private:
	void freeEngineData(ScratchBird::CheckStatusWrapper* status);
};

class JProvider final :
	public ScratchBird::StdPlugin<ScratchBird::IProviderImpl<JProvider, ScratchBird::CheckStatusWrapper> >
{
public:
	explicit JProvider(ScratchBird::IPluginConfig* pConf)
		: cryptCallback(NULL), pluginConfig(pConf)
	{ }

	static JProvider* getInstance()
	{
		JProvider* p = FB_NEW JProvider(NULL);
		p->addRef();
		return p;
	}

	ScratchBird::ICryptKeyCallback* getCryptCallback()
	{
		return cryptCallback;
	}

	// IProvider implementation
	JAttachment* attachDatabase(ScratchBird::CheckStatusWrapper* status, const char* fileName,
		unsigned int dpbLength, const unsigned char* dpb);
	JAttachment* createDatabase(ScratchBird::CheckStatusWrapper* status, const char* fileName,
		unsigned int dpbLength, const unsigned char* dpb);
	JService* attachServiceManager(ScratchBird::CheckStatusWrapper* status, const char* service,
		unsigned int spbLength, const unsigned char* spb);
	void shutdown(ScratchBird::CheckStatusWrapper* status, unsigned int timeout, const int reason);
	void setDbCryptCallback(ScratchBird::CheckStatusWrapper* status,
		ScratchBird::ICryptKeyCallback* cryptCb);

private:
	JAttachment* internalAttach(ScratchBird::CheckStatusWrapper* status, const char* const fileName,
		unsigned int dpbLength, const unsigned char* dpb, const UserId* existingId);
	ScratchBird::ICryptKeyCallback* cryptCallback;
	ScratchBird::IPluginConfig* pluginConfig;
};

} // namespace Jrd

#endif // JRD_ENGINE_INTERFACE_H
