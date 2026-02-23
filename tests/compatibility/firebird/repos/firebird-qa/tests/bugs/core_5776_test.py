#coding:utf-8

"""
ID:          issue-6039
ISSUE:       6039
TITLE:       "Input parameter mismatch" error after altering external function into PSQL function
DESCRIPTION:
  *** FOR FB 3.X ONLY ***
  Test uses UDF 'sright' declared in ib_udf.sql script which for sure present in every FB snapshot.
  After this, we try to create PSQL function with the same signature.

  *** FOR FB 4.X AND ABOVE  ***
  Added separate code for running on FB 4.0.x: use udf_compat!UC_frac from UDR engine, and then alter it
  by changing to PSQL with the same signature.

  UDF usage is deprecated in FB 4+, see: ".../doc/README.incompatibilities.3to4.txt".
  Functions div, frac, dow, sdow, getExactTimestampUTC and isLeapYear got safe replacement
  in UDR library "udf_compat", see it in folder: ../plugins/udr/
JIRA:        CORE-5776
FBTEST:      bugs.core_5776
"""

import pytest
from firebird.qa import *

# version: 3.0.4

db = db_factory()

test_script_1 = """
    set list on;
    set term ^;
    execute block as
    begin
        execute statement 'drop function sright';
        when any do begin end
    end
    ^
    set term ;^
    commit;

    declare external function sright
      varchar(100) by descriptor,
      smallint,
      varchar(100) by descriptor returns parameter 3
      entry_point 'right' module_name 'fbudf';
    commit;


    select rdb$return_argument from rdb$functions
      where rdb$function_name = 'SRIGHT';

    select rdb$argument_position, rdb$argument_name from rdb$function_arguments
     where rdb$function_name = 'SRIGHT';

    ------------------------------------------------

    commit;

    set term ^;
    alter function sright (str varchar(100), len int)  returns varchar(100)
    as
    begin
      return right(str, len);
    end^
    set term ;^
    commit;

    select sright('function', 2) from rdb$database;


    select rdb$return_argument from rdb$functions
      where rdb$function_name = 'SRIGHT';

    select rdb$argument_position, rdb$argument_name from rdb$function_arguments
     where rdb$function_name = 'SRIGHT';
"""

act_1 = isql_act('db', test_script_1)

expected_stdout_1 = """
    RDB$RETURN_ARGUMENT             3

    RDB$ARGUMENT_POSITION           1
    RDB$ARGUMENT_NAME               <null>

    RDB$ARGUMENT_POSITION           2
    RDB$ARGUMENT_NAME               <null>

    RDB$ARGUMENT_POSITION           3
    RDB$ARGUMENT_NAME               <null>

    SRIGHT                          on

    RDB$RETURN_ARGUMENT             0

    RDB$ARGUMENT_POSITION           0
    RDB$ARGUMENT_NAME               <null>

    RDB$ARGUMENT_POSITION           1
    RDB$ARGUMENT_NAME               STR

    RDB$ARGUMENT_POSITION           2
    RDB$ARGUMENT_NAME               LEN
"""

@pytest.mark.version('>=3.0.4,<4.0')
def test_1(act_1: Action):
    act_1.expected_stdout = expected_stdout_1
    act_1.execute()
    assert act_1.clean_stdout == act_1.clean_expected_stdout

# version: 4.0

test_script_2 = """
    set list on;

    set term ^;
    create or alter function the_frac( val double precision ) returns double precision as
    begin
        return 1. / log10( val - cast(val as int) )  ;
    end
    ^
    set term ;^
    commit;

    select the_frac( pi() ) as the_frac_0 from rdb$database;
    commit;

    alter function the_frac(
        val double precision
    ) returns double precision
        external name 'udf_compat!UC_frac'
        engine udr;
    commit;

    select the_frac( -pi() ) as the_frac_1 from rdb$database;
    commit;

    -----------------------------

    set term ^;
    alter function the_frac( val double precision ) returns double precision as
    begin
        return 1. / exp( -(val - cast(val as int)) )  ;
    end
    ^
    set term ;^
    commit;

    select the_frac( -pi() ) as the_frac_2 from rdb$database;
    commit;

"""

act_2 = isql_act('db', test_script_2)

expected_stdout_2 = """
    THE_FRAC_0                      -1.177912798268244
    THE_FRAC_1                      -0.1415926535897931
    THE_FRAC_2                      0.8679747508826116
"""

@pytest.mark.version('>=4.0')
def test_2(act_2: Action):
    act_2.expected_stdout = expected_stdout_2
    act_2.execute()
    assert act_2.clean_stdout == act_2.clean_expected_stdout
