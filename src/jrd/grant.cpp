/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		grant.cpp
 *	DESCRIPTION:	SQL Grant/Revoke Handler
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
 *
 * 2002.10.29 Sean Leyne - Removed obsolete "Netware" port
 *
 */

#include "scratchbird.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "../jrd/jrd.h"
#include "../jrd/scl.h"
#include "../jrd/acl.h"
#include "../jrd/irq.h"
#include "../jrd/blb.h"
#include "../jrd/btr.h"
#include "../jrd/req.h"  
#include "../jrd/tra.h"
#include "../jrd/val.h"
#include "../jrd/met.h"
#include "../jrd/intl.h"
#include "../jrd/blb_proto.h"
#include "../jrd/cmp_proto.h"
#include "../jrd/dfw_proto.h"
#include "../jrd/dpm_proto.h"
#include "../jrd/err_proto.h"
#include "../jrd/exe_proto.h"
#include "../yvalve/gds_proto.h"
#include "../jrd/grant_proto.h"
#include "../jrd/jrd_proto.h"
#include "../jrd/met_proto.h"
#include "../jrd/scl_proto.h"
#include "../common/utils_proto.h"
#include "../common/classes/array.h"
#include "../jrd/constants.h"

using namespace ScratchBird;
using namespace Jrd;

// privileges given to the owner of a relation

inline void CHECK_AND_MOVE(Acl& to, UCHAR from)
{
	to.add(from);
}

// Replaced GPRE DATABASE DB = STATIC "yachts.lnk"; with modern approach
// Database access is handled through existing attachment mechanisms

// Constants for GPRE conversion
#ifndef MAX_SQL_IDENTIFIER_LEN
#define MAX_SQL_IDENTIFIER_LEN 68
#endif

static void define_default_class(thread_db*, const QualifiedName&, MetaName&, const Acl&,
							jrd_tra*);
static void finish_security_class(Acl&, SecurityClass::flags_t);
static void get_object_info(thread_db*, const QualifiedName&, ObjectType,
							MetaName&, MetaName&, MetaName&, bool&);
static SecurityClass::flags_t get_public_privs(thread_db*, const QualifiedName&, SSHORT);
static void get_user_privs(thread_db*, Acl&, const QualifiedName&, SSHORT, const MetaName&,
							SecurityClass::flags_t);
static void grant_user(Acl&, const QualifiedName&, SSHORT, SecurityClass::flags_t);
static SecurityClass::flags_t save_field_privileges(thread_db*, Acl&, const QualifiedName&,
							const MetaName&, SecurityClass::flags_t, jrd_tra*);
static void save_security_class(thread_db*, const MetaName&, const Acl&, jrd_tra*);
static SecurityClass::flags_t trans_sql_priv(const TEXT*);
static SecurityClass::flags_t squeeze_acl(Acl&, const QualifiedName&, SSHORT);
static bool check_string(const UCHAR*, const MetaName&);


void GRANT_privileges(thread_db* tdbb, const QualifiedName& name, ObjectType id, jrd_tra* transaction)
{
/**************************************
 *
 *	G R A N T _ p r i v i l e g e s
 *
 **************************************
 *
 * Functional description
 *	Compute access control list from SQL privileges.
 *	This calculation is tricky and involves interaction between
 *	the relation-level and field-level privileges.  Do not change
 *	the order of operations	lightly.
 *
 **************************************/
	SET_TDBB(tdbb);

	bool restrct = false;

	MetaName s_class, default_class;
	MetaName owner;
	bool view; // unused after being retrieved.
	get_object_info(tdbb, name, id, owner, s_class, default_class, view);

	if (s_class.isEmpty())
		return;

	// start the acl off by giving the owner all privileges
	Acl acl, default_acl;

	CHECK_AND_MOVE(acl, ACL_version);

	SecurityClass::flags_t priv = SCL_control | SCL_drop | SCL_alter;

	switch (id)
	{
		case obj_relation:
			priv |= SCL_references;
		case obj_view:
			priv |= SCL_select | SCL_insert | SCL_update | SCL_delete;
			break;

		case obj_procedure:
		case obj_udf:
		case obj_package_header:
			priv |= SCL_execute;
			break;

		case obj_field:
		case obj_exception:
		case obj_generator:
		case obj_charset:
		case obj_collation:
		case obj_schema:
			priv |= SCL_usage;
			break;

		default:
			if (isDdlObject(id))
				priv = SCL_create | SCL_drop | SCL_alter;
			break;
	}

	grant_user(acl, QualifiedName(owner), obj_user, priv);

	// Pick up core privileges

	const SecurityClass::flags_t public_priv = get_public_privs(tdbb, name, id);
	get_user_privs(tdbb, acl, name, id, owner, public_priv);

	if (id == obj_relation)
	{
		// Now handle field-level privileges.  This might require adding
		// UPDATE privilege to the relation-level acl,  Therefore, save
		// off the relation acl because we need to add a default field
		// acl in that case.

		default_acl.assign(acl);

		const SecurityClass::flags_t aggregate_public =
			save_field_privileges(tdbb, acl, name, owner, public_priv, transaction);

		// finish off and store the security class for the relation

		finish_security_class(acl, aggregate_public);

		save_security_class(tdbb, s_class, acl, transaction);

		if (acl.getCount() != default_acl.getCount())	// relation privs were added?
			restrct = true;

		// if there have been privileges added at the relation level which
		// need to be restricted from other fields in the relation,
		// update the acl for them

		if (restrct)
		{
			finish_security_class(default_acl, public_priv);
			define_default_class(tdbb, name, default_class, default_acl, transaction);
		}
	}
	else
	{
		finish_security_class(acl, public_priv);
		save_security_class(tdbb, s_class, acl, transaction);
	}
}


static void define_default_class(thread_db* tdbb,
								 const QualifiedName& relationName,
								 MetaName& default_class,
								 const Acl& acl,
								 jrd_tra* transaction)
{
/**************************************
 *
 *	d e f i n e _ d e f a u l t _ c l a s s
 *
 **************************************
 *
 * Functional description
 *	Update the default security class for fields
 *	which have not been specifically granted
 *	any privileges.  We must grant them all
 *	privileges which were specifically granted
 *	at the relation level, but none of the
 *	privileges we added at the relation level
 *	for the purpose of accessing other fields.
 *
 **************************************/
	SET_TDBB(tdbb);

	if (default_class.isEmpty())
	{
		default_class.printf("%s%" SQUADFORMAT, DEFAULT_CLASS,
			DPM_gen_id(tdbb, MET_lookup_generator(tdbb, QualifiedName(DEFAULT_CLASS, SYSTEM_SCHEMA)), false, 1));

		AutoCacheRequest request(tdbb, irq_grant7, IRQ_REQUESTS);

		// Converted FOR loop #1: FOR(REQUEST_HANDLE request TRANSACTION_HANDLE transaction) REL IN RDB$RELATIONS
		jrd_req* handle = request;
		EXE_start(tdbb, handle, transaction);
		
		struct {
			TEXT schema_name[MAX_SQL_IDENTIFIER_LEN];
			TEXT relation_name[MAX_SQL_IDENTIFIER_LEN];
		} rel_input;
		
		strcpy(rel_input.schema_name, relationName.schema.c_str());
		strcpy(rel_input.relation_name, relationName.object.c_str());
		
		EXE_send(tdbb, handle, 0, sizeof(rel_input), reinterpret_cast<UCHAR*>(&rel_input));

		struct {
			TEXT RDB$DEFAULT_CLASS[MAX_SQL_IDENTIFIER_LEN];
		} rel_data;

		while (!EXE_receive(tdbb, handle, 1, sizeof(rel_data), reinterpret_cast<UCHAR*>(&rel_data)))
		{
			// Converted MODIFY operation #1: MODIFY REL USING
			AutoCacheRequest modify_request(tdbb, irq_modify_rel_default_class, IRQ_REQUESTS);
			jrd_req* modify_handle = modify_request;
			EXE_start(tdbb, modify_handle, transaction);
			
			struct {
				TEXT schema_name[MAX_SQL_IDENTIFIER_LEN];
				TEXT relation_name[MAX_SQL_IDENTIFIER_LEN];
				TEXT RDB$DEFAULT_CLASS[MAX_SQL_IDENTIFIER_LEN];
				SSHORT default_class_null;
			} modify_data;
			
			strcpy(modify_data.schema_name, relationName.schema.c_str());
			strcpy(modify_data.relation_name, relationName.object.c_str());
			
			modify_data.default_class_null = FALSE;
			jrd_vtof(default_class.c_str(), modify_data.RDB$DEFAULT_CLASS, sizeof(modify_data.RDB$DEFAULT_CLASS));
			
			EXE_send(tdbb, modify_handle, 0, sizeof(modify_data), reinterpret_cast<UCHAR*>(&modify_data));
		}
	}

	save_security_class(tdbb, default_class, acl, transaction);

	dsc schemaDesc, nameDesc;
	schemaDesc.makeText((USHORT) strlen(relationName.schema.c_str()), CS_METADATA, (UCHAR*) relationName.schema.c_str());
	nameDesc.makeText((USHORT) strlen(relationName.object.c_str()), CS_METADATA, (UCHAR*) relationName.object.c_str());
	DFW_post_work(transaction, dfw_scan_relation, &nameDesc, &schemaDesc, 0);
}


static void finish_security_class(Acl& acl, SecurityClass::flags_t public_priv)
{
/**************************************
 *
 *	f i n i s h _ s e c u r i t y _ c l a s s
 *
 **************************************
 *
 * Functional description
 *	Finish off a security class, putting
 *	in a wildcard for any public privileges.
 *
 **************************************/
	if (public_priv)
	{
		CHECK_AND_MOVE(acl, ACL_id_list);
		SCL_move_priv(public_priv, acl);
	}

	CHECK_AND_MOVE(acl, ACL_end);
}


