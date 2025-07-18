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
 *  The Original Code was created by Dmitry Yemanov
 *  for the ScratchBird Open Source RDBMS project.
 *
 *  Copyright (c) 2011 Dmitry Yemanov <dimitr@firebirdsql.org>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#include "firebird.h"
#include "../jrd/jrd.h"
#include "../jrd/req.h"
#include "../dsql/BoolNodes.h"
#include "../jrd/cmp_proto.h"
#include "../jrd/evl_proto.h"
#include "../jrd/mov_proto.h"
#include "../jrd/evl_proto.h"

#include "RecordSource.h"

using namespace ScratchBird;
using namespace Jrd;

// ------------------------------------
// Data access: predicate driven filter
// ------------------------------------

ConditionalStream::ConditionalStream(CompilerScratch* csb,
									 RecordSource* first, RecordSource* second,
									 BoolExprNode* boolean)
	: RecordSource(csb),
	  m_first(first),
	  m_second(second),
	  m_boolean(boolean)
{
	fb_assert(m_first && m_second && m_boolean);

	m_impure = csb->allocImpure<Impure>();
	m_cardinality = (first->getCardinality() + second->getCardinality()) / 2;
}

void ConditionalStream::internalOpen(thread_db* tdbb) const
{
	Request* const request = tdbb->getRequest();
	Impure* const impure = request->getImpure<Impure>(m_impure);

	impure->irsb_next = m_boolean->execute(tdbb, request) ? m_first : m_second;

	impure->irsb_flags = irsb_open;
	impure->irsb_next->open(tdbb);
}

void ConditionalStream::close(thread_db* tdbb) const
{
	Request* const request = tdbb->getRequest();

	invalidateRecords(request);

	Impure* const impure = request->getImpure<Impure>(m_impure);

	if (impure->irsb_flags & irsb_open)
	{
		impure->irsb_flags &= ~irsb_open;

		if (impure->irsb_next)
			impure->irsb_next->close(tdbb);
	}
}

bool ConditionalStream::internalGetRecord(thread_db* tdbb) const
{
	JRD_reschedule(tdbb);

	Request* const request = tdbb->getRequest();
	Impure* const impure = request->getImpure<Impure>(m_impure);

	if (!(impure->irsb_flags & irsb_open))
		return false;

	return impure->irsb_next->getRecord(tdbb);
}

bool ConditionalStream::refetchRecord(thread_db* tdbb) const
{
	Request* const request = tdbb->getRequest();
	Impure* const impure = request->getImpure<Impure>(m_impure);

	if (!(impure->irsb_flags & irsb_open))
		return false;

	return impure->irsb_next->refetchRecord(tdbb);
}

WriteLockResult ConditionalStream::lockRecord(thread_db* tdbb) const
{
	Request* const request = tdbb->getRequest();
	Impure* const impure = request->getImpure<Impure>(m_impure);

	if (!(impure->irsb_flags & irsb_open))
		return WriteLockResult::CONFLICTED;

	return impure->irsb_next->lockRecord(tdbb);
}

void ConditionalStream::getLegacyPlan(thread_db* tdbb, string& plan, unsigned level) const
{
	if (!level)
		plan += "(";

	m_first->getLegacyPlan(tdbb, plan, level + 1);

	plan += ", ";

	m_second->getLegacyPlan(tdbb, plan, level + 1);

	if (!level)
		plan += ")";
}

void ConditionalStream::internalGetPlan(thread_db* tdbb, PlanEntry& planEntry, unsigned level, bool recurse) const
{
	planEntry.className = "ConditionalStream";

	planEntry.lines.add().text = "Condition";
	printOptInfo(planEntry.lines);

	if (recurse)
	{
		++level;
		m_first->getPlan(tdbb, planEntry.children.add(), level, recurse);
		m_second->getPlan(tdbb, planEntry.children.add(), level, recurse);
	}
}

void ConditionalStream::markRecursive()
{
	m_first->markRecursive();
	m_second->markRecursive();
}

void ConditionalStream::findUsedStreams(StreamList& streams, bool expandAll) const
{
	m_first->findUsedStreams(streams, expandAll);
	m_second->findUsedStreams(streams, expandAll);
}

bool ConditionalStream::isDependent(const StreamList& streams) const
{
	return m_boolean->containsAnyStream(streams) ||
		m_first->isDependent(streams) ||
		m_second->isDependent(streams);
}

void ConditionalStream::invalidateRecords(Request* request) const
{
	m_first->invalidateRecords(request);
	m_second->invalidateRecords(request);
}

void ConditionalStream::nullRecords(thread_db* tdbb) const
{
	m_first->nullRecords(tdbb);
	m_second->nullRecords(tdbb);
}
