#coding:utf-8

"""
ID:          issue-512
ISSUE:       512
TITLE:       DB crashes if trigger BU deletes own row
DESCRIPTION:
NOTES:
Ortiginal test:
https://github.com/FirebirdSQL/fbtcs/blob/master/GTCS/tests/CF_ISQL_28.script
JIRA:        CORE-185
FBTEST:      bugs.core_0185
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    create table test (id integer not null);
    commit;
    set term ^ ;
    create trigger test_bu for test active before update position 0 as
    begin
        delete from test where id=old.id;
    end
    ^
    set term ; ^
    commit;

    insert into test values (1);
    insert into test values (2);
    insert into test values (3);
    insert into test values (4);
    insert into test values (5);
    insert into test values (6);
    commit;

    update test set id=-1 where id=1;
    rollback;
    set list on;
    select count(*) from test;
"""

act = isql_act('db', test_script)

expected_stdout = """
    COUNT                           6
"""
expected_stderr = """
    Statement failed, SQLSTATE = 22000
    no current record for fetch operation
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.expected_stderr = expected_stderr
    act.execute()
    assert (act.clean_stderr == act.clean_expected_stderr and
            act.clean_stdout == act.clean_expected_stdout)

