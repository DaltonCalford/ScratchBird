#coding:utf-8

"""
ID:          issue-8291
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/8291
TITLE:       NULLs should be skipped during index navigation when there's no lower bound and matched conditions are known to ignore NULLs
DESCRIPTION:
    Test uses script from ticket. We compare number of indexed reads with threshold - see variable MAX_ALLOWED_IDX_READS.
    BEFORE fix value of indexed was 500002 (both on 5.x and 6.x), after fix it became 886 (using default page_size = 8k).
NOTES:
    [25.10.2024] pzotov
    Confirmed problem on 6.0.0.485, 5.0.2.1519.
    Checked on 6.0.0.502-d2f4cf6, 5.0.2.1542-ab50e20 (intermediate builds).

    [25.02.2025] pzotov
    Splitted code that defines value of MAX_ALLOWED_IDX_READS: for 6.x it can be safely reduced to 6...10 since commit #5767b9.
    Checked on intermediate snapshot 6.0.0.654-5767b9e.
"""

import pytest
from firebird.qa import *

init_sql = """
    create table test (id int);

    set term ^;
    execute block as
        declare n int = 1000000;
        declare i int = 0;
    begin
        while (i < n) do
        begin
            insert into test(id) values( iif(mod(:i, 2) = 0, null, :i) );
            i = i + 1;
        end
    end^
    set term ;^
    commit;

    create index test_id on test(id);
    commit;
"""
db = db_factory(page_size = 8192, init = init_sql)

act = python_act('db')

#-----------------------------------------------------------

@pytest.mark.version('>=5.0.2')
def test_1(act: Action, capsys):

    # ::::::::::::::::::::::::::::::
    # :::    T H R E S H O L D   :::
    # ::::::::::::::::::::::::::::::
    if act.is_version('<6'):
        MAX_ALLOWED_IDX_READS = 1000
    else:
        # NB. For the query that is used here number of indexed reads have been drastically reduced
        # since #5767b9 (Ignore NULLs (if desired) while scanning keys during index navigation (#8446)).
        # https://github.com/FirebirdSQL/firebird/commit/5767b9e522aa0b0ef36790f041a26bfd4f2fe738
        # https://github.com/FirebirdSQL/firebird/pull/8446
        # For 6.0.0.652 this value is 3 (three), so we can safely set it to 6...10
        MAX_ALLOWED_IDX_READS = 6
    
    msg_prefix = 'Number of indexed reads:'
    expected_txt = 'EXPECTED'
    idx_reads = {}
    with act.db.connect() as con:
        cur = con.cursor()
        cur.execute("select rdb$relation_id from rdb$relations where rdb$relation_name = 'TEST'")
        test_rel_id = cur.fetchone()[0]
        idx_reads[test_rel_id] = 0

        for x_table in con.info.get_table_access_stats():
            if x_table.table_id == test_rel_id:
                idx_reads[test_rel_id] = -x_table.indexed

        cur.execute('select count(*) from (select id from test where id < 3 order by id)')
        data = cur.fetchall()

        for x_table in con.info.get_table_access_stats():
            if x_table.table_id == test_rel_id:
                idx_reads[test_rel_id] += x_table.indexed
            
        # BEFORE fix value of indexed was 500002. After fix: 886
        if idx_reads[test_rel_id] < MAX_ALLOWED_IDX_READS:
            print(f'{msg_prefix} {expected_txt}')
        else:
            print(f'{msg_prefix} UNEXPECTED: {idx_reads[test_rel_id]} - greater than threshold = {MAX_ALLOWED_IDX_READS}.')

    act.expected_stdout = f"""
        {msg_prefix} {expected_txt}
    """

    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
