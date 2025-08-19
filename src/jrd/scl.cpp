/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		scl.cpp
 *	DESCRIPTION:	Security class handler
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
 * 2001.6.12 Claudio Valderrama: the role should be wiped out if invalid.
 * 2001.8.12 Claudio Valderrama: Squash security bug when processing
 *           identifiers with embedded blanks: check_procedure, check_relation
 *           and check_string, the latter being called from many places.
 *
 */

#include "scratchbird.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "../jrd/jrd.h"
#include "../jrd/ods.h"
#include "../jrd/scl.h"
#include "../jrd/acl.h"
#include "../jrd/blb.h"
#include "../jrd/irq.h"
#include "../jrd/obj.h"
#include "../jrd/req.h"
#include "../jrd/tra.h"
#include "../common/gdsassert.h"
#include "../jrd/blb_proto.h"
#include "../jrd/cmp_proto.h"
#include "../jrd/err_proto.h"
#include "../jrd/exe_proto.h"
#include "../jrd/ini_proto.h"
#include "../yvalve/gds_proto.h"
#include "../common/isc_proto.h"
#include "../jrd/met_proto.h"
#include "../jrd/grant_proto.h"
#include "../jrd/scl_proto.h"
#include "../jrd/constants.h"
#include "sb_exception.h"
#include "../common/utils_proto.h"
#include "../common/classes/array.h"
#include "../common/config/config.h"
#include "../common/os/os_utils.h"
#include "../common/classes/ClumpletWriter.h"
#include "../jrd/PreparedStatement.h"
#include "../jrd/ResultSet.h"


inline constexpr int UIC_BASE = 10;

using namespace Jrd;
using namespace ScratchBird;

static bool check_number(const UCHAR*, USHORT);
static bool check_user_group(thread_db* tdbb, const UCHAR*, USHORT);
static bool check_string(const UCHAR*, const MetaName&);
static SecurityClass::flags_t compute_access(thread_db* tdbb, const SecurityClass*,	SLONG, const QualifiedName&);
static SecurityClass::flags_t get_sys_privileges(thread_db* tdbb);
static SecurityClass::flags_t walk_acl(thread_db* tdbb, const Acl&, const MetaName&,
	SLONG, const QualifiedName&);
static void raiseError(thread_db* tdbb, SecurityClass::flags_t mask, ObjectType type, const QualifiedName& name,
	const MetaName& col_name, const MetaName& invoker);
static bool check_object(thread_db* tdbb, bool found, const SecurityClass* s_class, SLONG obj_type,
	const QualifiedName& obj_name, SecurityClass::flags_t mask, ObjectType type, const QualifiedName& name);


namespace
{
	struct P_NAMES
	{
		SecurityClass::flags_t p_names_priv;
		USHORT p_names_acl;
		const TEXT* p_names_string;
	};

	inline constexpr P_NAMES p_names[] =
	{
		{ SCL_alter, priv_alter, "ALTER" },
		{ SCL_control, priv_control, "CONTROL" },
		{ SCL_drop, priv_drop, "DROP" },
		{ SCL_insert, priv_insert, "INSERT" },
		{ SCL_update, priv_update, "UPDATE" },
		{ SCL_delete, priv_delete, "DELETE" },
		{ SCL_select, priv_select, "SELECT" },
		{ SCL_references, priv_references, "REFERENCES" },
		{ SCL_execute, priv_execute, "EXECUTE" },
		{ SCL_usage, priv_usage, "USAGE" },
		{ SCL_create, priv_create, "CREATE" },
		{ 0, 0, "" }
	};
} // anonymous namespace


static void raiseError(thread_db* tdbb, SecurityClass::flags_t mask, ObjectType type, const QualifiedName& name,
	const MetaName& col_name, const MetaName& invoker)
{
	const P_NAMES* names;
	for (names = p_names; names->p_names_priv; names++)
	{
		if (names->p_names_priv & mask)
			break;
	}

	const char* const ddlObjectName = getDdlObjectName(type);
	const ScratchBird::string fullName = col_name.hasData() ?
		name.toQuotedString() + "." + col_name.toQuotedString() : name.toQuotedString();

	Arg::StatusVector status;
	status << Arg::Gds(isc_no_priv) << Arg::Str(names->p_names_string) <<
			  Arg::Str(ddlObjectName) <<
			  Arg::Str(fullName);
	if (invoker.hasData())
	{
		status << Arg::Gds(isc_effective_user) << Arg::Str(invoker);
	}
	ERR_post(status);

}


void SCL_check_access(thread_db* tdbb,
					  const SecurityClass* s_class,
					  SLONG obj_id,
					  const QualifiedName& obj_name,
					  SecurityClass::flags_t mask,
					  ObjectType type,
					  bool recursive,
					  const QualifiedName& name,
					  const MetaName& col_name)
{
/**************************************
 *
 *	S C L _ c h e c k _ a c c e s s
 *
 **************************************
 *
 * Functional description
 *	Check security class for desired permission.
 *	userName a name of user in which context permissions will be checked.
 *
 **************************************/
	SET_TDBB(tdbb);

	// Allow the replicator any access to database, its permissions are already validated
	if (tdbb->tdbb_flags & TDBB_replicator)
		return;

	const MetaName& userName = s_class->sclClassUser.second;

	if (s_class && (s_class->scl_flags & SCL_corrupt))
	{
		Arg::StatusVector status;
		status << Arg::Gds(isc_no_priv) << Arg::Str("(ACL unrecognized)") <<
										  Arg::Str("security_class") <<
										  s_class->sclClassUser.first;
		if (userName.hasData())
		{
			status << Arg::Gds(isc_effective_user) << Arg::Str(userName);
		}
		ERR_post(status);
	}

	// Make fast check first
	if (mask & get_sys_privileges(tdbb))
		return;

	// Check global DDL permissions with ANY option which allow user to make changes non owned objects
	if (isDdlObject(type) && (mask & SCL_get_object_mask(type, name.schema)))
		return;

	if (!s_class || (mask & s_class->scl_flags) )
		return;

	if (obj_name.object.hasData() && (compute_access(tdbb, s_class, obj_id, obj_name) & mask) )
	{
		return;
	}

	// Allow recursive procedure/function call

	if (recursive &&
		((type == obj_procedures && obj_id == id_procedure) ||
		 (type == obj_functions && obj_id == id_function)) &&
		obj_name == name)
	{
		return;
	}

	raiseError(tdbb, mask, type, name, col_name, userName);
}


void SCL_check_create_access(thread_db* tdbb, ObjectType type, const MetaName& schema)
{
/**************************************
 *
 *	S C L _ c h e c k _ c r e a t e _ a c c e s s
 *
 **************************************
 *
 * Functional description
 *	Check create access on a database object (DDL access)
 *
 **************************************/
	SET_TDBB(tdbb);

	// Allow the replicator any access to database, its permissions are already validated
	if (tdbb->tdbb_flags & TDBB_replicator)
		return;

	Jrd::Attachment* const attachment = tdbb->getAttachment();

	// Allow the locksmith any access to database
	if (attachment->locksmith(tdbb, SystemPrivilege::MODIFY_ANY_OBJECT_IN_DATABASE))
		return;

	const SecurityClass::flags_t obj_mask = SCL_get_object_mask(type, schema);

	if (!(obj_mask & SCL_create))
	{
		const char* name = getDdlObjectName(type);
		ERR_post(Arg::Gds(isc_dyn_no_create_priv) << name);
	}
}


void SCL_check_charset(thread_db* tdbb, const QualifiedName& name, SecurityClass::flags_t mask)
{
/**************************************
 *
 *	S C L _ c h e c k _ c h a r s e t
 *
 **************************************
 *
 * Functional description
 *	Given a character set name, check for a set of privileges.
 *
 **************************************/
	SET_TDBB(tdbb);
	Jrd::Attachment* const attachment = tdbb->getAttachment();

	const SecurityClass* s_class = NULL;
	AutoCacheRequest request(tdbb, irq_cs_security, IRQ_REQUESTS);

	// Converted FOR loop #1: FOR (REQUEST_HANDLE request) CS IN RDB$CHARACTER_SETS
	jrd_req* handle = request;
	EXE_start(tdbb, handle, attachment->getSysTransaction());
	
	struct {
		TEXT schema_name[MAX_SQL_IDENTIFIER_LEN];
		TEXT character_set_name[MAX_SQL_IDENTIFIER_LEN];
	} cs_input;
	
	strcpy(cs_input.schema_name, name.schema.c_str());
	strcpy(cs_input.character_set_name, name.object.c_str());
	
	EXE_send(tdbb, handle, 0, sizeof(cs_input), reinterpret_cast<UCHAR*>(&cs_input));

	struct {
		TEXT RDB$SECURITY_CLASS[MAX_SQL_IDENTIFIER_LEN];
		SSHORT security_class_null;
	} cs_data;

	while (!EXE_receive(tdbb, handle, 1, sizeof(cs_data), reinterpret_cast<UCHAR*>(&cs_data)))
	{
		if (!cs_data.security_class_null) // !CS.RDB$SECURITY_CLASS.NULL
			s_class = SCL_get_class(tdbb, cs_data.RDB$SECURITY_CLASS);
	}

	SCL_check_access(tdbb, s_class, 0, name, mask, obj_charsets, false, name);
}


