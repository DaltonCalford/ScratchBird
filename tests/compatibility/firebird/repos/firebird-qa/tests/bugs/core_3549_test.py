#coding:utf-8

"""
ID:          issue-3905
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/3905
TITLE:       Database corruption after end of session : page xxx is of wrong type expected 4 found 7
DESCRIPTION:
JIRA:        CORE-3549
FBTEST:      bugs.core_3549
NOTES:
    [24.07.2025] pzotov
    1. Could NOT reproduce on official 2.5.1 (WI-V2.5.1.26351, issued 03-oct-2011).
       According to https://github.com/FirebirdSQL/firebird/issues/3905#issuecomment-826222237,
       bug can be visible only in narrow scope of snapshots from ~30-may-2011 to 16-jul-2011.
       Fix for this ticket in 2.5.1 was before official 2.5.1 release: 17-jul-2011, rev. 53327

    2. Changed code: start usage DB with page_size = 8192 - this is minimal size in 6.x.
       The number of transactions which we have to start in order to force engine create record
       in RDB$PAGES with RDB$RELATION_ID = 0 can be evaluated using mon$database.mon$page_size
       (see https://github.com/FirebirdSQL/firebird/issues/3905#issuecomment-826222236).
    
    Checked on 6.0.0.1061; 5.0.3.1686; 4.0.6.3223; 3.0.13.33818.
"""

import pytest
from firebird.qa import *

db = db_factory(page_size = 8192)

substitutions = [('[ \t]+', ' ')]
act = python_act('db', substitutions = substitutions)

@pytest.mark.version('>=3')
def test_1(act: Action):

    test_script = f"""
        set list on;
        
        create view v_page_info as
        select rdb$page_type pg_type, count(distinct rdb$page_sequence) pg_seq_distinct
        from rdb$pages
        where rdb$relation_id = 0 and rdb$page_type=3 -- page_type = '3' --> TIP
        group by 1;
        commit;
        select * from v_page_info;

        set autoddl off;
        create global temporary table gtt_test(x int) on commit preserve rows;
        create index gtt_test_x on gtt_test(x);
        commit;

        set term ^;
        execute block as
            declare i int = 0;
            declare n int;
        begin
            select mon$page_size from mon$database into n;
            while (i < n * 4 + 1) do -- start page_size * 4 transactions + smth more (to be sure :))
            begin
                in autonomous transaction do
                execute statement 'insert into gtt_test values (1)';
                i = i + 1;
            end
        end
        ^
        set term ;^

        select * from v_page_info;
        commit;
        connect '{act.db.dsn}' user {act.db.user} password '{act.db.password}';
    """


    expected_stdout = """
        PG_TYPE                         3
        PG_SEQ_DISTINCT                 1
        PG_TYPE                         3
        PG_SEQ_DISTINCT                 2
    """

    act.expected_stdout = expected_stdout
    act.isql(switches = ['-q'], input = test_script, combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout

