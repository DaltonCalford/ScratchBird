#coding:utf-8

"""
ID:          issue-6162
ISSUE:       6162
TITLE:       An attempt to create global mapping with long (greater than SQL identifier length) FROM field fails
DESCRIPTION:
JIRA:        CORE-5904
FBTEST:      bugs.core_5904
"""

import pytest
from firebird.qa import *

db = db_factory(charset='UTF8')

test_script = """
    set count on;
    set list on;
    set bail on;
    recreate view v_test as
    select
        rdb$map_name,
        rdb$map_using,
        rdb$map_plugin,
        rdb$map_db,
        rdb$map_from_type,
        rdb$map_from,
        rdb$map_to_type,
        rdb$map_to
    from rdb$auth_mapping;
    commit;
    create or alter mapping krasnorutskayag using plugin win_sspi from user 'DOMN\\КрасноруцкаяАА' to user "DOMN\\Krasnorutskaya";
    create or alter global mapping krasnorutskayag using plugin win_sspi from user 'DOMN\\КрасноруцкаяАА' to user "DOMN\\Krasnorutskaya";
    commit;
    select * from v_test;
    drop mapping KrasnorutskayaG;
    drop global mapping KrasnorutskayaG;
    commit;
    select * from v_test;
"""

act = isql_act('db', test_script)

expected_stdout = """
    RDB$MAP_NAME                    KRASNORUTSKAYAG
    RDB$MAP_USING                   P
    RDB$MAP_PLUGIN                  WIN_SSPI
    RDB$MAP_DB                      <null>
    RDB$MAP_FROM_TYPE               USER
    RDB$MAP_FROM                    DOMN\\КрасноруцкаяАА
    RDB$MAP_TO_TYPE                 0
    RDB$MAP_TO                      DOMN\\Krasnorutskaya
    Records affected: 1
    Records affected: 0
"""

@pytest.mark.version('>=3.0.4')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
