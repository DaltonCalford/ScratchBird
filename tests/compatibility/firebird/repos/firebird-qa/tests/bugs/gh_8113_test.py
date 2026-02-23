#coding:utf-8

"""
ID:          issue-8113
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/8113
TITLE:       UNION ALL optimization with constant false condition
DESCRIPTION:
    Test uses script based on example from ticket.
    Number of UNIONed parts is defined via UNION_MEMBERS_CNT variable.
    We compare number of natural reads with threshold = 1 (see MAX_ALLOWED_NAT_READS).
NOTES:
    [18.11.2024] pzotov
    Confirmed excessive reads on 6.0.0.520.
    Checked on 6.0.0.532 -- all OK.
"""

import pytest
from firebird.qa import *

#########################
MAX_ALLOWED_NAT_READS = 1
UNION_MEMBERS_CNT = 254
#########################

view_ddl = f'recreate view v_test as '  + '\nunion all '.join( [f'select {i} as x from test' for i in range(UNION_MEMBERS_CNT)] ) + '\n;'
init_sql = f"""
    recreate table test(id int);
    insert into test(id) values(0);
    commit;
    {view_ddl}
"""

db = db_factory(init = init_sql)

act = python_act('db')

#-----------------------------------------------------------

@pytest.mark.version('>=6.0')
def test_1(act: Action, capsys):
    msg_prefix = 'Number of natural reads:'
    expected_txt = 'EXPECTED'
    nat_reads = {}
    with act.db.connect() as con:
        cur = con.cursor()

        cur.execute("select rdb$relation_id from rdb$relations where rdb$relation_name = upper('test')")
        src_relation_id = cur.fetchone()[0]
        nat_reads[src_relation_id] = 0

        for x_table in con.info.get_table_access_stats():
            if x_table.table_id == src_relation_id:
                nat_reads[src_relation_id] = -x_table.sequential

        cur.execute(f"select /* trace_me */ x from v_test where x = {UNION_MEMBERS_CNT-1}")
        data = cur.fetchall()

        for x_table in con.info.get_table_access_stats():
            if x_table.table_id == src_relation_id:
                nat_reads[src_relation_id] += x_table.sequential
            
        if nat_reads[src_relation_id] <= MAX_ALLOWED_NAT_READS:
            print(f'{msg_prefix} {expected_txt}')
        else:
            print(f'{msg_prefix} UNEXPECTED: {nat_reads[src_relation_id]} - greater than threshold = {MAX_ALLOWED_NAT_READS}.')
            print('Check view source:')
            cur.execute("select rdb$view_source from rdb$relations where rdb$relation_name = upper('v_test')")
            v_source = cur.fetchall()[0]
            for line in v_source[0].split('\n'):
                print(line)


    act.expected_stdout = f"""
        {msg_prefix} {expected_txt}
    """

    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
