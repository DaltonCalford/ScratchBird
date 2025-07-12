/*
 * The contents of this file are subject to the Interbase Public
 * License Version 1.0 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy
 * of the License at http://www.Inprise.com/IPL.html
 *
 * Software distributed under the License is distributed on an
 * "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, either express
 * or implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code was created by Inprise Corporation
 * and its predecessors. Portions created by Inprise Corporation are
 * Copyright (C) Inprise Corporation.
 *
 * All Rights Reserved.
 * Contributor(s): ______________________________________.
 *
 * Dmitry Yemanov
 * Sean Leyne
 * Alex Peshkoff
 * Adriano dos Santos Fernandes
 *
 */

#ifndef YVALVE_Y_OBJECTS_H
#define YVALVE_Y_OBJECTS_H

#include "firebird.h"
#include "firebird/Interface.h"
#include "iberror.h"
#include "../common/StatusHolder.h"
#include "../common/classes/sb_atomic.h"
#include "../common/classes/alloc.h"
#include "../common/classes/array.h"
#include "../common/MsgMetadata.h"
#include "../common/classes/ClumpletWriter.h"

#include <functional>

namespace Why
{


class YAttachment;
class YBlob;
class YRequest;
class YResultSet;
class YService;
class YStatement;
class IscStatement;
class YTransaction;
class Dispatcher;

class YObject
{
public:
	YObject()
		: handle(0)
	{
	}

protected:
	FB_API_HANDLE handle;
};

class CleanupCallback
{
public:
	virtual void cleanupCallbackFunction() = 0;
	virtual ~CleanupCallback() { }
};

template <typename T>
class HandleArray
{
public:
	explicit HandleArray(ScratchBird::MemoryPool& pool)
		: array(pool)
	{
	}

	void add(T* obj)
	{
		ScratchBird::MutexLockGuard guard(mtx, FB_FUNCTION);

		array.add(obj);
	}

	void remove(T* obj)
	{
		ScratchBird::MutexLockGuard guard(mtx, FB_FUNCTION);
		FB_SIZE_T pos;

		if (array.find(obj, pos))
			array.remove(pos);
	}

	void destroy(unsigned dstrFlags)
	{
		ScratchBird::MutexLockGuard guard(mtx, FB_FUNCTION);

		// Call destroy() only once even if handle is not removed from array
		// by this call for any reason
		for (int i = array.getCount() - 1; i >= 0; i--)
			array[i]->destroy(dstrFlags);

		clear();
	}

	void assign(HandleArray& from)
	{
		clear();
		array.assign(from.array);
	}

	void clear()
	{
		array.clear();
	}

private:
	ScratchBird::Mutex mtx;
	ScratchBird::SortedArray<T*> array;
};

template <typename Impl, typename Intf>
class YHelper : public ScratchBird::RefCntIface<Intf>, public YObject
{
public:
	typedef typename Intf::Declaration NextInterface;
	typedef YAttachment YRef;

	static const unsigned DF_RELEASE =		0x1;
	static const unsigned DF_KEEP_NEXT =	0x2;

	explicit YHelper(NextInterface* aNext, const char* m = NULL)
		:
#ifdef DEV_BUILD
		  ScratchBird::RefCntIface<Intf>(m),
#endif
		  next(ScratchBird::REF_NO_INCR, aNext)
	{ }

	int release() override
	{
		int rc = --this->refCounter;
		this->refCntDPrt('-');
		if (rc == 0)
		{
			if (next)
				destroy(0);
			delete this;
		}

		return rc;
	}

	virtual void destroy(unsigned dstrFlags) = 0;

	void destroy2(unsigned dstrFlags)
	{
		if (dstrFlags & DF_KEEP_NEXT)
			next.clear();
		else
			next = NULL;

		if (dstrFlags & DF_RELEASE)
		{
			this->release();
		}
	}

	ScratchBird::RefPtr<NextInterface> next;
};

template <class YT>
class AtomicYPtr
{
public:
	AtomicYPtr(YT* v)
	{
		atmPtr.store(v, std::memory_order_relaxed);
	}