void SCL_check_collation(thread_db* tdbb, const QualifiedName& name, SecurityClass::flags_t mask)
{
/**************************************
 *
 *	S C L _ c h e c k _ c o l l a t i o n
 *
 **************************************
 *
 * Functional description
 *	Given a collation name, check for a set of privileges.
 *
 **************************************/
	SET_TDBB(tdbb);
	Jrd::Attachment* const attachment = tdbb->getAttachment();

	const SecurityClass* s_class = NULL;
	AutoCacheRequest request(tdbb, irq_coll_security, IRQ_REQUESTS);

	// Converted FOR loop #2: FOR (REQUEST_HANDLE request) COLL IN RDB$COLLATIONS
	jrd_req* handle = request;
	EXE_start(tdbb, handle, attachment->getSysTransaction());
	
	struct {
		TEXT schema_name[MAX_SQL_IDENTIFIER_LEN];
		TEXT collation_name[MAX_SQL_IDENTIFIER_LEN];
	} coll_input;
	
	strcpy(coll_input.schema_name, name.schema.c_str());
	strcpy(coll_input.collation_name, name.object.c_str());
	
	EXE_send(tdbb, handle, 0, sizeof(coll_input), reinterpret_cast<UCHAR*>(&coll_input));

	struct {
		TEXT RDB$SECURITY_CLASS[MAX_SQL_IDENTIFIER_LEN];
		SSHORT security_class_null;
	} coll_data;

	while (!EXE_receive(tdbb, handle, 1, sizeof(coll_data), reinterpret_cast<UCHAR*>(&coll_data)))
	{
		if (!coll_data.security_class_null) // !COLL.RDB$SECURITY_CLASS.NULL
			s_class = SCL_get_class(tdbb, coll_data.RDB$SECURITY_CLASS);
	}

	SCL_check_access(tdbb, s_class, 0, name, mask, obj_collations, false, name);
}


void SCL_check_database(thread_db* tdbb, SecurityClass::flags_t mask)
{
/**************************************
 *
 *	S C L _ c h e c k _ d a t a b a s e
 *
 **************************************
 *
 * Functional description
 *	Check for a set of privileges of current database.
 *
 **************************************/
	SET_TDBB(tdbb);

	Jrd::Attachment* const attachment = tdbb->getAttachment();
	const SecurityClass* const att_class = attachment->att_security_class;
	if (att_class && (att_class->scl_flags & mask))
		return;

	if (mask == SCL_alter && attachment->locksmith(tdbb, USE_NBACKUP_UTILITY))
		return;

	if (mask == SCL_drop && attachment->locksmith(tdbb, DROP_DATABASE))
		return;

	const P_NAMES* names;
	for (names = p_names; names->p_names_priv; names++)
	{
		if (names->p_names_priv & mask)
			break;
	}

	ERR_post(Arg::Gds(isc_no_priv) << Arg::Str(names->p_names_string) <<
									  Arg::Str(getDdlObjectName(obj_database)) <<
									  Arg::Str(""));
}


void SCL_check_domain(thread_db* tdbb, const QualifiedName& name, SecurityClass::flags_t mask)
{
/**************************************
 *
 *	S C L _ c h e c k _ d o m a i n
 *
 **************************************
 *
 * Functional description
 *	Given a domain name, check for a set of privileges.
 *
 **************************************/
	SET_TDBB(tdbb);
	Jrd::Attachment* const attachment = tdbb->getAttachment();

	const SecurityClass* s_class = NULL;
	AutoCacheRequest request(tdbb, irq_gfld_security, IRQ_REQUESTS);

	// Converted FOR loop #3: FOR (REQUEST_HANDLE request) FLD IN RDB$FIELDS
	jrd_req* handle = request;
	EXE_start(tdbb, handle, attachment->getSysTransaction());
	
	struct {
		TEXT schema_name[MAX_SQL_IDENTIFIER_LEN];
		TEXT field_name[MAX_SQL_IDENTIFIER_LEN];
	} fld_input;
	
	strcpy(fld_input.schema_name, name.schema.c_str());
	strcpy(fld_input.field_name, name.object.c_str());
	
	EXE_send(tdbb, handle, 0, sizeof(fld_input), reinterpret_cast<UCHAR*>(&fld_input));

	struct {
		TEXT RDB$SECURITY_CLASS[MAX_SQL_IDENTIFIER_LEN];
		SSHORT security_class_null;
	} fld_data;

	while (!EXE_receive(tdbb, handle, 1, sizeof(fld_data), reinterpret_cast<UCHAR*>(&fld_data)))
	{
		if (!fld_data.security_class_null) // !FLD.RDB$SECURITY_CLASS.NULL
			s_class = SCL_get_class(tdbb, fld_data.RDB$SECURITY_CLASS);
	}

	SCL_check_access(tdbb, s_class, 0, name, mask, obj_domains, false, name);
}


bool SCL_check_exception(thread_db* tdbb, const QualifiedName& name, SecurityClass::flags_t mask)
{
/**************************************
 *
 *	S C L _ c h e c k _ e x c e p t i o n
 *
 **************************************
 *
 * Functional description
 *	Given an exception name, check for a set of privileges.
 *
 **************************************/
	SET_TDBB(tdbb);
	Jrd::Attachment* const attachment = tdbb->getAttachment();

	const SecurityClass* s_class = NULL;
	bool found = false;
	AutoCacheRequest request(tdbb, irq_exc_security, IRQ_REQUESTS);

	// Converted FOR loop #4: FOR (REQUEST_HANDLE request) XCP IN RDB$EXCEPTIONS
	jrd_req* handle = request;
	EXE_start(tdbb, handle, attachment->getSysTransaction());
	
	struct {
		TEXT schema_name[MAX_SQL_IDENTIFIER_LEN];
		TEXT exception_name[MAX_SQL_IDENTIFIER_LEN];
	} xcp_input;
	
	strcpy(xcp_input.schema_name, name.schema.c_str());
	strcpy(xcp_input.exception_name, name.object.c_str());
	
	EXE_send(tdbb, handle, 0, sizeof(xcp_input), reinterpret_cast<UCHAR*>(&xcp_input));

	struct {
		TEXT RDB$SECURITY_CLASS[MAX_SQL_IDENTIFIER_LEN];
		SSHORT security_class_null;
	} xcp_data;

	while (!EXE_receive(tdbb, handle, 1, sizeof(xcp_data), reinterpret_cast<UCHAR*>(&xcp_data)))
	{
		if (!xcp_data.security_class_null) // !XCP.RDB$SECURITY_CLASS.NULL
			s_class = SCL_get_class(tdbb, xcp_data.RDB$SECURITY_CLASS);
		found = true;
	}

	return check_object(tdbb, found, s_class, 0, name, mask, obj_exceptions, name);
}


bool SCL_check_generator(thread_db* tdbb, const QualifiedName& name, SecurityClass::flags_t mask)
{
/**************************************
 *
 *	S C L _ c h e c k _ g e n e r a t o r
 *
 **************************************
 *
 * Functional description
 *	Given a generator name, check for a set of privileges.
 *
 **************************************/
	SET_TDBB(tdbb);
	Jrd::Attachment* const attachment = tdbb->getAttachment();

	const SecurityClass* s_class = NULL;
	bool found = false;
	AutoCacheRequest request(tdbb, irq_gen_security, IRQ_REQUESTS);

	// Converted FOR loop #5: FOR (REQUEST_HANDLE request) GEN IN RDB$GENERATORS
	jrd_req* handle = request;
	EXE_start(tdbb, handle, attachment->getSysTransaction());
	
	struct {
		TEXT schema_name[MAX_SQL_IDENTIFIER_LEN];
		TEXT generator_name[MAX_SQL_IDENTIFIER_LEN];
	} gen_input;
	
	strcpy(gen_input.schema_name, name.schema.c_str());
	strcpy(gen_input.generator_name, name.object.c_str());
	
	EXE_send(tdbb, handle, 0, sizeof(gen_input), reinterpret_cast<UCHAR*>(&gen_input));

	struct {
		TEXT RDB$SECURITY_CLASS[MAX_SQL_IDENTIFIER_LEN];
		SSHORT security_class_null;
	} gen_data;

	while (!EXE_receive(tdbb, handle, 1, sizeof(gen_data), reinterpret_cast<UCHAR*>(&gen_data)))
	{
		if (!gen_data.security_class_null) // !GEN.RDB$SECURITY_CLASS.NULL
			s_class = SCL_get_class(tdbb, gen_data.RDB$SECURITY_CLASS);
		found = true;
	}

	return check_object(tdbb, found, s_class, 0, name, mask, obj_generators, name);
}


