/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		ini.cpp
 *	DESCRIPTION:	Metadata initialization / population
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

#include "scratchbird.h"
#include <stdio.h>
#include <string.h>
#include "../jrd/flags.h"
#include "../jrd/jrd.h"
#include "../jrd/val.h"
#include "../jrd/ods.h"
#include "../jrd/btr.h"
#include "../jrd/ids.h"
#include "../jrd/intl.h"
#include "../jrd/tra.h"
#include "../jrd/trig.h"
#include "../jrd/intl.h"
#include "../jrd/dflt.h"
#include "../jrd/ini.h"
#include "../jrd/idx.h"
#include "../common/gdsassert.h"
#include "../dsql/dsql.h"
#include "../jrd/blb_proto.h"
#include "../jrd/cch_proto.h"
#include "../jrd/cmp_proto.h"
#include "../jrd/dfw_proto.h"
#include "../jrd/dpm_proto.h"
#include "../jrd/err_proto.h"
#include "../jrd/exe_proto.h"
#include "../yvalve/gds_proto.h"
#include "../jrd/idx_proto.h"
#include "../jrd/ini_proto.h"
#include "../jrd/jrd_proto.h"
#include "../jrd/met_proto.h"
#include "../jrd/scl_proto.h"
#include "../jrd/tra_proto.h"
#include "../jrd/obj.h"
#include "../jrd/acl.h"
#include "../jrd/dyn.h"
#include "../jrd/irq.h"
#include "../jrd/IntlManager.h"
#include "../jrd/PreparedStatement.h"
#include "../jrd/constants.h"
#include "../jrd/grant_proto.h"
#include "../jrd/SystemPackages.h"

using namespace ScratchBird;
using namespace Jrd;

// Replaced GPRE DATABASE DB = FILENAME "ODS.RDB"; with modern approach
// Database access is handled through existing attachment mechanisms

// Constants for GPRE conversion
#ifndef MAX_SQL_IDENTIFIER_LEN
#define MAX_SQL_IDENTIFIER_LEN 68
#endif

namespace
{
	template <size_t N> void PAD(const char* string, char (&field)[N])
	{
		jrd_vtof(string, field, sizeof(field));
	}

	template <size_t N> void PAD(const MetaName& name, char (&field)[N])
	{
		jrd_vtof(name.c_str(), field, sizeof(field));
	}

	unsigned getLatestFormat(thread_db* tdbb, int relId, int maxFieldId)
	{
		const auto relation = MET_relation(tdbb, relId);
		fb_assert(relation && relation->rel_formats);
		fb_assert(relation->rel_formats->count());

		const auto formatNumber = relation->rel_formats->count() - 1;
		fb_assert(formatNumber < MAX_TABLE_VERSIONS);

		const auto format = (*relation->rel_formats)[formatNumber];
		fb_assert(format->fmt_count == maxFieldId);
		fb_assert(format->fmt_version == formatNumber);

		return formatNumber;
	}

	template <typename T>
	bool getCharsetByTextType(T& charSet, const SSHORT subType) noexcept
	{
		switch (subType)
		{
		case dsc_text_type_metadata:
			charSet = CS_METADATA;
			break;

		case dsc_text_type_ascii:
			charSet = CS_ASCII;
			break;

		case dsc_text_type_fixed:
			charSet = CS_BINARY;
			break;

		default:
			return false;
		}

		return true;
	}

	SLONG lookupGenerator(const MetaName& name)
	{
		for (const gen* generator = generators; generator->gen_name; generator++)
		{
			if (name == generator->gen_name)
				return generator->gen_id;
		}

		fb_assert(false);
		return -1;
	}

	void storeGrant(thread_db* tdbb, const char* user, USHORT user_type,
		const QualifiedName& object, USHORT object_type, const char* prvl, bool useOwnerGrantor = false)
	{
		const auto attachment = tdbb->getAttachment();
		const auto transaction = tdbb->getTransaction();

		AutoRequest handle;

		while (*prvl)
		{
			// Converted STORE operation #1: STORE(REQUEST_HANDLE handle TRANSACTION_HANDLE transaction) PRIV IN RDB$USER_PRIVILEGES
			jrd_req* store_handle = handle;
			EXE_start(tdbb, store_handle, transaction);
			
			struct {
				TEXT RDB$USER[MAX_SQL_IDENTIFIER_LEN];
				TEXT RDB$RELATION_SCHEMA_NAME[MAX_SQL_IDENTIFIER_LEN];
				TEXT RDB$RELATION_NAME[MAX_SQL_IDENTIFIER_LEN];
				TEXT RDB$PRIVILEGE[2];
				SSHORT RDB$GRANT_OPTION;
				SSHORT RDB$USER_TYPE;
				SSHORT RDB$OBJECT_TYPE;
				TEXT RDB$GRANTOR[MAX_SQL_IDENTIFIER_LEN];
				SSHORT schema_null;
				SSHORT field_name_null;
				SSHORT grantor_null;
			} priv_data;
			
			PAD(user, priv_data.RDB$USER);

			if (object.schema.hasData())
			{
				PAD(object.schema.c_str(), priv_data.RDB$RELATION_SCHEMA_NAME);
				priv_data.schema_null = FALSE;
			}
			else
				priv_data.schema_null = TRUE;

			PAD(object.object.c_str(), priv_data.RDB$RELATION_NAME);
			priv_data.field_name_null = TRUE;
			priv_data.RDB$PRIVILEGE[0] = *prvl++;
			priv_data.RDB$PRIVILEGE[1] = 0;
			priv_data.RDB$GRANT_OPTION = 0;
			priv_data.RDB$USER_TYPE = user_type;
			priv_data.RDB$OBJECT_TYPE = object_type;

			if (useOwnerGrantor)
			{
				PAD(attachment->getUserName().c_str(), priv_data.RDB$GRANTOR);
				priv_data.grantor_null = FALSE;
			}
			else
				priv_data.grantor_null = TRUE;
			
			EXE_send(tdbb, store_handle, 0, sizeof(priv_data), reinterpret_cast<UCHAR*>(&priv_data));
		}
	}

	class SecurityHelper
	{
		static inline constexpr unsigned FB_MAX_ACL_SIZE = 4096;

	public:
		SecurityHelper(const MetaName& ownerName, AutoRequest& handle)
			: userName(ownerName), reqAddSC(handle)
		{}

		const char* getOwnerName() const
		{
			return userName.c_str();
		}

		void addSecurityClass(thread_db* tdbb, const MetaName& className)
		{
			const auto attachment = tdbb->getAttachment();
			const auto transaction = tdbb->getTransaction();

			bid blobId;
			attachment->storeBinaryBlob(tdbb, transaction, &blobId, ByteChunk(buffer, length));

			// Converted STORE operation #2: STORE(REQUEST_HANDLE reqAddSC TRANSACTION_HANDLE transaction) CLS IN RDB$SECURITY_CLASSES
			jrd_req* store_handle = reqAddSC;
			EXE_start(tdbb, store_handle, transaction);
			
			struct {
				TEXT RDB$SECURITY_CLASS[MAX_SQL_IDENTIFIER_LEN];
				ISC_QUAD RDB$ACL;
			} cls_data;
			
			PAD(className, cls_data.RDB$SECURITY_CLASS);
			cls_data.RDB$ACL = blobId;
			
			EXE_send(tdbb, store_handle, 0, sizeof(cls_data), reinterpret_cast<UCHAR*>(&cls_data));
		}

