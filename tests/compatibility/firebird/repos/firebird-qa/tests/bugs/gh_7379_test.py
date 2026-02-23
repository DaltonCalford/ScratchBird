#coding:utf-8

"""
ID:          issue-7379
ISSUE:       7379
TITLE:       BLOB_APPEND with existing blob accepts malformed string
NOTES:
    [16.02.2023] pzotov
    Confirmed bug on 5.0.0.821, 4.0.3.2873
    Checked on 5.0.0.938, 4.0.3.2876 -- all fine.
    ::: NOTE :::
    If script is executed from command line using ISQL then problem looks as described in the ticket.
    But if the same script is executed from firebird-qa then FB 5.0.0.821 crashes.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;

    set term ^;
    create table t (b_utf8 blob sub_type text character set utf8)
    ^

    -- Correct: "Malformed string" error
    execute block
    as
        declare b blob sub_type text character set utf8;
    begin
        b = blob_append(b, x'FF');
    end
    ^

    -- Wrong: no "Malformed string" error
    execute block
    as
        declare b blob sub_type text character set utf8;
    begin
        b = blob_append(b, 'a');
        -- No error
        b = blob_append(b, x'FF');
        -- Malformed blob is saved to table
        insert into t (b_utf8) values (:b);
    end
    ^
"""

act = isql_act('db', test_script, substitutions=[('-At block line:.*', '')])

expected_stdout = """
    Statement failed, SQLSTATE = 22000
    Malformed string

    Statement failed, SQLSTATE = 22000
    Malformed string
"""

@pytest.mark.version('>=4.0.3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
