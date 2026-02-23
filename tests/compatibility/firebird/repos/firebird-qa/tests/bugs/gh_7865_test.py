#coding:utf-8

"""
ID:          issue-7865
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/7865
TITLE:       Consider the return value of deterministic functions to be invariant if all its arguments are invariant
DESCRIPTION:
    Test uses example provided in the ticket: we check performance of query which has `WHERE` clause
    with deterministic function in one of its parts.
NOTES:
    [12.05.2025] pzotov
    Further checks may be included into this test later.
    One may find this link useful (for those who can read in Russian):
    https://murcode.ru/search/deterministic/?message=True&topic=True&user=False&forum=2&orderby=byDefault

    Confirmed improvement on 6.0.0.779-136fa13: number of NR = 2 * <number_of_rows>
    Before this fix NR was <number_of_rows> * <number_of_rows> + <number_of_rows> (checked on 6.0.0.770-82c4a08)
"""

import pytest
from firebird.qa import *

######################################
ROWS_COUNT = 30
MAX_ALLOWED_NAT_READS = 2 * ROWS_COUNT
######################################

init_sql = f"""
    create table test(id int, x bigint);
    insert into test(id, x) select i, i*i from (select row_number()over() as i from rdb$types rows {ROWS_COUNT});
    commit;

    set term ^;
    create function fb_get_x_for_id(a_id int) returns bigint deterministic as
    begin
       return (select t.x from test t where t.id = :a_id);
    end
    ^
    commit
    ^
    set term ;^
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

        cur.execute(f"select * from test where x = fb_get_x_for_id({ROWS_COUNT})")
        # cur.execute(f"select * from test where fb_get_x_for_id({ROWS_COUNT}) = x") -- checked; result is the same.
        data = cur.fetchall()

        for x_table in con.info.get_table_access_stats():
            if x_table.table_id == src_relation_id:
                nat_reads[src_relation_id] += x_table.sequential
            
        if nat_reads[src_relation_id] <= MAX_ALLOWED_NAT_READS:
            print(f'{msg_prefix} {expected_txt}')
        else:
            print(f'{msg_prefix} UNEXPECTED: {nat_reads[src_relation_id]} - greater than threshold = {MAX_ALLOWED_NAT_READS}.')

    act.expected_stdout = f"""
        {msg_prefix} {expected_txt}
    """

    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
