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
 *  Copyright (c) 2010 Adriano dos Santos Fernandes <adrianosf@gmail.com>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#ifndef DSQL_AGG_NODES_H
#define DSQL_AGG_NODES_H

#include "firebird/impl/blr.h"
#include "../dsql/Nodes.h"
#include "../dsql/NodePrinter.h"

namespace Jrd {


class AnyValueAggNode final : public AggNode
{
public:
	explicit AnyValueAggNode(MemoryPool& pool, ValueExprNode* aArg = nullptr);

	static DmlNode* parse(thread_db* tdbb, MemoryPool& pool, CompilerScratch* csb, const UCHAR blrOp);

	unsigned getCapabilities() const override
	{
		return CAP_RESPECTS_WINDOW_FRAME | CAP_WANTS_AGG_CALLS;
	}

	void parseArgs(thread_db* tdbb, CompilerScratch* csb, unsigned count) override;

	ScratchBird::string internalPrint(NodePrinter& printer) const override;
	void make(DsqlCompilerScratch* dsqlScratch, dsc* desc) override;
	void getDesc(thread_db* tdbb, CompilerScratch* csb, dsc* desc) override;
	ValueExprNode* copy(thread_db* tdbb, NodeCopier& copier) const override;

	void aggInit(thread_db* tdbb, Request* request) const override;
	void aggPass(thread_db* tdbb, Request* request, dsc* desc) const override;
	dsc* aggExecute(thread_db* tdbb, Request* request) const override;

protected:
	AggNode* dsqlCopy(DsqlCompilerScratch* dsqlScratch) /*const*/ override;
};

class AvgAggNode final : public AggNode
{
public:
	explicit AvgAggNode(MemoryPool& pool, bool aDistinct, bool aDialect1, ValueExprNode* aArg = NULL);

	static DmlNode* parse(thread_db* tdbb, MemoryPool& pool, CompilerScratch* csb, const UCHAR blrOp);

	virtual const char* getCompatDialectVerb()
	{
		return "avg";
	}

	virtual unsigned getCapabilities() const
	{
		return CAP_RESPECTS_WINDOW_FRAME | CAP_WANTS_AGG_CALLS;
	}

	virtual ScratchBird::string internalPrint(NodePrinter& printer) const;
	virtual void make(DsqlCompilerScratch* dsqlScratch, dsc* desc);
	virtual void getDesc(thread_db* tdbb, CompilerScratch* csb, dsc* desc);
	virtual ValueExprNode* copy(thread_db* tdbb, NodeCopier& copier) const;
	virtual AggNode* pass2(thread_db* tdbb, CompilerScratch* csb);

	virtual void aggInit(thread_db* tdbb, Request* request) const;
	virtual void aggPass(thread_db* tdbb, Request* request, dsc* desc) const;
	virtual dsc* aggExecute(thread_db* tdbb, Request* request) const;

protected:
	virtual AggNode* dsqlCopy(DsqlCompilerScratch* dsqlScratch) /*const*/;

private:
	void outputDesc(dsc* desc) const;
	ULONG tempImpure;
};

class ListAggNode final : public AggNode
{
public:
	explicit ListAggNode(MemoryPool& pool, bool aDistinct, ValueExprNode* aArg = NULL,
		ValueExprNode* aDelimiter = NULL);

	static DmlNode* parse(thread_db* tdbb, MemoryPool& pool, CompilerScratch* csb, const UCHAR blrOp);

	virtual unsigned getCapabilities() const
	{
		return CAP_WANTS_AGG_CALLS;
	}

	virtual void getChildren(NodeRefsHolder& holder, bool dsql) const
	{
		AggNode::getChildren(holder, dsql);
		holder.add(delimiter);
	}

	virtual ScratchBird::string internalPrint(NodePrinter& printer) const;
	virtual void make(DsqlCompilerScratch* dsqlScratch, dsc* desc);
	virtual bool setParameterType(DsqlCompilerScratch* dsqlScratch,
		std::function<void (dsc*)> makeDesc, bool forceVarChar);
	virtual void getDesc(thread_db* tdbb, CompilerScratch* csb, dsc* desc);
	virtual ValueExprNode* copy(thread_db* tdbb, NodeCopier& copier) const;

