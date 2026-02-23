#coding:utf-8

"""
ID:          issue-3359
ISSUE:       3359
TITLE:       FB 2.1 incorrectly works with indexed fields of type DATE in OLD ODS (9.1)
DESCRIPTION:
JIRA:        CORE-2977
FBTEST:      bugs.core_2977
"""

import pytest
from firebird.qa import *

db = db_factory(sql_dialect=1)

test_script = """
    recreate table test(id int, opdate timestamp);
    insert into test values(1, '31.12.2000');
    insert into test values(2, '01.01.2001');
    insert into test values(3, '02.01.2001');
    insert into test values(4, '03.01.2001');
    commit;

    create index test_opdate on test(opdate);
    commit;

    set list on;
    -- Following query will have PLAN (TEST INDEX (TEST_OPDATE))
    select * from test where opdate <= '1/1/2001';
"""

act = isql_act('db', test_script, substitutions=[('01-JAN-', ' 1-JAN-')])

expected_stdout = """
    ID                              1
    OPDATE                          31-DEC-2000
    ID                              2
    OPDATE                           1-JAN-2001
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

