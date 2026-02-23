#coding:utf-8

"""
ID:          issue-4498
ISSUE:       4498
TITLE:       Creating external function (udf) to not existing dll - and then procedure with it - crash server
DESCRIPTION:
  *** FOR FB 4.X AND ABOVE  ***
  Added separate code for running on FB 4.0.x: we use create UDR function statement and specify
  non-existent library 'unknown_udf!UC_div'. The statement per se will pass and rdb$functions
  *will* contain record for just created function. But following COMMT will raise exception:
    Statement failed, SQLSTATE = HY000
    UDR module not loaded
    <localized message here>
  Then we rollback and query rdb$functions again. No record about this function must be there.

  STDERR is ignored in this test because of localized message about missed library.
NOTES:
[08.02.2022] pcisar
  Fails on Windows 3.0.8 with unexpected additional output line:
    + Rolling back work.
      Rolling back work.
      Statement failed, SQLSTATE = 39000
      .* at offset
      -function DUMMY_EXT is not defined
      -module name or entrypoint could not be found)

[04.03.2022] pzotov: RESOLVED.
  Problem on Windows 3.0.8 caused by excessive query:
      "select current_user, current_role from rdb$database"
  -- which is done by ISQL 3.x when it gets commands from STDIN via PIPE mechanism.
  Discussed with Alex et al, since 28-feb-2022 18:05 +0300.
  Alex explanation: 28-feb-2022 19:52 +0300
  subj: "Firebird new-QA: weird result for trivial test (outcome depends on presence of... running trace session!)"

  NOTE-1: according to source issue from ticket, creation of temporary database not required here.
  In old .fbt test this auto-created DB is immediately closed and script further makes two temp DBs.
  In contrary to .fbt, here connection to auto-created temporary database ('C:/TEMP/PYTEST.../TEST_.../TEST.FDB')
  is opened up to creation of first DB from SQL script  (create database '{str(temp_db_1_b)}';).
  We can suppress 'Rolling back' message by trivial action: add COMMIT or ROLLBACK before this 'create database ...'

  NOTE-2: I could not reproduce bugcheck on 32-bit snapshot WI-T3.0.0.30566 Alpha 1 (timestamp: 31-JUL-2013).
  Tried both SS and CS, used forlder with space characters in name (as it was in the source example) - no matter.


JIRA:        CORE-4172
FBTEST:      bugs.core_4172
"""

import pytest
import re
import platform
import locale
from pathlib import Path
from firebird.qa import *

db = db_factory()

# version: 3.0

act_1 = python_act('db', substitutions=[('.* at offset.*', '.* at offset')])

expected_stdout_1 = """
    X                               1
"""

expected_stderr_1 = """
    Statement failed, SQLSTATE = 39000
    invalid request BLR at offset
    -function DUMMY_EXT is not defined
    -module name or entrypoint could not be found
"""

temp_db_1_a = temp_file('tmp_4172_1.fdb')
temp_db_1_b = temp_file('tmp_4172_2.fdb')

@pytest.mark.version('>=3.0,<4')
def test_1(act_1: Action, temp_db_1_a: Path, temp_db_1_b: Path):
    test_script = f"""
    commit;
    create database '{str(temp_db_1_b)}';
    commit;
    create database '{str(temp_db_1_a)}';

    set autoddl off;
    commit;

    declare external function dummy_ext
    integer
    returns integer by value
    entry_point 'dummy_ext'
    module_name 'non_existing_udf.dll';
    commit;

    set term ^;
    create procedure sp_test ( a_id integer ) returns  ( o_name integer ) as
    begin
      o_name = dummy_ext(a_id);
      suspend;
    end
    ^
    set term ;^
    commit;

    rollback;

    connect '{str(temp_db_1_b)}';
    set list on;
    select 1 as x from rdb$database;
"""
    act_1.expected_stdout = expected_stdout_1
    act_1.expected_stderr = expected_stderr_1
    act_1.isql(switches=['-q'], input=test_script)
    assert (act_1.clean_stderr == act_1.clean_expected_stderr and
            act_1.clean_stdout == act_1.clean_expected_stdout)


#####################################################################################

# version: 4.0

act_2 = python_act('db')

test_script_2 = """
    recreate view v_check as
    select rdb$function_name, rdb$entrypoint, rdb$engine_name, rdb$legacy_flag
    from rdb$functions
    where rdb$system_flag is distinct from 1 and rdb$function_name starting with upper( 'the_' )
    ;
    commit;

    set list on;
    set term ^;
    execute block returns( o_gdscode int ) as
    begin
        begin
            execute statement
              q'{
                    create function the_div (
                        n1 integer,
                        n2 integer
                    ) returns double precision
                        external name 'unknown_udf!UC_div'
                        engine udr
                }';
                -- was: external name 'udf_compat!UC_div'

        when any do
            begin
                o_gdscode = gdscode;
            end
        end
        suspend;
    end
    ^
    set term ;^

    commit;

    set blob all;
    set count on;

    select * from v_check;
    rollback;

    select * from v_check;
    rollback;
"""

expected_stdout_2 = """
O_GDSCODE                       <null>
RDB$FUNCTION_NAME               THE_DIV
RDB$ENTRYPOINT                  unknown_udf!UC_div
RDB$ENGINE_NAME                 UDR
RDB$LEGACY_FLAG                 0
Records affected: 1
Records affected: 0

"""

@pytest.mark.version('>=4.0')
def test_2(act_2: Action):
    act_2.expected_stderr = 'We expect error, but ignore it'
    act_2.expected_stdout = expected_stdout_2
    act_2.isql(switches=[], input=test_script_2, io_enc=locale.getpreferredencoding())
    assert act_2.clean_stdout == act_2.clean_expected_stdout

