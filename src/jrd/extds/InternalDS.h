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
 *  The Original Code was created by Vlad Khorsun
 *  for the ScratchBird Open Source RDBMS project.
 *
 *  Copyright (c) 2008 Vlad Khorsun <hvlad@users.sourceforge.net>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#ifndef EXTDS_INTERNAL_H
#define EXTDS_INTERNAL_H


#include "ExtDS.h"

namespace EDS {

class InternalProvider : public Provider
{
public:
	explicit InternalProvider(const char* prvName) :
		Provider(prvName)
	{
	}

	~InternalProvider()
	{}

	virtual void jrdAttachmentEnd(Jrd::thread_db* tdbb, Jrd::Attachment* att, bool forced);

	virtual void initialize() {}
	virtual void getRemoteError(const Jrd::FbStatusVector* status, ScratchBird::string& err) const;

protected:
	virtual Connection* doCreateConnection();
};


class InternalConnection : public Connection
{
protected:
	friend class InternalProvider;

	explicit InternalConnection(InternalProvider& prov) :
	  Connection(prov),
	  m_attachment(0)
	{}

	virtual ~InternalConnection();

public:
	void attach(Jrd::thread_db* tdbb) override;

	bool cancelExecution(bool forced) override;
	bool resetSession(Jrd::thread_db* tdbb) override;

	bool isAvailable(Jrd::thread_db* tdbb, TraScope traScope) const override;

	bool isConnected() const override { return (m_attachment != 0); }
	bool validate(Jrd::thread_db* tdbb) override;

	bool isSameDatabase(const ScratchBird::PathName& dbName,
		ScratchBird::ClumpletReader& dpb, const CryptHash& ch) const override;

	bool isCurrent() const override { return m_dpb.isEmpty(); }

	Jrd::JAttachment* getJrdAtt() { return m_attachment; }

	Blob* createBlob() override;

protected:
	Transaction* doCreateTransaction() override;
	Statement* doCreateStatement() override;
	void doDetach(Jrd::thread_db* tdbb) override;

	ScratchBird::AutoPlugin<Jrd::JProvider> m_provider;
	ScratchBird::RefPtr<Jrd::JAttachment> m_attachment;
};


class InternalTransaction : public Transaction
{
protected:
	friend class InternalConnection;

	explicit InternalTransaction(InternalConnection& conn) :
	  Transaction(conn),
	  m_IntConnection(conn),
	  m_transaction(0)
	{}

	virtual ~InternalTransaction() {}

public:
	Jrd::JTransaction* getJrdTran() { return m_transaction; }

protected:
	virtual void doStart(Jrd::FbStatusVector* status, Jrd::thread_db* tdbb, ScratchBird::ClumpletWriter& tpb);
	virtual void doPrepare(Jrd::FbStatusVector* status, Jrd::thread_db* tdbb, int info_len, const char* info);
	virtual void doCommit(Jrd::FbStatusVector* status, Jrd::thread_db* tdbb, bool retain);
	virtual void doRollback(Jrd::FbStatusVector* status, Jrd::thread_db* tdbb, bool retain);

	InternalConnection& m_IntConnection;
	ScratchBird::RefPtr<Jrd::JTransaction> m_transaction;
};


class InternalStatement : public Statement
{
protected:
	friend class InternalConnection;

	explicit InternalStatement(InternalConnection& conn);
	~InternalStatement();

protected:
	virtual void doPrepare(Jrd::thread_db* tdbb, const ScratchBird::string& sql);
	virtual void doSetTimeout(Jrd::thread_db* tdbb, unsigned int timeout);
	virtual void doExecute(Jrd::thread_db* tdbb);
	virtual void doOpen(Jrd::thread_db* tdbb);
	virtual bool doFetch(Jrd::thread_db* tdbb);
	virtual void doClose(Jrd::thread_db* tdbb, bool drop);

	virtual void putExtBlob(Jrd::thread_db* tdbb, dsc& src, dsc& dst);
	virtual void getExtBlob(Jrd::thread_db* tdbb, const dsc& src, dsc& dst);

	InternalTransaction* getIntTransaction()
	{
		return (InternalTransaction*) m_transaction;
	}

	InternalConnection& m_intConnection;
	InternalTransaction* m_intTransaction;

	ScratchBird::RefPtr<Jrd::JStatement> m_request;
	ScratchBird::RefPtr<Jrd::JResultSet> m_cursor;
	ScratchBird::RefPtr<ScratchBird::MsgMetadata> m_inMetadata, m_outMetadata;
};


class InternalBlob : public Blob
{
	friend class InternalConnection;
protected:
	explicit InternalBlob(InternalConnection& conn);

public:
	~InternalBlob();

public:
	virtual void open(Jrd::thread_db* tdbb, Transaction& tran, const dsc& desc, const ScratchBird::UCharBuffer* bpb);
	virtual void create(Jrd::thread_db* tdbb, Transaction& tran, dsc& desc, const ScratchBird::UCharBuffer* bpb);
	virtual USHORT read(Jrd::thread_db* tdbb, UCHAR* buff, USHORT len);
	virtual void write(Jrd::thread_db* tdbb, const UCHAR* buff, USHORT len);
	virtual void close(Jrd::thread_db* tdbb);
	virtual void cancel(Jrd::thread_db* tdbb);

private:
	InternalConnection& m_connection;
	ScratchBird::RefPtr<Jrd::JBlob> m_blob;
	ISC_QUAD m_blob_id;
};

} // namespace EDS

#endif // EXTDS_INTERNAL_H
