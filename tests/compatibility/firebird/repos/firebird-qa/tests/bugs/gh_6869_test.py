#coding:utf-8

"""
ID:          issue-6869
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/6869
TITLE:       Domain CHECK-expression can be ignored when we DROP objects that are involved in it
NOTES:
    [25.02.2023] pzotov
        Confirmed bug on 5.0.0.520.
        Checked on 5.0.0.959 - all OK.
    [04.07.2025] pzotov
        Separated expected output for FB major versions prior/since 6.x.
        No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.
        Checked on 6.0.0.876; 5.0.3.1668.
"""

import pytest
from firebird.qa import *

init_script = """
    create domain dm_int as int;
    create table test(i dm_int);

    set term ^;
    create function fn_test(a_id dm_int) returns dm_int as
    begin
        return ( select min(i) from test );
    end
    ^
    set term ;^
    alter domain dm_int add constraint check ( value = (select fn_test(max(i)) from test) );
    commit;
"""
db = db_factory(init = init_script)

test_script = """
    set term ^;
    execute block as
    begin
        execute statement 'drop table test'; -- PASSED, despite having two dependent objects (function and domain expr.)
        execute statement 'drop function fn_test'; -- PASSED, despite having dependent object (domain expr.)
    end
    ^
    set term ;^
    commit;
"""

act = isql_act('db', test_script)

expected_stdout_5x = """
    Statement failed, SQLSTATE = 42000
    unsuccessful metadata update
    -cannot delete
    -COLUMN TEST.I
    -there are 1 dependencies

    Statement failed, SQLSTATE = 42000
    unsuccessful metadata update
    -cannot delete
    -COLUMN TEST.I
    -there are 1 dependencies
"""

expected_stdout_6x = """
    Statement failed, SQLSTATE = 42000
    unsuccessful metadata update
    -cannot delete
    -COLUMN "PUBLIC"."TEST"."I"
    -there are 1 dependencies
    Statement failed, SQLSTATE = 42000
    unsuccessful metadata update
    -cannot delete
    -COLUMN "PUBLIC"."TEST"."I"
    -there are 1 dependencies
"""

@pytest.mark.version('>=5.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
