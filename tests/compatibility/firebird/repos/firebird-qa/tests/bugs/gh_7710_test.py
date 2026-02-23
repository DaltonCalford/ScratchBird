#coding:utf-8

"""
ID:          issue-7710
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/7710
TITLE:       Expression index - more than one null value cause attempt to store duplicate value error
DESCRIPTION:
NOTES:
    [17.08.2023] pzotov
    Confirmed problem on 5.0.0.1163.
    Checked on intermediate build 5.0.0.1164.
"""

import pytest
from firebird.qa import *

db = db_factory(charset = 'utf8')

test_script = """
    set bail on;
    create table test (
        field1 char(10) not null
    );
    commit;

    insert into test(field1) values ('N');
    insert into test(field1) values ('N');
    insert into test(field1) values ('I');
    commit;
    create unique index test_i1 on test computed by (iif(field1 = 'I', 'I', null));
    commit;
    set heading off;
    select 'Ok.' from rdb$database;
"""

act = isql_act('db', test_script)

expected_stdout = """
    Ok.
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