void SCL_check_index(thread_db* tdbb, const QualifiedName& index_name, const int index_id, SecurityClass::flags_t mask)
{
/**************************************
 *
 *	S C L _ c h e c k _ i n d e x
 *
 **************************************
 *
 * Functional description
 *	Given a index name (as a TEXT), check for a
 *      set of privileges on the table that the index is on and
 *      on the fields involved in that index.
 *   CVC: Allow the same function to use the zero-based index id, too.
 *      The idx.idx_id value is zero based but system tables use
 *      index id's being one based, hence adjust the incoming value
 *      before calling this function. If you use index_id, index_name
 *      becomes relation_name since index ids are relative to tables.
 *
 **************************************/
	SET_TDBB(tdbb);
	Jrd::Attachment* const attachment = tdbb->getAttachment();

	const SecurityClass* s_class = NULL;
	const SecurityClass* default_s_class = NULL;

	// No security to check for if the index is not yet created

    if ((index_name.object.length() == 0) && (index_id < 1)) {
        return;
    }

	QualifiedName reln_name, aux_idx_name;
	auto idx_name_ptr = &index_name;
	const auto relation_name_ptr = &index_name;

	AutoRequest request;
	int systemFlag = 0;

	// No need to cache this request handle, it's only used when
	// new constraints are created

    if (index_id < 1)
    {
		// Converted FOR loop #6: FOR(REQUEST_HANDLE request) IND IN RDB$INDICES CROSS REL IN RDB$RELATIONS OVER RDB$SCHEMA_NAME, RDB$RELATION_NAME
		jrd_req* handle = request;
		EXE_start(tdbb, handle, attachment->getSysTransaction());
		
		struct {
			TEXT schema_name[MAX_SQL_IDENTIFIER_LEN];
			TEXT index_name[MAX_SQL_IDENTIFIER_LEN];
		} ind_input;
		
		strcpy(ind_input.schema_name, index_name.schema.c_str());
		strcpy(ind_input.index_name, index_name.object.c_str());
		
		EXE_send(tdbb, handle, 0, sizeof(ind_input), reinterpret_cast<UCHAR*>(&ind_input));

		struct {
			TEXT RDB$RELATION_NAME[MAX_SQL_IDENTIFIER_LEN];
			TEXT RDB$SCHEMA_NAME[MAX_SQL_IDENTIFIER_LEN];
			TEXT RDB$SECURITY_CLASS[MAX_SQL_IDENTIFIER_LEN];
			TEXT RDB$DEFAULT_CLASS[MAX_SQL_IDENTIFIER_LEN];
			SSHORT RDB$SYSTEM_FLAG;
			SSHORT security_class_null;
			SSHORT default_class_null;
		} ind_data;

		while (!EXE_receive(tdbb, handle, 1, sizeof(ind_data), reinterpret_cast<UCHAR*>(&ind_data)))
		{
            reln_name = QualifiedName(ind_data.RDB$RELATION_NAME, ind_data.RDB$SCHEMA_NAME);
		    if (!ind_data.security_class_null) // !REL.RDB$SECURITY_CLASS.NULL
                s_class = SCL_get_class(tdbb, ind_data.RDB$SECURITY_CLASS);
            if (!ind_data.default_class_null) // !REL.RDB$DEFAULT_CLASS.NULL
                default_s_class = SCL_get_class(tdbb, ind_data.RDB$DEFAULT_CLASS);
			systemFlag = ind_data.RDB$SYSTEM_FLAG;
		}
    }
    else
    {
        idx_name_ptr = &aux_idx_name;
		// Converted FOR loop #7: FOR (REQUEST_HANDLE request) IND IN RDB$INDICES CROSS REL IN RDB$RELATIONS OVER RDB$SCHEMA_NAME, RDB$RELATION_NAME
		jrd_req* handle = request;
		EXE_start(tdbb, handle, attachment->getSysTransaction());
		
		struct {
			TEXT schema_name[MAX_SQL_IDENTIFIER_LEN];
			TEXT relation_name[MAX_SQL_IDENTIFIER_LEN];
			SSHORT index_id;
		} ind_input2;
		
		strcpy(ind_input2.schema_name, relation_name_ptr->schema.c_str());
		strcpy(ind_input2.relation_name, relation_name_ptr->object.c_str());
		ind_input2.index_id = index_id;
		
		EXE_send(tdbb, handle, 0, sizeof(ind_input2), reinterpret_cast<UCHAR*>(&ind_input2));

		struct {
			TEXT RDB$INDEX_NAME[MAX_SQL_IDENTIFIER_LEN];
			TEXT IND_RDB$SCHEMA_NAME[MAX_SQL_IDENTIFIER_LEN];
			TEXT RDB$RELATION_NAME[MAX_SQL_IDENTIFIER_LEN];
			TEXT REL_RDB$SCHEMA_NAME[MAX_SQL_IDENTIFIER_LEN];
			TEXT RDB$SECURITY_CLASS[MAX_SQL_IDENTIFIER_LEN];
			TEXT RDB$DEFAULT_CLASS[MAX_SQL_IDENTIFIER_LEN];
			SSHORT RDB$SYSTEM_FLAG;
			SSHORT security_class_null;
			SSHORT default_class_null;
		} ind_data2;

		while (!EXE_receive(tdbb, handle, 1, sizeof(ind_data2), reinterpret_cast<UCHAR*>(&ind_data2)))
		{
            reln_name = QualifiedName(ind_data2.RDB$RELATION_NAME, ind_data2.REL_RDB$SCHEMA_NAME);
            aux_idx_name = QualifiedName(ind_data2.RDB$INDEX_NAME, ind_data2.IND_RDB$SCHEMA_NAME);
            if (!ind_data2.security_class_null) // !REL.RDB$SECURITY_CLASS.NULL
                s_class = SCL_get_class(tdbb, ind_data2.RDB$SECURITY_CLASS);
            if (!ind_data2.default_class_null) // !REL.RDB$DEFAULT_CLASS.NULL
                default_s_class = SCL_get_class(tdbb, ind_data2.RDB$DEFAULT_CLASS);
			systemFlag = ind_data2.RDB$SYSTEM_FLAG;
        }
    }

	if (systemFlag == 1 && !attachment->isRWGbak())
	{
		// Someone is going to reference system table in FK
		// Usually it's not good idea
		raiseError(tdbb, mask, obj_relations, reln_name, {}, {});
	}

	// Check if the relation exists. It may not have been created yet.
	// Just return in that case.

	if (reln_name.object.isEmpty())
		return;

	SCL_check_access(tdbb, s_class, 0, {}, mask, obj_relations, false, reln_name);

	request.reset();

	// Check if the field used in the index has the appropriate
	// permission. If the field in question does not have a security class
	// defined, then the default security class for the table applies for that
	// field.

	// No need to cache this request handle, it's only used when
	// new constraints are created

	// Converted FOR loop #8: FOR(REQUEST_HANDLE request) ISEG IN RDB$INDEX_SEGMENTS CROSS RF IN RDB$RELATION_FIELDS OVER RDB$SCHEMA_NAME, RDB$FIELD_NAME
	jrd_req* handle3 = request;
	EXE_start(tdbb, handle3, attachment->getSysTransaction());
	
	struct {
		TEXT rel_schema_name[MAX_SQL_IDENTIFIER_LEN];
		TEXT relation_name[MAX_SQL_IDENTIFIER_LEN];
		TEXT idx_schema_name[MAX_SQL_IDENTIFIER_LEN];
		TEXT index_name[MAX_SQL_IDENTIFIER_LEN];
	} iseg_input;
	
	strcpy(iseg_input.rel_schema_name, reln_name.schema.c_str());
	strcpy(iseg_input.relation_name, reln_name.object.c_str());
	strcpy(iseg_input.idx_schema_name, idx_name_ptr->schema.c_str());
	strcpy(iseg_input.index_name, idx_name_ptr->object.c_str());
	
	EXE_send(tdbb, handle3, 0, sizeof(iseg_input), reinterpret_cast<UCHAR*>(&iseg_input));

	struct {
		TEXT RDB$FIELD_NAME[MAX_SQL_IDENTIFIER_LEN];
		TEXT RDB$SECURITY_CLASS[MAX_SQL_IDENTIFIER_LEN];
		SSHORT security_class_null;
	} iseg_data;

	while (!EXE_receive(tdbb, handle3, 1, sizeof(iseg_data), reinterpret_cast<UCHAR*>(&iseg_data)))
	{
		s_class = (!iseg_data.security_class_null) ?
			SCL_get_class(tdbb, iseg_data.RDB$SECURITY_CLASS) : default_s_class;
		SCL_check_access(tdbb, s_class, 0, {}, mask,
						 obj_column, false, reln_name, iseg_data.RDB$FIELD_NAME);
	}
}


