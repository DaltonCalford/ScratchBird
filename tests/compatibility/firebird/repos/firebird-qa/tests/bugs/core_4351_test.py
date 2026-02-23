#coding:utf-8

"""
ID:          issue-4673
ISSUE:       4673
TITLE:       Incorrect default value when adding a new column
DESCRIPTION:
JIRA:        CORE-4351
FBTEST:      bugs.core_4351
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    recreate table test(id int);
    commit;
    insert into test values(1);
    commit;
    alter table test add pwd varchar(50) character set utf8 default 'MdX8fLruCUQ=' not null collate utf8;
    commit;
    set list on;
    select * from test;
    -- WI-V2.1.7.18553: pwd = 'MdX'
"""

act = isql_act('db', test_script)

expected_stdout = """
    ID                              1
    PWD                             MdX8fLruCUQ=
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

