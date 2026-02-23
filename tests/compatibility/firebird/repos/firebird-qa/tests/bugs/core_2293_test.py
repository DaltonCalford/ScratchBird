#coding:utf-8

"""
ID:          issue-2718
ISSUE:       2718
TITLE:       Wrong dependent object type (RELATION) in RDB$DEPEDENCIES for VIEW's
DESCRIPTION:
JIRA:        CORE-2293
FBTEST:      bugs.core_2293
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    create or alter procedure p_v as begin end;
    create or alter view v (id) as select 1 id from rdb$database;
    commit;

    set term ^ ;
    create or alter procedure p_v as
        declare x int;
    begin
        select id from v into :x;
        select 1 from rdb$database into :x;
    end ^
    set term ; ^
    commit;

    set list on;
    select d.rdb$depended_on_type, t.rdb$type_name -- according to the ticket text we have to check only these two columns
    from rdb$dependencies d
    join rdb$types t on
        d.rdb$depended_on_type = t.rdb$type
        and t.rdb$field_name = upper('RDB$OBJECT_TYPE')
    where d.rdb$dependent_name = upper('P_V');
"""

act = isql_act('db', test_script)

expected_stdout = """
    RDB$DEPENDED_ON_TYPE            0
    RDB$TYPE_NAME                   RELATION
    RDB$DEPENDED_ON_TYPE            1
    RDB$TYPE_NAME                   VIEW
    RDB$DEPENDED_ON_TYPE            1
    RDB$TYPE_NAME                   VIEW
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

