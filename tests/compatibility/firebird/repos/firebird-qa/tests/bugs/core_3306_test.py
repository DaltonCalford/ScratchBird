#coding:utf-8

"""
ID:          issue-3673
ISSUE:       3673
TITLE:       Invariant sub-query is treated as variant thus causing multiple invocations of a nested stored procedure
DESCRIPTION:
JIRA:        CORE-3306
FBTEST:      bugs.core_3306
"""

import pytest
from firebird.qa import *

init_script = """
    set term ^;
    create table test(field1 varchar(100))
    ^
    create or alter procedure sp_test (pname varchar(2000)) returns (svalue varchar(255)) as
    begin
        insert into test(field1) values(:pname);
        svalue = :pname;
        suspend;
    end
    ^
    set term ;^
    commit;
    select count(*)
    from rdb$types
    where rdb$field_name like (select upper(svalue) from sp_test('simsim'));
    commit;
"""

db = db_factory(init=init_script)

test_script = """
    set list on;
    select count(*) from test;
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    COUNT 1
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout

