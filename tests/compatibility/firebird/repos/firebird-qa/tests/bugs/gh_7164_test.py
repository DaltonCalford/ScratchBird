#coding:utf-8

"""
ID:          issue-7164
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/7164
TITLE:       Multi-way hash/merge joins are impossible for expression-based keys
DESCRIPTION:
NOTES:
    [28.02.2023] pzotov
        ::: NB :::
        Currently improvement relates only to the case when data sources have no appropriate indices.
        Otherwise "old" way is used: server attempts to make nested loopss, but finally it checks
        whether hash join will be cheaper. And, if yes, then it applies hash join, but it is applied
        to each joined stream, so execution plan will look as "nested" (multi-way) hashes.
        Thanks to dimitr for explanation (letter 28.02.2023 10:52).
        Checked on 5.0.0.961 - all OK.
    [04.07.2025] pzotov
        Separated expected output for FB major versions prior/since 6.x.
        No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.
        Checked on 6.0.0.863; 5.0.3.1668.
"""

import pytest
from firebird.qa import *

db = db_factory()
act = python_act('db')

@pytest.mark.version('>=5.0')
def test_1(act: Action):
    test_sql = """
        create table test_a (id int, f01 computed by(id+1));
        create view v_test as select a.id as id_a , b.id as id_b, c.id as id_c
        from test_a a, test_a b, test_a c
        where a.id+0 = b.id+0 and b.id+0 = c.id+0;
        commit;

        set plan;

        select *
        from test_a t1, test_a t2, test_a t3
        where t1.id+0 = t2.id+0 and t2.id+0 = t3.id+0;

        select *
        from test_a a, test_a b, test_a c
        where a.f01+0 = b.f01+0 and b.f01+0 = c.f01+0;

        select *
        from v_test v1, v_test v2, v_test v3
        where v1.id_a = v2.id_b and v2.id_b = v3.id_c
        ;

        select *
        from v_test u1, v_test u2, v_test u3
        where u1.id_a*1 = u2.id_b*1 and u2.id_b*1 = u3.id_c*1
        ;
        
    """

    expected_stdout_5x = """
        PLAN HASH (T1 NATURAL, T2 NATURAL, T3 NATURAL)
        PLAN HASH (A NATURAL, B NATURAL, C NATURAL)
        PLAN HASH (HASH (V1 A NATURAL, V1 B NATURAL, V1 C NATURAL), HASH (V2 A NATURAL, V2 B NATURAL, V2 C NATURAL), HASH (V3 A NATURAL, V3 B NATURAL, V3 C NATURAL))
        PLAN HASH (HASH (U1 A NATURAL, U1 B NATURAL, U1 C NATURAL), HASH (U2 A NATURAL, U2 B NATURAL, U2 C NATURAL), HASH (U3 A NATURAL, U3 B NATURAL, U3 C NATURAL))
    """

    expected_stdout_6x = """
        PLAN HASH ("T1" NATURAL, "T2" NATURAL, "T3" NATURAL)
        PLAN HASH ("A" NATURAL, "B" NATURAL, "C" NATURAL)
        PLAN HASH (HASH ("V1" "A" NATURAL, "V1" "B" NATURAL, "V1" "C" NATURAL), HASH ("V2" "A" NATURAL, "V2" "B" NATURAL, "V2" "C" NATURAL), HASH ("V3" "A" NATURAL, "V3" "B" NATURAL, "V3" "C" NATURAL))
        PLAN HASH (HASH ("U1" "A" NATURAL, "U1" "B" NATURAL, "U1" "C" NATURAL), HASH ("U2" "A" NATURAL, "U2" "B" NATURAL, "U2" "C" NATURAL), HASH ("U3" "A" NATURAL, "U3" "B" NATURAL, "U3" "C" NATURAL))
    """

    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.isql(switches=['-q'], input = test_sql, combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
