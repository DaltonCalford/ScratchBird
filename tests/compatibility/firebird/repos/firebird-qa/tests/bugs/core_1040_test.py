#coding:utf-8

"""
ID:          issue-1457
ISSUE:       1457
TITLE:       Wrong single-segment ascending index on character field with NULL and empty string values
DESCRIPTION:
JIRA:        CORE-1040
FBTEST:      bugs.core_1040
"""

import pytest
from firebird.qa import *

init_script = """
    recreate table test (x varchar(10));
    commit;
    insert into test select '' from rdb$types,(select 1 x from rdb$types rows 10);
    insert into test values (null);
    commit;

    create index test_x on test (x);
    commit;
"""

db = db_factory(init=init_script)

test_script = """
    set list on;
    -- set plan on;
    select count(*) from test where x is null;
"""

substitutions = [('[ \t]+', ' '), ('=', '')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    COUNT 1
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout

