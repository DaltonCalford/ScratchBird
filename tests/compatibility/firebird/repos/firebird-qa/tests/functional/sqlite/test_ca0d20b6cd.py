#coding:utf-8

"""
ID:          ca0d20b6cd
ISSUE:       https://www.sqlite.org/src/tktview/ca0d20b6cd
TITLE:       COLLATE operator masked by function calls
DESCRIPTION:
NOTES:
    [21.08.2025] pzotov
    Checked on 6.0.0.1232, 5.0.4.1701, 4.0.7.3231, 3.0.14.33824
"""

import pytest
from firebird.qa import *

db = db_factory(charset = 'utf8')

test_script = """
    set list on;
    create collation nocase for utf8 from unicode case insensitive;
    set count on;
    select 'abc' collate nocase = ('ABC' || '') collate nocase as v1 from rdb$database;
    select 'abc' collate nocase = ('ABC' || '' collate nocase) as v2 from rdb$database;
    select 'abc' collate nocase = ('ABC' || ('' collate nocase)) as v3 from rdb$database;
    select 'abc' collate nocase = ('ABC' || upper('' collate nocase))as v4 from rdb$database;
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    V1 <true>
    Records affected: 1
    V2 <true>
    Records affected: 1
    V3 <true>
    Records affected: 1
    V4 <true>
    Records affected: 1
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
