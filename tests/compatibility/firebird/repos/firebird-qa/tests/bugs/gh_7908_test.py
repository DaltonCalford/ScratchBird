#coding:utf-8

"""
ID:          issue-7908
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/7908
TITLE:       Unexpected results when the join condition contains the OR predicate
DESCRIPTION:
NOTES:
    [06.12.2023] pzotov
    Confirmed bug on 6.0.0.169, 5.0.0.1294
    Checked on 6.0.0.172, 5.0.0.1294 (intermediate builds, date: 06-dec-2023)
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    create table t1(bf boolean);
    create table t0(bf boolean, bf1 boolean);
    create unique index i0 on t0(bf1 , bf );
    commit;

    insert into t0 (bf) values (false);
    insert into t1 (bf) values (false);

    set count on;
    set list on;

    select 'case-1' as msg, t1.bf as t0_bf, t0.bf1 as t0_bf1, t1.bf as tf_bf from t1, t0 where true or (t1.bf <= false) = false
    UNION ALL
    select 'case-2' as msg, t1.bf as t0_bf, t0.bf1 as t0_bf1, t1.bf as tf_bf from t1, t0 where true or (t1.bf <= false) = t0.bf1;
"""

act = isql_act('db', test_script)

expected_stdout = """
    MSG                             case-1
    T0_BF                           <false>
    T0_BF1                          <null>
    TF_BF                           <false>

    MSG                             case-2
    T0_BF                           <false>
    T0_BF1                          <null>
    TF_BF                           <false>
    Records affected: 2
"""

@pytest.mark.version('>=5.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
