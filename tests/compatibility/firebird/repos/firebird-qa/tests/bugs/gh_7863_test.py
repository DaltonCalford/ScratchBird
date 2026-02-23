#coding:utf-8

"""
ID:          issue-7863
ISSUE:       7863
TITLE:       Non-correlated sub-query is evaluated multiple times if it is based on a VIEW rathe than on appropriate derived table.
DESCRIPTION:
NOTES:
    [18.01.2025] pzotov
        Resultset of cursor that executes using instance of selectable PreparedStatement must be stored
        in some variable in order to have ability close it EXPLICITLY (before PS will be freed).
        Otherwise access violation raises during Python GC and pytest hangs at final point (does not return control to OS).
        This occurs at least for: Python 3.11.2 / pytest: 7.4.4 / firebird.driver: 1.10.6 / Firebird.Qa: 0.19.3
        The reason of that was explained by Vlad, 26.10.24 17:42 ("oddities when use instances of selective statements").
    [25.07.2025] pzotov
        Separated test DB-init scripts for check on versions prior/since 6.x.
        On 6.x we have to take in account indexed fields containing SCHEMA names, see below DDL for rdb$fields.
        Thanks to dimitr for suggestion.
    
    [09.10.2025] pzotov
        Re-implemented: removed dependency on concrete data and indices that currently present for system RDB$FIELDS table.
        Rather, pre-defined DDL is used and fixed set of data is stored in the user-defined table 'tdata' and approprioate
        indices (see below). See letter from dimitr: 08-oct-2025 08:50:
            "... the newly added system indices are a temporary solution and may disappear in future versions.
            Maybe some of the failing tests should be re-implemented to be based on user tables instead of the system ones."
    Fixed in:
        * 6.x: https://github.com/FirebirdSQL/firebird/commit/fd135f41e8185e4b3286a954c800dadbe7298df5
        * 5.x: https://github.com/FirebirdSQL/firebird/commit/5affd44b1596f2ae553197954d2be044df558206
    Confirmed bug on 6.0.0.222 (built 23-jan-2024). Checked on 6.0.0.223 (24-jan-2024) -- all fine.
    Checked on 6.0.0.1312; 5.0.4.1725.
"""

from pathlib import Path

import pytest
from firebird.qa import *
from firebird.driver import DatabaseError

db = db_factory()
act = python_act('db')