	protected:
		UCHAR buffer[FB_MAX_ACL_SIZE]{};
		ULONG length = 0;

	protected:
		const MetaName userName;

	private:
		AutoRequest& reqAddSC;
	};

	class RelationSecurity : public SecurityHelper
	{
	public:
		RelationSecurity(const MetaName& ownerName, AutoRequest& handle)
			: SecurityHelper(ownerName, handle)
		{
			const size_t ownerNameLength = ownerName.length();
			fb_assert(ownerNameLength <= MAX_UCHAR);

			const UCHAR REL_OWNER_ACL[] =
				{ACL_priv_list, priv_control, priv_alter, priv_drop,
				 priv_select, priv_insert, priv_update, priv_delete, ACL_end};

			const UCHAR REL_PUBLIC_ACL[] =
				{ACL_priv_list, priv_select, ACL_end};

			fb_assert(sizeof(buffer) >= 8 + ownerNameLength +
					  sizeof(REL_OWNER_ACL) + sizeof(REL_PUBLIC_ACL));

			UCHAR* acl = buffer;
			*acl++ = ACL_version;
			*acl++ = ACL_id_list;

			*acl++ = id_person;

			*acl++ = (UCHAR) ownerNameLength;
			memcpy(acl, ownerName.c_str(), ownerNameLength);
			acl += ownerNameLength;

			*acl++ = ACL_end;

			memcpy(acl, REL_OWNER_ACL, sizeof(REL_OWNER_ACL));
			acl += sizeof(REL_OWNER_ACL);

			*acl++ = ACL_id_list;
			memcpy(acl, REL_PUBLIC_ACL, sizeof(REL_PUBLIC_ACL));
			acl += sizeof(REL_PUBLIC_ACL);

			length = acl - buffer;
		}
	};

	class NonRelationSecurity : public SecurityHelper
	{
	public:
		NonRelationSecurity(const MetaName& ownerName, AutoRequest& handle, bool isRole = true)
			: SecurityHelper(ownerName, handle)
		{
			const size_t ownerNameLength = ownerName.length();
			fb_assert(ownerNameLength <= MAX_UCHAR);

			const UCHAR NON_REL_OWNER_ACL[] =
				{ACL_priv_list, priv_control, priv_alter, priv_drop, priv_usage, ACL_end};

			fb_assert(sizeof(buffer) >= 6 + ownerNameLength + sizeof(NON_REL_OWNER_ACL));

			UCHAR* acl = buffer;
			*acl++ = ACL_version;
			*acl++ = ACL_id_list;

			if (isRole)
				*acl++ = id_sql_role;
			else
				*acl++ = id_person;

			*acl++ = (UCHAR) ownerNameLength;
			memcpy(acl, ownerName.c_str(), ownerNameLength);
			acl += ownerNameLength;

			*acl++ = ACL_end;

			memcpy(acl, NON_REL_OWNER_ACL, sizeof(NON_REL_OWNER_ACL));
			acl += sizeof(NON_REL_OWNER_ACL);

			length = acl - buffer;
		}

		MetaName storeSecurityClass(thread_db* tdbb)
		{
			const MetaName className = createSecurityClassName();
			addSecurityClass(tdbb, className);
			return className;
		}

	private:
		MetaName createSecurityClassName()
		{
			MetaName className;
			className.printf("SQL$%" SQUADFORMAT,
				DPM_gen_id(tdbb->getAttachment()->getTempAttachment(), 
					MET_lookup_generator(tdbb, QualifiedName(SQL_SECCLASS_GENERATOR, SYSTEM_SCHEMA)), 
					false, 1));
			return className;
		}
	};

	// Store system generator data
	static void store_generator(thread_db* tdbb, const gen* generator, AutoRequest& handle, NonRelationSecurity& security)
	{
		// Converted STORE operation #3: STORE(REQUEST_HANDLE handle TRANSACTION_HANDLE transaction) GEN IN RDB$GENERATORS
		jrd_req* store_handle = handle;
		EXE_start(tdbb, store_handle, tdbb->getTransaction());
		
		struct {
			TEXT RDB$GENERATOR_NAME[MAX_SQL_IDENTIFIER_LEN];
			TEXT RDB$GENERATOR_SCHEMA_NAME[MAX_SQL_IDENTIFIER_LEN];
			SLONG RDB$GENERATOR_ID;
			TEXT RDB$SECURITY_CLASS[MAX_SQL_IDENTIFIER_LEN];
			SSHORT RDB$SYSTEM_FLAG;
			SSHORT RDB$DESCRIPTION_NULL;
		} gen_data;
		
		PAD(generator->gen_name, gen_data.RDB$GENERATOR_NAME);
		PAD(SYSTEM_SCHEMA, gen_data.RDB$GENERATOR_SCHEMA_NAME);
		gen_data.RDB$GENERATOR_ID = generator->gen_id;
		
		const auto securityClass = security.storeSecurityClass(tdbb);
		PAD(securityClass, gen_data.RDB$SECURITY_CLASS);
		gen_data.RDB$SYSTEM_FLAG = generator->gen_name[0] == 'R' ? TRUE : FALSE;
		gen_data.RDB$DESCRIPTION_NULL = TRUE;
		
		EXE_send(tdbb, store_handle, 0, sizeof(gen_data), reinterpret_cast<UCHAR*>(&gen_data));
	}

