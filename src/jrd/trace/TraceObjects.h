/*
 *	PROGRAM:	ScratchBird Trace Services
 *	MODULE:		TraceObjects.h
 *	DESCRIPTION:	Trace API manager support
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
 *  The Original Code was created by Khorsun Vladyslav
 *  for the ScratchBird Open Source RDBMS project.
 *
 *  Copyright (c) 2009 Khorsun Vladyslav <hvlad@users.sourceforge.net>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 *
 */

#ifndef JRD_TRACE_OBJECTS_H
#define JRD_TRACE_OBJECTS_H

#include <time.h>
#include "../../common/classes/array.h"
#include "../../common/classes/fb_string.h"
#include "../../dsql/dsql.h"
#include "../../jrd/ntrace.h"
#include "../../common/dsc.h"
#include "../../common/isc_s_proto.h"
#include "../../jrd/ods_proto.h"
#include "../../jrd/req.h"
#include "../../jrd/svc.h"
#include "../../jrd/tra.h"
#include "../../jrd/status.h"
#include "../../jrd/Function.h"
#include "../../jrd/RuntimeStatistics.h"
#include "../../jrd/trace/TraceSession.h"
#include "../../common/classes/ImplementHelper.h"
#include "../../common/prett_proto.h"

//// TODO: DDL triggers, packages and external procedures and functions support
namespace Jrd {

class Database;
class Attachment;
class jrd_tra;


class StatementHolder
{
public:
	explicit StatementHolder(const Statement* statement)
		: m_statement(statement)
	{}

	explicit StatementHolder(const Request* request)
		: m_statement(request ? request->getStatement() : nullptr)
	{}

	SINT64 getId() const
	{
		return m_statement ? m_statement->getStatementId() : 0;
	}

	ScratchBird::string getName() const
	{
		if (m_statement)
		{
			if (m_statement->procedure)
				return m_statement->procedure->getName().toQuotedString();

			if (m_statement->function)
				return m_statement->function->getName().toQuotedString();

			if (m_statement->triggerName.object.hasData())
				return m_statement->triggerName.toQuotedString();
		}

		return "";
	}

	const char* ensurePlan(bool explained);

private:
	const Statement* const m_statement;
	ScratchBird::string m_plan;
	bool m_planExplained = false;
};


class TraceConnectionImpl :
	public ScratchBird::AutoIface<ScratchBird::ITraceDatabaseConnectionImpl<TraceConnectionImpl, ScratchBird::CheckStatusWrapper> >
{
public:
	TraceConnectionImpl(const Attachment* att) :
		m_att(att)
	{}

	// TraceConnection implementation
	unsigned getKind();
	int getProcessID();
	const char* getUserName();
	const char* getRoleName();
	const char* getCharSet();
	const char* getRemoteProtocol();
	const char* getRemoteAddress();
	int getRemoteProcessID();
	const char* getRemoteProcessName();

	// TraceDatabaseConnection implementation
	ISC_INT64 getConnectionID();
	const char* getDatabaseName();
private:
	const Attachment* const m_att;
};


class TraceTransactionImpl :
	public ScratchBird::AutoIface<ScratchBird::ITraceTransactionImpl<TraceTransactionImpl, ScratchBird::CheckStatusWrapper> >
{
public:
	TraceTransactionImpl(const jrd_tra* tran, ScratchBird::PerformanceInfo* perf = NULL, ISC_INT64 prevID = 0) :
		m_tran(tran),
		m_perf(perf),
		m_prevID(prevID)
	{}

	// TraceTransaction implementation
	ISC_INT64 getTransactionID();
	FB_BOOLEAN getReadOnly();
	int getWait();
	unsigned getIsolation();
	ScratchBird::PerformanceInfo* getPerf()	{ return m_perf; }
	ISC_INT64 getInitialID();
	ISC_INT64 getPreviousID()	{ return m_prevID; }

private:
	const jrd_tra* const m_tran;
	ScratchBird::PerformanceInfo* const m_perf;
	const ISC_INT64 m_prevID;
};


template <class Final>
class BLRPrinter :
	public ScratchBird::AutoIface<ScratchBird::ITraceBLRStatementImpl<Final, ScratchBird::CheckStatusWrapper> >
{
public:
	BLRPrinter(const unsigned char* blr, unsigned length) :
		m_blr(blr),
		m_length(length),
		m_text(*getDefaultMemoryPool())
	{}

	// TraceBLRStatement implementation
	const unsigned char* getData()	{ return m_blr; }
	unsigned getDataLength()		{ return m_length; }
	const char* getText()
	{
		if (m_text.empty() && getDataLength())
			fb_print_blr(getData(), (ULONG) getDataLength(), print_blr, this, 0);
		return m_text.c_str();
	}

private:
	static void print_blr(void* arg, SSHORT offset, const char* line)
	{
		BLRPrinter* blr = (BLRPrinter*) arg;

		ScratchBird::string temp;
		temp.printf("%4d %s\n", offset, line);
		blr->m_text.append(temp);
	}

	const unsigned char* const m_blr;
	const unsigned m_length;
	ScratchBird::string m_text;
};


class TraceBLRStatementImpl : public BLRPrinter<TraceBLRStatementImpl>
{
public:
	TraceBLRStatementImpl(const Statement* stmt, ScratchBird::PerformanceInfo* perf) :
		BLRPrinter(stmt->blr.begin(), stmt->blr.getCount()),
		m_stmt(stmt),
		m_perf(perf)
	{}