	virtual void aggInit(thread_db* tdbb, Request* request) const;
	virtual void aggPass(thread_db* tdbb, Request* request, dsc* desc) const;
	virtual dsc* aggExecute(thread_db* tdbb, Request* request) const;

protected:
	virtual AggNode* dsqlCopy(DsqlCompilerScratch* dsqlScratch) /*const*/;

private:
	NestConst<ValueExprNode> delimiter;
};

class CountAggNode final : public AggNode
{
public:
	explicit CountAggNode(MemoryPool& pool, bool aDistinct, bool aDialect1, ValueExprNode* aArg = NULL);

	static DmlNode* parse(thread_db* tdbb, MemoryPool& pool, CompilerScratch* csb, const UCHAR blrOp);

	virtual unsigned getCapabilities() const
	{
		return CAP_RESPECTS_WINDOW_FRAME | CAP_WANTS_AGG_CALLS;
	}

	virtual ScratchBird::string internalPrint(NodePrinter& printer) const;
	virtual void make(DsqlCompilerScratch* dsqlScratch, dsc* desc);
	virtual void genBlr(DsqlCompilerScratch* dsqlScratch);
	virtual void getDesc(thread_db* tdbb, CompilerScratch* csb, dsc* desc);
	virtual ValueExprNode* copy(thread_db* tdbb, NodeCopier& copier) const;

	virtual void aggInit(thread_db* tdbb, Request* request) const;
	virtual void aggPass(thread_db* tdbb, Request* request, dsc* desc) const;
	virtual dsc* aggExecute(thread_db* tdbb, Request* request) const;

protected:
	virtual AggNode* dsqlCopy(DsqlCompilerScratch* dsqlScratch) /*const*/;
};

class SumAggNode final : public AggNode
{
public:
	explicit SumAggNode(MemoryPool& pool, bool aDistinct, bool aDialect1, ValueExprNode* aArg = NULL);

	static DmlNode* parse(thread_db* tdbb, MemoryPool& pool, CompilerScratch* csb, const UCHAR blrOp);

	virtual const char* getCompatDialectVerb()
	{
		return "sum";
	}

	virtual unsigned getCapabilities() const
	{
		return CAP_RESPECTS_WINDOW_FRAME | CAP_WANTS_AGG_CALLS;
	}

	virtual ScratchBird::string internalPrint(NodePrinter& printer) const;
	virtual void make(DsqlCompilerScratch* dsqlScratch, dsc* desc);
	virtual void getDesc(thread_db* tdbb, CompilerScratch* csb, dsc* desc);
	virtual ValueExprNode* copy(thread_db* tdbb, NodeCopier& copier) const;

	virtual void aggInit(thread_db* tdbb, Request* request) const;
	virtual void aggPass(thread_db* tdbb, Request* request, dsc* desc) const;
	virtual dsc* aggExecute(thread_db* tdbb, Request* request) const;

protected:
	virtual AggNode* dsqlCopy(DsqlCompilerScratch* dsqlScratch) /*const*/;
};

class MaxMinAggNode final : public AggNode
{
public:
	enum MaxMinType
	{
		TYPE_MAX,
		TYPE_MIN
	};

	explicit MaxMinAggNode(MemoryPool& pool, MaxMinType aType, ValueExprNode* aArg = NULL);

	static DmlNode* parse(thread_db* tdbb, MemoryPool& pool, CompilerScratch* csb, const UCHAR blrOp);

	virtual unsigned getCapabilities() const
	{
		return CAP_RESPECTS_WINDOW_FRAME | CAP_WANTS_AGG_CALLS;
	}

	virtual ScratchBird::string internalPrint(NodePrinter& printer) const;
	virtual void make(DsqlCompilerScratch* dsqlScratch, dsc* desc);
	virtual void getDesc(thread_db* tdbb, CompilerScratch* csb, dsc* desc);
	virtual ValueExprNode* copy(thread_db* tdbb, NodeCopier& copier) const;

