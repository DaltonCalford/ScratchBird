Firebird 5 internal catalog (system relations and fields)

Source of truth (internal)
- `formal/catalog.json` (machine‑readable catalog schema)
- This document (human‑readable catalog definitions)
 - `appendix_catalog_bootstrap.md` (authoritative bootstrap rules and numeric IDs)

Relations

Relation: RDB$PAGES (rel_pages, rel_persistent, ODS_8_0)
- RDB$PAGE_NUMBER :: dtype_long len=sizeof(SLONG) sub=0 nullable=true default_blr=NULL field_id=fld_p_number update=0 ods=ODS_8_0
- RDB$RELATION_ID :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_r_id update=0 ods=ODS_8_0
- RDB$PAGE_SEQUENCE :: dtype_long len=sizeof(SLONG) sub=0 nullable=true default_blr=NULL field_id=fld_p_sequence update=0 ods=ODS_8_0
- RDB$PAGE_TYPE :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_p_type update=0 ods=ODS_8_0

Relation: RDB$DATABASE (rel_database, rel_persistent, ODS_8_0)
- RDB$DESCRIPTION :: dtype_blob len=BLOB_SIZE sub=isc_blob_text nullable=true default_blr=NULL field_id=fld_description update=1 ods=ODS_8_0
- RDB$RELATION_ID :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_r_id update=0 ods=ODS_8_0
- RDB$SECURITY_CLASS :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_class update=1 ods=ODS_8_0
- RDB$CHARACTER_SET_NAME :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_charset_name update=1 ods=ODS_8_0
- RDB$LINGER :: dtype_long len=sizeof(SLONG) sub=0 nullable=true default_blr=NULL field_id=fld_linger update=1 ods=ODS_12_0
- RDB$SQL_SECURITY :: dtype_boolean len=1 sub=0 nullable=true default_blr=NULL field_id=fld_b_sql_security update=1 ods=ODS_13_0
- RDB$CHARACTER_SET_SCHEMA_NAME :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_sch_name update=1 ods=ODS_14_0

Relation: RDB$FIELDS (rel_fields, rel_persistent, ODS_8_0)
- RDB$FIELD_NAME :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_f_name update=1 ods=ODS_8_0
- RDB$QUERY_NAME :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_f_name update=1 ods=ODS_8_0
- RDB$VALIDATION_BLR :: dtype_blob len=BLOB_SIZE sub=isc_blob_blr nullable=true default_blr=NULL field_id=fld_validation update=1 ods=ODS_8_0
- RDB$VALIDATION_SOURCE :: dtype_blob len=BLOB_SIZE sub=isc_blob_text nullable=true default_blr=NULL field_id=fld_source update=1 ods=ODS_8_0
- RDB$COMPUTED_BLR :: dtype_blob len=BLOB_SIZE sub=isc_blob_blr nullable=true default_blr=NULL field_id=fld_value update=1 ods=ODS_8_0
- RDB$COMPUTED_SOURCE :: dtype_blob len=BLOB_SIZE sub=isc_blob_text nullable=true default_blr=NULL field_id=fld_source update=1 ods=ODS_8_0
- RDB$DEFAULT_VALUE :: dtype_blob len=BLOB_SIZE sub=isc_blob_blr nullable=true default_blr=NULL field_id=fld_value update=1 ods=ODS_8_0
- RDB$DEFAULT_SOURCE :: dtype_blob len=BLOB_SIZE sub=isc_blob_text nullable=true default_blr=NULL field_id=fld_source update=1 ods=ODS_8_0
- RDB$FIELD_LENGTH :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_f_length update=1 ods=ODS_8_0
- RDB$FIELD_SCALE :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_f_scale update=1 ods=ODS_8_0
- RDB$FIELD_TYPE :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_f_type update=1 ods=ODS_8_0
- RDB$FIELD_SUB_TYPE :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_sub_type update=1 ods=ODS_8_0
- RDB$MISSING_VALUE :: dtype_blob len=BLOB_SIZE sub=isc_blob_blr nullable=true default_blr=NULL field_id=fld_value update=1 ods=ODS_8_0
- RDB$MISSING_SOURCE :: dtype_blob len=BLOB_SIZE sub=isc_blob_text nullable=true default_blr=NULL field_id=fld_source update=1 ods=ODS_8_0
- RDB$DESCRIPTION :: dtype_blob len=BLOB_SIZE sub=isc_blob_text nullable=true default_blr=NULL field_id=fld_description update=1 ods=ODS_8_0
- RDB$SYSTEM_FLAG :: dtype_short len=sizeof(SSHORT) sub=0 nullable=false default_blr=NULL field_id=fld_flag update=0 ods=ODS_8_0
- RDB$QUERY_HEADER :: dtype_blob len=BLOB_SIZE sub=isc_blob_text nullable=true default_blr=NULL field_id=fld_q_header update=1 ods=ODS_8_0
- RDB$SEGMENT_LENGTH :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_s_length update=1 ods=ODS_8_0
- RDB$EDIT_STRING :: dtype_varying len=127 sub=0 nullable=true default_blr=NULL field_id=fld_edit_string update=1 ods=ODS_8_0
- RDB$EXTERNAL_LENGTH :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_f_length update=1 ods=ODS_8_0
- RDB$EXTERNAL_SCALE :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_f_scale update=1 ods=ODS_8_0
- RDB$EXTERNAL_TYPE :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_f_type update=1 ods=ODS_8_0
- RDB$DIMENSIONS :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_dimensions update=1 ods=ODS_8_0
- RDB$NULL_FLAG :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_null_flag update=1 ods=ODS_8_0
- RDB$CHARACTER_LENGTH :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_f_length update=1 ods=ODS_8_0
- RDB$COLLATION_ID :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_collate_id update=1 ods=ODS_8_0
- RDB$CHARACTER_SET_ID :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_charset_id update=1 ods=ODS_8_0
- RDB$FIELD_PRECISION :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_f_precision update=1 ods=ODS_10_0
- RDB$SECURITY_CLASS :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_class update=1 ods=ODS_12_0
- RDB$OWNER_NAME :: dtype_text len=USERNAME_LENGTH sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_user update=1 ods=ODS_12_0
- RDB$SCHEMA_NAME :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_sch_name update=1 ods=ODS_14_0

Relation: RDB$INDEX_SEGMENTS (rel_segments, rel_persistent, ODS_8_0)
- RDB$INDEX_NAME :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_i_name update=1 ods=ODS_8_0
- RDB$FIELD_NAME :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_f_name update=1 ods=ODS_8_0
- RDB$FIELD_POSITION :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_f_position update=1 ods=ODS_8_0
- RDB$STATISTICS :: dtype_double len=sizeof(double) sub=0 nullable=true default_blr=NULL field_id=fld_statistics update=1 ods=ODS_11_0
- RDB$SCHEMA_NAME :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_sch_name update=1 ods=ODS_14_0

Relation: RDB$INDICES (rel_indices, rel_persistent, ODS_8_0)
- RDB$INDEX_NAME :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_i_name update=1 ods=ODS_8_0
- RDB$RELATION_NAME :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_r_name update=1 ods=ODS_8_0
- RDB$INDEX_ID :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_i_id update=0 ods=ODS_8_0
- RDB$UNIQUE_FLAG :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_flag_nullable update=1 ods=ODS_8_0
- RDB$DESCRIPTION :: dtype_blob len=BLOB_SIZE sub=isc_blob_text nullable=true default_blr=NULL field_id=fld_description update=1 ods=ODS_8_0
- RDB$SEGMENT_COUNT :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_s_count update=1 ods=ODS_8_0
- RDB$INDEX_INACTIVE :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_flag_nullable update=1 ods=ODS_8_0
- RDB$INDEX_TYPE :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_flag_nullable update=1 ods=ODS_8_0
- RDB$FOREIGN_KEY :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_r_name update=1 ods=ODS_8_0
- RDB$SYSTEM_FLAG :: dtype_short len=sizeof(SSHORT) sub=0 nullable=false default_blr=NULL field_id=fld_flag update=0 ods=ODS_8_0
- RDB$EXPRESSION_BLR :: dtype_blob len=BLOB_SIZE sub=isc_blob_blr nullable=true default_blr=NULL field_id=fld_value update=1 ods=ODS_8_0
- RDB$EXPRESSION_SOURCE :: dtype_blob len=BLOB_SIZE sub=isc_blob_text nullable=true default_blr=NULL field_id=fld_source update=1 ods=ODS_8_0
- RDB$STATISTICS :: dtype_double len=sizeof(double) sub=0 nullable=true default_blr=NULL field_id=fld_statistics update=1 ods=ODS_8_0
- RDB$CONDITION_BLR :: dtype_blob len=BLOB_SIZE sub=isc_blob_blr nullable=true default_blr=NULL field_id=fld_value update=1 ods=ODS_13_1
- RDB$CONDITION_SOURCE :: dtype_blob len=BLOB_SIZE sub=isc_blob_text nullable=true default_blr=NULL field_id=fld_source update=1 ods=ODS_13_1
- RDB$SCHEMA_NAME :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_sch_name update=1 ods=ODS_14_0
- RDB$FOREIGN_KEY_SCHEMA_NAME :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_sch_name update=1 ods=ODS_14_0

Relation: RDB$RELATION_FIELDS (rel_rfr, rel_persistent, ODS_8_0)
- RDB$FIELD_NAME :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_f_name update=1 ods=ODS_8_0
- RDB$RELATION_NAME :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_r_name update=1 ods=ODS_8_0
- RDB$FIELD_SOURCE :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_f_name update=1 ods=ODS_8_0
- RDB$QUERY_NAME :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_f_name update=1 ods=ODS_8_0
- RDB$BASE_FIELD :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_f_name update=1 ods=ODS_8_0
- RDB$EDIT_STRING :: dtype_varying len=127 sub=0 nullable=true default_blr=NULL field_id=fld_edit_string update=1 ods=ODS_8_0
- RDB$FIELD_POSITION :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_f_position update=1 ods=ODS_8_0
- RDB$QUERY_HEADER :: dtype_blob len=BLOB_SIZE sub=isc_blob_text nullable=true default_blr=NULL field_id=fld_q_header update=1 ods=ODS_8_0
- RDB$UPDATE_FLAG :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_flag_nullable update=1 ods=ODS_8_0
- RDB$FIELD_ID :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_f_id update=0 ods=ODS_8_0
- RDB$VIEW_CONTEXT :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_context update=1 ods=ODS_8_0
- RDB$DESCRIPTION :: dtype_blob len=BLOB_SIZE sub=isc_blob_text nullable=true default_blr=NULL field_id=fld_description update=1 ods=ODS_8_0
- RDB$DEFAULT_VALUE :: dtype_blob len=BLOB_SIZE sub=isc_blob_blr nullable=true default_blr=NULL field_id=fld_value update=1 ods=ODS_8_0
- RDB$SYSTEM_FLAG :: dtype_short len=sizeof(SSHORT) sub=0 nullable=false default_blr=NULL field_id=fld_flag update=0 ods=ODS_8_0
- RDB$SECURITY_CLASS :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_class update=1 ods=ODS_8_0
- RDB$COMPLEX_NAME :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_f_name update=1 ods=ODS_8_0
- RDB$NULL_FLAG :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_null_flag update=1 ods=ODS_8_0
- RDB$DEFAULT_SOURCE :: dtype_blob len=BLOB_SIZE sub=isc_blob_text nullable=true default_blr=NULL field_id=fld_source update=1 ods=ODS_8_0
- RDB$COLLATION_ID :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_collate_id update=1 ods=ODS_8_0
- RDB$GENERATOR_NAME :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_gen_name update=1 ods=ODS_12_0
- RDB$IDENTITY_TYPE :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_identity_type update=1 ods=ODS_12_0
- RDB$SCHEMA_NAME :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_sch_name update=1 ods=ODS_14_0
- RDB$FIELD_SOURCE_SCHEMA_NAME :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_sch_name update=1 ods=ODS_14_0