bool SCL_check_package(thread_db* tdbb, const QualifiedName& name, SecurityClass::flags_t mask)
{
/**************************************
 *
 *	S C L _ c h e c k _ p a c k a g e
 *
 **************************************
 *
 * Functional description
 *	Given a package name, check for a set of privileges.  The
 *	package in question may or may not have been created, let alone
 *	scanned.  This is used exclusively for meta-data operations.
 *
 **************************************/
	SET_TDBB(tdbb);

	// Get the name in CSTRING format, ending on NULL or SPACE

	Jrd::Attachment* const attachment = tdbb->getAttachment();

	const SecurityClass* s_class = NULL;
	bool found = false;
	AutoCacheRequest request(tdbb, irq_pkg_security, IRQ_REQUESTS);

	// Converted FOR loop #9: FOR (REQUEST_HANDLE request) PKG IN RDB$PACKAGES
	jrd_req* handle = request;
	EXE_start(tdbb, handle, attachment->getSysTransaction());
	
	struct {
		TEXT schema_name[MAX_SQL_IDENTIFIER_LEN];
		TEXT package_name[MAX_SQL_IDENTIFIER_LEN];
	} pkg_input;
	
	strcpy(pkg_input.schema_name, name.schema.c_str());
	strcpy(pkg_input.package_name, name.object.c_str());
	
	EXE_send(tdbb, handle, 0, sizeof(pkg_input), reinterpret_cast<UCHAR*>(&pkg_input));

	struct {
		TEXT RDB$SECURITY_CLASS[MAX_SQL_IDENTIFIER_LEN];
		SSHORT security_class_null;
	} pkg_data;

	while (!EXE_receive(tdbb, handle, 1, sizeof(pkg_data), reinterpret_cast<UCHAR*>(&pkg_data)))
	{
		if (!pkg_data.security_class_null) // !PKG.RDB$SECURITY_CLASS.NULL
			s_class = SCL_get_class(tdbb, pkg_data.RDB$SECURITY_CLASS);
		found = true;
	}

	return check_object(tdbb, found, s_class, id_package, name, mask, obj_packages, name);
}


bool SCL_check_procedure(thread_db* tdbb, const QualifiedName& name, SecurityClass::flags_t mask)
{
/**************************************
 *
 *	S C L _ c h e c k _ p r o c e d u r e
 *
 **************************************
 *
 * Functional description
 *	Given a procedure name, check for a set of privileges.  The
 *	procedure in question may or may not have been created, let alone
 *	scanned.  This is used exclusively for meta-data operations.
 *
 **************************************/
	SET_TDBB(tdbb);

	Jrd::Attachment* const attachment = tdbb->getAttachment();

	const SecurityClass* s_class = NULL;
	bool found = false;
	AutoCacheRequest request(tdbb, irq_p_security, IRQ_REQUESTS);

	// Converted FOR loop #10: FOR (REQUEST_HANDLE request) PRC IN RDB$PROCEDURES
	jrd_req* handle = request;
	EXE_start(tdbb, handle, attachment->getSysTransaction());
	
	struct {
		TEXT schema_name[MAX_SQL_IDENTIFIER_LEN];
		TEXT procedure_name[MAX_SQL_IDENTIFIER_LEN];
	} prc_input;
	
	strcpy(prc_input.schema_name, name.schema.c_str());
	strcpy(prc_input.procedure_name, name.object.c_str());
	
	EXE_send(tdbb, handle, 0, sizeof(prc_input), reinterpret_cast<UCHAR*>(&prc_input));

	struct {
		TEXT RDB$SECURITY_CLASS[MAX_SQL_IDENTIFIER_LEN];
		SSHORT security_class_null;
	} prc_data;

	while (!EXE_receive(tdbb, handle, 1, sizeof(prc_data), reinterpret_cast<UCHAR*>(&prc_data)))
	{
		if (!prc_data.security_class_null) // !PRC.RDB$SECURITY_CLASS.NULL
			s_class = SCL_get_class(tdbb, prc_data.RDB$SECURITY_CLASS);
		found = true;
	}

	return check_object(tdbb, found, s_class, id_procedure, name, mask, obj_procedures, name);
}


bool SCL_check_function(thread_db* tdbb, const QualifiedName& name, SecurityClass::flags_t mask)
{
/**************************************
 *
 *	S C L _ c h e c k _ f u n c t i o n
 *
 **************************************
 *
 * Functional description
 *	Given a function name, check for a set of privileges.  The
 *	function in question may or may not have been created, let alone
 *	scanned.  This is used exclusively for meta-data operations.
 *
 **************************************/
	SET_TDBB(tdbb);

	Jrd::Attachment* const attachment = tdbb->getAttachment();

	const SecurityClass* s_class = NULL;
	bool found = false;
	AutoCacheRequest request(tdbb, irq_f_security, IRQ_REQUESTS);

	// Converted FOR loop #11: FOR (REQUEST_HANDLE request) FUN IN RDB$FUNCTIONS
	jrd_req* handle = request;
	EXE_start(tdbb, handle, attachment->getSysTransaction());
	
	struct {
		TEXT schema_name[MAX_SQL_IDENTIFIER_LEN];
		TEXT function_name[MAX_SQL_IDENTIFIER_LEN];
	} fun_input;
	
	strcpy(fun_input.schema_name, name.schema.c_str());
	strcpy(fun_input.function_name, name.object.c_str());
	
	EXE_send(tdbb, handle, 0, sizeof(fun_input), reinterpret_cast<UCHAR*>(&fun_input));

	struct {
		TEXT RDB$SECURITY_CLASS[MAX_SQL_IDENTIFIER_LEN];
		SSHORT security_class_null;
	} fun_data;

	while (!EXE_receive(tdbb, handle, 1, sizeof(fun_data), reinterpret_cast<UCHAR*>(&fun_data)))
	{
		if (!fun_data.security_class_null) // !FUN.RDB$SECURITY_CLASS.NULL
			s_class = SCL_get_class(tdbb, fun_data.RDB$SECURITY_CLASS);
		found = true;
	}

	return check_object(tdbb, found, s_class, id_function, name, mask, obj_functions, name);
}


void SCL_check_filter(thread_db* tdbb, const MetaName& name, SecurityClass::flags_t mask)
{
/**************************************
 *
 *	S C L _ c h e c k _ f i l t e r
 *
 **************************************
 *
 * Functional description
 *	Given a filter name, check for a set of privileges.  The
 *	filter in question may or may not have been created, let alone
 *	scanned.  This is used exclusively for meta-data operations.
 *
 **************************************/
	SET_TDBB(tdbb);

	Jrd::Attachment* const attachment = tdbb->getAttachment();

	const SecurityClass* s_class = NULL;
	AutoCacheRequest request(tdbb, irq_f_security, IRQ_REQUESTS);

	// Converted FOR loop #12: FOR (REQUEST_HANDLE request) FLT IN RDB$FILTERS
	jrd_req* handle = request;
	EXE_start(tdbb, handle, attachment->getSysTransaction());
	
	struct {
		TEXT function_name[MAX_SQL_IDENTIFIER_LEN];
	} flt_input;
	
	strcpy(flt_input.function_name, name.c_str());
	
	EXE_send(tdbb, handle, 0, sizeof(flt_input), reinterpret_cast<UCHAR*>(&flt_input));

	struct {
		TEXT RDB$SECURITY_CLASS[MAX_SQL_IDENTIFIER_LEN];
		SSHORT security_class_null;
	} flt_data;

	while (!EXE_receive(tdbb, handle, 1, sizeof(flt_data), reinterpret_cast<UCHAR*>(&flt_data)))
	{
		if (!flt_data.security_class_null) // !FLT.RDB$SECURITY_CLASS.NULL
			s_class = SCL_get_class(tdbb, flt_data.RDB$SECURITY_CLASS);
	}

	SCL_check_access(tdbb, s_class, id_filter, QualifiedName(name), mask,
		obj_filters, false, QualifiedName(name));
}


void SCL_check_relation(thread_db* tdbb, const QualifiedName& name, SecurityClass::flags_t mask,
	bool protectSys)
{
/**************************************
 *
 *	S C L _ c h e c k _ r e l a t i o n
 *
 **************************************
 *
 * Functional description
 *	Given a relation name, check for a set of privileges.  The
 *	relation in question may or may not have been created, let alone
 *	scanned.  This is used exclusively for meta-data operations.
 *
 **************************************/
	SET_TDBB(tdbb);

	Jrd::Attachment* const attachment = tdbb->getAttachment();

	const SecurityClass* s_class = NULL;
	AutoCacheRequest request(tdbb, irq_v_security_r, IRQ_REQUESTS);

	// Converted FOR loop #13: FOR(REQUEST_HANDLE request) REL IN RDB$RELATIONS
	jrd_req* handle = request;
	EXE_start(tdbb, handle, attachment->getSysTransaction());
	
	struct {
		TEXT schema_name[MAX_SQL_IDENTIFIER_LEN];
		TEXT relation_name[MAX_SQL_IDENTIFIER_LEN];
	} rel_input;
	
	strcpy(rel_input.schema_name, name.schema.c_str());
	strcpy(rel_input.relation_name, name.object.c_str());
	
	EXE_send(tdbb, handle, 0, sizeof(rel_input), reinterpret_cast<UCHAR*>(&rel_input));

	struct {
		TEXT RDB$SECURITY_CLASS[MAX_SQL_IDENTIFIER_LEN];
		SSHORT RDB$SYSTEM_FLAG;
		SSHORT security_class_null;
	} rel_data;

	while (!EXE_receive(tdbb, handle, 1, sizeof(rel_data), reinterpret_cast<UCHAR*>(&rel_data)))
	{
		if (protectSys && rel_data.RDB$SYSTEM_FLAG == 1 && !attachment->isRWGbak())
		{
			// Someone is going to modify system table layout
			// Usually it's not good idea
			raiseError(tdbb, mask, obj_relations, name, {}, {});
		}

		if (!rel_data.security_class_null) // !REL.RDB$SECURITY_CLASS.NULL
			s_class = SCL_get_class(tdbb, rel_data.RDB$SECURITY_CLASS);
	}

	SCL_check_access(tdbb, s_class, 0, {}, mask, obj_relations, false, name);
}

