#coding:utf-8

"""
ID:          issue-4695
ISSUE:       4695
TITLE:       Duplicate names in package are not checked
DESCRIPTION:
JIRA:        CORE-4373
FBTEST:      bugs.core_4373
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
    set term ^ ;

    create package new_package
    as
    begin
       procedure external_proc;
       procedure external_proc;
       procedure external_proc;
    end^


    create package body new_package
    as
    begin
      procedure external_proc as
      begin
      end

      procedure internal_proc as
      begin
      end
      procedure internal_proc as
      begin
      end
      procedure internal_proc as
      begin
      end
    end^

    set term ; ^
"""

act = isql_act('db', test_script)

expected_stdout_5x = """
    Statement failed, SQLSTATE = 42000
    CREATE PACKAGE NEW_PACKAGE failed
    -Duplicate PROCEDURE EXTERNAL_PROC

    Statement failed, SQLSTATE = 42000
    CREATE PACKAGE BODY NEW_PACKAGE failed
    -Duplicate PROCEDURE INTERNAL_PROC
"""

expected_stdout_6x = """
    Statement failed, SQLSTATE = 42000
    CREATE PACKAGE "PUBLIC"."NEW_PACKAGE" failed
    -Duplicate PROCEDURE "EXTERNAL_PROC"

    Statement failed, SQLSTATE = 42000
    CREATE PACKAGE BODY "PUBLIC"."NEW_PACKAGE" failed
    -Duplicate PROCEDURE "INTERNAL_PROC"
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
