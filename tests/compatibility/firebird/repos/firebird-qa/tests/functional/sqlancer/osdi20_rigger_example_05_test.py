#coding:utf-8

"""
ID:          n/a
ISSUE:       https://www.usenix.org/system/files/osdi20-rigger.pdf
TITLE:       Incorrect result of LIKE.
DESCRIPTION:
    https://www.usenix.org/system/files/osdi20-rigger.pdf
    page 10 listing 5
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    create collation name_coll for utf8 from unicode case insensitive;
    create domain dm_test varchar(2) character set utf8 collate name_coll;

    create table t0 (c0 dm_test unique);
    insert into t0 (c0) values ( './');
    select * from t0 where c0 like './';
"""

act = isql_act('db', test_script, substitutions=[('[ \t]+', ' ')])

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = """
        C0 ./
    """
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
