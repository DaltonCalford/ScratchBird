#coding:utf-8

"""
ID:          issue-4691
ISSUE:       4691
TITLE:       BUGCHECK(177) for MERGE with multiple matches
DESCRIPTION:
NOTES:
[07.06.2020]
  separate section for FB 4.x was added since fix for core-2274 issued:
  MERGE can not change the same record multiple times.
  For this reason we have to check only presense of ERROR in 4.x and that result is the same after merge and rollback.
JIRA:        CORE-4369
FBTEST:      bugs.core_4369
"""

import pytest
from firebird.qa import *

db = db_factory()

# version: 3.0

test_script_1 = """
    recreate sequence g;

    recreate table t1 (
        id int,
        val int
    );

    recreate table t2 (
        id int,
        val int,
        seq_inside_merge int
    );
    commit;

    insert into t1 (id, val) select row_number() over(), 1000 from rdb$types rows 17; -- '17' ==> 'N', see below formula for seq_inside_merge
    insert into t2(id, val, seq_inside_merge)  select id, val, 0 from t1;
    commit;

    alter sequence g restart with 0;
    commit;

    merge into t2 as t
    using t1 as s
    on 1 = 1
    when matched then update set t.val = t.val + s.val, t.seq_inside_merge = next value for g
    ;

    set list on;
    select * from t2 order by id;
    -- 1st value of `SEQ_INSIDE_MERGE` = N * (N-1) + 1
    -- All subsequent values are incremented by 1.
    -- Confirmed result in WI-T3.0.0.31374 Beta-1:
    -- "internal Firebird consistency check (applied differences will not fit in record (177), file: sqz.cpp line: 147)"
"""

act_1 = isql_act('db', test_script_1)

expected_stdout_1 = """
    ID                              1
    VAL                             2000
    SEQ_INSIDE_MERGE                273

    ID                              2
    VAL                             2000
    SEQ_INSIDE_MERGE                274

    ID                              3
    VAL                             2000
    SEQ_INSIDE_MERGE                275

    ID                              4
    VAL                             2000
    SEQ_INSIDE_MERGE                276

    ID                              5
    VAL                             2000
    SEQ_INSIDE_MERGE                277

    ID                              6
    VAL                             2000
    SEQ_INSIDE_MERGE                278

    ID                              7
    VAL                             2000
    SEQ_INSIDE_MERGE                279

    ID                              8
    VAL                             2000
    SEQ_INSIDE_MERGE                280

    ID                              9
    VAL                             2000
    SEQ_INSIDE_MERGE                281

    ID                              10
    VAL                             2000
    SEQ_INSIDE_MERGE                282

    ID                              11
    VAL                             2000
    SEQ_INSIDE_MERGE                283

    ID                              12
    VAL                             2000
    SEQ_INSIDE_MERGE                284

    ID                              13
    VAL                             2000
    SEQ_INSIDE_MERGE                285

    ID                              14
    VAL                             2000
    SEQ_INSIDE_MERGE                286

    ID                              15
    VAL                             2000
    SEQ_INSIDE_MERGE                287

    ID                              16
    VAL                             2000
    SEQ_INSIDE_MERGE                288

    ID                              17
    VAL                             2000
    SEQ_INSIDE_MERGE                289
"""

@pytest.mark.version('>=3.0,<4.0')
def test_1(act_1: Action):
    act_1.expected_stdout = expected_stdout_1
    act_1.execute()
    assert act_1.clean_stdout == act_1.clean_expected_stdout

# version: 4.0

test_script_2 = """
    recreate sequence g;

    recreate table t1 (
        id int,
        val int
    );

    recreate table t2 (
        id int,
        val int,
        seq_inside_merge int
    );
    commit;

    insert into t1 (id, val) select row_number() over(), 1000 from rdb$types rows 17; -- '17' ==> 'N', see below formula for seq_inside_merge
    insert into t2(id, val, seq_inside_merge)  select id, val, 0 from t1;
    commit;

    alter sequence g restart with 0;
    commit;

    merge into t2 as t
    using t1 as s
    on 1 = 1
    when matched then update set t.val = t.val + s.val, t.seq_inside_merge = next value for g
    ;

    -- all values should remain unchanged:
    set list on;
    select * from t2 order by id;
"""

act_2 = isql_act('db', test_script_2)

expected_stdout_2 = """
    ID                              1
    VAL                             1000
    SEQ_INSIDE_MERGE                0

    ID                              2
    VAL                             1000
    SEQ_INSIDE_MERGE                0

    ID                              3
    VAL                             1000
    SEQ_INSIDE_MERGE                0

    ID                              4
    VAL                             1000
    SEQ_INSIDE_MERGE                0

    ID                              5
    VAL                             1000
    SEQ_INSIDE_MERGE                0

    ID                              6
    VAL                             1000
    SEQ_INSIDE_MERGE                0

    ID                              7
    VAL                             1000
    SEQ_INSIDE_MERGE                0

    ID                              8
    VAL                             1000
    SEQ_INSIDE_MERGE                0

    ID                              9
    VAL                             1000
    SEQ_INSIDE_MERGE                0

    ID                              10
    VAL                             1000
    SEQ_INSIDE_MERGE                0

    ID                              11
    VAL                             1000
    SEQ_INSIDE_MERGE                0

    ID                              12
    VAL                             1000
    SEQ_INSIDE_MERGE                0

    ID                              13
    VAL                             1000
    SEQ_INSIDE_MERGE                0

    ID                              14
    VAL                             1000
    SEQ_INSIDE_MERGE                0

    ID                              15
    VAL                             1000
    SEQ_INSIDE_MERGE                0

    ID                              16
    VAL                             1000
    SEQ_INSIDE_MERGE                0

    ID                              17
    VAL                             1000
    SEQ_INSIDE_MERGE                0

"""
expected_stderr_2 = """
    Statement failed, SQLSTATE = 21000
    Multiple source records cannot match the same target during MERGE
"""

@pytest.mark.version('>=4.0')
def test_2(act_2: Action):
    act_2.expected_stdout = expected_stdout_2
    act_2.expected_stderr = expected_stderr_2
    act_2.execute()
    assert (act_2.clean_stderr == act_2.clean_expected_stderr and
            act_2.clean_stdout == act_2.clean_expected_stdout)

