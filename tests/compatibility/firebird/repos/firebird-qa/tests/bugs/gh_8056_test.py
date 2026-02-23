#coding:utf-8

"""
ID:          issue-8056
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/8056
TITLE:       "Too many temporary blobs" with blob_append when select a stored procedue using rows-clause 
DESCRIPTION:
NOTES:
    Confirmed bug on 6.0.0.293, 5.0.1.1369 got:
        Statement failed, SQLSTATE = HY000
        Too many temporary blobs
        -At procedure 'SP_TEST' line: 6, col: 9
    Checked on 6.0.0.295 bf5ab97, 5.0.1.1370 906e270, 4.0.5.3080 5d44e7c
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set blob all;
    set list on;
    set bail on;
    set term ^;
    create or alter procedure sp_count (astart bigint, aend bigint) returns (ovalue bigint) as
    begin
        :ovalue = :astart;
        while (:ovalue <= :aend) do
        begin
            suspend;
            :ovalue = :ovalue + 1;
        end
    end
    ^

    create or alter procedure sp_test
    returns (
        blob_app_result blob sub_type 1 segment size 80 character set utf8 collate utf8
    ) as
    begin
        :blob_app_result = blob_append(null, '');
        :blob_app_result = blob_append(:blob_app_result, 'hello');
        :blob_app_result = blob_append(:blob_app_result, ' world');
        suspend;
    end
    ^
    commit
    ^
    execute block as
        declare ovalue bigint;
        declare blob_app_result blob;
    begin
        for
            select n.ovalue, p.blob_app_result
            from sp_count(1, 18000) n
            join sp_test p on 1 = 1
            rows 17500 to 18000
            as cursor c
        do begin
        end
    end
    ^
    select 'Passed' as msg from rdb$database
    ^
"""

act = isql_act('db', test_script, substitutions=[('[ \t]+', ' ')])

expected_stdout = """
    MSG Passed 
"""

@pytest.mark.version('>=4.0.5')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
