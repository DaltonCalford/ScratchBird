#coding:utf-8

"""
ID:          issue-8084
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/8084
TITLE:       Partial index uniqueness violation (changes in columns participating in index filtering expression are not properly tracked).
DESCRIPTION:
NOTES:
    [19.04.2024] pzotov
        Reduced min_version to 5.0.1 after backporting (commit #0e9ef69).
        Confirmed bug on 6.0.0.315; confirmed problem noted as second case (see ticket) in 6.0.0.321 #1d96c10.
        Checked on 6.0.0.325 #f5930a5, 5.0.1.1383 #0e9ef69 (intermediate snapshot) - all OK.
    [06.07.2025] pzotov
        Added 'SQL_SCHEMA_PREFIX' to be substituted in expected_* on FB 6.x
        Checked on 6.0.0.914; 5.0.3.1668.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;

    -- https://github.com/FirebirdSQL/firebird/issues/8084#issue-2247604539
    recreate table test1 (
      t1_id bigint primary key
      ,t1_a bigint not null
      ,t1_b smallint not null
    );

    create unique index test1_idx_a on test1(t1_a) where (t1_b = 1);

    insert into test1(t1_id, t1_a, t1_b) values (1, 1, 0);
    insert into test1(t1_id, t1_a, t1_b) values (2, 2, 1);
    commit;

    insert into test1(t1_id, t1_a, t1_b) values (3, 1, 0); -- must pass
    commit;
    insert into test1(t1_id, t1_a, t1_b) values (4, 2, 1); -- must fail with "attempt to store duplicate value"
    rollback;

    update test1 set t1_b = 1 where t1_id = 1; -- must pass
    commit;

    update test1 set t1_b = 1 where t1_id = 3; -- BUG was here: passed before fix but must fail.
    commit;

    select t1_a+0 as t1_a, count(*) as t1_a_cnt from test1 where t1_b+0 = 1 group by t1_a+0;
    rollback;

    -------------------------------------------------------------------------

    -- https://github.com/FirebirdSQL/firebird/issues/8084#issuecomment-2063121843
    recreate table test2 (
      t2_id bigint not null,
      t2_a bigint not null,
      t2_b smallint not null,
      constraint pk_test2 primary key(t2_id)
    );

    create unique index test2_idx_a on test2(t2_a) where (t2_b = 1);

    insert into test2(t2_id, t2_a, t2_b) values (1, 1, 0);
    insert into test2(t2_id, t2_a, t2_b) values (2, 2, 1);
    insert into test2(t2_id, t2_a, t2_b) values (3, 1, 0);
    commit;

    update test2 set t2_b=0;
    commit;

    insert into test2(t2_id, t2_a, t2_b) values (4, 2, 1); -- must pass

    select * from test2;

"""

act = isql_act('db', test_script, substitutions=[('[ \t]+', ' ')])

@pytest.mark.version('>=5.0.1')
def test_1(act: Action):

    SQL_SCHEMA_PREFIX = '' if act.is_version('<6') else  '"PUBLIC".'
    expected_stdout = f"""
        Statement failed, SQLSTATE = 23000
        attempt to store duplicate value (visible to active transactions) in unique index {SQL_SCHEMA_PREFIX}"TEST1_IDX_A"
        -Problematic key value is ("T1_A" = 2)
        Statement failed, SQLSTATE = 23000
        attempt to store duplicate value (visible to active transactions) in unique index {SQL_SCHEMA_PREFIX}"TEST1_IDX_A"
        -Problematic key value is ("T1_A" = 1)

        T1_A                            1
        T1_A_CNT                        1

        T1_A                            2
        T1_A_CNT                        1

        T2_ID                           1
        T2_A                            1
        T2_B                            0

        T2_ID                           2
        T2_A                            2
        T2_B                            0

        T2_ID                           3
        T2_A                            1
        T2_B                            0

        T2_ID                           4
        T2_A                            2
        T2_B                            1
    """

    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
