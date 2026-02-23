#coding:utf-8

"""
ID:          issue-7888
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/7888
TITLE:       The ability to retrieve the total number of pages in the database
NOTES:
    [10.01.2024] pzotov
    Checked on 6.0.0.199
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    select
        iif(mon_pages = pg_allo, 'PASSED', 'FAILED: mon_pages=' || mon_pages || ', pg_allo=' || pg_allo) as mon_pages_check
       ,iif(pg_allo = pg_used + pg_free, 'PASSED', 'FAILED: pg_allo=' || pg_allo || ', pg_used=' || pg_used || ', pg_free=' || pg_free) as pg_sum_check
    from (
        select
             m.mon$pages as mon_pages
            ,coalesce( cast(rdb$get_context('SYSTEM', 'PAGES_ALLOCATED') as int), -1) as pg_allo
            ,coalesce( cast(rdb$get_context('SYSTEM', 'PAGES_USED') as int), -1) as pg_used
            ,coalesce( cast(rdb$get_context('SYSTEM', 'PAGES_FREE') as int), -1) as pg_free
        from mon$database m
    );
"""

act = isql_act('db', test_script)

expected_stdout = """
    MON_PAGES_CHECK                 PASSED
    PG_SUM_CHECK                    PASSED
"""

@pytest.mark.version('>=6.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
