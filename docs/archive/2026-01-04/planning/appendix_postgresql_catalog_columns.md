# PostgreSQL Emulated Catalog Column Lists (Generated)
Source: PostgreSQL 16.3 source (src/include/catalog/*.h + system_views.sql + information_schema.sql).

## pg_catalog base tables (CATALOG definitions)
### pg_aggregate
aggfnoid, aggkind, aggnumdirectargs, aggtransfn, aggfinalfn, aggcombinefn, aggserialfn, aggdeserialfn, aggmtransfn, aggminvtransfn, aggmfinalfn, aggfinalextra, aggmfinalextra, aggfinalmodify, aggmfinalmodify, aggsortop, aggtranstype, aggtransspace, aggmtranstype, aggmtransspace, agginitval, aggminitval

### pg_am
oid, amname, amhandler, amtype

### pg_amop
oid, amopfamily, amoplefttype, amoprighttype, amopstrategy, amoppurpose, amopopr, amopmethod, amopsortfamily

### pg_amproc
oid, amprocfamily, amproclefttype, amprocrighttype, amprocnum, amproc

### pg_attrdef
oid, adrelid, adnum, adbin

### pg_attribute
attrelid, attname, atttypid, attlen, attnum, attcacheoff, atttypmod, attndims, attbyval, attalign, attstorage, attcompression, attnotnull, atthasdef, atthasmissing, attidentity, attgenerated, attisdropped, attislocal, attinhcount, attstattarget, attcollation, attacl, attoptions, attfdwoptions, attmissingval

### pg_auth_members
oid, roleid, member, grantor, admin_option, inherit_option, set_option

### pg_authid
oid, rolname, rolsuper, rolinherit, rolcreaterole, rolcreatedb, rolcanlogin, rolreplication, rolbypassrls, rolconnlimit, rolpassword, rolvaliduntil

### pg_cast
oid, castsource, casttarget, castfunc, castcontext, castmethod

### pg_class
oid, relname, relnamespace, reltype, reloftype, relowner, relam, relfilenode, reltablespace, relpages, reltuples, relallvisible, reltoastrelid, relhasindex, relisshared, relpersistence, relkind, relnatts, relchecks, relhasrules, relhastriggers, relhassubclass, relrowsecurity, relforcerowsecurity, relispopulated, relreplident, relispartition, relrewrite, relfrozenxid, relminmxid, relacl, reloptions, relpartbound

### pg_collation
oid, collname, collnamespace, collowner, collprovider, collisdeterministic, collencoding, collcollate, collctype, colliculocale, collicurules, collversion

### pg_constraint
oid, conname, connamespace, contype, condeferrable, condeferred, convalidated, conrelid, contypid, conindid, conparentid, confrelid, confupdtype, confdeltype, confmatchtype, conislocal, coninhcount, connoinherit, conkey, confkey, conpfeqop, conppeqop, conffeqop, confdelsetcols, conexclop, conbin

### pg_conversion
oid, conname, connamespace, conowner, conforencoding, contoencoding, conproc, condefault

### pg_database
oid, datname, datdba, encoding, datlocprovider, datistemplate, datallowconn, datconnlimit, datfrozenxid, datminmxid, dattablespace, datcollate, datctype, daticulocale, daticurules, datcollversion, datacl

### pg_db_role_setting
setdatabase, setrole, setconfig

### pg_default_acl
oid, defaclrole, defaclnamespace, defaclobjtype, defaclacl

### pg_depend
classid, objid, objsubid, refclassid, refobjid, refobjsubid, deptype

### pg_description
objoid, classoid, objsubid, description

### pg_enum
oid, enumtypid, enumsortorder, enumlabel

### pg_event_trigger
oid, evtname, evtevent, evtowner, evtfoid, evtenabled, evttags

### pg_extension
oid, extname, extowner, extnamespace, extrelocatable, extversion, extconfig, extcondition

### pg_foreign_data_wrapper
oid, fdwname, fdwowner, fdwhandler, fdwvalidator, fdwacl, fdwoptions

### pg_foreign_server
oid, srvname, srvowner, srvfdw, srvtype, srvversion, srvacl, srvoptions

### pg_foreign_table
ftrelid, ftserver, ftoptions

### pg_index
indexrelid, indrelid, indnatts, indnkeyatts, indisunique, indnullsnotdistinct, indisprimary, indisexclusion, indimmediate, indisclustered, indisvalid, indcheckxmin, indisready, indislive, indisreplident, indkey, indcollation, indclass, indoption, indexprs, indpred

### pg_inherits
inhrelid, inhparent, inhseqno, inhdetachpending

### pg_init_privs
objoid, classoid, objsubid, privtype, initprivs

### pg_language
oid, lanname, lanowner, lanispl, lanpltrusted, lanplcallfoid, laninline, lanvalidator, lanacl

### pg_largeobject
loid, pageno, data

### pg_largeobject_metadata
oid, lomowner, lomacl

### pg_namespace
oid, nspname, nspowner, nspacl

### pg_opclass
oid, opcmethod, opcname, opcnamespace, opcowner, opcfamily, opcintype, opcdefault, opckeytype

### pg_operator
oid, oprname, oprnamespace, oprowner, oprkind, oprcanmerge, oprcanhash, oprleft, oprright, oprresult, oprcom, oprnegate, oprcode, oprrest, oprjoin

### pg_opfamily
oid, opfmethod, opfname, opfnamespace, opfowner

### pg_parameter_acl
oid, parname, paracl

### pg_partitioned_table
partrelid, partstrat, partnatts, partdefid, partattrs, partclass, partcollation, partexprs

### pg_policy
oid, polname, polrelid, polcmd, polpermissive, polroles, polqual, polwithcheck

### pg_proc
oid, proname, pronamespace, proowner, prolang, procost, prorows, provariadic, prosupport, prokind, prosecdef, proleakproof, proisstrict, proretset, provolatile, proparallel, pronargs, pronargdefaults, prorettype, proargtypes, proallargtypes, proargmodes, proargnames, proargdefaults, protrftypes, prosrc, probin, prosqlbody, proconfig, proacl

### pg_publication
oid, pubname, pubowner, puballtables, pubinsert, pubupdate, pubdelete, pubtruncate, pubviaroot

### pg_publication_namespace
oid, pnpubid, pnnspid

### pg_publication_rel
oid, prpubid, prrelid, prqual, prattrs

### pg_range
rngtypid, rngsubtype, rngmultitypid, rngcollation, rngsubopc, rngcanonical, rngsubdiff

### pg_replication_origin
roident, roname

### pg_rewrite
oid, rulename, ev_class, ev_type, ev_enabled, is_instead, ev_qual, ev_action

### pg_seclabel
objoid, classoid, objsubid, provider, label

### pg_sequence
seqrelid, seqtypid, seqstart, seqincrement, seqmax, seqmin, seqcache, seqcycle

### pg_shdepend
dbid, classid, objid, objsubid, refclassid, refobjid, deptype

### pg_shdescription
objoid, classoid, description

### pg_shseclabel
objoid, classoid, provider, label

### pg_statistic
starelid, staattnum, stainherit, stanullfrac, stawidth, stadistinct, stakind1, stakind2, stakind3, stakind4, stakind5, staop1, staop2, staop3, staop4, staop5, stacoll1, stacoll2, stacoll3, stacoll4, stacoll5, stanumbers1, stanumbers2, stanumbers3, stanumbers4, stanumbers5, stavalues1, stavalues2, stavalues3, stavalues4, stavalues5

### pg_statistic_ext
oid, stxrelid, stxname, stxnamespace, stxowner, stxstattarget, stxkeys, stxkind, stxexprs

### pg_statistic_ext_data
stxoid, stxdinherit, stxdndistinct, stxddependencies, stxdmcv, stxdexpr

### pg_subscription
oid, subdbid, subskiplsn, subname, subowner, subenabled, subbinary, substream, subtwophasestate, subdisableonerr, subpasswordrequired, subrunasowner, subconninfo, subslotname, subsynccommit, subpublications, suborigin

### pg_subscription_rel
srsubid, srrelid, srsubstate, srsublsn

### pg_tablespace
oid, spcname, spcowner, spcacl, spcoptions

### pg_transform
oid, trftype, trflang, trffromsql, trftosql

### pg_trigger
oid, tgrelid, tgparentid, tgname, tgfoid, tgtype, tgenabled, tgisinternal, tgconstrrelid, tgconstrindid, tgconstraint, tgdeferrable, tginitdeferred, tgnargs, tgattr, tgargs, tgqual, tgoldtable, tgnewtable

### pg_ts_config
oid, cfgname, cfgnamespace, cfgowner, cfgparser

### pg_ts_config_map
mapcfg, maptokentype, mapseqno, mapdict

### pg_ts_dict
oid, dictname, dictnamespace, dictowner, dicttemplate, dictinitoption

### pg_ts_parser
oid, prsname, prsnamespace, prsstart, prstoken, prsend, prsheadline, prslextype

### pg_ts_template
oid, tmplname, tmplnamespace, tmplinit, tmpllexize

### pg_type
oid, typname, typnamespace, typowner, typlen, typbyval, typtype, typcategory, typispreferred, typisdefined, typdelim, typrelid, typsubscript, typelem, typarray, typinput, typoutput, typreceive, typsend, typmodin, typmodout, typanalyze, typalign, typstorage, typnotnull, typbasetype, typtypmod, typndims, typcollation, typdefaultbin, typdefault, typacl

### pg_user_mapping
oid, umuser, umserver, umoptions

## pg_catalog system views
### pg_available_extension_versions
name, version, installed, superuser, trusted, relocatable, schema, requires, comment

### pg_available_extensions
name, default_version, installed_version, comment

### pg_group
groname, grosysid, grolist

### pg_indexes
schemaname, tablename, indexname, tablespace, indexdef

### pg_matviews
schemaname, matviewname, matviewowner, tablespace, hasindexes, ispopulated, definition

### pg_policies
schemaname, tablename, policyname, permissive, roles, cmd, qual, with_check

### pg_prepared_xacts
transaction, gid, prepared, owner, database

### pg_publication_tables
pubname, schemaname, tablename, attnames, rowfilter

### pg_replication_slots
slot_name, plugin, slot_type, datoid, database, temporary, active, active_pid, xmin, catalog_xmin, restart_lsn, confirmed_flush_lsn, wal_status, safe_wal_size, two_phase, conflicting

### pg_roles
rolname, rolsuper, rolinherit, rolcreaterole, rolcreatedb, rolcanlogin, rolreplication, rolconnlimit, rolpassword, rolvaliduntil, rolbypassrls, rolconfig, oid

### pg_rules
schemaname, tablename, rulename, definition

### pg_seclabels
objoid, classoid, objsubid, objtype, objnamespace, objname, provider, label

### pg_sequences
schemaname, sequencename, sequenceowner, data_type, start_value, min_value, max_value, increment_by, cycle, cache_size, last_value

### pg_shadow
usename, usesysid, usecreatedb, usesuper, userepl, usebypassrls, passwd, valuntil, useconfig

### pg_stat_activity
datid, datname, pid, leader_pid, usesysid, usename, application_name, client_addr, client_hostname, client_port, backend_start, xact_start, query_start, state_change, wait_event_type, wait_event, state, backend_xid, backend_xmin, query_id, query, backend_type

### pg_stat_all_indexes
relid, indexrelid, schemaname, relname, indexrelname, idx_scan, last_idx_scan, idx_tup_read, idx_tup_fetch

### pg_stat_all_tables
relid, schemaname, relname, seq_scan, last_seq_scan, seq_tup_read, idx_scan, last_idx_scan, idx_tup_fetch, n_tup_ins, n_tup_upd, n_tup_del, n_tup_hot_upd, n_tup_newpage_upd, n_live_tup, n_dead_tup, n_mod_since_analyze, n_ins_since_vacuum, last_vacuum, last_autovacuum, last_analyze, last_autoanalyze, vacuum_count, autovacuum_count, analyze_count, autoanalyze_count

### pg_stat_archiver
archived_count, last_archived_wal, last_archived_time, failed_count, last_failed_wal, last_failed_time, stats_reset

### pg_stat_database
datid, datname, numbackends, xact_commit, xact_rollback, blks_read, blks_hit, tup_returned, tup_fetched, tup_inserted, tup_updated, tup_deleted, conflicts, temp_files, temp_bytes, deadlocks, checksum_failures, checksum_last_failure, blk_read_time, blk_write_time, session_time, active_time, idle_in_transaction_time, sessions, sessions_abandoned, sessions_fatal, sessions_killed, stats_reset

### pg_stat_database_conflicts
datid, datname, confl_tablespace, confl_lock, confl_snapshot, confl_bufferpin, confl_deadlock, confl_active_logicalslot

### pg_stat_gssapi
pid, gss_authenticated, principal, encrypted, credentials_delegated

### pg_stat_io
backend_type, object, context, reads, read_time, writes, write_time, writebacks, writeback_time, extends, extend_time, op_bytes, hits, evictions, reuses, fsyncs, fsync_time, stats_reset

### pg_stat_progress_analyze
pid, datid, datname, relid, phase, sample_blks_total, sample_blks_scanned, ext_stats_total, ext_stats_computed, child_tables_total, child_tables_done, current_child_table_relid

### pg_stat_progress_basebackup
pid, phase, backup_total, backup_streamed, tablespaces_total, tablespaces_streamed

### pg_stat_progress_cluster
pid, datid, datname, relid, command, phase, cluster_index_relid, heap_tuples_scanned, heap_tuples_written, heap_blks_total, heap_blks_scanned, index_rebuild_count

### pg_stat_progress_copy
pid, datid, datname, relid, command, type, bytes_processed, bytes_total, tuples_processed, tuples_excluded

### pg_stat_progress_create_index
pid, datid, datname, relid, index_relid, command, phase, lockers_total, lockers_done, current_locker_pid, blocks_total, blocks_done, tuples_total, tuples_done, partitions_total, partitions_done

### pg_stat_progress_vacuum
pid, datid, datname, relid, phase, heap_blks_total, heap_blks_scanned, heap_blks_vacuumed, index_vacuum_count, max_dead_tuples, num_dead_tuples

### pg_stat_recovery_prefetch
stats_reset, prefetch, hit, skip_init, skip_new, skip_fpw, skip_rep, wal_distance, block_distance, io_depth

### pg_stat_replication
pid, usesysid, usename, application_name, client_addr, client_hostname, client_port, backend_start, backend_xmin, state, sent_lsn, write_lsn, flush_lsn, replay_lsn, write_lag, flush_lag, replay_lag, sync_priority, sync_state, reply_time

### pg_stat_replication_slots
slot_name, spill_txns, spill_count, spill_bytes, stream_txns, stream_count, stream_bytes, total_txns, total_bytes, stats_reset

### pg_stat_slru
name, blks_zeroed, blks_hit, blks_read, blks_written, blks_exists, flushes, truncates, stats_reset

### pg_stat_ssl
pid, ssl, version, cipher, bits, client_dn, client_serial, issuer_dn

### pg_stat_subscription
subid, subname, pid, leader_pid, relid, received_lsn, last_msg_send_time, last_msg_receipt_time, latest_end_lsn, latest_end_time

### pg_stat_subscription_stats
subid, subname, apply_error_count, sync_error_count, stats_reset

### pg_stat_user_functions
funcid, schemaname, funcname, calls, total_time, self_time

### pg_stat_wal
wal_records, wal_fpi, wal_bytes, wal_buffers_full, wal_write, wal_sync, wal_write_time, wal_sync_time, stats_reset

### pg_stat_wal_receiver
pid, status, receive_start_lsn, receive_start_tli, written_lsn, flushed_lsn, received_tli, last_msg_send_time, last_msg_receipt_time, latest_end_lsn, latest_end_time, slot_name, sender_host, sender_port, conninfo

### pg_stat_xact_all_tables
relid, schemaname, relname, seq_scan, seq_tup_read, idx_scan, idx_tup_fetch, n_tup_ins, n_tup_upd, n_tup_del, n_tup_hot_upd, n_tup_newpage_upd

### pg_stat_xact_user_functions
funcid, schemaname, funcname, calls, total_time, self_time

### pg_statio_all_indexes
relid, indexrelid, schemaname, relname, indexrelname, idx_blks_read, idx_blks_hit

### pg_statio_all_sequences
relid, schemaname, relname, blks_read, blks_hit

### pg_statio_all_tables
relid, schemaname, relname, heap_blks_read, heap_blks_hit, idx_blks_read, idx_blks_hit, toast_blks_read, toast_blks_hit, tidx_blks_read, tidx_blks_hit

### pg_stats
schemaname, tablename, attname, inherited, null_frac, avg_width, n_distinct, most_common_vals, most_common_freqs, histogram_bounds, correlation, most_common_elems, most_common_elem_freqs, elem_count_histogram

### pg_stats_ext
schemaname, tablename, statistics_schemaname, statistics_name, statistics_owner, attnames, exprs, kinds, inherited, n_distinct, dependencies, most_common_vals, most_common_val_nulls, most_common_freqs, most_common_base_freqs

### pg_stats_ext_exprs
schemaname, tablename, statistics_schemaname, statistics_name, statistics_owner, expr, inherited, null_frac, avg_width, n_distinct, most_common_vals, most_common_freqs, histogram_bounds, correlation, most_common_elems, most_common_elem_freqs, elem_count_histogram

### pg_tables
schemaname, tablename, tableowner, tablespace, hasindexes, hasrules, hastriggers, rowsecurity

### pg_user
usename, usesysid, usecreatedb, usesuper, userepl, usebypassrls, passwd, valuntil, useconfig

### pg_user_mappings
umid, srvid, srvname, umuser, usename, umoptions

### pg_views
schemaname, viewname, viewowner, definition

## information_schema views
### _pg_foreign_data_wrappers
oid, fdwowner, fdwoptions, foreign_data_wrapper_catalog, foreign_data_wrapper_name, authorization_identifier, foreign_data_wrapper_language

### _pg_foreign_servers
oid, srvoptions, foreign_server_catalog, foreign_server_name, foreign_data_wrapper_catalog, foreign_data_wrapper_name, foreign_server_type, foreign_server_version, authorization_identifier

### _pg_foreign_table_columns
nspname, relname, attname, attfdwoptions

### _pg_foreign_tables
foreign_table_catalog, foreign_table_schema, foreign_table_name, ftoptions, foreign_server_catalog, foreign_server_name, authorization_identifier

### _pg_user_mappings
oid, umoptions, umuser, authorization_identifier, foreign_server_catalog, foreign_server_name, srvowner

### applicable_roles
grantee, role_name, is_grantable

### attributes
udt_catalog, udt_schema, udt_name, attribute_name, ordinal_position, attribute_default, is_nullable, data_type, character_maximum_length, character_octet_length, character_set_catalog, character_set_schema, character_set_name, collation_catalog, collation_schema, collation_name, numeric_precision, numeric_precision_radix, numeric_scale, datetime_precision, interval_type, interval_precision, attribute_udt_catalog, attribute_udt_schema, attribute_udt_name, scope_catalog, scope_schema, scope_name, maximum_cardinality, dtd_identifier, is_derived_reference_attribute

### character_sets
character_set_catalog, character_set_schema, character_set_name, character_repertoire, form_of_use, default_collate_catalog, default_collate_schema, default_collate_name

### check_constraint_routine_usage
constraint_catalog, constraint_schema, constraint_name, specific_catalog, specific_schema, specific_name

### check_constraints
constraint_catalog, constraint_schema, constraint_name, check_clause

### collation_character_set_applicability
collation_catalog, collation_schema, collation_name, character_set_catalog, character_set_schema, character_set_name

### collations
collation_catalog, collation_schema, collation_name, pad_attribute

### column_column_usage
table_catalog, table_schema, table_name, column_name, dependent_column

### column_domain_usage
domain_catalog, domain_schema, domain_name, table_catalog, table_schema, table_name, column_name

### column_options
table_catalog, table_schema, table_name, column_name, option_name, option_value

### column_privileges
grantor, grantee, table_catalog, table_schema, table_name, column_name, privilege_type, is_grantable

### column_udt_usage
udt_catalog, udt_schema, udt_name, table_catalog, table_schema, table_name, column_name

### columns
table_catalog, table_schema, table_name, column_name, ordinal_position, column_default, is_nullable, data_type, character_maximum_length, character_octet_length, numeric_precision, numeric_precision_radix, numeric_scale, datetime_precision, interval_type, interval_precision, character_set_catalog, character_set_schema, character_set_name, collation_catalog, collation_schema, collation_name, domain_catalog, domain_schema, domain_name, udt_catalog, udt_schema, udt_name, scope_catalog, scope_schema, scope_name, maximum_cardinality, dtd_identifier, is_self_referencing, is_identity, identity_generation, identity_start, identity_increment, identity_maximum, identity_minimum, identity_cycle, is_generated, generation_expression, is_updatable

### constraint_column_usage
table_catalog, table_schema, table_name, column_name, constraint_catalog, constraint_schema, constraint_name

### constraint_table_usage
table_catalog, table_schema, table_name, constraint_catalog, constraint_schema, constraint_name

### data_type_privileges
object_catalog, object_schema, object_name, object_type, dtd_identifier

### domain_constraints
constraint_catalog, constraint_schema, constraint_name, domain_catalog, domain_schema, domain_name, is_deferrable, initially_deferred

### domain_udt_usage
udt_catalog, udt_schema, udt_name, domain_catalog, domain_schema, domain_name

### domains
domain_catalog, domain_schema, domain_name, data_type, character_maximum_length, character_octet_length, character_set_catalog, character_set_schema, character_set_name, collation_catalog, collation_schema, collation_name, numeric_precision, numeric_precision_radix, numeric_scale, datetime_precision, interval_type, interval_precision, domain_default, udt_catalog, udt_schema, udt_name, scope_catalog, scope_schema, scope_name, maximum_cardinality, dtd_identifier

### element_types
object_catalog, object_schema, object_name, object_type, collection_type_identifier, data_type, character_maximum_length, character_octet_length, character_set_catalog, character_set_schema, character_set_name, collation_catalog, collation_schema, collation_name, numeric_precision, numeric_precision_radix, numeric_scale, datetime_precision, interval_type, interval_precision, domain_default, udt_catalog, udt_schema, udt_name, scope_catalog, scope_schema, scope_name, maximum_cardinality, dtd_identifier

### enabled_roles
role_name

### foreign_data_wrapper_options
foreign_data_wrapper_catalog, foreign_data_wrapper_name, option_name, option_value

### foreign_data_wrappers
foreign_data_wrapper_catalog, foreign_data_wrapper_name, authorization_identifier, library_name, foreign_data_wrapper_language

### foreign_server_options
foreign_server_catalog, foreign_server_name, option_name, option_value

### foreign_servers
foreign_server_catalog, foreign_server_name, foreign_data_wrapper_catalog, foreign_data_wrapper_name, foreign_server_type, foreign_server_version, authorization_identifier

### foreign_table_options
foreign_table_catalog, foreign_table_schema, foreign_table_name, option_name, option_value

### foreign_tables
foreign_table_catalog, foreign_table_schema, foreign_table_name, foreign_server_catalog, foreign_server_name

### key_column_usage
constraint_catalog, constraint_schema, constraint_name, table_catalog, table_schema, table_name, column_name, ordinal_position, position_in_unique_constraint

### parameters
specific_catalog, specific_schema, specific_name, ordinal_position, parameter_mode, is_result, as_locator, parameter_name, data_type, character_maximum_length, character_octet_length, character_set_catalog, character_set_schema, character_set_name, collation_catalog, collation_schema, collation_name, numeric_precision, numeric_precision_radix, numeric_scale, datetime_precision, interval_type, interval_precision, udt_catalog, udt_schema, udt_name, scope_catalog, scope_schema, scope_name, maximum_cardinality, dtd_identifier, parameter_default

### referential_constraints
constraint_catalog, constraint_schema, constraint_name, unique_constraint_catalog, unique_constraint_schema, unique_constraint_name, match_option, update_rule, delete_rule

### role_column_grants
grantor, grantee, table_catalog, table_schema, table_name, column_name, privilege_type, is_grantable

### role_routine_grants
grantor, grantee, specific_catalog, specific_schema, specific_name, routine_catalog, routine_schema, routine_name, privilege_type, is_grantable

### role_table_grants
grantor, grantee, table_catalog, table_schema, table_name, privilege_type, is_grantable, with_hierarchy

### role_udt_grants
grantor, grantee, udt_catalog, udt_schema, udt_name, privilege_type, is_grantable

### role_usage_grants
grantor, grantee, object_catalog, object_schema, object_name, object_type, privilege_type, is_grantable

### routine_column_usage
specific_catalog, specific_schema, specific_name, routine_catalog, routine_schema, routine_name, table_catalog, table_schema, table_name, column_name

### routine_privileges
grantor, grantee, specific_catalog, specific_schema, specific_name, routine_catalog, routine_schema, routine_name, privilege_type, is_grantable

### routine_routine_usage
specific_catalog, specific_schema, specific_name, routine_catalog, routine_schema, routine_name

### routine_sequence_usage
specific_catalog, specific_schema, specific_name, routine_catalog, routine_schema, routine_name, sequence_catalog, sequence_schema, sequence_name

### routine_table_usage
specific_catalog, specific_schema, specific_name, routine_catalog, routine_schema, routine_name, table_catalog, table_schema, table_name

### routines
specific_catalog, specific_schema, specific_name, routine_catalog, routine_schema, routine_name, routine_type, module_catalog, module_schema, module_name, udt_catalog, udt_schema, udt_name, data_type, character_maximum_length, character_octet_length, character_set_catalog, character_set_schema, character_set_name, collation_catalog, collation_schema, collation_name, numeric_precision, numeric_precision_radix, numeric_scale, datetime_precision, interval_type, interval_precision, type_udt_catalog, type_udt_schema, type_udt_name, scope_catalog, scope_schema, scope_name, maximum_cardinality, dtd_identifier, routine_body, routine_definition, external_name, external_language, parameter_style, is_deterministic, sql_data_access, is_null_call, sql_path, schema_level_routine, max_dynamic_result_sets, is_user_defined_cast, is_implicitly_invocable, security_type, to_sql_specific_catalog, to_sql_specific_schema, to_sql_specific_name, as_locator, created, last_altered, new_savepoint_level, is_udt_dependent, result_cast_from_data_type, result_cast_as_locator, result_cast_char_max_length, result_cast_char_octet_length, result_cast_char_set_catalog, result_cast_char_set_schema, result_cast_char_set_name, result_cast_collation_catalog, result_cast_collation_schema, result_cast_collation_name, result_cast_numeric_precision, result_cast_numeric_precision_radix, result_cast_numeric_scale, result_cast_datetime_precision, result_cast_interval_type, result_cast_interval_precision, result_cast_type_udt_catalog, result_cast_type_udt_schema, result_cast_type_udt_name, result_cast_scope_catalog, result_cast_scope_schema, result_cast_scope_name, result_cast_maximum_cardinality, result_cast_dtd_identifier

### schemata
catalog_name, schema_name, schema_owner, default_character_set_catalog, default_character_set_schema, default_character_set_name, sql_path

### sequences
sequence_catalog, sequence_schema, sequence_name, data_type, numeric_precision, numeric_precision_radix, numeric_scale, start_value, minimum_value, maximum_value, increment, cycle_option

### table_constraints
constraint_catalog, constraint_schema, constraint_name, table_catalog, table_schema, table_name, constraint_type, is_deferrable, initially_deferred, enforced, nulls_distinct

### table_privileges
grantor, grantee, table_catalog, table_schema, table_name, privilege_type, is_grantable, with_hierarchy

### tables
table_catalog, table_schema, table_name, table_type, self_referencing_column_name, reference_generation, user_defined_type_catalog, user_defined_type_schema, user_defined_type_name, is_insertable_into, is_typed, commit_action

### transforms
udt_catalog, udt_schema, udt_name, specific_catalog, specific_schema, specific_name, group_name, transform_type

### triggered_update_columns
trigger_catalog, trigger_schema, trigger_name, event_object_catalog, event_object_schema, event_object_table, event_object_column

### udt_privileges
grantor, grantee, udt_catalog, udt_schema, udt_name, privilege_type, is_grantable

### usage_privileges
grantor, grantee, object_catalog, object_schema, object_name, object_type, privilege_type, is_grantable

### user_defined_types
user_defined_type_catalog, user_defined_type_schema, user_defined_type_name, user_defined_type_category, is_instantiable, is_final, ordering_form, ordering_category, ordering_routine_catalog, ordering_routine_schema, ordering_routine_name, reference_type, data_type, character_maximum_length, character_octet_length, character_set_catalog, character_set_schema, character_set_name, collation_catalog, collation_schema, collation_name, numeric_precision, numeric_precision_radix, numeric_scale, datetime_precision, interval_type, interval_precision, source_dtd_identifier, ref_dtd_identifier

### user_mapping_options
authorization_identifier, foreign_server_catalog, foreign_server_name, option_name, option_value

### user_mappings
authorization_identifier, foreign_server_catalog, foreign_server_name

### view_column_usage
view_catalog, view_schema, view_name, table_catalog, table_schema, table_name, column_name

### view_routine_usage
table_catalog, table_schema, table_name, specific_catalog, specific_schema, specific_name

### view_table_usage
view_catalog, view_schema, view_name, table_catalog, table_schema, table_name

### views
table_catalog, table_schema, table_name, view_definition, check_option, is_updatable, is_insertable_into, is_trigger_updatable, is_trigger_deletable, is_trigger_insertable_into

