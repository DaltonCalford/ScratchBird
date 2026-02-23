#coding:utf-8

"""
ID:          issue-3970
ISSUE:       3970
TITLE:       Window Function: cume_dist()
DESCRIPTION:
  Could not find any interesting sample with this function. Decided to use string
  comparison with unicode_ci_ai collation.
JIRA:        CORE-3617
FBTEST:      bugs.core_3617
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    create domain dm_utf8 varchar(20) character set utf8 collate unicode_ci_ai;
    commit;
    recreate table test_ciai( id int, s dm_utf8 );
    commit;

    insert into test_ciai values(1, 'CANción');
    insert into test_ciai values(2, 'peluqueria');
    insert into test_ciai values(3, 'peluQUEría');
    insert into test_ciai values(4, 'cancíON');
    commit;

    set list on;

    select s, id, cume_dist()over(order by s) as c_dist
    from test_ciai
    order by cume_dist()over(order by id desc);
"""

act = isql_act('db', test_script)

expected_stdout = """
    S                               cancíON
    ID                              4
    C_DIST                          0.5000000000000000

    S                               peluQUEría
    ID                              3
    C_DIST                          1.000000000000000

    S                               peluqueria
    ID                              2
    C_DIST                          1.000000000000000

    S                               CANción
    ID                              1
    C_DIST                          0.5000000000000000
"""

@pytest.mark.version('>=4.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

