/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		jrd_proto.h
 *	DESCRIPTION:	Prototype header file for jrd.cpp
 *
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
 */


#ifndef JRD_JRD_PROTO_H
#define JRD_JRD_PROTO_H

#include "../common/classes/fb_string.h"
#include "../common/classes/objects_array.h"
#include "../jrd/status.h"

namespace Jrd {
	class Database;
	class Attachment;
	class jrd_tra;
	class blb;
	struct bid;
	class Request;
	class Statement;
	class Service;
	class thread_db;
	struct teb;
	class DsqlRequest;
	class MetaName;
}

void jrd_vtof(const char*, char*, SSHORT);

typedef ScratchBird::SortedObjectsArray<ScratchBird::PathName> PathNameList;
void JRD_enum_attachments(PathNameList*, ULONG&, ULONG&, ULONG&);

#ifdef DEBUG_PROCS
void	JRD_print_procedure_info(Jrd::thread_db*, const char*);
#endif


void JRD_autocommit_ddl(Jrd::thread_db* tdbb, Jrd::jrd_tra* transaction);
void JRD_receive(Jrd::thread_db* tdbb, Jrd::Request* request, USHORT msg_type, ULONG msg_length,
	void* msg);
void JRD_start(Jrd::thread_db* tdbb, Jrd::Request* request, Jrd::jrd_tra* transaction);

void JRD_commit_transaction(Jrd::thread_db* tdbb, Jrd::jrd_tra* transaction);
void JRD_commit_retaining(Jrd::thread_db* tdbb, Jrd::jrd_tra* transaction);
void JRD_rollback_transaction(Jrd::thread_db* tdbb, Jrd::jrd_tra* transaction);
void JRD_rollback_retaining(Jrd::thread_db* tdbb, Jrd::jrd_tra* transaction);
void JRD_run_trans_start_triggers(Jrd::thread_db* tdbb, Jrd::jrd_tra* transaction);
void JRD_send(Jrd::thread_db* tdbb, Jrd::Request* request, USHORT msg_type, ULONG msg_length,
	const void* msg);
void JRD_start_and_send(Jrd::thread_db* tdbb, Jrd::Request* request, Jrd::jrd_tra* transaction,
	USHORT msg_type, ULONG msg_length, const void* msg);
void JRD_start_transaction(Jrd::thread_db* tdbb, Jrd::jrd_tra** transaction,
	Jrd::Attachment* attachment, unsigned int tpb_length, const UCHAR* tpb);
void JRD_unwind_request(Jrd::thread_db* tdbb, Jrd::Request* request);
bool JRD_verify_database_access(const ScratchBird::PathName&);
void JRD_shutdown_attachment(Jrd::Attachment* attachment);
void JRD_shutdown_attachments(Jrd::Database* dbb);
void JRD_cancel_operation(Jrd::thread_db* tdbb, Jrd::Attachment* attachment, int option);
void JRD_make_role_name(Jrd::MetaName& userIdRole, const int dialect);
void JRD_transliterate(Jrd::thread_db* tdbb, ScratchBird::IStatus* vector) noexcept;

bool JRD_shutdown_database(Jrd::Database* dbb, const unsigned flags = 0);
// JRD_shutdown_database() flags
static const unsigned SHUT_DBB_RELEASE_POOLS =		0x01;
static const unsigned SHUT_DBB_LINGER =				0x02;
static const unsigned SHUT_DBB_OVERWRITE_CHECK =	0x04;

#endif /* JRD_JRD_PROTO_H */
