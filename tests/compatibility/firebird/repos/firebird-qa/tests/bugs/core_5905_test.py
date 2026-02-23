#coding:utf-8

"""
ID:          issue-6163
ISSUE:       6163
TITLE:       Inconsistencies with PSQL FUNCTION vs UDF
DESCRIPTION:
  *** FOR FB 3.X ONLY ***
  Test uses UDF 'strlen' declared in ib_udf.sql script which for sure present in every FB snapshot.
  After this, we try to create PSQL function with the same signature.

  *** FOR FB 4.X AND ABOVE  ***
  Added separate code for running on FB 4.0.x.
  UDF usage is deprecated in FB 4+, see: ".../doc/README.incompatibilities.3to4.txt".
  Functions div, frac, dow, sdow, getExactTimestampUTC and isLeapYear got safe replacement
  in UDR library "udf_compat", see it in folder: ../plugins/udr/

  Confirmed inconsistence output on: 3.0.4.32972, 4.0.0.875 and 4.0.0.1172
  (4.x - output phrase "UDF THE_FRAC" instead of "Function THE_FRAC" on attempt to drop function).
JIRA:        CORE-5905
FBTEST:      bugs.core_5905
NOTES:
    [02.07.2025] pzotov
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.
    Checked on 6.0.0.889; 5.0.3.1668; 4.0.6.3214.
"""

import pytest
from firebird.qa import *

db = db_factory()

# version: 3.0

test_script_1 = """
    set list on;
    create view v_check as
    select
        rf.rdb$function_name as func_name
       ,rf.rdb$legacy_flag as legacy_flag
    from rdb$functions rf where rf.rdb$function_name = upper('substrlen')
    ;
    commit;


    declare external function substrlen
    	cstring(255), smallint, smallint
    	returns cstring(255) free_it
    	entry_point 'IB_UDF_substrlen' module_name 'ib_udf'
    ;
    commit;

    set term ^;
    create procedure sp_main(input_str varchar(255), i smallint, n smallint) as
        declare s varchar(255);
    begin
        s = substrlen( input_str, i, n );
    end
    ^
    set term ;^
    commit;

    select * from v_check;
    commit;

    drop function substrlen;
    commit;

    -----------------------------

    set term ^;
    alter function substrlen(input_str varchar(255), i smallint, n smallint) returns varchar(255) as
    begin
        rdb$set_context('USER_SESSION', 'WAS_PSQL_INVOKED', 'Yes');
        return substring(input_str from i for n);
    end
    ^
    set term ^;
    commit;

    select * from v_check;
    commit;

    drop function substrlen;
    commit;
"""

act_1 = isql_act('db', test_script_1)

expected_stdout_1 = """
    FUNC_NAME                       SUBSTRLEN
    LEGACY_FLAG                     1

    FUNC_NAME                       SUBSTRLEN
    LEGACY_FLAG                     0
"""

expected_stderr_1 = """
    Statement failed, SQLSTATE = 38000
    unsuccessful metadata update
    -cannot delete
    -Function SUBSTRLEN
    -there are 1 dependencies

    Statement failed, SQLSTATE = 38000
    unsuccessful metadata update
    -cannot delete
    -Function SUBSTRLEN
    -there are 1 dependencies
"""

@pytest.mark.version('>=3.0.4,<4.0')
def test_1(act_1: Action):
    act_1.expected_stdout = expected_stdout_1
    act_1.expected_stderr = expected_stderr_1
    act_1.execute()
    assert (act_1.clean_stderr == act_1.clean_expected_stderr and
            act_1.clean_stdout == act_1.clean_expected_stdout)

##########################################################################
# version: 4.0

test_script_2 = """
    set list on;
    create view v_check as
    select
        rf.rdb$function_name as func_name
       ,rf.rdb$legacy_flag as legacy_flag
    from rdb$functions rf
    where rf.rdb$function_name = upper('the_frac')
    ;
    commit;

    -- See declaration sample in plugins\\udr\\UdfBackwardCompatibility.sql:

    create function the_frac(
        val double precision
    ) returns double precision
        external name 'udf_compat!UC_frac'
        engine udr;
    commit;

    select the_frac( -pi() ) as the_frac_1 from rdb$database;
    commit;

    set term ^;
    create procedure sp_main as
        declare r double precision;
    begin
        r = the_frac( pi() );
    end
    ^
    set term ;^
    commit;

    select * from v_check;
    commit;

    drop function the_frac; -- should FAIL because dependent procedure SP_MAIN does exist.
    commit;

    -----------------------------

    set term ^;
    alter function the_frac( val double precision ) returns double precision as
    begin
        return 1. / abs(val - cast(val as int) )  ;
    end
    ^
    set term ;^
    commit;

    select * from v_check;
    commit;

    select the_frac( -pi() ) as the_frac_2 from rdb$database;
    commit;

    alter function the_frac(
        val double precision
    ) returns double precision
        external name 'udf_compat!UC_frac'
        engine udr;
    commit;

    select the_frac( -pi() )  as the_frac_3 from rdb$database;
    commit;

    drop function the_frac; -- should again FAIL because procedure SP_MAIN still depends on it.
    commit;
"""

substitutions = [('[ \t]+', ' ')]
act_2 = isql_act('db', test_script_2, substitutions = substitutions)

expected_stdout_5x = """
    THE_FRAC_1                      -0.1415926535897931
    FUNC_NAME                       THE_FRAC
    LEGACY_FLAG                     0

    Statement failed, SQLSTATE = 38000
    unsuccessful metadata update
    -cannot delete
    -Function THE_FRAC
    -there are 1 dependencies

    FUNC_NAME                       THE_FRAC
    LEGACY_FLAG                     0
    THE_FRAC_2                      7.062513305931052
    THE_FRAC_3                      -0.1415926535897931

    Statement failed, SQLSTATE = 38000
    unsuccessful metadata update
    -cannot delete
    -Function THE_FRAC
    -there are 1 dependencies
"""

expected_stdout_6x = """
    THE_FRAC_1                      -0.1415926535897931
    FUNC_NAME                       THE_FRAC
    LEGACY_FLAG                     0

    Statement failed, SQLSTATE = 38000
    unsuccessful metadata update
    -cannot delete
    -Function "PUBLIC"."THE_FRAC"
    -there are 1 dependencies

    FUNC_NAME                       THE_FRAC
    LEGACY_FLAG                     0
    THE_FRAC_2                      7.062513305931052
    THE_FRAC_3                      -0.1415926535897931

    Statement failed, SQLSTATE = 38000
    unsuccessful metadata update
    -cannot delete
    -Function "PUBLIC"."THE_FRAC"
    -there are 1 dependencies
"""

@pytest.mark.version('>=4.0')
def test_2(act_2: Action):
    act_2.expected_stdout = expected_stdout_5x if act_2.is_version('<6') else expected_stdout_6x
    act_2.execute(combine_output = True)
    assert act_2.clean_stdout == act_2.clean_expected_stdout
