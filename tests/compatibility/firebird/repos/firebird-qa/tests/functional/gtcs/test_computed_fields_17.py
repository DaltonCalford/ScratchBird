#coding:utf-8

"""
ID:          computed-fields-17
FBTEST:      functional.gtcs.computed_fields_17
TITLE:       Computed fields
DESCRIPTION:
  Original test see in:
  https://github.com/FirebirdSQL/fbtcs/blob/master/GTCS/tests/CF_ISQL_17.script
  SQL script for creating test database ('gtcs_sp1.fbk') and fill it with some data:
  https://github.com/FirebirdSQL/fbtcs/blob/master/GTCS/tests/PROCS_QA_INIT_ISQL.script
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    create generator gen1;
    set generator gen1 to 999;

    create generator gen2;
    set generator gen2 to 199;

    create generator gen3;
    set generator gen3 to 29;

    create table t0 (
            a integer,
            genid_field1 computed by (gen_id(gen1, 1)),
            genid_field2 computed by (gen_id(gen2, genid_field1)),
            genid_field3 computed by (gen_id(gen3, genid_field2))
    );
    commit;

    insert into t0(a) values(4);
    insert into t0(a) values(1);

    /*************************************************
           first row:
           a:  4
           genid_field3:=genid3+(genid2+(genid1+1))
                       :=29+(199+(999+1)
                       :=29+(199+1000)
                       :=29+1199
                       :=1228
           second row:
           a: 1
           genid_field3:=genid3+(genid2+(genid1+1))
                       :=1228+(1199+(1000+1)
                       :=1228+(1199+1001)
                       :=1228+(2200)
                       :=3428

        so expected result is:

                   A          GENID_FIELD3
        ============ =====================

                   4                  1228
                   1                  3428

    *************************************************/

    select 'point-1' msg, p.a, p.genid_field3 from t0 p;

    /*************************************************
           first row:
           a:  4
           genid_field3:=genid3+(genid2+(genid1+1))
                       :=3428+(2200+(1001+1)
                       :=3428+(2200+1002)
                       :=3429+3202
                       :=6630
           genid_field2:=genid2+(genid1+1)
                       :=3202+(1002+1)
                       :=3202+1003
                       :=4205
           genid_field1:=genid1+1
                       :=1003+1
                       :=1004

           second row:
           a:  1
           genid_field3:=genid3+(genid2+(genid1+1))
                       :=6630+(4205+(1004+1))
                       :=6630+(4205+1005)
                       :=6630+5210
                       :=11840
           genid_field2:=genid2+(genid1+1)
                       :=5210+(1005+1)
                       :=5210+1006
                       :=6216
           genid_field1:=genid1+1
                       :=1006+1
                       :=1007

        so expected result is:

                   A          GENID_FIELD1          GENID_FIELD2          GENID_FIELD3
        ============ ===================== ===================== =====================

                   4                  1004                  4205                  6630
                   1                  1007                  6216                 11840

    **************************************************/

    select 'point-2' msg, p.* from t0 p;

"""

act = isql_act('db', test_script, substitutions=[('=', ''), ('[ \t]+', ' ')])

expected_stdout = """
    MSG                A          GENID_FIELD3
    point-1            4                  1228
    point-1            1                  3428

    MSG                A          GENID_FIELD1          GENID_FIELD2          GENID_FIELD3
    point-2            4                  1004                  4205                  6630
    point-2            1                  1007                  6216                 11840
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