Relation: RDB$RELATIONS (rel_relations, rel_persistent, ODS_8_0)
- RDB$VIEW_BLR :: dtype_blob len=BLOB_SIZE sub=isc_blob_blr nullable=true default_blr=NULL field_id=fld_v_blr update=1 ods=ODS_8_0
- RDB$VIEW_SOURCE :: dtype_blob len=BLOB_SIZE sub=isc_blob_text nullable=true default_blr=NULL field_id=fld_source update=1 ods=ODS_8_0
- RDB$DESCRIPTION :: dtype_blob len=BLOB_SIZE sub=isc_blob_text nullable=true default_blr=NULL field_id=fld_description update=1 ods=ODS_8_0
- RDB$RELATION_ID :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_r_id update=0 ods=ODS_8_0
- RDB$SYSTEM_FLAG :: dtype_short len=sizeof(SSHORT) sub=0 nullable=false default_blr=NULL field_id=fld_flag update=0 ods=ODS_8_0
- RDB$DBKEY_LENGTH :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_key_length update=0 ods=ODS_8_0
- RDB$FORMAT :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_format update=0 ods=ODS_8_0
- RDB$FIELD_ID :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_f_id update=0 ods=ODS_8_0
- RDB$RELATION_NAME :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_r_name update=1 ods=ODS_8_0
- RDB$SECURITY_CLASS :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_class update=1 ods=ODS_8_0
- RDB$EXTERNAL_FILE :: dtype_varying len=255 sub=0 nullable=true default_blr=NULL field_id=fld_file_name update=1 ods=ODS_8_0
- RDB$RUNTIME :: dtype_blob len=BLOB_SIZE sub=isc_blob_summary nullable=true default_blr=NULL field_id=fld_runtime update=1 ods=ODS_8_0
- RDB$EXTERNAL_DESCRIPTION :: dtype_blob len=BLOB_SIZE sub=isc_blob_extfile nullable=true default_blr=NULL field_id=fld_ext_desc update=1 ods=ODS_8_0
- RDB$OWNER_NAME :: dtype_text len=USERNAME_LENGTH sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_user update=1 ods=ODS_8_0
- RDB$DEFAULT_CLASS :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_class update=1 ods=ODS_8_0
- RDB$FLAGS :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_flag_nullable update=0 ods=ODS_8_0
- RDB$RELATION_TYPE :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_r_type update=0 ods=ODS_11_1
- RDB$SQL_SECURITY :: dtype_boolean len=1 sub=0 nullable=true default_blr=NULL field_id=fld_b_sql_security update=1 ods=ODS_13_0
- RDB$SCHEMA_NAME :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_sch_name update=1 ods=ODS_14_0

Relation: RDB$VIEW_RELATIONS (rel_vrel, rel_persistent, ODS_8_0)
- RDB$VIEW_NAME :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_r_name update=1 ods=ODS_8_0
- RDB$RELATION_NAME :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_r_name update=1 ods=ODS_8_0
- RDB$VIEW_CONTEXT :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_context update=1 ods=ODS_8_0
- RDB$CONTEXT_NAME :: dtype_text len=255 * METADATA_BYTES_PER_CHAR sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_ctx_name update=1 ods=ODS_8_0
- RDB$CONTEXT_TYPE :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_context update=1 ods=ODS_12_0
- RDB$PACKAGE_NAME :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_pkg_name update=1 ods=ODS_12_0
- RDB$SCHEMA_NAME :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_sch_name update=1 ods=ODS_14_0
- RDB$RELATION_SCHEMA_NAME :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_sch_name update=1 ods=ODS_14_0

Relation: RDB$FORMATS (rel_formats, rel_persistent, ODS_8_0)
- RDB$RELATION_ID :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_r_id update=0 ods=ODS_8_0
- RDB$FORMAT :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_format update=0 ods=ODS_8_0
- RDB$DESCRIPTOR :: dtype_blob len=BLOB_SIZE sub=isc_blob_format nullable=true default_blr=NULL field_id=fld_f_descr update=0 ods=ODS_8_0

Relation: RDB$SECURITY_CLASSES (rel_classes, rel_persistent, ODS_8_0)
- RDB$SECURITY_CLASS :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_class update=1 ods=ODS_8_0
- RDB$ACL :: dtype_blob len=BLOB_SIZE sub=isc_blob_acl nullable=true default_blr=NULL field_id=fld_acl update=1 ods=ODS_8_0
- RDB$DESCRIPTION :: dtype_blob len=BLOB_SIZE sub=isc_blob_text nullable=true default_blr=NULL field_id=fld_description update=1 ods=ODS_8_0

Relation: RDB$FILES (rel_files, rel_persistent, ODS_8_0)
- RDB$FILE_NAME :: dtype_varying len=255 sub=0 nullable=true default_blr=NULL field_id=fld_file_name update=1 ods=ODS_8_0
- RDB$FILE_SEQUENCE :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_file_seq update=1 ods=ODS_8_0
- RDB$FILE_START :: dtype_long len=sizeof(SLONG) sub=0 nullable=true default_blr=NULL field_id=fld_file_start update=1 ods=ODS_8_0
- RDB$FILE_LENGTH :: dtype_long len=sizeof(SLONG) sub=0 nullable=true default_blr=NULL field_id=fld_file_length update=1 ods=ODS_8_0
- RDB$FILE_FLAGS :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_file_flags update=1 ods=ODS_8_0
- RDB$SHADOW_NUMBER :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_shad_num update=1 ods=ODS_8_0

Relation: RDB$TYPES (rel_types, rel_persistent, ODS_8_0)
- RDB$FIELD_NAME :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_f_name update=1 ods=ODS_8_0
- RDB$TYPE :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_gnr_type update=1 ods=ODS_8_0
- RDB$TYPE_NAME :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_typ_name update=1 ods=ODS_8_0
- RDB$DESCRIPTION :: dtype_blob len=BLOB_SIZE sub=isc_blob_text nullable=true default_blr=NULL field_id=fld_description update=1 ods=ODS_8_0
- RDB$SYSTEM_FLAG :: dtype_short len=sizeof(SSHORT) sub=0 nullable=false default_blr=NULL field_id=fld_flag update=1 ods=ODS_8_0

Relation: RDB$TRIGGERS (rel_triggers, rel_persistent, ODS_8_0)
- RDB$TRIGGER_NAME :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_trg_name update=1 ods=ODS_8_0
- RDB$RELATION_NAME :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_r_name update=1 ods=ODS_8_0
- RDB$TRIGGER_SEQUENCE :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_trg_seq update=1 ods=ODS_8_0
- RDB$TRIGGER_TYPE :: dtype_int64 len=sizeof(SINT64) sub=0 nullable=true default_blr=NULL field_id=fld_trg_type update=1 ods=ODS_8_0
- RDB$TRIGGER_SOURCE :: dtype_blob len=BLOB_SIZE sub=isc_blob_text nullable=true default_blr=NULL field_id=fld_source update=1 ods=ODS_8_0
- RDB$TRIGGER_BLR :: dtype_blob len=BLOB_SIZE sub=isc_blob_blr nullable=true default_blr=NULL field_id=fld_trigger update=1 ods=ODS_8_0
- RDB$DESCRIPTION :: dtype_blob len=BLOB_SIZE sub=isc_blob_text nullable=true default_blr=NULL field_id=fld_description update=1 ods=ODS_8_0
- RDB$TRIGGER_INACTIVE :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_flag_nullable update=1 ods=ODS_8_0
- RDB$SYSTEM_FLAG :: dtype_short len=sizeof(SSHORT) sub=0 nullable=false default_blr=NULL field_id=fld_flag update=1 ods=ODS_8_0
- RDB$FLAGS :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_flag_nullable update=1 ods=ODS_8_0
- RDB$VALID_BLR :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_flag_nullable update=1 ods=ODS_11_1
- RDB$DEBUG_INFO :: dtype_blob len=BLOB_SIZE sub=isc_blob_debug_info nullable=true default_blr=NULL field_id=fld_debug_info update=1 ods=ODS_11_1
- RDB$ENGINE_NAME :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_engine_name update=1 ods=ODS_12_0
- RDB$ENTRYPOINT :: dtype_text len=255 sub=0 nullable=true default_blr=NULL field_id=fld_ext_name update=1 ods=ODS_12_0
- RDB$SQL_SECURITY :: dtype_boolean len=1 sub=0 nullable=true default_blr=NULL field_id=fld_b_sql_security update=1 ods=ODS_13_0
- RDB$SCHEMA_NAME :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_sch_name update=1 ods=ODS_14_0

Relation: RDB$DEPENDENCIES (rel_dpds, rel_persistent, ODS_8_0)
- RDB$DEPENDENT_NAME :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_gnr_name update=1 ods=ODS_8_0
- RDB$DEPENDED_ON_NAME :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_gnr_name update=1 ods=ODS_8_0
- RDB$FIELD_NAME :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_f_name update=1 ods=ODS_8_0
- RDB$DEPENDENT_TYPE :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_obj_type update=1 ods=ODS_8_0
- RDB$DEPENDED_ON_TYPE :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_obj_type update=1 ods=ODS_8_0
- RDB$PACKAGE_NAME :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_pkg_name update=1 ods=ODS_12_0
- RDB$DEPENDENT_SCHEMA_NAME :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_sch_name update=1 ods=ODS_14_0
- RDB$DEPENDED_ON_SCHEMA_NAME :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_sch_name update=1 ods=ODS_14_0

