#coding:utf-8

"""
ID:          issue-2456
ISSUE:       2456
TITLE:       UTF-8 conversion error (string truncation)
DESCRIPTION:
JIRA:        CORE-2019
FBTEST:      bugs.core_2019
"""

import pytest
from firebird.qa import *

init_script = """
recreate table test(column1 varchar(10) character set none collate none );
insert into test values ('1234567890');
commit;
"""


db_1 = db_factory(charset='UTF8', init=init_script)

test_script = """select coalesce(column1, '') from test;"""

act = isql_act('db_1', test_script)

expected_stdout = """
COALESCE
==========
1234567890

"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

