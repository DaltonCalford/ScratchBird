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

#include "../jrd/ResultSet.h"
#include "../jrd/PreparedStatement.h"
#include "../jrd/align.h"
#include "../jrd/jrd.h"
#include "../jrd/req.h"
#include "../dsql/dsql.h"
#include "firebird/impl/sqlda_pub.h"
#include "../dsql/dsql_proto.h"
#include "../jrd/mov_proto.h"

using namespace ScratchBird;

namespace Jrd {


ResultSet::ResultSet(thread_db* tdbb, PreparedStatement* aStmt, jrd_tra* aTransaction)
	: stmt(aStmt),
	  transaction(aTransaction),
	  firstFetchDone(false)
{
	stmt->open(tdbb, transaction);
	stmt->resultSet = this;
}


ResultSet::~ResultSet()
{
	if (!stmt)
		return;

	thread_db* tdbb = JRD_get_thread_data();

	stmt->resultSet = NULL;

	if (stmt->dsqlRequest->getDsqlStatement()->getType() != DsqlStatement::TYPE_EXEC_PROCEDURE)
		DSQL_free_statement(tdbb, stmt->dsqlRequest, DSQL_close);
}


bool ResultSet::fetch(thread_db* tdbb)
{
	if (stmt->dsqlRequest->getDsqlStatement()->getType() == DsqlStatement::TYPE_EXEC_PROCEDURE &&
		firstFetchDone)
	{
		return false;
	}

	memset(stmt->outMessage.begin(), 0, stmt->outMessage.getCount());

	if (!stmt->dsqlRequest->fetch(tdbb, stmt->outMessage.begin()))
		return false;

	if (stmt->builder)
		stmt->builder->moveFromResultSet(tdbb, this);

	firstFetchDone = true;

	return true;
}


bool ResultSet::isNull(unsigned param) const
{
	fb_assert(param > 0);

	const dsc* desc = &stmt->outValues[(param - 1) * 2 + 1];
	fb_assert(desc->dsc_dtype == dtype_short);

	return *reinterpret_cast<SSHORT*>(desc->dsc_address) != 0;
}


dsc& ResultSet::getDesc(unsigned param)
{
	fb_assert(param > 0);
	return stmt->outValues[(param - 1) * 2];
}


ScratchBird::string ResultSet::getString(thread_db* tdbb, unsigned param)
{
	fb_assert(param > 0);

	Request* request = stmt->getDsqlRequest()->getRequest();

	// Setup tdbb info necessary for blobs.
	AutoSetRestore2<Request*, thread_db> autoRequest(
		tdbb, &thread_db::getRequest, &thread_db::setRequest, request);
	AutoSetRestore<jrd_tra*> autoRequestTrans(&request->req_transaction,
		tdbb->getTransaction());

	return MOV_make_string2(tdbb, &getDesc(param), CS_NONE);
}


MetaName ResultSet::getMetaName(thread_db* tdbb, unsigned param)
{
	return getString(tdbb, param);
}


ScratchBird::MetaString ResultSet::getMetaString(thread_db* tdbb, unsigned param)
{
	return getString(tdbb, param);
}


void ResultSet::moveDesc(thread_db* tdbb, unsigned param, dsc& desc)
{
	fb_assert(param > 0);

	Request* request = stmt->getDsqlRequest()->getRequest();

	// Setup tdbb info necessary for blobs.
	AutoSetRestore2<Request*, thread_db> autoRequest(
		tdbb, &thread_db::getRequest, &thread_db::setRequest, request);
	AutoSetRestore<jrd_tra*> autoRequestTrans(&request->req_transaction,
		tdbb->getTransaction());

	MOV_move(tdbb, &getDesc(param), &desc);
}


}	// namespace