Relation: RDB$FUNCTIONS (rel_funs, rel_persistent, ODS_8_0)
- RDB$FUNCTION_NAME :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_fun_name update=1 ods=ODS_8_0
- RDB$FUNCTION_TYPE :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_fun_type update=1 ods=ODS_8_0
- RDB$QUERY_NAME :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_f_name update=1 ods=ODS_8_0
- RDB$DESCRIPTION :: dtype_blob len=BLOB_SIZE sub=isc_blob_text nullable=true default_blr=NULL field_id=fld_description update=1 ods=ODS_8_0
- RDB$MODULE_NAME :: dtype_varying len=255 sub=0 nullable=true default_blr=NULL field_id=fld_file_name update=1 ods=ODS_8_0
- RDB$ENTRYPOINT :: dtype_text len=255 sub=0 nullable=true default_blr=NULL field_id=fld_ext_name update=1 ods=ODS_8_0
- RDB$RETURN_ARGUMENT :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_f_position update=1 ods=ODS_8_0
- RDB$SYSTEM_FLAG :: dtype_short len=sizeof(SSHORT) sub=0 nullable=false default_blr=NULL field_id=fld_flag update=0 ods=ODS_8_0
- RDB$ENGINE_NAME :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_engine_name update=1 ods=ODS_12_0
- RDB$PACKAGE_NAME :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_pkg_name update=1 ods=ODS_12_0
- RDB$PRIVATE_FLAG :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_flag_nullable update=1 ods=ODS_12_0
- RDB$FUNCTION_SOURCE :: dtype_blob len=BLOB_SIZE sub=isc_blob_text nullable=true default_blr=NULL field_id=fld_source update=1 ods=ODS_12_0
- RDB$FUNCTION_ID :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_fun_id update=0 ods=ODS_12_0
- RDB$FUNCTION_BLR :: dtype_blob len=BLOB_SIZE sub=isc_blob_blr nullable=true default_blr=NULL field_id=fld_fun_blr update=1 ods=ODS_12_0
- RDB$VALID_BLR :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_flag_nullable update=1 ods=ODS_12_0
- RDB$DEBUG_INFO :: dtype_blob len=BLOB_SIZE sub=isc_blob_debug_info nullable=true default_blr=NULL field_id=fld_debug_info update=1 ods=ODS_12_0
- RDB$SECURITY_CLASS :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_class update=1 ods=ODS_12_0
- RDB$OWNER_NAME :: dtype_text len=USERNAME_LENGTH sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_user update=1 ods=ODS_12_0
- RDB$LEGACY_FLAG :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_flag_nullable update=0 ods=ODS_12_0
- RDB$DETERMINISTIC_FLAG :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_flag_nullable update=0 ods=ODS_12_0
- RDB$SQL_SECURITY :: dtype_boolean len=1 sub=0 nullable=true default_blr=NULL field_id=fld_b_sql_security update=1 ods=ODS_13_0
- RDB$SCHEMA_NAME :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_sch_name update=1 ods=ODS_14_0

Relation: RDB$FUNCTION_ARGUMENTS (rel_args, rel_persistent, ODS_8_0)
- RDB$FUNCTION_NAME :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_fun_name update=1 ods=ODS_8_0
- RDB$ARGUMENT_POSITION :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_f_position update=1 ods=ODS_8_0
- RDB$MECHANISM :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_mechanism update=1 ods=ODS_8_0
- RDB$FIELD_TYPE :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_f_type update=0 ods=ODS_8_0
- RDB$FIELD_SCALE :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_f_scale update=0 ods=ODS_8_0
- RDB$FIELD_LENGTH :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_f_length update=0 ods=ODS_8_0
- RDB$FIELD_SUB_TYPE :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_sub_type update=0 ods=ODS_8_0
- RDB$CHARACTER_SET_ID :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_charset_id update=1 ods=ODS_8_0
- RDB$FIELD_PRECISION :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_f_precision update=1 ods=ODS_10_0
- RDB$CHARACTER_LENGTH :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_f_length update=1 ods=ODS_10_0
- RDB$PACKAGE_NAME :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_pkg_name update=1 ods=ODS_12_0
- RDB$ARGUMENT_NAME :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_arg_name update=1 ods=ODS_12_0
- RDB$FIELD_SOURCE :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_f_name update=1 ods=ODS_12_0
- RDB$DEFAULT_VALUE :: dtype_blob len=BLOB_SIZE sub=isc_blob_blr nullable=true default_blr=NULL field_id=fld_value update=1 ods=ODS_12_0
- RDB$DEFAULT_SOURCE :: dtype_blob len=BLOB_SIZE sub=isc_blob_text nullable=true default_blr=NULL field_id=fld_source update=1 ods=ODS_12_0
- RDB$COLLATION_ID :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_collate_id update=1 ods=ODS_12_0
- RDB$NULL_FLAG :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_null_flag update=1 ods=ODS_12_0
- RDB$ARGUMENT_MECHANISM :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_arg_mechanism update=1 ods=ODS_12_0
- RDB$FIELD_NAME :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_f_name update=1 ods=ODS_12_0
- RDB$RELATION_NAME :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_r_name update=1 ods=ODS_12_0
- RDB$SYSTEM_FLAG :: dtype_short len=sizeof(SSHORT) sub=0 nullable=false default_blr=NULL field_id=fld_flag update=0 ods=ODS_12_0
- RDB$DESCRIPTION :: dtype_blob len=BLOB_SIZE sub=isc_blob_text nullable=true default_blr=NULL field_id=fld_description update=1 ods=ODS_12_0
- RDB$SCHEMA_NAME :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_sch_name update=1 ods=ODS_14_0
- RDB$RELATION_SCHEMA_NAME :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_sch_name update=1 ods=ODS_14_0
- RDB$FIELD_SOURCE_SCHEMA_NAME :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_sch_name update=1 ods=ODS_14_0

Relation: RDB$FILTERS (rel_filters, rel_persistent, ODS_8_0)
- RDB$FUNCTION_NAME :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_fun_name update=1 ods=ODS_8_0
- RDB$DESCRIPTION :: dtype_blob len=BLOB_SIZE sub=isc_blob_text nullable=true default_blr=NULL field_id=fld_description update=1 ods=ODS_8_0
- RDB$MODULE_NAME :: dtype_varying len=255 sub=0 nullable=true default_blr=NULL field_id=fld_file_name update=1 ods=ODS_8_0
- RDB$ENTRYPOINT :: dtype_text len=255 sub=0 nullable=true default_blr=NULL field_id=fld_ext_name update=1 ods=ODS_8_0
- RDB$INPUT_SUB_TYPE :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_sub_type update=1 ods=ODS_8_0
- RDB$OUTPUT_SUB_TYPE :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_sub_type update=1 ods=ODS_8_0
- RDB$SYSTEM_FLAG :: dtype_short len=sizeof(SSHORT) sub=0 nullable=false default_blr=NULL field_id=fld_flag update=0 ods=ODS_8_0
- RDB$SECURITY_CLASS :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_class update=1 ods=ODS_12_0
- RDB$OWNER_NAME :: dtype_text len=USERNAME_LENGTH sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_user update=1 ods=ODS_12_0

Relation: RDB$TRIGGER_MESSAGES (rel_msgs, rel_persistent, ODS_8_0)
- RDB$TRIGGER_NAME :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_trg_name update=1 ods=ODS_8_0
- RDB$MESSAGE_NUMBER :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_msg_num update=1 ods=ODS_8_0
- RDB$MESSAGE :: dtype_varying len=1023 sub=0 nullable=true default_blr=NULL field_id=fld_msg update=1 ods=ODS_8_0
- RDB$SCHEMA_NAME :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_sch_name update=1 ods=ODS_14_0

Relation: RDB$USER_PRIVILEGES (rel_priv, rel_persistent, ODS_8_0)
- RDB$USER :: dtype_text len=USERNAME_LENGTH sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_user update=1 ods=ODS_8_0
- RDB$GRANTOR :: dtype_text len=USERNAME_LENGTH sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_user update=1 ods=ODS_8_0
- RDB$PRIVILEGE :: dtype_text len=6 sub=0 nullable=true default_blr=NULL field_id=fld_privilege update=1 ods=ODS_8_0
- RDB$GRANT_OPTION :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_flag_nullable update=1 ods=ODS_8_0
- RDB$RELATION_NAME :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_gnr_name update=1 ods=ODS_8_0
- RDB$FIELD_NAME :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_f_name update=1 ods=ODS_8_0
- RDB$USER_TYPE :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_obj_type update=1 ods=ODS_8_0
- RDB$OBJECT_TYPE :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_obj_type update=1 ods=ODS_8_0
- RDB$RELATION_SCHEMA_NAME :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_sch_name update=1 ods=ODS_14_0
- RDB$USER_SCHEMA_NAME :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_sch_name update=1 ods=ODS_14_0

Relation: RDB$TRANSACTIONS (rel_trans, rel_persistent, ODS_8_0)
- RDB$TRANSACTION_ID :: dtype_int64 len=sizeof(SINT64) sub=0 nullable=true default_blr=NULL field_id=fld_trans_id update=1 ods=ODS_8_0
- RDB$TRANSACTION_STATE :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_trans_state update=1 ods=ODS_8_0
- RDB$TIMESTAMP :: dtype_timestamp_tz len=TIMESTAMP_TZ_SIZE sub=0 nullable=true default_blr=NULL field_id=fld_timestamp_tz update=1 ods=ODS_8_0
- RDB$TRANSACTION_DESCRIPTION :: dtype_blob len=BLOB_SIZE sub=isc_blob_tra nullable=true default_blr=NULL field_id=fld_trans_desc update=1 ods=ODS_8_0

Relation: RDB$GENERATORS (rel_gens, rel_persistent, ODS_8_0)
- RDB$GENERATOR_NAME :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_gen_name update=1 ods=ODS_8_0
- RDB$GENERATOR_ID :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_gen_id update=1 ods=ODS_8_0
- RDB$SYSTEM_FLAG :: dtype_short len=sizeof(SSHORT) sub=0 nullable=false default_blr=NULL field_id=fld_flag update=1 ods=ODS_8_0
- RDB$DESCRIPTION :: dtype_blob len=BLOB_SIZE sub=isc_blob_text nullable=true default_blr=NULL field_id=fld_description update=1 ods=ODS_11_0
- RDB$SECURITY_CLASS :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_class update=1 ods=ODS_12_0
- RDB$OWNER_NAME :: dtype_text len=USERNAME_LENGTH sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_user update=1 ods=ODS_12_0
- RDB$INITIAL_VALUE :: dtype_int64 len=sizeof(SINT64) sub=0 nullable=true default_blr=NULL field_id=fld_gen_val update=1 ods=ODS_12_0
- RDB$GENERATOR_INCREMENT :: dtype_long len=sizeof(SLONG) sub=0 nullable=false default_blr=NULL field_id=fld_gen_increment update=1 ods=ODS_12_0
- RDB$SCHEMA_NAME :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_sch_name update=1 ods=ODS_14_0