	YT* get()
	{
		return atmPtr.load(std::memory_order_relaxed);
	}

	YT* release()
	{
		YT* v = atmPtr;
		if (v && atmPtr.compare_exchange_strong(v, nullptr))
			return v;
		return nullptr;
	}

private:
	std::atomic<YT*> atmPtr;
};

typedef AtomicYPtr<YAttachment> AtomicAttPtr;
typedef AtomicYPtr<YTransaction> AtomicTraPtr;

class YEvents final :
	public YHelper<YEvents, ScratchBird::IEventsImpl<YEvents, ScratchBird::CheckStatusWrapper> >
{
public:
	static const ISC_STATUS ERROR_CODE = isc_bad_events_handle;

	YEvents(YAttachment* aAttachment, ScratchBird::IEvents* aNext, ScratchBird::IEventCallback* aCallback);

	void destroy(unsigned dstrFlags) override;
	FB_API_HANDLE& getHandle();

	// IEvents implementation
	void cancel(ScratchBird::CheckStatusWrapper* status) override;
	void deprecatedCancel(ScratchBird::CheckStatusWrapper* status) override;

public:
	AtomicAttPtr attachment;
	ScratchBird::RefPtr<ScratchBird::IEventCallback> callback;

private:
	ScratchBird::AtomicCounter destroyed;
};

class YRequest final :
	public YHelper<YRequest, ScratchBird::IRequestImpl<YRequest, ScratchBird::CheckStatusWrapper> >
{
public:
	static const ISC_STATUS ERROR_CODE = isc_bad_req_handle;

	YRequest(YAttachment* aAttachment, ScratchBird::IRequest* aNext);

	void destroy(unsigned dstrFlags) override;
	isc_req_handle& getHandle();

	// IRequest implementation
	void receive(ScratchBird::CheckStatusWrapper* status, int level, unsigned int msgType,
		unsigned int length, void* message) override;
	void send(ScratchBird::CheckStatusWrapper* status, int level, unsigned int msgType,
		unsigned int length, const void* message) override;
	void getInfo(ScratchBird::CheckStatusWrapper* status, int level, unsigned int itemsLength,
		const unsigned char* items, unsigned int bufferLength, unsigned char* buffer) override;
	void start(ScratchBird::CheckStatusWrapper* status, ScratchBird::ITransaction* transaction, int level) override;
	void startAndSend(ScratchBird::CheckStatusWrapper* status, ScratchBird::ITransaction* transaction, int level,
		unsigned int msgType, unsigned int length, const void* message) override;
	void unwind(ScratchBird::CheckStatusWrapper* status, int level) override;
	void free(ScratchBird::CheckStatusWrapper* status) override;
	void deprecatedFree(ScratchBird::CheckStatusWrapper* status) override;

public:
	AtomicAttPtr attachment;
	isc_req_handle* userHandle;
};

class YTransaction final :
	public YHelper<YTransaction, ScratchBird::ITransactionImpl<YTransaction, ScratchBird::CheckStatusWrapper> >
{
public:
	static const ISC_STATUS ERROR_CODE = isc_bad_trans_handle;

	YTransaction(YAttachment* aAttachment, ScratchBird::ITransaction* aNext);

	void destroy(unsigned dstrFlags) override;
	isc_tr_handle& getHandle();

	// ITransaction implementation
	void getInfo(ScratchBird::CheckStatusWrapper* status, unsigned int itemsLength,
		const unsigned char* items, unsigned int bufferLength, unsigned char* buffer) override;
	void prepare(ScratchBird::CheckStatusWrapper* status, unsigned int msgLength,
		const unsigned char* message) override;
	void commit(ScratchBird::CheckStatusWrapper* status) override;
	void commitRetaining(ScratchBird::CheckStatusWrapper* status) override;
	void rollback(ScratchBird::CheckStatusWrapper* status) override;
	void rollbackRetaining(ScratchBird::CheckStatusWrapper* status) override;
	void disconnect(ScratchBird::CheckStatusWrapper* status) override;
	ScratchBird::ITransaction* join(ScratchBird::CheckStatusWrapper* status, ScratchBird::ITransaction* transaction) override;
	ScratchBird::ITransaction* validate(ScratchBird::CheckStatusWrapper* status, ScratchBird::IAttachment* testAtt) override;
	YTransaction* enterDtc(ScratchBird::CheckStatusWrapper* status) override;
	void deprecatedCommit(ScratchBird::CheckStatusWrapper* status) override;
	void deprecatedRollback(ScratchBird::CheckStatusWrapper* status) override;
	void deprecatedDisconnect(ScratchBird::CheckStatusWrapper* status) override;

	void addCleanupHandler(ScratchBird::CheckStatusWrapper* status, CleanupCallback* callback);
	void selfCheck();

public:
	AtomicAttPtr attachment;
	HandleArray<YBlob> childBlobs;
	HandleArray<YResultSet> childCursors;
	ScratchBird::Array<CleanupCallback*> cleanupHandlers;

private:
	YTransaction(YTransaction* from)
		: YHelper(from->next),
		  attachment(from->attachment.get()),
		  childBlobs(getPool()),
		  childCursors(getPool()),
		  cleanupHandlers(getPool())
	{
		childBlobs.assign(from->childBlobs);
		from->childBlobs.clear();
		childCursors.assign(from->childCursors);
		from->childCursors.clear();
		cleanupHandlers.assign(from->cleanupHandlers);
		from->cleanupHandlers.clear();
	}
};

typedef ScratchBird::RefPtr<ScratchBird::ITransaction> NextTransaction;

class YBlob final :
	public YHelper<YBlob, ScratchBird::IBlobImpl<YBlob, ScratchBird::CheckStatusWrapper> >
{
public:
	static const ISC_STATUS ERROR_CODE = isc_bad_segstr_handle;

	YBlob(YAttachment* aAttachment, YTransaction* aTransaction, ScratchBird::IBlob* aNext);

	void destroy(unsigned dstrFlags) override;
	isc_blob_handle& getHandle();

	// IBlob implementation
	void getInfo(ScratchBird::CheckStatusWrapper* status, unsigned int itemsLength,
		const unsigned char* items, unsigned int bufferLength, unsigned char* buffer) override;
	int getSegment(ScratchBird::CheckStatusWrapper* status, unsigned int length, void* buffer,
								   unsigned int* segmentLength) override;
	void putSegment(ScratchBird::CheckStatusWrapper* status, unsigned int length, const void* buffer) override;
	void cancel(ScratchBird::CheckStatusWrapper* status) override;
	void close(ScratchBird::CheckStatusWrapper* status) override;
	int seek(ScratchBird::CheckStatusWrapper* status, int mode, int offset) override;
	void deprecatedCancel(ScratchBird::CheckStatusWrapper* status) override;
	void deprecatedClose(ScratchBird::CheckStatusWrapper* status) override;

public:
	AtomicAttPtr attachment;
	AtomicTraPtr transaction;
};

class YResultSet final :
	public YHelper<YResultSet, ScratchBird::IResultSetImpl<YResultSet, ScratchBird::CheckStatusWrapper> >
{
public:
	static const ISC_STATUS ERROR_CODE = isc_bad_result_set;

	YResultSet(YAttachment* anAttachment, YTransaction* aTransaction, ScratchBird::IResultSet* aNext);
	YResultSet(YAttachment* anAttachment, YTransaction* aTransaction, YStatement* aStatement,
		ScratchBird::IResultSet* aNext);

	void destroy(unsigned dstrFlags) override;

	// IResultSet implementation
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
	AtomicAttPtr attachment;
	AtomicTraPtr transaction;
	YStatement* statement;
};

class YBatch final :
	public YHelper<YBatch, ScratchBird::IBatchImpl<YBatch, ScratchBird::CheckStatusWrapper> >
{
public:
	static const ISC_STATUS ERROR_CODE = isc_bad_result_set;	// isc_bad_batch

	YBatch(YAttachment* anAttachment, ScratchBird::IBatch* aNext);

	void destroy(unsigned dstrFlags) override;

	// IBatch implementation
	void add(ScratchBird::CheckStatusWrapper* status, unsigned count, const void* inBuffer) override;
	void addBlob(ScratchBird::CheckStatusWrapper* status, unsigned length, const void* inBuffer, ISC_QUAD* blobId,
		unsigned parLength, const unsigned char* par) override;
	void appendBlobData(ScratchBird::CheckStatusWrapper* status, unsigned length, const void* inBuffer) override;
	void addBlobStream(ScratchBird::CheckStatusWrapper* status, unsigned length, const void* inBuffer) override;
	unsigned getBlobAlignment(ScratchBird::CheckStatusWrapper* status) override;
	ScratchBird::IMessageMetadata* getMetadata(ScratchBird::CheckStatusWrapper* status) override;
	void registerBlob(ScratchBird::CheckStatusWrapper* status, const ISC_QUAD* existingBlob, ISC_QUAD* blobId) override;
	ScratchBird::IBatchCompletionState* execute(ScratchBird::CheckStatusWrapper* status, ScratchBird::ITransaction* transaction) override;
	void cancel(ScratchBird::CheckStatusWrapper* status) override;
	void setDefaultBpb(ScratchBird::CheckStatusWrapper* status, unsigned parLength, const unsigned char* par) override;
	void close(ScratchBird::CheckStatusWrapper* status) override;
	void deprecatedClose(ScratchBird::CheckStatusWrapper* status) override;
	void getInfo(ScratchBird::CheckStatusWrapper* status, unsigned int itemsLength, const unsigned char* items,
		unsigned int bufferLength, unsigned char* buffer) override;

public:
	AtomicAttPtr attachment;
};


class YReplicator final :
	public YHelper<YReplicator, ScratchBird::IReplicatorImpl<YReplicator, ScratchBird::CheckStatusWrapper> >
{
public:
	static const ISC_STATUS ERROR_CODE = isc_bad_repl_handle;

	YReplicator(YAttachment* anAttachment, ScratchBird::IReplicator* aNext);

	void destroy(unsigned dstrFlags) override;

	// IReplicator implementation
	void process(ScratchBird::CheckStatusWrapper* status, unsigned length, const unsigned char* data) override;
	void close(ScratchBird::CheckStatusWrapper* status) override;
	void deprecatedClose(ScratchBird::CheckStatusWrapper* status) override;

public:
	AtomicAttPtr attachment;
};


class YMetadata
{
public:
	explicit YMetadata(bool in)
		: flag(false), input(in)
	{ }

