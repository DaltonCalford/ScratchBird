#coding:utf-8

"""
ID:          n/a
ISSUE:       https://www.usenix.org/system/files/osdi20-rigger.pdf
TITLE:       Unexpected null value when index is used
DESCRIPTION:
    https://www.usenix.org/system/files/osdi20-rigger.pdf
    page 11 listing 12
NOTES:
    [05.06.2025] pzotov
    page_size = 32K is used in this test.
"""

import pytest
from firebird.qa import *

db = db_factory(page_size = 32768)

test_script = """
    set list on;
    recreate table t0 (c0 varchar(8183));
    insert into t0 (c0) values('b');
    insert into t0 (c0) values('a');

    insert into t0 (c0) values (null);
    update t0 set c0 = 'a';
    commit;

    create index i0 on t0 (c0);

    set count on;
    select * from t0 where 'baaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa' > t0.c0 ; -- error : found unexpected null value in index " i0 "
    commit;
"""

act = isql_act('db', test_script, substitutions=[('[ \t]+', ' ')])

@pytest.mark.version('>=4.0')
def test_1(act: Action):
    act.expected_stdout = """
        C0 a
        C0 a
        C0 a
        Records affected: 3
    """
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
