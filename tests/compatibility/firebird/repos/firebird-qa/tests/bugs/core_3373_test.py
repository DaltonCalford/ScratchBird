#coding:utf-8

"""
ID:          issue-3739
ISSUE:       3739
TITLE:       It is possible to store string with lenght 31 chars into column varchar(25)
DESCRIPTION:
JIRA:        CORE-3373
FBTEST:      bugs.core_3373
"""

import pytest
from firebird.qa import *

init_script = """
    recreate table t1(c varchar(25));
    recreate table t2(c varchar(25));
    commit;

    set term ^;
    execute block as
    begin
        execute statement 'drop domain dm_vc25';
    when any do
        begin end
    end
    ^
    set term ;^
    commit;

    create domain dm_vc25 varchar(25) character set utf8;
    commit;
    recreate table t2(c dm_vc25);
    commit;
"""

db = db_factory(sql_dialect=3, init=init_script)

test_script = """
    set count on;
    set echo on;
    insert into t1(c) values ('1234567890123456789012345xxxxxx');
    insert into t2(c) values ('1234567890123456789012345xxxxxx');
"""

act = isql_act('db', test_script)

expected_stdout = """
    insert into t1(c) values ('1234567890123456789012345xxxxxx');
    Records affected: 0
    insert into t2(c) values ('1234567890123456789012345xxxxxx');
    Records affected: 0
"""

expected_stderr = """
    Statement failed, SQLSTATE = 22001
    arithmetic exception, numeric overflow, or string truncation
    -string right truncation
    -expected length 25, actual 31
    Statement failed, SQLSTATE = 22001
    arithmetic exception, numeric overflow, or string truncation
    -string right truncation
    -expected length 25, actual 31
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.expected_stderr = expected_stderr
    act.execute()
    assert (act.clean_stderr == act.clean_expected_stderr and
            act.clean_stdout == act.clean_expected_stdout)