	ScratchBird::IMessageMetadata* get(ScratchBird::IStatement* next, YStatement* statement);

private:
	ScratchBird::RefPtr<ScratchBird::MsgMetadata> metadata;
	volatile bool flag;
	bool input;
};

class YStatement final :
	public YHelper<YStatement, ScratchBird::IStatementImpl<YStatement, ScratchBird::CheckStatusWrapper> >
{
public:
	static const ISC_STATUS ERROR_CODE = isc_bad_stmt_handle;

	YStatement(YAttachment* aAttachment, ScratchBird::IStatement* aNext);

	void destroy(unsigned dstrFlags) override;

	// IStatement implementation
	void getInfo(ScratchBird::CheckStatusWrapper* status,
		unsigned int itemsLength, const unsigned char* items,
		unsigned int bufferLength, unsigned char* buffer) override;
	unsigned getType(ScratchBird::CheckStatusWrapper* status) override;
	const char* getPlan(ScratchBird::CheckStatusWrapper* status, FB_BOOLEAN detailed) override;
	ISC_UINT64 getAffectedRecords(ScratchBird::CheckStatusWrapper* status) override;
	ScratchBird::IMessageMetadata* getInputMetadata(ScratchBird::CheckStatusWrapper* status) override;
	ScratchBird::IMessageMetadata* getOutputMetadata(ScratchBird::CheckStatusWrapper* status) override;
	ScratchBird::ITransaction* execute(ScratchBird::CheckStatusWrapper* status, ScratchBird::ITransaction* transaction,
		ScratchBird::IMessageMetadata* inMetadata, void* inBuffer,
		ScratchBird::IMessageMetadata* outMetadata, void* outBuffer) override;
	ScratchBird::IResultSet* openCursor(ScratchBird::CheckStatusWrapper* status, ScratchBird::ITransaction* transaction,
		ScratchBird::IMessageMetadata* inMetadata, void* inBuffer, ScratchBird::IMessageMetadata* outMetadata,
		unsigned int flags) override;
	void setCursorName(ScratchBird::CheckStatusWrapper* status, const char* name) override;
	void free(ScratchBird::CheckStatusWrapper* status) override;
	void deprecatedFree(ScratchBird::CheckStatusWrapper* status) override;
	unsigned getFlags(ScratchBird::CheckStatusWrapper* status) override;

	unsigned int getTimeout(ScratchBird::CheckStatusWrapper* status) override;
	void setTimeout(ScratchBird::CheckStatusWrapper* status, unsigned int timeOut) override;
	YBatch* createBatch(ScratchBird::CheckStatusWrapper* status, ScratchBird::IMessageMetadata* inMetadata,
		unsigned parLength, const unsigned char* par) override;

	unsigned getMaxInlineBlobSize(ScratchBird::CheckStatusWrapper* status) override;
	void setMaxInlineBlobSize(ScratchBird::CheckStatusWrapper* status, unsigned size) override;

public:
	AtomicAttPtr attachment;
	ScratchBird::Mutex statementMutex;
	YResultSet* cursor;

	ScratchBird::IMessageMetadata* getMetadata(bool in, ScratchBird::IStatement* next);

private:
	YMetadata input, output;
};

class EnterCount
{
public:
	EnterCount()
		: enterCount(0)
	{}

