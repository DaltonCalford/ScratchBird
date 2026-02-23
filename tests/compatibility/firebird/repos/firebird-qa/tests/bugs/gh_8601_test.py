#coding:utf-8

"""
ID:          issue-8601
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/8601
TITLE:       Charset and collation are not found in the search path when altering a table (FB 6.0 SQL schemas)
DESCRIPTION:
NOTES:
    [14.06.2025] pzotov
    Confirmed bug on 6.0.0.835, got: -Data type unknown / -CHARACTER SET "PUBLIC"."UTF8" is not defined
    Checked on 6.0.0.838-0-0b49fa8.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set bail on;
    set list on;
    recreate table t1 (
        id bigint not null primary key,
        name varchar(10) character set utf8
    );

    alter table t1
    alter name type varchar(10) character set utf8 collate unicode_ci_ai;
    insert into t1(id, name) values(1, 'äÄöÖõÕšŠžŽ');
    insert into t1(id, name) values(2, 'AaOoOOSsZZ');
    select count(*) from t1 where name like 'AA%Zz';
"""

act = isql_act('db', test_script, substitutions=[ ('[ \\t]+', ' ') ])

expected_stdout = """
    COUNT 2
"""

@pytest.mark.version('>=6.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
