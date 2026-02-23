#coding:utf-8

"""
ID:          issue-5128
ISSUE:       5128
TITLE:       Revoke all on all from role <R> -- failed with "SQL role <R> does not exist in security database"
DESCRIPTION:
JIRA:        CORE-4831
FBTEST:      bugs.core_4831
"""

import pytest
from firebird.qa import *

db = db_factory()

tmp_role = role_factory('db', name='r_20150608_20h03m')
act = isql_act('db')

@pytest.mark.version('>=3.0')
def test_1(act: Action, tmp_role: Role):

    test_sql = f"""
        set list on;
        set count on;
        set wng off;

        -- this failed, possibly by: http://sourceforge.net/p/firebird/code/61729
        -- alexpeshkoff 2015-06-04
        -- Postfix for CORE-4821: fixed segfault in REVOKE ALL ON ALL
        revoke all on all from role {tmp_role.name};

        commit;
        select
            g.rdb$privilege
            ,g.rdb$grant_option
            ,g.rdb$relation_name
            ,g.rdb$user
            ,g.rdb$object_type
        from rdb$user_privileges g
        join sec$users u on g.rdb$user = u.sec$user_name
        where g.rdb$user = '{tmp_role.name.upper()}'
        ;
    """
 
    act.expected_stdout = """
        Records affected: 0
    """

    act.isql(input = test_sql, combine_output = True)

    assert act.clean_stdout == act.clean_expected_stdout