Relation: RDB$FIELD_DIMENSIONS (rel_dims, rel_persistent, ODS_8_0)
- RDB$FIELD_NAME :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_f_name update=1 ods=ODS_8_0
- RDB$DIMENSION :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_dim update=1 ods=ODS_8_0
- RDB$LOWER_BOUND :: dtype_long len=sizeof(SLONG) sub=0 nullable=true default_blr=NULL field_id=fld_bound update=1 ods=ODS_8_0
- RDB$UPPER_BOUND :: dtype_long len=sizeof(SLONG) sub=0 nullable=true default_blr=NULL field_id=fld_bound update=1 ods=ODS_8_0
- RDB$SCHEMA_NAME :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_sch_name update=1 ods=ODS_14_0

Relation: RDB$RELATION_CONSTRAINTS (rel_rcon, rel_persistent, ODS_8_0)
- RDB$CONSTRAINT_NAME :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_con_name update=1 ods=ODS_8_0
- RDB$CONSTRAINT_TYPE :: dtype_text len=11 sub=0 nullable=true default_blr=NULL field_id=fld_con_type update=1 ods=ODS_8_0
- RDB$RELATION_NAME :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_r_name update=1 ods=ODS_8_0
- RDB$DEFERRABLE :: dtype_text len=3 sub=0 nullable=true default_blr=dflt_no field_id=fld_defer update=1 ods=ODS_8_0
- RDB$INITIALLY_DEFERRED :: dtype_text len=3 sub=0 nullable=true default_blr=dflt_no field_id=fld_defer update=1 ods=ODS_8_0
- RDB$INDEX_NAME :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_i_name update=1 ods=ODS_8_0
- RDB$SCHEMA_NAME :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_sch_name update=1 ods=ODS_14_0

Relation: RDB$REF_CONSTRAINTS (rel_refc, rel_persistent, ODS_8_0)
- RDB$CONSTRAINT_NAME :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_con_name update=1 ods=ODS_8_0
- RDB$CONST_NAME_UQ :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_con_name update=1 ods=ODS_8_0
- RDB$MATCH_OPTION :: dtype_text len=7 sub=0 nullable=true default_blr=dflt_full field_id=fld_match update=1 ods=ODS_8_0
- RDB$UPDATE_RULE :: dtype_text len=11 sub=0 nullable=true default_blr=dflt_restrict field_id=fld_rule update=1 ods=ODS_8_0
- RDB$DELETE_RULE :: dtype_text len=11 sub=0 nullable=true default_blr=dflt_restrict field_id=fld_rule update=1 ods=ODS_8_0
- RDB$SCHEMA_NAME :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_sch_name update=1 ods=ODS_14_0
- RDB$CONST_SCHEMA_NAME_UQ :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_sch_name update=1 ods=ODS_14_0

Relation: RDB$CHECK_CONSTRAINTS (rel_ccon, rel_persistent, ODS_8_0)
- RDB$CONSTRAINT_NAME :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_con_name update=1 ods=ODS_8_0
- RDB$TRIGGER_NAME :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_trg_name update=1 ods=ODS_8_0
- RDB$SCHEMA_NAME :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_sch_name update=1 ods=ODS_14_0

Relation: RDB$LOG_FILES (rel_log, rel_persistent, ODS_8_0)
- RDB$FILE_NAME :: dtype_varying len=255 sub=0 nullable=true default_blr=NULL field_id=fld_file_name update=1 ods=ODS_8_0
- RDB$FILE_SEQUENCE :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_file_seq update=1 ods=ODS_8_0
- RDB$FILE_LENGTH :: dtype_long len=sizeof(SLONG) sub=0 nullable=true default_blr=NULL field_id=fld_file_length update=1 ods=ODS_8_0
- RDB$FILE_PARTITIONS :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_file_partitions update=1 ods=ODS_8_0
- RDB$FILE_P_OFFSET :: dtype_long len=sizeof(SLONG) sub=0 nullable=true default_blr=NULL field_id=fld_file_p_offset update=1 ods=ODS_8_0
- RDB$FILE_FLAGS :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_file_flags update=1 ods=ODS_8_0

Relation: RDB$PROCEDURES (rel_procedures, rel_persistent, ODS_8_0)
- RDB$PROCEDURE_NAME :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_prc_name update=1 ods=ODS_8_0
- RDB$PROCEDURE_ID :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_prc_id update=0 ods=ODS_8_0
- RDB$PROCEDURE_INPUTS :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_prc_prm update=1 ods=ODS_8_0
- RDB$PROCEDURE_OUTPUTS :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_prc_prm update=1 ods=ODS_8_0
- RDB$DESCRIPTION :: dtype_blob len=BLOB_SIZE sub=isc_blob_text nullable=true default_blr=NULL field_id=fld_description update=1 ods=ODS_8_0
- RDB$PROCEDURE_SOURCE :: dtype_blob len=BLOB_SIZE sub=isc_blob_text nullable=true default_blr=NULL field_id=fld_source update=1 ods=ODS_8_0
- RDB$PROCEDURE_BLR :: dtype_blob len=BLOB_SIZE sub=isc_blob_blr nullable=true default_blr=NULL field_id=fld_prc_blr update=1 ods=ODS_8_0
- RDB$SECURITY_CLASS :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_class update=1 ods=ODS_8_0
- RDB$OWNER_NAME :: dtype_text len=USERNAME_LENGTH sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_user update=1 ods=ODS_8_0
- RDB$RUNTIME :: dtype_blob len=BLOB_SIZE sub=isc_blob_summary nullable=true default_blr=NULL field_id=fld_runtime update=1 ods=ODS_8_0
- RDB$SYSTEM_FLAG :: dtype_short len=sizeof(SSHORT) sub=0 nullable=false default_blr=NULL field_id=fld_flag update=0 ods=ODS_8_0
- RDB$PROCEDURE_TYPE :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_prc_type update=1 ods=ODS_11_1
- RDB$VALID_BLR :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_flag_nullable update=1 ods=ODS_11_1
- RDB$DEBUG_INFO :: dtype_blob len=BLOB_SIZE sub=isc_blob_debug_info nullable=true default_blr=NULL field_id=fld_debug_info update=1 ods=ODS_11_1
- RDB$ENGINE_NAME :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_engine_name update=1 ods=ODS_12_0
- RDB$ENTRYPOINT :: dtype_text len=255 sub=0 nullable=true default_blr=NULL field_id=fld_ext_name update=1 ods=ODS_12_0
- RDB$PACKAGE_NAME :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_pkg_name update=1 ods=ODS_12_0
- RDB$PRIVATE_FLAG :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_flag_nullable update=1 ods=ODS_12_0
- RDB$SQL_SECURITY :: dtype_boolean len=1 sub=0 nullable=true default_blr=NULL field_id=fld_b_sql_security update=1 ods=ODS_13_0
- RDB$SCHEMA_NAME :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_sch_name update=1 ods=ODS_14_0

Relation: RDB$PROCEDURE_PARAMETERS (rel_prc_prms, rel_persistent, ODS_8_0)
- RDB$PARAMETER_NAME :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_prm_name update=1 ods=ODS_8_0
- RDB$PROCEDURE_NAME :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_prc_name update=1 ods=ODS_8_0
- RDB$PARAMETER_NUMBER :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_prm_number update=1 ods=ODS_8_0
- RDB$PARAMETER_TYPE :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_prm_type update=1 ods=ODS_8_0
- RDB$FIELD_SOURCE :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_f_name update=1 ods=ODS_8_0
- RDB$DESCRIPTION :: dtype_blob len=BLOB_SIZE sub=isc_blob_text nullable=true default_blr=NULL field_id=fld_description update=1 ods=ODS_8_0
- RDB$SYSTEM_FLAG :: dtype_short len=sizeof(SSHORT) sub=0 nullable=false default_blr=NULL field_id=fld_flag update=0 ods=ODS_8_0
- RDB$DEFAULT_VALUE :: dtype_blob len=BLOB_SIZE sub=isc_blob_blr nullable=true default_blr=NULL field_id=fld_value update=1 ods=ODS_11_1
- RDB$DEFAULT_SOURCE :: dtype_blob len=BLOB_SIZE sub=isc_blob_text nullable=true default_blr=NULL field_id=fld_source update=1 ods=ODS_11_1
- RDB$COLLATION_ID :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_collate_id update=1 ods=ODS_11_1
- RDB$NULL_FLAG :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_null_flag update=1 ods=ODS_11_1
- RDB$PARAMETER_MECHANISM :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_mechanism update=1 ods=ODS_11_1
- RDB$FIELD_NAME :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_f_name update=1 ods=ODS_11_2
- RDB$RELATION_NAME :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_r_name update=1 ods=ODS_11_2
- RDB$PACKAGE_NAME :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_pkg_name update=1 ods=ODS_12_0
- RDB$SCHEMA_NAME :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_sch_name update=1 ods=ODS_14_0
- RDB$RELATION_SCHEMA_NAME :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_sch_name update=1 ods=ODS_14_0
- RDB$FIELD_SOURCE_SCHEMA_NAME :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_sch_name update=1 ods=ODS_14_0

Relation: RDB$CHARACTER_SETS (rel_charsets, rel_persistent, ODS_8_0)
- RDB$CHARACTER_SET_NAME :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_charset_name update=1 ods=ODS_8_0
- RDB$FORM_OF_USE :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_gnr_name update=1 ods=ODS_8_0
- RDB$NUMBER_OF_CHARACTERS :: dtype_long len=sizeof(SLONG) sub=0 nullable=true default_blr=NULL field_id=fld_num_chars update=1 ods=ODS_8_0
- RDB$DEFAULT_COLLATE_NAME :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_collate_name update=1 ods=ODS_8_0
- RDB$CHARACTER_SET_ID :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_charset_id update=1 ods=ODS_8_0
- RDB$SYSTEM_FLAG :: dtype_short len=sizeof(SSHORT) sub=0 nullable=false default_blr=NULL field_id=fld_flag update=0 ods=ODS_8_0
- RDB$DESCRIPTION :: dtype_blob len=BLOB_SIZE sub=isc_blob_text nullable=true default_blr=NULL field_id=fld_description update=1 ods=ODS_8_0
- RDB$FUNCTION_NAME :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_fun_name update=1 ods=ODS_8_0
- RDB$BYTES_PER_CHARACTER :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_f_length update=1 ods=ODS_8_0
- RDB$SECURITY_CLASS :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_class update=1 ods=ODS_12_0
- RDB$OWNER_NAME :: dtype_text len=USERNAME_LENGTH sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_user update=1 ods=ODS_12_0
- RDB$SCHEMA_NAME :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_sch_name update=1 ods=ODS_14_0
- RDB$DEFAULT_COLLATE_SCHEMA_NAME :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_sch_name update=1 ods=ODS_14_0

