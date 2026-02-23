#coding:utf-8

"""
ID:          issue-6036
ISSUE:       6036
TITLE:       PSQL cursor doesn't see inserted record
DESCRIPTION:
JIRA:        CORE-5773
FBTEST:      bugs.core_5773
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    set count on;
    create or alter procedure sp_test as begin end;
    recreate table test (id bigint);
    commit;

    set term ^;
    create or alter procedure sp_test returns (
        rowcount integer
    ) as
        declare id bigint;
        declare c_ins cursor for (
            select id from test
        );
    begin
        insert into test(id) values(1);
        open c_ins;
            fetch c_ins into :id;
            rowcount = row_count;
            suspend;
        close c_ins;
    end^
    set term ;^
    select * from sp_test;
"""

act = isql_act('db', test_script)

expected_stdout = """
    ROWCOUNT                        1
    Records affected: 1
"""

@pytest.mark.version('>=3.0.4')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