static SecurityClass::flags_t get_public_privs(thread_db* tdbb,
											   const QualifiedName& object_name,
											   SSHORT obj_type)
{
/**************************************
 *
 *	g e t _ p u b l i c _ p r i v s
 *
 **************************************
 *
 * Functional description
 *	Get public privileges for a particular object.
 *
 **************************************/
	SET_TDBB(tdbb);
	Jrd::Attachment* attachment = tdbb->getAttachment();

	SecurityClass::flags_t public_priv = 0;

	AutoCacheRequest request(tdbb, irq_grant5, IRQ_REQUESTS);

	// Converted FOR loop #2: FOR(REQUEST_HANDLE request) PRV IN RDB$USER_PRIVILEGES
	jrd_req* handle = request;
	EXE_start(tdbb, handle, attachment->getSysTransaction());
	
	struct {
		TEXT schema_name[MAX_SQL_IDENTIFIER_LEN];
		TEXT relation_name[MAX_SQL_IDENTIFIER_LEN];
		SSHORT object_type;
		TEXT user_name[MAX_SQL_IDENTIFIER_LEN];
		SSHORT user_type;
		SSHORT schema_null;
		SSHORT relation_null;
	} prv_input;
	
	if (object_name.schema.hasData())
	{
		strcpy(prv_input.schema_name, object_name.schema.c_str());
		prv_input.schema_null = FALSE;
	}
	else
		prv_input.schema_null = TRUE;
		
	if (object_name.object.hasData())
	{
		strcpy(prv_input.relation_name, object_name.object.c_str());
		prv_input.relation_null = FALSE;
	}
	else
		prv_input.relation_null = TRUE;
		
	prv_input.object_type = obj_type;
	strcpy(prv_input.user_name, "PUBLIC");
	prv_input.user_type = obj_user;
	
	EXE_send(tdbb, handle, 0, sizeof(prv_input), reinterpret_cast<UCHAR*>(&prv_input));

	struct {
		TEXT RDB$PRIVILEGE[2];
		SSHORT field_name_null;
	} prv_data;

	while (!EXE_receive(tdbb, handle, 1, sizeof(prv_data), reinterpret_cast<UCHAR*>(&prv_data)))
	{
		if (prv_data.field_name_null) // PRV.RDB$FIELD_NAME MISSING
		{
			public_priv |= trans_sql_priv(prv_data.RDB$PRIVILEGE);
		}
	}

	return public_priv;
}


static void get_object_info(thread_db* tdbb,
							const QualifiedName& object_name,
							ObjectType obj_type,
							MetaName& owner,
							MetaName& s_class,
							MetaName& default_class,
							bool& view)
{
/**************************************
 *
 *	g e t _ o b j e c t _ i n f o
 *
 **************************************
 *
 * Functional description
 *	This could be done in MET_scan_relation () or MET_lookup_procedure,
 *	but presumably we wish to make sure the information we have is
 *	up-to-the-minute.
 *
 **************************************/
	SET_TDBB(tdbb);
	Jrd::Attachment* attachment = tdbb->getAttachment();

	owner = "";
	s_class = "";
	default_class = "";
	view = false;

	if (obj_type == obj_relation)
	{
		AutoCacheRequest request(tdbb, irq_grant1, IRQ_REQUESTS);

		// Converted FOR loop #3: FOR(REQUEST_HANDLE request) REL IN RDB$RELATIONS
		jrd_req* handle = request;
		EXE_start(tdbb, handle, attachment->getSysTransaction());
		
		struct {
			TEXT schema_name[MAX_SQL_IDENTIFIER_LEN];
			TEXT relation_name[MAX_SQL_IDENTIFIER_LEN];
		} rel_input;
		
		strcpy(rel_input.schema_name, object_name.schema.c_str());
		strcpy(rel_input.relation_name, object_name.object.c_str());
		
		EXE_send(tdbb, handle, 0, sizeof(rel_input), reinterpret_cast<UCHAR*>(&rel_input));

		struct {
			TEXT RDB$SECURITY_CLASS[MAX_SQL_IDENTIFIER_LEN];
			TEXT RDB$DEFAULT_CLASS[MAX_SQL_IDENTIFIER_LEN];
			TEXT RDB$OWNER_NAME[MAX_SQL_IDENTIFIER_LEN];
			ISC_QUAD RDB$VIEW_BLR;
			SSHORT security_class_null;
			SSHORT default_class_null;
			SSHORT view_blr_null;
		} rel_data;

		while (!EXE_receive(tdbb, handle, 1, sizeof(rel_data), reinterpret_cast<UCHAR*>(&rel_data)))
		{
			if (!rel_data.security_class_null)
				s_class = rel_data.RDB$SECURITY_CLASS;
			if (!rel_data.default_class_null)
				default_class = rel_data.RDB$DEFAULT_CLASS;
			owner = rel_data.RDB$OWNER_NAME;
			view = !rel_data.view_blr_null;
		}
	}
	else if (obj_type == obj_package_header)
	{
		AutoCacheRequest request(tdbb, irq_grant10, IRQ_REQUESTS);

		// Converted FOR loop #4: FOR (REQUEST_HANDLE request) PKG IN RDB$PACKAGES
		jrd_req* handle = request;
		EXE_start(tdbb, handle, attachment->getSysTransaction());
		
		struct {
			TEXT schema_name[MAX_SQL_IDENTIFIER_LEN];
			TEXT package_name[MAX_SQL_IDENTIFIER_LEN];
		} pkg_input;
		
		strcpy(pkg_input.schema_name, object_name.schema.c_str());
		strcpy(pkg_input.package_name, object_name.object.c_str());
		
		EXE_send(tdbb, handle, 0, sizeof(pkg_input), reinterpret_cast<UCHAR*>(&pkg_input));

		struct {
			TEXT RDB$SECURITY_CLASS[MAX_SQL_IDENTIFIER_LEN];
			TEXT RDB$OWNER_NAME[MAX_SQL_IDENTIFIER_LEN];
			SSHORT security_class_null;
		} pkg_data;

		while (!EXE_receive(tdbb, handle, 1, sizeof(pkg_data), reinterpret_cast<UCHAR*>(&pkg_data)))
		{
			if (!pkg_data.security_class_null)
				s_class = pkg_data.RDB$SECURITY_CLASS;
			default_class = "";
			owner = pkg_data.RDB$OWNER_NAME;
			view = false;
		}
	}
	else if (obj_type == obj_procedure)
	{
		AutoCacheRequest request(tdbb, irq_grant9, IRQ_REQUESTS);

		// Converted FOR loop #5: FOR(REQUEST_HANDLE request) PRC IN RDB$PROCEDURES
		jrd_req* handle = request;
		EXE_start(tdbb, handle, attachment->getSysTransaction());
		
		struct {
			TEXT schema_name[MAX_SQL_IDENTIFIER_LEN];
			TEXT procedure_name[MAX_SQL_IDENTIFIER_LEN];
		} prc_input;
		
		strcpy(prc_input.schema_name, object_name.schema.c_str());
		strcpy(prc_input.procedure_name, object_name.object.c_str());
		
		EXE_send(tdbb, handle, 0, sizeof(prc_input), reinterpret_cast<UCHAR*>(&prc_input));

		struct {
			TEXT RDB$SECURITY_CLASS[MAX_SQL_IDENTIFIER_LEN];
			TEXT RDB$OWNER_NAME[MAX_SQL_IDENTIFIER_LEN];
			SSHORT RDB$PACKAGE_NAME_NULL;
			SSHORT security_class_null;
		} prc_data;

		while (!EXE_receive(tdbb, handle, 1, sizeof(prc_data), reinterpret_cast<UCHAR*>(&prc_data)))
		{
			if (prc_data.RDB$PACKAGE_NAME_NULL) // PRC.RDB$PACKAGE_NAME MISSING
			{
				if (!prc_data.security_class_null)
					s_class = prc_data.RDB$SECURITY_CLASS;
				default_class = "";
				owner = prc_data.RDB$OWNER_NAME;
				view = false;
			}
		}
	}
	else if (obj_type == obj_udf)
	{
		AutoCacheRequest request(tdbb, irq_grant11, IRQ_REQUESTS);

		// Converted FOR loop #6: FOR(REQUEST_HANDLE request) FUN IN RDB$FUNCTIONS
		jrd_req* handle = request;
		EXE_start(tdbb, handle, attachment->getSysTransaction());
		
		struct {
			TEXT schema_name[MAX_SQL_IDENTIFIER_LEN];
			TEXT function_name[MAX_SQL_IDENTIFIER_LEN];
		} fun_input;
		
		strcpy(fun_input.schema_name, object_name.schema.c_str());
		strcpy(fun_input.function_name, object_name.object.c_str());
		
		EXE_send(tdbb, handle, 0, sizeof(fun_input), reinterpret_cast<UCHAR*>(&fun_input));

		struct {
			TEXT RDB$SECURITY_CLASS[MAX_SQL_IDENTIFIER_LEN];
			TEXT RDB$OWNER_NAME[MAX_SQL_IDENTIFIER_LEN];
			SSHORT RDB$PACKAGE_NAME_NULL;
			SSHORT security_class_null;
		} fun_data;

		while (!EXE_receive(tdbb, handle, 1, sizeof(fun_data), reinterpret_cast<UCHAR*>(&fun_data)))
		{
			if (fun_data.RDB$PACKAGE_NAME_NULL) // FUN.RDB$PACKAGE_NAME MISSING
			{
				if (!fun_data.security_class_null)
					s_class = fun_data.RDB$SECURITY_CLASS;
				default_class = "";
				owner = fun_data.RDB$OWNER_NAME;
				view = false;
			}
		}
	}
}