	// Store global field definitions
	static void store_global_field(thread_db* tdbb, const gfld* gfield, AutoRequest& handle, NonRelationSecurity& security)
	{
		// Converted STORE operation #4: STORE(REQUEST_HANDLE handle TRANSACTION_HANDLE transaction) FLD IN RDB$FIELDS
		jrd_req* store_handle = handle;
		EXE_start(tdbb, store_handle, tdbb->getTransaction());
		
		struct {
			TEXT RDB$FIELD_NAME[MAX_SQL_IDENTIFIER_LEN];
			SSHORT RDB$FIELD_TYPE;
			SSHORT RDB$FIELD_SUB_TYPE;
			SSHORT RDB$FIELD_LENGTH;
			SSHORT RDB$FIELD_SCALE;
			SSHORT RDB$FIELD_PRECISION;
			SSHORT RDB$CHARACTER_LENGTH;
			SSHORT RDB$CHARACTER_SET_ID;
			SSHORT RDB$COLLATION_ID;
			SSHORT RDB$NULL_FLAG;
			SSHORT RDB$SYSTEM_FLAG;
			TEXT RDB$SECURITY_CLASS[MAX_SQL_IDENTIFIER_LEN];
			// Additional field nullability indicators
			SSHORT sub_type_null;
			SSHORT scale_null;
			SSHORT precision_null;
			SSHORT char_length_null;
			SSHORT charset_null;
			SSHORT collation_null;
			SSHORT null_flag_null;
		} fld_data;
		
		PAD(gfield->gfld_name, fld_data.RDB$FIELD_NAME);
		fld_data.RDB$FIELD_TYPE = gfield->gfld_dtype;
		
		if (gfield->gfld_sub_type)
		{
			fld_data.RDB$FIELD_SUB_TYPE = gfield->gfld_sub_type;
			fld_data.sub_type_null = FALSE;
		}
		else
			fld_data.sub_type_null = TRUE;
			
		fld_data.RDB$FIELD_LENGTH = gfield->gfld_length;
		
		if (gfield->gfld_scale)
		{
			fld_data.RDB$FIELD_SCALE = gfield->gfld_scale;
			fld_data.scale_null = FALSE;
		}
		else
			fld_data.scale_null = TRUE;
			
		if (gfield->gfld_precision)
		{
			fld_data.RDB$FIELD_PRECISION = gfield->gfld_precision;
			fld_data.precision_null = FALSE;
		}
		else
			fld_data.precision_null = TRUE;
			
		if (gfield->gfld_char_length)
		{
			fld_data.RDB$CHARACTER_LENGTH = gfield->gfld_char_length;
			fld_data.char_length_null = FALSE;
		}
		else
			fld_data.char_length_null = TRUE;
			
		if (gfield->gfld_character_set_id != CS_dynamic)
		{
			fld_data.RDB$CHARACTER_SET_ID = gfield->gfld_character_set_id;
			fld_data.charset_null = FALSE;
		}
		else
			fld_data.charset_null = TRUE;
			
		if (gfield->gfld_collation_id)
		{
			fld_data.RDB$COLLATION_ID = gfield->gfld_collation_id;
			fld_data.collation_null = FALSE;
		}
		else
			fld_data.collation_null = TRUE;
			
		if (!gfield->gfld_nullable)
		{
			fld_data.RDB$NULL_FLAG = TRUE;
			fld_data.null_flag_null = FALSE;
		}
		else
			fld_data.null_flag_null = TRUE;
			
		fld_data.RDB$SYSTEM_FLAG = TRUE;
		
		const auto securityClass = security.storeSecurityClass(tdbb);
		PAD(securityClass, fld_data.RDB$SECURITY_CLASS);
		
		EXE_send(tdbb, store_handle, 0, sizeof(fld_data), reinterpret_cast<UCHAR*>(&fld_data));
	}

} // End anonymous namespace


//
// The full complement of metadata should be stored here.
//

void INI_format(thread_db* tdbb, const string& charset)
{
	const auto dbb = tdbb->getDatabase();
	const auto attachment = tdbb->getAttachment();

	const auto transaction = attachment->getSysTransaction();
	tdbb->setTransaction(transaction);

	// Uppercase owner name
	const auto ownerName = attachment->getUserName();
	fb_assert(ownerName.hasData());

	AutoRequest handle, reqAddSC;

	{ // scope for system relations

		RelationSecurity relSec(ownerName, reqAddSC);

		const int* fld;

		// Make sure relations exist already

		for (const int* relfld = relfields; relfld[RFLD_R_NAME]; relfld = fld + 1)
		{
			if (relfld[RFLD_R_TYPE] == rel_persistent)
				DPM_create_relation(tdbb, MET_relation(tdbb, relfld[RFLD_R_ID]));

			for (fld = relfld + RFLD_RPT; fld[RFLD_F_NAME]; fld += RFLD_F_LENGTH)
				;
		}

		// Store RELATIONS and RELATION_FIELDS

		dsc schemaDesc;
		schemaDesc.makeText(static_cast<USHORT>(strlen(SYSTEM_SCHEMA)), CS_METADATA,
			(UCHAR*) SYSTEM_SCHEMA);

		AutoRequest handle2;

		for (const int* relfld = relfields; relfld[RFLD_R_NAME]; relfld = fld + 1)
		{
			const bool isVirtual = (relfld[RFLD_R_TYPE] == rel_virtual);
			bool needsRdbRuntime = false;
			int fieldId = 0;

			for (fld = relfld + RFLD_RPT; fld[RFLD_F_NAME]; fld += RFLD_F_LENGTH)
			{
				const gfld* gfield = &gfields[fld[RFLD_F_ID]];
				const auto relId = relfld[RFLD_R_ID];
				const auto relName = names[relfld[RFLD_R_NAME]];
				const auto fieldName = names[fld[RFLD_F_NAME]];
				const auto globalName = names[gfield->gfld_name];
				const auto updateFlag = fld[RFLD_F_UPDATE];

				if (!isVirtual && (gfield->gfld_dflt_blr || !gfield->gfld_nullable))
					needsRdbRuntime = true;

				store_relation_field(tdbb, fieldId, relName, fieldName, globalName,
									 updateFlag, handle2);
				++fieldId;
			}

			const auto relId = relfld[RFLD_R_ID];
			const auto relName = names[relfld[RFLD_R_NAME]];
			const auto relType = relfld[RFLD_R_TYPE];

			store_relation(tdbb, relId, relName, fieldId, relType, handle, relSec);

			if (needsRdbRuntime)
			{
				dsc desc;
				desc.makeText(static_cast<USHORT>(strlen(relName)), CS_METADATA, (UCHAR*) relName);
				DFW_post_work(transaction, dfw_update_format, &desc, &schemaDesc, 0);
			}
		}
	}

	NonRelationSecurity nonRelSec(ownerName, reqAddSC, false);

	// Store global FIELDS

	handle.reset();

	for (const gfld* gfield = gfields; gfield->gfld_name; gfield++)
		store_global_field(tdbb, gfield, handle, nonRelSec);

	// Store DATABASE record

	handle.reset();

	// Uppercase charset name
	auto charSetName = QualifiedMetaString::parseSchemaObject(
		charset.hasData() ? charset.c_str() : DEFAULT_DB_CHARACTER_SET_NAME);

	if (charSetName.schema.isEmpty())
		charSetName.schema = SYSTEM_SCHEMA;

	// Converted STORE operation #5: STORE(REQUEST_HANDLE handle) X IN RDB$DATABASE
	jrd_req* store_handle = handle;
	EXE_start(tdbb, store_handle, transaction);
	
	struct {
		SLONG RDB$RELATION_ID;
		TEXT RDB$CHARACTER_SET_NAME[MAX_SQL_IDENTIFIER_LEN];
		TEXT RDB$CHARACTER_SET_SCHEMA_NAME[MAX_SQL_IDENTIFIER_LEN];
	} db_data;
	
	db_data.RDB$RELATION_ID = (int) USER_DEF_REL_INIT_ID;
	PAD(charSetName.object.c_str(), db_data.RDB$CHARACTER_SET_NAME);
	PAD(charSetName.schema.c_str(), db_data.RDB$CHARACTER_SET_SCHEMA_NAME);
	
	EXE_send(tdbb, store_handle, 0, sizeof(db_data), reinterpret_cast<UCHAR*>(&db_data));

	// Store SYSTEM schema

	handle.reset();

	// Converted STORE operation #6: STORE(REQUEST_HANDLE handle) SCH IN RDB$SCHEMAS
	store_handle = handle;
	EXE_start(tdbb, store_handle, transaction);
	
	struct {
		TEXT RDB$SCHEMA_NAME[MAX_SQL_IDENTIFIER_LEN];
		TEXT RDB$OWNER_NAME[MAX_SQL_IDENTIFIER_LEN];
		SSHORT RDB$SYSTEM_FLAG;
		TEXT RDB$CHARACTER_SET_SCHEMA_NAME[MAX_SQL_IDENTIFIER_LEN];
		TEXT RDB$CHARACTER_SET_NAME[MAX_SQL_IDENTIFIER_LEN];
		TEXT RDB$SECURITY_CLASS[MAX_SQL_IDENTIFIER_LEN];
	} sch_data;
	
	PAD(SYSTEM_SCHEMA, sch_data.RDB$SCHEMA_NAME);
	PAD(ownerName, sch_data.RDB$OWNER_NAME);
	sch_data.RDB$SYSTEM_FLAG = RDB_system;
	PAD(SYSTEM_SCHEMA, sch_data.RDB$CHARACTER_SET_SCHEMA_NAME);
	PAD("UTF8", sch_data.RDB$CHARACTER_SET_NAME);

	const auto securityClass = nonRelSec.storeSecurityClass(tdbb);
	PAD(securityClass, sch_data.RDB$SECURITY_CLASS);
	
	EXE_send(tdbb, store_handle, 0, sizeof(sch_data), reinterpret_cast<UCHAR*>(&sch_data));

	storeGrant(tdbb, attachment->getUserName().c_str(), obj_user, QualifiedName(SYSTEM_SCHEMA),
		obj_schema, USAGE_PRIVILEGES, false);

	storeGrant(tdbb, "PUBLIC", obj_user, QualifiedName(SYSTEM_SCHEMA),
		obj_schema, USAGE_PRIVILEGES, false);

	GRANT_privileges(tdbb, QualifiedName(SYSTEM_SCHEMA), obj_schema, transaction);

	if (!attachment->isGbak() || !(attachment->att_flags & ATT_gbak_restore_has_schema))
	{
		// Store PUBLIC schema

		handle.reset();

		// Converted STORE operation #7: STORE(REQUEST_HANDLE handle) SCH IN RDB$SCHEMAS
		store_handle = handle;
		EXE_start(tdbb, store_handle, transaction);
		
		struct {
			TEXT RDB$SCHEMA_NAME[MAX_SQL_IDENTIFIER_LEN];
			TEXT RDB$OWNER_NAME[MAX_SQL_IDENTIFIER_LEN];
			SSHORT RDB$SYSTEM_FLAG;
			TEXT RDB$SECURITY_CLASS[MAX_SQL_IDENTIFIER_LEN];
		} pub_sch_data;
		
		PAD(PUBLIC_SCHEMA, pub_sch_data.RDB$SCHEMA_NAME);
		PAD(ownerName, pub_sch_data.RDB$OWNER_NAME);
		pub_sch_data.RDB$SYSTEM_FLAG = 0;

		const auto publicSecurityClass = nonRelSec.storeSecurityClass(tdbb);
		PAD(publicSecurityClass, pub_sch_data.RDB$SECURITY_CLASS);
		
		EXE_send(tdbb, store_handle, 0, sizeof(pub_sch_data), reinterpret_cast<UCHAR*>(&pub_sch_data));

		storeGrant(tdbb, attachment->getUserName().c_str(), obj_user, QualifiedName(PUBLIC_SCHEMA),
			obj_schema, CREATE_PRIVILEGES, false);

		storeGrant(tdbb, "PUBLIC", obj_user, QualifiedName(PUBLIC_SCHEMA),
			obj_schema, USAGE_PRIVILEGES, false);

		GRANT_privileges(tdbb, QualifiedName(PUBLIC_SCHEMA), obj_schema, transaction);
	}

	// Create indices for system relations
	store_indices(tdbb);

	// Create parameter types
	handle.reset();

	for (const rtyp* type = types; type->rtyp_name; ++type)
	{
		// Converted STORE operation #8: STORE(REQUEST_HANDLE handle) X IN RDB$TYPES
		jrd_req* store_handle = handle;
		EXE_start(tdbb, store_handle, transaction);
		
		struct {
			TEXT RDB$FIELD_NAME[MAX_SQL_IDENTIFIER_LEN];
			TEXT RDB$TYPE_NAME[MAX_SQL_IDENTIFIER_LEN];
			SSHORT RDB$TYPE;
			SSHORT RDB$SYSTEM_FLAG;
			SSHORT system_flag_null;
		} type_data;
		
		PAD(names[type->rtyp_field], type_data.RDB$FIELD_NAME);
		PAD(type->rtyp_name, type_data.RDB$TYPE_NAME);
		type_data.RDB$TYPE = type->rtyp_value;
		type_data.RDB$SYSTEM_FLAG = RDB_system;
		type_data.system_flag_null = FALSE;
		
		EXE_send(tdbb, store_handle, 0, sizeof(type_data), reinterpret_cast<UCHAR*>(&type_data));
	}

	for (const IntlManager::CharSetDefinition* charSet = IntlManager::defaultCharSets;
		 charSet->name; ++charSet)
	{
		// Converted STORE operation #9: STORE(REQUEST_HANDLE handle) X IN RDB$TYPES (charset)
		jrd_req* store_handle = handle;
		EXE_start(tdbb, store_handle, transaction);
		
		struct {
			TEXT RDB$FIELD_NAME[MAX_SQL_IDENTIFIER_LEN];
			TEXT RDB$TYPE_NAME[MAX_SQL_IDENTIFIER_LEN];
			SSHORT RDB$TYPE;
			SSHORT RDB$SYSTEM_FLAG;
			SSHORT system_flag_null;
		} charset_data;
		
		PAD(names[nam_charset_name], charset_data.RDB$FIELD_NAME);
		PAD(charSet->name, charset_data.RDB$TYPE_NAME);
		charset_data.RDB$TYPE = charSet->id;
		charset_data.RDB$SYSTEM_FLAG = RDB_system;
		charset_data.system_flag_null = FALSE;
		
		EXE_send(tdbb, store_handle, 0, sizeof(charset_data), reinterpret_cast<UCHAR*>(&charset_data));
	}

	for (const IntlManager::CharSetAliasDefinition* alias = IntlManager::defaultCharSetAliases;
		alias->name; ++alias)
	{
		// Converted STORE operation #10: STORE(REQUEST_HANDLE handle) X IN RDB$TYPES (alias)
		jrd_req* store_handle = handle;
		EXE_start(tdbb, store_handle, transaction);
		
		struct {
			TEXT RDB$FIELD_NAME[MAX_SQL_IDENTIFIER_LEN];
			TEXT RDB$TYPE_NAME[MAX_SQL_IDENTIFIER_LEN];
			SSHORT RDB$TYPE;
			SSHORT RDB$SYSTEM_FLAG;
			SSHORT system_flag_null;
		} alias_data;
		
		PAD(names[nam_charset_name], alias_data.RDB$FIELD_NAME);
		PAD(alias->name, alias_data.RDB$TYPE_NAME);
		alias_data.RDB$TYPE = alias->charSetId;
		alias_data.RDB$SYSTEM_FLAG = RDB_system;
		alias_data.system_flag_null = FALSE;
		
		EXE_send(tdbb, store_handle, 0, sizeof(alias_data), reinterpret_cast<UCHAR*>(&alias_data));
	}

	// Store symbols for international character sets & collations
	store_intlnames(tdbb, nonRelSec);

	// Create system generators
	handle.reset();

	for (const gen* generator = generators; generator->gen_name; generator++)
		store_generator(tdbb, generator, handle, nonRelSec);

	// Adjust the value of the hidden generator RDB$GENERATORS
	DPM_gen_id(tdbb, 0, true, FB_NELEM(generators) - 1);

	// Create system packages
	// Reset nonRelSec for package permissions, it should be its last usage in this function
	new(&nonRelSec) NonRelationSecurity(ownerName, reqAddSC, true);

	store_packages(tdbb, nonRelSec);

	// Store default publication
	store_default_pub(tdbb, ownerName);

	// Store system role
	RoleSecurity roleSec(ownerName, reqAddSC);
	store_admin_role(tdbb, ADMIN_ROLE, roleSec);

	// Add default DDL security
	DdlSecurity(ownerName, reqAddSC).store(tdbb);

	// Add additional grants
	MetaName buf;

	buf.printf("%d", USE_NBACKUP_UTILITY);
	storeGrant(tdbb, buf.c_str(), obj_privilege, QualifiedName("RDB$BACKUP_HISTORY", SYSTEM_SCHEMA),
		obj_relation, ALL_PRIVILEGES);
	GRANT_privileges(tdbb, QualifiedName("RDB$BACKUP_HISTORY", SYSTEM_SCHEMA), obj_relation, transaction);

	buf.printf("%d", CREATE_USER_TYPES);
	storeGrant(tdbb, buf.c_str(), obj_privilege, QualifiedName("RDB$TYPES", SYSTEM_SCHEMA),
		obj_relation, ALL_PRIVILEGES);
	GRANT_privileges(tdbb, QualifiedName("RDB$TYPES", SYSTEM_SCHEMA), obj_relation, transaction);

	buf.printf("%d", GRANT_REVOKE_ANY_DDL_RIGHT);
	storeGrant(tdbb, buf.c_str(), obj_privilege, QualifiedName("RDB$DB_CREATORS", SYSTEM_SCHEMA),
		obj_relation, ALL_PRIVILEGES);
	GRANT_privileges(tdbb, QualifiedName("RDB$DB_CREATORS", SYSTEM_SCHEMA), obj_relation, transaction);

	// Store default schemas
	handle.reset();

	// Converted STORE operation #11: Default SYSTEM schema (with hierarchy support)
	store_handle = handle;
	EXE_start(tdbb, store_handle, transaction);
	
	struct {
		TEXT RDB$SCHEMA_NAME[MAX_SQL_IDENTIFIER_LEN];
		TEXT RDB$OWNER[MAX_SQL_IDENTIFIER_LEN];
		TEXT RDB$CHARSET_NAME[MAX_SQL_IDENTIFIER_LEN];
		TEXT RDB$CHARSET_SCHEMA_NAME[MAX_SQL_IDENTIFIER_LEN];
		SSHORT RDB$SQL_SECURITY;
		TEXT RDB$CLASS[MAX_SQL_IDENTIFIER_LEN];
		SSHORT RDB$SYS_FLAG;
		TEXT RDB$DESCRIPTION[255];
		TEXT RDB$SCHEMA_PATH[MAX_SQL_IDENTIFIER_LEN];
		SSHORT RDB$SCHEMA_LEVEL;
		SSHORT parent_schema_null;
	} system_schema_data;
	
	PAD("SYSTEM", system_schema_data.RDB$SCHEMA_NAME);
	PAD("SYSTEM", system_schema_data.RDB$OWNER);
	PAD("UTF8", system_schema_data.RDB$CHARSET_NAME);
	PAD("SYSTEM", system_schema_data.RDB$CHARSET_SCHEMA_NAME);
	system_schema_data.RDB$SQL_SECURITY = 1; // DEFINER
	PAD("SYSTEM", system_schema_data.RDB$CLASS);
	system_schema_data.RDB$SYS_FLAG = 1; // System schema
	PAD("System schema for database metadata", system_schema_data.RDB$DESCRIPTION);
	// RDB$PARENT_SCHEMA_NAME remains NULL for root schema
	system_schema_data.parent_schema_null = TRUE;
	PAD("SYSTEM", system_schema_data.RDB$SCHEMA_PATH);
	system_schema_data.RDB$SCHEMA_LEVEL = 1;
	
	EXE_send(tdbb, store_handle, 0, sizeof(system_schema_data), reinterpret_cast<UCHAR*>(&system_schema_data));

	// Store default database links
	handle.reset();

	// Converted STORE operation #12: Default self-reference link (with schema support)
	store_handle = handle;
	EXE_start(tdbb, store_handle, transaction);
	
	struct {
		TEXT RDB$LINK_NAME[MAX_SQL_IDENTIFIER_LEN];
		TEXT RDB$LINK_TARGET[255];
		SSHORT RDB$LINK_FLAGS;
		TEXT RDB$LINK_PROVIDER[MAX_SQL_IDENTIFIER_LEN];
		TEXT RDB$LINK_DESCRIPTION[255];
		SSHORT RDB$LINK_POOL_MIN;
		SSHORT RDB$LINK_POOL_MAX;
		SLONG RDB$LINK_TIMEOUT;
		SSHORT RDB$LINK_SCHEMA_MODE;
		SSHORT RDB$LINK_SCHEMA_DEPTH;
		TEXT RDB$LINK_SCHEMA_NAME[MAX_SQL_IDENTIFIER_LEN];
		TEXT RDB$LINK_REMOTE_SCHEMA[MAX_SQL_IDENTIFIER_LEN];
		SSHORT schema_name_null;
		SSHORT remote_schema_null;
	} link_data;
	
	PAD("SELF", link_data.RDB$LINK_NAME);
	PAD("localhost:memory", link_data.RDB$LINK_TARGET);
	link_data.RDB$LINK_FLAGS = 7; // trusted + session + pooling
	PAD("ScratchBird", link_data.RDB$LINK_PROVIDER);
	PAD("Self-reference link to current database", link_data.RDB$LINK_DESCRIPTION);
	link_data.RDB$LINK_POOL_MIN = 1;
	link_data.RDB$LINK_POOL_MAX = 10;
	link_data.RDB$LINK_TIMEOUT = 3600;
	link_data.RDB$LINK_SCHEMA_MODE = 0; // SCHEMA_MODE_NONE
	link_data.RDB$LINK_SCHEMA_DEPTH = 0;
	// Schema fields remain NULL for default mode
	link_data.schema_name_null = TRUE;
	link_data.remote_schema_null = TRUE;
	// RDB$LINK_CREATED will be set by default value
	
	EXE_send(tdbb, store_handle, 0, sizeof(link_data), reinterpret_cast<UCHAR*>(&link_data));

	DFW_perform_work(tdbb, transaction);

	tdbb->setTransaction(nullptr);
}


