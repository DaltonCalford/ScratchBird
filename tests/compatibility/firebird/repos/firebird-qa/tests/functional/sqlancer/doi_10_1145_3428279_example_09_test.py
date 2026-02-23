#coding:utf-8

"""
ID:          n/a
ISSUE:       https://dl.acm.org/doi/pdf/10.1145/3428279
TITLE:       GROUP BY ignores COLLATE with case insensitive attribute
DESCRIPTION:
    Manuel Rigger and Zhendong Su
    Finding Bugs in Database Systems via Query Partitioning
    https://dl.acm.org/doi/pdf/10.1145/3428279
    page 13 listing 9
NOTES:
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    create collation name_coll for utf8 from unicode case insensitive;
    create domain dm_test varchar(1) character set utf8 collate name_coll;

    create table t0 (c0 dm_test);
    insert into t0 (c0) values ( 'a');
    insert into t0 (c0) values ( 'A');
    select count(*) as grouping_cnt from (
        select t0.c0 from t0 group by t0.c0
    ); 
"""

act = isql_act('db', test_script, substitutions=[('[ \t]+', ' ')])

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = """
        GROUPING_CNT 1
    """
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