Relation: RDB$COLLATIONS (rel_collations, rel_persistent, ODS_8_0)
- RDB$COLLATION_NAME :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_collate_name update=1 ods=ODS_8_0
- RDB$COLLATION_ID :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_collate_id update=1 ods=ODS_8_0
- RDB$CHARACTER_SET_ID :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_charset_id update=1 ods=ODS_8_0
- RDB$COLLATION_ATTRIBUTES :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_gnr_type update=1 ods=ODS_8_0
- RDB$SYSTEM_FLAG :: dtype_short len=sizeof(SSHORT) sub=0 nullable=false default_blr=NULL field_id=fld_flag update=0 ods=ODS_8_0
- RDB$DESCRIPTION :: dtype_blob len=BLOB_SIZE sub=isc_blob_text nullable=true default_blr=NULL field_id=fld_description update=1 ods=ODS_8_0
- RDB$FUNCTION_NAME :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_fun_name update=1 ods=ODS_8_0
- RDB$BASE_COLLATION_NAME :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_collate_name update=1 ods=ODS_11_0
- RDB$SPECIFIC_ATTRIBUTES :: dtype_blob len=BLOB_SIZE sub=isc_blob_text nullable=true default_blr=NULL field_id=fld_specific_attr update=1 ods=ODS_11_0
- RDB$SECURITY_CLASS :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_class update=1 ods=ODS_12_0
- RDB$OWNER_NAME :: dtype_text len=USERNAME_LENGTH sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_user update=1 ods=ODS_12_0
- RDB$SCHEMA_NAME :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_sch_name update=1 ods=ODS_14_0

Relation: RDB$EXCEPTIONS (rel_exceptions, rel_persistent, ODS_8_0)
- RDB$EXCEPTION_NAME :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_xcp_name update=1 ods=ODS_8_0
- RDB$EXCEPTION_NUMBER :: dtype_long len=sizeof(SLONG) sub=0 nullable=true default_blr=NULL field_id=fld_xcp_number update=1 ods=ODS_8_0
- RDB$MESSAGE :: dtype_varying len=1023 sub=0 nullable=true default_blr=NULL field_id=fld_msg update=1 ods=ODS_8_0
- RDB$DESCRIPTION :: dtype_blob len=BLOB_SIZE sub=isc_blob_text nullable=true default_blr=NULL field_id=fld_description update=1 ods=ODS_8_0
- RDB$SYSTEM_FLAG :: dtype_short len=sizeof(SSHORT) sub=0 nullable=false default_blr=NULL field_id=fld_flag update=1 ods=ODS_8_0
- RDB$SECURITY_CLASS :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_class update=1 ods=ODS_12_0
- RDB$OWNER_NAME :: dtype_text len=USERNAME_LENGTH sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_user update=1 ods=ODS_12_0
- RDB$SCHEMA_NAME :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_sch_name update=1 ods=ODS_14_0

Relation: RDB$ROLES (rel_roles, rel_persistent, ODS_9_0)
- RDB$ROLE_NAME :: dtype_text len=USERNAME_LENGTH sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_user update=1 ods=ODS_9_0
- RDB$OWNER_NAME :: dtype_text len=USERNAME_LENGTH sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_user update=1 ods=ODS_9_0
- RDB$DESCRIPTION :: dtype_blob len=BLOB_SIZE sub=isc_blob_text nullable=true default_blr=NULL field_id=fld_description update=1 ods=ODS_11_0
- RDB$SYSTEM_FLAG :: dtype_short len=sizeof(SSHORT) sub=0 nullable=false default_blr=NULL field_id=fld_flag update=1 ods=ODS_11_0
- RDB$SECURITY_CLASS :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_class update=1 ods=ODS_12_0
- RDB$SYSTEM_PRIVILEGES :: dtype_text len=8 sub=dsc_text_type_fixed nullable=true default_blr=dflt_no_privs field_id=fld_system_privileges update=1 ods=ODS_13_0

Relation: RDB$BACKUP_HISTORY (rel_backup_history, rel_persistent, ODS_11_0)
- RDB$BACKUP_ID :: dtype_long len=sizeof(SLONG) sub=0 nullable=true default_blr=NULL field_id=fld_backup_id update=1 ods=ODS_11_0
- RDB$TIMESTAMP :: dtype_timestamp_tz len=TIMESTAMP_TZ_SIZE sub=0 nullable=true default_blr=NULL field_id=fld_timestamp_tz update=1 ods=ODS_11_0
- RDB$BACKUP_LEVEL :: dtype_long len=sizeof(SLONG) sub=0 nullable=true default_blr=NULL field_id=fld_backup_level update=1 ods=ODS_11_0
- RDB$GUID :: dtype_text len=38 sub=0 nullable=true default_blr=NULL field_id=fld_guid update=1 ods=ODS_11_0
- RDB$SCN :: dtype_long len=sizeof(SLONG) sub=0 nullable=true default_blr=NULL field_id=fld_scn update=1 ods=ODS_11_0
- RDB$FILE_NAME :: dtype_varying len=255 sub=0 nullable=true default_blr=NULL field_id=fld_file_name update=1 ods=ODS_11_0

Relation: MON$DATABASE (rel_mon_database, rel_virtual, ODS_11_1)
- MON$DATABASE_NAME :: dtype_varying len=255 * METADATA_BYTES_PER_CHAR sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_file_name2 update=0 ods=ODS_11_1
- MON$PAGE_SIZE :: dtype_long len=sizeof(SLONG) sub=0 nullable=true default_blr=NULL field_id=fld_page_size update=0 ods=ODS_11_1
- MON$ODS_MAJOR :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_ods_number update=0 ods=ODS_11_1
- MON$ODS_MINOR :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_ods_number update=0 ods=ODS_11_1
- MON$OLDEST_TRANSACTION :: dtype_int64 len=sizeof(SINT64) sub=0 nullable=true default_blr=NULL field_id=fld_trans_id update=0 ods=ODS_11_1
- MON$OLDEST_ACTIVE :: dtype_int64 len=sizeof(SINT64) sub=0 nullable=true default_blr=NULL field_id=fld_trans_id update=0 ods=ODS_11_1
- MON$OLDEST_SNAPSHOT :: dtype_int64 len=sizeof(SINT64) sub=0 nullable=true default_blr=NULL field_id=fld_trans_id update=0 ods=ODS_11_1
- MON$NEXT_TRANSACTION :: dtype_int64 len=sizeof(SINT64) sub=0 nullable=true default_blr=NULL field_id=fld_trans_id update=0 ods=ODS_11_1
- MON$PAGE_BUFFERS :: dtype_long len=sizeof(SLONG) sub=0 nullable=true default_blr=NULL field_id=fld_page_bufs update=0 ods=ODS_11_1
- MON$SQL_DIALECT :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_sql_dialect update=0 ods=ODS_11_1
- MON$SHUTDOWN_MODE :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_shut_mode update=0 ods=ODS_11_1
- MON$SWEEP_INTERVAL :: dtype_long len=sizeof(SLONG) sub=0 nullable=true default_blr=NULL field_id=fld_sweep_int update=0 ods=ODS_11_1
- MON$READ_ONLY :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_flag_nullable update=0 ods=ODS_11_1
- MON$FORCED_WRITES :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_flag_nullable update=0 ods=ODS_11_1
- MON$RESERVE_SPACE :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_flag_nullable update=0 ods=ODS_11_1
- MON$CREATION_DATE :: dtype_timestamp_tz len=TIMESTAMP_TZ_SIZE sub=0 nullable=true default_blr=NULL field_id=fld_timestamp_tz update=0 ods=ODS_11_1
- MON$PAGES :: dtype_int64 len=sizeof(SINT64) sub=0 nullable=true default_blr=NULL field_id=fld_counter update=0 ods=ODS_11_1
- MON$STAT_ID :: dtype_long len=sizeof(SLONG) sub=0 nullable=true default_blr=NULL field_id=fld_stat_id update=0 ods=ODS_11_1
- MON$BACKUP_STATE :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_backup_state update=0 ods=ODS_11_1
- MON$CRYPT_PAGE :: dtype_int64 len=sizeof(SINT64) sub=0 nullable=true default_blr=NULL field_id=fld_counter update=0 ods=ODS_12_0
- MON$OWNER :: dtype_text len=USERNAME_LENGTH sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_user update=0 ods=ODS_12_0
- MON$SEC_DATABASE :: dtype_text len=7 sub=dsc_text_type_ascii nullable=false default_blr=NULL field_id=fld_sec_db update=0 ods=ODS_12_0
- MON$CRYPT_STATE :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_crypt_state update=0 ods=ODS_13_0
- MON$GUID :: dtype_text len=38 sub=0 nullable=true default_blr=NULL field_id=fld_guid update=0 ods=ODS_13_0
- MON$FILE_ID :: dtype_varying len=255 sub=dsc_text_type_ascii nullable=false default_blr=NULL field_id=fld_file_id update=0 ods=ODS_13_0
- MON$NEXT_ATTACHMENT :: dtype_int64 len=sizeof(SINT64) sub=0 nullable=true default_blr=NULL field_id=fld_att_id update=0 ods=ODS_13_0
- MON$NEXT_STATEMENT :: dtype_int64 len=sizeof(SINT64) sub=0 nullable=true default_blr=NULL field_id=fld_stmt_id update=0 ods=ODS_13_0
- MON$REPLICA_MODE :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_repl_mode update=0 ods=ODS_13_0