// Given a schema name, check for a set of privileges.
bool SCL_check_schema(thread_db* tdbb, const MetaName& name, SecurityClass::flags_t mask)
{
	SET_TDBB(tdbb);

	Jrd::Attachment* const attachment = tdbb->getAttachment();

	const SecurityClass* s_class = nullptr;
	bool found = false;

	static const CachedRequestId requestId;
	AutoCacheRequest request(tdbb, requestId);

	// Converted FOR loop #14: FOR(REQUEST_HANDLE request) SCH IN RDB$SCHEMAS
	jrd_req* handle = request;
	EXE_start(tdbb, handle, attachment->getSysTransaction());
	
	struct {
		TEXT schema_name[MAX_SQL_IDENTIFIER_LEN];
	} sch_input;
	
	strcpy(sch_input.schema_name, name.c_str());
	
	EXE_send(tdbb, handle, 0, sizeof(sch_input), reinterpret_cast<UCHAR*>(&sch_input));

	struct {
		TEXT RDB$SECURITY_CLASS[MAX_SQL_IDENTIFIER_LEN];
		SSHORT security_class_null;
	} sch_data;

	while (!EXE_receive(tdbb, handle, 1, sizeof(sch_data), reinterpret_cast<UCHAR*>(&sch_data)))
	{
		if (!sch_data.security_class_null) // !SCH.RDB$SECURITY_CLASS.NULL
			s_class = SCL_get_class(tdbb, sch_data.RDB$SECURITY_CLASS);
		found = true;
	}

	return check_object(tdbb, found, s_class, 0, {}, mask, obj_schemas, QualifiedName(name));
}

bool SCL_check_view(thread_db* tdbb, const QualifiedName& name, SecurityClass::flags_t mask)
{
/**************************************
 *
 *	S C L _ c h e c k _ v i e w
 *
 **************************************
 *
 * Functional description
 *	Given a view name, check for a set of privileges.  The
 *	relation in question may or may not have been created, let alone
 *	scanned.  This is used exclusively for meta-data operations.
 *
 **************************************/
	SET_TDBB(tdbb);

	Jrd::Attachment* const attachment = tdbb->getAttachment();

	const SecurityClass* s_class = NULL;
	bool found = false;
	AutoCacheRequest request(tdbb, irq_v_security_v, IRQ_REQUESTS);

	// Converted FOR loop #15: FOR(REQUEST_HANDLE request) REL IN RDB$RELATIONS
	jrd_req* handle = request;
	EXE_start(tdbb, handle, attachment->getSysTransaction());
	
	struct {
		TEXT schema_name[MAX_SQL_IDENTIFIER_LEN];
		TEXT relation_name[MAX_SQL_IDENTIFIER_LEN];
	} rel_input;
	
	strcpy(rel_input.schema_name, name.schema.c_str());
	strcpy(rel_input.relation_name, name.object.c_str());
	
	EXE_send(tdbb, handle, 0, sizeof(rel_input), reinterpret_cast<UCHAR*>(&rel_input));

	struct {
		TEXT RDB$SECURITY_CLASS[MAX_SQL_IDENTIFIER_LEN];
		SSHORT security_class_null;
	} rel_data;

	while (!EXE_receive(tdbb, handle, 1, sizeof(rel_data), reinterpret_cast<UCHAR*>(&rel_data)))
	{
		if (!rel_data.security_class_null) // !REL.RDB$SECURITY_CLASS.NULL
			s_class = SCL_get_class(tdbb, rel_data.RDB$SECURITY_CLASS);
		found = true;
	}

	return check_object(tdbb, found, s_class, 0, {}, mask, obj_views, name);
}

void SCL_check_role(thread_db* tdbb, const MetaName& name, SecurityClass::flags_t mask)
{
/**************************************
 *
 *	S C L _ c h e c k _ r o l e
 *
 **************************************
 *
 * Functional description
 *	Given a role name, check for a set of privileges.
 *
 **************************************/
	SET_TDBB(tdbb);

	Jrd::Attachment* const attachment = tdbb->getAttachment();

	const SecurityClass* s_class = NULL;
	AutoCacheRequest request(tdbb, irq_v_security_o, IRQ_REQUESTS);

	// Converted FOR loop #16: FOR(REQUEST_HANDLE request) R IN RDB$ROLES
	jrd_req* handle = request;
	EXE_start(tdbb, handle, attachment->getSysTransaction());
	
	struct {
		TEXT role_name[MAX_SQL_IDENTIFIER_LEN];
	} role_input;
	
	strcpy(role_input.role_name, name.c_str());
	
	EXE_send(tdbb, handle, 0, sizeof(role_input), reinterpret_cast<UCHAR*>(&role_input));

	struct {
		TEXT RDB$SECURITY_CLASS[MAX_SQL_IDENTIFIER_LEN];
		SSHORT security_class_null;
	} role_data;

	while (!EXE_receive(tdbb, handle, 1, sizeof(role_data), reinterpret_cast<UCHAR*>(&role_data)))
	{
		if (!role_data.security_class_null) // !R.RDB$SECURITY_CLASS.NULL
			s_class = SCL_get_class(tdbb, role_data.RDB$SECURITY_CLASS);
	}

	SCL_check_access(tdbb, s_class, 0, {}, mask, obj_roles, false, QualifiedName(name));
}

SecurityClass* SCL_get_class(thread_db* tdbb, const MetaName& name)
{
/**************************************
 *
 *	S C L _ g e t _ c l a s s
 *
 **************************************
 *
 * Functional description
 *	Look up security class of the effective user first in memory, then in database.
 *	If we don't find it, just return NULL. If we do, return a security class block.
 *
 **************************************/
	SET_TDBB(tdbb);

	// Name may be absent.
	if (name.isEmpty())
		return nullptr;

	Jrd::Attachment* const attachment = tdbb->getAttachment();
	const MetaString& userName = attachment->getEffectiveUserName();

	const MetaNamePair key(name, userName);
	// Look for the class already known

	SecurityClassList* list = attachment->att_security_classes;
	if (list && list->locate(key))
		return list->current();

	// Class isn't known. So make up a new security class block.

	MemoryPool& pool = *attachment->att_pool;

	SecurityClass* const s_class = FB_NEW_POOL(pool) SecurityClass(pool, name, userName);
	s_class->scl_flags = compute_access(tdbb, s_class, 0, {});

	if (s_class->scl_flags & SCL_exists)
	{
		if (!list) {
			attachment->att_security_classes = list = FB_NEW_POOL(pool) SecurityClassList(pool);
		}

		list->add(s_class);
		return s_class;
	}

	delete s_class;

	return NULL;
}


SecurityClass::flags_t SCL_get_mask(thread_db* tdbb, const QualifiedName& relation_name, const TEXT* field_name)
{
/**************************************
 *
 *	S C L _ g e t _ m a s k
 *
 **************************************
 *
 * Functional description
 *	Get a protection mask for a named object.  If field and
 *	relation names are present, get access to field.  If just
 *	relation name, get access to relation.
 *
 **************************************/
	SET_TDBB(tdbb);
	Jrd::Attachment* const attachment = tdbb->getAttachment();
	SecurityClass::flags_t access = ~0;

	// If there's a relation, track it down
	jrd_rel* relation;
	if (relation_name.object.hasData() && (relation = MET_lookup_relation(tdbb, relation_name)))
	{
		MET_scan_relation(tdbb, relation);
		const SecurityClass* s_class;
		if ( (s_class = SCL_get_class(tdbb, relation->rel_security_name.object)) )
		{
			access &= s_class->scl_flags;
		}

		const jrd_fld* field;
		SSHORT id;
		if (field_name &&
			(id = MET_lookup_field(tdbb, relation, field_name)) >= 0 &&
			(field = MET_get_field(relation, id)) &&
			(s_class = SCL_get_class(tdbb, field->fld_security_name)))
		{
			access &= s_class->scl_flags;
		}
	}

	return access & (SCL_select | SCL_drop | SCL_control |
					 SCL_insert | SCL_update |
					 SCL_delete | SCL_alter | SCL_references |
					 SCL_execute | SCL_usage);
}


bool SCL_role_granted(thread_db* tdbb, const UserId& usr, const TEXT* sql_role)
{
/**************************************
 *
 *	S C L _ r o l e _ g r a n t e d
 *
 **************************************
 *
 * Functional description
 *	Check is sql_role granted to the user.
 *
 **************************************/
	SET_TDBB(tdbb);
	Jrd::Attachment* const attachment = tdbb->getAttachment();

	if (!strcmp(sql_role, NULL_ROLE))
	{
		return true;
	}

	bool found = false;

	AutoCacheRequest request(tdbb, irq_verify_role_name, IRQ_REQUESTS);

	// CVC: The caller has hopefully uppercased the role or stripped quotes. Of course,
	// uppercase should only happen if the role wasn't enclosed in quotes.
	// Shortsighted developers named the field rdb$relation_name instead of rdb$object_name.
	// This request is not exactly the same than irq_get_role_mem, sorry, I can't reuse that.
	// If you think that an unknown role cannot be granted, think again: someone made sure
	// in DYN that SYSDBA can do almost anything, including invalid grants.

	// Converted FOR loop #17: FOR (REQUEST_HANDLE request) FIRST 1 RR IN RDB$ROLES CROSS UU IN RDB$USER_PRIVILEGES
	jrd_req* handle = request;
	EXE_start(tdbb, handle, attachment->getSysTransaction());
	
	struct {
		TEXT role_name[MAX_SQL_IDENTIFIER_LEN];
		TEXT user_name[MAX_SQL_IDENTIFIER_LEN];
	} role_input;
	
	strcpy(role_input.role_name, sql_role);
	strcpy(role_input.user_name, usr.getUserName().c_str());
	
	EXE_send(tdbb, handle, 0, sizeof(role_input), reinterpret_cast<UCHAR*>(&role_input));

	struct {
		TEXT RDB$USER[MAX_SQL_IDENTIFIER_LEN];
		SSHORT user_null;
	} role_data;

	while (!EXE_receive(tdbb, handle, 1, sizeof(role_data), reinterpret_cast<UCHAR*>(&role_data)))
	{
		if (!role_data.user_null) // !UU.RDB$USER.NULL
			found = true;
		break; // FIRST 1
	}

	return found;
}


void UserId::findGrantedRoles(thread_db* tdbb) const
{
	try
	{
		SET_TDBB(tdbb);
		Jrd::Attachment* const attachment = tdbb->getAttachment();

		PreparedStatement::Builder sql;
		MetaName usr_get_role;
		string usr_get_priv;
		sql << "with recursive role_tree as ( "
			<< "   select rdb$relation_name as nm, 0 as ur from system.rdb$user_privileges "
			<< "       where rdb$privilege = 'M' and rdb$field_name = 'D'"
			<< "			and (rdb$user = " << usr_user_name << "	or rdb$user = 'PUBLIC')"
			<< "			and rdb$user_type = 8 "
			<< "   union all "
			<< "   select rdb$role_name as nm, 1 as ur from system.rdb$roles "
			<< "       where rdb$role_name = " << usr_sql_role_name
			<< "   union all "
			<< "   select p.rdb$relation_name as nm, t.ur from system.rdb$user_privileges p "
			<< "       join role_tree t on t.nm = p.rdb$user "
			<< "       where p.rdb$privilege = 'M' and (p.rdb$field_name = 'D' or t.ur = 1)) "
			<< "select " << sql("r.rdb$role_name, ",  usr_get_role)
			<< 	   sql("r.rdb$system_privileges ", usr_get_priv)
			<< "   from role_tree t join system.rdb$roles r on t.nm = r.rdb$role_name ";

		AutoPreparedStatement stmt(attachment->prepareStatement(tdbb, attachment->getSysTransaction(), sql));
		AutoResultSet rs(stmt->executeQuery(tdbb, attachment->getSysTransaction()));

		usr_granted_roles.clear();
		usr_privileges.clearAll();

		while (rs->fetch(tdbb))
		{
			if (!usr_granted_roles.exist(usr_get_role))	// SQL request can return duplicates
			{
				usr_granted_roles.add(usr_get_role);
				Privileges p;
				p.load(usr_get_priv.c_str());
				usr_privileges |= p;
			}
		}
	}
	catch (const Exception& e)
	{
		if (!(usr_flags & USR_sysdba))
			throw;

		e.stuffException(tdbb->tdbb_status_vector);
		if (!fb_utils::containsErrorCode(tdbb->tdbb_status_vector->getErrors(), isc_collation_not_installed))
			throw;

		usr_privileges.setAll();
		tdbb->tdbb_status_vector->init();
	}

	usr_flags &= ~(USR_newrole | USR_sysdba);
}


void UserId::setRoleTrusted()
{
	if (!usr_trusted_role.hasData())
		Arg::Gds(isc_miss_trusted_role).raise();
	setSqlRole(usr_trusted_role);
}


void UserId::sclInit(thread_db* tdbb, bool create)
{
/**************************************
 *
 *	S C L _ i n i t
 *
 **************************************
 *
 * Functional description
 *	Check database access control list.
 *
 *	Finally fills UserId information
 *	(role, flags, etc.).
 *
 **************************************/
	SET_TDBB(tdbb);
	Database* const dbb = tdbb->getDatabase();
	Jrd::Attachment* const attachment = tdbb->getAttachment();

	const TEXT* sql_role = getSqlRole().nullStr();

    // CVC: We'll verify the role and wipe it out when it doesn't exist

	if (getUserName().hasData() && !create)
	{
		const TEXT* login_name = getUserName().c_str();

		AutoCacheRequest request(tdbb, irq_get_role_name, IRQ_REQUESTS);

		// Converted FOR loop #18: FOR(REQUEST_HANDLE request) X IN RDB$ROLES
		jrd_req* handle = request;
		EXE_start(tdbb, handle, attachment->getSysTransaction());
		
		struct {
			TEXT role_name[MAX_SQL_IDENTIFIER_LEN];
		} role_input;
		
		strcpy(role_input.role_name, login_name);
		
		EXE_send(tdbb, handle, 0, sizeof(role_input), reinterpret_cast<UCHAR*>(&role_input));

		struct {
			// No output data needed, just check if role exists
		} role_data;

		while (!EXE_receive(tdbb, handle, 1, sizeof(role_data), reinterpret_cast<UCHAR*>(&role_data)))
		{
			ERR_post(Arg::Gds(isc_login_same_as_role_name) << Arg::Str(login_name));
		}
	}

    // CVC: If we aren't creating a db and sql_role was specified,
    // then verify it against rdb$roles and rdb$user_privileges

    if (!create && sql_role && *sql_role)
    {
        if (!SCL_role_granted(tdbb, *this, sql_role))
            sql_role = NULL;
    }

	if (!sql_role)
		sql_role = getTrustedRole().nullStr();

	MetaString role_name(sql_role ? sql_role : NULL_ROLE);

	MemoryPool& pool = *attachment->att_pool;
	UserId* const user = FB_NEW_POOL(pool) UserId(pool, *this);
	user->setSqlRole(role_name);
	user->usr_init_role = role_name;
	attachment->att_user = user;

	if (!create)
	{
		AutoCacheRequest request(tdbb, irq_get_att_class, IRQ_REQUESTS);

		// Converted FOR loop #19: FOR(REQUEST_HANDLE request) X IN RDB$DATABASE
		jrd_req* handle = request;
		EXE_start(tdbb, handle, attachment->getSysTransaction());
		
		// No input parameters needed
		EXE_send(tdbb, handle, 0, 0, nullptr);

		struct {
			TEXT RDB$SECURITY_CLASS[MAX_SQL_IDENTIFIER_LEN];
			SSHORT security_class_null;
		} db_data;

		while (!EXE_receive(tdbb, handle, 1, sizeof(db_data), reinterpret_cast<UCHAR*>(&db_data)))
		{
			if (!db_data.security_class_null) // !X.RDB$SECURITY_CLASS.NULL
				attachment->att_security_class = SCL_get_class(tdbb, db_data.RDB$SECURITY_CLASS);
		}

		if (dbb->dbb_owner.isEmpty())
		{
			AutoRequest request2;

			// Converted FOR loop #20: FOR(REQUEST_HANDLE request2) FIRST 1 REL IN RDB$RELATIONS
			jrd_req* handle2 = request2;
			EXE_start(tdbb, handle2, attachment->getSysTransaction());
			
			struct {
				TEXT schema_name[MAX_SQL_IDENTIFIER_LEN];
				TEXT relation_name[MAX_SQL_IDENTIFIER_LEN];
			} rel_input;
			
			strcpy(rel_input.schema_name, SYSTEM_SCHEMA);
			strcpy(rel_input.relation_name, "RDB$DATABASE");
			
			EXE_send(tdbb, handle2, 0, sizeof(rel_input), reinterpret_cast<UCHAR*>(&rel_input));

			struct {
				TEXT RDB$OWNER_NAME[MAX_SQL_IDENTIFIER_LEN];
				SSHORT owner_name_null;
			} rel_data;

			while (!EXE_receive(tdbb, handle2, 1, sizeof(rel_data), reinterpret_cast<UCHAR*>(&rel_data)))
			{
	            if (!rel_data.owner_name_null) // !REL.RDB$OWNER_NAME.NULL
					dbb->dbb_owner = rel_data.RDB$OWNER_NAME;
				break; // FIRST 1
			}
		}
	}
	else
	{
		dbb->dbb_owner = user->getUserName();
		user->usr_privileges.load(INI_owner_privileges().c_str());
		user->usr_granted_roles.clear();
		user->usr_granted_roles.add("RDB$ADMIN");

		user->usr_flags &= ~USR_newrole;
	}
}