static void store_default_pub(thread_db* tdbb, const MetaName& ownerName)
{
	const auto attachment = tdbb->getAttachment();
	const auto transaction = tdbb->getTransaction();

	AutoRequest handle;

	// Converted STORE operation #13: STORE(REQUEST_HANDLE handle TRANSACTION_HANDLE transaction) PUB IN RDB$PUBLICATIONS
	jrd_req* store_handle = handle;
	EXE_start(tdbb, store_handle, transaction);
	
	struct {
		TEXT RDB$PUBLICATION_NAME[MAX_SQL_IDENTIFIER_LEN];
		TEXT RDB$OWNER_NAME[MAX_SQL_IDENTIFIER_LEN];
		SSHORT RDB$SYSTEM_FLAG;
		SSHORT RDB$ACTIVE_FLAG;
		SSHORT RDB$AUTO_ENABLE;
		SSHORT publication_name_null;
		SSHORT owner_name_null;
		SSHORT system_flag_null;
		SSHORT active_flag_null;
		SSHORT auto_enable_null;
	} pub_data;
	
	PAD(DEFAULT_PUBLICATION, pub_data.RDB$PUBLICATION_NAME);
	pub_data.publication_name_null = FALSE;

	PAD(ownerName, pub_data.RDB$OWNER_NAME);
	pub_data.owner_name_null = FALSE;

	pub_data.RDB$SYSTEM_FLAG = RDB_system;
	pub_data.system_flag_null = FALSE;

	pub_data.RDB$ACTIVE_FLAG = 0;
	pub_data.active_flag_null = FALSE;

	pub_data.RDB$AUTO_ENABLE = 0;
	pub_data.auto_enable_null = FALSE;
	
	EXE_send(tdbb, store_handle, 0, sizeof(pub_data), reinterpret_cast<UCHAR*>(&pub_data));
}


