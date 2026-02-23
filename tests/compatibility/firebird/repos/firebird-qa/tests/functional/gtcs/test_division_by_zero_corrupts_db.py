#coding:utf-8

"""
ID:          gtcs.division-by-zero-corrupts-db
FBTEST:      functional.gtcs.division_by_zero_corrupts_db
TITLE:       Zero divide in SP can crash database when call this SP several times
DESCRIPTION:
  ::: NB :::
  ### Name of original test has no any relation with actual task of this test: ###
  https://github.com/FirebirdSQL/fbtcs/blob/master/GTCS/tests/CF_ISQL_29.script

  Issue in original test:
  Division by 0 corrupt database
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set term ^ ;
    create procedure spx_aux_test (par1 bigint) returns (ret1 bigint)
    as
        declare lok1 bigint ;
        declare itmpvar integer;
    begin
        begin
            lok1=2;
            itmpvar = 1/0;
            when any do
            begin
                exception;
            end
        end
    end
    ^
    commit
    ^
    set term ;^

    connect '$(DSN)' user 'SYSDBA' password 'masterkey'; -- this is done in original script.

    set term ^;
    create or alter procedure spx_aux_test (par1 bigint) returns (ret1 bigint)
    as
        declare lok1 bigint ;
        declare itmpvar integer;
    begin
        begin
            lok1=2;
            itmpvar = 1/0;
        end
    end
    ^
    commit
    ^
    set term ;^

    execute procedure spx_aux_test (1);
    execute procedure spx_aux_test (1);
    execute procedure spx_aux_test (1);
"""

act = isql_act('db', test_script, substitutions=[("(-)At procedure.*", 'At procedure'),
                                                 ('[ \t]+', ' ')])

expected_stdout = """
    Statement failed, SQLSTATE = 22012
    arithmetic exception, numeric overflow, or string truncation
    -Integer divide by zero. The code attempted to divide an integer value by an integer divisor of zero.
    At procedure
    Statement failed, SQLSTATE = 22012
    arithmetic exception, numeric overflow, or string truncation
    -Integer divide by zero. The code attempted to divide an integer value by an integer divisor of zero.
    At procedure
    Statement failed, SQLSTATE = 22012
    arithmetic exception, numeric overflow, or string truncation
    -Integer divide by zero. The code attempted to divide an integer value by an integer divisor of zero.
    At procedure
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