bool SCL_move_priv(SecurityClass::flags_t mask, Acl& acl)
{
/**************************************
 *
 *	S C L _ m o v e _ p r i v
 *
 **************************************
 *
 * Functional description
 *	Given a mask of privileges, move privileges types to acl.
 *
 **************************************/
	// Terminate identification criteria, and move privileges

	acl.push(ACL_end);
	acl.push(ACL_priv_list);

	bool rc = false;
	for (const P_NAMES* priv = p_names; priv->p_names_priv; priv++)
	{
		if (mask & priv->p_names_priv)
		{
			fb_assert(priv->p_names_acl <= MAX_UCHAR);
			acl.push(priv->p_names_acl);
			rc = true;
		}
	}

	acl.push(0);
	return rc;
}


void SCL_clear_classes(thread_db* tdbb, const MetaName& name)
{
/**************************************
 *
 *	S C L _ c l e a r _ c l a s s e s
 *
 **************************************
 *
 * Functional description
 *	Something changed with a security class, remove them from cache.
 *	We need to clear security classes for every user.
 *
 **************************************/
	SET_TDBB(tdbb);

	Attachment* attachment = tdbb->getAttachment();

	SecurityClassList* list = attachment->att_security_classes;
	if (!list)
		return;

	MetaNamePair key(name, {});
	if (!list->locate(ScratchBird::locGreatEqual, key))
		return;

	while (list->current()->sclClassUser.first == name)
	{
		delete list->current();
		if (!list->fastRemove())	// removing moves current item to the next one
			break;
	}
}


void SCL_release_all(SecurityClassList*& list)
{
/**************************************
 *
 *	S C L _ r e l e a s e _ a l l
 *
 **************************************
 *
 * Functional description
 *	Release all security classes.
 *
 **************************************/
	if (!list)
		return;

	if (list->getFirst())
	{
		do {
			delete list->current();
		} while (list->getNext());
	}

	delete list;
	list = NULL;
}


SecurityClass::flags_t SCL_get_object_mask(ObjectType object_type, const MetaName& schema)
{
/**************************************
 *
 *	S C L _ g e t _ o b j e c t _ m a s k
 *
 **************************************
 *
 *	Functional description
 *	Get a protection mask for database object.
 *
 **************************************/
	thread_db* tdbb = JRD_get_thread_data();
	Database* dbb = tdbb->getDatabase();

	const auto securityClass = SCL_getDdlSecurityClassName(object_type, schema);

	if (const auto* const secClass = SCL_get_class(tdbb, securityClass))
		return secClass->scl_flags;

	return -1 & ~SCL_corrupt;
}


ULONG SCL_get_number(const UCHAR* acl)
{
/**************************************
 *
 *	g e t _ n u m b e r
 *
 **************************************
 *
 * Functional description
 *	Get value of acl numeric string.
 *
 **************************************/
	ULONG n = 0;
	USHORT l = *acl++;
	if (l)
	{
		do {
			n = n * UIC_BASE + *acl++ - '0';
		} while (--l);
	}

	return n;
}


static bool check_number(const UCHAR* acl, USHORT number)
{
/**************************************
 *
 *	c h e c k _ n u m b e r
 *
 **************************************
 *
 * Functional description
 *	Check a string against and acl numeric string.  If they don't match,
 *	return true.
 *
 **************************************/
	return (SCL_get_number(acl) != number);
}


static bool check_user_group(thread_db* tdbb, const UCHAR* acl, USHORT number)
{
/**************************************
 *
 *	c h e c k _ u s e r _ g r o u p
 *
 **************************************
 *
 * Functional description
 *
 *	Check a string against an acl numeric string.
 *
 * logic:
 *
 *  If the string contains user group name,
 *    then
 *      converts user group name to numeric user group id.
 *    else
 *      converts character user group id to numeric user group id.
 *
 *	Check numeric user group id against an acl numeric string.
 *  If they don't match, return true.
 *
 **************************************/
	SET_TDBB(tdbb);

	ULONG n = 0;

	USHORT l = *acl++;
	if (l)
	{
		if (isdigit(*acl))	// this is a group id
		{
			do {
				n = n * UIC_BASE + *acl++ - '0';
			} while (--l);
		}
		else				// processing group name
		{
			ScratchBird::string user_group_name;
			do {
				const TEXT one_char = *acl++;
				user_group_name += LOWWER(one_char);
			} while (--l);

			// convert unix group name to unix group id
			n = os_utils::get_user_group_id(user_group_name.c_str());
		}
	}

	return (n != number);
}


static bool check_string(const UCHAR* acl, const MetaName& string)
{
/**************************************
 *
 *	c h e c k _ s t r i n g
 *
 **************************************
 *
 * Functional description
 *	Check a string against and acl string.  If they don't match,
 *	return true.
 *
 **************************************/
	fb_assert(acl);

	const FB_SIZE_T length = *acl++;
	const TEXT* const ptr = (TEXT*) acl;

	return (string.compare(ptr, length) != 0);
}

static void get_string(const UCHAR* acl, MetaName& string)
{
/**************************************
 *
 *	g e t _ s t r i n g
 *
 **************************************
 *
 * Functional description
 *	Get a string from acl string.
 *
 **************************************/
	fb_assert(acl);

	const FB_SIZE_T length = *acl++;
	const TEXT* const ptr = (TEXT*) acl;
	string.assign(ptr, length);
}

static SecurityClass::flags_t get_sys_privileges(thread_db* tdbb)
{
/**************************************
 *
 *	g e t _ s y s _ p r i v i l e g e s
 *
 **************************************
 *
 * Functional description
 *	Returns access flags for current user's system-wide privileges
 *
 **************************************/
	const Jrd::Attachment* attachment = tdbb->getAttachment();
	if (!attachment)
		return 0;

	SecurityClass::flags_t flags = 0;

	if (attachment->locksmith(tdbb, ACCESS_ANY_OBJECT_IN_DATABASE))
		flags |= SCL_ACCESS_ANY;
	else if (attachment->locksmith(tdbb, SELECT_ANY_OBJECT_IN_DATABASE))
		flags |= SCL_SELECT_ANY;

	if (attachment->locksmith(tdbb, MODIFY_ANY_OBJECT_IN_DATABASE))
		flags |= SCL_MODIFY_ANY;

	return flags;
}

static SecurityClass::flags_t compute_access(thread_db* tdbb,
											 const SecurityClass* s_class,
											 SLONG obj_type,
											 const QualifiedName& obj_name)
{
/**************************************
 *
 *	c o m p u t e _ a c c e s s
 *
 **************************************
 *
 * Functional description
 *	Compute access for security class.  If a relation block is
 *	present, it is a view, and we should check for enhanced view
 *	access permissions.  Return a flag word of recognized privileges.
 *
 **************************************/
	Acl acl;

	SET_TDBB(tdbb);
	Jrd::Attachment* const attachment = tdbb->getAttachment();
	jrd_tra* sysTransaction = attachment->getSysTransaction();

	SecurityClass::flags_t privileges = 0;
	SecurityClass::flags_t sysPriv = SCL_exists | get_sys_privileges(tdbb);

	AutoCacheRequest request(tdbb, irq_l_security, IRQ_REQUESTS);

	// Converted FOR loop #21: FOR(REQUEST_HANDLE request) X IN RDB$SECURITY_CLASSES
	jrd_req* handle = request;
	EXE_start(tdbb, handle, attachment->getSysTransaction());
	
	struct {
		TEXT security_class[MAX_SQL_IDENTIFIER_LEN];
	} sec_input;
	
	strcpy(sec_input.security_class, s_class->sclClassUser.first.c_str());
	
	EXE_send(tdbb, handle, 0, sizeof(sec_input), reinterpret_cast<UCHAR*>(&sec_input));

	struct {
		ISC_QUAD RDB$ACL;
	} sec_data;

	while (!EXE_receive(tdbb, handle, 1, sizeof(sec_data), reinterpret_cast<UCHAR*>(&sec_data)))
	{
		privileges |= sysPriv;
		blb* blob = blb::open(tdbb, sysTransaction, &sec_data.RDB$ACL);
		UCHAR* buffer = acl.getBuffer(ACL_BLOB_BUFFER_SIZE);
		UCHAR* end = buffer;
		while (true)
		{
			end += blob->BLB_get_segment(tdbb, end, (USHORT) (acl.getCount() - (end - buffer)) );
			if (blob->blb_flags & BLB_eof)
				break;

			// There was not enough space, realloc point acl to the correct location

			if (blob->getFragmentSize())
			{
				const ptrdiff_t old_offset = end - buffer;
				buffer = acl.getBuffer(acl.getCount() + ACL_BLOB_BUFFER_SIZE);
				end = buffer + old_offset;
			}
		}
		blob->BLB_close(tdbb);
		blob = NULL;
		acl.shrink(end - buffer);

		if (acl.getCount() > 0)
			privileges |= walk_acl(tdbb, acl, s_class->sclClassUser.second, obj_type, obj_name);
	}

	return privileges;
}


