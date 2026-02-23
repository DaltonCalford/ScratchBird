#coding:utf-8

"""
ID:          issue-4040
ISSUE:       4040
TITLE:       Cannot drop a NOT NULL constraint on a field participating in the UNIQUE constraint
DESCRIPTION:
JIRA:        CORE-3692
FBTEST:      bugs.core_3692
NOTES:
    [05.10.2023] pzotov
    Removed SHOW command for check result because its output often changes.
"""

import pytest
from firebird.qa import *

init_script = """
    recreate table test (s varchar(250) character set none not null);
    commit;
    alter table test add constraint test_unq unique (s);
    commit;
"""

db = db_factory(init=init_script)

test_script = """
    set bail on;
    set list on;
    alter table test alter column s drop not null;
    commit;
    insert into test(s) values(null);
    select s from test;
"""

act = isql_act('db', test_script, substitutions = [ ('[ \t]+', ''), ] )

expected_stdout = """
    S <null>
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
