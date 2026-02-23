#coding:utf-8

"""
ID:          issue-671
ISSUE:       671
TITLE:       Unsuccessful execution caused by system error <...> bad BLR -- invalid stream
DESCRIPTION:
  Original ticket title: "Lost connexion with Big request"
  Test checks that:
  1) we *can* run query with <N> unions, where <N> is limit specific for 2.5.x vs 3.x
  2) we can *not* run query with <N+1> unions.
  Actual value of <N> is 128 for 2.5.x (NOT 255 as errormessage issues!) and 255 for 3.0.
JIRA:        CORE-335
FBTEST:      bugs.core_0335
"""

import pytest
from firebird.qa import *

init_script = """
    recreate table test(id int);
    commit;
    insert into test select 1 from rdb$types rows 10;
    commit;
"""

db = db_factory(init=init_script)

test_script = """
    set list on;
    set count on;

    -- ##########################
    -- FIRST  QUERY: SHOULD PASS.
    -- ##########################
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union -- 10
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union -- 20
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union -- 30
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union -- 40
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union -- 50
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union -- 60
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union -- 70
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union -- 80
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union -- 90
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union --100

    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union --110
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union --120
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union -- last allowed in 2.5.7; add subsequent leads in 2.5.x to Too many Contexts of Relation/Procedure/Views. Maximum allowed is 255
    select first 5 * from test union
    select first 5 * from test union --130
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union --140
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union --150
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union --160
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union --170
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union --180
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union --190
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union --200

    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union --210
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union --220
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union --230
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union --240
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union --250
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test ------------------- last allowed in 3.0
    ; /* union select first 5 * from test; */


    -- ##########################
    -- SECOND QUERY: SHOULD FAIL.
    -- ##########################
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union -- 10
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union -- 20
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union -- 30
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union -- 40
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union -- 50
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union -- 60
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union -- 70
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union -- 80
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union -- 90
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union --100

    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union --110
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union --120
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union -- last allowed in 2.5.7; add subsequent leads in 2.5.x to Too many Contexts of Relation/Procedure/Views. Maximum allowed is 255
    select first 5 * from test union
    select first 5 * from test union --130
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union --140
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union --150
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union --160
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union --170
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union --180
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union --190
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union --200

    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union --210
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union --220
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union --230
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union --240
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union --250
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test union
    select first 5 * from test ------------------- last allowed in 3.0
    union all select first 5 * from test;

"""

act = isql_act('db', test_script)

expected_stdout = """
    ID                              1
    Records affected: 1
"""

expected_stderr = """
    Statement failed, SQLSTATE = 54001
    Dynamic SQL Error
    -Too many Contexts of Relation/Procedure/Views. Maximum allowed is 256
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.expected_stderr = expected_stderr
    act.execute()
    assert (act.clean_stderr == act.clean_expected_stderr and
            act.clean_stdout == act.clean_expected_stdout)

