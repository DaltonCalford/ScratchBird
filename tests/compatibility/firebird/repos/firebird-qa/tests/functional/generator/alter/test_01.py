#coding:utf-8

"""
ID:          generator.alter-01
FBTEST:      functional.generator.alter.01
TITLE:       ALTER SEQUENCE
DESCRIPTION:
  Create sequence and try several cases of ALTER SEQUENCE statement.
  Then check result that is stored in RDB$GENERATORS table and gen_id(<seq>, 0) value.
  NB:  we have to issue 'COMMIT' after each ALTER SEQUENCE statement in order to see new values in RDB.
NOTES:
[07.08.2020]
  we have to separate test for 3.0 and 4.0 because INITIAL value of new sequence
  in FB 4.x now differs from "old good zero" (this is so since CORE-6084 was fixed).
[13.08.2020]
  changed code for FB 4.x after introduction of fix for CORE-6386: value that was initially
  written into RDB$GENERATORD.RDB$INITIAL_VALUE column must NOT changed on any kind of ALTER EQUENCE
  statement, even when it contains 'RESTART WITH' clause. Checked on 4.0.0.2151.

  See also: doc/README.incompatibilities.3to4.txt
"""

import pytest
from firebird.qa import *

substitutions = [('===.*', ''), ('[ \t]+', ' ')]

db = db_factory()

# version: 3.0

test_script_1 = """
    recreate generator g;
    recreate view v_info as select rdb$initial_value as rdb_init, rdb$generator_increment as rdb_incr, gen_id(g,0) as gen_id_0 from rdb$generators where rdb$generator_name=upper('g');
    commit;
    select 'point-00' as msg, v.* from v_info v;

    set heading off;

    alter sequence g increment by -1; commit;
    select 'point-01' as msg, v.* from v_info v;

    alter sequence g increment by 1; commit;
    select 'point-02' as msg, v.* from v_info v;

    alter sequence g restart with 0; commit;
    select 'point-03' as msg, v.* from v_info v;

    alter sequence g restart with -1; commit;
    select 'point-04' as msg, v.* from v_info v;

    alter sequence g restart with -1 increment by -1; commit;
    select 'point-05' as msg, v.* from v_info v;

    alter sequence g restart with -1 increment by 1; commit;
    select 'point-06' as msg, v.* from v_info v;

    alter sequence g restart with 1 increment by -1; commit;
    select 'point-07' as msg, v.* from v_info v;

    alter sequence g restart with 1 increment by 1; commit;
    select 'point-08' as msg, v.* from v_info v;
    commit;

"""

act_1 = isql_act('db', test_script_1, substitutions=substitutions)

expected_stdout_1 = """
    MSG                   RDB_INIT     RDB_INCR              GEN_ID_0
    ======== ===================== ============ =====================
    point-00                     0            1                     0
    point-01                     0           -1                     0
    point-02                     0            1                     0
    point-03                     0            1                     0
    point-04                    -1            1                    -1
    point-05                    -1           -1                    -1
    point-06                    -1            1                    -1
    point-07                     1           -1                     1
    point-08                     1            1                     1
"""

@pytest.mark.version('>=3.0,<4.0')
def test_1(act_1: Action):
    act_1.expected_stdout = expected_stdout_1
    act_1.execute()
    assert act_1.clean_stdout == act_1.clean_expected_stdout

# version: 4.0

test_script_2 = """
    recreate generator g start with 7654321;
    set term ^;
    create procedure sp_gen_info returns( rdb_init bigint, rdb_incr bigint, gen_id_curr bigint, gen_id_next bigint) as
    begin
        select rdb$initial_value , rdb$generator_increment
        from rdb$generators
        where rdb$generator_name=upper('g')
        into rdb_init, rdb_incr;

        execute statement 'select gen_id(g,0) from rdb$database' into gen_id_curr;
        execute statement 'select next value for g from rdb$database' into gen_id_next;
        suspend;
    end^
    set term ;^
    commit;

    select 'point-00' as msg, p.* from sp_gen_info p;
    --set echo on;
    set heading off;

    -----------------------------------------------------------------------

    -- Test when only INCREMENT BY clause presents:

    recreate generator g start with 7654321; commit; alter sequence g increment by -23456789; commit;
    select 'point-01' as msg, p.* from sp_gen_info p;
    -----------------------------------------------------------------------

    recreate generator g start with 7654321; commit; alter sequence g increment by 23456789; commit;
    select 'point-02' as msg, p.* from sp_gen_info p;
    -----------------------------------------------------------------------


    -- Test when only RESTART clause presents:

    recreate generator g start with 7654321; commit; alter sequence g restart with -1234567; commit;
    select 'point-03' as msg, p.* from sp_gen_info p;

    -----------------------------------------------------------------------
    recreate generator g start with 7654321; commit; alter sequence g restart with  1234567; commit;
    select 'point-04' as msg, p.* from sp_gen_info p;

    -----------------------------------------------------------------------

    -- Test when both RESTART and INCREMENT BY clauses present:

    recreate generator g start with 7654321; commit; alter sequence g restart with -1234567 increment by -23456789; commit;
    select 'point-05' as msg, p.* from sp_gen_info p;

    -----------------------------------------------------------------------
    recreate generator g start with 7654321; commit; alter sequence g restart with -1234567 increment by 23456789; commit;
    select 'point-06' as msg, p.* from sp_gen_info p;

    -----------------------------------------------------------------------
    recreate generator g start with 7654321; commit; alter sequence g restart with 1234567 increment by -23456789; commit;
    select 'point-07' as msg, p.* from sp_gen_info p;

    -----------------------------------------------------------------------
    recreate generator g start with 7654321; commit; alter sequence g restart with 1234567 increment by 23456789; commit;
    select 'point-08' as msg, p.* from sp_gen_info p;
"""

act_2 = isql_act('db', test_script_2, substitutions=substitutions)

expected_stdout_2 = """
    MSG                   RDB_INIT              RDB_INCR           GEN_ID_CURR           GEN_ID_NEXT
    ======== ===================== ===================== ===================== =====================
    point-00               7654321                     1               7654320               7654321
    point-01               7654321             -23456789               7654320             -15802469
    point-02               7654321              23456789               7654320              31111109
    point-03               7654321                     1              -1234568              -1234567
    point-04               7654321                     1               1234566               1234567
    point-05               7654321             -23456789              22222222              -1234567
    point-06               7654321              23456789             -24691356              -1234567
    point-07               7654321             -23456789              24691356               1234567
    point-08               7654321              23456789             -22222222               1234567
"""

@pytest.mark.version('>=4.0')
def test_2(act_2: Action):
    act_2.expected_stdout = expected_stdout_2
    act_2.execute()
    assert act_2.clean_stdout == act_2.clean_expected_stdout