static void get_user_privs(thread_db* tdbb,
						   Acl& acl,
						   const QualifiedName& object_name,
						   SSHORT obj_type,
						   const MetaName& owner,
						   SecurityClass::flags_t public_priv)
{
/**************************************
 *
 *	g e t _ u s e r _ p r i v s
 *
 **************************************
 *
 * Functional description
 *	Get privileges for a particular object.
 *
 **************************************/
	SET_TDBB(tdbb);
	Jrd::Attachment* attachment = tdbb->getAttachment();

	QualifiedName user;
	SSHORT user_type = -2;
	SecurityClass::flags_t priv = 0;

	AutoCacheRequest request(tdbb, irq_grant2, IRQ_REQUESTS);

	// Converted FOR loop #7: FOR(REQUEST_HANDLE request) PRV IN RDB$USER_PRIVILEGES
	jrd_req* handle = request;
	EXE_start(tdbb, handle, attachment->getSysTransaction());
	
	struct {
		TEXT schema_name[MAX_SQL_IDENTIFIER_LEN];
		TEXT relation_name[MAX_SQL_IDENTIFIER_LEN];
		SSHORT object_type;
		TEXT owner_name[MAX_SQL_IDENTIFIER_LEN];
		SSHORT owner_type;
		SSHORT schema_null;
		SSHORT relation_null;
	} prv_input;
	
	if (object_name.schema.hasData())
	{
		strcpy(prv_input.schema_name, object_name.schema.c_str());
		prv_input.schema_null = FALSE;
	}
	else
		prv_input.schema_null = TRUE;
		
	if (object_name.object.hasData())
	{
		strcpy(prv_input.relation_name, object_name.object.c_str());
		prv_input.relation_null = FALSE;
	}
	else
		prv_input.relation_null = TRUE;
		
	prv_input.object_type = obj_type;
	strcpy(prv_input.owner_name, owner.c_str());
	prv_input.owner_type = obj_user;
	
	EXE_send(tdbb, handle, 0, sizeof(prv_input), reinterpret_cast<UCHAR*>(&prv_input));

	struct {
		TEXT RDB$USER[MAX_SQL_IDENTIFIER_LEN];
		TEXT RDB$USER_SCHEMA_NAME[MAX_SQL_IDENTIFIER_LEN];
		SSHORT RDB$USER_TYPE;
		TEXT RDB$PRIVILEGE[2];
		SSHORT user_schema_null;
	} prv_data;

	while (!EXE_receive(tdbb, handle, 1, sizeof(prv_data), reinterpret_cast<UCHAR*>(&prv_data)))
	{
		const QualifiedName thisUser(prv_data.RDB$USER, 
			prv_data.user_schema_null ? "" : prv_data.RDB$USER_SCHEMA_NAME);

		if (user != thisUser || user_type != prv_data.RDB$USER_TYPE)
		{
			if (user.object.hasData())
				grant_user(acl, user, user_type, priv);

			user_type = prv_data.RDB$USER_TYPE;

			if (user_type == obj_user)
				priv = public_priv;
			else
				priv = 0;

			user = thisUser;
		}

		priv |= trans_sql_priv(obj_type == obj_sql_role ? "O" : prv_data.RDB$PRIVILEGE);
	}

	if (user.object.length())
		grant_user(acl, user, user_type, priv);
}


