#coding:utf-8

"""
ID:          issue-7683
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/7683
TITLE:       rdb$time_zone_util.transitions returns an infinite resultset
DESCRIPTION:
NOTES:
    Confirmed bug on 5.0.0.1132, 4.0.3.2966
    Checked on 5.0.0.1145, 4.0.3.2975 -- all fine.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = f"""
    set list on;
    select max(cnt) as max_cnt
    from (
        select
            rdb$start_timestamp start_timestamp,
            rdb$end_timestamp stop_timestamp,
            rdb$effective_offset tz_offset,
            count(*) cnt
        from (
            select * from
            rdb$time_zone_util.transitions(
                'Europe/Moscow',
                '0001-01-01 00:00:00.0000 +00:00',
                '9999-12-31 23:59:59.9999 +00:00'
            )
            rows 1000
        )
        group by 1,2,3
    )
    ;
"""

act = isql_act('db', test_script)

expected_stdout = """
    MAX_CNT                         1
"""

@pytest.mark.version('>=4.0.3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