Relation: MON$ATTACHMENTS (rel_mon_attachments, rel_virtual, ODS_11_1)
- MON$ATTACHMENT_ID :: dtype_int64 len=sizeof(SINT64) sub=0 nullable=true default_blr=NULL field_id=fld_att_id update=0 ods=ODS_11_1
- MON$SERVER_PID :: dtype_long len=sizeof(SLONG) sub=0 nullable=true default_blr=NULL field_id=fld_pid update=0 ods=ODS_11_1
- MON$STATE :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_state update=0 ods=ODS_11_1
- MON$ATTACHMENT_NAME :: dtype_varying len=255 * METADATA_BYTES_PER_CHAR sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_file_name2 update=0 ods=ODS_11_1
- MON$USER :: dtype_text len=USERNAME_LENGTH sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_user update=0 ods=ODS_11_1
- MON$ROLE :: dtype_text len=USERNAME_LENGTH sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_user update=0 ods=ODS_11_1
- MON$REMOTE_PROTOCOL :: dtype_varying len=10 sub=dsc_text_type_ascii nullable=true default_blr=NULL field_id=fld_remote_proto update=0 ods=ODS_11_1
- MON$REMOTE_ADDRESS :: dtype_varying len=255 sub=dsc_text_type_ascii nullable=true default_blr=NULL field_id=fld_remote_addr update=0 ods=ODS_11_1
- MON$REMOTE_PID :: dtype_long len=sizeof(SLONG) sub=0 nullable=true default_blr=NULL field_id=fld_pid update=0 ods=ODS_11_1
- MON$CHARACTER_SET_ID :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_charset_id update=0 ods=ODS_11_1
- MON$TIMESTAMP :: dtype_timestamp_tz len=TIMESTAMP_TZ_SIZE sub=0 nullable=true default_blr=NULL field_id=fld_timestamp_tz update=0 ods=ODS_11_1
- MON$GARBAGE_COLLECTION :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_flag_nullable update=0 ods=ODS_11_1
- MON$REMOTE_PROCESS :: dtype_varying len=255 * METADATA_BYTES_PER_CHAR sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_file_name2 update=0 ods=ODS_11_1
- MON$STAT_ID :: dtype_long len=sizeof(SLONG) sub=0 nullable=true default_blr=NULL field_id=fld_stat_id update=0 ods=ODS_11_1
- MON$CLIENT_VERSION :: dtype_varying len=255 sub=dsc_text_type_ascii nullable=true default_blr=NULL field_id=fld_client_ver update=0 ods=ODS_12_0
- MON$REMOTE_VERSION :: dtype_varying len=255 sub=dsc_text_type_ascii nullable=true default_blr=NULL field_id=fld_remote_ver update=0 ods=ODS_12_0
- MON$REMOTE_HOST :: dtype_varying len=255 * METADATA_BYTES_PER_CHAR sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_host_name update=0 ods=ODS_12_0
- MON$REMOTE_OS_USER :: dtype_varying len=255 * METADATA_BYTES_PER_CHAR sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_os_user update=0 ods=ODS_12_0
- MON$AUTH_METHOD :: dtype_varying len=255 sub=dsc_text_type_ascii nullable=true default_blr=NULL field_id=fld_auth_method update=0 ods=ODS_12_0
- MON$SYSTEM_FLAG :: dtype_short len=sizeof(SSHORT) sub=0 nullable=false default_blr=NULL field_id=fld_flag update=0 ods=ODS_12_0
- MON$IDLE_TIMEOUT :: dtype_long len=sizeof(SLONG) sub=0 nullable=false default_blr=NULL field_id=fld_idle_timeout update=0 ods=ODS_13_0
- MON$IDLE_TIMER :: dtype_timestamp_tz len=TIMESTAMP_TZ_SIZE sub=0 nullable=true default_blr=NULL field_id=fld_idle_timer update=0 ods=ODS_13_0
- MON$STATEMENT_TIMEOUT :: dtype_long len=sizeof(SLONG) sub=0 nullable=false default_blr=NULL field_id=fld_stmt_timeout update=0 ods=ODS_13_0
- MON$WIRE_COMPRESSED :: dtype_boolean len=1 sub=0 nullable=true default_blr=NULL field_id=fld_bool update=0 ods=ODS_13_0
- MON$WIRE_ENCRYPTED :: dtype_boolean len=1 sub=0 nullable=true default_blr=NULL field_id=fld_bool update=0 ods=ODS_13_0
- MON$WIRE_CRYPT_PLUGIN :: dtype_varying len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_remote_crypt update=0 ods=ODS_13_0
- MON$SESSION_TIMEZONE :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_tz_name update=0 ods=ODS_13_1
- MON$PARALLEL_WORKERS :: dtype_long len=sizeof(SLONG) sub=0 nullable=true default_blr=NULL field_id=fld_par_workers update=0 ods=ODS_13_1
- MON$SEARCH_PATH :: dtype_varying len=MAX_VARY_COLUMN_SIZE / METADATA_BYTES_PER_CHAR * METADATA_BYTES_PER_CHAR sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_text_max update=0 ods=ODS_14_0

Relation: MON$TRANSACTIONS (rel_mon_transactions, rel_virtual, ODS_11_1)
- MON$TRANSACTION_ID :: dtype_int64 len=sizeof(SINT64) sub=0 nullable=true default_blr=NULL field_id=fld_trans_id update=0 ods=ODS_11_1
- MON$ATTACHMENT_ID :: dtype_int64 len=sizeof(SINT64) sub=0 nullable=true default_blr=NULL field_id=fld_att_id update=0 ods=ODS_11_1
- MON$STATE :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_state update=0 ods=ODS_11_1
- MON$TIMESTAMP :: dtype_timestamp_tz len=TIMESTAMP_TZ_SIZE sub=0 nullable=true default_blr=NULL field_id=fld_timestamp_tz update=0 ods=ODS_11_1
- MON$TOP_TRANSACTION :: dtype_int64 len=sizeof(SINT64) sub=0 nullable=true default_blr=NULL field_id=fld_trans_id update=0 ods=ODS_11_1
- MON$OLDEST_TRANSACTION :: dtype_int64 len=sizeof(SINT64) sub=0 nullable=true default_blr=NULL field_id=fld_trans_id update=0 ods=ODS_11_1
- MON$OLDEST_ACTIVE :: dtype_int64 len=sizeof(SINT64) sub=0 nullable=true default_blr=NULL field_id=fld_trans_id update=0 ods=ODS_11_1
- MON$ISOLATION_MODE :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_iso_mode update=0 ods=ODS_11_1
- MON$LOCK_TIMEOUT :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_lock_timeout update=0 ods=ODS_11_1
- MON$READ_ONLY :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_flag_nullable update=0 ods=ODS_11_1
- MON$AUTO_COMMIT :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_flag_nullable update=0 ods=ODS_11_1
- MON$AUTO_UNDO :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_flag_nullable update=0 ods=ODS_11_1
- MON$STAT_ID :: dtype_long len=sizeof(SLONG) sub=0 nullable=true default_blr=NULL field_id=fld_stat_id update=0 ods=ODS_11_1
- MON$AUTO_RELEASE_TEMP_BLOBID :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_flag_nullable update=0 ods=ODS_14_0

Relation: MON$STATEMENTS (rel_mon_statements, rel_virtual, ODS_11_1)
- MON$STATEMENT_ID :: dtype_int64 len=sizeof(SINT64) sub=0 nullable=true default_blr=NULL field_id=fld_stmt_id update=0 ods=ODS_11_1
- MON$ATTACHMENT_ID :: dtype_int64 len=sizeof(SINT64) sub=0 nullable=true default_blr=NULL field_id=fld_att_id update=0 ods=ODS_11_1
- MON$TRANSACTION_ID :: dtype_int64 len=sizeof(SINT64) sub=0 nullable=true default_blr=NULL field_id=fld_trans_id update=0 ods=ODS_11_1
- MON$STATE :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_state update=0 ods=ODS_11_1
- MON$TIMESTAMP :: dtype_timestamp_tz len=TIMESTAMP_TZ_SIZE sub=0 nullable=true default_blr=NULL field_id=fld_timestamp_tz update=0 ods=ODS_11_1
- MON$SQL_TEXT :: dtype_blob len=BLOB_SIZE sub=isc_blob_text nullable=true default_blr=NULL field_id=fld_source update=0 ods=ODS_11_1
- MON$STAT_ID :: dtype_long len=sizeof(SLONG) sub=0 nullable=true default_blr=NULL field_id=fld_stat_id update=0 ods=ODS_11_1
- MON$EXPLAINED_PLAN :: dtype_blob len=BLOB_SIZE sub=isc_blob_text nullable=true default_blr=NULL field_id=fld_source update=0 ods=ODS_11_1
- MON$STATEMENT_TIMEOUT :: dtype_long len=sizeof(SLONG) sub=0 nullable=false default_blr=NULL field_id=fld_stmt_timeout update=0 ods=ODS_13_0
- MON$STATEMENT_TIMER :: dtype_timestamp_tz len=TIMESTAMP_TZ_SIZE sub=0 nullable=true default_blr=NULL field_id=fld_stmt_timer update=0 ods=ODS_13_0
- MON$COMPILED_STATEMENT_ID :: dtype_int64 len=sizeof(SINT64) sub=0 nullable=true default_blr=NULL field_id=fld_stmt_id update=0 ods=ODS_13_1

Relation: MON$CALL_STACK (rel_mon_calls, rel_virtual, ODS_11_1)
- MON$CALL_ID :: dtype_int64 len=sizeof(SINT64) sub=0 nullable=true default_blr=NULL field_id=fld_call_id update=0 ods=ODS_11_1
- MON$STATEMENT_ID :: dtype_int64 len=sizeof(SINT64) sub=0 nullable=true default_blr=NULL field_id=fld_stmt_id update=0 ods=ODS_11_1
- MON$CALLER_ID :: dtype_int64 len=sizeof(SINT64) sub=0 nullable=true default_blr=NULL field_id=fld_call_id update=0 ods=ODS_11_1
- MON$OBJECT_NAME :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_gnr_name update=0 ods=ODS_11_1
- MON$OBJECT_TYPE :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_obj_type update=0 ods=ODS_11_1
- MON$TIMESTAMP :: dtype_timestamp_tz len=TIMESTAMP_TZ_SIZE sub=0 nullable=true default_blr=NULL field_id=fld_timestamp_tz update=0 ods=ODS_11_1
- MON$SOURCE_LINE :: dtype_long len=sizeof(SLONG) sub=0 nullable=true default_blr=NULL field_id=fld_src_info update=0 ods=ODS_11_1
- MON$SOURCE_COLUMN :: dtype_long len=sizeof(SLONG) sub=0 nullable=true default_blr=NULL field_id=fld_src_info update=0 ods=ODS_11_1
- MON$STAT_ID :: dtype_long len=sizeof(SLONG) sub=0 nullable=true default_blr=NULL field_id=fld_stat_id update=0 ods=ODS_11_1
- MON$PACKAGE_NAME :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_pkg_name update=0 ods=ODS_12_0
- MON$COMPILED_STATEMENT_ID :: dtype_int64 len=sizeof(SINT64) sub=0 nullable=true default_blr=NULL field_id=fld_stmt_id update=0 ods=ODS_13_1
- MON$SCHEMA_NAME :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_sch_name update=0 ods=ODS_14_0

Relation: MON$IO_STATS (rel_mon_io_stats, rel_virtual, ODS_11_1)
- MON$STAT_ID :: dtype_long len=sizeof(SLONG) sub=0 nullable=true default_blr=NULL field_id=fld_stat_id update=0 ods=ODS_11_1
- MON$STAT_GROUP :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_stat_group update=0 ods=ODS_11_1
- MON$PAGE_READS :: dtype_int64 len=sizeof(SINT64) sub=0 nullable=true default_blr=NULL field_id=fld_counter update=0 ods=ODS_11_1
- MON$PAGE_WRITES :: dtype_int64 len=sizeof(SINT64) sub=0 nullable=true default_blr=NULL field_id=fld_counter update=0 ods=ODS_11_1
- MON$PAGE_FETCHES :: dtype_int64 len=sizeof(SINT64) sub=0 nullable=true default_blr=NULL field_id=fld_counter update=0 ods=ODS_11_1
- MON$PAGE_MARKS :: dtype_int64 len=sizeof(SINT64) sub=0 nullable=true default_blr=NULL field_id=fld_counter update=0 ods=ODS_11_1