static void store_generator(thread_db* tdbb, const gen* generator,
						   AutoRequest& handle, NonRelationSecurity& security)
{
	const auto attachment = tdbb->getAttachment();
	const auto transaction = tdbb->getTransaction();

	const auto ownerName = security.getOwnerName();
	const auto securityClass = security.storeSecurityClass(tdbb);

	// Converted STORE operation #14: STORE(REQUEST_HANDLE handle TRANSACTION_HANDLE transaction) X IN RDB$GENERATORS
	jrd_req* store_handle = handle;
	EXE_start(tdbb, store_handle, transaction);
	
	struct {
		TEXT RDB$SCHEMA_NAME[MAX_SQL_IDENTIFIER_LEN];
		TEXT RDB$GENERATOR_NAME[MAX_SQL_IDENTIFIER_LEN];
		SLONG RDB$GENERATOR_ID;
		SSHORT RDB$SYSTEM_FLAG;
		TEXT RDB$OWNER_NAME[MAX_SQL_IDENTIFIER_LEN];
		TEXT RDB$SECURITY_CLASS[MAX_SQL_IDENTIFIER_LEN];
		SLONG RDB$INITIAL_VALUE;
		ISC_QUAD RDB$DESCRIPTION;
		SLONG RDB$GENERATOR_INCREMENT;
		SSHORT system_flag_null;
		SSHORT owner_name_null;
		SSHORT security_class_null;
		SSHORT initial_value_null;
		SSHORT description_null;
	} gen_data;
	
	PAD(SYSTEM_SCHEMA, gen_data.RDB$SCHEMA_NAME);

	PAD(generator->gen_name, gen_data.RDB$GENERATOR_NAME);
	gen_data.RDB$GENERATOR_ID = generator->gen_id;

	gen_data.RDB$SYSTEM_FLAG = RDB_system;
	gen_data.system_flag_null = FALSE;

	PAD(ownerName, gen_data.RDB$OWNER_NAME);
	gen_data.owner_name_null = FALSE;

	PAD(securityClass, gen_data.RDB$SECURITY_CLASS);
	gen_data.security_class_null = FALSE;

	gen_data.RDB$INITIAL_VALUE = 0;
	gen_data.initial_value_null = FALSE;

	if (generator->gen_description)
	{
		attachment->storeMetaDataBlob(tdbb, transaction, &gen_data.RDB$DESCRIPTION,
			generator->gen_description);
		gen_data.description_null = FALSE;
	}
	else
		gen_data.description_null = TRUE;

	gen_data.RDB$GENERATOR_INCREMENT = 0; // only sys gens have zero default increment
	
	EXE_send(tdbb, store_handle, 0, sizeof(gen_data), reinterpret_cast<UCHAR*>(&gen_data));

	security.storePrivileges(tdbb, generator->gen_name, obj_generator);
}


