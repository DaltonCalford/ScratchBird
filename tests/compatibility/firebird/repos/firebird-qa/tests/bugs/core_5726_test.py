#coding:utf-8

"""
ID:          issue-5992
ISSUE:       5992
TITLE:       Unclear error message when inserting value exceeding max of dec_fixed decimal
DESCRIPTION:
    FB40SS, build 4.0.0.1008: OK, 1.641s.
    Previously used:
        create table extdecimal( dec34_34 decimal(34, 34) );
        insert into extdecimal values(1);
    -- and this raised following exception:
        SQLCODE: -901
        Decimal float invalid operation.  An indeterminant error occurred during an operation.
        numeric value is out of range
    ==================================
    Since 30.10.2019 DDL was changed:
      create table test(n numeric(38,38) );
      insert into test values( 1.70141183460469231731687303715884105727 ); -- must PASS
      insert into test values( 1.70141183460469231731687303715884105727001 ); -- must FAIL.
    Explanation:
      1.70141183460469231731687303715884105727 represents
      2^127-1 // 170141183460469231731687303715884105728-1

    Checked on:  4.0.0.1635
JIRA:        CORE-5726
FBTEST:      bugs.core_5726
NOTES:
    [25.06.2020] pzotov
        4.0.0.2076: type in SQLDA was changed from numeric to int128 
        (adjusted output after discussion with Alex about CORE-6342).
    [13.12.2023] pzotov
        Added 'SQLSTATE' in substitutions: runtime error must not be filtered out by '?!(...)' pattern
        ("negative lookahead assertion", see https://docs.python.org/3/library/re.html#regular-expression-syntax).
        Added 'combine_output = True' in order to see SQLSTATE if any error occurs.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    recreate table test (
       id integer generated always as identity primary key,
       n numeric(38,38)
    );
    commit;
    insert into test(n) values( 1.70141183460469231731687303715884105727 );
    insert into test(n) values( 1.70141183460469231731687303715884105727001 );
    set sqlda_display on;
    select n as "max_precise_number" from test;
"""

act = isql_act('db', test_script, substitutions = [ ('^((?!(SQLSTATE|sqltype|max_precise_number)).)*$', ''), ('[ \t]+', ' '), ('.*alias.*', '') ] )

expected_stdout = """
    Statement failed, SQLSTATE = 22003
    arithmetic exception, numeric overflow, or string truncation
    -numeric value is out of range

    01: sqltype: 32752 INT128 Nullable scale: -38 subtype: 1 len: 16
    max_precise_number 1.70141183460469231731687303715884105727
"""

@pytest.mark.version('>=4.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