	~EnterCount()
	{
		fb_assert(enterCount == 0);
	}

	int enterCount;
	ScratchBird::Mutex enterMutex;
};

class YAttachment final :
	public YHelper<YAttachment, ScratchBird::IAttachmentImpl<YAttachment, ScratchBird::CheckStatusWrapper> >,
	public EnterCount
{
public:
	static const ISC_STATUS ERROR_CODE = isc_bad_db_handle;

	YAttachment(ScratchBird::IProvider* aProvider, ScratchBird::IAttachment* aNext,
		const ScratchBird::PathName& aDbPath);
	~YAttachment();

	void destroy(unsigned dstrFlags) override;
	void shutdown();
	isc_db_handle& getHandle();
	void getOdsVersion(USHORT* majorVersion, USHORT* minorVersion);

	// IAttachment implementation
	void getInfo(ScratchBird::CheckStatusWrapper* status, unsigned int itemsLength,
		const unsigned char* items, unsigned int bufferLength, unsigned char* buffer) override;
	YTransaction* startTransaction(ScratchBird::CheckStatusWrapper* status, unsigned int tpbLength,
		const unsigned char* tpb) override;
	YTransaction* reconnectTransaction(ScratchBird::CheckStatusWrapper* status, unsigned int length,
		const unsigned char* id) override;
	YRequest* compileRequest(ScratchBird::CheckStatusWrapper* status, unsigned int blrLength,
		const unsigned char* blr) override;
	void transactRequest(ScratchBird::CheckStatusWrapper* status, ScratchBird::ITransaction* transaction,
		unsigned int blrLength, const unsigned char* blr, unsigned int inMsgLength,
		const unsigned char* inMsg, unsigned int outMsgLength, unsigned char* outMsg) override;
	YBlob* createBlob(ScratchBird::CheckStatusWrapper* status, ScratchBird::ITransaction* transaction, ISC_QUAD* id,
		unsigned int bpbLength, const unsigned char* bpb) override;
	YBlob* openBlob(ScratchBird::CheckStatusWrapper* status, ScratchBird::ITransaction* transaction, ISC_QUAD* id,
		unsigned int bpbLength, const unsigned char* bpb) override;
	int getSlice(ScratchBird::CheckStatusWrapper* status, ScratchBird::ITransaction* transaction, ISC_QUAD* id,
		unsigned int sdlLength, const unsigned char* sdl, unsigned int paramLength,
		const unsigned char* param, int sliceLength, unsigned char* slice) override;
	void putSlice(ScratchBird::CheckStatusWrapper* status, ScratchBird::ITransaction* transaction, ISC_QUAD* id,
		unsigned int sdlLength, const unsigned char* sdl, unsigned int paramLength,
		const unsigned char* param, int sliceLength, unsigned char* slice) override;
	void executeDyn(ScratchBird::CheckStatusWrapper* status, ScratchBird::ITransaction* transaction, unsigned int length,
		const unsigned char* dyn) override;
	YStatement* prepare(ScratchBird::CheckStatusWrapper* status, ScratchBird::ITransaction* tra,
		unsigned int stmtLength, const char* sqlStmt, unsigned int dialect, unsigned int flags) override;
	ScratchBird::ITransaction* execute(ScratchBird::CheckStatusWrapper* status, ScratchBird::ITransaction* transaction,
		unsigned int stmtLength, const char* sqlStmt, unsigned int dialect,
		ScratchBird::IMessageMetadata* inMetadata, void* inBuffer,
		ScratchBird::IMessageMetadata* outMetadata, void* outBuffer) override;
	ScratchBird::IResultSet* openCursor(ScratchBird::CheckStatusWrapper* status, ScratchBird::ITransaction* transaction,
		unsigned int stmtLength, const char* sqlStmt, unsigned int dialect,
		ScratchBird::IMessageMetadata* inMetadata, void* inBuffer, ScratchBird::IMessageMetadata* outMetadata,
		const char* cursorName, unsigned int cursorFlags) override;
	YEvents* queEvents(ScratchBird::CheckStatusWrapper* status, ScratchBird::IEventCallback* callback,
		unsigned int length, const unsigned char* eventsData) override;
	void cancelOperation(ScratchBird::CheckStatusWrapper* status, int option) override;
	void ping(ScratchBird::CheckStatusWrapper* status) override;
	void detach(ScratchBird::CheckStatusWrapper* status) override;
	void dropDatabase(ScratchBird::CheckStatusWrapper* status) override;
	void deprecatedDetach(ScratchBird::CheckStatusWrapper* status) override;
	void deprecatedDropDatabase(ScratchBird::CheckStatusWrapper* status) override;

	void addCleanupHandler(ScratchBird::CheckStatusWrapper* status, CleanupCallback* callback);
	YTransaction* getTransaction(ScratchBird::ITransaction* tra);
	void getNextTransaction(ScratchBird::CheckStatusWrapper* status, ScratchBird::ITransaction* tra, NextTransaction& next);
	void execute(ScratchBird::CheckStatusWrapper* status, isc_tr_handle* traHandle,
		unsigned int stmtLength, const char* sqlStmt, unsigned int dialect,
		ScratchBird::IMessageMetadata* inMetadata, void* inBuffer,
		ScratchBird::IMessageMetadata* outMetadata, void* outBuffer);

	unsigned int getIdleTimeout(ScratchBird::CheckStatusWrapper* status) override;
	void setIdleTimeout(ScratchBird::CheckStatusWrapper* status, unsigned int timeOut) override;
	unsigned int getStatementTimeout(ScratchBird::CheckStatusWrapper* status) override;
	void setStatementTimeout(ScratchBird::CheckStatusWrapper* status, unsigned int timeOut) override;
	YBatch* createBatch(ScratchBird::CheckStatusWrapper* status, ScratchBird::ITransaction* transaction,
		unsigned stmtLength, const char* sqlStmt, unsigned dialect,
		ScratchBird::IMessageMetadata* inMetadata, unsigned parLength, const unsigned char* par) override;
	YReplicator* createReplicator(ScratchBird::CheckStatusWrapper* status) override;

	unsigned getMaxBlobCacheSize(ScratchBird::CheckStatusWrapper* status) override;
	void setMaxBlobCacheSize(ScratchBird::CheckStatusWrapper* status, unsigned size) override;
	unsigned getMaxInlineBlobSize(ScratchBird::CheckStatusWrapper* status) override;
	void setMaxInlineBlobSize(ScratchBird::CheckStatusWrapper* status, unsigned size) override;

public:
	ScratchBird::IProvider* provider;
	ScratchBird::PathName dbPath;
	HandleArray<YBlob> childBlobs;
	HandleArray<YEvents> childEvents;
	HandleArray<YRequest> childRequests;
	HandleArray<YStatement> childStatements;
	HandleArray<IscStatement> childIscStatements;
	HandleArray<YTransaction> childTransactions;
	ScratchBird::Array<CleanupCallback*> cleanupHandlers;
	ScratchBird::StatusHolder savedStatus;	// Do not use raise() method of this class in yValve.

private:
	USHORT cachedOdsMajorVersion = 0;
	USHORT cachedOdsMinorVersion = 0;
};

class YService final :
	public YHelper<YService, ScratchBird::IServiceImpl<YService, ScratchBird::CheckStatusWrapper> >,
	public EnterCount
{
public:
	static const ISC_STATUS ERROR_CODE = isc_bad_svc_handle;

	YService(ScratchBird::IProvider* aProvider, ScratchBird::IService* aNext, bool utf8, Dispatcher* yProvider);
	~YService();

	void shutdown();
	void destroy(unsigned dstrFlags) override;
	isc_svc_handle& getHandle();

	// IService implementation
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
	typedef ScratchBird::IService NextInterface;
	typedef YService YRef;

private:
	ScratchBird::IProvider* provider;
	bool utf8Connection;		// Client talks to us using UTF8, else - system default charset

public:
	ScratchBird::RefPtr<IService> alternativeHandle;
	ScratchBird::ClumpletWriter attachSpb;
	ScratchBird::RefPtr<Dispatcher> ownProvider;
};

class Dispatcher final :
	public ScratchBird::StdPlugin<ScratchBird::IProviderImpl<Dispatcher, ScratchBird::CheckStatusWrapper> >
{
public:
	Dispatcher()
		: cryptCallback(NULL)
	{ }

	// IProvider implementation
	YAttachment* attachDatabase(ScratchBird::CheckStatusWrapper* status, const char* filename,
		unsigned int dpbLength, const unsigned char* dpb) override;
	YAttachment* createDatabase(ScratchBird::CheckStatusWrapper* status, const char* filename,
		unsigned int dpbLength, const unsigned char* dpb) override;
	YService* attachServiceManager(ScratchBird::CheckStatusWrapper* status, const char* serviceName,
		unsigned int spbLength, const unsigned char* spb) override;
	void shutdown(ScratchBird::CheckStatusWrapper* status, unsigned int timeout, const int reason) override;
	void setDbCryptCallback(ScratchBird::CheckStatusWrapper* status,
		ScratchBird::ICryptKeyCallback* cryptCallback) override;

	void destroy(unsigned)
	{ }

public:
	ScratchBird::IService* internalServiceAttach(ScratchBird::CheckStatusWrapper* status,
		const ScratchBird::PathName& svcName, ScratchBird::ClumpletReader& spb,
		std::function<void(ScratchBird::CheckStatusWrapper*, ScratchBird::IService*)> start,
		ScratchBird::IProvider** retProvider);

private:
	YAttachment* attachOrCreateDatabase(ScratchBird::CheckStatusWrapper* status, bool createFlag,
		const char* filename, unsigned int dpbLength, const unsigned char* dpb);

	ScratchBird::ICryptKeyCallback* cryptCallback;
};

class UtilInterface final :
	public ScratchBird::AutoIface<ScratchBird::IUtilImpl<UtilInterface, ScratchBird::CheckStatusWrapper> >
{
	// IUtil implementation
public:
	void getFbVersion(ScratchBird::CheckStatusWrapper* status, ScratchBird::IAttachment* att,
		ScratchBird::IVersionCallback* callback) override;
	void loadBlob(ScratchBird::CheckStatusWrapper* status, ISC_QUAD* blobId, ScratchBird::IAttachment* att,
		ScratchBird::ITransaction* tra, const char* file, FB_BOOLEAN txt) override;
	void dumpBlob(ScratchBird::CheckStatusWrapper* status, ISC_QUAD* blobId, ScratchBird::IAttachment* att,
		ScratchBird::ITransaction* tra, const char* file, FB_BOOLEAN txt) override;
	void getPerfCounters(ScratchBird::CheckStatusWrapper* status, ScratchBird::IAttachment* att,
		const char* countersSet, ISC_INT64* counters) override;			// in perf.cpp

	YAttachment* executeCreateDatabase(ScratchBird::CheckStatusWrapper* status,
		unsigned stmtLength, const char* creatDBstatement, unsigned dialect,
		FB_BOOLEAN* stmtIsCreateDb = nullptr) override
	{
		return executeCreateDatabase2(status, stmtLength, creatDBstatement, dialect,
			0, nullptr, stmtIsCreateDb);
	}

	YAttachment* executeCreateDatabase2(ScratchBird::CheckStatusWrapper* status,
		unsigned stmtLength, const char* creatDBstatement, unsigned dialect,
		unsigned dpbLength, const unsigned char* dpb,
		FB_BOOLEAN* stmtIsCreateDb = nullptr) override;

	void decodeDate(ISC_DATE date, unsigned* year, unsigned* month, unsigned* day) override;
	void decodeTime(ISC_TIME time,
		unsigned* hours, unsigned* minutes, unsigned* seconds, unsigned* fractions) override;
	ISC_DATE encodeDate(unsigned year, unsigned month, unsigned day) override;
	ISC_TIME encodeTime(unsigned hours, unsigned minutes, unsigned seconds, unsigned fractions) override;
	unsigned formatStatus(char* buffer, unsigned bufferSize, ScratchBird::IStatus* status) override;
	unsigned getClientVersion() override;
	ScratchBird::IXpbBuilder* getXpbBuilder(ScratchBird::CheckStatusWrapper* status,
		unsigned kind, const unsigned char* buf, unsigned len) override;
	unsigned setOffsets(ScratchBird::CheckStatusWrapper* status, ScratchBird::IMessageMetadata* metadata,
		ScratchBird::IOffsetsCallback* callback) override;
	ScratchBird::IDecFloat16* getDecFloat16(ScratchBird::CheckStatusWrapper* status) override;
	ScratchBird::IDecFloat34* getDecFloat34(ScratchBird::CheckStatusWrapper* status) override;
	void decodeTimeTz(ScratchBird::CheckStatusWrapper* status, const ISC_TIME_TZ* timeTz,
		unsigned* hours, unsigned* minutes, unsigned* seconds, unsigned* fractions,
		unsigned timeZoneBufferLength, char* timeZoneBuffer) override;
	void decodeTimeStampTz(ScratchBird::CheckStatusWrapper* status, const ISC_TIMESTAMP_TZ* timeStampTz,
		unsigned* year, unsigned* month, unsigned* day, unsigned* hours, unsigned* minutes, unsigned* seconds,
		unsigned* fractions, unsigned timeZoneBufferLength, char* timeZoneBuffer) override;
	void encodeTimeTz(ScratchBird::CheckStatusWrapper* status, ISC_TIME_TZ* timeTz,
		unsigned hours, unsigned minutes, unsigned seconds, unsigned fractions, const char* timeZone) override;
	void encodeTimeStampTz(ScratchBird::CheckStatusWrapper* status, ISC_TIMESTAMP_TZ* timeStampTz,
		unsigned year, unsigned month, unsigned day,
		unsigned hours, unsigned minutes, unsigned seconds, unsigned fractions, const char* timeZone) override;
	ScratchBird::IInt128* getInt128(ScratchBird::CheckStatusWrapper* status) override;
	void decodeTimeTzEx(ScratchBird::CheckStatusWrapper* status, const ISC_TIME_TZ_EX* timeEx,
		unsigned* hours, unsigned* minutes, unsigned* seconds, unsigned* fractions,
		unsigned timeZoneBufferLength, char* timeZoneBuffer) override;
	void decodeTimeStampTzEx(ScratchBird::CheckStatusWrapper* status, const ISC_TIMESTAMP_TZ_EX* timeStampEx,
		unsigned* year, unsigned* month, unsigned* day, unsigned* hours, unsigned* minutes, unsigned* seconds,
		unsigned* fractions, unsigned timeZoneBufferLength, char* timeZoneBuffer) override;
};

}	// namespace Why

#endif	// YVALVE_Y_OBJECTS_H
