#coding:utf-8

"""
ID:          decfloat.round-modes
ISSUE:       5803
JIRA:        CORE-5535
TITLE:       Check validity of different ROUNDING modes that are defined for DECFLOAT datatype
DESCRIPTION:
  See  doc/sql.extensions/README.data_types

  Sample with results of diff. rounding modes: ibm.com/developerworks/ru/library/dm-0801chainani/
  Sample for round(1608.90*5/100, 2):  sql.ru/forum/actualutils.aspx?action=gotomsg&tid=729836&msg=8243077
FBTEST:      functional.datatypes.decfloat_round_modes
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    /******
    round-mode	12.341	12.345	12.349 	12.355 	12.405 	-12.345
    ---------------------------------------------------------------
    CEILING 	12.35 	12.35 	12.35 	12.36 	12.41 	-12.34
    UP        	12.35 	12.35 	12.35 	12.36 	12.41 	-12.35
    HALF_UP 	12.34 	12.35 	12.35 	12.36 	12.41 	-12.35
    HALF_EVEN 	12.34 	12.34 	12.35 	12.36 	12.40 	-12.34
    HALF_DOWN	12.34 	12.34 	12.35 	12.35 	12.40 	-12.34
    DOWN     	12.34 	12.34 	12.34	12.35 	12.40 	-12.34
    FLOOR    	12.34 	12.34 	12.34 	12.35 	12.40 	-12.35
    REROUND   	12.34	12.34 	12.34 	12.36 	12.41 	-12.34
    *******/

    recreate view v_test2 as select 1 id from rdb$database;
    commit;

    recreate table test2(
         v1 decfloat(16)
        ,v2 decfloat(16)
        ,v3 decfloat(16)
        ,v4 decfloat(16)
        ,v5 decfloat(16)
        ,v6 decfloat(16)
        ,vc decfloat(16)
        ,vp decfloat(16)
        ,vd decfloat(16)
        ,vx computed by (vc * vp / vd)
        ,vy computed by (vc * vp / vd)
    )
    ;
    commit;

    insert into test2( v1,     v2,     v3,     v4,     v5,      v6,        vc,   vp,     vd)
                values(12.341, 12.345, 12.349, 12.355, 12.405, -12.345,  1608.90, 5.00, 100.00);
    commit;

    recreate view v_test2 as
    select
        round(v1, 2) r1, round(v2, 2) r2, round(v3, 2) r3,
        round(v4, 2) r4, round(v5, 2) r5, round(v6, 2) r6,
        round( vx, 2) as rx,
        round( -vy, 2) as ry
    from test2;
    commit;

    set decfloat round ceiling;
    select 'ceiling' as round_mode, v.* from v_test2 v; --   +80.45; -80.44

    set decfloat round up;
    select 'up' as round_mode, v.* from v_test2 v; --   +80.45; -80.45

    set decfloat round half_up;
    select 'half_up' as round_mode, v.* from v_test2 v; --   +80.45; -80.45

    set decfloat round half_even;
    select 'half_even' as round_mode, v.* from v_test2 v; --   +80.44; -80.44

    set decfloat round half_down;
    select 'half_down' as round_mode, v.* from v_test2 v; --   +80.44; -80.44

    set decfloat round down;
    select 'down' as round_mode, v.* from v_test2 v; --   +80.44; -80.44

    set decfloat round floor;
    select 'floor' as round_mode, v.* from v_test2 v;--   +80.44; -80.45

    set decfloat round reround;
    select 'reround' as round_mode, v.* from v_test2 v; --   +80.44; -80.44
"""

act = isql_act('db', test_script, substitutions=[('[ ]+', ' ')])

expected_stdout = """
    ROUND_MODE ceiling
    R1 12.35
    R2 12.35
    R3 12.35
    R4 12.36
    R5 12.41
    R6 -12.34
    RX 80.45
    RY -80.44
    ROUND_MODE up
    R1 12.35
    R2 12.35
    R3 12.35
    R4 12.36
    R5 12.41
    R6 -12.35
    RX 80.45
    RY -80.45
    ROUND_MODE half_up
    R1 12.34
    R2 12.35
    R3 12.35
    R4 12.36
    R5 12.41
    R6 -12.35
    RX 80.45
    RY -80.45
    ROUND_MODE half_even
    R1 12.34
    R2 12.34
    R3 12.35
    R4 12.36
    R5 12.40
    R6 -12.34
    RX 80.44
    RY -80.44
    ROUND_MODE half_down
    R1 12.34
    R2 12.34
    R3 12.35
    R4 12.35
    R5 12.40
    R6 -12.34
    RX 80.44
    RY -80.44
    ROUND_MODE down
    R1 12.34
    R2 12.34
    R3 12.34
    R4 12.35
    R5 12.40
    R6 -12.34
    RX 80.44
    RY -80.44
    ROUND_MODE floor
    R1 12.34
    R2 12.34
    R3 12.34
    R4 12.35
    R5 12.40
    R6 -12.35
    RX 80.44
    RY -80.45
    ROUND_MODE reround
    R1 12.34
    R2 12.34
    R3 12.34
    R4 12.36
    R5 12.41
    R6 -12.34
    RX 80.44
    RY -80.44
"""

@pytest.mark.version('>=4.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
