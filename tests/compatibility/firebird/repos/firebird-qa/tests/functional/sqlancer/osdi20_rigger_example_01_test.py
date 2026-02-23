#coding:utf-8

"""
ID:          n/a
ISSUE:       https://www.usenix.org/system/files/osdi20-rigger.pdf
TITLE:       Partial index must not use incorrect assumption that 'c0 IS NOT 1' implied 'c0 NOT NULL'
DESCRIPTION:
    https://www.usenix.org/system/files/osdi20-rigger.pdf
    page 3 listing 1
NOTES:
    [05.06.2025] pzotov
    Support for partial indices in FB:
    https://github.com/FirebirdSQL/firebird/pull/7257
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    recreate table t0 ( c0 int );
    create index i0 on t0 (c0) where c0 is not null;
    insert into t0 values (0);
    insert into t0 values (1);
    insert into t0 values (null);
    select c0 from t0 where c0 is distinct from 1;
"""

act = isql_act('db', test_script, substitutions=[('[ \t]+', ' ')])

@pytest.mark.version('>=5.0')
def test_1(act: Action):
    act.expected_stdout = """
        C0 0
        C0 <null>
    """
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
