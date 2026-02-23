#coding:utf-8

"""
ID:          issue-3812
ISSUE:       3812
TITLE:       Query with SP doesn't accept explicit plan [CORE3451]
NOTES:
    [18.02.2023] pzotov
        Confirmed problem on 5.0.0.743, got:
            Statement failed, SQLSTATE = 42S02
            -Invalid command
            -there is no alias or table named TMP_SP1 at this scope level
        Checked on 5.0.0.745 - all OK.
    [04.07.2025] pzotov
        Separated expected output for FB major versions prior/since 6.x.
        No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.
        Checked on 6.0.0.876; 5.0.3.1668.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set bail on;
    recreate table tmp_tbl1(
       fld1 int
       ,fld2 int
       ,fld3 int
    );
    create index tmp_tbl1_fld2 on tmp_tbl1(fld2);

    recreate table tmp_tbl2(
       fld1 int
       ,fld2 int
       ,fld3 int
    );
    create index tmp_tbl2_fld1 on tmp_tbl2(fld1);


    recreate table tmp_tbl3(
       fld1 int
       ,fld2 int
       ,fld3 int
    );
    create index tmp_tbl3_fld1 on tmp_tbl3(fld1);

    set term ^;
    create or alter procedure tmp_sp1 returns(fld1 int) as
    begin
        fld1 = 2;
        suspend;
    end
    ^
    set term ;^
    commit;

    set plan on;
    select t2.fld1
    from tmp_tbl2 t2
    join tmp_tbl1 t1 on t1.fld1=t2.fld1
    join tmp_sp1 on tmp_sp1.fld1=t1.fld2
    join tmp_tbl3 t3 on t3.fld1=t1.fld3
    where t2.fld2=2
    PLAN JOIN (JOIN (TMP_SP1 NATURAL, T1 INDEX (TMP_TBL1_FLD2)), T2 INDEX (TMP_TBL2_FLD1), T3 INDEX (TMP_TBL3_FLD1))
    ;
"""

act = isql_act('db', test_script)

expected_stdout_5x = """
    PLAN JOIN (JOIN (TMP_SP1 NATURAL, T1 INDEX (TMP_TBL1_FLD2)), T2 INDEX (TMP_TBL2_FLD1), T3 INDEX (TMP_TBL3_FLD1))
"""

expected_stdout_6x = """
    PLAN JOIN (JOIN ("PUBLIC"."TMP_SP1" NATURAL, "T1" INDEX ("PUBLIC"."TMP_TBL1_FLD2")), "T2" INDEX ("PUBLIC"."TMP_TBL2_FLD1"), "T3" INDEX ("PUBLIC"."TMP_TBL3_FLD1"))
"""

@pytest.mark.version('>=5.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
