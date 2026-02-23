#coding:utf-8

"""
ID:          issue-7237
ISSUE:       7237
TITLE:       Starting operator become unstable on indexed varchar fields
NOTES:
    [22.02.2023] pzotov
	Confirmed bug on 3.0.10.33601
    Cheched on 5.0.0.958; 4.0.3.2903; 3.0.11.33664 - all fine.
"""

import pytest
from firebird.qa import *

db = db_factory(charset = 'utf8')

test_script = """
    set list on;
    create table test(id int, str_field varchar(10) character set utf8 collate unicode_ci);
    create index test_idx1 on test (str_field);
    insert into test(id, str_field) values(1, 'ПАПА');

    -- Tests
    set count on;
    select id from test where str_field starting 'П';    -- Found
    select id from test where str_field starting 'ПА';   -- Not found
    select id from test where str_field starting 'ПАП';  -- Found
    select id from test where str_field starting 'ПАПА'; -- Not found
"""

expected_stdout = """
    ID                              1
    Records affected: 1

    ID                              1
    Records affected: 1

    ID                              1
    Records affected: 1

    ID                              1
    Records affected: 1
"""

act = isql_act('db', test_script)

@pytest.mark.version('>=3.0.10')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True, charset = 'utf8')
    assert act.clean_stdout == act.clean_expected_stdout
