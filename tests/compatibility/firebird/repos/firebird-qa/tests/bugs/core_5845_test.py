#coding:utf-8

"""
ID:          issue-6106
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/6106
TITLE:       ORDER BY on index can cause suboptimal index choices
DESCRIPTION:
JIRA:        CORE-5845
FBTEST:      bugs.core_5845
NOTES:
    [12.09.2023] pzotov
    1. Added several queries provided by dimitr.
    2. Plan for query "select ... where t.id1 = 1 and t.x > 0 and t.id2 = 0" - has been changed in intermediate snapshot
       5.0.0.1204 (timestamp: 20230912 08:00). One need to split expected results for FB 5.x+ and older versions.
       See:
           https://github.com/FirebirdSQL/firebird/commit/022f09287747dd05753bd11acd3b3fe4b0756f6e
           https://github.com/FirebirdSQL/firebird/compare/252c5b2b2f88...784f7bd8a6f5

    [25.06.2025] pzotov
    1. Re-implemented: use same Python dictionary which stores QUERIES as its KEYS and execution plans as values,
       depending on major FB version.
    2. ::: ACHTUNG :::
       We have to call <prepard_sttm_instance>.free() in order to prevent from pytest hanging after all tests completed.
       Workaround was provided by Vlad, letter 25.06.2025 13:36.
       See also explaination by Vlad: 26.10.24 17:42 ("oddities when use instances of selective statements").

    3. Separated expected output for FB major versions prior/since 6.x.
       No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.

    Checked on 6.0.0.863; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

P_KEY_IDX = "test_pk_id1_id2_id3".upper()
SINGLE_X_IDX = "test_single".upper()
COMPOUND_IDX = "test_compound".upper()

init_sql = f"""
    recreate table test
    (
        id1 int,
        id2 int,
        id3 int,
        x numeric(18,2),
        constraint pk_test primary key(id1, id2, id3) using index {P_KEY_IDX}
    );
    create index {SINGLE_X_IDX} on test(x);
    create index {COMPOUND_IDX} on test(id1, x);
    commit;
"""

chk_qry_map = {
     "select * from test ta where ta.id1=1 and ta.x>0" :
     (
         f"PLAN (TA INDEX ({COMPOUND_IDX}))",                  # 3.x ... 4.x
         f"PLAN (TA INDEX ({COMPOUND_IDX}))",                  # 5.x
         f'PLAN ("TA" INDEX ("PUBLIC"."{COMPOUND_IDX}"))',     # 6.x+
     )
    ,"select * from test tb where tb.id1=1 and tb.x>0 order by tb.id1, tb.id2, tb.id3" :
     (
         f"PLAN SORT (TB INDEX ({COMPOUND_IDX}))",
         f"PLAN SORT (TB INDEX ({COMPOUND_IDX}))",
         f'PLAN SORT ("TB" INDEX ("PUBLIC"."{COMPOUND_IDX}"))',
     )
    ,"select * from test tc where tc.id1=1 and tc.x>0 order by tc.id1+0, tc.id2, tc.id3" :
     (
         f"PLAN SORT (TC INDEX ({COMPOUND_IDX}))",
         f"PLAN SORT (TC INDEX ({COMPOUND_IDX}))",
         f'PLAN SORT ("TC" INDEX ("PUBLIC"."{COMPOUND_IDX}"))',
     )

    # following examples were provided by dimitr, 12-sep-2023:
    ##########################################################
    # ::: NB ::: Since 5.0.0.1204 (intermediate snapshot with timestamp 20230912 08:00) plan for following query
    # "select ... where t.id1 = 1 and t.x > 0 and t.id2 = 0" -- has been changed to: PLAN (T INDEX (TEST_PK_ID1_ID2_ID3))
    ##########################################################
    ,"select * from test td where td.id1 = 1 and td.x > 0 and td.id2 = 0" :
     (
         f"PLAN (TD INDEX ({COMPOUND_IDX}))",
         f"PLAN (TD INDEX ({P_KEY_IDX}))",
         f'PLAN ("TD" INDEX ("PUBLIC"."{P_KEY_IDX}"))',
     )
    ,"select * from test te where te.id1 = 1 and te.x = 0" :
     (
         f"PLAN (TE INDEX ({COMPOUND_IDX}))",
         f"PLAN (TE INDEX ({COMPOUND_IDX}))",
         f'PLAN ("TE" INDEX ("PUBLIC"."{COMPOUND_IDX}"))',
     )
    ,"select * from test tf where tf.id1 = 1 and tf.id2 = 2" :
     (
         f"PLAN (TF INDEX ({P_KEY_IDX}))",
         f"PLAN (TF INDEX ({P_KEY_IDX}))",
         f'PLAN ("TF" INDEX ("PUBLIC"."{P_KEY_IDX}"))',
     )
    ,"select * from test tg where tg.id1 = 1 and tg.id2 = 2 and tg.id3 = 3" :
     (
         f"PLAN (TG INDEX ({P_KEY_IDX}))",
         f"PLAN (TG INDEX ({P_KEY_IDX}))",
         f'PLAN ("TG" INDEX ("PUBLIC"."{P_KEY_IDX}"))',
     )
    ,"select * from test th where th.id1 = 1 and th.x = 0 and th.id2 = 0" :
     (
         f"PLAN (TH INDEX ({COMPOUND_IDX}))",
         f"PLAN (TH INDEX ({COMPOUND_IDX}))",
         f'PLAN ("TH" INDEX ("PUBLIC"."{COMPOUND_IDX}"))',
     )
}

db = db_factory(init = init_sql)

act = python_act('db')

@pytest.mark.version('>=3.0.4')
def test_1(act: Action, capsys):

    expected_plans_lst = []
    with act.db.connect() as con:
        cur = con.cursor()
        #for q in chk_qry_map.keys():
        for q, v in chk_qry_map.items():
            cur.execute(q)
            ps = cur.prepare(q)
            print(q)
            print( ps.plan )
            
            ps.free() # ::: achtung ::: 25.06.2024, need to prevent from pytest hanging after all tests completed.

            expected_qry_plan = v[0] if act.is_version('<5') else v[1] if act.is_version('<6') else v[2]
            expected_plans_lst.append( q )
            expected_plans_lst.append( expected_qry_plan + '\n' )
    
    act.expected_stdout = '\n'.join(expected_plans_lst)
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