	ISC_INT64 getStmtID()		{ return m_stmt->getStatementId(); }
	ScratchBird::PerformanceInfo* getPerf()	{ return m_perf; }

private:
	const Statement* const m_stmt;
	ScratchBird::PerformanceInfo* const m_perf;
};


class TraceFailedBLRStatement : public BLRPrinter<TraceFailedBLRStatement>
{
public:
	TraceFailedBLRStatement(const unsigned char* blr, unsigned length) :
		BLRPrinter(blr, length)
	{}

	ISC_INT64 getStmtID()		{ return 0; }
	ScratchBird::PerformanceInfo* getPerf()	{ return NULL; }
};


class TraceSQLStatementImpl :
	public ScratchBird::AutoIface<ScratchBird::ITraceSQLStatementImpl<TraceSQLStatementImpl, ScratchBird::CheckStatusWrapper> >,
	public StatementHolder
{
public:
	TraceSQLStatementImpl(DsqlRequest* stmt, ScratchBird::PerformanceInfo* perf, const UCHAR* inputBuffer) :
		StatementHolder(stmt ? stmt->getStatement() : nullptr),
		m_stmt(stmt),
		m_perf(perf),
		m_inputs(stmt, inputBuffer)
	{}

	// TraceSQLStatement implementation
	ISC_INT64 getStmtID();
	ScratchBird::PerformanceInfo* getPerf();
	ScratchBird::ITraceParams* getInputs();
	const char* getText();
	const char* getTextUTF8();

	const char* getPlan()
	{
		return ensurePlan(false);
	}

	const char* getExplainedPlan()
	{
		return ensurePlan(true);
	}

private:
	class DSQLParamsImpl :
		public ScratchBird::AutoIface<ScratchBird::ITraceParamsImpl<DSQLParamsImpl, ScratchBird::CheckStatusWrapper> >
	{
	public:
		explicit DSQLParamsImpl(DsqlRequest* const stmt, const UCHAR* const inputBuffer) :
			m_stmt(stmt), m_buffer(inputBuffer)
		{
		}

		FB_SIZE_T getCount();
		const paramdsc* getParam(FB_SIZE_T idx);
		const char* getTextUTF8(ScratchBird::CheckStatusWrapper* status, FB_SIZE_T idx);

	private:
		void fillParams();

		DsqlRequest* const m_stmt;
		const UCHAR* m_buffer;
		ScratchBird::HalfStaticArray<paramdsc, 16> m_descs;
		ScratchBird::string m_tempUTF8;
	};

	DsqlRequest* const m_stmt;
	ScratchBird::PerformanceInfo* const m_perf;
	DSQLParamsImpl m_inputs;
	ScratchBird::string m_textUTF8;
};


class TraceFailedSQLStatement :
	public ScratchBird::AutoIface<ScratchBird::ITraceSQLStatementImpl<TraceFailedSQLStatement, ScratchBird::CheckStatusWrapper> >
{
public:
	TraceFailedSQLStatement(ScratchBird::string& text) :
		m_text(text)
	{}

	// TraceSQLStatement implementation
	ISC_INT64 getStmtID()		{ return 0; }
	ScratchBird::PerformanceInfo* getPerf()	{ return NULL; }
	ScratchBird::ITraceParams* getInputs()	{ return NULL; }
	const char* getText()		{ return m_text.c_str(); }
	const char* getPlan()		{ return ""; }
	const char* getTextUTF8();
	const char* getExplainedPlan()	{ return ""; }

private:
	ScratchBird::string& m_text;
	ScratchBird::string m_textUTF8;
};


class TraceContextVarImpl :
	public ScratchBird::AutoIface<ScratchBird::ITraceContextVariableImpl<TraceContextVarImpl, ScratchBird::CheckStatusWrapper> >
{
public:
	TraceContextVarImpl(const char* ns, const char* name, const char* value) :
		m_namespace(ns),
		m_name(name),
		m_value(value)
	{}

	// TraceContextVariable implementation
	const char* getNameSpace()	{ return m_namespace; }
	const char* getVarName()	{ return m_name; }
	const char* getVarValue()	{ return m_value; }

private:
	const char* const m_namespace;
	const char* const m_name;
	const char* const m_value;
};


// forward declaration
class TraceDescriptors;

class TraceParamsImpl :
	public ScratchBird::AutoIface<ScratchBird::ITraceParamsImpl<TraceParamsImpl, ScratchBird::CheckStatusWrapper> >
{
public:
	explicit TraceParamsImpl(TraceDescriptors *descs) :
		m_descs(descs)
	{}

	// TraceParams implementation
	FB_SIZE_T getCount();
	const paramdsc* getParam(FB_SIZE_T idx);
	const char* getTextUTF8(ScratchBird::CheckStatusWrapper* status, FB_SIZE_T idx);

private:
	TraceDescriptors* m_descs;
	ScratchBird::string m_tempUTF8;
};


class TraceDescriptors
{
public:
	TraceDescriptors() :
		m_traceParams(this)
	{
	}