static void grant_user(Acl& acl,
					   const QualifiedName& user,
					   SSHORT user_type,
					   SecurityClass::flags_t privs)
{
/**************************************
 *
 *	g r a n t _ u s e r
 *
 **************************************
 *
 * Functional description
 *	Grant privileges to a particular user.
 *
 **************************************/
	Acl::size_type back = acl.getCount();

	CHECK_AND_MOVE(acl, ACL_id_list);
	switch (user_type)
	{
	case obj_user_group:
		CHECK_AND_MOVE(acl, id_group);
		break;

	case obj_sql_role:
		CHECK_AND_MOVE(acl, id_sql_role);
		break;

	case obj_user:
		CHECK_AND_MOVE(acl, id_person);
		break;

	case obj_package_header:
		CHECK_AND_MOVE(acl, id_package);
		break;

	case obj_procedure:
		CHECK_AND_MOVE(acl, id_procedure);
		break;

	case obj_udf:
		CHECK_AND_MOVE(acl, id_function);
		break;

	case obj_trigger:
		CHECK_AND_MOVE(acl, id_trigger);
		break;

	case obj_view:
		CHECK_AND_MOVE(acl, id_view);
		break;

	case obj_privilege:
		CHECK_AND_MOVE(acl, id_privilege);
		fb_assert(isdigit(user.object[0]));
		break;

	default:
		BUGCHECK(292);			// Illegal user_type
	}

	switch (user_type)
	{
		case obj_package_header:
		case obj_procedure:
		case obj_udf:
		case obj_trigger:
		case obj_view:
		{
			fb_assert(user.schema.hasData());

			const UCHAR length = user.schema.length();
			CHECK_AND_MOVE(acl, length);

			if (length)
				acl.add(reinterpret_cast<const UCHAR*>(user.schema.c_str()), length);

			break;
		}
	}

	const UCHAR length = user.object.length();
	CHECK_AND_MOVE(acl, length);

	if (length)
		acl.add(reinterpret_cast<const UCHAR*>(user.object.c_str()), length);

	if (!SCL_move_priv(privs, acl))
		acl.shrink(back);
}