static SecurityClass::flags_t walk_acl(thread_db* tdbb,
									   const Acl& acl,
									   const MetaName& userName,
									   SLONG obj_type,
									   const QualifiedName& obj_name)
{
/**************************************
 *
 *	w a l k _ a c l
 *
 **************************************
 *
 * Functional description
 *	Walk an access control list looking for a hit.  If a hit
 *	is found, return privileges.
 * userName a name of user which privileges must be computed. NULL is current user (INVOKER)
 *
 **************************************/
	SET_TDBB(tdbb);
	Jrd::Attachment* const attachment = tdbb->getAttachment();

	// Munch ACL. If we find a hit, eat up privileges.

	UserId user;
	if (userName.hasData())
		user = *attachment->getUserId(userName);

	SecurityClass::flags_t privilege = 0;
	const UCHAR* a = acl.begin();

	if (*a++ != ACL_version)
	{
		BUGCHECK(160);	// msg 160 wrong ACL version
	}

	const TEXT* p;
	bool hit = false;
	UCHAR c;

	while ( (c = *a++) )
	{
		switch (c)
		{
		case ACL_id_list:
			hit = true;
			while ( (c = *a++) )
			{
				switch (c)
				{
				case id_person:
					if (!(p = user.getUserName().nullStr()) || check_string(a, p))
						hit = false;
					break;

				case id_project:
					if (!(p = user.usr_project_name.nullStr()) || check_string(a, p))
						hit = false;
					break;

				case id_organization:
					if (!(p = user.usr_org_name.nullStr()) || check_string(a, p))
						hit = false;
					break;

				case id_group:
					if (check_user_group(tdbb, a, user.usr_group_id))
						hit = false;
					break;

				case id_sql_role:
				{
					MetaName role_name;
					get_string(a, role_name);
					if (!user.roleInUse(tdbb, role_name))
						hit = false;
					break;
				}

				case id_view:
				case id_package:
				case id_procedure:
				case id_trigger:
				case id_function:
					if (c != obj_type)
						hit = false;

					if (check_string(a, obj_name.schema))
						hit = false;
					a += *a + 1;

					if (check_string(a, obj_name.object))
						hit = false;
					break;

				case id_views:
					// Disable this catch-all that messes up the view security.
					// Note that this id_views is not generated anymore, this code
					// is only here for compatibility. id_views was only
					// generated for SQL.

					hit = false;
					//if (!view)
					//	hit = false;
					break;

				case id_user:
					if (check_number(a, user.usr_user_id))
						hit = false;
					break;

				case id_privilege:
					if (!user.locksmith(tdbb, SCL_get_number(a)))
						hit = false;
					break;

				case id_node:
					break;

				default:
					return SCL_corrupt;
				}

				a += *a + 1;
			}
			break;

		case ACL_priv_list:
			if (hit)
			{
				while ( (c = *a++) )
				{
					switch (c)
					{
					case priv_control:
						privilege |= SCL_control;
						break;

					case priv_select:
						// Note that SELECT access must imply REFERENCES
						// access for upward compatibility of existing
						// security classes
						privilege |= SCL_select | SCL_references;
						break;

					case priv_insert:
						privilege |= SCL_insert;
						break;

					case priv_delete:
						privilege |= SCL_delete;
						break;

					case priv_references:
						privilege |= SCL_references;
						break;

					case priv_update:
						privilege |= SCL_update;
						break;

					case priv_drop:
						privilege |= SCL_drop;
						break;

					case priv_alter:
						privilege |= SCL_alter;
						break;

					case priv_execute:
						privilege |= SCL_execute;
						break;

					case priv_usage:
						privilege |= SCL_usage;
						break;

					case priv_write:
						// unused, but supported for backward compatibility
						privilege |= SCL_insert | SCL_update | SCL_delete;
						break;

					case priv_grant:
						// unused
						break;

					case priv_create:
						privilege |= SCL_create;
						break;

					default:
						return SCL_corrupt;
					}
				}

				// For a relation the first hit does not give the privilege.
				// Because, there could be some permissions for the table
				// (for user1) and some permissions for a column on that
				// table for public/user2, causing two hits.
				// Hence, we do not return at this point.
				// -- Madhukar Thakur (May 1, 1995)
			}
			else
				while (*a++);
			break;

		default:
			return SCL_corrupt;
		}
	}

	fb_assert(a == acl.end());

	return privilege;
}

void UserId::populateDpb(ScratchBird::ClumpletWriter& dpb, bool embeddedSupport)
{
	if (usr_auth_block.hasData())
		dpb.insertBytes(isc_dpb_auth_block, usr_auth_block.begin(), usr_auth_block.getCount());
	else if (embeddedSupport)
		dpb.insertString(isc_dpb_user_name, usr_user_name);

	if (usr_sql_role_name.hasData() && usr_sql_role_name != NULL_ROLE && !dpb.find(isc_dpb_sql_role_name))
		dpb.insertString(isc_dpb_sql_role_name, usr_sql_role_name);
}

void UserId::makeRoleName(ScratchBird::MetaString& role, const int dialect)
{
	if (role.isEmpty())
		return;

	switch (dialect)
	{
	case SQL_DIALECT_V5:
		// Invoke utility twice: first to strip quotes, next to uppercase if needed
		// For unquoted string nothing bad happens
		fb_utils::dpbItemUpper(role);
		{
			ScratchBird::string tmp(role);
			tmp.upper();
			role = tmp;
		}
		break;

	case SQL_DIALECT_V6_TRANSITION:
	case SQL_DIALECT_V6:
		fb_utils::dpbItemUpper(role);
		break;

	default:
		break;
	}
}

// get privilege bit by name
USHORT SCL_convert_privilege(thread_db* tdbb, jrd_tra* transaction, const ScratchBird::string& priv)
{
	static GlobalPtr<Mutex> privCacheMutex;
	static bool cacheFlag = false;
	typedef NonPooled<MetaString, USHORT> CachedPriv;
	static GlobalPtr<GenericMap<CachedPriv> > privCache;

	if (!cacheFlag)
	{
		MutexLockGuard g(privCacheMutex, FB_FUNCTION);

		if (!cacheFlag)
		{
			privCache->clear();

			AutoCacheRequest request(tdbb, irq_get_priv_bit, IRQ_REQUESTS);
			// Converted FOR loop #22: FOR(REQUEST_HANDLE request TRANSACTION_HANDLE transaction) T IN RDB$TYPES
			jrd_req* handle = request;
			EXE_start(tdbb, handle, transaction);
			
			struct {
				TEXT field_name[MAX_SQL_IDENTIFIER_LEN];
			} type_input;
			
			strcpy(type_input.field_name, "RDB$SYSTEM_PRIVILEGES");
			
			EXE_send(tdbb, handle, 0, sizeof(type_input), reinterpret_cast<UCHAR*>(&type_input));

			struct {
				TEXT RDB$TYPE_NAME[MAX_SQL_IDENTIFIER_LEN];
				SSHORT RDB$TYPE;
			} type_data;

			while (!EXE_receive(tdbb, handle, 1, sizeof(type_data), reinterpret_cast<UCHAR*>(&type_data)))
			{
				privCache->put(type_data.RDB$TYPE_NAME, type_data.RDB$TYPE);
			}

			cacheFlag = true;
		}
	}

	USHORT rc;
	if (!privCache->get(priv, rc))
		(Arg::Gds(isc_wrong_prvlg) << priv).raise();

	return rc;
}

// check object existence and access rights
static bool check_object(thread_db* tdbb,
						 bool found,
						 const SecurityClass* s_class,
						 SLONG obj_type,
						 const QualifiedName& obj_name,
						 SecurityClass::flags_t mask,
						 ObjectType type,
						 const QualifiedName& name)
{
	if (s_class)
	{
		SCL_check_access(tdbb, s_class, obj_type, obj_name, mask, type, false, name);
		return true;
	}
	return found;
}