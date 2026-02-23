#coding:utf-8

"""
ID:          fb8c538a8f
ISSUE:       https://www.sqlite.org/src/tktview/fb8c538a8f
TITLE:       Incorrect sorting when a column uses BINARY collation in the ORDER BY, but is compared with a different collation in the WHERE clause
DESCRIPTION:
NOTES:
    [15.08.2025] pzotov
    Checked on 6.0.0.1204, 5.0.4.1701, 4.0.7.3231, 3.0.14.33824
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    create collation nocase_coll for utf8 from unicode case insensitive;
    create domain dm_nocase varchar(3) character set utf8 collate nocase_coll;

    CREATE TABLE t1(b dm_nocase);
    insert into t1 values('abc');
    insert into t1 values('ABC');
    insert into t1 values('aBC');

    set count on;

    -- correctly returns: "ABC aBC abc"
    select * from t1 order by cast(b as char(16) character set octets);

    -- incorrectly returned: "abc ABC aBC"
    select * from t1 where b = 'abc' order by cast(b as char(16) character set octets);
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    B ABC
    B aBC
    B abc
    Records affected: 3

    B ABC
    B aBC
    B abc
    Records affected: 3
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
