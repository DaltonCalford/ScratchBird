#coding:utf-8

"""
ID:          issue-4259
ISSUE:       4259
TITLE:       Bugcheck 291 (cannot find record back version) if GTT is modified concurrently
  using at least one read-committed read-only transaction
DESCRIPTION:
JIRA:        CORE-3924
FBTEST:      bugs.core_3924
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    recreate global temporary table gt(f01 int) on commit preserve rows;
    commit;
    insert into gt values(1);
    commit;
    set transaction read only read committed record_version;
    delete from gt;
    set term ^;
    execute block as
    begin
        in autonomous transaction
        do update gt set f01=-1;
    end
    ^
    set term ;^
    commit;
"""

act = isql_act('db', test_script,
                 substitutions=[('-concurrent.*', ''),
                                ('-At block line: [\\d]+, col: [\\d]+', '-At block line')])

expected_stderr = """
    Statement failed, SQLSTATE = 40001
    deadlock
    -update conflicts with concurrent update
    -At block line: 1, col: 53
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stderr = expected_stderr
    act.execute()
    assert act.clean_stderr == act.clean_expected_stderr