static SecurityClass::flags_t save_field_privileges(thread_db* tdbb,
													Acl& relation_acl,
													const QualifiedName& relation_name,
													const MetaName& owner,
													SecurityClass::flags_t public_priv,
													jrd_tra* transaction)
{
/**************************************
 *
 *	s a v e _ f i e l d _ p r i v i l e g e s
 *
 **************************************
 *
 * Functional description
 *	Compute the privileges for all fields within a relation.
 *	All fields must be given the initial relation-level privileges.
 *	Conversely, field-level privileges must be added to the relation
 *	security class to be effective.
 *
 **************************************/
	SET_TDBB(tdbb);
	Jrd::Attachment* attachment = tdbb->getAttachment();

	Acl field_acl(relation_acl);
	const Acl acl_start(relation_acl);

	QualifiedName user;
	MetaName field_name;
	MetaName s_class;
	SecurityClass::flags_t aggregate_public = public_priv;
	SecurityClass::flags_t priv = 0;
	SecurityClass::flags_t field_public = 0;
	SSHORT user_type = -1;

	AutoCacheRequest request(tdbb, irq_grant6, IRQ_REQUESTS);
	AutoRequest request2, request3;

	// Converted FOR loop #8: FOR(REQUEST_HANDLE request TRANSACTION_HANDLE transaction) FLD IN RDB$RELATION_FIELDS CROSS PRV IN RDB$USER_PRIVILEGES
	jrd_req* handle = request;
	EXE_start(tdbb, handle, transaction);
	
	struct {
		TEXT schema_name[MAX_SQL_IDENTIFIER_LEN];
		TEXT relation_name[MAX_SQL_IDENTIFIER_LEN];
		TEXT owner_name[MAX_SQL_IDENTIFIER_LEN];
	} fld_input;
	
	strcpy(fld_input.schema_name, relation_name.schema.c_str());
	strcpy(fld_input.relation_name, relation_name.object.c_str());
	strcpy(fld_input.owner_name, owner.c_str());
	
	EXE_send(tdbb, handle, 0, sizeof(fld_input), reinterpret_cast<UCHAR*>(&fld_input));

	struct {
		TEXT RDB$FIELD_NAME[MAX_SQL_IDENTIFIER_LEN];
		TEXT RDB$SECURITY_CLASS[MAX_SQL_IDENTIFIER_LEN];
		TEXT RDB$USER[MAX_SQL_IDENTIFIER_LEN];
		TEXT RDB$USER_SCHEMA_NAME[MAX_SQL_IDENTIFIER_LEN];
		SSHORT RDB$USER_TYPE;
		TEXT RDB$PRIVILEGE[2];
		SSHORT security_class_null;
		SSHORT user_schema_null;
	} fld_data;

	while (!EXE_receive(tdbb, handle, 1, sizeof(fld_data), reinterpret_cast<UCHAR*>(&fld_data)))
	{
		const QualifiedName thisUser(fld_data.RDB$USER, 
			fld_data.user_schema_null ? "" : fld_data.RDB$USER_SCHEMA_NAME);

		// create a control break on field_name,user
		if (user != thisUser || field_name != fld_data.RDB$FIELD_NAME)
		{
			// flush out information for old user
			if (user.object.hasData())
			{
				if (!(user.schema.isEmpty() && user.object == "PUBLIC"))
				{
					const SecurityClass::flags_t field_priv =
						public_priv | priv | squeeze_acl(field_acl, user, user_type);
					grant_user(field_acl, user, user_type, field_priv);

					const SecurityClass::flags_t relation_priv =
						public_priv | priv | squeeze_acl(relation_acl, user, user_type);
					grant_user(relation_acl, user, user_type, relation_priv);
				}
				else
					field_public = field_public | public_priv | priv;
			}

			// initialize for new user
			priv = 0;
			user = thisUser;
			user_type = fld_data.RDB$USER_TYPE;
		}

		// create a control break on field_name
		if (field_name != fld_data.RDB$FIELD_NAME)
		{
			// finish off the last field, adding a wildcard at end, giving PUBLIC
			// all privileges available at the table level as well as those
			// granted at the field level
			if (field_name.length())
			{
				aggregate_public |= field_public;
				finish_security_class(field_acl, (field_public | public_priv));
				save_security_class(tdbb, s_class, field_acl, transaction);
			}

			// initialize for new field
			field_name = fld_data.RDB$FIELD_NAME;
			s_class = fld_data.RDB$SECURITY_CLASS;

			if (fld_data.security_class_null || s_class.isEmpty())
			{
				bool unique = false;

				// Converted FOR loop #9: FOR(REQUEST_HANDLE request2 TRANSACTION_HANDLE transaction) RFR IN RDB$RELATION_FIELDS
				AutoCacheRequest modify_request(tdbb, irq_modify_field_security, IRQ_REQUESTS);
				jrd_req* modify_handle = modify_request;
				EXE_start(tdbb, modify_handle, transaction);
				
				struct {
					TEXT schema_name[MAX_SQL_IDENTIFIER_LEN];
					TEXT relation_name[MAX_SQL_IDENTIFIER_LEN];
					TEXT field_name[MAX_SQL_IDENTIFIER_LEN];
				} rfr_input;
				
				strcpy(rfr_input.schema_name, fld_data.RDB$FIELD_NAME); // Field schema
				strcpy(rfr_input.relation_name, relation_name.object.c_str());
				strcpy(rfr_input.field_name, fld_data.RDB$FIELD_NAME);
				
				EXE_send(tdbb, modify_handle, 0, sizeof(rfr_input), reinterpret_cast<UCHAR*>(&rfr_input));

				struct {
					TEXT RDB$SECURITY_CLASS[MAX_SQL_IDENTIFIER_LEN];
				} rfr_data;

				while (!EXE_receive(tdbb, modify_handle, 1, sizeof(rfr_data), reinterpret_cast<UCHAR*>(&rfr_data)))
				{
					// Converted MODIFY operation #2: MODIFY RFR
					AutoCacheRequest modify_update_request(tdbb, irq_update_field_security, IRQ_REQUESTS);
					jrd_req* update_handle = modify_update_request;
					EXE_start(tdbb, update_handle, transaction);
					
					struct {
						TEXT schema_name[MAX_SQL_IDENTIFIER_LEN];
						TEXT relation_name[MAX_SQL_IDENTIFIER_LEN];
						TEXT field_name[MAX_SQL_IDENTIFIER_LEN];
						TEXT RDB$SECURITY_CLASS[MAX_SQL_IDENTIFIER_LEN];
						SSHORT security_class_null;
					} update_data;
					
					while (!unique)
					{
						snprintf(update_data.RDB$SECURITY_CLASS, sizeof(update_data.RDB$SECURITY_CLASS),
							"%s%" SQUADFORMAT, SQL_FLD_SECCLASS_PREFIX,
							DPM_gen_id(tdbb,
								MET_lookup_generator(tdbb, QualifiedName(SQL_SECCLASS_GENERATOR, SYSTEM_SCHEMA)),
								false, 1));

						unique = true;
						
						// Converted FOR loop #10: FOR (REQUEST_HANDLE request3) RFR2 IN RDB$RELATION_FIELDS
						AutoCacheRequest check_request(tdbb, irq_check_field_security, IRQ_REQUESTS);
						jrd_req* check_handle = check_request;
						EXE_start(tdbb, check_handle, attachment->getSysTransaction());
						
						struct {
							TEXT schema_name[MAX_SQL_IDENTIFIER_LEN];
							TEXT security_class[MAX_SQL_IDENTIFIER_LEN];
						} check_input;
						
						strcpy(check_input.schema_name, rfr_input.schema_name);
						strcpy(check_input.security_class, update_data.RDB$SECURITY_CLASS);
						
						EXE_send(tdbb, check_handle, 0, sizeof(check_input), reinterpret_cast<UCHAR*>(&check_input));

						struct {
							TEXT RDB$SECURITY_CLASS[MAX_SQL_IDENTIFIER_LEN];
						} check_data;

						while (!EXE_receive(tdbb, check_handle, 1, sizeof(check_data), reinterpret_cast<UCHAR*>(&check_data)))
						{
							unique = false;
						}
					}

					strcpy(update_data.schema_name, rfr_input.schema_name);
					strcpy(update_data.relation_name, rfr_input.relation_name);
					strcpy(update_data.field_name, rfr_input.field_name);
					update_data.security_class_null = FALSE;
					s_class = update_data.RDB$SECURITY_CLASS;
					
					EXE_send(tdbb, update_handle, 0, sizeof(update_data), reinterpret_cast<UCHAR*>(&update_data));
				}
			}

			field_public = 0;

			// restart a security class at the end of the relation-level privs
			field_acl.assign(acl_start);
		}

		priv |= trans_sql_priv(fld_data.RDB$PRIVILEGE);
	}

	// flush out the last user's info
	if (user.object.hasData())
	{
		if (!(user.schema.isEmpty() && user.object == "PUBLIC"))
		{
			const SecurityClass::flags_t field_priv =
				public_priv | priv | squeeze_acl(field_acl, user, user_type);
			grant_user(field_acl, user, user_type, field_priv);

			const SecurityClass::flags_t relation_priv =
				public_priv | priv | squeeze_acl(relation_acl, user, user_type);
			grant_user(relation_acl, user, user_type, relation_priv);
		}
		else
		{
			field_public = field_public | public_priv | priv;
		}
	}

	// flush out the last field's info, and schedule a format update
	if (field_name.length())
	{
		aggregate_public |= field_public;
		finish_security_class(field_acl, (field_public | public_priv));
		save_security_class(tdbb, s_class, field_acl, transaction);

		dsc schemaDesc, nameDesc;
		schemaDesc.makeText((USHORT) strlen(relation_name.schema.c_str()), CS_METADATA,
			(UCHAR*) relation_name.schema.c_str());
		nameDesc.makeText((USHORT) strlen(relation_name.object.c_str()), CS_METADATA,
			(UCHAR*) relation_name.object.c_str());
		DFW_post_work(transaction, dfw_update_format, &nameDesc, &schemaDesc, 0);
	}

	return aggregate_public;
}