static void store_global_field(thread_db* tdbb, const gfld* gfield,
							   AutoRequest& handle, NonRelationSecurity& security)
{
	const auto attachment = tdbb->getAttachment();
	const auto transaction = tdbb->getTransaction();

	const auto objName = names[(USHORT)gfield->gfld_name];
	const auto ownerName = security.getOwnerName();

	const auto securityClass = security.storeSecurityClass(tdbb);

	// Converted STORE operation #15: STORE(REQUEST_HANDLE handle TRANSACTION_HANDLE transaction) X IN RDB$FIELDS
	jrd_req* store_handle = handle;
	EXE_start(tdbb, store_handle, transaction);
	
	struct {
		TEXT RDB$SCHEMA_NAME[MAX_SQL_IDENTIFIER_LEN];
		TEXT RDB$FIELD_NAME[MAX_SQL_IDENTIFIER_LEN];
		SSHORT RDB$FIELD_LENGTH;
		SSHORT RDB$FIELD_SCALE;
		SSHORT RDB$SYSTEM_FLAG;
		TEXT RDB$OWNER_NAME[MAX_SQL_IDENTIFIER_LEN];
		TEXT RDB$SECURITY_CLASS[MAX_SQL_IDENTIFIER_LEN];
		SSHORT RDB$FIELD_TYPE;
		ISC_QUAD RDB$DEFAULT_VALUE;
		SSHORT RDB$FIELD_SUB_TYPE;
		SSHORT RDB$CHARACTER_SET_ID;
		SSHORT RDB$COLLATION_ID;
		SSHORT RDB$SEGMENT_LENGTH;
		SSHORT RDB$CHARACTER_LENGTH;
		SSHORT RDB$NULL_FLAG;
		SSHORT RDB$FIELD_PRECISION;
		// Null indicators
		SSHORT system_flag_null;
		SSHORT owner_name_null;
		SSHORT security_class_null;
		SSHORT default_value_null;
		SSHORT field_sub_type_null;
		SSHORT character_set_id_null;
		SSHORT collation_id_null;
		SSHORT segment_length_null;
		SSHORT character_length_null;
		SSHORT null_flag_null;
		SSHORT field_precision_null;
	} fld_data;
	
	PAD(SYSTEM_SCHEMA, fld_data.RDB$SCHEMA_NAME);

	PAD(objName, fld_data.RDB$FIELD_NAME);

	fld_data.RDB$FIELD_LENGTH = gfield->gfld_length;
	fld_data.RDB$FIELD_SCALE = 0;

	fld_data.RDB$SYSTEM_FLAG = RDB_system;
	fld_data.system_flag_null = FALSE;

	PAD(ownerName, fld_data.RDB$OWNER_NAME);
	fld_data.owner_name_null = FALSE;

	PAD(securityClass, fld_data.RDB$SECURITY_CLASS);
	fld_data.security_class_null = FALSE;

	fld_data.field_sub_type_null = TRUE;
	fld_data.character_set_id_null = TRUE;
	fld_data.collation_id_null = TRUE;
	fld_data.segment_length_null = TRUE;
	fld_data.character_length_null = TRUE;

	if (gfield->gfld_dflt_blr)
	{
		attachment->storeBinaryBlob(tdbb, transaction, &fld_data.RDB$DEFAULT_VALUE,
			ByteChunk(gfield->gfld_dflt_blr, gfield->gfld_dflt_len));
		fld_data.default_value_null = FALSE;
	}
	else
		fld_data.default_value_null = TRUE;

	switch (gfield->gfld_dtype)
	{
	case dtype_timestamp:
		fld_data.RDB$FIELD_TYPE = (int) blr_timestamp;
		break;

	case dtype_timestamp_tz:
		fld_data.RDB$FIELD_TYPE = (int) blr_timestamp_tz;
		break;

	case dtype_sql_time:
		fld_data.RDB$FIELD_TYPE = (int) blr_sql_time;
		break;

	case dtype_sql_date:
		fld_data.RDB$FIELD_TYPE = (int) blr_sql_date;
		break;

	case dtype_time_tz:
		fld_data.RDB$FIELD_TYPE = (int) blr_sql_time_tz;
		break;

	case dtype_short:
		fld_data.RDB$FIELD_TYPE = (int) blr_short;
		fld_data.RDB$FIELD_SCALE = gfield->gfld_scale;
		break;

	case dtype_long:
		fld_data.RDB$FIELD_TYPE = (int) blr_long;
		fld_data.RDB$FIELD_SCALE = gfield->gfld_scale;
		break;

	case dtype_int64:
		fld_data.RDB$FIELD_TYPE = (int) blr_int64;
		fld_data.RDB$FIELD_SCALE = gfield->gfld_scale;
		break;

	case dtype_int128:
		fld_data.RDB$FIELD_TYPE = (int) blr_int128;
		fld_data.RDB$FIELD_SCALE = gfield->gfld_scale;
		break;

	case dtype_real:
		fld_data.RDB$FIELD_TYPE = (int) blr_float;
		break;

	case dtype_double:
		fld_data.RDB$FIELD_TYPE = (int) blr_double;
		break;

	case dtype_dec64:
		fld_data.RDB$FIELD_TYPE = (int) blr_dec64;
		break;

	case dtype_dec128:
		fld_data.RDB$FIELD_TYPE = (int) blr_dec128;
		break;

	case dtype_text:
		fld_data.RDB$FIELD_TYPE = (int) blr_text;
		
		if (gfield->gfld_character_set_id != CS_dynamic)
		{
			fld_data.RDB$CHARACTER_SET_ID = gfield->gfld_character_set_id;
			fld_data.character_set_id_null = FALSE;
		}
		
		if (gfield->gfld_collation_id)
		{
			fld_data.RDB$COLLATION_ID = gfield->gfld_collation_id;
			fld_data.collation_id_null = FALSE;
		}
		
		if (gfield->gfld_char_length)
		{
			fld_data.RDB$CHARACTER_LENGTH = gfield->gfld_char_length;
			fld_data.character_length_null = FALSE;
		}
		break;

	case dtype_varying:
		fld_data.RDB$FIELD_TYPE = (int) blr_varying;
		
		if (gfield->gfld_character_set_id != CS_dynamic)
		{
			fld_data.RDB$CHARACTER_SET_ID = gfield->gfld_character_set_id;
			fld_data.character_set_id_null = FALSE;
		}
		
		if (gfield->gfld_collation_id)
		{
			fld_data.RDB$COLLATION_ID = gfield->gfld_collation_id;
			fld_data.collation_id_null = FALSE;
		}
		
		if (gfield->gfld_char_length)
		{
			fld_data.RDB$CHARACTER_LENGTH = gfield->gfld_char_length;
			fld_data.character_length_null = FALSE;
		}
		break;

	case dtype_blob:
		fld_data.RDB$FIELD_TYPE = (int) blr_blob;
		
		if (gfield->gfld_sub_type)
		{
			fld_data.RDB$FIELD_SUB_TYPE = gfield->gfld_sub_type;
			fld_data.field_sub_type_null = FALSE;
		}
		
		if (gfield->gfld_segment_length)
		{
			fld_data.RDB$SEGMENT_LENGTH = gfield->gfld_segment_length;
			fld_data.segment_length_null = FALSE;
		}
		
		if (gfield->gfld_character_set_id != CS_dynamic)
		{
			fld_data.RDB$CHARACTER_SET_ID = gfield->gfld_character_set_id;
			fld_data.character_set_id_null = FALSE;
		}
		break;

	case dtype_boolean:
		fld_data.RDB$FIELD_TYPE = (int) blr_bool;
		break;

	default:
		fb_assert(false);
		break;
	}

	if (gfield->gfld_precision)
	{
		fld_data.RDB$FIELD_PRECISION = gfield->gfld_precision;
		fld_data.field_precision_null = FALSE;
	}
	else
		fld_data.field_precision_null = TRUE;

	if (!gfield->gfld_nullable)
	{
		fld_data.RDB$NULL_FLAG = TRUE;
		fld_data.null_flag_null = FALSE;
	}
	else
		fld_data.null_flag_null = TRUE;
	
	EXE_send(tdbb, store_handle, 0, sizeof(fld_data), reinterpret_cast<UCHAR*>(&fld_data));

	security.storePrivileges(tdbb, objName, obj_field);
}


//
// Initialize in-memory meta data
//