	virtual void aggInit(thread_db* tdbb, Request* request) const;
	virtual void aggPass(thread_db* tdbb, Request* request, dsc* desc) const;
	virtual dsc* aggExecute(thread_db* tdbb, Request* request) const;

protected:
	virtual AggNode* dsqlCopy(DsqlCompilerScratch* dsqlScratch) /*const*/;

public:
	const MaxMinType type;
};

class StdDevAggNode final : public AggNode
{
public:
	enum StdDevType
	{
		TYPE_STDDEV_SAMP,
		TYPE_STDDEV_POP,
		TYPE_VAR_SAMP,
		TYPE_VAR_POP
	};

	union StdDevImpure
	{
		struct
		{
			double x, x2;
		} dbl;
		struct
		{
			ScratchBird::Decimal128 x, x2;
		} dec;
	};

	explicit StdDevAggNode(MemoryPool& pool, StdDevType aType, ValueExprNode* aArg = NULL);

	virtual unsigned getCapabilities() const
	{
		return CAP_RESPECTS_WINDOW_FRAME | CAP_WANTS_AGG_CALLS;
	}

	virtual void parseArgs(thread_db* tdbb, CompilerScratch* csb, unsigned count);

	virtual ScratchBird::string internalPrint(NodePrinter& printer) const;
	virtual void make(DsqlCompilerScratch* dsqlScratch, dsc* desc);
	virtual void getDesc(thread_db* tdbb, CompilerScratch* csb, dsc* desc);
	virtual ValueExprNode* copy(thread_db* tdbb, NodeCopier& copier) const;
	virtual AggNode* pass2(thread_db* tdbb, CompilerScratch* csb);

	virtual void aggInit(thread_db* tdbb, Request* request) const;
	virtual void aggPass(thread_db* tdbb, Request* request, dsc* desc) const;
	virtual dsc* aggExecute(thread_db* tdbb, Request* request) const;

protected:
	virtual AggNode* dsqlCopy(DsqlCompilerScratch* dsqlScratch) /*const*/;

public:
	const StdDevType type;

private:
	ULONG impure2Offset;
};

class CorrAggNode final : public AggNode
{
public:
	enum CorrType
	{
		TYPE_COVAR_SAMP,
		TYPE_COVAR_POP,
		TYPE_CORR
	};

	union CorrImpure
	{
		struct
		{
			double x, x2, y, y2, xy;
		} dbl;
		struct
		{
			ScratchBird::Decimal128 x, x2, y, y2, xy;
		} dec;
	};

	explicit CorrAggNode(MemoryPool& pool, CorrType aType,
		ValueExprNode* aArg = NULL, ValueExprNode* aArg2 = NULL);

	virtual unsigned getCapabilities() const
	{
		return CAP_RESPECTS_WINDOW_FRAME | CAP_WANTS_AGG_CALLS;
	}

	virtual void parseArgs(thread_db* tdbb, CompilerScratch* csb, unsigned count);

	virtual void getChildren(NodeRefsHolder& holder, bool dsql) const
	{
		AggNode::getChildren(holder, dsql);
		holder.add(arg2);
	}

	virtual ScratchBird::string internalPrint(NodePrinter& printer) const;
	virtual void make(DsqlCompilerScratch* dsqlScratch, dsc* desc);
	virtual void getDesc(thread_db* tdbb, CompilerScratch* csb, dsc* desc);
	virtual ValueExprNode* copy(thread_db* tdbb, NodeCopier& copier) const;
	virtual AggNode* pass2(thread_db* tdbb, CompilerScratch* csb);

