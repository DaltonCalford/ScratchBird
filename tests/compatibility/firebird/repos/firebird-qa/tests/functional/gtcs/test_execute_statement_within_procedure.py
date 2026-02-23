#coding:utf-8

"""
ID:          gtcs.execute-statement-within-procedure
FBTEST:      functional.gtcs.execute_statement_within_procedure
TITLE:       EXECUTE STATEMENT within a stored procedure could lead to a problems
DESCRIPTION:
  ::: NB :::
  ### Name of original test has no any relation with actual task of this test: ###
  https://github.com/FirebirdSQL/fbtcs/blob/master/GTCS/tests/CF_ISQL_27.script

  Discuss in fb-devel (subj: "Vulcan, CF_ISQL_27.sql"):
  https://sourceforge.net/p/firebird/mailman/message/17631672/

  Author said that example from this test did not return any error (and he expacted this)
  plus either did not return value into output parameter or even lead server to crash
  (when such SP was called twise).
  For current FB versions no error occurs and output value is issued w/o any problems.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    create generator gen_test1;
    set generator gen_test1 to 1111111;
    create generator gen_test2;
    set generator gen_test2 to 2222222;
    set term ^;
    create procedure rpl$generator_values
    returns (
       gen_name varchar(31),
       gen_value integer
    )
    as
    begin
      for
        select rdb$generator_name
        from rdb$generators
        where coalesce (rdb$system_flag, 0) = 0
        order by 1
        into :gen_name do
      begin
        execute statement 'select gen_id(' || gen_name || ',0) from rdb$database' into :gen_value;
        suspend;
      end
    end
    ^
    set term ;^
    commit;

    set list on;
    select 'point-1' msg, p.* from rpl$generator_values p;
    select 'point-2' msg, p.* from rpl$generator_values p;

"""

act = isql_act('db', test_script)

expected_stdout = """
    MSG                             point-1
    GEN_NAME                        GEN_TEST1
    GEN_VALUE                       1111111

    MSG                             point-1
    GEN_NAME                        GEN_TEST2
    GEN_VALUE                       2222222

    MSG                             point-2
    GEN_NAME                        GEN_TEST1
    GEN_VALUE                       1111111

    MSG                             point-2
    GEN_NAME                        GEN_TEST2
    GEN_VALUE                       2222222
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