	FB_SIZE_T getCount()
	{
		fillParams();
		return m_descs.getCount();
	}

	const paramdsc* getParam(FB_SIZE_T idx)
	{
		fillParams();

		if (idx < m_descs.getCount())
			return &m_descs[idx];

		return NULL;
	}

	operator ScratchBird::ITraceParams* ()
	{
		return &m_traceParams;
	}

protected:
	virtual void fillParams() = 0;

	ScratchBird::HalfStaticArray<paramdsc, 16> m_descs;

private:
	TraceParamsImpl	m_traceParams;
};


class TraceDscFromValues : public TraceDescriptors
{
public:
	TraceDscFromValues(Request* request, const ValueListNode* params) :
		m_request(request),
		m_params(params)
	{}

protected:
	void fillParams();

private:
	Request* const m_request;
	const ValueListNode* const m_params;
};


class TraceDscFromMsg : public TraceDescriptors
{
public:
	TraceDscFromMsg(const Format* format, const UCHAR* inMsg, ULONG inMsgLength) :
		m_format(format),
		m_inMsg(inMsg),
		m_inMsgLength(inMsgLength)
	{}

protected:
	void fillParams();

private:
	const Format* const m_format;
	const UCHAR* const m_inMsg;
	const ULONG m_inMsgLength;
};


class TraceDscFromDsc : public TraceDescriptors
{
public:
	TraceDscFromDsc(const dsc* desc)
	{
		if (desc)
			m_descs.add(*desc);
		else
		{
			m_descs.grow(1);
			m_descs[0].dsc_flags |= DSC_null;
		}
	}

protected:
	void fillParams() {}
};


class TraceProcedureImpl :
	public ScratchBird::AutoIface<ScratchBird::ITraceProcedureImpl<TraceProcedureImpl, ScratchBird::CheckStatusWrapper> >,
	public StatementHolder
{
public:
	TraceProcedureImpl(const ScratchBird::string& name, const Statement* statement) :
		StatementHolder(statement),
		m_name(name),
		m_perf(nullptr),
		m_inputs(nullptr, nullptr)
	{}

	TraceProcedureImpl(Request* request, ScratchBird::PerformanceInfo* perf) :
		StatementHolder(request),
		m_name(getName()),
		m_perf(perf),
		m_inputs(request->req_proc_caller, request->req_proc_inputs)
	{}

	// TraceProcedure implementation
	const char* getProcName()
	{
		return m_name.c_str();
	}

	ScratchBird::ITraceParams* getInputs()
	{
		return m_inputs;
	}

	ScratchBird::PerformanceInfo* getPerf()
	{
		return m_perf;
	};

	ISC_INT64 getStmtID()
	{
		return getId();
	}

	const char* getPlan()
	{
		return ensurePlan(false);
	}

	const char* getExplainedPlan()
	{
		return ensurePlan(true);
	}

private:
	const ScratchBird::string m_name;
	ScratchBird::PerformanceInfo* const m_perf;
	TraceDscFromValues m_inputs;
};


class TraceFunctionImpl :
	public ScratchBird::AutoIface<ScratchBird::ITraceFunctionImpl<TraceFunctionImpl, ScratchBird::CheckStatusWrapper> >,
	public StatementHolder
{
public:
	TraceFunctionImpl(const ScratchBird::string& name, const Statement* statement) :
		StatementHolder(statement),
		m_name(name),
		m_perf(nullptr),
		m_inputs(nullptr),
		m_value(nullptr)
	{}

	TraceFunctionImpl(Request* request, ScratchBird::PerformanceInfo* perf,
					  ScratchBird::ITraceParams* inputs, const dsc* value) :
		StatementHolder(request),
		m_name(getName()),
		m_perf(perf),
		m_inputs(inputs),
		m_value(value)
	{}

	// TraceFunction implementation
	const char* getFuncName()
	{
		return m_name.c_str();
	}

	ScratchBird::ITraceParams* getInputs()
	{
		return m_inputs;
	}

	ScratchBird::ITraceParams* getResult()
	{
		return m_value;
	}

	ScratchBird::PerformanceInfo* getPerf()
	{
		return m_perf;
	};

	ISC_INT64 getStmtID()
	{
		return getId();
	}

	const char* getPlan()
	{
		return ensurePlan(false);
	}

	const char* getExplainedPlan()
	{
		return ensurePlan(true);
	}

private:
	ScratchBird::string m_name;
	ScratchBird::PerformanceInfo* const m_perf;
	ScratchBird::ITraceParams* const m_inputs;
	TraceDscFromDsc m_value;
};


class TraceTriggerImpl :
	public ScratchBird::AutoIface<ScratchBird::ITraceTriggerImpl<TraceTriggerImpl, ScratchBird::CheckStatusWrapper> >,
	public StatementHolder
{
public:
	TraceTriggerImpl(const ScratchBird::string& name, const ScratchBird::string& relationName,
		int which, int action, const Statement* statement) :
		StatementHolder(statement),
		m_name(name),
		m_relationName(relationName),
		m_which(which),
		m_action(action),
		m_perf(nullptr)
	{}

	TraceTriggerImpl(int which, const Request* request, ScratchBird::PerformanceInfo* perf) :
		StatementHolder(request),
		m_name(getName()),
		m_relationName((request->req_rpb.hasData() && request->req_rpb[0].rpb_relation) ?
			request->req_rpb[0].rpb_relation->rel_name.toQuotedString() : ""),
		m_which(which),
		m_action(request->req_trigger_action),
		m_perf(perf)
	{}

	// TraceTrigger implementation
	const char* getTriggerName()
	{
		return m_name.nullStr();
	}

	const char* getRelationName()
	{
		return m_relationName.nullStr();
	}

	int getWhich()
	{
		return m_which;
	}

	int getAction()
	{
		return m_action;
	}

	ScratchBird::PerformanceInfo* getPerf()
	{
		return m_perf;
	}

	ISC_INT64 getStmtID()
	{
		return getId();
	}

	const char* getPlan()
	{
		return ensurePlan(false);
	}

	const char* getExplainedPlan()
	{
		return ensurePlan(true);
	}

private:
	const ScratchBird::string m_name;
	const ScratchBird::string m_relationName;
	const int m_which;
	const int m_action;
	ScratchBird::PerformanceInfo* const m_perf;
};


class TraceServiceImpl :
	public ScratchBird::AutoIface<ScratchBird::ITraceServiceConnectionImpl<TraceServiceImpl, ScratchBird::CheckStatusWrapper> >
{
public:
	TraceServiceImpl(const Service* svc) :
		m_svc(svc)
	{}

	// TraceConnection implementation
	unsigned getKind();
	const char* getUserName();
	const char* getRoleName();
	const char* getCharSet();
	int getProcessID();
	const char* getRemoteProtocol();
	const char* getRemoteAddress();
	int getRemoteProcessID();
	const char* getRemoteProcessName();

	// TraceServiceConnection implementation
	void* getServiceID();
	const char* getServiceMgr();
	const char* getServiceName();
private:
	const Service* const m_svc;
};


class TraceRuntimeStats
{
public:
	TraceRuntimeStats(Attachment* att, RuntimeStatistics* baseline, RuntimeStatistics* stats,
		SINT64 clock, SINT64 records_fetched);