	virtual void aggInit(thread_db* tdbb, Request* request) const;
	virtual bool aggPass(thread_db* tdbb, Request* request) const;
	virtual void aggPass(thread_db* tdbb, Request* request, dsc* desc) const;
	virtual dsc* aggExecute(thread_db* tdbb, Request* request) const;

protected:
	virtual AggNode* dsqlCopy(DsqlCompilerScratch* dsqlScratch) /*const*/;

public:
	const CorrType type;
	NestConst<ValueExprNode> arg2;

private:
	ULONG impure2Offset;
};

class RegrAggNode final : public AggNode
{
public:
	enum RegrType
	{
		TYPE_REGR_AVGX,
		TYPE_REGR_AVGY,
		TYPE_REGR_INTERCEPT,
		TYPE_REGR_R2,
		TYPE_REGR_SLOPE,
		TYPE_REGR_SXX,
		TYPE_REGR_SXY,
		TYPE_REGR_SYY
	};

	union RegrImpure
	{
		struct
		{
			double x, x2, y, y2, xy;
		} dbl;
		struct
		{
			ScratchBird::Decimal128 x, x2, y, y2, xy;
		} dec;
	};

	explicit RegrAggNode(MemoryPool& pool, RegrType aType,
		ValueExprNode* aArg = NULL, ValueExprNode* aArg2 = NULL);

	virtual unsigned getCapabilities() const
	{
		return CAP_RESPECTS_WINDOW_FRAME | CAP_WANTS_AGG_CALLS;
	}

	virtual void parseArgs(thread_db* tdbb, CompilerScratch* csb, unsigned count);

	virtual void getChildren(NodeRefsHolder& holder, bool dsql) const
	{
		AggNode::getChildren(holder, dsql);
		holder.add(arg2);
	}

	virtual ScratchBird::string internalPrint(NodePrinter& printer) const;
	virtual void make(DsqlCompilerScratch* dsqlScratch, dsc* desc);
	virtual void getDesc(thread_db* tdbb, CompilerScratch* csb, dsc* desc);
	virtual ValueExprNode* copy(thread_db* tdbb, NodeCopier& copier) const;
	virtual AggNode* pass2(thread_db* tdbb, CompilerScratch* csb);

	virtual void aggInit(thread_db* tdbb, Request* request) const;
	virtual bool aggPass(thread_db* tdbb, Request* request) const;
	virtual void aggPass(thread_db* tdbb, Request* request, dsc* desc) const;
	virtual dsc* aggExecute(thread_db* tdbb, Request* request) const;

protected:
	virtual AggNode* dsqlCopy(DsqlCompilerScratch* dsqlScratch) /*const*/;

public:
	const RegrType type;
	NestConst<ValueExprNode> arg2;

private:
	ULONG impure2Offset;
};

class RegrCountAggNode final : public AggNode
{
public:
	explicit RegrCountAggNode(MemoryPool& pool,
		ValueExprNode* aArg = NULL, ValueExprNode* aArg2 = NULL);

	virtual unsigned getCapabilities() const
	{
		return CAP_RESPECTS_WINDOW_FRAME | CAP_WANTS_AGG_CALLS;
	}

	virtual void parseArgs(thread_db* tdbb, CompilerScratch* csb, unsigned count);

	virtual void getChildren(NodeRefsHolder& holder, bool dsql) const
	{
		AggNode::getChildren(holder, dsql);
		holder.add(arg2);
	}

	virtual ScratchBird::string internalPrint(NodePrinter& printer) const;
	virtual void make(DsqlCompilerScratch* dsqlScratch, dsc* desc);
	virtual void getDesc(thread_db* tdbb, CompilerScratch* csb, dsc* desc);
	virtual ValueExprNode* copy(thread_db* tdbb, NodeCopier& copier) const;

	virtual void aggInit(thread_db* tdbb, Request* request) const;
	virtual bool aggPass(thread_db* tdbb, Request* request) const;
	virtual void aggPass(thread_db* tdbb, Request* request, dsc* desc) const;
	virtual dsc* aggExecute(thread_db* tdbb, Request* request) const;

protected:
	virtual AggNode* dsqlCopy(DsqlCompilerScratch* dsqlScratch) /*const*/;

public:
	NestConst<ValueExprNode> arg2;
};

} // namespace

#endif // DSQL_AGG_NODES_H