@pytest.mark.version('>=5.0.1')
def test_1(act: Action, capsys):

    init_script = """
        recreate table tdata (
            f_name         char(63) character set utf8,
            s_name        char(63) character set utf8
        );

        insert into tdata (f_name, s_name) values ('rdb_view_context', 'system');
        insert into tdata (f_name, s_name) values ('rdb_context_name', 'system');
        insert into tdata (f_name, s_name) values ('rdb_description', 'system');
        insert into tdata (f_name, s_name) values ('rdb_edit_string', 'system');
        insert into tdata (f_name, s_name) values ('rdb_field_id', 'system');
        insert into tdata (f_name, s_name) values ('f_name', 'system');
        insert into tdata (f_name, s_name) values ('rdb_system_flag', 'system');
        insert into tdata (f_name, s_name) values ('rdb_system_nullflag', 'system');
        insert into tdata (f_name, s_name) values ('rdb_index_id', 'system');
        insert into tdata (f_name, s_name) values ('rdb_index_name', 'system');
        insert into tdata (f_name, s_name) values ('rdb_field_length', 'system');
        insert into tdata (f_name, s_name) values ('rdb_field_position', 'system');
        insert into tdata (f_name, s_name) values ('rdb_field_scale', 'system');
        insert into tdata (f_name, s_name) values ('rdb_field_type', 'system');
        insert into tdata (f_name, s_name) values ('rdb_format', 'system');
        insert into tdata (f_name, s_name) values ('rdb_dbkey_length', 'system');
        insert into tdata (f_name, s_name) values ('rdb_page_number', 'system');
        insert into tdata (f_name, s_name) values ('rdb_page_sequence', 'system');
        insert into tdata (f_name, s_name) values ('rdb_page_type', 'system');
        insert into tdata (f_name, s_name) values ('rdb_query_header', 'system');
        insert into tdata (f_name, s_name) values ('rdb_relation_id', 'system');
        insert into tdata (f_name, s_name) values ('rdb_relation_name', 'system');
        insert into tdata (f_name, s_name) values ('rdb_segment_count', 'system');
        insert into tdata (f_name, s_name) values ('rdb_segment_length', 'system');
        insert into tdata (f_name, s_name) values ('rdb_source', 'system');
        insert into tdata (f_name, s_name) values ('rdb_field_sub_type', 'system');
        insert into tdata (f_name, s_name) values ('rdb_view_blr', 'system');
        insert into tdata (f_name, s_name) values ('rdb_validation_blr', 'system');
        insert into tdata (f_name, s_name) values ('rdb_value', 'system');
        insert into tdata (f_name, s_name) values ('rdb_security_class', 'system');
        insert into tdata (f_name, s_name) values ('rdb_acl', 'system');
        insert into tdata (f_name, s_name) values ('rdb_file_name', 'system');
        insert into tdata (f_name, s_name) values ('rdb_file_name2', 'system');
        insert into tdata (f_name, s_name) values ('rdb_file_sequence', 'system');
        insert into tdata (f_name, s_name) values ('rdb_file_start', 'system');
        insert into tdata (f_name, s_name) values ('rdb_file_length', 'system');
        insert into tdata (f_name, s_name) values ('rdb_file_flags', 'system');
        insert into tdata (f_name, s_name) values ('rdb_trigger_blr', 'system');
        insert into tdata (f_name, s_name) values ('rdb_trigger_name', 'system');
        insert into tdata (f_name, s_name) values ('rdb_generic_name', 'system');
        insert into tdata (f_name, s_name) values ('rdb_function_name', 'system');
        insert into tdata (f_name, s_name) values ('rdb_external_name', 'system');
        insert into tdata (f_name, s_name) values ('rdb_type_name', 'system');
        insert into tdata (f_name, s_name) values ('rdb_dimensions', 'system');
        insert into tdata (f_name, s_name) values ('rdb_runtime', 'system');
        insert into tdata (f_name, s_name) values ('rdb_trigger_sequence', 'system');
        insert into tdata (f_name, s_name) values ('rdb_generic_type', 'system');
        insert into tdata (f_name, s_name) values ('rdb_trigger_type', 'system');
        insert into tdata (f_name, s_name) values ('rdb_object_type', 'system');
        insert into tdata (f_name, s_name) values ('rdb_mechanism', 'system');
        insert into tdata (f_name, s_name) values ('rdb_descriptor', 'system');
        insert into tdata (f_name, s_name) values ('rdb_function_type', 'system');
        insert into tdata (f_name, s_name) values ('rdb_transaction_id', 'system');
        insert into tdata (f_name, s_name) values ('rdb_transaction_state', 'system');
        insert into tdata (f_name, s_name) values ('rdb_timestamp', 'system');
        insert into tdata (f_name, s_name) values ('rdb_transaction_description', 'system');
        insert into tdata (f_name, s_name) values ('rdb_message', 'system');
        insert into tdata (f_name, s_name) values ('rdb_message_number', 'system');
        insert into tdata (f_name, s_name) values ('rdb_user', 'system');
        insert into tdata (f_name, s_name) values ('rdb_privilege', 'system');
        insert into tdata (f_name, s_name) values ('rdb_external_description', 'system');
        insert into tdata (f_name, s_name) values ('rdb_shadow_number', 'system');
        insert into tdata (f_name, s_name) values ('rdb_generator_name', 'system');
        insert into tdata (f_name, s_name) values ('rdb_generator_id', 'system');
        insert into tdata (f_name, s_name) values ('rdb_bound', 'system');
        insert into tdata (f_name, s_name) values ('rdb_dimension', 'system');
        insert into tdata (f_name, s_name) values ('rdb_statistics', 'system');
        insert into tdata (f_name, s_name) values ('rdb_null_flag', 'system');
        insert into tdata (f_name, s_name) values ('rdb_constraint_name', 'system');
        insert into tdata (f_name, s_name) values ('rdb_constraint_type', 'system');
        insert into tdata (f_name, s_name) values ('rdb_deferrable', 'system');
        insert into tdata (f_name, s_name) values ('rdb_match_option', 'system');
        insert into tdata (f_name, s_name) values ('rdb_rule', 'system');
        insert into tdata (f_name, s_name) values ('rdb_file_partitions', 'system');
        insert into tdata (f_name, s_name) values ('rdb_procedure_blr', 'system');
        insert into tdata (f_name, s_name) values ('rdb_procedure_id', 'system');
        insert into tdata (f_name, s_name) values ('rdb_procedure_parameters', 'system');
        insert into tdata (f_name, s_name) values ('rdb_procedure_name', 'system');
        insert into tdata (f_name, s_name) values ('rdb_parameter_name', 'system');
        insert into tdata (f_name, s_name) values ('rdb_parameter_number', 'system');
        insert into tdata (f_name, s_name) values ('rdb_parameter_type', 'system');
        insert into tdata (f_name, s_name) values ('rdb_character_set_name', 'system');
        insert into tdata (f_name, s_name) values ('rdb_character_set_id', 'system');
        insert into tdata (f_name, s_name) values ('rdb_collation_name', 'system');
        insert into tdata (f_name, s_name) values ('rdb_collation_id', 'system');
        insert into tdata (f_name, s_name) values ('rdb_number_of_characters', 'system');
        insert into tdata (f_name, s_name) values ('rdb_exception_name', 'system');
        insert into tdata (f_name, s_name) values ('rdb_exception_number', 'system');
        insert into tdata (f_name, s_name) values ('rdb_file_p_offset', 'system');
        insert into tdata (f_name, s_name) values ('rdb_field_precision', 'system');
        insert into tdata (f_name, s_name) values ('rdb_backup_id', 'system');
        insert into tdata (f_name, s_name) values ('rdb_backup_level', 'system');
        insert into tdata (f_name, s_name) values ('rdb_guid', 'system');
        insert into tdata (f_name, s_name) values ('rdb_scn', 'system');
        insert into tdata (f_name, s_name) values ('rdb_specific_attributes', 'system');
        insert into tdata (f_name, s_name) values ('rdb_plugin', 'system');
        insert into tdata (f_name, s_name) values ('rdb_relation_type', 'system');
        insert into tdata (f_name, s_name) values ('rdb_procedure_type', 'system');
        insert into tdata (f_name, s_name) values ('rdb_attachment_id', 'system');
        insert into tdata (f_name, s_name) values ('rdb_statement_id', 'system');
        insert into tdata (f_name, s_name) values ('rdb_call_id', 'system');
        insert into tdata (f_name, s_name) values ('rdb_stat_id', 'system');
        insert into tdata (f_name, s_name) values ('rdb_pid', 'system');
        insert into tdata (f_name, s_name) values ('rdb_state', 'system');
        insert into tdata (f_name, s_name) values ('rdb_ods_number', 'system');
        insert into tdata (f_name, s_name) values ('rdb_page_size', 'system');
        insert into tdata (f_name, s_name) values ('rdb_page_buffers', 'system');
        insert into tdata (f_name, s_name) values ('rdb_shutdown_mode', 'system');
        insert into tdata (f_name, s_name) values ('rdb_sql_dialect', 'system');
        insert into tdata (f_name, s_name) values ('rdb_sweep_interval', 'system');
        insert into tdata (f_name, s_name) values ('rdb_counter', 'system');
        insert into tdata (f_name, s_name) values ('rdb_remote_protocol', 'system');
        insert into tdata (f_name, s_name) values ('rdb_remote_address', 'system');
        insert into tdata (f_name, s_name) values ('rdb_isolation_mode', 'system');
        insert into tdata (f_name, s_name) values ('rdb_lock_timeout', 'system');
        insert into tdata (f_name, s_name) values ('rdb_backup_state', 'system');
        insert into tdata (f_name, s_name) values ('rdb_stat_group', 'system');
        insert into tdata (f_name, s_name) values ('rdb_debug_info', 'system');
        insert into tdata (f_name, s_name) values ('rdb_parameter_mechanism', 'system');
        insert into tdata (f_name, s_name) values ('rdb_source_info', 'system');
        insert into tdata (f_name, s_name) values ('rdb_context_var_name', 'system');
        insert into tdata (f_name, s_name) values ('rdb_context_var_value', 'system');
        insert into tdata (f_name, s_name) values ('rdb_engine_name', 'system');
        insert into tdata (f_name, s_name) values ('rdb_package_name', 'system');
        insert into tdata (f_name, s_name) values ('rdb_function_id', 'system');
        insert into tdata (f_name, s_name) values ('rdb_function_blr', 'system');
        insert into tdata (f_name, s_name) values ('rdb_argument_name', 'system');
        insert into tdata (f_name, s_name) values ('rdb_argument_mechanism', 'system');
        insert into tdata (f_name, s_name) values ('rdb_identity_type', 'system');
        insert into tdata (f_name, s_name) values ('rdb_boolean', 'system');
        insert into tdata (f_name, s_name) values ('sec$user_name', 'system');
        insert into tdata (f_name, s_name) values ('sec$key', 'system');
        insert into tdata (f_name, s_name) values ('sec$value', 'system');
        insert into tdata (f_name, s_name) values ('sec$name_part', 'system');
        insert into tdata (f_name, s_name) values ('rdb_client_version', 'system');
        insert into tdata (f_name, s_name) values ('rdb_remote_version', 'system');
        insert into tdata (f_name, s_name) values ('rdb_host_name', 'system');
        insert into tdata (f_name, s_name) values ('rdb_os_user', 'system');
        insert into tdata (f_name, s_name) values ('rdb_generator_value', 'system');
        insert into tdata (f_name, s_name) values ('rdb_auth_method', 'system');
        insert into tdata (f_name, s_name) values ('rdb_linger', 'system');
        insert into tdata (f_name, s_name) values ('mon$sec_database', 'system');
        insert into tdata (f_name, s_name) values ('rdb_map_name', 'system');
        insert into tdata (f_name, s_name) values ('rdb_map_using', 'system');
        insert into tdata (f_name, s_name) values ('rdb_map_db', 'system');
        insert into tdata (f_name, s_name) values ('rdb_map_from_type', 'system');
        insert into tdata (f_name, s_name) values ('rdb_map_from', 'system');
        insert into tdata (f_name, s_name) values ('rdb_map_to', 'system');
        insert into tdata (f_name, s_name) values ('rdb_generator_increment', 'system');
        insert into tdata (f_name, s_name) values ('rdb_plan', 'system');
        insert into tdata (f_name, s_name) values ('rdb_system_privileges', 'system');
        insert into tdata (f_name, s_name) values ('rdb_sql_security', 'system');
        insert into tdata (f_name, s_name) values ('mon$idle_timeout', 'system');
        insert into tdata (f_name, s_name) values ('mon$idle_timer', 'system');
        insert into tdata (f_name, s_name) values ('mon$statement_timeout', 'system');
        insert into tdata (f_name, s_name) values ('mon$statement_timer', 'system');
        insert into tdata (f_name, s_name) values ('rdb_time_zone_id', 'system');
        insert into tdata (f_name, s_name) values ('rdb_time_zone_name', 'system');
        insert into tdata (f_name, s_name) values ('rdb_time_zone_offset', 'system');
        insert into tdata (f_name, s_name) values ('rdb_timestamp_tz', 'system');
        insert into tdata (f_name, s_name) values ('rdb_dbtz_version', 'system');
        insert into tdata (f_name, s_name) values ('rdb_crypt_state', 'system');
        insert into tdata (f_name, s_name) values ('mon$wire_crypt_plugin', 'system');
        insert into tdata (f_name, s_name) values ('rdb_publication_name', 'system');
        insert into tdata (f_name, s_name) values ('rdb_file_id', 'system');
        insert into tdata (f_name, s_name) values ('rdb_config_id', 'system');
        insert into tdata (f_name, s_name) values ('rdb_config_name', 'system');
        insert into tdata (f_name, s_name) values ('rdb_config_value', 'system');
        insert into tdata (f_name, s_name) values ('rdb_config_is_set', 'system');
        insert into tdata (f_name, s_name) values ('rdb_replica_mode', 'system');
        insert into tdata (f_name, s_name) values ('rdb_keyword_name', 'system');
        insert into tdata (f_name, s_name) values ('rdb_keyword_reserved', 'system');
        insert into tdata (f_name, s_name) values ('rdb_short_description', 'system');
        insert into tdata (f_name, s_name) values ('rdb_seconds_interval', 'system');
        insert into tdata (f_name, s_name) values ('rdb_profile_session_id', 'system');
        insert into tdata (f_name, s_name) values ('rdb_blob_util_handle', 'system');
        insert into tdata (f_name, s_name) values ('rdb_blob', 'system');
        insert into tdata (f_name, s_name) values ('rdb_varbinary_max', 'system');
        insert into tdata (f_name, s_name) values ('rdb_integer', 'system');
        insert into tdata (f_name, s_name) values ('mon$parallel_workers', 'system');
        insert into tdata (f_name, s_name) values ('s_name', 'system');
        insert into tdata (f_name, s_name) values ('rdb_text_max', 'system');
        insert into tdata (f_name, s_name) values ('rdb_1', 'public');
        insert into tdata (f_name, s_name) values ('rdb_2', 'public');
        commit;
        create table tmain(id int);
        insert into tmain(id) select row_number()over() from rdb$types rows 100;
        commit;

    """

    if act.is_version('<6'):
        init_script += """
            create unique index tdata_unq_fn on tdata (f_name);
            create view v_tdata_nr as select 1 i from tdata rows 50;
            create view v_tdata_ir1 as select 1 i from tdata where f_name > '' rows 50;
            create view v_tdata_ir2 as select 1 i from tdata where f_name > '' order by f_name rows 50;
        """
    else:
        init_script += """
            alter table tdata add constraint tdata_unq_sn_fn unique (s_name, f_name);
            create index tdata_fn on tdata (f_name);

            create view v_tdata_nr as select 1 i from tdata rows 50;
            create view v_tdata_ir1 as select 1 i from tdata where s_name = lower('public') and f_name > '' rows 50;
            create view v_tdata_ir2 as select 1 i from tdata where s_name = lower('public') and f_name > '' order by f_name rows 50;
        """

    act.isql(switches = ['-q'], input = init_script, combine_output = True)
    assert act.clean_stdout == '', f'Init script failed: {act.clean_stdout=}'
    act.reset()

    ##################################################################################

    t_map = { 'tdata' : -1, }

    query1 = """
        select /* case-2 */ count(*) as cnt_via_view from tmain where (select i from v_tdata_nr rows 1) >= 0;
    """

    query2 = """
        select /* case-3b */ count(*) as cnt_via_view from tmain where (select i from v_tdata_ir2 rows 1) >= 0;
    """

    query3 = """
        select /* case-3a */ count(*) as cnt_via_view from tmain where (select i from v_tdata_ir1 rows 1) >= 0;
    """
    q_map = {query1 : '', query2 : '', query3 : ''}
    
    with act.db.connect() as con:
        cur = con.cursor()
        for k in t_map.keys():
            cur.execute(f"select rdb$relation_id from rdb$relations where rdb$relation_name = upper('{k}')")
            test_rel_id = None
            for r in cur:
                test_rel_id = r[0]
            assert test_rel_id, f"Could not find ID for relation '{k}'. Check its name!"
            t_map[ k ] = test_rel_id

        result_map = {}

        for qry_txt in q_map.keys():
            ps, rs = None, None
            try:
                ps = cur.prepare(qry_txt)
                q_map[qry_txt] = ps.detailed_plan
                for tab_nm,tab_id in t_map.items():
                    tabstat1 = [ p for p in con.info.get_table_access_stats() if p.table_id == tab_id ]
            
                    # ::: NB ::: 'ps' returns data, i.e. this is SELECTABLE expression.
                    # We have to store result of cur.execute(<psInstance>) in order to
                    # close it explicitly.
                    # Otherwise AV can occur during Python garbage collection and this
                    # causes pytest to hang on its final point.
                    # Explained by hvlad, email 26.10.24 17:42
                    rs = cur.execute(ps)
                    for r in rs:
                        pass
                    tabstat2 = [ p for p in con.info.get_table_access_stats() if p.table_id == tab_id ]

                    result_map[qry_txt, tab_nm] = \
                        (
                           tabstat2[0].sequential if tabstat2[0].sequential else 0
                          ,tabstat2[0].indexed if tabstat2[0].indexed else 0
                        )
                    if tabstat1:
                        seq, idx = result_map[qry_txt, tab_nm]
                        seq -= (tabstat1[0].sequential if tabstat1[0].sequential else 0)
                        idx -= (tabstat1[0].indexed if tabstat1[0].indexed else 0)
                        result_map[qry_txt, tab_nm] = (seq, idx)
            except DatabaseError as e:
                print( e.__str__() )
                print(e.gds_codes)
            finally:
                if rs:
                    rs.close() # <<< EXPLICITLY CLOSING CURSOR RESULTS
                if ps:
                    ps.free()

        for k,v in result_map.items():
            print(k[0]) # query
            print(f'seq={v[0]}, idx={v[1]}')
            print('')

    act.expected_stdout = f"""
        {query1}
        seq=1, idx=0

        {query2}
        seq=0, idx=1
        
        {query3}
        seq=0, idx=1
    """
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