	ScratchBird::PerformanceInfo* getPerf()	{ return &m_info; }

private:
	ScratchBird::PerformanceInfo m_info;
	TraceCountsArray m_counts;
	ScratchBird::ObjectsArray<ScratchBird::string> m_tempNames;
	static SINT64 m_dummy_counts[RuntimeStatistics::TOTAL_ITEMS];	// Zero-initialized array with zero counts
};


class TraceInitInfoImpl :
	public ScratchBird::AutoIface<ScratchBird::ITraceInitInfoImpl<TraceInitInfoImpl, ScratchBird::CheckStatusWrapper> >
{
public:
	TraceInitInfoImpl(const ScratchBird::TraceSession& session, const Attachment* att,
					const char* filename) :
		m_session(session),
		m_trace_conn(att),
		m_filename(filename),
		m_attachment(att)
	{
		if (m_attachment && !m_attachment->att_filename.empty()) {
			m_filename = m_attachment->att_filename.c_str();
		}
	}

	// TraceInitInfo implementation
	const char* getConfigText()			{ return m_session.ses_config.c_str(); }
	int getTraceSessionID()				{ return m_session.ses_id; }
	const char* getTraceSessionName()	{ return m_session.ses_name.c_str(); }

	const char* getScratchBirdRootDirectory();
	const char* getDatabaseName()		{ return m_filename; }

	ScratchBird::ITraceDatabaseConnection* getConnection()
	{
		if (m_attachment)
			return &m_trace_conn;

		return NULL;
	}

	ScratchBird::ITraceLogWriter* getLogWriter();

private:
	const ScratchBird::TraceSession& m_session;
	ScratchBird::RefPtr<ScratchBird::ITraceLogWriter> m_logWriter;
	TraceConnectionImpl m_trace_conn;
	const char* m_filename;
	const Attachment* const m_attachment;
};


class TraceStatusVectorImpl :
	public ScratchBird::AutoIface<ScratchBird::ITraceStatusVectorImpl<TraceStatusVectorImpl, ScratchBird::CheckStatusWrapper> >
{
public:
	enum Kind {TS_ERRORS, TS_WARNINGS};

	TraceStatusVectorImpl(FbStatusVector* status, Kind k) :
		m_status(status), kind(k)
	{
	}

	FB_BOOLEAN hasError()
	{
		return m_status->getState() & ScratchBird::IStatus::STATE_ERRORS;
	}

	FB_BOOLEAN hasWarning()
	{
		return m_status->getState() & ScratchBird::IStatus::STATE_WARNINGS;
	}

	ScratchBird::IStatus* getStatus()
	{
		return m_status;
	}

	const char* getText();

private:
	ScratchBird::string m_error;
	FbStatusVector* m_status;
	Kind kind;
};

class TraceSweepImpl :
	public ScratchBird::AutoIface<ScratchBird::ITraceSweepInfoImpl<TraceSweepImpl, ScratchBird::CheckStatusWrapper> >
{
public:
	TraceSweepImpl()
	{
		m_oit = 0;
		m_ost = 0;
		m_oat = 0;
		m_next = 0;
		m_perf = 0;
	}

	void update(const Ods::header_page* header)
	{
		m_oit = header->hdr_oldest_transaction;
		m_ost = header->hdr_oldest_snapshot;
		m_oat = header->hdr_oldest_active;
		m_next = header->hdr_next_transaction;
	}

	void setPerf(ScratchBird::PerformanceInfo* perf)
	{
		m_perf = perf;
	}

	ISC_INT64 getOIT()			{ return m_oit; };
	ISC_INT64 getOST()			{ return m_ost; };
	ISC_INT64 getOAT()			{ return m_oat; };
	ISC_INT64 getNext()			{ return m_next; };
	ScratchBird::PerformanceInfo* getPerf()	{ return m_perf; };

private:
	TraNumber m_oit;
	TraNumber m_ost;
	TraNumber m_oat;
	TraNumber m_next;
	ScratchBird::PerformanceInfo* m_perf;
};

} // namespace Jrd

#endif // JRD_TRACE_OBJECTS_H