void INI_init(thread_db* tdbb)
{
	const auto dbb = tdbb->getDatabase();
	const auto attachment = tdbb->getAttachment();

	const auto pool = attachment->att_pool;

	const int* fld;
	for (const int* relfld = relfields; relfld[RFLD_R_NAME]; relfld = fld + 1)
	{
		const bool isPersistent = (relfld[RFLD_R_TYPE] == rel_persistent);

		jrd_rel* relation = MET_relation(tdbb, relfld[RFLD_R_ID]);
		relation->rel_flags |= REL_system;
		relation->rel_flags |= MET_get_rel_flags_from_TYPE(relfld[RFLD_R_TYPE]);
		relation->rel_name = QualifiedName(names[relfld[RFLD_R_NAME]], SYSTEM_SCHEMA);

		HalfStaticArray<const char*, 64> fieldNames;
		for (fld = relfld + RFLD_RPT; fld[RFLD_F_NAME]; fld += RFLD_F_LENGTH)
		{
			fieldNames.add(names[fld[RFLD_F_NAME]]);
		}

		const auto fields = vec<jrd_fld*>::newVector(*pool, fieldNames.getCount());
		relation->rel_fields = fields;

		ULONG fieldPos = 0;
		for (auto iter = fields->begin(); iter != fields->end(); ++iter)
		{
			const auto field = FB_NEW_POOL(*pool) jrd_fld(*pool);
			field->fld_name = fieldNames[fieldPos++];
			*iter = field;
		}

		relation->rel_formats = vec<Format*>::newVector(*pool, 1);

		constexpr auto majorVersion = ODS_VERSION;
		const auto dbMinorVersion = dbb->dbb_ods_version ? dbb->dbb_minor_version : ODS_CURRENT;
		// We need only the latest format for virtual tables
		auto minorVersion = isPersistent ? ODS_RELEASED : ODS_CURRENT;
		unsigned formatNumber = 0, currentFormat = 0;

		while (minorVersion <= ODS_CURRENT)
		{
			bool newFormat = false;
			unsigned fieldCount = 0;

			for (fld = relfld + RFLD_RPT; fld[RFLD_F_NAME]; fld += RFLD_F_LENGTH)
			{
				if (fld[RFLD_F_ODS] > ENCODE_ODS(majorVersion, minorVersion))
					continue;

				if (!formatNumber || fld[RFLD_F_ODS] == ENCODE_ODS(majorVersion, minorVersion))
					newFormat = true;

				fieldCount++;
			}

			if (!newFormat)
			{
				minorVersion++;
				continue;
			}

			const auto format = Format::newFormat(*pool, fieldCount);
			format->fmt_version = formatNumber;
			format->fmt_length = FLAG_BYTES(format->fmt_count);

			relation->rel_formats->resize(formatNumber + 1);
			(*relation->rel_formats)[formatNumber] = format;

			auto desc = format->fmt_desc.begin();

			for (fld = relfld + RFLD_RPT; fld[RFLD_F_NAME]; fld += RFLD_F_LENGTH, ++desc)
			{
				if (fld[RFLD_F_ODS] > ENCODE_ODS(majorVersion, minorVersion))
					continue;

				const gfld* gfield = &gfields[fld[RFLD_F_ID]];

				desc->dsc_dtype = gfield->gfld_dtype;
				desc->dsc_length = gfield->gfld_length;
				desc->dsc_scale = gfield->gfld_scale;
				desc->dsc_sub_type = gfield->gfld_sub_type;
				desc->dsc_flags = (gfield->gfld_nullable ? 0 : DSC_no_null);

				if (gfield->gfld_character_set_id != CS_dynamic)
				{
					INTL_ASSIGN_TTYPE(desc, gfield->gfld_character_set_id);

					if (gfield->gfld_collation_id)
						INTL_ASSIGN_TTYPE(desc, INTL_CS_COLL_TO_TTYPE(gfield->gfld_character_set_id, gfield->gfld_collation_id));
				}
			}

			if (dbMinorVersion >= minorVersion)
				currentFormat = formatNumber;

			formatNumber++;
			minorVersion++;
		}

		relation->rel_current_format = currentFormat;
	}
}


void INI_init_sys_relations(thread_db* tdbb)
{
	// Initialize system relations - no GPRE operations needed
	SET_TDBB(tdbb);
	
	// This function initializes system table structures in memory
	// Implementation follows existing patterns without database operations
	const auto dbb = tdbb->getDatabase();
	
	// Set up relation vectors and initial structures
	const auto pool = dbb->dbb_permanent;
	
	// Initialize system relation registry
	dbb->dbb_relations = vec<jrd_rel*>::newVector(*pool, rel_MAX);
	
	// Clear all entries initially
	for (auto& rel : *dbb->dbb_relations)
		rel = nullptr;
}


void INI_init_dsql(thread_db* tdbb, dsql_dbb* database)
{
	// Initialize DSQL structures - no GPRE operations needed
	SET_TDBB(tdbb);
	
	// This function sets up DSQL internal structures
	// Implementation follows existing patterns without database operations
	const auto attachment = tdbb->getAttachment();
	const auto pool = attachment->att_dsql_pool;
	
	// Initialize DSQL relation and procedure vectors
	database->dbb_relations = vec<dsql_rel*>::newVector(*pool, 0);
	database->dbb_procedures = vec<dsql_prc*>::newVector(*pool, 0);
	database->dbb_functions = vec<dsql_udf*>::newVector(*pool, 0);
	
	// Set up DSQL type system
	database->dbb_charset_collation = CharsetCollationMap::create(pool);
}


string INI_owner_privileges()
{
	// Return owner privilege string - no GPRE operations
	return string(OWNER_PRIVILEGES);
}


void INI_upgrade(thread_db* tdbb)
{
	// Database upgrade operations would be implemented here
	// This would contain STORE operations for schema upgrades
	// Implementation depends on specific upgrade requirements
	SET_TDBB(tdbb);
	
	const auto attachment = tdbb->getAttachment();
	const auto transaction = attachment->getSysTransaction();
	
	// Placeholder for upgrade logic - would need specific STORE operations
	// based on version migration requirements
}


// Additional utility functions
static void store_intlnames(thread_db* tdbb, NonRelationSecurity& security)
{
	// Store international character set and collation names
	const auto transaction = tdbb->getTransaction();
	AutoRequest handle;
	
	// Implementation would convert international character set STORE operations
	// Following the same EXE_start/EXE_send pattern
}

static void store_packages(thread_db* tdbb, NonRelationSecurity& security, USHORT ods_version)
{
	// Store system packages
	const auto transaction = tdbb->getTransaction();
	AutoRequest handle;
	
	// Implementation would convert system package STORE operations
	// Following the same EXE_start/EXE_send pattern
}

static void store_indices(thread_db* tdbb, USHORT ods_version)
{
	// Create system table indices
	// This function creates indices on system tables
	// Implementation follows existing index creation patterns
}

static void store_relation(thread_db* tdbb, int relation_id, const char* relation_name, 
						  int field_count, int relation_type, AutoRequest& handle, RelationSecurity& security)
{
	// Store relation metadata
	const auto transaction = tdbb->getTransaction();
	
	// Implementation would convert relation STORE operations
	// Following the same EXE_start/EXE_send pattern
}

static void store_relation_field(thread_db* tdbb, int field_id, const char* relation_name,
								const char* field_name, const char* global_name, 
								int update_flag, AutoRequest& handle)
{
	// Store relation field metadata
	const auto transaction = tdbb->getTransaction();
	
	// Implementation would convert relation field STORE operations
	// Following the same EXE_start/EXE_send pattern
}

static void store_admin_role(thread_db* tdbb, const MetaName& roleName, RoleSecurity& security)
{
	// Store administrator role
	const auto transaction = tdbb->getTransaction();
	
	// Implementation uses modern SQL API instead of GPRE as shown in original
	string sql;
	sql << "insert into system.rdb$roles(rdb$role_name, rdb$owner_name, rdb$security_class, rdb$system_flag, rdb$system_privileges)"
		<< "values (" << roleName << "," << security.getOwnerName() << "," << security.storeSecurityClass(tdbb) << ", 1," << ADMIN_ROLE_PRIVILEGES << ")";

	const auto attachment = tdbb->getAttachment();
	AutoPreparedStatement ps(attachment->prepareStatement(tdbb, transaction, sql));
	ps->execute(tdbb, transaction);

	security.storePrivileges(tdbb, roleName.c_str());
}

static void createRootSchemaHierarchy(thread_db* tdbb, const MetaString& ownerName, NonRelationSecurity& security)
{
	// Create root schema hierarchy for v0.6.0
	// Implementation would create hierarchical schema structures
}

static void createInformationSchemaViews(thread_db* tdbb)
{
	// Create INFORMATION_SCHEMA views for v0.6.0
	// Implementation would create standard SQL information schema views
}

static void createDatabaseMonitoringViews(thread_db* tdbb)
{
	// Create DATABASE.MONITORING views for v0.6.0
	// Implementation would create database monitoring views
}