#coding:utf-8

"""
ID:          issue-4445
ISSUE:       4445
TITLE:       COMPUTED BY field is evaluated as NULL if used as an exception parameter directly
DESCRIPTION:
  Exception with computed by field as parameter produces error message without this field
  value if this field is not a part of excpression
JIRA:        CORE-4117
FBTEST:      bugs.core_4117
NOTES:
    [28.06.2025] pzotov
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.

    Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

init_script = """
    recreate table test1 (id int not null, c computed by (id * 10));
    recreate table test2 (id int not null, c computed by (id * 10));
    commit;

    set term ^;
    execute block as
    begin
      begin
      execute statement 'drop exception ex_bad_computed_field_value';
      when any do begin end
      end
    end
    ^
    set term ;^
    commit;

    create exception ex_bad_computed_field_value '';
    commit;
    insert into test1 (id) values (1);
    insert into test2 (id) values (1);
    commit;

    -- 1. exception with computed by field as parameter
    set term ^;
    create or alter trigger test1_bu for test1 active before update position 0 as
    begin
      exception ex_bad_computed_field_value new.c;
    end
    ^
    set term ;^
    commit;

    -- 2. exception with computed by field as a part of expression
    set term ^;
    create or alter trigger test2_bu for test2 active before update position 0 as
    begin
      exception ex_bad_computed_field_value new.c + 0; -- any expression
    end
    ^
    set term ;^
    commit;

    -- Confirmed on 2.5.2: statement update test1 ... will raise exception WITHOUT value:
    -- Statement failed, SQLSTATE = 42000
    -- exception 1
    -- -EX_BAD_COMPUTED_FIELD_VALUE
    -- -At trigger 'TEST1_BU' line: 3, col: 7

    -- Compare with 2.5.5:
    -- Statement failed, SQLSTATE = 42000
    -- exception 2
    -- -EX_BAD_COMPUTED_FIELD_VALUE
    -- -20 <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< this is added here <<<<<<<<<<<<<
    -- -At trigger 'TEST1_BU' line: 3, col: 7

"""

db = db_factory(init=init_script)

test_script = """
    update test1 set id = 2;
    update test2 set id = 2;
"""

act = isql_act('db', test_script, substitutions=[('line: \\d+.*', '')])

expected_stdout_5x = """
    Statement failed, SQLSTATE = HY000
    exception 1
    -EX_BAD_COMPUTED_FIELD_VALUE
    -20
    -At trigger 'TEST1_BU'

    Statement failed, SQLSTATE = HY000
    exception 1
    -EX_BAD_COMPUTED_FIELD_VALUE
    -20
    -At trigger 'TEST2_BU'
"""

expected_stdout_6x = """
    Statement failed, SQLSTATE = HY000
    exception 1
    -"PUBLIC"."EX_BAD_COMPUTED_FIELD_VALUE"
    -20
    -At trigger "PUBLIC"."TEST1_BU"

    Statement failed, SQLSTATE = HY000
    exception 1
    -"PUBLIC"."EX_BAD_COMPUTED_FIELD_VALUE"
    -20
    -At trigger "PUBLIC"."TEST2_BU"
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
