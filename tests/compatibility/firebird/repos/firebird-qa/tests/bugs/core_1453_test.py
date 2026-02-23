#coding:utf-8

"""
ID:          issue-1871
ISSUE:       1871
TITLE:       Allow usage of functions in LIST delimiter parameter
DESCRIPTION:
JIRA:        CORE-1443
FBTEST:      bugs.core_1453
NOTES:
    [23.08.2024] pzotov
    Reimplemented: we have to avoid to show result of LIST() call because unpredictable order of its tokens.
    This can cause fail if we change OptimizeForFirstRows = true config parameter.
    Instead, test apply char_len() to the result of list(<...>, <func>).
"""

import pytest
from firebird.qa import *

init_script = """
    create table t1 (id integer, name char(20));
    commit;
    insert into t1 (id,name) values (1,'orange');
    insert into t1 (id,name) values (1,'apple');
    insert into t1 (id,name) values (1,'lemon');
    insert into t1 (id,name) values (2,'orange');
    insert into t1 (id,name) values (2,'apple');
    insert into t1 (id,name) values (2,'pear');
    commit;
"""

db = db_factory(init=init_script)

test_script = """
    set list on;
    select id, char_length(list( trim(name), ascii_char(35) )) chr_len
    from t1
    group by id
    order by id;
"""

act = isql_act('db', test_script, substitutions = [ ('[ \t]+', ' '), ])

expected_stdout = """
    ID                              1
    CHR_LEN                         18
    ID                              2
    CHR_LEN                         17
"""

@pytest.mark.version('>=2.5.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

