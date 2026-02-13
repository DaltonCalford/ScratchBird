# Appendix: Firebird 5 Catalog Bootstrap (Authoritative)

This appendix defines the **minimum system catalog image** and the **numeric relation/field IDs** required to produce Firebird-compatible metadata. It is authoritative for bootstrapping the catalog and for responding to system table queries without ambiguity.

## Constants (Authoritative)
- `BLOB_SIZE = 8`
- `METADATA_IDENTIFIER_CHAR_LEN = 63`
- `METADATA_BYTES_PER_CHAR = 4`
- `MAX_SQL_IDENTIFIER_LEN = 252`
- `USERNAME_LENGTH = 252`
- `isc_blob_untyped = 0`
- `isc_blob_text = 1`
- `isc_blob_blr = 2`
- `isc_blob_acl = 3`
- `isc_blob_summary = 5`
- `isc_blob_format = 6`
- `isc_blob_tra = 7`
- `isc_blob_extfile = 8`
- `dsc_text_type_metadata = 3`
- `rel_persistent = 0`
- `rel_view = 1`
- `rel_external = 2`
- `rel_virtual = 3`
- `rel_temp_preserve = 4`
- `rel_temp_delete = 5`
- `RDB_system = 1`
- `SYSTEM_SCHEMA = "SYSTEM"`
- `PUBLIC_SCHEMA = "PUBLIC"`
- `DEFAULT_DB_CHARACTER_SET_NAME = "SYSTEM.NONE"`
- `USER_DEF_REL_INIT_ID = 128`
- `SQL_SECCLASS_PREFIX = "SQL$"`
- `SQL_SECCLASS_GENERATOR = "RDB$SECURITY_CLASS"`
- `DEFAULT_CLASS = "SQL$DEFAULT"`
- `TEXTTYPE_ATTR_PAD_SPACE = 1`
- `TEXTTYPE_ATTR_CASE_INSENSITIVE = 2`
- `TEXTTYPE_ATTR_ACCENT_INSENSITIVE = 4`

## Size Evaluation Rules
- `sizeof(SSHORT) = 2`
- `sizeof(USHORT) = 2`
- `sizeof(SLONG) = 4`
- `sizeof(ULONG) = 4`
- `sizeof(SINT64) = 8`
- `sizeof(UINT64) = 8`
- `sizeof(float) = 4`
- `sizeof(double) = 8`
- `sizeof(ISC_DATE) = 4`
- `sizeof(ISC_TIME) = 4`
- `sizeof(ISC_TIMESTAMP) = 8`
- `sizeof(ISC_TIME_TZ) = 8`
- `sizeof(ISC_TIME_TZ_EX) = 8`
- `sizeof(ISC_TIMESTAMP_TZ) = 12`
- `sizeof(ISC_TIMESTAMP_TZ_EX) = 12`
- `sizeof(ISC_QUAD) = 8`
- `sizeof(Decimal64) = 8`
- `sizeof(Decimal128) = 16`
- `sizeof(Int128) = 16`

## Relation IDs (Numeric, Authoritative)
Relation IDs are the **zero-based order** of relations in the table below. These IDs must be used in `RDB$RELATIONS.RDB$RELATION_ID` and in any on-disk or wire-level metadata references.

- RDB$PAGES = 0 (rel_pages, rel_persistent)
- RDB$DATABASE = 1 (rel_database, rel_persistent)
- RDB$FIELDS = 2 (rel_fields, rel_persistent)
- RDB$INDEX_SEGMENTS = 3 (rel_segments, rel_persistent)
- RDB$INDICES = 4 (rel_indices, rel_persistent)
- RDB$RELATION_FIELDS = 5 (rel_rfr, rel_persistent)
- RDB$RELATIONS = 6 (rel_relations, rel_persistent)
- RDB$VIEW_RELATIONS = 7 (rel_vrel, rel_persistent)
- RDB$FORMATS = 8 (rel_formats, rel_persistent)
- RDB$SECURITY_CLASSES = 9 (rel_classes, rel_persistent)
- RDB$FILES = 10 (rel_files, rel_persistent)
- RDB$TYPES = 11 (rel_types, rel_persistent)
- RDB$TRIGGERS = 12 (rel_triggers, rel_persistent)
- RDB$DEPENDENCIES = 13 (rel_dpds, rel_persistent)
- RDB$FUNCTIONS = 14 (rel_funs, rel_persistent)
- RDB$FUNCTION_ARGUMENTS = 15 (rel_args, rel_persistent)
- RDB$FILTERS = 16 (rel_filters, rel_persistent)
- RDB$TRIGGER_MESSAGES = 17 (rel_msgs, rel_persistent)
- RDB$USER_PRIVILEGES = 18 (rel_priv, rel_persistent)
- RDB$TRANSACTIONS = 19 (rel_trans, rel_persistent)
- RDB$GENERATORS = 20 (rel_gens, rel_persistent)
- RDB$FIELD_DIMENSIONS = 21 (rel_dims, rel_persistent)
- RDB$RELATION_CONSTRAINTS = 22 (rel_rcon, rel_persistent)
- RDB$REF_CONSTRAINTS = 23 (rel_refc, rel_persistent)
- RDB$CHECK_CONSTRAINTS = 24 (rel_ccon, rel_persistent)
- RDB$LOG_FILES = 25 (rel_log, rel_persistent)
- RDB$PROCEDURES = 26 (rel_procedures, rel_persistent)
- RDB$PROCEDURE_PARAMETERS = 27 (rel_prc_prms, rel_persistent)
- RDB$CHARACTER_SETS = 28 (rel_charsets, rel_persistent)
- RDB$COLLATIONS = 29 (rel_collations, rel_persistent)
- RDB$EXCEPTIONS = 30 (rel_exceptions, rel_persistent)
- RDB$ROLES = 31 (rel_roles, rel_persistent)
- RDB$BACKUP_HISTORY = 32 (rel_backup_history, rel_persistent)
- MON$DATABASE = 33 (rel_mon_database, rel_virtual)
- MON$ATTACHMENTS = 34 (rel_mon_attachments, rel_virtual)
- MON$TRANSACTIONS = 35 (rel_mon_transactions, rel_virtual)
- MON$STATEMENTS = 36 (rel_mon_statements, rel_virtual)
- MON$CALL_STACK = 37 (rel_mon_calls, rel_virtual)
- MON$IO_STATS = 38 (rel_mon_io_stats, rel_virtual)
- MON$RECORD_STATS = 39 (rel_mon_rec_stats, rel_virtual)
- MON$CONTEXT_VARIABLES = 40 (rel_mon_ctx_vars, rel_virtual)
- MON$MEMORY_USAGE = 41 (rel_mon_mem_usage, rel_virtual)
- RDB$PACKAGES = 42 (rel_packages, rel_persistent)
- SEC$USERS = 43 (rel_sec_users, rel_virtual)
- SEC$USER_ATTRIBUTES = 44 (rel_sec_user_attributes, rel_virtual)
- RDB$AUTH_MAPPING = 45 (rel_auth_mapping, rel_persistent)
- SEC$GLOBAL_AUTH_MAPPING = 46 (rel_global_auth_mapping, rel_virtual)
- RDB$DB_CREATORS = 47 (rel_db_creators, rel_persistent)
- SEC$DB_CREATORS = 48 (rel_sec_db_creators, rel_virtual)
- MON$TABLE_STATS = 49 (rel_mon_tab_stats, rel_virtual)
- RDB$TIME_ZONES = 50 (rel_time_zones, rel_virtual)
- RDB$PUBLICATIONS = 51 (rel_pubs, rel_persistent)
- RDB$PUBLICATION_TABLES = 52 (rel_pub_tables, rel_persistent)
- RDB$CONFIG = 53 (rel_config, rel_virtual)
- RDB$KEYWORDS = 54 (rel_keywords, rel_virtual)
- MON$COMPILED_STATEMENTS = 55 (rel_mon_compiled_statements, rel_virtual)
- RDB$SCHEMAS = 56 (rel_schemas, rel_persistent)
- MON$LOCAL_TEMPORARY_TABLES = 57 (rel_mon_local_temp_tables, rel_virtual)
- MON$LOCAL_TEMPORARY_TABLE_COLUMNS = 58 (rel_mon_local_temp_table_columns, rel_virtual)

## Field IDs by Relation (Numeric, Authoritative)
Field IDs are the **zero-based field position** within each relation as defined below. These IDs must be used in `RDB$RELATION_FIELDS.RDB$FIELD_ID` and for metadata access.

### RDB$PAGES
- RDB$PAGE_NUMBER = 0 (f_pag_page, def fld_p_number)
- RDB$RELATION_ID = 1 (f_pag_id, def fld_r_id)
- RDB$PAGE_SEQUENCE = 2 (f_pag_seq, def fld_p_sequence)
- RDB$PAGE_TYPE = 3 (f_pag_type, def fld_p_type)

### RDB$DATABASE
- RDB$DESCRIPTION = 0 (f_dat_desc, def fld_description)
- RDB$RELATION_ID = 1 (f_dat_id, def fld_r_id)
- RDB$SECURITY_CLASS = 2 (f_dat_class, def fld_class)
- RDB$CHARACTER_SET_NAME = 3 (f_dat_charset, def fld_charset_name)
- RDB$LINGER = 4 (f_dat_linger, def fld_linger)
- RDB$SQL_SECURITY = 5 (f_dat_sql_security, def fld_b_sql_security)
- RDB$CHARACTER_SET_SCHEMA_NAME = 6 (f_dat_charset_schema, def fld_sch_name)

### RDB$FIELDS
- RDB$FIELD_NAME = 0 (f_fld_name, def fld_f_name)
- RDB$QUERY_NAME = 1 (f_fld_qname, def fld_f_name)
- RDB$VALIDATION_BLR = 2 (f_fld_v_blr, def fld_validation)
- RDB$VALIDATION_SOURCE = 3 (f_fld_v_source, def fld_source)
- RDB$COMPUTED_BLR = 4 (f_fld_computed, def fld_value)
- RDB$COMPUTED_SOURCE = 5 (f_fld_csource, def fld_source)
- RDB$DEFAULT_VALUE = 6 (f_fld_default, def fld_value)
- RDB$DEFAULT_SOURCE = 7 (f_fld_dsource, def fld_source)
- RDB$FIELD_LENGTH = 8 (f_fld_length, def fld_f_length)
- RDB$FIELD_SCALE = 9 (f_fld_scale, def fld_f_scale)
- RDB$FIELD_TYPE = 10 (f_fld_type, def fld_f_type)
- RDB$FIELD_SUB_TYPE = 11 (f_fld_sub_type, def fld_sub_type)
- RDB$MISSING_VALUE = 12 (f_fld_missing, def fld_value)
- RDB$MISSING_SOURCE = 13 (f_fld_msource, def fld_source)
- RDB$DESCRIPTION = 14 (f_fld_desc, def fld_description)
- RDB$SYSTEM_FLAG = 15 (f_fld_sys_flag, def fld_flag)
- RDB$QUERY_HEADER = 16 (f_fld_qheader, def fld_q_header)
- RDB$SEGMENT_LENGTH = 17 (f_fld_seg_len, def fld_s_length)
- RDB$EDIT_STRING = 18 (f_fld_estring, def fld_edit_string)
- RDB$EXTERNAL_LENGTH = 19 (f_fld_ext_length, def fld_f_length)
- RDB$EXTERNAL_SCALE = 20 (f_fld_ext_scale, def fld_f_scale)
- RDB$EXTERNAL_TYPE = 21 (f_fld_ext_type, def fld_f_type)
- RDB$DIMENSIONS = 22 (f_fld_dimensions, def fld_dimensions)
- RDB$NULL_FLAG = 23 (f_fld_null_flag, def fld_null_flag)
- RDB$CHARACTER_LENGTH = 24 (f_fld_char_length, def fld_f_length)
- RDB$COLLATION_ID = 25 (f_fld_coll_id, def fld_collate_id)
- RDB$CHARACTER_SET_ID = 26 (f_fld_charset_id, def fld_charset_id)
- RDB$FIELD_PRECISION = 27 (f_fld_precision, def fld_f_precision)
- RDB$SECURITY_CLASS = 28 (f_fld_class, def fld_class)
- RDB$OWNER_NAME = 29 (f_fld_owner, def fld_user)
- RDB$SCHEMA_NAME = 30 (f_fld_schema, def fld_sch_name)

### RDB$INDEX_SEGMENTS
- RDB$INDEX_NAME = 0 (f_seg_name, def fld_i_name)
- RDB$FIELD_NAME = 1 (f_seg_field, def fld_f_name)
- RDB$FIELD_POSITION = 2 (f_seg_position, def fld_f_position)
- RDB$STATISTICS = 3 (f_seg_statistics, def fld_statistics)
- RDB$SCHEMA_NAME = 4 (f_seg_schema, def fld_sch_name)

### RDB$INDICES
- RDB$INDEX_NAME = 0 (f_idx_name, def fld_i_name)
- RDB$RELATION_NAME = 1 (f_idx_relation, def fld_r_name)
- RDB$INDEX_ID = 2 (f_idx_id, def fld_i_id)
- RDB$UNIQUE_FLAG = 3 (f_idx_flag, def fld_flag_nullable)
- RDB$DESCRIPTION = 4 (f_idx_desc, def fld_description)
- RDB$SEGMENT_COUNT = 5 (f_idx_count, def fld_s_count)
- RDB$INDEX_INACTIVE = 6 (f_idx_inactive, def fld_flag_nullable)
- RDB$INDEX_TYPE = 7 (f_idx_type, def fld_flag_nullable)
- RDB$FOREIGN_KEY = 8 (f_idx_foreign, def fld_r_name)
- RDB$SYSTEM_FLAG = 9 (f_idx_sys_flag, def fld_flag)
- RDB$EXPRESSION_BLR = 10 (f_idx_exp_blr, def fld_value)
- RDB$EXPRESSION_SOURCE = 11 (f_idx_exp_source, def fld_source)
- RDB$STATISTICS = 12 (f_idx_statistics, def fld_statistics)
- RDB$CONDITION_BLR = 13 (f_idx_cond_blr, def fld_value)
- RDB$CONDITION_SOURCE = 14 (f_idx_cond_source, def fld_source)
- RDB$SCHEMA_NAME = 15 (f_idx_schema, def fld_sch_name)
- RDB$FOREIGN_KEY_SCHEMA_NAME = 16 (f_idx_foreign_schema, def fld_sch_name)

### RDB$RELATION_FIELDS
- RDB$FIELD_NAME = 0 (f_rfr_fname, def fld_f_name)
- RDB$RELATION_NAME = 1 (f_rfr_rname, def fld_r_name)
- RDB$FIELD_SOURCE = 2 (f_rfr_sname, def fld_f_name)
- RDB$QUERY_NAME = 3 (f_rfr_qname, def fld_f_name)
- RDB$BASE_FIELD = 4 (f_rfr_base, def fld_f_name)
- RDB$EDIT_STRING = 5 (f_rfr_estring, def fld_edit_string)
- RDB$FIELD_POSITION = 6 (f_rfr_position, def fld_f_position)
- RDB$QUERY_HEADER = 7 (f_rfr_qheader, def fld_q_header)
- RDB$UPDATE_FLAG = 8 (f_rfr_flag, def fld_flag_nullable)
- RDB$FIELD_ID = 9 (f_rfr_id, def fld_f_id)
- RDB$VIEW_CONTEXT = 10 (f_rfr_context, def fld_context)
- RDB$DESCRIPTION = 11 (f_rfr_desc, def fld_description)
- RDB$DEFAULT_VALUE = 12 (f_rfr_default, def fld_value)
- RDB$SYSTEM_FLAG = 13 (f_rfr_sys_flag, def fld_flag)
- RDB$SECURITY_CLASS = 14 (f_rfr_class, def fld_class)
- RDB$COMPLEX_NAME = 15 (f_rfr_complex, def fld_f_name)
- RDB$NULL_FLAG = 16 (f_rfr_null_flag, def fld_null_flag)
- RDB$DEFAULT_SOURCE = 17 (f_rfr_dsource, def fld_source)
- RDB$COLLATION_ID = 18 (f_rfr_coll_id, def fld_collate_id)
- RDB$GENERATOR_NAME = 19 (f_rfr_gen_name, def fld_gen_name)
- RDB$IDENTITY_TYPE = 20 (f_rfr_identity_type, def fld_identity_type)
- RDB$SCHEMA_NAME = 21 (f_rfr_schema, def fld_sch_name)
- RDB$FIELD_SOURCE_SCHEMA_NAME = 22 (f_rfr_field_source_schema, def fld_sch_name)

### RDB$RELATIONS
- RDB$VIEW_BLR = 0 (f_rel_blr, def fld_v_blr)
- RDB$VIEW_SOURCE = 1 (f_rel_source, def fld_source)
- RDB$DESCRIPTION = 2 (f_rel_desc, def fld_description)
- RDB$RELATION_ID = 3 (f_rel_id, def fld_r_id)
- RDB$SYSTEM_FLAG = 4 (f_rel_sys_flag, def fld_flag)
- RDB$DBKEY_LENGTH = 5 (f_rel_key_len, def fld_key_length)
- RDB$FORMAT = 6 (f_rel_format, def fld_format)
- RDB$FIELD_ID = 7 (f_rel_field_id, def fld_f_id)
- RDB$RELATION_NAME = 8 (f_rel_name, def fld_r_name)
- RDB$SECURITY_CLASS = 9 (f_rel_class, def fld_class)
- RDB$EXTERNAL_FILE = 10 (f_rel_ext_file, def fld_file_name)
- RDB$RUNTIME = 11 (f_rel_runtime, def fld_runtime)
- RDB$EXTERNAL_DESCRIPTION = 12 (f_rel_ext_desc, def fld_ext_desc)
- RDB$OWNER_NAME = 13 (f_rel_owner, def fld_user)
- RDB$DEFAULT_CLASS = 14 (f_rel_def_class, def fld_class)
- RDB$FLAGS = 15 (f_rel_flags, def fld_flag_nullable)
- RDB$RELATION_TYPE = 16 (f_rel_type, def fld_r_type)
- RDB$SQL_SECURITY = 17 (f_rel_sql_security, def fld_b_sql_security)
- RDB$SCHEMA_NAME = 18 (f_rel_schema, def fld_sch_name)

### RDB$VIEW_RELATIONS
- RDB$VIEW_NAME = 0 (f_vrl_vname, def fld_r_name)
- RDB$RELATION_NAME = 1 (f_vrl_rname, def fld_r_name)
- RDB$VIEW_CONTEXT = 2 (f_vrl_context, def fld_context)
- RDB$CONTEXT_NAME = 3 (f_vrl_cname, def fld_ctx_name)
- RDB$CONTEXT_TYPE = 4 (f_vrl_context_type, def fld_context)
- RDB$PACKAGE_NAME = 5 (f_vrl_pkg_name, def fld_pkg_name)
- RDB$SCHEMA_NAME = 6 (f_vrl_schema, def fld_sch_name)
- RDB$RELATION_SCHEMA_NAME = 7 (f_vrl_rname_schema, def fld_sch_name)

### RDB$FORMATS
- RDB$RELATION_ID = 0 (f_fmt_rid, def fld_r_id)
- RDB$FORMAT = 1 (f_fmt_format, def fld_format)
- RDB$DESCRIPTOR = 2 (f_fmt_desc, def fld_f_descr)

### RDB$SECURITY_CLASSES
- RDB$SECURITY_CLASS = 0 (f_cls_class, def fld_class)
- RDB$ACL = 1 (f_cls_acl, def fld_acl)
- RDB$DESCRIPTION = 2 (f_cls_desc, def fld_description)

### RDB$FILES
- RDB$FILE_NAME = 0 (f_file_name, def fld_file_name)
- RDB$FILE_SEQUENCE = 1 (f_file_seq, def fld_file_seq)
- RDB$FILE_START = 2 (f_file_start, def fld_file_start)
- RDB$FILE_LENGTH = 3 (f_file_length, def fld_file_length)
- RDB$FILE_FLAGS = 4 (f_file_flags, def fld_file_flags)
- RDB$SHADOW_NUMBER = 5 (f_file_shad_num, def fld_shad_num)

### RDB$TYPES
- RDB$FIELD_NAME = 0 (f_typ_field, def fld_f_name)
- RDB$TYPE = 1 (f_typ_type, def fld_gnr_type)
- RDB$TYPE_NAME = 2 (f_typ_name, def fld_typ_name)
- RDB$DESCRIPTION = 3 (f_typ_desc, def fld_description)
- RDB$SYSTEM_FLAG = 4 (f_typ_sys_flag, def fld_flag)

### RDB$TRIGGERS
- RDB$TRIGGER_NAME = 0 (f_trg_name, def fld_trg_name)
- RDB$RELATION_NAME = 1 (f_trg_rname, def fld_r_name)
- RDB$TRIGGER_SEQUENCE = 2 (f_trg_seq, def fld_trg_seq)
- RDB$TRIGGER_TYPE = 3 (f_trg_type, def fld_trg_type)
- RDB$TRIGGER_SOURCE = 4 (f_trg_source, def fld_source)
- RDB$TRIGGER_BLR = 5 (f_trg_blr, def fld_trigger)
- RDB$DESCRIPTION = 6 (f_trg_desc, def fld_description)
- RDB$TRIGGER_INACTIVE = 7 (f_trg_inactive, def fld_flag_nullable)
- RDB$SYSTEM_FLAG = 8 (f_trg_sys_flag, def fld_flag)
- RDB$FLAGS = 9 (f_trg_flags, def fld_flag_nullable)
- RDB$VALID_BLR = 10 (f_trg_valid_blr, def fld_flag_nullable)
- RDB$DEBUG_INFO = 11 (f_trg_debug_info, def fld_debug_info)
- RDB$ENGINE_NAME = 12 (f_trg_engine_name, def fld_engine_name)
- RDB$ENTRYPOINT = 13 (f_trg_entry, def fld_ext_name)
- RDB$SQL_SECURITY = 14 (f_trg_sql_security, def fld_b_sql_security)
- RDB$SCHEMA_NAME = 15 (f_trg_schema, def fld_sch_name)

### RDB$DEPENDENCIES
- RDB$DEPENDENT_NAME = 0 (f_dpd_name, def fld_gnr_name)
- RDB$DEPENDED_ON_NAME = 1 (f_dpd_o_name, def fld_gnr_name)
- RDB$FIELD_NAME = 2 (f_dpd_f_name, def fld_f_name)
- RDB$DEPENDENT_TYPE = 3 (f_dpd_type, def fld_obj_type)
- RDB$DEPENDED_ON_TYPE = 4 (f_dpd_o_type, def fld_obj_type)
- RDB$PACKAGE_NAME = 5 (f_dpd_pkg_name, def fld_pkg_name)
- RDB$DEPENDENT_SCHEMA_NAME = 6 (f_dpd_schema, def fld_sch_name)
- RDB$DEPENDED_ON_SCHEMA_NAME = 7 (f_dpd_o_schema, def fld_sch_name)

### RDB$FUNCTIONS
- RDB$FUNCTION_NAME = 0 (f_fun_name, def fld_fun_name)
- RDB$FUNCTION_TYPE = 1 (f_fun_type, def fld_fun_type)
- RDB$QUERY_NAME = 2 (f_fun_qname, def fld_f_name)
- RDB$DESCRIPTION = 3 (f_fun_desc, def fld_description)
- RDB$MODULE_NAME = 4 (f_fun_module, def fld_file_name)
- RDB$ENTRYPOINT = 5 (f_fun_entry, def fld_ext_name)
- RDB$RETURN_ARGUMENT = 6 (f_fun_ret_arg, def fld_f_position)
- RDB$SYSTEM_FLAG = 7 (f_fun_sys_flag, def fld_flag)
- RDB$ENGINE_NAME = 8 (f_fun_engine_name, def fld_engine_name)
- RDB$PACKAGE_NAME = 9 (f_fun_pkg_name, def fld_pkg_name)
- RDB$PRIVATE_FLAG = 10 (f_fun_private_flag, def fld_flag_nullable)
- RDB$FUNCTION_SOURCE = 11 (f_fun_source, def fld_source)
- RDB$FUNCTION_ID = 12 (f_fun_id, def fld_fun_id)
- RDB$FUNCTION_BLR = 13 (f_fun_blr, def fld_fun_blr)
- RDB$VALID_BLR = 14 (f_fun_valid_blr, def fld_flag_nullable)
- RDB$DEBUG_INFO = 15 (f_fun_debug_info, def fld_debug_info)
- RDB$SECURITY_CLASS = 16 (f_fun_class, def fld_class)
- RDB$OWNER_NAME = 17 (f_fun_owner, def fld_user)
- RDB$LEGACY_FLAG = 18 (f_fun_legacy_flag, def fld_flag_nullable)
- RDB$DETERMINISTIC_FLAG = 19 (f_fun_deterministic_flag, def fld_flag_nullable)
- RDB$SQL_SECURITY = 20 (f_fun_sql_security, def fld_b_sql_security)
- RDB$SCHEMA_NAME = 21 (f_fun_schema, def fld_sch_name)

### RDB$FUNCTION_ARGUMENTS
- RDB$FUNCTION_NAME = 0 (f_arg_fun_name, def fld_fun_name)
- RDB$ARGUMENT_POSITION = 1 (f_arg_pos, def fld_f_position)
- RDB$MECHANISM = 2 (f_arg_mech, def fld_mechanism)
- RDB$FIELD_TYPE = 3 (f_arg_type, def fld_f_type)
- RDB$FIELD_SCALE = 4 (f_arg_scale, def fld_f_scale)
- RDB$FIELD_LENGTH = 5 (f_arg_length, def fld_f_length)
- RDB$FIELD_SUB_TYPE = 6 (f_arg_sub_type, def fld_sub_type)
- RDB$CHARACTER_SET_ID = 7 (f_arg_charset_id, def fld_charset_id)
- RDB$FIELD_PRECISION = 8 (f_arg_precision, def fld_f_precision)
- RDB$CHARACTER_LENGTH = 9 (f_arg_char_length, def fld_f_length)
- RDB$PACKAGE_NAME = 10 (f_arg_pkg_name, def fld_pkg_name)
- RDB$ARGUMENT_NAME = 11 (f_arg_name, def fld_arg_name)
- RDB$FIELD_SOURCE = 12 (f_arg_sname, def fld_f_name)
- RDB$DEFAULT_VALUE = 13 (f_arg_default, def fld_value)
- RDB$DEFAULT_SOURCE = 14 (f_arg_dsource, def fld_source)
- RDB$COLLATION_ID = 15 (f_arg_coll_id, def fld_collate_id)
- RDB$NULL_FLAG = 16 (f_arg_null_flag, def fld_null_flag)
- RDB$ARGUMENT_MECHANISM = 17 (f_arg_arg_mech, def fld_arg_mechanism)
- RDB$FIELD_NAME = 18 (f_arg_fname, def fld_f_name)
- RDB$RELATION_NAME = 19 (f_arg_rname, def fld_r_name)
- RDB$SYSTEM_FLAG = 20 (f_arg_sys_flag, def fld_flag)
- RDB$DESCRIPTION = 21 (f_arg_desc, def fld_description)
- RDB$SCHEMA_NAME = 22 (f_arg_schema, def fld_sch_name)
- RDB$RELATION_SCHEMA_NAME = 23 (f_arg_rel_schema, def fld_sch_name)
- RDB$FIELD_SOURCE_SCHEMA_NAME = 24 (f_arg_field_source_schema, def fld_sch_name)

### RDB$FILTERS
- RDB$FUNCTION_NAME = 0 (f_flt_name, def fld_fun_name)
- RDB$DESCRIPTION = 1 (f_flt_desc, def fld_description)
- RDB$MODULE_NAME = 2 (f_flt_module, def fld_file_name)
- RDB$ENTRYPOINT = 3 (f_flt_entry, def fld_ext_name)
- RDB$INPUT_SUB_TYPE = 4 (f_flt_input, def fld_sub_type)
- RDB$OUTPUT_SUB_TYPE = 5 (f_flt_output, def fld_sub_type)
- RDB$SYSTEM_FLAG = 6 (f_flt_sys_flag, def fld_flag)
- RDB$SECURITY_CLASS = 7 (f_flt_class, def fld_class)
- RDB$OWNER_NAME = 8 (f_flt_owner, def fld_user)

### RDB$TRIGGER_MESSAGES
- RDB$TRIGGER_NAME = 0 (f_msg_trigger, def fld_trg_name)
- RDB$MESSAGE_NUMBER = 1 (f_msg_number, def fld_msg_num)
- RDB$MESSAGE = 2 (f_msg_msg, def fld_msg)
- RDB$SCHEMA_NAME = 3 (f_msg_schema, def fld_sch_name)

### RDB$USER_PRIVILEGES
- RDB$USER = 0 (f_prv_user, def fld_user)
- RDB$GRANTOR = 1 (f_prv_grantor, def fld_user)
- RDB$PRIVILEGE = 2 (f_prv_priv, def fld_privilege)
- RDB$GRANT_OPTION = 3 (f_prv_grant, def fld_flag_nullable)
- RDB$RELATION_NAME = 4 (f_prv_rname, def fld_gnr_name)
- RDB$FIELD_NAME = 5 (f_prv_fname, def fld_f_name)
- RDB$USER_TYPE = 6 (f_prv_u_type, def fld_obj_type)
- RDB$OBJECT_TYPE = 7 (f_prv_o_type, def fld_obj_type)
- RDB$RELATION_SCHEMA_NAME = 8 (f_prv_rel_schema, def fld_sch_name)
- RDB$USER_SCHEMA_NAME = 9 (f_prv_user_schema, def fld_sch_name)

### RDB$TRANSACTIONS
- RDB$TRANSACTION_ID = 0 (f_trn_id, def fld_trans_id)
- RDB$TRANSACTION_STATE = 1 (f_trn_state, def fld_trans_state)
- RDB$TIMESTAMP = 2 (f_trn_time, def fld_timestamp_tz)
- RDB$TRANSACTION_DESCRIPTION = 3 (f_trn_desc, def fld_trans_desc)

### RDB$GENERATORS
- RDB$GENERATOR_NAME = 0 (f_gen_name, def fld_gen_name)
- RDB$GENERATOR_ID = 1 (f_gen_id, def fld_gen_id)
- RDB$SYSTEM_FLAG = 2 (f_gen_sys_flag, def fld_flag)
- RDB$DESCRIPTION = 3 (f_gen_desc, def fld_description)
- RDB$SECURITY_CLASS = 4 (f_gen_class, def fld_class)
- RDB$OWNER_NAME = 5 (f_gen_owner, def fld_user)
- RDB$INITIAL_VALUE = 6 (f_gen_init_val, def fld_gen_val)
- RDB$GENERATOR_INCREMENT = 7 (f_gen_increment, def fld_gen_increment)
- RDB$SCHEMA_NAME = 8 (f_gen_schema, def fld_sch_name)

### RDB$FIELD_DIMENSIONS
- RDB$FIELD_NAME = 0 (f_dims_fname, def fld_f_name)
- RDB$DIMENSION = 1 (f_dims_dim, def fld_dim)
- RDB$LOWER_BOUND = 2 (f_dims_lower, def fld_bound)
- RDB$UPPER_BOUND = 3 (f_dims_upper, def fld_bound)
- RDB$SCHEMA_NAME = 4 (f_dims_schema, def fld_sch_name)

### RDB$RELATION_CONSTRAINTS
- RDB$CONSTRAINT_NAME = 0 (f_rcon_cname, def fld_con_name)
- RDB$CONSTRAINT_TYPE = 1 (f_rcon_ctype, def fld_con_type)
- RDB$RELATION_NAME = 2 (f_rcon_rname, def fld_r_name)
- RDB$DEFERRABLE = 3 (f_rcon_dfr, def fld_defer)
- RDB$INITIALLY_DEFERRED = 4 (f_rcon_idfr, def fld_defer)
- RDB$INDEX_NAME = 5 (f_rcon_iname, def fld_i_name)
- RDB$SCHEMA_NAME = 6 (f_rcon_schema, def fld_sch_name)

### RDB$REF_CONSTRAINTS
- RDB$CONSTRAINT_NAME = 0 (f_refc_cname, def fld_con_name)
- RDB$CONST_NAME_UQ = 1 (f_refc_uq, def fld_con_name)
- RDB$MATCH_OPTION = 2 (f_refc_match, def fld_match)
- RDB$UPDATE_RULE = 3 (f_refc_upd_rul, def fld_rule)
- RDB$DELETE_RULE = 4 (f_refc_del_rul, def fld_rule)
- RDB$SCHEMA_NAME = 5 (f_refc_schema, def fld_sch_name)
- RDB$CONST_SCHEMA_NAME_UQ = 6 (f_refc_uq_schema, def fld_sch_name)

### RDB$CHECK_CONSTRAINTS
- RDB$CONSTRAINT_NAME = 0 (f_ccon_cname, def fld_con_name)
- RDB$TRIGGER_NAME = 1 (f_ccon_tname, def fld_trg_name)
- RDB$SCHEMA_NAME = 2 (f_ccon_schema, def fld_sch_name)

### RDB$LOG_FILES
- RDB$FILE_NAME = 0 (f_log_name, def fld_file_name)
- RDB$FILE_SEQUENCE = 1 (f_log_seq, def fld_file_seq)
- RDB$FILE_LENGTH = 2 (f_log_length, def fld_file_length)
- RDB$FILE_PARTITIONS = 3 (f_log_partitions, def fld_file_partitions)
- RDB$FILE_P_OFFSET = 4 (f_log_p_offset, def fld_file_p_offset)
- RDB$FILE_FLAGS = 5 (f_log_flags, def fld_file_flags)

### RDB$PROCEDURES
- RDB$PROCEDURE_NAME = 0 (f_prc_name, def fld_prc_name)
- RDB$PROCEDURE_ID = 1 (f_prc_id, def fld_prc_id)
- RDB$PROCEDURE_INPUTS = 2 (f_prc_inputs, def fld_prc_prm)
- RDB$PROCEDURE_OUTPUTS = 3 (f_prc_outputs, def fld_prc_prm)
- RDB$DESCRIPTION = 4 (f_prc_desc, def fld_description)
- RDB$PROCEDURE_SOURCE = 5 (f_prc_source, def fld_source)
- RDB$PROCEDURE_BLR = 6 (f_prc_blr, def fld_prc_blr)
- RDB$SECURITY_CLASS = 7 (f_prc_class, def fld_class)
- RDB$OWNER_NAME = 8 (f_prc_owner, def fld_user)
- RDB$RUNTIME = 9 (f_prc_runtime, def fld_runtime)
- RDB$SYSTEM_FLAG = 10 (f_prc_sys_flag, def fld_flag)
- RDB$PROCEDURE_TYPE = 11 (f_prc_type, def fld_prc_type)
- RDB$VALID_BLR = 12 (f_prc_valid_blr, def fld_flag_nullable)
- RDB$DEBUG_INFO = 13 (f_prc_debug_info, def fld_debug_info)
- RDB$ENGINE_NAME = 14 (f_prc_engine_name, def fld_engine_name)
- RDB$ENTRYPOINT = 15 (f_prc_entry, def fld_ext_name)
- RDB$PACKAGE_NAME = 16 (f_prc_pkg_name, def fld_pkg_name)
- RDB$PRIVATE_FLAG = 17 (f_prc_private_flag, def fld_flag_nullable)
- RDB$SQL_SECURITY = 18 (f_prc_sql_security, def fld_b_sql_security)
- RDB$SCHEMA_NAME = 19 (f_prc_schema, def fld_sch_name)

### RDB$PROCEDURE_PARAMETERS
- RDB$PARAMETER_NAME = 0 (f_prm_name, def fld_prm_name)
- RDB$PROCEDURE_NAME = 1 (f_prm_procedure, def fld_prc_name)
- RDB$PARAMETER_NUMBER = 2 (f_prm_number, def fld_prm_number)
- RDB$PARAMETER_TYPE = 3 (f_prm_type, def fld_prm_type)
- RDB$FIELD_SOURCE = 4 (f_prm_sname, def fld_f_name)
- RDB$DESCRIPTION = 5 (f_prm_desc, def fld_description)
- RDB$SYSTEM_FLAG = 6 (f_prm_sys_flag, def fld_flag)
- RDB$DEFAULT_VALUE = 7 (f_prm_default, def fld_value)
- RDB$DEFAULT_SOURCE = 8 (f_prm_dsource, def fld_source)
- RDB$COLLATION_ID = 9 (f_prm_coll_id, def fld_collate_id)
- RDB$NULL_FLAG = 10 (f_prm_null_flag, def fld_null_flag)
- RDB$PARAMETER_MECHANISM = 11 (f_prm_mech, def fld_mechanism)
- RDB$FIELD_NAME = 12 (f_prm_fname, def fld_f_name)
- RDB$RELATION_NAME = 13 (f_prm_rname, def fld_r_name)
- RDB$PACKAGE_NAME = 14 (f_prm_pkg_name, def fld_pkg_name)
- RDB$SCHEMA_NAME = 15 (f_prm_schema, def fld_sch_name)
- RDB$RELATION_SCHEMA_NAME = 16 (f_prm_rel_schema, def fld_sch_name)
- RDB$FIELD_SOURCE_SCHEMA_NAME = 17 (f_prm_field_source_schema, def fld_sch_name)

### RDB$CHARACTER_SETS
- RDB$CHARACTER_SET_NAME = 0 (f_cs_cs_name, def fld_charset_name)
- RDB$FORM_OF_USE = 1 (f_cs_form_of_use, def fld_gnr_name)
- RDB$NUMBER_OF_CHARACTERS = 2 (f_cs_num_chars, def fld_num_chars)
- RDB$DEFAULT_COLLATE_NAME = 3 (f_cs_def_collate, def fld_collate_name)
- RDB$CHARACTER_SET_ID = 4 (f_cs_id, def fld_charset_id)
- RDB$SYSTEM_FLAG = 5 (f_cs_sys_flag, def fld_flag)
- RDB$DESCRIPTION = 6 (f_cs_desc, def fld_description)
- RDB$FUNCTION_NAME = 7 (f_cs_fun_name, def fld_fun_name)
- RDB$BYTES_PER_CHARACTER = 8 (f_cs_bytes_char, def fld_f_length)
- RDB$SECURITY_CLASS = 9 (f_cs_class, def fld_class)
- RDB$OWNER_NAME = 10 (f_cs_owner, def fld_user)
- RDB$SCHEMA_NAME = 11 (f_cs_schema, def fld_sch_name)
- RDB$DEFAULT_COLLATE_SCHEMA_NAME = 12 (f_cs_def_coll_schema, def fld_sch_name)

### RDB$COLLATIONS
- RDB$COLLATION_NAME = 0 (f_coll_name, def fld_collate_name)
- RDB$COLLATION_ID = 1 (f_coll_id, def fld_collate_id)
- RDB$CHARACTER_SET_ID = 2 (f_coll_cs_id, def fld_charset_id)
- RDB$COLLATION_ATTRIBUTES = 3 (f_coll_attr, def fld_gnr_type)
- RDB$SYSTEM_FLAG = 4 (f_coll_sys_flag, def fld_flag)
- RDB$DESCRIPTION = 5 (f_coll_desc, def fld_description)
- RDB$FUNCTION_NAME = 6 (f_coll_fun_name, def fld_fun_name)
- RDB$BASE_COLLATION_NAME = 7 (f_coll_base_collation_name, def fld_collate_name)
- RDB$SPECIFIC_ATTRIBUTES = 8 (f_coll_specific_attr, def fld_specific_attr)
- RDB$SECURITY_CLASS = 9 (f_coll_class, def fld_class)
- RDB$OWNER_NAME = 10 (f_coll_owner, def fld_user)
- RDB$SCHEMA_NAME = 11 (f_coll_schema, def fld_sch_name)

### RDB$EXCEPTIONS
- RDB$EXCEPTION_NAME = 0 (f_xcp_name, def fld_xcp_name)
- RDB$EXCEPTION_NUMBER = 1 (f_xcp_number, def fld_xcp_number)
- RDB$MESSAGE = 2 (f_xcp_msg, def fld_msg)
- RDB$DESCRIPTION = 3 (f_xcp_desc, def fld_description)
- RDB$SYSTEM_FLAG = 4 (f_xcp_sys_flag, def fld_flag)
- RDB$SECURITY_CLASS = 5 (f_xcp_class, def fld_class)
- RDB$OWNER_NAME = 6 (f_xcp_owner, def fld_user)
- RDB$SCHEMA_NAME = 7 (f_xcp_schema, def fld_sch_name)

### RDB$ROLES
- RDB$ROLE_NAME = 0 (f_rol_name, def fld_user)
- RDB$OWNER_NAME = 1 (f_rol_owner, def fld_user)
- RDB$DESCRIPTION = 2 (f_rol_desc, def fld_description)
- RDB$SYSTEM_FLAG = 3 (f_rol_sys_flag, def fld_flag)
- RDB$SECURITY_CLASS = 4 (f_rol_class, def fld_class)
- RDB$SYSTEM_PRIVILEGES = 5 (f_rol_sys_priv, def fld_system_privileges)

### RDB$BACKUP_HISTORY
- RDB$BACKUP_ID = 0 (f_backup_id, def fld_backup_id)
- RDB$TIMESTAMP = 1 (f_backup_time, def fld_timestamp_tz)
- RDB$BACKUP_LEVEL = 2 (f_backup_level, def fld_backup_level)
- RDB$GUID = 3 (f_backup_guid, def fld_guid)
- RDB$SCN = 4 (f_backup_scn, def fld_scn)
- RDB$FILE_NAME = 5 (f_backup_name, def fld_file_name)

### MON$DATABASE
- MON$DATABASE_NAME = 0 (f_mon_db_name, def fld_file_name2)
- MON$PAGE_SIZE = 1 (f_mon_db_page_size, def fld_page_size)
- MON$ODS_MAJOR = 2 (f_mon_db_ods_major, def fld_ods_number)
- MON$ODS_MINOR = 3 (f_mon_db_ods_minor, def fld_ods_number)
- MON$OLDEST_TRANSACTION = 4 (f_mon_db_oit, def fld_trans_id)
- MON$OLDEST_ACTIVE = 5 (f_mon_db_oat, def fld_trans_id)
- MON$OLDEST_SNAPSHOT = 6 (f_mon_db_ost, def fld_trans_id)
- MON$NEXT_TRANSACTION = 7 (f_mon_db_nt, def fld_trans_id)
- MON$PAGE_BUFFERS = 8 (f_mon_db_page_bufs, def fld_page_bufs)
- MON$SQL_DIALECT = 9 (f_mon_db_dialect, def fld_sql_dialect)
- MON$SHUTDOWN_MODE = 10 (f_mon_db_shut_mode, def fld_shut_mode)
- MON$SWEEP_INTERVAL = 11 (f_mon_db_sweep_int, def fld_sweep_int)
- MON$READ_ONLY = 12 (f_mon_db_read_only, def fld_flag_nullable)
- MON$FORCED_WRITES = 13 (f_mon_db_forced_writes, def fld_flag_nullable)
- MON$RESERVE_SPACE = 14 (f_mon_db_res_space, def fld_flag_nullable)
- MON$CREATION_DATE = 15 (f_mon_db_created, def fld_timestamp_tz)
- MON$PAGES = 16 (f_mon_db_pages, def fld_counter)
- MON$STAT_ID = 17 (f_mon_db_stat_id, def fld_stat_id)
- MON$BACKUP_STATE = 18 (f_mon_db_backup_state, def fld_backup_state)
- MON$CRYPT_PAGE = 19 (f_mon_db_crypt_page, def fld_counter)
- MON$OWNER = 20 (f_mon_db_owner, def fld_user)
- MON$SEC_DATABASE = 21 (f_mon_db_secdb, def fld_sec_db)
- MON$CRYPT_STATE = 22 (f_mon_db_crypt_state, def fld_crypt_state)
- MON$GUID = 23 (f_mon_db_guid, def fld_guid)
- MON$FILE_ID = 24 (f_mon_db_file_id, def fld_file_id)
- MON$NEXT_ATTACHMENT = 25 (f_mon_db_na, def fld_att_id)
- MON$NEXT_STATEMENT = 26 (f_mon_db_ns, def fld_stmt_id)
- MON$REPLICA_MODE = 27 (f_mon_db_repl_mode, def fld_repl_mode)

### MON$ATTACHMENTS
- MON$ATTACHMENT_ID = 0 (f_mon_att_id, def fld_att_id)
- MON$SERVER_PID = 1 (f_mon_att_server_pid, def fld_pid)
- MON$STATE = 2 (f_mon_att_state, def fld_state)
- MON$ATTACHMENT_NAME = 3 (f_mon_att_name, def fld_file_name2)
- MON$USER = 4 (f_mon_att_user, def fld_user)
- MON$ROLE = 5 (f_mon_att_role, def fld_user)
- MON$REMOTE_PROTOCOL = 6 (f_mon_att_remote_proto, def fld_remote_proto)
- MON$REMOTE_ADDRESS = 7 (f_mon_att_remote_addr, def fld_remote_addr)
- MON$REMOTE_PID = 8 (f_mon_att_remote_pid, def fld_pid)
- MON$CHARACTER_SET_ID = 9 (f_mon_att_charset_id, def fld_charset_id)
- MON$TIMESTAMP = 10 (f_mon_att_timestamp, def fld_timestamp_tz)
- MON$GARBAGE_COLLECTION = 11 (f_mon_att_gc, def fld_flag_nullable)
- MON$REMOTE_PROCESS = 12 (f_mon_att_remote_process, def fld_file_name2)
- MON$STAT_ID = 13 (f_mon_att_stat_id, def fld_stat_id)
- MON$CLIENT_VERSION = 14 (f_mon_att_client_version, def fld_client_ver)
- MON$REMOTE_VERSION = 15 (f_mon_att_remote_version, def fld_remote_ver)
- MON$REMOTE_HOST = 16 (f_mon_att_remote_host, def fld_host_name)
- MON$REMOTE_OS_USER = 17 (f_mon_att_remote_os_user, def fld_os_user)
- MON$AUTH_METHOD = 18 (f_mon_att_auth_method, def fld_auth_method)
- MON$SYSTEM_FLAG = 19 (f_mon_att_sys_flag, def fld_flag)
- MON$IDLE_TIMEOUT = 20 (f_mon_att_idle_timeout, def fld_idle_timeout)
- MON$IDLE_TIMER = 21 (f_mon_att_idle_timer, def fld_idle_timer)
- MON$STATEMENT_TIMEOUT = 22 (f_mon_att_stmt_timeout, def fld_stmt_timeout)
- MON$WIRE_COMPRESSED = 23 (f_mon_att_wire_compressed, def fld_bool)
- MON$WIRE_ENCRYPTED = 24 (f_mon_att_wire_encrypted, def fld_bool)
- MON$WIRE_CRYPT_PLUGIN = 25 (f_mon_att_remote_crypt, def fld_remote_crypt)
- MON$SESSION_TIMEZONE = 26 (f_mon_att_session_tz, def fld_tz_name)
- MON$PARALLEL_WORKERS = 27 (f_mon_att_par_workers, def fld_par_workers)
- MON$SEARCH_PATH = 28 (f_mon_att_search_path, def fld_text_max)

### MON$TRANSACTIONS
- MON$TRANSACTION_ID = 0 (f_mon_tra_id, def fld_trans_id)
- MON$ATTACHMENT_ID = 1 (f_mon_tra_att_id, def fld_att_id)
- MON$STATE = 2 (f_mon_tra_state, def fld_state)
- MON$TIMESTAMP = 3 (f_mon_tra_timestamp, def fld_timestamp_tz)
- MON$TOP_TRANSACTION = 4 (f_mon_tra_top, def fld_trans_id)
- MON$OLDEST_TRANSACTION = 5 (f_mon_tra_oit, def fld_trans_id)
- MON$OLDEST_ACTIVE = 6 (f_mon_tra_oat, def fld_trans_id)
- MON$ISOLATION_MODE = 7 (f_mon_tra_iso_mode, def fld_iso_mode)
- MON$LOCK_TIMEOUT = 8 (f_mon_tra_lock_timeout, def fld_lock_timeout)
- MON$READ_ONLY = 9 (f_mon_tra_read_only, def fld_flag_nullable)
- MON$AUTO_COMMIT = 10 (f_mon_tra_auto_commit, def fld_flag_nullable)
- MON$AUTO_UNDO = 11 (f_mon_tra_auto_undo, def fld_flag_nullable)
- MON$STAT_ID = 12 (f_mon_tra_stat_id, def fld_stat_id)
- MON$AUTO_RELEASE_TEMP_BLOBID = 13 (f_mon_tra_auto_release_temp_blobid, def fld_flag_nullable)

### MON$STATEMENTS
- MON$STATEMENT_ID = 0 (f_mon_stmt_id, def fld_stmt_id)
- MON$ATTACHMENT_ID = 1 (f_mon_stmt_att_id, def fld_att_id)
- MON$TRANSACTION_ID = 2 (f_mon_stmt_tra_id, def fld_trans_id)
- MON$STATE = 3 (f_mon_stmt_state, def fld_state)
- MON$TIMESTAMP = 4 (f_mon_stmt_timestamp, def fld_timestamp_tz)
- MON$SQL_TEXT = 5 (f_mon_stmt_sql_text, def fld_source)
- MON$STAT_ID = 6 (f_mon_stmt_stat_id, def fld_stat_id)
- MON$EXPLAINED_PLAN = 7 (f_mon_stmt_expl_plan, def fld_source)
- MON$STATEMENT_TIMEOUT = 8 (f_mon_stmt_timeout, def fld_stmt_timeout)
- MON$STATEMENT_TIMER = 9 (f_mon_stmt_timer, def fld_stmt_timer)
- MON$COMPILED_STATEMENT_ID = 10 (f_mon_stmt_cmp_stmt_id, def fld_stmt_id)

### MON$CALL_STACK
- MON$CALL_ID = 0 (f_mon_call_id, def fld_call_id)
- MON$STATEMENT_ID = 1 (f_mon_call_stmt_id, def fld_stmt_id)
- MON$CALLER_ID = 2 (f_mon_call_caller_id, def fld_call_id)
- MON$OBJECT_NAME = 3 (f_mon_call_name, def fld_gnr_name)
- MON$OBJECT_TYPE = 4 (f_mon_call_type, def fld_obj_type)
- MON$TIMESTAMP = 5 (f_mon_call_timestamp, def fld_timestamp_tz)
- MON$SOURCE_LINE = 6 (f_mon_call_src_line, def fld_src_info)
- MON$SOURCE_COLUMN = 7 (f_mon_call_src_column, def fld_src_info)
- MON$STAT_ID = 8 (f_mon_call_stat_id, def fld_stat_id)
- MON$PACKAGE_NAME = 9 (f_mon_call_pkg_name, def fld_pkg_name)
- MON$COMPILED_STATEMENT_ID = 10 (f_mon_call_cmp_stmt_id, def fld_stmt_id)
- MON$SCHEMA_NAME = 11 (f_mon_call_sch_name, def fld_sch_name)

### MON$IO_STATS
- MON$STAT_ID = 0 (f_mon_io_stat_id, def fld_stat_id)
- MON$STAT_GROUP = 1 (f_mon_io_stat_group, def fld_stat_group)
- MON$PAGE_READS = 2 (f_mon_io_page_reads, def fld_counter)
- MON$PAGE_WRITES = 3 (f_mon_io_page_writes, def fld_counter)
- MON$PAGE_FETCHES = 4 (f_mon_io_page_fetches, def fld_counter)
- MON$PAGE_MARKS = 5 (f_mon_io_page_marks, def fld_counter)

### MON$RECORD_STATS
- MON$STAT_ID = 0 (f_mon_rec_stat_id, def fld_stat_id)
- MON$STAT_GROUP = 1 (f_mon_rec_stat_group, def fld_stat_group)
- MON$RECORD_SEQ_READS = 2 (f_mon_rec_seq_reads, def fld_counter)
- MON$RECORD_IDX_READS = 3 (f_mon_rec_idx_reads, def fld_counter)
- MON$RECORD_INSERTS = 4 (f_mon_rec_inserts, def fld_counter)
- MON$RECORD_UPDATES = 5 (f_mon_rec_updates, def fld_counter)
- MON$RECORD_DELETES = 6 (f_mon_rec_deletes, def fld_counter)
- MON$RECORD_BACKOUTS = 7 (f_mon_rec_backouts, def fld_counter)
- MON$RECORD_PURGES = 8 (f_mon_rec_purges, def fld_counter)
- MON$RECORD_EXPUNGES = 9 (f_mon_rec_expunges, def fld_counter)
- MON$RECORD_LOCKS = 10 (f_mon_rec_locks, def fld_counter)
- MON$RECORD_WAITS = 11 (f_mon_rec_waits, def fld_counter)
- MON$RECORD_CONFLICTS = 12 (f_mon_rec_conflicts, def fld_counter)
- MON$BACKVERSION_READS = 13 (f_mon_rec_bkver_reads, def fld_counter)
- MON$FRAGMENT_READS = 14 (f_mon_rec_frg_reads, def fld_counter)
- MON$RECORD_RPT_READS = 15 (f_mon_rec_rpt_reads, def fld_counter)
- MON$RECORD_IMGC = 16 (f_mon_rec_imgc, def fld_counter)

### MON$CONTEXT_VARIABLES
- MON$ATTACHMENT_ID = 0 (f_mon_ctx_var_att_id, def fld_att_id)
- MON$TRANSACTION_ID = 1 (f_mon_ctx_var_tra_id, def fld_trans_id)
- MON$VARIABLE_NAME = 2 (f_mon_ctx_var_name, def fld_ctx_var_name)
- MON$VARIABLE_VALUE = 3 (f_mon_ctx_var_value, def fld_ctx_var_value)

### MON$MEMORY_USAGE
- MON$STAT_ID = 0 (f_mon_mem_stat_id, def fld_stat_id)
- MON$STAT_GROUP = 1 (f_mon_mem_stat_group, def fld_stat_group)
- MON$MEMORY_USED = 2 (f_mon_mem_cur_used, def fld_counter)
- MON$MEMORY_ALLOCATED = 3 (f_mon_mem_cur_alloc, def fld_counter)
- MON$MAX_MEMORY_USED = 4 (f_mon_mem_max_used, def fld_counter)
- MON$MAX_MEMORY_ALLOCATED = 5 (f_mon_mem_max_alloc, def fld_counter)

### RDB$PACKAGES
- RDB$PACKAGE_NAME = 0 (f_pkg_name, def fld_pkg_name)
- RDB$PACKAGE_HEADER_SOURCE = 1 (f_pkg_header_source, def fld_source)
- RDB$PACKAGE_BODY_SOURCE = 2 (f_pkg_body_source, def fld_source)
- RDB$VALID_BODY_FLAG = 3 (f_pkg_valid_body_flag, def fld_flag_nullable)
- RDB$SECURITY_CLASS = 4 (f_pkg_class, def fld_class)
- RDB$OWNER_NAME = 5 (f_pkg_owner, def fld_user)
- RDB$SYSTEM_FLAG = 6 (f_pkg_sys_flag, def fld_flag)
- RDB$DESCRIPTION = 7 (f_pkg_desc, def fld_description)
- RDB$SQL_SECURITY = 8 (f_pkg_sql_security, def fld_b_sql_security)
- RDB$SCHEMA_NAME = 9 (f_pkg_schema, def fld_sch_name)

### SEC$USERS
- SEC$USER_NAME = 0 (f_sec_user_name, def fld_user)
- SEC$FIRST_NAME = 1 (f_sec_first_name, def fld_name_part)
- SEC$MIDDLE_NAME = 2 (f_sec_middle_name, def fld_name_part)
- SEC$LAST_NAME = 3 (f_sec_last_name, def fld_name_part)
- SEC$ACTIVE = 4 (f_sec_active, def fld_bool)
- SEC$ADMIN = 5 (f_sec_admin, def fld_bool)
- SEC$DESCRIPTION = 6 (f_sec_comment, def fld_description)
- SEC$PLUGIN = 7 (f_sec_plugin, def fld_plugin_name)

### SEC$USER_ATTRIBUTES
- SEC$USER_NAME = 0 (f_sec_attr_user, def fld_user)
- SEC$KEY = 1 (f_sec_attr_key, def fld_attr_key)
- SEC$VALUE = 2 (f_sec_attr_value, def fld_attr_value)
- SEC$PLUGIN = 3 (f_sec_attr_plugin, def fld_plugin_name)

### RDB$AUTH_MAPPING
- RDB$MAP_NAME = 0 (f_map_name, def fld_map_name)
- RDB$MAP_USING = 1 (f_map_using, def fld_map_using)
- RDB$MAP_PLUGIN = 2 (f_map_plugin, def fld_plugin_name)
- RDB$MAP_DB = 3 (f_map_db, def fld_map_db)
- RDB$MAP_FROM_TYPE = 4 (f_map_from_type, def fld_map_from_type)
- RDB$MAP_FROM = 5 (f_map_from, def fld_map_from)
- RDB$MAP_TO_TYPE = 6 (f_map_to_type, def fld_obj_type)
- RDB$MAP_TO = 7 (f_map_to, def fld_map_to)
- RDB$SYSTEM_FLAG = 8 (f_map_sys_flag, def fld_flag)
- RDB$DESCRIPTION = 9 (f_map_desc, def fld_description)

### SEC$GLOBAL_AUTH_MAPPING
- SEC$MAP_NAME = 0 (f_sec_map_name, def fld_map_name)
- SEC$MAP_USING = 1 (f_sec_map_using, def fld_map_using)
- SEC$MAP_PLUGIN = 2 (f_sec_map_plugin, def fld_plugin_name)
- SEC$MAP_DB = 3 (f_sec_map_db, def fld_map_db)
- SEC$MAP_FROM_TYPE = 4 (f_sec_map_from_type, def fld_map_from_type)
- SEC$MAP_FROM = 5 (f_sec_map_from, def fld_map_from)
- SEC$MAP_TO_TYPE = 6 (f_sec_map_to_type, def fld_obj_type)
- SEC$MAP_TO = 7 (f_sec_map_to, def fld_map_to)
- SEC$DESCRIPTION = 8 (f_sec_map_comment, def fld_description)

### RDB$DB_CREATORS
- RDB$USER = 0 (f_crt_user, def fld_user)
- RDB$USER_TYPE = 1 (f_crt_u_type, def fld_obj_type)

### SEC$DB_CREATORS
- SEC$USER = 0 (f_sec_crt_user, def fld_user)
- SEC$USER_TYPE = 1 (f_sec_crt_u_type, def fld_obj_type)

### MON$TABLE_STATS
- MON$STAT_ID = 0 (f_mon_tab_stat_id, def fld_stat_id)
- MON$STAT_GROUP = 1 (f_mon_tab_stat_group, def fld_stat_group)
- MON$TABLE_NAME = 2 (f_mon_tab_name, def fld_r_name)
- MON$RECORD_STAT_ID = 3 (f_mon_tab_rec_stat_id, def fld_stat_id)
- MON$SCHEMA_NAME = 4 (f_mon_tab_sch_name, def fld_sch_name)
- MON$TABLE_TYPE = 5 (f_mon_tab_type, def fld_tab_type)

### RDB$TIME_ZONES
- RDB$TIME_ZONE_ID = 0 (f_tz_id, def fld_tz_id)
- RDB$TIME_ZONE_NAME = 1 (f_tz_name, def fld_tz_name)

### RDB$PUBLICATIONS
- RDB$PUBLICATION_NAME = 0 (f_pub_name, def fld_pub_name)
- RDB$OWNER_NAME = 1 (f_pub_owner, def fld_user)
- RDB$SYSTEM_FLAG = 2 (f_pub_sys_flag, def fld_flag)
- RDB$ACTIVE_FLAG = 3 (f_pub_active_flag, def fld_flag)
- RDB$AUTO_ENABLE = 4 (f_pub_auto_enable, def fld_flag)

### RDB$PUBLICATION_TABLES
- RDB$PUBLICATION_NAME = 0 (f_pubtab_pub_name, def fld_pub_name)
- RDB$TABLE_NAME = 1 (f_pubtab_tab_name, def fld_r_name)
- RDB$TABLE_SCHEMA_NAME = 2 (f_pubtab_tab_schema, def fld_sch_name)

### RDB$CONFIG
- RDB$CONFIG_ID = 0 (f_cfg_id, def fld_cfg_id)
- RDB$CONFIG_NAME = 1 (f_cfg_name, def fld_cfg_name)
- RDB$CONFIG_VALUE = 2 (f_cfg_value, def fld_cfg_value)
- RDB$CONFIG_DEFAULT = 3 (f_cfg_default, def fld_cfg_value)
- RDB$CONFIG_IS_SET = 4 (f_cfg_is_set, def fld_cfg_is_set)
- RDB$CONFIG_SOURCE = 5 (f_cfg_source, def fld_file_name2)

### RDB$KEYWORDS
- RDB$KEYWORD_NAME = 0 (f_keyword_name, def fld_keyword_name)
- RDB$KEYWORD_RESERVED = 1 (f_keyword_reserved, def fld_keyword_reserved)

### MON$COMPILED_STATEMENTS
- MON$COMPILED_STATEMENT_ID = 0 (f_mon_cmp_stmt_id, def fld_stmt_id)
- MON$SQL_TEXT = 1 (f_mon_cmp_stmt_sql_text, def fld_source)
- MON$EXPLAINED_PLAN = 2 (f_mon_cmp_stmt_expl_plan, def fld_source)
- MON$OBJECT_NAME = 3 (f_mon_cmp_stmt_name, def fld_gnr_name)
- MON$OBJECT_TYPE = 4 (f_mon_cmp_stmt_type, def fld_obj_type)
- MON$PACKAGE_NAME = 5 (f_mon_cmp_stmt_pkg_name, def fld_pkg_name)
- MON$STAT_ID = 6 (f_mon_cmp_stmt_stat_id, def fld_stat_id)
- MON$SCHEMA_NAME = 7 (f_mon_cmp_sch_name, def fld_sch_name)

### RDB$SCHEMAS
- RDB$SCHEMA_NAME = 0 (f_sch_schema, def fld_sch_name)
- RDB$OWNER_NAME = 1 (f_sch_owner, def fld_user)
- RDB$CHARACTER_SET_NAME = 2 (f_sch_charset, def fld_charset_name)
- RDB$CHARACTER_SET_SCHEMA_NAME = 3 (f_sch_charset_schema, def fld_sch_name)
- RDB$SQL_SECURITY = 4 (f_sch_sql_security, def fld_b_sql_security)
- RDB$SECURITY_CLASS = 5 (f_sch_class, def fld_class)
- RDB$SYSTEM_FLAG = 6 (f_sch_sys_flag, def fld_flag)
- RDB$DESCRIPTION = 7 (f_sch_desc, def fld_description)

### MON$LOCAL_TEMPORARY_TABLES
- MON$ATTACHMENT_ID = 0 (f_mon_ltt_att_id, def fld_att_id)
- MON$TABLE_ID = 1 (f_mon_ltt_id, def fld_integer)
- MON$TABLE_NAME = 2 (f_mon_ltt_name, def fld_r_name)
- MON$SCHEMA_NAME = 3 (f_mon_ltt_schema, def fld_sch_name)
- MON$TABLE_TYPE = 4 (f_mon_ltt_type, def fld_tab_type)

### MON$LOCAL_TEMPORARY_TABLE_COLUMNS
- MON$ATTACHMENT_ID = 0 (f_mon_lttc_att_id, def fld_att_id)
- MON$TABLE_NAME = 1 (f_mon_lttc_name, def fld_r_name)
- MON$SCHEMA_NAME = 2 (f_mon_lttc_schema, def fld_sch_name)
- MON$FIELD_NAME = 3 (f_mon_lttc_field_name, def fld_f_name)
- MON$FIELD_POSITION = 4 (f_mon_lttc_position, def fld_f_position)
- MON$FIELD_TYPE = 5 (f_mon_lttc_type, def fld_f_type)
- MON$FIELD_PRECISION = 6 (f_mon_lttc_precision, def fld_f_precision)
- MON$FIELD_SCALE = 7 (f_mon_lttc_scale, def fld_f_scale)
- MON$CHAR_LENGTH = 8 (f_mon_lttc_char_length, def fld_f_length)
- MON$FIELD_LENGTH = 9 (f_mon_lttc_length, def fld_f_length)
- MON$FIELD_SUB_TYPE = 10 (f_mon_lttc_sub_type, def fld_sub_type)
- MON$NOT_NULL = 11 (f_mon_lttc_not_null, def fld_null_flag)
- MON$CHARACTER_SET_ID = 12 (f_mon_lttc_charset_id, def fld_charset_id)
- MON$COLLATION_ID = 13 (f_mon_lttc_collate_id, def fld_collate_id)

## Bootstrap Rules (Authoritative)
The system catalog MUST be materialized so that queries against system tables return values exactly derivable from this specification.

### RDB$RELATIONS
For **each** relation in `formal/catalog.json`:
- Insert one row into `RDB$RELATIONS` with:
  - `RDB$SCHEMA_NAME` = `SYSTEM`
  - `RDB$RELATION_NAME` = relation name
  - `RDB$RELATION_ID` = numeric ID from this appendix
  - `RDB$RELATION_TYPE` = numeric `rel_*` value
  - `RDB$OWNER_NAME` = `<DB_OWNER>`
  - `RDB$SECURITY_CLASS` = `SQL$<n>` per `appendix_security_classes.md`
  - `RDB$DEFAULT_CLASS` = `SQL$DEFAULT<n>` per `appendix_security_classes.md`
  - `RDB$SYSTEM_FLAG` = 1
  - `RDB$DBKEY_LENGTH` = 8
  - `RDB$FORMAT` = 0
  - `RDB$FIELD_ID` = **field count** for this relation (number of fields)
- All other columns must be NULL unless explicitly defined elsewhere in this spec set.

### RDB$FIELDS
For **each** global field definition in `fields.h` (see `appendix_security_classes.md` for order):
- Insert one row into `RDB$FIELDS` with:
  - `RDB$SCHEMA_NAME` = `SYSTEM`
  - `RDB$FIELD_NAME` = field definition name
  - `RDB$FIELD_TYPE` = `blr_*` numeric code (per `store_global_field` mapping)
  - `RDB$FIELD_LENGTH` = evaluate `definition.length` using Size Evaluation Rules
  - `RDB$FIELD_SCALE` = 0
  - `RDB$FIELD_SUB_TYPE` / `RDB$CHARACTER_SET_ID` / `RDB$COLLATION_ID` / `RDB$SEGMENT_LENGTH` / `RDB$CHARACTER_LENGTH` = set per `store_global_field` rules (see default rows below)
  - `RDB$DEFAULT_VALUE` = NULL or binary-blob default BLR bytes (see default rows below)
  - `RDB$NULL_FLAG` = 1 if `definition.nullable` is `false`, else 0
  - `RDB$SYSTEM_FLAG` = 1
  - `RDB$OWNER_NAME` = `<DB_OWNER>`
  - `RDB$SECURITY_CLASS` = `SQL$<n>` per `appendix_security_classes.md`
- All other columns must be NULL unless explicitly defined elsewhere in this spec set.

### RDB$RELATION_FIELDS
For **each field occurrence** in each relation in `formal/catalog.json`:
- Insert one row into `RDB$RELATION_FIELDS` with:
  - `RDB$SCHEMA_NAME` = `SYSTEM`
  - `RDB$RELATION_NAME` = relation name
  - `RDB$FIELD_NAME` = field name
  - `RDB$FIELD_SOURCE_SCHEMA_NAME` = `SYSTEM`
  - `RDB$FIELD_SOURCE` = `definition.name`
  - `RDB$FIELD_POSITION` = field ID (numeric index)
  - `RDB$FIELD_ID` = field ID (numeric index)
  - `RDB$SYSTEM_FLAG` = 1
  - `RDB$UPDATE_FLAG` = value from `relations.h`
- All other columns must be NULL unless explicitly defined elsewhere in this spec set.

### Default Rows (Authoritative)
These rows MUST exist immediately after catalog bootstrap. Unless stated otherwise, any column not listed is NULL.

Tokenized values used in the row lists below:
- `<DB_OWNER>` = uppercase database owner user name (the authenticated user creating the database).
- `SQL$<n>` = security class name generated by `SQL_SECCLASS_GENERATOR` (`RDB$SECURITY_CLASS`). `DPM_gen_id(..., increment=1)` returns `n`; the stored value is `SQL$` + decimal `n`.

#### RDB$DATABASE (Single Row)
Insert exactly one row with these values at initial format time (other columns NULL):
- `RDB$RELATION_ID = USER_DEF_REL_INIT_ID` (128).
- `RDB$CHARACTER_SET_NAME =` default database character set object name (if CREATE DATABASE omitted this, use `NONE`).
- `RDB$CHARACTER_SET_SCHEMA_NAME =` schema for the default database character set (if omitted, use `SYSTEM`).
After DDL security bootstrap, set `RDB$SECURITY_CLASS` to a newly generated `SQL$<n>` (one call to `SQL_SECCLASS_GENERATOR`).

#### RDB$RELATIONS (Default Rows)
Rows must be inserted in the exact order listed. `RDB$SYSTEM_FLAG = RDB_system (1)` for all rows. `RDB$OWNER_NAME = <DB_OWNER>` for all rows. `RDB$SECURITY_CLASS` and `RDB$DEFAULT_CLASS` use the generator order in `appendix_security_classes.md`.

```csv
RDB$SCHEMA_NAME,RDB$RELATION_NAME,RDB$RELATION_ID,RDB$RELATION_TYPE,RDB$OWNER_NAME,RDB$SECURITY_CLASS,RDB$DEFAULT_CLASS,RDB$FIELD_ID,RDB$FORMAT,RDB$SYSTEM_FLAG,RDB$DBKEY_LENGTH
SYSTEM,RDB$PAGES,0,0,<DB_OWNER>,SQL$1,SQL$DEFAULT1,4,0,1,8
SYSTEM,RDB$DATABASE,1,0,<DB_OWNER>,SQL$2,SQL$DEFAULT2,7,0,1,8
SYSTEM,RDB$FIELDS,2,0,<DB_OWNER>,SQL$3,SQL$DEFAULT3,31,0,1,8
SYSTEM,RDB$INDEX_SEGMENTS,3,0,<DB_OWNER>,SQL$4,SQL$DEFAULT4,5,0,1,8
SYSTEM,RDB$INDICES,4,0,<DB_OWNER>,SQL$5,SQL$DEFAULT5,17,0,1,8
SYSTEM,RDB$RELATION_FIELDS,5,0,<DB_OWNER>,SQL$6,SQL$DEFAULT6,23,0,1,8
SYSTEM,RDB$RELATIONS,6,0,<DB_OWNER>,SQL$7,SQL$DEFAULT7,19,0,1,8
SYSTEM,RDB$VIEW_RELATIONS,7,0,<DB_OWNER>,SQL$8,SQL$DEFAULT8,8,0,1,8
SYSTEM,RDB$FORMATS,8,0,<DB_OWNER>,SQL$9,SQL$DEFAULT9,3,0,1,8
SYSTEM,RDB$SECURITY_CLASSES,9,0,<DB_OWNER>,SQL$10,SQL$DEFAULT10,3,0,1,8
SYSTEM,RDB$FILES,10,0,<DB_OWNER>,SQL$11,SQL$DEFAULT11,6,0,1,8
SYSTEM,RDB$TYPES,11,0,<DB_OWNER>,SQL$12,SQL$DEFAULT12,5,0,1,8
SYSTEM,RDB$TRIGGERS,12,0,<DB_OWNER>,SQL$13,SQL$DEFAULT13,16,0,1,8
SYSTEM,RDB$DEPENDENCIES,13,0,<DB_OWNER>,SQL$14,SQL$DEFAULT14,8,0,1,8
SYSTEM,RDB$FUNCTIONS,14,0,<DB_OWNER>,SQL$15,SQL$DEFAULT15,22,0,1,8
SYSTEM,RDB$FUNCTION_ARGUMENTS,15,0,<DB_OWNER>,SQL$16,SQL$DEFAULT16,25,0,1,8
SYSTEM,RDB$FILTERS,16,0,<DB_OWNER>,SQL$17,SQL$DEFAULT17,9,0,1,8
SYSTEM,RDB$TRIGGER_MESSAGES,17,0,<DB_OWNER>,SQL$18,SQL$DEFAULT18,4,0,1,8
SYSTEM,RDB$USER_PRIVILEGES,18,0,<DB_OWNER>,SQL$19,SQL$DEFAULT19,10,0,1,8
SYSTEM,RDB$TRANSACTIONS,19,0,<DB_OWNER>,SQL$20,SQL$DEFAULT20,4,0,1,8
SYSTEM,RDB$GENERATORS,20,0,<DB_OWNER>,SQL$21,SQL$DEFAULT21,9,0,1,8
SYSTEM,RDB$FIELD_DIMENSIONS,21,0,<DB_OWNER>,SQL$22,SQL$DEFAULT22,5,0,1,8
SYSTEM,RDB$RELATION_CONSTRAINTS,22,0,<DB_OWNER>,SQL$23,SQL$DEFAULT23,7,0,1,8
SYSTEM,RDB$REF_CONSTRAINTS,23,0,<DB_OWNER>,SQL$24,SQL$DEFAULT24,7,0,1,8
SYSTEM,RDB$CHECK_CONSTRAINTS,24,0,<DB_OWNER>,SQL$25,SQL$DEFAULT25,3,0,1,8
SYSTEM,RDB$LOG_FILES,25,0,<DB_OWNER>,SQL$26,SQL$DEFAULT26,6,0,1,8
SYSTEM,RDB$PROCEDURES,26,0,<DB_OWNER>,SQL$27,SQL$DEFAULT27,20,0,1,8
SYSTEM,RDB$PROCEDURE_PARAMETERS,27,0,<DB_OWNER>,SQL$28,SQL$DEFAULT28,18,0,1,8
SYSTEM,RDB$CHARACTER_SETS,28,0,<DB_OWNER>,SQL$29,SQL$DEFAULT29,13,0,1,8
SYSTEM,RDB$COLLATIONS,29,0,<DB_OWNER>,SQL$30,SQL$DEFAULT30,12,0,1,8
SYSTEM,RDB$EXCEPTIONS,30,0,<DB_OWNER>,SQL$31,SQL$DEFAULT31,8,0,1,8
SYSTEM,RDB$ROLES,31,0,<DB_OWNER>,SQL$32,SQL$DEFAULT32,6,0,1,8
SYSTEM,RDB$BACKUP_HISTORY,32,0,<DB_OWNER>,SQL$33,SQL$DEFAULT33,6,0,1,8
SYSTEM,MON$DATABASE,33,3,<DB_OWNER>,SQL$34,SQL$DEFAULT34,28,0,1,8
SYSTEM,MON$ATTACHMENTS,34,3,<DB_OWNER>,SQL$35,SQL$DEFAULT35,29,0,1,8
SYSTEM,MON$TRANSACTIONS,35,3,<DB_OWNER>,SQL$36,SQL$DEFAULT36,14,0,1,8
SYSTEM,MON$STATEMENTS,36,3,<DB_OWNER>,SQL$37,SQL$DEFAULT37,11,0,1,8
SYSTEM,MON$CALL_STACK,37,3,<DB_OWNER>,SQL$38,SQL$DEFAULT38,12,0,1,8
SYSTEM,MON$IO_STATS,38,3,<DB_OWNER>,SQL$39,SQL$DEFAULT39,6,0,1,8
SYSTEM,MON$RECORD_STATS,39,3,<DB_OWNER>,SQL$40,SQL$DEFAULT40,17,0,1,8
SYSTEM,MON$CONTEXT_VARIABLES,40,3,<DB_OWNER>,SQL$41,SQL$DEFAULT41,4,0,1,8
SYSTEM,MON$MEMORY_USAGE,41,3,<DB_OWNER>,SQL$42,SQL$DEFAULT42,6,0,1,8
SYSTEM,RDB$PACKAGES,42,0,<DB_OWNER>,SQL$43,SQL$DEFAULT43,10,0,1,8
SYSTEM,SEC$USERS,43,3,<DB_OWNER>,SQL$44,SQL$DEFAULT44,8,0,1,8
SYSTEM,SEC$USER_ATTRIBUTES,44,3,<DB_OWNER>,SQL$45,SQL$DEFAULT45,4,0,1,8
SYSTEM,RDB$AUTH_MAPPING,45,0,<DB_OWNER>,SQL$46,SQL$DEFAULT46,10,0,1,8
SYSTEM,SEC$GLOBAL_AUTH_MAPPING,46,3,<DB_OWNER>,SQL$47,SQL$DEFAULT47,9,0,1,8
SYSTEM,RDB$DB_CREATORS,47,0,<DB_OWNER>,SQL$48,SQL$DEFAULT48,2,0,1,8
SYSTEM,SEC$DB_CREATORS,48,3,<DB_OWNER>,SQL$49,SQL$DEFAULT49,2,0,1,8
SYSTEM,MON$TABLE_STATS,49,3,<DB_OWNER>,SQL$50,SQL$DEFAULT50,6,0,1,8
SYSTEM,RDB$TIME_ZONES,50,3,<DB_OWNER>,SQL$51,SQL$DEFAULT51,2,0,1,8
SYSTEM,RDB$PUBLICATIONS,51,0,<DB_OWNER>,SQL$52,SQL$DEFAULT52,5,0,1,8
SYSTEM,RDB$PUBLICATION_TABLES,52,0,<DB_OWNER>,SQL$53,SQL$DEFAULT53,3,0,1,8
SYSTEM,RDB$CONFIG,53,3,<DB_OWNER>,SQL$54,SQL$DEFAULT54,6,0,1,8
SYSTEM,RDB$KEYWORDS,54,3,<DB_OWNER>,SQL$55,SQL$DEFAULT55,2,0,1,8
SYSTEM,MON$COMPILED_STATEMENTS,55,3,<DB_OWNER>,SQL$56,SQL$DEFAULT56,8,0,1,8
SYSTEM,RDB$SCHEMAS,56,0,<DB_OWNER>,SQL$57,SQL$DEFAULT57,8,0,1,8
SYSTEM,MON$LOCAL_TEMPORARY_TABLES,57,3,<DB_OWNER>,SQL$58,SQL$DEFAULT58,5,0,1,8
SYSTEM,MON$LOCAL_TEMPORARY_TABLE_COLUMNS,58,3,<DB_OWNER>,SQL$59,SQL$DEFAULT59,14,0,1,8
```

#### RDB$RELATION_FIELDS (Default Rows)
Rows must be inserted in the exact order listed (relation order, then field order within the relation). `RDB$SYSTEM_FLAG = RDB_system (1)` for all rows.

```csv
RDB$SCHEMA_NAME,RDB$RELATION_NAME,RDB$FIELD_NAME,RDB$FIELD_SOURCE_SCHEMA_NAME,RDB$FIELD_SOURCE,RDB$FIELD_POSITION,RDB$FIELD_ID,RDB$SYSTEM_FLAG,RDB$UPDATE_FLAG
SYSTEM,RDB$PAGES,RDB$PAGE_NUMBER,SYSTEM,RDB$PAGE_NUMBER,0,0,1,0
SYSTEM,RDB$PAGES,RDB$RELATION_ID,SYSTEM,RDB$RELATION_ID,1,1,1,0
SYSTEM,RDB$PAGES,RDB$PAGE_SEQUENCE,SYSTEM,RDB$PAGE_SEQUENCE,2,2,1,0
SYSTEM,RDB$PAGES,RDB$PAGE_TYPE,SYSTEM,RDB$PAGE_TYPE,3,3,1,0
SYSTEM,RDB$DATABASE,RDB$DESCRIPTION,SYSTEM,RDB$DESCRIPTION,0,0,1,1
SYSTEM,RDB$DATABASE,RDB$RELATION_ID,SYSTEM,RDB$RELATION_ID,1,1,1,0
SYSTEM,RDB$DATABASE,RDB$SECURITY_CLASS,SYSTEM,RDB$SECURITY_CLASS,2,2,1,1
SYSTEM,RDB$DATABASE,RDB$CHARACTER_SET_NAME,SYSTEM,RDB$CHARACTER_SET_NAME,3,3,1,1
SYSTEM,RDB$DATABASE,RDB$LINGER,SYSTEM,RDB$LINGER,4,4,1,1
SYSTEM,RDB$DATABASE,RDB$SQL_SECURITY,SYSTEM,RDB$SQL_SECURITY,5,5,1,1
SYSTEM,RDB$DATABASE,RDB$CHARACTER_SET_SCHEMA_NAME,SYSTEM,RDB$SCHEMA_NAME,6,6,1,1
SYSTEM,RDB$FIELDS,RDB$FIELD_NAME,SYSTEM,RDB$FIELD_NAME,0,0,1,1
SYSTEM,RDB$FIELDS,RDB$QUERY_NAME,SYSTEM,RDB$FIELD_NAME,1,1,1,1
SYSTEM,RDB$FIELDS,RDB$VALIDATION_BLR,SYSTEM,RDB$VALIDATION_BLR,2,2,1,1
SYSTEM,RDB$FIELDS,RDB$VALIDATION_SOURCE,SYSTEM,RDB$SOURCE,3,3,1,1
SYSTEM,RDB$FIELDS,RDB$COMPUTED_BLR,SYSTEM,RDB$VALUE,4,4,1,1
SYSTEM,RDB$FIELDS,RDB$COMPUTED_SOURCE,SYSTEM,RDB$SOURCE,5,5,1,1
SYSTEM,RDB$FIELDS,RDB$DEFAULT_VALUE,SYSTEM,RDB$VALUE,6,6,1,1
SYSTEM,RDB$FIELDS,RDB$DEFAULT_SOURCE,SYSTEM,RDB$SOURCE,7,7,1,1
SYSTEM,RDB$FIELDS,RDB$FIELD_LENGTH,SYSTEM,RDB$FIELD_LENGTH,8,8,1,1
SYSTEM,RDB$FIELDS,RDB$FIELD_SCALE,SYSTEM,RDB$FIELD_SCALE,9,9,1,1
SYSTEM,RDB$FIELDS,RDB$FIELD_TYPE,SYSTEM,RDB$FIELD_TYPE,10,10,1,1
SYSTEM,RDB$FIELDS,RDB$FIELD_SUB_TYPE,SYSTEM,RDB$FIELD_SUB_TYPE,11,11,1,1
SYSTEM,RDB$FIELDS,RDB$MISSING_VALUE,SYSTEM,RDB$VALUE,12,12,1,1
SYSTEM,RDB$FIELDS,RDB$MISSING_SOURCE,SYSTEM,RDB$SOURCE,13,13,1,1
SYSTEM,RDB$FIELDS,RDB$DESCRIPTION,SYSTEM,RDB$DESCRIPTION,14,14,1,1
SYSTEM,RDB$FIELDS,RDB$SYSTEM_FLAG,SYSTEM,RDB$SYSTEM_FLAG,15,15,1,0
SYSTEM,RDB$FIELDS,RDB$QUERY_HEADER,SYSTEM,RDB$QUERY_HEADER,16,16,1,1
SYSTEM,RDB$FIELDS,RDB$SEGMENT_LENGTH,SYSTEM,RDB$SEGMENT_LENGTH,17,17,1,1
SYSTEM,RDB$FIELDS,RDB$EDIT_STRING,SYSTEM,RDB$EDIT_STRING,18,18,1,1
SYSTEM,RDB$FIELDS,RDB$EXTERNAL_LENGTH,SYSTEM,RDB$FIELD_LENGTH,19,19,1,1
SYSTEM,RDB$FIELDS,RDB$EXTERNAL_SCALE,SYSTEM,RDB$FIELD_SCALE,20,20,1,1
SYSTEM,RDB$FIELDS,RDB$EXTERNAL_TYPE,SYSTEM,RDB$FIELD_TYPE,21,21,1,1
SYSTEM,RDB$FIELDS,RDB$DIMENSIONS,SYSTEM,RDB$DIMENSIONS,22,22,1,1
SYSTEM,RDB$FIELDS,RDB$NULL_FLAG,SYSTEM,RDB$NULL_FLAG,23,23,1,1
SYSTEM,RDB$FIELDS,RDB$CHARACTER_LENGTH,SYSTEM,RDB$FIELD_LENGTH,24,24,1,1
SYSTEM,RDB$FIELDS,RDB$COLLATION_ID,SYSTEM,RDB$COLLATION_ID,25,25,1,1
SYSTEM,RDB$FIELDS,RDB$CHARACTER_SET_ID,SYSTEM,RDB$CHARACTER_SET_ID,26,26,1,1
SYSTEM,RDB$FIELDS,RDB$FIELD_PRECISION,SYSTEM,RDB$FIELD_PRECISION,27,27,1,1
SYSTEM,RDB$FIELDS,RDB$SECURITY_CLASS,SYSTEM,RDB$SECURITY_CLASS,28,28,1,1
SYSTEM,RDB$FIELDS,RDB$OWNER_NAME,SYSTEM,RDB$USER,29,29,1,1
SYSTEM,RDB$FIELDS,RDB$SCHEMA_NAME,SYSTEM,RDB$SCHEMA_NAME,30,30,1,1
SYSTEM,RDB$INDEX_SEGMENTS,RDB$INDEX_NAME,SYSTEM,RDB$INDEX_NAME,0,0,1,1
SYSTEM,RDB$INDEX_SEGMENTS,RDB$FIELD_NAME,SYSTEM,RDB$FIELD_NAME,1,1,1,1
SYSTEM,RDB$INDEX_SEGMENTS,RDB$FIELD_POSITION,SYSTEM,RDB$FIELD_POSITION,2,2,1,1
SYSTEM,RDB$INDEX_SEGMENTS,RDB$STATISTICS,SYSTEM,RDB$STATISTICS,3,3,1,1
SYSTEM,RDB$INDEX_SEGMENTS,RDB$SCHEMA_NAME,SYSTEM,RDB$SCHEMA_NAME,4,4,1,1
SYSTEM,RDB$INDICES,RDB$INDEX_NAME,SYSTEM,RDB$INDEX_NAME,0,0,1,1
SYSTEM,RDB$INDICES,RDB$RELATION_NAME,SYSTEM,RDB$RELATION_NAME,1,1,1,1
SYSTEM,RDB$INDICES,RDB$INDEX_ID,SYSTEM,RDB$INDEX_ID,2,2,1,0
SYSTEM,RDB$INDICES,RDB$UNIQUE_FLAG,SYSTEM,RDB$SYSTEM_NULLFLAG,3,3,1,1
SYSTEM,RDB$INDICES,RDB$DESCRIPTION,SYSTEM,RDB$DESCRIPTION,4,4,1,1
SYSTEM,RDB$INDICES,RDB$SEGMENT_COUNT,SYSTEM,RDB$SEGMENT_COUNT,5,5,1,1
SYSTEM,RDB$INDICES,RDB$INDEX_INACTIVE,SYSTEM,RDB$SYSTEM_NULLFLAG,6,6,1,1
SYSTEM,RDB$INDICES,RDB$INDEX_TYPE,SYSTEM,RDB$SYSTEM_NULLFLAG,7,7,1,1
SYSTEM,RDB$INDICES,RDB$FOREIGN_KEY,SYSTEM,RDB$RELATION_NAME,8,8,1,1
SYSTEM,RDB$INDICES,RDB$SYSTEM_FLAG,SYSTEM,RDB$SYSTEM_FLAG,9,9,1,0
SYSTEM,RDB$INDICES,RDB$EXPRESSION_BLR,SYSTEM,RDB$VALUE,10,10,1,1
SYSTEM,RDB$INDICES,RDB$EXPRESSION_SOURCE,SYSTEM,RDB$SOURCE,11,11,1,1
SYSTEM,RDB$INDICES,RDB$STATISTICS,SYSTEM,RDB$STATISTICS,12,12,1,1
SYSTEM,RDB$INDICES,RDB$CONDITION_BLR,SYSTEM,RDB$VALUE,13,13,1,1
SYSTEM,RDB$INDICES,RDB$CONDITION_SOURCE,SYSTEM,RDB$SOURCE,14,14,1,1
SYSTEM,RDB$INDICES,RDB$SCHEMA_NAME,SYSTEM,RDB$SCHEMA_NAME,15,15,1,1
SYSTEM,RDB$INDICES,RDB$FOREIGN_KEY_SCHEMA_NAME,SYSTEM,RDB$SCHEMA_NAME,16,16,1,1
SYSTEM,RDB$RELATION_FIELDS,RDB$FIELD_NAME,SYSTEM,RDB$FIELD_NAME,0,0,1,1
SYSTEM,RDB$RELATION_FIELDS,RDB$RELATION_NAME,SYSTEM,RDB$RELATION_NAME,1,1,1,1
SYSTEM,RDB$RELATION_FIELDS,RDB$FIELD_SOURCE,SYSTEM,RDB$FIELD_NAME,2,2,1,1
SYSTEM,RDB$RELATION_FIELDS,RDB$QUERY_NAME,SYSTEM,RDB$FIELD_NAME,3,3,1,1
SYSTEM,RDB$RELATION_FIELDS,RDB$BASE_FIELD,SYSTEM,RDB$FIELD_NAME,4,4,1,1
SYSTEM,RDB$RELATION_FIELDS,RDB$EDIT_STRING,SYSTEM,RDB$EDIT_STRING,5,5,1,1
SYSTEM,RDB$RELATION_FIELDS,RDB$FIELD_POSITION,SYSTEM,RDB$FIELD_POSITION,6,6,1,1
SYSTEM,RDB$RELATION_FIELDS,RDB$QUERY_HEADER,SYSTEM,RDB$QUERY_HEADER,7,7,1,1
SYSTEM,RDB$RELATION_FIELDS,RDB$UPDATE_FLAG,SYSTEM,RDB$SYSTEM_NULLFLAG,8,8,1,1
SYSTEM,RDB$RELATION_FIELDS,RDB$FIELD_ID,SYSTEM,RDB$FIELD_ID,9,9,1,0
SYSTEM,RDB$RELATION_FIELDS,RDB$VIEW_CONTEXT,SYSTEM,RDB$VIEW_CONTEXT,10,10,1,1
SYSTEM,RDB$RELATION_FIELDS,RDB$DESCRIPTION,SYSTEM,RDB$DESCRIPTION,11,11,1,1
SYSTEM,RDB$RELATION_FIELDS,RDB$DEFAULT_VALUE,SYSTEM,RDB$VALUE,12,12,1,1
SYSTEM,RDB$RELATION_FIELDS,RDB$SYSTEM_FLAG,SYSTEM,RDB$SYSTEM_FLAG,13,13,1,0
SYSTEM,RDB$RELATION_FIELDS,RDB$SECURITY_CLASS,SYSTEM,RDB$SECURITY_CLASS,14,14,1,1
SYSTEM,RDB$RELATION_FIELDS,RDB$COMPLEX_NAME,SYSTEM,RDB$FIELD_NAME,15,15,1,1
SYSTEM,RDB$RELATION_FIELDS,RDB$NULL_FLAG,SYSTEM,RDB$NULL_FLAG,16,16,1,1
SYSTEM,RDB$RELATION_FIELDS,RDB$DEFAULT_SOURCE,SYSTEM,RDB$SOURCE,17,17,1,1
SYSTEM,RDB$RELATION_FIELDS,RDB$COLLATION_ID,SYSTEM,RDB$COLLATION_ID,18,18,1,1
SYSTEM,RDB$RELATION_FIELDS,RDB$GENERATOR_NAME,SYSTEM,RDB$GENERATOR_NAME,19,19,1,1
SYSTEM,RDB$RELATION_FIELDS,RDB$IDENTITY_TYPE,SYSTEM,RDB$IDENTITY_TYPE,20,20,1,1
SYSTEM,RDB$RELATION_FIELDS,RDB$SCHEMA_NAME,SYSTEM,RDB$SCHEMA_NAME,21,21,1,1
SYSTEM,RDB$RELATION_FIELDS,RDB$FIELD_SOURCE_SCHEMA_NAME,SYSTEM,RDB$SCHEMA_NAME,22,22,1,1
SYSTEM,RDB$RELATIONS,RDB$VIEW_BLR,SYSTEM,RDB$VIEW_BLR,0,0,1,1
SYSTEM,RDB$RELATIONS,RDB$VIEW_SOURCE,SYSTEM,RDB$SOURCE,1,1,1,1
SYSTEM,RDB$RELATIONS,RDB$DESCRIPTION,SYSTEM,RDB$DESCRIPTION,2,2,1,1
SYSTEM,RDB$RELATIONS,RDB$RELATION_ID,SYSTEM,RDB$RELATION_ID,3,3,1,0
SYSTEM,RDB$RELATIONS,RDB$SYSTEM_FLAG,SYSTEM,RDB$SYSTEM_FLAG,4,4,1,0
SYSTEM,RDB$RELATIONS,RDB$DBKEY_LENGTH,SYSTEM,RDB$DBKEY_LENGTH,5,5,1,0
SYSTEM,RDB$RELATIONS,RDB$FORMAT,SYSTEM,RDB$FORMAT,6,6,1,0
SYSTEM,RDB$RELATIONS,RDB$FIELD_ID,SYSTEM,RDB$FIELD_ID,7,7,1,0
SYSTEM,RDB$RELATIONS,RDB$RELATION_NAME,SYSTEM,RDB$RELATION_NAME,8,8,1,1
SYSTEM,RDB$RELATIONS,RDB$SECURITY_CLASS,SYSTEM,RDB$SECURITY_CLASS,9,9,1,1
SYSTEM,RDB$RELATIONS,RDB$EXTERNAL_FILE,SYSTEM,RDB$FILE_NAME,10,10,1,1
SYSTEM,RDB$RELATIONS,RDB$RUNTIME,SYSTEM,RDB$RUNTIME,11,11,1,1
SYSTEM,RDB$RELATIONS,RDB$EXTERNAL_DESCRIPTION,SYSTEM,RDB$EXTERNAL_DESCRIPTION,12,12,1,1
SYSTEM,RDB$RELATIONS,RDB$OWNER_NAME,SYSTEM,RDB$USER,13,13,1,1
SYSTEM,RDB$RELATIONS,RDB$DEFAULT_CLASS,SYSTEM,RDB$SECURITY_CLASS,14,14,1,1
SYSTEM,RDB$RELATIONS,RDB$FLAGS,SYSTEM,RDB$SYSTEM_NULLFLAG,15,15,1,0
SYSTEM,RDB$RELATIONS,RDB$RELATION_TYPE,SYSTEM,RDB$RELATION_TYPE,16,16,1,0
SYSTEM,RDB$RELATIONS,RDB$SQL_SECURITY,SYSTEM,RDB$SQL_SECURITY,17,17,1,1
SYSTEM,RDB$RELATIONS,RDB$SCHEMA_NAME,SYSTEM,RDB$SCHEMA_NAME,18,18,1,1
SYSTEM,RDB$VIEW_RELATIONS,RDB$VIEW_NAME,SYSTEM,RDB$RELATION_NAME,0,0,1,1
SYSTEM,RDB$VIEW_RELATIONS,RDB$RELATION_NAME,SYSTEM,RDB$RELATION_NAME,1,1,1,1
SYSTEM,RDB$VIEW_RELATIONS,RDB$VIEW_CONTEXT,SYSTEM,RDB$VIEW_CONTEXT,2,2,1,1
SYSTEM,RDB$VIEW_RELATIONS,RDB$CONTEXT_NAME,SYSTEM,RDB$CONTEXT_NAME,3,3,1,1
SYSTEM,RDB$VIEW_RELATIONS,RDB$CONTEXT_TYPE,SYSTEM,RDB$VIEW_CONTEXT,4,4,1,1
SYSTEM,RDB$VIEW_RELATIONS,RDB$PACKAGE_NAME,SYSTEM,RDB$PACKAGE_NAME,5,5,1,1
SYSTEM,RDB$VIEW_RELATIONS,RDB$SCHEMA_NAME,SYSTEM,RDB$SCHEMA_NAME,6,6,1,1
SYSTEM,RDB$VIEW_RELATIONS,RDB$RELATION_SCHEMA_NAME,SYSTEM,RDB$SCHEMA_NAME,7,7,1,1
SYSTEM,RDB$FORMATS,RDB$RELATION_ID,SYSTEM,RDB$RELATION_ID,0,0,1,0
SYSTEM,RDB$FORMATS,RDB$FORMAT,SYSTEM,RDB$FORMAT,1,1,1,0
SYSTEM,RDB$FORMATS,RDB$DESCRIPTOR,SYSTEM,RDB$DESCRIPTOR,2,2,1,0
SYSTEM,RDB$SECURITY_CLASSES,RDB$SECURITY_CLASS,SYSTEM,RDB$SECURITY_CLASS,0,0,1,1
SYSTEM,RDB$SECURITY_CLASSES,RDB$ACL,SYSTEM,RDB$ACL,1,1,1,1
SYSTEM,RDB$SECURITY_CLASSES,RDB$DESCRIPTION,SYSTEM,RDB$DESCRIPTION,2,2,1,1
SYSTEM,RDB$FILES,RDB$FILE_NAME,SYSTEM,RDB$FILE_NAME,0,0,1,1
SYSTEM,RDB$FILES,RDB$FILE_SEQUENCE,SYSTEM,RDB$FILE_SEQUENCE,1,1,1,1
SYSTEM,RDB$FILES,RDB$FILE_START,SYSTEM,RDB$FILE_START,2,2,1,1
SYSTEM,RDB$FILES,RDB$FILE_LENGTH,SYSTEM,RDB$FILE_LENGTH,3,3,1,1
SYSTEM,RDB$FILES,RDB$FILE_FLAGS,SYSTEM,RDB$FILE_FLAGS,4,4,1,1
SYSTEM,RDB$FILES,RDB$SHADOW_NUMBER,SYSTEM,RDB$SHADOW_NUMBER,5,5,1,1
SYSTEM,RDB$TYPES,RDB$FIELD_NAME,SYSTEM,RDB$FIELD_NAME,0,0,1,1
SYSTEM,RDB$TYPES,RDB$TYPE,SYSTEM,RDB$GENERIC_TYPE,1,1,1,1
SYSTEM,RDB$TYPES,RDB$TYPE_NAME,SYSTEM,RDB$TYPE_NAME,2,2,1,1
SYSTEM,RDB$TYPES,RDB$DESCRIPTION,SYSTEM,RDB$DESCRIPTION,3,3,1,1
SYSTEM,RDB$TYPES,RDB$SYSTEM_FLAG,SYSTEM,RDB$SYSTEM_FLAG,4,4,1,1
SYSTEM,RDB$TRIGGERS,RDB$TRIGGER_NAME,SYSTEM,RDB$TRIGGER_NAME,0,0,1,1
SYSTEM,RDB$TRIGGERS,RDB$RELATION_NAME,SYSTEM,RDB$RELATION_NAME,1,1,1,1
SYSTEM,RDB$TRIGGERS,RDB$TRIGGER_SEQUENCE,SYSTEM,RDB$TRIGGER_SEQUENCE,2,2,1,1
SYSTEM,RDB$TRIGGERS,RDB$TRIGGER_TYPE,SYSTEM,RDB$TRIGGER_TYPE,3,3,1,1
SYSTEM,RDB$TRIGGERS,RDB$TRIGGER_SOURCE,SYSTEM,RDB$SOURCE,4,4,1,1
SYSTEM,RDB$TRIGGERS,RDB$TRIGGER_BLR,SYSTEM,RDB$TRIGGER_BLR,5,5,1,1
SYSTEM,RDB$TRIGGERS,RDB$DESCRIPTION,SYSTEM,RDB$DESCRIPTION,6,6,1,1
SYSTEM,RDB$TRIGGERS,RDB$TRIGGER_INACTIVE,SYSTEM,RDB$SYSTEM_NULLFLAG,7,7,1,1
SYSTEM,RDB$TRIGGERS,RDB$SYSTEM_FLAG,SYSTEM,RDB$SYSTEM_FLAG,8,8,1,1
SYSTEM,RDB$TRIGGERS,RDB$FLAGS,SYSTEM,RDB$SYSTEM_NULLFLAG,9,9,1,1
SYSTEM,RDB$TRIGGERS,RDB$VALID_BLR,SYSTEM,RDB$SYSTEM_NULLFLAG,10,10,1,1
SYSTEM,RDB$TRIGGERS,RDB$DEBUG_INFO,SYSTEM,RDB$DEBUG_INFO,11,11,1,1
SYSTEM,RDB$TRIGGERS,RDB$ENGINE_NAME,SYSTEM,RDB$ENGINE_NAME,12,12,1,1
SYSTEM,RDB$TRIGGERS,RDB$ENTRYPOINT,SYSTEM,RDB$EXTERNAL_NAME,13,13,1,1
SYSTEM,RDB$TRIGGERS,RDB$SQL_SECURITY,SYSTEM,RDB$SQL_SECURITY,14,14,1,1
SYSTEM,RDB$TRIGGERS,RDB$SCHEMA_NAME,SYSTEM,RDB$SCHEMA_NAME,15,15,1,1
SYSTEM,RDB$DEPENDENCIES,RDB$DEPENDENT_NAME,SYSTEM,RDB$GENERIC_NAME,0,0,1,1
SYSTEM,RDB$DEPENDENCIES,RDB$DEPENDED_ON_NAME,SYSTEM,RDB$GENERIC_NAME,1,1,1,1
SYSTEM,RDB$DEPENDENCIES,RDB$FIELD_NAME,SYSTEM,RDB$FIELD_NAME,2,2,1,1
SYSTEM,RDB$DEPENDENCIES,RDB$DEPENDENT_TYPE,SYSTEM,RDB$OBJECT_TYPE,3,3,1,1
SYSTEM,RDB$DEPENDENCIES,RDB$DEPENDED_ON_TYPE,SYSTEM,RDB$OBJECT_TYPE,4,4,1,1
SYSTEM,RDB$DEPENDENCIES,RDB$PACKAGE_NAME,SYSTEM,RDB$PACKAGE_NAME,5,5,1,1
SYSTEM,RDB$DEPENDENCIES,RDB$DEPENDENT_SCHEMA_NAME,SYSTEM,RDB$SCHEMA_NAME,6,6,1,1
SYSTEM,RDB$DEPENDENCIES,RDB$DEPENDED_ON_SCHEMA_NAME,SYSTEM,RDB$SCHEMA_NAME,7,7,1,1
SYSTEM,RDB$FUNCTIONS,RDB$FUNCTION_NAME,SYSTEM,RDB$FUNCTION_NAME,0,0,1,1
SYSTEM,RDB$FUNCTIONS,RDB$FUNCTION_TYPE,SYSTEM,RDB$FUNCTION_TYPE,1,1,1,1
SYSTEM,RDB$FUNCTIONS,RDB$QUERY_NAME,SYSTEM,RDB$FIELD_NAME,2,2,1,1
SYSTEM,RDB$FUNCTIONS,RDB$DESCRIPTION,SYSTEM,RDB$DESCRIPTION,3,3,1,1
SYSTEM,RDB$FUNCTIONS,RDB$MODULE_NAME,SYSTEM,RDB$FILE_NAME,4,4,1,1
SYSTEM,RDB$FUNCTIONS,RDB$ENTRYPOINT,SYSTEM,RDB$EXTERNAL_NAME,5,5,1,1
SYSTEM,RDB$FUNCTIONS,RDB$RETURN_ARGUMENT,SYSTEM,RDB$FIELD_POSITION,6,6,1,1
SYSTEM,RDB$FUNCTIONS,RDB$SYSTEM_FLAG,SYSTEM,RDB$SYSTEM_FLAG,7,7,1,0
SYSTEM,RDB$FUNCTIONS,RDB$ENGINE_NAME,SYSTEM,RDB$ENGINE_NAME,8,8,1,1
SYSTEM,RDB$FUNCTIONS,RDB$PACKAGE_NAME,SYSTEM,RDB$PACKAGE_NAME,9,9,1,1
SYSTEM,RDB$FUNCTIONS,RDB$PRIVATE_FLAG,SYSTEM,RDB$SYSTEM_NULLFLAG,10,10,1,1
SYSTEM,RDB$FUNCTIONS,RDB$FUNCTION_SOURCE,SYSTEM,RDB$SOURCE,11,11,1,1
SYSTEM,RDB$FUNCTIONS,RDB$FUNCTION_ID,SYSTEM,RDB$FUNCTION_ID,12,12,1,0
SYSTEM,RDB$FUNCTIONS,RDB$FUNCTION_BLR,SYSTEM,RDB$FUNCTION_BLR,13,13,1,1
SYSTEM,RDB$FUNCTIONS,RDB$VALID_BLR,SYSTEM,RDB$SYSTEM_NULLFLAG,14,14,1,1
SYSTEM,RDB$FUNCTIONS,RDB$DEBUG_INFO,SYSTEM,RDB$DEBUG_INFO,15,15,1,1
SYSTEM,RDB$FUNCTIONS,RDB$SECURITY_CLASS,SYSTEM,RDB$SECURITY_CLASS,16,16,1,1
SYSTEM,RDB$FUNCTIONS,RDB$OWNER_NAME,SYSTEM,RDB$USER,17,17,1,1
SYSTEM,RDB$FUNCTIONS,RDB$LEGACY_FLAG,SYSTEM,RDB$SYSTEM_NULLFLAG,18,18,1,0
SYSTEM,RDB$FUNCTIONS,RDB$DETERMINISTIC_FLAG,SYSTEM,RDB$SYSTEM_NULLFLAG,19,19,1,0
SYSTEM,RDB$FUNCTIONS,RDB$SQL_SECURITY,SYSTEM,RDB$SQL_SECURITY,20,20,1,1
SYSTEM,RDB$FUNCTIONS,RDB$SCHEMA_NAME,SYSTEM,RDB$SCHEMA_NAME,21,21,1,1
SYSTEM,RDB$FUNCTION_ARGUMENTS,RDB$FUNCTION_NAME,SYSTEM,RDB$FUNCTION_NAME,0,0,1,1
SYSTEM,RDB$FUNCTION_ARGUMENTS,RDB$ARGUMENT_POSITION,SYSTEM,RDB$FIELD_POSITION,1,1,1,1
SYSTEM,RDB$FUNCTION_ARGUMENTS,RDB$MECHANISM,SYSTEM,RDB$MECHANISM,2,2,1,1
SYSTEM,RDB$FUNCTION_ARGUMENTS,RDB$FIELD_TYPE,SYSTEM,RDB$FIELD_TYPE,3,3,1,0
SYSTEM,RDB$FUNCTION_ARGUMENTS,RDB$FIELD_SCALE,SYSTEM,RDB$FIELD_SCALE,4,4,1,0
SYSTEM,RDB$FUNCTION_ARGUMENTS,RDB$FIELD_LENGTH,SYSTEM,RDB$FIELD_LENGTH,5,5,1,0
SYSTEM,RDB$FUNCTION_ARGUMENTS,RDB$FIELD_SUB_TYPE,SYSTEM,RDB$FIELD_SUB_TYPE,6,6,1,0
SYSTEM,RDB$FUNCTION_ARGUMENTS,RDB$CHARACTER_SET_ID,SYSTEM,RDB$CHARACTER_SET_ID,7,7,1,1
SYSTEM,RDB$FUNCTION_ARGUMENTS,RDB$FIELD_PRECISION,SYSTEM,RDB$FIELD_PRECISION,8,8,1,1
SYSTEM,RDB$FUNCTION_ARGUMENTS,RDB$CHARACTER_LENGTH,SYSTEM,RDB$FIELD_LENGTH,9,9,1,1
SYSTEM,RDB$FUNCTION_ARGUMENTS,RDB$PACKAGE_NAME,SYSTEM,RDB$PACKAGE_NAME,10,10,1,1
SYSTEM,RDB$FUNCTION_ARGUMENTS,RDB$ARGUMENT_NAME,SYSTEM,RDB$ARGUMENT_NAME,11,11,1,1
SYSTEM,RDB$FUNCTION_ARGUMENTS,RDB$FIELD_SOURCE,SYSTEM,RDB$FIELD_NAME,12,12,1,1
SYSTEM,RDB$FUNCTION_ARGUMENTS,RDB$DEFAULT_VALUE,SYSTEM,RDB$VALUE,13,13,1,1
SYSTEM,RDB$FUNCTION_ARGUMENTS,RDB$DEFAULT_SOURCE,SYSTEM,RDB$SOURCE,14,14,1,1
SYSTEM,RDB$FUNCTION_ARGUMENTS,RDB$COLLATION_ID,SYSTEM,RDB$COLLATION_ID,15,15,1,1
SYSTEM,RDB$FUNCTION_ARGUMENTS,RDB$NULL_FLAG,SYSTEM,RDB$NULL_FLAG,16,16,1,1
SYSTEM,RDB$FUNCTION_ARGUMENTS,RDB$ARGUMENT_MECHANISM,SYSTEM,RDB$ARGUMENT_MECHANISM,17,17,1,1
SYSTEM,RDB$FUNCTION_ARGUMENTS,RDB$FIELD_NAME,SYSTEM,RDB$FIELD_NAME,18,18,1,1
SYSTEM,RDB$FUNCTION_ARGUMENTS,RDB$RELATION_NAME,SYSTEM,RDB$RELATION_NAME,19,19,1,1
SYSTEM,RDB$FUNCTION_ARGUMENTS,RDB$SYSTEM_FLAG,SYSTEM,RDB$SYSTEM_FLAG,20,20,1,0
SYSTEM,RDB$FUNCTION_ARGUMENTS,RDB$DESCRIPTION,SYSTEM,RDB$DESCRIPTION,21,21,1,1
SYSTEM,RDB$FUNCTION_ARGUMENTS,RDB$SCHEMA_NAME,SYSTEM,RDB$SCHEMA_NAME,22,22,1,1
SYSTEM,RDB$FUNCTION_ARGUMENTS,RDB$RELATION_SCHEMA_NAME,SYSTEM,RDB$SCHEMA_NAME,23,23,1,1
SYSTEM,RDB$FUNCTION_ARGUMENTS,RDB$FIELD_SOURCE_SCHEMA_NAME,SYSTEM,RDB$SCHEMA_NAME,24,24,1,1
SYSTEM,RDB$FILTERS,RDB$FUNCTION_NAME,SYSTEM,RDB$FUNCTION_NAME,0,0,1,1
SYSTEM,RDB$FILTERS,RDB$DESCRIPTION,SYSTEM,RDB$DESCRIPTION,1,1,1,1
SYSTEM,RDB$FILTERS,RDB$MODULE_NAME,SYSTEM,RDB$FILE_NAME,2,2,1,1
SYSTEM,RDB$FILTERS,RDB$ENTRYPOINT,SYSTEM,RDB$EXTERNAL_NAME,3,3,1,1
SYSTEM,RDB$FILTERS,RDB$INPUT_SUB_TYPE,SYSTEM,RDB$FIELD_SUB_TYPE,4,4,1,1
SYSTEM,RDB$FILTERS,RDB$OUTPUT_SUB_TYPE,SYSTEM,RDB$FIELD_SUB_TYPE,5,5,1,1
SYSTEM,RDB$FILTERS,RDB$SYSTEM_FLAG,SYSTEM,RDB$SYSTEM_FLAG,6,6,1,0
SYSTEM,RDB$FILTERS,RDB$SECURITY_CLASS,SYSTEM,RDB$SECURITY_CLASS,7,7,1,1
SYSTEM,RDB$FILTERS,RDB$OWNER_NAME,SYSTEM,RDB$USER,8,8,1,1
SYSTEM,RDB$TRIGGER_MESSAGES,RDB$TRIGGER_NAME,SYSTEM,RDB$TRIGGER_NAME,0,0,1,1
SYSTEM,RDB$TRIGGER_MESSAGES,RDB$MESSAGE_NUMBER,SYSTEM,RDB$MESSAGE_NUMBER,1,1,1,1
SYSTEM,RDB$TRIGGER_MESSAGES,RDB$MESSAGE,SYSTEM,RDB$MESSAGE,2,2,1,1
SYSTEM,RDB$TRIGGER_MESSAGES,RDB$SCHEMA_NAME,SYSTEM,RDB$SCHEMA_NAME,3,3,1,1
SYSTEM,RDB$USER_PRIVILEGES,RDB$USER,SYSTEM,RDB$USER,0,0,1,1
SYSTEM,RDB$USER_PRIVILEGES,RDB$GRANTOR,SYSTEM,RDB$USER,1,1,1,1
SYSTEM,RDB$USER_PRIVILEGES,RDB$PRIVILEGE,SYSTEM,RDB$PRIVILEGE,2,2,1,1
SYSTEM,RDB$USER_PRIVILEGES,RDB$GRANT_OPTION,SYSTEM,RDB$SYSTEM_NULLFLAG,3,3,1,1
SYSTEM,RDB$USER_PRIVILEGES,RDB$RELATION_NAME,SYSTEM,RDB$GENERIC_NAME,4,4,1,1
SYSTEM,RDB$USER_PRIVILEGES,RDB$FIELD_NAME,SYSTEM,RDB$FIELD_NAME,5,5,1,1
SYSTEM,RDB$USER_PRIVILEGES,RDB$USER_TYPE,SYSTEM,RDB$OBJECT_TYPE,6,6,1,1
SYSTEM,RDB$USER_PRIVILEGES,RDB$OBJECT_TYPE,SYSTEM,RDB$OBJECT_TYPE,7,7,1,1
SYSTEM,RDB$USER_PRIVILEGES,RDB$RELATION_SCHEMA_NAME,SYSTEM,RDB$SCHEMA_NAME,8,8,1,1
SYSTEM,RDB$USER_PRIVILEGES,RDB$USER_SCHEMA_NAME,SYSTEM,RDB$SCHEMA_NAME,9,9,1,1
SYSTEM,RDB$TRANSACTIONS,RDB$TRANSACTION_ID,SYSTEM,RDB$TRANSACTION_ID,0,0,1,1
SYSTEM,RDB$TRANSACTIONS,RDB$TRANSACTION_STATE,SYSTEM,RDB$TRANSACTION_STATE,1,1,1,1
SYSTEM,RDB$TRANSACTIONS,RDB$TIMESTAMP,SYSTEM,RDB$TIMESTAMP_TZ,2,2,1,1
SYSTEM,RDB$TRANSACTIONS,RDB$TRANSACTION_DESCRIPTION,SYSTEM,RDB$TRANSACTION_DESCRIPTION,3,3,1,1
SYSTEM,RDB$GENERATORS,RDB$GENERATOR_NAME,SYSTEM,RDB$GENERATOR_NAME,0,0,1,1
SYSTEM,RDB$GENERATORS,RDB$GENERATOR_ID,SYSTEM,RDB$GENERATOR_ID,1,1,1,1
SYSTEM,RDB$GENERATORS,RDB$SYSTEM_FLAG,SYSTEM,RDB$SYSTEM_FLAG,2,2,1,1
SYSTEM,RDB$GENERATORS,RDB$DESCRIPTION,SYSTEM,RDB$DESCRIPTION,3,3,1,1
SYSTEM,RDB$GENERATORS,RDB$SECURITY_CLASS,SYSTEM,RDB$SECURITY_CLASS,4,4,1,1
SYSTEM,RDB$GENERATORS,RDB$OWNER_NAME,SYSTEM,RDB$USER,5,5,1,1
SYSTEM,RDB$GENERATORS,RDB$INITIAL_VALUE,SYSTEM,RDB$GENERATOR_VALUE,6,6,1,1
SYSTEM,RDB$GENERATORS,RDB$GENERATOR_INCREMENT,SYSTEM,RDB$GENERATOR_INCREMENT,7,7,1,1
SYSTEM,RDB$GENERATORS,RDB$SCHEMA_NAME,SYSTEM,RDB$SCHEMA_NAME,8,8,1,1
SYSTEM,RDB$FIELD_DIMENSIONS,RDB$FIELD_NAME,SYSTEM,RDB$FIELD_NAME,0,0,1,1
SYSTEM,RDB$FIELD_DIMENSIONS,RDB$DIMENSION,SYSTEM,RDB$DIMENSION,1,1,1,1
SYSTEM,RDB$FIELD_DIMENSIONS,RDB$LOWER_BOUND,SYSTEM,RDB$BOUND,2,2,1,1
SYSTEM,RDB$FIELD_DIMENSIONS,RDB$UPPER_BOUND,SYSTEM,RDB$BOUND,3,3,1,1
SYSTEM,RDB$FIELD_DIMENSIONS,RDB$SCHEMA_NAME,SYSTEM,RDB$SCHEMA_NAME,4,4,1,1
SYSTEM,RDB$RELATION_CONSTRAINTS,RDB$CONSTRAINT_NAME,SYSTEM,RDB$CONSTRAINT_NAME,0,0,1,1
SYSTEM,RDB$RELATION_CONSTRAINTS,RDB$CONSTRAINT_TYPE,SYSTEM,RDB$CONSTRAINT_TYPE,1,1,1,1
SYSTEM,RDB$RELATION_CONSTRAINTS,RDB$RELATION_NAME,SYSTEM,RDB$RELATION_NAME,2,2,1,1
SYSTEM,RDB$RELATION_CONSTRAINTS,RDB$DEFERRABLE,SYSTEM,RDB$DEFERRABLE,3,3,1,1
SYSTEM,RDB$RELATION_CONSTRAINTS,RDB$INITIALLY_DEFERRED,SYSTEM,RDB$DEFERRABLE,4,4,1,1
SYSTEM,RDB$RELATION_CONSTRAINTS,RDB$INDEX_NAME,SYSTEM,RDB$INDEX_NAME,5,5,1,1
SYSTEM,RDB$RELATION_CONSTRAINTS,RDB$SCHEMA_NAME,SYSTEM,RDB$SCHEMA_NAME,6,6,1,1
SYSTEM,RDB$REF_CONSTRAINTS,RDB$CONSTRAINT_NAME,SYSTEM,RDB$CONSTRAINT_NAME,0,0,1,1
SYSTEM,RDB$REF_CONSTRAINTS,RDB$CONST_NAME_UQ,SYSTEM,RDB$CONSTRAINT_NAME,1,1,1,1
SYSTEM,RDB$REF_CONSTRAINTS,RDB$MATCH_OPTION,SYSTEM,RDB$MATCH_OPTION,2,2,1,1
SYSTEM,RDB$REF_CONSTRAINTS,RDB$UPDATE_RULE,SYSTEM,RDB$RULE,3,3,1,1
SYSTEM,RDB$REF_CONSTRAINTS,RDB$DELETE_RULE,SYSTEM,RDB$RULE,4,4,1,1
SYSTEM,RDB$REF_CONSTRAINTS,RDB$SCHEMA_NAME,SYSTEM,RDB$SCHEMA_NAME,5,5,1,1
SYSTEM,RDB$REF_CONSTRAINTS,RDB$CONST_SCHEMA_NAME_UQ,SYSTEM,RDB$SCHEMA_NAME,6,6,1,1
SYSTEM,RDB$CHECK_CONSTRAINTS,RDB$CONSTRAINT_NAME,SYSTEM,RDB$CONSTRAINT_NAME,0,0,1,1
SYSTEM,RDB$CHECK_CONSTRAINTS,RDB$TRIGGER_NAME,SYSTEM,RDB$TRIGGER_NAME,1,1,1,1
SYSTEM,RDB$CHECK_CONSTRAINTS,RDB$SCHEMA_NAME,SYSTEM,RDB$SCHEMA_NAME,2,2,1,1
SYSTEM,RDB$LOG_FILES,RDB$FILE_NAME,SYSTEM,RDB$FILE_NAME,0,0,1,1
SYSTEM,RDB$LOG_FILES,RDB$FILE_SEQUENCE,SYSTEM,RDB$FILE_SEQUENCE,1,1,1,1
SYSTEM,RDB$LOG_FILES,RDB$FILE_LENGTH,SYSTEM,RDB$FILE_LENGTH,2,2,1,1
SYSTEM,RDB$LOG_FILES,RDB$FILE_PARTITIONS,SYSTEM,RDB$FILE_PARTITIONS,3,3,1,1
SYSTEM,RDB$LOG_FILES,RDB$FILE_P_OFFSET,SYSTEM,RDB$FILE_P_OFFSET,4,4,1,1
SYSTEM,RDB$LOG_FILES,RDB$FILE_FLAGS,SYSTEM,RDB$FILE_FLAGS,5,5,1,1
SYSTEM,RDB$PROCEDURES,RDB$PROCEDURE_NAME,SYSTEM,RDB$PROCEDURE_NAME,0,0,1,1
SYSTEM,RDB$PROCEDURES,RDB$PROCEDURE_ID,SYSTEM,RDB$PROCEDURE_ID,1,1,1,0
SYSTEM,RDB$PROCEDURES,RDB$PROCEDURE_INPUTS,SYSTEM,RDB$PROCEDURE_PARAMETERS,2,2,1,1
SYSTEM,RDB$PROCEDURES,RDB$PROCEDURE_OUTPUTS,SYSTEM,RDB$PROCEDURE_PARAMETERS,3,3,1,1
SYSTEM,RDB$PROCEDURES,RDB$DESCRIPTION,SYSTEM,RDB$DESCRIPTION,4,4,1,1
SYSTEM,RDB$PROCEDURES,RDB$PROCEDURE_SOURCE,SYSTEM,RDB$SOURCE,5,5,1,1
SYSTEM,RDB$PROCEDURES,RDB$PROCEDURE_BLR,SYSTEM,RDB$PROCEDURE_BLR,6,6,1,1
SYSTEM,RDB$PROCEDURES,RDB$SECURITY_CLASS,SYSTEM,RDB$SECURITY_CLASS,7,7,1,1
SYSTEM,RDB$PROCEDURES,RDB$OWNER_NAME,SYSTEM,RDB$USER,8,8,1,1
SYSTEM,RDB$PROCEDURES,RDB$RUNTIME,SYSTEM,RDB$RUNTIME,9,9,1,1
SYSTEM,RDB$PROCEDURES,RDB$SYSTEM_FLAG,SYSTEM,RDB$SYSTEM_FLAG,10,10,1,0
SYSTEM,RDB$PROCEDURES,RDB$PROCEDURE_TYPE,SYSTEM,RDB$PROCEDURE_TYPE,11,11,1,1
SYSTEM,RDB$PROCEDURES,RDB$VALID_BLR,SYSTEM,RDB$SYSTEM_NULLFLAG,12,12,1,1
SYSTEM,RDB$PROCEDURES,RDB$DEBUG_INFO,SYSTEM,RDB$DEBUG_INFO,13,13,1,1
SYSTEM,RDB$PROCEDURES,RDB$ENGINE_NAME,SYSTEM,RDB$ENGINE_NAME,14,14,1,1
SYSTEM,RDB$PROCEDURES,RDB$ENTRYPOINT,SYSTEM,RDB$EXTERNAL_NAME,15,15,1,1
SYSTEM,RDB$PROCEDURES,RDB$PACKAGE_NAME,SYSTEM,RDB$PACKAGE_NAME,16,16,1,1
SYSTEM,RDB$PROCEDURES,RDB$PRIVATE_FLAG,SYSTEM,RDB$SYSTEM_NULLFLAG,17,17,1,1
SYSTEM,RDB$PROCEDURES,RDB$SQL_SECURITY,SYSTEM,RDB$SQL_SECURITY,18,18,1,1
SYSTEM,RDB$PROCEDURES,RDB$SCHEMA_NAME,SYSTEM,RDB$SCHEMA_NAME,19,19,1,1
SYSTEM,RDB$PROCEDURE_PARAMETERS,RDB$PARAMETER_NAME,SYSTEM,RDB$PARAMETER_NAME,0,0,1,1
SYSTEM,RDB$PROCEDURE_PARAMETERS,RDB$PROCEDURE_NAME,SYSTEM,RDB$PROCEDURE_NAME,1,1,1,1
SYSTEM,RDB$PROCEDURE_PARAMETERS,RDB$PARAMETER_NUMBER,SYSTEM,RDB$PARAMETER_NUMBER,2,2,1,1
SYSTEM,RDB$PROCEDURE_PARAMETERS,RDB$PARAMETER_TYPE,SYSTEM,RDB$PARAMETER_TYPE,3,3,1,1
SYSTEM,RDB$PROCEDURE_PARAMETERS,RDB$FIELD_SOURCE,SYSTEM,RDB$FIELD_NAME,4,4,1,1
SYSTEM,RDB$PROCEDURE_PARAMETERS,RDB$DESCRIPTION,SYSTEM,RDB$DESCRIPTION,5,5,1,1
SYSTEM,RDB$PROCEDURE_PARAMETERS,RDB$SYSTEM_FLAG,SYSTEM,RDB$SYSTEM_FLAG,6,6,1,0
SYSTEM,RDB$PROCEDURE_PARAMETERS,RDB$DEFAULT_VALUE,SYSTEM,RDB$VALUE,7,7,1,1
SYSTEM,RDB$PROCEDURE_PARAMETERS,RDB$DEFAULT_SOURCE,SYSTEM,RDB$SOURCE,8,8,1,1
SYSTEM,RDB$PROCEDURE_PARAMETERS,RDB$COLLATION_ID,SYSTEM,RDB$COLLATION_ID,9,9,1,1
SYSTEM,RDB$PROCEDURE_PARAMETERS,RDB$NULL_FLAG,SYSTEM,RDB$NULL_FLAG,10,10,1,1
SYSTEM,RDB$PROCEDURE_PARAMETERS,RDB$PARAMETER_MECHANISM,SYSTEM,RDB$MECHANISM,11,11,1,1
SYSTEM,RDB$PROCEDURE_PARAMETERS,RDB$FIELD_NAME,SYSTEM,RDB$FIELD_NAME,12,12,1,1
SYSTEM,RDB$PROCEDURE_PARAMETERS,RDB$RELATION_NAME,SYSTEM,RDB$RELATION_NAME,13,13,1,1
SYSTEM,RDB$PROCEDURE_PARAMETERS,RDB$PACKAGE_NAME,SYSTEM,RDB$PACKAGE_NAME,14,14,1,1
SYSTEM,RDB$PROCEDURE_PARAMETERS,RDB$SCHEMA_NAME,SYSTEM,RDB$SCHEMA_NAME,15,15,1,1
SYSTEM,RDB$PROCEDURE_PARAMETERS,RDB$RELATION_SCHEMA_NAME,SYSTEM,RDB$SCHEMA_NAME,16,16,1,1
SYSTEM,RDB$PROCEDURE_PARAMETERS,RDB$FIELD_SOURCE_SCHEMA_NAME,SYSTEM,RDB$SCHEMA_NAME,17,17,1,1
SYSTEM,RDB$CHARACTER_SETS,RDB$CHARACTER_SET_NAME,SYSTEM,RDB$CHARACTER_SET_NAME,0,0,1,1
SYSTEM,RDB$CHARACTER_SETS,RDB$FORM_OF_USE,SYSTEM,RDB$GENERIC_NAME,1,1,1,1
SYSTEM,RDB$CHARACTER_SETS,RDB$NUMBER_OF_CHARACTERS,SYSTEM,RDB$NUMBER_OF_CHARACTERS,2,2,1,1
SYSTEM,RDB$CHARACTER_SETS,RDB$DEFAULT_COLLATE_NAME,SYSTEM,RDB$COLLATION_NAME,3,3,1,1
SYSTEM,RDB$CHARACTER_SETS,RDB$CHARACTER_SET_ID,SYSTEM,RDB$CHARACTER_SET_ID,4,4,1,1
SYSTEM,RDB$CHARACTER_SETS,RDB$SYSTEM_FLAG,SYSTEM,RDB$SYSTEM_FLAG,5,5,1,0
SYSTEM,RDB$CHARACTER_SETS,RDB$DESCRIPTION,SYSTEM,RDB$DESCRIPTION,6,6,1,1
SYSTEM,RDB$CHARACTER_SETS,RDB$FUNCTION_NAME,SYSTEM,RDB$FUNCTION_NAME,7,7,1,1
SYSTEM,RDB$CHARACTER_SETS,RDB$BYTES_PER_CHARACTER,SYSTEM,RDB$FIELD_LENGTH,8,8,1,1
SYSTEM,RDB$CHARACTER_SETS,RDB$SECURITY_CLASS,SYSTEM,RDB$SECURITY_CLASS,9,9,1,1
SYSTEM,RDB$CHARACTER_SETS,RDB$OWNER_NAME,SYSTEM,RDB$USER,10,10,1,1
SYSTEM,RDB$CHARACTER_SETS,RDB$SCHEMA_NAME,SYSTEM,RDB$SCHEMA_NAME,11,11,1,1
SYSTEM,RDB$CHARACTER_SETS,RDB$DEFAULT_COLLATE_SCHEMA_NAME,SYSTEM,RDB$SCHEMA_NAME,12,12,1,1
SYSTEM,RDB$COLLATIONS,RDB$COLLATION_NAME,SYSTEM,RDB$COLLATION_NAME,0,0,1,1
SYSTEM,RDB$COLLATIONS,RDB$COLLATION_ID,SYSTEM,RDB$COLLATION_ID,1,1,1,1
SYSTEM,RDB$COLLATIONS,RDB$CHARACTER_SET_ID,SYSTEM,RDB$CHARACTER_SET_ID,2,2,1,1
SYSTEM,RDB$COLLATIONS,RDB$COLLATION_ATTRIBUTES,SYSTEM,RDB$GENERIC_TYPE,3,3,1,1
SYSTEM,RDB$COLLATIONS,RDB$SYSTEM_FLAG,SYSTEM,RDB$SYSTEM_FLAG,4,4,1,0
SYSTEM,RDB$COLLATIONS,RDB$DESCRIPTION,SYSTEM,RDB$DESCRIPTION,5,5,1,1
SYSTEM,RDB$COLLATIONS,RDB$FUNCTION_NAME,SYSTEM,RDB$FUNCTION_NAME,6,6,1,1
SYSTEM,RDB$COLLATIONS,RDB$BASE_COLLATION_NAME,SYSTEM,RDB$COLLATION_NAME,7,7,1,1
SYSTEM,RDB$COLLATIONS,RDB$SPECIFIC_ATTRIBUTES,SYSTEM,RDB$SPECIFIC_ATTRIBUTES,8,8,1,1
SYSTEM,RDB$COLLATIONS,RDB$SECURITY_CLASS,SYSTEM,RDB$SECURITY_CLASS,9,9,1,1
SYSTEM,RDB$COLLATIONS,RDB$OWNER_NAME,SYSTEM,RDB$USER,10,10,1,1
SYSTEM,RDB$COLLATIONS,RDB$SCHEMA_NAME,SYSTEM,RDB$SCHEMA_NAME,11,11,1,1
SYSTEM,RDB$EXCEPTIONS,RDB$EXCEPTION_NAME,SYSTEM,RDB$EXCEPTION_NAME,0,0,1,1
SYSTEM,RDB$EXCEPTIONS,RDB$EXCEPTION_NUMBER,SYSTEM,RDB$EXCEPTION_NUMBER,1,1,1,1
SYSTEM,RDB$EXCEPTIONS,RDB$MESSAGE,SYSTEM,RDB$MESSAGE,2,2,1,1
SYSTEM,RDB$EXCEPTIONS,RDB$DESCRIPTION,SYSTEM,RDB$DESCRIPTION,3,3,1,1
SYSTEM,RDB$EXCEPTIONS,RDB$SYSTEM_FLAG,SYSTEM,RDB$SYSTEM_FLAG,4,4,1,1
SYSTEM,RDB$EXCEPTIONS,RDB$SECURITY_CLASS,SYSTEM,RDB$SECURITY_CLASS,5,5,1,1
SYSTEM,RDB$EXCEPTIONS,RDB$OWNER_NAME,SYSTEM,RDB$USER,6,6,1,1
SYSTEM,RDB$EXCEPTIONS,RDB$SCHEMA_NAME,SYSTEM,RDB$SCHEMA_NAME,7,7,1,1
SYSTEM,RDB$ROLES,RDB$ROLE_NAME,SYSTEM,RDB$USER,0,0,1,1
SYSTEM,RDB$ROLES,RDB$OWNER_NAME,SYSTEM,RDB$USER,1,1,1,1
SYSTEM,RDB$ROLES,RDB$DESCRIPTION,SYSTEM,RDB$DESCRIPTION,2,2,1,1
SYSTEM,RDB$ROLES,RDB$SYSTEM_FLAG,SYSTEM,RDB$SYSTEM_FLAG,3,3,1,1
SYSTEM,RDB$ROLES,RDB$SECURITY_CLASS,SYSTEM,RDB$SECURITY_CLASS,4,4,1,1
SYSTEM,RDB$ROLES,RDB$SYSTEM_PRIVILEGES,SYSTEM,RDB$SYSTEM_PRIVILEGES,5,5,1,1
SYSTEM,RDB$BACKUP_HISTORY,RDB$BACKUP_ID,SYSTEM,RDB$BACKUP_ID,0,0,1,1
SYSTEM,RDB$BACKUP_HISTORY,RDB$TIMESTAMP,SYSTEM,RDB$TIMESTAMP_TZ,1,1,1,1
SYSTEM,RDB$BACKUP_HISTORY,RDB$BACKUP_LEVEL,SYSTEM,RDB$BACKUP_LEVEL,2,2,1,1
SYSTEM,RDB$BACKUP_HISTORY,RDB$GUID,SYSTEM,RDB$GUID,3,3,1,1
SYSTEM,RDB$BACKUP_HISTORY,RDB$SCN,SYSTEM,RDB$SCN,4,4,1,1
SYSTEM,RDB$BACKUP_HISTORY,RDB$FILE_NAME,SYSTEM,RDB$FILE_NAME,5,5,1,1
SYSTEM,MON$DATABASE,MON$DATABASE_NAME,SYSTEM,RDB$FILE_NAME2,0,0,1,0
SYSTEM,MON$DATABASE,MON$PAGE_SIZE,SYSTEM,RDB$PAGE_SIZE,1,1,1,0
SYSTEM,MON$DATABASE,MON$ODS_MAJOR,SYSTEM,RDB$ODS_NUMBER,2,2,1,0
SYSTEM,MON$DATABASE,MON$ODS_MINOR,SYSTEM,RDB$ODS_NUMBER,3,3,1,0
SYSTEM,MON$DATABASE,MON$OLDEST_TRANSACTION,SYSTEM,RDB$TRANSACTION_ID,4,4,1,0
SYSTEM,MON$DATABASE,MON$OLDEST_ACTIVE,SYSTEM,RDB$TRANSACTION_ID,5,5,1,0
SYSTEM,MON$DATABASE,MON$OLDEST_SNAPSHOT,SYSTEM,RDB$TRANSACTION_ID,6,6,1,0
SYSTEM,MON$DATABASE,MON$NEXT_TRANSACTION,SYSTEM,RDB$TRANSACTION_ID,7,7,1,0
SYSTEM,MON$DATABASE,MON$PAGE_BUFFERS,SYSTEM,RDB$PAGE_BUFFERS,8,8,1,0
SYSTEM,MON$DATABASE,MON$SQL_DIALECT,SYSTEM,RDB$SQL_DIALECT,9,9,1,0
SYSTEM,MON$DATABASE,MON$SHUTDOWN_MODE,SYSTEM,RDB$SHUTDOWN_MODE,10,10,1,0
SYSTEM,MON$DATABASE,MON$SWEEP_INTERVAL,SYSTEM,RDB$SWEEP_INTERVAL,11,11,1,0
SYSTEM,MON$DATABASE,MON$READ_ONLY,SYSTEM,RDB$SYSTEM_NULLFLAG,12,12,1,0
SYSTEM,MON$DATABASE,MON$FORCED_WRITES,SYSTEM,RDB$SYSTEM_NULLFLAG,13,13,1,0
SYSTEM,MON$DATABASE,MON$RESERVE_SPACE,SYSTEM,RDB$SYSTEM_NULLFLAG,14,14,1,0
SYSTEM,MON$DATABASE,MON$CREATION_DATE,SYSTEM,RDB$TIMESTAMP_TZ,15,15,1,0
SYSTEM,MON$DATABASE,MON$PAGES,SYSTEM,RDB$COUNTER,16,16,1,0
SYSTEM,MON$DATABASE,MON$STAT_ID,SYSTEM,RDB$STAT_ID,17,17,1,0
SYSTEM,MON$DATABASE,MON$BACKUP_STATE,SYSTEM,RDB$BACKUP_STATE,18,18,1,0
SYSTEM,MON$DATABASE,MON$CRYPT_PAGE,SYSTEM,RDB$COUNTER,19,19,1,0
SYSTEM,MON$DATABASE,MON$OWNER,SYSTEM,RDB$USER,20,20,1,0
SYSTEM,MON$DATABASE,MON$SEC_DATABASE,SYSTEM,MON$SEC_DATABASE,21,21,1,0
SYSTEM,MON$DATABASE,MON$CRYPT_STATE,SYSTEM,RDB$CRYPT_STATE,22,22,1,0
SYSTEM,MON$DATABASE,MON$GUID,SYSTEM,RDB$GUID,23,23,1,0
SYSTEM,MON$DATABASE,MON$FILE_ID,SYSTEM,RDB$FILE_ID,24,24,1,0
SYSTEM,MON$DATABASE,MON$NEXT_ATTACHMENT,SYSTEM,RDB$ATTACHMENT_ID,25,25,1,0
SYSTEM,MON$DATABASE,MON$NEXT_STATEMENT,SYSTEM,RDB$STATEMENT_ID,26,26,1,0
SYSTEM,MON$DATABASE,MON$REPLICA_MODE,SYSTEM,RDB$REPLICA_MODE,27,27,1,0
SYSTEM,MON$ATTACHMENTS,MON$ATTACHMENT_ID,SYSTEM,RDB$ATTACHMENT_ID,0,0,1,0
SYSTEM,MON$ATTACHMENTS,MON$SERVER_PID,SYSTEM,RDB$PID,1,1,1,0
SYSTEM,MON$ATTACHMENTS,MON$STATE,SYSTEM,RDB$STATE,2,2,1,0
SYSTEM,MON$ATTACHMENTS,MON$ATTACHMENT_NAME,SYSTEM,RDB$FILE_NAME2,3,3,1,0
SYSTEM,MON$ATTACHMENTS,MON$USER,SYSTEM,RDB$USER,4,4,1,0
SYSTEM,MON$ATTACHMENTS,MON$ROLE,SYSTEM,RDB$USER,5,5,1,0
SYSTEM,MON$ATTACHMENTS,MON$REMOTE_PROTOCOL,SYSTEM,RDB$REMOTE_PROTOCOL,6,6,1,0
SYSTEM,MON$ATTACHMENTS,MON$REMOTE_ADDRESS,SYSTEM,RDB$REMOTE_ADDRESS,7,7,1,0
SYSTEM,MON$ATTACHMENTS,MON$REMOTE_PID,SYSTEM,RDB$PID,8,8,1,0
SYSTEM,MON$ATTACHMENTS,MON$CHARACTER_SET_ID,SYSTEM,RDB$CHARACTER_SET_ID,9,9,1,0
SYSTEM,MON$ATTACHMENTS,MON$TIMESTAMP,SYSTEM,RDB$TIMESTAMP_TZ,10,10,1,0
SYSTEM,MON$ATTACHMENTS,MON$GARBAGE_COLLECTION,SYSTEM,RDB$SYSTEM_NULLFLAG,11,11,1,0
SYSTEM,MON$ATTACHMENTS,MON$REMOTE_PROCESS,SYSTEM,RDB$FILE_NAME2,12,12,1,0
SYSTEM,MON$ATTACHMENTS,MON$STAT_ID,SYSTEM,RDB$STAT_ID,13,13,1,0
SYSTEM,MON$ATTACHMENTS,MON$CLIENT_VERSION,SYSTEM,RDB$CLIENT_VERSION,14,14,1,0
SYSTEM,MON$ATTACHMENTS,MON$REMOTE_VERSION,SYSTEM,RDB$REMOTE_VERSION,15,15,1,0
SYSTEM,MON$ATTACHMENTS,MON$REMOTE_HOST,SYSTEM,RDB$HOST_NAME,16,16,1,0
SYSTEM,MON$ATTACHMENTS,MON$REMOTE_OS_USER,SYSTEM,RDB$OS_USER,17,17,1,0
SYSTEM,MON$ATTACHMENTS,MON$AUTH_METHOD,SYSTEM,RDB$AUTH_METHOD,18,18,1,0
SYSTEM,MON$ATTACHMENTS,MON$SYSTEM_FLAG,SYSTEM,RDB$SYSTEM_FLAG,19,19,1,0
SYSTEM,MON$ATTACHMENTS,MON$IDLE_TIMEOUT,SYSTEM,MON$IDLE_TIMEOUT,20,20,1,0
SYSTEM,MON$ATTACHMENTS,MON$IDLE_TIMER,SYSTEM,MON$IDLE_TIMER,21,21,1,0
SYSTEM,MON$ATTACHMENTS,MON$STATEMENT_TIMEOUT,SYSTEM,MON$STATEMENT_TIMEOUT,22,22,1,0
SYSTEM,MON$ATTACHMENTS,MON$WIRE_COMPRESSED,SYSTEM,RDB$BOOLEAN,23,23,1,0
SYSTEM,MON$ATTACHMENTS,MON$WIRE_ENCRYPTED,SYSTEM,RDB$BOOLEAN,24,24,1,0
SYSTEM,MON$ATTACHMENTS,MON$WIRE_CRYPT_PLUGIN,SYSTEM,MON$WIRE_CRYPT_PLUGIN,25,25,1,0
SYSTEM,MON$ATTACHMENTS,MON$SESSION_TIMEZONE,SYSTEM,RDB$TIME_ZONE_NAME,26,26,1,0
SYSTEM,MON$ATTACHMENTS,MON$PARALLEL_WORKERS,SYSTEM,MON$PARALLEL_WORKERS,27,27,1,0
SYSTEM,MON$ATTACHMENTS,MON$SEARCH_PATH,SYSTEM,RDB$TEXT_MAX,28,28,1,0
SYSTEM,MON$TRANSACTIONS,MON$TRANSACTION_ID,SYSTEM,RDB$TRANSACTION_ID,0,0,1,0
SYSTEM,MON$TRANSACTIONS,MON$ATTACHMENT_ID,SYSTEM,RDB$ATTACHMENT_ID,1,1,1,0
SYSTEM,MON$TRANSACTIONS,MON$STATE,SYSTEM,RDB$STATE,2,2,1,0
SYSTEM,MON$TRANSACTIONS,MON$TIMESTAMP,SYSTEM,RDB$TIMESTAMP_TZ,3,3,1,0
SYSTEM,MON$TRANSACTIONS,MON$TOP_TRANSACTION,SYSTEM,RDB$TRANSACTION_ID,4,4,1,0
SYSTEM,MON$TRANSACTIONS,MON$OLDEST_TRANSACTION,SYSTEM,RDB$TRANSACTION_ID,5,5,1,0
SYSTEM,MON$TRANSACTIONS,MON$OLDEST_ACTIVE,SYSTEM,RDB$TRANSACTION_ID,6,6,1,0
SYSTEM,MON$TRANSACTIONS,MON$ISOLATION_MODE,SYSTEM,RDB$ISOLATION_MODE,7,7,1,0
SYSTEM,MON$TRANSACTIONS,MON$LOCK_TIMEOUT,SYSTEM,RDB$LOCK_TIMEOUT,8,8,1,0
SYSTEM,MON$TRANSACTIONS,MON$READ_ONLY,SYSTEM,RDB$SYSTEM_NULLFLAG,9,9,1,0
SYSTEM,MON$TRANSACTIONS,MON$AUTO_COMMIT,SYSTEM,RDB$SYSTEM_NULLFLAG,10,10,1,0
SYSTEM,MON$TRANSACTIONS,MON$AUTO_UNDO,SYSTEM,RDB$SYSTEM_NULLFLAG,11,11,1,0
SYSTEM,MON$TRANSACTIONS,MON$STAT_ID,SYSTEM,RDB$STAT_ID,12,12,1,0
SYSTEM,MON$TRANSACTIONS,MON$AUTO_RELEASE_TEMP_BLOBID,SYSTEM,RDB$SYSTEM_NULLFLAG,13,13,1,0
SYSTEM,MON$STATEMENTS,MON$STATEMENT_ID,SYSTEM,RDB$STATEMENT_ID,0,0,1,0
SYSTEM,MON$STATEMENTS,MON$ATTACHMENT_ID,SYSTEM,RDB$ATTACHMENT_ID,1,1,1,0
SYSTEM,MON$STATEMENTS,MON$TRANSACTION_ID,SYSTEM,RDB$TRANSACTION_ID,2,2,1,0
SYSTEM,MON$STATEMENTS,MON$STATE,SYSTEM,RDB$STATE,3,3,1,0
SYSTEM,MON$STATEMENTS,MON$TIMESTAMP,SYSTEM,RDB$TIMESTAMP_TZ,4,4,1,0
SYSTEM,MON$STATEMENTS,MON$SQL_TEXT,SYSTEM,RDB$SOURCE,5,5,1,0
SYSTEM,MON$STATEMENTS,MON$STAT_ID,SYSTEM,RDB$STAT_ID,6,6,1,0
SYSTEM,MON$STATEMENTS,MON$EXPLAINED_PLAN,SYSTEM,RDB$SOURCE,7,7,1,0
SYSTEM,MON$STATEMENTS,MON$STATEMENT_TIMEOUT,SYSTEM,MON$STATEMENT_TIMEOUT,8,8,1,0
SYSTEM,MON$STATEMENTS,MON$STATEMENT_TIMER,SYSTEM,MON$STATEMENT_TIMER,9,9,1,0
SYSTEM,MON$STATEMENTS,MON$COMPILED_STATEMENT_ID,SYSTEM,RDB$STATEMENT_ID,10,10,1,0
SYSTEM,MON$CALL_STACK,MON$CALL_ID,SYSTEM,RDB$CALL_ID,0,0,1,0
SYSTEM,MON$CALL_STACK,MON$STATEMENT_ID,SYSTEM,RDB$STATEMENT_ID,1,1,1,0
SYSTEM,MON$CALL_STACK,MON$CALLER_ID,SYSTEM,RDB$CALL_ID,2,2,1,0
SYSTEM,MON$CALL_STACK,MON$OBJECT_NAME,SYSTEM,RDB$GENERIC_NAME,3,3,1,0
SYSTEM,MON$CALL_STACK,MON$OBJECT_TYPE,SYSTEM,RDB$OBJECT_TYPE,4,4,1,0
SYSTEM,MON$CALL_STACK,MON$TIMESTAMP,SYSTEM,RDB$TIMESTAMP_TZ,5,5,1,0
SYSTEM,MON$CALL_STACK,MON$SOURCE_LINE,SYSTEM,RDB$SOURCE_INFO,6,6,1,0
SYSTEM,MON$CALL_STACK,MON$SOURCE_COLUMN,SYSTEM,RDB$SOURCE_INFO,7,7,1,0
SYSTEM,MON$CALL_STACK,MON$STAT_ID,SYSTEM,RDB$STAT_ID,8,8,1,0
SYSTEM,MON$CALL_STACK,MON$PACKAGE_NAME,SYSTEM,RDB$PACKAGE_NAME,9,9,1,0
SYSTEM,MON$CALL_STACK,MON$COMPILED_STATEMENT_ID,SYSTEM,RDB$STATEMENT_ID,10,10,1,0
SYSTEM,MON$CALL_STACK,MON$SCHEMA_NAME,SYSTEM,RDB$SCHEMA_NAME,11,11,1,0
SYSTEM,MON$IO_STATS,MON$STAT_ID,SYSTEM,RDB$STAT_ID,0,0,1,0
SYSTEM,MON$IO_STATS,MON$STAT_GROUP,SYSTEM,RDB$STAT_GROUP,1,1,1,0
SYSTEM,MON$IO_STATS,MON$PAGE_READS,SYSTEM,RDB$COUNTER,2,2,1,0
SYSTEM,MON$IO_STATS,MON$PAGE_WRITES,SYSTEM,RDB$COUNTER,3,3,1,0
SYSTEM,MON$IO_STATS,MON$PAGE_FETCHES,SYSTEM,RDB$COUNTER,4,4,1,0
SYSTEM,MON$IO_STATS,MON$PAGE_MARKS,SYSTEM,RDB$COUNTER,5,5,1,0
SYSTEM,MON$RECORD_STATS,MON$STAT_ID,SYSTEM,RDB$STAT_ID,0,0,1,0
SYSTEM,MON$RECORD_STATS,MON$STAT_GROUP,SYSTEM,RDB$STAT_GROUP,1,1,1,0
SYSTEM,MON$RECORD_STATS,MON$RECORD_SEQ_READS,SYSTEM,RDB$COUNTER,2,2,1,0
SYSTEM,MON$RECORD_STATS,MON$RECORD_IDX_READS,SYSTEM,RDB$COUNTER,3,3,1,0
SYSTEM,MON$RECORD_STATS,MON$RECORD_INSERTS,SYSTEM,RDB$COUNTER,4,4,1,0
SYSTEM,MON$RECORD_STATS,MON$RECORD_UPDATES,SYSTEM,RDB$COUNTER,5,5,1,0
SYSTEM,MON$RECORD_STATS,MON$RECORD_DELETES,SYSTEM,RDB$COUNTER,6,6,1,0
SYSTEM,MON$RECORD_STATS,MON$RECORD_BACKOUTS,SYSTEM,RDB$COUNTER,7,7,1,0
SYSTEM,MON$RECORD_STATS,MON$RECORD_PURGES,SYSTEM,RDB$COUNTER,8,8,1,0
SYSTEM,MON$RECORD_STATS,MON$RECORD_EXPUNGES,SYSTEM,RDB$COUNTER,9,9,1,0
SYSTEM,MON$RECORD_STATS,MON$RECORD_LOCKS,SYSTEM,RDB$COUNTER,10,10,1,0
SYSTEM,MON$RECORD_STATS,MON$RECORD_WAITS,SYSTEM,RDB$COUNTER,11,11,1,0
SYSTEM,MON$RECORD_STATS,MON$RECORD_CONFLICTS,SYSTEM,RDB$COUNTER,12,12,1,0
SYSTEM,MON$RECORD_STATS,MON$BACKVERSION_READS,SYSTEM,RDB$COUNTER,13,13,1,0
SYSTEM,MON$RECORD_STATS,MON$FRAGMENT_READS,SYSTEM,RDB$COUNTER,14,14,1,0
SYSTEM,MON$RECORD_STATS,MON$RECORD_RPT_READS,SYSTEM,RDB$COUNTER,15,15,1,0
SYSTEM,MON$RECORD_STATS,MON$RECORD_IMGC,SYSTEM,RDB$COUNTER,16,16,1,0
SYSTEM,MON$CONTEXT_VARIABLES,MON$ATTACHMENT_ID,SYSTEM,RDB$ATTACHMENT_ID,0,0,1,0
SYSTEM,MON$CONTEXT_VARIABLES,MON$TRANSACTION_ID,SYSTEM,RDB$TRANSACTION_ID,1,1,1,0
SYSTEM,MON$CONTEXT_VARIABLES,MON$VARIABLE_NAME,SYSTEM,RDB$CONTEXT_VAR_NAME,2,2,1,0
SYSTEM,MON$CONTEXT_VARIABLES,MON$VARIABLE_VALUE,SYSTEM,RDB$CONTEXT_VAR_VALUE,3,3,1,0
SYSTEM,MON$MEMORY_USAGE,MON$STAT_ID,SYSTEM,RDB$STAT_ID,0,0,1,0
SYSTEM,MON$MEMORY_USAGE,MON$STAT_GROUP,SYSTEM,RDB$STAT_GROUP,1,1,1,0
SYSTEM,MON$MEMORY_USAGE,MON$MEMORY_USED,SYSTEM,RDB$COUNTER,2,2,1,0
SYSTEM,MON$MEMORY_USAGE,MON$MEMORY_ALLOCATED,SYSTEM,RDB$COUNTER,3,3,1,0
SYSTEM,MON$MEMORY_USAGE,MON$MAX_MEMORY_USED,SYSTEM,RDB$COUNTER,4,4,1,0
SYSTEM,MON$MEMORY_USAGE,MON$MAX_MEMORY_ALLOCATED,SYSTEM,RDB$COUNTER,5,5,1,0
SYSTEM,RDB$PACKAGES,RDB$PACKAGE_NAME,SYSTEM,RDB$PACKAGE_NAME,0,0,1,1
SYSTEM,RDB$PACKAGES,RDB$PACKAGE_HEADER_SOURCE,SYSTEM,RDB$SOURCE,1,1,1,1
SYSTEM,RDB$PACKAGES,RDB$PACKAGE_BODY_SOURCE,SYSTEM,RDB$SOURCE,2,2,1,1
SYSTEM,RDB$PACKAGES,RDB$VALID_BODY_FLAG,SYSTEM,RDB$SYSTEM_NULLFLAG,3,3,1,1
SYSTEM,RDB$PACKAGES,RDB$SECURITY_CLASS,SYSTEM,RDB$SECURITY_CLASS,4,4,1,1
SYSTEM,RDB$PACKAGES,RDB$OWNER_NAME,SYSTEM,RDB$USER,5,5,1,1
SYSTEM,RDB$PACKAGES,RDB$SYSTEM_FLAG,SYSTEM,RDB$SYSTEM_FLAG,6,6,1,1
SYSTEM,RDB$PACKAGES,RDB$DESCRIPTION,SYSTEM,RDB$DESCRIPTION,7,7,1,1
SYSTEM,RDB$PACKAGES,RDB$SQL_SECURITY,SYSTEM,RDB$SQL_SECURITY,8,8,1,1
SYSTEM,RDB$PACKAGES,RDB$SCHEMA_NAME,SYSTEM,RDB$SCHEMA_NAME,9,9,1,1
SYSTEM,SEC$USERS,SEC$USER_NAME,SYSTEM,RDB$USER,0,0,1,0
SYSTEM,SEC$USERS,SEC$FIRST_NAME,SYSTEM,SEC$NAME_PART,1,1,1,0
SYSTEM,SEC$USERS,SEC$MIDDLE_NAME,SYSTEM,SEC$NAME_PART,2,2,1,0
SYSTEM,SEC$USERS,SEC$LAST_NAME,SYSTEM,SEC$NAME_PART,3,3,1,0
SYSTEM,SEC$USERS,SEC$ACTIVE,SYSTEM,RDB$BOOLEAN,4,4,1,0
SYSTEM,SEC$USERS,SEC$ADMIN,SYSTEM,RDB$BOOLEAN,5,5,1,0
SYSTEM,SEC$USERS,SEC$DESCRIPTION,SYSTEM,RDB$DESCRIPTION,6,6,1,0
SYSTEM,SEC$USERS,SEC$PLUGIN,SYSTEM,RDB$PLUGIN,7,7,1,0
SYSTEM,SEC$USER_ATTRIBUTES,SEC$USER_NAME,SYSTEM,RDB$USER,0,0,1,0
SYSTEM,SEC$USER_ATTRIBUTES,SEC$KEY,SYSTEM,SEC$KEY,1,1,1,0
SYSTEM,SEC$USER_ATTRIBUTES,SEC$VALUE,SYSTEM,SEC$VALUE,2,2,1,0
SYSTEM,SEC$USER_ATTRIBUTES,SEC$PLUGIN,SYSTEM,RDB$PLUGIN,3,3,1,0
SYSTEM,RDB$AUTH_MAPPING,RDB$MAP_NAME,SYSTEM,RDB$MAP_NAME,0,0,1,1
SYSTEM,RDB$AUTH_MAPPING,RDB$MAP_USING,SYSTEM,RDB$MAP_USING,1,1,1,1
SYSTEM,RDB$AUTH_MAPPING,RDB$MAP_PLUGIN,SYSTEM,RDB$PLUGIN,2,2,1,1
SYSTEM,RDB$AUTH_MAPPING,RDB$MAP_DB,SYSTEM,RDB$MAP_DB,3,3,1,1
SYSTEM,RDB$AUTH_MAPPING,RDB$MAP_FROM_TYPE,SYSTEM,RDB$MAP_FROM_TYPE,4,4,1,1
SYSTEM,RDB$AUTH_MAPPING,RDB$MAP_FROM,SYSTEM,RDB$MAP_FROM,5,5,1,1
SYSTEM,RDB$AUTH_MAPPING,RDB$MAP_TO_TYPE,SYSTEM,RDB$OBJECT_TYPE,6,6,1,1
SYSTEM,RDB$AUTH_MAPPING,RDB$MAP_TO,SYSTEM,RDB$MAP_TO,7,7,1,1
SYSTEM,RDB$AUTH_MAPPING,RDB$SYSTEM_FLAG,SYSTEM,RDB$SYSTEM_FLAG,8,8,1,1
SYSTEM,RDB$AUTH_MAPPING,RDB$DESCRIPTION,SYSTEM,RDB$DESCRIPTION,9,9,1,1
SYSTEM,SEC$GLOBAL_AUTH_MAPPING,SEC$MAP_NAME,SYSTEM,RDB$MAP_NAME,0,0,1,0
SYSTEM,SEC$GLOBAL_AUTH_MAPPING,SEC$MAP_USING,SYSTEM,RDB$MAP_USING,1,1,1,0
SYSTEM,SEC$GLOBAL_AUTH_MAPPING,SEC$MAP_PLUGIN,SYSTEM,RDB$PLUGIN,2,2,1,0
SYSTEM,SEC$GLOBAL_AUTH_MAPPING,SEC$MAP_DB,SYSTEM,RDB$MAP_DB,3,3,1,0
SYSTEM,SEC$GLOBAL_AUTH_MAPPING,SEC$MAP_FROM_TYPE,SYSTEM,RDB$MAP_FROM_TYPE,4,4,1,0
SYSTEM,SEC$GLOBAL_AUTH_MAPPING,SEC$MAP_FROM,SYSTEM,RDB$MAP_FROM,5,5,1,0
SYSTEM,SEC$GLOBAL_AUTH_MAPPING,SEC$MAP_TO_TYPE,SYSTEM,RDB$OBJECT_TYPE,6,6,1,0
SYSTEM,SEC$GLOBAL_AUTH_MAPPING,SEC$MAP_TO,SYSTEM,RDB$MAP_TO,7,7,1,0
SYSTEM,SEC$GLOBAL_AUTH_MAPPING,SEC$DESCRIPTION,SYSTEM,RDB$DESCRIPTION,8,8,1,0
SYSTEM,RDB$DB_CREATORS,RDB$USER,SYSTEM,RDB$USER,0,0,1,1
SYSTEM,RDB$DB_CREATORS,RDB$USER_TYPE,SYSTEM,RDB$OBJECT_TYPE,1,1,1,1
SYSTEM,SEC$DB_CREATORS,SEC$USER,SYSTEM,RDB$USER,0,0,1,0
SYSTEM,SEC$DB_CREATORS,SEC$USER_TYPE,SYSTEM,RDB$OBJECT_TYPE,1,1,1,0
SYSTEM,MON$TABLE_STATS,MON$STAT_ID,SYSTEM,RDB$STAT_ID,0,0,1,0
SYSTEM,MON$TABLE_STATS,MON$STAT_GROUP,SYSTEM,RDB$STAT_GROUP,1,1,1,0
SYSTEM,MON$TABLE_STATS,MON$TABLE_NAME,SYSTEM,RDB$RELATION_NAME,2,2,1,0
SYSTEM,MON$TABLE_STATS,MON$RECORD_STAT_ID,SYSTEM,RDB$STAT_ID,3,3,1,0
SYSTEM,MON$TABLE_STATS,MON$SCHEMA_NAME,SYSTEM,RDB$SCHEMA_NAME,4,4,1,0
SYSTEM,MON$TABLE_STATS,MON$TABLE_TYPE,SYSTEM,MON$TABLE_TYPE,5,5,1,0
SYSTEM,RDB$TIME_ZONES,RDB$TIME_ZONE_ID,SYSTEM,RDB$TIME_ZONE_ID,0,0,1,0
SYSTEM,RDB$TIME_ZONES,RDB$TIME_ZONE_NAME,SYSTEM,RDB$TIME_ZONE_NAME,1,1,1,0
SYSTEM,RDB$PUBLICATIONS,RDB$PUBLICATION_NAME,SYSTEM,RDB$PUBLICATION_NAME,0,0,1,1
SYSTEM,RDB$PUBLICATIONS,RDB$OWNER_NAME,SYSTEM,RDB$USER,1,1,1,1
SYSTEM,RDB$PUBLICATIONS,RDB$SYSTEM_FLAG,SYSTEM,RDB$SYSTEM_FLAG,2,2,1,1
SYSTEM,RDB$PUBLICATIONS,RDB$ACTIVE_FLAG,SYSTEM,RDB$SYSTEM_FLAG,3,3,1,1
SYSTEM,RDB$PUBLICATIONS,RDB$AUTO_ENABLE,SYSTEM,RDB$SYSTEM_FLAG,4,4,1,1
SYSTEM,RDB$PUBLICATION_TABLES,RDB$PUBLICATION_NAME,SYSTEM,RDB$PUBLICATION_NAME,0,0,1,1
SYSTEM,RDB$PUBLICATION_TABLES,RDB$TABLE_NAME,SYSTEM,RDB$RELATION_NAME,1,1,1,1
SYSTEM,RDB$PUBLICATION_TABLES,RDB$TABLE_SCHEMA_NAME,SYSTEM,RDB$SCHEMA_NAME,2,2,1,1
SYSTEM,RDB$CONFIG,RDB$CONFIG_ID,SYSTEM,RDB$CONFIG_ID,0,0,1,0
SYSTEM,RDB$CONFIG,RDB$CONFIG_NAME,SYSTEM,RDB$CONFIG_NAME,1,1,1,0
SYSTEM,RDB$CONFIG,RDB$CONFIG_VALUE,SYSTEM,RDB$CONFIG_VALUE,2,2,1,0
SYSTEM,RDB$CONFIG,RDB$CONFIG_DEFAULT,SYSTEM,RDB$CONFIG_VALUE,3,3,1,0
SYSTEM,RDB$CONFIG,RDB$CONFIG_IS_SET,SYSTEM,RDB$CONFIG_IS_SET,4,4,1,0
SYSTEM,RDB$CONFIG,RDB$CONFIG_SOURCE,SYSTEM,RDB$FILE_NAME2,5,5,1,0
SYSTEM,RDB$KEYWORDS,RDB$KEYWORD_NAME,SYSTEM,RDB$KEYWORD_NAME,0,0,1,0
SYSTEM,RDB$KEYWORDS,RDB$KEYWORD_RESERVED,SYSTEM,RDB$KEYWORD_RESERVED,1,1,1,0
SYSTEM,MON$COMPILED_STATEMENTS,MON$COMPILED_STATEMENT_ID,SYSTEM,RDB$STATEMENT_ID,0,0,1,0
SYSTEM,MON$COMPILED_STATEMENTS,MON$SQL_TEXT,SYSTEM,RDB$SOURCE,1,1,1,0
SYSTEM,MON$COMPILED_STATEMENTS,MON$EXPLAINED_PLAN,SYSTEM,RDB$SOURCE,2,2,1,0
SYSTEM,MON$COMPILED_STATEMENTS,MON$OBJECT_NAME,SYSTEM,RDB$GENERIC_NAME,3,3,1,0
SYSTEM,MON$COMPILED_STATEMENTS,MON$OBJECT_TYPE,SYSTEM,RDB$OBJECT_TYPE,4,4,1,0
SYSTEM,MON$COMPILED_STATEMENTS,MON$PACKAGE_NAME,SYSTEM,RDB$PACKAGE_NAME,5,5,1,0
SYSTEM,MON$COMPILED_STATEMENTS,MON$STAT_ID,SYSTEM,RDB$STAT_ID,6,6,1,0
SYSTEM,MON$COMPILED_STATEMENTS,MON$SCHEMA_NAME,SYSTEM,RDB$SCHEMA_NAME,7,7,1,0
SYSTEM,RDB$SCHEMAS,RDB$SCHEMA_NAME,SYSTEM,RDB$SCHEMA_NAME,0,0,1,1
SYSTEM,RDB$SCHEMAS,RDB$OWNER_NAME,SYSTEM,RDB$USER,1,1,1,1
SYSTEM,RDB$SCHEMAS,RDB$CHARACTER_SET_NAME,SYSTEM,RDB$CHARACTER_SET_NAME,2,2,1,1
SYSTEM,RDB$SCHEMAS,RDB$CHARACTER_SET_SCHEMA_NAME,SYSTEM,RDB$SCHEMA_NAME,3,3,1,1
SYSTEM,RDB$SCHEMAS,RDB$SQL_SECURITY,SYSTEM,RDB$SQL_SECURITY,4,4,1,1
SYSTEM,RDB$SCHEMAS,RDB$SECURITY_CLASS,SYSTEM,RDB$SECURITY_CLASS,5,5,1,1
SYSTEM,RDB$SCHEMAS,RDB$SYSTEM_FLAG,SYSTEM,RDB$SYSTEM_FLAG,6,6,1,1
SYSTEM,RDB$SCHEMAS,RDB$DESCRIPTION,SYSTEM,RDB$DESCRIPTION,7,7,1,1
SYSTEM,MON$LOCAL_TEMPORARY_TABLES,MON$ATTACHMENT_ID,SYSTEM,RDB$ATTACHMENT_ID,0,0,1,0
SYSTEM,MON$LOCAL_TEMPORARY_TABLES,MON$TABLE_ID,SYSTEM,RDB$INTEGER,1,1,1,0
SYSTEM,MON$LOCAL_TEMPORARY_TABLES,MON$TABLE_NAME,SYSTEM,RDB$RELATION_NAME,2,2,1,0
SYSTEM,MON$LOCAL_TEMPORARY_TABLES,MON$SCHEMA_NAME,SYSTEM,RDB$SCHEMA_NAME,3,3,1,0
SYSTEM,MON$LOCAL_TEMPORARY_TABLES,MON$TABLE_TYPE,SYSTEM,MON$TABLE_TYPE,4,4,1,0
SYSTEM,MON$LOCAL_TEMPORARY_TABLE_COLUMNS,MON$ATTACHMENT_ID,SYSTEM,RDB$ATTACHMENT_ID,0,0,1,0
SYSTEM,MON$LOCAL_TEMPORARY_TABLE_COLUMNS,MON$TABLE_NAME,SYSTEM,RDB$RELATION_NAME,1,1,1,0
SYSTEM,MON$LOCAL_TEMPORARY_TABLE_COLUMNS,MON$SCHEMA_NAME,SYSTEM,RDB$SCHEMA_NAME,2,2,1,0
SYSTEM,MON$LOCAL_TEMPORARY_TABLE_COLUMNS,MON$FIELD_NAME,SYSTEM,RDB$FIELD_NAME,3,3,1,0
SYSTEM,MON$LOCAL_TEMPORARY_TABLE_COLUMNS,MON$FIELD_POSITION,SYSTEM,RDB$FIELD_POSITION,4,4,1,0
SYSTEM,MON$LOCAL_TEMPORARY_TABLE_COLUMNS,MON$FIELD_TYPE,SYSTEM,RDB$FIELD_TYPE,5,5,1,0
SYSTEM,MON$LOCAL_TEMPORARY_TABLE_COLUMNS,MON$FIELD_PRECISION,SYSTEM,RDB$FIELD_PRECISION,6,6,1,0
SYSTEM,MON$LOCAL_TEMPORARY_TABLE_COLUMNS,MON$FIELD_SCALE,SYSTEM,RDB$FIELD_SCALE,7,7,1,0
SYSTEM,MON$LOCAL_TEMPORARY_TABLE_COLUMNS,MON$CHAR_LENGTH,SYSTEM,RDB$FIELD_LENGTH,8,8,1,0
SYSTEM,MON$LOCAL_TEMPORARY_TABLE_COLUMNS,MON$FIELD_LENGTH,SYSTEM,RDB$FIELD_LENGTH,9,9,1,0
SYSTEM,MON$LOCAL_TEMPORARY_TABLE_COLUMNS,MON$FIELD_SUB_TYPE,SYSTEM,RDB$FIELD_SUB_TYPE,10,10,1,0
SYSTEM,MON$LOCAL_TEMPORARY_TABLE_COLUMNS,MON$NOT_NULL,SYSTEM,RDB$NULL_FLAG,11,11,1,0
SYSTEM,MON$LOCAL_TEMPORARY_TABLE_COLUMNS,MON$CHARACTER_SET_ID,SYSTEM,RDB$CHARACTER_SET_ID,12,12,1,0
SYSTEM,MON$LOCAL_TEMPORARY_TABLE_COLUMNS,MON$COLLATION_ID,SYSTEM,RDB$COLLATION_ID,13,13,1,0
```

#### RDB$FIELDS (Default Rows)
Rows must be inserted in the exact order listed. `RDB$SYSTEM_FLAG = RDB_system (1)` and `RDB$OWNER_NAME = <DB_OWNER>` for all rows. `RDB$SECURITY_CLASS` uses the generator order in `appendix_security_classes.md`.
`RDB$DEFAULT_VALUE` uses the binary blob encoding defined in the "Binary Blob Encoding" section (hex byte lists).

```csv
RDB$SCHEMA_NAME,RDB$FIELD_NAME,RDB$FIELD_LENGTH,RDB$FIELD_SCALE,RDB$SYSTEM_FLAG,RDB$OWNER_NAME,RDB$SECURITY_CLASS,RDB$DEFAULT_VALUE,RDB$FIELD_TYPE,RDB$FIELD_SUB_TYPE,RDB$CHARACTER_SET_ID,RDB$COLLATION_ID,RDB$SEGMENT_LENGTH,RDB$CHARACTER_LENGTH,RDB$NULL_FLAG
SYSTEM,RDB$VIEW_CONTEXT,2,0,1,<DB_OWNER>,SQL$60,NULL,7,NULL,NULL,NULL,NULL,NULL,0
SYSTEM,RDB$CONTEXT_NAME,1020,0,1,<DB_OWNER>,SQL$61,NULL,14,3,4,0,NULL,255,0
SYSTEM,RDB$DESCRIPTION,8,0,1,<DB_OWNER>,SQL$62,NULL,261,1,4,NULL,80,NULL,0
SYSTEM,RDB$EDIT_STRING,127,0,1,<DB_OWNER>,SQL$63,NULL,37,NULL,0,0,NULL,NULL,0
SYSTEM,RDB$FIELD_ID,2,0,1,<DB_OWNER>,SQL$64,NULL,7,NULL,NULL,NULL,NULL,NULL,0
SYSTEM,RDB$FIELD_NAME,252,0,1,<DB_OWNER>,SQL$65,NULL,14,3,4,0,NULL,63,0
SYSTEM,RDB$SYSTEM_FLAG,2,0,1,<DB_OWNER>,SQL$66,NULL,7,NULL,NULL,NULL,NULL,NULL,1
SYSTEM,RDB$SYSTEM_NULLFLAG,2,0,1,<DB_OWNER>,SQL$67,NULL,7,NULL,NULL,NULL,NULL,NULL,0
SYSTEM,RDB$INDEX_ID,2,0,1,<DB_OWNER>,SQL$68,NULL,7,NULL,NULL,NULL,NULL,NULL,0
SYSTEM,RDB$INDEX_NAME,252,0,1,<DB_OWNER>,SQL$69,NULL,14,3,4,0,NULL,63,0
SYSTEM,RDB$FIELD_LENGTH,2,0,1,<DB_OWNER>,SQL$70,NULL,7,NULL,NULL,NULL,NULL,NULL,0
SYSTEM,RDB$FIELD_POSITION,2,0,1,<DB_OWNER>,SQL$71,NULL,7,NULL,NULL,NULL,NULL,NULL,0
SYSTEM,RDB$FIELD_SCALE,2,0,1,<DB_OWNER>,SQL$72,NULL,7,NULL,NULL,NULL,NULL,NULL,0
SYSTEM,RDB$FIELD_TYPE,2,0,1,<DB_OWNER>,SQL$73,NULL,7,NULL,NULL,NULL,NULL,NULL,0
SYSTEM,RDB$FORMAT,2,0,1,<DB_OWNER>,SQL$74,NULL,7,NULL,NULL,NULL,NULL,NULL,0
SYSTEM,RDB$DBKEY_LENGTH,2,0,1,<DB_OWNER>,SQL$75,NULL,7,NULL,NULL,NULL,NULL,NULL,0
SYSTEM,RDB$PAGE_NUMBER,4,0,1,<DB_OWNER>,SQL$76,NULL,8,NULL,NULL,NULL,NULL,NULL,0
SYSTEM,RDB$PAGE_SEQUENCE,4,0,1,<DB_OWNER>,SQL$77,NULL,8,NULL,NULL,NULL,NULL,NULL,0
SYSTEM,RDB$PAGE_TYPE,2,0,1,<DB_OWNER>,SQL$78,NULL,7,NULL,NULL,NULL,NULL,NULL,0
SYSTEM,RDB$QUERY_HEADER,8,0,1,<DB_OWNER>,SQL$79,NULL,261,1,4,NULL,80,NULL,0
SYSTEM,RDB$RELATION_ID,2,0,1,<DB_OWNER>,SQL$80,NULL,7,NULL,NULL,NULL,NULL,NULL,0
SYSTEM,RDB$RELATION_NAME,252,0,1,<DB_OWNER>,SQL$81,NULL,14,3,4,0,NULL,63,0
SYSTEM,RDB$SEGMENT_COUNT,2,0,1,<DB_OWNER>,SQL$82,NULL,7,NULL,NULL,NULL,NULL,NULL,0
SYSTEM,RDB$SEGMENT_LENGTH,2,0,1,<DB_OWNER>,SQL$83,NULL,7,NULL,NULL,NULL,NULL,NULL,0
SYSTEM,RDB$SOURCE,8,0,1,<DB_OWNER>,SQL$84,NULL,261,1,4,NULL,80,NULL,0
SYSTEM,RDB$FIELD_SUB_TYPE,2,0,1,<DB_OWNER>,SQL$85,NULL,7,NULL,NULL,NULL,NULL,NULL,0
SYSTEM,RDB$VIEW_BLR,8,0,1,<DB_OWNER>,SQL$86,NULL,261,2,NULL,NULL,80,NULL,0
SYSTEM,RDB$VALIDATION_BLR,8,0,1,<DB_OWNER>,SQL$87,NULL,261,2,NULL,NULL,80,NULL,0
SYSTEM,RDB$VALUE,8,0,1,<DB_OWNER>,SQL$88,NULL,261,2,NULL,NULL,80,NULL,0
SYSTEM,RDB$SECURITY_CLASS,252,0,1,<DB_OWNER>,SQL$89,NULL,14,3,4,0,NULL,63,0
SYSTEM,RDB$ACL,8,0,1,<DB_OWNER>,SQL$90,NULL,261,3,NULL,NULL,80,NULL,0
SYSTEM,RDB$FILE_NAME,255,0,1,<DB_OWNER>,SQL$91,NULL,37,NULL,0,0,NULL,NULL,0
SYSTEM,RDB$FILE_NAME2,1020,0,1,<DB_OWNER>,SQL$92,NULL,37,3,4,0,NULL,255,0
SYSTEM,RDB$FILE_SEQUENCE,2,0,1,<DB_OWNER>,SQL$93,NULL,7,NULL,NULL,NULL,NULL,NULL,0
SYSTEM,RDB$FILE_START,4,0,1,<DB_OWNER>,SQL$94,NULL,8,NULL,NULL,NULL,NULL,NULL,0
SYSTEM,RDB$FILE_LENGTH,4,0,1,<DB_OWNER>,SQL$95,NULL,8,NULL,NULL,NULL,NULL,NULL,0
SYSTEM,RDB$FILE_FLAGS,2,0,1,<DB_OWNER>,SQL$96,NULL,7,NULL,NULL,NULL,NULL,NULL,0
SYSTEM,RDB$TRIGGER_BLR,8,0,1,<DB_OWNER>,SQL$97,NULL,261,2,NULL,NULL,80,NULL,0
SYSTEM,RDB$TRIGGER_NAME,252,0,1,<DB_OWNER>,SQL$98,NULL,14,3,4,0,NULL,63,0
SYSTEM,RDB$GENERIC_NAME,252,0,1,<DB_OWNER>,SQL$99,NULL,14,3,4,0,NULL,63,0
SYSTEM,RDB$FUNCTION_NAME,252,0,1,<DB_OWNER>,SQL$100,NULL,14,3,4,0,NULL,63,0
SYSTEM,RDB$EXTERNAL_NAME,255,0,1,<DB_OWNER>,SQL$101,NULL,14,NULL,0,0,NULL,NULL,0
SYSTEM,RDB$TYPE_NAME,252,0,1,<DB_OWNER>,SQL$102,NULL,14,3,4,0,NULL,63,0
SYSTEM,RDB$DIMENSIONS,2,0,1,<DB_OWNER>,SQL$103,NULL,7,NULL,NULL,NULL,NULL,NULL,0
SYSTEM,RDB$RUNTIME,8,0,1,<DB_OWNER>,SQL$104,NULL,261,5,NULL,NULL,80,NULL,0
SYSTEM,RDB$TRIGGER_SEQUENCE,2,0,1,<DB_OWNER>,SQL$105,NULL,7,NULL,NULL,NULL,NULL,NULL,0
SYSTEM,RDB$GENERIC_TYPE,2,0,1,<DB_OWNER>,SQL$106,NULL,7,NULL,NULL,NULL,NULL,NULL,0
SYSTEM,RDB$TRIGGER_TYPE,8,0,1,<DB_OWNER>,SQL$107,NULL,16,NULL,NULL,NULL,NULL,NULL,0
SYSTEM,RDB$OBJECT_TYPE,2,0,1,<DB_OWNER>,SQL$108,NULL,7,NULL,NULL,NULL,NULL,NULL,0
SYSTEM,RDB$MECHANISM,2,0,1,<DB_OWNER>,SQL$109,NULL,7,NULL,NULL,NULL,NULL,NULL,0
SYSTEM,RDB$DESCRIPTOR,8,0,1,<DB_OWNER>,SQL$110,NULL,261,6,NULL,NULL,80,NULL,0
SYSTEM,RDB$FUNCTION_TYPE,2,0,1,<DB_OWNER>,SQL$111,NULL,7,NULL,NULL,NULL,NULL,NULL,0
SYSTEM,RDB$TRANSACTION_ID,8,0,1,<DB_OWNER>,SQL$112,NULL,16,NULL,NULL,NULL,NULL,NULL,0
SYSTEM,RDB$TRANSACTION_STATE,2,0,1,<DB_OWNER>,SQL$113,NULL,7,NULL,NULL,NULL,NULL,NULL,0
SYSTEM,RDB$TIMESTAMP,8,0,1,<DB_OWNER>,SQL$114,NULL,35,NULL,NULL,NULL,NULL,NULL,0
SYSTEM,RDB$TRANSACTION_DESCRIPTION,8,0,1,<DB_OWNER>,SQL$115,NULL,261,7,NULL,NULL,80,NULL,0
SYSTEM,RDB$MESSAGE,1023,0,1,<DB_OWNER>,SQL$116,NULL,37,NULL,0,0,NULL,NULL,0
SYSTEM,RDB$MESSAGE_NUMBER,2,0,1,<DB_OWNER>,SQL$117,NULL,7,NULL,NULL,NULL,NULL,NULL,0
SYSTEM,RDB$USER,252,0,1,<DB_OWNER>,SQL$118,NULL,14,3,4,0,NULL,63,0
SYSTEM,RDB$PRIVILEGE,6,0,1,<DB_OWNER>,SQL$119,NULL,14,NULL,0,0,NULL,NULL,0
SYSTEM,RDB$EXTERNAL_DESCRIPTION,8,0,1,<DB_OWNER>,SQL$120,NULL,261,8,NULL,NULL,80,NULL,0
SYSTEM,RDB$SHADOW_NUMBER,2,0,1,<DB_OWNER>,SQL$121,NULL,7,NULL,NULL,NULL,NULL,NULL,0
SYSTEM,RDB$GENERATOR_NAME,252,0,1,<DB_OWNER>,SQL$122,NULL,14,3,4,0,NULL,63,0
SYSTEM,RDB$GENERATOR_ID,2,0,1,<DB_OWNER>,SQL$123,NULL,7,NULL,NULL,NULL,NULL,NULL,0
SYSTEM,RDB$BOUND,4,0,1,<DB_OWNER>,SQL$124,NULL,8,NULL,NULL,NULL,NULL,NULL,0
SYSTEM,RDB$DIMENSION,2,0,1,<DB_OWNER>,SQL$125,NULL,7,NULL,NULL,NULL,NULL,NULL,0
SYSTEM,RDB$STATISTICS,8,0,1,<DB_OWNER>,SQL$126,NULL,27,NULL,NULL,NULL,NULL,NULL,0
SYSTEM,RDB$NULL_FLAG,2,0,1,<DB_OWNER>,SQL$127,NULL,7,NULL,NULL,NULL,NULL,NULL,0
SYSTEM,RDB$CONSTRAINT_NAME,252,0,1,<DB_OWNER>,SQL$128,NULL,14,3,4,0,NULL,63,0
SYSTEM,RDB$CONSTRAINT_TYPE,11,0,1,<DB_OWNER>,SQL$129,NULL,14,NULL,0,0,NULL,NULL,0
SYSTEM,RDB$DEFERRABLE,3,0,1,<DB_OWNER>,SQL$130,0x05 0x15 0x0F 0x02 0x00 0x02 0x00 0x4E 0x4F 0x4C,14,NULL,0,0,NULL,NULL,0
SYSTEM,RDB$MATCH_OPTION,7,0,1,<DB_OWNER>,SQL$131,0x05 0x15 0x0F 0x02 0x00 0x04 0x00 0x46 0x55 0x4C 0x4C 0x4C,14,NULL,0,0,NULL,NULL,0
SYSTEM,RDB$RULE,11,0,1,<DB_OWNER>,SQL$132,0x05 0x15 0x0F 0x02 0x00 0x08 0x00 0x52 0x45 0x53 0x54 0x52 0x49 0x43 0x54 0x4C,14,NULL,0,0,NULL,NULL,0
SYSTEM,RDB$FILE_PARTITIONS,2,0,1,<DB_OWNER>,SQL$133,NULL,7,NULL,NULL,NULL,NULL,NULL,0
SYSTEM,RDB$PROCEDURE_BLR,8,0,1,<DB_OWNER>,SQL$134,NULL,261,2,NULL,NULL,80,NULL,0
SYSTEM,RDB$PROCEDURE_ID,2,0,1,<DB_OWNER>,SQL$135,NULL,7,NULL,NULL,NULL,NULL,NULL,0
SYSTEM,RDB$PROCEDURE_PARAMETERS,2,0,1,<DB_OWNER>,SQL$136,NULL,7,NULL,NULL,NULL,NULL,NULL,0
SYSTEM,RDB$PROCEDURE_NAME,252,0,1,<DB_OWNER>,SQL$137,NULL,14,3,4,0,NULL,63,0
SYSTEM,RDB$PARAMETER_NAME,252,0,1,<DB_OWNER>,SQL$138,NULL,14,3,4,0,NULL,63,0
SYSTEM,RDB$PARAMETER_NUMBER,2,0,1,<DB_OWNER>,SQL$139,NULL,7,NULL,NULL,NULL,NULL,NULL,0
SYSTEM,RDB$PARAMETER_TYPE,2,0,1,<DB_OWNER>,SQL$140,NULL,7,NULL,NULL,NULL,NULL,NULL,0
SYSTEM,RDB$CHARACTER_SET_NAME,252,0,1,<DB_OWNER>,SQL$141,NULL,14,3,4,0,NULL,63,0
SYSTEM,RDB$CHARACTER_SET_ID,2,0,1,<DB_OWNER>,SQL$142,NULL,7,NULL,NULL,NULL,NULL,NULL,0
SYSTEM,RDB$COLLATION_NAME,252,0,1,<DB_OWNER>,SQL$143,NULL,14,3,4,0,NULL,63,0
SYSTEM,RDB$COLLATION_ID,2,0,1,<DB_OWNER>,SQL$144,NULL,7,NULL,NULL,NULL,NULL,NULL,0
SYSTEM,RDB$NUMBER_OF_CHARACTERS,4,0,1,<DB_OWNER>,SQL$145,NULL,8,NULL,NULL,NULL,NULL,NULL,0
SYSTEM,RDB$EXCEPTION_NAME,252,0,1,<DB_OWNER>,SQL$146,NULL,14,3,4,0,NULL,63,0
SYSTEM,RDB$EXCEPTION_NUMBER,4,0,1,<DB_OWNER>,SQL$147,NULL,8,NULL,NULL,NULL,NULL,NULL,0
SYSTEM,RDB$FILE_P_OFFSET,4,0,1,<DB_OWNER>,SQL$148,NULL,8,NULL,NULL,NULL,NULL,NULL,0
SYSTEM,RDB$FIELD_PRECISION,2,0,1,<DB_OWNER>,SQL$149,NULL,7,NULL,NULL,NULL,NULL,NULL,0
SYSTEM,RDB$BACKUP_ID,4,0,1,<DB_OWNER>,SQL$150,NULL,8,NULL,NULL,NULL,NULL,NULL,0
SYSTEM,RDB$BACKUP_LEVEL,4,0,1,<DB_OWNER>,SQL$151,NULL,8,NULL,NULL,NULL,NULL,NULL,0
SYSTEM,RDB$GUID,38,0,1,<DB_OWNER>,SQL$152,NULL,14,NULL,0,0,NULL,NULL,0
SYSTEM,RDB$SCN,4,0,1,<DB_OWNER>,SQL$153,NULL,8,NULL,NULL,NULL,NULL,NULL,0
SYSTEM,RDB$SPECIFIC_ATTRIBUTES,8,0,1,<DB_OWNER>,SQL$154,NULL,261,1,4,NULL,80,NULL,0
SYSTEM,RDB$PLUGIN,252,0,1,<DB_OWNER>,SQL$155,NULL,14,3,4,0,NULL,63,0
SYSTEM,RDB$RELATION_TYPE,2,0,1,<DB_OWNER>,SQL$156,NULL,7,NULL,NULL,NULL,NULL,NULL,0
SYSTEM,RDB$PROCEDURE_TYPE,2,0,1,<DB_OWNER>,SQL$157,NULL,7,NULL,NULL,NULL,NULL,NULL,0
SYSTEM,RDB$ATTACHMENT_ID,8,0,1,<DB_OWNER>,SQL$158,NULL,16,NULL,NULL,NULL,NULL,NULL,0
SYSTEM,RDB$STATEMENT_ID,8,0,1,<DB_OWNER>,SQL$159,NULL,16,NULL,NULL,NULL,NULL,NULL,0
SYSTEM,RDB$CALL_ID,8,0,1,<DB_OWNER>,SQL$160,NULL,16,NULL,NULL,NULL,NULL,NULL,0
SYSTEM,RDB$STAT_ID,4,0,1,<DB_OWNER>,SQL$161,NULL,8,NULL,NULL,NULL,NULL,NULL,0
SYSTEM,RDB$PID,4,0,1,<DB_OWNER>,SQL$162,NULL,8,NULL,NULL,NULL,NULL,NULL,0
SYSTEM,RDB$STATE,2,0,1,<DB_OWNER>,SQL$163,NULL,7,NULL,NULL,NULL,NULL,NULL,0
SYSTEM,RDB$ODS_NUMBER,2,0,1,<DB_OWNER>,SQL$164,NULL,7,NULL,NULL,NULL,NULL,NULL,0
SYSTEM,RDB$PAGE_SIZE,4,0,1,<DB_OWNER>,SQL$165,NULL,8,NULL,NULL,NULL,NULL,NULL,0
SYSTEM,RDB$PAGE_BUFFERS,4,0,1,<DB_OWNER>,SQL$166,NULL,8,NULL,NULL,NULL,NULL,NULL,0
SYSTEM,RDB$SHUTDOWN_MODE,2,0,1,<DB_OWNER>,SQL$167,NULL,7,NULL,NULL,NULL,NULL,NULL,0
SYSTEM,RDB$SQL_DIALECT,2,0,1,<DB_OWNER>,SQL$168,NULL,7,NULL,NULL,NULL,NULL,NULL,0
SYSTEM,RDB$SWEEP_INTERVAL,4,0,1,<DB_OWNER>,SQL$169,NULL,8,NULL,NULL,NULL,NULL,NULL,0
SYSTEM,RDB$COUNTER,8,0,1,<DB_OWNER>,SQL$170,NULL,16,NULL,NULL,NULL,NULL,NULL,0
SYSTEM,RDB$REMOTE_PROTOCOL,10,0,1,<DB_OWNER>,SQL$171,NULL,37,2,2,0,NULL,NULL,0
SYSTEM,RDB$REMOTE_ADDRESS,255,0,1,<DB_OWNER>,SQL$172,NULL,37,2,2,0,NULL,NULL,0
SYSTEM,RDB$ISOLATION_MODE,2,0,1,<DB_OWNER>,SQL$173,NULL,7,NULL,NULL,NULL,NULL,NULL,0
SYSTEM,RDB$LOCK_TIMEOUT,2,0,1,<DB_OWNER>,SQL$174,NULL,7,NULL,NULL,NULL,NULL,NULL,0
SYSTEM,RDB$BACKUP_STATE,2,0,1,<DB_OWNER>,SQL$175,NULL,7,NULL,NULL,NULL,NULL,NULL,0
SYSTEM,RDB$STAT_GROUP,2,0,1,<DB_OWNER>,SQL$176,NULL,7,NULL,NULL,NULL,NULL,NULL,0
SYSTEM,RDB$DEBUG_INFO,8,0,1,<DB_OWNER>,SQL$177,NULL,261,9,NULL,NULL,80,NULL,0
SYSTEM,RDB$PARAMETER_MECHANISM,2,0,1,<DB_OWNER>,SQL$178,NULL,7,NULL,NULL,NULL,NULL,NULL,0
SYSTEM,RDB$SOURCE_INFO,4,0,1,<DB_OWNER>,SQL$179,NULL,8,NULL,NULL,NULL,NULL,NULL,0
SYSTEM,RDB$CONTEXT_VAR_NAME,80,0,1,<DB_OWNER>,SQL$180,NULL,37,NULL,0,0,NULL,NULL,0
SYSTEM,RDB$CONTEXT_VAR_VALUE,32765,0,1,<DB_OWNER>,SQL$181,NULL,37,NULL,0,0,NULL,NULL,0
SYSTEM,RDB$ENGINE_NAME,252,0,1,<DB_OWNER>,SQL$182,NULL,14,3,4,0,NULL,63,0
SYSTEM,RDB$PACKAGE_NAME,252,0,1,<DB_OWNER>,SQL$183,NULL,14,3,4,0,NULL,63,0
SYSTEM,RDB$FUNCTION_ID,2,0,1,<DB_OWNER>,SQL$184,NULL,7,NULL,NULL,NULL,NULL,NULL,0
SYSTEM,RDB$FUNCTION_BLR,8,0,1,<DB_OWNER>,SQL$185,NULL,261,2,NULL,NULL,80,NULL,0
SYSTEM,RDB$ARGUMENT_NAME,252,0,1,<DB_OWNER>,SQL$186,NULL,14,3,4,0,NULL,63,0
SYSTEM,RDB$ARGUMENT_MECHANISM,2,0,1,<DB_OWNER>,SQL$187,NULL,7,NULL,NULL,NULL,NULL,NULL,0
SYSTEM,RDB$IDENTITY_TYPE,2,0,1,<DB_OWNER>,SQL$188,NULL,7,NULL,NULL,NULL,NULL,NULL,0
SYSTEM,RDB$BOOLEAN,1,0,1,<DB_OWNER>,SQL$189,NULL,23,NULL,NULL,NULL,NULL,NULL,0
SYSTEM,SEC$USER_NAME,252,0,1,<DB_OWNER>,SQL$190,NULL,37,3,4,0,NULL,63,0
SYSTEM,SEC$KEY,252,0,1,<DB_OWNER>,SQL$191,NULL,37,3,4,0,NULL,63,0
SYSTEM,SEC$VALUE,1020,0,1,<DB_OWNER>,SQL$192,NULL,37,3,4,0,NULL,255,0
SYSTEM,SEC$NAME_PART,128,0,1,<DB_OWNER>,SQL$193,NULL,37,3,4,0,NULL,32,0
SYSTEM,RDB$CLIENT_VERSION,255,0,1,<DB_OWNER>,SQL$194,NULL,37,2,2,0,NULL,NULL,0
SYSTEM,RDB$REMOTE_VERSION,255,0,1,<DB_OWNER>,SQL$195,NULL,37,2,2,0,NULL,NULL,0
SYSTEM,RDB$HOST_NAME,1020,0,1,<DB_OWNER>,SQL$196,NULL,37,3,4,0,NULL,255,0
SYSTEM,RDB$OS_USER,1020,0,1,<DB_OWNER>,SQL$197,NULL,37,3,4,0,NULL,255,0
SYSTEM,RDB$GENERATOR_VALUE,8,0,1,<DB_OWNER>,SQL$198,NULL,16,NULL,NULL,NULL,NULL,NULL,0
SYSTEM,RDB$AUTH_METHOD,255,0,1,<DB_OWNER>,SQL$199,NULL,37,2,2,0,NULL,NULL,0
SYSTEM,RDB$LINGER,4,0,1,<DB_OWNER>,SQL$200,NULL,8,NULL,NULL,NULL,NULL,NULL,0
SYSTEM,MON$SEC_DATABASE,7,0,1,<DB_OWNER>,SQL$201,NULL,14,2,2,0,NULL,NULL,1
SYSTEM,RDB$MAP_NAME,252,0,1,<DB_OWNER>,SQL$202,NULL,14,3,4,0,NULL,63,1
SYSTEM,RDB$MAP_USING,4,0,1,<DB_OWNER>,SQL$203,NULL,14,3,4,0,NULL,1,1
SYSTEM,RDB$MAP_DB,252,0,1,<DB_OWNER>,SQL$204,NULL,14,3,4,0,NULL,63,0
SYSTEM,RDB$MAP_FROM_TYPE,252,0,1,<DB_OWNER>,SQL$205,NULL,14,3,4,0,NULL,63,1
SYSTEM,RDB$MAP_FROM,1020,0,1,<DB_OWNER>,SQL$206,NULL,14,3,4,0,NULL,255,0
SYSTEM,RDB$MAP_TO,252,0,1,<DB_OWNER>,SQL$207,NULL,14,3,4,0,NULL,63,0
SYSTEM,RDB$GENERATOR_INCREMENT,4,0,1,<DB_OWNER>,SQL$208,NULL,8,NULL,NULL,NULL,NULL,NULL,1
SYSTEM,RDB$PLAN,8,0,1,<DB_OWNER>,SQL$209,NULL,261,1,4,NULL,80,NULL,0
SYSTEM,RDB$SYSTEM_PRIVILEGES,8,0,1,<DB_OWNER>,SQL$210,0x05 0x15 0x0F 0x01 0x00 0x08 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x4C,14,1,1,0,NULL,NULL,0
SYSTEM,RDB$SQL_SECURITY,1,0,1,<DB_OWNER>,SQL$211,NULL,23,NULL,NULL,NULL,NULL,NULL,0
SYSTEM,MON$IDLE_TIMEOUT,4,0,1,<DB_OWNER>,SQL$212,NULL,8,NULL,NULL,NULL,NULL,NULL,1
SYSTEM,MON$IDLE_TIMER,12,0,1,<DB_OWNER>,SQL$213,NULL,29,NULL,NULL,NULL,NULL,NULL,0
SYSTEM,MON$STATEMENT_TIMEOUT,4,0,1,<DB_OWNER>,SQL$214,NULL,8,NULL,NULL,NULL,NULL,NULL,1
SYSTEM,MON$STATEMENT_TIMER,12,0,1,<DB_OWNER>,SQL$215,NULL,29,NULL,NULL,NULL,NULL,NULL,0
SYSTEM,RDB$TIME_ZONE_ID,4,0,1,<DB_OWNER>,SQL$216,NULL,8,NULL,NULL,NULL,NULL,NULL,0
SYSTEM,RDB$TIME_ZONE_NAME,252,0,1,<DB_OWNER>,SQL$217,NULL,14,3,4,0,NULL,63,0
SYSTEM,RDB$TIME_ZONE_OFFSET,2,0,1,<DB_OWNER>,SQL$218,NULL,7,NULL,NULL,NULL,NULL,NULL,0
SYSTEM,RDB$TIMESTAMP_TZ,12,0,1,<DB_OWNER>,SQL$219,NULL,29,NULL,NULL,NULL,NULL,NULL,0
SYSTEM,RDB$DBTZ_VERSION,10,0,1,<DB_OWNER>,SQL$220,NULL,37,2,2,0,NULL,NULL,0
SYSTEM,RDB$CRYPT_STATE,2,0,1,<DB_OWNER>,SQL$221,NULL,7,NULL,NULL,NULL,NULL,NULL,0
SYSTEM,MON$WIRE_CRYPT_PLUGIN,252,0,1,<DB_OWNER>,SQL$222,NULL,37,3,4,0,NULL,63,0
SYSTEM,RDB$PUBLICATION_NAME,252,0,1,<DB_OWNER>,SQL$223,NULL,14,3,4,0,NULL,63,1
SYSTEM,RDB$FILE_ID,255,0,1,<DB_OWNER>,SQL$224,NULL,37,2,2,0,NULL,NULL,1
SYSTEM,RDB$CONFIG_ID,4,0,1,<DB_OWNER>,SQL$225,NULL,8,NULL,NULL,NULL,NULL,NULL,1
SYSTEM,RDB$CONFIG_NAME,63,0,1,<DB_OWNER>,SQL$226,NULL,37,2,2,0,NULL,NULL,1
SYSTEM,RDB$CONFIG_VALUE,1020,0,1,<DB_OWNER>,SQL$227,NULL,37,3,4,0,NULL,255,0
SYSTEM,RDB$CONFIG_IS_SET,1,0,1,<DB_OWNER>,SQL$228,NULL,23,NULL,NULL,NULL,NULL,NULL,1
SYSTEM,RDB$REPLICA_MODE,2,0,1,<DB_OWNER>,SQL$229,NULL,7,NULL,NULL,NULL,NULL,NULL,0
SYSTEM,RDB$KEYWORD_NAME,63,0,1,<DB_OWNER>,SQL$230,NULL,37,2,2,0,NULL,NULL,1
SYSTEM,RDB$KEYWORD_RESERVED,1,0,1,<DB_OWNER>,SQL$231,NULL,23,NULL,NULL,NULL,NULL,NULL,1
SYSTEM,RDB$SHORT_DESCRIPTION,1020,0,1,<DB_OWNER>,SQL$232,NULL,37,3,4,0,NULL,255,0
SYSTEM,RDB$SECONDS_INTERVAL,4,0,1,<DB_OWNER>,SQL$233,NULL,8,NULL,NULL,NULL,NULL,NULL,0
SYSTEM,RDB$PROFILE_SESSION_ID,8,0,1,<DB_OWNER>,SQL$234,NULL,16,NULL,NULL,NULL,NULL,NULL,0
SYSTEM,RDB$BLOB_UTIL_HANDLE,4,0,1,<DB_OWNER>,SQL$235,NULL,8,NULL,NULL,NULL,NULL,NULL,0
SYSTEM,RDB$BLOB,8,0,1,<DB_OWNER>,SQL$236,NULL,261,0,NULL,NULL,80,NULL,0
SYSTEM,RDB$VARBINARY_MAX,32765,0,1,<DB_OWNER>,SQL$237,NULL,37,NULL,0,0,NULL,NULL,0
SYSTEM,RDB$INTEGER,4,0,1,<DB_OWNER>,SQL$238,NULL,8,NULL,NULL,NULL,NULL,NULL,0
SYSTEM,MON$PARALLEL_WORKERS,4,0,1,<DB_OWNER>,SQL$239,NULL,8,NULL,NULL,NULL,NULL,NULL,0
SYSTEM,RDB$SCHEMA_NAME,252,0,1,<DB_OWNER>,SQL$240,NULL,14,3,4,0,NULL,63,0
SYSTEM,RDB$TEXT_MAX,32765,0,1,<DB_OWNER>,SQL$241,NULL,37,3,4,0,NULL,8191,0
SYSTEM,MON$TABLE_TYPE,32,0,1,<DB_OWNER>,SQL$242,NULL,37,2,2,0,NULL,NULL,0
```
#### RDB$CHARACTER_SETS (Default Rows)
Rows must be inserted in the exact order listed. `RDB$SYSTEM_FLAG = RDB_system (1)` for all rows. `RDB$OWNER_NAME = <DB_OWNER>` and `RDB$SECURITY_CLASS = SQL$<n>` for each row (one new security class per row, in row order).

```csv
RDB$SCHEMA_NAME,RDB$CHARACTER_SET_NAME,RDB$DEFAULT_COLLATE_SCHEMA_NAME,RDB$DEFAULT_COLLATE_NAME,RDB$CHARACTER_SET_ID,RDB$BYTES_PER_CHARACTER,RDB$SYSTEM_FLAG,RDB$OWNER_NAME,RDB$SECURITY_CLASS
SYSTEM,NONE,SYSTEM,NONE,0,1,1,<DB_OWNER>,SQL$<n>
SYSTEM,OCTETS,SYSTEM,OCTETS,1,1,1,<DB_OWNER>,SQL$<n>
SYSTEM,ASCII,SYSTEM,ASCII,2,1,1,<DB_OWNER>,SQL$<n>
SYSTEM,UNICODE_FSS,SYSTEM,UNICODE_FSS,3,3,1,<DB_OWNER>,SQL$<n>
SYSTEM,UTF8,SYSTEM,UTF8,4,4,1,<DB_OWNER>,SQL$<n>
SYSTEM,SJIS_0208,SYSTEM,SJIS_0208,5,2,1,<DB_OWNER>,SQL$<n>
SYSTEM,EUCJ_0208,SYSTEM,EUCJ_0208,6,2,1,<DB_OWNER>,SQL$<n>
SYSTEM,DOS437,SYSTEM,DOS437,10,1,1,<DB_OWNER>,SQL$<n>
SYSTEM,DOS850,SYSTEM,DOS850,11,1,1,<DB_OWNER>,SQL$<n>
SYSTEM,DOS865,SYSTEM,DOS865,12,1,1,<DB_OWNER>,SQL$<n>
SYSTEM,ISO8859_1,SYSTEM,ISO8859_1,21,1,1,<DB_OWNER>,SQL$<n>
SYSTEM,ISO8859_2,SYSTEM,ISO8859_2,22,1,1,<DB_OWNER>,SQL$<n>
SYSTEM,ISO8859_3,SYSTEM,ISO8859_3,23,1,1,<DB_OWNER>,SQL$<n>
SYSTEM,ISO8859_4,SYSTEM,ISO8859_4,34,1,1,<DB_OWNER>,SQL$<n>
SYSTEM,ISO8859_5,SYSTEM,ISO8859_5,35,1,1,<DB_OWNER>,SQL$<n>
SYSTEM,ISO8859_6,SYSTEM,ISO8859_6,36,1,1,<DB_OWNER>,SQL$<n>
SYSTEM,ISO8859_7,SYSTEM,ISO8859_7,37,1,1,<DB_OWNER>,SQL$<n>
SYSTEM,ISO8859_8,SYSTEM,ISO8859_8,38,1,1,<DB_OWNER>,SQL$<n>
SYSTEM,ISO8859_9,SYSTEM,ISO8859_9,39,1,1,<DB_OWNER>,SQL$<n>
SYSTEM,ISO8859_13,SYSTEM,ISO8859_13,40,1,1,<DB_OWNER>,SQL$<n>
SYSTEM,DOS852,SYSTEM,DOS852,45,1,1,<DB_OWNER>,SQL$<n>
SYSTEM,DOS857,SYSTEM,DOS857,46,1,1,<DB_OWNER>,SQL$<n>
SYSTEM,DOS860,SYSTEM,DOS860,13,1,1,<DB_OWNER>,SQL$<n>
SYSTEM,DOS861,SYSTEM,DOS861,47,1,1,<DB_OWNER>,SQL$<n>
SYSTEM,DOS863,SYSTEM,DOS863,14,1,1,<DB_OWNER>,SQL$<n>
SYSTEM,CYRL,SYSTEM,CYRL,50,1,1,<DB_OWNER>,SQL$<n>
SYSTEM,DOS737,SYSTEM,DOS737,9,1,1,<DB_OWNER>,SQL$<n>
SYSTEM,DOS775,SYSTEM,DOS775,15,1,1,<DB_OWNER>,SQL$<n>
SYSTEM,DOS858,SYSTEM,DOS858,16,1,1,<DB_OWNER>,SQL$<n>
SYSTEM,DOS862,SYSTEM,DOS862,17,1,1,<DB_OWNER>,SQL$<n>
SYSTEM,DOS864,SYSTEM,DOS864,18,1,1,<DB_OWNER>,SQL$<n>
SYSTEM,DOS866,SYSTEM,DOS866,48,1,1,<DB_OWNER>,SQL$<n>
SYSTEM,DOS869,SYSTEM,DOS869,49,1,1,<DB_OWNER>,SQL$<n>
SYSTEM,WIN1250,SYSTEM,WIN1250,51,1,1,<DB_OWNER>,SQL$<n>
SYSTEM,WIN1251,SYSTEM,WIN1251,52,1,1,<DB_OWNER>,SQL$<n>
SYSTEM,WIN1252,SYSTEM,WIN1252,53,1,1,<DB_OWNER>,SQL$<n>
SYSTEM,WIN1253,SYSTEM,WIN1253,54,1,1,<DB_OWNER>,SQL$<n>
SYSTEM,WIN1254,SYSTEM,WIN1254,55,1,1,<DB_OWNER>,SQL$<n>
SYSTEM,NEXT,SYSTEM,NEXT,19,1,1,<DB_OWNER>,SQL$<n>
SYSTEM,WIN1255,SYSTEM,WIN1255,58,1,1,<DB_OWNER>,SQL$<n>
SYSTEM,WIN1256,SYSTEM,WIN1256,59,1,1,<DB_OWNER>,SQL$<n>
SYSTEM,WIN1257,SYSTEM,WIN1257,60,1,1,<DB_OWNER>,SQL$<n>
SYSTEM,KSC_5601,SYSTEM,KSC_5601,44,2,1,<DB_OWNER>,SQL$<n>
SYSTEM,BIG_5,SYSTEM,BIG_5,56,2,1,<DB_OWNER>,SQL$<n>
SYSTEM,GB_2312,SYSTEM,GB_2312,57,2,1,<DB_OWNER>,SQL$<n>
SYSTEM,KOI8R,SYSTEM,KOI8R,63,1,1,<DB_OWNER>,SQL$<n>
SYSTEM,KOI8U,SYSTEM,KOI8U,64,1,1,<DB_OWNER>,SQL$<n>
SYSTEM,WIN1258,SYSTEM,WIN1258,65,1,1,<DB_OWNER>,SQL$<n>
SYSTEM,TIS620,SYSTEM,TIS620,66,1,1,<DB_OWNER>,SQL$<n>
SYSTEM,GBK,SYSTEM,GBK,67,2,1,<DB_OWNER>,SQL$<n>
SYSTEM,CP943C,SYSTEM,CP943C,68,2,1,<DB_OWNER>,SQL$<n>
SYSTEM,GB18030,SYSTEM,GB18030,69,4,1,<DB_OWNER>,SQL$<n>
```

#### RDB$COLLATIONS (Default Rows)
Rows must be inserted in the exact order listed. `RDB$SYSTEM_FLAG = RDB_system (1)` for all rows. `RDB$OWNER_NAME = <DB_OWNER>` and `RDB$SECURITY_CLASS = SQL$<n>` for each row (one new security class per row, in row order).
`RDB$SPECIFIC_ATTRIBUTES` is a text blob (subtype `isc_blob_text`, charset `dsc_text_type_metadata`) whose contents are the literal string shown, or NULL if the CSV field is `NULL`.

```csv
RDB$SCHEMA_NAME,RDB$COLLATION_NAME,RDB$BASE_COLLATION_NAME,RDB$CHARACTER_SET_ID,RDB$COLLATION_ID,RDB$COLLATION_ATTRIBUTES,RDB$SPECIFIC_ATTRIBUTES,RDB$SYSTEM_FLAG,RDB$OWNER_NAME,RDB$SECURITY_CLASS
SYSTEM,NONE,NULL,0,0,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,OCTETS,NULL,1,0,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,ASCII,NULL,2,0,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,UNICODE_FSS,NULL,3,0,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,UTF8,NULL,4,0,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,UCS_BASIC,NULL,4,1,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,UNICODE,NULL,4,2,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,UNICODE_CI,UNICODE,4,3,3,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,UNICODE_CI_AI,UNICODE,4,4,7,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,SJIS_0208,NULL,5,0,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,EUCJ_0208,NULL,6,0,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,DOS437,NULL,10,0,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,PDOX_ASCII,NULL,10,1,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,PDOX_INTL,NULL,10,2,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,PDOX_SWEDFIN,NULL,10,3,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,DB_DEU437,NULL,10,4,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,DB_ESP437,NULL,10,5,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,DB_FIN437,NULL,10,6,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,DB_FRA437,NULL,10,7,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,DB_ITA437,NULL,10,8,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,DB_NLD437,NULL,10,9,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,DB_SVE437,NULL,10,10,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,DB_UK437,NULL,10,11,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,DB_US437,NULL,10,12,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,DOS850,NULL,11,0,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,DB_FRC850,NULL,11,1,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,DB_DEU850,NULL,11,2,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,DB_ESP850,NULL,11,3,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,DB_FRA850,NULL,11,4,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,DB_ITA850,NULL,11,5,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,DB_NLD850,NULL,11,6,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,DB_PTB850,NULL,11,7,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,DB_SVE850,NULL,11,8,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,DB_UK850,NULL,11,9,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,DB_US850,NULL,11,10,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,DOS865,NULL,12,0,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,PDOX_NORDAN4,NULL,12,1,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,DB_DAN865,NULL,12,2,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,DB_NOR865,NULL,12,3,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,ISO8859_1,NULL,21,0,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,DA_DA,NULL,21,1,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,DU_NL,NULL,21,2,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,FI_FI,NULL,21,3,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,FR_FR,NULL,21,4,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,FR_CA,NULL,21,5,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,DE_DE,NULL,21,6,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,IS_IS,NULL,21,7,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,IT_IT,NULL,21,8,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,NO_NO,NULL,21,9,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,ES_ES,NULL,21,10,1,DISABLE-COMPRESSIONS=1;SPECIALS-FIRST=1,1,<DB_OWNER>,SQL$<n>
SYSTEM,SV_SV,NULL,21,11,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,EN_UK,NULL,21,12,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,EN_US,NULL,21,14,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,PT_PT,NULL,21,15,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,PT_BR,NULL,21,16,7,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,ES_ES_CI_AI,NULL,21,17,7,DISABLE-COMPRESSIONS=1;SPECIALS-FIRST=1,1,<DB_OWNER>,SQL$<n>
SYSTEM,FR_FR_CI_AI,FR_FR,21,18,7,SPECIALS-FIRST=1,1,<DB_OWNER>,SQL$<n>
SYSTEM,FR_CA_CI_AI,FR_CA,21,19,7,SPECIALS-FIRST=1,1,<DB_OWNER>,SQL$<n>
SYSTEM,ISO8859_2,NULL,22,0,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,CS_CZ,NULL,22,1,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,ISO_HUN,NULL,22,2,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,ISO_PLK,NULL,22,3,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,ISO8859_3,NULL,23,0,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,ISO8859_4,NULL,34,0,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,ISO8859_5,NULL,35,0,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,ISO8859_6,NULL,36,0,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,ISO8859_7,NULL,37,0,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,ISO8859_8,NULL,38,0,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,ISO8859_9,NULL,39,0,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,ISO8859_13,NULL,40,0,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,LT_LT,NULL,40,1,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,DOS852,NULL,45,0,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,DB_CSY,NULL,45,1,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,DB_PLK,NULL,45,2,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,DB_SLO,NULL,45,4,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,PDOX_CSY,NULL,45,5,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,PDOX_PLK,NULL,45,6,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,PDOX_HUN,NULL,45,7,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,PDOX_SLO,NULL,45,8,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,DOS857,NULL,46,0,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,DB_TRK,NULL,46,1,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,DOS860,NULL,13,0,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,DB_PTG860,NULL,13,1,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,DOS861,NULL,47,0,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,PDOX_ISL,NULL,47,1,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,DOS863,NULL,14,0,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,DB_FRC863,NULL,14,1,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,CYRL,NULL,50,0,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,DB_RUS,NULL,50,1,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,PDOX_CYRL,NULL,50,2,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,DOS737,NULL,9,0,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,DOS775,NULL,15,0,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,DOS858,NULL,16,0,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,DOS862,NULL,17,0,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,DOS864,NULL,18,0,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,DOS866,NULL,48,0,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,DOS869,NULL,49,0,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,WIN1250,NULL,51,0,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,PXW_CSY,NULL,51,1,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,PXW_HUNDC,NULL,51,2,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,PXW_PLK,NULL,51,3,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,PXW_SLOV,NULL,51,4,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,PXW_HUN,NULL,51,5,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,BS_BA,NULL,51,6,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,WIN_CZ,NULL,51,7,3,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,WIN_CZ_CI_AI,NULL,51,8,7,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,WIN1251,NULL,52,0,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,PXW_CYRL,NULL,52,1,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,WIN1251_UA,NULL,52,2,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,WIN1252,NULL,53,0,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,PXW_INTL,NULL,53,1,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,PXW_INTL850,NULL,53,2,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,PXW_NORDAN4,NULL,53,3,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,PXW_SPAN,NULL,53,4,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,PXW_SWEDFIN,NULL,53,5,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,WIN_PTBR,NULL,53,6,7,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,WIN1253,NULL,54,0,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,PXW_GREEK,NULL,54,1,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,WIN1254,NULL,55,0,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,PXW_TURK,NULL,55,1,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,NEXT,NULL,19,0,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,NXT_US,NULL,19,1,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,NXT_DEU,NULL,19,2,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,NXT_FRA,NULL,19,3,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,NXT_ITA,NULL,19,4,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,NXT_ESP,NULL,19,5,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,WIN1255,NULL,58,0,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,WIN1256,NULL,59,0,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,WIN1257,NULL,60,0,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,WIN1257_EE,NULL,60,1,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,WIN1257_LT,NULL,60,2,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,WIN1257_LV,NULL,60,3,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,KSC_5601,NULL,44,0,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,KSC_DICTIONARY,NULL,44,1,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,BIG_5,NULL,56,0,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,GB_2312,NULL,57,0,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,KOI8R,NULL,63,0,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,KOI8R_RU,NULL,63,1,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,KOI8U,NULL,64,0,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,KOI8U_UA,NULL,64,1,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,WIN1258,NULL,65,0,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,TIS620,NULL,66,0,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,TIS620_UNICODE,NULL,66,1,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,GBK,NULL,67,0,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,GBK_UNICODE,NULL,67,1,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,CP943C,NULL,68,0,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,CP943C_UNICODE,NULL,68,1,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,GB18030,NULL,69,0,1,NULL,1,<DB_OWNER>,SQL$<n>
SYSTEM,GB18030_UNICODE,NULL,69,1,1,NULL,1,<DB_OWNER>,SQL$<n>
```

#### RDB$TYPES (Default Rows)
Rows must be inserted in the exact order listed. `RDB$SYSTEM_FLAG = RDB_system (1)` for all rows.

```csv
RDB$FIELD_NAME,RDB$TYPE_NAME,RDB$TYPE,RDB$SYSTEM_FLAG
RDB$FIELD_TYPE,TEXT,14,1
RDB$FIELD_TYPE,SHORT,7,1
RDB$FIELD_TYPE,LONG,8,1
RDB$FIELD_TYPE,QUAD,9,1
RDB$FIELD_TYPE,FLOAT,10,1
RDB$FIELD_TYPE,DOUBLE,27,1
RDB$FIELD_TYPE,TIMESTAMP,35,1
RDB$FIELD_TYPE,VARYING,37,1
RDB$FIELD_TYPE,BLOB,261,1
RDB$FIELD_TYPE,CSTRING,40,1
RDB$FIELD_TYPE,BLOB_ID,45,1
RDB$FIELD_TYPE,DATE,12,1
RDB$FIELD_TYPE,TIME,13,1
RDB$FIELD_TYPE,INT64,16,1
RDB$FIELD_TYPE,BOOLEAN,23,1
RDB$FIELD_TYPE,DECFLOAT(16),24,1
RDB$FIELD_TYPE,DECFLOAT(34),25,1
RDB$FIELD_TYPE,TIMESTAMP WITH TIME ZONE,29,1
RDB$FIELD_TYPE,TIME WITH TIME ZONE,28,1
RDB$FIELD_TYPE,INT128,26,1
RDB$FIELD_SUB_TYPE,BINARY,0,1
RDB$FIELD_SUB_TYPE,TEXT,1,1
RDB$FIELD_SUB_TYPE,BLR,2,1
RDB$FIELD_SUB_TYPE,ACL,3,1
RDB$FIELD_SUB_TYPE,RANGES,4,1
RDB$FIELD_SUB_TYPE,SUMMARY,5,1
RDB$FIELD_SUB_TYPE,FORMAT,6,1
RDB$FIELD_SUB_TYPE,TRANSACTION_DESCRIPTION,7,1
RDB$FIELD_SUB_TYPE,EXTERNAL_FILE_DESCRIPTION,8,1
RDB$FIELD_SUB_TYPE,DEBUG_INFORMATION,9,1
RDB$FUNCTION_TYPE,VALUE,0,1
RDB$FUNCTION_TYPE,BOOLEAN,1,1
RDB$MECHANISM,BY_VALUE,0,1
RDB$MECHANISM,BY_REFERENCE,1,1
RDB$MECHANISM,BY_VMS_DESCRIPTOR,2,1
RDB$MECHANISM,BY_ISC_DESCRIPTOR,3,1
RDB$MECHANISM,BY_SCALAR_ARRAY_DESCRIPTOR,4,1
RDB$MECHANISM,BY_REFERENCE_WITH_NULL,5,1
RDB$TRIGGER_TYPE,PRE_STORE,1,1
RDB$TRIGGER_TYPE,POST_STORE,2,1
RDB$TRIGGER_TYPE,PRE_MODIFY,3,1
RDB$TRIGGER_TYPE,POST_MODIFY,4,1
RDB$TRIGGER_TYPE,PRE_ERASE,5,1
RDB$TRIGGER_TYPE,POST_ERASE,6,1
RDB$TRIGGER_TYPE,CONNECT,8192,1
RDB$TRIGGER_TYPE,DISCONNECT,8193,1
RDB$TRIGGER_TYPE,TRANSACTION_START,8194,1
RDB$TRIGGER_TYPE,TRANSACTION_COMMIT,8195,1
RDB$TRIGGER_TYPE,TRANSACTION_ROLLBACK,8196,1
RDB$OBJECT_TYPE,RELATION,0,1
RDB$OBJECT_TYPE,VIEW,1,1
RDB$OBJECT_TYPE,TRIGGER,2,1
RDB$OBJECT_TYPE,COMPUTED_FIELD,3,1
RDB$OBJECT_TYPE,VALIDATION,4,1
RDB$OBJECT_TYPE,PROCEDURE,5,1
RDB$OBJECT_TYPE,INDEX_EXPRESSION,6,1
RDB$OBJECT_TYPE,EXCEPTION,7,1
RDB$OBJECT_TYPE,USER,8,1
RDB$OBJECT_TYPE,FIELD,9,1
RDB$OBJECT_TYPE,INDEX,10,1
RDB$OBJECT_TYPE,CHARACTER_SET,11,1
RDB$OBJECT_TYPE,USER_GROUP,12,1
RDB$OBJECT_TYPE,ROLE,13,1
RDB$OBJECT_TYPE,GENERATOR,14,1
RDB$OBJECT_TYPE,UDF,15,1
RDB$OBJECT_TYPE,BLOB_FILTER,16,1
RDB$OBJECT_TYPE,COLLATION,17,1
RDB$OBJECT_TYPE,PACKAGE,18,1
RDB$OBJECT_TYPE,PACKAGE BODY,19,1
RDB$OBJECT_TYPE,INDEX_CONDITION,37,1
RDB$TRANSACTION_STATE,LIMBO,1,1
RDB$TRANSACTION_STATE,COMMITTED,2,1
RDB$TRANSACTION_STATE,ROLLED_BACK,3,1
RDB$SYSTEM_FLAG,USER,0,1
RDB$SYSTEM_FLAG,SYSTEM,1,1
RDB$SYSTEM_FLAG,QLI,2,1
RDB$SYSTEM_FLAG,CHECK_CONSTRAINT,3,1
RDB$SYSTEM_FLAG,REFERENTIAL_CONSTRAINT,4,1
RDB$SYSTEM_FLAG,VIEW_CHECK,5,1
RDB$SYSTEM_FLAG,IDENTITY_GENERATOR,6,1
RDB$RELATION_TYPE,PERSISTENT,0,1
RDB$RELATION_TYPE,VIEW,1,1
RDB$RELATION_TYPE,EXTERNAL,2,1
RDB$RELATION_TYPE,VIRTUAL,3,1
RDB$RELATION_TYPE,GLOBAL_TEMPORARY_PRESERVE,4,1
RDB$RELATION_TYPE,GLOBAL_TEMPORARY_DELETE,5,1
RDB$PROCEDURE_TYPE,LEGACY,0,1
RDB$PROCEDURE_TYPE,SELECTABLE,1,1
RDB$PROCEDURE_TYPE,EXECUTABLE,2,1
RDB$PARAMETER_MECHANISM,NORMAL,0,1
RDB$PARAMETER_MECHANISM,TYPE OF,1,1
MON$STATE,IDLE,0,1
MON$STATE,ACTIVE,1,1
MON$STATE,STALLED,2,1
MON$SHUTDOWN_MODE,ONLINE,0,1
MON$SHUTDOWN_MODE,MULTI_USER_SHUTDOWN,1,1
MON$SHUTDOWN_MODE,SINGLE_USER_SHUTDOWN,2,1
MON$SHUTDOWN_MODE,FULL_SHUTDOWN,3,1
MON$ISOLATION_MODE,CONSISTENCY,0,1
MON$ISOLATION_MODE,CONCURRENCY,1,1
MON$ISOLATION_MODE,READ_COMMITTED_VERSION,2,1
MON$ISOLATION_MODE,READ_COMMITTED_NO_VERSION,3,1
MON$ISOLATION_MODE,READ_COMMITTED_READ_CONSISTENCY,4,1
MON$BACKUP_STATE,NORMAL,0,1
MON$BACKUP_STATE,STALLED,1,1
MON$BACKUP_STATE,MERGE,2,1
MON$STAT_GROUP,DATABASE,0,1
MON$STAT_GROUP,ATTACHMENT,1,1
MON$STAT_GROUP,TRANSACTION,2,1
MON$STAT_GROUP,STATEMENT,3,1
MON$STAT_GROUP,CALL,4,1
RDB$IDENTITY_TYPE,ALWAYS,0,1
RDB$IDENTITY_TYPE,BY DEFAULT,1,1
RDB$PARAMETER_TYPE,INPUT,0,1
RDB$PARAMETER_TYPE,OUTPUT,1,1
RDB$TRIGGER_INACTIVE,ACTIVE,0,1
RDB$TRIGGER_INACTIVE,INACTIVE,1,1
RDB$INDEX_INACTIVE,ACTIVE,0,1
RDB$INDEX_INACTIVE,INACTIVE,1,1
RDB$UNIQUE_FLAG,NON_UNIQUE,0,1
RDB$UNIQUE_FLAG,UNIQUE,1,1
RDB$GRANT_OPTION,NONE,0,1
RDB$GRANT_OPTION,GRANT_OPTION,1,1
RDB$GRANT_OPTION,ADMIN_OPTION,2,1
RDB$PAGE_TYPE,HEADER,1,1
RDB$PAGE_TYPE,PAGE_INVENTORY,2,1
RDB$PAGE_TYPE,TRANSACTION_INVENTORY,3,1
RDB$PAGE_TYPE,POINTER,4,1
RDB$PAGE_TYPE,DATA,5,1
RDB$PAGE_TYPE,INDEX_ROOT,6,1
RDB$PAGE_TYPE,INDEX_BUCKET,7,1
RDB$PAGE_TYPE,BLOB,8,1
RDB$PAGE_TYPE,GENERATOR,9,1
RDB$PAGE_TYPE,SCN_INVENTORY,10,1
RDB$PRIVATE_FLAG,PUBLIC,0,1
RDB$PRIVATE_FLAG,PRIVATE,1,1
RDB$LEGACY_FLAG,NEW_STYLE,0,1
RDB$LEGACY_FLAG,LEGACY_STYLE,1,1
RDB$DETERMINISTIC_FLAG,NON_DETERMINISTIC,0,1
RDB$DETERMINISTIC_FLAG,DETERMINISTIC,1,1
RDB$MAP_TO_TYPE,USER,0,1
RDB$MAP_TO_TYPE,ROLE,1,1
MON$CRYPT_STATE,NOT ENCRYPTED,0,1
MON$CRYPT_STATE,ENCRYPTED,1,1
MON$CRYPT_STATE,DECRYPT IN PROGRESS,2,1
MON$CRYPT_STATE,ENCRYPT IN PROGRESS,3,1
MON$REPLICA_MODE,NONE,0,1
MON$REPLICA_MODE,READ-ONLY,1,1
MON$REPLICA_MODE,READ-WRITE,2,1
RDB$SYSTEM_PRIVILEGES,USER_MANAGEMENT,1,1
RDB$SYSTEM_PRIVILEGES,READ_RAW_PAGES,2,1
RDB$SYSTEM_PRIVILEGES,CREATE_USER_TYPES,3,1
RDB$SYSTEM_PRIVILEGES,USE_NBACKUP_UTILITY,4,1
RDB$SYSTEM_PRIVILEGES,CHANGE_SHUTDOWN_MODE,5,1
RDB$SYSTEM_PRIVILEGES,TRACE_ANY_ATTACHMENT,6,1
RDB$SYSTEM_PRIVILEGES,MONITOR_ANY_ATTACHMENT,7,1
RDB$SYSTEM_PRIVILEGES,ACCESS_SHUTDOWN_DATABASE,8,1
RDB$SYSTEM_PRIVILEGES,CREATE_DATABASE,9,1
RDB$SYSTEM_PRIVILEGES,DROP_DATABASE,10,1
RDB$SYSTEM_PRIVILEGES,USE_GBAK_UTILITY,11,1
RDB$SYSTEM_PRIVILEGES,USE_GSTAT_UTILITY,12,1
RDB$SYSTEM_PRIVILEGES,USE_GFIX_UTILITY,13,1
RDB$SYSTEM_PRIVILEGES,IGNORE_DB_TRIGGERS,14,1
RDB$SYSTEM_PRIVILEGES,CHANGE_HEADER_SETTINGS,15,1
RDB$SYSTEM_PRIVILEGES,SELECT_ANY_OBJECT_IN_DATABASE,16,1
RDB$SYSTEM_PRIVILEGES,ACCESS_ANY_OBJECT_IN_DATABASE,17,1
RDB$SYSTEM_PRIVILEGES,MODIFY_ANY_OBJECT_IN_DATABASE,18,1
RDB$SYSTEM_PRIVILEGES,CHANGE_MAPPING_RULES,19,1
RDB$SYSTEM_PRIVILEGES,USE_GRANTED_BY_CLAUSE,20,1
RDB$SYSTEM_PRIVILEGES,GRANT_REVOKE_ON_ANY_OBJECT,21,1
RDB$SYSTEM_PRIVILEGES,GRANT_REVOKE_ANY_DDL_RIGHT,22,1
RDB$SYSTEM_PRIVILEGES,CREATE_PRIVILEGED_ROLES,23,1
RDB$SYSTEM_PRIVILEGES,GET_DBCRYPT_INFO,24,1
RDB$SYSTEM_PRIVILEGES,MODIFY_EXT_CONN_POOL,25,1
RDB$SYSTEM_PRIVILEGES,REPLICATE_INTO_DATABASE,26,1
RDB$SYSTEM_PRIVILEGES,PROFILE_ANY_ATTACHMENT,27,1
RDB$CHARACTER_SET_NAME,NONE,0,1
RDB$CHARACTER_SET_NAME,OCTETS,1,1
RDB$CHARACTER_SET_NAME,ASCII,2,1
RDB$CHARACTER_SET_NAME,UNICODE_FSS,3,1
RDB$CHARACTER_SET_NAME,UTF8,4,1
RDB$CHARACTER_SET_NAME,SJIS_0208,5,1
RDB$CHARACTER_SET_NAME,EUCJ_0208,6,1
RDB$CHARACTER_SET_NAME,DOS437,10,1
RDB$CHARACTER_SET_NAME,DOS850,11,1
RDB$CHARACTER_SET_NAME,DOS865,12,1
RDB$CHARACTER_SET_NAME,ISO8859_1,21,1
RDB$CHARACTER_SET_NAME,ISO8859_2,22,1
RDB$CHARACTER_SET_NAME,ISO8859_3,23,1
RDB$CHARACTER_SET_NAME,ISO8859_4,34,1
RDB$CHARACTER_SET_NAME,ISO8859_5,35,1
RDB$CHARACTER_SET_NAME,ISO8859_6,36,1
RDB$CHARACTER_SET_NAME,ISO8859_7,37,1
RDB$CHARACTER_SET_NAME,ISO8859_8,38,1
RDB$CHARACTER_SET_NAME,ISO8859_9,39,1
RDB$CHARACTER_SET_NAME,ISO8859_13,40,1
RDB$CHARACTER_SET_NAME,DOS852,45,1
RDB$CHARACTER_SET_NAME,DOS857,46,1
RDB$CHARACTER_SET_NAME,DOS860,13,1
RDB$CHARACTER_SET_NAME,DOS861,47,1
RDB$CHARACTER_SET_NAME,DOS863,14,1
RDB$CHARACTER_SET_NAME,CYRL,50,1
RDB$CHARACTER_SET_NAME,DOS737,9,1
RDB$CHARACTER_SET_NAME,DOS775,15,1
RDB$CHARACTER_SET_NAME,DOS858,16,1
RDB$CHARACTER_SET_NAME,DOS862,17,1
RDB$CHARACTER_SET_NAME,DOS864,18,1
RDB$CHARACTER_SET_NAME,DOS866,48,1
RDB$CHARACTER_SET_NAME,DOS869,49,1
RDB$CHARACTER_SET_NAME,WIN1250,51,1
RDB$CHARACTER_SET_NAME,WIN1251,52,1
RDB$CHARACTER_SET_NAME,WIN1252,53,1
RDB$CHARACTER_SET_NAME,WIN1253,54,1
RDB$CHARACTER_SET_NAME,WIN1254,55,1
RDB$CHARACTER_SET_NAME,NEXT,19,1
RDB$CHARACTER_SET_NAME,WIN1255,58,1
RDB$CHARACTER_SET_NAME,WIN1256,59,1
RDB$CHARACTER_SET_NAME,WIN1257,60,1
RDB$CHARACTER_SET_NAME,KSC_5601,44,1
RDB$CHARACTER_SET_NAME,BIG_5,56,1
RDB$CHARACTER_SET_NAME,GB_2312,57,1
RDB$CHARACTER_SET_NAME,KOI8R,63,1
RDB$CHARACTER_SET_NAME,KOI8U,64,1
RDB$CHARACTER_SET_NAME,WIN1258,65,1
RDB$CHARACTER_SET_NAME,TIS620,66,1
RDB$CHARACTER_SET_NAME,GBK,67,1
RDB$CHARACTER_SET_NAME,CP943C,68,1
RDB$CHARACTER_SET_NAME,GB18030,69,1
RDB$CHARACTER_SET_NAME,BINARY,1,1
RDB$CHARACTER_SET_NAME,USASCII,2,1
RDB$CHARACTER_SET_NAME,ASCII7,2,1
RDB$CHARACTER_SET_NAME,UTF_FSS,3,1
RDB$CHARACTER_SET_NAME,SQL_TEXT,3,1
RDB$CHARACTER_SET_NAME,UTF-8,4,1
RDB$CHARACTER_SET_NAME,SJIS,5,1
RDB$CHARACTER_SET_NAME,EUCJ,6,1
RDB$CHARACTER_SET_NAME,DOS_437,10,1
RDB$CHARACTER_SET_NAME,DOS_850,11,1
RDB$CHARACTER_SET_NAME,DOS_865,12,1
RDB$CHARACTER_SET_NAME,ISO88591,21,1
RDB$CHARACTER_SET_NAME,LATIN1,21,1
RDB$CHARACTER_SET_NAME,ANSI,21,1
RDB$CHARACTER_SET_NAME,ISO88592,22,1
RDB$CHARACTER_SET_NAME,LATIN2,22,1
RDB$CHARACTER_SET_NAME,ISO-8859-2,22,1
RDB$CHARACTER_SET_NAME,ISO88593,23,1
RDB$CHARACTER_SET_NAME,LATIN3,23,1
RDB$CHARACTER_SET_NAME,ISO-8859-3,23,1
RDB$CHARACTER_SET_NAME,ISO88594,34,1
RDB$CHARACTER_SET_NAME,LATIN4,34,1
RDB$CHARACTER_SET_NAME,ISO-8859-4,34,1
RDB$CHARACTER_SET_NAME,ISO88595,35,1
RDB$CHARACTER_SET_NAME,ISO-8859-5,35,1
RDB$CHARACTER_SET_NAME,ISO88596,36,1
RDB$CHARACTER_SET_NAME,ISO-8859-6,36,1
RDB$CHARACTER_SET_NAME,ISO88597,37,1
RDB$CHARACTER_SET_NAME,ISO-8859-7,37,1
RDB$CHARACTER_SET_NAME,ISO88598,38,1
RDB$CHARACTER_SET_NAME,ISO-8859-8,38,1
RDB$CHARACTER_SET_NAME,ISO88599,39,1
RDB$CHARACTER_SET_NAME,LATIN5,39,1
RDB$CHARACTER_SET_NAME,ISO-8859-9,39,1
RDB$CHARACTER_SET_NAME,ISO885913,40,1
RDB$CHARACTER_SET_NAME,LATIN7,40,1
RDB$CHARACTER_SET_NAME,ISO-8859-13,40,1
RDB$CHARACTER_SET_NAME,DOS_852,45,1
RDB$CHARACTER_SET_NAME,DOS_857,46,1
RDB$CHARACTER_SET_NAME,DOS_860,13,1
RDB$CHARACTER_SET_NAME,DOS_861,47,1
RDB$CHARACTER_SET_NAME,DOS_863,14,1
RDB$CHARACTER_SET_NAME,DOS_737,9,1
RDB$CHARACTER_SET_NAME,DOS_775,15,1
RDB$CHARACTER_SET_NAME,DOS_858,16,1
RDB$CHARACTER_SET_NAME,DOS_862,17,1
RDB$CHARACTER_SET_NAME,DOS_864,18,1
RDB$CHARACTER_SET_NAME,DOS_866,48,1
RDB$CHARACTER_SET_NAME,DOS_869,49,1
RDB$CHARACTER_SET_NAME,WIN_1250,51,1
RDB$CHARACTER_SET_NAME,WIN_1251,52,1
RDB$CHARACTER_SET_NAME,WIN_1252,53,1
RDB$CHARACTER_SET_NAME,WIN_1253,54,1
RDB$CHARACTER_SET_NAME,WIN_1254,55,1
RDB$CHARACTER_SET_NAME,WIN_1255,58,1
RDB$CHARACTER_SET_NAME,WIN_1256,59,1
RDB$CHARACTER_SET_NAME,WIN_1257,60,1
RDB$CHARACTER_SET_NAME,WIN_1258,65,1
RDB$CHARACTER_SET_NAME,KSC5601,44,1
RDB$CHARACTER_SET_NAME,DOS_949,44,1
RDB$CHARACTER_SET_NAME,WIN_949,44,1
RDB$CHARACTER_SET_NAME,BIG5,56,1
RDB$CHARACTER_SET_NAME,DOS_950,56,1
RDB$CHARACTER_SET_NAME,WIN_950,56,1
RDB$CHARACTER_SET_NAME,GB2312,57,1
RDB$CHARACTER_SET_NAME,DOS_936,57,1
RDB$CHARACTER_SET_NAME,WIN_936,57,1
```

#### RDB$SECURITY_CLASSES (Default Rows)
Rows must be inserted in the exact order listed. `RDB$ACL` is a binary blob constructed from the referenced ACL template in `appendix_security_classes.md`.

```csv
RDB$SECURITY_CLASS,RDB$ACL_TEMPLATE,APPLIES_TO
SQL$1,RELATION_ACL,RDB$RELATIONS.RDB$PAGES.RDB$SECURITY_CLASS
SQL$DEFAULT1,RELATION_ACL,RDB$RELATIONS.RDB$PAGES.RDB$DEFAULT_CLASS
SQL$2,RELATION_ACL,RDB$RELATIONS.RDB$DATABASE.RDB$SECURITY_CLASS
SQL$DEFAULT2,RELATION_ACL,RDB$RELATIONS.RDB$DATABASE.RDB$DEFAULT_CLASS
SQL$3,RELATION_ACL,RDB$RELATIONS.RDB$FIELDS.RDB$SECURITY_CLASS
SQL$DEFAULT3,RELATION_ACL,RDB$RELATIONS.RDB$FIELDS.RDB$DEFAULT_CLASS
SQL$4,RELATION_ACL,RDB$RELATIONS.RDB$INDEX_SEGMENTS.RDB$SECURITY_CLASS
SQL$DEFAULT4,RELATION_ACL,RDB$RELATIONS.RDB$INDEX_SEGMENTS.RDB$DEFAULT_CLASS
SQL$5,RELATION_ACL,RDB$RELATIONS.RDB$INDICES.RDB$SECURITY_CLASS
SQL$DEFAULT5,RELATION_ACL,RDB$RELATIONS.RDB$INDICES.RDB$DEFAULT_CLASS
SQL$6,RELATION_ACL,RDB$RELATIONS.RDB$RELATION_FIELDS.RDB$SECURITY_CLASS
SQL$DEFAULT6,RELATION_ACL,RDB$RELATIONS.RDB$RELATION_FIELDS.RDB$DEFAULT_CLASS
SQL$7,RELATION_ACL,RDB$RELATIONS.RDB$RELATIONS.RDB$SECURITY_CLASS
SQL$DEFAULT7,RELATION_ACL,RDB$RELATIONS.RDB$RELATIONS.RDB$DEFAULT_CLASS
SQL$8,RELATION_ACL,RDB$RELATIONS.RDB$VIEW_RELATIONS.RDB$SECURITY_CLASS
SQL$DEFAULT8,RELATION_ACL,RDB$RELATIONS.RDB$VIEW_RELATIONS.RDB$DEFAULT_CLASS
SQL$9,RELATION_ACL,RDB$RELATIONS.RDB$FORMATS.RDB$SECURITY_CLASS
SQL$DEFAULT9,RELATION_ACL,RDB$RELATIONS.RDB$FORMATS.RDB$DEFAULT_CLASS
SQL$10,RELATION_ACL,RDB$RELATIONS.RDB$SECURITY_CLASSES.RDB$SECURITY_CLASS
SQL$DEFAULT10,RELATION_ACL,RDB$RELATIONS.RDB$SECURITY_CLASSES.RDB$DEFAULT_CLASS
SQL$11,RELATION_ACL,RDB$RELATIONS.RDB$FILES.RDB$SECURITY_CLASS
SQL$DEFAULT11,RELATION_ACL,RDB$RELATIONS.RDB$FILES.RDB$DEFAULT_CLASS
SQL$12,RELATION_ACL,RDB$RELATIONS.RDB$TYPES.RDB$SECURITY_CLASS
SQL$DEFAULT12,RELATION_ACL,RDB$RELATIONS.RDB$TYPES.RDB$DEFAULT_CLASS
SQL$13,RELATION_ACL,RDB$RELATIONS.RDB$TRIGGERS.RDB$SECURITY_CLASS
SQL$DEFAULT13,RELATION_ACL,RDB$RELATIONS.RDB$TRIGGERS.RDB$DEFAULT_CLASS
SQL$14,RELATION_ACL,RDB$RELATIONS.RDB$DEPENDENCIES.RDB$SECURITY_CLASS
SQL$DEFAULT14,RELATION_ACL,RDB$RELATIONS.RDB$DEPENDENCIES.RDB$DEFAULT_CLASS
SQL$15,RELATION_ACL,RDB$RELATIONS.RDB$FUNCTIONS.RDB$SECURITY_CLASS
SQL$DEFAULT15,RELATION_ACL,RDB$RELATIONS.RDB$FUNCTIONS.RDB$DEFAULT_CLASS
SQL$16,RELATION_ACL,RDB$RELATIONS.RDB$FUNCTION_ARGUMENTS.RDB$SECURITY_CLASS
SQL$DEFAULT16,RELATION_ACL,RDB$RELATIONS.RDB$FUNCTION_ARGUMENTS.RDB$DEFAULT_CLASS
SQL$17,RELATION_ACL,RDB$RELATIONS.RDB$FILTERS.RDB$SECURITY_CLASS
SQL$DEFAULT17,RELATION_ACL,RDB$RELATIONS.RDB$FILTERS.RDB$DEFAULT_CLASS
SQL$18,RELATION_ACL,RDB$RELATIONS.RDB$TRIGGER_MESSAGES.RDB$SECURITY_CLASS
SQL$DEFAULT18,RELATION_ACL,RDB$RELATIONS.RDB$TRIGGER_MESSAGES.RDB$DEFAULT_CLASS
SQL$19,RELATION_ACL,RDB$RELATIONS.RDB$USER_PRIVILEGES.RDB$SECURITY_CLASS
SQL$DEFAULT19,RELATION_ACL,RDB$RELATIONS.RDB$USER_PRIVILEGES.RDB$DEFAULT_CLASS
SQL$20,RELATION_ACL,RDB$RELATIONS.RDB$TRANSACTIONS.RDB$SECURITY_CLASS
SQL$DEFAULT20,RELATION_ACL,RDB$RELATIONS.RDB$TRANSACTIONS.RDB$DEFAULT_CLASS
SQL$21,RELATION_ACL,RDB$RELATIONS.RDB$GENERATORS.RDB$SECURITY_CLASS
SQL$DEFAULT21,RELATION_ACL,RDB$RELATIONS.RDB$GENERATORS.RDB$DEFAULT_CLASS
SQL$22,RELATION_ACL,RDB$RELATIONS.RDB$FIELD_DIMENSIONS.RDB$SECURITY_CLASS
SQL$DEFAULT22,RELATION_ACL,RDB$RELATIONS.RDB$FIELD_DIMENSIONS.RDB$DEFAULT_CLASS
SQL$23,RELATION_ACL,RDB$RELATIONS.RDB$RELATION_CONSTRAINTS.RDB$SECURITY_CLASS
SQL$DEFAULT23,RELATION_ACL,RDB$RELATIONS.RDB$RELATION_CONSTRAINTS.RDB$DEFAULT_CLASS
SQL$24,RELATION_ACL,RDB$RELATIONS.RDB$REF_CONSTRAINTS.RDB$SECURITY_CLASS
SQL$DEFAULT24,RELATION_ACL,RDB$RELATIONS.RDB$REF_CONSTRAINTS.RDB$DEFAULT_CLASS
SQL$25,RELATION_ACL,RDB$RELATIONS.RDB$CHECK_CONSTRAINTS.RDB$SECURITY_CLASS
SQL$DEFAULT25,RELATION_ACL,RDB$RELATIONS.RDB$CHECK_CONSTRAINTS.RDB$DEFAULT_CLASS
SQL$26,RELATION_ACL,RDB$RELATIONS.RDB$LOG_FILES.RDB$SECURITY_CLASS
SQL$DEFAULT26,RELATION_ACL,RDB$RELATIONS.RDB$LOG_FILES.RDB$DEFAULT_CLASS
SQL$27,RELATION_ACL,RDB$RELATIONS.RDB$PROCEDURES.RDB$SECURITY_CLASS
SQL$DEFAULT27,RELATION_ACL,RDB$RELATIONS.RDB$PROCEDURES.RDB$DEFAULT_CLASS
SQL$28,RELATION_ACL,RDB$RELATIONS.RDB$PROCEDURE_PARAMETERS.RDB$SECURITY_CLASS
SQL$DEFAULT28,RELATION_ACL,RDB$RELATIONS.RDB$PROCEDURE_PARAMETERS.RDB$DEFAULT_CLASS
SQL$29,RELATION_ACL,RDB$RELATIONS.RDB$CHARACTER_SETS.RDB$SECURITY_CLASS
SQL$DEFAULT29,RELATION_ACL,RDB$RELATIONS.RDB$CHARACTER_SETS.RDB$DEFAULT_CLASS
SQL$30,RELATION_ACL,RDB$RELATIONS.RDB$COLLATIONS.RDB$SECURITY_CLASS
SQL$DEFAULT30,RELATION_ACL,RDB$RELATIONS.RDB$COLLATIONS.RDB$DEFAULT_CLASS
SQL$31,RELATION_ACL,RDB$RELATIONS.RDB$EXCEPTIONS.RDB$SECURITY_CLASS
SQL$DEFAULT31,RELATION_ACL,RDB$RELATIONS.RDB$EXCEPTIONS.RDB$DEFAULT_CLASS
SQL$32,RELATION_ACL,RDB$RELATIONS.RDB$ROLES.RDB$SECURITY_CLASS
SQL$DEFAULT32,RELATION_ACL,RDB$RELATIONS.RDB$ROLES.RDB$DEFAULT_CLASS
SQL$33,RELATION_ACL,RDB$RELATIONS.RDB$BACKUP_HISTORY.RDB$SECURITY_CLASS
SQL$DEFAULT33,RELATION_ACL,RDB$RELATIONS.RDB$BACKUP_HISTORY.RDB$DEFAULT_CLASS
SQL$34,RELATION_ACL,RDB$RELATIONS.MON$DATABASE.RDB$SECURITY_CLASS
SQL$DEFAULT34,RELATION_ACL,RDB$RELATIONS.MON$DATABASE.RDB$DEFAULT_CLASS
SQL$35,RELATION_ACL,RDB$RELATIONS.MON$ATTACHMENTS.RDB$SECURITY_CLASS
SQL$DEFAULT35,RELATION_ACL,RDB$RELATIONS.MON$ATTACHMENTS.RDB$DEFAULT_CLASS
SQL$36,RELATION_ACL,RDB$RELATIONS.MON$TRANSACTIONS.RDB$SECURITY_CLASS
SQL$DEFAULT36,RELATION_ACL,RDB$RELATIONS.MON$TRANSACTIONS.RDB$DEFAULT_CLASS
SQL$37,RELATION_ACL,RDB$RELATIONS.MON$STATEMENTS.RDB$SECURITY_CLASS
SQL$DEFAULT37,RELATION_ACL,RDB$RELATIONS.MON$STATEMENTS.RDB$DEFAULT_CLASS
SQL$38,RELATION_ACL,RDB$RELATIONS.MON$CALL_STACK.RDB$SECURITY_CLASS
SQL$DEFAULT38,RELATION_ACL,RDB$RELATIONS.MON$CALL_STACK.RDB$DEFAULT_CLASS
SQL$39,RELATION_ACL,RDB$RELATIONS.MON$IO_STATS.RDB$SECURITY_CLASS
SQL$DEFAULT39,RELATION_ACL,RDB$RELATIONS.MON$IO_STATS.RDB$DEFAULT_CLASS
SQL$40,RELATION_ACL,RDB$RELATIONS.MON$RECORD_STATS.RDB$SECURITY_CLASS
SQL$DEFAULT40,RELATION_ACL,RDB$RELATIONS.MON$RECORD_STATS.RDB$DEFAULT_CLASS
SQL$41,RELATION_ACL,RDB$RELATIONS.MON$CONTEXT_VARIABLES.RDB$SECURITY_CLASS
SQL$DEFAULT41,RELATION_ACL,RDB$RELATIONS.MON$CONTEXT_VARIABLES.RDB$DEFAULT_CLASS
SQL$42,RELATION_ACL,RDB$RELATIONS.MON$MEMORY_USAGE.RDB$SECURITY_CLASS
SQL$DEFAULT42,RELATION_ACL,RDB$RELATIONS.MON$MEMORY_USAGE.RDB$DEFAULT_CLASS
SQL$43,RELATION_ACL,RDB$RELATIONS.RDB$PACKAGES.RDB$SECURITY_CLASS
SQL$DEFAULT43,RELATION_ACL,RDB$RELATIONS.RDB$PACKAGES.RDB$DEFAULT_CLASS
SQL$44,RELATION_ACL,RDB$RELATIONS.SEC$USERS.RDB$SECURITY_CLASS
SQL$DEFAULT44,RELATION_ACL,RDB$RELATIONS.SEC$USERS.RDB$DEFAULT_CLASS
SQL$45,RELATION_ACL,RDB$RELATIONS.SEC$USER_ATTRIBUTES.RDB$SECURITY_CLASS
SQL$DEFAULT45,RELATION_ACL,RDB$RELATIONS.SEC$USER_ATTRIBUTES.RDB$DEFAULT_CLASS
SQL$46,RELATION_ACL,RDB$RELATIONS.RDB$AUTH_MAPPING.RDB$SECURITY_CLASS
SQL$DEFAULT46,RELATION_ACL,RDB$RELATIONS.RDB$AUTH_MAPPING.RDB$DEFAULT_CLASS
SQL$47,RELATION_ACL,RDB$RELATIONS.SEC$GLOBAL_AUTH_MAPPING.RDB$SECURITY_CLASS
SQL$DEFAULT47,RELATION_ACL,RDB$RELATIONS.SEC$GLOBAL_AUTH_MAPPING.RDB$DEFAULT_CLASS
SQL$48,RELATION_ACL,RDB$RELATIONS.RDB$DB_CREATORS.RDB$SECURITY_CLASS
SQL$DEFAULT48,RELATION_ACL,RDB$RELATIONS.RDB$DB_CREATORS.RDB$DEFAULT_CLASS
SQL$49,RELATION_ACL,RDB$RELATIONS.SEC$DB_CREATORS.RDB$SECURITY_CLASS
SQL$DEFAULT49,RELATION_ACL,RDB$RELATIONS.SEC$DB_CREATORS.RDB$DEFAULT_CLASS
SQL$50,RELATION_ACL,RDB$RELATIONS.MON$TABLE_STATS.RDB$SECURITY_CLASS
SQL$DEFAULT50,RELATION_ACL,RDB$RELATIONS.MON$TABLE_STATS.RDB$DEFAULT_CLASS
SQL$51,RELATION_ACL,RDB$RELATIONS.RDB$TIME_ZONES.RDB$SECURITY_CLASS
SQL$DEFAULT51,RELATION_ACL,RDB$RELATIONS.RDB$TIME_ZONES.RDB$DEFAULT_CLASS
SQL$52,RELATION_ACL,RDB$RELATIONS.RDB$PUBLICATIONS.RDB$SECURITY_CLASS
SQL$DEFAULT52,RELATION_ACL,RDB$RELATIONS.RDB$PUBLICATIONS.RDB$DEFAULT_CLASS
SQL$53,RELATION_ACL,RDB$RELATIONS.RDB$PUBLICATION_TABLES.RDB$SECURITY_CLASS
SQL$DEFAULT53,RELATION_ACL,RDB$RELATIONS.RDB$PUBLICATION_TABLES.RDB$DEFAULT_CLASS
SQL$54,RELATION_ACL,RDB$RELATIONS.RDB$CONFIG.RDB$SECURITY_CLASS
SQL$DEFAULT54,RELATION_ACL,RDB$RELATIONS.RDB$CONFIG.RDB$DEFAULT_CLASS
SQL$55,RELATION_ACL,RDB$RELATIONS.RDB$KEYWORDS.RDB$SECURITY_CLASS
SQL$DEFAULT55,RELATION_ACL,RDB$RELATIONS.RDB$KEYWORDS.RDB$DEFAULT_CLASS
SQL$56,RELATION_ACL,RDB$RELATIONS.MON$COMPILED_STATEMENTS.RDB$SECURITY_CLASS
SQL$DEFAULT56,RELATION_ACL,RDB$RELATIONS.MON$COMPILED_STATEMENTS.RDB$DEFAULT_CLASS
SQL$57,RELATION_ACL,RDB$RELATIONS.RDB$SCHEMAS.RDB$SECURITY_CLASS
SQL$DEFAULT57,RELATION_ACL,RDB$RELATIONS.RDB$SCHEMAS.RDB$DEFAULT_CLASS
SQL$58,RELATION_ACL,RDB$RELATIONS.MON$LOCAL_TEMPORARY_TABLES.RDB$SECURITY_CLASS
SQL$DEFAULT58,RELATION_ACL,RDB$RELATIONS.MON$LOCAL_TEMPORARY_TABLES.RDB$DEFAULT_CLASS
SQL$59,RELATION_ACL,RDB$RELATIONS.MON$LOCAL_TEMPORARY_TABLE_COLUMNS.RDB$SECURITY_CLASS
SQL$DEFAULT59,RELATION_ACL,RDB$RELATIONS.MON$LOCAL_TEMPORARY_TABLE_COLUMNS.RDB$DEFAULT_CLASS
SQL$60,NONREL_USAGE_ACL,RDB$FIELDS.RDB$VIEW_CONTEXT.RDB$SECURITY_CLASS
SQL$61,NONREL_USAGE_ACL,RDB$FIELDS.RDB$CONTEXT_NAME.RDB$SECURITY_CLASS
SQL$62,NONREL_USAGE_ACL,RDB$FIELDS.RDB$DESCRIPTION.RDB$SECURITY_CLASS
SQL$63,NONREL_USAGE_ACL,RDB$FIELDS.RDB$EDIT_STRING.RDB$SECURITY_CLASS
SQL$64,NONREL_USAGE_ACL,RDB$FIELDS.RDB$FIELD_ID.RDB$SECURITY_CLASS
SQL$65,NONREL_USAGE_ACL,RDB$FIELDS.RDB$FIELD_NAME.RDB$SECURITY_CLASS
SQL$66,NONREL_USAGE_ACL,RDB$FIELDS.RDB$SYSTEM_FLAG.RDB$SECURITY_CLASS
SQL$67,NONREL_USAGE_ACL,RDB$FIELDS.RDB$SYSTEM_NULLFLAG.RDB$SECURITY_CLASS
SQL$68,NONREL_USAGE_ACL,RDB$FIELDS.RDB$INDEX_ID.RDB$SECURITY_CLASS
SQL$69,NONREL_USAGE_ACL,RDB$FIELDS.RDB$INDEX_NAME.RDB$SECURITY_CLASS
SQL$70,NONREL_USAGE_ACL,RDB$FIELDS.RDB$FIELD_LENGTH.RDB$SECURITY_CLASS
SQL$71,NONREL_USAGE_ACL,RDB$FIELDS.RDB$FIELD_POSITION.RDB$SECURITY_CLASS
SQL$72,NONREL_USAGE_ACL,RDB$FIELDS.RDB$FIELD_SCALE.RDB$SECURITY_CLASS
SQL$73,NONREL_USAGE_ACL,RDB$FIELDS.RDB$FIELD_TYPE.RDB$SECURITY_CLASS
SQL$74,NONREL_USAGE_ACL,RDB$FIELDS.RDB$FORMAT.RDB$SECURITY_CLASS
SQL$75,NONREL_USAGE_ACL,RDB$FIELDS.RDB$DBKEY_LENGTH.RDB$SECURITY_CLASS
SQL$76,NONREL_USAGE_ACL,RDB$FIELDS.RDB$PAGE_NUMBER.RDB$SECURITY_CLASS
SQL$77,NONREL_USAGE_ACL,RDB$FIELDS.RDB$PAGE_SEQUENCE.RDB$SECURITY_CLASS
SQL$78,NONREL_USAGE_ACL,RDB$FIELDS.RDB$PAGE_TYPE.RDB$SECURITY_CLASS
SQL$79,NONREL_USAGE_ACL,RDB$FIELDS.RDB$QUERY_HEADER.RDB$SECURITY_CLASS
SQL$80,NONREL_USAGE_ACL,RDB$FIELDS.RDB$RELATION_ID.RDB$SECURITY_CLASS
SQL$81,NONREL_USAGE_ACL,RDB$FIELDS.RDB$RELATION_NAME.RDB$SECURITY_CLASS
SQL$82,NONREL_USAGE_ACL,RDB$FIELDS.RDB$SEGMENT_COUNT.RDB$SECURITY_CLASS
SQL$83,NONREL_USAGE_ACL,RDB$FIELDS.RDB$SEGMENT_LENGTH.RDB$SECURITY_CLASS
SQL$84,NONREL_USAGE_ACL,RDB$FIELDS.RDB$SOURCE.RDB$SECURITY_CLASS
SQL$85,NONREL_USAGE_ACL,RDB$FIELDS.RDB$FIELD_SUB_TYPE.RDB$SECURITY_CLASS
SQL$86,NONREL_USAGE_ACL,RDB$FIELDS.RDB$VIEW_BLR.RDB$SECURITY_CLASS
SQL$87,NONREL_USAGE_ACL,RDB$FIELDS.RDB$VALIDATION_BLR.RDB$SECURITY_CLASS
SQL$88,NONREL_USAGE_ACL,RDB$FIELDS.RDB$VALUE.RDB$SECURITY_CLASS
SQL$89,NONREL_USAGE_ACL,RDB$FIELDS.RDB$SECURITY_CLASS.RDB$SECURITY_CLASS
SQL$90,NONREL_USAGE_ACL,RDB$FIELDS.RDB$ACL.RDB$SECURITY_CLASS
SQL$91,NONREL_USAGE_ACL,RDB$FIELDS.RDB$FILE_NAME.RDB$SECURITY_CLASS
SQL$92,NONREL_USAGE_ACL,RDB$FIELDS.RDB$FILE_NAME2.RDB$SECURITY_CLASS
SQL$93,NONREL_USAGE_ACL,RDB$FIELDS.RDB$FILE_SEQUENCE.RDB$SECURITY_CLASS
SQL$94,NONREL_USAGE_ACL,RDB$FIELDS.RDB$FILE_START.RDB$SECURITY_CLASS
SQL$95,NONREL_USAGE_ACL,RDB$FIELDS.RDB$FILE_LENGTH.RDB$SECURITY_CLASS
SQL$96,NONREL_USAGE_ACL,RDB$FIELDS.RDB$FILE_FLAGS.RDB$SECURITY_CLASS
SQL$97,NONREL_USAGE_ACL,RDB$FIELDS.RDB$TRIGGER_BLR.RDB$SECURITY_CLASS
SQL$98,NONREL_USAGE_ACL,RDB$FIELDS.RDB$TRIGGER_NAME.RDB$SECURITY_CLASS
SQL$99,NONREL_USAGE_ACL,RDB$FIELDS.RDB$GENERIC_NAME.RDB$SECURITY_CLASS
SQL$100,NONREL_USAGE_ACL,RDB$FIELDS.RDB$FUNCTION_NAME.RDB$SECURITY_CLASS
SQL$101,NONREL_USAGE_ACL,RDB$FIELDS.RDB$EXTERNAL_NAME.RDB$SECURITY_CLASS
SQL$102,NONREL_USAGE_ACL,RDB$FIELDS.RDB$TYPE_NAME.RDB$SECURITY_CLASS
SQL$103,NONREL_USAGE_ACL,RDB$FIELDS.RDB$DIMENSIONS.RDB$SECURITY_CLASS
SQL$104,NONREL_USAGE_ACL,RDB$FIELDS.RDB$RUNTIME.RDB$SECURITY_CLASS
SQL$105,NONREL_USAGE_ACL,RDB$FIELDS.RDB$TRIGGER_SEQUENCE.RDB$SECURITY_CLASS
SQL$106,NONREL_USAGE_ACL,RDB$FIELDS.RDB$GENERIC_TYPE.RDB$SECURITY_CLASS
SQL$107,NONREL_USAGE_ACL,RDB$FIELDS.RDB$TRIGGER_TYPE.RDB$SECURITY_CLASS
SQL$108,NONREL_USAGE_ACL,RDB$FIELDS.RDB$OBJECT_TYPE.RDB$SECURITY_CLASS
SQL$109,NONREL_USAGE_ACL,RDB$FIELDS.RDB$MECHANISM.RDB$SECURITY_CLASS
SQL$110,NONREL_USAGE_ACL,RDB$FIELDS.RDB$DESCRIPTOR.RDB$SECURITY_CLASS
SQL$111,NONREL_USAGE_ACL,RDB$FIELDS.RDB$FUNCTION_TYPE.RDB$SECURITY_CLASS
SQL$112,NONREL_USAGE_ACL,RDB$FIELDS.RDB$TRANSACTION_ID.RDB$SECURITY_CLASS
SQL$113,NONREL_USAGE_ACL,RDB$FIELDS.RDB$TRANSACTION_STATE.RDB$SECURITY_CLASS
SQL$114,NONREL_USAGE_ACL,RDB$FIELDS.RDB$TIMESTAMP.RDB$SECURITY_CLASS
SQL$115,NONREL_USAGE_ACL,RDB$FIELDS.RDB$TRANSACTION_DESCRIPTION.RDB$SECURITY_CLASS
SQL$116,NONREL_USAGE_ACL,RDB$FIELDS.RDB$MESSAGE.RDB$SECURITY_CLASS
SQL$117,NONREL_USAGE_ACL,RDB$FIELDS.RDB$MESSAGE_NUMBER.RDB$SECURITY_CLASS
SQL$118,NONREL_USAGE_ACL,RDB$FIELDS.RDB$USER.RDB$SECURITY_CLASS
SQL$119,NONREL_USAGE_ACL,RDB$FIELDS.RDB$PRIVILEGE.RDB$SECURITY_CLASS
SQL$120,NONREL_USAGE_ACL,RDB$FIELDS.RDB$EXTERNAL_DESCRIPTION.RDB$SECURITY_CLASS
SQL$121,NONREL_USAGE_ACL,RDB$FIELDS.RDB$SHADOW_NUMBER.RDB$SECURITY_CLASS
SQL$122,NONREL_USAGE_ACL,RDB$FIELDS.RDB$GENERATOR_NAME.RDB$SECURITY_CLASS
SQL$123,NONREL_USAGE_ACL,RDB$FIELDS.RDB$GENERATOR_ID.RDB$SECURITY_CLASS
SQL$124,NONREL_USAGE_ACL,RDB$FIELDS.RDB$BOUND.RDB$SECURITY_CLASS
SQL$125,NONREL_USAGE_ACL,RDB$FIELDS.RDB$DIMENSION.RDB$SECURITY_CLASS
SQL$126,NONREL_USAGE_ACL,RDB$FIELDS.RDB$STATISTICS.RDB$SECURITY_CLASS
SQL$127,NONREL_USAGE_ACL,RDB$FIELDS.RDB$NULL_FLAG.RDB$SECURITY_CLASS
SQL$128,NONREL_USAGE_ACL,RDB$FIELDS.RDB$CONSTRAINT_NAME.RDB$SECURITY_CLASS
SQL$129,NONREL_USAGE_ACL,RDB$FIELDS.RDB$CONSTRAINT_TYPE.RDB$SECURITY_CLASS
SQL$130,NONREL_USAGE_ACL,RDB$FIELDS.RDB$DEFERRABLE.RDB$SECURITY_CLASS
SQL$131,NONREL_USAGE_ACL,RDB$FIELDS.RDB$MATCH_OPTION.RDB$SECURITY_CLASS
SQL$132,NONREL_USAGE_ACL,RDB$FIELDS.RDB$RULE.RDB$SECURITY_CLASS
SQL$133,NONREL_USAGE_ACL,RDB$FIELDS.RDB$FILE_PARTITIONS.RDB$SECURITY_CLASS
SQL$134,NONREL_USAGE_ACL,RDB$FIELDS.RDB$PROCEDURE_BLR.RDB$SECURITY_CLASS
SQL$135,NONREL_USAGE_ACL,RDB$FIELDS.RDB$PROCEDURE_ID.RDB$SECURITY_CLASS
SQL$136,NONREL_USAGE_ACL,RDB$FIELDS.RDB$PROCEDURE_PARAMETERS.RDB$SECURITY_CLASS
SQL$137,NONREL_USAGE_ACL,RDB$FIELDS.RDB$PROCEDURE_NAME.RDB$SECURITY_CLASS
SQL$138,NONREL_USAGE_ACL,RDB$FIELDS.RDB$PARAMETER_NAME.RDB$SECURITY_CLASS
SQL$139,NONREL_USAGE_ACL,RDB$FIELDS.RDB$PARAMETER_NUMBER.RDB$SECURITY_CLASS
SQL$140,NONREL_USAGE_ACL,RDB$FIELDS.RDB$PARAMETER_TYPE.RDB$SECURITY_CLASS
SQL$141,NONREL_USAGE_ACL,RDB$FIELDS.RDB$CHARACTER_SET_NAME.RDB$SECURITY_CLASS
SQL$142,NONREL_USAGE_ACL,RDB$FIELDS.RDB$CHARACTER_SET_ID.RDB$SECURITY_CLASS
SQL$143,NONREL_USAGE_ACL,RDB$FIELDS.RDB$COLLATION_NAME.RDB$SECURITY_CLASS
SQL$144,NONREL_USAGE_ACL,RDB$FIELDS.RDB$COLLATION_ID.RDB$SECURITY_CLASS
SQL$145,NONREL_USAGE_ACL,RDB$FIELDS.RDB$NUMBER_OF_CHARACTERS.RDB$SECURITY_CLASS
SQL$146,NONREL_USAGE_ACL,RDB$FIELDS.RDB$EXCEPTION_NAME.RDB$SECURITY_CLASS
SQL$147,NONREL_USAGE_ACL,RDB$FIELDS.RDB$EXCEPTION_NUMBER.RDB$SECURITY_CLASS
SQL$148,NONREL_USAGE_ACL,RDB$FIELDS.RDB$FILE_P_OFFSET.RDB$SECURITY_CLASS
SQL$149,NONREL_USAGE_ACL,RDB$FIELDS.RDB$FIELD_PRECISION.RDB$SECURITY_CLASS
SQL$150,NONREL_USAGE_ACL,RDB$FIELDS.RDB$BACKUP_ID.RDB$SECURITY_CLASS
SQL$151,NONREL_USAGE_ACL,RDB$FIELDS.RDB$BACKUP_LEVEL.RDB$SECURITY_CLASS
SQL$152,NONREL_USAGE_ACL,RDB$FIELDS.RDB$GUID.RDB$SECURITY_CLASS
SQL$153,NONREL_USAGE_ACL,RDB$FIELDS.RDB$SCN.RDB$SECURITY_CLASS
SQL$154,NONREL_USAGE_ACL,RDB$FIELDS.RDB$SPECIFIC_ATTRIBUTES.RDB$SECURITY_CLASS
SQL$155,NONREL_USAGE_ACL,RDB$FIELDS.RDB$PLUGIN.RDB$SECURITY_CLASS
SQL$156,NONREL_USAGE_ACL,RDB$FIELDS.RDB$RELATION_TYPE.RDB$SECURITY_CLASS
SQL$157,NONREL_USAGE_ACL,RDB$FIELDS.RDB$PROCEDURE_TYPE.RDB$SECURITY_CLASS
SQL$158,NONREL_USAGE_ACL,RDB$FIELDS.RDB$ATTACHMENT_ID.RDB$SECURITY_CLASS
SQL$159,NONREL_USAGE_ACL,RDB$FIELDS.RDB$STATEMENT_ID.RDB$SECURITY_CLASS
SQL$160,NONREL_USAGE_ACL,RDB$FIELDS.RDB$CALL_ID.RDB$SECURITY_CLASS
SQL$161,NONREL_USAGE_ACL,RDB$FIELDS.RDB$STAT_ID.RDB$SECURITY_CLASS
SQL$162,NONREL_USAGE_ACL,RDB$FIELDS.RDB$PID.RDB$SECURITY_CLASS
SQL$163,NONREL_USAGE_ACL,RDB$FIELDS.RDB$STATE.RDB$SECURITY_CLASS
SQL$164,NONREL_USAGE_ACL,RDB$FIELDS.RDB$ODS_NUMBER.RDB$SECURITY_CLASS
SQL$165,NONREL_USAGE_ACL,RDB$FIELDS.RDB$PAGE_SIZE.RDB$SECURITY_CLASS
SQL$166,NONREL_USAGE_ACL,RDB$FIELDS.RDB$PAGE_BUFFERS.RDB$SECURITY_CLASS
SQL$167,NONREL_USAGE_ACL,RDB$FIELDS.RDB$SHUTDOWN_MODE.RDB$SECURITY_CLASS
SQL$168,NONREL_USAGE_ACL,RDB$FIELDS.RDB$SQL_DIALECT.RDB$SECURITY_CLASS
SQL$169,NONREL_USAGE_ACL,RDB$FIELDS.RDB$SWEEP_INTERVAL.RDB$SECURITY_CLASS
SQL$170,NONREL_USAGE_ACL,RDB$FIELDS.RDB$COUNTER.RDB$SECURITY_CLASS
SQL$171,NONREL_USAGE_ACL,RDB$FIELDS.RDB$REMOTE_PROTOCOL.RDB$SECURITY_CLASS
SQL$172,NONREL_USAGE_ACL,RDB$FIELDS.RDB$REMOTE_ADDRESS.RDB$SECURITY_CLASS
SQL$173,NONREL_USAGE_ACL,RDB$FIELDS.RDB$ISOLATION_MODE.RDB$SECURITY_CLASS
SQL$174,NONREL_USAGE_ACL,RDB$FIELDS.RDB$LOCK_TIMEOUT.RDB$SECURITY_CLASS
SQL$175,NONREL_USAGE_ACL,RDB$FIELDS.RDB$BACKUP_STATE.RDB$SECURITY_CLASS
SQL$176,NONREL_USAGE_ACL,RDB$FIELDS.RDB$STAT_GROUP.RDB$SECURITY_CLASS
SQL$177,NONREL_USAGE_ACL,RDB$FIELDS.RDB$DEBUG_INFO.RDB$SECURITY_CLASS
SQL$178,NONREL_USAGE_ACL,RDB$FIELDS.RDB$PARAMETER_MECHANISM.RDB$SECURITY_CLASS
SQL$179,NONREL_USAGE_ACL,RDB$FIELDS.RDB$SOURCE_INFO.RDB$SECURITY_CLASS
SQL$180,NONREL_USAGE_ACL,RDB$FIELDS.RDB$CONTEXT_VAR_NAME.RDB$SECURITY_CLASS
SQL$181,NONREL_USAGE_ACL,RDB$FIELDS.RDB$CONTEXT_VAR_VALUE.RDB$SECURITY_CLASS
SQL$182,NONREL_USAGE_ACL,RDB$FIELDS.RDB$ENGINE_NAME.RDB$SECURITY_CLASS
SQL$183,NONREL_USAGE_ACL,RDB$FIELDS.RDB$PACKAGE_NAME.RDB$SECURITY_CLASS
SQL$184,NONREL_USAGE_ACL,RDB$FIELDS.RDB$FUNCTION_ID.RDB$SECURITY_CLASS
SQL$185,NONREL_USAGE_ACL,RDB$FIELDS.RDB$FUNCTION_BLR.RDB$SECURITY_CLASS
SQL$186,NONREL_USAGE_ACL,RDB$FIELDS.RDB$ARGUMENT_NAME.RDB$SECURITY_CLASS
SQL$187,NONREL_USAGE_ACL,RDB$FIELDS.RDB$ARGUMENT_MECHANISM.RDB$SECURITY_CLASS
SQL$188,NONREL_USAGE_ACL,RDB$FIELDS.RDB$IDENTITY_TYPE.RDB$SECURITY_CLASS
SQL$189,NONREL_USAGE_ACL,RDB$FIELDS.RDB$BOOLEAN.RDB$SECURITY_CLASS
SQL$190,NONREL_USAGE_ACL,RDB$FIELDS.SEC$USER_NAME.RDB$SECURITY_CLASS
SQL$191,NONREL_USAGE_ACL,RDB$FIELDS.SEC$KEY.RDB$SECURITY_CLASS
SQL$192,NONREL_USAGE_ACL,RDB$FIELDS.SEC$VALUE.RDB$SECURITY_CLASS
SQL$193,NONREL_USAGE_ACL,RDB$FIELDS.SEC$NAME_PART.RDB$SECURITY_CLASS
SQL$194,NONREL_USAGE_ACL,RDB$FIELDS.RDB$CLIENT_VERSION.RDB$SECURITY_CLASS
SQL$195,NONREL_USAGE_ACL,RDB$FIELDS.RDB$REMOTE_VERSION.RDB$SECURITY_CLASS
SQL$196,NONREL_USAGE_ACL,RDB$FIELDS.RDB$HOST_NAME.RDB$SECURITY_CLASS
SQL$197,NONREL_USAGE_ACL,RDB$FIELDS.RDB$OS_USER.RDB$SECURITY_CLASS
SQL$198,NONREL_USAGE_ACL,RDB$FIELDS.RDB$GENERATOR_VALUE.RDB$SECURITY_CLASS
SQL$199,NONREL_USAGE_ACL,RDB$FIELDS.RDB$AUTH_METHOD.RDB$SECURITY_CLASS
SQL$200,NONREL_USAGE_ACL,RDB$FIELDS.RDB$LINGER.RDB$SECURITY_CLASS
SQL$201,NONREL_USAGE_ACL,RDB$FIELDS.MON$SEC_DATABASE.RDB$SECURITY_CLASS
SQL$202,NONREL_USAGE_ACL,RDB$FIELDS.RDB$MAP_NAME.RDB$SECURITY_CLASS
SQL$203,NONREL_USAGE_ACL,RDB$FIELDS.RDB$MAP_USING.RDB$SECURITY_CLASS
SQL$204,NONREL_USAGE_ACL,RDB$FIELDS.RDB$MAP_DB.RDB$SECURITY_CLASS
SQL$205,NONREL_USAGE_ACL,RDB$FIELDS.RDB$MAP_FROM_TYPE.RDB$SECURITY_CLASS
SQL$206,NONREL_USAGE_ACL,RDB$FIELDS.RDB$MAP_FROM.RDB$SECURITY_CLASS
SQL$207,NONREL_USAGE_ACL,RDB$FIELDS.RDB$MAP_TO.RDB$SECURITY_CLASS
SQL$208,NONREL_USAGE_ACL,RDB$FIELDS.RDB$GENERATOR_INCREMENT.RDB$SECURITY_CLASS
SQL$209,NONREL_USAGE_ACL,RDB$FIELDS.RDB$PLAN.RDB$SECURITY_CLASS
SQL$210,NONREL_USAGE_ACL,RDB$FIELDS.RDB$SYSTEM_PRIVILEGES.RDB$SECURITY_CLASS
SQL$211,NONREL_USAGE_ACL,RDB$FIELDS.RDB$SQL_SECURITY.RDB$SECURITY_CLASS
SQL$212,NONREL_USAGE_ACL,RDB$FIELDS.MON$IDLE_TIMEOUT.RDB$SECURITY_CLASS
SQL$213,NONREL_USAGE_ACL,RDB$FIELDS.MON$IDLE_TIMER.RDB$SECURITY_CLASS
SQL$214,NONREL_USAGE_ACL,RDB$FIELDS.MON$STATEMENT_TIMEOUT.RDB$SECURITY_CLASS
SQL$215,NONREL_USAGE_ACL,RDB$FIELDS.MON$STATEMENT_TIMER.RDB$SECURITY_CLASS
SQL$216,NONREL_USAGE_ACL,RDB$FIELDS.RDB$TIME_ZONE_ID.RDB$SECURITY_CLASS
SQL$217,NONREL_USAGE_ACL,RDB$FIELDS.RDB$TIME_ZONE_NAME.RDB$SECURITY_CLASS
SQL$218,NONREL_USAGE_ACL,RDB$FIELDS.RDB$TIME_ZONE_OFFSET.RDB$SECURITY_CLASS
SQL$219,NONREL_USAGE_ACL,RDB$FIELDS.RDB$TIMESTAMP_TZ.RDB$SECURITY_CLASS
SQL$220,NONREL_USAGE_ACL,RDB$FIELDS.RDB$DBTZ_VERSION.RDB$SECURITY_CLASS
SQL$221,NONREL_USAGE_ACL,RDB$FIELDS.RDB$CRYPT_STATE.RDB$SECURITY_CLASS
SQL$222,NONREL_USAGE_ACL,RDB$FIELDS.MON$WIRE_CRYPT_PLUGIN.RDB$SECURITY_CLASS
SQL$223,NONREL_USAGE_ACL,RDB$FIELDS.RDB$PUBLICATION_NAME.RDB$SECURITY_CLASS
SQL$224,NONREL_USAGE_ACL,RDB$FIELDS.RDB$FILE_ID.RDB$SECURITY_CLASS
SQL$225,NONREL_USAGE_ACL,RDB$FIELDS.RDB$CONFIG_ID.RDB$SECURITY_CLASS
SQL$226,NONREL_USAGE_ACL,RDB$FIELDS.RDB$CONFIG_NAME.RDB$SECURITY_CLASS
SQL$227,NONREL_USAGE_ACL,RDB$FIELDS.RDB$CONFIG_VALUE.RDB$SECURITY_CLASS
SQL$228,NONREL_USAGE_ACL,RDB$FIELDS.RDB$CONFIG_IS_SET.RDB$SECURITY_CLASS
SQL$229,NONREL_USAGE_ACL,RDB$FIELDS.RDB$REPLICA_MODE.RDB$SECURITY_CLASS
SQL$230,NONREL_USAGE_ACL,RDB$FIELDS.RDB$KEYWORD_NAME.RDB$SECURITY_CLASS
SQL$231,NONREL_USAGE_ACL,RDB$FIELDS.RDB$KEYWORD_RESERVED.RDB$SECURITY_CLASS
SQL$232,NONREL_USAGE_ACL,RDB$FIELDS.RDB$SHORT_DESCRIPTION.RDB$SECURITY_CLASS
SQL$233,NONREL_USAGE_ACL,RDB$FIELDS.RDB$SECONDS_INTERVAL.RDB$SECURITY_CLASS
SQL$234,NONREL_USAGE_ACL,RDB$FIELDS.RDB$PROFILE_SESSION_ID.RDB$SECURITY_CLASS
SQL$235,NONREL_USAGE_ACL,RDB$FIELDS.RDB$BLOB_UTIL_HANDLE.RDB$SECURITY_CLASS
SQL$236,NONREL_USAGE_ACL,RDB$FIELDS.RDB$BLOB.RDB$SECURITY_CLASS
SQL$237,NONREL_USAGE_ACL,RDB$FIELDS.RDB$VARBINARY_MAX.RDB$SECURITY_CLASS
SQL$238,NONREL_USAGE_ACL,RDB$FIELDS.RDB$INTEGER.RDB$SECURITY_CLASS
SQL$239,NONREL_USAGE_ACL,RDB$FIELDS.MON$PARALLEL_WORKERS.RDB$SECURITY_CLASS
SQL$240,NONREL_USAGE_ACL,RDB$FIELDS.RDB$SCHEMA_NAME.RDB$SECURITY_CLASS
SQL$241,NONREL_USAGE_ACL,RDB$FIELDS.RDB$TEXT_MAX.RDB$SECURITY_CLASS
SQL$242,NONREL_USAGE_ACL,RDB$FIELDS.MON$TABLE_TYPE.RDB$SECURITY_CLASS
SQL$243,NONREL_USAGE_ACL,RDB$SCHEMAS.SYSTEM.RDB$SECURITY_CLASS
SQL$244,NONREL_USAGE_ACL,RDB$SCHEMAS.PUBLIC.RDB$SECURITY_CLASS
SQL$245,NONREL_USAGE_ACL,RDB$CHARACTER_SETS.NONE.RDB$SECURITY_CLASS
SQL$246,NONREL_USAGE_ACL,RDB$CHARACTER_SETS.OCTETS.RDB$SECURITY_CLASS
SQL$247,NONREL_USAGE_ACL,RDB$CHARACTER_SETS.ASCII.RDB$SECURITY_CLASS
SQL$248,NONREL_USAGE_ACL,RDB$CHARACTER_SETS.UNICODE_FSS.RDB$SECURITY_CLASS
SQL$249,NONREL_USAGE_ACL,RDB$CHARACTER_SETS.UTF8.RDB$SECURITY_CLASS
SQL$250,NONREL_USAGE_ACL,RDB$CHARACTER_SETS.SJIS_0208.RDB$SECURITY_CLASS
SQL$251,NONREL_USAGE_ACL,RDB$CHARACTER_SETS.EUCJ_0208.RDB$SECURITY_CLASS
SQL$252,NONREL_USAGE_ACL,RDB$CHARACTER_SETS.DOS437.RDB$SECURITY_CLASS
SQL$253,NONREL_USAGE_ACL,RDB$CHARACTER_SETS.DOS850.RDB$SECURITY_CLASS
SQL$254,NONREL_USAGE_ACL,RDB$CHARACTER_SETS.DOS865.RDB$SECURITY_CLASS
SQL$255,NONREL_USAGE_ACL,RDB$CHARACTER_SETS.ISO8859_1.RDB$SECURITY_CLASS
SQL$256,NONREL_USAGE_ACL,RDB$CHARACTER_SETS.ISO8859_2.RDB$SECURITY_CLASS
SQL$257,NONREL_USAGE_ACL,RDB$CHARACTER_SETS.ISO8859_3.RDB$SECURITY_CLASS
SQL$258,NONREL_USAGE_ACL,RDB$CHARACTER_SETS.ISO8859_4.RDB$SECURITY_CLASS
SQL$259,NONREL_USAGE_ACL,RDB$CHARACTER_SETS.ISO8859_5.RDB$SECURITY_CLASS
SQL$260,NONREL_USAGE_ACL,RDB$CHARACTER_SETS.ISO8859_6.RDB$SECURITY_CLASS
SQL$261,NONREL_USAGE_ACL,RDB$CHARACTER_SETS.ISO8859_7.RDB$SECURITY_CLASS
SQL$262,NONREL_USAGE_ACL,RDB$CHARACTER_SETS.ISO8859_8.RDB$SECURITY_CLASS
SQL$263,NONREL_USAGE_ACL,RDB$CHARACTER_SETS.ISO8859_9.RDB$SECURITY_CLASS
SQL$264,NONREL_USAGE_ACL,RDB$CHARACTER_SETS.ISO8859_13.RDB$SECURITY_CLASS
SQL$265,NONREL_USAGE_ACL,RDB$CHARACTER_SETS.DOS852.RDB$SECURITY_CLASS
SQL$266,NONREL_USAGE_ACL,RDB$CHARACTER_SETS.DOS857.RDB$SECURITY_CLASS
SQL$267,NONREL_USAGE_ACL,RDB$CHARACTER_SETS.DOS860.RDB$SECURITY_CLASS
SQL$268,NONREL_USAGE_ACL,RDB$CHARACTER_SETS.DOS861.RDB$SECURITY_CLASS
SQL$269,NONREL_USAGE_ACL,RDB$CHARACTER_SETS.DOS863.RDB$SECURITY_CLASS
SQL$270,NONREL_USAGE_ACL,RDB$CHARACTER_SETS.CYRL.RDB$SECURITY_CLASS
SQL$271,NONREL_USAGE_ACL,RDB$CHARACTER_SETS.DOS737.RDB$SECURITY_CLASS
SQL$272,NONREL_USAGE_ACL,RDB$CHARACTER_SETS.DOS775.RDB$SECURITY_CLASS
SQL$273,NONREL_USAGE_ACL,RDB$CHARACTER_SETS.DOS858.RDB$SECURITY_CLASS
SQL$274,NONREL_USAGE_ACL,RDB$CHARACTER_SETS.DOS862.RDB$SECURITY_CLASS
SQL$275,NONREL_USAGE_ACL,RDB$CHARACTER_SETS.DOS864.RDB$SECURITY_CLASS
SQL$276,NONREL_USAGE_ACL,RDB$CHARACTER_SETS.DOS866.RDB$SECURITY_CLASS
SQL$277,NONREL_USAGE_ACL,RDB$CHARACTER_SETS.DOS869.RDB$SECURITY_CLASS
SQL$278,NONREL_USAGE_ACL,RDB$CHARACTER_SETS.WIN1250.RDB$SECURITY_CLASS
SQL$279,NONREL_USAGE_ACL,RDB$CHARACTER_SETS.WIN1251.RDB$SECURITY_CLASS
SQL$280,NONREL_USAGE_ACL,RDB$CHARACTER_SETS.WIN1252.RDB$SECURITY_CLASS
SQL$281,NONREL_USAGE_ACL,RDB$CHARACTER_SETS.WIN1253.RDB$SECURITY_CLASS
SQL$282,NONREL_USAGE_ACL,RDB$CHARACTER_SETS.WIN1254.RDB$SECURITY_CLASS
SQL$283,NONREL_USAGE_ACL,RDB$CHARACTER_SETS.NEXT.RDB$SECURITY_CLASS
SQL$284,NONREL_USAGE_ACL,RDB$CHARACTER_SETS.WIN1255.RDB$SECURITY_CLASS
SQL$285,NONREL_USAGE_ACL,RDB$CHARACTER_SETS.WIN1256.RDB$SECURITY_CLASS
SQL$286,NONREL_USAGE_ACL,RDB$CHARACTER_SETS.WIN1257.RDB$SECURITY_CLASS
SQL$287,NONREL_USAGE_ACL,RDB$CHARACTER_SETS.KSC_5601.RDB$SECURITY_CLASS
SQL$288,NONREL_USAGE_ACL,RDB$CHARACTER_SETS.BIG_5.RDB$SECURITY_CLASS
SQL$289,NONREL_USAGE_ACL,RDB$CHARACTER_SETS.GB_2312.RDB$SECURITY_CLASS
SQL$290,NONREL_USAGE_ACL,RDB$CHARACTER_SETS.KOI8R.RDB$SECURITY_CLASS
SQL$291,NONREL_USAGE_ACL,RDB$CHARACTER_SETS.KOI8U.RDB$SECURITY_CLASS
SQL$292,NONREL_USAGE_ACL,RDB$CHARACTER_SETS.WIN1258.RDB$SECURITY_CLASS
SQL$293,NONREL_USAGE_ACL,RDB$CHARACTER_SETS.TIS620.RDB$SECURITY_CLASS
SQL$294,NONREL_USAGE_ACL,RDB$CHARACTER_SETS.GBK.RDB$SECURITY_CLASS
SQL$295,NONREL_USAGE_ACL,RDB$CHARACTER_SETS.CP943C.RDB$SECURITY_CLASS
SQL$296,NONREL_USAGE_ACL,RDB$CHARACTER_SETS.GB18030.RDB$SECURITY_CLASS
SQL$297,NONREL_USAGE_ACL,RDB$COLLATIONS.NONE.RDB$SECURITY_CLASS
SQL$298,NONREL_USAGE_ACL,RDB$COLLATIONS.OCTETS.RDB$SECURITY_CLASS
SQL$299,NONREL_USAGE_ACL,RDB$COLLATIONS.ASCII.RDB$SECURITY_CLASS
SQL$300,NONREL_USAGE_ACL,RDB$COLLATIONS.UNICODE_FSS.RDB$SECURITY_CLASS
SQL$301,NONREL_USAGE_ACL,RDB$COLLATIONS.UTF8.RDB$SECURITY_CLASS
SQL$302,NONREL_USAGE_ACL,RDB$COLLATIONS.UCS_BASIC.RDB$SECURITY_CLASS
SQL$303,NONREL_USAGE_ACL,RDB$COLLATIONS.UNICODE.RDB$SECURITY_CLASS
SQL$304,NONREL_USAGE_ACL,RDB$COLLATIONS.UNICODE_CI.RDB$SECURITY_CLASS
SQL$305,NONREL_USAGE_ACL,RDB$COLLATIONS.UNICODE_CI_AI.RDB$SECURITY_CLASS
SQL$306,NONREL_USAGE_ACL,RDB$COLLATIONS.SJIS_0208.RDB$SECURITY_CLASS
SQL$307,NONREL_USAGE_ACL,RDB$COLLATIONS.EUCJ_0208.RDB$SECURITY_CLASS
SQL$308,NONREL_USAGE_ACL,RDB$COLLATIONS.DOS437.RDB$SECURITY_CLASS
SQL$309,NONREL_USAGE_ACL,RDB$COLLATIONS.PDOX_ASCII.RDB$SECURITY_CLASS
SQL$310,NONREL_USAGE_ACL,RDB$COLLATIONS.PDOX_INTL.RDB$SECURITY_CLASS
SQL$311,NONREL_USAGE_ACL,RDB$COLLATIONS.PDOX_SWEDFIN.RDB$SECURITY_CLASS
SQL$312,NONREL_USAGE_ACL,RDB$COLLATIONS.DB_DEU437.RDB$SECURITY_CLASS
SQL$313,NONREL_USAGE_ACL,RDB$COLLATIONS.DB_ESP437.RDB$SECURITY_CLASS
SQL$314,NONREL_USAGE_ACL,RDB$COLLATIONS.DB_FIN437.RDB$SECURITY_CLASS
SQL$315,NONREL_USAGE_ACL,RDB$COLLATIONS.DB_FRA437.RDB$SECURITY_CLASS
SQL$316,NONREL_USAGE_ACL,RDB$COLLATIONS.DB_ITA437.RDB$SECURITY_CLASS
SQL$317,NONREL_USAGE_ACL,RDB$COLLATIONS.DB_NLD437.RDB$SECURITY_CLASS
SQL$318,NONREL_USAGE_ACL,RDB$COLLATIONS.DB_SVE437.RDB$SECURITY_CLASS
SQL$319,NONREL_USAGE_ACL,RDB$COLLATIONS.DB_UK437.RDB$SECURITY_CLASS
SQL$320,NONREL_USAGE_ACL,RDB$COLLATIONS.DB_US437.RDB$SECURITY_CLASS
SQL$321,NONREL_USAGE_ACL,RDB$COLLATIONS.DOS850.RDB$SECURITY_CLASS
SQL$322,NONREL_USAGE_ACL,RDB$COLLATIONS.DB_FRC850.RDB$SECURITY_CLASS
SQL$323,NONREL_USAGE_ACL,RDB$COLLATIONS.DB_DEU850.RDB$SECURITY_CLASS
SQL$324,NONREL_USAGE_ACL,RDB$COLLATIONS.DB_ESP850.RDB$SECURITY_CLASS
SQL$325,NONREL_USAGE_ACL,RDB$COLLATIONS.DB_FRA850.RDB$SECURITY_CLASS
SQL$326,NONREL_USAGE_ACL,RDB$COLLATIONS.DB_ITA850.RDB$SECURITY_CLASS
SQL$327,NONREL_USAGE_ACL,RDB$COLLATIONS.DB_NLD850.RDB$SECURITY_CLASS
SQL$328,NONREL_USAGE_ACL,RDB$COLLATIONS.DB_PTB850.RDB$SECURITY_CLASS
SQL$329,NONREL_USAGE_ACL,RDB$COLLATIONS.DB_SVE850.RDB$SECURITY_CLASS
SQL$330,NONREL_USAGE_ACL,RDB$COLLATIONS.DB_UK850.RDB$SECURITY_CLASS
SQL$331,NONREL_USAGE_ACL,RDB$COLLATIONS.DB_US850.RDB$SECURITY_CLASS
SQL$332,NONREL_USAGE_ACL,RDB$COLLATIONS.DOS865.RDB$SECURITY_CLASS
SQL$333,NONREL_USAGE_ACL,RDB$COLLATIONS.PDOX_NORDAN4.RDB$SECURITY_CLASS
SQL$334,NONREL_USAGE_ACL,RDB$COLLATIONS.DB_DAN865.RDB$SECURITY_CLASS
SQL$335,NONREL_USAGE_ACL,RDB$COLLATIONS.DB_NOR865.RDB$SECURITY_CLASS
SQL$336,NONREL_USAGE_ACL,RDB$COLLATIONS.ISO8859_1.RDB$SECURITY_CLASS
SQL$337,NONREL_USAGE_ACL,RDB$COLLATIONS.DA_DA.RDB$SECURITY_CLASS
SQL$338,NONREL_USAGE_ACL,RDB$COLLATIONS.DU_NL.RDB$SECURITY_CLASS
SQL$339,NONREL_USAGE_ACL,RDB$COLLATIONS.FI_FI.RDB$SECURITY_CLASS
SQL$340,NONREL_USAGE_ACL,RDB$COLLATIONS.FR_FR.RDB$SECURITY_CLASS
SQL$341,NONREL_USAGE_ACL,RDB$COLLATIONS.FR_CA.RDB$SECURITY_CLASS
SQL$342,NONREL_USAGE_ACL,RDB$COLLATIONS.DE_DE.RDB$SECURITY_CLASS
SQL$343,NONREL_USAGE_ACL,RDB$COLLATIONS.IS_IS.RDB$SECURITY_CLASS
SQL$344,NONREL_USAGE_ACL,RDB$COLLATIONS.IT_IT.RDB$SECURITY_CLASS
SQL$345,NONREL_USAGE_ACL,RDB$COLLATIONS.NO_NO.RDB$SECURITY_CLASS
SQL$346,NONREL_USAGE_ACL,RDB$COLLATIONS.ES_ES.RDB$SECURITY_CLASS
SQL$347,NONREL_USAGE_ACL,RDB$COLLATIONS.SV_SV.RDB$SECURITY_CLASS
SQL$348,NONREL_USAGE_ACL,RDB$COLLATIONS.EN_UK.RDB$SECURITY_CLASS
SQL$349,NONREL_USAGE_ACL,RDB$COLLATIONS.EN_US.RDB$SECURITY_CLASS
SQL$350,NONREL_USAGE_ACL,RDB$COLLATIONS.PT_PT.RDB$SECURITY_CLASS
SQL$351,NONREL_USAGE_ACL,RDB$COLLATIONS.PT_BR.RDB$SECURITY_CLASS
SQL$352,NONREL_USAGE_ACL,RDB$COLLATIONS.ES_ES_CI_AI.RDB$SECURITY_CLASS
SQL$353,NONREL_USAGE_ACL,RDB$COLLATIONS.FR_FR_CI_AI.RDB$SECURITY_CLASS
SQL$354,NONREL_USAGE_ACL,RDB$COLLATIONS.FR_CA_CI_AI.RDB$SECURITY_CLASS
SQL$355,NONREL_USAGE_ACL,RDB$COLLATIONS.ISO8859_2.RDB$SECURITY_CLASS
SQL$356,NONREL_USAGE_ACL,RDB$COLLATIONS.CS_CZ.RDB$SECURITY_CLASS
SQL$357,NONREL_USAGE_ACL,RDB$COLLATIONS.ISO_HUN.RDB$SECURITY_CLASS
SQL$358,NONREL_USAGE_ACL,RDB$COLLATIONS.ISO_PLK.RDB$SECURITY_CLASS
SQL$359,NONREL_USAGE_ACL,RDB$COLLATIONS.ISO8859_3.RDB$SECURITY_CLASS
SQL$360,NONREL_USAGE_ACL,RDB$COLLATIONS.ISO8859_4.RDB$SECURITY_CLASS
SQL$361,NONREL_USAGE_ACL,RDB$COLLATIONS.ISO8859_5.RDB$SECURITY_CLASS
SQL$362,NONREL_USAGE_ACL,RDB$COLLATIONS.ISO8859_6.RDB$SECURITY_CLASS
SQL$363,NONREL_USAGE_ACL,RDB$COLLATIONS.ISO8859_7.RDB$SECURITY_CLASS
SQL$364,NONREL_USAGE_ACL,RDB$COLLATIONS.ISO8859_8.RDB$SECURITY_CLASS
SQL$365,NONREL_USAGE_ACL,RDB$COLLATIONS.ISO8859_9.RDB$SECURITY_CLASS
SQL$366,NONREL_USAGE_ACL,RDB$COLLATIONS.ISO8859_13.RDB$SECURITY_CLASS
SQL$367,NONREL_USAGE_ACL,RDB$COLLATIONS.LT_LT.RDB$SECURITY_CLASS
SQL$368,NONREL_USAGE_ACL,RDB$COLLATIONS.DOS852.RDB$SECURITY_CLASS
SQL$369,NONREL_USAGE_ACL,RDB$COLLATIONS.DB_CSY.RDB$SECURITY_CLASS
SQL$370,NONREL_USAGE_ACL,RDB$COLLATIONS.DB_PLK.RDB$SECURITY_CLASS
SQL$371,NONREL_USAGE_ACL,RDB$COLLATIONS.DB_SLO.RDB$SECURITY_CLASS
SQL$372,NONREL_USAGE_ACL,RDB$COLLATIONS.PDOX_CSY.RDB$SECURITY_CLASS
SQL$373,NONREL_USAGE_ACL,RDB$COLLATIONS.PDOX_PLK.RDB$SECURITY_CLASS
SQL$374,NONREL_USAGE_ACL,RDB$COLLATIONS.PDOX_HUN.RDB$SECURITY_CLASS
SQL$375,NONREL_USAGE_ACL,RDB$COLLATIONS.PDOX_SLO.RDB$SECURITY_CLASS
SQL$376,NONREL_USAGE_ACL,RDB$COLLATIONS.DOS857.RDB$SECURITY_CLASS
SQL$377,NONREL_USAGE_ACL,RDB$COLLATIONS.DB_TRK.RDB$SECURITY_CLASS
SQL$378,NONREL_USAGE_ACL,RDB$COLLATIONS.DOS860.RDB$SECURITY_CLASS
SQL$379,NONREL_USAGE_ACL,RDB$COLLATIONS.DB_PTG860.RDB$SECURITY_CLASS
SQL$380,NONREL_USAGE_ACL,RDB$COLLATIONS.DOS861.RDB$SECURITY_CLASS
SQL$381,NONREL_USAGE_ACL,RDB$COLLATIONS.PDOX_ISL.RDB$SECURITY_CLASS
SQL$382,NONREL_USAGE_ACL,RDB$COLLATIONS.DOS863.RDB$SECURITY_CLASS
SQL$383,NONREL_USAGE_ACL,RDB$COLLATIONS.DB_FRC863.RDB$SECURITY_CLASS
SQL$384,NONREL_USAGE_ACL,RDB$COLLATIONS.CYRL.RDB$SECURITY_CLASS
SQL$385,NONREL_USAGE_ACL,RDB$COLLATIONS.DB_RUS.RDB$SECURITY_CLASS
SQL$386,NONREL_USAGE_ACL,RDB$COLLATIONS.PDOX_CYRL.RDB$SECURITY_CLASS
SQL$387,NONREL_USAGE_ACL,RDB$COLLATIONS.DOS737.RDB$SECURITY_CLASS
SQL$388,NONREL_USAGE_ACL,RDB$COLLATIONS.DOS775.RDB$SECURITY_CLASS
SQL$389,NONREL_USAGE_ACL,RDB$COLLATIONS.DOS858.RDB$SECURITY_CLASS
SQL$390,NONREL_USAGE_ACL,RDB$COLLATIONS.DOS862.RDB$SECURITY_CLASS
SQL$391,NONREL_USAGE_ACL,RDB$COLLATIONS.DOS864.RDB$SECURITY_CLASS
SQL$392,NONREL_USAGE_ACL,RDB$COLLATIONS.DOS866.RDB$SECURITY_CLASS
SQL$393,NONREL_USAGE_ACL,RDB$COLLATIONS.DOS869.RDB$SECURITY_CLASS
SQL$394,NONREL_USAGE_ACL,RDB$COLLATIONS.WIN1250.RDB$SECURITY_CLASS
SQL$395,NONREL_USAGE_ACL,RDB$COLLATIONS.PXW_CSY.RDB$SECURITY_CLASS
SQL$396,NONREL_USAGE_ACL,RDB$COLLATIONS.PXW_HUNDC.RDB$SECURITY_CLASS
SQL$397,NONREL_USAGE_ACL,RDB$COLLATIONS.PXW_PLK.RDB$SECURITY_CLASS
SQL$398,NONREL_USAGE_ACL,RDB$COLLATIONS.PXW_SLOV.RDB$SECURITY_CLASS
SQL$399,NONREL_USAGE_ACL,RDB$COLLATIONS.PXW_HUN.RDB$SECURITY_CLASS
SQL$400,NONREL_USAGE_ACL,RDB$COLLATIONS.BS_BA.RDB$SECURITY_CLASS
SQL$401,NONREL_USAGE_ACL,RDB$COLLATIONS.WIN_CZ.RDB$SECURITY_CLASS
SQL$402,NONREL_USAGE_ACL,RDB$COLLATIONS.WIN_CZ_CI_AI.RDB$SECURITY_CLASS
SQL$403,NONREL_USAGE_ACL,RDB$COLLATIONS.WIN1251.RDB$SECURITY_CLASS
SQL$404,NONREL_USAGE_ACL,RDB$COLLATIONS.PXW_CYRL.RDB$SECURITY_CLASS
SQL$405,NONREL_USAGE_ACL,RDB$COLLATIONS.WIN1251_UA.RDB$SECURITY_CLASS
SQL$406,NONREL_USAGE_ACL,RDB$COLLATIONS.WIN1252.RDB$SECURITY_CLASS
SQL$407,NONREL_USAGE_ACL,RDB$COLLATIONS.PXW_INTL.RDB$SECURITY_CLASS
SQL$408,NONREL_USAGE_ACL,RDB$COLLATIONS.PXW_INTL850.RDB$SECURITY_CLASS
SQL$409,NONREL_USAGE_ACL,RDB$COLLATIONS.PXW_NORDAN4.RDB$SECURITY_CLASS
SQL$410,NONREL_USAGE_ACL,RDB$COLLATIONS.PXW_SPAN.RDB$SECURITY_CLASS
SQL$411,NONREL_USAGE_ACL,RDB$COLLATIONS.PXW_SWEDFIN.RDB$SECURITY_CLASS
SQL$412,NONREL_USAGE_ACL,RDB$COLLATIONS.WIN_PTBR.RDB$SECURITY_CLASS
SQL$413,NONREL_USAGE_ACL,RDB$COLLATIONS.WIN1253.RDB$SECURITY_CLASS
SQL$414,NONREL_USAGE_ACL,RDB$COLLATIONS.PXW_GREEK.RDB$SECURITY_CLASS
SQL$415,NONREL_USAGE_ACL,RDB$COLLATIONS.WIN1254.RDB$SECURITY_CLASS
SQL$416,NONREL_USAGE_ACL,RDB$COLLATIONS.PXW_TURK.RDB$SECURITY_CLASS
SQL$417,NONREL_USAGE_ACL,RDB$COLLATIONS.NEXT.RDB$SECURITY_CLASS
SQL$418,NONREL_USAGE_ACL,RDB$COLLATIONS.NXT_US.RDB$SECURITY_CLASS
SQL$419,NONREL_USAGE_ACL,RDB$COLLATIONS.NXT_DEU.RDB$SECURITY_CLASS
SQL$420,NONREL_USAGE_ACL,RDB$COLLATIONS.NXT_FRA.RDB$SECURITY_CLASS
SQL$421,NONREL_USAGE_ACL,RDB$COLLATIONS.NXT_ITA.RDB$SECURITY_CLASS
SQL$422,NONREL_USAGE_ACL,RDB$COLLATIONS.NXT_ESP.RDB$SECURITY_CLASS
SQL$423,NONREL_USAGE_ACL,RDB$COLLATIONS.WIN1255.RDB$SECURITY_CLASS
SQL$424,NONREL_USAGE_ACL,RDB$COLLATIONS.WIN1256.RDB$SECURITY_CLASS
SQL$425,NONREL_USAGE_ACL,RDB$COLLATIONS.WIN1257.RDB$SECURITY_CLASS
SQL$426,NONREL_USAGE_ACL,RDB$COLLATIONS.WIN1257_EE.RDB$SECURITY_CLASS
SQL$427,NONREL_USAGE_ACL,RDB$COLLATIONS.WIN1257_LT.RDB$SECURITY_CLASS
SQL$428,NONREL_USAGE_ACL,RDB$COLLATIONS.WIN1257_LV.RDB$SECURITY_CLASS
SQL$429,NONREL_USAGE_ACL,RDB$COLLATIONS.KSC_5601.RDB$SECURITY_CLASS
SQL$430,NONREL_USAGE_ACL,RDB$COLLATIONS.KSC_DICTIONARY.RDB$SECURITY_CLASS
SQL$431,NONREL_USAGE_ACL,RDB$COLLATIONS.BIG_5.RDB$SECURITY_CLASS
SQL$432,NONREL_USAGE_ACL,RDB$COLLATIONS.GB_2312.RDB$SECURITY_CLASS
SQL$433,NONREL_USAGE_ACL,RDB$COLLATIONS.KOI8R.RDB$SECURITY_CLASS
SQL$434,NONREL_USAGE_ACL,RDB$COLLATIONS.KOI8R_RU.RDB$SECURITY_CLASS
SQL$435,NONREL_USAGE_ACL,RDB$COLLATIONS.KOI8U.RDB$SECURITY_CLASS
SQL$436,NONREL_USAGE_ACL,RDB$COLLATIONS.KOI8U_UA.RDB$SECURITY_CLASS
SQL$437,NONREL_USAGE_ACL,RDB$COLLATIONS.WIN1258.RDB$SECURITY_CLASS
SQL$438,NONREL_USAGE_ACL,RDB$COLLATIONS.TIS620.RDB$SECURITY_CLASS
SQL$439,NONREL_USAGE_ACL,RDB$COLLATIONS.TIS620_UNICODE.RDB$SECURITY_CLASS
SQL$440,NONREL_USAGE_ACL,RDB$COLLATIONS.GBK.RDB$SECURITY_CLASS
SQL$441,NONREL_USAGE_ACL,RDB$COLLATIONS.GBK_UNICODE.RDB$SECURITY_CLASS
SQL$442,NONREL_USAGE_ACL,RDB$COLLATIONS.CP943C.RDB$SECURITY_CLASS
SQL$443,NONREL_USAGE_ACL,RDB$COLLATIONS.CP943C_UNICODE.RDB$SECURITY_CLASS
SQL$444,NONREL_USAGE_ACL,RDB$COLLATIONS.GB18030.RDB$SECURITY_CLASS
SQL$445,NONREL_USAGE_ACL,RDB$COLLATIONS.GB18030_UNICODE.RDB$SECURITY_CLASS
SQL$446,NONREL_USAGE_ACL,RDB$GENERATORS.RDB$SECURITY_CLASS.RDB$SECURITY_CLASS
SQL$447,NONREL_USAGE_ACL,RDB$GENERATORS.SQL$DEFAULT.RDB$SECURITY_CLASS
SQL$448,NONREL_USAGE_ACL,RDB$GENERATORS.RDB$PROCEDURES.RDB$SECURITY_CLASS
SQL$449,NONREL_USAGE_ACL,RDB$GENERATORS.RDB$EXCEPTIONS.RDB$SECURITY_CLASS
SQL$450,NONREL_USAGE_ACL,RDB$GENERATORS.RDB$CONSTRAINT_NAME.RDB$SECURITY_CLASS
SQL$451,NONREL_USAGE_ACL,RDB$GENERATORS.RDB$FIELD_NAME.RDB$SECURITY_CLASS
SQL$452,NONREL_USAGE_ACL,RDB$GENERATORS.RDB$INDEX_NAME.RDB$SECURITY_CLASS
SQL$453,NONREL_USAGE_ACL,RDB$GENERATORS.RDB$TRIGGER_NAME.RDB$SECURITY_CLASS
SQL$454,NONREL_USAGE_ACL,RDB$GENERATORS.RDB$BACKUP_HISTORY.RDB$SECURITY_CLASS
SQL$455,NONREL_USAGE_ACL,RDB$GENERATORS.RDB$FUNCTIONS.RDB$SECURITY_CLASS
SQL$456,NONREL_USAGE_ACL,RDB$GENERATORS.RDB$GENERATOR_NAME.RDB$SECURITY_CLASS
SQL$457,NONREL_EXEC_ACL,RDB$PACKAGES.RDB$TIME_ZONE_UTIL.RDB$SECURITY_CLASS
SQL$458,NONREL_EXEC_ACL,RDB$PACKAGES.RDB$BLOB_UTIL.RDB$SECURITY_CLASS
SQL$459,NONREL_EXEC_ACL,RDB$PACKAGES.RDB$PROFILER.RDB$SECURITY_CLASS
SQL$460,NONREL_EXEC_ACL,RDB$PACKAGES.RDB$SQL.RDB$SECURITY_CLASS
SQL$461,ROLE_ACL,RDB$ROLES.RDB$ADMIN.RDB$SECURITY_CLASS
SQL$D22PUBLIC,DDL_ACL,DDL obj_relations PUBLIC
SQL$D22SYSTEM,DDL_ACL,DDL obj_relations SYSTEM
SQL$D23PUBLIC,DDL_ACL,DDL obj_views PUBLIC
SQL$D23SYSTEM,DDL_ACL,DDL obj_views SYSTEM
SQL$D24PUBLIC,DDL_ACL,DDL obj_procedures PUBLIC
SQL$D24SYSTEM,DDL_ACL,DDL obj_procedures SYSTEM
SQL$D25PUBLIC,DDL_ACL,DDL obj_functions PUBLIC
SQL$D25SYSTEM,DDL_ACL,DDL obj_functions SYSTEM
SQL$D26PUBLIC,DDL_ACL,DDL obj_packages PUBLIC
SQL$D26SYSTEM,DDL_ACL,DDL obj_packages SYSTEM
SQL$D27PUBLIC,DDL_ACL,DDL obj_generators PUBLIC
SQL$D27SYSTEM,DDL_ACL,DDL obj_generators SYSTEM
SQL$D28PUBLIC,DDL_ACL,DDL obj_domains PUBLIC
SQL$D28SYSTEM,DDL_ACL,DDL obj_domains SYSTEM
SQL$D29PUBLIC,DDL_ACL,DDL obj_exceptions PUBLIC
SQL$D29SYSTEM,DDL_ACL,DDL obj_exceptions SYSTEM
SQL$D30,DDL_ACL,DDL obj_roles
SQL$D31PUBLIC,DDL_ACL,DDL obj_charsets PUBLIC
SQL$D31SYSTEM,DDL_ACL,DDL obj_charsets SYSTEM
SQL$D32PUBLIC,DDL_ACL,DDL obj_collations PUBLIC
SQL$D32SYSTEM,DDL_ACL,DDL obj_collations SYSTEM
SQL$D33,DDL_ACL,DDL obj_filters
SQL$D34,DDL_ACL,DDL obj_jobs
SQL$D36,DDL_ACL,DDL obj_tablespaces
SQL$D39,DDL_ACL,DDL obj_schemas
SQL$462,DDL_ACL,RDB$DATABASE.RDB$SECURITY_CLASS
```

#### RDB$USER_PRIVILEGES (Default Rows)
Rows must be inserted in the exact order listed. This list assumes a normal CREATE DATABASE (not gbak restore). Conditional rows:
- If database owner is `SYSDBA`, then only the `SYSDBA` row for `RDB$ADMIN` applies; the `<DB_OWNER>` row is omitted because it is identical.
- If PUBLIC schema is not created (gbak restore with schema), omit all rows where `RDB$RELATION_SCHEMA_NAME = PUBLIC` or where `RDB$RELATION_NAME = PUBLIC` with `RDB$RELATION_SCHEMA_NAME = PUBLIC`.

```csv
RDB$USER,RDB$USER_TYPE,RDB$RELATION_NAME,RDB$RELATION_SCHEMA_NAME,RDB$FIELD_NAME,RDB$PRIVILEGE,RDB$GRANT_OPTION,RDB$OBJECT_TYPE,RDB$GRANTOR
<DB_OWNER>,8,RDB$PAGES,SYSTEM,NULL,S,1,0,NULL
PUBLIC,8,RDB$PAGES,SYSTEM,NULL,S,0,0,NULL
<DB_OWNER>,8,RDB$PAGES,SYSTEM,NULL,I,1,0,NULL
<DB_OWNER>,8,RDB$PAGES,SYSTEM,NULL,U,1,0,NULL
<DB_OWNER>,8,RDB$PAGES,SYSTEM,NULL,D,1,0,NULL
<DB_OWNER>,8,RDB$PAGES,SYSTEM,NULL,R,1,0,NULL
<DB_OWNER>,8,RDB$DATABASE,SYSTEM,NULL,S,1,0,NULL
PUBLIC,8,RDB$DATABASE,SYSTEM,NULL,S,0,0,NULL
<DB_OWNER>,8,RDB$DATABASE,SYSTEM,NULL,I,1,0,NULL
<DB_OWNER>,8,RDB$DATABASE,SYSTEM,NULL,U,1,0,NULL
<DB_OWNER>,8,RDB$DATABASE,SYSTEM,NULL,D,1,0,NULL
<DB_OWNER>,8,RDB$DATABASE,SYSTEM,NULL,R,1,0,NULL
<DB_OWNER>,8,RDB$FIELDS,SYSTEM,NULL,S,1,0,NULL
PUBLIC,8,RDB$FIELDS,SYSTEM,NULL,S,0,0,NULL
<DB_OWNER>,8,RDB$FIELDS,SYSTEM,NULL,I,1,0,NULL
<DB_OWNER>,8,RDB$FIELDS,SYSTEM,NULL,U,1,0,NULL
<DB_OWNER>,8,RDB$FIELDS,SYSTEM,NULL,D,1,0,NULL
<DB_OWNER>,8,RDB$FIELDS,SYSTEM,NULL,R,1,0,NULL
<DB_OWNER>,8,RDB$INDEX_SEGMENTS,SYSTEM,NULL,S,1,0,NULL
PUBLIC,8,RDB$INDEX_SEGMENTS,SYSTEM,NULL,S,0,0,NULL
<DB_OWNER>,8,RDB$INDEX_SEGMENTS,SYSTEM,NULL,I,1,0,NULL
<DB_OWNER>,8,RDB$INDEX_SEGMENTS,SYSTEM,NULL,U,1,0,NULL
<DB_OWNER>,8,RDB$INDEX_SEGMENTS,SYSTEM,NULL,D,1,0,NULL
<DB_OWNER>,8,RDB$INDEX_SEGMENTS,SYSTEM,NULL,R,1,0,NULL
<DB_OWNER>,8,RDB$INDICES,SYSTEM,NULL,S,1,0,NULL
PUBLIC,8,RDB$INDICES,SYSTEM,NULL,S,0,0,NULL
<DB_OWNER>,8,RDB$INDICES,SYSTEM,NULL,I,1,0,NULL
<DB_OWNER>,8,RDB$INDICES,SYSTEM,NULL,U,1,0,NULL
<DB_OWNER>,8,RDB$INDICES,SYSTEM,NULL,D,1,0,NULL
<DB_OWNER>,8,RDB$INDICES,SYSTEM,NULL,R,1,0,NULL
<DB_OWNER>,8,RDB$RELATION_FIELDS,SYSTEM,NULL,S,1,0,NULL
PUBLIC,8,RDB$RELATION_FIELDS,SYSTEM,NULL,S,0,0,NULL
<DB_OWNER>,8,RDB$RELATION_FIELDS,SYSTEM,NULL,I,1,0,NULL
<DB_OWNER>,8,RDB$RELATION_FIELDS,SYSTEM,NULL,U,1,0,NULL
<DB_OWNER>,8,RDB$RELATION_FIELDS,SYSTEM,NULL,D,1,0,NULL
<DB_OWNER>,8,RDB$RELATION_FIELDS,SYSTEM,NULL,R,1,0,NULL
<DB_OWNER>,8,RDB$RELATIONS,SYSTEM,NULL,S,1,0,NULL
PUBLIC,8,RDB$RELATIONS,SYSTEM,NULL,S,0,0,NULL
<DB_OWNER>,8,RDB$RELATIONS,SYSTEM,NULL,I,1,0,NULL
<DB_OWNER>,8,RDB$RELATIONS,SYSTEM,NULL,U,1,0,NULL
<DB_OWNER>,8,RDB$RELATIONS,SYSTEM,NULL,D,1,0,NULL
<DB_OWNER>,8,RDB$RELATIONS,SYSTEM,NULL,R,1,0,NULL
<DB_OWNER>,8,RDB$VIEW_RELATIONS,SYSTEM,NULL,S,1,0,NULL
PUBLIC,8,RDB$VIEW_RELATIONS,SYSTEM,NULL,S,0,0,NULL
<DB_OWNER>,8,RDB$VIEW_RELATIONS,SYSTEM,NULL,I,1,0,NULL
<DB_OWNER>,8,RDB$VIEW_RELATIONS,SYSTEM,NULL,U,1,0,NULL
<DB_OWNER>,8,RDB$VIEW_RELATIONS,SYSTEM,NULL,D,1,0,NULL
<DB_OWNER>,8,RDB$VIEW_RELATIONS,SYSTEM,NULL,R,1,0,NULL
<DB_OWNER>,8,RDB$FORMATS,SYSTEM,NULL,S,1,0,NULL
PUBLIC,8,RDB$FORMATS,SYSTEM,NULL,S,0,0,NULL
<DB_OWNER>,8,RDB$FORMATS,SYSTEM,NULL,I,1,0,NULL
<DB_OWNER>,8,RDB$FORMATS,SYSTEM,NULL,U,1,0,NULL
<DB_OWNER>,8,RDB$FORMATS,SYSTEM,NULL,D,1,0,NULL
<DB_OWNER>,8,RDB$FORMATS,SYSTEM,NULL,R,1,0,NULL
<DB_OWNER>,8,RDB$SECURITY_CLASSES,SYSTEM,NULL,S,1,0,NULL
PUBLIC,8,RDB$SECURITY_CLASSES,SYSTEM,NULL,S,0,0,NULL
<DB_OWNER>,8,RDB$SECURITY_CLASSES,SYSTEM,NULL,I,1,0,NULL
<DB_OWNER>,8,RDB$SECURITY_CLASSES,SYSTEM,NULL,U,1,0,NULL
<DB_OWNER>,8,RDB$SECURITY_CLASSES,SYSTEM,NULL,D,1,0,NULL
<DB_OWNER>,8,RDB$SECURITY_CLASSES,SYSTEM,NULL,R,1,0,NULL
<DB_OWNER>,8,RDB$FILES,SYSTEM,NULL,S,1,0,NULL
PUBLIC,8,RDB$FILES,SYSTEM,NULL,S,0,0,NULL
<DB_OWNER>,8,RDB$FILES,SYSTEM,NULL,I,1,0,NULL
<DB_OWNER>,8,RDB$FILES,SYSTEM,NULL,U,1,0,NULL
<DB_OWNER>,8,RDB$FILES,SYSTEM,NULL,D,1,0,NULL
<DB_OWNER>,8,RDB$FILES,SYSTEM,NULL,R,1,0,NULL
<DB_OWNER>,8,RDB$TYPES,SYSTEM,NULL,S,1,0,NULL
PUBLIC,8,RDB$TYPES,SYSTEM,NULL,S,0,0,NULL
<DB_OWNER>,8,RDB$TYPES,SYSTEM,NULL,I,1,0,NULL
<DB_OWNER>,8,RDB$TYPES,SYSTEM,NULL,U,1,0,NULL
<DB_OWNER>,8,RDB$TYPES,SYSTEM,NULL,D,1,0,NULL
<DB_OWNER>,8,RDB$TYPES,SYSTEM,NULL,R,1,0,NULL
<DB_OWNER>,8,RDB$TRIGGERS,SYSTEM,NULL,S,1,0,NULL
PUBLIC,8,RDB$TRIGGERS,SYSTEM,NULL,S,0,0,NULL
<DB_OWNER>,8,RDB$TRIGGERS,SYSTEM,NULL,I,1,0,NULL
<DB_OWNER>,8,RDB$TRIGGERS,SYSTEM,NULL,U,1,0,NULL
<DB_OWNER>,8,RDB$TRIGGERS,SYSTEM,NULL,D,1,0,NULL
<DB_OWNER>,8,RDB$TRIGGERS,SYSTEM,NULL,R,1,0,NULL
<DB_OWNER>,8,RDB$DEPENDENCIES,SYSTEM,NULL,S,1,0,NULL
PUBLIC,8,RDB$DEPENDENCIES,SYSTEM,NULL,S,0,0,NULL
<DB_OWNER>,8,RDB$DEPENDENCIES,SYSTEM,NULL,I,1,0,NULL
<DB_OWNER>,8,RDB$DEPENDENCIES,SYSTEM,NULL,U,1,0,NULL
<DB_OWNER>,8,RDB$DEPENDENCIES,SYSTEM,NULL,D,1,0,NULL
<DB_OWNER>,8,RDB$DEPENDENCIES,SYSTEM,NULL,R,1,0,NULL
<DB_OWNER>,8,RDB$FUNCTIONS,SYSTEM,NULL,S,1,0,NULL
PUBLIC,8,RDB$FUNCTIONS,SYSTEM,NULL,S,0,0,NULL
<DB_OWNER>,8,RDB$FUNCTIONS,SYSTEM,NULL,I,1,0,NULL
<DB_OWNER>,8,RDB$FUNCTIONS,SYSTEM,NULL,U,1,0,NULL
<DB_OWNER>,8,RDB$FUNCTIONS,SYSTEM,NULL,D,1,0,NULL
<DB_OWNER>,8,RDB$FUNCTIONS,SYSTEM,NULL,R,1,0,NULL
<DB_OWNER>,8,RDB$FUNCTION_ARGUMENTS,SYSTEM,NULL,S,1,0,NULL
PUBLIC,8,RDB$FUNCTION_ARGUMENTS,SYSTEM,NULL,S,0,0,NULL
<DB_OWNER>,8,RDB$FUNCTION_ARGUMENTS,SYSTEM,NULL,I,1,0,NULL
<DB_OWNER>,8,RDB$FUNCTION_ARGUMENTS,SYSTEM,NULL,U,1,0,NULL
<DB_OWNER>,8,RDB$FUNCTION_ARGUMENTS,SYSTEM,NULL,D,1,0,NULL
<DB_OWNER>,8,RDB$FUNCTION_ARGUMENTS,SYSTEM,NULL,R,1,0,NULL
<DB_OWNER>,8,RDB$FILTERS,SYSTEM,NULL,S,1,0,NULL
PUBLIC,8,RDB$FILTERS,SYSTEM,NULL,S,0,0,NULL
<DB_OWNER>,8,RDB$FILTERS,SYSTEM,NULL,I,1,0,NULL
<DB_OWNER>,8,RDB$FILTERS,SYSTEM,NULL,U,1,0,NULL
<DB_OWNER>,8,RDB$FILTERS,SYSTEM,NULL,D,1,0,NULL
<DB_OWNER>,8,RDB$FILTERS,SYSTEM,NULL,R,1,0,NULL
<DB_OWNER>,8,RDB$TRIGGER_MESSAGES,SYSTEM,NULL,S,1,0,NULL
PUBLIC,8,RDB$TRIGGER_MESSAGES,SYSTEM,NULL,S,0,0,NULL
<DB_OWNER>,8,RDB$TRIGGER_MESSAGES,SYSTEM,NULL,I,1,0,NULL
<DB_OWNER>,8,RDB$TRIGGER_MESSAGES,SYSTEM,NULL,U,1,0,NULL
<DB_OWNER>,8,RDB$TRIGGER_MESSAGES,SYSTEM,NULL,D,1,0,NULL
<DB_OWNER>,8,RDB$TRIGGER_MESSAGES,SYSTEM,NULL,R,1,0,NULL
<DB_OWNER>,8,RDB$USER_PRIVILEGES,SYSTEM,NULL,S,1,0,NULL
PUBLIC,8,RDB$USER_PRIVILEGES,SYSTEM,NULL,S,0,0,NULL
<DB_OWNER>,8,RDB$USER_PRIVILEGES,SYSTEM,NULL,I,1,0,NULL
<DB_OWNER>,8,RDB$USER_PRIVILEGES,SYSTEM,NULL,U,1,0,NULL
<DB_OWNER>,8,RDB$USER_PRIVILEGES,SYSTEM,NULL,D,1,0,NULL
<DB_OWNER>,8,RDB$USER_PRIVILEGES,SYSTEM,NULL,R,1,0,NULL
<DB_OWNER>,8,RDB$TRANSACTIONS,SYSTEM,NULL,S,1,0,NULL
PUBLIC,8,RDB$TRANSACTIONS,SYSTEM,NULL,S,0,0,NULL
<DB_OWNER>,8,RDB$TRANSACTIONS,SYSTEM,NULL,I,1,0,NULL
<DB_OWNER>,8,RDB$TRANSACTIONS,SYSTEM,NULL,U,1,0,NULL
<DB_OWNER>,8,RDB$TRANSACTIONS,SYSTEM,NULL,D,1,0,NULL
<DB_OWNER>,8,RDB$TRANSACTIONS,SYSTEM,NULL,R,1,0,NULL
<DB_OWNER>,8,RDB$GENERATORS,SYSTEM,NULL,S,1,0,NULL
PUBLIC,8,RDB$GENERATORS,SYSTEM,NULL,S,0,0,NULL
<DB_OWNER>,8,RDB$GENERATORS,SYSTEM,NULL,I,1,0,NULL
<DB_OWNER>,8,RDB$GENERATORS,SYSTEM,NULL,U,1,0,NULL
<DB_OWNER>,8,RDB$GENERATORS,SYSTEM,NULL,D,1,0,NULL
<DB_OWNER>,8,RDB$GENERATORS,SYSTEM,NULL,R,1,0,NULL
<DB_OWNER>,8,RDB$FIELD_DIMENSIONS,SYSTEM,NULL,S,1,0,NULL
PUBLIC,8,RDB$FIELD_DIMENSIONS,SYSTEM,NULL,S,0,0,NULL
<DB_OWNER>,8,RDB$FIELD_DIMENSIONS,SYSTEM,NULL,I,1,0,NULL
<DB_OWNER>,8,RDB$FIELD_DIMENSIONS,SYSTEM,NULL,U,1,0,NULL
<DB_OWNER>,8,RDB$FIELD_DIMENSIONS,SYSTEM,NULL,D,1,0,NULL
<DB_OWNER>,8,RDB$FIELD_DIMENSIONS,SYSTEM,NULL,R,1,0,NULL
<DB_OWNER>,8,RDB$RELATION_CONSTRAINTS,SYSTEM,NULL,S,1,0,NULL
PUBLIC,8,RDB$RELATION_CONSTRAINTS,SYSTEM,NULL,S,0,0,NULL
<DB_OWNER>,8,RDB$RELATION_CONSTRAINTS,SYSTEM,NULL,I,1,0,NULL
<DB_OWNER>,8,RDB$RELATION_CONSTRAINTS,SYSTEM,NULL,U,1,0,NULL
<DB_OWNER>,8,RDB$RELATION_CONSTRAINTS,SYSTEM,NULL,D,1,0,NULL
<DB_OWNER>,8,RDB$RELATION_CONSTRAINTS,SYSTEM,NULL,R,1,0,NULL
<DB_OWNER>,8,RDB$REF_CONSTRAINTS,SYSTEM,NULL,S,1,0,NULL
PUBLIC,8,RDB$REF_CONSTRAINTS,SYSTEM,NULL,S,0,0,NULL
<DB_OWNER>,8,RDB$REF_CONSTRAINTS,SYSTEM,NULL,I,1,0,NULL
<DB_OWNER>,8,RDB$REF_CONSTRAINTS,SYSTEM,NULL,U,1,0,NULL
<DB_OWNER>,8,RDB$REF_CONSTRAINTS,SYSTEM,NULL,D,1,0,NULL
<DB_OWNER>,8,RDB$REF_CONSTRAINTS,SYSTEM,NULL,R,1,0,NULL
<DB_OWNER>,8,RDB$CHECK_CONSTRAINTS,SYSTEM,NULL,S,1,0,NULL
PUBLIC,8,RDB$CHECK_CONSTRAINTS,SYSTEM,NULL,S,0,0,NULL
<DB_OWNER>,8,RDB$CHECK_CONSTRAINTS,SYSTEM,NULL,I,1,0,NULL
<DB_OWNER>,8,RDB$CHECK_CONSTRAINTS,SYSTEM,NULL,U,1,0,NULL
<DB_OWNER>,8,RDB$CHECK_CONSTRAINTS,SYSTEM,NULL,D,1,0,NULL
<DB_OWNER>,8,RDB$CHECK_CONSTRAINTS,SYSTEM,NULL,R,1,0,NULL
<DB_OWNER>,8,RDB$LOG_FILES,SYSTEM,NULL,S,1,0,NULL
PUBLIC,8,RDB$LOG_FILES,SYSTEM,NULL,S,0,0,NULL
<DB_OWNER>,8,RDB$LOG_FILES,SYSTEM,NULL,I,1,0,NULL
<DB_OWNER>,8,RDB$LOG_FILES,SYSTEM,NULL,U,1,0,NULL
<DB_OWNER>,8,RDB$LOG_FILES,SYSTEM,NULL,D,1,0,NULL
<DB_OWNER>,8,RDB$LOG_FILES,SYSTEM,NULL,R,1,0,NULL
<DB_OWNER>,8,RDB$PROCEDURES,SYSTEM,NULL,S,1,0,NULL
PUBLIC,8,RDB$PROCEDURES,SYSTEM,NULL,S,0,0,NULL
<DB_OWNER>,8,RDB$PROCEDURES,SYSTEM,NULL,I,1,0,NULL
<DB_OWNER>,8,RDB$PROCEDURES,SYSTEM,NULL,U,1,0,NULL
<DB_OWNER>,8,RDB$PROCEDURES,SYSTEM,NULL,D,1,0,NULL
<DB_OWNER>,8,RDB$PROCEDURES,SYSTEM,NULL,R,1,0,NULL
<DB_OWNER>,8,RDB$PROCEDURE_PARAMETERS,SYSTEM,NULL,S,1,0,NULL
PUBLIC,8,RDB$PROCEDURE_PARAMETERS,SYSTEM,NULL,S,0,0,NULL
<DB_OWNER>,8,RDB$PROCEDURE_PARAMETERS,SYSTEM,NULL,I,1,0,NULL
<DB_OWNER>,8,RDB$PROCEDURE_PARAMETERS,SYSTEM,NULL,U,1,0,NULL
<DB_OWNER>,8,RDB$PROCEDURE_PARAMETERS,SYSTEM,NULL,D,1,0,NULL
<DB_OWNER>,8,RDB$PROCEDURE_PARAMETERS,SYSTEM,NULL,R,1,0,NULL
<DB_OWNER>,8,RDB$CHARACTER_SETS,SYSTEM,NULL,S,1,0,NULL
PUBLIC,8,RDB$CHARACTER_SETS,SYSTEM,NULL,S,0,0,NULL
<DB_OWNER>,8,RDB$CHARACTER_SETS,SYSTEM,NULL,I,1,0,NULL
<DB_OWNER>,8,RDB$CHARACTER_SETS,SYSTEM,NULL,U,1,0,NULL
<DB_OWNER>,8,RDB$CHARACTER_SETS,SYSTEM,NULL,D,1,0,NULL
<DB_OWNER>,8,RDB$CHARACTER_SETS,SYSTEM,NULL,R,1,0,NULL
<DB_OWNER>,8,RDB$COLLATIONS,SYSTEM,NULL,S,1,0,NULL
PUBLIC,8,RDB$COLLATIONS,SYSTEM,NULL,S,0,0,NULL
<DB_OWNER>,8,RDB$COLLATIONS,SYSTEM,NULL,I,1,0,NULL
<DB_OWNER>,8,RDB$COLLATIONS,SYSTEM,NULL,U,1,0,NULL
<DB_OWNER>,8,RDB$COLLATIONS,SYSTEM,NULL,D,1,0,NULL
<DB_OWNER>,8,RDB$COLLATIONS,SYSTEM,NULL,R,1,0,NULL
<DB_OWNER>,8,RDB$EXCEPTIONS,SYSTEM,NULL,S,1,0,NULL
PUBLIC,8,RDB$EXCEPTIONS,SYSTEM,NULL,S,0,0,NULL
<DB_OWNER>,8,RDB$EXCEPTIONS,SYSTEM,NULL,I,1,0,NULL
<DB_OWNER>,8,RDB$EXCEPTIONS,SYSTEM,NULL,U,1,0,NULL
<DB_OWNER>,8,RDB$EXCEPTIONS,SYSTEM,NULL,D,1,0,NULL
<DB_OWNER>,8,RDB$EXCEPTIONS,SYSTEM,NULL,R,1,0,NULL
<DB_OWNER>,8,RDB$ROLES,SYSTEM,NULL,S,1,0,NULL
PUBLIC,8,RDB$ROLES,SYSTEM,NULL,S,0,0,NULL
<DB_OWNER>,8,RDB$ROLES,SYSTEM,NULL,I,1,0,NULL
<DB_OWNER>,8,RDB$ROLES,SYSTEM,NULL,U,1,0,NULL
<DB_OWNER>,8,RDB$ROLES,SYSTEM,NULL,D,1,0,NULL
<DB_OWNER>,8,RDB$ROLES,SYSTEM,NULL,R,1,0,NULL
<DB_OWNER>,8,RDB$BACKUP_HISTORY,SYSTEM,NULL,S,1,0,NULL
PUBLIC,8,RDB$BACKUP_HISTORY,SYSTEM,NULL,S,0,0,NULL
<DB_OWNER>,8,RDB$BACKUP_HISTORY,SYSTEM,NULL,I,1,0,NULL
<DB_OWNER>,8,RDB$BACKUP_HISTORY,SYSTEM,NULL,U,1,0,NULL
<DB_OWNER>,8,RDB$BACKUP_HISTORY,SYSTEM,NULL,D,1,0,NULL
<DB_OWNER>,8,RDB$BACKUP_HISTORY,SYSTEM,NULL,R,1,0,NULL
<DB_OWNER>,8,MON$DATABASE,SYSTEM,NULL,S,1,0,NULL
PUBLIC,8,MON$DATABASE,SYSTEM,NULL,S,0,0,NULL
<DB_OWNER>,8,MON$DATABASE,SYSTEM,NULL,I,1,0,NULL
<DB_OWNER>,8,MON$DATABASE,SYSTEM,NULL,U,1,0,NULL
<DB_OWNER>,8,MON$DATABASE,SYSTEM,NULL,D,1,0,NULL
<DB_OWNER>,8,MON$DATABASE,SYSTEM,NULL,R,1,0,NULL
<DB_OWNER>,8,MON$ATTACHMENTS,SYSTEM,NULL,S,1,0,NULL
PUBLIC,8,MON$ATTACHMENTS,SYSTEM,NULL,S,0,0,NULL
<DB_OWNER>,8,MON$ATTACHMENTS,SYSTEM,NULL,I,1,0,NULL
<DB_OWNER>,8,MON$ATTACHMENTS,SYSTEM,NULL,U,1,0,NULL
<DB_OWNER>,8,MON$ATTACHMENTS,SYSTEM,NULL,D,1,0,NULL
<DB_OWNER>,8,MON$ATTACHMENTS,SYSTEM,NULL,R,1,0,NULL
<DB_OWNER>,8,MON$TRANSACTIONS,SYSTEM,NULL,S,1,0,NULL
PUBLIC,8,MON$TRANSACTIONS,SYSTEM,NULL,S,0,0,NULL
<DB_OWNER>,8,MON$TRANSACTIONS,SYSTEM,NULL,I,1,0,NULL
<DB_OWNER>,8,MON$TRANSACTIONS,SYSTEM,NULL,U,1,0,NULL
<DB_OWNER>,8,MON$TRANSACTIONS,SYSTEM,NULL,D,1,0,NULL
<DB_OWNER>,8,MON$TRANSACTIONS,SYSTEM,NULL,R,1,0,NULL
<DB_OWNER>,8,MON$STATEMENTS,SYSTEM,NULL,S,1,0,NULL
PUBLIC,8,MON$STATEMENTS,SYSTEM,NULL,S,0,0,NULL
<DB_OWNER>,8,MON$STATEMENTS,SYSTEM,NULL,I,1,0,NULL
<DB_OWNER>,8,MON$STATEMENTS,SYSTEM,NULL,U,1,0,NULL
<DB_OWNER>,8,MON$STATEMENTS,SYSTEM,NULL,D,1,0,NULL
<DB_OWNER>,8,MON$STATEMENTS,SYSTEM,NULL,R,1,0,NULL
<DB_OWNER>,8,MON$CALL_STACK,SYSTEM,NULL,S,1,0,NULL
PUBLIC,8,MON$CALL_STACK,SYSTEM,NULL,S,0,0,NULL
<DB_OWNER>,8,MON$CALL_STACK,SYSTEM,NULL,I,1,0,NULL
<DB_OWNER>,8,MON$CALL_STACK,SYSTEM,NULL,U,1,0,NULL
<DB_OWNER>,8,MON$CALL_STACK,SYSTEM,NULL,D,1,0,NULL
<DB_OWNER>,8,MON$CALL_STACK,SYSTEM,NULL,R,1,0,NULL
<DB_OWNER>,8,MON$IO_STATS,SYSTEM,NULL,S,1,0,NULL
PUBLIC,8,MON$IO_STATS,SYSTEM,NULL,S,0,0,NULL
<DB_OWNER>,8,MON$IO_STATS,SYSTEM,NULL,I,1,0,NULL
<DB_OWNER>,8,MON$IO_STATS,SYSTEM,NULL,U,1,0,NULL
<DB_OWNER>,8,MON$IO_STATS,SYSTEM,NULL,D,1,0,NULL
<DB_OWNER>,8,MON$IO_STATS,SYSTEM,NULL,R,1,0,NULL
<DB_OWNER>,8,MON$RECORD_STATS,SYSTEM,NULL,S,1,0,NULL
PUBLIC,8,MON$RECORD_STATS,SYSTEM,NULL,S,0,0,NULL
<DB_OWNER>,8,MON$RECORD_STATS,SYSTEM,NULL,I,1,0,NULL
<DB_OWNER>,8,MON$RECORD_STATS,SYSTEM,NULL,U,1,0,NULL
<DB_OWNER>,8,MON$RECORD_STATS,SYSTEM,NULL,D,1,0,NULL
<DB_OWNER>,8,MON$RECORD_STATS,SYSTEM,NULL,R,1,0,NULL
<DB_OWNER>,8,MON$CONTEXT_VARIABLES,SYSTEM,NULL,S,1,0,NULL
PUBLIC,8,MON$CONTEXT_VARIABLES,SYSTEM,NULL,S,0,0,NULL
<DB_OWNER>,8,MON$CONTEXT_VARIABLES,SYSTEM,NULL,I,1,0,NULL
<DB_OWNER>,8,MON$CONTEXT_VARIABLES,SYSTEM,NULL,U,1,0,NULL
<DB_OWNER>,8,MON$CONTEXT_VARIABLES,SYSTEM,NULL,D,1,0,NULL
<DB_OWNER>,8,MON$CONTEXT_VARIABLES,SYSTEM,NULL,R,1,0,NULL
<DB_OWNER>,8,MON$MEMORY_USAGE,SYSTEM,NULL,S,1,0,NULL
PUBLIC,8,MON$MEMORY_USAGE,SYSTEM,NULL,S,0,0,NULL
<DB_OWNER>,8,MON$MEMORY_USAGE,SYSTEM,NULL,I,1,0,NULL
<DB_OWNER>,8,MON$MEMORY_USAGE,SYSTEM,NULL,U,1,0,NULL
<DB_OWNER>,8,MON$MEMORY_USAGE,SYSTEM,NULL,D,1,0,NULL
<DB_OWNER>,8,MON$MEMORY_USAGE,SYSTEM,NULL,R,1,0,NULL
<DB_OWNER>,8,RDB$PACKAGES,SYSTEM,NULL,S,1,0,NULL
PUBLIC,8,RDB$PACKAGES,SYSTEM,NULL,S,0,0,NULL
<DB_OWNER>,8,RDB$PACKAGES,SYSTEM,NULL,I,1,0,NULL
<DB_OWNER>,8,RDB$PACKAGES,SYSTEM,NULL,U,1,0,NULL
<DB_OWNER>,8,RDB$PACKAGES,SYSTEM,NULL,D,1,0,NULL
<DB_OWNER>,8,RDB$PACKAGES,SYSTEM,NULL,R,1,0,NULL
<DB_OWNER>,8,SEC$USERS,SYSTEM,NULL,S,1,0,NULL
PUBLIC,8,SEC$USERS,SYSTEM,NULL,S,0,0,NULL
<DB_OWNER>,8,SEC$USERS,SYSTEM,NULL,I,1,0,NULL
<DB_OWNER>,8,SEC$USERS,SYSTEM,NULL,U,1,0,NULL
<DB_OWNER>,8,SEC$USERS,SYSTEM,NULL,D,1,0,NULL
<DB_OWNER>,8,SEC$USERS,SYSTEM,NULL,R,1,0,NULL
<DB_OWNER>,8,SEC$USER_ATTRIBUTES,SYSTEM,NULL,S,1,0,NULL
PUBLIC,8,SEC$USER_ATTRIBUTES,SYSTEM,NULL,S,0,0,NULL
<DB_OWNER>,8,SEC$USER_ATTRIBUTES,SYSTEM,NULL,I,1,0,NULL
<DB_OWNER>,8,SEC$USER_ATTRIBUTES,SYSTEM,NULL,U,1,0,NULL
<DB_OWNER>,8,SEC$USER_ATTRIBUTES,SYSTEM,NULL,D,1,0,NULL
<DB_OWNER>,8,SEC$USER_ATTRIBUTES,SYSTEM,NULL,R,1,0,NULL
<DB_OWNER>,8,RDB$AUTH_MAPPING,SYSTEM,NULL,S,1,0,NULL
PUBLIC,8,RDB$AUTH_MAPPING,SYSTEM,NULL,S,0,0,NULL
<DB_OWNER>,8,RDB$AUTH_MAPPING,SYSTEM,NULL,I,1,0,NULL
<DB_OWNER>,8,RDB$AUTH_MAPPING,SYSTEM,NULL,U,1,0,NULL
<DB_OWNER>,8,RDB$AUTH_MAPPING,SYSTEM,NULL,D,1,0,NULL
<DB_OWNER>,8,RDB$AUTH_MAPPING,SYSTEM,NULL,R,1,0,NULL
<DB_OWNER>,8,SEC$GLOBAL_AUTH_MAPPING,SYSTEM,NULL,S,1,0,NULL
PUBLIC,8,SEC$GLOBAL_AUTH_MAPPING,SYSTEM,NULL,S,0,0,NULL
<DB_OWNER>,8,SEC$GLOBAL_AUTH_MAPPING,SYSTEM,NULL,I,1,0,NULL
<DB_OWNER>,8,SEC$GLOBAL_AUTH_MAPPING,SYSTEM,NULL,U,1,0,NULL
<DB_OWNER>,8,SEC$GLOBAL_AUTH_MAPPING,SYSTEM,NULL,D,1,0,NULL
<DB_OWNER>,8,SEC$GLOBAL_AUTH_MAPPING,SYSTEM,NULL,R,1,0,NULL
<DB_OWNER>,8,RDB$DB_CREATORS,SYSTEM,NULL,S,1,0,NULL
PUBLIC,8,RDB$DB_CREATORS,SYSTEM,NULL,S,0,0,NULL
<DB_OWNER>,8,RDB$DB_CREATORS,SYSTEM,NULL,I,1,0,NULL
<DB_OWNER>,8,RDB$DB_CREATORS,SYSTEM,NULL,U,1,0,NULL
<DB_OWNER>,8,RDB$DB_CREATORS,SYSTEM,NULL,D,1,0,NULL
<DB_OWNER>,8,RDB$DB_CREATORS,SYSTEM,NULL,R,1,0,NULL
<DB_OWNER>,8,SEC$DB_CREATORS,SYSTEM,NULL,S,1,0,NULL
PUBLIC,8,SEC$DB_CREATORS,SYSTEM,NULL,S,0,0,NULL
<DB_OWNER>,8,SEC$DB_CREATORS,SYSTEM,NULL,I,1,0,NULL
<DB_OWNER>,8,SEC$DB_CREATORS,SYSTEM,NULL,U,1,0,NULL
<DB_OWNER>,8,SEC$DB_CREATORS,SYSTEM,NULL,D,1,0,NULL
<DB_OWNER>,8,SEC$DB_CREATORS,SYSTEM,NULL,R,1,0,NULL
<DB_OWNER>,8,MON$TABLE_STATS,SYSTEM,NULL,S,1,0,NULL
PUBLIC,8,MON$TABLE_STATS,SYSTEM,NULL,S,0,0,NULL
<DB_OWNER>,8,MON$TABLE_STATS,SYSTEM,NULL,I,1,0,NULL
<DB_OWNER>,8,MON$TABLE_STATS,SYSTEM,NULL,U,1,0,NULL
<DB_OWNER>,8,MON$TABLE_STATS,SYSTEM,NULL,D,1,0,NULL
<DB_OWNER>,8,MON$TABLE_STATS,SYSTEM,NULL,R,1,0,NULL
<DB_OWNER>,8,RDB$TIME_ZONES,SYSTEM,NULL,S,1,0,NULL
PUBLIC,8,RDB$TIME_ZONES,SYSTEM,NULL,S,0,0,NULL
<DB_OWNER>,8,RDB$TIME_ZONES,SYSTEM,NULL,I,1,0,NULL
<DB_OWNER>,8,RDB$TIME_ZONES,SYSTEM,NULL,U,1,0,NULL
<DB_OWNER>,8,RDB$TIME_ZONES,SYSTEM,NULL,D,1,0,NULL
<DB_OWNER>,8,RDB$TIME_ZONES,SYSTEM,NULL,R,1,0,NULL
<DB_OWNER>,8,RDB$PUBLICATIONS,SYSTEM,NULL,S,1,0,NULL
PUBLIC,8,RDB$PUBLICATIONS,SYSTEM,NULL,S,0,0,NULL
<DB_OWNER>,8,RDB$PUBLICATIONS,SYSTEM,NULL,I,1,0,NULL
<DB_OWNER>,8,RDB$PUBLICATIONS,SYSTEM,NULL,U,1,0,NULL
<DB_OWNER>,8,RDB$PUBLICATIONS,SYSTEM,NULL,D,1,0,NULL
<DB_OWNER>,8,RDB$PUBLICATIONS,SYSTEM,NULL,R,1,0,NULL
<DB_OWNER>,8,RDB$PUBLICATION_TABLES,SYSTEM,NULL,S,1,0,NULL
PUBLIC,8,RDB$PUBLICATION_TABLES,SYSTEM,NULL,S,0,0,NULL
<DB_OWNER>,8,RDB$PUBLICATION_TABLES,SYSTEM,NULL,I,1,0,NULL
<DB_OWNER>,8,RDB$PUBLICATION_TABLES,SYSTEM,NULL,U,1,0,NULL
<DB_OWNER>,8,RDB$PUBLICATION_TABLES,SYSTEM,NULL,D,1,0,NULL
<DB_OWNER>,8,RDB$PUBLICATION_TABLES,SYSTEM,NULL,R,1,0,NULL
<DB_OWNER>,8,RDB$CONFIG,SYSTEM,NULL,S,1,0,NULL
PUBLIC,8,RDB$CONFIG,SYSTEM,NULL,S,0,0,NULL
<DB_OWNER>,8,RDB$CONFIG,SYSTEM,NULL,I,1,0,NULL
<DB_OWNER>,8,RDB$CONFIG,SYSTEM,NULL,U,1,0,NULL
<DB_OWNER>,8,RDB$CONFIG,SYSTEM,NULL,D,1,0,NULL
<DB_OWNER>,8,RDB$CONFIG,SYSTEM,NULL,R,1,0,NULL
<DB_OWNER>,8,RDB$KEYWORDS,SYSTEM,NULL,S,1,0,NULL
PUBLIC,8,RDB$KEYWORDS,SYSTEM,NULL,S,0,0,NULL
<DB_OWNER>,8,RDB$KEYWORDS,SYSTEM,NULL,I,1,0,NULL
<DB_OWNER>,8,RDB$KEYWORDS,SYSTEM,NULL,U,1,0,NULL
<DB_OWNER>,8,RDB$KEYWORDS,SYSTEM,NULL,D,1,0,NULL
<DB_OWNER>,8,RDB$KEYWORDS,SYSTEM,NULL,R,1,0,NULL
<DB_OWNER>,8,MON$COMPILED_STATEMENTS,SYSTEM,NULL,S,1,0,NULL
PUBLIC,8,MON$COMPILED_STATEMENTS,SYSTEM,NULL,S,0,0,NULL
<DB_OWNER>,8,MON$COMPILED_STATEMENTS,SYSTEM,NULL,I,1,0,NULL
<DB_OWNER>,8,MON$COMPILED_STATEMENTS,SYSTEM,NULL,U,1,0,NULL
<DB_OWNER>,8,MON$COMPILED_STATEMENTS,SYSTEM,NULL,D,1,0,NULL
<DB_OWNER>,8,MON$COMPILED_STATEMENTS,SYSTEM,NULL,R,1,0,NULL
<DB_OWNER>,8,RDB$SCHEMAS,SYSTEM,NULL,S,1,0,NULL
PUBLIC,8,RDB$SCHEMAS,SYSTEM,NULL,S,0,0,NULL
<DB_OWNER>,8,RDB$SCHEMAS,SYSTEM,NULL,I,1,0,NULL
<DB_OWNER>,8,RDB$SCHEMAS,SYSTEM,NULL,U,1,0,NULL
<DB_OWNER>,8,RDB$SCHEMAS,SYSTEM,NULL,D,1,0,NULL
<DB_OWNER>,8,RDB$SCHEMAS,SYSTEM,NULL,R,1,0,NULL
<DB_OWNER>,8,MON$LOCAL_TEMPORARY_TABLES,SYSTEM,NULL,S,1,0,NULL
PUBLIC,8,MON$LOCAL_TEMPORARY_TABLES,SYSTEM,NULL,S,0,0,NULL
<DB_OWNER>,8,MON$LOCAL_TEMPORARY_TABLES,SYSTEM,NULL,I,1,0,NULL
<DB_OWNER>,8,MON$LOCAL_TEMPORARY_TABLES,SYSTEM,NULL,U,1,0,NULL
<DB_OWNER>,8,MON$LOCAL_TEMPORARY_TABLES,SYSTEM,NULL,D,1,0,NULL
<DB_OWNER>,8,MON$LOCAL_TEMPORARY_TABLES,SYSTEM,NULL,R,1,0,NULL
<DB_OWNER>,8,MON$LOCAL_TEMPORARY_TABLE_COLUMNS,SYSTEM,NULL,S,1,0,NULL
PUBLIC,8,MON$LOCAL_TEMPORARY_TABLE_COLUMNS,SYSTEM,NULL,S,0,0,NULL
<DB_OWNER>,8,MON$LOCAL_TEMPORARY_TABLE_COLUMNS,SYSTEM,NULL,I,1,0,NULL
<DB_OWNER>,8,MON$LOCAL_TEMPORARY_TABLE_COLUMNS,SYSTEM,NULL,U,1,0,NULL
<DB_OWNER>,8,MON$LOCAL_TEMPORARY_TABLE_COLUMNS,SYSTEM,NULL,D,1,0,NULL
<DB_OWNER>,8,MON$LOCAL_TEMPORARY_TABLE_COLUMNS,SYSTEM,NULL,R,1,0,NULL
<DB_OWNER>,8,RDB$VIEW_CONTEXT,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$VIEW_CONTEXT,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$CONTEXT_NAME,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$CONTEXT_NAME,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$DESCRIPTION,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$DESCRIPTION,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$EDIT_STRING,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$EDIT_STRING,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$FIELD_ID,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$FIELD_ID,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$FIELD_NAME,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$FIELD_NAME,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$SYSTEM_FLAG,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$SYSTEM_FLAG,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$SYSTEM_NULLFLAG,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$SYSTEM_NULLFLAG,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$INDEX_ID,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$INDEX_ID,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$INDEX_NAME,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$INDEX_NAME,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$FIELD_LENGTH,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$FIELD_LENGTH,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$FIELD_POSITION,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$FIELD_POSITION,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$FIELD_SCALE,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$FIELD_SCALE,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$FIELD_TYPE,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$FIELD_TYPE,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$FORMAT,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$FORMAT,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$DBKEY_LENGTH,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$DBKEY_LENGTH,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$PAGE_NUMBER,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$PAGE_NUMBER,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$PAGE_SEQUENCE,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$PAGE_SEQUENCE,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$PAGE_TYPE,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$PAGE_TYPE,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$QUERY_HEADER,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$QUERY_HEADER,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$RELATION_ID,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$RELATION_ID,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$RELATION_NAME,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$RELATION_NAME,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$SEGMENT_COUNT,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$SEGMENT_COUNT,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$SEGMENT_LENGTH,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$SEGMENT_LENGTH,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$SOURCE,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$SOURCE,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$FIELD_SUB_TYPE,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$FIELD_SUB_TYPE,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$VIEW_BLR,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$VIEW_BLR,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$VALIDATION_BLR,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$VALIDATION_BLR,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$VALUE,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$VALUE,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$SECURITY_CLASS,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$SECURITY_CLASS,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$ACL,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$ACL,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$FILE_NAME,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$FILE_NAME,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$FILE_NAME2,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$FILE_NAME2,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$FILE_SEQUENCE,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$FILE_SEQUENCE,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$FILE_START,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$FILE_START,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$FILE_LENGTH,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$FILE_LENGTH,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$FILE_FLAGS,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$FILE_FLAGS,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$TRIGGER_BLR,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$TRIGGER_BLR,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$TRIGGER_NAME,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$TRIGGER_NAME,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$GENERIC_NAME,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$GENERIC_NAME,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$FUNCTION_NAME,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$FUNCTION_NAME,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$EXTERNAL_NAME,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$EXTERNAL_NAME,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$TYPE_NAME,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$TYPE_NAME,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$DIMENSIONS,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$DIMENSIONS,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$RUNTIME,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$RUNTIME,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$TRIGGER_SEQUENCE,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$TRIGGER_SEQUENCE,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$GENERIC_TYPE,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$GENERIC_TYPE,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$TRIGGER_TYPE,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$TRIGGER_TYPE,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$OBJECT_TYPE,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$OBJECT_TYPE,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$MECHANISM,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$MECHANISM,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$DESCRIPTOR,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$DESCRIPTOR,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$FUNCTION_TYPE,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$FUNCTION_TYPE,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$TRANSACTION_ID,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$TRANSACTION_ID,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$TRANSACTION_STATE,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$TRANSACTION_STATE,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$TIMESTAMP,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$TIMESTAMP,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$TRANSACTION_DESCRIPTION,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$TRANSACTION_DESCRIPTION,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$MESSAGE,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$MESSAGE,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$MESSAGE_NUMBER,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$MESSAGE_NUMBER,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$USER,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$USER,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$PRIVILEGE,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$PRIVILEGE,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$EXTERNAL_DESCRIPTION,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$EXTERNAL_DESCRIPTION,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$SHADOW_NUMBER,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$SHADOW_NUMBER,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$GENERATOR_NAME,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$GENERATOR_NAME,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$GENERATOR_ID,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$GENERATOR_ID,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$BOUND,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$BOUND,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$DIMENSION,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$DIMENSION,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$STATISTICS,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$STATISTICS,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$NULL_FLAG,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$NULL_FLAG,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$CONSTRAINT_NAME,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$CONSTRAINT_NAME,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$CONSTRAINT_TYPE,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$CONSTRAINT_TYPE,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$DEFERRABLE,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$DEFERRABLE,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$MATCH_OPTION,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$MATCH_OPTION,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$RULE,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$RULE,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$FILE_PARTITIONS,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$FILE_PARTITIONS,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$PROCEDURE_BLR,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$PROCEDURE_BLR,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$PROCEDURE_ID,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$PROCEDURE_ID,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$PROCEDURE_PARAMETERS,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$PROCEDURE_PARAMETERS,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$PROCEDURE_NAME,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$PROCEDURE_NAME,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$PARAMETER_NAME,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$PARAMETER_NAME,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$PARAMETER_NUMBER,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$PARAMETER_NUMBER,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$PARAMETER_TYPE,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$PARAMETER_TYPE,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$CHARACTER_SET_NAME,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$CHARACTER_SET_NAME,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$CHARACTER_SET_ID,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$CHARACTER_SET_ID,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$COLLATION_NAME,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$COLLATION_NAME,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$COLLATION_ID,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$COLLATION_ID,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$NUMBER_OF_CHARACTERS,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$NUMBER_OF_CHARACTERS,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$EXCEPTION_NAME,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$EXCEPTION_NAME,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$EXCEPTION_NUMBER,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$EXCEPTION_NUMBER,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$FILE_P_OFFSET,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$FILE_P_OFFSET,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$FIELD_PRECISION,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$FIELD_PRECISION,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$BACKUP_ID,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$BACKUP_ID,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$BACKUP_LEVEL,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$BACKUP_LEVEL,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$GUID,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$GUID,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$SCN,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$SCN,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$SPECIFIC_ATTRIBUTES,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$SPECIFIC_ATTRIBUTES,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$PLUGIN,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$PLUGIN,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$RELATION_TYPE,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$RELATION_TYPE,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$PROCEDURE_TYPE,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$PROCEDURE_TYPE,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$ATTACHMENT_ID,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$ATTACHMENT_ID,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$STATEMENT_ID,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$STATEMENT_ID,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$CALL_ID,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$CALL_ID,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$STAT_ID,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$STAT_ID,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$PID,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$PID,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$STATE,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$STATE,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$ODS_NUMBER,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$ODS_NUMBER,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$PAGE_SIZE,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$PAGE_SIZE,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$PAGE_BUFFERS,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$PAGE_BUFFERS,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$SHUTDOWN_MODE,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$SHUTDOWN_MODE,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$SQL_DIALECT,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$SQL_DIALECT,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$SWEEP_INTERVAL,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$SWEEP_INTERVAL,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$COUNTER,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$COUNTER,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$REMOTE_PROTOCOL,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$REMOTE_PROTOCOL,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$REMOTE_ADDRESS,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$REMOTE_ADDRESS,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$ISOLATION_MODE,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$ISOLATION_MODE,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$LOCK_TIMEOUT,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$LOCK_TIMEOUT,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$BACKUP_STATE,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$BACKUP_STATE,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$STAT_GROUP,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$STAT_GROUP,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$DEBUG_INFO,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$DEBUG_INFO,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$PARAMETER_MECHANISM,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$PARAMETER_MECHANISM,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$SOURCE_INFO,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$SOURCE_INFO,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$CONTEXT_VAR_NAME,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$CONTEXT_VAR_NAME,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$CONTEXT_VAR_VALUE,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$CONTEXT_VAR_VALUE,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$ENGINE_NAME,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$ENGINE_NAME,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$PACKAGE_NAME,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$PACKAGE_NAME,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$FUNCTION_ID,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$FUNCTION_ID,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$FUNCTION_BLR,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$FUNCTION_BLR,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$ARGUMENT_NAME,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$ARGUMENT_NAME,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$ARGUMENT_MECHANISM,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$ARGUMENT_MECHANISM,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$IDENTITY_TYPE,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$IDENTITY_TYPE,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$BOOLEAN,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$BOOLEAN,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,SEC$USER_NAME,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,SEC$USER_NAME,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,SEC$KEY,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,SEC$KEY,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,SEC$VALUE,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,SEC$VALUE,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,SEC$NAME_PART,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,SEC$NAME_PART,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$CLIENT_VERSION,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$CLIENT_VERSION,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$REMOTE_VERSION,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$REMOTE_VERSION,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$HOST_NAME,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$HOST_NAME,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$OS_USER,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$OS_USER,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$GENERATOR_VALUE,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$GENERATOR_VALUE,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$AUTH_METHOD,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$AUTH_METHOD,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$LINGER,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$LINGER,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,MON$SEC_DATABASE,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,MON$SEC_DATABASE,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$MAP_NAME,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$MAP_NAME,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$MAP_USING,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$MAP_USING,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$MAP_DB,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$MAP_DB,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$MAP_FROM_TYPE,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$MAP_FROM_TYPE,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$MAP_FROM,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$MAP_FROM,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$MAP_TO,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$MAP_TO,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$GENERATOR_INCREMENT,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$GENERATOR_INCREMENT,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$PLAN,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$PLAN,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$SYSTEM_PRIVILEGES,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$SYSTEM_PRIVILEGES,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$SQL_SECURITY,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$SQL_SECURITY,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,MON$IDLE_TIMEOUT,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,MON$IDLE_TIMEOUT,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,MON$IDLE_TIMER,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,MON$IDLE_TIMER,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,MON$STATEMENT_TIMEOUT,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,MON$STATEMENT_TIMEOUT,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,MON$STATEMENT_TIMER,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,MON$STATEMENT_TIMER,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$TIME_ZONE_ID,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$TIME_ZONE_ID,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$TIME_ZONE_NAME,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$TIME_ZONE_NAME,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$TIME_ZONE_OFFSET,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$TIME_ZONE_OFFSET,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$TIMESTAMP_TZ,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$TIMESTAMP_TZ,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$DBTZ_VERSION,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$DBTZ_VERSION,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$CRYPT_STATE,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$CRYPT_STATE,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,MON$WIRE_CRYPT_PLUGIN,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,MON$WIRE_CRYPT_PLUGIN,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$PUBLICATION_NAME,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$PUBLICATION_NAME,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$FILE_ID,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$FILE_ID,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$CONFIG_ID,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$CONFIG_ID,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$CONFIG_NAME,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$CONFIG_NAME,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$CONFIG_VALUE,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$CONFIG_VALUE,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$CONFIG_IS_SET,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$CONFIG_IS_SET,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$REPLICA_MODE,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$REPLICA_MODE,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$KEYWORD_NAME,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$KEYWORD_NAME,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$KEYWORD_RESERVED,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$KEYWORD_RESERVED,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$SHORT_DESCRIPTION,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$SHORT_DESCRIPTION,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$SECONDS_INTERVAL,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$SECONDS_INTERVAL,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$PROFILE_SESSION_ID,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$PROFILE_SESSION_ID,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$BLOB_UTIL_HANDLE,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$BLOB_UTIL_HANDLE,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$BLOB,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$BLOB,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$VARBINARY_MAX,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$VARBINARY_MAX,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$INTEGER,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$INTEGER,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,MON$PARALLEL_WORKERS,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,MON$PARALLEL_WORKERS,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$SCHEMA_NAME,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$SCHEMA_NAME,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,RDB$TEXT_MAX,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,RDB$TEXT_MAX,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,MON$TABLE_TYPE,SYSTEM,NULL,G,1,9,NULL
PUBLIC,8,MON$TABLE_TYPE,SYSTEM,NULL,G,0,9,NULL
<DB_OWNER>,8,NONE,SYSTEM,NULL,G,1,11,NULL
PUBLIC,8,NONE,SYSTEM,NULL,G,0,11,NULL
<DB_OWNER>,8,OCTETS,SYSTEM,NULL,G,1,11,NULL
PUBLIC,8,OCTETS,SYSTEM,NULL,G,0,11,NULL
<DB_OWNER>,8,ASCII,SYSTEM,NULL,G,1,11,NULL
PUBLIC,8,ASCII,SYSTEM,NULL,G,0,11,NULL
<DB_OWNER>,8,UNICODE_FSS,SYSTEM,NULL,G,1,11,NULL
PUBLIC,8,UNICODE_FSS,SYSTEM,NULL,G,0,11,NULL
<DB_OWNER>,8,UTF8,SYSTEM,NULL,G,1,11,NULL
PUBLIC,8,UTF8,SYSTEM,NULL,G,0,11,NULL
<DB_OWNER>,8,SJIS_0208,SYSTEM,NULL,G,1,11,NULL
PUBLIC,8,SJIS_0208,SYSTEM,NULL,G,0,11,NULL
<DB_OWNER>,8,EUCJ_0208,SYSTEM,NULL,G,1,11,NULL
PUBLIC,8,EUCJ_0208,SYSTEM,NULL,G,0,11,NULL
<DB_OWNER>,8,DOS437,SYSTEM,NULL,G,1,11,NULL
PUBLIC,8,DOS437,SYSTEM,NULL,G,0,11,NULL
<DB_OWNER>,8,DOS850,SYSTEM,NULL,G,1,11,NULL
PUBLIC,8,DOS850,SYSTEM,NULL,G,0,11,NULL
<DB_OWNER>,8,DOS865,SYSTEM,NULL,G,1,11,NULL
PUBLIC,8,DOS865,SYSTEM,NULL,G,0,11,NULL
<DB_OWNER>,8,ISO8859_1,SYSTEM,NULL,G,1,11,NULL
PUBLIC,8,ISO8859_1,SYSTEM,NULL,G,0,11,NULL
<DB_OWNER>,8,ISO8859_2,SYSTEM,NULL,G,1,11,NULL
PUBLIC,8,ISO8859_2,SYSTEM,NULL,G,0,11,NULL
<DB_OWNER>,8,ISO8859_3,SYSTEM,NULL,G,1,11,NULL
PUBLIC,8,ISO8859_3,SYSTEM,NULL,G,0,11,NULL
<DB_OWNER>,8,ISO8859_4,SYSTEM,NULL,G,1,11,NULL
PUBLIC,8,ISO8859_4,SYSTEM,NULL,G,0,11,NULL
<DB_OWNER>,8,ISO8859_5,SYSTEM,NULL,G,1,11,NULL
PUBLIC,8,ISO8859_5,SYSTEM,NULL,G,0,11,NULL
<DB_OWNER>,8,ISO8859_6,SYSTEM,NULL,G,1,11,NULL
PUBLIC,8,ISO8859_6,SYSTEM,NULL,G,0,11,NULL
<DB_OWNER>,8,ISO8859_7,SYSTEM,NULL,G,1,11,NULL
PUBLIC,8,ISO8859_7,SYSTEM,NULL,G,0,11,NULL
<DB_OWNER>,8,ISO8859_8,SYSTEM,NULL,G,1,11,NULL
PUBLIC,8,ISO8859_8,SYSTEM,NULL,G,0,11,NULL
<DB_OWNER>,8,ISO8859_9,SYSTEM,NULL,G,1,11,NULL
PUBLIC,8,ISO8859_9,SYSTEM,NULL,G,0,11,NULL
<DB_OWNER>,8,ISO8859_13,SYSTEM,NULL,G,1,11,NULL
PUBLIC,8,ISO8859_13,SYSTEM,NULL,G,0,11,NULL
<DB_OWNER>,8,DOS852,SYSTEM,NULL,G,1,11,NULL
PUBLIC,8,DOS852,SYSTEM,NULL,G,0,11,NULL
<DB_OWNER>,8,DOS857,SYSTEM,NULL,G,1,11,NULL
PUBLIC,8,DOS857,SYSTEM,NULL,G,0,11,NULL
<DB_OWNER>,8,DOS860,SYSTEM,NULL,G,1,11,NULL
PUBLIC,8,DOS860,SYSTEM,NULL,G,0,11,NULL
<DB_OWNER>,8,DOS861,SYSTEM,NULL,G,1,11,NULL
PUBLIC,8,DOS861,SYSTEM,NULL,G,0,11,NULL
<DB_OWNER>,8,DOS863,SYSTEM,NULL,G,1,11,NULL
PUBLIC,8,DOS863,SYSTEM,NULL,G,0,11,NULL
<DB_OWNER>,8,CYRL,SYSTEM,NULL,G,1,11,NULL
PUBLIC,8,CYRL,SYSTEM,NULL,G,0,11,NULL
<DB_OWNER>,8,DOS737,SYSTEM,NULL,G,1,11,NULL
PUBLIC,8,DOS737,SYSTEM,NULL,G,0,11,NULL
<DB_OWNER>,8,DOS775,SYSTEM,NULL,G,1,11,NULL
PUBLIC,8,DOS775,SYSTEM,NULL,G,0,11,NULL
<DB_OWNER>,8,DOS858,SYSTEM,NULL,G,1,11,NULL
PUBLIC,8,DOS858,SYSTEM,NULL,G,0,11,NULL
<DB_OWNER>,8,DOS862,SYSTEM,NULL,G,1,11,NULL
PUBLIC,8,DOS862,SYSTEM,NULL,G,0,11,NULL
<DB_OWNER>,8,DOS864,SYSTEM,NULL,G,1,11,NULL
PUBLIC,8,DOS864,SYSTEM,NULL,G,0,11,NULL
<DB_OWNER>,8,DOS866,SYSTEM,NULL,G,1,11,NULL
PUBLIC,8,DOS866,SYSTEM,NULL,G,0,11,NULL
<DB_OWNER>,8,DOS869,SYSTEM,NULL,G,1,11,NULL
PUBLIC,8,DOS869,SYSTEM,NULL,G,0,11,NULL
<DB_OWNER>,8,WIN1250,SYSTEM,NULL,G,1,11,NULL
PUBLIC,8,WIN1250,SYSTEM,NULL,G,0,11,NULL
<DB_OWNER>,8,WIN1251,SYSTEM,NULL,G,1,11,NULL
PUBLIC,8,WIN1251,SYSTEM,NULL,G,0,11,NULL
<DB_OWNER>,8,WIN1252,SYSTEM,NULL,G,1,11,NULL
PUBLIC,8,WIN1252,SYSTEM,NULL,G,0,11,NULL
<DB_OWNER>,8,WIN1253,SYSTEM,NULL,G,1,11,NULL
PUBLIC,8,WIN1253,SYSTEM,NULL,G,0,11,NULL
<DB_OWNER>,8,WIN1254,SYSTEM,NULL,G,1,11,NULL
PUBLIC,8,WIN1254,SYSTEM,NULL,G,0,11,NULL
<DB_OWNER>,8,NEXT,SYSTEM,NULL,G,1,11,NULL
PUBLIC,8,NEXT,SYSTEM,NULL,G,0,11,NULL
<DB_OWNER>,8,WIN1255,SYSTEM,NULL,G,1,11,NULL
PUBLIC,8,WIN1255,SYSTEM,NULL,G,0,11,NULL
<DB_OWNER>,8,WIN1256,SYSTEM,NULL,G,1,11,NULL
PUBLIC,8,WIN1256,SYSTEM,NULL,G,0,11,NULL
<DB_OWNER>,8,WIN1257,SYSTEM,NULL,G,1,11,NULL
PUBLIC,8,WIN1257,SYSTEM,NULL,G,0,11,NULL
<DB_OWNER>,8,KSC_5601,SYSTEM,NULL,G,1,11,NULL
PUBLIC,8,KSC_5601,SYSTEM,NULL,G,0,11,NULL
<DB_OWNER>,8,BIG_5,SYSTEM,NULL,G,1,11,NULL
PUBLIC,8,BIG_5,SYSTEM,NULL,G,0,11,NULL
<DB_OWNER>,8,GB_2312,SYSTEM,NULL,G,1,11,NULL
PUBLIC,8,GB_2312,SYSTEM,NULL,G,0,11,NULL
<DB_OWNER>,8,KOI8R,SYSTEM,NULL,G,1,11,NULL
PUBLIC,8,KOI8R,SYSTEM,NULL,G,0,11,NULL
<DB_OWNER>,8,KOI8U,SYSTEM,NULL,G,1,11,NULL
PUBLIC,8,KOI8U,SYSTEM,NULL,G,0,11,NULL
<DB_OWNER>,8,WIN1258,SYSTEM,NULL,G,1,11,NULL
PUBLIC,8,WIN1258,SYSTEM,NULL,G,0,11,NULL
<DB_OWNER>,8,TIS620,SYSTEM,NULL,G,1,11,NULL
PUBLIC,8,TIS620,SYSTEM,NULL,G,0,11,NULL
<DB_OWNER>,8,GBK,SYSTEM,NULL,G,1,11,NULL
PUBLIC,8,GBK,SYSTEM,NULL,G,0,11,NULL
<DB_OWNER>,8,CP943C,SYSTEM,NULL,G,1,11,NULL
PUBLIC,8,CP943C,SYSTEM,NULL,G,0,11,NULL
<DB_OWNER>,8,GB18030,SYSTEM,NULL,G,1,11,NULL
PUBLIC,8,GB18030,SYSTEM,NULL,G,0,11,NULL
<DB_OWNER>,8,NONE,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,NONE,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,OCTETS,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,OCTETS,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,ASCII,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,ASCII,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,UNICODE_FSS,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,UNICODE_FSS,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,UTF8,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,UTF8,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,UCS_BASIC,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,UCS_BASIC,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,UNICODE,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,UNICODE,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,UNICODE_CI,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,UNICODE_CI,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,UNICODE_CI_AI,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,UNICODE_CI_AI,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,SJIS_0208,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,SJIS_0208,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,EUCJ_0208,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,EUCJ_0208,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,DOS437,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,DOS437,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,PDOX_ASCII,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,PDOX_ASCII,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,PDOX_INTL,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,PDOX_INTL,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,PDOX_SWEDFIN,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,PDOX_SWEDFIN,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,DB_DEU437,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,DB_DEU437,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,DB_ESP437,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,DB_ESP437,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,DB_FIN437,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,DB_FIN437,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,DB_FRA437,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,DB_FRA437,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,DB_ITA437,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,DB_ITA437,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,DB_NLD437,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,DB_NLD437,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,DB_SVE437,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,DB_SVE437,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,DB_UK437,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,DB_UK437,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,DB_US437,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,DB_US437,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,DOS850,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,DOS850,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,DB_FRC850,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,DB_FRC850,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,DB_DEU850,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,DB_DEU850,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,DB_ESP850,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,DB_ESP850,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,DB_FRA850,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,DB_FRA850,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,DB_ITA850,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,DB_ITA850,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,DB_NLD850,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,DB_NLD850,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,DB_PTB850,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,DB_PTB850,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,DB_SVE850,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,DB_SVE850,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,DB_UK850,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,DB_UK850,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,DB_US850,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,DB_US850,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,DOS865,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,DOS865,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,PDOX_NORDAN4,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,PDOX_NORDAN4,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,DB_DAN865,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,DB_DAN865,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,DB_NOR865,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,DB_NOR865,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,ISO8859_1,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,ISO8859_1,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,DA_DA,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,DA_DA,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,DU_NL,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,DU_NL,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,FI_FI,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,FI_FI,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,FR_FR,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,FR_FR,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,FR_CA,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,FR_CA,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,DE_DE,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,DE_DE,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,IS_IS,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,IS_IS,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,IT_IT,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,IT_IT,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,NO_NO,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,NO_NO,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,ES_ES,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,ES_ES,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,SV_SV,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,SV_SV,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,EN_UK,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,EN_UK,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,EN_US,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,EN_US,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,PT_PT,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,PT_PT,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,PT_BR,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,PT_BR,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,ES_ES_CI_AI,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,ES_ES_CI_AI,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,FR_FR_CI_AI,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,FR_FR_CI_AI,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,FR_CA_CI_AI,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,FR_CA_CI_AI,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,ISO8859_2,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,ISO8859_2,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,CS_CZ,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,CS_CZ,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,ISO_HUN,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,ISO_HUN,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,ISO_PLK,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,ISO_PLK,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,ISO8859_3,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,ISO8859_3,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,ISO8859_4,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,ISO8859_4,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,ISO8859_5,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,ISO8859_5,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,ISO8859_6,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,ISO8859_6,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,ISO8859_7,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,ISO8859_7,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,ISO8859_8,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,ISO8859_8,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,ISO8859_9,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,ISO8859_9,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,ISO8859_13,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,ISO8859_13,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,LT_LT,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,LT_LT,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,DOS852,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,DOS852,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,DB_CSY,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,DB_CSY,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,DB_PLK,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,DB_PLK,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,DB_SLO,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,DB_SLO,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,PDOX_CSY,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,PDOX_CSY,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,PDOX_PLK,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,PDOX_PLK,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,PDOX_HUN,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,PDOX_HUN,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,PDOX_SLO,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,PDOX_SLO,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,DOS857,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,DOS857,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,DB_TRK,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,DB_TRK,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,DOS860,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,DOS860,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,DB_PTG860,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,DB_PTG860,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,DOS861,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,DOS861,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,PDOX_ISL,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,PDOX_ISL,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,DOS863,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,DOS863,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,DB_FRC863,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,DB_FRC863,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,CYRL,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,CYRL,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,DB_RUS,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,DB_RUS,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,PDOX_CYRL,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,PDOX_CYRL,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,DOS737,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,DOS737,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,DOS775,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,DOS775,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,DOS858,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,DOS858,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,DOS862,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,DOS862,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,DOS864,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,DOS864,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,DOS866,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,DOS866,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,DOS869,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,DOS869,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,WIN1250,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,WIN1250,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,PXW_CSY,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,PXW_CSY,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,PXW_HUNDC,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,PXW_HUNDC,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,PXW_PLK,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,PXW_PLK,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,PXW_SLOV,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,PXW_SLOV,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,PXW_HUN,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,PXW_HUN,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,BS_BA,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,BS_BA,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,WIN_CZ,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,WIN_CZ,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,WIN_CZ_CI_AI,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,WIN_CZ_CI_AI,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,WIN1251,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,WIN1251,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,PXW_CYRL,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,PXW_CYRL,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,WIN1251_UA,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,WIN1251_UA,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,WIN1252,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,WIN1252,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,PXW_INTL,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,PXW_INTL,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,PXW_INTL850,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,PXW_INTL850,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,PXW_NORDAN4,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,PXW_NORDAN4,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,PXW_SPAN,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,PXW_SPAN,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,PXW_SWEDFIN,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,PXW_SWEDFIN,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,WIN_PTBR,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,WIN_PTBR,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,WIN1253,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,WIN1253,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,PXW_GREEK,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,PXW_GREEK,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,WIN1254,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,WIN1254,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,PXW_TURK,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,PXW_TURK,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,NEXT,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,NEXT,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,NXT_US,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,NXT_US,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,NXT_DEU,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,NXT_DEU,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,NXT_FRA,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,NXT_FRA,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,NXT_ITA,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,NXT_ITA,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,NXT_ESP,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,NXT_ESP,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,WIN1255,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,WIN1255,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,WIN1256,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,WIN1256,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,WIN1257,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,WIN1257,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,WIN1257_EE,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,WIN1257_EE,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,WIN1257_LT,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,WIN1257_LT,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,WIN1257_LV,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,WIN1257_LV,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,KSC_5601,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,KSC_5601,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,KSC_DICTIONARY,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,KSC_DICTIONARY,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,BIG_5,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,BIG_5,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,GB_2312,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,GB_2312,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,KOI8R,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,KOI8R,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,KOI8R_RU,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,KOI8R_RU,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,KOI8U,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,KOI8U,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,KOI8U_UA,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,KOI8U_UA,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,WIN1258,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,WIN1258,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,TIS620,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,TIS620,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,TIS620_UNICODE,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,TIS620_UNICODE,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,GBK,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,GBK,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,GBK_UNICODE,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,GBK_UNICODE,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,CP943C,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,CP943C,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,CP943C_UNICODE,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,CP943C_UNICODE,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,GB18030,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,GB18030,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,GB18030_UNICODE,SYSTEM,NULL,G,1,17,NULL
PUBLIC,8,GB18030_UNICODE,SYSTEM,NULL,G,0,17,NULL
<DB_OWNER>,8,RDB$SECURITY_CLASS,SYSTEM,NULL,G,1,14,NULL
PUBLIC,8,RDB$SECURITY_CLASS,SYSTEM,NULL,G,0,14,NULL
<DB_OWNER>,8,SQL$DEFAULT,SYSTEM,NULL,G,1,14,NULL
PUBLIC,8,SQL$DEFAULT,SYSTEM,NULL,G,0,14,NULL
<DB_OWNER>,8,RDB$PROCEDURES,SYSTEM,NULL,G,1,14,NULL
PUBLIC,8,RDB$PROCEDURES,SYSTEM,NULL,G,0,14,NULL
<DB_OWNER>,8,RDB$EXCEPTIONS,SYSTEM,NULL,G,1,14,NULL
PUBLIC,8,RDB$EXCEPTIONS,SYSTEM,NULL,G,0,14,NULL
<DB_OWNER>,8,RDB$CONSTRAINT_NAME,SYSTEM,NULL,G,1,14,NULL
PUBLIC,8,RDB$CONSTRAINT_NAME,SYSTEM,NULL,G,0,14,NULL
<DB_OWNER>,8,RDB$FIELD_NAME,SYSTEM,NULL,G,1,14,NULL
PUBLIC,8,RDB$FIELD_NAME,SYSTEM,NULL,G,0,14,NULL
<DB_OWNER>,8,RDB$INDEX_NAME,SYSTEM,NULL,G,1,14,NULL
PUBLIC,8,RDB$INDEX_NAME,SYSTEM,NULL,G,0,14,NULL
<DB_OWNER>,8,RDB$TRIGGER_NAME,SYSTEM,NULL,G,1,14,NULL
PUBLIC,8,RDB$TRIGGER_NAME,SYSTEM,NULL,G,0,14,NULL
<DB_OWNER>,8,RDB$BACKUP_HISTORY,SYSTEM,NULL,G,1,14,NULL
PUBLIC,8,RDB$BACKUP_HISTORY,SYSTEM,NULL,G,0,14,NULL
<DB_OWNER>,8,RDB$FUNCTIONS,SYSTEM,NULL,G,1,14,NULL
PUBLIC,8,RDB$FUNCTIONS,SYSTEM,NULL,G,0,14,NULL
<DB_OWNER>,8,RDB$GENERATOR_NAME,SYSTEM,NULL,G,1,14,NULL
PUBLIC,8,RDB$GENERATOR_NAME,SYSTEM,NULL,G,0,14,NULL
<DB_OWNER>,8,RDB$TIME_ZONE_UTIL,SYSTEM,NULL,X,1,18,NULL
PUBLIC,8,RDB$TIME_ZONE_UTIL,SYSTEM,NULL,X,0,18,NULL
<DB_OWNER>,8,RDB$BLOB_UTIL,SYSTEM,NULL,X,1,18,NULL
PUBLIC,8,RDB$BLOB_UTIL,SYSTEM,NULL,X,0,18,NULL
<DB_OWNER>,8,RDB$PROFILER,SYSTEM,NULL,X,1,18,NULL
PUBLIC,8,RDB$PROFILER,SYSTEM,NULL,X,0,18,NULL
<DB_OWNER>,8,RDB$SQL,SYSTEM,NULL,X,1,18,NULL
PUBLIC,8,RDB$SQL,SYSTEM,NULL,X,0,18,NULL
<DB_OWNER>,8,SYSTEM,SYSTEM,NULL,G,0,38,NULL
PUBLIC,8,SYSTEM,SYSTEM,NULL,G,0,38,NULL
<DB_OWNER>,8,PUBLIC,PUBLIC,NULL,G,0,38,<DB_OWNER>
PUBLIC,8,PUBLIC,PUBLIC,NULL,G,0,38,<DB_OWNER>
<DB_OWNER>,8,SQL$TABLES,PUBLIC,NULL,C,0,22,<DB_OWNER>
<DB_OWNER>,8,SQL$TABLES,PUBLIC,NULL,L,0,22,<DB_OWNER>
<DB_OWNER>,8,SQL$TABLES,PUBLIC,NULL,O,0,22,<DB_OWNER>
<DB_OWNER>,8,SQL$TABLES,SYSTEM,NULL,C,0,22,NULL
<DB_OWNER>,8,SQL$TABLES,SYSTEM,NULL,L,0,22,NULL
<DB_OWNER>,8,SQL$TABLES,SYSTEM,NULL,O,0,22,NULL
<DB_OWNER>,8,SQL$VIEWS,PUBLIC,NULL,C,0,23,<DB_OWNER>
<DB_OWNER>,8,SQL$VIEWS,PUBLIC,NULL,L,0,23,<DB_OWNER>
<DB_OWNER>,8,SQL$VIEWS,PUBLIC,NULL,O,0,23,<DB_OWNER>
<DB_OWNER>,8,SQL$VIEWS,SYSTEM,NULL,C,0,23,NULL
<DB_OWNER>,8,SQL$VIEWS,SYSTEM,NULL,L,0,23,NULL
<DB_OWNER>,8,SQL$VIEWS,SYSTEM,NULL,O,0,23,NULL
<DB_OWNER>,8,SQL$PROCEDURES,PUBLIC,NULL,C,0,24,<DB_OWNER>
<DB_OWNER>,8,SQL$PROCEDURES,PUBLIC,NULL,L,0,24,<DB_OWNER>
<DB_OWNER>,8,SQL$PROCEDURES,PUBLIC,NULL,O,0,24,<DB_OWNER>
<DB_OWNER>,8,SQL$PROCEDURES,SYSTEM,NULL,C,0,24,NULL
<DB_OWNER>,8,SQL$PROCEDURES,SYSTEM,NULL,L,0,24,NULL
<DB_OWNER>,8,SQL$PROCEDURES,SYSTEM,NULL,O,0,24,NULL
<DB_OWNER>,8,SQL$FUNCTIONS,PUBLIC,NULL,C,0,25,<DB_OWNER>
<DB_OWNER>,8,SQL$FUNCTIONS,PUBLIC,NULL,L,0,25,<DB_OWNER>
<DB_OWNER>,8,SQL$FUNCTIONS,PUBLIC,NULL,O,0,25,<DB_OWNER>
<DB_OWNER>,8,SQL$FUNCTIONS,SYSTEM,NULL,C,0,25,NULL
<DB_OWNER>,8,SQL$FUNCTIONS,SYSTEM,NULL,L,0,25,NULL
<DB_OWNER>,8,SQL$FUNCTIONS,SYSTEM,NULL,O,0,25,NULL
<DB_OWNER>,8,SQL$PACKAGES,PUBLIC,NULL,C,0,26,<DB_OWNER>
<DB_OWNER>,8,SQL$PACKAGES,PUBLIC,NULL,L,0,26,<DB_OWNER>
<DB_OWNER>,8,SQL$PACKAGES,PUBLIC,NULL,O,0,26,<DB_OWNER>
<DB_OWNER>,8,SQL$PACKAGES,SYSTEM,NULL,C,0,26,NULL
<DB_OWNER>,8,SQL$PACKAGES,SYSTEM,NULL,L,0,26,NULL
<DB_OWNER>,8,SQL$PACKAGES,SYSTEM,NULL,O,0,26,NULL
<DB_OWNER>,8,SQL$GENERATORS,PUBLIC,NULL,C,0,27,<DB_OWNER>
<DB_OWNER>,8,SQL$GENERATORS,PUBLIC,NULL,L,0,27,<DB_OWNER>
<DB_OWNER>,8,SQL$GENERATORS,PUBLIC,NULL,O,0,27,<DB_OWNER>
<DB_OWNER>,8,SQL$GENERATORS,SYSTEM,NULL,C,0,27,NULL
<DB_OWNER>,8,SQL$GENERATORS,SYSTEM,NULL,L,0,27,NULL
<DB_OWNER>,8,SQL$GENERATORS,SYSTEM,NULL,O,0,27,NULL
<DB_OWNER>,8,SQL$DOMAINS,PUBLIC,NULL,C,0,28,<DB_OWNER>
<DB_OWNER>,8,SQL$DOMAINS,PUBLIC,NULL,L,0,28,<DB_OWNER>
<DB_OWNER>,8,SQL$DOMAINS,PUBLIC,NULL,O,0,28,<DB_OWNER>
<DB_OWNER>,8,SQL$DOMAINS,SYSTEM,NULL,C,0,28,NULL
<DB_OWNER>,8,SQL$DOMAINS,SYSTEM,NULL,L,0,28,NULL
<DB_OWNER>,8,SQL$DOMAINS,SYSTEM,NULL,O,0,28,NULL
<DB_OWNER>,8,SQL$EXCEPTIONS,PUBLIC,NULL,C,0,29,<DB_OWNER>
<DB_OWNER>,8,SQL$EXCEPTIONS,PUBLIC,NULL,L,0,29,<DB_OWNER>
<DB_OWNER>,8,SQL$EXCEPTIONS,PUBLIC,NULL,O,0,29,<DB_OWNER>
<DB_OWNER>,8,SQL$EXCEPTIONS,SYSTEM,NULL,C,0,29,NULL
<DB_OWNER>,8,SQL$EXCEPTIONS,SYSTEM,NULL,L,0,29,NULL
<DB_OWNER>,8,SQL$EXCEPTIONS,SYSTEM,NULL,O,0,29,NULL
<DB_OWNER>,8,SQL$ROLES,NULL,NULL,C,0,30,NULL
<DB_OWNER>,8,SQL$ROLES,NULL,NULL,L,0,30,NULL
<DB_OWNER>,8,SQL$ROLES,NULL,NULL,O,0,30,NULL
<DB_OWNER>,8,SQL$CHARSETS,PUBLIC,NULL,C,0,31,<DB_OWNER>
<DB_OWNER>,8,SQL$CHARSETS,PUBLIC,NULL,L,0,31,<DB_OWNER>
<DB_OWNER>,8,SQL$CHARSETS,PUBLIC,NULL,O,0,31,<DB_OWNER>
<DB_OWNER>,8,SQL$CHARSETS,SYSTEM,NULL,C,0,31,NULL
<DB_OWNER>,8,SQL$CHARSETS,SYSTEM,NULL,L,0,31,NULL
<DB_OWNER>,8,SQL$CHARSETS,SYSTEM,NULL,O,0,31,NULL
<DB_OWNER>,8,SQL$COLLATIONS,PUBLIC,NULL,C,0,32,<DB_OWNER>
<DB_OWNER>,8,SQL$COLLATIONS,PUBLIC,NULL,L,0,32,<DB_OWNER>
<DB_OWNER>,8,SQL$COLLATIONS,PUBLIC,NULL,O,0,32,<DB_OWNER>
<DB_OWNER>,8,SQL$COLLATIONS,SYSTEM,NULL,C,0,32,NULL
<DB_OWNER>,8,SQL$COLLATIONS,SYSTEM,NULL,L,0,32,NULL
<DB_OWNER>,8,SQL$COLLATIONS,SYSTEM,NULL,O,0,32,NULL
<DB_OWNER>,8,SQL$FILTERS,NULL,NULL,C,0,33,NULL
<DB_OWNER>,8,SQL$FILTERS,NULL,NULL,L,0,33,NULL
<DB_OWNER>,8,SQL$FILTERS,NULL,NULL,O,0,33,NULL
<DB_OWNER>,8,SQL$JOBS,NULL,NULL,C,0,34,NULL
<DB_OWNER>,8,SQL$JOBS,NULL,NULL,L,0,34,NULL
<DB_OWNER>,8,SQL$JOBS,NULL,NULL,O,0,34,NULL
<DB_OWNER>,8,SQL$TABLESPACES,NULL,NULL,C,0,36,NULL
<DB_OWNER>,8,SQL$TABLESPACES,NULL,NULL,L,0,36,NULL
<DB_OWNER>,8,SQL$TABLESPACES,NULL,NULL,O,0,36,NULL
<DB_OWNER>,8,SQL$SCHEMAS,NULL,NULL,C,0,39,NULL
<DB_OWNER>,8,SQL$SCHEMAS,NULL,NULL,L,0,39,NULL
<DB_OWNER>,8,SQL$SCHEMAS,NULL,NULL,O,0,39,NULL
SYSDBA,8,RDB$ADMIN,NULL,D,M,2,13,NULL
<DB_OWNER>,8,RDB$ADMIN,NULL,D,M,2,13,NULL
4,20,RDB$BACKUP_HISTORY,SYSTEM,NULL,S,0,0,NULL
4,20,RDB$BACKUP_HISTORY,SYSTEM,NULL,I,0,0,NULL
4,20,RDB$BACKUP_HISTORY,SYSTEM,NULL,U,0,0,NULL
4,20,RDB$BACKUP_HISTORY,SYSTEM,NULL,D,0,0,NULL
4,20,RDB$BACKUP_HISTORY,SYSTEM,NULL,R,0,0,NULL
3,20,RDB$TYPES,SYSTEM,NULL,S,0,0,NULL
3,20,RDB$TYPES,SYSTEM,NULL,I,0,0,NULL
3,20,RDB$TYPES,SYSTEM,NULL,U,0,0,NULL
3,20,RDB$TYPES,SYSTEM,NULL,D,0,0,NULL
3,20,RDB$TYPES,SYSTEM,NULL,R,0,0,NULL
22,20,RDB$DB_CREATORS,SYSTEM,NULL,S,0,0,NULL
22,20,RDB$DB_CREATORS,SYSTEM,NULL,I,0,0,NULL
22,20,RDB$DB_CREATORS,SYSTEM,NULL,U,0,0,NULL
22,20,RDB$DB_CREATORS,SYSTEM,NULL,D,0,0,NULL
22,20,RDB$DB_CREATORS,SYSTEM,NULL,R,0,0,NULL
```


#### Binary Blob Encoding (Authoritative)
Binary blob columns added in this section use **hex byte lists** with `0x`-prefixed bytes separated by spaces. The bytes are stored exactly in that order.

#### RDB$INDICES (Default Rows)
```csv
RDB$SCHEMA_NAME,RDB$INDEX_NAME,RDB$RELATION_NAME,RDB$INDEX_ID,RDB$UNIQUE_FLAG,RDB$SEGMENT_COUNT,RDB$INDEX_TYPE,RDB$SYSTEM_FLAG,RDB$INDEX_INACTIVE
SYSTEM,RDB$INDEX_0,RDB$RELATIONS,1,0,1,NULL,1,0
SYSTEM,RDB$INDEX_1,RDB$RELATIONS,2,0,1,NULL,1,0
SYSTEM,RDB$INDEX_2,RDB$FIELDS,3,0,1,NULL,1,0
SYSTEM,RDB$INDEX_3,RDB$RELATION_FIELDS,4,0,1,NULL,1,0
SYSTEM,RDB$INDEX_4,RDB$RELATION_FIELDS,5,0,1,NULL,1,0
SYSTEM,RDB$INDEX_5,RDB$INDICES,6,0,1,NULL,1,0
SYSTEM,RDB$INDEX_6,RDB$INDEX_SEGMENTS,7,0,1,NULL,1,0
SYSTEM,RDB$INDEX_7,RDB$SECURITY_CLASSES,8,1,1,NULL,1,0
SYSTEM,RDB$INDEX_8,RDB$TRIGGERS,9,0,1,NULL,1,0
SYSTEM,RDB$INDEX_9,RDB$FUNCTIONS,10,0,2,NULL,1,0
SYSTEM,RDB$INDEX_10,RDB$FUNCTION_ARGUMENTS,11,0,2,NULL,1,0
SYSTEM,RDB$INDEX_11,RDB$GENERATORS,12,0,1,NULL,1,0
SYSTEM,RDB$INDEX_12,RDB$RELATION_CONSTRAINTS,13,0,1,NULL,1,0
SYSTEM,RDB$INDEX_13,RDB$REF_CONSTRAINTS,14,0,1,NULL,1,0
SYSTEM,RDB$INDEX_14,RDB$CHECK_CONSTRAINTS,15,0,1,NULL,1,0
SYSTEM,RDB$INDEX_15,RDB$RELATION_FIELDS,16,0,2,NULL,1,0
SYSTEM,RDB$INDEX_16,RDB$FORMATS,17,0,2,NULL,1,0
SYSTEM,RDB$INDEX_17,RDB$FILTERS,18,1,2,NULL,1,0
SYSTEM,RDB$INDEX_18,RDB$PROCEDURE_PARAMETERS,19,0,3,NULL,1,0
SYSTEM,RDB$INDEX_19,RDB$CHARACTER_SETS,20,0,1,NULL,1,0
SYSTEM,RDB$INDEX_20,RDB$COLLATIONS,21,0,1,NULL,1,0
SYSTEM,RDB$INDEX_21,RDB$PROCEDURES,22,0,2,NULL,1,0
SYSTEM,RDB$INDEX_22,RDB$PROCEDURES,23,1,1,NULL,1,0
SYSTEM,RDB$INDEX_23,RDB$EXCEPTIONS,24,0,1,NULL,1,0
SYSTEM,RDB$INDEX_24,RDB$EXCEPTIONS,25,1,1,NULL,1,0
SYSTEM,RDB$INDEX_25,RDB$CHARACTER_SETS,26,1,1,NULL,1,0
SYSTEM,RDB$INDEX_26,RDB$COLLATIONS,27,1,2,NULL,1,0
SYSTEM,RDB$INDEX_27,RDB$DEPENDENCIES,28,0,2,NULL,1,0
SYSTEM,RDB$INDEX_28,RDB$DEPENDENCIES,29,0,3,NULL,1,0
SYSTEM,RDB$INDEX_29,RDB$USER_PRIVILEGES,30,0,1,NULL,1,0
SYSTEM,RDB$INDEX_30,RDB$USER_PRIVILEGES,31,0,1,NULL,1,0
SYSTEM,RDB$INDEX_31,RDB$INDICES,32,0,1,NULL,1,0
SYSTEM,RDB$INDEX_32,RDB$TRANSACTIONS,33,1,1,NULL,1,0
SYSTEM,RDB$INDEX_33,RDB$VIEW_RELATIONS,34,0,1,NULL,1,0
SYSTEM,RDB$INDEX_34,RDB$VIEW_RELATIONS,35,0,1,NULL,1,0
SYSTEM,RDB$INDEX_35,RDB$TRIGGER_MESSAGES,36,0,1,NULL,1,0
SYSTEM,RDB$INDEX_36,RDB$FIELD_DIMENSIONS,37,0,1,NULL,1,0
SYSTEM,RDB$INDEX_37,RDB$TYPES,38,0,1,NULL,1,0
SYSTEM,RDB$INDEX_38,RDB$TRIGGERS,39,0,1,NULL,1,0
SYSTEM,RDB$INDEX_39,RDB$ROLES,40,1,1,NULL,1,0
SYSTEM,RDB$INDEX_40,RDB$CHECK_CONSTRAINTS,41,0,1,NULL,1,0
SYSTEM,RDB$INDEX_41,RDB$INDICES,42,0,1,NULL,1,0
SYSTEM,RDB$INDEX_42,RDB$RELATION_CONSTRAINTS,43,0,2,NULL,1,0
SYSTEM,RDB$INDEX_43,RDB$RELATION_CONSTRAINTS,44,0,1,NULL,1,0
SYSTEM,RDB$INDEX_44,RDB$BACKUP_HISTORY,45,1,2,1,1,0
SYSTEM,RDB$INDEX_45,RDB$FILTERS,46,0,1,NULL,1,0
SYSTEM,RDB$INDEX_46,RDB$GENERATORS,47,1,1,NULL,1,0
SYSTEM,RDB$INDEX_47,RDB$PACKAGES,48,0,1,NULL,1,0
SYSTEM,RDB$INDEX_48,RDB$PROCEDURE_PARAMETERS,49,0,1,NULL,1,0
SYSTEM,RDB$INDEX_49,RDB$FUNCTION_ARGUMENTS,50,0,1,NULL,1,0
SYSTEM,RDB$INDEX_50,RDB$PROCEDURE_PARAMETERS,51,0,2,NULL,1,0
SYSTEM,RDB$INDEX_51,RDB$FUNCTION_ARGUMENTS,52,0,2,NULL,1,0
SYSTEM,RDB$INDEX_52,RDB$AUTH_MAPPING,53,0,1,NULL,1,0
SYSTEM,RDB$INDEX_53,RDB$FUNCTIONS,54,1,1,NULL,1,0
SYSTEM,RDB$INDEX_54,RDB$BACKUP_HISTORY,55,1,1,NULL,1,0
SYSTEM,RDB$INDEX_55,RDB$PUBLICATIONS,56,1,1,NULL,1,0
SYSTEM,RDB$INDEX_56,RDB$PUBLICATION_TABLES,57,0,2,NULL,1,0
SYSTEM,RDB$INDEX_57,RDB$BACKUP_HISTORY,58,0,1,1,1,0
SYSTEM,RDB$INDEX_58,RDB$SCHEMAS,59,1,1,NULL,1,0
SYSTEM,RDB$INDEX_59,RDB$RELATIONS,60,1,2,NULL,1,0
SYSTEM,RDB$INDEX_60,RDB$FIELDS,61,1,2,NULL,1,0
SYSTEM,RDB$INDEX_61,RDB$RELATION_FIELDS,62,0,2,NULL,1,0
SYSTEM,RDB$INDEX_62,RDB$RELATION_FIELDS,63,0,2,NULL,1,0
SYSTEM,RDB$INDEX_63,RDB$INDICES,64,1,2,NULL,1,0
SYSTEM,RDB$INDEX_64,RDB$INDEX_SEGMENTS,65,0,2,NULL,1,0
SYSTEM,RDB$INDEX_65,RDB$TRIGGERS,66,1,2,NULL,1,0
SYSTEM,RDB$INDEX_66,RDB$FUNCTIONS,67,1,3,NULL,1,0
SYSTEM,RDB$INDEX_67,RDB$FUNCTION_ARGUMENTS,68,0,3,NULL,1,0
SYSTEM,RDB$INDEX_68,RDB$GENERATORS,69,1,2,NULL,1,0
SYSTEM,RDB$INDEX_69,RDB$RELATION_CONSTRAINTS,70,1,2,NULL,1,0
SYSTEM,RDB$INDEX_70,RDB$REF_CONSTRAINTS,71,1,2,NULL,1,0
SYSTEM,RDB$INDEX_71,RDB$CHECK_CONSTRAINTS,72,0,2,NULL,1,0
SYSTEM,RDB$INDEX_72,RDB$RELATION_FIELDS,73,1,3,NULL,1,0
SYSTEM,RDB$INDEX_73,RDB$PROCEDURE_PARAMETERS,74,1,4,NULL,1,0
SYSTEM,RDB$INDEX_74,RDB$CHARACTER_SETS,75,1,2,NULL,1,0
SYSTEM,RDB$INDEX_75,RDB$COLLATIONS,76,1,2,NULL,1,0
SYSTEM,RDB$INDEX_76,RDB$PROCEDURES,77,1,3,NULL,1,0
SYSTEM,RDB$INDEX_77,RDB$EXCEPTIONS,78,1,2,NULL,1,0
SYSTEM,RDB$INDEX_78,RDB$DEPENDENCIES,79,0,3,NULL,1,0
SYSTEM,RDB$INDEX_79,RDB$DEPENDENCIES,80,0,4,NULL,1,0
SYSTEM,RDB$INDEX_80,RDB$USER_PRIVILEGES,81,0,2,NULL,1,0
SYSTEM,RDB$INDEX_81,RDB$USER_PRIVILEGES,82,0,2,NULL,1,0
SYSTEM,RDB$INDEX_82,RDB$INDICES,83,0,2,NULL,1,0
SYSTEM,RDB$INDEX_83,RDB$VIEW_RELATIONS,84,0,2,NULL,1,0
SYSTEM,RDB$INDEX_84,RDB$VIEW_RELATIONS,85,0,2,NULL,1,0
SYSTEM,RDB$INDEX_85,RDB$TRIGGER_MESSAGES,86,0,2,NULL,1,0
SYSTEM,RDB$INDEX_86,RDB$FIELD_DIMENSIONS,87,0,2,NULL,1,0
SYSTEM,RDB$INDEX_87,RDB$TRIGGERS,88,0,2,NULL,1,0
SYSTEM,RDB$INDEX_88,RDB$CHECK_CONSTRAINTS,89,0,2,NULL,1,0
SYSTEM,RDB$INDEX_89,RDB$INDICES,90,0,2,NULL,1,0
SYSTEM,RDB$INDEX_90,RDB$RELATION_CONSTRAINTS,91,0,3,NULL,1,0
SYSTEM,RDB$INDEX_91,RDB$RELATION_CONSTRAINTS,92,0,2,NULL,1,0
SYSTEM,RDB$INDEX_92,RDB$PACKAGES,93,1,2,NULL,1,0
SYSTEM,RDB$INDEX_93,RDB$PROCEDURE_PARAMETERS,94,0,2,NULL,1,0
SYSTEM,RDB$INDEX_94,RDB$FUNCTION_ARGUMENTS,95,0,2,NULL,1,0
SYSTEM,RDB$INDEX_95,RDB$PROCEDURE_PARAMETERS,96,0,3,NULL,1,0
SYSTEM,RDB$INDEX_96,RDB$FUNCTION_ARGUMENTS,97,0,3,NULL,1,0
SYSTEM,RDB$INDEX_97,RDB$PUBLICATION_TABLES,98,1,3,NULL,1,0
```

#### RDB$INDEX_SEGMENTS (Default Rows)
```csv
RDB$SCHEMA_NAME,RDB$INDEX_NAME,RDB$FIELD_NAME,RDB$FIELD_POSITION
SYSTEM,RDB$INDEX_0,RDB$RELATION_NAME,0
SYSTEM,RDB$INDEX_1,RDB$RELATION_ID,0
SYSTEM,RDB$INDEX_2,RDB$FIELD_NAME,0
SYSTEM,RDB$INDEX_3,RDB$FIELD_SOURCE,0
SYSTEM,RDB$INDEX_4,RDB$RELATION_NAME,0
SYSTEM,RDB$INDEX_5,RDB$INDEX_NAME,0
SYSTEM,RDB$INDEX_6,RDB$INDEX_NAME,0
SYSTEM,RDB$INDEX_7,RDB$SECURITY_CLASS,0
SYSTEM,RDB$INDEX_8,RDB$TRIGGER_NAME,0
SYSTEM,RDB$INDEX_9,RDB$PACKAGE_NAME,0
SYSTEM,RDB$INDEX_9,RDB$FUNCTION_NAME,1
SYSTEM,RDB$INDEX_10,RDB$PACKAGE_NAME,0
SYSTEM,RDB$INDEX_10,RDB$FUNCTION_NAME,1
SYSTEM,RDB$INDEX_11,RDB$GENERATOR_NAME,0
SYSTEM,RDB$INDEX_12,RDB$CONSTRAINT_NAME,0
SYSTEM,RDB$INDEX_13,RDB$CONSTRAINT_NAME,0
SYSTEM,RDB$INDEX_14,RDB$CONSTRAINT_NAME,0
SYSTEM,RDB$INDEX_15,RDB$FIELD_NAME,0
SYSTEM,RDB$INDEX_15,RDB$RELATION_NAME,1
SYSTEM,RDB$INDEX_16,RDB$RELATION_ID,0
SYSTEM,RDB$INDEX_16,RDB$FORMAT,1
SYSTEM,RDB$INDEX_17,RDB$INPUT_SUB_TYPE,0
SYSTEM,RDB$INDEX_17,RDB$OUTPUT_SUB_TYPE,1
SYSTEM,RDB$INDEX_18,RDB$PACKAGE_NAME,0
SYSTEM,RDB$INDEX_18,RDB$PROCEDURE_NAME,1
SYSTEM,RDB$INDEX_18,RDB$PARAMETER_NAME,2
SYSTEM,RDB$INDEX_19,RDB$CHARACTER_SET_NAME,0
SYSTEM,RDB$INDEX_20,RDB$COLLATION_NAME,0
SYSTEM,RDB$INDEX_21,RDB$PACKAGE_NAME,0
SYSTEM,RDB$INDEX_21,RDB$PROCEDURE_NAME,1
SYSTEM,RDB$INDEX_22,RDB$PROCEDURE_ID,0
SYSTEM,RDB$INDEX_23,RDB$EXCEPTION_NAME,0
SYSTEM,RDB$INDEX_24,RDB$EXCEPTION_NUMBER,0
SYSTEM,RDB$INDEX_25,RDB$CHARACTER_SET_ID,0
SYSTEM,RDB$INDEX_26,RDB$COLLATION_ID,0
SYSTEM,RDB$INDEX_26,RDB$CHARACTER_SET_ID,1
SYSTEM,RDB$INDEX_27,RDB$DEPENDENT_NAME,0
SYSTEM,RDB$INDEX_27,RDB$DEPENDENT_TYPE,1
SYSTEM,RDB$INDEX_28,RDB$DEPENDED_ON_NAME,0
SYSTEM,RDB$INDEX_28,RDB$DEPENDED_ON_TYPE,1
SYSTEM,RDB$INDEX_28,RDB$FIELD_NAME,2
SYSTEM,RDB$INDEX_29,RDB$RELATION_NAME,0
SYSTEM,RDB$INDEX_30,RDB$USER,0
SYSTEM,RDB$INDEX_31,RDB$RELATION_NAME,0
SYSTEM,RDB$INDEX_32,RDB$TRANSACTION_ID,0
SYSTEM,RDB$INDEX_33,RDB$VIEW_NAME,0
SYSTEM,RDB$INDEX_34,RDB$RELATION_NAME,0
SYSTEM,RDB$INDEX_35,RDB$TRIGGER_NAME,0
SYSTEM,RDB$INDEX_36,RDB$FIELD_NAME,0
SYSTEM,RDB$INDEX_37,RDB$TYPE_NAME,0
SYSTEM,RDB$INDEX_38,RDB$RELATION_NAME,0
SYSTEM,RDB$INDEX_39,RDB$ROLE_NAME,0
SYSTEM,RDB$INDEX_40,RDB$TRIGGER_NAME,0
SYSTEM,RDB$INDEX_41,RDB$FOREIGN_KEY,0
SYSTEM,RDB$INDEX_42,RDB$RELATION_NAME,0
SYSTEM,RDB$INDEX_42,RDB$CONSTRAINT_TYPE,1
SYSTEM,RDB$INDEX_43,RDB$INDEX_NAME,0
SYSTEM,RDB$INDEX_44,RDB$BACKUP_LEVEL,0
SYSTEM,RDB$INDEX_44,RDB$BACKUP_ID,1
SYSTEM,RDB$INDEX_45,RDB$FUNCTION_NAME,0
SYSTEM,RDB$INDEX_46,RDB$GENERATOR_ID,0
SYSTEM,RDB$INDEX_47,RDB$PACKAGE_NAME,0
SYSTEM,RDB$INDEX_48,RDB$FIELD_SOURCE,0
SYSTEM,RDB$INDEX_49,RDB$FIELD_SOURCE,0
SYSTEM,RDB$INDEX_50,RDB$RELATION_NAME,0
SYSTEM,RDB$INDEX_50,RDB$FIELD_NAME,1
SYSTEM,RDB$INDEX_51,RDB$RELATION_NAME,0
SYSTEM,RDB$INDEX_51,RDB$FIELD_NAME,1
SYSTEM,RDB$INDEX_52,RDB$MAP_NAME,0
SYSTEM,RDB$INDEX_53,RDB$FUNCTION_ID,0
SYSTEM,RDB$INDEX_54,RDB$GUID,0
SYSTEM,RDB$INDEX_55,RDB$PUBLICATION_NAME,0
SYSTEM,RDB$INDEX_56,RDB$TABLE_NAME,0
SYSTEM,RDB$INDEX_56,RDB$PUBLICATION_NAME,1
SYSTEM,RDB$INDEX_57,RDB$TIMESTAMP,0
SYSTEM,RDB$INDEX_58,RDB$SCHEMA_NAME,0
SYSTEM,RDB$INDEX_59,RDB$SCHEMA_NAME,0
SYSTEM,RDB$INDEX_59,RDB$RELATION_NAME,1
SYSTEM,RDB$INDEX_60,RDB$SCHEMA_NAME,0
SYSTEM,RDB$INDEX_60,RDB$FIELD_NAME,1
SYSTEM,RDB$INDEX_61,RDB$FIELD_SOURCE_SCHEMA_NAME,0
SYSTEM,RDB$INDEX_61,RDB$FIELD_SOURCE,1
SYSTEM,RDB$INDEX_62,RDB$SCHEMA_NAME,0
SYSTEM,RDB$INDEX_62,RDB$RELATION_NAME,1
SYSTEM,RDB$INDEX_63,RDB$SCHEMA_NAME,0
SYSTEM,RDB$INDEX_63,RDB$INDEX_NAME,1
SYSTEM,RDB$INDEX_64,RDB$SCHEMA_NAME,0
SYSTEM,RDB$INDEX_64,RDB$INDEX_NAME,1
SYSTEM,RDB$INDEX_65,RDB$SCHEMA_NAME,0
SYSTEM,RDB$INDEX_65,RDB$TRIGGER_NAME,1
SYSTEM,RDB$INDEX_66,RDB$SCHEMA_NAME,0
SYSTEM,RDB$INDEX_66,RDB$PACKAGE_NAME,1
SYSTEM,RDB$INDEX_66,RDB$FUNCTION_NAME,2
SYSTEM,RDB$INDEX_67,RDB$SCHEMA_NAME,0
SYSTEM,RDB$INDEX_67,RDB$PACKAGE_NAME,1
SYSTEM,RDB$INDEX_67,RDB$FUNCTION_NAME,2
SYSTEM,RDB$INDEX_68,RDB$SCHEMA_NAME,0
SYSTEM,RDB$INDEX_68,RDB$GENERATOR_NAME,1
SYSTEM,RDB$INDEX_69,RDB$SCHEMA_NAME,0
SYSTEM,RDB$INDEX_69,RDB$CONSTRAINT_NAME,1
SYSTEM,RDB$INDEX_70,RDB$SCHEMA_NAME,0
SYSTEM,RDB$INDEX_70,RDB$CONSTRAINT_NAME,1
SYSTEM,RDB$INDEX_71,RDB$SCHEMA_NAME,0
SYSTEM,RDB$INDEX_71,RDB$CONSTRAINT_NAME,1
SYSTEM,RDB$INDEX_72,RDB$FIELD_NAME,0
SYSTEM,RDB$INDEX_72,RDB$SCHEMA_NAME,1
SYSTEM,RDB$INDEX_72,RDB$RELATION_NAME,2
SYSTEM,RDB$INDEX_73,RDB$SCHEMA_NAME,0
SYSTEM,RDB$INDEX_73,RDB$PACKAGE_NAME,1
SYSTEM,RDB$INDEX_73,RDB$PROCEDURE_NAME,2
SYSTEM,RDB$INDEX_73,RDB$PARAMETER_NAME,3
SYSTEM,RDB$INDEX_74,RDB$SCHEMA_NAME,0
SYSTEM,RDB$INDEX_74,RDB$CHARACTER_SET_NAME,1
SYSTEM,RDB$INDEX_75,RDB$SCHEMA_NAME,0
SYSTEM,RDB$INDEX_75,RDB$COLLATION_NAME,1
SYSTEM,RDB$INDEX_76,RDB$SCHEMA_NAME,0
SYSTEM,RDB$INDEX_76,RDB$PACKAGE_NAME,1
SYSTEM,RDB$INDEX_76,RDB$PROCEDURE_NAME,2
SYSTEM,RDB$INDEX_77,RDB$SCHEMA_NAME,0
SYSTEM,RDB$INDEX_77,RDB$EXCEPTION_NAME,1
SYSTEM,RDB$INDEX_78,RDB$DEPENDENT_SCHEMA_NAME,0
SYSTEM,RDB$INDEX_78,RDB$DEPENDENT_NAME,1
SYSTEM,RDB$INDEX_78,RDB$DEPENDENT_TYPE,2
SYSTEM,RDB$INDEX_79,RDB$DEPENDED_ON_SCHEMA_NAME,0
SYSTEM,RDB$INDEX_79,RDB$DEPENDED_ON_NAME,1
SYSTEM,RDB$INDEX_79,RDB$DEPENDED_ON_TYPE,2
SYSTEM,RDB$INDEX_79,RDB$FIELD_NAME,3
SYSTEM,RDB$INDEX_80,RDB$RELATION_SCHEMA_NAME,0
SYSTEM,RDB$INDEX_80,RDB$RELATION_NAME,1
SYSTEM,RDB$INDEX_81,RDB$USER_SCHEMA_NAME,0
SYSTEM,RDB$INDEX_81,RDB$USER,1
SYSTEM,RDB$INDEX_82,RDB$SCHEMA_NAME,0
SYSTEM,RDB$INDEX_82,RDB$RELATION_NAME,1
SYSTEM,RDB$INDEX_83,RDB$SCHEMA_NAME,0
SYSTEM,RDB$INDEX_83,RDB$VIEW_NAME,1
SYSTEM,RDB$INDEX_84,RDB$RELATION_SCHEMA_NAME,0
SYSTEM,RDB$INDEX_84,RDB$RELATION_NAME,1
SYSTEM,RDB$INDEX_85,RDB$SCHEMA_NAME,0
SYSTEM,RDB$INDEX_85,RDB$TRIGGER_NAME,1
SYSTEM,RDB$INDEX_86,RDB$SCHEMA_NAME,0
SYSTEM,RDB$INDEX_86,RDB$FIELD_NAME,1
SYSTEM,RDB$INDEX_87,RDB$SCHEMA_NAME,0
SYSTEM,RDB$INDEX_87,RDB$RELATION_NAME,1
SYSTEM,RDB$INDEX_88,RDB$SCHEMA_NAME,0
SYSTEM,RDB$INDEX_88,RDB$TRIGGER_NAME,1
SYSTEM,RDB$INDEX_89,RDB$FOREIGN_KEY_SCHEMA_NAME,0
SYSTEM,RDB$INDEX_89,RDB$FOREIGN_KEY,1
SYSTEM,RDB$INDEX_90,RDB$SCHEMA_NAME,0
SYSTEM,RDB$INDEX_90,RDB$RELATION_NAME,1
SYSTEM,RDB$INDEX_90,RDB$CONSTRAINT_TYPE,2
SYSTEM,RDB$INDEX_91,RDB$SCHEMA_NAME,0
SYSTEM,RDB$INDEX_91,RDB$INDEX_NAME,1
SYSTEM,RDB$INDEX_92,RDB$SCHEMA_NAME,0
SYSTEM,RDB$INDEX_92,RDB$PACKAGE_NAME,1
SYSTEM,RDB$INDEX_93,RDB$FIELD_SOURCE_SCHEMA_NAME,0
SYSTEM,RDB$INDEX_93,RDB$FIELD_SOURCE,1
SYSTEM,RDB$INDEX_94,RDB$FIELD_SOURCE_SCHEMA_NAME,0
SYSTEM,RDB$INDEX_94,RDB$FIELD_SOURCE,1
SYSTEM,RDB$INDEX_95,RDB$RELATION_SCHEMA_NAME,0
SYSTEM,RDB$INDEX_95,RDB$RELATION_NAME,1
SYSTEM,RDB$INDEX_95,RDB$FIELD_NAME,2
SYSTEM,RDB$INDEX_96,RDB$RELATION_SCHEMA_NAME,0
SYSTEM,RDB$INDEX_96,RDB$RELATION_NAME,1
SYSTEM,RDB$INDEX_96,RDB$FIELD_NAME,2
SYSTEM,RDB$INDEX_97,RDB$TABLE_SCHEMA_NAME,0
SYSTEM,RDB$INDEX_97,RDB$TABLE_NAME,1
SYSTEM,RDB$INDEX_97,RDB$PUBLICATION_NAME,2
```

#### RDB$RELATION_CONSTRAINTS (Default Rows)
```csv
RDB$SCHEMA_NAME,RDB$CONSTRAINT_NAME,RDB$INDEX_NAME,RDB$RELATION_NAME,RDB$CONSTRAINT_TYPE,RDB$DEFERRABLE,RDB$INITIALLY_DEFERRED
SYSTEM,RDB$INDEX_7,RDB$INDEX_7,RDB$SECURITY_CLASSES,UNIQUE,NO,NO
SYSTEM,RDB$INDEX_17,RDB$INDEX_17,RDB$FILTERS,UNIQUE,NO,NO
SYSTEM,RDB$INDEX_22,RDB$INDEX_22,RDB$PROCEDURES,UNIQUE,NO,NO
SYSTEM,RDB$INDEX_24,RDB$INDEX_24,RDB$EXCEPTIONS,UNIQUE,NO,NO
SYSTEM,RDB$INDEX_25,RDB$INDEX_25,RDB$CHARACTER_SETS,UNIQUE,NO,NO
SYSTEM,RDB$INDEX_26,RDB$INDEX_26,RDB$COLLATIONS,UNIQUE,NO,NO
SYSTEM,RDB$INDEX_32,RDB$INDEX_32,RDB$TRANSACTIONS,UNIQUE,NO,NO
SYSTEM,RDB$INDEX_39,RDB$INDEX_39,RDB$ROLES,UNIQUE,NO,NO
SYSTEM,RDB$INDEX_44,RDB$INDEX_44,RDB$BACKUP_HISTORY,UNIQUE,NO,NO
SYSTEM,RDB$INDEX_46,RDB$INDEX_46,RDB$GENERATORS,UNIQUE,NO,NO
SYSTEM,RDB$INDEX_53,RDB$INDEX_53,RDB$FUNCTIONS,UNIQUE,NO,NO
SYSTEM,RDB$INDEX_54,RDB$INDEX_54,RDB$BACKUP_HISTORY,UNIQUE,NO,NO
SYSTEM,RDB$INDEX_55,RDB$INDEX_55,RDB$PUBLICATIONS,UNIQUE,NO,NO
SYSTEM,RDB$INDEX_58,RDB$INDEX_58,RDB$SCHEMAS,UNIQUE,NO,NO
SYSTEM,RDB$INDEX_59,RDB$INDEX_59,RDB$RELATIONS,UNIQUE,NO,NO
SYSTEM,RDB$INDEX_60,RDB$INDEX_60,RDB$FIELDS,UNIQUE,NO,NO
SYSTEM,RDB$INDEX_63,RDB$INDEX_63,RDB$INDICES,UNIQUE,NO,NO
SYSTEM,RDB$INDEX_65,RDB$INDEX_65,RDB$TRIGGERS,UNIQUE,NO,NO
SYSTEM,RDB$INDEX_66,RDB$INDEX_66,RDB$FUNCTIONS,UNIQUE,NO,NO
SYSTEM,RDB$INDEX_68,RDB$INDEX_68,RDB$GENERATORS,UNIQUE,NO,NO
SYSTEM,RDB$INDEX_69,RDB$INDEX_69,RDB$RELATION_CONSTRAINTS,UNIQUE,NO,NO
SYSTEM,RDB$INDEX_70,RDB$INDEX_70,RDB$REF_CONSTRAINTS,UNIQUE,NO,NO
SYSTEM,RDB$INDEX_72,RDB$INDEX_72,RDB$RELATION_FIELDS,UNIQUE,NO,NO
SYSTEM,RDB$INDEX_73,RDB$INDEX_73,RDB$PROCEDURE_PARAMETERS,UNIQUE,NO,NO
SYSTEM,RDB$INDEX_74,RDB$INDEX_74,RDB$CHARACTER_SETS,UNIQUE,NO,NO
SYSTEM,RDB$INDEX_75,RDB$INDEX_75,RDB$COLLATIONS,UNIQUE,NO,NO
SYSTEM,RDB$INDEX_76,RDB$INDEX_76,RDB$PROCEDURES,UNIQUE,NO,NO
SYSTEM,RDB$INDEX_77,RDB$INDEX_77,RDB$EXCEPTIONS,UNIQUE,NO,NO
SYSTEM,RDB$INDEX_92,RDB$INDEX_92,RDB$PACKAGES,UNIQUE,NO,NO
SYSTEM,RDB$INDEX_97,RDB$INDEX_97,RDB$PUBLICATION_TABLES,UNIQUE,NO,NO
```

#### RDB$GENERATORS (Default Rows)
```csv
RDB$SCHEMA_NAME,RDB$GENERATOR_NAME,RDB$GENERATOR_ID,RDB$SYSTEM_FLAG,RDB$OWNER_NAME,RDB$SECURITY_CLASS,RDB$INITIAL_VALUE,RDB$GENERATOR_INCREMENT,RDB$DESCRIPTION
SYSTEM,RDB$SECURITY_CLASS,1,1,<DB_OWNER>,SQL$<n>,0,0,NULL
SYSTEM,SQL$DEFAULT,2,1,<DB_OWNER>,SQL$<n>,0,0,NULL
SYSTEM,RDB$PROCEDURES,3,1,<DB_OWNER>,SQL$<n>,0,0,Procedure ID
SYSTEM,RDB$EXCEPTIONS,4,1,<DB_OWNER>,SQL$<n>,0,0,Exception ID
SYSTEM,RDB$CONSTRAINT_NAME,5,1,<DB_OWNER>,SQL$<n>,0,0,Implicit constraint name
SYSTEM,RDB$FIELD_NAME,6,1,<DB_OWNER>,SQL$<n>,0,0,Implicit domain name
SYSTEM,RDB$INDEX_NAME,7,1,<DB_OWNER>,SQL$<n>,0,0,Implicit index name
SYSTEM,RDB$TRIGGER_NAME,8,1,<DB_OWNER>,SQL$<n>,0,0,Implicit trigger name
SYSTEM,RDB$BACKUP_HISTORY,9,1,<DB_OWNER>,SQL$<n>,0,0,Nbackup technology
SYSTEM,RDB$FUNCTIONS,10,1,<DB_OWNER>,SQL$<n>,0,0,Function ID
SYSTEM,RDB$GENERATOR_NAME,11,1,<DB_OWNER>,SQL$<n>,0,0,Implicit generator name
```

#### RDB$PACKAGES (Default Rows)
```csv
RDB$SCHEMA_NAME,RDB$PACKAGE_NAME,RDB$OWNER_NAME,RDB$SECURITY_CLASS,RDB$SYSTEM_FLAG,RDB$VALID_BODY_FLAG
SYSTEM,RDB$TIME_ZONE_UTIL,<DB_OWNER>,SQL$<n>,1,TRUE
SYSTEM,RDB$PROFILER,<DB_OWNER>,SQL$<n>,1,TRUE
SYSTEM,RDB$BLOB_UTIL,<DB_OWNER>,SQL$<n>,1,TRUE
SYSTEM,RDB$SQL,<DB_OWNER>,SQL$<n>,1,TRUE
```

#### RDB$PROCEDURES (Default Rows)
```csv
RDB$SCHEMA_NAME,RDB$PACKAGE_NAME,RDB$PROCEDURE_NAME,RDB$OWNER_NAME,RDB$SYSTEM_FLAG,RDB$PROCEDURE_ID,RDB$PROCEDURE_INPUTS,RDB$PROCEDURE_OUTPUTS,RDB$PROCEDURE_TYPE,RDB$PRIVATE_FLAG,RDB$VALID_BLR,RDB$ENGINE_NAME
SYSTEM,RDB$TIME_ZONE_UTIL,TRANSITIONS,<DB_OWNER>,1,1,3,5,1,FALSE,TRUE,SYSTEM
SYSTEM,RDB$PROFILER,CANCEL_SESSION,<DB_OWNER>,1,2,1,0,2,FALSE,TRUE,SYSTEM
SYSTEM,RDB$PROFILER,DISCARD,<DB_OWNER>,1,3,1,0,2,FALSE,TRUE,SYSTEM
SYSTEM,RDB$PROFILER,FINISH_SESSION,<DB_OWNER>,1,4,2,0,2,FALSE,TRUE,SYSTEM
SYSTEM,RDB$PROFILER,FLUSH,<DB_OWNER>,1,5,1,0,2,FALSE,TRUE,SYSTEM
SYSTEM,RDB$PROFILER,PAUSE_SESSION,<DB_OWNER>,1,6,2,0,2,FALSE,TRUE,SYSTEM
SYSTEM,RDB$PROFILER,RESUME_SESSION,<DB_OWNER>,1,7,1,0,2,FALSE,TRUE,SYSTEM
SYSTEM,RDB$PROFILER,SET_FLUSH_INTERVAL,<DB_OWNER>,1,8,2,0,2,FALSE,TRUE,SYSTEM
SYSTEM,RDB$BLOB_UTIL,CANCEL_BLOB,<DB_OWNER>,1,9,1,0,2,FALSE,TRUE,SYSTEM
SYSTEM,RDB$BLOB_UTIL,CLOSE_HANDLE,<DB_OWNER>,1,10,1,0,2,FALSE,TRUE,SYSTEM
SYSTEM,RDB$SQL,EXPLAIN,<DB_OWNER>,1,11,1,13,1,FALSE,TRUE,SYSTEM
SYSTEM,RDB$SQL,PARSE_UNQUALIFIED_NAMES,<DB_OWNER>,1,12,1,1,1,FALSE,TRUE,SYSTEM
```

#### RDB$PROCEDURE_PARAMETERS (Default Rows)
```csv
RDB$SCHEMA_NAME,RDB$PACKAGE_NAME,RDB$PROCEDURE_NAME,RDB$PARAMETER_NAME,RDB$SYSTEM_FLAG,RDB$PARAMETER_NUMBER,RDB$PARAMETER_TYPE,RDB$PARAMETER_MECHANISM,RDB$NULL_FLAG,RDB$FIELD_SOURCE_SCHEMA_NAME,RDB$FIELD_SOURCE,RDB$DEFAULT_SOURCE,RDB$DEFAULT_VALUE
SYSTEM,RDB$TIME_ZONE_UTIL,TRANSITIONS,RDB$TIME_ZONE_NAME,1,0,0,prm_mech_normal,1,SYSTEM,RDB$TIME_ZONE_NAME,NULL,NULL
SYSTEM,RDB$TIME_ZONE_UTIL,TRANSITIONS,RDB$FROM_TIMESTAMP,1,1,0,prm_mech_normal,1,SYSTEM,RDB$TIMESTAMP_TZ,NULL,NULL
SYSTEM,RDB$TIME_ZONE_UTIL,TRANSITIONS,RDB$TO_TIMESTAMP,1,2,0,prm_mech_normal,1,SYSTEM,RDB$TIMESTAMP_TZ,NULL,NULL
SYSTEM,RDB$TIME_ZONE_UTIL,TRANSITIONS,RDB$START_TIMESTAMP,1,0,1,prm_mech_normal,1,SYSTEM,RDB$TIMESTAMP_TZ,NULL,NULL
SYSTEM,RDB$TIME_ZONE_UTIL,TRANSITIONS,RDB$END_TIMESTAMP,1,1,1,prm_mech_normal,1,SYSTEM,RDB$TIMESTAMP_TZ,NULL,NULL
SYSTEM,RDB$TIME_ZONE_UTIL,TRANSITIONS,RDB$ZONE_OFFSET,1,2,1,prm_mech_normal,1,SYSTEM,RDB$TIME_ZONE_OFFSET,NULL,NULL
SYSTEM,RDB$TIME_ZONE_UTIL,TRANSITIONS,RDB$DST_OFFSET,1,3,1,prm_mech_normal,1,SYSTEM,RDB$TIME_ZONE_OFFSET,NULL,NULL
SYSTEM,RDB$TIME_ZONE_UTIL,TRANSITIONS,RDB$EFFECTIVE_OFFSET,1,4,1,prm_mech_normal,1,SYSTEM,RDB$TIME_ZONE_OFFSET,NULL,NULL
SYSTEM,RDB$PROFILER,CANCEL_SESSION,ATTACHMENT_ID,1,0,0,prm_mech_normal,0,SYSTEM,RDB$ATTACHMENT_ID,default null,0x05 0x2D 0x4C
SYSTEM,RDB$PROFILER,DISCARD,ATTACHMENT_ID,1,0,0,prm_mech_normal,0,SYSTEM,RDB$ATTACHMENT_ID,default null,0x05 0x2D 0x4C
SYSTEM,RDB$PROFILER,FINISH_SESSION,FLUSH,1,0,0,prm_mech_normal,1,SYSTEM,RDB$BOOLEAN,default true,0x05 0x15 0x17 0x01 0x4C
SYSTEM,RDB$PROFILER,FINISH_SESSION,ATTACHMENT_ID,1,1,0,prm_mech_normal,0,SYSTEM,RDB$ATTACHMENT_ID,default null,0x05 0x2D 0x4C
SYSTEM,RDB$PROFILER,FLUSH,ATTACHMENT_ID,1,0,0,prm_mech_normal,0,SYSTEM,RDB$ATTACHMENT_ID,default null,0x05 0x2D 0x4C
SYSTEM,RDB$PROFILER,PAUSE_SESSION,FLUSH,1,0,0,prm_mech_normal,1,SYSTEM,RDB$BOOLEAN,default false,0x05 0x15 0x17 0x00 0x4C
SYSTEM,RDB$PROFILER,PAUSE_SESSION,ATTACHMENT_ID,1,1,0,prm_mech_normal,0,SYSTEM,RDB$ATTACHMENT_ID,default null,0x05 0x2D 0x4C
SYSTEM,RDB$PROFILER,RESUME_SESSION,ATTACHMENT_ID,1,0,0,prm_mech_normal,0,SYSTEM,RDB$ATTACHMENT_ID,default null,0x05 0x2D 0x4C
SYSTEM,RDB$PROFILER,SET_FLUSH_INTERVAL,FLUSH_INTERVAL,1,0,0,prm_mech_normal,1,SYSTEM,RDB$SECONDS_INTERVAL,NULL,NULL
SYSTEM,RDB$PROFILER,SET_FLUSH_INTERVAL,ATTACHMENT_ID,1,1,0,prm_mech_normal,0,SYSTEM,RDB$ATTACHMENT_ID,default null,0x05 0x2D 0x4C
SYSTEM,RDB$BLOB_UTIL,CANCEL_BLOB,BLOB,1,0,0,prm_mech_normal,1,SYSTEM,RDB$BLOB,NULL,NULL
SYSTEM,RDB$BLOB_UTIL,CLOSE_HANDLE,HANDLE,1,0,0,prm_mech_normal,1,SYSTEM,RDB$BLOB_UTIL_HANDLE,NULL,NULL
SYSTEM,RDB$SQL,EXPLAIN,SQL,1,0,0,prm_mech_normal,1,SYSTEM,RDB$DESCRIPTION,NULL,NULL
SYSTEM,RDB$SQL,EXPLAIN,PLAN_LINE,1,0,1,prm_mech_normal,1,SYSTEM,RDB$INTEGER,NULL,NULL
SYSTEM,RDB$SQL,EXPLAIN,RECORD_SOURCE_ID,1,1,1,prm_mech_normal,1,SYSTEM,RDB$GENERATOR_VALUE,NULL,NULL
SYSTEM,RDB$SQL,EXPLAIN,PARENT_RECORD_SOURCE_ID,1,2,1,prm_mech_normal,0,SYSTEM,RDB$GENERATOR_VALUE,NULL,NULL
SYSTEM,RDB$SQL,EXPLAIN,LEVEL,1,3,1,prm_mech_normal,1,SYSTEM,RDB$INTEGER,NULL,NULL
SYSTEM,RDB$SQL,EXPLAIN,OBJECT_TYPE,1,4,1,prm_mech_normal,0,SYSTEM,RDB$OBJECT_TYPE,NULL,NULL
SYSTEM,RDB$SQL,EXPLAIN,SCHEMA_NAME,1,5,1,prm_mech_normal,0,SYSTEM,RDB$SCHEMA_NAME,NULL,NULL
SYSTEM,RDB$SQL,EXPLAIN,PACKAGE_NAME,1,6,1,prm_mech_normal,0,SYSTEM,RDB$PACKAGE_NAME,NULL,NULL
SYSTEM,RDB$SQL,EXPLAIN,OBJECT_NAME,1,7,1,prm_mech_normal,0,SYSTEM,RDB$RELATION_NAME,NULL,NULL
SYSTEM,RDB$SQL,EXPLAIN,ALIAS,1,8,1,prm_mech_normal,0,SYSTEM,RDB$SHORT_DESCRIPTION,NULL,NULL
SYSTEM,RDB$SQL,EXPLAIN,CARDINALITY,1,9,1,prm_mech_normal,0,SYSTEM,RDB$STATISTICS,NULL,NULL
SYSTEM,RDB$SQL,EXPLAIN,RECORD_LENGTH,1,10,1,prm_mech_normal,0,SYSTEM,RDB$INTEGER,NULL,NULL
SYSTEM,RDB$SQL,EXPLAIN,KEY_LENGTH,1,11,1,prm_mech_normal,0,SYSTEM,RDB$INTEGER,NULL,NULL
SYSTEM,RDB$SQL,EXPLAIN,ACCESS_PATH,1,12,1,prm_mech_normal,1,SYSTEM,RDB$DESCRIPTION,NULL,NULL
SYSTEM,RDB$SQL,PARSE_UNQUALIFIED_NAMES,NAMES,1,0,0,prm_mech_normal,0,SYSTEM,RDB$TEXT_MAX,NULL,NULL
SYSTEM,RDB$SQL,PARSE_UNQUALIFIED_NAMES,NAME,1,0,1,prm_mech_normal,1,SYSTEM,RDB$RELATION_NAME,NULL,NULL
```

#### RDB$FUNCTIONS (Default Rows)
```csv
RDB$SCHEMA_NAME,RDB$PACKAGE_NAME,RDB$FUNCTION_NAME,RDB$OWNER_NAME,RDB$SYSTEM_FLAG,RDB$FUNCTION_ID,RDB$RETURN_ARGUMENT,RDB$PRIVATE_FLAG,RDB$VALID_BLR,RDB$ENGINE_NAME
SYSTEM,RDB$TIME_ZONE_UTIL,DATABASE_VERSION,<DB_OWNER>,1,1,0,FALSE,TRUE,SYSTEM
SYSTEM,RDB$PROFILER,START_SESSION,<DB_OWNER>,1,2,0,FALSE,TRUE,SYSTEM
SYSTEM,RDB$BLOB_UTIL,IS_WRITABLE,<DB_OWNER>,1,3,0,FALSE,TRUE,SYSTEM
SYSTEM,RDB$BLOB_UTIL,NEW_BLOB,<DB_OWNER>,1,4,0,FALSE,TRUE,SYSTEM
SYSTEM,RDB$BLOB_UTIL,OPEN_BLOB,<DB_OWNER>,1,5,0,FALSE,TRUE,SYSTEM
SYSTEM,RDB$BLOB_UTIL,SEEK,<DB_OWNER>,1,6,0,FALSE,TRUE,SYSTEM
SYSTEM,RDB$BLOB_UTIL,READ_DATA,<DB_OWNER>,1,7,0,FALSE,TRUE,SYSTEM
```

#### RDB$FUNCTION_ARGUMENTS (Default Rows)
```csv
RDB$SCHEMA_NAME,RDB$PACKAGE_NAME,RDB$FUNCTION_NAME,RDB$ARGUMENT_NAME,RDB$SYSTEM_FLAG,RDB$ARGUMENT_POSITION,RDB$NULL_FLAG,RDB$FIELD_SOURCE_SCHEMA_NAME,RDB$FIELD_SOURCE,RDB$DEFAULT_SOURCE,RDB$DEFAULT_VALUE
SYSTEM,RDB$TIME_ZONE_UTIL,DATABASE_VERSION,NULL,1,0,1,SYSTEM,RDB$DBTZ_VERSION,NULL,NULL
SYSTEM,RDB$PROFILER,START_SESSION,NULL,1,0,1,SYSTEM,RDB$PROFILE_SESSION_ID,NULL,NULL
SYSTEM,RDB$PROFILER,START_SESSION,DESCRIPTION,1,1,0,SYSTEM,RDB$SHORT_DESCRIPTION,default null,0x05 0x2D 0x4C
SYSTEM,RDB$PROFILER,START_SESSION,FLUSH_INTERVAL,1,2,0,SYSTEM,RDB$SECONDS_INTERVAL,default null,0x05 0x2D 0x4C
SYSTEM,RDB$PROFILER,START_SESSION,ATTACHMENT_ID,1,3,0,SYSTEM,RDB$ATTACHMENT_ID,default null,0x05 0x2D 0x4C
SYSTEM,RDB$PROFILER,START_SESSION,PLUGIN_NAME,1,4,0,SYSTEM,RDB$FILE_NAME2,default null,0x05 0x2D 0x4C
SYSTEM,RDB$PROFILER,START_SESSION,PLUGIN_OPTIONS,1,5,0,SYSTEM,RDB$SHORT_DESCRIPTION,default null,0x05 0x2D 0x4C
SYSTEM,RDB$BLOB_UTIL,IS_WRITABLE,NULL,1,0,1,SYSTEM,RDB$BOOLEAN,NULL,NULL
SYSTEM,RDB$BLOB_UTIL,IS_WRITABLE,BLOB,1,1,1,SYSTEM,RDB$BLOB,NULL,NULL
SYSTEM,RDB$BLOB_UTIL,NEW_BLOB,NULL,1,0,1,SYSTEM,RDB$BLOB,NULL,NULL
SYSTEM,RDB$BLOB_UTIL,NEW_BLOB,SEGMENTED,1,1,1,SYSTEM,RDB$BOOLEAN,NULL,NULL
SYSTEM,RDB$BLOB_UTIL,NEW_BLOB,TEMP_STORAGE,1,2,1,SYSTEM,RDB$BOOLEAN,NULL,NULL
SYSTEM,RDB$BLOB_UTIL,OPEN_BLOB,NULL,1,0,1,SYSTEM,RDB$BLOB_UTIL_HANDLE,NULL,NULL
SYSTEM,RDB$BLOB_UTIL,OPEN_BLOB,BLOB,1,1,1,SYSTEM,RDB$BLOB,NULL,NULL
SYSTEM,RDB$BLOB_UTIL,SEEK,NULL,1,0,1,SYSTEM,RDB$INTEGER,NULL,NULL
SYSTEM,RDB$BLOB_UTIL,SEEK,HANDLE,1,1,1,SYSTEM,RDB$BLOB_UTIL_HANDLE,NULL,NULL
SYSTEM,RDB$BLOB_UTIL,SEEK,MODE,1,2,1,SYSTEM,RDB$INTEGER,NULL,NULL
SYSTEM,RDB$BLOB_UTIL,SEEK,OFFSET,1,3,1,SYSTEM,RDB$INTEGER,NULL,NULL
SYSTEM,RDB$BLOB_UTIL,READ_DATA,NULL,1,0,0,SYSTEM,RDB$VARBINARY_MAX,NULL,NULL
SYSTEM,RDB$BLOB_UTIL,READ_DATA,HANDLE,1,1,1,SYSTEM,RDB$BLOB_UTIL_HANDLE,NULL,NULL
SYSTEM,RDB$BLOB_UTIL,READ_DATA,LENGTH,1,2,0,SYSTEM,RDB$INTEGER,NULL,NULL
```

#### RDB$ROLES (Default Rows)
```csv
RDB$ROLE_NAME,RDB$OWNER_NAME,RDB$SECURITY_CLASS,RDB$SYSTEM_FLAG,RDB$SYSTEM_PRIVILEGES
RDB$ADMIN,<DB_OWNER>,SQL$<n>,1,0xFFFFFFFFFFFFFFFF
```

#### RDB$PUBLICATIONS (Default Rows)
```csv
RDB$PUBLICATION_NAME,RDB$OWNER_NAME,RDB$SYSTEM_FLAG,RDB$ACTIVE_FLAG,RDB$AUTO_ENABLE
RDB$DEFAULT,<DB_OWNER>,1,0,0
```

### Other System Tables
All remaining system tables not explicitly listed in this appendix or in `appendix_system_triggers.md` MUST be present and queryable. At minimum, they must return **zero rows** unless populated by engine-created objects. The emulator MUST NOT invent rows for these tables beyond what is mandated by SQL DDL or this specification.
