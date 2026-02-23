#coding:utf-8

"""
ID:          issue-7124
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/7124
TITLE:       Inconsistent RDB$USER_PRIVILEGES after dropping identity
DESCRIPTION:
NOTES:
    [28.02.2023] pzotov
    Confirmed bug on 4.0.1.2692.
    Checked on 5.0.0.961, 4.0.3.2903 - all OK.
"""

import pytest
from firebird.qa import *

db = db_factory()
act = python_act('db')

expected_stdout = """
    RDB_NAME                        rdb$generators
    OBJ_NAME                        RDB$1
    RDB_NAME                        rdb$privileges
    OBJ_NAME                        RDB$1
    Records affected: 2
    Records affected: 0
"""
@pytest.mark.version('>=4.0.2')
def test_1(act: Action):
    test_sql = """
        set list on;
        set count on;

        create table test(foo int generated always as identity);
        commit;

        create view v_get_info as
        select 'rdb$generators' as rdb_name, rdb$generator_name as obj_name from rdb$generators where rdb$generator_name = 'RDB$1'
        UNION ALL
        select 'rdb$privileges', rdb$relation_name from rdb$user_privileges where rdb$relation_name = 'RDB$1' and rdb$object_type = 14
        ;

        select * from v_get_info;

        alter table test alter foo drop identity;
        commit;

        select * from v_get_info v;
    """
    act.expected_stdout = expected_stdout
    act.isql(switches=['-q'], input = test_sql, combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
