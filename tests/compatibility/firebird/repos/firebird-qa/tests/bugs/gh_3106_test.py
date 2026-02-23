#coding:utf-8

"""
ID:          issue-3106
ISSUE:       3106
TITLE:       Indexed reads in a compound index with NULLs present even if record does not exist
DESCRIPTION:
JIRA:        CORE-2709
FBTEST:      bugs.gh_3106
NOTES:
    [21.06.2022] pzotov
    Confirmed bug on 4.0.0.2451: table statistics has three indexed reads for 'TEST' table.
    Checked on 4.0.1.2692, 5.0.0.509.

    [04.07.2025] pzotov
    Re-implemented using con.info.get_table_access_stats(), removed trace launch and parsing.
    Confirmed problem on 4.0.0.2451.
    Checked on 6.0.0.892; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import locale
import re
import pytest
from firebird.qa import *

init_script = '''
    recreate table test (
        id1 int,
        id2 int,
        id3 int
    );
    commit;

    insert into test (id1, id2, id3) values (1, 1, null);
    insert into test (id1, id2, id3) values (1, 2, null);
    insert into test (id1, id2, id3) values (1, 3, null);
    insert into test (id1, id2, id3) values (2, 1, null);
    insert into test (id1, id2, id3) values (2, 2, null);
    insert into test (id1, id2, id3) values (2, 3, null);
    commit;

    create index test_idx_compound on test (id1,id2,id3);
    commit;
'''

db = db_factory(init = init_script)
act = python_act('db', substitutions=[('[ \t]+', ' ')])

expected_out = 'EXPECTED'

@pytest.mark.trace
@pytest.mark.version('>=3.0')
def test_1(act: Action, capsys):

    with act.db.connect() as con:
        idx_reads = 0
        cur = con.cursor()
        cur.execute("select rdb$relation_id from rdb$relations where rdb$relation_name = upper('test')")
        src_relation_id = cur.fetchone()[0]

        for x_table in con.info.get_table_access_stats():
            if x_table.table_id == src_relation_id:
                idx_reads = -x_table.indexed if x_table.indexed else 0

        cur.execute('select 1 /* trace_me */ from test where ID1 = 1 and ID2 IS NULL')
        data = cur.fetchall()
        for x_table in con.info.get_table_access_stats():
            if x_table.table_id == src_relation_id:
                idx_reads += x_table.indexed if x_table.indexed else 0

    if idx_reads == 0:
        print(expected_out)
    else:
        print(f'UNEXPECTED: {data=}, {idx_reads=} ')

    act.expected_stdout = expected_out
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
