#coding:utf-8

"""
ID:          issue-320
ISSUE:       320
JIRA:        CORE-2
TITLE:       Incorrect value returned with execute statement calculation
DESCRIPTION:
FBTEST:      bugs.core_0000
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    create table t1 (
        campo1 numeric(15,2),
        campo2 numeric(15,2)
    );
    commit;
    set term ^;
    create procedure teste
    returns (
        retorno numeric(15,2))
    as
    begin
      execute statement 'select first 1 (campo1*campo2) from t1' into :retorno;
      suspend;
    end
    ^
    set term ;^
    commit;

    insert into t1 (campo1, campo2) values (10.5, 5.5);
    commit;

    select * from teste;
"""

act = isql_act('db', test_script)

expected_stdout = """
    RETORNO                         57.75
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

