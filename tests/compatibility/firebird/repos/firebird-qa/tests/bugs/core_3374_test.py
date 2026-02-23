#coding:utf-8

"""
ID:          issue-3740
ISSUE:       3740
TITLE:       Server may crash or corrupt data if SELECT WITH LOCK is issued against records not in the latest format
DESCRIPTION:
JIRA:        CORE-3374
FBTEST:      bugs.core_3374
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    create table test (col1 int, col2 varchar(10), col3 date);
    insert into test values (1, 'qwerty', date '01.01.2015');
    alter table test drop col2;
    set list on;
    select * from test order by col1 with lock; -- crash here
"""

act = isql_act('db', test_script)

expected_stdout = """
    COL1                            1
    COL3                            2015-01-01
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

