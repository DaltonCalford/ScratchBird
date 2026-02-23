#coding:utf-8

"""
ID:          issue-4698
ISSUE:       4698
TITLE:       Preparation of erroneous DDL statement does not show the main command failed
DESCRIPTION:
JIRA:        CORE-4376
FBTEST:      bugs.core_4376
NOTES:
    [29.06.2025] pzotov
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.

    Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    recreate table test1(id int, name varchar(30));
    commit;

    set term ^;
    create or alter procedure sp_test(a_id int) returns (o_name varchar(30)) as
    begin
        select text from test where id = :a_id into o_name;
        suspend;
    end
    ^

    create or alter package pkg_test as
    begin
      procedure sp_test1a(a_id int) returns (o_name varchar(30));
      procedure sp_test1b(a_id int) returns (o_name varchar(30));
      procedure sp_test1c(a_id int) returns (o_name varchar(30));
    end
    ^

    recreate package body pkg_test as
    begin

      procedure sp_test1a(a_id int) returns (o_name varchar(30)) as
      begin
        select name from test1 where id = :a_id into o_name;
        suspend;
      end

      procedure sp_test1b(a_id int) returns (o_name varchar(30)) as
      begin
        select non_existent_field from test1 where id = :a_id into o_name;
        suspend;
      end

      procedure sp_test1c(a_id int) returns (o_name varchar(30)) as
      begin
        select name from non_existent_table where id = :a_id into o_name;
        suspend;
      end
    end
    ^
"""

substitutions = [(r'^\s*(-)?At line.*', '')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout_5x = """
    Statement failed, SQLSTATE = 42S02
    unsuccessful metadata update
    -CREATE OR ALTER PROCEDURE SP_TEST failed
    -Dynamic SQL Error
    -SQL error code = -204
    -Table unknown
    -TEST

    Statement failed, SQLSTATE = 42S22
    unsuccessful metadata update
    -RECREATE PACKAGE BODY PKG_TEST failed
    -Dynamic SQL Error
    -SQL error code = -206
    -Column unknown
    -NON_EXISTENT_FIELD
"""

expected_stdout_6x = """
    Statement failed, SQLSTATE = 42S02
    unsuccessful metadata update
    -CREATE OR ALTER PROCEDURE "PUBLIC"."SP_TEST" failed
    -Dynamic SQL Error
    -SQL error code = -204
    -Table unknown
    -"TEST"

    Statement failed, SQLSTATE = 42S22
    unsuccessful metadata update
    -RECREATE PACKAGE BODY "PUBLIC"."PKG_TEST" failed
    -Dynamic SQL Error
    -SQL error code = -206
    -Column unknown
    -"NON_EXISTENT_FIELD"
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
