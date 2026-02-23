#coding:utf-8

"""
ID:          05f43be8fd
ISSUE:       https://www.sqlite.org/src/tktview/05f43be8fd
TITLE:       Incorrect use of index with LIKE operators when the LHS is a blob
DESCRIPTION:
NOTES:
    [21.08.2025] pzotov
    Checked on 6.0.0.1232, 5.0.4.1701, 4.0.7.3231, 3.0.14.33824
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    create collation coll_ci for utf8 from unicode case insensitive;
    create domain dm_char_ci as varchar(10) character set utf8 collate coll_ci;
    create domain dm_blob_ci as blob character set utf8 collate coll_ci;
    create table t1(x dm_char_ci unique, y dm_blob_ci);
    insert into t1(x, y) values(x'616263', x'616263');
    set count on;
    select 'q1' msg, x from t1 where x like 'A%';
    select 'q2' msg, x from t1 where y like 'A%';

"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    MSG q1
    X abc
    Records affected: 1
    MSG q2
    X abc
    Records affected: 1
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