Relation: MON$RECORD_STATS (rel_mon_rec_stats, rel_virtual, ODS_11_1)
- MON$STAT_ID :: dtype_long len=sizeof(SLONG) sub=0 nullable=true default_blr=NULL field_id=fld_stat_id update=0 ods=ODS_11_1
- MON$STAT_GROUP :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_stat_group update=0 ods=ODS_11_1
- MON$RECORD_SEQ_READS :: dtype_int64 len=sizeof(SINT64) sub=0 nullable=true default_blr=NULL field_id=fld_counter update=0 ods=ODS_11_1
- MON$RECORD_IDX_READS :: dtype_int64 len=sizeof(SINT64) sub=0 nullable=true default_blr=NULL field_id=fld_counter update=0 ods=ODS_11_1
- MON$RECORD_INSERTS :: dtype_int64 len=sizeof(SINT64) sub=0 nullable=true default_blr=NULL field_id=fld_counter update=0 ods=ODS_11_1
- MON$RECORD_UPDATES :: dtype_int64 len=sizeof(SINT64) sub=0 nullable=true default_blr=NULL field_id=fld_counter update=0 ods=ODS_11_1
- MON$RECORD_DELETES :: dtype_int64 len=sizeof(SINT64) sub=0 nullable=true default_blr=NULL field_id=fld_counter update=0 ods=ODS_11_1
- MON$RECORD_BACKOUTS :: dtype_int64 len=sizeof(SINT64) sub=0 nullable=true default_blr=NULL field_id=fld_counter update=0 ods=ODS_11_1
- MON$RECORD_PURGES :: dtype_int64 len=sizeof(SINT64) sub=0 nullable=true default_blr=NULL field_id=fld_counter update=0 ods=ODS_11_1
- MON$RECORD_EXPUNGES :: dtype_int64 len=sizeof(SINT64) sub=0 nullable=true default_blr=NULL field_id=fld_counter update=0 ods=ODS_11_1
- MON$RECORD_LOCKS :: dtype_int64 len=sizeof(SINT64) sub=0 nullable=true default_blr=NULL field_id=fld_counter update=0 ods=ODS_12_0
- MON$RECORD_WAITS :: dtype_int64 len=sizeof(SINT64) sub=0 nullable=true default_blr=NULL field_id=fld_counter update=0 ods=ODS_12_0
- MON$RECORD_CONFLICTS :: dtype_int64 len=sizeof(SINT64) sub=0 nullable=true default_blr=NULL field_id=fld_counter update=0 ods=ODS_12_0
- MON$BACKVERSION_READS :: dtype_int64 len=sizeof(SINT64) sub=0 nullable=true default_blr=NULL field_id=fld_counter update=0 ods=ODS_12_0
- MON$FRAGMENT_READS :: dtype_int64 len=sizeof(SINT64) sub=0 nullable=true default_blr=NULL field_id=fld_counter update=0 ods=ODS_12_0
- MON$RECORD_RPT_READS :: dtype_int64 len=sizeof(SINT64) sub=0 nullable=true default_blr=NULL field_id=fld_counter update=0 ods=ODS_12_0
- MON$RECORD_IMGC :: dtype_int64 len=sizeof(SINT64) sub=0 nullable=true default_blr=NULL field_id=fld_counter update=0 ods=ODS_13_0

Relation: MON$CONTEXT_VARIABLES (rel_mon_ctx_vars, rel_virtual, ODS_11_2)
- MON$ATTACHMENT_ID :: dtype_int64 len=sizeof(SINT64) sub=0 nullable=true default_blr=NULL field_id=fld_att_id update=0 ods=ODS_11_2
- MON$TRANSACTION_ID :: dtype_int64 len=sizeof(SINT64) sub=0 nullable=true default_blr=NULL field_id=fld_trans_id update=0 ods=ODS_11_2
- MON$VARIABLE_NAME :: dtype_varying len=80 sub=0 nullable=true default_blr=NULL field_id=fld_ctx_var_name update=0 ods=ODS_11_2
- MON$VARIABLE_VALUE :: dtype_varying len=MAX_VARY_COLUMN_SIZE sub=0 nullable=true default_blr=NULL field_id=fld_ctx_var_value update=0 ods=ODS_11_2

Relation: MON$MEMORY_USAGE (rel_mon_mem_usage, rel_virtual, ODS_11_2)
- MON$STAT_ID :: dtype_long len=sizeof(SLONG) sub=0 nullable=true default_blr=NULL field_id=fld_stat_id update=0 ods=ODS_11_2
- MON$STAT_GROUP :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_stat_group update=0 ods=ODS_11_2
- MON$MEMORY_USED :: dtype_int64 len=sizeof(SINT64) sub=0 nullable=true default_blr=NULL field_id=fld_counter update=0 ods=ODS_11_2
- MON$MEMORY_ALLOCATED :: dtype_int64 len=sizeof(SINT64) sub=0 nullable=true default_blr=NULL field_id=fld_counter update=0 ods=ODS_11_2
- MON$MAX_MEMORY_USED :: dtype_int64 len=sizeof(SINT64) sub=0 nullable=true default_blr=NULL field_id=fld_counter update=0 ods=ODS_11_2
- MON$MAX_MEMORY_ALLOCATED :: dtype_int64 len=sizeof(SINT64) sub=0 nullable=true default_blr=NULL field_id=fld_counter update=0 ods=ODS_11_2

Relation: RDB$PACKAGES (rel_packages, rel_persistent, ODS_12_0)
- RDB$PACKAGE_NAME :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_pkg_name update=1 ods=ODS_12_0
- RDB$PACKAGE_HEADER_SOURCE :: dtype_blob len=BLOB_SIZE sub=isc_blob_text nullable=true default_blr=NULL field_id=fld_source update=1 ods=ODS_12_0
- RDB$PACKAGE_BODY_SOURCE :: dtype_blob len=BLOB_SIZE sub=isc_blob_text nullable=true default_blr=NULL field_id=fld_source update=1 ods=ODS_12_0
- RDB$VALID_BODY_FLAG :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_flag_nullable update=1 ods=ODS_12_0
- RDB$SECURITY_CLASS :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_class update=1 ods=ODS_12_0
- RDB$OWNER_NAME :: dtype_text len=USERNAME_LENGTH sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_user update=1 ods=ODS_12_0
- RDB$SYSTEM_FLAG :: dtype_short len=sizeof(SSHORT) sub=0 nullable=false default_blr=NULL field_id=fld_flag update=1 ods=ODS_12_0
- RDB$DESCRIPTION :: dtype_blob len=BLOB_SIZE sub=isc_blob_text nullable=true default_blr=NULL field_id=fld_description update=1 ods=ODS_12_0
- RDB$SQL_SECURITY :: dtype_boolean len=1 sub=0 nullable=true default_blr=NULL field_id=fld_b_sql_security update=1 ods=ODS_13_0
- RDB$SCHEMA_NAME :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_sch_name update=1 ods=ODS_14_0

Relation: SEC$USERS (rel_sec_users, rel_virtual, ODS_12_0)
- SEC$USER_NAME :: dtype_text len=USERNAME_LENGTH sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_user update=0 ods=ODS_12_0
- SEC$FIRST_NAME :: dtype_varying len=32 * METADATA_BYTES_PER_CHAR sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_name_part update=0 ods=ODS_12_0
- SEC$MIDDLE_NAME :: dtype_varying len=32 * METADATA_BYTES_PER_CHAR sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_name_part update=0 ods=ODS_12_0
- SEC$LAST_NAME :: dtype_varying len=32 * METADATA_BYTES_PER_CHAR sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_name_part update=0 ods=ODS_12_0
- SEC$ACTIVE :: dtype_boolean len=1 sub=0 nullable=true default_blr=NULL field_id=fld_bool update=0 ods=ODS_12_0
- SEC$ADMIN :: dtype_boolean len=1 sub=0 nullable=true default_blr=NULL field_id=fld_bool update=0 ods=ODS_12_0
- SEC$DESCRIPTION :: dtype_blob len=BLOB_SIZE sub=isc_blob_text nullable=true default_blr=NULL field_id=fld_description update=0 ods=ODS_12_0
- SEC$PLUGIN :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_plugin_name update=0 ods=ODS_12_0

Relation: SEC$USER_ATTRIBUTES (rel_sec_user_attributes, rel_virtual, ODS_12_0)
- SEC$USER_NAME :: dtype_text len=USERNAME_LENGTH sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_user update=0 ods=ODS_12_0
- SEC$KEY :: dtype_varying len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_attr_key update=0 ods=ODS_12_0
- SEC$VALUE :: dtype_varying len=255 * METADATA_BYTES_PER_CHAR sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_attr_value update=0 ods=ODS_12_0
- SEC$PLUGIN :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_plugin_name update=0 ods=ODS_12_0

Relation: RDB$AUTH_MAPPING (rel_auth_mapping, rel_persistent, ODS_12_0)
- RDB$MAP_NAME :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=false default_blr=NULL field_id=fld_map_name update=1 ods=ODS_12_0
- RDB$MAP_USING :: dtype_text len=1 * METADATA_BYTES_PER_CHAR sub=dsc_text_type_metadata nullable=false default_blr=NULL field_id=fld_map_using update=1 ods=ODS_12_0
- RDB$MAP_PLUGIN :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_plugin_name update=1 ods=ODS_12_0
- RDB$MAP_DB :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_map_db update=1 ods=ODS_12_0
- RDB$MAP_FROM_TYPE :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=false default_blr=NULL field_id=fld_map_from_type update=1 ods=ODS_12_0
- RDB$MAP_FROM :: dtype_text len=255 * METADATA_BYTES_PER_CHAR sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_map_from update=1 ods=ODS_12_0
- RDB$MAP_TO_TYPE :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_obj_type update=1 ods=ODS_12_0
- RDB$MAP_TO :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_map_to update=1 ods=ODS_12_0
- RDB$SYSTEM_FLAG :: dtype_short len=sizeof(SSHORT) sub=0 nullable=false default_blr=NULL field_id=fld_flag update=1 ods=ODS_12_0
- RDB$DESCRIPTION :: dtype_blob len=BLOB_SIZE sub=isc_blob_text nullable=true default_blr=NULL field_id=fld_description update=1 ods=ODS_12_0

Relation: SEC$GLOBAL_AUTH_MAPPING (rel_global_auth_mapping, rel_virtual, ODS_12_0)
- SEC$MAP_NAME :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=false default_blr=NULL field_id=fld_map_name update=0 ods=ODS_12_0
- SEC$MAP_USING :: dtype_text len=1 * METADATA_BYTES_PER_CHAR sub=dsc_text_type_metadata nullable=false default_blr=NULL field_id=fld_map_using update=0 ods=ODS_12_0
- SEC$MAP_PLUGIN :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_plugin_name update=0 ods=ODS_12_0
- SEC$MAP_DB :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_map_db update=0 ods=ODS_12_0
- SEC$MAP_FROM_TYPE :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=false default_blr=NULL field_id=fld_map_from_type update=0 ods=ODS_12_0
- SEC$MAP_FROM :: dtype_text len=255 * METADATA_BYTES_PER_CHAR sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_map_from update=0 ods=ODS_12_0
- SEC$MAP_TO_TYPE :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_obj_type update=0 ods=ODS_12_0
- SEC$MAP_TO :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_map_to update=0 ods=ODS_12_0
- SEC$DESCRIPTION :: dtype_blob len=BLOB_SIZE sub=isc_blob_text nullable=true default_blr=NULL field_id=fld_description update=0 ods=ODS_13_1

Relation: RDB$DB_CREATORS (rel_db_creators, rel_persistent, ODS_12_0)
- RDB$USER :: dtype_text len=USERNAME_LENGTH sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_user update=1 ods=ODS_12_0
- RDB$USER_TYPE :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_obj_type update=1 ods=ODS_12_0

Relation: SEC$DB_CREATORS (rel_sec_db_creators, rel_virtual, ODS_12_0)
- SEC$USER :: dtype_text len=USERNAME_LENGTH sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_user update=0 ods=ODS_12_0
- SEC$USER_TYPE :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_obj_type update=0 ods=ODS_12_0

Relation: MON$TABLE_STATS (rel_mon_tab_stats, rel_virtual, ODS_12_0)
- MON$STAT_ID :: dtype_long len=sizeof(SLONG) sub=0 nullable=true default_blr=NULL field_id=fld_stat_id update=0 ods=ODS_12_0
- MON$STAT_GROUP :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_stat_group update=0 ods=ODS_12_0
- MON$TABLE_NAME :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_r_name update=0 ods=ODS_12_0
- MON$RECORD_STAT_ID :: dtype_long len=sizeof(SLONG) sub=0 nullable=true default_blr=NULL field_id=fld_stat_id update=0 ods=ODS_12_0
- MON$SCHEMA_NAME :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_sch_name update=0 ods=ODS_14_0
- MON$TABLE_TYPE :: dtype_varying len=32 sub=dsc_text_type_ascii nullable=true default_blr=NULL field_id=fld_tab_type update=0 ods=ODS_14_0

Relation: RDB$TIME_ZONES (rel_time_zones, rel_virtual, ODS_13_0)
- RDB$TIME_ZONE_ID :: dtype_long len=sizeof(SLONG) sub=0 nullable=true default_blr=NULL field_id=fld_tz_id update=0 ods=ODS_13_0
- RDB$TIME_ZONE_NAME :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_tz_name update=0 ods=ODS_13_0

Relation: RDB$PUBLICATIONS (rel_pubs, rel_persistent, ODS_13_0)
- RDB$PUBLICATION_NAME :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=false default_blr=NULL field_id=fld_pub_name update=1 ods=ODS_13_0
- RDB$OWNER_NAME :: dtype_text len=USERNAME_LENGTH sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_user update=1 ods=ODS_13_0
- RDB$SYSTEM_FLAG :: dtype_short len=sizeof(SSHORT) sub=0 nullable=false default_blr=NULL field_id=fld_flag update=1 ods=ODS_13_0
- RDB$ACTIVE_FLAG :: dtype_short len=sizeof(SSHORT) sub=0 nullable=false default_blr=NULL field_id=fld_flag update=1 ods=ODS_13_0
- RDB$AUTO_ENABLE :: dtype_short len=sizeof(SSHORT) sub=0 nullable=false default_blr=NULL field_id=fld_flag update=1 ods=ODS_13_0

Relation: RDB$PUBLICATION_TABLES (rel_pub_tables, rel_persistent, ODS_13_0)
- RDB$PUBLICATION_NAME :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=false default_blr=NULL field_id=fld_pub_name update=1 ods=ODS_13_0
- RDB$TABLE_NAME :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_r_name update=1 ods=ODS_13_0
- RDB$TABLE_SCHEMA_NAME :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_sch_name update=1 ods=ODS_14_0

Relation: RDB$CONFIG (rel_config, rel_virtual, ODS_13_0)
- RDB$CONFIG_ID :: dtype_long len=sizeof(SLONG) sub=0 nullable=false default_blr=NULL field_id=fld_cfg_id update=0 ods=ODS_13_0
- RDB$CONFIG_NAME :: dtype_varying len=MAX_CONFIG_NAME_LEN sub=dsc_text_type_ascii nullable=false default_blr=NULL field_id=fld_cfg_name update=0 ods=ODS_13_0
- RDB$CONFIG_VALUE :: dtype_varying len=255 * METADATA_BYTES_PER_CHAR sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_cfg_value update=0 ods=ODS_13_0
- RDB$CONFIG_DEFAULT :: dtype_varying len=255 * METADATA_BYTES_PER_CHAR sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_cfg_value update=0 ods=ODS_13_0
- RDB$CONFIG_IS_SET :: dtype_boolean len=1 sub=0 nullable=false default_blr=NULL field_id=fld_cfg_is_set update=0 ods=ODS_13_0
- RDB$CONFIG_SOURCE :: dtype_varying len=255 * METADATA_BYTES_PER_CHAR sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_file_name2 update=0 ods=ODS_13_0

Relation: RDB$KEYWORDS (rel_keywords, rel_virtual, ODS_13_1)
- RDB$KEYWORD_NAME :: dtype_varying len=METADATA_IDENTIFIER_CHAR_LEN sub=dsc_text_type_ascii nullable=false default_blr=NULL field_id=fld_keyword_name update=0 ods=ODS_13_1
- RDB$KEYWORD_RESERVED :: dtype_boolean len=1 sub=0 nullable=false default_blr=NULL field_id=fld_keyword_reserved update=0 ods=ODS_13_1

Relation: MON$COMPILED_STATEMENTS (rel_mon_compiled_statements, rel_virtual, ODS_13_1)
- MON$COMPILED_STATEMENT_ID :: dtype_int64 len=sizeof(SINT64) sub=0 nullable=true default_blr=NULL field_id=fld_stmt_id update=0 ods=ODS_13_1
- MON$SQL_TEXT :: dtype_blob len=BLOB_SIZE sub=isc_blob_text nullable=true default_blr=NULL field_id=fld_source update=0 ods=ODS_13_1
- MON$EXPLAINED_PLAN :: dtype_blob len=BLOB_SIZE sub=isc_blob_text nullable=true default_blr=NULL field_id=fld_source update=0 ods=ODS_13_1
- MON$OBJECT_NAME :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_gnr_name update=0 ods=ODS_13_1
- MON$OBJECT_TYPE :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_obj_type update=0 ods=ODS_13_1
- MON$PACKAGE_NAME :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_pkg_name update=0 ods=ODS_13_1
- MON$STAT_ID :: dtype_long len=sizeof(SLONG) sub=0 nullable=true default_blr=NULL field_id=fld_stat_id update=0 ods=ODS_13_1
- MON$SCHEMA_NAME :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_sch_name update=0 ods=ODS_14_0

Relation: RDB$SCHEMAS (rel_schemas, rel_persistent, ODS_14_0)
- RDB$SCHEMA_NAME :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_sch_name update=1 ods=ODS_14_0
- RDB$OWNER_NAME :: dtype_text len=USERNAME_LENGTH sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_user update=1 ods=ODS_14_0
- RDB$CHARACTER_SET_NAME :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_charset_name update=1 ods=ODS_14_0
- RDB$CHARACTER_SET_SCHEMA_NAME :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_sch_name update=1 ods=ODS_14_0
- RDB$SQL_SECURITY :: dtype_boolean len=1 sub=0 nullable=true default_blr=NULL field_id=fld_b_sql_security update=1 ods=ODS_14_0
- RDB$SECURITY_CLASS :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_class update=1 ods=ODS_14_0
- RDB$SYSTEM_FLAG :: dtype_short len=sizeof(SSHORT) sub=0 nullable=false default_blr=NULL field_id=fld_flag update=1 ods=ODS_14_0
- RDB$DESCRIPTION :: dtype_blob len=BLOB_SIZE sub=isc_blob_text nullable=true default_blr=NULL field_id=fld_description update=1 ods=ODS_14_0

Relation: MON$LOCAL_TEMPORARY_TABLES (rel_mon_local_temp_tables, rel_virtual, ODS_14_0)
- MON$ATTACHMENT_ID :: dtype_int64 len=sizeof(SINT64) sub=0 nullable=true default_blr=NULL field_id=fld_att_id update=0 ods=ODS_14_0
- MON$TABLE_ID :: dtype_long len=sizeof(SLONG) sub=0 nullable=true default_blr=NULL field_id=fld_integer update=0 ods=ODS_14_0
- MON$TABLE_NAME :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_r_name update=0 ods=ODS_14_0
- MON$SCHEMA_NAME :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_sch_name update=0 ods=ODS_14_0
- MON$TABLE_TYPE :: dtype_varying len=32 sub=dsc_text_type_ascii nullable=true default_blr=NULL field_id=fld_tab_type update=0 ods=ODS_14_0

Relation: MON$LOCAL_TEMPORARY_TABLE_COLUMNS (rel_mon_local_temp_table_columns, rel_virtual, ODS_14_0)
- MON$ATTACHMENT_ID :: dtype_int64 len=sizeof(SINT64) sub=0 nullable=true default_blr=NULL field_id=fld_att_id update=0 ods=ODS_14_0
- MON$TABLE_NAME :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_r_name update=0 ods=ODS_14_0
- MON$SCHEMA_NAME :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_sch_name update=0 ods=ODS_14_0
- MON$FIELD_NAME :: dtype_text len=MAX_SQL_IDENTIFIER_LEN sub=dsc_text_type_metadata nullable=true default_blr=NULL field_id=fld_f_name update=0 ods=ODS_14_0
- MON$FIELD_POSITION :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_f_position update=0 ods=ODS_14_0
- MON$FIELD_TYPE :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_f_type update=0 ods=ODS_14_0
- MON$FIELD_PRECISION :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_f_precision update=0 ods=ODS_14_0
- MON$FIELD_SCALE :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_f_scale update=0 ods=ODS_14_0
- MON$CHAR_LENGTH :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_f_length update=0 ods=ODS_14_0
- MON$FIELD_LENGTH :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_f_length update=0 ods=ODS_14_0
- MON$FIELD_SUB_TYPE :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_sub_type update=0 ods=ODS_14_0
- MON$NOT_NULL :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_null_flag update=0 ods=ODS_14_0
- MON$CHARACTER_SET_ID :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_charset_id update=0 ods=ODS_14_0
- MON$COLLATION_ID :: dtype_short len=sizeof(SSHORT) sub=0 nullable=true default_blr=NULL field_id=fld_collate_id update=0 ods=ODS_14_0