static void save_security_class(thread_db* tdbb,
								const MetaName& s_class,
								const Acl& acl,
								jrd_tra* transaction)
{
/**************************************
 *
 *	s a v e _ s e c u r i t y _ c l a s s
 *
 **************************************
 *
 * Functional description
 *	Store or update the named security class.
 *
 **************************************/
	SET_TDBB(tdbb);

	bid blob_id;
	blb* blob = blb::create(tdbb, transaction, &blob_id);
	size_t length = acl.getCount();
	const UCHAR* buffer = acl.begin();
	while (length)
	{
		const size_t step = length > ACL_BLOB_BUFFER_SIZE ? ACL_BLOB_BUFFER_SIZE : length;
		blob->BLB_put_segment(tdbb, buffer, static_cast<USHORT>(step));
		length -= step;
		buffer += step;
	}
	blob->BLB_close(tdbb);

	AutoCacheRequest request(tdbb, irq_grant3, IRQ_REQUESTS);

	bool found = false;
	
	// Converted FOR loop #11: FOR(REQUEST_HANDLE request TRANSACTION_HANDLE transaction) CLS IN RDB$SECURITY_CLASSES
	jrd_req* handle = request;
	EXE_start(tdbb, handle, transaction);
	
	struct {
		TEXT security_class[MAX_SQL_IDENTIFIER_LEN];
	} cls_input;
	
	strcpy(cls_input.security_class, s_class.c_str());
	
	EXE_send(tdbb, handle, 0, sizeof(cls_input), reinterpret_cast<UCHAR*>(&cls_input));

	struct {
		ISC_QUAD RDB$ACL;
	} cls_data;

	while (!EXE_receive(tdbb, handle, 1, sizeof(cls_data), reinterpret_cast<UCHAR*>(&cls_data)))
	{
		found = true;
		
		// Converted MODIFY operation #3: MODIFY CLS
		AutoCacheRequest modify_request(tdbb, irq_modify_security_class, IRQ_REQUESTS);
		jrd_req* modify_handle = modify_request;
		EXE_start(tdbb, modify_handle, transaction);
		
		struct {
			TEXT security_class[MAX_SQL_IDENTIFIER_LEN];
			ISC_QUAD RDB$ACL;
		} modify_data;
		
		strcpy(modify_data.security_class, s_class.c_str());
		modify_data.RDB$ACL = blob_id;
		
		EXE_send(tdbb, modify_handle, 0, sizeof(modify_data), reinterpret_cast<UCHAR*>(&modify_data));
	}

	if (!found)
	{
		request.reset(tdbb, irq_grant4, IRQ_REQUESTS);

		// Converted STORE operation: STORE(REQUEST_HANDLE request TRANSACTION_HANDLE transaction) CLS IN RDB$SECURITY_CLASSES
		jrd_req* store_handle = request;
		EXE_start(tdbb, store_handle, transaction);
		
		struct {
			TEXT RDB$SECURITY_CLASS[MAX_SQL_IDENTIFIER_LEN];
			ISC_QUAD RDB$ACL;
		} store_data;
		
		jrd_vtof(s_class.c_str(), store_data.RDB$SECURITY_CLASS, sizeof(store_data.RDB$SECURITY_CLASS));
		store_data.RDB$ACL = blob_id;
		
		EXE_send(tdbb, store_handle, 0, sizeof(store_data), reinterpret_cast<UCHAR*>(&store_data));
	}
}


static SecurityClass::flags_t trans_sql_priv(const TEXT* privileges)
{
/**************************************
 *
 *	t r a n s _ s q l _ p r i v
 *
 **************************************
 *
 * Functional description
 *	Map a SQL privilege letter into an internal privilege bit.
 *
 **************************************/
	SecurityClass::flags_t priv = 0;

	switch (UPPER7(privileges[0]))
	{
	case 'S':
		priv |= SCL_select;
		break;
	case 'I':
		priv |= SCL_insert;
		break;
	case 'U':
		priv |= SCL_update;
		break;
	case 'D':
		priv |= SCL_delete;
		break;
	case 'R':
		priv |= SCL_references;
		break;
	case 'X':
		priv |= SCL_execute;
		break;
	case 'G':
		priv |= SCL_usage;
		break;
	case 'C':
		priv |= SCL_create;
		break;
	case 'L':
		priv |= SCL_alter;
		break;
	case 'O':
		priv |= SCL_drop;
		break;
	}

	return priv;
}


static SecurityClass::flags_t squeeze_acl(Acl& acl, const QualifiedName& user, SSHORT user_type)
{
/**************************************
 *
 *	s q u e e z e _ a c l
 *
 **************************************
 *
 * Functional description
 *	Walk an access control list looking for a hit.  If a hit
 *	is found, return privileges and squeeze out that acl-element.
 *	The caller will use the returned privilege to insert a new
 *	privilege for the input user.
 *
 **************************************/
	UCHAR* dup_acl = NULL;
	SecurityClass::flags_t privilege = 0;
	UCHAR c;

	// Make sure that this half-finished acl looks good enough to process.
	acl.push(0);

	UCHAR* a = acl.begin();

	if (*a++ != ACL_version)
		BUGCHECK(160);			// msg 160 wrong ACL version

	bool hit = false;

	while ( (c = *a++) )
	{
		switch (c)
		{
		case ACL_id_list:
			dup_acl = a - 1;
			hit = true;
			while ( (c = *a++) )
			{
				switch (c)
				{
				case id_person:
					if (user_type != obj_user)
						hit = false;
					if (check_string(a, user.object))
						hit = false;
					break;

				case id_sql_role:
					if (user_type != obj_sql_role)
						hit = false;
					if (check_string(a, user.object))
						hit = false;
					break;

				case id_view:
					if (user_type != obj_view)
						hit = false;

					if (check_string(a, user.schema))
						hit = false;
					a += *a + 1;

					if (check_string(a, user.object))
						hit = false;
					break;

				case id_procedure:
					if (user_type != obj_procedure)
						hit = false;

					if (check_string(a, user.schema))
						hit = false;
					a += *a + 1;

					if (check_string(a, user.object))
						hit = false;
					break;

				case id_function:
					if (user_type != obj_udf)
						hit = false;

					if (check_string(a, user.schema))
						hit = false;
					a += *a + 1;

					if (check_string(a, user.object))
						hit = false;
					break;

				case id_trigger:
					if (user_type != obj_trigger)
						hit = false;

					if (check_string(a, user.schema))
						hit = false;
					a += *a + 1;

					if (check_string(a, user.object))
						hit = false;
					break;

				case id_project:
				case id_organization:
					hit = false;
					// CVC: What's the idea of calling a function whose only
					// result is boolean without checking it?
					check_string(a, user.object);
					break;

				case id_views:
					hit = false;
					break;

				case id_node:
				case id_user:
					{
						hit = false;
						// Seems strange with the same increment just after the switch.
						a += *a + 1;
					}
					break;

				case id_group:
					if (user_type != obj_user_group)
						hit = false;
					if (check_string(a, user.object))
						hit = false;
					break;

				case id_privilege:
					if (user_type != obj_privilege)
						hit = false;
					if (check_string(a, user.object))
						hit = false;
					break;

				default:
					BUGCHECK(293);	// bad ACL
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
						privilege |= SCL_select;
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

					default:
						BUGCHECK(293);	// bad ACL
					}
				}

				// Squeeze out duplicate acl element.
				fb_assert(dup_acl);
				acl.remove(dup_acl, a);
				a = dup_acl;
			}
			else
				while (*a++);
			break;

		default:
			BUGCHECK(293);		// bad ACL
		}
	}

	// remove added extra '\0' byte
    acl.pop();

	return privilege;
}


static bool check_string(const UCHAR* acl, const MetaName& name)
{
/**************************************
 *
 *      c h e c k _ s t r i n g
 *
 **************************************
 *
 * Functional description
 *      Check a string against an acl string.  If they don't match,
 *      return true.
 *
 **************************************/
	// JPN: Since Kanji User names are not allowed, No need to fix this UPPER loop.

	USHORT l = *acl++;
	const TEXT* string = name.c_str();
	if (l)
	{
		do
		{
			const UCHAR c1 = *acl++;
			const TEXT c2 = *string++;
			if (UPPER7(c1) != UPPER7(c2))
			{
				return true;
			}
		} while (--l);
	}

	return (*string && *string != ' ') ? true : false;
}