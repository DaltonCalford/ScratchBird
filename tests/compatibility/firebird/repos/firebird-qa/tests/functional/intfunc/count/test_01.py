#coding:utf-8

"""
ID:          intfunc.count-01
TITLE:       COUNT - Select from empty table
DESCRIPTION:
FBTEST:      functional.intfunc.count.01
"""

import pytest
from firebird.qa import *

db = db_factory(init="create table test(id int);")

test_script = """
    set list on;
    select count(*) from test;
    commit;
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    COUNT 0
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout

