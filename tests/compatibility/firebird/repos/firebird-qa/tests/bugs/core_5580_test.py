#coding:utf-8

"""
ID:          issue-5847
ISSUE:       5847
TITLE:       Signature of packaged functions is not checked for mismatch with [NOT] DETERMINISTIC attribute
DESCRIPTION:
JIRA:        CORE-5580
FBTEST:      bugs.core_5580
NOTES:
    [01.07.2025] pzotov
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.

    Checked on 6.0.0.881; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set term ^;
    recreate package pk1 as
    begin
        function f1() returns int deterministic;
        function f2() returns int not deterministic;
    end
    ^
    recreate package body pk1 as
    begin
        function f1() returns int not deterministic as
        begin
            return 123;
        end

        function f2() returns int not deterministic as
        begin
            return 123 * rand();
        end

    end
    ^
    set term ;^
    commit;

    set list on;

    select pk1.f1() as f1_result from rdb$database;
    select pk1.f2() as f2_result  from rdb$database;
"""

act = isql_act('db', test_script)

expected_stdout_5x = """
    Statement failed, SQLSTATE = 42000
    unsuccessful metadata update
    -RECREATE PACKAGE BODY PK1 failed
    -Function F1 has a signature mismatch on package body PK1

    Statement failed, SQLSTATE = 2F000
    Cannot execute function F1 of the unimplemented package PK1

    Statement failed, SQLSTATE = 2F000
    Cannot execute function F2 of the unimplemented package PK1
"""

expected_stdout_6x = """
    Statement failed, SQLSTATE = 42000
    unsuccessful metadata update
    -RECREATE PACKAGE BODY "PUBLIC"."PK1" failed
    -Function F1 has a signature mismatch on package body "PUBLIC"."PK1"

    Statement failed, SQLSTATE = 2F000
    Cannot execute function "F1" of the unimplemented package "PUBLIC"."PK1"

    Statement failed, SQLSTATE = 2F000
    Cannot execute function "F2" of the unimplemented package "PUBLIC"."PK1"
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
