#coding:utf-8

"""
ID:          syspriv.change-mapping-rules
TITLE:       Check ability to manage auth mappings
DESCRIPTION: Verify ability to issue CREATE / ALTER / DROP MAPPING by non-sysdba user.
FBTEST:      functional.syspriv.change_mapping_rules
"""

import pytest
from firebird.qa import *

db = db_factory()

test_user = user_factory('db', name='john_smith_mapping_manager', do_not_create=True)
test_role = role_factory('db', name='tmp_role_for_change_mapping', do_not_create=True)

test_script = """
    set wng off;
    set count on;
    set list on;

    create or alter view v_map_info as
    select
        rdb$map_name  as map_name
        ,rdb$map_using as map_using
        ,rdb$map_plugin as map_plugin
        ,rdb$map_db as map_db
        ,rdb$map_from_type as map_from_type
        ,rdb$map_from as map_from
        ,rdb$map_to_type as map_to_type
        ,rdb$map_to as map_to
        ,rdb$system_flag as map_sys_flag
        -- ,rdb$description as map_descr
    from rdb$auth_mapping
    union all
    select
        sec$map_name
        ,sec$map_using
        ,sec$map_plugin
        ,sec$map_db
        ,sec$map_from_type
        ,sec$map_from
        ,sec$map_to_type
        ,sec$map_to
        ,1
        -- ,sec$description
    from sec$global_auth_mapping
    ;
    commit;
    grant select on v_map_info to public;

    -- NB: without 'grant admin role' it is unable to create GLOBAL mapping:
    -- Statement failed, SQLSTATE = 28000 / ... / -CREATE OR ALTER MAPPING ... failed
    -- -Unable to perform operation /-System privilege CHANGE_MAPPING_RULES is missing
    create or alter
         user john_smith_mapping_manager
         password '123'
         grant admin role --- [ !!! ]
    ;

    set term ^;
    execute block as
    begin
        execute statement 'drop role tmp_role_for_change_mapping';
        when any do begin end
    end^
    set term ;^

    create role tmp_role_for_change_mapping set system privileges to CHANGE_MAPPING_RULES;
    commit;

    grant default tmp_role_for_change_mapping to user john_smith_mapping_manager;
    commit;

    connect '$(DSN)' user john_smith_mapping_manager password '123'; --  role tmp_role_for_change_mapping;

    create or alter mapping tmp_syspriv_local_map using plugin srp from any user to user;
    create or alter global mapping tmp_syspriv_global_map using plugin srp from any user to user;
    commit;

    select * from v_map_info;
    commit;

    drop global mapping tmp_syspriv_global_map;
    drop mapping tmp_syspriv_local_map;
    commit;

    select * from v_map_info;
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    MAP_NAME TMP_SYSPRIV_LOCAL_MAP
    MAP_USING P
    MAP_PLUGIN SRP
    MAP_DB <null>
    MAP_FROM_TYPE USER
    MAP_FROM *
    MAP_TO_TYPE 0
    MAP_TO <null>
    MAP_SYS_FLAG 0
    MAP_NAME TMP_SYSPRIV_GLOBAL_MAP
    MAP_USING P
    MAP_PLUGIN SRP
    MAP_DB <null>
    MAP_FROM_TYPE USER
    MAP_FROM *
    MAP_TO_TYPE 0
    MAP_TO <null>
    MAP_SYS_FLAG 1
    Records affected: 2
    Records affected: 0
"""

@pytest.mark.version('>=4.0')
def test_1(act: Action, test_user, test_role):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
