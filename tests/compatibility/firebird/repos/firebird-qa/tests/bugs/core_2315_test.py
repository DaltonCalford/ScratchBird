#coding:utf-8

"""
ID:          issue-2739
ISSUE:       2739
TITLE:       Firebird float support does not conform to Interbase spec
DESCRIPTION:
JIRA:        CORE-2315
FBTEST:      bugs.core_2315
"""

import pytest
from firebird.qa import *

init_script = """create table float_test (i integer, f float);
"""

db = db_factory(init=init_script)

test_script = """insert into float_test values (1, 3.0);
insert into float_test values (1, 3.402823466e+38);
select * from float_test;
"""

act = isql_act('db', test_script)

expected_stdout = """
           I              F
============ ==============
           1      3.0000000
           1  3.4028235e+38

"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

